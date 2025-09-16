/**
 * @file    amdsmi_hello.c
 * @author  Dong Jun Woun 
 *          djwoun@gmail.com
 *
 */
#include "test_harness.h"

#include "papi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv) {
    // Unbuffer stdout so the final status line always shows.
    setvbuf(stdout, NULL, _IONBF, 0);

    harness_accept_tests_quiet(&argc, argv);
    HarnessOpts opts = parse_harness_cli(argc, argv);

    // Default event (can override via argv[1], e.g. "./amdsmi_hello amd_smi:::power_average:device=0")
    const char* ev = "amd_smi:::temp_current:device=0:sensor=1";
    if (argc > 1 && strncmp(argv[1], "--", 2) != 0) ev = argv[1];

    // Require AMD SMI root so the component can dlopen the library
    const char* root = getenv("PAPI_AMDSMI_ROOT");
    if (!root || !*root) {
        SKIP("PAPI_AMDSMI_ROOT not set");
    }

    // Init PAPI
    int rc = PAPI_library_init(PAPI_VER_CURRENT);
    if (rc != PAPI_VER_CURRENT) {
        NOTE("PAPI_library_init failed: %s", PAPI_strerror(rc));
        return eval_result(opts, 1);
    }

    // Create an EventSet
    int EventSet = PAPI_NULL;
    rc = PAPI_create_eventset(&EventSet);
    if (rc != PAPI_OK) {
        NOTE("PAPI_create_eventset: %s", PAPI_strerror(rc));
        return eval_result(opts, 1);
    }

    // Add event
    rc = PAPI_add_named_event(EventSet, ev);
    if (rc == PAPI_ENOEVNT || rc == PAPI_ECNFLCT || rc == PAPI_EPERM) {
        NOTE("Event unavailable or HW/resource-limited: %s (%s)", ev, PAPI_strerror(rc));
        SKIP("Event unavailable or HW/resource-limited");
    } else if (rc != PAPI_OK) {
        NOTE("PAPI_add_named_event(%s): %s", ev, PAPI_strerror(rc));
        PAPI_destroy_eventset(&EventSet);
        return eval_result(opts, 1);
    }

    // Start | short wait | stop/read
    rc = PAPI_start(EventSet);
    if (rc == PAPI_ECNFLCT || rc == PAPI_EPERM) {
        NOTE("Cannot start counters: %s", PAPI_strerror(rc));
        SKIP("Cannot start counters");
    } else if (rc != PAPI_OK) {
        NOTE("PAPI_start: %s", PAPI_strerror(rc));
        PAPI_destroy_eventset(&EventSet);
        return eval_result(opts, 1);
    }

    usleep(100000); // ~100ms
    long long val = 0;
    rc = PAPI_stop(EventSet, &val);
    if (rc != PAPI_OK) {
        NOTE("PAPI_stop: %s", PAPI_strerror(rc));
        PAPI_destroy_eventset(&EventSet);
        return eval_result(opts, 1);
    }

    (void)PAPI_cleanup_eventset(EventSet);
    (void)PAPI_destroy_eventset(&EventSet);
    PAPI_shutdown();

    if (opts.print) {
        printf("Event: %s\nValue: %lld\n", ev, val);
    }
    return eval_result(opts, 0);
}
