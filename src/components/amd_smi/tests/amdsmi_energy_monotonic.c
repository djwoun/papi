/**
 * @file    amdsmi_energy_monotonic.c
 * @author  Dong Jun Woun 
 *          djwoun@gmail.com
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "papi.h"
#include "test_harness.h"

int main(int argc, char **argv) {
    harness_accept_tests_quiet(&argc, argv);
    HarnessOpts opts = parse_harness_cli(argc, argv);

    const char* root = getenv("PAPI_AMDSMI_ROOT");
    if (!root || !*root) {
        SKIP("PAPI_AMDSMI_ROOT not set");
    }

    int ret = PAPI_library_init(PAPI_VER_CURRENT);
    if (ret != PAPI_VER_CURRENT) {
        NOTE("PAPI_library_init failed: %s", PAPI_strerror(ret));
        return eval_result(opts, 1);
    }

    int EventSet = PAPI_NULL;
    ret = PAPI_create_eventset(&EventSet);
    if (ret != PAPI_OK) {
        NOTE("PAPI_create_eventset: %s", PAPI_strerror(ret));
        return eval_result(opts, 1);
    }

    const char *ev = "amd_smi:::energy_consumed:device=0";
    ret = PAPI_add_named_event(EventSet, ev);
    if (ret == PAPI_ENOEVNT) {
        SKIP("energy_consumed:device=0 not supported");
    } else if (ret != PAPI_OK) {
        NOTE("PAPI_add_named_event(%s): %s", ev, PAPI_strerror(ret));
        return eval_result(opts, 1);
    }

    ret = PAPI_start(EventSet);
    if (ret != PAPI_OK) {
        NOTE("PAPI_start: %s", PAPI_strerror(ret));
        return eval_result(opts, 1);
    }

    long long v1 = 0, v2 = 0;
    ret = PAPI_read(EventSet, &v1);
    if (ret != PAPI_OK) {
        NOTE("PAPI_read(1): %s", PAPI_strerror(ret));
        long long dummy=0; PAPI_stop(EventSet, &dummy);
        return eval_result(opts, 1);
    }

    // Try up to 1 second for the energy counter to advance
    for (int i = 0; i < 10; ++i) {
        usleep(100000);

        ret = PAPI_read(EventSet, &v2);
        if (ret != PAPI_OK) {
            NOTE("PAPI_read(2): %s", PAPI_strerror(ret));
            long long dummy=0; PAPI_stop(EventSet, &dummy);
            return eval_result(opts, 1);
        }
        if (v2 > v1) break;
    }

    long long dummy=0;
    PAPI_stop(EventSet, &dummy);
    PAPI_cleanup_eventset(EventSet);
    PAPI_destroy_eventset(&EventSet);

    if (opts.print) {
        printf("energy_consumed: first=%lld  second=%lld  delta=%lld\n", v1, v2, (v2 - v1));
    }

    int failed = (v2 <= v1) ? 1 : 0;
    if (failed) NOTE("Energy did not increase");
    return eval_result(opts, failed);
}
