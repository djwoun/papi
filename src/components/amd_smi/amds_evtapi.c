/**
 * @file    amds_evtapi.c
 * @author  Dong Jun Woun 
 *          djwoun@gmail.com
 *
 */

#include "amds.h"
#include "amds_priv.h"
#include "htable.h"
#include "papi.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Event enumeration: iterate over native events.
 * We return an encoded identifier (stored in the unsigned int EventCode)
 * that carries: device (6 bits), qualifier mask (1 bit), and nameid bits.
 * nameid remains the table index (compat with amd_smi’s per-device entries).
 */
int amds_evt_enum(unsigned int *EventCode, int modifier) {
  if (!EventCode) return PAPI_EINVAL;
  if (!ntv_table_p) return PAPI_ECMP;

  if (modifier == PAPI_ENUM_FIRST) {
    if (ntv_table_p->count == 0) return PAPI_ENOEVNT;
    int nameid = 0;
    native_event_t *ev = &ntv_table_p->events[nameid];
    int dev   = (ev->device >= 0 ? ev->device : 0);
    int flags = (ev->device >= 0 ? AMDS_DEVICE_FLAG : 0);
    uint64_t code = (((uint64_t)dev)   << AMDS_DEVICE_SHIFT) |
                    (((uint64_t)flags) << AMDS_QLMASK_SHIFT)  |
                    (((uint64_t)nameid)<< AMDS_NAMEID_SHIFT);
    *EventCode = (unsigned int)code;
    return PAPI_OK;
  }
  else if (modifier == PAPI_ENUM_EVENTS) {
    uint64_t prev = (uint64_t)(*EventCode);
    int nameid = (int)((prev & AMDS_NAMEID_MASK) >> AMDS_NAMEID_SHIFT);
    if (nameid + 1 < ntv_table_p->count) {
      int nid = nameid + 1;
      native_event_t *ev = &ntv_table_p->events[nid];
      int dev   = (ev->device >= 0 ? ev->device : 0);
      int flags = (ev->device >= 0 ? AMDS_DEVICE_FLAG : 0);
      uint64_t code = (((uint64_t)dev)   << AMDS_DEVICE_SHIFT) |
                      (((uint64_t)flags) << AMDS_QLMASK_SHIFT)  |
                      (((uint64_t)nid)   << AMDS_NAMEID_SHIFT);
      *EventCode = (unsigned int)code;
      return PAPI_OK;
    }
    return PAPI_ENOEVNT;
  }
  else if (modifier == PAPI_NTV_ENUM_UMASKS) {
    /* We have a single qualifier: :device=<id>. If the provided code has no
     * qualifier mask yet, return the masked version; otherwise we’re done. */
    uint64_t prev  = (uint64_t)(*EventCode);
    int flags = (int)((prev & AMDS_QLMASK_MASK) >> AMDS_QLMASK_SHIFT);
    if ((flags & AMDS_DEVICE_FLAG) == 0) {
      int nameid = (int)((prev & AMDS_NAMEID_MASK) >> AMDS_NAMEID_SHIFT);
      if (nameid < 0 || nameid >= ntv_table_p->count) return PAPI_ENOEVNT;
      native_event_t *ev = &ntv_table_p->events[nameid];
      int dev = (ev->device >= 0 ? ev->device : 0);
      uint64_t code = (((uint64_t)dev) << AMDS_DEVICE_SHIFT) |
                      (((uint64_t)AMDS_DEVICE_FLAG) << AMDS_QLMASK_SHIFT) |
                      (((uint64_t)nameid) << AMDS_NAMEID_SHIFT);
      *EventCode = (unsigned int)code;
      return PAPI_OK;
    }
    return PAPI_ENOEVNT;
  }
  return PAPI_EINVAL;
}

int amds_evt_code_to_name(unsigned int EventCode, char *name, int len) {
  if (!name || len <= 0) return PAPI_EINVAL;
  if (!ntv_table_p) return PAPI_ECMP;
  uint64_t code = (uint64_t)EventCode;
  int nameid = (int)((code & AMDS_NAMEID_MASK) >> AMDS_NAMEID_SHIFT);
  if (nameid < 0 || nameid >= ntv_table_p->count) return PAPI_EINVAL;
  snprintf(name, (size_t)len, "%s", ntv_table_p->events[nameid].name);
  return PAPI_OK;
}

int amds_evt_name_to_code(const char *name, unsigned int *EventCode) {
  if (!name || !EventCode) return PAPI_EINVAL;
  if (!htable || !ntv_table_p) return PAPI_ECMP;

  native_event_t *event = NULL;
  int hret = htable_find(htable, name, (void **)&event);
  if (hret != HTABLE_SUCCESS) {
    return (hret == HTABLE_ENOVAL) ? PAPI_ENOEVNT : PAPI_ECMP;
  }

  /* Mandatory device qualifier: :device=<id> */
  const char *q = strstr(name, ":device=");
  if (!q) {
    return PAPI_ENOEVNT;
  }
  q += strlen(":device=");
  char *endp = NULL;
  long dev = strtol(q, &endp, 10);
  if (endp == q || (endp && *endp != '\0')) {
    return PAPI_ENOEVNT;
  }
  if (dev < 0 || dev >= amds_get_device_count() || dev >= 64) {
    return PAPI_ENOEVNT;
  }
  if ((int32_t)dev != event->device) {
    /* Named device must match the event table’s device */
    return PAPI_ENOEVNT;
  }

  int nameid = (int)event->id;
  uint64_t code = (((uint64_t)dev) << AMDS_DEVICE_SHIFT) |
                  (((uint64_t)AMDS_DEVICE_FLAG) << AMDS_QLMASK_SHIFT) |
                  (((uint64_t)nameid) << AMDS_NAMEID_SHIFT);
  *EventCode = (unsigned int)code;
  return PAPI_OK;
}

int amds_evt_code_to_descr(unsigned int EventCode, char *descr, int len) {
  if (!descr || len <= 0) return PAPI_EINVAL;
  if (!ntv_table_p) return PAPI_ECMP;
  uint64_t code = (uint64_t)EventCode;
  int nameid = (int)((code & AMDS_NAMEID_MASK) >> AMDS_NAMEID_SHIFT);
  if (nameid < 0 || nameid >= ntv_table_p->count) return PAPI_EINVAL;
  snprintf(descr, (size_t)len, "%s", ntv_table_p->events[nameid].descr);
  return PAPI_OK;
}
