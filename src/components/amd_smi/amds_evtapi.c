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
#include "papi_vector.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern papi_vector_t _amd_smi_vector;

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
  uint32_t base = EventCode & AMDS_DEVICE_MASK;
  if (base >= (unsigned int)ntv_table_p->count) {
    return PAPI_EINVAL;
  }
  snprintf(name, (size_t)len, "%s", ntv_table_p->events[base].name);
  return PAPI_OK;
}

int amds_evt_name_to_code(const char *name, unsigned int *EventCode) {
  if (!name || !EventCode) {
    return PAPI_EINVAL;
  }
  if (!htable || !ntv_table_p) {
    return PAPI_ECMP;
  }

  char base_name[PAPI_MAX_STR_LEN];
  snprintf(base_name, sizeof(base_name), "%s", name);

  int device = -1;
  const char *dev_str = strstr(name, ":device=");
  if (dev_str) {
    device = atoi(dev_str + 8);
  }
  char *base_dev = strstr(base_name, ":device=");
  if (base_dev) {
    char *after = strchr(base_dev + 1, ':');
    if (after) {
      memmove(base_dev, after, strlen(after) + 1);
    } else {
      *base_dev = '\0';
    }
  }

  native_event_t *event = NULL;
  int hret = htable_find(htable, base_name, (void **)&event);
  if (hret != HTABLE_SUCCESS || !event) {
    return PAPI_ENOEVNT;
  }

  if ((unsigned int)event->id > AMDS_DEVICE_MASK) {
    return PAPI_ECMP;
  }

  unsigned int code = (unsigned int)event->id;
  if (event->devices) {
    if (device < 0) {
      device = event->device;
    }
    if (device < 0 || device >= 64 || !(event->devices & (UINT64_C(1) << device))) {
      return PAPI_ENOEVNT;
    }
    code |= ((unsigned int)device << AMDS_DEVICE_SHIFT);
  } else if (device >= 0) {
    return PAPI_ENOEVNT;
  }

  *EventCode = code;
  return PAPI_OK;
}

int amds_evt_code_to_descr(unsigned int EventCode, char *descr, int len) {
  if (!descr || len <= 0) {
    return PAPI_EINVAL;
  }
  if (!ntv_table_p) {
    return PAPI_ECMP;
  }
  uint32_t base = EventCode & AMDS_DEVICE_MASK;
  if (base >= (unsigned int)ntv_table_p->count) {
    return PAPI_EINVAL;
  }
  snprintf(descr, (size_t)len, "%s", ntv_table_p->events[base].descr);
  return PAPI_OK;
}

int amds_evt_code_to_info(unsigned int EventCode, PAPI_event_info_t *info) {
  if (!info) {
    return PAPI_EINVAL;
  }
  if (!ntv_table_p) {
    return PAPI_ECMP;
  }

  uint32_t base = EventCode & AMDS_DEVICE_MASK;
  if (base >= (unsigned int)ntv_table_p->count) {
    return PAPI_EINVAL;
  }

  native_event_t *ev = &ntv_table_p->events[base];

  memset(info, 0, sizeof(*info));
  info->event_code = EventCode;
  snprintf(info->symbol, sizeof(info->symbol), "%s:::%s",
           _amd_smi_vector.cmp_info.short_name, ev->name);
  snprintf(info->short_descr, sizeof(info->short_descr), "%s", ev->name);
  snprintf(info->long_descr, sizeof(info->long_descr), "%s", ev->descr);
  info->component_index = _amd_smi_vector.cmp_info.CmpIdx;
  info->unit_mul = 1;

  if (ev->devices) {
    info->num_quals = 1;
    unsigned int default_dev = 0;
    if (ev->device >= 0 && ev->device < 64 &&
        (ev->devices & (UINT64_C(1) << ev->device))) {
      default_dev = (unsigned int)ev->device;
    } else {
      for (int d = 0; d < 64; ++d) {
        if (ev->devices & (UINT64_C(1) << d)) {
          default_dev = (unsigned int)d;
          break;
        }
      }
    }
    snprintf(info->quals[0], sizeof(info->quals[0]), "device=%u", default_dev);

    char devlist[256];
    devlist[0] = '\0';
    size_t written = 0;
    bool first = true;
    for (int d = 0; d < 64; ++d) {
      if (ev->devices & (UINT64_C(1) << d)) {
        int ret = snprintf(devlist + written,
                           (written < sizeof(devlist)) ? sizeof(devlist) - written : 0,
                           "%s%d", first ? "" : ",", d);
        if (ret > 0 && written < sizeof(devlist)) {
          size_t add = (size_t)ret;
          if (add >= sizeof(devlist) - written) {
            written = sizeof(devlist) - 1;
          } else {
            written += add;
          }
        }
        first = false;
      }
    }
    devlist[sizeof(devlist) - 1] = '\0';
    snprintf(info->quals_descrs[0], sizeof(info->quals_descrs[0]),
             "mandatory device qualifier [devices: %s]", devlist);
  }

  return PAPI_OK;
}

