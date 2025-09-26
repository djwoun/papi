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

/*
 * Event identifier encoding format:
 * +------------------------------+-------+-----+------------------+
 * |           unused             | device| qmsk|      nameid      |
 * +------------------------------+-------+-----+------------------+
 *
 * unused : remaining bits of the unsigned int payload
 * device : 7-bit device selector ([0 - 127] devices)
 * qmsk   : 1-bit qualifier mask (device flag)
 * nameid : remaining bits index into native event table
 */
#define AMDS_EVENTCODE_WIDTH   (sizeof(unsigned int) * 8)
#define AMDS_DEVICE_WIDTH      7
#define AMDS_QMASK_WIDTH       1
#define AMDS_NAMEID_WIDTH                                                  \
  (AMDS_EVENTCODE_WIDTH - AMDS_DEVICE_WIDTH - AMDS_QMASK_WIDTH)
#define AMDS_DEVICE_SHIFT      (AMDS_EVENTCODE_WIDTH - AMDS_DEVICE_WIDTH)
#define AMDS_QMASK_SHIFT       (AMDS_DEVICE_SHIFT - AMDS_QMASK_WIDTH)
#define AMDS_NAMEID_SHIFT      (AMDS_QMASK_SHIFT - AMDS_NAMEID_WIDTH)
#define AMDS_DEVICE_MASK       (((1u << AMDS_DEVICE_WIDTH) - 1)               \
                                << AMDS_DEVICE_SHIFT)
#define AMDS_QMASK_MASK        (((1u << AMDS_QMASK_WIDTH) - 1)               \
                                << AMDS_QMASK_SHIFT)
#define AMDS_NAMEID_MASK       (((1u << AMDS_NAMEID_WIDTH) - 1)              \
                                << AMDS_NAMEID_SHIFT)

static int format_device_bitmap(uint64_t bitmap, char *buf, size_t len) {
  if (!buf || len == 0)
    return PAPI_EINVAL;
  buf[0] = '\0';

  size_t used = 0;
  int have = 0;
  int limit = device_count;
  if (limit <= 0 || limit > 64)
    limit = 64;
  int d;
  for (d = 0; d < limit; ++d) {
    if (!amds_dev_check(bitmap, d))
      continue;
    int written = snprintf(buf + used, len - used, "%s%d", have ? "," : "", d);
    if (written < 0 || (size_t)written >= len - used)
      return PAPI_EBUF;
    used += (size_t)written;
    have = 1;
  }
  return have ? PAPI_OK : PAPI_ENOEVNT;
}

static int device_bitmap_limit(void) {
  if (device_count > 0 && device_count < 64)
    return device_count;
  return 64;
}

static int device_first(uint64_t bitmap) {
  int limit = device_bitmap_limit();
  int d;
  for (d = 0; d < limit; ++d) {
    if (amds_dev_check(bitmap, d))
      return d;
  }
  return -1;
}

int amds_evt_id_create(amds_event_info_t *info, unsigned int *event_code) {
  if (!info || !event_code)
    return PAPI_EINVAL;

  unsigned int code = 0;
  unsigned int device_bits = (unsigned int)(info->device & ((1u << AMDS_DEVICE_WIDTH) - 1));
  unsigned int flag_bits = (unsigned int)(info->flags & ((1u << AMDS_QMASK_WIDTH) - 1));
  unsigned int name_bits = (unsigned int)(info->nameid & ((1u << AMDS_NAMEID_WIDTH) - 1));

  code |= (device_bits << AMDS_DEVICE_SHIFT);
  code |= (flag_bits << AMDS_QMASK_SHIFT);
  code |= (name_bits << AMDS_NAMEID_SHIFT);
  *event_code = code;
  return PAPI_OK;
}

int amds_evt_id_to_info(unsigned int event_code, amds_event_info_t *info) {
  if (!info)
    return PAPI_EINVAL;
  if (!ntv_table_p)
    return PAPI_ECMP;

  info->device = (int)((event_code & AMDS_DEVICE_MASK) >> AMDS_DEVICE_SHIFT);
  info->flags = (event_code & AMDS_QMASK_MASK) >> AMDS_QMASK_SHIFT;
  info->nameid = (int)((event_code & AMDS_NAMEID_MASK) >> AMDS_NAMEID_SHIFT);

  if (info->nameid < 0 || info->nameid >= ntv_table_p->count)
    return PAPI_ENOEVNT;

  native_event_t *event = &ntv_table_p->events[info->nameid];
  if (info->flags & AMDS_DEVICE_FLAG) {
    if (!event->device_map)
      return PAPI_ENOEVNT;
    int limit = device_bitmap_limit();
    if (info->device < 0 || info->device >= limit)
      return PAPI_ENOEVNT;
    if (!amds_dev_check(event->device_map, info->device))
      return PAPI_ENOEVNT;
  } else {
    info->device = 0;
  }

  return PAPI_OK;
}

int amds_evt_enum(unsigned int *EventCode, int modifier) {
  if (!EventCode)
    return PAPI_EINVAL;
  if (!ntv_table_p)
    return PAPI_ECMP;

  amds_event_info_t info = { .device = 0, .flags = 0, .nameid = 0 };
  int papi_errno = PAPI_OK;

  switch (modifier) {
  case PAPI_ENUM_FIRST:
    if (ntv_table_p->count == 0)
      return PAPI_ENOEVNT;
    return amds_evt_id_create(&info, EventCode);
  case PAPI_ENUM_EVENTS:
    papi_errno = amds_evt_id_to_info(*EventCode, &info);
    if (papi_errno != PAPI_OK)
      return papi_errno;
    if (info.nameid + 1 >= ntv_table_p->count)
      return PAPI_ENOEVNT;
    info.nameid++;
    info.device = 0;
    info.flags = 0;
    return amds_evt_id_create(&info, EventCode);
  case PAPI_NTV_ENUM_UMASKS:
    papi_errno = amds_evt_id_to_info(*EventCode, &info);
    if (papi_errno != PAPI_OK)
      return papi_errno;
    native_event_t *event = &ntv_table_p->events[info.nameid];
    if (!event->device_map)
      return PAPI_ENOEVNT;
    if (info.flags & AMDS_DEVICE_FLAG)
      return PAPI_ENOEVNT;
    int first = device_first(event->device_map);
    if (first < 0)
      return PAPI_ENOEVNT;
    info.flags |= AMDS_DEVICE_FLAG;
    info.device = first;
    return amds_evt_id_create(&info, EventCode);
  default:
    return PAPI_EINVAL;
  }
}

int amds_evt_code_to_name(unsigned int EventCode, char *name, int len) {
  if (!name || len <= 0)
    return PAPI_EINVAL;

  amds_event_info_t info;
  int papi_errno = amds_evt_id_to_info(EventCode, &info);
  if (papi_errno != PAPI_OK)
    return papi_errno;

  native_event_t *event = &ntv_table_p->events[info.nameid];
  if (info.flags & AMDS_DEVICE_FLAG)
    snprintf(name, (size_t)len, "%s:device=%d", event->name, info.device);
  else
    snprintf(name, (size_t)len, "%s", event->name);
  return PAPI_OK;
}

int amds_evt_name_to_code(const char *name, unsigned int *EventCode) {
  if (!name || !EventCode)
    return PAPI_EINVAL;
  if (!htable)
    return PAPI_ECMP;

  char working[PAPI_MAX_STR_LEN];
  size_t nlen = strlen(name);
  if (nlen >= sizeof(working))
    return PAPI_EBUF;
  strcpy(working, name);

  int requested_device = -1;
  char *cursor = working;
  char *device_pos = NULL;
  while ((device_pos = strstr(cursor, ":device=")) != NULL) {
    if (requested_device >= 0)
      return PAPI_ENOEVNT; // duplicate device qualifier
    char *value = device_pos + strlen(":device=");
    char *endptr = value;
    long dev = strtol(value, &endptr, 10);
    if (endptr == value)
      return PAPI_EINVAL;
    if (dev < 0 || dev >= 64)
      return PAPI_EINVAL;
    requested_device = (int)dev;
    if (*endptr == ':')
      memmove(device_pos, endptr, strlen(endptr) + 1);
    else if (*endptr == '\0')
      *device_pos = '\0';
    else
      return PAPI_EINVAL;
    cursor = device_pos;
  }

  native_event_t *event = NULL;
  int hret = htable_find(htable, working, (void **)&event);
  if (hret != HTABLE_SUCCESS)
    return (hret == HTABLE_ENOVAL) ? PAPI_ENOEVNT : PAPI_ECMP;

  amds_event_info_t info = { .device = 0, .flags = 0,
                             .nameid = (int)(event - ntv_table_p->events) };
  if (event->device_map) {
    info.flags |= AMDS_DEVICE_FLAG;
    if (requested_device >= 0) {
      if (!amds_dev_check(event->device_map, requested_device))
        return PAPI_ENOEVNT;
      info.device = requested_device;
    } else {
      int first = device_first(event->device_map);
      if (first < 0)
        return PAPI_ENOEVNT;
      info.device = first;
    }
  } else if (requested_device >= 0) {
    return PAPI_ENOEVNT;
  }

  return amds_evt_id_create(&info, EventCode);
}

int amds_evt_code_to_descr(unsigned int EventCode, char *descr, int len) {
  if (!descr || len <= 0)
    return PAPI_EINVAL;

  amds_event_info_t info;
  int papi_errno = amds_evt_id_to_info(EventCode, &info);
  if (papi_errno != PAPI_OK)
    return papi_errno;

  native_event_t *event = &ntv_table_p->events[info.nameid];
  snprintf(descr, (size_t)len, "%s", event->descr);
  return PAPI_OK;
}

int amds_evt_code_to_info(unsigned int EventCode, PAPI_event_info_t *info) {
  if (!info)
    return PAPI_EINVAL;

  amds_event_info_t code_info;
  int papi_errno = amds_evt_id_to_info(EventCode, &code_info);
  if (papi_errno != PAPI_OK)
    return papi_errno;

  native_event_t *event = &ntv_table_p->events[code_info.nameid];
  unsigned int device_flag = code_info.flags & AMDS_DEVICE_FLAG;

  switch (device_flag) {
    case 0:
      snprintf(info->symbol, sizeof(info->symbol), "%s", event->name);
      snprintf(info->long_descr, sizeof(info->long_descr), "%s", event->descr);
      snprintf(info->short_descr, sizeof(info->short_descr), "%s", event->descr);
      info->num_quals = event->device_map ? 1 : 0;
      break;
    case AMDS_DEVICE_FLAG: {
      char devices[PAPI_MAX_STR_LEN] = {0};
      papi_errno = format_device_bitmap(event->device_map, devices, sizeof(devices));
      if (papi_errno != PAPI_OK)
        return papi_errno;
      snprintf(info->symbol, sizeof(info->symbol), "%s:device=%d", event->name,
               code_info.device);
      snprintf(info->long_descr, sizeof(info->long_descr),
               "%s, masks:Mandatory device qualifier [%s]", event->descr,
               devices);
      snprintf(info->short_descr, sizeof(info->short_descr),
               "Mandatory device qualifier [%s]", devices);
      info->num_quals = 0;
      break;
    }
    default:
      return PAPI_ENOSUPP;
  }
  info->event_code = EventCode;
  info->code[0] = EventCode;
  return PAPI_OK;
}
