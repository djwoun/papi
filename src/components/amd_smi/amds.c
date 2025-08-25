#include <string.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include "amdsmi.h"
#include <inttypes.h>
#include "papi.h"
#include "papi_memory.h"
#include "amds.h"
#include "htable.h"
#define AMDSMI_DISABLE_ESMI

unsigned int _amd_smi_lock;
typedef enum {
    PAPI_MODE_READ = 1,
    PAPI_MODE_WRITE,
    PAPI_MODE_RDWR,
} rocs_access_mode_e;
/* Pointers to AMD SMI library functions (dynamically loaded) */
static amdsmi_status_t (*amdsmi_init_p)(uint64_t);
static amdsmi_status_t (*amdsmi_shut_down_p)(void);
static amdsmi_status_t (*amdsmi_get_socket_handles_p)(uint32_t *, amdsmi_socket_handle *);
static amdsmi_status_t (*amdsmi_get_processor_handles_by_type_p)(amdsmi_socket_handle, processor_type_t, amdsmi_processor_handle *, uint32_t *);
static amdsmi_status_t (*amdsmi_get_temp_metric_p)(amdsmi_processor_handle, amdsmi_temperature_type_t, amdsmi_temperature_metric_t, int64_t *);
static amdsmi_status_t (*amdsmi_get_gpu_fan_rpms_p)(amdsmi_processor_handle, uint32_t, int64_t *);
static amdsmi_status_t (*amdsmi_get_gpu_fan_speed_p)(amdsmi_processor_handle, uint32_t, int64_t *);
static amdsmi_status_t (*amdsmi_get_gpu_fan_speed_max_p)(amdsmi_processor_handle, uint32_t, int64_t *);
static amdsmi_status_t (*amdsmi_get_total_memory_p)(amdsmi_processor_handle, amdsmi_memory_type_t, uint64_t *);
static amdsmi_status_t (*amdsmi_get_memory_usage_p)(amdsmi_processor_handle, amdsmi_memory_type_t, uint64_t *);
static amdsmi_status_t (*amdsmi_get_gpu_activity_p)(amdsmi_processor_handle, amdsmi_engine_usage_t *);
static amdsmi_status_t (*amdsmi_get_power_cap_info_p)(amdsmi_processor_handle, uint32_t, amdsmi_power_cap_info_t *);
static amdsmi_status_t (*amdsmi_get_gpu_power_cap_set_p)(amdsmi_processor_handle, uint32_t, uint64_t);
static amdsmi_status_t (*amdsmi_get_power_info_p)(amdsmi_processor_handle, amdsmi_power_info_t *);
static amdsmi_status_t (*amdsmi_set_power_cap_p)(amdsmi_processor_handle, uint32_t, uint64_t);
static amdsmi_status_t (*amdsmi_get_gpu_pci_throughput_p)(amdsmi_processor_handle, uint64_t *, uint64_t *, uint64_t *);
static amdsmi_status_t (*amdsmi_get_gpu_pci_replay_counter_p)(amdsmi_processor_handle, uint64_t *);
static amdsmi_status_t (*amdsmi_get_clk_freq_p)(amdsmi_processor_handle, amdsmi_clk_type_t, amdsmi_frequencies_t *);
static amdsmi_status_t (*amdsmi_set_clk_freq_p)(amdsmi_processor_handle, amdsmi_clk_type_t, uint64_t);
static amdsmi_status_t (*amdsmi_get_gpu_metrics_info_p)(amdsmi_processor_handle, amdsmi_gpu_metrics_t *);
/* New AMD SMI function pointers */
static amdsmi_status_t (*amdsmi_get_gpu_id_p)(amdsmi_processor_handle, uint16_t *);
static amdsmi_status_t (*amdsmi_get_gpu_revision_p)(amdsmi_processor_handle, uint16_t *);
static amdsmi_status_t (*amdsmi_get_gpu_subsystem_id_p)(amdsmi_processor_handle, uint16_t *);
//static amdsmi_status_t (*amdsmi_get_gpu_virtualization_mode_p)(amdsmi_processor_handle, amdsmi_virtualization_mode_t *);
static amdsmi_status_t (*amdsmi_get_gpu_pci_bandwidth_p)(amdsmi_processor_handle, amdsmi_pcie_bandwidth_t *);
static amdsmi_status_t (*amdsmi_get_gpu_bdf_id_p)(amdsmi_processor_handle, uint64_t *);
static amdsmi_status_t (*amdsmi_get_gpu_topo_numa_affinity_p)(amdsmi_processor_handle, int32_t *);
static amdsmi_status_t (*amdsmi_get_energy_count_p)(amdsmi_processor_handle, uint64_t *, float *, uint64_t *);
static amdsmi_status_t (*amdsmi_get_gpu_power_profile_presets_p)(amdsmi_processor_handle, uint32_t, amdsmi_power_profile_status_t *);
static amdsmi_status_t (*amdsmi_get_socket_info_p)(amdsmi_socket_handle, size_t, char *);
static amdsmi_status_t (*amdsmi_get_processor_type_p)(amdsmi_processor_handle, processor_type_t *);
static amdsmi_status_t (*amdsmi_get_processor_handle_from_bdf_p)(amdsmi_bdf_t, amdsmi_processor_handle *);
static amdsmi_status_t (*amdsmi_get_gpu_device_bdf_p)(amdsmi_processor_handle, amdsmi_bdf_t *);
static amdsmi_status_t (*amdsmi_get_gpu_device_uuid_p)(amdsmi_processor_handle, char *, size_t);
static amdsmi_status_t (*amdsmi_get_gpu_enumeration_info_p)(amdsmi_enumeration_info_t *);
static amdsmi_status_t (*amdsmi_get_gpu_vendor_name_p)(amdsmi_processor_handle, char *, size_t);
static amdsmi_status_t (*amdsmi_get_gpu_vram_vendor_p)(amdsmi_processor_handle, char *, size_t);
static amdsmi_status_t (*amdsmi_get_gpu_subsystem_name_p)(amdsmi_processor_handle, char *, size_t);
static amdsmi_status_t (*amdsmi_get_gpu_bad_page_info_p)(amdsmi_processor_handle, amdsmi_retired_page_record_t *, uint32_t *);
static amdsmi_status_t (*amdsmi_get_gpu_bad_page_threshold_p)(amdsmi_processor_handle, uint32_t *);
static amdsmi_status_t (*amdsmi_get_gpu_ras_feature_info_p)(amdsmi_processor_handle, amdsmi_ras_feature_t *);
static amdsmi_status_t (*amdsmi_get_gpu_ras_block_features_enabled_p)(amdsmi_processor_handle, uint64_t *);
static amdsmi_status_t (*amdsmi_get_gpu_memory_reserved_pages_p)(amdsmi_processor_handle, uint32_t *);
static amdsmi_status_t (*amdsmi_get_gpu_volt_metric_p)(amdsmi_processor_handle, amdsmi_voltage_type_t, amdsmi_voltage_metric_t, int64_t *);
static amdsmi_status_t (*amdsmi_get_gpu_perf_level_p)(amdsmi_processor_handle, amdsmi_dev_perf_level_t *);
static amdsmi_status_t (*amdsmi_get_gpu_overdrive_level_p)(amdsmi_processor_handle, uint32_t *);
static amdsmi_status_t (*amdsmi_get_gpu_mem_overdrive_level_p)(amdsmi_processor_handle, uint32_t *);
static amdsmi_status_t (*amdsmi_get_gpu_od_volt_info_p)(amdsmi_processor_handle, void *);
static amdsmi_status_t (*amdsmi_get_gpu_metrics_header_info_p)(amdsmi_processor_handle, amd_metrics_table_header_t *);
static amdsmi_status_t (*amdsmi_get_gpu_pm_metrics_info_p)(amdsmi_processor_handle, amdsmi_gpu_metrics_t *);
static amdsmi_status_t (*amdsmi_get_gpu_reg_table_info_p)(amdsmi_processor_handle, void *);
static amdsmi_status_t (*amdsmi_get_gpu_od_volt_curve_regions_p)(amdsmi_processor_handle, amdsmi_freq_volt_region_t *, uint32_t *);
static amdsmi_status_t (*amdsmi_get_soc_pstate_p)(amdsmi_processor_handle, uint32_t *);
static amdsmi_status_t (*amdsmi_get_xgmi_plpd_p)(amdsmi_processor_handle, uint64_t *);
static amdsmi_status_t (*amdsmi_get_gpu_process_isolation_p)(amdsmi_processor_handle, uint32_t *);
static amdsmi_status_t (*amdsmi_get_lib_version_p)(amdsmi_version_t *);
static amdsmi_status_t (*amdsmi_get_gpu_ecc_count_p)(amdsmi_processor_handle, amdsmi_error_count_t *);
static amdsmi_status_t (*amdsmi_get_gpu_ecc_enabled_p)(amdsmi_processor_handle, uint32_t *);
static amdsmi_status_t (*amdsmi_get_gpu_total_ecc_count_p)(amdsmi_processor_handle, amdsmi_error_count_t *);
static amdsmi_status_t (*amdsmi_get_gpu_ecc_status_p)(amdsmi_processor_handle, uint64_t *);
static amdsmi_status_t (*amdsmi_gpu_xgmi_error_status_p)(amdsmi_processor_handle, uint64_t *);
static amdsmi_status_t (*amdsmi_get_xgmi_info_p)(amdsmi_processor_handle, amdsmi_xgmi_info_t *);
static amdsmi_status_t (*amdsmi_get_gpu_xgmi_link_status_p)(amdsmi_processor_handle, amdsmi_xgmi_link_status_t *);
static amdsmi_status_t (*amdsmi_topo_get_numa_node_number_p)(amdsmi_processor_handle, uint32_t *);
static amdsmi_status_t (*amdsmi_topo_get_link_weight_p)(amdsmi_processor_handle, amdsmi_processor_handle, uint64_t *);
static amdsmi_status_t (*amdsmi_topo_get_link_type_p)(amdsmi_processor_handle, amdsmi_processor_handle, uint64_t *, amdsmi_io_link_type_t *);
static amdsmi_status_t (*amdsmi_get_link_topology_nearest_p)(amdsmi_processor_handle, amdsmi_link_type_t, amdsmi_topology_nearest_t *);
static amdsmi_status_t (*amdsmi_is_P2P_accessible_p)(amdsmi_processor_handle, amdsmi_processor_handle, bool *);
static amdsmi_status_t (*amdsmi_topo_get_p2p_status_p)(amdsmi_processor_handle, amdsmi_processor_handle, amdsmi_io_link_type_t *, amdsmi_p2p_capability_t *);
static amdsmi_status_t (*amdsmi_get_gpu_compute_partition_p)(amdsmi_processor_handle, uint32_t *);
static amdsmi_status_t (*amdsmi_get_gpu_memory_partition_p)(amdsmi_processor_handle, uint32_t *);
static amdsmi_status_t (*amdsmi_get_gpu_memory_partition_config_p)(amdsmi_processor_handle, amdsmi_memory_partition_config_t *);
static amdsmi_status_t (*amdsmi_get_gpu_accelerator_partition_profile_config_p)(amdsmi_processor_handle, amdsmi_accelerator_partition_profile_config_t *);
static amdsmi_status_t (*amdsmi_get_gpu_accelerator_partition_profile_p)(amdsmi_processor_handle, amdsmi_accelerator_partition_profile_t *);
static amdsmi_status_t (*amdsmi_get_gpu_event_notification_p)(amdsmi_processor_handle, amdsmi_evt_notification_data_t *);
static amdsmi_status_t (*amdsmi_get_gpu_driver_info_p)(amdsmi_processor_handle, amdsmi_driver_info_t *);
static amdsmi_status_t (*amdsmi_get_gpu_asic_info_p)(amdsmi_processor_handle, amdsmi_asic_info_t *);
static amdsmi_status_t (*amdsmi_get_gpu_kfd_info_p)(amdsmi_processor_handle, amdsmi_kfd_info_t *);
static amdsmi_status_t (*amdsmi_get_gpu_vram_info_p)(amdsmi_processor_handle, amdsmi_vram_info_t *);
static amdsmi_status_t (*amdsmi_get_gpu_board_info_p)(amdsmi_processor_handle, amdsmi_board_info_t *);
static amdsmi_status_t (*amdsmi_get_pcie_info_p)(amdsmi_processor_handle, amdsmi_pcie_info_t *);
static amdsmi_status_t (*amdsmi_get_gpu_xcd_counter_p)(amdsmi_processor_handle, uint64_t *);
static amdsmi_status_t (*amdsmi_get_fw_info_p)(amdsmi_processor_handle, amdsmi_fw_info_t *);
static amdsmi_status_t (*amdsmi_get_gpu_vbios_info_p)(amdsmi_processor_handle, amdsmi_vbios_info_t *);
static amdsmi_status_t (*amdsmi_is_gpu_power_management_enabled_p)(amdsmi_processor_handle, uint32_t *);
static amdsmi_status_t (*amdsmi_get_clock_info_p)(amdsmi_processor_handle, amdsmi_clk_info_t *);
static amdsmi_status_t (*amdsmi_get_gpu_vram_usage_p)(amdsmi_processor_handle, amdsmi_vram_usage_t *);
static amdsmi_status_t (*amdsmi_get_violation_status_p)(amdsmi_processor_handle, amdsmi_violation_status_t *);
static amdsmi_status_t (*amdsmi_get_gpu_process_list_p)(amdsmi_processor_handle, uint32_t *, amdsmi_proc_info_t *);
static const char *(*amdsmi_status_code_to_string_p)(amdsmi_status_t);


#ifndef AMDSMI_DISABLE_ESMI
/* CPU function pointers */
static amdsmi_status_t (*amdsmi_get_cpu_handles_p)(uint32_t *, amdsmi_processor_handle *);
static amdsmi_status_t (*amdsmi_get_cpucore_handles_p)(uint32_t *, amdsmi_processor_handle *);
static amdsmi_status_t (*amdsmi_get_cpu_socket_power_p)(amdsmi_processor_handle, uint32_t *);
static amdsmi_status_t (*amdsmi_get_cpu_socket_power_cap_p)(amdsmi_processor_handle, uint32_t *);
static amdsmi_status_t (*amdsmi_get_cpu_socket_power_cap_max_p)(amdsmi_processor_handle, uint32_t *);
static amdsmi_status_t (*amdsmi_get_cpu_core_energy_p)(amdsmi_processor_handle, uint64_t *);
static amdsmi_status_t (*amdsmi_get_cpu_socket_energy_p)(amdsmi_processor_handle, uint64_t *);
static amdsmi_status_t (*amdsmi_get_cpu_smu_fw_version_p)(amdsmi_processor_handle, amdsmi_smu_fw_version_t *);
static amdsmi_status_t (*amdsmi_get_threads_per_core_p)(uint32_t *);
static amdsmi_status_t (*amdsmi_get_cpu_family_p)(uint32_t *);
static amdsmi_status_t (*amdsmi_get_cpu_model_p)(uint32_t *);
static amdsmi_status_t (*amdsmi_get_cpu_core_boostlimit_p)(amdsmi_processor_handle, uint32_t *);
static amdsmi_status_t (*amdsmi_get_cpu_socket_current_active_freq_limit_p)(amdsmi_processor_handle, uint16_t *, char **);
static amdsmi_status_t (*amdsmi_get_cpu_socket_freq_range_p)(amdsmi_processor_handle, uint16_t *, uint16_t *);
static amdsmi_status_t (*amdsmi_get_cpu_core_current_freq_limit_p)(amdsmi_processor_handle, uint32_t *);
static amdsmi_status_t (*amdsmi_get_minmax_bandwidth_between_processors_p)(amdsmi_processor_handle, amdsmi_processor_handle, uint64_t *, uint64_t *);
static amdsmi_status_t (*amdsmi_get_cpu_dimm_temp_range_and_refresh_rate_p)(amdsmi_processor_handle, uint8_t, amdsmi_temp_range_refresh_rate_t *);
static amdsmi_status_t (*amdsmi_get_cpu_dimm_power_consumption_p)(amdsmi_processor_handle, uint8_t, amdsmi_dimm_power_t *);
static amdsmi_status_t (*amdsmi_get_cpu_dimm_thermal_sensor_p)(amdsmi_processor_handle, uint8_t, amdsmi_dimm_thermal_t *);
#endif
/* Global device list and count */
static int32_t device_count = 0;
static amdsmi_processor_handle *device_handles = NULL;
static int32_t device_mask = 0;
static int32_t gpu_count = 0;
static int32_t cpu_count = 0;
static amdsmi_processor_handle **cpu_core_handles = NULL;
static uint32_t *cores_per_socket = NULL;
static void *amds_dlp = NULL;
static void *htable = NULL;
static char error_string[PAPI_MAX_STR_LEN+1];
/* forward declarations for internal helpers */
static int load_amdsmi_sym(void);
static int unload_amdsmi_sym(void);
static int init_device_table(void);
static int shutdown_device_table(void);
static int init_event_table(void);
static int shutdown_event_table(void);
/* Event descriptor structure for native events */
typedef struct native_event {
    unsigned int id;
    char *name;
    char *descr;
    int32_t device;                /* device index or -1 if not applicable */
    uint64_t value;               /* last read value or set value */
    uint32_t mode;                /* access mode (read/write) */
    uint32_t variant;             /* variant index (for metric type, etc.) */
    uint32_t subvariant;          /* subvariant index (for sensor index or sub-type) */
    int (*open_func)(struct native_event *);    /* optional open (reserve resources) */
    int (*close_func)(struct native_event *);   /* optional close (release resources) */
    int (*start_func)(struct native_event *);   /* optional start (begin counting) */
    int (*stop_func)(struct native_event *);    /* optional stop (stop counting) */
    int (*access_func)(int mode, void *arg);    /* read or write the event value */
} native_event_t;
/* Table of all native events */
typedef struct {
    native_event_t *events;
    int count;
} native_event_table_t;
static native_event_table_t ntv_table;
static native_event_table_t *ntv_table_p = NULL;
/* Locking device usage for contexts */
static int
acquire_devices(unsigned int *events_id, int num_events, int32_t *bitmask)
{
    int32_t mask_acq = 0;
    for (int i = 0; i < num_events; ++i) {
        int32_t dev_id = ntv_table_p->events[events_id[i]].device;
        if (dev_id < 0) continue;
        mask_acq |= (1 << dev_id);
    }
    if (mask_acq & device_mask) {
        return PAPI_ECNFLCT;  // conflict: device already in use
    }
    device_mask |= mask_acq;
    *bitmask = mask_acq;
    return PAPI_OK;
}
static int
release_devices(int32_t *bitmask)
{
    int32_t mask_rel = *bitmask;
    if ((mask_rel & device_mask) != mask_rel) {
        return PAPI_EMISC;
    }
    device_mask ^= mask_rel;
    *bitmask = 0;
    return PAPI_OK;
}
/* Prototypes for event access (read/write) functions */
static int access_amdsmi_temp_metric(int mode, void *arg);
static int access_amdsmi_fan_speed(int mode, void *arg);
static int access_amdsmi_fan_rpms(int mode, void *arg);
static int access_amdsmi_mem_total(int mode, void *arg);
static int access_amdsmi_mem_usage(int mode, void *arg);
static int access_amdsmi_power_cap(int mode, void *arg);
static int access_amdsmi_power_cap_range(int mode, void *arg);
static int access_amdsmi_power_average(int mode, void *arg);
static int access_amdsmi_pci_throughput(int mode, void *arg);
static int access_amdsmi_pci_replay_counter(int mode, void *arg);
static int access_amdsmi_clk_freq(int mode, void *arg);
static int access_amdsmi_gpu_metrics(int mode, void *arg);
static int access_amdsmi_gpu_info(int mode, void *arg);
static int access_amdsmi_gpu_activity(int mode, void *arg);
static int access_amdsmi_fan_speed_max(int mode, void *arg);
static int access_amdsmi_pci_bandwidth(int mode, void *arg);
static int access_amdsmi_energy_count(int mode, void *arg);
static int access_amdsmi_power_profile_status(int mode, void *arg);
#ifndef AMDSMI_DISABLE_ESMI
static int access_amdsmi_cpu_socket_power(int mode, void *arg);
static int access_amdsmi_cpu_socket_energy(int mode, void *arg);
static int access_amdsmi_cpu_socket_freq_limit(int mode, void *arg);
static int access_amdsmi_cpu_socket_freq_range(int mode, void *arg);
static int access_amdsmi_cpu_power_cap(int mode, void *arg);
static int access_amdsmi_cpu_core_energy(int mode, void *arg);
static int access_amdsmi_cpu_core_freq_limit(int mode, void *arg);
static int access_amdsmi_cpu_core_boostlimit(int mode, void *arg);
static int access_amdsmi_dimm_temp(int mode, void *arg);
static int access_amdsmi_dimm_power(int mode, void *arg);
static int access_amdsmi_dimm_range_refresh(int mode, void *arg);
static int access_amdsmi_threads_per_core(int mode, void *arg);
static int access_amdsmi_cpu_family(int mode, void *arg);
static int access_amdsmi_cpu_model(int mode, void *arg);
static int access_amdsmi_smu_fw_version(int mode, void *arg);
static int access_amdsmi_xgmi_bandwidth(int mode, void *arg);
#endif
/* Simple open/close/start/stop functions (no special handling needed for most events) */
static int open_simple(native_event_t *event)   { (void)event; return PAPI_OK; }
static int close_simple(native_event_t *event)  { (void)event; return PAPI_OK; }
static int start_simple(native_event_t *event)  { (void)event; return PAPI_OK; }
static int stop_simple(native_event_t *event)   { (void)event; return PAPI_OK; }
/* Dynamic load of AMD SMI library symbols */
static void *sym(const char *preferred, const char *fallback) {
    void *p = dlsym(amds_dlp, preferred);
    return p ? p : (fallback ? dlsym(amds_dlp, fallback) : NULL);
}
static int load_amdsmi_sym(void) {
    const char *root = getenv("PAPI_AMDSMI_ROOT");
    char so_path[PATH_MAX] = {0};
    if (!root) {
        snprintf(error_string, sizeof(error_string),
                 "PAPI_AMDSMI_ROOT not set; cannot find libamd_smi.so");
        return PAPI_ENOSUPP;
    }
    snprintf(so_path, sizeof(so_path), "%s/lib/libamd_smi.so", root);
    amds_dlp = dlopen(so_path, RTLD_NOW | RTLD_GLOBAL);
    if (!amds_dlp) {
        snprintf(error_string, sizeof(error_string),
                 "dlopen(\"%s\"): %s", so_path, dlerror());
        return PAPI_ENOSUPP;
    }
    /* Resolve required function symbols */
    amdsmi_init_p          = sym("amdsmi_init", NULL);
    amdsmi_shut_down_p     = sym("amdsmi_shut_down", NULL);
    amdsmi_get_socket_handles_p = sym("amdsmi_get_socket_handles", NULL);
    amdsmi_get_processor_handles_by_type_p = sym("amdsmi_get_processor_handles_by_type", NULL);
    /* Sensors */
    amdsmi_get_temp_metric_p       = sym("amdsmi_get_temp_metric", NULL);
    amdsmi_get_gpu_fan_rpms_p      = sym("amdsmi_get_gpu_fan_rpms", NULL);
    amdsmi_get_gpu_fan_speed_p     = sym("amdsmi_get_gpu_fan_speed", NULL);
    amdsmi_get_gpu_fan_speed_max_p = sym("amdsmi_get_gpu_fan_speed_max", NULL);
    /* Memory */
    amdsmi_get_total_memory_p   = sym("amdsmi_get_gpu_memory_total", "amdsmi_get_total_memory");
    amdsmi_get_memory_usage_p   = sym("amdsmi_get_gpu_memory_usage", "amdsmi_get_memory_usage");
    /* Utilization / activity */
    amdsmi_get_gpu_activity_p   = sym("amdsmi_get_gpu_activity", "amdsmi_get_engine_usage");
    /* Power */
    amdsmi_get_power_info_p     = sym("amdsmi_get_power_info", NULL);
    amdsmi_get_power_cap_info_p = sym("amdsmi_get_power_cap_info", NULL);
    amdsmi_set_power_cap_p      = sym("amdsmi_set_power_cap", "amdsmi_dev_set_power_cap");
    /* PCIe */
    amdsmi_get_gpu_pci_throughput_p     = sym("amdsmi_get_gpu_pci_throughput", NULL);
    amdsmi_get_gpu_pci_replay_counter_p = sym("amdsmi_get_gpu_pci_replay_counter", NULL);
    /* Clocks */
    amdsmi_get_clk_freq_p       = sym("amdsmi_get_clk_freq", NULL);
    amdsmi_set_clk_freq_p       = sym("amdsmi_set_clk_freq", NULL);
    /* GPU metrics */
    amdsmi_get_gpu_metrics_info_p = sym("amdsmi_get_gpu_metrics_info", NULL);
    /* Identification and other queries */
    amdsmi_get_gpu_id_p                    = sym("amdsmi_get_gpu_id", NULL);
    amdsmi_get_gpu_revision_p             = sym("amdsmi_get_gpu_revision", NULL);
    amdsmi_get_gpu_subsystem_id_p         = sym("amdsmi_get_gpu_subsystem_id", NULL);
//    amdsmi_get_gpu_virtualization_mode_p  = sym("amdsmi_get_gpu_virtualization_mode", NULL);
    amdsmi_get_gpu_pci_bandwidth_p        = sym("amdsmi_get_gpu_pci_bandwidth", NULL);
    amdsmi_get_gpu_bdf_id_p               = sym("amdsmi_get_gpu_bdf_id", NULL);
    amdsmi_get_gpu_topo_numa_affinity_p   = sym("amdsmi_get_gpu_topo_numa_affinity", NULL);
    amdsmi_get_energy_count_p             = sym("amdsmi_get_energy_count", NULL);
    amdsmi_get_gpu_power_profile_presets_p = sym("amdsmi_get_gpu_power_profile_presets", NULL);
    
    amdsmi_get_socket_info_p       = sym("amdsmi_get_socket_info", NULL);
    amdsmi_get_processor_type_p    = sym("amdsmi_get_processor_type", NULL);
    amdsmi_get_processor_handle_from_bdf_p = sym("amdsmi_get_processor_handle_from_bdf", NULL);
    amdsmi_get_gpu_device_bdf_p    = sym("amdsmi_get_gpu_device_bdf", NULL);
    amdsmi_get_gpu_device_uuid_p   = sym("amdsmi_get_gpu_device_uuid", NULL);
    amdsmi_get_gpu_enumeration_info_p = sym("amdsmi_get_gpu_enumeration_info", NULL);
    amdsmi_get_gpu_vendor_name_p   = sym("amdsmi_get_gpu_vendor_name", NULL);
    amdsmi_get_gpu_vram_vendor_p   = sym("amdsmi_get_gpu_vram_vendor", NULL);
    amdsmi_get_gpu_subsystem_name_p = sym("amdsmi_get_gpu_subsystem_name", NULL);
    amdsmi_get_gpu_bad_page_info_p = sym("amdsmi_get_gpu_bad_page_info", NULL);
    amdsmi_get_gpu_bad_page_threshold_p = sym("amdsmi_get_gpu_bad_page_threshold", NULL);
    amdsmi_get_gpu_ras_feature_info_p = sym("amdsmi_get_gpu_ras_feature_info", NULL);
    amdsmi_get_gpu_ras_block_features_enabled_p = sym("amdsmi_get_gpu_ras_block_features_enabled", NULL);
    amdsmi_get_gpu_memory_reserved_pages_p = sym("amdsmi_get_gpu_memory_reserved_pages", NULL);
    amdsmi_get_gpu_volt_metric_p   = sym("amdsmi_get_gpu_volt_metric", NULL);
    amdsmi_get_gpu_perf_level_p    = sym("amdsmi_get_gpu_perf_level", NULL);
    amdsmi_get_gpu_overdrive_level_p = sym("amdsmi_get_gpu_overdrive_level", NULL);
    amdsmi_get_gpu_mem_overdrive_level_p = sym("amdsmi_get_gpu_mem_overdrive_level", NULL);
    amdsmi_get_gpu_od_volt_info_p  = sym("amdsmi_get_gpu_od_volt_info", NULL);
    amdsmi_get_gpu_metrics_header_info_p = sym("amdsmi_get_gpu_metrics_header_info", NULL);
    amdsmi_get_gpu_pm_metrics_info_p = sym("amdsmi_get_gpu_pm_metrics_info", NULL);
    amdsmi_get_gpu_reg_table_info_p = sym("amdsmi_get_gpu_reg_table_info", NULL);
    amdsmi_get_gpu_od_volt_curve_regions_p = sym("amdsmi_get_gpu_od_volt_curve_regions", NULL);
    amdsmi_get_soc_pstate_p        = sym("amdsmi_get_soc_pstate", NULL);
    amdsmi_get_xgmi_plpd_p         = sym("amdsmi_get_xgmi_plpd", NULL);
    amdsmi_get_gpu_process_isolation_p = sym("amdsmi_get_gpu_process_isolation", NULL);
    amdsmi_get_lib_version_p       = sym("amdsmi_get_lib_version", NULL);
    amdsmi_get_gpu_ecc_count_p     = sym("amdsmi_get_gpu_ecc_count", NULL);
    amdsmi_get_gpu_ecc_enabled_p   = sym("amdsmi_get_gpu_ecc_enabled", NULL);
    amdsmi_get_gpu_total_ecc_count_p = sym("amdsmi_get_gpu_total_ecc_count", NULL);
    amdsmi_get_gpu_ecc_status_p    = sym("amdsmi_get_gpu_ecc_status", NULL);
    amdsmi_gpu_xgmi_error_status_p = sym("amdsmi_gpu_xgmi_error_status", NULL);
    amdsmi_get_xgmi_info_p         = sym("amdsmi_get_xgmi_info", NULL);
    amdsmi_get_gpu_xgmi_link_status_p = sym("amdsmi_get_gpu_xgmi_link_status", NULL);
    amdsmi_topo_get_numa_node_number_p = sym("amdsmi_topo_get_numa_node_number", NULL);
    amdsmi_topo_get_link_weight_p  = sym("amdsmi_topo_get_link_weight", NULL);
    amdsmi_topo_get_link_type_p    = sym("amdsmi_topo_get_link_type", NULL);
    amdsmi_get_link_topology_nearest_p = sym("amdsmi_get_link_topology_nearest", NULL);
    amdsmi_is_P2P_accessible_p     = sym("amdsmi_is_P2P_accessible", NULL);
    amdsmi_topo_get_p2p_status_p   = sym("amdsmi_topo_get_p2p_status", NULL);
    amdsmi_get_gpu_compute_partition_p = sym("amdsmi_get_gpu_compute_partition", NULL);
    amdsmi_get_gpu_memory_partition_p = sym("amdsmi_get_gpu_memory_partition", NULL);
    amdsmi_get_gpu_memory_partition_config_p = sym("amdsmi_get_gpu_memory_partition_config", NULL);
    amdsmi_get_gpu_accelerator_partition_profile_config_p = sym("amdsmi_get_gpu_accelerator_partition_profile_config", NULL);
    amdsmi_get_gpu_accelerator_partition_profile_p = sym("amdsmi_get_gpu_accelerator_partition_profile", NULL);
    amdsmi_get_gpu_event_notification_p = sym("amdsmi_get_gpu_event_notification", NULL);
    amdsmi_get_gpu_driver_info_p   = sym("amdsmi_get_gpu_driver_info", NULL);
    amdsmi_get_gpu_asic_info_p     = sym("amdsmi_get_gpu_asic_info", NULL);
    amdsmi_get_gpu_kfd_info_p      = sym("amdsmi_get_gpu_kfd_info", NULL);
    amdsmi_get_gpu_vram_info_p     = sym("amdsmi_get_gpu_vram_info", NULL);
    amdsmi_get_gpu_board_info_p    = sym("amdsmi_get_gpu_board_info", NULL);
    amdsmi_get_pcie_info_p         = sym("amdsmi_get_pcie_info", NULL);
    amdsmi_get_gpu_xcd_counter_p   = sym("amdsmi_get_gpu_xcd_counter", NULL);
    amdsmi_get_fw_info_p           = sym("amdsmi_get_fw_info", NULL);
    amdsmi_get_gpu_vbios_info_p    = sym("amdsmi_get_gpu_vbios_info", NULL);
    amdsmi_is_gpu_power_management_enabled_p = sym("amdsmi_is_gpu_power_management_enabled", NULL);
    amdsmi_get_clock_info_p        = sym("amdsmi_get_clock_info", NULL);
    amdsmi_get_gpu_vram_usage_p    = sym("amdsmi_get_gpu_vram_usage", NULL);
    amdsmi_get_violation_status_p  = sym("amdsmi_get_violation_status", NULL);
    amdsmi_get_gpu_process_list_p  = sym("amdsmi_get_gpu_process_list", NULL);
    amdsmi_status_code_to_string_p = sym("amdsmi_status_code_to_string", NULL);
    
#ifndef AMDSMI_DISABLE_ESMI
    /* CPU functions */
    amdsmi_get_cpu_handles_p = sym("amdsmi_get_cpu_handles", NULL);
    amdsmi_get_cpucore_handles_p = sym("amdsmi_get_cpucore_handles", NULL);
    amdsmi_get_cpu_socket_power_p = sym("amdsmi_get_cpu_socket_power", NULL);
    amdsmi_get_cpu_socket_power_cap_p = sym("amdsmi_get_cpu_socket_power_cap", NULL);
    amdsmi_get_cpu_socket_power_cap_max_p = sym("amdsmi_get_cpu_socket_power_cap_max", NULL);
    amdsmi_get_cpu_core_energy_p = sym("amdsmi_get_cpu_core_energy", NULL);
    amdsmi_get_cpu_socket_energy_p = sym("amdsmi_get_cpu_socket_energy", NULL);
    amdsmi_get_cpu_smu_fw_version_p = sym("amdsmi_get_cpu_smu_fw_version", NULL);
    amdsmi_get_threads_per_core_p = sym("amdsmi_get_threads_per_core", NULL);
    amdsmi_get_cpu_family_p = sym("amdsmi_get_cpu_family", NULL);
    amdsmi_get_cpu_model_p = sym("amdsmi_get_cpu_model", NULL);
    amdsmi_get_cpu_core_boostlimit_p = sym("amdsmi_get_cpu_core_boostlimit", NULL);
    amdsmi_get_cpu_socket_current_active_freq_limit_p = sym("amdsmi_get_cpu_socket_current_active_freq_limit", NULL);
    amdsmi_get_cpu_socket_freq_range_p = sym("amdsmi_get_cpu_socket_freq_range", NULL);
    amdsmi_get_cpu_core_current_freq_limit_p = sym("amdsmi_get_cpu_core_current_freq_limit", NULL);
    amdsmi_get_minmax_bandwidth_between_processors_p = sym("amdsmi_get_minmax_bandwidth_between_processors", NULL);
    amdsmi_get_cpu_dimm_temp_range_and_refresh_rate_p = sym("amdsmi_get_cpu_dimm_temp_range_and_refresh_rate", NULL);
    amdsmi_get_cpu_dimm_power_consumption_p = sym("amdsmi_get_cpu_dimm_power_consumption", NULL);
    amdsmi_get_cpu_dimm_thermal_sensor_p = sym("amdsmi_get_cpu_dimm_thermal_sensor", NULL);
#endif
    /* Verify that all required symbols are loaded */
    struct { const char *name; void *ptr; } required[] = {
        { "amdsmi_init",          amdsmi_init_p },
        { "amdsmi_shut_down",     amdsmi_shut_down_p },
        { "amdsmi_get_socket_handles", amdsmi_get_socket_handles_p },
        { "amdsmi_get_processor_handles_by_type", amdsmi_get_processor_handles_by_type_p },
        { "amdsmi_get_temp_metric",    amdsmi_get_temp_metric_p },
        { "amdsmi_get_gpu_memory_total", amdsmi_get_total_memory_p },
        { "amdsmi_get_gpu_memory_usage", amdsmi_get_memory_usage_p },
        { "amdsmi_get_gpu_activity",    amdsmi_get_gpu_activity_p },
        { "amdsmi_get_power_cap_info",  amdsmi_get_power_cap_info_p },
        { "amdsmi_set_power_cap",       amdsmi_set_power_cap_p },
        { "amdsmi_get_power_info",      amdsmi_get_power_info_p },
        { "amdsmi_get_gpu_pci_throughput",  amdsmi_get_gpu_pci_throughput_p },
        { "amdsmi_get_gpu_pci_replay_counter", amdsmi_get_gpu_pci_replay_counter_p },
        { "amdsmi_get_gpu_fan_rpms",    amdsmi_get_gpu_fan_rpms_p },
        { "amdsmi_get_gpu_fan_speed",   amdsmi_get_gpu_fan_speed_p },
        { "amdsmi_get_gpu_fan_speed_max", amdsmi_get_gpu_fan_speed_max_p },
        { "amdsmi_get_clk_freq",        amdsmi_get_clk_freq_p },
        { "amdsmi_set_clk_freq",        amdsmi_set_clk_freq_p },
        { "amdsmi_get_gpu_metrics_info", amdsmi_get_gpu_metrics_info_p },
        { "amdsmi_get_gpu_id",          amdsmi_get_gpu_id_p },
        { "amdsmi_get_gpu_revision",    amdsmi_get_gpu_revision_p },
        { "amdsmi_get_gpu_subsystem_id", amdsmi_get_gpu_subsystem_id_p },
//        { "amdsmi_get_gpu_virtualization_mode", amdsmi_get_gpu_virtualization_mode_p },
        { "amdsmi_get_gpu_pci_bandwidth", amdsmi_get_gpu_pci_bandwidth_p },
        { "amdsmi_get_gpu_bdf_id",      amdsmi_get_gpu_bdf_id_p },
        { "amdsmi_get_gpu_topo_numa_affinity", amdsmi_get_gpu_topo_numa_affinity_p },
        { "amdsmi_get_energy_count",    amdsmi_get_energy_count_p },
        { "amdsmi_get_gpu_power_profile_presets", amdsmi_get_gpu_power_profile_presets_p },
#ifndef AMDSMI_DISABLE_ESMI
        { "amdsmi_get_cpu_handles", amdsmi_get_cpu_handles_p },
        { "amdsmi_get_cpucore_handles", amdsmi_get_cpucore_handles_p },
        { "amdsmi_get_cpu_socket_power", amdsmi_get_cpu_socket_power_p },
        { "amdsmi_get_cpu_socket_power_cap", amdsmi_get_cpu_socket_power_cap_p },
        { "amdsmi_get_cpu_socket_power_cap_max", amdsmi_get_cpu_socket_power_cap_max_p },
        { "amdsmi_get_cpu_core_energy", amdsmi_get_cpu_core_energy_p },
        { "amdsmi_get_cpu_socket_energy", amdsmi_get_cpu_socket_energy_p },
        { "amdsmi_get_cpu_smu_fw_version", amdsmi_get_cpu_smu_fw_version_p },
        { "amdsmi_get_threads_per_core", amdsmi_get_threads_per_core_p },
        { "amdsmi_get_cpu_family", amdsmi_get_cpu_family_p },
        { "amdsmi_get_cpu_model", amdsmi_get_cpu_model_p },
        { "amdsmi_get_cpu_core_boostlimit", amdsmi_get_cpu_core_boostlimit_p },
        { "amdsmi_get_cpu_socket_current_active_freq_limit", amdsmi_get_cpu_socket_current_active_freq_limit_p },
        { "amdsmi_get_cpu_socket_freq_range", amdsmi_get_cpu_socket_freq_range_p },
        { "amdsmi_get_cpu_core_current_freq_limit", amdsmi_get_cpu_core_current_freq_limit_p },
        { "amdsmi_get_minmax_bandwidth_between_processors", amdsmi_get_minmax_bandwidth_between_processors_p },
        { "amdsmi_get_cpu_dimm_temp_range_and_refresh_rate", amdsmi_get_cpu_dimm_temp_range_and_refresh_rate_p },
        { "amdsmi_get_cpu_dimm_power_consumption", amdsmi_get_cpu_dimm_power_consumption_p },
        { "amdsmi_get_cpu_dimm_thermal_sensor", amdsmi_get_cpu_dimm_thermal_sensor_p }
#endif
    };
    int miss = 0;
    int pos = snprintf(error_string, sizeof(error_string), "Missing AMD SMI symbols:");
    for (size_t i = 0; i < sizeof(required)/sizeof(required[0]); ++i) {
        if (!required[i].ptr) {
            miss++;
            pos += snprintf(error_string + pos, sizeof(error_string) - pos, "\n  %s", required[i].name);
        }
    }
    if (miss) {
        dlclose(amds_dlp); amds_dlp = NULL;
        return PAPI_ENOSUPP;
    }
    return PAPI_OK;
}
static int unload_amdsmi_sym(void) {
    /* Reset all function pointers and close library */
    amdsmi_init_p = NULL;
    amdsmi_shut_down_p = NULL;
    amdsmi_get_socket_handles_p = NULL;
    amdsmi_get_processor_handles_by_type_p = NULL;
    amdsmi_get_temp_metric_p = NULL;
    amdsmi_get_gpu_fan_rpms_p = NULL;
    amdsmi_get_gpu_fan_speed_p = NULL;
    amdsmi_get_gpu_fan_speed_max_p = NULL;
    amdsmi_get_total_memory_p = NULL;
    amdsmi_get_memory_usage_p = NULL;
    amdsmi_get_gpu_activity_p = NULL;
    amdsmi_get_power_cap_info_p = NULL;
    amdsmi_set_power_cap_p = NULL;
    amdsmi_get_power_info_p = NULL;
    amdsmi_get_gpu_pci_throughput_p = NULL;
    amdsmi_get_gpu_pci_replay_counter_p = NULL;
    amdsmi_get_clk_freq_p = NULL;
    amdsmi_set_clk_freq_p = NULL;
    amdsmi_get_gpu_metrics_info_p = NULL;
    amdsmi_get_gpu_id_p = NULL;
    amdsmi_get_gpu_revision_p = NULL;
    amdsmi_get_gpu_subsystem_id_p = NULL;
//    amdsmi_get_gpu_virtualization_mode_p = NULL;
    amdsmi_get_gpu_pci_bandwidth_p = NULL;
    amdsmi_get_gpu_bdf_id_p = NULL;
    amdsmi_get_gpu_topo_numa_affinity_p = NULL;
    amdsmi_get_energy_count_p = NULL;
    amdsmi_get_gpu_power_profile_presets_p = NULL;
    
    amdsmi_get_socket_info_p       = NULL;
    amdsmi_get_processor_type_p    = NULL;
    amdsmi_get_processor_handle_from_bdf_p = NULL;
    amdsmi_get_gpu_device_bdf_p    = NULL;
    amdsmi_get_gpu_device_uuid_p   = NULL;
    amdsmi_get_gpu_enumeration_info_p = NULL;
    amdsmi_get_gpu_vendor_name_p   = NULL;
    amdsmi_get_gpu_vram_vendor_p   = NULL;
    amdsmi_get_gpu_subsystem_name_p = NULL;
    amdsmi_get_gpu_bad_page_info_p = NULL;
    amdsmi_get_gpu_bad_page_threshold_p = NULL;
    amdsmi_get_gpu_ras_feature_info_p = NULL;
    amdsmi_get_gpu_ras_block_features_enabled_p = NULL;
    amdsmi_get_gpu_memory_reserved_pages_p = NULL;
    amdsmi_get_gpu_volt_metric_p   = NULL;
    amdsmi_get_gpu_perf_level_p    = NULL;
    amdsmi_get_gpu_overdrive_level_p = NULL;
    amdsmi_get_gpu_mem_overdrive_level_p = NULL;
    amdsmi_get_gpu_od_volt_info_p  = NULL;
    amdsmi_get_gpu_metrics_header_info_p = NULL;
    amdsmi_get_gpu_pm_metrics_info_p = NULL;
    amdsmi_get_gpu_reg_table_info_p = NULL;
    amdsmi_get_gpu_od_volt_curve_regions_p = NULL;
    amdsmi_get_soc_pstate_p        = NULL;
    amdsmi_get_xgmi_plpd_p         = NULL;
    amdsmi_get_gpu_process_isolation_p = NULL;
    amdsmi_get_lib_version_p       = NULL;
    amdsmi_get_gpu_ecc_count_p     = NULL;
    amdsmi_get_gpu_ecc_enabled_p   = NULL;
    amdsmi_get_gpu_total_ecc_count_p = NULL;
    amdsmi_get_gpu_ecc_status_p    = NULL;
    amdsmi_gpu_xgmi_error_status_p = NULL;
    amdsmi_get_xgmi_info_p         = NULL;
    amdsmi_get_gpu_xgmi_link_status_p = NULL;
    amdsmi_topo_get_numa_node_number_p = NULL;
    amdsmi_topo_get_link_weight_p  = NULL;
    amdsmi_topo_get_link_type_p    = NULL;
    amdsmi_get_link_topology_nearest_p = NULL;
    amdsmi_is_P2P_accessible_p     = NULL;
    amdsmi_topo_get_p2p_status_p   = NULL;
    amdsmi_get_gpu_compute_partition_p = NULL;
    amdsmi_get_gpu_memory_partition_p = NULL;
    amdsmi_get_gpu_memory_partition_config_p = NULL;
    amdsmi_get_gpu_accelerator_partition_profile_config_p = NULL;
    amdsmi_get_gpu_accelerator_partition_profile_p = NULL;
    amdsmi_get_gpu_event_notification_p = NULL;
    amdsmi_get_gpu_driver_info_p   = NULL;
    amdsmi_get_gpu_asic_info_p     = NULL;
    amdsmi_get_gpu_kfd_info_p      = NULL;
    amdsmi_get_gpu_vram_info_p     = NULL;
    amdsmi_get_gpu_board_info_p    = NULL;
    amdsmi_get_pcie_info_p         = NULL;
    amdsmi_get_gpu_xcd_counter_p   = NULL;
    amdsmi_get_fw_info_p           = NULL;
    amdsmi_get_gpu_vbios_info_p    = NULL;
    amdsmi_is_gpu_power_management_enabled_p = NULL;
    amdsmi_get_clock_info_p        = NULL;
    amdsmi_get_gpu_vram_usage_p    = NULL;
    amdsmi_get_violation_status_p  = NULL;
    amdsmi_get_gpu_process_list_p  = NULL;
    amdsmi_status_code_to_string_p = NULL;
    
#ifndef AMDSMI_DISABLE_ESMI
    amdsmi_get_cpu_handles_p = NULL;
    amdsmi_get_cpucore_handles_p = NULL;
    amdsmi_get_cpu_socket_power_p = NULL;
    amdsmi_get_cpu_socket_power_cap_p = NULL;
    amdsmi_get_cpu_socket_power_cap_max_p = NULL;
    amdsmi_get_cpu_core_energy_p = NULL;
    amdsmi_get_cpu_socket_energy_p = NULL;
    amdsmi_get_cpu_smu_fw_version_p = NULL;
    amdsmi_get_threads_per_core_p = NULL;
    amdsmi_get_cpu_family_p = NULL;
    amdsmi_get_cpu_model_p = NULL;
    amdsmi_get_cpu_core_boostlimit_p = NULL;
    amdsmi_get_cpu_socket_current_active_freq_limit_p = NULL;
    amdsmi_get_cpu_socket_freq_range_p = NULL;
    amdsmi_get_cpu_core_current_freq_limit_p = NULL;
    amdsmi_get_minmax_bandwidth_between_processors_p = NULL;
    amdsmi_get_cpu_dimm_temp_range_and_refresh_rate_p = NULL;
    amdsmi_get_cpu_dimm_power_consumption_p = NULL;
    amdsmi_get_cpu_dimm_thermal_sensor_p = NULL;
#endif
    if (amds_dlp) {
        dlclose(amds_dlp);
        amds_dlp = NULL;
    }
    return PAPI_OK;
}
/* Initialize AMD SMI library and discover devices */
int amds_init(void) {
    // Check if already initialized to avoid expensive re-initialization
    if (device_handles != NULL && device_count > 0) {
        return PAPI_OK;  // Already initialized
    }
    int papi_errno = load_amdsmi_sym();
    if (papi_errno != PAPI_OK) {
        return papi_errno;
    }
    //AMDSMI_INIT_AMD_CPUS
    amdsmi_status_t status = amdsmi_init_p(AMDSMI_INIT_AMD_GPUS);
    if (status != AMDSMI_STATUS_SUCCESS) {
        strcpy(error_string, "amdsmi_init failed");
        return PAPI_ENOSUPP;
    }
    htable_init(&htable);
    // Discover GPU and CPU devices
    uint32_t socket_count = 0;
    status = amdsmi_get_socket_handles_p(&socket_count, NULL);
    if (status != AMDSMI_STATUS_SUCCESS || socket_count == 0) {
        sprintf(error_string, "Error discovering sockets or no AMD socket found.");
        papi_errno = PAPI_ENOEVNT;
        goto fn_fail;
    }
    amdsmi_socket_handle *sockets = (amdsmi_socket_handle *) papi_calloc(socket_count, sizeof(amdsmi_socket_handle));
    if (!sockets) {
        papi_errno = PAPI_ENOMEM;
        goto fn_fail;
    }
    status = amdsmi_get_socket_handles_p(&socket_count, sockets);
    if (status != AMDSMI_STATUS_SUCCESS) {
        sprintf(error_string, "Error getting socket handles.");
        papi_free(sockets);
        papi_errno = PAPI_ENOSUPP;
        goto fn_fail;
    }
    device_count = 0;
    uint32_t total_gpu_count = 0;
    for (uint32_t s = 0; s < socket_count; ++s) {
        uint32_t gpu_count_local = 0;
        processor_type_t proc_type = AMDSMI_PROCESSOR_TYPE_AMD_GPU;
        amdsmi_status_t st = amdsmi_get_processor_handles_by_type_p(sockets[s], proc_type, NULL, &gpu_count_local);
        if (st == AMDSMI_STATUS_SUCCESS) {
            total_gpu_count += gpu_count_local;
        }
    }
    uint32_t total_cpu_count = 0;
#ifndef AMDSMI_DISABLE_ESMI
    status = amdsmi_get_cpu_handles_p(&total_cpu_count, NULL);
    if (status != AMDSMI_STATUS_SUCCESS) {
        total_cpu_count = 0;
    }
#endif
    if (total_gpu_count == 0 && total_cpu_count == 0) {
        sprintf(error_string, "No AMD GPU or CPU devices found.");
        papi_errno = PAPI_ENOEVNT;
        papi_free(sockets);
        goto fn_fail;
    }
    device_handles = (amdsmi_processor_handle *) papi_calloc(total_gpu_count + total_cpu_count, sizeof(*device_handles));
    if (!device_handles) {
        papi_errno = PAPI_ENOMEM;
        sprintf(error_string, "Memory allocation error for device handles.");
        papi_free(sockets);
        goto fn_fail;
    }
    // Retrieve GPU processor handles for each socket - optimized to reduce allocations
    for (uint32_t s = 0; s < socket_count; ++s) {
        uint32_t gpu_count_local = 0;
        processor_type_t proc_type = AMDSMI_PROCESSOR_TYPE_AMD_GPU;
        status = amdsmi_get_processor_handles_by_type_p(sockets[s], proc_type, NULL, &gpu_count_local);
        if (status != AMDSMI_STATUS_SUCCESS || gpu_count_local == 0) {
            continue;  // no GPU on this socket or error
        }
        // Use the main device_handles array directly to avoid extra allocation
        amdsmi_processor_handle *gpu_handles = &device_handles[device_count];
        status = amdsmi_get_processor_handles_by_type_p(sockets[s], proc_type, gpu_handles, &gpu_count_local);
        if (status == AMDSMI_STATUS_SUCCESS) {
            device_count += gpu_count_local;
        }
    }
    papi_free(sockets);
    // Set gpu_count for use in event table initialization
    gpu_count = device_count;  // All devices added so far are GPUs
#ifndef AMDSMI_DISABLE_ESMI
    // Retrieve CPU socket handles
    amdsmi_processor_handle *cpu_handles = NULL;
    if (total_cpu_count > 0) {
        cpu_handles = (amdsmi_processor_handle *) papi_calloc(total_cpu_count, sizeof(amdsmi_processor_handle));
        if (!cpu_handles) {
            papi_errno = PAPI_ENOMEM;
            sprintf(error_string, "Memory allocation error for CPU handles.");
            goto fn_fail;
        }
        status = amdsmi_get_cpu_handles_p(&total_cpu_count, cpu_handles);
        if (status != AMDSMI_STATUS_SUCCESS) {
            papi_free(cpu_handles);
            cpu_handles = NULL;
            total_cpu_count = 0;
        }
    }
    if (cpu_handles) {
        for (uint32_t i = 0; i < total_cpu_count; ++i) {
            device_handles[device_count++] = cpu_handles[i];
        }
        papi_free(cpu_handles);
    }
#endif
    // Set global GPU/CPU counts
    gpu_count = total_gpu_count;
    cpu_count = total_cpu_count;
    // Retrieve CPU core handles for each CPU socket
    if (cpu_count > 0) {
        cpu_core_handles = (amdsmi_processor_handle **) papi_calloc(cpu_count, sizeof(amdsmi_processor_handle *));
        cores_per_socket = (uint32_t *) papi_calloc(cpu_count, sizeof(uint32_t));
        if (!cpu_core_handles || !cores_per_socket) {
            papi_errno = PAPI_ENOMEM;
            sprintf(error_string, "Memory allocation error for CPU core handles.");
            if (cpu_core_handles) papi_free(cpu_core_handles);
            if (cores_per_socket) papi_free(cores_per_socket);
            goto fn_fail;
        }
        for (uint32_t s = 0; s < cpu_count; ++s) {
            uint32_t core_count = 0;
            amdsmi_status_t st = amdsmi_get_processor_handles_by_type_p(device_handles[gpu_count + s], AMDSMI_PROCESSOR_TYPE_AMD_CPU_CORE, NULL, &core_count);
            if (st != AMDSMI_STATUS_SUCCESS || core_count == 0) {
                cores_per_socket[s] = 0;
                cpu_core_handles[s] = NULL;
                continue;
            }
            cpu_core_handles[s] = (amdsmi_processor_handle *) papi_calloc(core_count, sizeof(amdsmi_processor_handle));
            if (!cpu_core_handles[s]) {
                papi_errno = PAPI_ENOMEM;
                sprintf(error_string, "Memory allocation error for CPU core handles on socket %u.", s);
                for (uint32_t t = 0; t < s; ++t) {
                    if (cpu_core_handles[t]) papi_free(cpu_core_handles[t]);
                }
                papi_free(cpu_core_handles);
                papi_free(cores_per_socket);
                goto fn_fail;
            }
            st = amdsmi_get_processor_handles_by_type_p(device_handles[gpu_count + s], AMDSMI_PROCESSOR_TYPE_AMD_CPU_CORE, cpu_core_handles[s], &core_count);
            if (st != AMDSMI_STATUS_SUCCESS) {
                papi_free(cpu_core_handles[s]);
                cpu_core_handles[s] = NULL;
                cores_per_socket[s] = 0;
            } else {
                cores_per_socket[s] = core_count;
            }
        }
    }
    // Initialize the native event table for all discovered metrics
    papi_errno = init_event_table();
    if (papi_errno != PAPI_OK) {
        sprintf(error_string, "Error while initializing the native event table.");
        goto fn_fail;
    }
    ntv_table_p = &ntv_table;
    return PAPI_OK;
fn_fail:
    htable_shutdown(htable);
    if (device_handles) {
        papi_free(device_handles);
        device_handles = NULL;
        device_count = 0;
    }
    // sockets already freed if allocated
    if (cpu_core_handles) {
        for (int s = 0; s < cpu_count; ++s) {
            if (cpu_core_handles[s]) papi_free(cpu_core_handles[s]);
        }
        papi_free(cpu_core_handles);
        cpu_core_handles = NULL;
    }
    if (cores_per_socket) {
        papi_free(cores_per_socket);
        cores_per_socket = NULL;
    }
    amdsmi_shut_down_p();
    unload_amdsmi_sym();
    return papi_errno;
}
int amds_shutdown(void) {
    shutdown_event_table();
    shutdown_device_table();
    htable_shutdown(htable);
    amdsmi_shut_down_p();  // shutdown AMD SMI library
    return unload_amdsmi_sym();
}
int amds_err_get_last(const char **err_string) {
    if (err_string) *err_string = error_string;
    return PAPI_OK;
}
/* Event enumeration: iterate over native events */
int
amds_evt_enum(unsigned int *EventCode, int modifier)
{
    if (modifier == PAPI_ENUM_FIRST) {
        if (ntv_table_p->count == 0) {
            return PAPI_ENOEVNT;
        }
        *EventCode = 0;
        return PAPI_OK;
    } else if (modifier == PAPI_ENUM_EVENTS) {
        if (*EventCode + 1 < (unsigned int) ntv_table_p->count) {
            *EventCode = *EventCode + 1;
            return PAPI_OK;
        } else {
            return PAPI_ENOEVNT;
        }
    }
    return PAPI_EINVAL;
}
int amds_evt_code_to_name(unsigned int EventCode, char *name, int len) {
    if (EventCode >= (unsigned int) ntv_table_p->count) {
        return PAPI_EINVAL;
    }
    strncpy(name, ntv_table_p->events[EventCode].name, len);
    return PAPI_OK;
}
int amds_evt_name_to_code(const char *name, unsigned int *EventCode) {
    native_event_t *event = NULL;
    int hret = htable_find(htable, name, (void **) &event);
    if (hret != HTABLE_SUCCESS) {
        return (hret == HTABLE_ENOVAL) ? PAPI_ENOEVNT : PAPI_ECMP;
    }
    *EventCode = event->id;
    return PAPI_OK;
}
int amds_evt_code_to_descr(unsigned int EventCode, char *descr, int len) {
    if (EventCode >= (unsigned int) ntv_table_p->count) {
        return PAPI_EINVAL;
    }
    strncpy(descr, ntv_table_p->events[EventCode].descr, len);
    return PAPI_OK;
}
/* Context management: open/close, start/stop, read/write, reset */
struct amds_ctx {
    int state;
    unsigned int *events_id;
    int num_events;
    long long *counters;
    int32_t device_mask;
};
int
amds_ctx_open(unsigned int *event_ids, int num_events, amds_ctx_t *ctx)
{
    amds_ctx_t new_ctx = (amds_ctx_t) papi_calloc(1, sizeof(struct amds_ctx));
    if (new_ctx == NULL) {
        return PAPI_ENOMEM;
    }
    new_ctx->events_id = event_ids;  // Store pointer (original approach)
    new_ctx->num_events = num_events;
    new_ctx->counters = (long long *) papi_calloc(num_events, sizeof(long long));
    if (new_ctx->counters == NULL) {
        papi_free(new_ctx);
        return PAPI_ENOMEM;
    }
    // Acquire devices needed by these events to avoid conflicts
    int papi_errno = acquire_devices(event_ids, num_events, &new_ctx->device_mask);
    if (papi_errno != PAPI_OK) {
        papi_free(new_ctx->counters);
        papi_free(new_ctx);
        return papi_errno;
    }
    *ctx = new_ctx;
    return PAPI_OK;
}
int
amds_ctx_close(amds_ctx_t ctx)
{
    if (!ctx) return PAPI_OK;
    // release device usage
    release_devices(&ctx->device_mask);
    papi_free(ctx->counters);
    papi_free(ctx);
    return PAPI_OK;
}
int
amds_ctx_start(amds_ctx_t ctx)
{
    // No additional actions needed to start in this design (all reads are on-demand)
    ctx->state |= AMDS_EVENTS_RUNNING;
    return PAPI_OK;
}
int
amds_ctx_stop(amds_ctx_t ctx)
{
    if (!(ctx->state & AMDS_EVENTS_RUNNING)) {
        return PAPI_OK;
    }
    ctx->state &= ~AMDS_EVENTS_RUNNING;
    return PAPI_OK;
}
int
amds_ctx_read(amds_ctx_t ctx, long long **counts)
{
    printf("A\n");
    int papi_errno = PAPI_OK;
    for (int i = 0; i < ctx->num_events; ++i) {
     printf("B\n");
        unsigned int id = ctx->events_id[i];
        papi_errno = ntv_table_p->events[id].access_func(PAPI_MODE_READ, &ntv_table_p->events[id]);
        if (papi_errno != PAPI_OK) {
            return papi_errno;
        }
        ctx->counters[i] = (long long) ntv_table_p->events[id].value;
    }
    printf("C\n");
    *counts = ctx->counters;
    return papi_errno;
}
int
amds_ctx_write(amds_ctx_t ctx, long long *counts)
{
    int papi_errno = PAPI_OK;
    for (int i = 0; i < ctx->num_events; ++i) {
        unsigned int id = ctx->events_id[i];
        ntv_table_p->events[id].value = counts[i];
        papi_errno = ntv_table_p->events[id].access_func(PAPI_MODE_WRITE, &ntv_table_p->events[id]);
        if (papi_errno != PAPI_OK) {
            return papi_errno;
        }
    }
    return papi_errno;
}
int
amds_ctx_reset(amds_ctx_t ctx)
{
    for (int i = 0; i < ctx->num_events; ++i) {
        unsigned int id = ctx->events_id[i];
        ntv_table_p->events[id].value = 0;
        ctx->counters[i] = 0;
    }
    return PAPI_OK;
}

/* Forward declarations for access functions used by init_event_table */
static int access_amdsmi_ras_block_enabled(int, void *);
static int access_amdsmi_ecc_count(int, void *);
static int access_amdsmi_ecc_total(int, void *);
static int access_amdsmi_ecc_enabled(int, void *);
static int access_amdsmi_xgmi_error(int, void *);
static int access_amdsmi_volt_metric(int, void *);
static int access_amdsmi_perf_level(int, void *);
static int access_amdsmi_overdrive(int, void *);
static int access_amdsmi_mem_overdrive(int, void *);
static int access_amdsmi_compute_partition(int, void *);
static int access_amdsmi_memory_partition(int, void *);
static int access_amdsmi_accel_profile(int, void *);
static int access_amdsmi_process_isolation(int, void *);
static int access_amdsmi_vram_info(int, void *);
static int access_amdsmi_pcie_info(int, void *);
static int access_amdsmi_power_management(int, void *);
static int access_amdsmi_link_weight(int, void *);
static int access_amdsmi_link_type(int, void *);
static int access_amdsmi_p2p_accessible(int, void *);
static int access_amdsmi_p2p_status(int, void *);
static int access_amdsmi_lib_version(int, void *);
/* End forward declarations */
/* Initialize native event table: enumerate all supported events */
static int init_event_table(void) {
    // Check if event table is already initialized
    if (ntv_table.count > 0 && ntv_table.events != NULL) {
        return PAPI_OK;  // Already initialized, skip expensive rebuild
    }
    ntv_table.count = 0;
    int idx = 0;
    // Safety check - if no devices, return early
    if (device_count <= 0) {
        ntv_table.events = NULL;
        return PAPI_OK;
    }
    // Keep original allocation approach
    ntv_table.events = (native_event_t *) papi_calloc(1024 * device_count, sizeof(native_event_t));
    if (!ntv_table.events) {
        return PAPI_ENOMEM;
    }
    char name_buf[PAPI_MAX_STR_LEN];
    char descr_buf[PAPI_MAX_STR_LEN];
    // Define sensor arrays first
    amdsmi_temperature_type_t temp_sensors[] = {
        AMDSMI_TEMPERATURE_TYPE_EDGE, AMDSMI_TEMPERATURE_TYPE_JUNCTION, 
        AMDSMI_TEMPERATURE_TYPE_VRAM, AMDSMI_TEMPERATURE_TYPE_HBM_0,
        AMDSMI_TEMPERATURE_TYPE_HBM_1, AMDSMI_TEMPERATURE_TYPE_HBM_2,
        AMDSMI_TEMPERATURE_TYPE_HBM_3, AMDSMI_TEMPERATURE_TYPE_PLX
    };
    const int num_temp_sensors = sizeof(temp_sensors)/sizeof(temp_sensors[0]);
    const amdsmi_temperature_metric_t temp_metrics[] = {
        AMDSMI_TEMP_CURRENT, AMDSMI_TEMP_MAX, AMDSMI_TEMP_MIN, AMDSMI_TEMP_MAX_HYST,
        AMDSMI_TEMP_MIN_HYST, AMDSMI_TEMP_CRITICAL, AMDSMI_TEMP_CRITICAL_HYST,
        AMDSMI_TEMP_EMERGENCY, AMDSMI_TEMP_EMERGENCY_HYST, AMDSMI_TEMP_CRIT_MIN,
        AMDSMI_TEMP_CRIT_MIN_HYST, AMDSMI_TEMP_OFFSET, AMDSMI_TEMP_LOWEST, AMDSMI_TEMP_HIGHEST
    };
    const char *temp_metric_names[] = {
        "temp_current", "temp_max", "temp_min", "temp_max_hyst", "temp_min_hyst",
        "temp_critical", "temp_critical_hyst", "temp_emergency", "temp_emergency_hyst",
        "temp_crit_min", "temp_crit_min_hyst", "temp_offset", "temp_lowest", "temp_highest"
    };
    /* Temperature sensors - device-level cache + individual testing */
    for (int d = 0; d < gpu_count; ++d) {
        // Safety check for device handle
        if (!device_handles || !device_handles[d]) {
            continue;
        }
        for (int si = 0; si < num_temp_sensors && si < 8; ++si) {
            // Test each sensor individually first
            int64_t sensor_test_val;
            if (!amdsmi_get_temp_metric_p || 
                amdsmi_get_temp_metric_p(device_handles[d], temp_sensors[si], AMDSMI_TEMP_CURRENT, &sensor_test_val) != AMDSMI_STATUS_SUCCESS) {
                continue;  // Skip this specific sensor if it doesn't work
            }
            // Register metrics for this working sensor, testing each metric individually
            for (size_t mi = 0; mi < sizeof(temp_metrics)/sizeof(temp_metrics[0]); ++mi) {
                // Bounds check to prevent buffer overflow
                if (idx >= 512 * device_count) {
                    return PAPI_ENOSUPP; // Too many events
                }
                // Test this specific metric on this specific sensor
                int64_t metric_val;
                if (amdsmi_get_temp_metric_p(device_handles[d], temp_sensors[si],
                                             temp_metrics[mi], &metric_val) != AMDSMI_STATUS_SUCCESS) {
                    continue;  /* skip this specific metric if not supported */
                }
                snprintf(name_buf, sizeof(name_buf), "%s:device=%d:sensor=%d",
                         temp_metric_names[mi], d, (int) temp_sensors[si]);
                snprintf(descr_buf, sizeof(descr_buf), "Device %d %s for sensor %d",
                         d, temp_metric_names[mi], (int) temp_sensors[si]);
                native_event_t *ev = &ntv_table.events[idx];
                ev->id = idx;
                ev->name = strdup(name_buf);
                ev->descr = strdup(descr_buf);
                if (!ev->name || !ev->descr) {
                    return PAPI_ENOMEM;
                }
                ev->device = d;
                ev->value = 0;
                ev->mode = PAPI_MODE_READ;
                ev->variant = temp_metrics[mi];
                ev->subvariant = temp_sensors[si];
                ev->open_func = open_simple;
                ev->close_func = close_simple;
                ev->start_func = start_simple;
                ev->stop_func = stop_simple;
                ev->access_func = access_amdsmi_temp_metric;
                htable_insert(htable, ev->name, ev);
                idx++;
            }
        }
    }
    /* Fan metrics - test each device individually */
    for (int d = 0; d < gpu_count; ++d) {
        // Safety check for device handle
        if (!device_handles || !device_handles[d]) {
            continue;
        }
        /* Register Fan RPM if available */
        int64_t dummy_rpm;
        if (amdsmi_get_gpu_fan_rpms_p && 
            amdsmi_get_gpu_fan_rpms_p(device_handles[d], 0, &dummy_rpm) == AMDSMI_STATUS_SUCCESS) {
            if (idx >= 512 * device_count) {
                return PAPI_ENOSUPP;
            }
            snprintf(name_buf, sizeof(name_buf),
                     "fan_rpms:device=%d:sensor=0", d);
            snprintf(descr_buf, sizeof(descr_buf),
                     "Device %d fan speed in RPM", d);
            native_event_t *ev_rpm = &ntv_table.events[idx];
            ev_rpm->id         = idx;
            ev_rpm->name       = strdup(name_buf);
            ev_rpm->descr      = strdup(descr_buf);
            if (!ev_rpm->name || !ev_rpm->descr) {
                return PAPI_ENOMEM;
            }
            ev_rpm->device     = d;
            ev_rpm->value      = 0;
            ev_rpm->mode       = PAPI_MODE_READ;
            ev_rpm->variant    = 0;
            ev_rpm->subvariant = 0;
            ev_rpm->open_func  = open_simple;
            ev_rpm->close_func = close_simple;
            ev_rpm->start_func = start_simple;
            ev_rpm->stop_func  = stop_simple;
            ev_rpm->access_func= access_amdsmi_fan_rpms;
            htable_insert(htable, ev_rpm->name, ev_rpm);
            idx++;
        }
        /* Register Fan SPEED if available */
        int64_t dummy_speed;
        if (amdsmi_get_gpu_fan_speed_p && 
            amdsmi_get_gpu_fan_speed_p(device_handles[d], 0, &dummy_speed) == AMDSMI_STATUS_SUCCESS) {
            if (idx >= 512 * device_count) {
                return PAPI_ENOSUPP;
            }
            snprintf(name_buf, sizeof(name_buf),
                     "fan_speed:device=%d:sensor=0", d);
            snprintf(descr_buf, sizeof(descr_buf),
                     "Device %d fan speed (0-255 relative)", d);
            native_event_t *ev_fan = &ntv_table.events[idx];
            ev_fan->id         = idx;
            ev_fan->name       = strdup(name_buf);
            ev_fan->descr      = strdup(descr_buf);
            if (!ev_fan->name || !ev_fan->descr) {
                return PAPI_ENOMEM;
            }
            ev_fan->device     = d;
            ev_fan->value      = 0;
            ev_fan->mode       = PAPI_MODE_READ;
            ev_fan->variant    = 0;
            ev_fan->subvariant = 0;
            ev_fan->open_func  = open_simple;
            ev_fan->close_func = close_simple;
            ev_fan->start_func = start_simple;
            ev_fan->stop_func  = stop_simple;
            ev_fan->access_func= access_amdsmi_fan_speed;
            htable_insert(htable, ev_fan->name, ev_fan);
            idx++;
        }
        /* Register Fan Max Speed - always probe directly */
        int64_t dummy_max;
        if (amdsmi_get_gpu_fan_speed_max_p && 
            amdsmi_get_gpu_fan_speed_max_p(device_handles[d], 0, &dummy_max) == AMDSMI_STATUS_SUCCESS) {
            if (idx >= 512 * device_count) {
                return PAPI_ENOSUPP;
            }
            snprintf(name_buf, sizeof(name_buf),
                     "fan_rpms_max:device=%d:sensor=0", d);
            snprintf(descr_buf, sizeof(descr_buf),
                     "Device %d fan maximum speed in RPM", d);
            native_event_t *ev_fanmax = &ntv_table.events[idx];
            ev_fanmax->id         = idx;
            ev_fanmax->name       = strdup(name_buf);
            ev_fanmax->descr      = strdup(descr_buf);
            if (!ev_fanmax->name || !ev_fanmax->descr) {
                return PAPI_ENOMEM;
            }
            ev_fanmax->device     = d;
            ev_fanmax->value      = 0;
            ev_fanmax->mode       = PAPI_MODE_READ;
            ev_fanmax->variant    = 0;
            ev_fanmax->subvariant = 0;
            ev_fanmax->open_func  = open_simple;
            ev_fanmax->close_func = close_simple;
            ev_fanmax->start_func = start_simple;
            ev_fanmax->stop_func  = stop_simple;
            ev_fanmax->access_func= access_amdsmi_fan_speed_max;
            htable_insert(htable, ev_fanmax->name, ev_fanmax);
            idx++;
        }
    }
    /* VRAM memory metrics - test each device individually */
    for (int d = 0; d < gpu_count; ++d) {
        // Safety check for device handle
        if (!device_handles || !device_handles[d]) {
            continue;
        }
        /* total VRAM bytes - test directly */
        uint64_t dummy_total;
        if (amdsmi_get_total_memory_p && 
            amdsmi_get_total_memory_p(device_handles[d], AMDSMI_MEM_TYPE_VRAM, &dummy_total) == AMDSMI_STATUS_SUCCESS) {
            if (idx >= 512 * device_count) {
                return PAPI_ENOSUPP;
            }
            snprintf(name_buf, sizeof(name_buf),
                     "mem_total_VRAM:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf),
                     "Device %d total VRAM memory (bytes)", d);
            native_event_t *ev = &ntv_table.events[idx];
            ev->id   = idx;
            ev->name = strdup(name_buf);
            ev->descr= strdup(descr_buf);
            if (!ev->name || !ev->descr) {
                return PAPI_ENOMEM;
            }
            ev->device = d;
            ev->value  = 0;
            ev->mode   = PAPI_MODE_READ;
            ev->variant= AMDSMI_MEM_TYPE_VRAM;
            ev->subvariant = 0;
            ev->open_func  = open_simple;
            ev->close_func = close_simple;
            ev->start_func = start_simple;
            ev->stop_func  = stop_simple;
            ev->access_func= access_amdsmi_mem_total;
            htable_insert(htable, ev->name, ev);
            ++idx;
        }
        /* used VRAM bytes - test directly */
        uint64_t dummy_usage;
        if (amdsmi_get_memory_usage_p && 
            amdsmi_get_memory_usage_p(device_handles[d], AMDSMI_MEM_TYPE_VRAM, &dummy_usage) == AMDSMI_STATUS_SUCCESS) {
            if (idx >= 512 * device_count) {
                return PAPI_ENOSUPP;
            }
            snprintf(name_buf, sizeof(name_buf),
                     "mem_usage_VRAM:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf),
                     "Device %d VRAM memory usage (bytes)", d);
            native_event_t *ev = &ntv_table.events[idx];
            ev->id   = idx;
            ev->name = strdup(name_buf);
            ev->descr= strdup(descr_buf);
            if (!ev->name || !ev->descr) {
                return PAPI_ENOMEM;
            }
            ev->device = d;
            ev->value  = 0;
            ev->mode   = PAPI_MODE_READ;
            ev->variant= AMDSMI_MEM_TYPE_VRAM;
            ev->subvariant = 0;
            ev->open_func  = open_simple;
            ev->close_func = close_simple;
            ev->start_func = start_simple;
            ev->stop_func  = stop_simple;
            ev->access_func= access_amdsmi_mem_usage;
            htable_insert(htable, ev->name, ev);
            ++idx;
        }
    }
    /* GPU power metrics: average power, power cap, and cap range */
    for (int d = 0; d < gpu_count; ++d) {
        // Safety check for device handle
        if (!device_handles || !device_handles[d]) {
            continue;
        }
        // Register power average event - test directly
        amdsmi_power_info_t dummy_power;
        if (amdsmi_get_power_info_p && 
            amdsmi_get_power_info_p(device_handles[d], &dummy_power) == AMDSMI_STATUS_SUCCESS) {
            // Average power consumption (in Watts or microWatts)
            snprintf(name_buf, sizeof(name_buf), "power_average:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d average power consumption (W)", d);
            native_event_t *ev_pwr_avg = &ntv_table.events[idx];
            ev_pwr_avg->id = idx;
            ev_pwr_avg->name = strdup(name_buf);
            ev_pwr_avg->descr = strdup(descr_buf);
            ev_pwr_avg->device = d;
            ev_pwr_avg->value = 0;
            ev_pwr_avg->mode = PAPI_MODE_READ;
            ev_pwr_avg->variant = 0;
            ev_pwr_avg->subvariant = 0;
            ev_pwr_avg->open_func = open_simple;
            ev_pwr_avg->close_func = close_simple;
            ev_pwr_avg->start_func = start_simple;
            ev_pwr_avg->stop_func = stop_simple;
            ev_pwr_avg->access_func = access_amdsmi_power_average;
            htable_insert(htable, ev_pwr_avg->name, ev_pwr_avg);
            idx++;
        }
        // Register power cap events (if power cap functions are available) - test directly
        amdsmi_power_cap_info_t dummy_cap_info;
        if (amdsmi_get_power_cap_info_p && 
            amdsmi_get_power_cap_info_p(device_handles[d], 0, &dummy_cap_info) == AMDSMI_STATUS_SUCCESS) {
            // Current power cap limit
            snprintf(name_buf, sizeof(name_buf), "power_cap:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d current power cap (W)", d);
            native_event_t *ev_pcap = &ntv_table.events[idx];
            ev_pcap->id = idx;
            ev_pcap->name = strdup(name_buf);
            ev_pcap->descr = strdup(descr_buf);
            ev_pcap->device = d;
            ev_pcap->value = 0;
            ev_pcap->mode = PAPI_MODE_READ | PAPI_MODE_WRITE;
            ev_pcap->variant = 0;
            ev_pcap->subvariant = 0;
            ev_pcap->open_func = open_simple;
            ev_pcap->close_func = close_simple;
            ev_pcap->start_func = start_simple;
            ev_pcap->stop_func = stop_simple;
            ev_pcap->access_func = access_amdsmi_power_cap;
            htable_insert(htable, ev_pcap->name, ev_pcap);
            idx++;
            // Minimum allowed power cap
            snprintf(name_buf, sizeof(name_buf), "power_cap_range_min:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d minimum allowed power cap (W)", d);
            native_event_t *ev_pcap_min = &ntv_table.events[idx];
            ev_pcap_min->id = idx;
            ev_pcap_min->name = strdup(name_buf);
            ev_pcap_min->descr = strdup(descr_buf);
            ev_pcap_min->device = d;
            ev_pcap_min->value = 0;
            ev_pcap_min->mode = PAPI_MODE_READ;
            ev_pcap_min->variant = 1;  // variant 1 => min
            ev_pcap_min->subvariant = 0;
            ev_pcap_min->open_func = open_simple;
            ev_pcap_min->close_func = close_simple;
            ev_pcap_min->start_func = start_simple;
            ev_pcap_min->stop_func = stop_simple;
            ev_pcap_min->access_func = access_amdsmi_power_cap_range;
            htable_insert(htable, ev_pcap_min->name, ev_pcap_min);
            idx++;
            // Maximum allowed power cap
            snprintf(name_buf, sizeof(name_buf), "power_cap_range_max:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d maximum allowed power cap (W)", d);
            native_event_t *ev_pcap_max = &ntv_table.events[idx];
            ev_pcap_max->id = idx;
            ev_pcap_max->name = strdup(name_buf);
            ev_pcap_max->descr = strdup(descr_buf);
            ev_pcap_max->device = d;
            ev_pcap_max->value = 0;
            ev_pcap_max->mode = PAPI_MODE_READ;
            ev_pcap_max->variant = 2;  // variant 2 => max
            ev_pcap_max->subvariant = 0;
            ev_pcap_max->open_func = open_simple;
            ev_pcap_max->close_func = close_simple;
            ev_pcap_max->start_func = start_simple;
            ev_pcap_max->stop_func = stop_simple;
            ev_pcap_max->access_func = access_amdsmi_power_cap_range;
            htable_insert(htable, ev_pcap_max->name, ev_pcap_max);
            idx++;
        }
    }
    /* PCIe throughput and replay counter metrics */
        for (int d = 0; d < gpu_count; ++d) {
        uint64_t tx = 0, rx = 0, pkt = 0;
        amdsmi_status_t st_thr =
            amdsmi_get_gpu_pci_throughput_p(device_handles[d],
                                            &tx, &rx, &pkt);
        if (st_thr == AMDSMI_STATUS_SUCCESS) {
            /* bytes sent per second */
            snprintf(name_buf, sizeof(name_buf),
                     "pci_throughput_sent:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf),
                     "Device %d PCIe bytes sent per second", d);
            native_event_t *ev_tx = &ntv_table.events[idx];
            ev_tx->id   = idx;
            ev_tx->name = strdup(name_buf);
            ev_tx->descr= strdup(descr_buf);
            ev_tx->device = d;
            ev_tx->value  = 0;
            ev_tx->mode   = PAPI_MODE_READ;
            ev_tx->variant= 0; /* sent */
            ev_tx->subvariant = 0;
            ev_tx->open_func  = open_simple;
            ev_tx->close_func = close_simple;
            ev_tx->start_func = start_simple;
            ev_tx->stop_func  = stop_simple;
            ev_tx->access_func= access_amdsmi_pci_throughput;
            htable_insert(htable, ev_tx->name, ev_tx);
            ++idx;
            /* bytes received per second */
            snprintf(name_buf, sizeof(name_buf),
                     "pci_throughput_received:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf),
                     "Device %d PCIe bytes received per second", d);
            native_event_t *ev_rx = &ntv_table.events[idx];
            ev_rx->id   = idx;
            ev_rx->name = strdup(name_buf);
            ev_rx->descr= strdup(descr_buf);
            ev_rx->device = d;
            ev_rx->value  = 0;
            ev_rx->mode   = PAPI_MODE_READ;
            ev_rx->variant= 1; /* received */
            ev_rx->subvariant = 0;
            ev_rx->open_func  = open_simple;
            ev_rx->close_func = close_simple;
            ev_rx->start_func = start_simple;
            ev_rx->stop_func  = stop_simple;
            ev_rx->access_func= access_amdsmi_pci_throughput;
            htable_insert(htable, ev_rx->name, ev_rx);
            ++idx;
            /* max packet size */
            snprintf(name_buf, sizeof(name_buf),
                     "pci_throughput_max_packet:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf),
                     "Device %d PCIe max packet size (bytes)", d);
            native_event_t *ev_pkt = &ntv_table.events[idx];
            ev_pkt->id   = idx;
            ev_pkt->name = strdup(name_buf);
            ev_pkt->descr= strdup(descr_buf);
            ev_pkt->device = d;
            ev_pkt->value  = 0;
            ev_pkt->mode   = PAPI_MODE_READ;
            ev_pkt->variant= 2; /* max pkt */
            ev_pkt->subvariant = 0;
            ev_pkt->open_func  = open_simple;
            ev_pkt->close_func = close_simple;
            ev_pkt->start_func = start_simple;
            ev_pkt->stop_func  = stop_simple;
            ev_pkt->access_func= access_amdsmi_pci_throughput;
            htable_insert(htable, ev_pkt->name, ev_pkt);
            ++idx;
        }
        uint64_t replay = 0;
        if (amdsmi_get_gpu_pci_replay_counter_p(device_handles[d],
                                                &replay) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf),
                     "pci_replay_counter:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf),
                     "Device %d PCIe replay (NAK) counter", d);
            native_event_t *ev_rep = &ntv_table.events[idx];
            ev_rep->id   = idx;
            ev_rep->name = strdup(name_buf);
            ev_rep->descr= strdup(descr_buf);
            ev_rep->device = d;
            ev_rep->value  = 0;
            ev_rep->mode   = PAPI_MODE_READ;
            ev_rep->variant= 0;
            ev_rep->subvariant = 0;
            ev_rep->open_func  = open_simple;
            ev_rep->close_func = close_simple;
            ev_rep->start_func = start_simple;
            ev_rep->stop_func  = stop_simple;
            ev_rep->access_func= access_amdsmi_pci_replay_counter;
            htable_insert(htable, ev_rep->name, ev_rep);
            ++idx;
        }
    }
    /* Additional GPU metrics and system information */
    /* GPU engine utilization metrics - test each device individually */
    for (int d = 0; d < gpu_count; ++d) {
        // Safety check for device handle
        if (!device_handles || !device_handles[d]) {
            continue;
        }
        // Register GFX activity event - test directly
        amdsmi_engine_usage_t dummy_usage;
        if (amdsmi_get_gpu_activity_p && 
            amdsmi_get_gpu_activity_p(device_handles[d], &dummy_usage) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "gfx_activity:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d GFX engine activity (%%)", d);
            native_event_t *ev_gfx = &ntv_table.events[idx];
            ev_gfx->id = idx;
            ev_gfx->name = strdup(name_buf);
            ev_gfx->descr = strdup(descr_buf);
            ev_gfx->device = d;
            ev_gfx->value = 0;
            ev_gfx->mode = PAPI_MODE_READ;
            ev_gfx->variant = 0;
            ev_gfx->subvariant = 0;
            ev_gfx->open_func = open_simple;
            ev_gfx->close_func = close_simple;
            ev_gfx->start_func = start_simple;
            ev_gfx->stop_func = stop_simple;
            ev_gfx->access_func = access_amdsmi_gpu_activity;
            htable_insert(htable, ev_gfx->name, ev_gfx);
            idx++;
            snprintf(name_buf, sizeof(name_buf), "umc_activity:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d UMC engine activity (%%)", d);
            native_event_t *ev_umc = &ntv_table.events[idx];
            ev_umc->id = idx;
            ev_umc->name = strdup(name_buf);
            ev_umc->descr = strdup(descr_buf);
            ev_umc->device = d;
            ev_umc->value = 0;
            ev_umc->mode = PAPI_MODE_READ;
            ev_umc->variant = 1;
            ev_umc->subvariant = 0;
            ev_umc->open_func = open_simple;
            ev_umc->close_func = close_simple;
            ev_umc->start_func = start_simple;
            ev_umc->stop_func = stop_simple;
            ev_umc->access_func = access_amdsmi_gpu_activity;
            htable_insert(htable, ev_umc->name, ev_umc);
            idx++;
            snprintf(name_buf, sizeof(name_buf), "mm_activity:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d MM engine activity (%%)", d);
            native_event_t *ev_mm = &ntv_table.events[idx];
            ev_mm->id = idx;
            ev_mm->name = strdup(name_buf);
            ev_mm->descr = strdup(descr_buf);
            ev_mm->device = d;
            ev_mm->value = 0;
            ev_mm->mode = PAPI_MODE_READ;
            ev_mm->variant = 2;
            ev_mm->subvariant = 0;
            ev_mm->open_func = open_simple;
            ev_mm->close_func = close_simple;
            ev_mm->start_func = start_simple;
            ev_mm->stop_func = stop_simple;
            ev_mm->access_func = access_amdsmi_gpu_activity;
            htable_insert(htable, ev_mm->name, ev_mm);
            idx++;
        }
    }
    /* GPU clock frequency levels */
    for (int d = 0; d < gpu_count; ++d) {
        amdsmi_frequencies_t f;
        if (amdsmi_get_clk_freq_p(device_handles[d], AMDSMI_CLK_TYPE_SYS, &f) != AMDSMI_STATUS_SUCCESS || f.num_supported == 0) {
            continue;
        }
        // Number of supported frequencies
        snprintf(name_buf, sizeof(name_buf), "clk_freq_count:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d number of supported GPU clock frequencies", d);
        native_event_t *ev_clk_count = &ntv_table.events[idx];
        ev_clk_count->id = idx;
        ev_clk_count->name = strdup(name_buf);
        ev_clk_count->descr = strdup(descr_buf);
        ev_clk_count->device = d;
        ev_clk_count->value = 0;
        ev_clk_count->mode = PAPI_MODE_READ;
        ev_clk_count->variant = 0;
        ev_clk_count->subvariant = 0;
        ev_clk_count->open_func = open_simple;
        ev_clk_count->close_func = close_simple;
        ev_clk_count->start_func = start_simple;
        ev_clk_count->stop_func = stop_simple;
        ev_clk_count->access_func = access_amdsmi_clk_freq;
        htable_insert(htable, ev_clk_count->name, ev_clk_count);
        idx++;
        // Current clock frequency
        snprintf(name_buf, sizeof(name_buf), "clk_freq_current:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d current GPU clock frequency (MHz)", d);
        native_event_t *ev_clk_cur = &ntv_table.events[idx];
        ev_clk_cur->id = idx;
        ev_clk_cur->name = strdup(name_buf);
        ev_clk_cur->descr = strdup(descr_buf);
        ev_clk_cur->device = d;
        ev_clk_cur->value = 0;
        ev_clk_cur->mode = PAPI_MODE_READ;
        ev_clk_cur->variant = 0;
        ev_clk_cur->subvariant = 1;
        ev_clk_cur->open_func = open_simple;
        ev_clk_cur->close_func = close_simple;
        ev_clk_cur->start_func = start_simple;
        ev_clk_cur->stop_func = stop_simple;
        ev_clk_cur->access_func = access_amdsmi_clk_freq;
        htable_insert(htable, ev_clk_cur->name, ev_clk_cur);
        idx++;
        // Supported frequency levels
        for (uint32_t fi = 0; fi < f.num_supported; ++fi) {
            snprintf(name_buf, sizeof(name_buf), "clk_freq_level_%u:device=%d", fi, d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d supported clock frequency level %u (MHz)", d, fi);
            native_event_t *ev_clk_lvl = &ntv_table.events[idx];
            ev_clk_lvl->id = idx;
            ev_clk_lvl->name = strdup(name_buf);
            ev_clk_lvl->descr = strdup(descr_buf);
            ev_clk_lvl->device = d;
            ev_clk_lvl->value = 0;
            ev_clk_lvl->mode = PAPI_MODE_READ;
            ev_clk_lvl->variant = 0;
            ev_clk_lvl->subvariant = fi + 2;
            ev_clk_lvl->open_func = open_simple;
            ev_clk_lvl->close_func = close_simple;
            ev_clk_lvl->start_func = start_simple;
            ev_clk_lvl->stop_func = stop_simple;
            ev_clk_lvl->access_func = access_amdsmi_clk_freq;
            htable_insert(htable, ev_clk_lvl->name, ev_clk_lvl);
            idx++;
        }
    }
    /* GPU identification and topology metrics */
    for (int d = 0; d < gpu_count; ++d) {
        uint16_t id16;
        uint64_t id64;
        //amdsmi_virtualization_mode_t vmode;
        int32_t numa;
        // GPU ID
        if (amdsmi_get_gpu_id_p(device_handles[d], &id16) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "gpu_id:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d GPU identifier (Device ID)", d);
            native_event_t *ev_id = &ntv_table.events[idx];
            ev_id->id = idx;
            ev_id->name = strdup(name_buf);
            ev_id->descr = strdup(descr_buf);
            ev_id->device = d;
            ev_id->value = 0;
            ev_id->mode = PAPI_MODE_READ;
            ev_id->variant = 0;
            ev_id->subvariant = 0;
            ev_id->open_func = open_simple;
            ev_id->close_func = close_simple;
            ev_id->start_func = start_simple;
            ev_id->stop_func = stop_simple;
            ev_id->access_func = access_amdsmi_gpu_info;
            htable_insert(htable, ev_id->name, ev_id);
            idx++;
        }
        // GPU Revision
        if (amdsmi_get_gpu_revision_p(device_handles[d], &id16) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "gpu_revision:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d GPU revision ID", d);
            native_event_t *ev_rev = &ntv_table.events[idx];
            ev_rev->id = idx;
            ev_rev->name = strdup(name_buf);
            ev_rev->descr = strdup(descr_buf);
            ev_rev->device = d;
            ev_rev->value = 0;
            ev_rev->mode = PAPI_MODE_READ;
            ev_rev->variant = 1;
            ev_rev->subvariant = 0;
            ev_rev->open_func = open_simple;
            ev_rev->close_func = close_simple;
            ev_rev->start_func = start_simple;
            ev_rev->stop_func = stop_simple;
            ev_rev->access_func = access_amdsmi_gpu_info;
            htable_insert(htable, ev_rev->name, ev_rev);
            idx++;
        }
        // GPU Subsystem ID
        if (amdsmi_get_gpu_subsystem_id_p(device_handles[d], &id16) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "gpu_subsystem_id:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d GPU subsystem ID", d);
            native_event_t *ev_subid = &ntv_table.events[idx];
            ev_subid->id = idx;
            ev_subid->name = strdup(name_buf);
            ev_subid->descr = strdup(descr_buf);
            ev_subid->device = d;
            ev_subid->value = 0;
            ev_subid->mode = PAPI_MODE_READ;
            ev_subid->variant = 2;
            ev_subid->subvariant = 0;
            ev_subid->open_func = open_simple;
            ev_subid->close_func = close_simple;
            ev_subid->start_func = start_simple;
            ev_subid->stop_func = stop_simple;
            ev_subid->access_func = access_amdsmi_gpu_info;
            htable_insert(htable, ev_subid->name, ev_subid);
            idx++;
        }
        // GPU BDF ID
        if (amdsmi_get_gpu_bdf_id_p(device_handles[d], &id64) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "gpu_bdfid:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d GPU PCI BDF identifier", d);
            native_event_t *ev_bdf = &ntv_table.events[idx];
            ev_bdf->id = idx;
            ev_bdf->name = strdup(name_buf);
            ev_bdf->descr = strdup(descr_buf);
            ev_bdf->device = d;
            ev_bdf->value = 0;
            ev_bdf->mode = PAPI_MODE_READ;
            ev_bdf->variant = 3;
            ev_bdf->subvariant = 0;
            ev_bdf->open_func = open_simple;
            ev_bdf->close_func = close_simple;
            ev_bdf->start_func = start_simple;
            ev_bdf->stop_func = stop_simple;
            ev_bdf->access_func = access_amdsmi_gpu_info;
            htable_insert(htable, ev_bdf->name, ev_bdf);
            idx++;
        }
        /*
        // GPU Virtualization Mode
        if (amdsmi_get_gpu_virtualization_mode_p(device_handles[d], &vmode) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "gpu_virtualization_mode:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d GPU virtualization mode", d);
            native_event_t *ev_vmode = &ntv_table.events[idx];
            ev_vmode->id = idx;
            ev_vmode->name = strdup(name_buf);
            ev_vmode->descr = strdup(descr_buf);
            ev_vmode->device = d;
            ev_vmode->value = 0;
            ev_vmode->mode = PAPI_MODE_READ;
            ev_vmode->variant = 4;
            ev_vmode->subvariant = 0;
            ev_vmode->open_func = open_simple;
            ev_vmode->close_func = close_simple;
            ev_vmode->start_func = start_simple;
            ev_vmode->stop_func = stop_simple;
            ev_vmode->access_func = access_amdsmi_gpu_info;
            htable_insert(htable, ev_vmode->name, ev_vmode);
            idx++;
        }
        */
        // GPU NUMA Node
        if (amdsmi_get_gpu_topo_numa_affinity_p(device_handles[d], &numa) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "numa_node:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d NUMA node", d);
            native_event_t *ev_numa = &ntv_table.events[idx];
            ev_numa->id = idx;
            ev_numa->name = strdup(name_buf);
            ev_numa->descr = strdup(descr_buf);
            ev_numa->device = d;
            ev_numa->value = 0;
            ev_numa->mode = PAPI_MODE_READ;
            ev_numa->variant = 5;
            ev_numa->subvariant = 0;
            ev_numa->open_func = open_simple;
            ev_numa->close_func = close_simple;
            ev_numa->start_func = start_simple;
            ev_numa->stop_func = stop_simple;
            ev_numa->access_func = access_amdsmi_gpu_info;
            htable_insert(htable, ev_numa->name, ev_numa);
            idx++;
        }
    }
    /* Energy consumption counter */
    for (int d = 0; d < gpu_count; ++d) {
        uint64_t energy = 0;
        float resolution = 0.0;
        uint64_t timestamp = 0;
        if (amdsmi_get_energy_count_p(device_handles[d], &energy, &resolution, &timestamp) != AMDSMI_STATUS_SUCCESS) {
            continue;
        }
        snprintf(name_buf, sizeof(name_buf), "energy_consumed:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d energy consumed (microJoules)", d);
        native_event_t *ev_energy = &ntv_table.events[idx];
        ev_energy->id = idx;
        ev_energy->name = strdup(name_buf);
        ev_energy->descr = strdup(descr_buf);
        ev_energy->device = d;
        ev_energy->value = 0;
        ev_energy->mode = PAPI_MODE_READ;
        ev_energy->variant = 0;
        ev_energy->subvariant = 0;
        ev_energy->open_func = open_simple;
        ev_energy->close_func = close_simple;
        ev_energy->start_func = start_simple;
        ev_energy->stop_func = stop_simple;
        ev_energy->access_func = access_amdsmi_energy_count;
        htable_insert(htable, ev_energy->name, ev_energy);
        idx++;
    }
    /* GPU power profile information */
    for (int d = 0; d < gpu_count; ++d) {
        amdsmi_power_profile_status_t profile_status;
        if (amdsmi_get_gpu_power_profile_presets_p(device_handles[d], 0, &profile_status) != AMDSMI_STATUS_SUCCESS) {
            continue;
        }
        snprintf(name_buf, sizeof(name_buf), "power_profiles_count:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d number of supported power profiles", d);
        native_event_t *ev_prof_count = &ntv_table.events[idx];
        ev_prof_count->id = idx;
        ev_prof_count->name = strdup(name_buf);
        ev_prof_count->descr = strdup(descr_buf);
        ev_prof_count->device = d;
        ev_prof_count->value = 0;
        ev_prof_count->mode = PAPI_MODE_READ;
        ev_prof_count->variant = 0;
        ev_prof_count->subvariant = 0;
        ev_prof_count->open_func = open_simple;
        ev_prof_count->close_func = close_simple;
        ev_prof_count->start_func = start_simple;
        ev_prof_count->stop_func = stop_simple;
        ev_prof_count->access_func = access_amdsmi_power_profile_status;
        htable_insert(htable, ev_prof_count->name, ev_prof_count);
        idx++;
        snprintf(name_buf, sizeof(name_buf), "power_profile_current:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d current power profile mask", d);
        native_event_t *ev_prof_curr = &ntv_table.events[idx];
        ev_prof_curr->id = idx;
        ev_prof_curr->name = strdup(name_buf);
        ev_prof_curr->descr = strdup(descr_buf);
        ev_prof_curr->device = d;
        ev_prof_curr->value = 0;
        ev_prof_curr->mode = PAPI_MODE_READ;
        ev_prof_curr->variant = 1;
        ev_prof_curr->subvariant = 0;
        ev_prof_curr->open_func = open_simple;
        ev_prof_curr->close_func = close_simple;
        ev_prof_curr->start_func = start_simple;
        ev_prof_curr->stop_func = stop_simple;
        ev_prof_curr->access_func = access_amdsmi_power_profile_status;
        htable_insert(htable, ev_prof_curr->name, ev_prof_curr);
        idx++;
    }
    
        /* GPU RAS and ECC metrics events */
    for (int d = 0; d < gpu_count; ++d) {
        if (!device_handles[d]) continue;
        uint64_t ras_mask;
        if (amdsmi_get_gpu_ras_block_features_enabled_p &&
            amdsmi_get_gpu_ras_block_features_enabled_p(device_handles[d], &ras_mask) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "ras_blocks_enabled:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d RAS enabled block mask", d);
            native_event_t *ev_ras = &ntv_table.events[idx];
            ev_ras->id = idx;
            ev_ras->name = strdup(name_buf);
            ev_ras->descr = strdup(descr_buf);
            ev_ras->device = d;
            ev_ras->value = 0;
            ev_ras->mode = PAPI_MODE_READ;
            ev_ras->variant = 0;
            ev_ras->subvariant = 0;
            ev_ras->open_func = open_simple;
            ev_ras->close_func = close_simple;
            ev_ras->start_func = start_simple;
            ev_ras->stop_func = stop_simple;
            ev_ras->access_func = access_amdsmi_ras_block_enabled;
            htable_insert(htable, ev_ras->name, ev_ras);
            idx++;
        }
        amdsmi_error_count_t ecc_counts;
        if (amdsmi_get_gpu_ecc_count_p &&
            amdsmi_get_gpu_ecc_count_p(device_handles[d], &ecc_counts) == AMDSMI_STATUS_SUCCESS) {
            // Correctable ECC errors
            snprintf(name_buf, sizeof(name_buf), "ecc_correctable:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d ECC correctable error count", d);
            native_event_t *ev_ecc_ce = &ntv_table.events[idx];
            ev_ecc_ce->id = idx;
            ev_ecc_ce->name = strdup(name_buf);
            ev_ecc_ce->descr = strdup(descr_buf);
            ev_ecc_ce->device = d;
            ev_ecc_ce->value = 0;
            ev_ecc_ce->mode = PAPI_MODE_READ;
            ev_ecc_ce->variant = 0;  // 0 for correctable
            ev_ecc_ce->subvariant = 0;
            ev_ecc_ce->open_func = open_simple;
            ev_ecc_ce->close_func = close_simple;
            ev_ecc_ce->start_func = start_simple;
            ev_ecc_ce->stop_func = stop_simple;
            ev_ecc_ce->access_func = access_amdsmi_ecc_count;
            htable_insert(htable, ev_ecc_ce->name, ev_ecc_ce);
            idx++;
            // Uncorrectable ECC errors
            snprintf(name_buf, sizeof(name_buf), "ecc_uncorrectable:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d ECC uncorrectable error count", d);
            native_event_t *ev_ecc_ue = &ntv_table.events[idx];
            ev_ecc_ue->id = idx;
            ev_ecc_ue->name = strdup(name_buf);
            ev_ecc_ue->descr = strdup(descr_buf);
            ev_ecc_ue->device = d;
            ev_ecc_ue->value = 0;
            ev_ecc_ue->mode = PAPI_MODE_READ;
            ev_ecc_ue->variant = 1;  // 1 for uncorrectable
            ev_ecc_ue->subvariant = 0;
            ev_ecc_ue->open_func = open_simple;
            ev_ecc_ue->close_func = close_simple;
            ev_ecc_ue->start_func = start_simple;
            ev_ecc_ue->stop_func = stop_simple;
            ev_ecc_ue->access_func = access_amdsmi_ecc_count;
            htable_insert(htable, ev_ecc_ue->name, ev_ecc_ue);
            idx++;
        }
        if (amdsmi_get_gpu_total_ecc_count_p &&
            amdsmi_get_gpu_total_ecc_count_p(device_handles[d], &ecc_counts) == AMDSMI_STATUS_SUCCESS) {
            // Total correctable ECC errors (all time)
            snprintf(name_buf, sizeof(name_buf), "ecc_correctable_total:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d total correctable ECC errors", d);
            native_event_t *ev_ecc_ce_tot = &ntv_table.events[idx];
            ev_ecc_ce_tot->id = idx;
            ev_ecc_ce_tot->name = strdup(name_buf);
            ev_ecc_ce_tot->descr = strdup(descr_buf);
            ev_ecc_ce_tot->device = d;
            ev_ecc_ce_tot->value = 0;
            ev_ecc_ce_tot->mode = PAPI_MODE_READ;
            ev_ecc_ce_tot->variant = 0;
            ev_ecc_ce_tot->subvariant = 0;
            ev_ecc_ce_tot->open_func = open_simple;
            ev_ecc_ce_tot->close_func = close_simple;
            ev_ecc_ce_tot->start_func = start_simple;
            ev_ecc_ce_tot->stop_func = stop_simple;
            ev_ecc_ce_tot->access_func = access_amdsmi_ecc_total;
            htable_insert(htable, ev_ecc_ce_tot->name, ev_ecc_ce_tot);
            idx++;
            // Total uncorrectable ECC errors
            snprintf(name_buf, sizeof(name_buf), "ecc_uncorrectable_total:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d total uncorrectable ECC errors", d);
            native_event_t *ev_ecc_ue_tot = &ntv_table.events[idx];
            ev_ecc_ue_tot->id = idx;
            ev_ecc_ue_tot->name = strdup(name_buf);
            ev_ecc_ue_tot->descr = strdup(descr_buf);
            ev_ecc_ue_tot->device = d;
            ev_ecc_ue_tot->value = 0;
            ev_ecc_ue_tot->mode = PAPI_MODE_READ;
            ev_ecc_ue_tot->variant = 1;
            ev_ecc_ue_tot->subvariant = 0;
            ev_ecc_ue_tot->open_func = open_simple;
            ev_ecc_ue_tot->close_func = close_simple;
            ev_ecc_ue_tot->start_func = start_simple;
            ev_ecc_ue_tot->stop_func = stop_simple;
            ev_ecc_ue_tot->access_func = access_amdsmi_ecc_total;
            htable_insert(htable, ev_ecc_ue_tot->name, ev_ecc_ue_tot);
            idx++;
        }
        uint32_t ecc_enabled;
        if (amdsmi_get_gpu_ecc_enabled_p &&
            amdsmi_get_gpu_ecc_enabled_p(device_handles[d], &ecc_enabled) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "ecc_enabled:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d ECC enabled (1 or 0)", d);
            native_event_t *ev_ecc_en = &ntv_table.events[idx];
            ev_ecc_en->id = idx;
            ev_ecc_en->name = strdup(name_buf);
            ev_ecc_en->descr = strdup(descr_buf);
            ev_ecc_en->device = d;
            ev_ecc_en->value = 0;
            ev_ecc_en->mode = PAPI_MODE_READ;
            ev_ecc_en->variant = 0;
            ev_ecc_en->subvariant = 0;
            ev_ecc_en->open_func = open_simple;
            ev_ecc_en->close_func = close_simple;
            ev_ecc_en->start_func = start_simple;
            ev_ecc_en->stop_func = stop_simple;
            ev_ecc_en->access_func = access_amdsmi_ecc_enabled;
            htable_insert(htable, ev_ecc_en->name, ev_ecc_en);
            idx++;
        }
        uint64_t xgmi_err;
        if (amdsmi_gpu_xgmi_error_status_p &&
            amdsmi_gpu_xgmi_error_status_p(device_handles[d], &xgmi_err) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "xgmi_error_count:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d XGMI error count", d);
            native_event_t *ev_xgmi_err = &ntv_table.events[idx];
            ev_xgmi_err->id = idx;
            ev_xgmi_err->name = strdup(name_buf);
            ev_xgmi_err->descr = strdup(descr_buf);
            ev_xgmi_err->device = d;
            ev_xgmi_err->value = 0;
            ev_xgmi_err->mode = PAPI_MODE_READ;
            ev_xgmi_err->variant = 0;
            ev_xgmi_err->subvariant = 0;
            ev_xgmi_err->open_func = open_simple;
            ev_xgmi_err->close_func = close_simple;
            ev_xgmi_err->start_func = start_simple;
            ev_xgmi_err->stop_func = stop_simple;
            ev_xgmi_err->access_func = access_amdsmi_xgmi_error;
            htable_insert(htable, ev_xgmi_err->name, ev_xgmi_err);
            idx++;
        }
    }
    /* GPU voltage and power metrics events */
    for (int d = 0; d < gpu_count; ++d) {
        if (!device_handles[d]) continue;
        // Voltage metrics (VDDGFX) if available
        int64_t dummy_volt;
        if (amdsmi_get_gpu_volt_metric_p &&
            amdsmi_get_gpu_volt_metric_p(device_handles[d], AMDSMI_VOLT_TYPE_VDDGFX, AMDSMI_VOLT_CURRENT, &dummy_volt) == AMDSMI_STATUS_SUCCESS) {
            // Current voltage (mV)
            snprintf(name_buf, sizeof(name_buf), "vddgfx_voltage_current:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d VDDGFX current voltage (mV)", d);
            native_event_t *ev_vcur = &ntv_table.events[idx];
            ev_vcur->id = idx;
            ev_vcur->name = strdup(name_buf);
            ev_vcur->descr = strdup(descr_buf);
            ev_vcur->device = d;
            ev_vcur->value = 0;
            ev_vcur->mode = PAPI_MODE_READ;
            ev_vcur->variant = AMDSMI_VOLT_CURRENT;
            ev_vcur->subvariant = AMDSMI_VOLT_TYPE_VDDGFX;
            ev_vcur->open_func = open_simple;
            ev_vcur->close_func = close_simple;
            ev_vcur->start_func = start_simple;
            ev_vcur->stop_func = stop_simple;
            ev_vcur->access_func = access_amdsmi_volt_metric;
            htable_insert(htable, ev_vcur->name, ev_vcur);
            idx++;
            // Average voltage
            snprintf(name_buf, sizeof(name_buf), "vddgfx_voltage_average:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d VDDGFX average voltage (mV)", d);
            native_event_t *ev_vavg = &ntv_table.events[idx];
            ev_vavg->id = idx;
            ev_vavg->name = strdup(name_buf);
            ev_vavg->descr = strdup(descr_buf);
            ev_vavg->device = d;
            ev_vavg->value = 0;
            ev_vavg->mode = PAPI_MODE_READ;
            ev_vavg->variant = AMDSMI_VOLT_AVERAGE;
            ev_vavg->subvariant = AMDSMI_VOLT_TYPE_VDDGFX;
            ev_vavg->open_func = open_simple;
            ev_vavg->close_func = close_simple;
            ev_vavg->start_func = start_simple;
            ev_vavg->stop_func = stop_simple;
            ev_vavg->access_func = access_amdsmi_volt_metric;
            htable_insert(htable, ev_vavg->name, ev_vavg);
            idx++;
            // Minimum voltage
            snprintf(name_buf, sizeof(name_buf), "vddgfx_voltage_min:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d VDDGFX lowest voltage (mV)", d);
            native_event_t *ev_vmin = &ntv_table.events[idx];
            ev_vmin->id = idx;
            ev_vmin->name = strdup(name_buf);
            ev_vmin->descr = strdup(descr_buf);
            ev_vmin->device = d;
            ev_vmin->value = 0;
            ev_vmin->mode = PAPI_MODE_READ;
            ev_vmin->variant = AMDSMI_VOLT_MIN;
            ev_vmin->subvariant = AMDSMI_VOLT_TYPE_VDDGFX;
            ev_vmin->open_func = open_simple;
            ev_vmin->close_func = close_simple;
            ev_vmin->start_func = start_simple;
            ev_vmin->stop_func = stop_simple;
            ev_vmin->access_func = access_amdsmi_volt_metric;
            htable_insert(htable, ev_vmin->name, ev_vmin);
            idx++;
            // Maximum voltage
            snprintf(name_buf, sizeof(name_buf), "vddgfx_voltage_max:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d VDDGFX highest voltage (mV)", d);
            native_event_t *ev_vmax = &ntv_table.events[idx];
            ev_vmax->id = idx;
            ev_vmax->name = strdup(name_buf);
            ev_vmax->descr = strdup(descr_buf);
            ev_vmax->device = d;
            ev_vmax->value = 0;
            ev_vmax->mode = PAPI_MODE_READ;
            ev_vmax->variant = AMDSMI_VOLT_MAX;
            ev_vmax->subvariant = AMDSMI_VOLT_TYPE_VDDGFX;
            ev_vmax->open_func = open_simple;
            ev_vmax->close_func = close_simple;
            ev_vmax->start_func = start_simple;
            ev_vmax->stop_func = stop_simple;
            ev_vmax->access_func = access_amdsmi_volt_metric;
            htable_insert(htable, ev_vmax->name, ev_vmax);
            idx++;
        }
        // Performance level
        amdsmi_dev_perf_level_t perf;
        if (amdsmi_get_gpu_perf_level_p &&
            amdsmi_get_gpu_perf_level_p(device_handles[d], &perf) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "perf_level:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d performance level (enum)", d);
            native_event_t *ev_perf = &ntv_table.events[idx];
            ev_perf->id = idx;
            ev_perf->name = strdup(name_buf);
            ev_perf->descr = strdup(descr_buf);
            ev_perf->device = d;
            ev_perf->value = 0;
            ev_perf->mode = PAPI_MODE_READ;
            ev_perf->variant = 0;
            ev_perf->subvariant = 0;
            ev_perf->open_func = open_simple;
            ev_perf->close_func = close_simple;
            ev_perf->start_func = start_simple;
            ev_perf->stop_func = stop_simple;
            ev_perf->access_func = access_amdsmi_perf_level;
            htable_insert(htable, ev_perf->name, ev_perf);
            idx++;
        }
        // Overdrive settings
        uint32_t od_val;
        if (amdsmi_get_gpu_overdrive_level_p &&
            amdsmi_get_gpu_overdrive_level_p(device_handles[d], &od_val) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "gpu_overdrive_level:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d GPU core overdrive level", d);
            native_event_t *ev_od = &ntv_table.events[idx];
            ev_od->id = idx;
            ev_od->name = strdup(name_buf);
            ev_od->descr = strdup(descr_buf);
            ev_od->device = d;
            ev_od->value = 0;
            ev_od->mode = PAPI_MODE_READ;
            ev_od->variant = 0;
            ev_od->subvariant = 0;
            ev_od->open_func = open_simple;
            ev_od->close_func = close_simple;
            ev_od->start_func = start_simple;
            ev_od->stop_func = stop_simple;
            ev_od->access_func = access_amdsmi_overdrive;
            htable_insert(htable, ev_od->name, ev_od);
            idx++;
        }
        if (amdsmi_get_gpu_mem_overdrive_level_p &&
            amdsmi_get_gpu_mem_overdrive_level_p(device_handles[d], &od_val) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "mem_overdrive_level:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d VRAM overdrive level", d);
            native_event_t *ev_mod = &ntv_table.events[idx];
            ev_mod->id = idx;
            ev_mod->name = strdup(name_buf);
            ev_mod->descr = strdup(descr_buf);
            ev_mod->device = d;
            ev_mod->value = 0;
            ev_mod->mode = PAPI_MODE_READ;
            ev_mod->variant = 0;
            ev_mod->subvariant = 0;
            ev_mod->open_func = open_simple;
            ev_mod->close_func = close_simple;
            ev_mod->start_func = start_simple;
            ev_mod->stop_func = stop_simple;
            ev_mod->access_func = access_amdsmi_mem_overdrive;
            htable_insert(htable, ev_mod->name, ev_mod);
            idx++;
        }
    }
    /* GPU partitioning and isolation events */
    for (int d = 0; d < gpu_count; ++d) {
        if (!device_handles[d]) continue;
        uint32_t part_val;
        if (amdsmi_get_gpu_compute_partition_p &&
            amdsmi_get_gpu_compute_partition_p(device_handles[d], &part_val) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "compute_partition_mode:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d compute partition mode (ID)", d);
            native_event_t *ev_cmp = &ntv_table.events[idx];
            ev_cmp->id = idx;
            ev_cmp->name = strdup(name_buf);
            ev_cmp->descr = strdup(descr_buf);
            ev_cmp->device = d;
            ev_cmp->value = 0;
            ev_cmp->mode = PAPI_MODE_READ;
            ev_cmp->variant = 0;
            ev_cmp->subvariant = 0;
            ev_cmp->open_func = open_simple;
            ev_cmp->close_func = close_simple;
            ev_cmp->start_func = start_simple;
            ev_cmp->stop_func = stop_simple;
            ev_cmp->access_func = access_amdsmi_compute_partition;
            htable_insert(htable, ev_cmp->name, ev_cmp);
            idx++;
        }
        if (amdsmi_get_gpu_memory_partition_p &&
            amdsmi_get_gpu_memory_partition_p(device_handles[d], &part_val) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "memory_partition_mode:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d memory partition mode (ID)", d);
            native_event_t *ev_mpart = &ntv_table.events[idx];
            ev_mpart->id = idx;
            ev_mpart->name = strdup(name_buf);
            ev_mpart->descr = strdup(descr_buf);
            ev_mpart->device = d;
            ev_mpart->value = 0;
            ev_mpart->mode = PAPI_MODE_READ;
            ev_mpart->variant = 0;
            ev_mpart->subvariant = 0;
            ev_mpart->open_func = open_simple;
            ev_mpart->close_func = close_simple;
            ev_mpart->start_func = start_simple;
            ev_mpart->stop_func = stop_simple;
            ev_mpart->access_func = access_amdsmi_memory_partition;
            htable_insert(htable, ev_mpart->name, ev_mpart);
            idx++;
        }
        if (amdsmi_get_gpu_accelerator_partition_profile_p &&
            amdsmi_get_gpu_accelerator_partition_profile_p(device_handles[d], (amdsmi_accelerator_partition_profile_t *)&part_val) == AMDSMI_STATUS_SUCCESS) {
            // Using part_val as profile ID for simplicity
            snprintf(name_buf, sizeof(name_buf), "accelerator_profile:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d accelerator partition profile ID", d);
            native_event_t *ev_prof = &ntv_table.events[idx];
            ev_prof->id = idx;
            ev_prof->name = strdup(name_buf);
            ev_prof->descr = strdup(descr_buf);
            ev_prof->device = d;
            ev_prof->value = 0;
            ev_prof->mode = PAPI_MODE_READ;
            ev_prof->variant = 0;
            ev_prof->subvariant = 0;
            ev_prof->open_func = open_simple;
            ev_prof->close_func = close_simple;
            ev_prof->start_func = start_simple;
            ev_prof->stop_func = stop_simple;
            ev_prof->access_func = access_amdsmi_accel_profile;
            htable_insert(htable, ev_prof->name, ev_prof);
            idx++;
        }
        uint32_t iso;
        if (amdsmi_get_gpu_process_isolation_p &&
            amdsmi_get_gpu_process_isolation_p(device_handles[d], &iso) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "process_isolation:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d process isolation enabled (1/0)", d);
            native_event_t *ev_iso = &ntv_table.events[idx];
            ev_iso->id = idx;
            ev_iso->name = strdup(name_buf);
            ev_iso->descr = strdup(descr_buf);
            ev_iso->device = d;
            ev_iso->value = 0;
            ev_iso->mode = PAPI_MODE_READ;
            ev_iso->variant = 0;
            ev_iso->subvariant = 0;
            ev_iso->open_func = open_simple;
            ev_iso->close_func = close_simple;
            ev_iso->start_func = start_simple;
            ev_iso->stop_func = stop_simple;
            ev_iso->access_func = access_amdsmi_process_isolation;
            htable_insert(htable, ev_iso->name, ev_iso);
            idx++;
        }
    }
    /* GPU memory and PCIe information events */
    for (int d = 0; d < gpu_count; ++d) {
        if (!device_handles[d]) continue;
        // VRAM vendor and type
        amdsmi_vram_info_t vram;
        if (amdsmi_get_gpu_vram_info_p &&
            amdsmi_get_gpu_vram_info_p(device_handles[d], &vram) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "vram_type:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d VRAM memory type (enum)", d);
            native_event_t *ev_vtype = &ntv_table.events[idx];
            ev_vtype->id = idx;
            ev_vtype->name = strdup(name_buf);
            ev_vtype->descr = strdup(descr_buf);
            ev_vtype->device = d;
            ev_vtype->value = 0;
            ev_vtype->mode = PAPI_MODE_READ;
            ev_vtype->variant = 1;  // 1 = vendor, 0 = type (we'll invert in access)
            ev_vtype->subvariant = 0;
            ev_vtype->open_func = open_simple;
            ev_vtype->close_func = close_simple;
            ev_vtype->start_func = start_simple;
            ev_vtype->stop_func = stop_simple;
            ev_vtype->access_func = access_amdsmi_vram_info;
            htable_insert(htable, ev_vtype->name, ev_vtype);
            idx++;
            snprintf(name_buf, sizeof(name_buf), "vram_vendor_id:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d VRAM vendor ID (enum code)", d);
            native_event_t *ev_vven = &ntv_table.events[idx];
            ev_vven->id = idx;
            ev_vven->name = strdup(name_buf);
            ev_vven->descr = strdup(descr_buf);
            ev_vven->device = d;
            ev_vven->value = 0;
            ev_vven->mode = PAPI_MODE_READ;
            ev_vven->variant = 0;  // 0 = type, 1 = vendor (swap logic in access)
            ev_vven->subvariant = 0;
            ev_vven->open_func = open_simple;
            ev_vven->close_func = close_simple;
            ev_vven->start_func = start_simple;
            ev_vven->stop_func = stop_simple;
            ev_vven->access_func = access_amdsmi_vram_info;
            htable_insert(htable, ev_vven->name, ev_vven);
            idx++;
        }
        // PCIe link information
        amdsmi_pcie_info_t pinfo;
        if (amdsmi_get_pcie_info_p &&
            amdsmi_get_pcie_info_p(device_handles[d], &pinfo) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "pcie_link_gen_current:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d PCIe current link generation", d);
            native_event_t *ev_lgen = &ntv_table.events[idx];
            ev_lgen->id = idx;
            ev_lgen->name = strdup(name_buf);
            ev_lgen->descr = strdup(descr_buf);
            ev_lgen->device = d;
            ev_lgen->value = 0;
            ev_lgen->mode = PAPI_MODE_READ;
            ev_lgen->variant = 0;
            ev_lgen->subvariant = 0;
            ev_lgen->open_func = open_simple;
            ev_lgen->close_func = close_simple;
            ev_lgen->start_func = start_simple;
            ev_lgen->stop_func = stop_simple;
            ev_lgen->access_func = access_amdsmi_pcie_info;
            htable_insert(htable, ev_lgen->name, ev_lgen);
            idx++;
            snprintf(name_buf, sizeof(name_buf), "pcie_link_width_current:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d PCIe current link width (lanes)", d);
            native_event_t *ev_lwid = &ntv_table.events[idx];
            ev_lwid->id = idx;
            ev_lwid->name = strdup(name_buf);
            ev_lwid->descr = strdup(descr_buf);
            ev_lwid->device = d;
            ev_lwid->value = 0;
            ev_lwid->mode = PAPI_MODE_READ;
            ev_lwid->variant = 1;
            ev_lwid->subvariant = 0;
            ev_lwid->open_func = open_simple;
            ev_lwid->close_func = close_simple;
            ev_lwid->start_func = start_simple;
            ev_lwid->stop_func = stop_simple;
            ev_lwid->access_func = access_amdsmi_pcie_info;
            htable_insert(htable, ev_lwid->name, ev_lwid);
            idx++;
            snprintf(name_buf, sizeof(name_buf), "pcie_link_gen_max:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d PCIe maximum supported link generation", d);
            native_event_t *ev_lgenmax = &ntv_table.events[idx];
            ev_lgenmax->id = idx;
            ev_lgenmax->name = strdup(name_buf);
            ev_lgenmax->descr = strdup(descr_buf);
            ev_lgenmax->device = d;
            ev_lgenmax->value = 0;
            ev_lgenmax->mode = PAPI_MODE_READ;
            ev_lgenmax->variant = 2;
            ev_lgenmax->subvariant = 0;
            ev_lgenmax->open_func = open_simple;
            ev_lgenmax->close_func = close_simple;
            ev_lgenmax->start_func = start_simple;
            ev_lgenmax->stop_func = stop_simple;
            ev_lgenmax->access_func = access_amdsmi_pcie_info;
            htable_insert(htable, ev_lgenmax->name, ev_lgenmax);
            idx++;
            snprintf(name_buf, sizeof(name_buf), "pcie_link_width_max:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d PCIe maximum supported link width", d);
            native_event_t *ev_lwidmax = &ntv_table.events[idx];
            ev_lwidmax->id = idx;
            ev_lwidmax->name = strdup(name_buf);
            ev_lwidmax->descr = strdup(descr_buf);
            ev_lwidmax->device = d;
            ev_lwidmax->value = 0;
            ev_lwidmax->mode = PAPI_MODE_READ;
            ev_lwidmax->variant = 3;
            ev_lwidmax->subvariant = 0;
            ev_lwidmax->open_func = open_simple;
            ev_lwidmax->close_func = close_simple;
            ev_lwidmax->start_func = start_simple;
            ev_lwidmax->stop_func = stop_simple;
            ev_lwidmax->access_func = access_amdsmi_pcie_info;
            htable_insert(htable, ev_lwidmax->name, ev_lwidmax);
            idx++;
        }
        // Process isolation status and power management
        uint32_t pm_en;
        if (amdsmi_is_gpu_power_management_enabled_p &&
            amdsmi_is_gpu_power_management_enabled_p(device_handles[d], &pm_en) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "power_management_enabled:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d power management enabled (1/0)", d);
            native_event_t *ev_pm = &ntv_table.events[idx];
            ev_pm->id = idx;
            ev_pm->name = strdup(name_buf);
            ev_pm->descr = strdup(descr_buf);
            ev_pm->device = d;
            ev_pm->value = 0;
            ev_pm->mode = PAPI_MODE_READ;
            ev_pm->variant = 0;
            ev_pm->subvariant = 0;
            ev_pm->open_func = open_simple;
            ev_pm->close_func = close_simple;
            ev_pm->start_func = start_simple;
            ev_pm->stop_func = stop_simple;
            ev_pm->access_func = access_amdsmi_power_management;
            htable_insert(htable, ev_pm->name, ev_pm);
            idx++;
        }
    }
    /* GPU topology (XGMI/PCIE) link events between devices */
    for (int i = 0; i < gpu_count; ++i) {
        if (!device_handles[i]) continue;
        for (int j = i + 1; j < gpu_count; ++j) {
            if (!device_handles[j]) continue;
            uint64_t link_weight;
            if (amdsmi_topo_get_link_weight_p &&
                amdsmi_topo_get_link_weight_p(device_handles[i], device_handles[j], &link_weight) == AMDSMI_STATUS_SUCCESS) {
                snprintf(name_buf, sizeof(name_buf), "link_weight:device=%d:peer=%d", i, j);
                snprintf(descr_buf, sizeof(descr_buf), "Link weight between device %d and %d", i, j);
                native_event_t *ev_lw = &ntv_table.events[idx];
                ev_lw->id = idx;
                ev_lw->name = strdup(name_buf);
                ev_lw->descr = strdup(descr_buf);
                ev_lw->device = i;
                ev_lw->value = 0;
                ev_lw->mode = PAPI_MODE_READ;
                ev_lw->variant = 0;
                ev_lw->subvariant = j;
                ev_lw->open_func = open_simple;
                ev_lw->close_func = close_simple;
                ev_lw->start_func = start_simple;
                ev_lw->stop_func = stop_simple;
                ev_lw->access_func = access_amdsmi_link_weight;
                htable_insert(htable, ev_lw->name, ev_lw);
                idx++;
            }
            uint64_t _hops = 0; amdsmi_io_link_type_t link_type;
            if (amdsmi_topo_get_link_type_p && amdsmi_topo_get_link_type_p(device_handles[i], device_handles[j], &_hops, &link_type) == AMDSMI_STATUS_SUCCESS) {
                snprintf(name_buf, sizeof(name_buf), "link_type:device=%d:peer=%d", i, j);
                snprintf(descr_buf, sizeof(descr_buf), "Link type between device %d and %d", i, j);
                native_event_t *ev_lt = &ntv_table.events[idx];
                ev_lt->id = idx;
                ev_lt->name = strdup(name_buf);
                ev_lt->descr = strdup(descr_buf);
                ev_lt->device = i;
                ev_lt->value = 0;
                ev_lt->mode = PAPI_MODE_READ;
                ev_lt->variant = 0;
                ev_lt->subvariant = j;
                ev_lt->open_func = open_simple;
                ev_lt->close_func = close_simple;
                ev_lt->start_func = start_simple;
                ev_lt->stop_func = stop_simple;
                ev_lt->access_func = access_amdsmi_link_type;
                htable_insert(htable, ev_lt->name, ev_lt);
                idx++;
            }
            bool p2p_flag;
            if (amdsmi_is_P2P_accessible_p &&
                amdsmi_is_P2P_accessible_p(device_handles[i], device_handles[j], &p2p_flag) == AMDSMI_STATUS_SUCCESS) {
                snprintf(name_buf, sizeof(name_buf), "p2p_accessible:device=%d:peer=%d", i, j);
                snprintf(descr_buf, sizeof(descr_buf), "P2P access between device %d and %d (1=Yes,0=No)", i, j);
                native_event_t *ev_p2p = &ntv_table.events[idx];
                ev_p2p->id = idx;
                ev_p2p->name = strdup(name_buf);
                ev_p2p->descr = strdup(descr_buf);
                ev_p2p->device = i;
                ev_p2p->value = 0;
                ev_p2p->mode = PAPI_MODE_READ;
                ev_p2p->variant = 0;
                ev_p2p->subvariant = j;
                ev_p2p->open_func = open_simple;
                ev_p2p->close_func = close_simple;
                ev_p2p->start_func = start_simple;
                ev_p2p->stop_func = stop_simple;
                ev_p2p->access_func = access_amdsmi_p2p_accessible;
                htable_insert(htable, ev_p2p->name, ev_p2p);
                idx++;
            }
            amdsmi_io_link_type_t _io_type; amdsmi_p2p_capability_t p2p_status;
            if (amdsmi_topo_get_p2p_status_p && amdsmi_topo_get_p2p_status_p(device_handles[i], device_handles[j], &_io_type, &p2p_status) == AMDSMI_STATUS_SUCCESS) {
                snprintf(name_buf, sizeof(name_buf), "p2p_status:device=%d:peer=%d", i, j);
                snprintf(descr_buf, sizeof(descr_buf), "P2P status between device %d and %d (code)", i, j);
                native_event_t *ev_p2ps = &ntv_table.events[idx];
                ev_p2ps->id = idx;
                ev_p2ps->name = strdup(name_buf);
                ev_p2ps->descr = strdup(descr_buf);
                ev_p2ps->device = i;
                ev_p2ps->value = 0;
                ev_p2ps->mode = PAPI_MODE_READ;
                ev_p2ps->variant = 0;
                ev_p2ps->subvariant = j;
                ev_p2ps->open_func = open_simple;
                ev_p2ps->close_func = close_simple;
                ev_p2ps->start_func = start_simple;
                ev_p2ps->stop_func = stop_simple;
                ev_p2ps->access_func = access_amdsmi_p2p_status;
                htable_insert(htable, ev_p2ps->name, ev_p2ps);
                idx++;
            }
        }
    }
    /* AMD SMI library version events (global) */
    if (amdsmi_get_lib_version_p && gpu_count > 0) {
        amdsmi_version_t ver;
        if (amdsmi_get_lib_version_p(&ver) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "amdsmi_lib_version_major");
            snprintf(descr_buf, sizeof(descr_buf), "AMD SMI library major version");
            native_event_t *ev_ver_maj = &ntv_table.events[idx];
            ev_ver_maj->id = idx;
            ev_ver_maj->name = strdup(name_buf);
            ev_ver_maj->descr = strdup(descr_buf);
            ev_ver_maj->device = 0;
            ev_ver_maj->value = 0;
            ev_ver_maj->mode = PAPI_MODE_READ;
            ev_ver_maj->variant = 0;
            ev_ver_maj->subvariant = 0;
            ev_ver_maj->open_func = open_simple;
            ev_ver_maj->close_func = close_simple;
            ev_ver_maj->start_func = start_simple;
            ev_ver_maj->stop_func = stop_simple;
            ev_ver_maj->access_func = access_amdsmi_lib_version;
            htable_insert(htable, ev_ver_maj->name, ev_ver_maj);
            idx++;
            snprintf(name_buf, sizeof(name_buf), "amdsmi_lib_version_minor");
            snprintf(descr_buf, sizeof(descr_buf), "AMD SMI library minor version");
            native_event_t *ev_ver_min = &ntv_table.events[idx];
            ev_ver_min->id = idx;
            ev_ver_min->name = strdup(name_buf);
            ev_ver_min->descr = strdup(descr_buf);
            ev_ver_min->device = 0;
            ev_ver_min->value = 0;
            ev_ver_min->mode = PAPI_MODE_READ;
            ev_ver_min->variant = 1;
            ev_ver_min->subvariant = 0;
            ev_ver_min->open_func = open_simple;
            ev_ver_min->close_func = close_simple;
            ev_ver_min->start_func = start_simple;
            ev_ver_min->stop_func = stop_simple;
            ev_ver_min->access_func = access_amdsmi_lib_version;
            htable_insert(htable, ev_ver_min->name, ev_ver_min);
            idx++;
            snprintf(name_buf, sizeof(name_buf), "amdsmi_lib_version_patch");
            snprintf(descr_buf, sizeof(descr_buf), "AMD SMI library patch version");
            native_event_t *ev_ver_pat = &ntv_table.events[idx];
            ev_ver_pat->id = idx;
            ev_ver_pat->name = strdup(name_buf);
            ev_ver_pat->descr = strdup(descr_buf);
            ev_ver_pat->device = 0;
            ev_ver_pat->value = 0;
            ev_ver_pat->mode = PAPI_MODE_READ;
            ev_ver_pat->variant = 2;
            ev_ver_pat->subvariant = 0;
            ev_ver_pat->open_func = open_simple;
            ev_ver_pat->close_func = close_simple;
            ev_ver_pat->start_func = start_simple;
            ev_ver_pat->stop_func = stop_simple;
            ev_ver_pat->access_func = access_amdsmi_lib_version;
            htable_insert(htable, ev_ver_pat->name, ev_ver_pat);
            idx++;
        }
    }
    
#ifndef AMDSMI_DISABLE_ESMI
    /* CPU metrics events */
    if (cpu_count > 0) {
        // CPU socket-level events
        for (int s = 0; s < cpu_count; ++s) {
            int dev = gpu_count + s;
            uint32_t pwr;
            if (amdsmi_get_cpu_socket_power_p(device_handles[dev], &pwr) == AMDSMI_STATUS_SUCCESS) {
                snprintf(name_buf, sizeof(name_buf), "power:socket=%d", s);
                snprintf(descr_buf, sizeof(descr_buf), "Socket %d power (W)", s);
                native_event_t *ev_pwr = &ntv_table.events[idx];
                ev_pwr->id = idx;
                ev_pwr->name = strdup(name_buf);
                ev_pwr->descr = strdup(descr_buf);
                ev_pwr->device = dev;
                ev_pwr->value = 0;
                ev_pwr->mode = PAPI_MODE_READ;
                ev_pwr->variant = 0;
                ev_pwr->subvariant = 0;
                ev_pwr->open_func = open_simple;
                ev_pwr->close_func = close_simple;
                ev_pwr->start_func = start_simple;
                ev_pwr->stop_func = stop_simple;
                ev_pwr->access_func = access_amdsmi_cpu_socket_power;
                htable_insert(htable, ev_pwr->name, ev_pwr);
                idx++;
            }
            uint64_t sock_energy;
            if (amdsmi_get_cpu_socket_energy_p(device_handles[dev], &sock_energy) == AMDSMI_STATUS_SUCCESS) {
                snprintf(name_buf, sizeof(name_buf), "energy:socket=%d", s);
                snprintf(descr_buf, sizeof(descr_buf), "Socket %d energy consumed (uJ)", s);
                native_event_t *ev_sock_energy = &ntv_table.events[idx];
                ev_sock_energy->id = idx;
                ev_sock_energy->name = strdup(name_buf);
                ev_sock_energy->descr = strdup(descr_buf);
                ev_sock_energy->device = dev;
                ev_sock_energy->value = 0;
                ev_sock_energy->mode = PAPI_MODE_READ;
                ev_sock_energy->variant = 0;
                ev_sock_energy->subvariant = 0;
                ev_sock_energy->open_func = open_simple;
                ev_sock_energy->close_func = close_simple;
                ev_sock_energy->start_func = start_simple;
                ev_sock_energy->stop_func = stop_simple;
                ev_sock_energy->access_func = access_amdsmi_cpu_socket_energy;
                htable_insert(htable, ev_sock_energy->name, ev_sock_energy);
                idx++;
            }
            uint16_t fmax, fmin;
            if (amdsmi_get_cpu_socket_freq_range_p(device_handles[dev], &fmax, &fmin) == AMDSMI_STATUS_SUCCESS) {
                snprintf(name_buf, sizeof(name_buf), "freq_max:socket=%d", s);
                snprintf(descr_buf, sizeof(descr_buf), "Socket %d maximum frequency (MHz)", s);
                native_event_t *ev_fmax = &ntv_table.events[idx];
                ev_fmax->id = idx;
                ev_fmax->name = strdup(name_buf);
                ev_fmax->descr = strdup(descr_buf);
                ev_fmax->device = dev;
                ev_fmax->value = 0;
                ev_fmax->mode = PAPI_MODE_READ;
                ev_fmax->variant = 1;
                ev_fmax->subvariant = 0;
                ev_fmax->open_func = open_simple;
                ev_fmax->close_func = close_simple;
                ev_fmax->start_func = start_simple;
                ev_fmax->stop_func = stop_simple;
                ev_fmax->access_func = access_amdsmi_cpu_socket_freq_range;
                htable_insert(htable, ev_fmax->name, ev_fmax);
                idx++;
                snprintf(name_buf, sizeof(name_buf), "freq_min:socket=%d", s);
                snprintf(descr_buf, sizeof(descr_buf), "Socket %d minimum frequency (MHz)", s);
                native_event_t *ev_fmin = &ntv_table.events[idx];
                ev_fmin->id = idx;
                ev_fmin->name = strdup(name_buf);
                ev_fmin->descr = strdup(descr_buf);
                ev_fmin->device = dev;
                ev_fmin->value = 0;
                ev_fmin->mode = PAPI_MODE_READ;
                ev_fmin->variant = 0;
                ev_fmin->subvariant = 0;
                ev_fmin->open_func = open_simple;
                ev_fmin->close_func = close_simple;
                ev_fmin->start_func = start_simple;
                ev_fmin->stop_func = stop_simple;
                ev_fmin->access_func = access_amdsmi_cpu_socket_freq_range;
                htable_insert(htable, ev_fmin->name, ev_fmin);
                idx++;
            }
            uint32_t cap;
            amdsmi_status_t st_cap = amdsmi_get_cpu_socket_power_cap_p(device_handles[dev], &cap);
            uint32_t cap_max;
            amdsmi_status_t st_capmax = amdsmi_get_cpu_socket_power_cap_max_p(device_handles[dev], &cap_max);
            if (st_cap == AMDSMI_STATUS_SUCCESS || st_capmax == AMDSMI_STATUS_SUCCESS) {
                if (st_cap == AMDSMI_STATUS_SUCCESS) {
                    snprintf(name_buf, sizeof(name_buf), "power_cap:socket=%d", s);
                    snprintf(descr_buf, sizeof(descr_buf), "Socket %d current power cap (W)", s);
                    native_event_t *ev_cap = &ntv_table.events[idx];
                    ev_cap->id = idx;
                    ev_cap->name = strdup(name_buf);
                    ev_cap->descr = strdup(descr_buf);
                    ev_cap->device = dev;
                    ev_cap->value = 0;
                    ev_cap->mode = PAPI_MODE_READ;
                    ev_cap->variant = 0;
                    ev_cap->subvariant = 0;
                    ev_cap->open_func = open_simple;
                    ev_cap->close_func = close_simple;
                    ev_cap->start_func = start_simple;
                    ev_cap->stop_func = stop_simple;
                    ev_cap->access_func = access_amdsmi_cpu_power_cap;
                    htable_insert(htable, ev_cap->name, ev_cap);
                    idx++;
                }
                if (st_capmax == AMDSMI_STATUS_SUCCESS) {
                    snprintf(name_buf, sizeof(name_buf), "power_cap_max:socket=%d", s);
                    snprintf(descr_buf, sizeof(descr_buf), "Socket %d max power cap (W)", s);
                    native_event_t *ev_capmax = &ntv_table.events[idx];
                    ev_capmax->id = idx;
                    ev_capmax->name = strdup(name_buf);
                    ev_capmax->descr = strdup(descr_buf);
                    ev_capmax->device = dev;
                    ev_capmax->value = 0;
                    ev_capmax->mode = PAPI_MODE_READ;
                    ev_capmax->variant = 1;
                    ev_capmax->subvariant = 0;
                    ev_capmax->open_func = open_simple;
                    ev_capmax->close_func = close_simple;
                    ev_capmax->start_func = start_simple;
                    ev_capmax->stop_func = stop_simple;
                    ev_capmax->access_func = access_amdsmi_cpu_power_cap;
                    htable_insert(htable, ev_capmax->name, ev_capmax);
                    idx++;
                }
            }
            uint16_t freq;
            char *src_type = NULL;
            if (amdsmi_get_cpu_socket_current_active_freq_limit_p(device_handles[dev], &freq, &src_type) == AMDSMI_STATUS_SUCCESS) {
                if (src_type) free(src_type);
                snprintf(name_buf, sizeof(name_buf), "freq_limit:socket=%d", s);
                snprintf(descr_buf, sizeof(descr_buf), "Socket %d current frequency limit (MHz)", s);
                native_event_t *ev_flim = &ntv_table.events[idx];
                ev_flim->id = idx;
                ev_flim->name = strdup(name_buf);
                ev_flim->descr = strdup(descr_buf);
                ev_flim->device = dev;
                ev_flim->value = 0;
                ev_flim->mode = PAPI_MODE_READ;
                ev_flim->variant = 0;
                ev_flim->subvariant = 0;
                ev_flim->open_func = open_simple;
                ev_flim->close_func = close_simple;
                ev_flim->start_func = start_simple;
                ev_flim->stop_func = stop_simple;
                ev_flim->access_func = access_amdsmi_cpu_socket_freq_limit;
                htable_insert(htable, ev_flim->name, ev_flim);
                idx++;
            }
            amdsmi_smu_fw_version_t fw;
            if (amdsmi_get_cpu_smu_fw_version_p(device_handles[dev], &fw) == AMDSMI_STATUS_SUCCESS) {
                snprintf(name_buf, sizeof(name_buf), "smu_fw_version:socket=%d", s);
                snprintf(descr_buf, sizeof(descr_buf), "Socket %d SMU firmware version (encoded)", s);
                native_event_t *ev_fw = &ntv_table.events[idx];
                ev_fw->id = idx;
                ev_fw->name = strdup(name_buf);
                ev_fw->descr = strdup(descr_buf);
                ev_fw->device = dev;
                ev_fw->value = 0;
                ev_fw->mode = PAPI_MODE_READ;
                ev_fw->variant = 0;
                ev_fw->subvariant = 0;
                ev_fw->open_func = open_simple;
                ev_fw->close_func = close_simple;
                ev_fw->start_func = start_simple;
                ev_fw->stop_func = stop_simple;
                ev_fw->access_func = access_amdsmi_smu_fw_version;
                htable_insert(htable, ev_fw->name, ev_fw);
                idx++;
            }
        }
        // CPU core-level events
        for (int s = 0; s < cpu_count; ++s) {
            int dev = gpu_count + s;
            for (uint32_t c = 0; c < cores_per_socket[s]; ++c) {
                uint64_t energy;
                if (amdsmi_get_cpu_core_energy_p(cpu_core_handles[s][c], &energy) == AMDSMI_STATUS_SUCCESS) {
                    snprintf(name_buf, sizeof(name_buf), "energy:socket=%d:core=%d", s, c);
                    snprintf(descr_buf, sizeof(descr_buf), "Socket %d Core %d energy (uJ)", s, c);
                    native_event_t *ev_core_energy = &ntv_table.events[idx];
                    ev_core_energy->id = idx;
                    ev_core_energy->name = strdup(name_buf);
                    ev_core_energy->descr = strdup(descr_buf);
                    ev_core_energy->device = dev;
                    ev_core_energy->value = 0;
                    ev_core_energy->mode = PAPI_MODE_READ;
                    ev_core_energy->variant = 0;
                    ev_core_energy->subvariant = c;
                    ev_core_energy->open_func = open_simple;
                    ev_core_energy->close_func = close_simple;
                    ev_core_energy->start_func = start_simple;
                    ev_core_energy->stop_func = stop_simple;
                    ev_core_energy->access_func = access_amdsmi_cpu_core_energy;
                    htable_insert(htable, ev_core_energy->name, ev_core_energy);
                    idx++;
                }
                uint32_t freq;
                if (amdsmi_get_cpu_core_current_freq_limit_p(cpu_core_handles[s][c], &freq) == AMDSMI_STATUS_SUCCESS) {
                    snprintf(name_buf, sizeof(name_buf), "freq_limit:socket=%d:core=%d", s, c);
                    snprintf(descr_buf, sizeof(descr_buf), "Socket %d Core %d frequency limit (MHz)", s, c);
                    native_event_t *ev_core_flim = &ntv_table.events[idx];
                    ev_core_flim->id = idx;
                    ev_core_flim->name = strdup(name_buf);
                    ev_core_flim->descr = strdup(descr_buf);
                    ev_core_flim->device = dev;
                    ev_core_flim->value = 0;
                    ev_core_flim->mode = PAPI_MODE_READ;
                    ev_core_flim->variant = 0;
                    ev_core_flim->subvariant = c;
                    ev_core_flim->open_func = open_simple;
                    ev_core_flim->close_func = close_simple;
                    ev_core_flim->start_func = start_simple;
                    ev_core_flim->stop_func = stop_simple;
                    ev_core_flim->access_func = access_amdsmi_cpu_core_freq_limit;
                    htable_insert(htable, ev_core_flim->name, ev_core_flim);
                    idx++;
                }
                uint32_t boost;
                if (amdsmi_get_cpu_core_boostlimit_p(cpu_core_handles[s][c], &boost) == AMDSMI_STATUS_SUCCESS) {
                    snprintf(name_buf, sizeof(name_buf), "boostlimit:socket=%d:core=%d", s, c);
                    snprintf(descr_buf, sizeof(descr_buf), "Socket %d Core %d boost limit (MHz)", s, c);
                    native_event_t *ev_boost = &ntv_table.events[idx];
                    ev_boost->id = idx;
                    ev_boost->name = strdup(name_buf);
                    ev_boost->descr = strdup(descr_buf);
                    ev_boost->device = dev;
                    ev_boost->value = 0;
                    ev_boost->mode = PAPI_MODE_READ;
                    ev_boost->variant = 0;
                    ev_boost->subvariant = c;
                    ev_boost->open_func = open_simple;
                    ev_boost->close_func = close_simple;
                    ev_boost->start_func = start_simple;
                    ev_boost->stop_func = stop_simple;
                    ev_boost->access_func = access_amdsmi_cpu_core_boostlimit;
                    htable_insert(htable, ev_boost->name, ev_boost);
                    idx++;
                }
            }
        }
        // CPU DIMM events
        for (int s = 0; s < cpu_count; ++s) {
            int dev = gpu_count + s;
            for (uint8_t dimm = 0; dimm < 16; ++dimm) {
                amdsmi_dimm_thermal_t dimm_temp;
                amdsmi_dimm_power_t dimm_pow;
                amdsmi_temp_range_refresh_rate_t range_info;
                amdsmi_status_t st_temp = amdsmi_get_cpu_dimm_thermal_sensor_p(device_handles[dev], dimm, &dimm_temp);
                amdsmi_status_t st_power = amdsmi_get_cpu_dimm_power_consumption_p(device_handles[dev], dimm, &dimm_pow);
                amdsmi_status_t st_range = amdsmi_get_cpu_dimm_temp_range_and_refresh_rate_p(device_handles[dev], dimm, &range_info);
                if (st_temp != AMDSMI_STATUS_SUCCESS && st_power != AMDSMI_STATUS_SUCCESS && st_range != AMDSMI_STATUS_SUCCESS) {
                    continue;
                }
                if (st_temp == AMDSMI_STATUS_SUCCESS) {
                    snprintf(name_buf, sizeof(name_buf), "dimm_temp:socket=%d:dimm=%d", s, dimm);
                    snprintf(descr_buf, sizeof(descr_buf), "Socket %d DIMM %d temperature (C)", s, dimm);
                    native_event_t *ev_dimm_temp = &ntv_table.events[idx];
                    ev_dimm_temp->id = idx;
                    ev_dimm_temp->name = strdup(name_buf);
                    ev_dimm_temp->descr = strdup(descr_buf);
                    ev_dimm_temp->device = dev;
                    ev_dimm_temp->value = 0;
                    ev_dimm_temp->mode = PAPI_MODE_READ;
                    ev_dimm_temp->variant = 0;
                    ev_dimm_temp->subvariant = dimm;
                    ev_dimm_temp->open_func = open_simple;
                    ev_dimm_temp->close_func = close_simple;
                    ev_dimm_temp->start_func = start_simple;
                    ev_dimm_temp->stop_func = stop_simple;
                    ev_dimm_temp->access_func = access_amdsmi_dimm_temp;
                    htable_insert(htable, ev_dimm_temp->name, ev_dimm_temp);
                    idx++;
                }
                if (st_power == AMDSMI_STATUS_SUCCESS) {
                    snprintf(name_buf, sizeof(name_buf), "dimm_power:socket=%d:dimm=%d", s, dimm);
                    snprintf(descr_buf, sizeof(descr_buf), "Socket %d DIMM %d power (mW)", s, dimm);
                    native_event_t *ev_dimm_pow = &ntv_table.events[idx];
                    ev_dimm_pow->id = idx;
                    ev_dimm_pow->name = strdup(name_buf);
                    ev_dimm_pow->descr = strdup(descr_buf);
                    ev_dimm_pow->device = dev;
                    ev_dimm_pow->value = 0;
                    ev_dimm_pow->mode = PAPI_MODE_READ;
                    ev_dimm_pow->variant = 0;
                    ev_dimm_pow->subvariant = dimm;
                    ev_dimm_pow->open_func = open_simple;
                    ev_dimm_pow->close_func = close_simple;
                    ev_dimm_pow->start_func = start_simple;
                    ev_dimm_pow->stop_func = stop_simple;
                    ev_dimm_pow->access_func = access_amdsmi_dimm_power;
                    htable_insert(htable, ev_dimm_pow->name, ev_dimm_pow);
                    idx++;
                }
                if (st_range == AMDSMI_STATUS_SUCCESS) {
                    snprintf(name_buf, sizeof(name_buf), "dimm_temp_range:socket=%d:dimm=%d", s, dimm);
                    snprintf(descr_buf, sizeof(descr_buf), "Socket %d DIMM %d temperature range", s, dimm);
                    native_event_t *ev_range = &ntv_table.events[idx];
                    ev_range->id = idx;
                    ev_range->name = strdup(name_buf);
                    ev_range->descr = strdup(descr_buf);
                    ev_range->device = dev;
                    ev_range->value = 0;
                    ev_range->mode = PAPI_MODE_READ;
                    ev_range->variant = 0;
                    ev_range->subvariant = dimm;
                    ev_range->open_func = open_simple;
                    ev_range->close_func = close_simple;
                    ev_range->start_func = start_simple;
                    ev_range->stop_func = stop_simple;
                    ev_range->access_func = access_amdsmi_dimm_range_refresh;
                    htable_insert(htable, ev_range->name, ev_range);
                    idx++;
                    snprintf(name_buf, sizeof(name_buf), "dimm_refresh_rate:socket=%d:dimm=%d", s, dimm);
                    snprintf(descr_buf, sizeof(descr_buf), "Socket %d DIMM %d refresh rate mode", s, dimm);
                    native_event_t *ev_ref = &ntv_table.events[idx];
                    ev_ref->id = idx;
                    ev_ref->name = strdup(name_buf);
                    ev_ref->descr = strdup(descr_buf);
                    ev_ref->device = dev;
                    ev_ref->value = 0;
                    ev_ref->mode = PAPI_MODE_READ;
                    ev_ref->variant = 1;
                    ev_ref->subvariant = dimm;
                    ev_ref->open_func = open_simple;
                    ev_ref->close_func = close_simple;
                    ev_ref->start_func = start_simple;
                    ev_ref->stop_func = stop_simple;
                    ev_ref->access_func = access_amdsmi_dimm_range_refresh;
                    htable_insert(htable, ev_ref->name, ev_ref);
                    idx++;
                }
            }
        }
        // System-wide CPU events
        uint32_t threads;
        if (amdsmi_get_threads_per_core_p(&threads) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "threads_per_core");
            snprintf(descr_buf, sizeof(descr_buf), "SMT threads per core");
            native_event_t *ev_threads = &ntv_table.events[idx];
            ev_threads->id = idx;
            ev_threads->name = strdup(name_buf);
            ev_threads->descr = strdup(descr_buf);
            ev_threads->device = -1;
            ev_threads->value = 0;
            ev_threads->mode = PAPI_MODE_READ;
            ev_threads->variant = 0;
            ev_threads->subvariant = 0;
            ev_threads->open_func = open_simple;
            ev_threads->close_func = close_simple;
            ev_threads->start_func = start_simple;
            ev_threads->stop_func = stop_simple;
            ev_threads->access_func = access_amdsmi_threads_per_core;
            htable_insert(htable, ev_threads->name, ev_threads);
            idx++;
        }
        uint32_t family;
        if (amdsmi_get_cpu_family_p(&family) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "cpu_family");
            snprintf(descr_buf, sizeof(descr_buf), "CPU family ID");
            native_event_t *ev_family = &ntv_table.events[idx];
            ev_family->id = idx;
            ev_family->name = strdup(name_buf);
            ev_family->descr = strdup(descr_buf);
            ev_family->device = -1;
            ev_family->value = 0;
            ev_family->mode = PAPI_MODE_READ;
            ev_family->variant = 0;
            ev_family->subvariant = 0;
            ev_family->open_func = open_simple;
            ev_family->close_func = close_simple;
            ev_family->start_func = start_simple;
            ev_family->stop_func = stop_simple;
            ev_family->access_func = access_amdsmi_cpu_family;
            htable_insert(htable, ev_family->name, ev_family);
            idx++;
        }
        uint32_t model;
        if (amdsmi_get_cpu_model_p(&model) == AMDSMI_STATUS_SUCCESS) {
            snprintf(name_buf, sizeof(name_buf), "cpu_model");
            snprintf(descr_buf, sizeof(descr_buf), "CPU model ID");
            native_event_t *ev_model = &ntv_table.events[idx];
            ev_model->id = idx;
            ev_model->name = strdup(name_buf);
            ev_model->descr = strdup(descr_buf);
            ev_model->device = -1;
            ev_model->value = 0;
            ev_model->mode = PAPI_MODE_READ;
            ev_model->variant = 0;
            ev_model->subvariant = 0;
            ev_model->open_func = open_simple;
            ev_model->close_func = close_simple;
            ev_model->start_func = start_simple;
            ev_model->stop_func = stop_simple;
            ev_model->access_func = access_amdsmi_cpu_model;
            htable_insert(htable, ev_model->name, ev_model);
            idx++;
        }
    }
#endif
    // Cleanup - no device capabilities cache to free
    ntv_table.count = idx;
    return PAPI_OK;
}
static int shutdown_event_table(void) {
    // Remove all events from hash table and free their names/descr
    for (int i = 0; i < ntv_table.count; ++i) {
        htable_delete(htable, ntv_table.events[i].name);
        papi_free(ntv_table.events[i].name);
        papi_free(ntv_table.events[i].descr);
    }
    papi_free(ntv_table.events);
    ntv_table.events = NULL;
    ntv_table.count = 0;
    return PAPI_OK;
}
static int init_device_table(void) {
    // Nothing to do (device_handles and device_count already set in amds_init)
    return PAPI_OK;
}
static int shutdown_device_table(void) {
    if (device_handles) {
        papi_free(device_handles);
        device_handles = NULL;
    }
    if (cpu_core_handles) {
        for (int s = 0; s < cpu_count; ++s) {
            if (cpu_core_handles[s]) papi_free(cpu_core_handles[s]);
        }
        papi_free(cpu_core_handles);
        cpu_core_handles = NULL;
    }
    if (cores_per_socket) {
        papi_free(cores_per_socket);
        cores_per_socket = NULL;
    }
    device_count = 0;
    gpu_count = 0;
    cpu_count = 0;
    return PAPI_OK;
}
/* Access function implementations (read/write operations for each event) */
static int access_amdsmi_temp_metric(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;  /* ensure device handle is valid */
    }
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    amdsmi_status_t status = 
    amdsmi_get_temp_metric_p(device_handles[event->device],
                        (amdsmi_temperature_type_t) event->subvariant,
                        (amdsmi_temperature_metric_t) event->variant,
                        (int64_t *)&event->value);
    return (status == AMDSMI_STATUS_SUCCESS ? PAPI_OK : PAPI_EMISC);
}
static int access_amdsmi_fan_rpms(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    int64_t speed = 0;
    amdsmi_status_t status = 
    amdsmi_get_gpu_fan_rpms_p(device_handles[event->device],
                                                      event->subvariant, &speed);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    event->value = speed;
    return PAPI_OK;
}
static int access_amdsmi_fan_speed(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;  // writing fan speed not supported
    }
    int64_t val = 0;
    amdsmi_status_t status = 
    amdsmi_get_gpu_fan_speed_p(device_handles[event->device],
                                                         event->subvariant, &val);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    event->value = val;
    return PAPI_OK;
}
static int access_amdsmi_mem_total(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) return PAPI_ENOSUPP;
    uint64_t data = 0;
    amdsmi_status_t status = 
    amdsmi_get_total_memory_p(device_handles[event->device],
                                                         (amdsmi_memory_type_t) event->variant, &data);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    event->value = (int64_t) data;
    return PAPI_OK;
}
static int access_amdsmi_mem_usage(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) return PAPI_ENOSUPP;
    uint64_t data = 0;
    amdsmi_status_t status = 
    amdsmi_get_memory_usage_p(device_handles[event->device],
                                                         (amdsmi_memory_type_t) event->variant, &data);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    event->value = (int64_t) data;
    return PAPI_OK;
}
static int access_amdsmi_power_cap(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;
    }
    if (mode == PAPI_MODE_READ) {
        // Read current power cap
        amdsmi_power_cap_info_t info;
        amdsmi_status_t status = 
        amdsmi_get_power_cap_info_p(device_handles[event->device], 0, &info);  // sensor index 0
        if (status != AMDSMI_STATUS_SUCCESS) {
            return PAPI_EMISC;
        }
        event->value = (int64_t) info.power_cap;
        return PAPI_OK;
    } else if (mode == PAPI_MODE_WRITE) {
        // Set new power cap (value expected in microWatts if API uses uW)
        uint64_t new_cap = (uint64_t) event->value;
        amdsmi_status_t status = 
        amdsmi_set_power_cap_p(device_handles[event->device], 0, new_cap);
        return (status == AMDSMI_STATUS_SUCCESS ? PAPI_OK : PAPI_EMISC);
    }
    return PAPI_ENOSUPP;
}
static int access_amdsmi_power_cap_range(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) return PAPI_ENOSUPP;
    amdsmi_power_cap_info_t info;
    amdsmi_status_t status = 
    amdsmi_get_power_cap_info_p(device_handles[event->device], 0, &info);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    if (event->variant == 1) {
        event->value = (int64_t) info.min_power_cap;
    } else if (event->variant == 2) {
        event->value = (int64_t) info.max_power_cap;
    } else {
        return PAPI_EMISC;
    }
    return PAPI_OK;
}
static int access_amdsmi_power_average(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) return PAPI_ENOSUPP;
    amdsmi_power_info_t power;
    amdsmi_status_t status = 
    amdsmi_get_power_info_p(device_handles[event->device], &power);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    event->value = (int64_t) power.average_socket_power;
    return PAPI_OK;
}
static int access_amdsmi_pci_throughput(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) return PAPI_ENOSUPP;
    uint64_t sent = 0, received = 0, max_pkt = 0;
    amdsmi_status_t status = 
    amdsmi_get_gpu_pci_throughput_p(device_handles[event->device],
                                                             &sent, &received, &max_pkt);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    switch (event->variant) {
        case 0: event->value = (int64_t) sent; break;
        case 1: event->value = (int64_t) received; break;
        case 2: event->value = (int64_t) max_pkt; break;
        default: return PAPI_EMISC;
    }
    return PAPI_OK;
}
static int access_amdsmi_pci_replay_counter(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) return PAPI_ENOSUPP;
    uint64_t counter = 0;
    amdsmi_status_t status = 
    amdsmi_get_gpu_pci_replay_counter_p(device_handles[event->device], &counter);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    event->value = (int64_t) counter;
    return PAPI_OK;
}
static int access_amdsmi_clk_freq(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) return PAPI_ENOSUPP;
    amdsmi_frequencies_t freq_info;
    amdsmi_status_t status = 
    amdsmi_get_clk_freq_p(device_handles[event->device], AMDSMI_CLK_TYPE_SYS, &freq_info);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    // Simplified: variant 0 -> count, 1 -> current frequency, >=2 -> specific index
    if (event->subvariant == 0) {
        event->value = freq_info.num_supported;
    } else if (event->subvariant == 1) {
        if (freq_info.num_supported > 0) {
            event->value = freq_info.frequency[0];  // assume first is current
        } else {
            event->value = 0;
        }
    } else {
        int idx = event->subvariant - 2;
        if (idx >= 0 && idx < (int)freq_info.num_supported) {
            event->value = freq_info.frequency[idx];
        } else {
            return PAPI_EMISC;
        }
    }
    return PAPI_OK;
}
static int access_amdsmi_gpu_metrics(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) return PAPI_ENOSUPP;
    amdsmi_gpu_metrics_t metrics;
    amdsmi_status_t status = 
    amdsmi_get_gpu_metrics_info_p(device_handles[event->device], &metrics);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    // Parsing of metrics is not fully implemented; just return OK.
    // (In a full implementation, event->variant or subvariant would select a specific field of 'metrics'.)
    return PAPI_OK;
}
static int access_amdsmi_gpu_info(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    amdsmi_status_t status;
    switch (event->variant) {
        case 0: {
            uint16_t id = 0;
            status = amdsmi_get_gpu_id_p(device_handles[event->device], &id);
            if (status == AMDSMI_STATUS_SUCCESS) {
                event->value = id;
            }
            break;
        }
        case 1: {
            uint16_t rev = 0;
            status = amdsmi_get_gpu_revision_p(device_handles[event->device], &rev);
            if (status == AMDSMI_STATUS_SUCCESS) {
                event->value = rev;
            }
            break;
        }
        case 2: {
            uint16_t subid = 0;
            status = amdsmi_get_gpu_subsystem_id_p(device_handles[event->device], &subid);
            if (status == AMDSMI_STATUS_SUCCESS) {
                event->value = subid;
            }
            break;
        }
        case 3: {
            uint64_t bdfid = 0;
            status = amdsmi_get_gpu_bdf_id_p(device_handles[event->device], &bdfid);
            if (status == AMDSMI_STATUS_SUCCESS) {
                event->value = (int64_t) bdfid;
            }
            break;
        }
        /*case 4: {
            amdsmi_virtualization_mode_t mode_val;
            status = amdsmi_get_gpu_virtualization_mode_p(device_handles[event->device], &mode_val);
            if (status == AMDSMI_STATUS_SUCCESS) {
                event->value = mode_val;
            }
            break;
        }*/
        case 5: {
            int32_t numa_node = -1;
            status = amdsmi_get_gpu_topo_numa_affinity_p(device_handles[event->device], &numa_node);
            if (status == AMDSMI_STATUS_SUCCESS) {
                event->value = numa_node;
            }
            break;
        }
        default:
            return PAPI_EMISC;
    }
    return (status == AMDSMI_STATUS_SUCCESS ? PAPI_OK : PAPI_EMISC);
}
static int access_amdsmi_gpu_activity(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    amdsmi_engine_usage_t usage;
    amdsmi_status_t status = 
    amdsmi_get_gpu_activity_p(device_handles[event->device], &usage);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    switch (event->variant) {
        case 0: event->value = usage.gfx_activity; break;
        case 1: event->value = usage.umc_activity; break;
        case 2: event->value = usage.mm_activity; break;
        default: return PAPI_EMISC;
    }
    return PAPI_OK;
}
static int access_amdsmi_fan_speed_max(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    int64_t max_speed = 0;
    amdsmi_status_t status = 
    amdsmi_get_gpu_fan_speed_max_p(device_handles[event->device], event->subvariant, &max_speed);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    event->value = max_speed;
    return PAPI_OK;
}
static int access_amdsmi_pci_bandwidth(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    amdsmi_pcie_bandwidth_t bw;
    amdsmi_status_t status = 
    amdsmi_get_gpu_pci_bandwidth_p(device_handles[event->device], &bw);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    uint32_t cur_index = bw.transfer_rate.current;
    if (cur_index >= bw.transfer_rate.num_supported) {
        return PAPI_EMISC;
    }
    switch (event->variant) {
        case 0:
            event->value = bw.transfer_rate.num_supported;
            break;
        case 1:
            event->value = (int64_t) bw.transfer_rate.frequency[cur_index];
            break;
        case 2:
            event->value = bw.lanes[cur_index];
            break;
        default:
            return PAPI_EMISC;
    }
    return PAPI_OK;
}
static int access_amdsmi_energy_count(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    uint64_t energy = 0;
    float resolution = 0.0;
    uint64_t timestamp = 0;
    amdsmi_status_t status = 
    amdsmi_get_energy_count_p(device_handles[event->device], &energy, &resolution, &timestamp);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    // Convert energy count to microJoules
    event->value = (int64_t) (energy * resolution);
    return PAPI_OK;
}
static int access_amdsmi_power_profile_status(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    amdsmi_power_profile_status_t status_info;
    amdsmi_status_t status = 
    amdsmi_get_gpu_power_profile_presets_p(device_handles[event->device], 0, &status_info);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    if (event->variant == 0) {
        event->value = status_info.num_profiles;
    } else if (event->variant == 1) {
        event->value = (int64_t) status_info.current;
    } else {
        return PAPI_EMISC;
    }
    return PAPI_OK;
}

/* Access function for RAS block features enabled mask */
static int access_amdsmi_ras_block_enabled(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device])
        return PAPI_EMISC;
    if (mode != PAPI_MODE_READ)
        return PAPI_ENOSUPP;
    uint64_t mask = 0;
    amdsmi_status_t status = amdsmi_get_gpu_ras_block_features_enabled_p(device_handles[event->device], &mask);
    if (status != AMDSMI_STATUS_SUCCESS)
        return PAPI_EMISC;
    event->value = (int64_t) mask;
    return PAPI_OK;
}

/* Access function for ECC error counts (correctable/uncorrectable) */
static int access_amdsmi_ecc_count(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device])
        return PAPI_EMISC;
    if (mode != PAPI_MODE_READ)
        return PAPI_ENOSUPP;
    amdsmi_error_count_t counts;
    amdsmi_status_t status = amdsmi_get_gpu_ecc_count_p(device_handles[event->device], &counts);
    if (status != AMDSMI_STATUS_SUCCESS)
        return PAPI_EMISC;
    if (event->variant == 0) {
        event->value = (int64_t) counts.correctable_count;
    } else {
        event->value = (int64_t) counts.uncorrectable_count;
    }
    return PAPI_OK;
}

/* Access function for total ECC error counts */
static int access_amdsmi_ecc_total(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device])
        return PAPI_EMISC;
    if (mode != PAPI_MODE_READ)
        return PAPI_ENOSUPP;
    amdsmi_error_count_t counts;
    amdsmi_status_t status = amdsmi_get_gpu_total_ecc_count_p(device_handles[event->device], &counts);
    if (status != AMDSMI_STATUS_SUCCESS)
        return PAPI_EMISC;
    if (event->variant == 0) {
        event->value = (int64_t) counts.correctable_count;
    } else {
        event->value = (int64_t) counts.uncorrectable_count;
    }
    return PAPI_OK;
}

/* Access function for ECC enabled flag */
static int access_amdsmi_ecc_enabled(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device])
        return PAPI_EMISC;
    if (mode != PAPI_MODE_READ)
        return PAPI_ENOSUPP;
    uint32_t enabled = 0;
    amdsmi_status_t status = amdsmi_get_gpu_ecc_enabled_p(device_handles[event->device], &enabled);
    if (status != AMDSMI_STATUS_SUCCESS)
        return PAPI_EMISC;
    event->value = (int64_t) enabled;
    return PAPI_OK;
}

/* Access function for XGMI error count */
static int access_amdsmi_xgmi_error(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device])
        return PAPI_EMISC;
    if (mode != PAPI_MODE_READ)
        return PAPI_ENOSUPP;
    uint64_t count = 0;
    amdsmi_status_t status = amdsmi_gpu_xgmi_error_status_p(device_handles[event->device], &count);
    if (status != AMDSMI_STATUS_SUCCESS)
        return PAPI_EMISC;
    event->value = (int64_t) count;
    return PAPI_OK;
}

/* Access function for voltage metrics (VDDGFX) */
static int access_amdsmi_volt_metric(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device])
        return PAPI_EMISC;
    if (mode != PAPI_MODE_READ)
        return PAPI_ENOSUPP;
    int64_t volt_val = 0;
    amdsmi_status_t status = amdsmi_get_gpu_volt_metric_p(device_handles[event->device],
                                                         (amdsmi_voltage_type_t) event->subvariant,
                                                         (amdsmi_voltage_metric_t) event->variant,
                                                         &volt_val);
    if (status != AMDSMI_STATUS_SUCCESS)
        return PAPI_EMISC;
    event->value = volt_val;
    return PAPI_OK;
}

/* Access function for performance level */
static int access_amdsmi_perf_level(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device])
        return PAPI_EMISC;
    if (mode != PAPI_MODE_READ)
        return PAPI_ENOSUPP;
    amdsmi_dev_perf_level_t level;
    amdsmi_status_t status = amdsmi_get_gpu_perf_level_p(device_handles[event->device], &level);
    if (status != AMDSMI_STATUS_SUCCESS)
        return PAPI_EMISC;
    event->value = (int64_t) level;
    return PAPI_OK;
}

/* Access function for GPU core overdrive level */
static int access_amdsmi_overdrive(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device])
        return PAPI_EMISC;
    if (mode != PAPI_MODE_READ)
        return PAPI_ENOSUPP;
    uint32_t od_level = 0;
    amdsmi_status_t status = amdsmi_get_gpu_overdrive_level_p(device_handles[event->device], &od_level);
    if (status != AMDSMI_STATUS_SUCCESS)
        return PAPI_EMISC;
    event->value = (int64_t) od_level;
    return PAPI_OK;
}

/* Access function for VRAM overdrive level */
static int access_amdsmi_mem_overdrive(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device])
        return PAPI_EMISC;
    if (mode != PAPI_MODE_READ)
        return PAPI_ENOSUPP;
    uint32_t mem_od = 0;
    amdsmi_status_t status = amdsmi_get_gpu_mem_overdrive_level_p(device_handles[event->device], &mem_od);
    if (status != AMDSMI_STATUS_SUCCESS)
        return PAPI_EMISC;
    event->value = (int64_t) mem_od;
    return PAPI_OK;
}

/* Access function for compute partition mode */
static int access_amdsmi_compute_partition(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device])
        return PAPI_EMISC;
    if (mode != PAPI_MODE_READ)
        return PAPI_ENOSUPP;
    uint32_t part = 0;
    amdsmi_status_t status = amdsmi_get_gpu_compute_partition_p(device_handles[event->device], &part);
    if (status != AMDSMI_STATUS_SUCCESS)
        return PAPI_EMISC;
    event->value = (int64_t) part;
    return PAPI_OK;
}

/* Access function for memory partition mode */
static int access_amdsmi_memory_partition(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device])
        return PAPI_EMISC;
    if (mode != PAPI_MODE_READ)
        return PAPI_ENOSUPP;
    uint32_t part = 0;
    amdsmi_status_t status = amdsmi_get_gpu_memory_partition_p(device_handles[event->device], &part);
    if (status != AMDSMI_STATUS_SUCCESS)
        return PAPI_EMISC;
    event->value = (int64_t) part;
    return PAPI_OK;
}

/* Access function for accelerator partition profile */
static int access_amdsmi_accel_profile(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device])
        return PAPI_EMISC;
    if (mode != PAPI_MODE_READ)
        return PAPI_ENOSUPP;
    uint32_t profile_id = 0;
    amdsmi_status_t status = amdsmi_get_gpu_accelerator_partition_profile_p(device_handles[event->device], (amdsmi_accelerator_partition_profile_t *)&profile_id);
    if (status != AMDSMI_STATUS_SUCCESS)
        return PAPI_EMISC;
    event->value = (int64_t) profile_id;
    return PAPI_OK;
}

/* Access function for GPU process isolation flag */
static int access_amdsmi_process_isolation(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device])
        return PAPI_EMISC;
    if (mode != PAPI_MODE_READ)
        return PAPI_ENOSUPP;
    uint32_t iso = 0;
    amdsmi_status_t status = amdsmi_get_gpu_process_isolation_p(device_handles[event->device], &iso);
    if (status != AMDSMI_STATUS_SUCCESS)
        return PAPI_EMISC;
    event->value = (int64_t) iso;
    return PAPI_OK;
}

/* Access function for VRAM info (type or vendor) */
static int access_amdsmi_vram_info(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device])
        return PAPI_EMISC;
    if (mode != PAPI_MODE_READ)
        return PAPI_ENOSUPP;
    amdsmi_vram_info_t info;
    amdsmi_status_t status = amdsmi_get_gpu_vram_info_p(device_handles[event->device], &info);
    if (status != AMDSMI_STATUS_SUCCESS)
        return PAPI_EMISC;
    // Note: We treat variant=0 as vendor ID, variant=1 as memory type (adjusted when creating events)
    if (event->variant == 0) {
        event->value = (int64_t) info.vram_vendor;   // vendor enum code
    } else {
        event->value = (int64_t) info.vram_type;     // memory type enum code
    }
    return PAPI_OK;
}

/* Access function for PCIe link info */
static int access_amdsmi_pcie_info(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device])
        return PAPI_EMISC;
    if (mode != PAPI_MODE_READ)
        return PAPI_ENOSUPP;
    amdsmi_pcie_info_t info;
    amdsmi_status_t status = amdsmi_get_pcie_info_p(device_handles[event->device], &info);
    if (status != AMDSMI_STATUS_SUCCESS)
        return PAPI_EMISC;
    // We assume info contains fields for current and max link generation and width
    switch (event->variant) {
        case 0:  // current link generation
            event->value = info.pcie_static.pcie_interface_version;
            break;
        case 1:  // current link width
            event->value = info.pcie_metric.pcie_width;
            break;
        case 2:  // max link generation
            event->value = info.pcie_static.max_pcie_interface_version;
            break;
        case 3:  // max link width
            event->value = info.pcie_static.max_pcie_width;
            break;
        default:
            return PAPI_EMISC;
    }
    return PAPI_OK;
}

/* Access function for P2P link weight between two GPUs */
static int access_amdsmi_link_weight(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->subvariant < 0 || event->device >= device_count || event->subvariant >= device_count ||
        !device_handles || !device_handles[event->device] || !device_handles[event->subvariant])
        return PAPI_EMISC;
    if (mode != PAPI_MODE_READ)
        return PAPI_ENOSUPP;
    uint64_t weight = 0;
    amdsmi_status_t status = amdsmi_topo_get_link_weight_p(device_handles[event->device], device_handles[event->subvariant], &weight);
    if (status != AMDSMI_STATUS_SUCCESS)
        return PAPI_EMISC;
    event->value = (int64_t) weight;
    return PAPI_OK;
}

/* Access function for P2P link type between two GPUs */
static int access_amdsmi_link_type(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->subvariant < 0 || event->device >= device_count || event->subvariant >= device_count ||
        !device_handles || !device_handles[event->device] || !device_handles[event->subvariant])
        return PAPI_EMISC;
    if (mode != PAPI_MODE_READ)
        return PAPI_ENOSUPP;
    amdsmi_io_link_type_t type; uint64_t hops = 0;
    amdsmi_status_t status = amdsmi_topo_get_link_type_p(device_handles[event->device], device_handles[event->subvariant], &hops, &type);
    if (status != AMDSMI_STATUS_SUCCESS)
        return PAPI_EMISC;
    event->value = (int64_t) type;
    return PAPI_OK;
}

/* Access function for P2P accessibility flag */
static int access_amdsmi_p2p_accessible(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->subvariant < 0 || event->device >= device_count || event->subvariant >= device_count ||
        !device_handles || !device_handles[event->device] || !device_handles[event->subvariant])
        return PAPI_EMISC;
    if (mode != PAPI_MODE_READ)
        return PAPI_ENOSUPP;
    bool accessible = false;
    amdsmi_status_t status = amdsmi_is_P2P_accessible_p(device_handles[event->device], device_handles[event->subvariant], &accessible);
    if (status != AMDSMI_STATUS_SUCCESS)
        return PAPI_EMISC;
    event->value = (int64_t)(accessible ? 1 : 0);
    return PAPI_OK;
}

/* Access function for detailed P2P status code */
static int access_amdsmi_p2p_status(int mode, void *arg)
{
    native_event_t *event = (native_event_t *)arg;
    if (mode != PAPI_MODE_READ)
        return PAPI_EINVAL;

    if (!amdsmi_topo_get_p2p_status_p || event->device < 0 || event->subvariant < 0)
        return PAPI_EMISC;

    amdsmi_io_link_type_t io_t;
    amdsmi_p2p_capability_t cap;
    amdsmi_status_t status = amdsmi_topo_get_p2p_status_p(
        device_handles[event->device],
        device_handles[event->subvariant],
        &io_t,
        &cap);

    if (status != AMDSMI_STATUS_SUCCESS)
        return PAPI_EMISC;

    uint64_t mask = 0;
    if (cap.is_iolink_coherent)          mask |= 1ull;
    if (cap.is_iolink_atomics_32bit)     mask |= 1ull << 1;
    if (cap.is_iolink_atomics_64bit)     mask |= 1ull << 2;
    if (cap.is_iolink_dma)               mask |= 1ull << 3;
    if (cap.is_iolink_bi_directional)    mask |= 1ull << 4;

    event->value = (int64_t)mask;
    return PAPI_OK;
}


static int access_amdsmi_power_management(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device])
        return PAPI_EMISC;
    if (mode != PAPI_MODE_READ)
        return PAPI_ENOSUPP;
    uint32_t enabled = 0;
    amdsmi_status_t status = amdsmi_is_gpu_power_management_enabled_p(device_handles[event->device], &enabled);
    if (status != AMDSMI_STATUS_SUCCESS)
        return PAPI_EMISC;
    event->value = (int64_t) enabled;
    return PAPI_OK;
}

static int access_amdsmi_lib_version(int mode, void *arg)
{
    native_event_t *event = (native_event_t *)arg;
    if (mode != PAPI_MODE_READ)
        return PAPI_EINVAL;

    if (!amdsmi_get_lib_version_p)
        return PAPI_EMISC;

    amdsmi_version_t ver;
    amdsmi_status_t status = amdsmi_get_lib_version_p(&ver);
    if (status != AMDSMI_STATUS_SUCCESS)
        return PAPI_EMISC;

    if (event->variant == 0) {
        event->value = ver.major;
    } else if (event->variant == 1) {
        event->value = ver.minor;
    } else if (event->variant == 2) {
        event->value = ver.release;
    } else {
        event->value = ver.year;
    }
    return PAPI_OK;
}



/* Access function for GPU process count (number of active processes on GPU) */
static int access_amdsmi_process_count(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device])
        return PAPI_EMISC;
    if (mode != PAPI_MODE_READ)
        return PAPI_ENOSUPP;
    amdsmi_proc_info_t proc_list[64];  // allocate space for up to 64 processes
    uint32_t max_count = 64;
    amdsmi_status_t status = amdsmi_get_gpu_process_list_p(device_handles[event->device], &max_count, proc_list);
    if (status != AMDSMI_STATUS_SUCCESS && status != AMDSMI_STATUS_INSUFFICIENT_SIZE)
        return PAPI_EMISC;
    // If INSUFFICIENT_SIZE, max_count holds required count (but we limited array); we'll still report max_count (capped at 64)
    event->value = (int64_t) max_count;
    return PAPI_OK;
}


#ifndef AMDSMI_DISABLE_ESMI
/* The functions below implement CPU metrics access */
static int access_amdsmi_cpu_socket_power(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    uint32_t power = 0;
    amdsmi_status_t status = amdsmi_get_cpu_socket_power_p(device_handles[event->device], &power);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    event->value = (int64_t) power;
    return PAPI_OK;
}
static int access_amdsmi_cpu_socket_energy(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    uint64_t energy = 0;
    amdsmi_status_t status = amdsmi_get_cpu_socket_energy_p(device_handles[event->device], &energy);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    event->value = (int64_t) energy;
    return PAPI_OK;
}
static int access_amdsmi_cpu_socket_freq_limit(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    uint16_t freq = 0;
    char *src = NULL;
    amdsmi_status_t status = amdsmi_get_cpu_socket_current_active_freq_limit_p(device_handles[event->device], &freq, &src);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    if (src) free(src);
    event->value = freq;
    return PAPI_OK;
}
static int access_amdsmi_cpu_socket_freq_range(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    uint16_t fmax = 0, fmin = 0;
    amdsmi_status_t status = amdsmi_get_cpu_socket_freq_range_p(device_handles[event->device], &fmax, &fmin);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    if (event->variant == 0) {
        event->value = fmin;
    } else {
        event->value = fmax;
    }
    return PAPI_OK;
}
static int access_amdsmi_cpu_power_cap(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles || !device_handles[event->device]) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    uint32_t cap_value = 0;
    amdsmi_status_t status;
    if (event->variant == 0) {
        status = amdsmi_get_cpu_socket_power_cap_p(device_handles[event->device], &cap_value);
    } else {
        status = amdsmi_get_cpu_socket_power_cap_max_p(device_handles[event->device], &cap_value);
    }
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    event->value = (int64_t) cap_value;
    return PAPI_OK;
}
static int access_amdsmi_cpu_core_energy(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    int s_index = event->device - gpu_count;
    if (s_index < 0 || s_index >= cpu_count) {
        return PAPI_EMISC;
    }
    uint64_t energy = 0;
    amdsmi_status_t status = amdsmi_get_cpu_core_energy_p(cpu_core_handles[s_index][event->subvariant], &energy);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    event->value = (int64_t) energy;
    return PAPI_OK;
}
static int access_amdsmi_cpu_core_freq_limit(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    int s_index = event->device - gpu_count;
    if (s_index < 0 || s_index >= cpu_count) {
        return PAPI_EMISC;
    }
    uint32_t freq = 0;
    amdsmi_status_t status = amdsmi_get_cpu_core_current_freq_limit_p(cpu_core_handles[s_index][event->subvariant], &freq);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    event->value = freq;
    return PAPI_OK;
}
static int access_amdsmi_cpu_core_boostlimit(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    int s_index = event->device - gpu_count;
    if (s_index < 0 || s_index >= cpu_count) {
        return PAPI_EMISC;
    }
    uint32_t boost = 0;
    amdsmi_status_t status = amdsmi_get_cpu_core_boostlimit_p(cpu_core_handles[s_index][event->subvariant], &boost);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    event->value = boost;
    return PAPI_OK;
}
static int access_amdsmi_dimm_temp(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    amdsmi_dimm_thermal_t dimm_temp;
    amdsmi_status_t status = amdsmi_get_cpu_dimm_thermal_sensor_p(device_handles[event->device], (uint8_t) event->subvariant, &dimm_temp);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    event->value = (int64_t) dimm_temp.temp;
    return PAPI_OK;
}
static int access_amdsmi_dimm_power(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    amdsmi_dimm_power_t dimm_pow;
    amdsmi_status_t status = amdsmi_get_cpu_dimm_power_consumption_p(device_handles[event->device], (uint8_t) event->subvariant, &dimm_pow);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    event->value = dimm_pow.power;
    return PAPI_OK;
}
static int access_amdsmi_dimm_range_refresh(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    amdsmi_temp_range_refresh_rate_t rate;
    amdsmi_status_t status = amdsmi_get_cpu_dimm_temp_range_and_refresh_rate_p(device_handles[event->device], (uint8_t) event->subvariant, &rate);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    if (event->variant == 0) {
        event->value = rate.range;
    } else {
        event->value = rate.ref_rate;
    }
    return PAPI_OK;
}
static int access_amdsmi_threads_per_core(int mode, void *arg) {
    (void) arg;
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    uint32_t threads = 0;
    amdsmi_status_t status = amdsmi_get_threads_per_core_p(&threads);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    ((native_event_t *) arg)->value = threads;
    return PAPI_OK;
}
static int access_amdsmi_cpu_family(int mode, void *arg) {
    (void) arg;
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    uint32_t family = 0;
    amdsmi_status_t status = amdsmi_get_cpu_family_p(&family);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    ((native_event_t *) arg)->value = family;
    return PAPI_OK;
}
static int access_amdsmi_cpu_model(int mode, void *arg) {
    (void) arg;
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    uint32_t model = 0;
    amdsmi_status_t status = amdsmi_get_cpu_model_p(&model);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    ((native_event_t *) arg)->value = model;
    return PAPI_OK;
}
static int access_amdsmi_smu_fw_version(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    amdsmi_smu_fw_version_t fw;
    amdsmi_status_t status = amdsmi_get_cpu_smu_fw_version_p(device_handles[event->device], &fw);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    int encoded = ((int) fw.major << 16) | ((int) fw.minor << 8) | fw.debug;
    event->value = encoded;
    return PAPI_OK;
}
static int access_amdsmi_xgmi_bandwidth(int mode, void *arg) {
    native_event_t *event = (native_event_t *) arg;
    if (event->device < 0 || event->device >= device_count || !device_handles) {
        return PAPI_EMISC;
    }
    if (mode != PAPI_MODE_READ) {
        return PAPI_ENOSUPP;
    }
    amdsmi_processor_handle src = device_handles[event->device];
    amdsmi_processor_handle dst = device_handles[gpu_count + event->subvariant];
    uint64_t min_bw = 0, max_bw = 0;
    amdsmi_status_t status = amdsmi_get_minmax_bandwidth_between_processors_p(src, dst, &min_bw, &max_bw);
    if (status != AMDSMI_STATUS_SUCCESS) {
        return PAPI_EMISC;
    }
    if (event->variant == 0) {
        event->value = (int64_t) min_bw;
    } else {
        event->value = (int64_t) max_bw;
    }
    return PAPI_OK;
}
#endif
