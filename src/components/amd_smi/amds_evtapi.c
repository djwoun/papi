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
#include <ctype.h>
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
    int strLen = snprintf(buf + used, len - used, "%s%d", have ? "," : "", d);
    if (strLen < 0 || (size_t)strLen >= len - used)
      return PAPI_EBUF;
    used += (size_t)strLen;
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

static int device_next(uint64_t bitmap, int after) {
  int limit = device_bitmap_limit();
  for (int d = after + 1; d < limit; ++d) {
    if (amds_dev_check(bitmap, d))
      return d;
  }
  return -1;
}

static const char *find_device_token_span(const char *str,
                                          const char **digits_start,
                                          const char **digits_end) {
  if (digits_start)
    *digits_start = NULL;
  if (digits_end)
    *digits_end = NULL;
  if (!str)
    return NULL;

  const char *p = str;
  while ((p = strstr(p, "Device ")) != NULL) {
    const char *dstart = p + strlen("Device ");
    if (!isdigit((unsigned char)*dstart)) {
      p = dstart;
      continue;
    }
    const char *dend = dstart;
    while (isdigit((unsigned char)*dend))
      dend++;
    if (digits_start)
      *digits_start = dstart;
    if (digits_end)
      *digits_end = dend;
    return p;
  }

  return NULL;
}

static void normalize_descr_ignore_device_numbers(const char *src, char *dst,
                                                  size_t len,
                                                  int *found_any) {
  if (found_any)
    *found_any = 0;
  if (!dst || len == 0)
    return;
  dst[0] = '\0';
  if (!src)
    return;

  size_t used = 0;
  const char *p = src;
  while (*p && used < len - 1) {
    const char *tok = strstr(p, "Device ");
    if (!tok) {
      size_t chunk = strlen(p);
      if (chunk >= len - used)
        chunk = (len - used) - 1;
      memcpy(dst + used, p, chunk);
      used += chunk;
      break;
    }

    size_t chunk = (size_t)(tok - p);
    if (chunk >= len - used)
      chunk = (len - used) - 1;
    memcpy(dst + used, p, chunk);
    used += chunk;
    if (used >= len - 1)
      break;

    const char *after = tok + strlen("Device ");
    size_t toklen = strlen("Device ");
    if (toklen >= len - used)
      toklen = (len - used) - 1;
    memcpy(dst + used, tok, toklen);
    used += toklen;
    if (used >= len - 1)
      break;

    if (isdigit((unsigned char)*after)) {
      if (found_any)
        *found_any = 1;
      const char *d = after;
      while (isdigit((unsigned char)*d))
        d++;
      dst[used++] = 'X';
      p = d;
    } else {
      p = after;
    }
  }

  dst[used] = '\0';
}

static const char *event_descr_for_device(const native_event_t *event,
                                          const amds_per_device_descr_t *pd,
                                          int device,
                                          const char *fallback) {
  const char *src = fallback;
  if (event && (event->evtinfo_flags & AMDS_EVTINFO_FLAG_PER_DEVICE_DESCR)) {
    if (pd && pd->descrs && pd->num_devices > 0 && device >= 0 &&
        device < pd->num_devices && pd->descrs[device]) {
      src = pd->descrs[device];
    }
  }
  return src;
}

static int try_collapse_base_device_descriptions(const native_event_t *event,
                                                 const char *fallback,
                                                 char *buf, size_t len) {
  if (!event || !buf || len == 0)
    return 0;
  if (!(event->evtinfo_flags & AMDS_EVTINFO_FLAG_PER_DEVICE_DESCR))
    return 0;
  if (!event->device_map)
    return 0;

  int first = device_first(event->device_map);
  if (first < 0)
    return 0;
  int second = device_next(event->device_map, first);
  if (second < 0)
    return 0; // only one device -> nothing to collapse

  const amds_per_device_descr_t *pd = event->per_device_descr;
  const char *ref_line = event_descr_for_device(event, pd, first, fallback);

  char ref_norm[PAPI_MAX_STR_LEN];
  int found_ref = 0;
  normalize_descr_ignore_device_numbers(ref_line, ref_norm, sizeof(ref_norm),
                                       &found_ref);
  if (!found_ref)
    return 0;

  for (int dev = second; dev >= 0; dev = device_next(event->device_map, dev)) {
    const char *line = event_descr_for_device(event, pd, dev, fallback);
    char norm[PAPI_MAX_STR_LEN];
    int found = 0;
    normalize_descr_ignore_device_numbers(line, norm, sizeof(norm), &found);
    if (!found)
      return 0;
    if (strcmp(norm, ref_norm) != 0)
      return 0;
  }

  char devices[PAPI_MAX_STR_LEN];
  if (format_device_bitmap(event->device_map, devices, sizeof(devices)) !=
      PAPI_OK)
    return 0;

  const char *digits_start = NULL;
  const char *digits_end = NULL;
  const char *tok = find_device_token_span(ref_line, &digits_start, &digits_end);
  if (!tok || !digits_start || !digits_end)
    return 0;
  // Only collapse strings that contain exactly one "Device <n>" token.
  if (find_device_token_span(digits_end, NULL, NULL))
    return 0;

  int prefix_len = (int)(tok - ref_line);
  int strLen =
      snprintf(buf, len, "%.*sDevices %s%s", prefix_len, ref_line, devices,
               digits_end);
  if (strLen < 0 || (size_t)strLen >= len)
    return 0;

  return 1;
}

static int fill_event_description(const native_event_t *event, int device,
                                  char *buf, size_t len) {
  if (!buf || len == 0)
    return PAPI_EINVAL;
  buf[0] = '\0';

  if (!event)
    return PAPI_OK;

  const char *fallback = event->descr ? event->descr : "";

  /* Device-qualified event: use per-device descr if present, else fallback. */
  if (device >= 0) {
    const char *src = fallback;
    if (event->evtinfo_flags & AMDS_EVTINFO_FLAG_PER_DEVICE_DESCR) {
      const amds_per_device_descr_t *pd = event->per_device_descr;
      if (pd && pd->descrs && pd->num_devices > 0 && device < pd->num_devices &&
          pd->descrs[device]) {
        src = pd->descrs[device];
      }
    }
    CHECK_SNPRINTF(buf, len, "%s", src);
    return PAPI_OK;
  }

  /*
   * Base event: if this event has per-device descrs, pack them into a single
   * string using fixed-width fields so tools like papi_native_avail fold each
   * device descr onto its own output line.
   */
  if (!(event->evtinfo_flags & AMDS_EVTINFO_FLAG_PER_DEVICE_DESCR)) {
    CHECK_SNPRINTF(buf, len, "%s", fallback);
    return PAPI_OK;
  }

  if (!event->device_map) {
    snprintf(buf, len, "%s", fallback);
    return PAPI_OK;
  }

  if (try_collapse_base_device_descriptions(event, fallback, buf, len))
    return PAPI_OK;

  size_t used = 0;
  // Keep in sync with papi_native_avail's fixed-width folding (EVT_LINE - 12 - 2).
  const size_t line_width = 80 - 12 - 2;

  const amds_per_device_descr_t *pd = event->per_device_descr;
  for (int dev = device_first(event->device_map); dev >= 0;) {
    int next = device_next(event->device_map, dev);

    const char *line = fallback;
    if (pd && pd->descrs && pd->num_devices > 0 && dev < pd->num_devices &&
        pd->descrs[dev]) {
      line = pd->descrs[dev];
    }

    int strLen = snprintf(buf + used, len - used, "%s", line);
    if (strLen < 0) {
      buf[used] = '\0';
      break;
    }
    if ((size_t)strLen >= len - used) {
      buf[len - 1] = '\0';
      break;
    }
    used += (size_t)strLen;

    if (next < 0)
      break;

    size_t rem = used % line_width;
    if (rem != 0) {
      size_t pad = line_width - rem;
      if (pad >= len - used)
        pad = (len - used) - 1;
      memset(buf + used, ' ', pad);
      used += pad;
      buf[used] = '\0';
      if (pad != line_width - rem)
        break;
    }

    dev = next;
  }

  if (buf[0] == '\0')
    snprintf(buf, len, "%s", fallback);

  return PAPI_OK;
}

#define CHECK_FILL_EVENT_DESCRIPTION(event, device, buf, len)                \
  do {                                                                       \
    int _papi_errno = fill_event_description((event), (device), (buf), (len)); \
    if (_papi_errno != PAPI_OK)                                              \
      return _papi_errno;                                                    \
  } while (0)

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
  case PAPI_NTV_ENUM_UMASKS: {
    papi_errno = amds_evt_id_to_info(*EventCode, &info);
    if (papi_errno != PAPI_OK)
      return papi_errno;

    native_event_t *event = &ntv_table_p->events[info.nameid];
    if (!event->device_map)
      return PAPI_ENOEVNT;

    if (!(info.flags & AMDS_DEVICE_FLAG)) {
      int first = device_first(event->device_map);
      if (first < 0)
        return PAPI_ENOEVNT;
      info.flags |= AMDS_DEVICE_FLAG;
      info.device = first;
      return amds_evt_id_create(&info, EventCode);
    }

    int next = device_next(event->device_map, info.device);
    if (next < 0)
      return PAPI_ENOEVNT;
    info.device = next;
    return amds_evt_id_create(&info, EventCode);
  }
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
    CHECK_SNPRINTF(name, (size_t)len, "%s:device=%d", event->name, info.device);
  else
    CHECK_SNPRINTF(name, (size_t)len, "%s", event->name);
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
  return fill_event_description(event,
                                (info.flags & AMDS_DEVICE_FLAG) ? info.device
                                                               : -1,
                                descr, (size_t)len);
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
  int total_qualifiers = event->device_map ? 1 : 0;
  int quals_to_report = total_qualifiers;
  char devices[PAPI_MAX_STR_LEN] = {0};

  if (total_qualifiers > 0) {
    papi_errno = format_device_bitmap(event->device_map, devices, sizeof(devices));
    if (papi_errno != PAPI_OK)
      return papi_errno;
    CHECK_SNPRINTF(info->quals[0], sizeof(info->quals[0]), ":device=");
    CHECK_SNPRINTF(info->quals_descrs[0], sizeof(info->quals_descrs[0]),
             "Mandatory device qualifier [%s]", devices);
  }

  int canonical_device = -1;
  if (device_flag == AMDS_DEVICE_FLAG) {
    canonical_device = device_first(event->device_map);
    if (canonical_device < 0)
      return PAPI_ENOEVNT;
  }

  switch (device_flag) {
    case 0:
      CHECK_SNPRINTF(info->symbol, sizeof(info->symbol), "%s", event->name);
      CHECK_FILL_EVENT_DESCRIPTION(event, -1, info->long_descr,
                                   sizeof(info->long_descr));
      break;
    case AMDS_DEVICE_FLAG:
      if (code_info.device != canonical_device) {
        // Suppress duplicate qualifier dumps for non-canonical variants so
        // tools like papi_native_avail match the legacy CUDA-style output.
        CHECK_SNPRINTF(info->symbol, sizeof(info->symbol), "%s", event->name);
        CHECK_FILL_EVENT_DESCRIPTION(event, code_info.device, info->long_descr,
                                     sizeof(info->long_descr));
        quals_to_report = 0;
      } else {
        CHECK_SNPRINTF(info->symbol, sizeof(info->symbol), "%s:device=%d", event->name,
                 canonical_device);
        char event_descr[PAPI_HUGE_STR_LEN];
        CHECK_FILL_EVENT_DESCRIPTION(event, canonical_device, event_descr,
                                     sizeof(event_descr));
        CHECK_SNPRINTF(info->long_descr, sizeof(info->long_descr),
                 "%s, masks:Mandatory device qualifier [%s]",
                 event_descr,
                 devices);
      }
      break;
    default:
      return PAPI_ENOSUPP;
  }
  info->num_quals = quals_to_report;
  return PAPI_OK;
}
