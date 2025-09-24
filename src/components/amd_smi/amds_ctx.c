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

static int acquire_devices(uint64_t *events_id, int num_events, uint64_t *bitmask) {
  if (!bitmask) return PAPI_EINVAL;
  if (num_events < 0) return PAPI_EINVAL;
  if (num_events > 0 && !events_id) return PAPI_EINVAL;
  if (!ntv_table_p) return PAPI_ECMP;

  uint64_t mask_acq = 0;
  for (int i = 0; i < num_events; ++i) {
    int device = -1;
    int has_device = 0;
    int papi_errno = amds_evt_decode(events_id[i], NULL, &device, &has_device);
    if (papi_errno != PAPI_OK) {
      return papi_errno;
    }
    if (has_device) {
      if (device < 0 || device >= 64) {
        return PAPI_EINVAL;
      }
      mask_acq |= (UINT64_C(1) << device);
    }
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
  uint64_t *events_id;
  int num_events;
  long long *counters;
  uint64_t device_mask;       /* was int32_t: now 64-bit to match global */
  native_event_t **events;
};

int amds_ctx_open(uint64_t *event_ids, int num_events, amds_ctx_t *ctx) {
  if (!ctx) return PAPI_EINVAL;
  if (num_events < 0) return PAPI_EINVAL;
  if (num_events > 0 && !event_ids) return PAPI_EINVAL;
  if (!ntv_table_p) return PAPI_ECMP;

  amds_ctx_t new_ctx = (amds_ctx_t)papi_calloc(1, sizeof(struct amds_ctx));
  if (new_ctx == NULL) {
    return PAPI_ENOMEM;
  }
  new_ctx->events_id = event_ids;
  new_ctx->num_events = num_events;
  if (num_events > 0) {
    new_ctx->counters = (long long *)papi_calloc((size_t)num_events, sizeof(long long));
    if (new_ctx->counters == NULL) {
      papi_free(new_ctx);
      return PAPI_ENOMEM;
    }
    new_ctx->events = (native_event_t **)papi_calloc((size_t)num_events, sizeof(native_event_t *));
    if (new_ctx->events == NULL) {
      papi_free(new_ctx->counters);
      papi_free(new_ctx);
      return PAPI_ENOMEM;
    }
  }

  int papi_errno = acquire_devices(event_ids, num_events, &new_ctx->device_mask);
  if (papi_errno != PAPI_OK) {
    if (new_ctx->events) papi_free(new_ctx->events);
    papi_free(new_ctx->counters);
    papi_free(new_ctx);
    return papi_errno;
  }

  int prepared = 0;
  for (int i = 0; i < num_events; ++i) {
    int nameid = -1;
    int device = -1;
    int has_device = 0;
    papi_errno = amds_evt_decode(event_ids[i], &nameid, &device, &has_device);
    if (papi_errno != PAPI_OK)
      goto fn_fail;

    native_event_t *base = &ntv_table_p->events[nameid];
    native_event_t *copy = (native_event_t *)papi_malloc(sizeof(*copy));
    if (!copy) {
      papi_errno = PAPI_ENOMEM;
      goto fn_fail;
    }
    *copy = *base;
    copy->device = has_device ? device : -1;
    copy->value = 0;
    copy->priv = NULL;
    new_ctx->events[i] = copy;

    if (copy->open_func) {
      papi_errno = copy->open_func(copy);
      if (papi_errno != PAPI_OK) {
        papi_free(copy);
        new_ctx->events[i] = NULL;
        goto fn_fail;
      }
    }
    prepared = i + 1;
  }

  *ctx = new_ctx;
  return PAPI_OK;

fn_fail:
  if (new_ctx->events) {
    for (int j = 0; j < prepared; ++j) {
      native_event_t *ev = new_ctx->events[j];
      if (!ev)
        continue;
      if (ev->close_func)
        ev->close_func(ev);
      papi_free(ev);
      new_ctx->events[j] = NULL;
    }
    papi_free(new_ctx->events);
    new_ctx->events = NULL;
  }
  if (new_ctx->device_mask) {
    release_devices(&new_ctx->device_mask);
  }
  if (new_ctx->counters) {
    papi_free(new_ctx->counters);
  }
  papi_free(new_ctx);
  return papi_errno;
}

int amds_ctx_close(amds_ctx_t ctx) {
  if (!ctx)
    return PAPI_OK;
  if (ctx->events) {
    for (int i = 0; i < ctx->num_events; ++i) {
      native_event_t *ev = ctx->events[i];
      if (!ev)
        continue;
      if (ev->close_func)
        ev->close_func(ev);
      papi_free(ev);
      ctx->events[i] = NULL;
    }
    papi_free(ctx->events);
    ctx->events = NULL;
  }
  (void)release_devices(&ctx->device_mask);
  papi_free(ctx->counters);
  papi_free(ctx);
  return PAPI_OK;
}

int amds_ctx_start(amds_ctx_t ctx) {
  if (!ctx) return PAPI_EINVAL;
  int papi_errno = PAPI_OK;
  for (int i = 0; i < ctx->num_events; ++i) {
    native_event_t *ev = ctx->events ? ctx->events[i] : NULL;
    if (!ev)
      continue;
    if (ev->start_func) {
      papi_errno = ev->start_func(ev);
      if (papi_errno != PAPI_OK)
        return papi_errno;
    }
  }
  ctx->state |= AMDS_EVENTS_RUNNING;
  return papi_errno;
}

int amds_ctx_stop(amds_ctx_t ctx) {
  if (!ctx) return PAPI_EINVAL;
  if (!(ctx->state & AMDS_EVENTS_RUNNING)) {
    return PAPI_OK;
  }
  int papi_errno = PAPI_OK;
  for (int i = 0; i < ctx->num_events; ++i) {
    native_event_t *ev = ctx->events ? ctx->events[i] : NULL;
    if (!ev)
      continue;
    if (ev->stop_func) {
      int papi_errno_stop = ev->stop_func(ev);
      if (papi_errno == PAPI_OK)
        papi_errno = papi_errno_stop;
    }
  }
  ctx->state &= ~AMDS_EVENTS_RUNNING;
  return papi_errno;
}

int amds_ctx_read(amds_ctx_t ctx, long long **counts) {
  if (!ctx || !counts) return PAPI_EINVAL;

  /* Always produce a fully defined buffer */
  for (int i = 0; i < ctx->num_events; ++i) {
    ctx->counters[i] = 0;  /* default if read fails */
  }

  /* Optional: track first error, but don't bail early */
  int papi_errno = PAPI_OK;

  for (int i = 0; i < ctx->num_events; ++i) {
    native_event_t *ev = ctx->events ? ctx->events[i] : NULL;
    if (!ev) {
      if (papi_errno == PAPI_OK) papi_errno = PAPI_EINVAL;
      continue;
    }

    int papi_errno_access = PAPI_OK;
    if (ev->access_func) {
      papi_errno_access = ev->access_func(PAPI_MODE_READ, ev);
    } else {
      papi_errno_access = PAPI_ECMP;
    }

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

  int papi_errno = PAPI_OK;
  for (int i = 0; i < ctx->num_events; ++i) {
    native_event_t *ev = ctx->events ? ctx->events[i] : NULL;
    if (!ev || !ev->access_func) return PAPI_ECMP;
    ev->value = counts[i];
    papi_errno = ev->access_func(PAPI_MODE_WRITE, ev);
    if (papi_errno != PAPI_OK) {
      return papi_errno;
    }
  }
  return papi_errno;
}

int amds_ctx_reset(amds_ctx_t ctx) {
  if (!ctx) return PAPI_EINVAL;

  for (int i = 0; i < ctx->num_events; ++i) {
    native_event_t *ev = ctx->events ? ctx->events[i] : NULL;
    if (!ev) return PAPI_EINVAL;
    ev->value = 0;
    if (ctx->counters) ctx->counters[i] = 0;
  }
  return PAPI_OK;
}
