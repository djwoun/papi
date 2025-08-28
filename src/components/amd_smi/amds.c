#include "amds.h"
#include "amds_priv.h"
#include "amdsmi.h"
#include "htable.h"
#include "papi.h"
#include "papi_memory.h"
#include <dlfcn.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
 #define MAX_EVENTS_PER_DEVICE 1024

unsigned int _amd_smi_lock;
/* Pointers to AMD SMI library functions (dynamically loaded) */
amdsmi_status_t (*amdsmi_init_p)(uint64_t);
amdsmi_status_t (*amdsmi_shut_down_p)(void);
amdsmi_status_t (*amdsmi_get_socket_handles_p)(uint32_t *, amdsmi_socket_handle *);
amdsmi_status_t (*amdsmi_get_processor_handles_by_type_p)(amdsmi_socket_handle, processor_type_t, amdsmi_processor_handle *,
                                                          uint32_t *);
amdsmi_status_t (*amdsmi_get_temp_metric_p)(amdsmi_processor_handle, amdsmi_temperature_type_t, amdsmi_temperature_metric_t,
                                            int64_t *);
amdsmi_status_t (*amdsmi_get_gpu_fan_rpms_p)(amdsmi_processor_handle, uint32_t, int64_t *);
amdsmi_status_t (*amdsmi_get_gpu_fan_speed_p)(amdsmi_processor_handle, uint32_t, int64_t *);
amdsmi_status_t (*amdsmi_get_gpu_fan_speed_max_p)(amdsmi_processor_handle, uint32_t, int64_t *);
amdsmi_status_t (*amdsmi_get_total_memory_p)(amdsmi_processor_handle, amdsmi_memory_type_t, uint64_t *);
amdsmi_status_t (*amdsmi_get_memory_usage_p)(amdsmi_processor_handle, amdsmi_memory_type_t, uint64_t *);
amdsmi_status_t (*amdsmi_get_gpu_activity_p)(amdsmi_processor_handle, amdsmi_engine_usage_t *);
amdsmi_status_t (*amdsmi_get_power_cap_info_p)(amdsmi_processor_handle, uint32_t, amdsmi_power_cap_info_t *);
amdsmi_status_t (*amdsmi_get_gpu_power_cap_set_p)(amdsmi_processor_handle, uint32_t, uint64_t);
amdsmi_status_t (*amdsmi_get_power_info_p)(amdsmi_processor_handle, amdsmi_power_info_t *);
amdsmi_status_t (*amdsmi_set_power_cap_p)(amdsmi_processor_handle, uint32_t, uint64_t);
amdsmi_status_t (*amdsmi_get_gpu_pci_throughput_p)(amdsmi_processor_handle, uint64_t *, uint64_t *, uint64_t *);
amdsmi_status_t (*amdsmi_get_gpu_pci_replay_counter_p)(amdsmi_processor_handle, uint64_t *);
amdsmi_status_t (*amdsmi_get_clk_freq_p)(amdsmi_processor_handle, amdsmi_clk_type_t, amdsmi_frequencies_t *);
amdsmi_status_t (*amdsmi_set_clk_freq_p)(amdsmi_processor_handle, amdsmi_clk_type_t, uint64_t);
amdsmi_status_t (*amdsmi_get_gpu_metrics_info_p)(amdsmi_processor_handle, amdsmi_gpu_metrics_t *);

/* Additional read-only AMD SMI function pointers */
amdsmi_status_t (*amdsmi_get_lib_version_p)(amdsmi_version_t *);
amdsmi_status_t (*amdsmi_get_gpu_driver_info_p)(amdsmi_processor_handle, amdsmi_driver_info_t *);
amdsmi_status_t (*amdsmi_get_gpu_asic_info_p)(amdsmi_processor_handle, amdsmi_asic_info_t *);
amdsmi_status_t (*amdsmi_get_gpu_board_info_p)(amdsmi_processor_handle, amdsmi_board_info_t *);
amdsmi_status_t (*amdsmi_get_fw_info_p)(amdsmi_processor_handle, amdsmi_fw_info_t *);
amdsmi_status_t (*amdsmi_get_gpu_vbios_info_p)(amdsmi_processor_handle, amdsmi_vbios_info_t *);
amdsmi_status_t (*amdsmi_get_gpu_device_uuid_p)(amdsmi_processor_handle, unsigned int *, char *);
amdsmi_status_t (*amdsmi_get_gpu_enumeration_info_p)(amdsmi_processor_handle, amdsmi_enumeration_info_t *);
amdsmi_status_t (*amdsmi_get_gpu_vendor_name_p)(amdsmi_processor_handle, char *, size_t);
amdsmi_status_t (*amdsmi_get_gpu_vram_vendor_p)(amdsmi_processor_handle, char *, uint32_t);
amdsmi_status_t (*amdsmi_get_gpu_subsystem_name_p)(amdsmi_processor_handle, char *, size_t);
amdsmi_status_t (*amdsmi_get_link_metrics_p)(amdsmi_processor_handle, amdsmi_link_metrics_t *);
amdsmi_status_t (*amdsmi_get_gpu_process_list_p)(amdsmi_processor_handle, uint32_t *, amdsmi_proc_info_t *);
amdsmi_status_t (*amdsmi_get_gpu_ecc_enabled_p)(amdsmi_processor_handle, uint64_t *);
amdsmi_status_t (*amdsmi_get_gpu_total_ecc_count_p)(amdsmi_processor_handle, amdsmi_error_count_t *);
amdsmi_status_t (*amdsmi_get_gpu_ecc_count_p)(amdsmi_processor_handle, amdsmi_gpu_block_t, amdsmi_error_count_t *);
amdsmi_status_t (*amdsmi_get_gpu_ecc_status_p)(amdsmi_processor_handle, amdsmi_gpu_block_t, amdsmi_ras_err_state_t *);
amdsmi_status_t (*amdsmi_get_gpu_compute_partition_p)(amdsmi_processor_handle, char *, uint32_t);
amdsmi_status_t (*amdsmi_get_gpu_memory_partition_p)(amdsmi_processor_handle, char *, uint32_t);
amdsmi_status_t (*amdsmi_get_gpu_accelerator_partition_profile_p)(amdsmi_processor_handle, amdsmi_accelerator_partition_profile_t *,
                                                                  uint32_t *);
amdsmi_status_t (*amdsmi_get_gpu_id_p)(amdsmi_processor_handle, uint16_t *);
amdsmi_status_t (*amdsmi_get_gpu_revision_p)(amdsmi_processor_handle, uint16_t *);
amdsmi_status_t (*amdsmi_get_gpu_subsystem_id_p)(amdsmi_processor_handle, uint16_t *);
amdsmi_status_t (*amdsmi_get_gpu_virtualization_mode_p)(amdsmi_processor_handle, amdsmi_virtualization_mode_t *);
amdsmi_status_t (*amdsmi_get_gpu_process_isolation_p)(amdsmi_processor_handle, uint32_t *);
amdsmi_status_t (*amdsmi_get_gpu_xcd_counter_p)(amdsmi_processor_handle, uint16_t *);
amdsmi_status_t (*amdsmi_get_gpu_pci_bandwidth_p)(amdsmi_processor_handle, amdsmi_pcie_bandwidth_t *);
amdsmi_status_t (*amdsmi_get_gpu_bdf_id_p)(amdsmi_processor_handle, uint64_t *);
amdsmi_status_t (*amdsmi_get_gpu_topo_numa_affinity_p)(amdsmi_processor_handle, int32_t *);
amdsmi_status_t (*amdsmi_get_energy_count_p)(amdsmi_processor_handle, uint64_t *, float *, uint64_t *);
amdsmi_status_t (*amdsmi_get_gpu_power_profile_presets_p)(amdsmi_processor_handle, uint32_t, amdsmi_power_profile_status_t *);
amdsmi_status_t (*amdsmi_get_gpu_cache_info_p)(amdsmi_processor_handle, amdsmi_gpu_cache_info_t *);
amdsmi_status_t (*amdsmi_get_gpu_mem_overdrive_level_p)(amdsmi_processor_handle, uint32_t *);
amdsmi_status_t (*amdsmi_get_gpu_od_volt_curve_regions_p)(amdsmi_processor_handle, uint32_t *, amdsmi_freq_volt_region_t *);
amdsmi_status_t (*amdsmi_get_gpu_od_volt_info_p)(amdsmi_processor_handle, amdsmi_od_volt_freq_data_t *);
amdsmi_status_t (*amdsmi_get_gpu_overdrive_level_p)(amdsmi_processor_handle, uint32_t *);
amdsmi_status_t (*amdsmi_get_gpu_perf_level_p)(amdsmi_processor_handle, amdsmi_dev_perf_level_t *);
amdsmi_status_t (*amdsmi_get_gpu_pm_metrics_info_p)(amdsmi_processor_handle, amdsmi_name_value_t **, uint32_t *);
amdsmi_status_t (*amdsmi_get_gpu_ras_feature_info_p)(amdsmi_processor_handle, amdsmi_ras_feature_t *);
amdsmi_status_t (*amdsmi_get_gpu_ras_block_features_enabled_p)(amdsmi_processor_handle, amdsmi_gpu_block_t, amdsmi_ras_err_state_t *);
amdsmi_status_t (*amdsmi_get_gpu_reg_table_info_p)(amdsmi_processor_handle, amdsmi_reg_type_t, amdsmi_name_value_t **, uint32_t *);
amdsmi_status_t (*amdsmi_get_gpu_volt_metric_p)(amdsmi_processor_handle, amdsmi_voltage_type_t, amdsmi_voltage_metric_t, int64_t *);
amdsmi_status_t (*amdsmi_get_gpu_vram_info_p)(amdsmi_processor_handle, amdsmi_vram_info_t *);
amdsmi_status_t (*amdsmi_get_gpu_vram_usage_p)(amdsmi_processor_handle, amdsmi_vram_usage_t *);
amdsmi_status_t (*amdsmi_get_pcie_info_p)(amdsmi_processor_handle, amdsmi_pcie_info_t *);
amdsmi_status_t (*amdsmi_get_processor_count_from_handles_p)(amdsmi_processor_handle *, uint32_t *, uint32_t *, uint32_t *, uint32_t *);
amdsmi_status_t (*amdsmi_get_soc_pstate_p)(amdsmi_processor_handle, amdsmi_dpm_policy_t *);
amdsmi_status_t (*amdsmi_get_xgmi_plpd_p)(amdsmi_processor_handle, amdsmi_dpm_policy_t *);
amdsmi_status_t (*amdsmi_get_gpu_bad_page_info_p)(amdsmi_processor_handle, uint32_t *, amdsmi_retired_page_record_t *);
amdsmi_status_t (*amdsmi_get_gpu_bad_page_threshold_p)(amdsmi_processor_handle, uint32_t *);
amdsmi_status_t (*amdsmi_get_power_info_v2_p)(amdsmi_processor_handle, uint32_t, amdsmi_power_info_t *);
amdsmi_status_t (*amdsmi_init_gpu_event_notification_p)(amdsmi_processor_handle);
amdsmi_status_t (*amdsmi_set_gpu_event_notification_mask_p)(amdsmi_processor_handle, uint64_t);
amdsmi_status_t (*amdsmi_get_gpu_event_notification_p)(int, uint32_t *, amdsmi_evt_notification_data_t *);
amdsmi_status_t (*amdsmi_stop_gpu_event_notification_p)(amdsmi_processor_handle);

#ifndef AMDSMI_DISABLE_ESMI
/* CPU function pointers */
amdsmi_status_t (*amdsmi_get_cpu_handles_p)(uint32_t *, amdsmi_processor_handle *);
amdsmi_status_t (*amdsmi_get_cpucore_handles_p)(uint32_t *, amdsmi_processor_handle *);
amdsmi_status_t (*amdsmi_get_cpu_socket_power_p)(amdsmi_processor_handle, uint32_t *);
amdsmi_status_t (*amdsmi_get_cpu_socket_power_cap_p)(amdsmi_processor_handle, uint32_t *);
amdsmi_status_t (*amdsmi_get_cpu_socket_power_cap_max_p)(amdsmi_processor_handle, uint32_t *);
amdsmi_status_t (*amdsmi_get_cpu_core_energy_p)(amdsmi_processor_handle, uint64_t *);
amdsmi_status_t (*amdsmi_get_cpu_socket_energy_p)(amdsmi_processor_handle, uint64_t *);
amdsmi_status_t (*amdsmi_get_cpu_smu_fw_version_p)(amdsmi_processor_handle, amdsmi_smu_fw_version_t *);
amdsmi_status_t (*amdsmi_get_threads_per_core_p)(uint32_t *);
amdsmi_status_t (*amdsmi_get_cpu_family_p)(uint32_t *);
amdsmi_status_t (*amdsmi_get_cpu_model_p)(uint32_t *);
amdsmi_status_t (*amdsmi_get_cpu_core_boostlimit_p)(amdsmi_processor_handle, uint32_t *);
amdsmi_status_t (*amdsmi_get_cpu_socket_current_active_freq_limit_p)(amdsmi_processor_handle, uint16_t *, char **);
amdsmi_status_t (*amdsmi_get_cpu_socket_freq_range_p)(amdsmi_processor_handle, uint16_t *, uint16_t *);
amdsmi_status_t (*amdsmi_get_cpu_core_current_freq_limit_p)(amdsmi_processor_handle, uint32_t *);
amdsmi_status_t (*amdsmi_get_minmax_bandwidth_between_processors_p)(amdsmi_processor_handle, amdsmi_processor_handle, uint64_t *,
                                                                    uint64_t *);
amdsmi_status_t (*amdsmi_get_cpu_dimm_temp_range_and_refresh_rate_p)(amdsmi_processor_handle, uint8_t,
                                                                     amdsmi_temp_range_refresh_rate_t *);
amdsmi_status_t (*amdsmi_get_cpu_dimm_power_consumption_p)(amdsmi_processor_handle, uint8_t, amdsmi_dimm_power_t *);
amdsmi_status_t (*amdsmi_get_cpu_dimm_thermal_sensor_p)(amdsmi_processor_handle, uint8_t, amdsmi_dimm_thermal_t *);
#endif
/* Global device list and count */
int32_t device_count = 0;
amdsmi_processor_handle *device_handles = NULL;
int32_t device_mask = 0;
int32_t gpu_count = 0;
int32_t cpu_count = 0;
amdsmi_processor_handle **cpu_core_handles = NULL;
uint32_t *cores_per_socket = NULL;
static void *amds_dlp = NULL;
void *htable = NULL;
static char error_string[PAPI_MAX_STR_LEN + 1];
/* forward declarations for internal helpers */
static int load_amdsmi_sym(void);
static int init_device_table(void);
static int shutdown_device_table(void);
static int init_event_table(void);
static int shutdown_event_table(void);
native_event_table_t ntv_table;
native_event_table_t *ntv_table_p = NULL;

/* Redirects stderr to /dev/null, returns dup of old stderr (or -1 on failure). */
static int silence_stderr_begin(void) {
  int devnull = open("/dev/null", O_WRONLY);
  if (devnull < 0) return -1;
  int saved = dup(STDERR_FILENO);
  if (saved < 0) { close(devnull); return -1; }
  (void)dup2(devnull, STDERR_FILENO);
  close(devnull);
  return saved;
}

/* Restores stderr using the fd returned by silence_stderr_begin(). */
static void silence_stderr_end(int saved_fd) {
  if (saved_fd >= 0) {
    (void)dup2(saved_fd, STDERR_FILENO);
    close(saved_fd);
  }
}

/* Simple open/close/start/stop functions (no special handling needed for most
 * events) */
static int open_simple(native_event_t *event) {
  (void)event;
  return PAPI_OK;
}
static int close_simple(native_event_t *event) {
  (void)event;
  return PAPI_OK;
}
static int start_simple(native_event_t *event) {
  (void)event;
  return PAPI_OK;
}
static int stop_simple(native_event_t *event) {
  (void)event;
  return PAPI_OK;
}

/* Replace any non-alphanumeric characters with '_' to build safe event names */
static void sanitize_name(const char *src, char *dst, size_t len) {
  size_t j = 0;
  for (size_t i = 0; src[i] && j < len - 1; ++i) {
    char c = src[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
      dst[j++] = c;
    else
      dst[j++] = '_';
  }
  dst[j] = '\0';
}
/* Dynamic load of AMD SMI library symbols */
static void *sym(const char *preferred, const char *fallback) {
  void *p = dlsym(amds_dlp, preferred);
  return p ? p : (fallback ? dlsym(amds_dlp, fallback) : NULL);
}
static int load_amdsmi_sym(void) {
  const char *root = getenv("PAPI_AMDSMI_ROOT");
  char so_path[PATH_MAX] = {0};
  if (!root) {
    snprintf(error_string, sizeof(error_string), "PAPI_AMDSMI_ROOT not set; cannot find libamd_smi.so");
    return PAPI_ENOSUPP;
  }
  snprintf(so_path, sizeof(so_path), "%s/lib/libamd_smi.so", root);
  amds_dlp = dlopen(so_path, RTLD_NOW | RTLD_GLOBAL);
  if (!amds_dlp) {
    snprintf(error_string, sizeof(error_string), "dlopen(\"%s\"): %s", so_path, dlerror());
    return PAPI_ENOSUPP;
  }
  /* Resolve required function symbols */
  amdsmi_init_p = sym("amdsmi_init", NULL);
  amdsmi_shut_down_p = sym("amdsmi_shut_down", NULL);
  amdsmi_get_socket_handles_p = sym("amdsmi_get_socket_handles", NULL);
  amdsmi_get_processor_handles_by_type_p = sym("amdsmi_get_processor_handles_by_type", NULL);
  /* Sensors */
  amdsmi_get_temp_metric_p = sym("amdsmi_get_temp_metric", NULL);
  amdsmi_get_gpu_fan_rpms_p = sym("amdsmi_get_gpu_fan_rpms", NULL);
  amdsmi_get_gpu_fan_speed_p = sym("amdsmi_get_gpu_fan_speed", NULL);
  amdsmi_get_gpu_fan_speed_max_p = sym("amdsmi_get_gpu_fan_speed_max", NULL);
  /* Memory */
  amdsmi_get_total_memory_p = sym("amdsmi_get_gpu_memory_total", "amdsmi_get_total_memory");
  amdsmi_get_memory_usage_p = sym("amdsmi_get_gpu_memory_usage", "amdsmi_get_memory_usage");
  /* Utilization / activity */
  amdsmi_get_gpu_activity_p = sym("amdsmi_get_gpu_activity", "amdsmi_get_engine_usage");
  /* Power */
  amdsmi_get_power_info_p = sym("amdsmi_get_power_info", NULL);
  amdsmi_get_power_cap_info_p = sym("amdsmi_get_power_cap_info", NULL);
  amdsmi_set_power_cap_p = sym("amdsmi_set_power_cap", "amdsmi_dev_set_power_cap");
  /* PCIe */
  amdsmi_get_gpu_pci_throughput_p = sym("amdsmi_get_gpu_pci_throughput", NULL);
  amdsmi_get_gpu_pci_replay_counter_p = sym("amdsmi_get_gpu_pci_replay_counter", NULL);
  /* Clocks */
  amdsmi_get_clk_freq_p = sym("amdsmi_get_clk_freq", NULL);
  amdsmi_set_clk_freq_p = sym("amdsmi_set_clk_freq", NULL);
  /* GPU metrics */
  amdsmi_get_gpu_metrics_info_p = sym("amdsmi_get_gpu_metrics_info", NULL);
  /* Identification and other queries */
  amdsmi_get_gpu_id_p = sym("amdsmi_get_gpu_id", NULL);
  amdsmi_get_gpu_revision_p = sym("amdsmi_get_gpu_revision", NULL);
  amdsmi_get_gpu_subsystem_id_p = sym("amdsmi_get_gpu_subsystem_id", NULL);
  amdsmi_get_gpu_virtualization_mode_p = sym("amdsmi_get_gpu_virtualization_mode", NULL);
  amdsmi_get_gpu_process_isolation_p = sym("amdsmi_get_gpu_process_isolation", NULL);
  amdsmi_get_gpu_xcd_counter_p = sym("amdsmi_get_gpu_xcd_counter", NULL);
  amdsmi_get_gpu_pci_bandwidth_p = sym("amdsmi_get_gpu_pci_bandwidth", NULL);
  amdsmi_get_gpu_bdf_id_p = sym("amdsmi_get_gpu_bdf_id", NULL);
  amdsmi_get_gpu_topo_numa_affinity_p = sym("amdsmi_get_gpu_topo_numa_affinity", NULL);
  amdsmi_get_energy_count_p = sym("amdsmi_get_energy_count", NULL);
  amdsmi_get_gpu_power_profile_presets_p = sym("amdsmi_get_gpu_power_profile_presets", NULL);
  /* Additional read-only queries */
  amdsmi_get_lib_version_p = sym("amdsmi_get_lib_version", NULL);
  amdsmi_get_gpu_driver_info_p = sym("amdsmi_get_gpu_driver_info", NULL);
  amdsmi_get_gpu_asic_info_p = sym("amdsmi_get_gpu_asic_info", NULL);
  amdsmi_get_gpu_board_info_p = sym("amdsmi_get_gpu_board_info", NULL);
  amdsmi_get_fw_info_p = sym("amdsmi_get_fw_info", NULL);
  amdsmi_get_gpu_vbios_info_p = sym("amdsmi_get_gpu_vbios_info", NULL);
  amdsmi_get_gpu_device_uuid_p = sym("amdsmi_get_gpu_device_uuid", NULL);
  amdsmi_get_gpu_enumeration_info_p = sym("amdsmi_get_gpu_enumeration_info", NULL);
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
  amdsmi_get_gpu_cache_info_p = sym("amdsmi_get_gpu_cache_info", NULL);
  amdsmi_get_gpu_mem_overdrive_level_p = sym("amdsmi_get_gpu_mem_overdrive_level", NULL);
  amdsmi_get_gpu_od_volt_curve_regions_p = sym("amdsmi_get_gpu_od_volt_curve_regions", NULL);
  amdsmi_get_gpu_od_volt_info_p = sym("amdsmi_get_gpu_od_volt_info", NULL);
  amdsmi_get_gpu_overdrive_level_p = sym("amdsmi_get_gpu_overdrive_level", NULL);
  amdsmi_get_gpu_perf_level_p = sym("amdsmi_get_gpu_perf_level", NULL);
  amdsmi_get_gpu_pm_metrics_info_p = sym("amdsmi_get_gpu_pm_metrics_info", NULL);
  amdsmi_get_gpu_ras_feature_info_p = sym("amdsmi_get_gpu_ras_feature_info", NULL);
  amdsmi_get_gpu_ras_block_features_enabled_p = sym("amdsmi_get_gpu_ras_block_features_enabled", NULL);
  amdsmi_get_gpu_reg_table_info_p = sym("amdsmi_get_gpu_reg_table_info", NULL);
  amdsmi_get_gpu_volt_metric_p = sym("amdsmi_get_gpu_volt_metric", NULL);
  amdsmi_get_gpu_vram_info_p = sym("amdsmi_get_gpu_vram_info", NULL);
  amdsmi_get_gpu_vram_usage_p = sym("amdsmi_get_gpu_vram_usage", NULL);
  amdsmi_get_pcie_info_p = sym("amdsmi_get_pcie_info", NULL);
  amdsmi_get_processor_count_from_handles_p = sym("amdsmi_get_processor_count_from_handles", NULL);
  amdsmi_get_soc_pstate_p = sym("amdsmi_get_soc_pstate", NULL);
  amdsmi_get_xgmi_plpd_p = sym("amdsmi_get_xgmi_plpd", NULL);
  amdsmi_get_gpu_bad_page_info_p = sym("amdsmi_get_gpu_bad_page_info", NULL);
  amdsmi_get_gpu_bad_page_threshold_p = sym("amdsmi_get_gpu_bad_page_threshold", NULL);
  amdsmi_get_power_info_v2_p = sym("amdsmi_get_power_info_v2", NULL);
  amdsmi_init_gpu_event_notification_p = sym("amdsmi_init_gpu_event_notification", NULL);
  amdsmi_set_gpu_event_notification_mask_p = sym("amdsmi_set_gpu_event_notification_mask", NULL);
  amdsmi_get_gpu_event_notification_p = sym("amdsmi_get_gpu_event_notification", NULL);
  amdsmi_stop_gpu_event_notification_p = sym("amdsmi_stop_gpu_event_notification", NULL);

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
  return PAPI_OK;
}

/* Initialize AMD SMI library and discover devices */
int amds_init(void) {
  // Check if already initialized to avoid expensive re-initialization
  if (device_handles != NULL && device_count > 0) {
    return PAPI_OK; // Already initialized
  }
  int papi_errno = load_amdsmi_sym();
  if (papi_errno != PAPI_OK) {
    return papi_errno;
  }
  // AMDSMI_INIT_AMD_CPUS
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
  amdsmi_socket_handle *sockets = (amdsmi_socket_handle *)papi_calloc(socket_count, sizeof(amdsmi_socket_handle));
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
  device_handles = (amdsmi_processor_handle *)papi_calloc(total_gpu_count + total_cpu_count, sizeof(*device_handles));
  if (!device_handles) {
    papi_errno = PAPI_ENOMEM;
    sprintf(error_string, "Memory allocation error for device handles.");
    papi_free(sockets);
    goto fn_fail;
  }
  // Retrieve GPU processor handles for each socket - optimized to reduce
  // allocations
  for (uint32_t s = 0; s < socket_count; ++s) {
    uint32_t gpu_count_local = 0;
    processor_type_t proc_type = AMDSMI_PROCESSOR_TYPE_AMD_GPU;
    status = amdsmi_get_processor_handles_by_type_p(sockets[s], proc_type, NULL, &gpu_count_local);
    if (status != AMDSMI_STATUS_SUCCESS || gpu_count_local == 0) {
      continue; // no GPU on this socket or error
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
  gpu_count = device_count; // All devices added so far are GPUs
#ifndef AMDSMI_DISABLE_ESMI
  // Retrieve CPU socket handles
  amdsmi_processor_handle *cpu_handles = NULL;
  if (total_cpu_count > 0) {
    cpu_handles = (amdsmi_processor_handle *)papi_calloc(total_cpu_count, sizeof(amdsmi_processor_handle));
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
    cpu_core_handles = (amdsmi_processor_handle **)papi_calloc(cpu_count, sizeof(amdsmi_processor_handle *));
    cores_per_socket = (uint32_t *)papi_calloc(cpu_count, sizeof(uint32_t));
    if (!cpu_core_handles || !cores_per_socket) {
      papi_errno = PAPI_ENOMEM;
      sprintf(error_string, "Memory allocation error for CPU core handles.");
      if (cpu_core_handles)
        papi_free(cpu_core_handles);
      if (cores_per_socket)
        papi_free(cores_per_socket);
      goto fn_fail;
    }
    for (uint32_t s = 0; s < cpu_count; ++s) {
      uint32_t core_count = 0;
      amdsmi_status_t st =
          amdsmi_get_processor_handles_by_type_p(device_handles[gpu_count + s], AMDSMI_PROCESSOR_TYPE_AMD_CPU_CORE, NULL, &core_count);
      if (st != AMDSMI_STATUS_SUCCESS || core_count == 0) {
        cores_per_socket[s] = 0;
        cpu_core_handles[s] = NULL;
        continue;
      }
      cpu_core_handles[s] = (amdsmi_processor_handle *)papi_calloc(core_count, sizeof(amdsmi_processor_handle));
      if (!cpu_core_handles[s]) {
        papi_errno = PAPI_ENOMEM;
        sprintf(error_string, "Memory allocation error for CPU core handles on socket %u.", s);
        for (uint32_t t = 0; t < s; ++t) {
          if (cpu_core_handles[t])
            papi_free(cpu_core_handles[t]);
        }
        papi_free(cpu_core_handles);
        papi_free(cores_per_socket);
        goto fn_fail;
      }
      st = amdsmi_get_processor_handles_by_type_p(device_handles[gpu_count + s], AMDSMI_PROCESSOR_TYPE_AMD_CPU_CORE, cpu_core_handles[s],
                                                  &core_count);
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
      if (cpu_core_handles[s])
        papi_free(cpu_core_handles[s]);
    }
    papi_free(cpu_core_handles);
    cpu_core_handles = NULL;
  }
  if (cores_per_socket) {
    papi_free(cores_per_socket);
    cores_per_socket = NULL;
  }
  amdsmi_shut_down_p();
  return papi_errno;
}
int amds_shutdown(void) {
  shutdown_event_table();
  shutdown_device_table();
  htable_shutdown(htable);
 
  return amdsmi_shut_down_p();
}
int amds_err_get_last(const char **err_string) {
  if (err_string)
    *err_string = error_string;
  return PAPI_OK;
}
/* Initialize native event table: enumerate all supported events */
static int init_event_table(void) {
  // Check if event table is already initialized
  if (ntv_table.count > 0 && ntv_table.events != NULL) {
    return PAPI_OK; // Already initialized, skip expensive rebuild
  }
  ntv_table.count = 0;
  int idx = 0;
  // Safety check - if no devices, return early
  if (device_count <= 0) {
    ntv_table.events = NULL;
    return PAPI_OK;
  }
  // Keep original allocation approach
  ntv_table.events = (native_event_t *)papi_calloc(MAX_EVENTS_PER_DEVICE * device_count, sizeof(native_event_t));
  if (!ntv_table.events) {
    return PAPI_ENOMEM;
  }
  char name_buf[PAPI_MAX_STR_LEN];
  char descr_buf[PAPI_MAX_STR_LEN];
  // Define sensor arrays first
  amdsmi_temperature_type_t temp_sensors[] = {
      AMDSMI_TEMPERATURE_TYPE_EDGE,  AMDSMI_TEMPERATURE_TYPE_JUNCTION, AMDSMI_TEMPERATURE_TYPE_VRAM,  AMDSMI_TEMPERATURE_TYPE_HBM_0,
      AMDSMI_TEMPERATURE_TYPE_HBM_1, AMDSMI_TEMPERATURE_TYPE_HBM_2,    AMDSMI_TEMPERATURE_TYPE_HBM_3, AMDSMI_TEMPERATURE_TYPE_PLX};
  const int num_temp_sensors = sizeof(temp_sensors) / sizeof(temp_sensors[0]);
  const amdsmi_temperature_metric_t temp_metrics[] = {
      AMDSMI_TEMP_CURRENT,       AMDSMI_TEMP_MAX,           AMDSMI_TEMP_MIN,       AMDSMI_TEMP_MAX_HYST,       AMDSMI_TEMP_MIN_HYST,
      AMDSMI_TEMP_CRITICAL,      AMDSMI_TEMP_CRITICAL_HYST, AMDSMI_TEMP_EMERGENCY, AMDSMI_TEMP_EMERGENCY_HYST, AMDSMI_TEMP_CRIT_MIN,
      AMDSMI_TEMP_CRIT_MIN_HYST, AMDSMI_TEMP_OFFSET,        AMDSMI_TEMP_LOWEST,    AMDSMI_TEMP_HIGHEST};
  const char *temp_metric_names[] = {"temp_current",       "temp_max",           "temp_min",       "temp_max_hyst",       "temp_min_hyst",
                                     "temp_critical",      "temp_critical_hyst", "temp_emergency", "temp_emergency_hyst", "temp_crit_min",
                                     "temp_crit_min_hyst", "temp_offset",        "temp_lowest",    "temp_highest"};
  /* Temperature sensors - device-level cache + individual testing */
  for (int d = 0; d < gpu_count; ++d) {
    // Safety check for device handle
    if (!device_handles || !device_handles[d]) {
      continue;
    }
    
    
    
        // GPU cache info events
    if (amdsmi_get_gpu_cache_info_p) {
      amdsmi_gpu_cache_info_t cache_info;
      if (amdsmi_get_gpu_cache_info_p(device_handles[d], &cache_info) == AMDSMI_STATUS_SUCCESS) {
        for (uint32_t i = 0; i < cache_info.num_cache_types; ++i) {
          if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
            papi_free(ntv_table.events);
            return PAPI_ENOSUPP;
          }
          uint32_t level = cache_info.cache[i].cache_level;
          uint32_t prop = cache_info.cache[i].cache_properties;
          char type_str[8] = "cache";
          if ((prop & AMDSMI_CACHE_PROPERTY_INST_CACHE) && !(prop & AMDSMI_CACHE_PROPERTY_DATA_CACHE)) {
            strcpy(type_str, "icache");
          } else if ((prop & AMDSMI_CACHE_PROPERTY_DATA_CACHE) && !(prop & AMDSMI_CACHE_PROPERTY_INST_CACHE)) {
            strcpy(type_str, "dcache");
          } else {
            strcpy(type_str, "cache");
          }
          snprintf(name_buf, sizeof(name_buf), "L%u_%s_size:device=%d", level, type_str, d);
          snprintf(descr_buf, sizeof(descr_buf), "Device %d L%u %s size (bytes)", d, level, (strcmp(type_str, "cache")==0 ? "cache" : (strcmp(type_str, "icache")==0 ? "instruction cache" : "data cache")));
          native_event_t *ev_cache = &ntv_table.events[idx];
          ev_cache->id = idx;
          ev_cache->name = strdup(name_buf);
          ev_cache->descr = strdup(descr_buf);
          ev_cache->device = d;
          ev_cache->value = 0;
          ev_cache->mode = PAPI_MODE_READ;
          ev_cache->variant = 0;
          ev_cache->subvariant = i;
          ev_cache->open_func = open_simple;
          ev_cache->close_func = close_simple;
          ev_cache->start_func = start_simple;
          ev_cache->stop_func = stop_simple;
          ev_cache->access_func = access_amdsmi_cache_stat;
          htable_insert(htable, ev_cache->name, ev_cache);
          idx++;

          if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
          snprintf(name_buf, sizeof(name_buf), "L%u_%s_cu_shared:device=%d", level, type_str, d);
          snprintf(descr_buf, sizeof(descr_buf), "Device %d L%u %s max CUs sharing", d, level, type_str);
          native_event_t *ev_cache_cu = &ntv_table.events[idx];
          ev_cache_cu->id = idx;
          ev_cache_cu->name = strdup(name_buf);
          ev_cache_cu->descr = strdup(descr_buf);
          ev_cache_cu->device = d;
          ev_cache_cu->value = 0;
          ev_cache_cu->mode = PAPI_MODE_READ;
          ev_cache_cu->variant = 1;
          ev_cache_cu->subvariant = i;
          ev_cache_cu->open_func = open_simple;
          ev_cache_cu->close_func = close_simple;
          ev_cache_cu->start_func = start_simple;
          ev_cache_cu->stop_func = stop_simple;
          ev_cache_cu->access_func = access_amdsmi_cache_stat;
          htable_insert(htable, ev_cache_cu->name, ev_cache_cu);
          idx++;

          if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
          snprintf(name_buf, sizeof(name_buf), "L%u_%s_instances:device=%d", level, type_str, d);
          snprintf(descr_buf, sizeof(descr_buf), "Device %d L%u %s instances", d, level, type_str);
          native_event_t *ev_cache_inst = &ntv_table.events[idx];
          ev_cache_inst->id = idx;
          ev_cache_inst->name = strdup(name_buf);
          ev_cache_inst->descr = strdup(descr_buf);
          ev_cache_inst->device = d;
          ev_cache_inst->value = 0;
          ev_cache_inst->mode = PAPI_MODE_READ;
          ev_cache_inst->variant = 2;
          ev_cache_inst->subvariant = i;
          ev_cache_inst->open_func = open_simple;
          ev_cache_inst->close_func = close_simple;
          ev_cache_inst->start_func = start_simple;
          ev_cache_inst->stop_func = stop_simple;
          ev_cache_inst->access_func = access_amdsmi_cache_stat;
          htable_insert(htable, ev_cache_inst->name, ev_cache_inst);
          idx++;
        }
      }
    }
    // GPU VRAM info events
    if (amdsmi_get_gpu_vram_info_p) {
      amdsmi_vram_info_t vram_info;
      if (amdsmi_get_gpu_vram_info_p(device_handles[d], &vram_info) == AMDSMI_STATUS_SUCCESS) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "vram_bus_width:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d VRAM bus width (bits)", d);
        native_event_t *ev_vram_width = &ntv_table.events[idx];
        ev_vram_width->id = idx;
        ev_vram_width->name = strdup(name_buf);
        ev_vram_width->descr = strdup(descr_buf);
        ev_vram_width->device = d;
        ev_vram_width->value = 0;
        ev_vram_width->mode = PAPI_MODE_READ;
        ev_vram_width->variant = 0;
        ev_vram_width->subvariant = 0;
        ev_vram_width->open_func = open_simple;
        ev_vram_width->close_func = close_simple;
        ev_vram_width->start_func = start_simple;
        ev_vram_width->stop_func = stop_simple;
        ev_vram_width->access_func = access_amdsmi_vram_width;
        htable_insert(htable, ev_vram_width->name, ev_vram_width);
        idx++;

        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "vram_size_bytes:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d VRAM size (bytes)", d);
        native_event_t *ev_vram_size = &ntv_table.events[idx];
        ev_vram_size->id = idx;
        ev_vram_size->name = strdup(name_buf);
        ev_vram_size->descr = strdup(descr_buf);
        ev_vram_size->device = d;
        ev_vram_size->value = 0;
        ev_vram_size->mode = PAPI_MODE_READ;
        ev_vram_size->variant = 0;
        ev_vram_size->subvariant = 0;
        ev_vram_size->open_func = open_simple;
        ev_vram_size->close_func = close_simple;
        ev_vram_size->start_func = start_simple;
        ev_vram_size->stop_func = stop_simple;
        ev_vram_size->access_func = access_amdsmi_vram_size;
        htable_insert(htable, ev_vram_size->name, ev_vram_size);
        idx++;

        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "vram_type:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d VRAM type id", d);
        native_event_t *ev_vram_type = &ntv_table.events[idx];
        ev_vram_type->id = idx;
        ev_vram_type->name = strdup(name_buf);
        ev_vram_type->descr = strdup(descr_buf);
        ev_vram_type->device = d;
        ev_vram_type->value = 0;
        ev_vram_type->mode = PAPI_MODE_READ;
        ev_vram_type->variant = 0;
        ev_vram_type->subvariant = 0;
        ev_vram_type->open_func = open_simple;
        ev_vram_type->close_func = close_simple;
        ev_vram_type->start_func = start_simple;
        ev_vram_type->stop_func = stop_simple;
        ev_vram_type->access_func = access_amdsmi_vram_type;
        htable_insert(htable, ev_vram_type->name, ev_vram_type);
        idx++;

        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "vram_vendor_id:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d VRAM vendor id", d);
        native_event_t *ev_vram_vendor = &ntv_table.events[idx];
        ev_vram_vendor->id = idx;
        ev_vram_vendor->name = strdup(name_buf);
        ev_vram_vendor->descr = strdup(descr_buf);
        ev_vram_vendor->device = d;
        ev_vram_vendor->value = 0;
        ev_vram_vendor->mode = PAPI_MODE_READ;
        ev_vram_vendor->variant = 0;
        ev_vram_vendor->subvariant = 0;
        ev_vram_vendor->open_func = open_simple;
        ev_vram_vendor->close_func = close_simple;
        ev_vram_vendor->start_func = start_simple;
        ev_vram_vendor->stop_func = stop_simple;
        ev_vram_vendor->access_func = access_amdsmi_vram_vendor;
        htable_insert(htable, ev_vram_vendor->name, ev_vram_vendor);
        idx++;
      }
    }
    // GPU Overdrive level events
    if (amdsmi_get_gpu_overdrive_level_p) {
      uint32_t od_val;
      if (amdsmi_get_gpu_overdrive_level_p(device_handles[d], &od_val) == AMDSMI_STATUS_SUCCESS) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "gpu_overdrive_percent:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d GPU core clock overdrive (%%)", d);
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
        ev_od->access_func = access_amdsmi_overdrive_level;
        htable_insert(htable, ev_od->name, ev_od);
        idx++;
      }
    }
    if (amdsmi_get_gpu_mem_overdrive_level_p) {
      uint32_t od_val;
      if (amdsmi_get_gpu_mem_overdrive_level_p(device_handles[d], &od_val) == AMDSMI_STATUS_SUCCESS) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "gpu_mem_overdrive_percent:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d GPU memory clock overdrive (%%)", d);
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
        ev_mod->access_func = access_amdsmi_mem_overdrive_level;
        htable_insert(htable, ev_mod->name, ev_mod);
        idx++;
      }
    }
    // GPU performance level event
    if (amdsmi_get_gpu_perf_level_p) {
      amdsmi_dev_perf_level_t perf;
      if (amdsmi_get_gpu_perf_level_p(device_handles[d], &perf) == AMDSMI_STATUS_SUCCESS) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "perf_level:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d current performance level", d);
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
    }
    
    // GPU PM metrics count event
    if (amdsmi_get_gpu_pm_metrics_info_p) {
      amdsmi_name_value_t *metrics = NULL;
      uint32_t mcount = 0;

      int saved_stderr = silence_stderr_begin();
      amdsmi_status_t st = amdsmi_get_gpu_pm_metrics_info_p(device_handles[d], &metrics, &mcount);
      silence_stderr_end(saved_stderr);

      if (st == AMDSMI_STATUS_SUCCESS && mcount > 0) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { if (metrics) papi_free(metrics); papi_free(ntv_table.events); return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "pm_metrics_count:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d number of PM metrics available", d);
        native_event_t *ev_pmcount = &ntv_table.events[idx];
        ev_pmcount->id = idx;
        ev_pmcount->name = strdup(name_buf);
        ev_pmcount->descr = strdup(descr_buf);
        ev_pmcount->device = d;
        ev_pmcount->value = 0;
        ev_pmcount->mode = PAPI_MODE_READ;
        ev_pmcount->variant = 0;
        ev_pmcount->subvariant = 0;
        ev_pmcount->open_func = open_simple;
        ev_pmcount->close_func = close_simple;
        ev_pmcount->start_func = start_simple;
        ev_pmcount->stop_func = stop_simple;
        ev_pmcount->access_func = access_amdsmi_pm_metrics_count;
        htable_insert(htable, ev_pmcount->name, ev_pmcount);
        idx++;

        for (uint32_t i = 0; i < mcount; ++i) {
          if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(metrics); papi_free(ntv_table.events); return PAPI_ENOSUPP; }
          char metric_name[MAX_AMDSMI_NAME_LENGTH];
          sanitize_name(metrics[i].name, metric_name, sizeof(metric_name));
          snprintf(name_buf, sizeof(name_buf), "pm_%s:device=%d", metric_name, d);
          snprintf(descr_buf, sizeof(descr_buf), "Device %d PM metric %s", d, metrics[i].name);
          native_event_t *ev_pm = &ntv_table.events[idx];
          ev_pm->id = idx;
          ev_pm->name = strdup(name_buf);
          ev_pm->descr = strdup(descr_buf);
          ev_pm->device = d;
          ev_pm->value = 0;
          ev_pm->mode = PAPI_MODE_READ;
          ev_pm->variant = i;
          ev_pm->subvariant = 0;
          ev_pm->open_func = open_simple;
          ev_pm->close_func = close_simple;
          ev_pm->start_func = start_simple;
          ev_pm->stop_func = stop_simple;
          ev_pm->access_func = access_amdsmi_pm_metric_value;
          htable_insert(htable, ev_pm->name, ev_pm);
          idx++;
        }
      }
      if (metrics) papi_free(metrics);
    }
    // GPU RAS feature (ECC schema) event
    if (amdsmi_get_gpu_ras_feature_info_p) {
      amdsmi_ras_feature_t ras;
      if (amdsmi_get_gpu_ras_feature_info_p(device_handles[d], &ras) == AMDSMI_STATUS_SUCCESS) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "ecc_correction_mask:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d ECC correction features mask", d);
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
        ev_ras->access_func = access_amdsmi_ras_ecc_schema;
        htable_insert(htable, ev_ras->name, ev_ras);
        idx++;

        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "ras_eeprom_version:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d RAS EEPROM version", d);
        native_event_t *ev_ras_ver = &ntv_table.events[idx];
        ev_ras_ver->id = idx;
        ev_ras_ver->name = strdup(name_buf);
        ev_ras_ver->descr = strdup(descr_buf);
        ev_ras_ver->device = d;
        ev_ras_ver->value = 0;
        ev_ras_ver->mode = PAPI_MODE_READ;
        ev_ras_ver->variant = 0;
        ev_ras_ver->subvariant = 0;
        ev_ras_ver->open_func = open_simple;
        ev_ras_ver->close_func = close_simple;
        ev_ras_ver->start_func = start_simple;
        ev_ras_ver->stop_func = stop_simple;
        ev_ras_ver->access_func = access_amdsmi_ras_eeprom_version;
        htable_insert(htable, ev_ras_ver->name, ev_ras_ver);
        idx++;
      }
    }
    if (amdsmi_get_gpu_ras_block_features_enabled_p) {
      amdsmi_gpu_block_t blocks[] = {
          AMDSMI_GPU_BLOCK_UMC,      AMDSMI_GPU_BLOCK_SDMA,    AMDSMI_GPU_BLOCK_GFX,
          AMDSMI_GPU_BLOCK_MMHUB,    AMDSMI_GPU_BLOCK_ATHUB,   AMDSMI_GPU_BLOCK_PCIE_BIF,
          AMDSMI_GPU_BLOCK_HDP,      AMDSMI_GPU_BLOCK_XGMI_WAFL, AMDSMI_GPU_BLOCK_DF,
          AMDSMI_GPU_BLOCK_SMN,      AMDSMI_GPU_BLOCK_SEM,     AMDSMI_GPU_BLOCK_MP0,
          AMDSMI_GPU_BLOCK_MP1,      AMDSMI_GPU_BLOCK_FUSE,    AMDSMI_GPU_BLOCK_MCA,
          AMDSMI_GPU_BLOCK_VCN,      AMDSMI_GPU_BLOCK_JPEG,    AMDSMI_GPU_BLOCK_IH,
          AMDSMI_GPU_BLOCK_MPIO};
      const char *block_names[] = {
          "umc", "sdma", "gfx", "mmhub", "athub", "pcie_bif", "hdp", "xgmi_wafl",
          "df",  "smn",  "sem", "mp0",  "mp1",  "fuse",    "mca", "vcn",
          "jpeg", "ih",  "mpio"};
      size_t nb = sizeof(blocks) / sizeof(blocks[0]);
      for (size_t bi = 0; bi < nb; ++bi) {
        amdsmi_ras_err_state_t st;
        if (amdsmi_get_gpu_ras_block_features_enabled_p(device_handles[d], blocks[bi], &st) == AMDSMI_STATUS_SUCCESS) {
          if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
          snprintf(name_buf, sizeof(name_buf), "ras_block_%s_state:device=%d", block_names[bi], d);
          snprintf(descr_buf, sizeof(descr_buf), "Device %d RAS state for %s block", d, block_names[bi]);
          native_event_t *ev_blk = &ntv_table.events[idx];
          ev_blk->id = idx;
          ev_blk->name = strdup(name_buf);
          ev_blk->descr = strdup(descr_buf);
          ev_blk->device = d;
          ev_blk->value = 0;
          ev_blk->mode = PAPI_MODE_READ;
          ev_blk->variant = (uint32_t)blocks[bi];
          ev_blk->subvariant = 0;
          ev_blk->open_func = open_simple;
          ev_blk->close_func = close_simple;
          ev_blk->start_func = start_simple;
          ev_blk->stop_func = stop_simple;
          ev_blk->access_func = access_amdsmi_ras_block_state;
          htable_insert(htable, ev_blk->name, ev_blk);
          idx++;
        }
      }
    }
    // GPU voltage metrics events
    if (amdsmi_get_gpu_volt_metric_p) {
      // Sensor 0: VDDGFX, Sensor 1: VDDBOARD
      int sensors[2] = {0, 1};
      const char *sensor_names[2] = {"vddgfx", "vddboard"};
      for (int si = 0; si < 2; ++si) {
        int64_t volt_val = 0;
        amdsmi_status_t st = amdsmi_get_gpu_volt_metric_p(device_handles[d], (amdsmi_voltage_type_t)sensors[si], AMDSMI_VOLT_CURRENT, &volt_val);
        if (st == AMDSMI_STATUS_SUCCESS) {
          if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
            papi_free(ntv_table.events);
            return PAPI_ENOSUPP;
          }
          snprintf(name_buf, sizeof(name_buf), "voltage_%s:device=%d", sensor_names[si], d);
          snprintf(descr_buf, sizeof(descr_buf), "Device %d %s voltage (mV)", d, sensor_names[si]);
          native_event_t *ev_volt = &ntv_table.events[idx];
          ev_volt->id = idx;
          ev_volt->name = strdup(name_buf);
          ev_volt->descr = strdup(descr_buf);
          ev_volt->device = d;
          ev_volt->value = 0;
          ev_volt->mode = PAPI_MODE_READ;
          ev_volt->variant = AMDSMI_VOLT_CURRENT;
          ev_volt->subvariant = sensors[si];
          ev_volt->open_func = open_simple;
          ev_volt->close_func = close_simple;
          ev_volt->start_func = start_simple;
          ev_volt->stop_func = stop_simple;
          ev_volt->access_func = access_amdsmi_voltage;
          htable_insert(htable, ev_volt->name, ev_volt);
          idx++;
        }
      }
    }
    // GPU OD voltage curve regions count event
    if (amdsmi_get_gpu_od_volt_curve_regions_p) {
      uint32_t num_regions = 0;
      amdsmi_freq_volt_region_t *buf = NULL;
      // Try once with small buffer to check support
      buf = (amdsmi_freq_volt_region_t *)papi_calloc(4, sizeof(amdsmi_freq_volt_region_t));
      if (buf) {
        num_regions = 4;
        amdsmi_status_t st = amdsmi_get_gpu_od_volt_curve_regions_p(device_handles[d], &num_regions, buf);
        papi_free(buf);
        if (st == AMDSMI_STATUS_SUCCESS) {
          if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
            papi_free(ntv_table.events);
            return PAPI_ENOSUPP;
          }
          snprintf(name_buf, sizeof(name_buf), "volt_curve_regions:device=%d", d);
          snprintf(descr_buf, sizeof(descr_buf), "Device %d number of voltage curve regions", d);
          native_event_t *ev_vcr = &ntv_table.events[idx];
          ev_vcr->id = idx;
          ev_vcr->name = strdup(name_buf);
          ev_vcr->descr = strdup(descr_buf);
          ev_vcr->device = d;
          ev_vcr->value = 0;
          ev_vcr->mode = PAPI_MODE_READ;
          ev_vcr->variant = 0;
          ev_vcr->subvariant = 0;
          ev_vcr->open_func = open_simple;
          ev_vcr->close_func = close_simple;
          ev_vcr->start_func = start_simple;
          ev_vcr->stop_func = stop_simple;
          ev_vcr->access_func = access_amdsmi_od_volt_regions_count;
          htable_insert(htable, ev_vcr->name, ev_vcr);
          idx++;
        }
      }
    }
    // GPU SoC P-state policy event
    if (amdsmi_get_soc_pstate_p) {
      amdsmi_dpm_policy_t policy;
      if (amdsmi_get_soc_pstate_p(device_handles[d], &policy) == AMDSMI_STATUS_SUCCESS && policy.num_supported > 0) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "soc_pstate_policy:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d current SoC P-state policy id", d);
        native_event_t *ev_soc = &ntv_table.events[idx];
        ev_soc->id = idx;
        ev_soc->name = strdup(name_buf);
        ev_soc->descr = strdup(descr_buf);
        ev_soc->device = d;
        ev_soc->value = 0;
        ev_soc->mode = PAPI_MODE_READ;
        ev_soc->variant = 0;
        ev_soc->subvariant = 0;
        ev_soc->open_func = open_simple;
        ev_soc->close_func = close_simple;
        ev_soc->start_func = start_simple;
        ev_soc->stop_func = stop_simple;
        ev_soc->access_func = access_amdsmi_soc_pstate_id;
        htable_insert(htable, ev_soc->name, ev_soc);
        idx++;
      }
    }
    // GPU XGMI PLPD policy events
    if (amdsmi_get_xgmi_plpd_p) {
      amdsmi_dpm_policy_t policy;
      if (amdsmi_get_xgmi_plpd_p(device_handles[d], &policy) == AMDSMI_STATUS_SUCCESS && policy.num_supported > 0) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "xgmi_plpd:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d current XGMI PLPD policy id", d);
        native_event_t *ev_xplpd = &ntv_table.events[idx];
        ev_xplpd->id = idx;
        ev_xplpd->name = strdup(name_buf);
        ev_xplpd->descr = strdup(descr_buf);
        ev_xplpd->device = d;
        ev_xplpd->value = 0;
        ev_xplpd->mode = PAPI_MODE_READ;
        ev_xplpd->variant = 0;
        ev_xplpd->subvariant = 0;
        ev_xplpd->open_func = open_simple;
        ev_xplpd->close_func = close_simple;
        ev_xplpd->start_func = start_simple;
        ev_xplpd->stop_func = stop_simple;
        ev_xplpd->access_func = access_amdsmi_xgmi_plpd_id;
        htable_insert(htable, ev_xplpd->name, ev_xplpd);
        idx++;
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "xgmi_plpd_supported:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d supported XGMI PLPD policy count", d);
        native_event_t *ev_xplpd_sup = &ntv_table.events[idx];
        ev_xplpd_sup->id = idx;
        ev_xplpd_sup->name = strdup(name_buf);
        ev_xplpd_sup->descr = strdup(descr_buf);
        ev_xplpd_sup->device = d;
        ev_xplpd_sup->value = 0;
        ev_xplpd_sup->mode = PAPI_MODE_READ;
        ev_xplpd_sup->variant = 0;
        ev_xplpd_sup->subvariant = 0;
        ev_xplpd_sup->open_func = open_simple;
        ev_xplpd_sup->close_func = close_simple;
        ev_xplpd_sup->start_func = start_simple;
        ev_xplpd_sup->stop_func = stop_simple;
        ev_xplpd_sup->access_func = access_amdsmi_xgmi_plpd_supported;
        htable_insert(htable, ev_xplpd_sup->name, ev_xplpd_sup);
        idx++;
      }
    }
    // GPU register table metrics count events
    if (amdsmi_get_gpu_reg_table_info_p) {
      amdsmi_reg_type_t reg_types[] = {AMDSMI_REG_XGMI, AMDSMI_REG_WAFL, AMDSMI_REG_PCIE, AMDSMI_REG_USR, AMDSMI_REG_USR1};
      const char *reg_names[] = {"XGMI", "WAFL", "PCIE", "USR", "USR1"};
      for (int rt = 0; rt < 5; ++rt) {
        amdsmi_name_value_t *reg_metrics = NULL;
        uint32_t num_metrics = 0;

        int saved_stderr = silence_stderr_begin();
        amdsmi_status_t st = amdsmi_get_gpu_reg_table_info_p(device_handles[d], reg_types[rt], &reg_metrics, &num_metrics);
        silence_stderr_end(saved_stderr);

        if (st == AMDSMI_STATUS_SUCCESS && num_metrics > 0) {
          if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { if (reg_metrics) papi_free(reg_metrics); papi_free(ntv_table.events); return PAPI_ENOSUPP; }
          snprintf(name_buf, sizeof(name_buf), "reg_%s_count:device=%d", reg_names[rt], d);
          snprintf(descr_buf, sizeof(descr_buf), "Device %d number of %s register metrics", d, reg_names[rt]);
          native_event_t *ev_reg = &ntv_table.events[idx];
          ev_reg->id = idx;
          ev_reg->name = strdup(name_buf);
          ev_reg->descr = strdup(descr_buf);
          ev_reg->device = d;
          ev_reg->value = 0;
          ev_reg->mode = PAPI_MODE_READ;
          ev_reg->variant = (uint32_t)reg_types[rt];
          ev_reg->subvariant = 0;
          ev_reg->open_func = open_simple;
          ev_reg->close_func = close_simple;
          ev_reg->start_func = start_simple;
          ev_reg->stop_func = stop_simple;
          ev_reg->access_func = access_amdsmi_reg_count;
          htable_insert(htable, ev_reg->name, ev_reg);
          idx++;

          for (uint32_t i = 0; i < num_metrics; ++i) {
            if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { if (reg_metrics) papi_free(reg_metrics); papi_free(ntv_table.events); return PAPI_ENOSUPP; }
            char reg_metric_name[MAX_AMDSMI_NAME_LENGTH];
            sanitize_name(reg_metrics[i].name, reg_metric_name, sizeof(reg_metric_name));
            snprintf(name_buf, sizeof(name_buf), "reg_%s_%s:device=%d", reg_names[rt], reg_metric_name, d);
            snprintf(descr_buf, sizeof(descr_buf), "Device %d %s register %s", d, reg_names[rt], reg_metrics[i].name);
            native_event_t *ev_reg_val = &ntv_table.events[idx];
            ev_reg_val->id = idx;
            ev_reg_val->name = strdup(name_buf);
            ev_reg_val->descr = strdup(descr_buf);
            ev_reg_val->device = d;
            ev_reg_val->value = 0;
            ev_reg_val->mode = PAPI_MODE_READ;
            ev_reg_val->variant = (uint32_t)reg_types[rt];
            ev_reg_val->subvariant = i;
            ev_reg_val->open_func = open_simple;
            ev_reg_val->close_func = close_simple;
            ev_reg_val->start_func = start_simple;
            ev_reg_val->stop_func = stop_simple;
            ev_reg_val->access_func = access_amdsmi_reg_value;
            htable_insert(htable, ev_reg_val->name, ev_reg_val);
            idx++;
          }
        }
        if (reg_metrics) papi_free(reg_metrics);
      }
    }
    

    
    for (int si = 0; si < num_temp_sensors && si < 8; ++si) {
      // Test each sensor individually first
      int64_t sensor_test_val;
      if (!amdsmi_get_temp_metric_p ||
          amdsmi_get_temp_metric_p(device_handles[d], temp_sensors[si], AMDSMI_TEMP_CURRENT, &sensor_test_val) != AMDSMI_STATUS_SUCCESS) {
        continue; // Skip this specific sensor if it doesn't work
      }
      // Register metrics for this working sensor, testing each metric
      // individually
      for (size_t mi = 0; mi < sizeof(temp_metrics) / sizeof(temp_metrics[0]); ++mi) {
        // Bounds check to prevent buffer overflow
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          return PAPI_ENOSUPP; // Too many events
        }
        // Test this specific metric on this specific sensor
        int64_t metric_val;
        if (amdsmi_get_temp_metric_p(device_handles[d], temp_sensors[si], temp_metrics[mi], &metric_val) != AMDSMI_STATUS_SUCCESS) {
          continue; /* skip this specific metric if not supported */
        }
        snprintf(name_buf, sizeof(name_buf), "%s:device=%d:sensor=%d", temp_metric_names[mi], d, (int)temp_sensors[si]);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d %s for sensor %d", d, temp_metric_names[mi], (int)temp_sensors[si]);
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
    if (amdsmi_get_gpu_fan_rpms_p && amdsmi_get_gpu_fan_rpms_p(device_handles[d], 0, &dummy_rpm) == AMDSMI_STATUS_SUCCESS) {
      if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
        return PAPI_ENOSUPP;
      }
      snprintf(name_buf, sizeof(name_buf), "fan_rpms:device=%d:sensor=0", d);
      snprintf(descr_buf, sizeof(descr_buf), "Device %d fan speed in RPM", d);
      native_event_t *ev_rpm = &ntv_table.events[idx];
      ev_rpm->id = idx;
      ev_rpm->name = strdup(name_buf);
      ev_rpm->descr = strdup(descr_buf);
      if (!ev_rpm->name || !ev_rpm->descr) {
        return PAPI_ENOMEM;
      }
      ev_rpm->device = d;
      ev_rpm->value = 0;
      ev_rpm->mode = PAPI_MODE_READ;
      ev_rpm->variant = 0;
      ev_rpm->subvariant = 0;
      ev_rpm->open_func = open_simple;
      ev_rpm->close_func = close_simple;
      ev_rpm->start_func = start_simple;
      ev_rpm->stop_func = stop_simple;
      ev_rpm->access_func = access_amdsmi_fan_rpms;
      htable_insert(htable, ev_rpm->name, ev_rpm);
      idx++;
    }
    /* Register Fan SPEED if available */
    int64_t dummy_speed;
    if (amdsmi_get_gpu_fan_speed_p && amdsmi_get_gpu_fan_speed_p(device_handles[d], 0, &dummy_speed) == AMDSMI_STATUS_SUCCESS) {
      if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
        return PAPI_ENOSUPP;
      }
      snprintf(name_buf, sizeof(name_buf), "fan_speed:device=%d:sensor=0", d);
      snprintf(descr_buf, sizeof(descr_buf), "Device %d fan speed (0-255 relative)", d);
      native_event_t *ev_fan = &ntv_table.events[idx];
      ev_fan->id = idx;
      ev_fan->name = strdup(name_buf);
      ev_fan->descr = strdup(descr_buf);
      if (!ev_fan->name || !ev_fan->descr) {
        return PAPI_ENOMEM;
      }
      ev_fan->device = d;
      ev_fan->value = 0;
      ev_fan->mode = PAPI_MODE_READ;
      ev_fan->variant = 0;
      ev_fan->subvariant = 0;
      ev_fan->open_func = open_simple;
      ev_fan->close_func = close_simple;
      ev_fan->start_func = start_simple;
      ev_fan->stop_func = stop_simple;
      ev_fan->access_func = access_amdsmi_fan_speed;
      htable_insert(htable, ev_fan->name, ev_fan);
      idx++;
    }
    /* Register Fan Max Speed - always probe directly */
    int64_t dummy_max;
    if (amdsmi_get_gpu_fan_speed_max_p && amdsmi_get_gpu_fan_speed_max_p(device_handles[d], 0, &dummy_max) == AMDSMI_STATUS_SUCCESS) {
      if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
        return PAPI_ENOSUPP;
      }
      snprintf(name_buf, sizeof(name_buf), "fan_rpms_max:device=%d:sensor=0", d);
      snprintf(descr_buf, sizeof(descr_buf), "Device %d fan maximum speed in RPM", d);
      native_event_t *ev_fanmax = &ntv_table.events[idx];
      ev_fanmax->id = idx;
      ev_fanmax->name = strdup(name_buf);
      ev_fanmax->descr = strdup(descr_buf);
      if (!ev_fanmax->name || !ev_fanmax->descr) {
        return PAPI_ENOMEM;
      }
      ev_fanmax->device = d;
      ev_fanmax->value = 0;
      ev_fanmax->mode = PAPI_MODE_READ;
      ev_fanmax->variant = 0;
      ev_fanmax->subvariant = 0;
      ev_fanmax->open_func = open_simple;
      ev_fanmax->close_func = close_simple;
      ev_fanmax->start_func = start_simple;
      ev_fanmax->stop_func = stop_simple;
      ev_fanmax->access_func = access_amdsmi_fan_speed_max;
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
      snprintf(name_buf, sizeof(name_buf), "mem_total_VRAM:device=%d", d);
      snprintf(descr_buf, sizeof(descr_buf), "Device %d total VRAM memory (bytes)", d);
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
      ev->variant = AMDSMI_MEM_TYPE_VRAM;
      ev->subvariant = 0;
      ev->open_func = open_simple;
      ev->close_func = close_simple;
      ev->start_func = start_simple;
      ev->stop_func = stop_simple;
      ev->access_func = access_amdsmi_mem_total;
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
      snprintf(name_buf, sizeof(name_buf), "mem_usage_VRAM:device=%d", d);
      snprintf(descr_buf, sizeof(descr_buf), "Device %d VRAM memory usage (bytes)", d);
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
      ev->variant = AMDSMI_MEM_TYPE_VRAM;
      ev->subvariant = 0;
      ev->open_func = open_simple;
      ev->close_func = close_simple;
      ev->start_func = start_simple;
      ev->stop_func = stop_simple;
      ev->access_func = access_amdsmi_mem_usage;
      htable_insert(htable, ev->name, ev);
      ++idx;
    }
    if (amdsmi_get_gpu_vram_usage_p) {
      amdsmi_vram_usage_t vu;
      if (amdsmi_get_gpu_vram_usage_p(device_handles[d], &vu) == AMDSMI_STATUS_SUCCESS) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "vram_total_mb:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d total VRAM (MB)", d);
        native_event_t *ev_tot = &ntv_table.events[idx];
        ev_tot->id = idx;
        ev_tot->name = strdup(name_buf);
        ev_tot->descr = strdup(descr_buf);
        ev_tot->device = d;
        ev_tot->value = 0;
        ev_tot->mode = PAPI_MODE_READ;
        ev_tot->variant = 0;
        ev_tot->subvariant = 0;
        ev_tot->open_func = open_simple;
        ev_tot->close_func = close_simple;
        ev_tot->start_func = start_simple;
        ev_tot->stop_func = stop_simple;
        ev_tot->access_func = access_amdsmi_vram_usage;
        htable_insert(htable, ev_tot->name, ev_tot);
        idx++;
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "vram_used_mb:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d used VRAM (MB)", d);
        native_event_t *ev_use = &ntv_table.events[idx];
        ev_use->id = idx;
        ev_use->name = strdup(name_buf);
        ev_use->descr = strdup(descr_buf);
        ev_use->device = d;
        ev_use->value = 0;
        ev_use->mode = PAPI_MODE_READ;
        ev_use->variant = 1;
        ev_use->subvariant = 0;
        ev_use->open_func = open_simple;
        ev_use->close_func = close_simple;
        ev_use->start_func = start_simple;
        ev_use->stop_func = stop_simple;
        ev_use->access_func = access_amdsmi_vram_usage;
        htable_insert(htable, ev_use->name, ev_use);
        idx++;
      }
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
    if (amdsmi_get_power_info_p && amdsmi_get_power_info_p(device_handles[d], &dummy_power) == AMDSMI_STATUS_SUCCESS) {
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
    // Register power cap events (if power cap functions are available) - test
    // directly
    amdsmi_power_cap_info_t dummy_cap_info;
    if (amdsmi_get_power_cap_info_p && amdsmi_get_power_cap_info_p(device_handles[d], 0, &dummy_cap_info) == AMDSMI_STATUS_SUCCESS) {
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
      ev_pcap_min->variant = 1; // variant 1 => min
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
      ev_pcap_max->variant = 2; // variant 2 => max
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
  uint64_t tx = 0, rx = 0, pkt = 0;
  amdsmi_status_t st_thr = amdsmi_get_gpu_pci_throughput_p(device_handles[0], &tx, &rx, &pkt);

  for (int d = 0; d < gpu_count; ++d) {
    if (st_thr == AMDSMI_STATUS_SUCCESS) {
      /* bytes sent per second */
      snprintf(name_buf, sizeof(name_buf), "pci_throughput_sent:device=%d", d);
      snprintf(descr_buf, sizeof(descr_buf), "Device %d PCIe bytes sent per second", d);
      native_event_t *ev_tx = &ntv_table.events[idx];
      ev_tx->id = idx;
      ev_tx->name = strdup(name_buf);
      ev_tx->descr = strdup(descr_buf);
      ev_tx->device = d;
      ev_tx->value = 0;
      ev_tx->mode = PAPI_MODE_READ;
      ev_tx->variant = 0; /* sent */
      ev_tx->subvariant = 0;
      ev_tx->open_func = open_simple;
      ev_tx->close_func = close_simple;
      ev_tx->start_func = start_simple;
      ev_tx->stop_func = stop_simple;
      ev_tx->access_func = access_amdsmi_pci_throughput;
      htable_insert(htable, ev_tx->name, ev_tx);
      ++idx;
      /* bytes received per second */
      snprintf(name_buf, sizeof(name_buf), "pci_throughput_received:device=%d", d);
      snprintf(descr_buf, sizeof(descr_buf), "Device %d PCIe bytes received per second", d);
      native_event_t *ev_rx = &ntv_table.events[idx];
      ev_rx->id = idx;
      ev_rx->name = strdup(name_buf);
      ev_rx->descr = strdup(descr_buf);
      ev_rx->device = d;
      ev_rx->value = 0;
      ev_rx->mode = PAPI_MODE_READ;
      ev_rx->variant = 1; /* received */
      ev_rx->subvariant = 0;
      ev_rx->open_func = open_simple;
      ev_rx->close_func = close_simple;
      ev_rx->start_func = start_simple;
      ev_rx->stop_func = stop_simple;
      ev_rx->access_func = access_amdsmi_pci_throughput;
      htable_insert(htable, ev_rx->name, ev_rx);
      ++idx;
      /* max packet size */
      snprintf(name_buf, sizeof(name_buf), "pci_throughput_max_packet:device=%d", d);
      snprintf(descr_buf, sizeof(descr_buf), "Device %d PCIe max packet size (bytes)", d);
      native_event_t *ev_pkt = &ntv_table.events[idx];
      ev_pkt->id = idx;
      ev_pkt->name = strdup(name_buf);
      ev_pkt->descr = strdup(descr_buf);
      ev_pkt->device = d;
      ev_pkt->value = 0;
      ev_pkt->mode = PAPI_MODE_READ;
      ev_pkt->variant = 2; /* max pkt */
      ev_pkt->subvariant = 0;
      ev_pkt->open_func = open_simple;
      ev_pkt->close_func = close_simple;
      ev_pkt->start_func = start_simple;
      ev_pkt->stop_func = stop_simple;
      ev_pkt->access_func = access_amdsmi_pci_throughput;
      htable_insert(htable, ev_pkt->name, ev_pkt);
      ++idx;
    }
    uint64_t replay = 0;
    if (amdsmi_get_gpu_pci_replay_counter_p(device_handles[d], &replay) == AMDSMI_STATUS_SUCCESS) {
      snprintf(name_buf, sizeof(name_buf), "pci_replay_counter:device=%d", d);
      snprintf(descr_buf, sizeof(descr_buf), "Device %d PCIe replay (NAK) counter", d);
      native_event_t *ev_rep = &ntv_table.events[idx];
      ev_rep->id = idx;
      ev_rep->name = strdup(name_buf);
      ev_rep->descr = strdup(descr_buf);
      ev_rep->device = d;
      ev_rep->value = 0;
      ev_rep->mode = PAPI_MODE_READ;
      ev_rep->variant = 0;
      ev_rep->subvariant = 0;
      ev_rep->open_func = open_simple;
      ev_rep->close_func = close_simple;
      ev_rep->start_func = start_simple;
      ev_rep->stop_func = stop_simple;
      ev_rep->access_func = access_amdsmi_pci_replay_counter;
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
    if (amdsmi_get_gpu_activity_p && amdsmi_get_gpu_activity_p(device_handles[d], &dummy_usage) == AMDSMI_STATUS_SUCCESS) {
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
    // amdsmi_virtualization_mode_t vmode;
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
    // GPU Virtualization Mode
    amdsmi_virtualization_mode_t vmode;
    if (amdsmi_get_gpu_virtualization_mode_p &&
        amdsmi_get_gpu_virtualization_mode_p(device_handles[d], &vmode) == AMDSMI_STATUS_SUCCESS) {
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

    if (amdsmi_get_gpu_process_isolation_p) {
      uint32_t pis = 0;
      if (amdsmi_get_gpu_process_isolation_p(device_handles[d], &pis) == AMDSMI_STATUS_SUCCESS) {
        snprintf(name_buf, sizeof(name_buf), "process_isolation:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d process isolation status", d);
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

    if (amdsmi_get_gpu_xcd_counter_p) {
      uint16_t xcd = 0;
      if (amdsmi_get_gpu_xcd_counter_p(device_handles[d], &xcd) == AMDSMI_STATUS_SUCCESS) {
        snprintf(name_buf, sizeof(name_buf), "xcd_counter:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d XCD counter", d);
        native_event_t *ev_xcd = &ntv_table.events[idx];
        ev_xcd->id = idx;
        ev_xcd->name = strdup(name_buf);
        ev_xcd->descr = strdup(descr_buf);
        ev_xcd->device = d;
        ev_xcd->value = 0;
        ev_xcd->mode = PAPI_MODE_READ;
        ev_xcd->variant = 0;
        ev_xcd->subvariant = 0;
        ev_xcd->open_func = open_simple;
        ev_xcd->close_func = close_simple;
        ev_xcd->start_func = start_simple;
        ev_xcd->stop_func = stop_simple;
        ev_xcd->access_func = access_amdsmi_xcd_counter;
        htable_insert(htable, ev_xcd->name, ev_xcd);
        idx++;
      }
    }

    if (amdsmi_get_gpu_board_info_p) {
      amdsmi_board_info_t binfo;
      if (amdsmi_get_gpu_board_info_p(device_handles[d], &binfo) == AMDSMI_STATUS_SUCCESS) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "board_serial_hash:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d board serial number (hash)", d);
        native_event_t *ev_brd = &ntv_table.events[idx];
        ev_brd->id = idx; ev_brd->name = strdup(name_buf); ev_brd->descr = strdup(descr_buf);
        ev_brd->device = d; ev_brd->value = 0; ev_brd->mode = PAPI_MODE_READ; ev_brd->variant = 0; ev_brd->subvariant = 0;
        ev_brd->open_func = open_simple; ev_brd->close_func = close_simple; ev_brd->start_func = start_simple; ev_brd->stop_func = stop_simple;
        ev_brd->access_func = access_amdsmi_board_serial_hash;
        htable_insert(htable, ev_brd->name, ev_brd);
        idx++;
      }
    }

    if (amdsmi_get_gpu_vram_info_p) {
      amdsmi_vram_info_t vinfo;
      if (amdsmi_get_gpu_vram_info_p(device_handles[d], &vinfo) == AMDSMI_STATUS_SUCCESS) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "vram_max_bandwidth:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d VRAM max bandwidth (GB/s)", d);
        native_event_t *ev_vbw = &ntv_table.events[idx];
        ev_vbw->id = idx; ev_vbw->name = strdup(name_buf); ev_vbw->descr = strdup(descr_buf);
        ev_vbw->device = d; ev_vbw->value = 0; ev_vbw->mode = PAPI_MODE_READ; ev_vbw->variant = 0; ev_vbw->subvariant = 0;
        ev_vbw->open_func = open_simple; ev_vbw->close_func = close_simple; ev_vbw->start_func = start_simple; ev_vbw->stop_func = stop_simple;
        ev_vbw->access_func = access_amdsmi_vram_max_bandwidth;
        htable_insert(htable, ev_vbw->name, ev_vbw);
        idx++;
      }
    }

    if (amdsmi_get_gpu_bad_page_info_p) {
      uint32_t nump = 0;
      if (amdsmi_get_gpu_bad_page_info_p(device_handles[d], &nump, NULL) == AMDSMI_STATUS_SUCCESS) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "bad_page_count:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d retired page count", d);
        native_event_t *ev_bpc = &ntv_table.events[idx];
        ev_bpc->id = idx; ev_bpc->name = strdup(name_buf); ev_bpc->descr = strdup(descr_buf);
        ev_bpc->device = d; ev_bpc->value = 0; ev_bpc->mode = PAPI_MODE_READ; ev_bpc->variant = 0; ev_bpc->subvariant = 0;
        ev_bpc->open_func = open_simple; ev_bpc->close_func = close_simple; ev_bpc->start_func = start_simple; ev_bpc->stop_func = stop_simple;
        ev_bpc->access_func = access_amdsmi_bad_page_count;
        htable_insert(htable, ev_bpc->name, ev_bpc);
        idx++;
      }
    }

    if (amdsmi_get_gpu_bad_page_threshold_p) {
      uint32_t thr = 0;
      if (amdsmi_get_gpu_bad_page_threshold_p(device_handles[d], &thr) == AMDSMI_STATUS_SUCCESS) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "bad_page_threshold:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d bad page threshold", d);
        native_event_t *ev_bpt = &ntv_table.events[idx];
        ev_bpt->id = idx; ev_bpt->name = strdup(name_buf); ev_bpt->descr = strdup(descr_buf);
        ev_bpt->device = d; ev_bpt->value = 0; ev_bpt->mode = PAPI_MODE_READ; ev_bpt->variant = 0; ev_bpt->subvariant = 0;
        ev_bpt->open_func = open_simple; ev_bpt->close_func = close_simple; ev_bpt->start_func = start_simple; ev_bpt->stop_func = stop_simple;
        ev_bpt->access_func = access_amdsmi_bad_page_threshold;
        htable_insert(htable, ev_bpt->name, ev_bpt);
        idx++;
      }
    }

    if (amdsmi_get_power_info_v2_p) {
      /* Probe for available power sensors. The API uses a sensor index and
       * returns AMDSMI_STATUS_SUCCESS while valid. Iterate until failure to
       * discover all sensors. */
      for (uint32_t s = 0; s < 16; ++s) {
        amdsmi_power_info_t pinfo;
        if (amdsmi_get_power_info_v2_p(device_handles[d], s, &pinfo) != AMDSMI_STATUS_SUCCESS)
          break;

        /* Register current socket power in Watts */
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "power_sensor_current_watts:device=%d:sensor=%u", d, s);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d power sensor %u current socket power (W)", d, s);
        native_event_t *ev_cur = &ntv_table.events[idx];
        ev_cur->id = idx; ev_cur->name = strdup(name_buf); ev_cur->descr = strdup(descr_buf);
        ev_cur->device = d; ev_cur->value = 0; ev_cur->mode = PAPI_MODE_READ; ev_cur->variant = 0; ev_cur->subvariant = s;
        ev_cur->open_func = open_simple; ev_cur->close_func = close_simple; ev_cur->start_func = start_simple; ev_cur->stop_func = stop_simple;
        ev_cur->access_func = access_amdsmi_power_sensor;
        htable_insert(htable, ev_cur->name, ev_cur); idx++;

        /* Register average socket power in Watts */
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "power_sensor_average_watts:device=%d:sensor=%u", d, s);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d power sensor %u average socket power (W)", d, s);
        native_event_t *ev_avg = &ntv_table.events[idx];
        ev_avg->id = idx; ev_avg->name = strdup(name_buf); ev_avg->descr = strdup(descr_buf);
        ev_avg->device = d; ev_avg->value = 0; ev_avg->mode = PAPI_MODE_READ; ev_avg->variant = 1; ev_avg->subvariant = s;
        ev_avg->open_func = open_simple; ev_avg->close_func = close_simple; ev_avg->start_func = start_simple; ev_avg->stop_func = stop_simple;
        ev_avg->access_func = access_amdsmi_power_sensor;
        htable_insert(htable, ev_avg->name, ev_avg); idx++;

        /* Register socket power in microwatts */
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "power_sensor_socket_microwatts:device=%d:sensor=%u", d, s);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d power sensor %u socket power (uW)", d, s);
        native_event_t *ev_sock = &ntv_table.events[idx];
        ev_sock->id = idx; ev_sock->name = strdup(name_buf); ev_sock->descr = strdup(descr_buf);
        ev_sock->device = d; ev_sock->value = 0; ev_sock->mode = PAPI_MODE_READ; ev_sock->variant = 2; ev_sock->subvariant = s;
        ev_sock->open_func = open_simple; ev_sock->close_func = close_simple; ev_sock->start_func = start_simple; ev_sock->stop_func = stop_simple;
        ev_sock->access_func = access_amdsmi_power_sensor;
        htable_insert(htable, ev_sock->name, ev_sock); idx++;

        /* Register GFX voltage */
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "power_sensor_gfx_voltage_mv:device=%d:sensor=%u", d, s);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d power sensor %u GFX voltage (mV)", d, s);
        native_event_t *ev_gfx = &ntv_table.events[idx];
        ev_gfx->id = idx; ev_gfx->name = strdup(name_buf); ev_gfx->descr = strdup(descr_buf);
        ev_gfx->device = d; ev_gfx->value = 0; ev_gfx->mode = PAPI_MODE_READ; ev_gfx->variant = 3; ev_gfx->subvariant = s;
        ev_gfx->open_func = open_simple; ev_gfx->close_func = close_simple; ev_gfx->start_func = start_simple; ev_gfx->stop_func = stop_simple;
        ev_gfx->access_func = access_amdsmi_power_sensor;
        htable_insert(htable, ev_gfx->name, ev_gfx); idx++;

        /* Register SOC voltage */
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "power_sensor_soc_voltage_mv:device=%d:sensor=%u", d, s);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d power sensor %u SOC voltage (mV)", d, s);
        native_event_t *ev_soc = &ntv_table.events[idx];
        ev_soc->id = idx; ev_soc->name = strdup(name_buf); ev_soc->descr = strdup(descr_buf);
        ev_soc->device = d; ev_soc->value = 0; ev_soc->mode = PAPI_MODE_READ; ev_soc->variant = 4; ev_soc->subvariant = s;
        ev_soc->open_func = open_simple; ev_soc->close_func = close_simple; ev_soc->start_func = start_simple; ev_soc->stop_func = stop_simple;
        ev_soc->access_func = access_amdsmi_power_sensor;
        htable_insert(htable, ev_soc->name, ev_soc); idx++;

        /* Register MEM voltage */
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "power_sensor_mem_voltage_mv:device=%d:sensor=%u", d, s);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d power sensor %u MEM voltage (mV)", d, s);
        native_event_t *ev_mem = &ntv_table.events[idx];
        ev_mem->id = idx; ev_mem->name = strdup(name_buf); ev_mem->descr = strdup(descr_buf);
        ev_mem->device = d; ev_mem->value = 0; ev_mem->mode = PAPI_MODE_READ; ev_mem->variant = 5; ev_mem->subvariant = s;
        ev_mem->open_func = open_simple; ev_mem->close_func = close_simple; ev_mem->start_func = start_simple; ev_mem->stop_func = stop_simple;
        ev_mem->access_func = access_amdsmi_power_sensor;
        htable_insert(htable, ev_mem->name, ev_mem); idx++;

        /* Register power limit */
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "power_sensor_limit_watts:device=%d:sensor=%u", d, s);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d power sensor %u power limit (W)", d, s);
        native_event_t *ev_lim = &ntv_table.events[idx];
        ev_lim->id = idx; ev_lim->name = strdup(name_buf); ev_lim->descr = strdup(descr_buf);
        ev_lim->device = d; ev_lim->value = 0; ev_lim->mode = PAPI_MODE_READ; ev_lim->variant = 6; ev_lim->subvariant = s;
        ev_lim->open_func = open_simple; ev_lim->close_func = close_simple; ev_lim->start_func = start_simple; ev_lim->stop_func = stop_simple;
        ev_lim->access_func = access_amdsmi_power_sensor;
        htable_insert(htable, ev_lim->name, ev_lim); idx++;
      }
    }

    if (amdsmi_init_gpu_event_notification_p && amdsmi_set_gpu_event_notification_mask_p &&
        amdsmi_get_gpu_event_notification_p && amdsmi_stop_gpu_event_notification_p) {
      if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
      snprintf(name_buf, sizeof(name_buf), "thermal_throttle_events:device=%d", d);
      snprintf(descr_buf, sizeof(descr_buf), "Device %d thermal throttle event notifications", d);
      native_event_t *ev_tt = &ntv_table.events[idx];
      ev_tt->id = idx; ev_tt->name = strdup(name_buf); ev_tt->descr = strdup(descr_buf);
      ev_tt->device = d; ev_tt->value = 0; ev_tt->mode = PAPI_MODE_READ; ev_tt->variant = AMDSMI_EVT_NOTIF_THERMAL_THROTTLE; ev_tt->subvariant = 0;
      ev_tt->open_func = open_simple; ev_tt->close_func = close_simple; ev_tt->start_func = start_simple; ev_tt->stop_func = stop_simple;
      ev_tt->access_func = access_amdsmi_event_notification;
      htable_insert(htable, ev_tt->name, ev_tt);
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
        if (src_type)
          free(src_type);
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
      if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
        papi_free(ntv_table.events);
        return PAPI_ENOSUPP;
      }
      snprintf(name_buf, sizeof(name_buf), "lib_version_major");
      snprintf(descr_buf, sizeof(descr_buf), "AMD SMI library major version");
      native_event_t *ev_lmaj = &ntv_table.events[idx];
      ev_lmaj->id = idx;
      ev_lmaj->name = strdup(name_buf);
      ev_lmaj->descr = strdup(descr_buf);
      ev_lmaj->device = -1;
      ev_lmaj->value = 0;
      ev_lmaj->mode = PAPI_MODE_READ;
      ev_lmaj->variant = 0;
      ev_lmaj->subvariant = 0;
      ev_lmaj->open_func = open_simple;
      ev_lmaj->close_func = close_simple;
      ev_lmaj->start_func = start_simple;
      ev_lmaj->stop_func = stop_simple;
      ev_lmaj->access_func = access_amdsmi_lib_version;
      htable_insert(htable, ev_lmaj->name, ev_lmaj);
      idx++;
      if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
        papi_free(ntv_table.events);
        return PAPI_ENOSUPP;
      }
      snprintf(name_buf, sizeof(name_buf), "lib_version_minor");
      snprintf(descr_buf, sizeof(descr_buf), "AMD SMI library minor version");
      native_event_t *ev_lmin = &ntv_table.events[idx];
      ev_lmin->id = idx;
      ev_lmin->name = strdup(name_buf);
      ev_lmin->descr = strdup(descr_buf);
      ev_lmin->device = -1;
      ev_lmin->value = 0;
      ev_lmin->mode = PAPI_MODE_READ;
      ev_lmin->variant = 1;
      ev_lmin->subvariant = 0;
      ev_lmin->open_func = open_simple;
      ev_lmin->close_func = close_simple;
      ev_lmin->start_func = start_simple;
      ev_lmin->stop_func = stop_simple;
      ev_lmin->access_func = access_amdsmi_lib_version;
      htable_insert(htable, ev_lmin->name, ev_lmin);
      idx++;
      if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
        papi_free(ntv_table.events);
        return PAPI_ENOSUPP;
      }
      snprintf(name_buf, sizeof(name_buf), "lib_version_release");
      snprintf(descr_buf, sizeof(descr_buf), "AMD SMI library release/patch version");
      native_event_t *ev_lrel = &ntv_table.events[idx];
      ev_lrel->id = idx;
      ev_lrel->name = strdup(name_buf);
      ev_lrel->descr = strdup(descr_buf);
      ev_lrel->device = -1;
      ev_lrel->value = 0;
      ev_lrel->mode = PAPI_MODE_READ;
      ev_lrel->variant = 2;
      ev_lrel->subvariant = 0;
      ev_lrel->open_func = open_simple;
      ev_lrel->close_func = close_simple;
      ev_lrel->start_func = start_simple;
      ev_lrel->stop_func = stop_simple;
      ev_lrel->access_func = access_amdsmi_lib_version;
      htable_insert(htable, ev_lrel->name, ev_lrel);
      idx++;
    }
  }
  for (int d = 0; d < gpu_count; ++d) {
    if (!device_handles || !device_handles[d])
      continue;
    /* Device UUID (hash) */
    if (amdsmi_get_gpu_device_uuid_p) {
      unsigned int uuid_len = 0;
      amdsmi_status_t st = amdsmi_get_gpu_device_uuid_p(device_handles[d], &uuid_len, NULL);
      /* Some builds require preflight to get length; we just attempt a fixed
       * buffer */
      char uuid_buf[128];
      uuid_len = sizeof(uuid_buf);
      st = amdsmi_get_gpu_device_uuid_p(device_handles[d], &uuid_len, uuid_buf);
      if (st == AMDSMI_STATUS_SUCCESS) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "uuid_hash:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d UUID (djb2 64-bit hash)", d);
        native_event_t *ev_uuid = &ntv_table.events[idx];
        ev_uuid->id = idx;
        ev_uuid->name = strdup(name_buf);
        ev_uuid->descr = strdup(descr_buf);
        ev_uuid->device = d;
        ev_uuid->value = 0;
        ev_uuid->mode = PAPI_MODE_READ;
        ev_uuid->variant = 0;
        ev_uuid->subvariant = 0;
        ev_uuid->open_func = open_simple;
        ev_uuid->close_func = close_simple;
        ev_uuid->start_func = start_simple;
        ev_uuid->stop_func = stop_simple;
        ev_uuid->access_func = access_amdsmi_uuid_hash;
        htable_insert(htable, ev_uuid->name, ev_uuid);
        idx++;
      }
    }
    /* Vendor / VRAM vendor / Subsystem name (hash) */
    if (amdsmi_get_gpu_vendor_name_p) {
      char tmp[256] = {0};
      if (amdsmi_get_gpu_vendor_name_p(device_handles[d], tmp, sizeof(tmp)) == AMDSMI_STATUS_SUCCESS) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "vendor_name_hash:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d vendor name (hash)", d);
        native_event_t *ev_vn = &ntv_table.events[idx];
        ev_vn->id = idx;
        ev_vn->name = strdup(name_buf);
        ev_vn->descr = strdup(descr_buf);
        if (!ev_vn->name || !ev_vn->descr)
          return PAPI_ENOMEM;
        ev_vn->device = d;
        ev_vn->value = 0;
        ev_vn->mode = PAPI_MODE_READ;
        ev_vn->variant = 0; /* access_amdsmi_gpu_string_hash -> vendor name */
        ev_vn->subvariant = 0;
        ev_vn->open_func = open_simple;
        ev_vn->close_func = close_simple;
        ev_vn->start_func = start_simple;
        ev_vn->stop_func = stop_simple;
        ev_vn->access_func = access_amdsmi_gpu_string_hash;
        htable_insert(htable, ev_vn->name, ev_vn);
        idx++;
      }
    }

    if (amdsmi_get_gpu_vram_vendor_p) {
      char tmp[256] = {0};
      if (amdsmi_get_gpu_vram_vendor_p(device_handles[d], tmp, (uint32_t)sizeof(tmp)) == AMDSMI_STATUS_SUCCESS) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "vram_vendor_hash:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d VRAM vendor (hash)", d);
        native_event_t *ev_vrv = &ntv_table.events[idx];
        ev_vrv->id = idx;
        ev_vrv->name = strdup(name_buf);
        ev_vrv->descr = strdup(descr_buf);
        if (!ev_vrv->name || !ev_vrv->descr)
          return PAPI_ENOMEM;
        ev_vrv->device = d;
        ev_vrv->value = 0;
        ev_vrv->mode = PAPI_MODE_READ;
        ev_vrv->variant = 1; /* access_amdsmi_gpu_string_hash -> VRAM vendor */
        ev_vrv->subvariant = 0;
        ev_vrv->open_func = open_simple;
        ev_vrv->close_func = close_simple;
        ev_vrv->start_func = start_simple;
        ev_vrv->stop_func = stop_simple;
        ev_vrv->access_func = access_amdsmi_gpu_string_hash;
        htable_insert(htable, ev_vrv->name, ev_vrv);
        idx++;
      }
    }

    if (amdsmi_get_gpu_subsystem_name_p) {
      char tmp[256] = {0};
      if (amdsmi_get_gpu_subsystem_name_p(device_handles[d], tmp, sizeof(tmp)) == AMDSMI_STATUS_SUCCESS) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "subsystem_name_hash:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d subsystem name (hash)", d);
        native_event_t *ev_ssn = &ntv_table.events[idx];
        ev_ssn->id = idx;
        ev_ssn->name = strdup(name_buf);
        ev_ssn->descr = strdup(descr_buf);
        if (!ev_ssn->name || !ev_ssn->descr)
          return PAPI_ENOMEM;
        ev_ssn->device = d;
        ev_ssn->value = 0;
        ev_ssn->mode = PAPI_MODE_READ;
        ev_ssn->variant = 2; /* access_amdsmi_gpu_string_hash -> subsystem name */
        ev_ssn->subvariant = 0;
        ev_ssn->open_func = open_simple;
        ev_ssn->close_func = close_simple;
        ev_ssn->start_func = start_simple;
        ev_ssn->stop_func = stop_simple;
        ev_ssn->access_func = access_amdsmi_gpu_string_hash;
        htable_insert(htable, ev_ssn->name, ev_ssn);
        idx++;
      }
    }

    /* Enumeration info (drm render/card, hsa/hip ids) */
    if (amdsmi_get_gpu_enumeration_info_p) {
      amdsmi_enumeration_info_t einfo;
      if (amdsmi_get_gpu_enumeration_info_p(device_handles[d], &einfo) == AMDSMI_STATUS_SUCCESS) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "enum_drm_render:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d DRM render node", d);
        native_event_t *ev_er = &ntv_table.events[idx];
        ev_er->id = idx;
        ev_er->name = strdup(name_buf);
        ev_er->descr = strdup(descr_buf);
        ev_er->device = d;
        ev_er->value = 0;
        ev_er->mode = PAPI_MODE_READ;
        ev_er->variant = 0;
        ev_er->subvariant = 0;
        ev_er->open_func = open_simple;
        ev_er->close_func = close_simple;
        ev_er->start_func = start_simple;
        ev_er->stop_func = stop_simple;
        ev_er->access_func = access_amdsmi_enumeration_info;
        htable_insert(htable, ev_er->name, ev_er);
        idx++;
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "enum_drm_card:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d DRM card index", d);
        native_event_t *ev_ec = &ntv_table.events[idx];
        ev_ec->id = idx;
        ev_ec->name = strdup(name_buf);
        ev_ec->descr = strdup(descr_buf);
        ev_ec->device = d;
        ev_ec->value = 0;
        ev_ec->mode = PAPI_MODE_READ;
        ev_ec->variant = 1;
        ev_ec->subvariant = 0;
        ev_ec->open_func = open_simple;
        ev_ec->close_func = close_simple;
        ev_ec->start_func = start_simple;
        ev_ec->stop_func = stop_simple;
        ev_ec->access_func = access_amdsmi_enumeration_info;
        htable_insert(htable, ev_ec->name, ev_ec);
        idx++;
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "enum_hsa_id:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d HSA ID", d);
        native_event_t *ev_eh = &ntv_table.events[idx];
        ev_eh->id = idx;
        ev_eh->name = strdup(name_buf);
        ev_eh->descr = strdup(descr_buf);
        ev_eh->device = d;
        ev_eh->value = 0;
        ev_eh->mode = PAPI_MODE_READ;
        ev_eh->variant = 2;
        ev_eh->subvariant = 0;
        ev_eh->open_func = open_simple;
        ev_eh->close_func = close_simple;
        ev_eh->start_func = start_simple;
        ev_eh->stop_func = stop_simple;
        ev_eh->access_func = access_amdsmi_enumeration_info;
        htable_insert(htable, ev_eh->name, ev_eh);
        idx++;
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "enum_hip_id:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d HIP ID", d);
        native_event_t *ev_ehip = &ntv_table.events[idx];
        ev_ehip->id = idx;
        ev_ehip->name = strdup(name_buf);
        ev_ehip->descr = strdup(descr_buf);
        ev_ehip->device = d;
        ev_ehip->value = 0;
        ev_ehip->mode = PAPI_MODE_READ;
        ev_ehip->variant = 3;
        ev_ehip->subvariant = 0;
        ev_ehip->open_func = open_simple;
        ev_ehip->close_func = close_simple;
        ev_ehip->start_func = start_simple;
        ev_ehip->stop_func = stop_simple;
        ev_ehip->access_func = access_amdsmi_enumeration_info;
        htable_insert(htable, ev_ehip->name, ev_ehip);
        idx++;
      }
    }
    /* ASIC info (numeric IDs & CU count) */
    if (amdsmi_get_gpu_asic_info_p) {
      amdsmi_asic_info_t ainfo;
      if (amdsmi_get_gpu_asic_info_p(device_handles[d], &ainfo) == AMDSMI_STATUS_SUCCESS) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "asic_vendor_id:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d ASIC vendor id", d);
        native_event_t *ev_av = &ntv_table.events[idx];
        ev_av->id = idx;
        ev_av->name = strdup(name_buf);
        ev_av->descr = strdup(descr_buf);
        ev_av->device = d;
        ev_av->value = 0;
        ev_av->mode = PAPI_MODE_READ;
        ev_av->variant = 0;
        ev_av->subvariant = 0;
        ev_av->open_func = open_simple;
        ev_av->close_func = close_simple;
        ev_av->start_func = start_simple;
        ev_av->stop_func = stop_simple;
        ev_av->access_func = access_amdsmi_asic_info;
        htable_insert(htable, ev_av->name, ev_av);
        idx++;
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "asic_device_id:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d ASIC device id", d);
        native_event_t *ev_ad = &ntv_table.events[idx];
        ev_ad->id = idx;
        ev_ad->name = strdup(name_buf);
        ev_ad->descr = strdup(descr_buf);
        ev_ad->device = d;
        ev_ad->value = 0;
        ev_ad->mode = PAPI_MODE_READ;
        ev_ad->variant = 1;
        ev_ad->subvariant = 0;
        ev_ad->open_func = open_simple;
        ev_ad->close_func = close_simple;
        ev_ad->start_func = start_simple;
        ev_ad->stop_func = stop_simple;
        ev_ad->access_func = access_amdsmi_asic_info;
        htable_insert(htable, ev_ad->name, ev_ad);
        idx++;
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "asic_subsystem_vendor_id:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d ASIC subsystem vendor id", d);
        native_event_t *ev_asv = &ntv_table.events[idx];
        ev_asv->id = idx;
        ev_asv->name = strdup(name_buf);
        ev_asv->descr = strdup(descr_buf);
        ev_asv->device = d;
        ev_asv->value = 0;
        ev_asv->mode = PAPI_MODE_READ;
        ev_asv->variant = 2;
        ev_asv->subvariant = 0;
        ev_asv->open_func = open_simple;
        ev_asv->close_func = close_simple;
        ev_asv->start_func = start_simple;
        ev_asv->stop_func = stop_simple;
        ev_asv->access_func = access_amdsmi_asic_info;
        htable_insert(htable, ev_asv->name, ev_asv);
        idx++;
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "asic_subsystem_id:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d ASIC subsystem id", d);
        native_event_t *ev_ass = &ntv_table.events[idx];
        ev_ass->id = idx;
        ev_ass->name = strdup(name_buf);
        ev_ass->descr = strdup(descr_buf);
        ev_ass->device = d;
        ev_ass->value = 0;
        ev_ass->mode = PAPI_MODE_READ;
        ev_ass->variant = 3;
        ev_ass->subvariant = 0;
        ev_ass->open_func = open_simple;
        ev_ass->close_func = close_simple;
        ev_ass->start_func = start_simple;
        ev_ass->stop_func = stop_simple;
        ev_ass->access_func = access_amdsmi_asic_info;
        htable_insert(htable, ev_ass->name, ev_ass);
        idx++;
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "asic_revision:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d ASIC revision id", d);
        native_event_t *ev_ar = &ntv_table.events[idx];
        ev_ar->id = idx;
        ev_ar->name = strdup(name_buf);
        ev_ar->descr = strdup(descr_buf);
        ev_ar->device = d;
        ev_ar->value = 0;
        ev_ar->mode = PAPI_MODE_READ;
        ev_ar->variant = 4;
        ev_ar->subvariant = 0;
        ev_ar->open_func = open_simple;
        ev_ar->close_func = close_simple;
        ev_ar->start_func = start_simple;
        ev_ar->stop_func = stop_simple;
        ev_ar->access_func = access_amdsmi_asic_info;
        htable_insert(htable, ev_ar->name, ev_ar);
        idx++;
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "compute_units:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d number of compute units", d);
        native_event_t *ev_cu = &ntv_table.events[idx];
        ev_cu->id = idx;
        ev_cu->name = strdup(name_buf);
        ev_cu->descr = strdup(descr_buf);
        ev_cu->device = d;
        ev_cu->value = 0;
        ev_cu->mode = PAPI_MODE_READ;
        ev_cu->variant = 5;
        ev_cu->subvariant = 0;
        ev_cu->open_func = open_simple;
        ev_cu->close_func = close_simple;
        ev_cu->start_func = start_simple;
        ev_cu->stop_func = stop_simple;
        ev_cu->access_func = access_amdsmi_asic_info;
        htable_insert(htable, ev_cu->name, ev_cu);
        idx++;
      }
    }
    /* Driver info (strings hashed) */
    if (amdsmi_get_gpu_driver_info_p) {
      amdsmi_driver_info_t dinfo;
      if (amdsmi_get_gpu_driver_info_p(device_handles[d], &dinfo) == AMDSMI_STATUS_SUCCESS) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "driver_name_hash:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d driver name (hash)", d);
        native_event_t *ev_dname = &ntv_table.events[idx];
        ev_dname->id = idx;
        ev_dname->name = strdup(name_buf);
        ev_dname->descr = strdup(descr_buf);
        ev_dname->device = d;
        ev_dname->value = 0;
        ev_dname->mode = PAPI_MODE_READ;
        ev_dname->variant = 3;
        ev_dname->subvariant = 0;
        ev_dname->open_func = open_simple;
        ev_dname->close_func = close_simple;
        ev_dname->start_func = start_simple;
        ev_dname->stop_func = stop_simple;
        ev_dname->access_func = access_amdsmi_gpu_string_hash;
        htable_insert(htable, ev_dname->name, ev_dname);
        idx++;
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "driver_date_hash:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d driver date (hash)", d);
        native_event_t *ev_dd = &ntv_table.events[idx];
        ev_dd->id = idx;
        ev_dd->name = strdup(name_buf);
        ev_dd->descr = strdup(descr_buf);
        ev_dd->device = d;
        ev_dd->value = 0;
        ev_dd->mode = PAPI_MODE_READ;
        ev_dd->variant = 4;
        ev_dd->subvariant = 0;
        ev_dd->open_func = open_simple;
        ev_dd->close_func = close_simple;
        ev_dd->start_func = start_simple;
        ev_dd->stop_func = stop_simple;
        ev_dd->access_func = access_amdsmi_gpu_string_hash;
        htable_insert(htable, ev_dd->name, ev_dd);
        idx++;
      }
    }
    /* VBIOS info (strings hashed) */
    /*if (amdsmi_get_gpu_vbios_info_p) {
      amdsmi_vbios_info_t vb;
      if (amdsmi_get_gpu_vbios_info_p(device_handles[d], &vb) == AMDSMI_STATUS_SUCCESS) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "vbios_version_hash:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d vBIOS version (hash)", d);
        native_event_t *ev_vbv = &ntv_table.events[idx];
        ev_vbv->id = idx;
        ev_vbv->name = strdup(name_buf);
        ev_vbv->descr = strdup(descr_buf);
        ev_vbv->device = d;
        ev_vbv->value = 0;
        ev_vbv->mode = PAPI_MODE_READ;
        ev_vbv->variant = 5;
        ev_vbv->subvariant = 0;
        ev_vbv->open_func = open_simple;
        ev_vbv->close_func = close_simple;
        ev_vbv->start_func = start_simple;
        ev_vbv->stop_func = stop_simple;
        ev_vbv->access_func = access_amdsmi_gpu_string_hash;
        htable_insert(htable, ev_vbv->name, ev_vbv);
        idx++;
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "vbios_part_number_hash:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d vBIOS part number (hash)", d);
        native_event_t *ev_vbp = &ntv_table.events[idx];
        ev_vbp->id = idx;
        ev_vbp->name = strdup(name_buf);
        ev_vbp->descr = strdup(descr_buf);
        ev_vbp->device = d;
        ev_vbp->value = 0;
        ev_vbp->mode = PAPI_MODE_READ;
        ev_vbp->variant = 6;
        ev_vbp->subvariant = 0;
        ev_vbp->open_func = open_simple;
        ev_vbp->close_func = close_simple;
        ev_vbp->start_func = start_simple;
        ev_vbp->stop_func = stop_simple;
        ev_vbp->access_func = access_amdsmi_gpu_string_hash;
        htable_insert(htable, ev_vbp->name, ev_vbp);
        idx++;
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "vbios_build_date_hash:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d vBIOS build date (hash)", d);
        native_event_t *ev_vbd = &ntv_table.events[idx];
        ev_vbd->id = idx;
        ev_vbd->name = strdup(name_buf);
        ev_vbd->descr = strdup(descr_buf);
        ev_vbd->device = d;
        ev_vbd->value = 0;
        ev_vbd->mode = PAPI_MODE_READ;
        ev_vbd->variant = 7;
        ev_vbd->subvariant = 0;
        ev_vbd->open_func = open_simple;
        ev_vbd->close_func = close_simple;
        ev_vbd->start_func = start_simple;
        ev_vbd->stop_func = stop_simple;
        ev_vbd->access_func = access_amdsmi_gpu_string_hash;
        htable_insert(htable, ev_vbd->name, ev_vbd);
        idx++;
      }
    }*/
    /* PCIe metrics via amdsmi_get_link_metrics (aggregate read/write kB over
     * XGMI) */
    amdsmi_link_metrics_t lm_probe; memset(&lm_probe, 0, sizeof(lm_probe));
    int has_xgmi = 0, has_pcie = 0;
    if (amdsmi_get_link_metrics_p(device_handles[d], &lm_probe) == AMDSMI_STATUS_SUCCESS) {
        uint32_t n = lm_probe.num_links;
        if (n > AMDSMI_MAX_NUM_XGMI_PHYSICAL_LINK) n = AMDSMI_MAX_NUM_XGMI_PHYSICAL_LINK;
        for (uint32_t i=0;i<n;i++) {
            if (lm_probe.links[i].link_type == AMDSMI_LINK_TYPE_XGMI) has_xgmi = 1;
            if (lm_probe.links[i].link_type == AMDSMI_LINK_TYPE_PCIE) has_pcie = 1;
        }
    }
    
    /* --- XGMI totals (only if present). Read/Write are XGMI-only. --- */
    if (has_xgmi) {
        /* xgmi_total_read_kB */
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "xgmi_total_read_kB:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d total XGMI read across links (kB)", d);
        native_event_t *ev_xr = &ntv_table.events[idx];
        ev_xr->id=idx; ev_xr->name=strdup(name_buf); ev_xr->descr=strdup(descr_buf);
        if (!ev_xr->name || !ev_xr->descr) return PAPI_ENOMEM;
        ev_xr->device=d; ev_xr->value=0; ev_xr->mode=PAPI_MODE_READ; ev_xr->variant=0;
        ev_xr->subvariant = AMDSMI_LINK_TYPE_XGMI;
        ev_xr->open_func=open_simple; ev_xr->close_func=close_simple; ev_xr->start_func=start_simple; ev_xr->stop_func=stop_simple;
        ev_xr->access_func=access_amdsmi_link_metrics; htable_insert(htable, ev_xr->name, ev_xr); idx++;
    
        /* xgmi_total_write_kB */
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "xgmi_total_write_kB:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d total XGMI write across links (kB)", d);
        native_event_t *ev_xw = &ntv_table.events[idx];
        ev_xw->id=idx; ev_xw->name=strdup(name_buf); ev_xw->descr=strdup(descr_buf);
        if (!ev_xw->name || !ev_xw->descr) return PAPI_ENOMEM;
        ev_xw->device=d; ev_xw->value=0; ev_xw->mode=PAPI_MODE_READ; ev_xw->variant=1;
        ev_xw->subvariant = AMDSMI_LINK_TYPE_XGMI;
        ev_xw->open_func=open_simple; ev_xw->close_func=close_simple; ev_xw->start_func=start_simple; ev_xw->stop_func=stop_simple;
        ev_xw->access_func=access_amdsmi_link_metrics; htable_insert(htable, ev_xw->name, ev_xw); idx++;
    
        /* Optional XGMI rates */
        /* ... same pattern with variant=2 (bit_rate_Gbps) and 3 (max_bandwidth_Gbps) ... */
    }
    
    /* --- PCIe aggregate rates (if present). No read/write here (N/A). --- */
    if (has_pcie) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "pcie_total_bit_rate_Gbps:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d aggregate PCIe link speed (Gb/s)", d);
        native_event_t *ev_pb = &ntv_table.events[idx];
        ev_pb->id=idx; ev_pb->name=strdup(name_buf); ev_pb->descr=strdup(descr_buf);
        if (!ev_pb->name || !ev_pb->descr) return PAPI_ENOMEM;
        ev_pb->device=d; ev_pb->value=0; ev_pb->mode=PAPI_MODE_READ; ev_pb->variant=2;
        ev_pb->subvariant = AMDSMI_LINK_TYPE_PCIE;
        ev_pb->open_func=open_simple; ev_pb->close_func=close_simple; ev_pb->start_func=start_simple; ev_pb->stop_func=stop_simple;
        ev_pb->access_func=access_amdsmi_link_metrics; htable_insert(htable, ev_pb->name, ev_pb); idx++;
    
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) { papi_free(ntv_table.events); return PAPI_ENOSUPP; }
        snprintf(name_buf, sizeof(name_buf), "pcie_total_max_bandwidth_Gbps:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d aggregate PCIe max bandwidth (Gb/s)", d);
        native_event_t *ev_pm = &ntv_table.events[idx];
        ev_pm->id=idx; ev_pm->name=strdup(name_buf); ev_pm->descr=strdup(descr_buf);
        if (!ev_pm->name || !ev_pm->descr) return PAPI_ENOMEM;
        ev_pm->device=d; ev_pm->value=0; ev_pm->mode=PAPI_MODE_READ; ev_pm->variant=3;
        ev_pm->subvariant = AMDSMI_LINK_TYPE_PCIE;
        ev_pm->open_func=open_simple; ev_pm->close_func=close_simple; ev_pm->start_func=start_simple; ev_pm->stop_func=stop_simple;
        ev_pm->access_func=access_amdsmi_link_metrics; htable_insert(htable, ev_pm->name, ev_pm); idx++;
    }

    /* Process list size (count of running GPU processes) */
    if (amdsmi_get_gpu_process_list_p) {
      if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
        papi_free(ntv_table.events);
        return PAPI_ENOSUPP;
      }
      snprintf(name_buf, sizeof(name_buf), "process_count:device=%d", d);
      snprintf(descr_buf, sizeof(descr_buf), "Device %d active GPU process count", d);
      native_event_t *ev_pc = &ntv_table.events[idx];
      ev_pc->id = idx;
      ev_pc->name = strdup(name_buf);
      ev_pc->descr = strdup(descr_buf);
      ev_pc->device = d;
      ev_pc->value = 0;
      ev_pc->mode = PAPI_MODE_READ;
      ev_pc->variant = 0;
      ev_pc->subvariant = 0;
      ev_pc->open_func = open_simple;
      ev_pc->close_func = close_simple;
      ev_pc->start_func = start_simple;
      ev_pc->stop_func = stop_simple;
      ev_pc->access_func = access_amdsmi_process_count;
      htable_insert(htable, ev_pc->name, ev_pc);
      idx++;
    }
    /* ECC totals & enabled mask (where supported) */
    if (amdsmi_get_gpu_total_ecc_count_p) {
      if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
        papi_free(ntv_table.events);
        return PAPI_ENOSUPP;
      }
      snprintf(name_buf, sizeof(name_buf), "ecc_total_correctable:device=%d", d);
      snprintf(descr_buf, sizeof(descr_buf), "Device %d total ECC correctable errors", d);
      native_event_t *ev_ecct = &ntv_table.events[idx];
      ev_ecct->id = idx;
      ev_ecct->name = strdup(name_buf);
      ev_ecct->descr = strdup(descr_buf);
      ev_ecct->device = d;
      ev_ecct->value = 0;
      ev_ecct->mode = PAPI_MODE_READ;
      ev_ecct->variant = 0;
      ev_ecct->subvariant = 0;
      ev_ecct->open_func = open_simple;
      ev_ecct->close_func = close_simple;
      ev_ecct->start_func = start_simple;
      ev_ecct->stop_func = stop_simple;
      ev_ecct->access_func = access_amdsmi_ecc_total;
      htable_insert(htable, ev_ecct->name, ev_ecct);
      idx++;
      if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
        papi_free(ntv_table.events);
        return PAPI_ENOSUPP;
      }
      snprintf(name_buf, sizeof(name_buf), "ecc_total_uncorrectable:device=%d", d);
      snprintf(descr_buf, sizeof(descr_buf), "Device %d total ECC uncorrectable errors", d);
      native_event_t *ev_ecctu = &ntv_table.events[idx];
      ev_ecctu->id = idx;
      ev_ecctu->name = strdup(name_buf);
      ev_ecctu->descr = strdup(descr_buf);
      ev_ecctu->device = d;
      ev_ecctu->value = 0;
      ev_ecctu->mode = PAPI_MODE_READ;
      ev_ecctu->variant = 1;
      ev_ecctu->subvariant = 0;
      ev_ecctu->open_func = open_simple;
      ev_ecctu->close_func = close_simple;
      ev_ecctu->start_func = start_simple;
      ev_ecctu->stop_func = stop_simple;
      ev_ecctu->access_func = access_amdsmi_ecc_total;
      htable_insert(htable, ev_ecctu->name, ev_ecctu);
      idx++;
      if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
        papi_free(ntv_table.events);
        return PAPI_ENOSUPP;
      }
      snprintf(name_buf, sizeof(name_buf), "ecc_total_deferred:device=%d", d);
      snprintf(descr_buf, sizeof(descr_buf), "Device %d total ECC deferred errors", d);
      native_event_t *ev_ecctd = &ntv_table.events[idx];
      ev_ecctd->id = idx;
      ev_ecctd->name = strdup(name_buf);
      ev_ecctd->descr = strdup(descr_buf);
      ev_ecctd->device = d;
      ev_ecctd->value = 0;
      ev_ecctd->mode = PAPI_MODE_READ;
      ev_ecctd->variant = 2;
      ev_ecctd->subvariant = 0;
      ev_ecctd->open_func = open_simple;
      ev_ecctd->close_func = close_simple;
      ev_ecctd->start_func = start_simple;
      ev_ecctd->stop_func = stop_simple;
      ev_ecctd->access_func = access_amdsmi_ecc_total;
      htable_insert(htable, ev_ecctd->name, ev_ecctd);
      idx++;
    }
    if (amdsmi_get_gpu_ecc_enabled_p) {
      if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
        papi_free(ntv_table.events);
        return PAPI_ENOSUPP;
      }
      snprintf(name_buf, sizeof(name_buf), "ecc_enabled_mask:device=%d", d);
      snprintf(descr_buf, sizeof(descr_buf), "Device %d ECC enabled block bitmask", d);
      native_event_t *ev_eccm = &ntv_table.events[idx];
      ev_eccm->id = idx;
      ev_eccm->name = strdup(name_buf);
      ev_eccm->descr = strdup(descr_buf);
      ev_eccm->device = d;
      ev_eccm->value = 0;
      ev_eccm->mode = PAPI_MODE_READ;
      ev_eccm->variant = 0;
      ev_eccm->subvariant = 0;
      ev_eccm->open_func = open_simple;
      ev_eccm->close_func = close_simple;
      ev_eccm->start_func = start_simple;
      ev_eccm->stop_func = stop_simple;
      ev_eccm->access_func = access_amdsmi_ecc_enabled_mask;
      htable_insert(htable, ev_eccm->name, ev_eccm);
      idx++;
    }
    /* Partitioning state (hash/enumeration) */
    if (amdsmi_get_gpu_compute_partition_p) {
      char dummy[128] = {0};
      if (amdsmi_get_gpu_compute_partition_p(device_handles[d], dummy, sizeof(dummy)) == AMDSMI_STATUS_SUCCESS) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "compute_partition_hash:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d compute partition (hash)", d);
        native_event_t *ev_cpart = &ntv_table.events[idx];
        ev_cpart->id = idx;
        ev_cpart->name = strdup(name_buf);
        ev_cpart->descr = strdup(descr_buf);
        if (!ev_cpart->name || !ev_cpart->descr) {
          return PAPI_ENOMEM;
        }
        ev_cpart->device = d;
        ev_cpart->value = 0;
        ev_cpart->mode = PAPI_MODE_READ;
        ev_cpart->variant = 0;
        ev_cpart->subvariant = 0;
        ev_cpart->open_func = open_simple;
        ev_cpart->close_func = close_simple;
        ev_cpart->start_func = start_simple;
        ev_cpart->stop_func = stop_simple;
        ev_cpart->access_func = access_amdsmi_compute_partition_hash;
        htable_insert(htable, ev_cpart->name, ev_cpart);
        idx++;
      }
    }
    if (amdsmi_get_gpu_memory_partition_p) {
      char dummy[128] = {0};
      if (amdsmi_get_gpu_memory_partition_p(device_handles[d], dummy, sizeof(dummy)) == AMDSMI_STATUS_SUCCESS) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "memory_partition_hash:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d memory partition (hash)", d);
        native_event_t *ev_mpart = &ntv_table.events[idx];
        ev_mpart->id = idx;
        ev_mpart->name = strdup(name_buf);
        ev_mpart->descr = strdup(descr_buf);
        if (!ev_mpart->name || !ev_mpart->descr) {
          return PAPI_ENOMEM;
        }
        ev_mpart->device = d;
        ev_mpart->value = 0;
        ev_mpart->mode = PAPI_MODE_READ;
        ev_mpart->variant = 0;
        ev_mpart->subvariant = 0;
        ev_mpart->open_func = open_simple;
        ev_mpart->close_func = close_simple;
        ev_mpart->start_func = start_simple;
        ev_mpart->stop_func = stop_simple;
        ev_mpart->access_func = access_amdsmi_memory_partition_hash;
        htable_insert(htable, ev_mpart->name, ev_mpart);
        idx++;
      }
    }
    if (amdsmi_get_gpu_accelerator_partition_profile_p) {
      amdsmi_accelerator_partition_profile_t prof;
      uint32_t ids[AMDSMI_MAX_ACCELERATOR_PARTITIONS] = {0};
      if (amdsmi_get_gpu_accelerator_partition_profile_p(device_handles[d], &prof, ids) == AMDSMI_STATUS_SUCCESS) {
        if (idx >= MAX_EVENTS_PER_DEVICE * device_count) {
          papi_free(ntv_table.events);
          return PAPI_ENOSUPP;
        }
        snprintf(name_buf, sizeof(name_buf), "accelerator_num_partitions:device=%d", d);
        snprintf(descr_buf, sizeof(descr_buf), "Device %d accelerator profile partitions", d);
        native_event_t *ev_anp = &ntv_table.events[idx];
        ev_anp->id = idx;
        ev_anp->name = strdup(name_buf);
        ev_anp->descr = strdup(descr_buf);
        if (!ev_anp->name || !ev_anp->descr) {
          return PAPI_ENOMEM;
        }
        ev_anp->device = d;
        ev_anp->value = 0;
        ev_anp->mode = PAPI_MODE_READ;
        ev_anp->variant = 0;
        ev_anp->subvariant = 0;
        ev_anp->open_func = open_simple;
        ev_anp->close_func = close_simple;
        ev_anp->start_func = start_simple;
        ev_anp->stop_func = stop_simple;
        ev_anp->access_func = access_amdsmi_accelerator_num_partitions;
        htable_insert(htable, ev_anp->name, ev_anp);
        idx++;
      }
    }
  }
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
      if (cpu_core_handles[s])
        papi_free(cpu_core_handles[s]);
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
