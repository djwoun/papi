/**
 * @file    amds_ctx.c
 * @author  Dong Jun Woun 
 *          djwoun@gmail.com
 *
 */

#include "amds.h"
#include "amds_priv.h"
#include "papi.h"
#include "papi_memory.h"
#include "papi_internal.h"
#include <stdint.h>   // for uint64_t, INT types

unsigned int _amd_smi_lock;

/* Use a 64-bit global device mask to support up to 64 devices */
static uint64_t device_mask = 0;

static int resolve_event_device(native_event_t *event, const event_info_t *info,
                                int *device_out) {
  if (!event || !info) {
    return PAPI_EINVAL;
  }
  int dev_id;
  if (event->device < 0) {
    if (info->flags & DEVICE_FLAG) {
      dev_id = info->device;
    } else {
      dev_id = event->device;
    }
  } else {
    if (info->flags & DEVICE_FLAG) {
      return PAPI_EINVAL;
    }
    dev_id = event->device;
  }
  if (device_out) {
    *device_out = dev_id;
  }
  return PAPI_OK;
}

static int invoke_event_func(native_event_t *event, const event_info_t *info,
                             int (*func)(native_event_t *)) {
  if (!func) {
    return PAPI_OK;
  }
  int dev_id = -1;
  int papi_errno = resolve_event_device(event, info, &dev_id);
  if (papi_errno != PAPI_OK) {
    return papi_errno;
  }
  int32_t saved_device = event->device;
  if (event->device < 0 && dev_id >= 0) {
    event->device = dev_id;
  }
  papi_errno = func(event);
  event->device = saved_device;
  return papi_errno;
}

static int acquire_devices(unsigned int *events_id, int num_events, uint64_t *bitmask) {
  if (!bitmask) return PAPI_EINVAL;
  if (num_events < 0) return PAPI_EINVAL;
  if (num_events > 0 && !events_id) return PAPI_EINVAL;
  if (!ntv_table_p) return PAPI_ECMP;

  uint64_t mask_acq = 0;
  for (int i = 0; i < num_events; ++i) {
    uint64_t code = (uint64_t)events_id[i];
    event_info_t info;
    decode_event_code(code, &info);
    if (info.nameid < 0 || info.nameid >= ntv_table_p->count) {
      return PAPI_EINVAL;
    }
    native_event_t *ev = &ntv_table_p->events[info.nameid];
    int dev_id;
    if (ev->device < 0) {
      if (info.flags & DEVICE_FLAG) {
        dev_id = info.device;
      } else {
        continue; /* no device associated */
      }
    } else {
      if (info.flags & DEVICE_FLAG) {
        return PAPI_EINVAL;
      }
      dev_id = ev->device;
    }
    if (dev_id < 0) {
      continue;
    }
    if (dev_id >= 64) return PAPI_EINVAL; /* out of representable range */
    mask_acq |= (UINT64_C(1) << dev_id);
  }

  _papi_hwi_lock(_amd_smi_lock);
  if (mask_acq & device_mask) {
    _papi_hwi_unlock(_amd_smi_lock);
    return PAPI_ECNFLCT; // conflict: device already in use
  }
  device_mask |= mask_acq;
  _papi_hwi_unlock(_amd_smi_lock);

  *bitmask = mask_acq;
  return PAPI_OK;
}

static int release_devices(uint64_t *bitmask) {
  if (!bitmask) return PAPI_EINVAL;
  uint64_t mask_rel = *bitmask;

  _papi_hwi_lock(_amd_smi_lock);
  if ((mask_rel & device_mask) != mask_rel) {
    _papi_hwi_unlock(_amd_smi_lock);
    return PAPI_EMISC;
  }
  /* Clear with &= ~mask for clarity and robustness */
  device_mask &= ~mask_rel;
  _papi_hwi_unlock(_amd_smi_lock);

  *bitmask = 0;
  return PAPI_OK;
}

/* Context management: open/close, start/stop, read/write, reset */
struct amds_ctx {
  int state;
  unsigned int *events_id;
  int num_events;
  long long *counters;
  uint64_t device_mask;       /* was int32_t: now 64-bit to match global */
};

int amds_ctx_open(unsigned int *event_ids, int num_events, amds_ctx_t *ctx) {
  if (!ctx) return PAPI_EINVAL;
  if (num_events < 0) return PAPI_EINVAL;
  if (num_events > 0 && !event_ids) return PAPI_EINVAL;
  if (!ntv_table_p) return PAPI_ECMP;

  amds_ctx_t new_ctx = (amds_ctx_t)papi_calloc(1, sizeof(struct amds_ctx));
  if (new_ctx == NULL) {
    return PAPI_ENOMEM;
  }
  new_ctx->events_id = event_ids; // Store pointer (caller must keep stable)
  new_ctx->num_events = num_events;
  new_ctx->counters = (long long *)papi_calloc((size_t)num_events, sizeof(long long));
  if (new_ctx->counters == NULL) {
    papi_free(new_ctx);
    return PAPI_ENOMEM;
  }

  /* Validate event ids range before acquiring devices */
  for (int i = 0; i < num_events; ++i) {
    event_info_t info;
    decode_event_code((uint64_t)event_ids[i], &info);
    if (info.nameid < 0 || info.nameid >= ntv_table_p->count) {
      papi_free(new_ctx->counters);
      papi_free(new_ctx);
      return PAPI_EINVAL;
    }
  }

  /* Acquire devices needed by these events to avoid conflicts */
  int papi_errno = acquire_devices(event_ids, num_events, &new_ctx->device_mask);
  if (papi_errno != PAPI_OK) {
    papi_free(new_ctx->counters);
    papi_free(new_ctx);
    return papi_errno;
  }

  for (int i = 0; i < num_events; ++i) {
    event_info_t info;
    decode_event_code((uint64_t)event_ids[i], &info);
    if (info.nameid < 0 || info.nameid >= ntv_table_p->count) {
      papi_errno = PAPI_EINVAL;
    } else {
      native_event_t *ev = &ntv_table_p->events[info.nameid];
      papi_errno = invoke_event_func(ev, &info, ev->open_func);
    }
    if (papi_errno != PAPI_OK) {
      for (int j = 0; j < i; ++j) {
        event_info_t info2;
        decode_event_code((uint64_t)event_ids[j], &info2);
        if (info2.nameid < 0 || info2.nameid >= ntv_table_p->count) {
          continue;
        }
        native_event_t *prev = &ntv_table_p->events[info2.nameid];
        (void)invoke_event_func(prev, &info2, prev->close_func);
      }
      release_devices(&new_ctx->device_mask);
      papi_free(new_ctx->counters);
      papi_free(new_ctx);
      return papi_errno;
    }
  }
  *ctx = new_ctx;
  return PAPI_OK;
}

int amds_ctx_close(amds_ctx_t ctx) {
  if (!ctx)
    return PAPI_OK;
  if (!ntv_table_p) {
    /* Best effort: release devices and free even if table is gone */
    (void)release_devices(&ctx->device_mask);
    papi_free(ctx->counters);
    papi_free(ctx);
    return PAPI_OK;
  }
  for (int i = 0; i < ctx->num_events; ++i) {
    event_info_t info;
    decode_event_code((uint64_t)ctx->events_id[i], &info);
    if (info.nameid < 0 || info.nameid >= ntv_table_p->count) {
      continue;
    }
    native_event_t *ev = &ntv_table_p->events[info.nameid];
    (void)invoke_event_func(ev, &info, ev->close_func);
  }
  /* release device usage */
  (void)release_devices(&ctx->device_mask);
  papi_free(ctx->counters);
  papi_free(ctx);
  return PAPI_OK;
}

int amds_ctx_start(amds_ctx_t ctx) {
  if (!ctx) return PAPI_EINVAL;
  if (!ntv_table_p) return PAPI_ECMP;

  int papi_errno = PAPI_OK;
  for (int i = 0; i < ctx->num_events; ++i) {
    event_info_t info;
    decode_event_code((uint64_t)ctx->events_id[i], &info);
    if (info.nameid < 0 || info.nameid >= ntv_table_p->count) return PAPI_EINVAL;
    native_event_t *ev = &ntv_table_p->events[info.nameid];
    papi_errno = invoke_event_func(ev, &info, ev->start_func);
    if (papi_errno != PAPI_OK)
      return papi_errno;
  }
  ctx->state |= AMDS_EVENTS_RUNNING;
  return papi_errno;
}

int amds_ctx_stop(amds_ctx_t ctx) {
  if (!ctx) return PAPI_EINVAL;
  if (!(ctx->state & AMDS_EVENTS_RUNNING)) {
    return PAPI_OK;
  }
  if (!ntv_table_p) return PAPI_ECMP;

  int papi_errno = PAPI_OK;
  for (int i = 0; i < ctx->num_events; ++i) {
    event_info_t info;
    decode_event_code((uint64_t)ctx->events_id[i], &info);
    if (info.nameid < 0 || info.nameid >= ntv_table_p->count) {
      continue;
    }
    native_event_t *ev = &ntv_table_p->events[info.nameid];
    int papi_errno_stop = invoke_event_func(ev, &info, ev->stop_func);
    if (papi_errno == PAPI_OK)
      papi_errno = papi_errno_stop;
  }
  ctx->state &= ~AMDS_EVENTS_RUNNING;
  return papi_errno;
}

int amds_ctx_read(amds_ctx_t ctx, long long **counts) {
  if (!ctx || !counts) return PAPI_EINVAL;
  if (!ntv_table_p) return PAPI_ECMP;

  /* Always produce a fully defined buffer */
  for (int i = 0; i < ctx->num_events; ++i) {
    ctx->counters[i] = 0;  /* default if read fails */
  }

  /* Optional: track first error, but don't bail early */
  int papi_errno = PAPI_OK;

  for (int i = 0; i < ctx->num_events; ++i) {
    event_info_t info;
    decode_event_code((uint64_t)ctx->events_id[i], &info);
    if (info.nameid < 0 || info.nameid >= ntv_table_p->count) {
      if (papi_errno == PAPI_OK) papi_errno = PAPI_EINVAL;
      continue;
    }
    native_event_t *ev = &ntv_table_p->events[info.nameid];

    int dev_id = -1;
    int papi_errno_access = resolve_event_device(ev, &info, &dev_id);
    if (papi_errno_access != PAPI_OK) {
      if (papi_errno == PAPI_OK) papi_errno = papi_errno_access;
      continue;
    }

    int32_t saved_device = ev->device;
    if (ev->device < 0 && dev_id >= 0) {
      ev->device = dev_id;
    }

    if (ev->access_func) {
      papi_errno_access = ev->access_func(PAPI_MODE_READ, ev);
    } else {
      papi_errno_access = PAPI_ECMP;
    }

    ev->device = saved_device;

    if (papi_errno_access == PAPI_OK) {
      ctx->counters[i] = (long long)ev->value;
    } else if (papi_errno == PAPI_OK) {
      papi_errno = papi_errno_access;  /* remember, but keep going */
    }
  }

  *counts = ctx->counters;
  
  return papi_errno;
}

int amds_ctx_write(amds_ctx_t ctx, long long *counts) {
  if (!ctx || !counts) return PAPI_EINVAL;
  if (!ntv_table_p) return PAPI_ECMP;

  int papi_errno = PAPI_OK;
  for (int i = 0; i < ctx->num_events; ++i) {
    event_info_t info;
    decode_event_code((uint64_t)ctx->events_id[i], &info);
    if (info.nameid < 0 || info.nameid >= ntv_table_p->count) return PAPI_EINVAL;
    native_event_t *ev = &ntv_table_p->events[info.nameid];
    if (!ev->access_func) return PAPI_ECMP;

    int dev_id = -1;
    papi_errno = resolve_event_device(ev, &info, &dev_id);
    if (papi_errno != PAPI_OK) {
      return papi_errno;
    }

    int32_t saved_device = ev->device;
    if (ev->device < 0 && dev_id >= 0) {
      ev->device = dev_id;
    }

    ev->value = counts[i];
    papi_errno = ev->access_func(PAPI_MODE_WRITE, ev);
    ev->device = saved_device;
    if (papi_errno != PAPI_OK) {
      return papi_errno;
    }
  }
  return papi_errno;
}

int amds_ctx_reset(amds_ctx_t ctx) {
  if (!ctx) return PAPI_EINVAL;
  if (!ntv_table_p) return PAPI_ECMP;

  for (int i = 0; i < ctx->num_events; ++i) {
    event_info_t info;
    decode_event_code((uint64_t)ctx->events_id[i], &info);
    if (info.nameid < 0 || info.nameid >= ntv_table_p->count) return PAPI_EINVAL;
    native_event_t *ev = &ntv_table_p->events[info.nameid];
    ev->value = 0;
    if (ctx->counters) ctx->counters[i] = 0;
  }
  return PAPI_OK;
}
