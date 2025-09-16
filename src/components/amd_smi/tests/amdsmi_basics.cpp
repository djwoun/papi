/**
 * @file    amdsmi_basics.c
 * @author  Dong Jun Woun 
 *          djwoun@gmail.com
 *          Enumerates every native AMD-SMI event exposed through PAPI and measures
 *          them one at a time.
 */
 
 
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "papi.h"
#include "test_harness.hpp"

// Return true if rc is a "warning, not failure" status for add/start/stop.
static inline bool is_warning_rc(int rc) {
  return (rc == PAPI_ENOEVNT) || (rc == PAPI_ECNFLCT) || (rc == PAPI_EPERM);
}

int main(int argc, char *argv[]) {
  // Unbuffer stdout so the final status line shows promptly.
   setvbuf(stdout, nullptr, _IONBF, 0);

  harness_accept_tests_quiet(&argc, argv);
  auto opts = parse_harness_cli(argc, argv);

  // 1. Initialise PAPI
  int ret = PAPI_library_init(PAPI_VER_CURRENT);
  if (ret != PAPI_VER_CURRENT) {
    NOTE("PAPI_library_init failed: %s", PAPI_strerror(ret));
    return eval_result(opts, 1);
  }

  // 2. Locate the AMD-SMI component
  int cid = -1;
  const int ncomps = PAPI_num_components();
  for (int i = 0; i < ncomps && cid < 0; ++i) {
    const PAPI_component_info_t *cinfo = PAPI_get_component_info(i);
    if (cinfo && std::strcmp(cinfo->name, "amd_smi") == 0) {
      cid = i;
    }
  }
  if (cid < 0) {
    // Can't conduct on this build/platform ¡æ pass with warning.
    SKIP("Unable to locate the amd_smi component (PAPI built without ROCm?)");
  }

  NOTE("Using AMD-SMI component id %d\n", cid);

  // 3. Enumerate every native event
  int ev_code = PAPI_NATIVE_MASK;
  if (PAPI_enum_cmp_event(&ev_code, PAPI_ENUM_FIRST, cid) != PAPI_OK) {
    // No events ¡æ treat as ¡°nothing to do¡± (warning instead of failing)
    SKIP("No native events found for AMD-SMI component");
  }

  int event_index = 0;
  int passed = 0, warned = 0, failed = 0, skipped = 0;

  do {
    char ev_name[PAPI_MAX_STR_LEN]{};
    if (PAPI_event_code_to_name(ev_code, ev_name) != PAPI_OK) {
      // Shouldn't happen; skip silently
      ++skipped;
      continue;
    }

    // Preserve your original skip for process* events
    if (std::strncmp(ev_name, "amd_smi:::process", 17) == 0 ||
        std::strncmp(ev_name, "process", 7) == 0) {
      ++skipped;
      NOTE("[%4d] Skipping %s (process events not testable)\n", event_index++, ev_name);
      continue;
    }

    NOTE("[%4d] Testing %s...", event_index, ev_name);

    // 4-7.  Create a fresh EventSet, read the event, print, cleanup
    int eventSet = PAPI_NULL;
    ret = PAPI_create_eventset(&eventSet);
    if (ret != PAPI_OK) {
      // Hard failure to create an EventSet
      NOTE("  ?  create_eventset failed: %s", PAPI_strerror(ret));
      ++failed; ++event_index;
      continue;
    }

    // Keep original explicit assignment to the component
    ret = PAPI_assign_eventset_component(eventSet, cid);
    if (ret != PAPI_OK) {
      NOTE("  ?  assign_eventset_component failed: %s", PAPI_strerror(ret));
      (void)PAPI_destroy_eventset(&eventSet);
      ++failed; ++event_index;
      continue;
    }

    ret = PAPI_add_event(eventSet, ev_code);
    if (ret != PAPI_OK) {
      if (is_warning_rc(ret)) {
        WARNF("Could not add %-50s (%s)", ev_name, PAPI_strerror(ret));
        (void)PAPI_destroy_eventset(&eventSet);
        ++warned; ++event_index;
      } else {
        NOTE("  ?  Could not add %s (%s)", ev_name, PAPI_strerror(ret));
        (void)PAPI_destroy_eventset(&eventSet);
        ++failed; ++event_index;
      }
      continue;
    }

    long long value = 0;
    ret = PAPI_start(eventSet);
    if (ret != PAPI_OK) {
      if (is_warning_rc(ret)) {
        WARNF("start %-54s (%s)", ev_name, PAPI_strerror(ret));
        (void)PAPI_cleanup_eventset(eventSet);
        (void)PAPI_destroy_eventset(&eventSet);
        ++warned; ++event_index;
      } else {
        NOTE("  ?  start failed for %s (%s)", ev_name, PAPI_strerror(ret));
        (void)PAPI_cleanup_eventset(eventSet);
        (void)PAPI_destroy_eventset(&eventSet);
        ++failed; ++event_index;
      }
      continue;
    }

    // Read once via stop (same as original)
    ret = PAPI_stop(eventSet, &value);
    if (ret != PAPI_OK) {
      if (is_warning_rc(ret)) {
        WARNF("stop  %-54s (%s)", ev_name, PAPI_strerror(ret));
        ++warned;
      } else {
        NOTE("  ?  stop failed for %s (%s)", ev_name, PAPI_strerror(ret));
        ++failed;
      }
      (void)PAPI_cleanup_eventset(eventSet);
      (void)PAPI_destroy_eventset(&eventSet);
      ++event_index;
      continue;
    }

    // Success path
    ++passed;
    if (opts.print) {
      printf("      %-60s = %lld\n\n", ev_name, value);
    }

    (void)PAPI_cleanup_eventset(eventSet);
    (void)PAPI_destroy_eventset(&eventSet);
    ++event_index;

  } while (PAPI_enum_cmp_event(&ev_code, PAPI_ENUM_EVENTS, cid) == PAPI_OK);

  if (opts.print) {
    printf("Summary: passed=%d  warned=%d  skipped=%d  failed=%d\n",
           passed, warned, skipped, failed);
  }

  PAPI_shutdown();

  // Final: fail only if we had real failures; warnings/skips are allowed.
  int rc = (failed == 0) ? 0 : 1;
  return eval_result(opts, rc);
}
