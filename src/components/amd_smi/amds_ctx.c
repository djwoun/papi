#include "amds.h"
#include "amds_priv.h"
#include "papi.h"
#include "papi_memory.h"
static int acquire_devices(unsigned int *events_id, int num_events, int32_t *bitmask) {
  int32_t mask_acq = 0;
  for (int i = 0; i < num_events; ++i) {
    int32_t dev_id = ntv_table_p->events[events_id[i]].device;
    if (dev_id < 0)
      continue;
    mask_acq |= (1 << dev_id);
  }
  if (mask_acq & device_mask) {
    return PAPI_ECNFLCT; // conflict: device already in use
  }
  device_mask |= mask_acq;
  *bitmask = mask_acq;
  return PAPI_OK;
}
static int release_devices(int32_t *bitmask) {
  int32_t mask_rel = *bitmask;
  if ((mask_rel & device_mask) != mask_rel) {
    return PAPI_EMISC;
  }
  device_mask ^= mask_rel;
  *bitmask = 0;
  return PAPI_OK;
}

/* Context management: open/close, start/stop, read/write, reset */
struct amds_ctx {
  int state;
  unsigned int *events_id;
  int num_events;
  long long *counters;
  int32_t device_mask;
};
int amds_ctx_open(unsigned int *event_ids, int num_events, amds_ctx_t *ctx) {
  amds_ctx_t new_ctx = (amds_ctx_t)papi_calloc(1, sizeof(struct amds_ctx));
  if (new_ctx == NULL) {
    return PAPI_ENOMEM;
  }
  new_ctx->events_id = event_ids; // Store pointer (original approach)
  new_ctx->num_events = num_events;
  new_ctx->counters = (long long *)papi_calloc(num_events, sizeof(long long));
  if (new_ctx->counters == NULL) {
    papi_free(new_ctx);
    return PAPI_ENOMEM;
  }
  // Acquire devices needed by these events to avoid conflicts
  int papi_errno = acquire_devices(event_ids, num_events, &new_ctx->device_mask);
  if (papi_errno != PAPI_OK) {
    papi_free(new_ctx->counters);
    papi_free(new_ctx);
    return papi_errno;
  }
  *ctx = new_ctx;
  return PAPI_OK;
}
int amds_ctx_close(amds_ctx_t ctx) {
  if (!ctx)
    return PAPI_OK;
  // release device usage
  release_devices(&ctx->device_mask);
  papi_free(ctx->counters);
  papi_free(ctx);
  return PAPI_OK;
}
int amds_ctx_start(amds_ctx_t ctx) {
  // No additional actions needed to start in this design (all reads are
  // on-demand)
  ctx->state |= AMDS_EVENTS_RUNNING;
  return PAPI_OK;
}
int amds_ctx_stop(amds_ctx_t ctx) {
  if (!(ctx->state & AMDS_EVENTS_RUNNING)) {
    return PAPI_OK;
  }
  ctx->state &= ~AMDS_EVENTS_RUNNING;
  return PAPI_OK;
}
int amds_ctx_read(amds_ctx_t ctx, long long **counts) {
  int papi_errno = PAPI_OK;
  for (int i = 0; i < ctx->num_events; ++i) {
    unsigned int id = ctx->events_id[i];
    papi_errno = ntv_table_p->events[id].access_func(PAPI_MODE_READ, &ntv_table_p->events[id]);
    if (papi_errno != PAPI_OK) {
      return papi_errno;
    }
    ctx->counters[i] = (long long)ntv_table_p->events[id].value;
  }
  *counts = ctx->counters;
  return papi_errno;
}
int amds_ctx_write(amds_ctx_t ctx, long long *counts) {
  int papi_errno = PAPI_OK;
  for (int i = 0; i < ctx->num_events; ++i) {
    unsigned int id = ctx->events_id[i];
    ntv_table_p->events[id].value = counts[i];
    papi_errno = ntv_table_p->events[id].access_func(PAPI_MODE_WRITE, &ntv_table_p->events[id]);
    if (papi_errno != PAPI_OK) {
      return papi_errno;
    }
  }
  return papi_errno;
}
int amds_ctx_reset(amds_ctx_t ctx) {
  for (int i = 0; i < ctx->num_events; ++i) {
    unsigned int id = ctx->events_id[i];
    ntv_table_p->events[id].value = 0;
    ctx->counters[i] = 0;
  }
  return PAPI_OK;
}
