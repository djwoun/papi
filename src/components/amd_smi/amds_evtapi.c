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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Event enumeration: iterate over native events */
int amds_evt_enum(unsigned int *EventCode, int modifier) {
  if (!EventCode) {
    return PAPI_EINVAL;
  }
  if (!ntv_table_p) {
    return PAPI_ECMP;
  }

  if (modifier == PAPI_ENUM_FIRST) {
    if (ntv_table_p->count == 0) {
      return PAPI_ENOEVNT;
    }
    *EventCode = 0;
    return PAPI_OK;
  } else if (modifier == PAPI_ENUM_EVENTS) {
    if (*EventCode >= (unsigned int)ntv_table_p->count) {
      return PAPI_ENOEVNT;
    }
    if (*EventCode + 1 < (unsigned int)ntv_table_p->count) {
      *EventCode = *EventCode + 1;
      return PAPI_OK;
    } else {
      return PAPI_ENOEVNT;
    }
  }
  return PAPI_EINVAL;
}

int amds_evt_code_to_name(unsigned int EventCode, char *name, int len) {
  if (!name || len <= 0) {
    return PAPI_EINVAL;
  }
  if (!ntv_table_p) {
    return PAPI_ECMP;
  }
  event_info_t info;
  uint64_t code = (uint64_t)EventCode;
  decode_event_code(code, &info);
  if (info.nameid < 0 || info.nameid >= ntv_table_p->count) {
    return PAPI_EINVAL;
  }
  native_event_t *event = &ntv_table_p->events[info.nameid];
  if ((info.flags & DEVICE_FLAG) && event->device < 0) {
    snprintf(name, (size_t)len, "%s:device=%d", event->name, info.device);
  } else {
    snprintf(name, (size_t)len, "%s", event->name);
  }
  return PAPI_OK;
}

int amds_evt_name_to_code(const char *name, unsigned int *EventCode) {
  if (!name || !EventCode) {
    return PAPI_EINVAL;
  }
  if (!htable) {
    return PAPI_ECMP;
  }
  native_event_t *event = NULL;
  int hret = htable_find(htable, name, (void **)&event);
  if (hret == HTABLE_SUCCESS) {
    if ((uint64_t)event->id > NAMEID_MASK) {
      return PAPI_EINVAL;
    }
    event_info_t info;
    info.device = 0;
    info.flags = 0;
    info.nameid = (int)event->id;
    uint64_t code64 = encode_event_code(&info);
    *EventCode = (unsigned int)code64;
    return PAPI_OK;
  }

  if (hret != HTABLE_ENOVAL) {
    return PAPI_ECMP;
  }

  const char *pos = strstr(name, ":device=");
  if (!pos) {
    return PAPI_ENOEVNT;
  }

  char *endptr = NULL;
  long dev_no = strtol(pos + 8, &endptr, 10);
  if (endptr == pos + 8 || (*endptr != '\0' && *endptr != ':')) {
    return PAPI_ENOEVNT;
  }
  if (dev_no < 0) {
    return PAPI_ENOEVNT;
  }
  int dev = (int)dev_no;

  char base_name[PAPI_MAX_STR_LEN];
  size_t prefix_len = (size_t)(pos - name);
  if (prefix_len >= sizeof(base_name)) {
    return PAPI_EINVAL;
  }
  memcpy(base_name, name, prefix_len);
  base_name[prefix_len] = '\0';
  if (*endptr == ':') {
    size_t remaining = strlen(endptr);
    if (prefix_len + remaining >= sizeof(base_name)) {
      return PAPI_EINVAL;
    }
    strncat(base_name, endptr, sizeof(base_name) - strlen(base_name) - 1);
  }

  hret = htable_find(htable, base_name, (void **)&event);
  if (hret != HTABLE_SUCCESS) {
    return PAPI_ENOEVNT;
  }
  if (event->device >= 0) {
    return PAPI_ENOEVNT;
  }
  if ((uint64_t)event->id > NAMEID_MASK) {
    return PAPI_EINVAL;
  }
  event_info_t info;
  info.device = dev;
  info.flags = DEVICE_FLAG;
  info.nameid = (int)event->id;
  uint64_t code64 = encode_event_code(&info);
  *EventCode = (unsigned int)code64;
  return PAPI_OK;
}

int amds_evt_code_to_descr(unsigned int EventCode, char *descr, int len) {
  if (!descr || len <= 0) {
    return PAPI_EINVAL;
  }
  if (!ntv_table_p) {
    return PAPI_ECMP;
  }
  event_info_t info;
  uint64_t code = (uint64_t)EventCode;
  decode_event_code(code, &info);
  if (info.nameid < 0 || info.nameid >= ntv_table_p->count) {
    return PAPI_EINVAL;
  }
  native_event_t *event = &ntv_table_p->events[info.nameid];
  snprintf(descr, (size_t)len, "%s", event->descr);
  return PAPI_OK;
}
