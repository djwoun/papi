/**
 * @file    amdsmi_basics.c
 * @author  Dong Jun Woun
 *          djwoun@gmail.com
 * @brief   Enumerates every native AMD-SMI event exposed through PAPI and measures
 *          them one at a time.
 */

#include "test_harness.h"
#include "papi.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// Return true if papi_errno is a "warning, not failure" status for add/start/stop.
static inline bool is_warning_papi_errno(int papi_errno) {
  return (papi_errno == PAPI_ENOEVNT) || (papi_errno == PAPI_ECNFLCT) ||
         (papi_errno == PAPI_EPERM);
}

#define MAX_AMDSMI_DEVICES 64

// Extract the device list from info->long_descr/short_descr (format: ... [0,1,2]).
static int parse_device_list(const PAPI_event_info_t *info, int *devices,
                             int max_devices) {
  if (!info || !devices || max_devices <= 0)
    return 0;

  const char *sources[2] = {info->long_descr, info->short_descr};
  int count = 0;

  for (int source_idx = 0; source_idx < 2 && count == 0; ++source_idx) {
    const char *text = sources[source_idx];
    if (!text || !*text)
      continue;

    const char *open = strchr(text, '[');
    const char *close = open ? strchr(open + 1, ']') : NULL;
    if (!open || !close || close <= open + 1)
      continue;

    size_t span = (size_t)(close - (open + 1));
    if (span == 0)
      continue;

    // Copy the comma-separated device list into a scratch buffer.
    char scratch[PAPI_MAX_STR_LEN];
    if (span >= sizeof(scratch))
      span = sizeof(scratch) - 1;
    memcpy(scratch, open + 1, span);
    scratch[span] = '\0';

    char *token = strtok(scratch, ",");
    while (token && count < max_devices) {
      while (*token == ' ')
        ++token;
      if (*token == '\0') {
        token = strtok(NULL, ",");
        continue;
      }
      char *endptr = NULL;
      long dev = strtol(token, &endptr, 10);
      if (endptr != token && dev >= 0 && dev <= INT_MAX)
        devices[count++] = (int)dev;
      token = strtok(NULL, ",");
    }
  }
  return count;
}

// Rewrite the :device= qualifier in ev_name with a new device value.
static bool rewrite_device_variant(const char *ev_name, int new_device,
                                   char *out, size_t out_size) {
  if (!ev_name || !out || out_size == 0)
    return false;

  const char *device_pos = strstr(ev_name, ":device=");
  if (!device_pos)
    return false;

  const char *value_start = device_pos + strlen(":device=");
  const char *suffix = value_start;
  while (*suffix >= '0' && *suffix <= '9')
    ++suffix;

  size_t keep = (size_t)(value_start - ev_name);
  if (keep >= out_size)
    return false;

  memcpy(out, ev_name, keep);
  int written = snprintf(out + keep, out_size - keep, "%d%s", new_device, suffix);
  if (written < 0)
    return false;
  return (size_t)written < (out_size - keep);
}

typedef struct {
  int passed;
  int warned;
  int failed;
  int skipped;
  int index;
} HarnessStats;

static void run_single_event(int event_code, const char *ev_name, int cid,
                             HarnessStats *stats, HarnessOpts opts) {
  NOTE("[%4d] Testing %s...", stats->index, ev_name);

  int papi_errno = PAPI_OK;
  int eventSet = PAPI_NULL;
  papi_errno = PAPI_create_eventset(&eventSet);
  if (papi_errno != PAPI_OK) {
    NOTE("  ?  create_eventset failed: %s", PAPI_strerror(papi_errno));
    ++stats->failed;
    ++stats->index;
    return;
  }

  papi_errno = PAPI_assign_eventset_component(eventSet, cid);
  if (papi_errno != PAPI_OK) {
    NOTE("  ?  assign_eventset_component failed: %s", PAPI_strerror(papi_errno));
    (void)PAPI_destroy_eventset(&eventSet);
    ++stats->failed;
    ++stats->index;
    return;
  }

  papi_errno = PAPI_add_event(eventSet, event_code);
  if (papi_errno != PAPI_OK) {
    if (is_warning_papi_errno(papi_errno)) {
      WARNF("Could not add %-50s (%s)", ev_name, PAPI_strerror(papi_errno));
      ++stats->warned;
    } else {
      NOTE("  ?  Could not add %s (%s)", ev_name, PAPI_strerror(papi_errno));
      ++stats->failed;
    }
    (void)PAPI_destroy_eventset(&eventSet);
    ++stats->index;
    return;
  }

  long long value = 0;
  papi_errno = PAPI_start(eventSet);
  if (papi_errno != PAPI_OK) {
    if (is_warning_papi_errno(papi_errno)) {
      WARNF("start %-54s (%s)", ev_name, PAPI_strerror(papi_errno));
      ++stats->warned;
    } else {
      NOTE("  ?  start failed for %s (%s)", ev_name, PAPI_strerror(papi_errno));
      ++stats->failed;
    }
    (void)PAPI_cleanup_eventset(eventSet);
    (void)PAPI_destroy_eventset(&eventSet);
    ++stats->index;
    return;
  }

  papi_errno = PAPI_stop(eventSet, &value);
  if (papi_errno != PAPI_OK) {
    if (is_warning_papi_errno(papi_errno)) {
      WARNF("stop  %-54s (%s)", ev_name, PAPI_strerror(papi_errno));
      ++stats->warned;
    } else {
      NOTE("  ?  stop failed for %s (%s)", ev_name, PAPI_strerror(papi_errno));
      ++stats->failed;
    }
    (void)PAPI_cleanup_eventset(eventSet);
    (void)PAPI_destroy_eventset(&eventSet);
    ++stats->index;
    return;
  }

  ++stats->passed;
  if (opts.print) {
    printf("      %-60s = %lld\n\n", ev_name, value);
  }

  (void)PAPI_cleanup_eventset(eventSet);
  (void)PAPI_destroy_eventset(&eventSet);
  ++stats->index;
}

int main(int argc, char *argv[]) {
  // Unbuffer stdout so the final status line shows promptly.
  setvbuf(stdout, NULL, _IONBF, 0);

  harness_accept_tests_quiet(&argc, argv);
  HarnessOpts opts = parse_harness_cli(argc, argv);

  // 1) Initialize PAPI.
  int papi_errno = PAPI_library_init(PAPI_VER_CURRENT);
  if (papi_errno != PAPI_VER_CURRENT) {
    NOTE("PAPI_library_init failed: %s", PAPI_strerror(papi_errno));
    return eval_result(opts, 1);
  }

  // 2) Locate the AMD-SMI component.
  int cid = -1;
  const int ncomps = PAPI_num_components();
  for (int i = 0; i < ncomps && cid < 0; ++i) {
    const PAPI_component_info_t *cinfo = PAPI_get_component_info(i);
    if (cinfo && strcmp(cinfo->name, "amd_smi") == 0) {
      cid = i;
    }
  }
  if (cid < 0) {
    // Can't run this test on this build/platform (likely PAPI built without ROCm) — skip with warning.
    SKIP("Unable to locate the amd_smi component (PAPI built without ROCm?)");
  }

  NOTE("Using AMD-SMI component id %d\n", cid);

  // 3) Enumerate every native event.
  int ev_code = PAPI_NATIVE_MASK;
  if (PAPI_enum_cmp_event(&ev_code, PAPI_ENUM_FIRST, cid) != PAPI_OK) {
    // No events — treat as "nothing to do" (warning instead of failing).
    SKIP("No native events found for AMD-SMI component");
  }

  HarnessStats stats = {0};

  do {
    int qualified_code = ev_code;
    bool enumerate_variants = false;
    PAPI_event_info_t einfo;
    memset(&einfo, 0, sizeof(einfo));
    if (PAPI_get_event_info(ev_code, &einfo) == PAPI_OK && einfo.num_quals > 0) {
      int tmp = ev_code;
      if (PAPI_enum_cmp_event(&tmp, PAPI_NTV_ENUM_UMASKS, cid) == PAPI_OK) {
        enumerate_variants = true;
        qualified_code = tmp;
      }
    }

    bool variants_remaining = true;
    while (variants_remaining) {
      char ev_name[PAPI_MAX_STR_LEN] = {0};
      if (PAPI_event_code_to_name(qualified_code, ev_name) != PAPI_OK) {
        NOTE("[%4d] Skipping 0x%x (unable to resolve name)", stats.index,
             qualified_code);
        ++stats.skipped;
        ++stats.index;
        goto advance_variant;
      }

      // Skip process* events; these aren't testable in this harness.
      if (strncmp(ev_name, "amd_smi:::process", 17) == 0 ||
          strncmp(ev_name, "process", 7) == 0) {
        ++stats.skipped;
        NOTE("[%4d] Skipping %s (process events not testable)", stats.index,
             ev_name);
        ++stats.index;
        goto advance_variant;
      }

      PAPI_event_info_t variant_info;
      memset(&variant_info, 0, sizeof(variant_info));
      int have_info = (PAPI_get_event_info(qualified_code, &variant_info) == PAPI_OK);

      const char *device_tag = strstr(ev_name, ":device=");
      if (!device_tag) {
        run_single_event(qualified_code, ev_name, cid, &stats, opts);
      } else {
        int devices[MAX_AMDSMI_DEVICES] = {0};
        int device_count = have_info
                               ? parse_device_list(&variant_info, devices,
                                                   MAX_AMDSMI_DEVICES)
                               : 0;

        if (device_count <= 0) {
          run_single_event(qualified_code, ev_name, cid, &stats, opts);
        } else {
          for (int d = 0; d < device_count; ++d) {
            char variant_name[PAPI_MAX_STR_LEN];
            if (!rewrite_device_variant(ev_name, devices[d], variant_name,
                                        sizeof(variant_name))) {
              NOTE("[%4d] Skipping %s (unable to rewrite :device=%d)",
                   stats.index, ev_name, devices[d]);
              ++stats.skipped;
              ++stats.index;
              continue;
            }

            int variant_code = 0;
            int rc = PAPI_event_name_to_code(variant_name, &variant_code);
            if (rc != PAPI_OK) {
              NOTE("[%4d] Skipping %s (unable to resolve variant: %s)",
                   stats.index, variant_name, PAPI_strerror(rc));
              ++stats.skipped;
              ++stats.index;
              continue;
            }

            run_single_event(variant_code, variant_name, cid, &stats, opts);
          }
        }
      }

advance_variant:
      if (enumerate_variants) {
        int next = qualified_code;
        if (PAPI_enum_cmp_event(&next, PAPI_NTV_ENUM_UMASKS, cid) == PAPI_OK) {
          qualified_code = next;
          continue;
        }
      }
      variants_remaining = false;
    }

  } while (PAPI_enum_cmp_event(&ev_code, PAPI_ENUM_EVENTS, cid) == PAPI_OK);

  if (opts.print) {
    printf("Summary: passed=%d  warned=%d  skipped=%d  failed=%d\n",
           stats.passed, stats.warned, stats.skipped, stats.failed);
  }

  PAPI_shutdown();

  // Final: fail only if we had real failures; warnings/skips are allowed.
  int exit_status = (stats.failed == 0) ? 0 : 1;
  return eval_result(opts, exit_status);
}
