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
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "htable.h"
extern int amds_strip_device_qualifier_(const char *name, char *base, int len);

unsigned int _amd_smi_lock;

/* Global device reservation mask (up to 64 devices) */
static uint64_t g_device_mask = 0;

static int amds_ctx_event_from_code(unsigned int code, native_event_t **out);

static int acquire_devices(unsigned int *event_ids, int num_events, uint64_t *bitmask_out)
{
  if (!bitmask_out) return PAPI_EINVAL;
  if (num_events < 0) return PAPI_EINVAL;
  if (num_events > 0 && !event_ids) return PAPI_EINVAL;

  uint64_t need = 0;
  for (int i = 0; i < num_events; ++i) {
    native_event_t *ev = NULL;
    int ret = amds_ctx_event_from_code(event_ids[i], &ev);
    if (ret != PAPI_OK) return ret;
    int dev_id = ev->device;
    if (dev_id < 0) continue;
    if (dev_id >= 64) return PAPI_EINVAL;
    need |= (1ULL << dev_id);
  }

  _papi_hwi_lock(_amd_smi_lock);
  if (g_device_mask & need) {
    _papi_hwi_unlock(_amd_smi_lock);
    return PAPI_ECNFLCT;
  }
  g_device_mask |= need;
  _papi_hwi_unlock(_amd_smi_lock);

  *bitmask_out = need;
  return PAPI_OK;
}

static int release_devices(uint64_t *bitmask_inout)
{
  if (!bitmask_inout) return PAPI_EINVAL;
  uint64_t mask = *bitmask_inout;
  _papi_hwi_lock(_amd_smi_lock);
  g_device_mask &= ~mask;
  _papi_hwi_unlock(_amd_smi_lock);
  *bitmask_inout = 0;
  return PAPI_OK;
}

static int amds_ctx_event_from_code(unsigned int code, native_event_t **out)
{
  if (!out) return PAPI_EINVAL;
  native_event_table_t *ntv = amds_get_ntv_table();
  if (!ntv) return PAPI_ECMP;

  int device  = (int)((code & AMDS_DEVICE_MASK) >> AMDS_DEVICE_SHIFT);
  int flags   = (int)((code & AMDS_QLMASK_MASK) >> AMDS_QLMASK_SHIFT);
  int nameid  = (int)((code & AMDS_NAMEID_MASK) >> AMDS_NAMEID_SHIFT);

  if (nameid < 0 || nameid >= ntv->count) return PAPI_ENOEVNT;
  if (device < 0 || device >= 64 || device >= amds_get_device_count()) return PAPI_ENOEVNT;
  if ((flags & AMDS_DEVICE_FLAG) == 0 && device > 0) return PAPI_ENOEVNT;

  char base[PAPI_MAX_STR_LEN] = {0};
  if (amds_strip_device_qualifier_(ntv->events[nameid].name, base, (int)sizeof(base)) != PAPI_OK)
    return PAPI_ENOEVNT;
  char full[PAPI_MAX_STR_LEN] = {0};
  snprintf(full, sizeof(full), "%s:device=%d", base, device);

  native_event_t *ev = NULL;
  void *htable_ptr = amds_get_htable();
  if (htable_ptr && htable_find(htable_ptr, full, (void **)&ev) == HTABLE_SUCCESS && ev) {
    *out = ev;
    return PAPI_OK;
  }
  for (int i = 0; i < ntv->count; ++i) {
    if (strcmp(ntv->events[i].name, full) == 0) {
      *out = &ntv->events[i];
      return PAPI_OK;
    }
  }
  return PAPI_ENOEVNT;
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

  /* Validate event codes: resolve them early */
  for (int i = 0; i < num_events; ++i) {
    native_event_t *tmp = NULL;
    if (amds_ctx_event_from_code(event_ids[i], &tmp) != PAPI_OK) {
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
    native_event_t *ev = NULL;
    if (amds_ctx_event_from_code(event_ids[i], &ev) != PAPI_OK) {
      release_devices(&new_ctx->device_mask);
      papi_free(new_ctx->counters);
      papi_free(new_ctx);
      return PAPI_EINVAL;
    }
    if (ev->open_func) {
      papi_errno = ev->open_func(ev);
      if (papi_errno != PAPI_OK) {
        for (int j = 0; j < i; ++j) {
          native_event_t *prev = NULL;
          amds_ctx_event_from_code(event_ids[j], &prev);
          if (prev && prev->close_func)
            prev->close_func(prev);
        }
        release_devices(&new_ctx->device_mask);
        papi_free(new_ctx->counters);
        papi_free(new_ctx);
        return papi_errno;
      }
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
    native_event_t *ev = NULL;
    if (amds_ctx_event_from_code(ctx->events_id[i], &ev) != PAPI_OK)
      continue;
    if (ev->close_func)
      ev->close_func(ev);
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
    native_event_t *ev = NULL;
    if (amds_ctx_event_from_code(ctx->events_id[i], &ev) != PAPI_OK)
      return PAPI_EINVAL;
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
  if (!ntv_table_p) return PAPI_ECMP;

  int papi_errno = PAPI_OK;
  for (int i = 0; i < ctx->num_events; ++i) {
    native_event_t *ev = NULL;
    if (amds_ctx_event_from_code(ctx->events_id[i], &ev) != PAPI_OK)
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
  if (!ntv_table_p) return PAPI_ECMP;

  /* Always produce a fully defined buffer */
  for (int i = 0; i < ctx->num_events; ++i) {
    ctx->counters[i] = 0;  /* default if read fails */
  }

  /* Optional: track first error, but don't bail early */
  int papi_errno = PAPI_OK;

  for (int i = 0; i < ctx->num_events; ++i) {
    native_event_t *ev = NULL;
    if (amds_ctx_event_from_code(ctx->events_id[i], &ev) != PAPI_OK) {
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
  if (!ntv_table_p) return PAPI_ECMP;

  int papi_errno = PAPI_OK;
  for (int i = 0; i < ctx->num_events; ++i) {
    native_event_t *ev = NULL;
    if (amds_ctx_event_from_code(ctx->events_id[i], &ev) != PAPI_OK) return PAPI_EINVAL;
    if (!ev->access_func) return PAPI_ECMP;
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
  if (!ntv_table_p) return PAPI_ECMP;

  for (int i = 0; i < ctx->num_events; ++i) {
    native_event_t *ev = NULL;
    if (amds_ctx_event_from_code(ctx->events_id[i], &ev) != PAPI_OK) return PAPI_EINVAL;
    ev->value = 0;
    if (ctx->counters) ctx->counters[i] = 0;
  }
  return PAPI_OK;
}
