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

// Return true if papi_errno is a "warning, not failure" status for add/start/stop.
static inline bool is_warning_papi_errno(int papi_errno) {
  return (papi_errno == PAPI_ENOEVNT) || (papi_errno == PAPI_ECNFLCT) ||
         (papi_errno == PAPI_EPERM);
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

  int event_index = 0;
  int passed = 0, warned = 0, failed = 0, skipped = 0;

  do {
    int qualified_code = ev_code;
    bool enumerate_variants = false;
    PAPI_event_info_t einfo;
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
        NOTE("[%4d] Skipping 0x%x (unable to resolve name)\n", event_index++,
             qualified_code);
        ++skipped;
        goto advance_variant;
      }

      // Skip process* events; these aren't testable in this harness.
      if (strncmp(ev_name, "amd_smi:::process", 17) == 0 ||
          strncmp(ev_name, "process", 7) == 0) {
        ++skipped;
        NOTE("[%4d] Skipping %s (process events not testable)\n", event_index++,
             ev_name);
        goto advance_variant;
      }

      NOTE("[%4d] Testing %s...", event_index, ev_name);

      // 4–7) Create a fresh EventSet, add the event, start, stop/read, print, cleanup.
      int eventSet = PAPI_NULL;
      papi_errno = PAPI_create_eventset(&eventSet);
      if (papi_errno != PAPI_OK) {
        // Hard failure to create an EventSet.
        NOTE("  ?  create_eventset failed: %s", PAPI_strerror(papi_errno));
        ++failed;
        ++event_index;
        goto advance_variant;
      }

      // Explicitly assign the component.
      papi_errno = PAPI_assign_eventset_component(eventSet, cid);
      if (papi_errno != PAPI_OK) {
        NOTE("  ?  assign_eventset_component failed: %s",
             PAPI_strerror(papi_errno));
        (void)PAPI_destroy_eventset(&eventSet);
        ++failed;
        ++event_index;
        goto advance_variant;
      }

      papi_errno = PAPI_add_event(eventSet, qualified_code);
      if (papi_errno != PAPI_OK) {
        if (is_warning_papi_errno(papi_errno)) {
          WARNF("Could not add %-50s (%s)", ev_name,
                PAPI_strerror(papi_errno));
          (void)PAPI_destroy_eventset(&eventSet);
          ++warned;
          ++event_index;
        } else {
          NOTE("  ?  Could not add %s (%s)", ev_name,
               PAPI_strerror(papi_errno));
          (void)PAPI_destroy_eventset(&eventSet);
          ++failed;
          ++event_index;
        }
        goto advance_variant;
      }

      long long value = 0;
      papi_errno = PAPI_start(eventSet);
      if (papi_errno != PAPI_OK) {
        if (is_warning_papi_errno(papi_errno)) {
          WARNF("start %-54s (%s)", ev_name, PAPI_strerror(papi_errno));
          (void)PAPI_cleanup_eventset(eventSet);
          (void)PAPI_destroy_eventset(&eventSet);
          ++warned;
          ++event_index;
        } else {
          NOTE("  ?  start failed for %s (%s)", ev_name,
               PAPI_strerror(papi_errno));
          (void)PAPI_cleanup_eventset(eventSet);
          (void)PAPI_destroy_eventset(&eventSet);
          ++failed;
          ++event_index;
        }
        goto advance_variant;
      }

      // Read once via stop().
      papi_errno = PAPI_stop(eventSet, &value);
      if (papi_errno != PAPI_OK) {
        if (is_warning_papi_errno(papi_errno)) {
          WARNF("stop  %-54s (%s)", ev_name, PAPI_strerror(papi_errno));
          ++warned;
        } else {
          NOTE("  ?  stop failed for %s (%s)", ev_name,
               PAPI_strerror(papi_errno));
          ++failed;
        }
        (void)PAPI_cleanup_eventset(eventSet);
        (void)PAPI_destroy_eventset(&eventSet);
        ++event_index;
        goto advance_variant;
      }

      // Success path.
      ++passed;
      if (opts.print) {
        printf("      %-60s = %lld\n\n", ev_name, value);
      }

      (void)PAPI_cleanup_eventset(eventSet);
      (void)PAPI_destroy_eventset(&eventSet);
      ++event_index;

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
           passed, warned, skipped, failed);
  }

  PAPI_shutdown();

  // Final: fail only if we had real failures; warnings/skips are allowed.
  int exit_status = (failed == 0) ? 0 : 1;
  return eval_result(opts, exit_status);
}
