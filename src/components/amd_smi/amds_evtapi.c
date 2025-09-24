/**
 * @file    amds_evtapi.c
 */

#include "amds.h"
#include "amds_priv.h"
#include "htable.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Event ID bit layout */
#define EVENTS_WIDTH (sizeof(uint64_t) * 8)
#define DEVICE_WIDTH (7)
#define NAMEID_WIDTH (12)
#define QLMASK_WIDTH (2)
#define UNUSED_WIDTH (EVENTS_WIDTH - DEVICE_WIDTH - NAMEID_WIDTH - QLMASK_WIDTH)
#define DEVICE_SHIFT (EVENTS_WIDTH - UNUSED_WIDTH - DEVICE_WIDTH)
#define QLMASK_SHIFT (DEVICE_SHIFT - QLMASK_WIDTH)
#define NAMEID_SHIFT (QLMASK_SHIFT - NAMEID_WIDTH)
#define DEVICE_MASK  ((UINT64_MAX >> (EVENTS_WIDTH - DEVICE_WIDTH)) << DEVICE_SHIFT)
#define QLMASK_MASK  ((UINT64_MAX >> (EVENTS_WIDTH - QLMASK_WIDTH)) << QLMASK_SHIFT)
#define NAMEID_MASK  ((UINT64_MAX >> (EVENTS_WIDTH - NAMEID_WIDTH)) << NAMEID_SHIFT)
#define DEVICE_FLAG  (0x2)

typedef struct {
  int device;
  int flags;
  int nameid;
} amds_evtinfo_t;

/* helpers for umask walking (device qualifiers) */
static inline int first_set_bit_u64(uint64_t map) {
  for (int d = 0; d < device_count && d < 64; ++d) {
    if (map & (UINT64_C(1) << d)) {
      return d;
    }
  }
  return -1;
}

static int evt_name_to_basename(const char *name, char *base, int len);
static int evt_name_to_device(const char *name, int *device, int *has_device);
static void evt_build_device_list(uint64_t device_map, char *buffer, size_t len);
static int amds_evt_id_create(const amds_evtinfo_t *info, uint64_t *event_id);
static int amds_evt_id_to_info(uint64_t event_id, amds_evtinfo_t *info);

static int amds_evt_id_create(const amds_evtinfo_t *info, uint64_t *event_id) {
  if (!info || !event_id) {
    return PAPI_EINVAL;
  }

  if ((info->flags & ~DEVICE_FLAG) != 0) {
    return PAPI_EINVAL;
  }

  uint64_t id = ((uint64_t)info->nameid << NAMEID_SHIFT) & NAMEID_MASK;
  id |= ((uint64_t)info->flags << QLMASK_SHIFT) & QLMASK_MASK;

  if (info->flags & DEVICE_FLAG) {
    if (info->device < 0 || info->device >= (1 << DEVICE_WIDTH)) {
      return PAPI_EINVAL;
    }
    id |= ((uint64_t)info->device << DEVICE_SHIFT) & DEVICE_MASK;
  }

  *event_id = id;
  return PAPI_OK;
}

static int amds_evt_id_to_info(uint64_t event_id, amds_evtinfo_t *info) {
  if (!info) {
    return PAPI_EINVAL;
  }
  if (!ntv_table_p) {
    return PAPI_ECMP;
  }

  info->device = (int)((event_id & DEVICE_MASK) >> DEVICE_SHIFT);
  info->flags = (int)((event_id & QLMASK_MASK) >> QLMASK_SHIFT);
  info->nameid = (int)((event_id & NAMEID_MASK) >> NAMEID_SHIFT);

  if (info->nameid < 0 || info->nameid >= ntv_table_p->count) {
    return PAPI_ENOEVNT;
  }

  if ((info->flags & ~DEVICE_FLAG) != 0) {
    return PAPI_ENOEVNT;
  }

  native_event_t *event = &ntv_table_p->events[info->nameid];

  if (info->flags & DEVICE_FLAG) {
    if (event->device_map == 0) {
      return PAPI_ENOEVNT;
    }
    if (info->device < 0 || info->device >= device_count || info->device >= 64) {
      return PAPI_ENOEVNT;
    }
    if ((event->device_map & (UINT64_C(1) << info->device)) == 0) {
      return PAPI_ENOEVNT;
    }
  } else {
    if ((event_id & DEVICE_MASK) != 0) {
      return PAPI_ENOEVNT;
    }
    info->device = -1;
  }

  return PAPI_OK;
}

int amds_evt_decode(uint64_t event_id, int *nameid, int *device, int *has_device) {
  amds_evtinfo_t info;
  int rc = amds_evt_id_to_info(event_id, &info);
  if (rc != PAPI_OK) {
    return rc;
  }
  if (nameid) {
    *nameid = info.nameid;
  }
  if (device) {
    *device = (info.flags & DEVICE_FLAG) ? info.device : -1;
  }
  if (has_device) {
    *has_device = (info.flags & DEVICE_FLAG) ? 1 : 0;
  }
  return PAPI_OK;
}

int amds_evt_enum(uint64_t *EventCode, int modifier)
{
  if (!EventCode) return PAPI_EINVAL;
  if (!ntv_table_p) return PAPI_ECMP;

  amds_evtinfo_t info;

  switch (modifier) {
  case PAPI_ENUM_FIRST: {
    if (ntv_table_p->count == 0)
      return PAPI_ENOEVNT;

    info.nameid = 0;
    info.flags  = 0;
    info.device = 0;
    return amds_evt_id_create(&info, EventCode);
  }

  case PAPI_ENUM_EVENTS: {
    int rc = amds_evt_id_to_info(*EventCode, &info);
    if (rc != PAPI_OK) return rc;

    int count = ntv_table_p->count;
    if (info.nameid + 1 < count) {
      info.nameid++;
      info.flags  = 0;
      info.device = 0;
      return amds_evt_id_create(&info, EventCode);
    }
    return PAPI_ENOEVNT;
  }

  case PAPI_NTV_ENUM_UMASKS: {
    int rc = amds_evt_id_to_info(*EventCode, &info);
    if (rc != PAPI_OK) return rc;

    native_event_t *event = &ntv_table_p->events[info.nameid];
    if (info.flags == 0) {
      if (event->device_map == 0)
        return PAPI_ENOEVNT;

      int d0 = first_set_bit_u64(event->device_map);
      if (d0 < 0)
        return PAPI_ENOEVNT;

      info.flags  = DEVICE_FLAG;
      info.device = d0;
      return amds_evt_id_create(&info, EventCode);
    }

    return PAPI_ENOEVNT;
  }

  default:
    return PAPI_EINVAL;
  }
}

int amds_evt_code_to_name(uint64_t EventCode, char *name, int len) {
  if (!name || len <= 0) {
    return PAPI_EINVAL;
  }
  if (!ntv_table_p) {
    return PAPI_ECMP;
  }

  amds_evtinfo_t info;
  int papi_errno = amds_evt_id_to_info(EventCode, &info);
  if (papi_errno != PAPI_OK) {
    return papi_errno;
  }

  native_event_t *event = &ntv_table_p->events[info.nameid];
  if (info.flags & DEVICE_FLAG) {
    snprintf(name, (size_t)len, "%s:device=%d", event->name, info.device);
  } else {
    snprintf(name, (size_t)len, "%s", event->name);
  }
  return PAPI_OK;
}

int amds_evt_name_to_code(const char *name, uint64_t *EventCode) {
  if (!name || !EventCode) {
    return PAPI_EINVAL;
  }
  if (!htable || !ntv_table_p) {
    return PAPI_ECMP;
  }

  int device = -1;
  int has_device = 0;
  int papi_errno = evt_name_to_device(name, &device, &has_device);
  if (papi_errno != PAPI_OK) {
    return papi_errno;
  }

  char base[PAPI_MAX_STR_LEN];
  papi_errno = evt_name_to_basename(name, base, PAPI_MAX_STR_LEN);
  if (papi_errno != PAPI_OK) {
    return papi_errno;
  }

  native_event_t *event = NULL;
  int hret = htable_find(htable, base, (void **)&event);
  if (hret != HTABLE_SUCCESS) {
    return (hret == HTABLE_ENOVAL) ? PAPI_ENOEVNT : PAPI_ECMP;
  }

  amds_evtinfo_t info = { .device = 0, .flags = 0, .nameid = (int)event->id };
  if (event->device_map != 0) {
    if (!has_device) {
      return PAPI_EINVAL;
    }
    if (device < 0 || device >= device_count || device >= 64) {
      return PAPI_EINVAL;
    }
    if ((event->device_map & (UINT64_C(1) << device)) == 0) {
      return PAPI_ENOEVNT;
    }
    info.flags = DEVICE_FLAG;
    info.device = device;
  } else if (has_device) {
    return PAPI_EINVAL;
  }

  return amds_evt_id_create(&info, EventCode);
}

int amds_evt_code_to_descr(uint64_t EventCode, char *descr, int len) {
  if (!descr || len <= 0) {
    return PAPI_EINVAL;
  }
  PAPI_event_info_t info;
  memset(&info, 0, sizeof(info));
  int papi_errno = amds_evt_code_to_info(EventCode, &info);
  if (papi_errno != PAPI_OK) {
    return papi_errno;
  }
  snprintf(descr, (size_t)len, "%s", info.long_descr);
  return PAPI_OK;
}

int amds_evt_code_to_info(uint64_t EventCode, PAPI_event_info_t *info) {
  if (!info) {
    return PAPI_EINVAL;
  }
  if (!ntv_table_p) {
    return PAPI_ECMP;
  }

  amds_evtinfo_t inf;
  int papi_errno = amds_evt_id_to_info(EventCode, &inf);
  if (papi_errno != PAPI_OK) {
    return papi_errno;
  }

  native_event_t *event = &ntv_table_p->events[inf.nameid];
  if (inf.flags & DEVICE_FLAG) {
    char devices[PAPI_MAX_STR_LEN] = {0};
    evt_build_device_list(event->device_map, devices, sizeof(devices));
    snprintf(info->symbol, sizeof(info->symbol), "%s:device=%d", event->name, inf.device);
    snprintf(info->long_descr, sizeof(info->long_descr),
             "%s\nmasks:Mandatory device qualifier [%s]",
             event->descr, devices);
  } else {
    snprintf(info->symbol, sizeof(info->symbol), "%s", event->name);
    snprintf(info->long_descr, sizeof(info->long_descr), "%s", event->descr);
  }
  return PAPI_OK;
}

static int evt_name_to_basename(const char *name, char *base, int len) {
  if (!name || !base || len <= 0) {
    return PAPI_EINVAL;
  }
  size_t j = 0;
  for (size_t i = 0; name[i] != '\0' && j < (size_t)(len - 1);) {
    if (strncmp(&name[i], ":device=", 8) == 0) {
      i += 8;
      while (name[i] != '\0' && name[i] != ':') {
        ++i;
      }
      continue;
    }
    base[j++] = name[i++];
  }
  if (j >= (size_t)len) {
    return PAPI_EINVAL;
  }
  base[j] = '\0';
  return PAPI_OK;
}

static int evt_name_to_device(const char *name, int *device, int *has_device) {
  if (!device || !has_device) {
    return PAPI_EINVAL;
  }
  *has_device = 0;
  *device = -1;

  const char *match = strstr(name, ":device=");
  if (!match) {
    return PAPI_OK;
  }

  const char *next = strstr(match + 1, ":device=");
  if (next) {
    return PAPI_EINVAL;
  }

  match += 8;
  char *endptr = NULL;
  long value = strtol(match, &endptr, 10);
  if (endptr == match) {
    return PAPI_EINVAL;
  }
  if (*endptr != '\0' && *endptr != ':') {
    return PAPI_EINVAL;
  }
  if (value < 0 || value >= (1 << DEVICE_WIDTH)) {
    return PAPI_EINVAL;
  }

  *device = (int)value;
  *has_device = 1;
  return PAPI_OK;
}

static void evt_build_device_list(uint64_t device_map, char *buffer, size_t len) {
  if (!buffer || len == 0) return;
  buffer[0] = '\0';
  size_t used = 0;
  for (int d = 0; d < device_count && d < 64; ++d) {
    if (!(device_map & (UINT64_C(1) << d))) continue;
    int n = snprintf(buffer + used, (len - used), (used ? ",%d" : "%d"), d);
    if (n < 0 || n >= (int)(len - used)) {
      buffer[len - 1] = '\0';
      return;
    }
    used += (size_t) n;
  }
}
