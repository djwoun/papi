/**
 * @file    amdsmi_set_test_powercap.c
 * @brief   Legacy standalone test for the AMD_SMI power_cap control. It mirrors
 *          the original harness we used before consolidating the write tests,
 *          including the ordering fixes (read min/max before starting the
 *          writable EventSet, etc.).
 */

#include "test_harness.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int eventset;
    int code;
    char name[PAPI_MAX_STR_LEN];
} event_handle_t;

static int canonicalize_event_name(const char *input,
                                   char *output,
                                   size_t len) {
    return harness_canonicalize_event_name(input, output, len);
}

static int prepare_event_handle(int cid, const char *event_name,
                                event_handle_t *handle) {
    if (!handle)
        return PAPI_EINVAL;

    memset(handle, 0, sizeof(*handle));
    handle->eventset = PAPI_NULL;

    int rc = canonicalize_event_name(event_name, handle->name,
                                     sizeof(handle->name));
    if (rc != PAPI_OK)
        return rc;

    rc = PAPI_create_eventset(&handle->eventset);
    if (rc != PAPI_OK)
        return rc;

    rc = PAPI_assign_eventset_component(handle->eventset, cid);
    if (rc != PAPI_OK)
        goto fail;

    rc = PAPI_event_name_to_code(handle->name, &handle->code);
    if (rc != PAPI_OK)
        goto fail;

    rc = PAPI_add_event(handle->eventset, handle->code);
    if (rc != PAPI_OK)
        goto fail;

    return PAPI_OK;

fail:
    if (handle->eventset != PAPI_NULL) {
        (void)PAPI_cleanup_eventset(handle->eventset);
        (void)PAPI_destroy_eventset(&handle->eventset);
        handle->eventset = PAPI_NULL;
    }
    return rc;
}

static void destroy_event_handle(event_handle_t *handle) {
    if (!handle)
        return;
    if (handle->eventset != PAPI_NULL) {
        (void)PAPI_cleanup_eventset(handle->eventset);
        (void)PAPI_destroy_eventset(&handle->eventset);
        handle->eventset = PAPI_NULL;
    }
}

static int read_scalar_event(int cid, const char *event_name, long long *out) {
    if (!out)
        return PAPI_EINVAL;

    event_handle_t handle;
    int rc = prepare_event_handle(cid, event_name, &handle);
    if (rc != PAPI_OK)
        return rc;

    rc = PAPI_start(handle.eventset);
    if (rc != PAPI_OK) {
        destroy_event_handle(&handle);
        return rc;
    }

    long long value = 0;
    rc = PAPI_read(handle.eventset, &value);
    long long stop_val = 0;
    (void)PAPI_stop(handle.eventset, &stop_val);
    destroy_event_handle(&handle);

    if (rc == PAPI_OK)
        *out = value;
    return rc;
}

static void test_power_cap(int cid, HarnessOpts opts) {
    (void)opts;

    event_handle_t handle;
    int rc = prepare_event_handle(cid, "amd_smi:::power_cap", &handle);
    if (rc != PAPI_OK) {
        EXIT_WARNING_ON_ADD(rc, "amd_smi:::power_cap");
        return;
    }

    long long min_cap = 0;
    rc = read_scalar_event(cid, "amd_smi:::power_cap_range_min", &min_cap);
    if (rc != PAPI_OK)
        min_cap = 0;

    long long max_cap = 0;
    rc = read_scalar_event(cid, "amd_smi:::power_cap_range_max", &max_cap);
    if (rc != PAPI_OK)
        max_cap = 0;

    rc = PAPI_start(handle.eventset);
    if (rc != PAPI_OK) {
        EXIT_WARNING_ON_START(rc, handle.name);
        destroy_event_handle(&handle);
        return;
    }

    long long original = 0;
    rc = PAPI_read(handle.eventset, &original);
    if (rc != PAPI_OK) {
        WARNF("Failed to read %s: %s", handle.name, PAPI_strerror(rc));
        goto done;
    }

    long long target = original;
    long long delta = original / 20;
    if (delta <= 0)
        delta = 1;

    if (min_cap > 0 && max_cap > min_cap) {
        target = original - delta;
        if (target < min_cap)
            target = min_cap;
        if (target == original && max_cap > original)
            target = original + delta;
        if (target > max_cap)
            target = max_cap;
    } else {
        if (original > delta)
            target = original - delta;
        else if (max_cap > original)
            target = original + delta;
    }

    if (target == original) {
        WARNF("power_cap: unable to choose alternate value (orig=%lld)", original);
        goto done;
    }

    rc = PAPI_write(handle.eventset, &target);
    if (rc != PAPI_OK) {
        WARNF("power_cap write failed: %s", PAPI_strerror(rc));
        goto done;
    }

    long long verify = 0;
    if (PAPI_read(handle.eventset, &verify) == PAPI_OK)
        NOTE("power_cap changed from %lld to %lld", original, verify);

    rc = PAPI_write(handle.eventset, &original);
    if (rc != PAPI_OK)
        WARNF("power_cap restore failed: %s", PAPI_strerror(rc));
    else {
        long long restored = 0;
        if (PAPI_read(handle.eventset, &restored) == PAPI_OK)
            NOTE("power_cap restored to %lld", restored);
    }

  done:
    {
        long long stop_val = 0;
        (void)PAPI_stop(handle.eventset, &stop_val);
    }
    destroy_event_handle(&handle);
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);

    harness_accept_tests_quiet(&argc, argv);
    HarnessOpts opts = parse_harness_cli(argc, argv);

    int rc = PAPI_library_init(PAPI_VER_CURRENT);
    if (rc != PAPI_VER_CURRENT) {
        WARNF("PAPI_library_init failed: %s", PAPI_strerror(rc));
        return eval_result(opts, 1);
    }

    int cid = -1;
    int num_comps = PAPI_num_components();
    for (int i = 0; i < num_comps; ++i) {
        const PAPI_component_info_t *cinfo = PAPI_get_component_info(i);
        if (cinfo && strcmp(cinfo->name, "amd_smi") == 0) {
            cid = i;
            break;
        }
    }

    if (cid < 0)
        SKIP("AMD SMI component not available");

    test_power_cap(cid, opts);

    PAPI_shutdown();
    return eval_result(opts, 0);
}
