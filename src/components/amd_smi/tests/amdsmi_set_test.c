/**
 * @file    amdsmi_set_test.c
 * @brief   Exercise AMD SMI writable controls via PAPI by toggling multiple
 *          settings (power cap, fan speed, PCIe mask, partitioning, etc.).
 */

#include "test_harness.h"
#include "amd_smi/amdsmi.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int eventset;
    int code;
    char name[PAPI_MAX_STR_LEN];
} event_handle_t;

static int canonicalize_event_name_or_warn(const char *base,
                                           char *output,
                                           size_t len) {
    int rc = harness_canonicalize_event_name(base, output, len);
    if (rc != PAPI_OK)
        return rc;
    return PAPI_OK;
}

static int prepare_event_handle(int cid, const char *base_name,
                                event_handle_t *handle) {
    if (!handle)
        return PAPI_EINVAL;

    memset(handle, 0, sizeof(*handle));
    handle->eventset = PAPI_NULL;

    int rc = canonicalize_event_name_or_warn(base_name, handle->name,
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

static int read_single_event(int cid, const char *base_name, long long *out) {
    if (!out)
        return PAPI_EINVAL;

    event_handle_t handle;
    int rc = prepare_event_handle(cid, base_name, &handle);
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

    if (rc != PAPI_OK)
        return rc;

    *out = value;
    return PAPI_OK;
}

static long long clamp_long_long(long long value, long long min_value, long long max_value) {
    if (value < min_value)
        return min_value;
    if (value > max_value)
        return max_value;
    return value;
}

static void log_papi_failure(const char *context, int rc) {
    WARNF("%s failed: %s", context, PAPI_strerror(rc));
}

static void test_power_cap(int cid, HarnessOpts opts) {
    event_handle_t handle;
    int rc = prepare_event_handle(cid, "amd_smi:::power_cap", &handle);
    if (rc != PAPI_OK) {
        EXIT_WARNING_ON_ADD(rc, "amd_smi:::power_cap");
        return;
    }

    long long min_cap = 0, max_cap = 0;
    int have_min = (read_single_event(cid, "amd_smi:::power_cap_range_min", &min_cap) == PAPI_OK);
    int have_max = (read_single_event(cid, "amd_smi:::power_cap_range_max", &max_cap) == PAPI_OK);

    rc = PAPI_start(handle.eventset);
    if (rc != PAPI_OK) {
        EXIT_WARNING_ON_START(rc, handle.name);
        destroy_event_handle(&handle);
        return;
    }

    long long original = 0;
    rc = PAPI_read(handle.eventset, &original);
    if (rc != PAPI_OK) {
        WARNF("Failed reading %s: %s", handle.name, PAPI_strerror(rc));
        goto done;
    }

    if (!have_min)
        min_cap = original;
    if (!have_max)
        max_cap = original;

    long long delta = original / 20;
    if (delta <= 0)
        delta = 1;
    long long target = original - delta;
    if (target < min_cap)
        target = min_cap;
    if (target == original && max_cap > original)
        target = original + delta;
    if (target > max_cap)
        target = max_cap;

    if (target == original) {
        WARNF("power_cap: unable to choose alternate value within range");
        goto done;
    }

    rc = PAPI_write(handle.eventset, &target);
    if (rc != PAPI_OK) {
        if (rc == PAPI_EPERM || rc == PAPI_ENOSUPP)
            WARNF("power_cap write unavailable (%s)", PAPI_strerror(rc));
        else
            log_papi_failure("PAPI_write(power_cap)", rc);
        goto done;
    }

    long long verify = 0;
    rc = PAPI_read(handle.eventset, &verify);
    if (rc == PAPI_OK)
        NOTE("power_cap set to %lld (requested %lld)", verify, target);

    rc = PAPI_write(handle.eventset, &original);
    if (rc != PAPI_OK)
        log_papi_failure("PAPI_write(power_cap restore)", rc);

    long long restore = 0;
    if (PAPI_read(handle.eventset, &restore) == PAPI_OK)
        NOTE("power_cap restored to %lld", restore);

  done:
    {
        long long stop_val = 0;
        (void)PAPI_stop(handle.eventset, &stop_val);
    }
    destroy_event_handle(&handle);
}

static void test_fan_speed(int cid, HarnessOpts opts) {
    event_handle_t handle;
    int rc = prepare_event_handle(cid, "amd_smi:::fan_speed_sensor=0", &handle);
    if (rc != PAPI_OK) {
        if (rc == PAPI_ENOEVNT)
            NOTE("Skipping fan_speed test: event unavailable");
        else
            EXIT_WARNING_ON_ADD(rc, "amd_smi:::fan_speed_sensor=0");
        return;
    }

    rc = PAPI_start(handle.eventset);
    if (rc != PAPI_OK) {
        if (rc == PAPI_ENOEVNT)
            NOTE("Skipping fan_speed test: start unavailable (ENOEVNT)");
        else
            EXIT_WARNING_ON_START(rc, handle.name);
        destroy_event_handle(&handle);
        return;
    }

    long long original = 0;
    rc = PAPI_read(handle.eventset, &original);
    if (rc != PAPI_OK) {
        log_papi_failure("PAPI_read(fan_speed)", rc);
        goto done;
    }

    long long target = original;
    if (original > 0)
        target = original - 1;
    if (target == original && original < 255)
        target = original + 1;
    target = clamp_long_long(target, 0, 255);

    if (target == original) {
        WARNF("fan_speed: unable to pick alternate value from %lld", original);
        goto done;
    }

    rc = PAPI_write(handle.eventset, &target);
    if (rc != PAPI_OK) {
        if (rc == PAPI_EPERM || rc == PAPI_ENOSUPP || rc == PAPI_ENOEVNT)
            WARNF("fan_speed write unavailable (%s)", PAPI_strerror(rc));
        else
            log_papi_failure("PAPI_write(fan_speed)", rc);
        goto done;
    }

    long long verify = 0;
    if (PAPI_read(handle.eventset, &verify) == PAPI_OK)
        NOTE("fan_speed now %lld (requested %lld)", verify, target);

    rc = PAPI_write(handle.eventset, &original);
    if (rc != PAPI_OK)
        log_papi_failure("PAPI_write(fan_speed restore)", rc);

  done:
    {
        long long stop_val = 0;
        (void)PAPI_stop(handle.eventset, &stop_val);
    }
    destroy_event_handle(&handle);
}

static void test_pci_bandwidth_mask(int cid, HarnessOpts opts) {
    event_handle_t handle;
    int rc = prepare_event_handle(cid, "amd_smi:::pci_bandwidth_mask", &handle);
    if (rc != PAPI_OK) {
        if (rc == PAPI_ENOEVNT)
            NOTE("Skipping pci_bandwidth_mask test: event unavailable");
        else
            EXIT_WARNING_ON_ADD(rc, "amd_smi:::pci_bandwidth_mask");
        return;
    }

    rc = PAPI_start(handle.eventset);
    if (rc != PAPI_OK) {
        if (rc == PAPI_ENOEVNT)
            NOTE("Skipping pci_bandwidth_mask test: start unavailable (ENOEVNT)");
        else
            EXIT_WARNING_ON_START(rc, handle.name);
        destroy_event_handle(&handle);
        return;
    }

    long long original = 0;
    rc = PAPI_read(handle.eventset, &original);
    if (rc != PAPI_OK || original == 0) {
        if (rc != PAPI_OK)
            log_papi_failure("PAPI_read(pci_bandwidth_mask)", rc);
        else
            WARNF("pci_bandwidth_mask reports zero mask; skipping");
        goto done;
    }

    uint64_t mask = (uint64_t)original;
    uint64_t lsb = mask & (~mask + 1ULL);
    uint64_t new_mask = (lsb && mask > lsb) ? lsb : (mask >> 1);
    if (new_mask == 0 || new_mask == mask)
        new_mask = mask;

    if (new_mask == mask) {
        WARNF("pci_bandwidth_mask: could not derive alternate mask");
        goto done;
    }

    long long write_val = (long long)new_mask;
    rc = PAPI_write(handle.eventset, &write_val);
    if (rc != PAPI_OK) {
        if (rc == PAPI_EPERM || rc == PAPI_ENOSUPP)
            WARNF("pci_bandwidth_mask write unavailable (%s)", PAPI_strerror(rc));
        else
            log_papi_failure("PAPI_write(pci_bandwidth_mask)", rc);
        goto done;
    }

    long long verify = 0;
    if (PAPI_read(handle.eventset, &verify) == PAPI_OK)
        NOTE("pci_bandwidth_mask now 0x%llx", (unsigned long long)verify);

    long long restore = original;
    rc = PAPI_write(handle.eventset, &restore);
    if (rc != PAPI_OK)
        log_papi_failure("PAPI_write(pci_bandwidth_mask restore)", rc);

  done:
    {
        long long stop_val = 0;
        (void)PAPI_stop(handle.eventset, &stop_val);
    }
    destroy_event_handle(&handle);
}

static void test_perf_determinism(int cid, HarnessOpts opts) {
    event_handle_t handle;
    int rc = prepare_event_handle(cid, "amd_smi:::perf_determinism_softmax", &handle);
    if (rc != PAPI_OK) {
        if (rc == PAPI_ENOEVNT)
            NOTE("Skipping perf_determinism test: event unavailable");
        else
            EXIT_WARNING_ON_ADD(rc, "amd_smi:::perf_determinism_softmax");
        return;
    }

    rc = PAPI_start(handle.eventset);
    if (rc != PAPI_OK) {
        if (rc == PAPI_ENOEVNT)
            NOTE("Skipping perf_determinism test: start unavailable (ENOEVNT)");
        else
            EXIT_WARNING_ON_START(rc, handle.name);
        destroy_event_handle(&handle);
        return;
    }

    long long current_softmax = 0;
    rc = PAPI_read(handle.eventset, &current_softmax);
    if (rc != PAPI_OK) {
        log_papi_failure("PAPI_read(perf_determinism)", rc);
        goto done;
    }

    long long limit_max = 0;
    if (read_single_event(cid, "amd_smi:::od_sclk_limit_max", &limit_max) != PAPI_OK ||
        limit_max == 0) {
        WARNF("perf_determinism: unable to query od_sclk_limit_max; skipping");
        goto done;
    }

    long long target = limit_max - 50;
    if (target <= 0)
        target = limit_max;
    if (target == current_softmax)
        target = limit_max;

    rc = PAPI_write(handle.eventset, &target);
    if (rc != PAPI_OK) {
        if (rc == PAPI_EPERM || rc == PAPI_ENOSUPP || rc == PAPI_ENOEVNT)
            WARNF("perf_determinism write unavailable (%s)", PAPI_strerror(rc));
        else
            log_papi_failure("PAPI_write(perf_determinism)", rc);
        goto done;
    }

    long long verify = 0;
    if (PAPI_read(handle.eventset, &verify) == PAPI_OK)
        NOTE("perf_determinism target set to %lld", verify);

    long long restore = -1;
    rc = PAPI_write(handle.eventset, &restore);
    if (rc != PAPI_OK)
        log_papi_failure("PAPI_write(perf_determinism restore)", rc);

  done:
    {
        long long stop_val = 0;
        (void)PAPI_stop(handle.eventset, &stop_val);
    }
    destroy_event_handle(&handle);
}

static void test_overdrive_level(int cid, HarnessOpts opts) {
    event_handle_t handle;
    int rc = prepare_event_handle(cid, "amd_smi:::gpu_overdrive_percent", &handle);
    if (rc != PAPI_OK) {
        EXIT_WARNING_ON_ADD(rc, "amd_smi:::gpu_overdrive_percent");
        return;
    }

    rc = PAPI_start(handle.eventset);
    if (rc != PAPI_OK) {
        EXIT_WARNING_ON_START(rc, handle.name);
        destroy_event_handle(&handle);
        return;
    }

    long long original = 0;
    rc = PAPI_read(handle.eventset, &original);
    if (rc != PAPI_OK) {
        log_papi_failure("PAPI_read(overdrive)", rc);
        goto done;
    }

    long long target = original;
    if (original >= 1)
        target = original - 1;
    if (target == original)
        target = original + 1;
    if (target < 0)
        target = 0;

    if (target == original) {
        WARNF("overdrive_level: unable to derive alternate value");
        goto done;
    }

    rc = PAPI_write(handle.eventset, &target);
    if (rc != PAPI_OK) {
        if (rc == PAPI_EPERM || rc == PAPI_ENOSUPP)
            WARNF("overdrive_level write unavailable (%s)", PAPI_strerror(rc));
        else
            log_papi_failure("PAPI_write(overdrive)", rc);
        goto done;
    }

    long long verify = 0;
    if (PAPI_read(handle.eventset, &verify) == PAPI_OK)
        NOTE("overdrive level now %lld", verify);

    rc = PAPI_write(handle.eventset, &original);
    if (rc != PAPI_OK)
        log_papi_failure("PAPI_write(overdrive restore)", rc);

  done:
    {
        long long stop_val = 0;
        (void)PAPI_stop(handle.eventset, &stop_val);
    }
    destroy_event_handle(&handle);
}

static void test_od_clk_limits(int cid, HarnessOpts opts) {
    const char *events[] = {
        "amd_smi:::od_sclk_limit_max",
        "amd_smi:::clk_range_sys_min",
        "amd_smi:::clk_range_sys_max"
    };
    const size_t count = sizeof(events) / sizeof(events[0]);

    for (size_t i = 0; i < count; ++i) {
        event_handle_t handle;
        int rc = prepare_event_handle(cid, events[i], &handle);
        if (rc != PAPI_OK) {
            EXIT_WARNING_ON_ADD(rc, events[i]);
            continue;
        }

        rc = PAPI_start(handle.eventset);
        if (rc != PAPI_OK) {
            EXIT_WARNING_ON_START(rc, handle.name);
            destroy_event_handle(&handle);
            continue;
        }

        long long original = 0;
        rc = PAPI_read(handle.eventset, &original);
        if (rc != PAPI_OK) {
            log_papi_failure("PAPI_read(od_clk)", rc);
            long long stop_val = 0;
            (void)PAPI_stop(handle.eventset, &stop_val);
            destroy_event_handle(&handle);
            continue;
        }

        long long target = original;
        if (original > 100)
            target = original - 50;
        if (target == original)
            target = original + 50;
        if (target < 0)
            target = original;

        if (target == original) {
            long long stop_val = 0;
            (void)PAPI_stop(handle.eventset, &stop_val);
            destroy_event_handle(&handle);
            continue;
        }

        rc = PAPI_write(handle.eventset, &target);
        if (rc != PAPI_OK) {
            if (rc == PAPI_EPERM || rc == PAPI_ENOSUPP)
                WARNF("%s write unavailable (%s)", handle.name, PAPI_strerror(rc));
            else
                log_papi_failure("PAPI_write(od_clk)", rc);
        } else {
            NOTE("%s set to %lld", handle.name, target);
            (void)PAPI_write(handle.eventset, &original);
        }

        long long stop_val = 0;
        (void)PAPI_stop(handle.eventset, &stop_val);
        destroy_event_handle(&handle);
    }
}

static void test_od_volt_point(int cid, HarnessOpts opts) {
    const char *freq_event = "amd_smi:::volt_curve_point_freq_point=0";
    const char *volt_event = "amd_smi:::volt_curve_point_volt_point=0";

    event_handle_t handle;
    int rc = prepare_event_handle(cid, freq_event, &handle);
    if (rc != PAPI_OK) {
        EXIT_WARNING_ON_ADD(rc, freq_event);
        return;
    }

    rc = PAPI_start(handle.eventset);
    if (rc != PAPI_OK) {
        EXIT_WARNING_ON_START(rc, handle.name);
        destroy_event_handle(&handle);
        return;
    }

    long long original = 0;
    rc = PAPI_read(handle.eventset, &original);
    if (rc == PAPI_OK) {
        (void)PAPI_write(handle.eventset, &original); /* set to same value */
    }

    long long stop_val = 0;
    (void)PAPI_stop(handle.eventset, &stop_val);
    destroy_event_handle(&handle);

    rc = prepare_event_handle(cid, volt_event, &handle);
    if (rc != PAPI_OK) {
        EXIT_WARNING_ON_ADD(rc, volt_event);
        return;
    }
    rc = PAPI_start(handle.eventset);
    if (rc == PAPI_OK) {
        long long orig = 0;
        if (PAPI_read(handle.eventset, &orig) == PAPI_OK)
            (void)PAPI_write(handle.eventset, &orig);
        (void)PAPI_stop(handle.eventset, &stop_val);
    }
    destroy_event_handle(&handle);
}

static amdsmi_compute_partition_type_t next_compute_partition(amdsmi_compute_partition_type_t current) {
    switch (current) {
        case AMDSMI_COMPUTE_PARTITION_SPX: return AMDSMI_COMPUTE_PARTITION_DPX;
        case AMDSMI_COMPUTE_PARTITION_DPX: return AMDSMI_COMPUTE_PARTITION_TPX;
        case AMDSMI_COMPUTE_PARTITION_TPX: return AMDSMI_COMPUTE_PARTITION_QPX;
        case AMDSMI_COMPUTE_PARTITION_QPX: return AMDSMI_COMPUTE_PARTITION_CPX;
        default: return AMDSMI_COMPUTE_PARTITION_SPX;
    }
}

static void test_compute_partition(int cid, HarnessOpts opts) {
    event_handle_t handle;
    int rc = prepare_event_handle(cid, "amd_smi:::compute_partition", &handle);
    if (rc != PAPI_OK) {
        EXIT_WARNING_ON_ADD(rc, "amd_smi:::compute_partition");
        return;
    }

    rc = PAPI_start(handle.eventset);
    if (rc != PAPI_OK) {
        EXIT_WARNING_ON_START(rc, handle.name);
        destroy_event_handle(&handle);
        return;
    }

    long long original = 0;
    rc = PAPI_read(handle.eventset, &original);
    if (rc != PAPI_OK) {
        log_papi_failure("PAPI_read(compute_partition)", rc);
        goto done;
    }

    amdsmi_compute_partition_type_t current = (amdsmi_compute_partition_type_t)original;
    amdsmi_compute_partition_type_t desired = next_compute_partition(current);
    if (desired == current)
        desired = AMDSMI_COMPUTE_PARTITION_SPX;

    long long write_val = (long long)desired;
    rc = PAPI_write(handle.eventset, &write_val);
    if (rc != PAPI_OK) {
        if (rc == PAPI_EPERM || rc == PAPI_ENOSUPP)
            WARNF("compute_partition write unavailable (%s)", PAPI_strerror(rc));
        else
            log_papi_failure("PAPI_write(compute_partition)", rc);
        goto done;
    }

    rc = PAPI_write(handle.eventset, &original);
    if (rc != PAPI_OK)
        log_papi_failure("PAPI_write(compute_partition restore)", rc);

  done:
    {
        long long stop_val = 0;
        (void)PAPI_stop(handle.eventset, &stop_val);
    }
    destroy_event_handle(&handle);
}

static amdsmi_memory_partition_type_t next_memory_partition(amdsmi_memory_partition_type_t current) {
    switch (current) {
        case AMDSMI_MEMORY_PARTITION_NPS1: return AMDSMI_MEMORY_PARTITION_NPS2;
        case AMDSMI_MEMORY_PARTITION_NPS2: return AMDSMI_MEMORY_PARTITION_NPS4;
        case AMDSMI_MEMORY_PARTITION_NPS4: return AMDSMI_MEMORY_PARTITION_NPS8;
        default: return AMDSMI_MEMORY_PARTITION_NPS1;
    }
}

static void test_memory_partition(int cid, HarnessOpts opts) {
    event_handle_t handle;
    int rc = prepare_event_handle(cid, "amd_smi:::memory_partition", &handle);
    if (rc != PAPI_OK) {
        EXIT_WARNING_ON_ADD(rc, "amd_smi:::memory_partition");
        return;
    }

    rc = PAPI_start(handle.eventset);
    if (rc != PAPI_OK) {
        EXIT_WARNING_ON_START(rc, handle.name);
        destroy_event_handle(&handle);
        return;
    }

    long long original = 0;
    rc = PAPI_read(handle.eventset, &original);
    if (rc != PAPI_OK) {
        log_papi_failure("PAPI_read(memory_partition)", rc);
        goto done;
    }

    amdsmi_memory_partition_type_t current =
        (amdsmi_memory_partition_type_t)original;
    amdsmi_memory_partition_type_t desired = next_memory_partition(current);
    long long write_val = (long long)desired;

    rc = PAPI_write(handle.eventset, &write_val);
    if (rc != PAPI_OK) {
        if (rc == PAPI_EPERM || rc == PAPI_ENOSUPP)
            WARNF("memory_partition write unavailable (%s)", PAPI_strerror(rc));
        else
            log_papi_failure("PAPI_write(memory_partition)", rc);
        goto done;
    }

    rc = PAPI_write(handle.eventset, &original);
    if (rc != PAPI_OK)
        log_papi_failure("PAPI_write(memory_partition restore)", rc);

  done:
    {
        long long stop_val = 0;
        (void)PAPI_stop(handle.eventset, &stop_val);
    }
    destroy_event_handle(&handle);
}

static void test_accelerator_partition_profile(int cid, HarnessOpts opts) {
    event_handle_t handle;
    int rc = prepare_event_handle(cid, "amd_smi:::accelerator_partition_profile",
                                  &handle);
    if (rc != PAPI_OK) {
        if (rc == PAPI_ENOEVNT)
            NOTE("Skipping accelerator_partition_profile test: event unavailable");
        else
            EXIT_WARNING_ON_ADD(rc, "amd_smi:::accelerator_partition_profile");
        return;
    }

    rc = PAPI_start(handle.eventset);
    if (rc != PAPI_OK) {
        if (rc == PAPI_ENOEVNT)
            NOTE("Skipping accelerator_partition_profile test: start unavailable (ENOEVNT)");
        else
            EXIT_WARNING_ON_START(rc, handle.name);
        destroy_event_handle(&handle);
        return;
    }

    long long original = 0;
    rc = PAPI_read(handle.eventset, &original);
    if (rc != PAPI_OK) {
        log_papi_failure("PAPI_read(accelerator_partition_profile)", rc);
        goto done;
    }

    long long desired = original + 1;
    if (desired > original + 3)
        desired = original;

    rc = PAPI_write(handle.eventset, &desired);
    if (rc != PAPI_OK) {
        if (rc == PAPI_EPERM || rc == PAPI_ENOSUPP || rc == PAPI_ENOEVNT)
            WARNF("accelerator_partition_profile write unavailable (%s)",
                  PAPI_strerror(rc));
        else
            log_papi_failure("PAPI_write(accelerator_partition_profile)", rc);
        goto done;
    }

    rc = PAPI_write(handle.eventset, &original);
    if (rc != PAPI_OK)
        log_papi_failure("PAPI_write(accelerator_partition_profile restore)", rc);

  done:
    {
        long long stop_val = 0;
        (void)PAPI_stop(handle.eventset, &stop_val);
    }
    destroy_event_handle(&handle);
}

static void test_xgmi_reset(int cid, HarnessOpts opts) {
    long long status = 0;
    int rc = read_single_event(cid, "amd_smi:::xgmi_error_status", &status);
    if (rc != PAPI_OK) {
        WARNF("Unable to read xgmi_error_status: %s", PAPI_strerror(rc));
    }

    event_handle_t handle;
    rc = prepare_event_handle(cid, "amd_smi:::xgmi_error_reset", &handle);
    if (rc != PAPI_OK) {
        EXIT_WARNING_ON_ADD(rc, "amd_smi:::xgmi_error_reset");
        return;
    }

    rc = PAPI_start(handle.eventset);
    if (rc != PAPI_OK) {
        EXIT_WARNING_ON_START(rc, handle.name);
        destroy_event_handle(&handle);
        return;
    }

    long long value = 1;
    rc = PAPI_write(handle.eventset, &value);
    if (rc != PAPI_OK) {
        if (rc == PAPI_EPERM || rc == PAPI_ENOSUPP)
            WARNF("xgmi_error_reset write unavailable (%s)", PAPI_strerror(rc));
        else
            log_papi_failure("PAPI_write(xgmi_error_reset)", rc);
    }

    long long stop_val = 0;
    (void)PAPI_stop(handle.eventset, &stop_val);
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

    if (cid < 0) {
        SKIP("AMD SMI component not available");
    }

    test_power_cap(cid, opts);
    test_fan_speed(cid, opts);
    test_pci_bandwidth_mask(cid, opts);
    test_perf_determinism(cid, opts);
    test_overdrive_level(cid, opts);
    test_od_clk_limits(cid, opts);
    test_od_volt_point(cid, opts);
    test_compute_partition(cid, opts);
    test_memory_partition(cid, opts);
    test_accelerator_partition_profile(cid, opts);
    test_xgmi_reset(cid, opts);

    PAPI_shutdown();
    return eval_result(opts, 0);
}
