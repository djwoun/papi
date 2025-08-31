#include <string.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <amd_smi/amdsmi.h>
#include <inttypes.h>
#include "papi.h"
#include "papi_memory.h"
#include "amds.h"
#include "htable.h"
#define MAX_EVENTS_PER_DEVICE 1024
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
/* Additional read-only AMD SMI function pointers */
static amdsmi_status_t (*amdsmi_get_lib_version_p)(amdsmi_version_t *);
static amdsmi_status_t (*amdsmi_get_gpu_driver_info_p)(amdsmi_processor_handle, amdsmi_driver_info_t *);
static amdsmi_status_t (*amdsmi_get_gpu_asic_info_p)(amdsmi_processor_handle, amdsmi_asic_info_t *);
static amdsmi_status_t (*amdsmi_get_gpu_board_info_p)(amdsmi_processor_handle, amdsmi_board_info_t *);
static amdsmi_status_t (*amdsmi_get_fw_info_p)(amdsmi_processor_handle, amdsmi_fw_info_t *);
static amdsmi_status_t (*amdsmi_get_gpu_vbios_info_p)(amdsmi_processor_handle, amdsmi_vbios_info_t *);
static amdsmi_status_t (*amdsmi_get_gpu_device_uuid_p)(amdsmi_processor_handle, unsigned int *, char *);
#ifdef AMDSMI_HAVE_ENUMERATION_INFO
static amdsmi_status_t (*amdsmi_get_gpu_enumeration_info_p)(amdsmi_processor_handle, amdsmi_enumeration_info_t *);
#endif
static amdsmi_status_t (*amdsmi_get_gpu_vendor_name_p)(amdsmi_processor_handle, char *, size_t);
static amdsmi_status_t (*amdsmi_get_gpu_vram_vendor_p)(amdsmi_processor_handle, char *, uint32_t);
static amdsmi_status_t (*amdsmi_get_gpu_subsystem_name_p)(amdsmi_processor_handle, char *, size_t);
static amdsmi_status_t (*amdsmi_get_link_metrics_p)(amdsmi_processor_handle, amdsmi_link_metrics_t *);
static amdsmi_status_t (*amdsmi_get_gpu_process_list_p)(amdsmi_processor_handle, uint32_t *, amdsmi_proc_info_t *);
static amdsmi_status_t (*amdsmi_get_gpu_ecc_enabled_p)(amdsmi_processor_handle, uint64_t *);
static amdsmi_status_t (*amdsmi_get_gpu_total_ecc_count_p)(amdsmi_processor_handle, amdsmi_error_count_t *);
static amdsmi_status_t (*amdsmi_get_gpu_ecc_count_p)(amdsmi_processor_handle, amdsmi_gpu_block_t, amdsmi_error_count_t *);
static amdsmi_status_t (*amdsmi_get_gpu_ecc_status_p)(amdsmi_processor_handle, amdsmi_gpu_block_t, amdsmi_ras_err_state_t *);
static amdsmi_status_t (*amdsmi_get_gpu_compute_partition_p)(amdsmi_processor_handle, char *, uint32_t);
static amdsmi_status_t (*amdsmi_get_gpu_memory_partition_p)(amdsmi_processor_handle, char *, uint32_t);
static amdsmi_status_t (*amdsmi_get_gpu_accelerator_partition_profile_p)(amdsmi_processor_handle, amdsmi_accelerator_partition_profile_t *, uint32_t *);

static amdsmi_status_t (*amdsmi_get_gpu_id_p)(amdsmi_processor_handle, uint16_t *);
static amdsmi_status_t (*amdsmi_get_gpu_revision_p)(amdsmi_processor_handle, uint16_t *);
static amdsmi_status_t (*amdsmi_get_gpu_subsystem_id_p)(amdsmi_processor_handle, uint16_t *);
//static amdsmi_status_t (*amdsmi_get_gpu_virtualization_mode_p)(amdsmi_processor_handle, amdsmi_virtualization_mode_t *);
static amdsmi_status_t (*amdsmi_get_gpu_pci_bandwidth_p)(amdsmi_processor_handle, amdsmi_pcie_bandwidth_t *);
static amdsmi_status_t (*amdsmi_get_gpu_bdf_id_p)(amdsmi_processor_handle, uint64_t *);
static amdsmi_status_t (*amdsmi_get_gpu_topo_numa_affinity_p)(amdsmi_processor_handle, int32_t *);
static amdsmi_status_t (*amdsmi_get_energy_count_p)(amdsmi_processor_handle, uint64_t *, float *, uint64_t *);
static amdsmi_status_t (*amdsmi_get_gpu_power_profile_presets_p)(amdsmi_processor_handle, uint32_t, amdsmi_power_profile_status_t *);
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
static uint64_t _str_to_u64_hash(const char *s);
static int access_amdsmi_uuid_hash(int mode, void *arg);
static int access_amdsmi_gpu_string_hash(int mode, void *arg);
#ifdef AMDSMI_HAVE_ENUMERATION_INFO
static int access_amdsmi_enumeration_info(int mode, void *arg);
#endif
static int access_amdsmi_asic_info(int mode, void *arg);
static int access_amdsmi_link_metrics(int mode, void *arg);
static int access_amdsmi_process_count(int mode, void *arg);
static int access_amdsmi_ecc_total(int mode, void *arg);
static int access_amdsmi_ecc_enabled_mask(int mode, void *arg);
static int access_amdsmi_compute_partition_hash(int mode, void *arg);
static int access_amdsmi_memory_partition_hash(int mode, void *arg);
static int access_amdsmi_accelerator_num_partitions(int mode, void *arg);
static int access_amdsmi_lib_version(int mode, void *arg);


/* Prototypes for added GPU/query accessors (outside AMDSMI_DISABLE_ESMI guard) */
static uint64_t _str_to_u64_hash(const char *s);
static int access_amdsmi_uuid_hash(int mode, void *arg);
static int access_amdsmi_gpu_string_hash(int mode, void *arg);
#ifdef AMDSMI_HAVE_ENUMERATION_INFO
static int access_amdsmi_enumeration_info(int mode, void *arg);
#endif
static int access_amdsmi_asic_info(int mode, void *arg);
static int access_amdsmi_link_metrics(int mode, void *arg);
static int access_amdsmi_process_count(int mode, void *arg);
static int access_amdsmi_ecc_total(int mode, void *arg);
static int access_amdsmi_ecc_enabled_mask(int mode, void *arg);
static int access_amdsmi_compute_partition_hash(int mode, void *arg);
static int access_amdsmi_memory_partition_hash(int mode, void *arg);
static int access_amdsmi_accelerator_num_partitions(int mode, void *arg);
static int access_amdsmi_lib_version(int mode, void *arg);
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
    /* Additional read-only queries */
    amdsmi_get_lib_version_p = sym("amdsmi_get_lib_version", NULL);
    amdsmi_get_gpu_driver_info_p = sym("amdsmi_get_gpu_driver_info", NULL);
    amdsmi_get_gpu_asic_info_p = sym("amdsmi_get_gpu_asic_info", NULL);
    amdsmi_get_gpu_board_info_p = sym("amdsmi_get_gpu_board_info", NULL);
    amdsmi_get_fw_info_p = sym("amdsmi_get_fw_info", NULL);
    amdsmi_get_gpu_vbios_info_p = sym("amdsmi_get_gpu_vbios_info", NULL);
    amdsmi_get_gpu_device_uuid_p = sym("amdsmi_get_gpu_device_uuid", NULL);
#ifdef AMDSMI_HAVE_ENUMERATION_INFO
    amdsmi_get_gpu_enumeration_info_p = sym("amdsmi_get_gpu_enumeration_info", NULL);
#endif
    amdsmi_get_gpu_vendor_name_p = sym("amdsmi_get_gpu_vendor_name", NULL);
    amdsmi_get_gpu_vram_vendor_p = sym("amdsmi_get_gpu_vram_vendor", NULL);
    amdsmi_get_gpu_subsystem_name_p = sym("amdsmi_get_gpu_subsystem_name", NULL);
    amdsmi_get_link_metrics_p = sym("amdsmi_get_link_metrics", NULL);
    amdsmi_get_gpu_process_list_p = sym("amdsmi_get_gpu_process_list", NULL);
    amdsmi_get_gpu_ecc_enabled_p = sym("amdsmi_get_gpu_ecc_enabled", NULL);
    amdsmi_get_gpu_total_ecc_count_p = sym("amdsmi_get_gpu_total_ecc_count", NULL);
    amdsmi_get_gpu_ecc_count_p = sym("amdsmi_get_gpu_ecc_count", NULL);
    amdsmi_get_gpu_ecc_status_p = sym("amdsmi_get_gpu_ecc_status", NULL);
    amdsmi_get_gpu_compute_partition_p = sym("amdsmi_get_gpu_compute_partition", NULL);
    amdsmi_get_gpu_memory_partition_p = sym("amdsmi_get_gpu_memory_partition", NULL);
    amdsmi_get_gpu_accelerator_partition_profile_p = sym("amdsmi_get_gpu_accelerator_partition_profile", NULL);

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
    int papi_errno = PAPI_OK;
    for (int i = 0; i < ctx->num_events; ++i) {
        unsigned int id = ctx->events_id[i];
        papi_errno = ntv_table_p->events[id].access_func(PAPI_MODE_READ, &ntv_table_p->events[id]);
        if (papi_errno != PAPI_OK) {
            return papi_errno;
        }
        ctx->counters[i] = (long long) ntv_table_p->events[id].value;
    }
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
    ntv_table.events = (native_event_t *) papi_calloc(MAX_EVENTS_PER_DEVICE * device_count, sizeof(native_event_t));
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
                if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
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
            if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
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
            if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
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
            if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
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
            if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
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
            if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
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
    
    /* -------- Additional GPU discovery & version info (read-only) -------- */
    /* Library version (global) */
    if (amdsmi_get_lib_version_p) {
        amdsmi_version_t vinfo;
        if (amdsmi_get_lib_version_p(&vinfo) == AMDSMI_STATUS_SUCCESS) {
            if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
            snprintf(name_buf, sizeof(name_buf), "lib_version_major");
            snprintf(descr_buf, sizeof(descr_buf), "AMD SMI library major version");
            native_event_t *ev_lmaj = &ntv_table.events[idx];
            ev_lmaj->id=idx; ev_lmaj->name=strdup(name_buf); ev_lmaj->descr=strdup(descr_buf);
            ev_lmaj->device = -1; ev_lmaj->value=0; ev_lmaj->mode=PAPI_MODE_READ;
            ev_lmaj->variant = 0; ev_lmaj->subvariant = 0; ev_lmaj->open_func=open_simple;
            ev_lmaj->close_func=close_simple; ev_lmaj->start_func=start_simple; ev_lmaj->stop_func=stop_simple;
            ev_lmaj->access_func = access_amdsmi_lib_version; htable_insert(htable, ev_lmaj->name, ev_lmaj); idx++;
            if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
            snprintf(name_buf, sizeof(name_buf), "lib_version_minor");
            snprintf(descr_buf, sizeof(descr_buf), "AMD SMI library minor version");
            native_event_t *ev_lmin = &ntv_table.events[idx];
            ev_lmin->id=idx; ev_lmin->name=strdup(name_buf); ev_lmin->descr=strdup(descr_buf);
            ev_lmin->device = -1; ev_lmin->value=0; ev_lmin->mode=PAPI_MODE_READ;
            ev_lmin->variant = 1; ev_lmin->subvariant = 0; ev_lmin->open_func=open_simple;
            ev_lmin->close_func=close_simple; ev_lmin->start_func=start_simple; ev_lmin->stop_func=stop_simple;
            ev_lmin->access_func = access_amdsmi_lib_version; htable_insert(htable, ev_lmin->name, ev_lmin); idx++;
            if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
            snprintf(name_buf, sizeof(name_buf), "lib_version_release");
            snprintf(descr_buf, sizeof(descr_buf), "AMD SMI library release/patch version");
            native_event_t *ev_lrel = &ntv_table.events[idx];
            ev_lrel->id=idx; ev_lrel->name=strdup(name_buf); ev_lrel->descr=strdup(descr_buf);
            ev_lrel->device = -1; ev_lrel->value=0; ev_lrel->mode=PAPI_MODE_READ;
            ev_lrel->variant = 2; ev_lrel->subvariant = 0; ev_lrel->open_func=open_simple;
            ev_lrel->close_func=close_simple; ev_lrel->start_func=start_simple; ev_lrel->stop_func=stop_simple;
            ev_lrel->access_func = access_amdsmi_lib_version; htable_insert(htable, ev_lrel->name, ev_lrel); idx++;
        }
    }
    for (int d = 0; d < gpu_count; ++d) {
        if (!device_handles || !device_handles[d]) continue;
        /* Device UUID (hash) */
        if (amdsmi_get_gpu_device_uuid_p) {
            unsigned int uuid_len = 0;
            amdsmi_status_t st = amdsmi_get_gpu_device_uuid_p(device_handles[d], &uuid_len, NULL);
            /* Some builds require preflight to get length; we just attempt a fixed buffer */
            char uuid_buf[128];
            uuid_len = sizeof(uuid_buf);
            st = amdsmi_get_gpu_device_uuid_p(device_handles[d], &uuid_len, uuid_buf);
            if (st == AMDSMI_STATUS_SUCCESS) {
                if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
                snprintf(name_buf, sizeof(name_buf), "uuid_hash:device=%d", d);
                snprintf(descr_buf, sizeof(descr_buf), "Device %d UUID (djb2 64-bit hash)", d);
                native_event_t *ev_uuid = &ntv_table.events[idx];
                ev_uuid->id=idx; ev_uuid->name=strdup(name_buf); ev_uuid->descr=strdup(descr_buf);
                ev_uuid->device=d; ev_uuid->value=0; ev_uuid->mode=PAPI_MODE_READ;
                ev_uuid->variant=0; ev_uuid->subvariant=0; ev_uuid->open_func=open_simple; ev_uuid->close_func=close_simple;
                ev_uuid->start_func=start_simple; ev_uuid->stop_func=stop_simple; ev_uuid->access_func=access_amdsmi_uuid_hash;
                htable_insert(htable, ev_uuid->name, ev_uuid); idx++;
            }
        }
        /* Vendor / VRAM vendor / Subsystem name (hash) */
        if (amdsmi_get_gpu_vendor_name_p) {
            if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
            snprintf(name_buf, sizeof(name_buf), "vendor_name_hash:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d vendor name (hash)", d);
            native_event_t *ev_vn = &ntv_table.events[idx];
            ev_vn->id=idx; ev_vn->name=strdup(name_buf); ev_vn->descr=strdup(descr_buf);
            ev_vn->device=d; ev_vn->value=0; ev_vn->mode=PAPI_MODE_READ; ev_vn->variant=0; ev_vn->subvariant=0;
            ev_vn->open_func=open_simple; ev_vn->close_func=close_simple; ev_vn->start_func=start_simple; ev_vn->stop_func=stop_simple;
            ev_vn->access_func=access_amdsmi_gpu_string_hash; htable_insert(htable, ev_vn->name, ev_vn); idx++;
        }
        if (amdsmi_get_gpu_vram_vendor_p) {
            if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
            snprintf(name_buf, sizeof(name_buf), "vram_vendor_hash:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d VRAM vendor (hash)", d);
            native_event_t *ev_vrv = &ntv_table.events[idx];
            ev_vrv->id=idx; ev_vrv->name=strdup(name_buf); ev_vrv->descr=strdup(descr_buf);
            ev_vrv->device=d; ev_vrv->value=0; ev_vrv->mode=PAPI_MODE_READ; ev_vrv->variant=1; ev_vrv->subvariant=0;
            ev_vrv->open_func=open_simple; ev_vrv->close_func=close_simple; ev_vrv->start_func=start_simple; ev_vrv->stop_func=stop_simple;
            ev_vrv->access_func=access_amdsmi_gpu_string_hash; htable_insert(htable, ev_vrv->name, ev_vrv); idx++;
        }
        if (amdsmi_get_gpu_subsystem_name_p) {
            if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
            snprintf(name_buf, sizeof(name_buf), "subsystem_name_hash:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d subsystem name (hash)", d);
            native_event_t *ev_ssn = &ntv_table.events[idx];
            ev_ssn->id=idx; ev_ssn->name=strdup(name_buf); ev_ssn->descr=strdup(descr_buf);
            ev_ssn->device=d; ev_ssn->value=0; ev_ssn->mode=PAPI_MODE_READ; ev_ssn->variant=2; ev_ssn->subvariant=0;
            ev_ssn->open_func=open_simple; ev_ssn->close_func=close_simple; ev_ssn->start_func=start_simple; ev_ssn->stop_func=stop_simple;
            ev_ssn->access_func=access_amdsmi_gpu_string_hash; htable_insert(htable, ev_ssn->name, ev_ssn); idx++;
        }
#ifdef AMDSMI_HAVE_ENUMERATION_INFO
        /* Enumeration info (drm render/card, hsa/hip ids) */
        if (amdsmi_get_gpu_enumeration_info_p) {
            amdsmi_enumeration_info_t einfo;
            if (amdsmi_get_gpu_enumeration_info_p(device_handles[d], &einfo) == AMDSMI_STATUS_SUCCESS) {
                if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
                snprintf(name_buf, sizeof(name_buf), "enum_drm_render:device=%d", d);
                snprintf(descr_buf, sizeof(descr_buf), "Device %d DRM render node", d);
                native_event_t *ev_er = &ntv_table.events[idx];
                ev_er->id=idx; ev_er->name=strdup(name_buf); ev_er->descr=strdup(descr_buf);
                ev_er->device=d; ev_er->value=0; ev_er->mode=PAPI_MODE_READ; ev_er->variant=0; ev_er->subvariant=0;
                ev_er->open_func=open_simple; ev_er->close_func=close_simple; ev_er->start_func=start_simple; ev_er->stop_func=stop_simple;
                ev_er->access_func=access_amdsmi_enumeration_info; htable_insert(htable, ev_er->name, ev_er); idx++;
                if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
                snprintf(name_buf, sizeof(name_buf), "enum_drm_card:device=%d", d);
                snprintf(descr_buf, sizeof(descr_buf), "Device %d DRM card index", d);
                native_event_t *ev_ec = &ntv_table.events[idx];
                ev_ec->id=idx; ev_ec->name=strdup(name_buf); ev_ec->descr=strdup(descr_buf);
                ev_ec->device=d; ev_ec->value=0; ev_ec->mode=PAPI_MODE_READ; ev_ec->variant=1; ev_ec->subvariant=0;
                ev_ec->open_func=open_simple; ev_ec->close_func=close_simple; ev_ec->start_func=start_simple; ev_ec->stop_func=stop_simple;
                ev_ec->access_func=access_amdsmi_enumeration_info; htable_insert(htable, ev_ec->name, ev_ec); idx++;
                if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
                snprintf(name_buf, sizeof(name_buf), "enum_hsa_id:device=%d", d);
                snprintf(descr_buf, sizeof(descr_buf), "Device %d HSA ID", d);
                native_event_t *ev_eh = &ntv_table.events[idx];
                ev_eh->id=idx; ev_eh->name=strdup(name_buf); ev_eh->descr=strdup(descr_buf);
                ev_eh->device=d; ev_eh->value=0; ev_eh->mode=PAPI_MODE_READ; ev_eh->variant=2; ev_eh->subvariant=0;
                ev_eh->open_func=open_simple; ev_eh->close_func=close_simple; ev_eh->start_func=start_simple; ev_eh->stop_func=stop_simple;
                ev_eh->access_func=access_amdsmi_enumeration_info; htable_insert(htable, ev_eh->name, ev_eh); idx++;
                if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
                snprintf(name_buf, sizeof(name_buf), "enum_hip_id:device=%d", d);
                snprintf(descr_buf, sizeof(descr_buf), "Device %d HIP ID", d);
                native_event_t *ev_ehip = &ntv_table.events[idx];
                ev_ehip->id=idx; ev_ehip->name=strdup(name_buf); ev_ehip->descr=strdup(descr_buf);
                ev_ehip->device=d; ev_ehip->value=0; ev_ehip->mode=PAPI_MODE_READ; ev_ehip->variant=3; ev_ehip->subvariant=0;
                ev_ehip->open_func=open_simple; ev_ehip->close_func=close_simple; ev_ehip->start_func=start_simple; ev_ehip->stop_func=stop_simple;
                ev_ehip->access_func=access_amdsmi_enumeration_info; htable_insert(htable, ev_ehip->name, ev_ehip); idx++;
            }
        }
#endif
        /* ASIC info (numeric IDs & CU count) */
        if (amdsmi_get_gpu_asic_info_p) {
            amdsmi_asic_info_t ainfo;
            if (amdsmi_get_gpu_asic_info_p(device_handles[d], &ainfo) == AMDSMI_STATUS_SUCCESS) {
                if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
                snprintf(name_buf, sizeof(name_buf), "asic_vendor_id:device=%d", d);
                snprintf(descr_buf, sizeof(descr_buf), "Device %d ASIC vendor id", d);
                native_event_t *ev_av = &ntv_table.events[idx];
                ev_av->id=idx; ev_av->name=strdup(name_buf); ev_av->descr=strdup(descr_buf);
                ev_av->device=d; ev_av->value=0; ev_av->mode=PAPI_MODE_READ; ev_av->variant=0; ev_av->subvariant=0;
                ev_av->open_func=open_simple; ev_av->close_func=close_simple; ev_av->start_func=start_simple; ev_av->stop_func=stop_simple;
                ev_av->access_func=access_amdsmi_asic_info; htable_insert(htable, ev_av->name, ev_av); idx++;
                if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
                snprintf(name_buf, sizeof(name_buf), "asic_device_id:device=%d", d);
                snprintf(descr_buf, sizeof(descr_buf), "Device %d ASIC device id", d);
                native_event_t *ev_ad = &ntv_table.events[idx];
                ev_ad->id=idx; ev_ad->name=strdup(name_buf); ev_ad->descr=strdup(descr_buf);
                ev_ad->device=d; ev_ad->value=0; ev_ad->mode=PAPI_MODE_READ; ev_ad->variant=1; ev_ad->subvariant=0;
                ev_ad->open_func=open_simple; ev_ad->close_func=close_simple; ev_ad->start_func=start_simple; ev_ad->stop_func=stop_simple;
                ev_ad->access_func=access_amdsmi_asic_info; htable_insert(htable, ev_ad->name, ev_ad); idx++;
                if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
                snprintf(name_buf, sizeof(name_buf), "asic_subsystem_vendor_id:device=%d", d);
                snprintf(descr_buf, sizeof(descr_buf), "Device %d ASIC subsystem vendor id", d);
                native_event_t *ev_asv = &ntv_table.events[idx];
                ev_asv->id=idx; ev_asv->name=strdup(name_buf); ev_asv->descr=strdup(descr_buf);
                ev_asv->device=d; ev_asv->value=0; ev_asv->mode=PAPI_MODE_READ; ev_asv->variant=2; ev_asv->subvariant=0;
                ev_asv->open_func=open_simple; ev_asv->close_func=close_simple; ev_asv->start_func=start_simple; ev_asv->stop_func=stop_simple;
                ev_asv->access_func=access_amdsmi_asic_info; htable_insert(htable, ev_asv->name, ev_asv); idx++;
                if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
                snprintf(name_buf, sizeof(name_buf), "asic_subsystem_id:device=%d", d);
                snprintf(descr_buf, sizeof(descr_buf), "Device %d ASIC subsystem id", d);
                native_event_t *ev_ass = &ntv_table.events[idx];
                ev_ass->id=idx; ev_ass->name=strdup(name_buf); ev_ass->descr=strdup(descr_buf);
                ev_ass->device=d; ev_ass->value=0; ev_ass->mode=PAPI_MODE_READ; ev_ass->variant=3; ev_ass->subvariant=0;
                ev_ass->open_func=open_simple; ev_ass->close_func=close_simple; ev_ass->start_func=start_simple; ev_ass->stop_func=stop_simple;
                ev_ass->access_func=access_amdsmi_asic_info; htable_insert(htable, ev_ass->name, ev_ass); idx++;
                if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
                snprintf(name_buf, sizeof(name_buf), "asic_revision:device=%d", d);
                snprintf(descr_buf, sizeof(descr_buf), "Device %d ASIC revision id", d);
                native_event_t *ev_ar = &ntv_table.events[idx];
                ev_ar->id=idx; ev_ar->name=strdup(name_buf); ev_ar->descr=strdup(descr_buf);
                ev_ar->device=d; ev_ar->value=0; ev_ar->mode=PAPI_MODE_READ; ev_ar->variant=4; ev_ar->subvariant=0;
                ev_ar->open_func=open_simple; ev_ar->close_func=close_simple; ev_ar->start_func=start_simple; ev_ar->stop_func=stop_simple;
                ev_ar->access_func=access_amdsmi_asic_info; htable_insert(htable, ev_ar->name, ev_ar); idx++;
                if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
                snprintf(name_buf, sizeof(name_buf), "compute_units:device=%d", d);
                snprintf(descr_buf, sizeof(descr_buf), "Device %d number of compute units", d);
                native_event_t *ev_cu = &ntv_table.events[idx];
                ev_cu->id=idx; ev_cu->name=strdup(name_buf); ev_cu->descr=strdup(descr_buf);
                ev_cu->device=d; ev_cu->value=0; ev_cu->mode=PAPI_MODE_READ; ev_cu->variant=5; ev_cu->subvariant=0;
                ev_cu->open_func=open_simple; ev_cu->close_func=close_simple; ev_cu->start_func=start_simple; ev_cu->stop_func=stop_simple;
                ev_cu->access_func=access_amdsmi_asic_info; htable_insert(htable, ev_cu->name, ev_cu); idx++;
            }
        }
        /* Driver info (strings hashed) */
        if (amdsmi_get_gpu_driver_info_p) {
            amdsmi_driver_info_t dinfo;
            if (amdsmi_get_gpu_driver_info_p(device_handles[d], &dinfo) == AMDSMI_STATUS_SUCCESS) {
                if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
                snprintf(name_buf, sizeof(name_buf), "driver_name_hash:device=%d", d);
                snprintf(descr_buf, sizeof(descr_buf), "Device %d driver name (hash)", d);
                native_event_t *ev_dname = &ntv_table.events[idx];
                ev_dname->id=idx; ev_dname->name=strdup(name_buf); ev_dname->descr=strdup(descr_buf);
                ev_dname->device=d; ev_dname->value=0; ev_dname->mode=PAPI_MODE_READ; ev_dname->variant=3; ev_dname->subvariant=0;
                ev_dname->open_func=open_simple; ev_dname->close_func=close_simple; ev_dname->start_func=start_simple; ev_dname->stop_func=stop_simple;
                ev_dname->access_func=access_amdsmi_gpu_string_hash; htable_insert(htable, ev_dname->name, ev_dname); idx++;
                if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
                snprintf(name_buf, sizeof(name_buf), "driver_date_hash:device=%d", d);
                snprintf(descr_buf, sizeof(descr_buf), "Device %d driver date (hash)", d);
                native_event_t *ev_dd = &ntv_table.events[idx];
                ev_dd->id=idx; ev_dd->name=strdup(name_buf); ev_dd->descr=strdup(descr_buf);
                ev_dd->device=d; ev_dd->value=0; ev_dd->mode=PAPI_MODE_READ; ev_dd->variant=4; ev_dd->subvariant=0;
                ev_dd->open_func=open_simple; ev_dd->close_func=close_simple; ev_dd->start_func=start_simple; ev_dd->stop_func=stop_simple;
                ev_dd->access_func=access_amdsmi_gpu_string_hash; htable_insert(htable, ev_dd->name, ev_dd); idx++;
            }
        }
        /* VBIOS info (strings hashed) */
        if (amdsmi_get_gpu_vbios_info_p) {
            amdsmi_vbios_info_t vb;
            if (amdsmi_get_gpu_vbios_info_p(device_handles[d], &vb) == AMDSMI_STATUS_SUCCESS) {
                if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
                snprintf(name_buf, sizeof(name_buf), "vbios_version_hash:device=%d", d);
                snprintf(descr_buf, sizeof(descr_buf), "Device %d vBIOS version (hash)", d);
                native_event_t *ev_vbv = &ntv_table.events[idx];
                ev_vbv->id=idx; ev_vbv->name=strdup(name_buf); ev_vbv->descr=strdup(descr_buf);
                ev_vbv->device=d; ev_vbv->value=0; ev_vbv->mode=PAPI_MODE_READ; ev_vbv->variant=5; ev_vbv->subvariant=0;
                ev_vbv->open_func=open_simple; ev_vbv->close_func=close_simple; ev_vbv->start_func=start_simple; ev_vbv->stop_func=stop_simple;
                ev_vbv->access_func=access_amdsmi_gpu_string_hash; htable_insert(htable, ev_vbv->name, ev_vbv); idx++;
                if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
                snprintf(name_buf, sizeof(name_buf), "vbios_part_number_hash:device=%d", d);
                snprintf(descr_buf, sizeof(descr_buf), "Device %d vBIOS part number (hash)", d);
                native_event_t *ev_vbp = &ntv_table.events[idx];
                ev_vbp->id=idx; ev_vbp->name=strdup(name_buf); ev_vbp->descr=strdup(descr_buf);
                ev_vbp->device=d; ev_vbp->value=0; ev_vbp->mode=PAPI_MODE_READ; ev_vbp->variant=6; ev_vbp->subvariant=0;
                ev_vbp->open_func=open_simple; ev_vbp->close_func=close_simple; ev_vbp->start_func=start_simple; ev_vbp->stop_func=stop_simple;
                ev_vbp->access_func=access_amdsmi_gpu_string_hash; htable_insert(htable, ev_vbp->name, ev_vbp); idx++;
                if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
                snprintf(name_buf, sizeof(name_buf), "vbios_build_date_hash:device=%d", d);
                snprintf(descr_buf, sizeof(descr_buf), "Device %d vBIOS build date (hash)", d);
                native_event_t *ev_vbd = &ntv_table.events[idx];
                ev_vbd->id=idx; ev_vbd->name=strdup(name_buf); ev_vbd->descr=strdup(descr_buf);
                ev_vbd->device=d; ev_vbd->value=0; ev_vbd->mode=PAPI_MODE_READ; ev_vbd->variant=7; ev_vbd->subvariant=0;
                ev_vbd->open_func=open_simple; ev_vbd->close_func=close_simple; ev_vbd->start_func=start_simple; ev_vbd->stop_func=stop_simple;
                ev_vbd->access_func=access_amdsmi_gpu_string_hash; htable_insert(htable, ev_vbd->name, ev_vbd); idx++;
            }
        }
        /* PCIe metrics via amdsmi_get_link_metrics (aggregate read/write kB over XGMI) */
        if (amdsmi_get_link_metrics_p) {
            amdsmi_link_metrics_t lm;
            if (amdsmi_get_link_metrics_p(device_handles[d], &lm) == AMDSMI_STATUS_SUCCESS) {
                if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
                snprintf(name_buf, sizeof(name_buf), "xgmi_total_read_kB:device=%d", d);
                snprintf(descr_buf, sizeof(descr_buf), "Device %d total XGMI read across links (kB)", d);
                native_event_t *ev_xr = &ntv_table.events[idx];
                ev_xr->id=idx; ev_xr->name=strdup(name_buf); ev_xr->descr=strdup(descr_buf);
                ev_xr->device=d; ev_xr->value=0; ev_xr->mode=PAPI_MODE_READ; ev_xr->variant=0; ev_xr->subvariant=0;
                ev_xr->open_func=open_simple; ev_xr->close_func=close_simple; ev_xr->start_func=start_simple; ev_xr->stop_func=stop_simple;
                ev_xr->access_func=access_amdsmi_link_metrics; htable_insert(htable, ev_xr->name, ev_xr); idx++;
                if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
                snprintf(name_buf, sizeof(name_buf), "xgmi_total_write_kB:device=%d", d);
                snprintf(descr_buf, sizeof(descr_buf), "Device %d total XGMI write across links (kB)", d);
                native_event_t *ev_xw = &ntv_table.events[idx];
                ev_xw->id=idx; ev_xw->name=strdup(name_buf); ev_xw->descr=strdup(descr_buf);
                ev_xw->device=d; ev_xw->value=0; ev_xw->mode=PAPI_MODE_READ; ev_xw->variant=1; ev_xw->subvariant=0;
                ev_xw->open_func=open_simple; ev_xw->close_func=close_simple; ev_xw->start_func=start_simple; ev_xw->stop_func=stop_simple;
                ev_xw->access_func=access_amdsmi_link_metrics; htable_insert(htable, ev_xw->name, ev_xw); idx++;
            }
        }
        /* Process list size (count of running GPU processes) */
        if (amdsmi_get_gpu_process_list_p) {
            if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
            snprintf(name_buf, sizeof(name_buf), "process_count:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d active GPU process count", d);
            native_event_t *ev_pc = &ntv_table.events[idx];
            ev_pc->id=idx; ev_pc->name=strdup(name_buf); ev_pc->descr=strdup(descr_buf);
            ev_pc->device=d; ev_pc->value=0; ev_pc->mode=PAPI_MODE_READ; ev_pc->variant=0; ev_pc->subvariant=0;
            ev_pc->open_func=open_simple; ev_pc->close_func=close_simple; ev_pc->start_func=start_simple; ev_pc->stop_func=stop_simple;
            ev_pc->access_func=access_amdsmi_process_count; htable_insert(htable, ev_pc->name, ev_pc); idx++;
        }
        /* ECC totals & enabled mask (where supported) */
        if (amdsmi_get_gpu_total_ecc_count_p) {
            if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
            snprintf(name_buf, sizeof(name_buf), "ecc_total_correctable:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d total ECC correctable errors", d);
            native_event_t *ev_ecct = &ntv_table.events[idx];
            ev_ecct->id=idx; ev_ecct->name=strdup(name_buf); ev_ecct->descr=strdup(descr_buf);
            ev_ecct->device=d; ev_ecct->value=0; ev_ecct->mode=PAPI_MODE_READ; ev_ecct->variant=0; ev_ecct->subvariant=0;
            ev_ecct->open_func=open_simple; ev_ecct->close_func=close_simple; ev_ecct->start_func=start_simple; ev_ecct->stop_func=stop_simple;
            ev_ecct->access_func=access_amdsmi_ecc_total; htable_insert(htable, ev_ecct->name, ev_ecct); idx++;
            if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
            snprintf(name_buf, sizeof(name_buf), "ecc_total_uncorrectable:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d total ECC uncorrectable errors", d);
            native_event_t *ev_ecctu = &ntv_table.events[idx];
            ev_ecctu->id=idx; ev_ecctu->name=strdup(name_buf); ev_ecctu->descr=strdup(descr_buf);
            ev_ecctu->device=d; ev_ecctu->value=0; ev_ecctu->mode=PAPI_MODE_READ; ev_ecctu->variant=1; ev_ecctu->subvariant=0;
            ev_ecctu->open_func=open_simple; ev_ecctu->close_func=close_simple; ev_ecctu->start_func=start_simple; ev_ecctu->stop_func=stop_simple;
            ev_ecctu->access_func=access_amdsmi_ecc_total; htable_insert(htable, ev_ecctu->name, ev_ecctu); idx++;
            if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
            snprintf(name_buf, sizeof(name_buf), "ecc_total_deferred:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d total ECC deferred errors", d);
            native_event_t *ev_ecctd = &ntv_table.events[idx];
            ev_ecctd->id=idx; ev_ecctd->name=strdup(name_buf); ev_ecctd->descr=strdup(descr_buf);
            ev_ecctd->device=d; ev_ecctd->value=0; ev_ecctd->mode=PAPI_MODE_READ; ev_ecctd->variant=2; ev_ecctd->subvariant=0;
            ev_ecctd->open_func=open_simple; ev_ecctd->close_func=close_simple; ev_ecctd->start_func=start_simple; ev_ecctd->stop_func=stop_simple;
            ev_ecctd->access_func=access_amdsmi_ecc_total; htable_insert(htable, ev_ecctd->name, ev_ecctd); idx++;
        }
        if (amdsmi_get_gpu_ecc_enabled_p) {
            if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
            snprintf(name_buf, sizeof(name_buf), "ecc_enabled_mask:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d ECC enabled block bitmask", d);
            native_event_t *ev_eccm = &ntv_table.events[idx];
            ev_eccm->id=idx; ev_eccm->name=strdup(name_buf); ev_eccm->descr=strdup(descr_buf);
            ev_eccm->device=d; ev_eccm->value=0; ev_eccm->mode=PAPI_MODE_READ; ev_eccm->variant=0; ev_eccm->subvariant=0;
            ev_eccm->open_func=open_simple; ev_eccm->close_func=close_simple; ev_eccm->start_func=start_simple; ev_eccm->stop_func=stop_simple;
            ev_eccm->access_func=access_amdsmi_ecc_enabled_mask; htable_insert(htable, ev_eccm->name, ev_eccm); idx++;
        }
        /* Partitioning state (hash/enumeration) */
        if (amdsmi_get_gpu_compute_partition_p) {
            if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
            snprintf(name_buf, sizeof(name_buf), "compute_partition_hash:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d compute partition (hash)", d);
            native_event_t *ev_cpart = &ntv_table.events[idx];
            ev_cpart->id=idx; ev_cpart->name=strdup(name_buf); ev_cpart->descr=strdup(descr_buf);
            ev_cpart->device=d; ev_cpart->value=0; ev_cpart->mode=PAPI_MODE_READ; ev_cpart->variant=0; ev_cpart->subvariant=0;
            ev_cpart->open_func=open_simple; ev_cpart->close_func=close_simple; ev_cpart->start_func=start_simple; ev_cpart->stop_func=stop_simple;
            ev_cpart->access_func=access_amdsmi_compute_partition_hash; htable_insert(htable, ev_cpart->name, ev_cpart); idx++;
        }
        if (amdsmi_get_gpu_memory_partition_p) {
            if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
            snprintf(name_buf, sizeof(name_buf), "memory_partition_hash:device=%d", d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d memory partition (hash)", d);
            native_event_t *ev_mpart = &ntv_table.events[idx];
            ev_mpart->id=idx; ev_mpart->name=strdup(name_buf); ev_mpart->descr=strdup(descr_buf);
            ev_mpart->device=d; ev_mpart->value=0; ev_mpart->mode=PAPI_MODE_READ; ev_mpart->variant=0; ev_mpart->subvariant=0;
            ev_mpart->open_func=open_simple; ev_mpart->close_func=close_simple; ev_mpart->start_func=start_simple; ev_mpart->stop_func=stop_simple;
            ev_mpart->access_func=access_amdsmi_memory_partition_hash; htable_insert(htable, ev_mpart->name, ev_mpart); idx++;
        }
        if (amdsmi_get_gpu_accelerator_partition_profile_p) {
            amdsmi_accelerator_partition_profile_t prof;
            uint32_t ids[AMDSMI_MAX_ACCELERATOR_PARTITIONS] = {0};
            if (amdsmi_get_gpu_accelerator_partition_profile_p(device_handles[d], &prof, ids) == AMDSMI_STATUS_SUCCESS) {
                if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
                snprintf(name_buf, sizeof(name_buf), "accelerator_num_partitions:device=%d", d);
                snprintf(descr_buf, sizeof(descr_buf), "Device %d accelerator profile partitions", d);
                native_event_t *ev_anp = &ntv_table.events[idx];
                ev_anp->id=idx; ev_anp->name=strdup(name_buf); ev_anp->descr=strdup(descr_buf);
                ev_anp->device=d; ev_anp->value=0; ev_anp->mode=PAPI_MODE_READ; ev_anp->variant=0; ev_anp->subvariant=0;
                ev_anp->open_func=open_simple; ev_anp->close_func=close_simple; ev_anp->start_func=start_simple; ev_anp->stop_func=stop_simple;
                ev_anp->access_func=access_amdsmi_accelerator_num_partitions; htable_insert(htable, ev_anp->name, ev_anp); idx++;
            }
        }
    }
// Cleanup - no device capabilities cache to free
    ntv_table.count = idx;
    return PAPI_OK;
}

/* -------- Helpers and new accessors (GPU read-only additions) -------- */
static uint64_t _str_to_u64_hash(const char *s) {
    /* djb2 64-bit */
    uint64_t hash = 5381;
    if (!s) return 0;
    int c;
    while ((c = *s++)) {
        hash = ((hash << 5) + hash) + (uint8_t)c;
    }
    return hash;
}
static int access_amdsmi_lib_version(int mode, void *arg) {
    if (mode != PAPI_MODE_READ) return PAPI_ENOSUPP;
    if (!amdsmi_get_lib_version_p) return PAPI_ENOSUPP;
    native_event_t *event = (native_event_t *)arg;
    amdsmi_version_t vinfo;
    amdsmi_status_t st = amdsmi_get_lib_version_p(&vinfo);
    if (st != AMDSMI_STATUS_SUCCESS) return PAPI_EMISC;
    switch (event->variant) {
        case 0: event->value = (int64_t)vinfo.major; break;
        case 1: event->value = (int64_t)vinfo.minor; break;
        case 2: event->value = (int64_t)vinfo.release; break;
        default: return PAPI_EMISC;
    }
    return PAPI_OK;
}
static int access_amdsmi_uuid_hash(int mode, void *arg) {
    if (mode != PAPI_MODE_READ) return PAPI_ENOSUPP;
    if (!amdsmi_get_gpu_device_uuid_p) return PAPI_ENOSUPP;
    native_event_t *event = (native_event_t *)arg;
    if (event->device < 0 || event->device >= device_count || !device_handles[event->device]) return PAPI_EMISC;
    char buf[128] = {0};
    unsigned int len = sizeof(buf);
    amdsmi_status_t st = amdsmi_get_gpu_device_uuid_p(device_handles[event->device], &len, buf);
    if (st != AMDSMI_STATUS_SUCCESS) return PAPI_EMISC;
    event->value = (int64_t)_str_to_u64_hash(buf);
    return PAPI_OK;
}
static int access_amdsmi_gpu_string_hash(int mode, void *arg) {
    if (mode != PAPI_MODE_READ) return PAPI_ENOSUPP;
    native_event_t *event = (native_event_t *)arg;
    if (event->device < 0 || event->device >= device_count || !device_handles[event->device]) return PAPI_EMISC;
    char buf[256] = {0};
    amdsmi_status_t st = AMDSMI_STATUS_NOT_SUPPORTED;
    switch (event->variant) {
        case 0: /* vendor name */
            if (!amdsmi_get_gpu_vendor_name_p) return PAPI_ENOSUPP;
            st = amdsmi_get_gpu_vendor_name_p(device_handles[event->device], buf, sizeof(buf));
            break;
        case 1: /* vram vendor */
            if (!amdsmi_get_gpu_vram_vendor_p) return PAPI_ENOSUPP;
            st = amdsmi_get_gpu_vram_vendor_p(device_handles[event->device], buf, sizeof(buf));
            break;
        case 2: /* subsystem name */
            if (!amdsmi_get_gpu_subsystem_name_p) return PAPI_ENOSUPP;
            st = amdsmi_get_gpu_subsystem_name_p(device_handles[event->device], buf, sizeof(buf));
            break;
        case 3: /* driver name */
        case 4: /* driver date */
            if (!amdsmi_get_gpu_driver_info_p) return PAPI_ENOSUPP;
            {
                amdsmi_driver_info_t dinfo;
                st = amdsmi_get_gpu_driver_info_p(device_handles[event->device], &dinfo);
                if (st == AMDSMI_STATUS_SUCCESS) {
                    if (event->variant == 3) strncpy(buf, dinfo.driver_name, sizeof(buf)-1);
                    else strncpy(buf, dinfo.driver_date, sizeof(buf)-1);
                }
            }
            break;
        case 5: /* vbios version */
        case 6: /* vbios part number */
        case 7: /* vbios build date */
            if (!amdsmi_get_gpu_vbios_info_p) return PAPI_ENOSUPP;
            {
                amdsmi_vbios_info_t vb;
                st = amdsmi_get_gpu_vbios_info_p(device_handles[event->device], &vb);
                if (st == AMDSMI_STATUS_SUCCESS) {
                    if (event->variant == 5) strncpy(buf, vb.version, sizeof(buf)-1);
                    else if (event->variant == 6) strncpy(buf, vb.part_number, sizeof(buf)-1);
                    else strncpy(buf, vb.build_date, sizeof(buf)-1);
                }
            }
            break;
        default:
            return PAPI_ENOSUPP;
    }
    if (st != AMDSMI_STATUS_SUCCESS) return PAPI_EMISC;
    event->value = (int64_t)_str_to_u64_hash(buf);
    return PAPI_OK;
}
#ifdef AMDSMI_HAVE_ENUMERATION_INFO
static int access_amdsmi_enumeration_info(int mode, void *arg) {
    if (mode != PAPI_MODE_READ) return PAPI_ENOSUPP;
    if (!amdsmi_get_gpu_enumeration_info_p) return PAPI_ENOSUPP;
    native_event_t *event = (native_event_t *)arg;
    if (event->device < 0 || event->device >= device_count || !device_handles[event->device]) return PAPI_EMISC;
    amdsmi_enumeration_info_t info;
    amdsmi_status_t st = amdsmi_get_gpu_enumeration_info_p(device_handles[event->device], &info);
    if (st != AMDSMI_STATUS_SUCCESS) return PAPI_EMISC;
    switch (event->variant) {
        case 0: event->value = (int64_t)info.drm_render; break;
        case 1: event->value = (int64_t)info.drm_card; break;
        case 2: event->value = (int64_t)info.hsa_id; break;
        case 3: event->value = (int64_t)info.hip_id; break;
        default: return PAPI_EMISC;
    }
    return PAPI_OK;
}
#endif
static int access_amdsmi_asic_info(int mode, void *arg) {
    if (mode != PAPI_MODE_READ) return PAPI_ENOSUPP;
    if (!amdsmi_get_gpu_asic_info_p) return PAPI_ENOSUPP;
    native_event_t *event = (native_event_t *)arg;
    if (event->device < 0 || event->device >= device_count || !device_handles[event->device]) return PAPI_EMISC;
    amdsmi_asic_info_t info;
    amdsmi_status_t st = amdsmi_get_gpu_asic_info_p(device_handles[event->device], &info);
    if (st != AMDSMI_STATUS_SUCCESS) return PAPI_EMISC;
    switch (event->variant) {
        case 0: event->value = (int64_t)info.vendor_id; break;
        case 1: event->value = (int64_t)info.device_id; break;
        case 2: event->value = (int64_t)info.subvendor_id; break;
        case 3: event->value = (int64_t)0 /* not provided in amdsmi_asic_info_t */; break;
        case 4: event->value = (int64_t)info.rev_id; break;
        case 5: event->value = (int64_t)info.num_of_compute_units; break;
        default: return PAPI_EMISC;
    }
    return PAPI_OK;
}
static int access_amdsmi_link_metrics(int mode, void *arg) {
    if (mode != PAPI_MODE_READ) return PAPI_ENOSUPP;
    if (!amdsmi_get_link_metrics_p) return PAPI_ENOSUPP;
    native_event_t *event = (native_event_t *)arg;
    if (event->device < 0 || event->device >= device_count || !device_handles[event->device]) return PAPI_EMISC;
    amdsmi_link_metrics_t lm;
    amdsmi_status_t st = amdsmi_get_link_metrics_p(device_handles[event->device], &lm);
    if (st != AMDSMI_STATUS_SUCCESS) return PAPI_EMISC;
    uint64_t total_read=0, total_write=0;
    /* Sum over links */
    for (int i = 0; i < AMDSMI_MAX_NUM_XGMI_PHYSICAL_LINK; ++i) {
        total_read  += lm.links[i].read;
        total_write += lm.links[i].write;
    }
    if (event->variant == 0) event->value = (int64_t)total_read;
    else event->value = (int64_t)total_write;
    return PAPI_OK;
}
static int access_amdsmi_process_count(int mode, void *arg) {
    if (mode != PAPI_MODE_READ) return PAPI_ENOSUPP;
    if (!amdsmi_get_gpu_process_list_p) return PAPI_ENOSUPP;
    native_event_t *event = (native_event_t *)arg;
    if (event->device < 0 || event->device >= device_count || !device_handles[event->device]) return PAPI_EMISC;
    uint32_t cap = 32;
    int tries = 0;
    amdsmi_proc_info_t *list = NULL;
    amdsmi_status_t st;
    while (tries++ < 4) {
        list = (amdsmi_proc_info_t *)papi_calloc(cap, sizeof(amdsmi_proc_info_t));
        if (!list) return PAPI_ENOMEM;
        uint32_t maxp = cap;
        st = amdsmi_get_gpu_process_list_p(device_handles[event->device], &maxp, list);
        if (st == AMDSMI_STATUS_OUT_OF_RESOURCES) {
            papi_free(list);
            cap *= 2;
            continue;
        }
        if (st != AMDSMI_STATUS_SUCCESS) {
            papi_free(list);
            return PAPI_EMISC;
        }
        event->value = (int64_t)maxp;
        papi_free(list);
        return PAPI_OK;
    }
    return PAPI_EMISC;
}
static int access_amdsmi_ecc_total(int mode, void *arg) {
    if (mode != PAPI_MODE_READ) return PAPI_ENOSUPP;
    if (!amdsmi_get_gpu_total_ecc_count_p) return PAPI_ENOSUPP;
    native_event_t *event = (native_event_t *)arg;
    if (event->device < 0 || event->device >= device_count || !device_handles[event->device]) return PAPI_EMISC;
    amdsmi_error_count_t ec;
    amdsmi_status_t st = amdsmi_get_gpu_total_ecc_count_p(device_handles[event->device], &ec);
    if (st != AMDSMI_STATUS_SUCCESS) return PAPI_EMISC;
    switch (event->variant) {
        case 0: event->value = (int64_t)ec.correctable_count; break;
        case 1: event->value = (int64_t)ec.uncorrectable_count; break;
        case 2: event->value = (int64_t)ec.deferred_count; break;
        default: return PAPI_EMISC;
    }
    return PAPI_OK;
}
static int access_amdsmi_ecc_enabled_mask(int mode, void *arg) {
    if (mode != PAPI_MODE_READ) return PAPI_ENOSUPP;
    if (!amdsmi_get_gpu_ecc_enabled_p) return PAPI_ENOSUPP;
    native_event_t *event = (native_event_t *)arg;
    if (event->device < 0 || event->device >= device_count || !device_handles[event->device]) return PAPI_EMISC;
    uint64_t mask=0;
    amdsmi_status_t st = amdsmi_get_gpu_ecc_enabled_p(device_handles[event->device], &mask);
    if (st != AMDSMI_STATUS_SUCCESS) return PAPI_EMISC;
    event->value = (int64_t)mask;
    return PAPI_OK;
}
static int access_amdsmi_compute_partition_hash(int mode, void *arg) {
    if (mode != PAPI_MODE_READ) return PAPI_ENOSUPP;
    if (!amdsmi_get_gpu_compute_partition_p) return PAPI_ENOSUPP;
    native_event_t *event = (native_event_t *)arg;
    if (event->device < 0 || event->device >= device_count || !device_handles[event->device]) return PAPI_EMISC;
    char buf[128] = {0};
    amdsmi_status_t st = amdsmi_get_gpu_compute_partition_p(device_handles[event->device], buf, sizeof(buf));
    if (st != AMDSMI_STATUS_SUCCESS) return PAPI_EMISC;
    event->value = (int64_t)_str_to_u64_hash(buf);
    return PAPI_OK;
}
static int access_amdsmi_memory_partition_hash(int mode, void *arg) {
    if (mode != PAPI_MODE_READ) return PAPI_ENOSUPP;
    if (!amdsmi_get_gpu_memory_partition_p) return PAPI_ENOSUPP;
    native_event_t *event = (native_event_t *)arg;
    if (event->device < 0 || event->device >= device_count || !device_handles[event->device]) return PAPI_EMISC;
    char buf[128] = {0};
    amdsmi_status_t st = amdsmi_get_gpu_memory_partition_p(device_handles[event->device], buf, sizeof(buf));
    if (st != AMDSMI_STATUS_SUCCESS) return PAPI_EMISC;
    event->value = (int64_t)_str_to_u64_hash(buf);
    return PAPI_OK;
}
static int access_amdsmi_accelerator_num_partitions(int mode, void *arg) {
    if (mode != PAPI_MODE_READ) return PAPI_ENOSUPP;
    if (!amdsmi_get_gpu_accelerator_partition_profile_p) return PAPI_ENOSUPP;
    native_event_t *event = (native_event_t *)arg;
    if (event->device < 0 || event->device >= device_count || !device_handles[event->device]) return PAPI_EMISC;
    amdsmi_accelerator_partition_profile_t prof;
    uint32_t ids[AMDSMI_MAX_ACCELERATOR_PARTITIONS] = {0};
    amdsmi_status_t st = amdsmi_get_gpu_accelerator_partition_profile_p(device_handles[event->device], &prof, ids);
    if (st != AMDSMI_STATUS_SUCCESS) return PAPI_EMISC;
    event->value = (int64_t)prof.num_partitions;
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
