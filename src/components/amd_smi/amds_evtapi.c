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
#include <ctype.h>

/* Build a "base key" for an event name without the device qualifier.
 * Examples:
 *   "amd_smi:::temp_current:device=0:sensor=1" -> "temp_current:sensor=1"
 *   "amd_smi:::link_type:device=1,peer=0"      -> "link_type:peer=0"
 * If there are no qualifiers beyond device, returns just the metric name.
 */
static void _amds_basekey_wo_device(const char *fullname, char *out, size_t outlen)
{
  if (!fullname || !out || outlen == 0) {
    if (out && outlen) out[0] = '\0';
    return;
  }

  /* Skip component prefix "amd_smi:::" if present */
  const char *s = fullname;
  const char *trip = strstr(fullname, ":::");
  if (trip) s = trip + 3;

  /* Copy metric name up to the first ':' */
  const char *p = strchr(s, ':');
  size_t n_metric = p ? (size_t)(p - s) : strlen(s);
  if (n_metric >= outlen) n_metric = outlen - 1;
  memcpy(out, s, n_metric);
  out[n_metric] = '\0';
  if (!p) return; /* no qualifiers */

  /* Parse qualifiers after the first ':'.
   * We normalize separators to ':' and drop "device=<num>".
   */
  const char *q = p + 1;
  char tmp[512];
  size_t tlen = 0;
  int wrote_any = 0;

  while (*q && tlen + 1 < sizeof(tmp)) {
    /* Drop device=<digits>[,<...>] */
    if (strncmp(q, "device=", 7) == 0) {
      q += 7;
      while (*q && isdigit((unsigned char)*q)) q++;
      if (*q == ',') q++;
      if (*q == ':') q++;
      continue;
    }

    /* Start a new token; we normalize to ':' as separator */
    if (!wrote_any) {
      if (tlen + 1 < sizeof(tmp)) tmp[tlen++] = ':';
      wrote_any = 1;
    } else {
      if (tlen + 1 < sizeof(tmp)) tmp[tlen++] = ':';
    }

    /* Copy token until next ',' or ':' */
    while (*q && *q != ':' && *q != ',') {
      if (tlen + 1 < sizeof(tmp)) tmp[tlen++] = *q;
      q++;
    }
    if (*q == ',' || *q == ':') q++;
  }

  /* Append the non-device qualifiers (if any) to 'out' */
  size_t olen = strlen(out);
  size_t avail = (olen < outlen) ? (outlen - 1 - olen) : 0;
  if (avail > 0 && tlen > 0) {
    size_t cpy = (tlen < avail) ? tlen : avail;
    memcpy(out + olen, tmp, cpy);
    out[olen + cpy] = '\0';
  }
}

/* List allowed device IDs for a given base key, as "0,1,3-5" etc. */
static void _amds_collect_allowed_devices(const char *basekey, char *ids, size_t idslen)
{
  if (!ids || idslen == 0) return;
  ids[0] = '\0';
  if (!ntv_table_p || !basekey) return;

  int present[AMDS_MAX_DEVICES] = {0};
  int max_dev = 0;

  for (int i = 0; i < ntv_table_p->count; ++i) {
    native_event_t *e = &ntv_table_p->events[i];
    if (e->device < 0 || e->device >= (int)AMDS_MAX_DEVICES) continue;
    char bk[512];
    _amds_basekey_wo_device(e->name, bk, sizeof(bk));
    if (strcmp(bk, basekey) == 0) {
      present[e->device] = 1;
      if (e->device + 1 > max_dev) max_dev = e->device + 1;
    }
  }

  /* Build compressed ranges, e.g. "0-2,4,6-7" */
  char buf[256];
  size_t blen = 0;
  int in_run = 0, start = -1, last = -1;
  for (int d = 0; d < max_dev && blen < sizeof(buf); ++d) {
    if (present[d]) {
      if (!in_run) { in_run = 1; start = last = d; }
      else last = d;
    } else if (in_run) {
      if (blen && blen < sizeof(buf)) buf[blen++] = ',';
      if (start == last) blen += (size_t)snprintf(buf + blen, sizeof(buf) - blen, "%d", start);
      else               blen += (size_t)snprintf(buf + blen, sizeof(buf) - blen, "%d-%d", start, last);
      in_run = 0; start = last = -1;
    }
  }
  if (in_run && blen < sizeof(buf)) {
    if (blen && blen < sizeof(buf)) buf[blen++] = ',';
    if (start == last) blen += (size_t)snprintf(buf + blen, sizeof(buf) - blen, "%d", start);
    else               blen += (size_t)snprintf(buf + blen, sizeof(buf) - blen, "%d-%d", start, last);
  }
  if (blen >= sizeof(buf)) blen = sizeof(buf) - 1;
  buf[blen] = '\0';
  snprintf(ids, idslen, "%s", buf);
}

/* Event enumeration: iterate over native events */
int amds_evt_enum(unsigned int *EventCode, int modifier)
{
  if (!EventCode) return PAPI_EINVAL;
  if (!ntv_table_p) return PAPI_ECMP;

  if (modifier == PAPI_ENUM_FIRST) {
    if (ntv_table_p->count == 0) return PAPI_ENOEVNT;
    *EventCode = ntv_table_p->events[0].id; /* encoded EventCode */
    return PAPI_OK;
  } else if (modifier == PAPI_ENUM_EVENTS) {
    /* Find current index by matching encoded EventCode to ev->id, then advance */
    int cur = -1;
    for (int i = 0; i < ntv_table_p->count; ++i) {
      if (ntv_table_p->events[i].id == *EventCode) { cur = i; break; }
    }
    if (cur < 0) return PAPI_ENOEVNT;
    if (cur + 1 < ntv_table_p->count) {
      *EventCode = ntv_table_p->events[cur + 1].id;
      return PAPI_OK;
    }
    return PAPI_ENOEVNT;
  }
  return PAPI_EINVAL;
}

int amds_evt_code_to_name(unsigned int EventCode, char *name, int len) {
  if (!name || len <= 0) return PAPI_EINVAL;
  if (!ntv_table_p) return PAPI_ECMP;
  /* Resolve encoded EventCode by scanning ev->id */
  for (int i = 0; i < ntv_table_p->count; ++i) {
    if (ntv_table_p->events[i].id == EventCode) {
      snprintf(name, (size_t)len, "%s", ntv_table_p->events[i].name);
      return PAPI_OK;
    }
  }
  return PAPI_EINVAL;
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
  if (hret != HTABLE_SUCCESS) {
    return (hret == HTABLE_ENOVAL) ? PAPI_ENOEVNT : PAPI_ECMP;
  }
  *EventCode = event->id;
  return PAPI_OK;
}

int amds_evt_code_to_descr(unsigned int EventCode, char *descr, int len) {
  if (!descr || len <= 0) return PAPI_EINVAL;
  if (!ntv_table_p) return PAPI_ECMP;

  native_event_t *ev = NULL;
  for (int i = 0; i < ntv_table_p->count; ++i) {
    if (ntv_table_p->events[i].id == EventCode) {
      ev = &ntv_table_p->events[i];
      break;
    }
  }
  if (!ev) return PAPI_EINVAL;

  int n = snprintf(descr, (size_t)len, "%s", ev->descr ? ev->descr : "");

  if (ev->device >= 0 && n >= 0 && n < len) {
    char basekey[512];
    char ids[256];
    basekey[0] = '\0';
    ids[0] = '\0';
    _amds_basekey_wo_device(ev->name, basekey, sizeof(basekey));
    if (basekey[0]) {
      _amds_collect_allowed_devices(basekey, ids, sizeof(ids));
    }

    n += snprintf(descr + n, (size_t)(len - n), "\n     :device=%d", ev->device);

    if (n >= 0 && n < len && ids[0] != '\0') {
      n += snprintf(descr + n, (size_t)(len - n),
                    "\n            Mandatory device qualifier [%s]", ids);
    }
  }

  return (n >= 0) ? PAPI_OK : PAPI_EMISC;
}
