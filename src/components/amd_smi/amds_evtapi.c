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
#include <stdint.h>
#include <ctype.h>

/* --- CUDA-style folding of base events + device qualifiers ---------------- */
/* We will enumerate *base names* as virtual codes with a top-bit tag.
 * Real measurement still uses concrete per-device ids (indices into ntv_table).
 * name_to_code accepts either full names ("x:device=1") or a base name "x"
 * (defaults to the first device that supports it), matching CUDA behavior. */

#define AMDS_VIRT_FLAG        (0x80000000u)   /* marks a virtual/base event code */
#define AMDS_VIRT_INDEX(code) ((unsigned int)((code) & ~AMDS_VIRT_FLAG))
#define AMDS_IS_VIRT(code)    (((code) & AMDS_VIRT_FLAG) != 0)

typedef struct {
  char       *base;      /* strdup of "name" up to first ':' (or full name if none) */
  unsigned    repr_id;   /* representative concrete event id (index in ntv_table) */
  uint64_t    devmap;    /* bitmap of devices that implement this base event */
} amds_base_ev_t;

static amds_base_ev_t *g_base = NULL;
static int             g_base_cnt = 0;
static int             g_base_built = 0;

static void basename_of(const char *name, char *out, size_t outlen)
{
  if (!name || !out || outlen == 0) return;
  const char *p = strchr(name, ':');
  if (!p) {
    snprintf(out, outlen, "%s", name);
  } else {
    size_t n = (size_t)(p - name);
    if (n >= outlen) n = outlen - 1;
    memcpy(out, name, n);
    out[n] = '\0';
  }
}

static int base_find(const char *base)
{
  for (int i = 0; i < g_base_cnt; ++i) {
    if (strcmp(g_base[i].base, base) == 0) return i;
  }
  return -1;
}

static void base_build_if_needed(void)
{
  if (g_base_built) return;
  if (!ntv_table_p || ntv_table_p->count <= 0) return;

  g_base = (amds_base_ev_t *)calloc((size_t)ntv_table_p->count, sizeof(*g_base));
  if (!g_base) return;

  char buf[PAPI_MAX_STR_LEN];
  for (int i = 0; i < ntv_table_p->count; ++i) {
    native_event_t *ev = &ntv_table_p->events[i];
    basename_of(ev->name, buf, sizeof buf);
    int idx = base_find(buf);
    if (idx < 0) {
      g_base[g_base_cnt].base = strdup(buf);
      g_base[g_base_cnt].repr_id = (unsigned)i;
      g_base[g_base_cnt].devmap = 0;
      if (ev->device >= 0 && ev->device < 64) {
        g_base[g_base_cnt].devmap |= (UINT64_C(1) << ev->device);
      }
      g_base_cnt++;
    } else {
      if (ev->device >= 0 && ev->device < 64) {
        g_base[idx].devmap |= (UINT64_C(1) << ev->device);
      }
      /* keep first-seen repr_id */
    }
  }
  g_base_built = 1;
}

/* Compose a list like "0,1,3" from devmap (<=64 devices) */
static void devlist_from_map(uint64_t map, char *out, size_t outlen, int *first_dev)
{
  out[0] = '\0';
  if (first_dev) *first_dev = -1;
  size_t used = 0;
  for (int d = 0; d < 64; ++d) {
    if (map & (UINT64_C(1) << d)) {
      if (first_dev && *first_dev < 0) *first_dev = d;
      int n = snprintf(out + used, (used < outlen ? outlen - used : 0), (used ? ",%d" : "%d"), d);
      if (n < 0) break;
      used += (size_t)n;
      if (used + 1 >= outlen) break;
    }
  }
}

/* Event enumeration: iterate over native events */
int amds_evt_enum(unsigned int *EventCode, int modifier) {
  if (!EventCode) {
    return PAPI_EINVAL;
  }
  if (!ntv_table_p) {
    return PAPI_ECMP;
  }

  base_build_if_needed();

  switch (modifier) {
    case PAPI_ENUM_FIRST: {
      if (g_base_cnt == 0) {
        return PAPI_ENOEVNT;
      }
      *EventCode = (AMDS_VIRT_FLAG | 0u);
      return PAPI_OK;
    }
    case PAPI_ENUM_EVENTS: {
      if (!AMDS_IS_VIRT(*EventCode)) {
        /* treat legacy enumeration as "done" */
        return PAPI_ENOEVNT;
      }
      unsigned i = AMDS_VIRT_INDEX(*EventCode);
      if (i + 1 < (unsigned)g_base_cnt) {
        *EventCode = (AMDS_VIRT_FLAG | (i + 1));
        return PAPI_OK;
      }
      return PAPI_ENOEVNT;
    }
    /* we don't enumerate umasks for AMD SMI (device qualifier is handled
       in code_to_info and by :device=# in names, like CUDA) */
    default:
      return PAPI_EINVAL;
  }
}

int amds_evt_code_to_name(unsigned int EventCode, char *name, int len) {
  if (!name || len <= 0) {
    return PAPI_EINVAL;
  }
  if (!ntv_table_p) {
    return PAPI_ECMP;
  }
  base_build_if_needed();

  if (AMDS_IS_VIRT(EventCode)) {
    unsigned i = AMDS_VIRT_INDEX(EventCode);
    if (i >= (unsigned)g_base_cnt) return PAPI_EINVAL;
    snprintf(name, (size_t)len, "%s", g_base[i].base);
    return PAPI_OK;
  } else {
    if (EventCode >= (unsigned)ntv_table_p->count) {
      return PAPI_EINVAL;
    }
    snprintf(name, (size_t)len, "%s", ntv_table_p->events[EventCode].name);
    return PAPI_OK;
  }
}

int amds_evt_name_to_code(const char *name, unsigned int *EventCode) {
  if (!name || !EventCode) {
    return PAPI_EINVAL;
  }
  if (!htable) {
    return PAPI_ECMP;
  }
  /* Fast path: exact match (legacy per-device names) */
  native_event_t *event = NULL;
  int hret = htable_find(htable, name, (void **)&event);
  if (hret == HTABLE_SUCCESS) {
    *EventCode = event->id;
    return PAPI_OK;
  }
  if (hret != HTABLE_ENOVAL) {
    return PAPI_ECMP;
  }

  /* Try CUDA-style: base name with (optional) :device=# qualifier. */
  base_build_if_needed();
  /* Extract base */
  char base[PAPI_MAX_STR_LEN]; base[0] = '\0';
  basename_of(name, base, sizeof base);
  int bidx = base_find(base);
  if (bidx < 0) {
    return PAPI_ENOEVNT;
  }
  /* Parse :device= if present; else choose first device in map */
  int dev = -1;
  const char *p = strstr(name, ":device=");
  if (p) {
    p += 8;
    if (!isdigit((unsigned char)*p)) {
      return PAPI_ENOEVNT;
    }
    char *endp = NULL;
    long d = strtol(p, &endp, 10);
    if (d < 0 || d >= 64) return PAPI_ENOEVNT;
    /* Allow only further qualifiers after :device=... (we ignore them) */
    if (endp && *endp && *endp != ':') {
      return PAPI_ENOEVNT;
    }
    dev = (int)d;
    if (!(g_base[bidx].devmap & (UINT64_C(1) << dev))) {
      return PAPI_ENOEVNT; /* device does not implement this event */
    }
  } else {
    /* Default to first device that implements this base (CUDA-like) */
    int first_dev = -1;
    char tmp[PAPI_MIN_STR_LEN];
    devlist_from_map(g_base[bidx].devmap, tmp, sizeof tmp, &first_dev);
    dev = first_dev;
    if (dev < 0) {
      return PAPI_ENOEVNT; /* base exists but not tied to a device; must be global */
    }
  }
  /* Build canonical per-device key "base:device=%d" and look it up */
  char key[PAPI_HUGE_STR_LEN];
  int n = snprintf(key, sizeof key, "%s:device=%d", base, dev);
  if (n < 0 || n >= (int)sizeof key) return PAPI_EBUF;
  event = NULL;
  hret = htable_find(htable, key, (void **)&event);
  if (hret != HTABLE_SUCCESS || !event) {
    /* Some events have more qualifiers after :device=...  – try to find any
       name that starts with "base:device=dev". We linearly probe the table. */
    for (int i = 0; i < ntv_table_p->count; ++i) {
      native_event_t *ev = &ntv_table_p->events[i];
      if (!strncmp(ev->name, key, (size_t)n)) {
        event = ev; break;
      }
    }
    if (!event) return PAPI_ENOEVNT;
  }
  *EventCode = event->id;
  return PAPI_OK;
}

int amds_evt_code_to_descr(unsigned int EventCode, char *descr, int len) {
  if (!descr || len <= 0) {
    return PAPI_EINVAL;
  }
  if (!ntv_table_p) {
    return PAPI_ECMP;
  }
  base_build_if_needed();

  if (AMDS_IS_VIRT(EventCode)) {
    unsigned i = AMDS_VIRT_INDEX(EventCode);
    if (i >= (unsigned)g_base_cnt) return PAPI_EINVAL;
    unsigned rid = g_base[i].repr_id;
    if (rid >= (unsigned)ntv_table_p->count) return PAPI_EINVAL;
    snprintf(descr, (size_t)len, "%s", ntv_table_p->events[rid].descr);
    return PAPI_OK;
  } else {
    if (EventCode >= (unsigned)ntv_table_p->count) {
      return PAPI_EINVAL;
    }
    snprintf(descr, (size_t)len, "%s", ntv_table_p->events[EventCode].descr);
    return PAPI_OK;
  }
}

int amds_evt_code_to_info(unsigned int EventCode, PAPI_event_info_t *info) {
  if (!info) return PAPI_EINVAL;
  if (!ntv_table_p) return PAPI_ECMP;
  base_build_if_needed();

  char symbol[PAPI_HUGE_STR_LEN] = {0};
  char longd[PAPI_HUGE_STR_LEN]  = {0};

  if (AMDS_IS_VIRT(EventCode)) {
    unsigned i = AMDS_VIRT_INDEX(EventCode);
    if (i >= (unsigned)g_base_cnt) return PAPI_EINVAL;
    native_event_t *ev = &ntv_table_p->events[g_base[i].repr_id];

    /* Symbol: base name only (folded), CUDA-like */
    int s = snprintf(symbol, sizeof symbol, "%s", g_base[i].base);
    if (s < 0 || s >= (int)sizeof symbol) return PAPI_EBUF;

    /* Long description: original description + "masks:Mandatory device qualifier [...]" */
    char devices[PAPI_HUGE_STR_LEN]; int first_dev = -1;
    devices[0] = '\0';
    if (g_base[i].devmap) {
      devlist_from_map(g_base[i].devmap, devices, sizeof devices, &first_dev);
      int l = snprintf(longd, sizeof longd, "%s masks:Mandatory device qualifier [%s]",
                       ev->descr ? ev->descr : "", devices);
      if (l < 0 || l >= (int)sizeof longd) return PAPI_EBUF;
    } else {
      /* base has no device qualifier (global event) */
      int l = snprintf(longd, sizeof longd, "%s", ev->descr ? ev->descr : "");
      if (l < 0 || l >= (int)sizeof longd) return PAPI_EBUF;
    }
  } else {
    /* Concrete per-device id (keep legacy behavior for callers that ask) */
    if (EventCode >= (unsigned)ntv_table_p->count) return PAPI_EINVAL;
    native_event_t *ev = &ntv_table_p->events[EventCode];
    int s = snprintf(symbol, sizeof symbol, "%s", ev->name);
    if (s < 0 || s >= (int)sizeof symbol) return PAPI_EBUF;
    int l = snprintf(longd, sizeof longd, "%s", ev->descr ? ev->descr : "");
    if (l < 0 || l >= (int)sizeof longd) return PAPI_EBUF;
  }

  /* Fill out info */
  symbol[sizeof symbol - 1] = 0;
  longd[sizeof longd - 1] = 0;
  strncpy(info->symbol, symbol, sizeof info->symbol);
  info->symbol[sizeof info->symbol - 1] = 0;
  info->short_descr[0] = 0;
  strncpy(info->long_descr, longd, sizeof info->long_descr);
  info->long_descr[sizeof info->long_descr - 1] = 0;

  return PAPI_OK;
}

