#ifndef __AMDS_PRIV_H__
#define __AMDS_PRIV_H__

#define AMDSMI_DISABLE_ESMI

#include <amd_smi/amdsmi.h>
#include <stdint.h>

/* Some older AMD SMI releases do not expose amdsmi_enumeration_info_t or the
 * associated query function.  Guard all related uses behind
 * AMDSMI_HAVE_ENUMERATION_INFO so the component can still build when using
 * those older headers.  Define AMDSMI_HAVE_ENUMERATION_INFO externally if the
 * enumeration APIs are available.
 */

/* Mode enumeration used by accessors */
typedef enum {
  PAPI_MODE_READ = 1,
  PAPI_MODE_WRITE,
  PAPI_MODE_RDWR,
} rocs_access_mode_e;

/* Native event descriptor */
typedef struct native_event {
  unsigned int id;
  char *name, *descr;
  int32_t device;
  uint64_t value;
  uint32_t mode, variant, subvariant;
  int (*open_func)(struct native_event *);
  int (*close_func)(struct native_event *);
  int (*start_func)(struct native_event *);
  int (*stop_func)(struct native_event *);
  int (*access_func)(int mode, void *arg);
} native_event_t;

typedef struct {
  native_event_t *events;
  int count;
} native_event_table_t;

/* Global state */
extern amdsmi_processor_handle *device_handles;
extern int32_t device_count;
extern int32_t gpu_count;
extern int32_t cpu_count;
extern int32_t device_mask;
extern amdsmi_processor_handle **cpu_core_handles;
extern uint32_t *cores_per_socket;
extern void *htable;
extern native_event_table_t ntv_table;
extern native_event_table_t *ntv_table_p;
extern unsigned int _amd_smi_lock;

/* AMD SMI function pointers */
extern amdsmi_status_t (*amdsmi_init_p)(uint64_t);
extern amdsmi_status_t (*amdsmi_shut_down_p)(void);
extern amdsmi_status_t (*amdsmi_get_socket_handles_p)(uint32_t *, amdsmi_socket_handle *);
extern amdsmi_status_t (*amdsmi_get_processor_handles_by_type_p)(amdsmi_socket_handle, processor_type_t, amdsmi_processor_handle *, uint32_t *);
extern amdsmi_status_t (*amdsmi_get_temp_metric_p)(amdsmi_processor_handle, amdsmi_temperature_type_t, amdsmi_temperature_metric_t, int64_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_fan_rpms_p)(amdsmi_processor_handle, uint32_t, int64_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_fan_speed_p)(amdsmi_processor_handle, uint32_t, int64_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_fan_speed_max_p)(amdsmi_processor_handle, uint32_t, int64_t *);
extern amdsmi_status_t (*amdsmi_get_total_memory_p)(amdsmi_processor_handle, amdsmi_memory_type_t, uint64_t *);
extern amdsmi_status_t (*amdsmi_get_memory_usage_p)(amdsmi_processor_handle, amdsmi_memory_type_t, uint64_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_activity_p)(amdsmi_processor_handle, amdsmi_engine_usage_t *);
extern amdsmi_status_t (*amdsmi_get_power_cap_info_p)(amdsmi_processor_handle, uint32_t, amdsmi_power_cap_info_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_power_cap_set_p)(amdsmi_processor_handle, uint32_t, uint64_t);
extern amdsmi_status_t (*amdsmi_get_power_info_p)(amdsmi_processor_handle, amdsmi_power_info_t *);
extern amdsmi_status_t (*amdsmi_set_power_cap_p)(amdsmi_processor_handle, uint32_t, uint64_t);
extern amdsmi_status_t (*amdsmi_get_gpu_pci_throughput_p)(amdsmi_processor_handle, uint64_t *, uint64_t *, uint64_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_pci_replay_counter_p)(amdsmi_processor_handle, uint64_t *);
extern amdsmi_status_t (*amdsmi_get_clk_freq_p)(amdsmi_processor_handle, amdsmi_clk_type_t, amdsmi_frequencies_t *);
extern amdsmi_status_t (*amdsmi_set_clk_freq_p)(amdsmi_processor_handle, amdsmi_clk_type_t, uint64_t);
extern amdsmi_status_t (*amdsmi_get_gpu_metrics_info_p)(amdsmi_processor_handle, amdsmi_gpu_metrics_t *);
extern amdsmi_status_t (*amdsmi_get_lib_version_p)(amdsmi_version_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_driver_info_p)(amdsmi_processor_handle, amdsmi_driver_info_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_asic_info_p)(amdsmi_processor_handle, amdsmi_asic_info_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_board_info_p)(amdsmi_processor_handle, amdsmi_board_info_t *);
extern amdsmi_status_t (*amdsmi_get_fw_info_p)(amdsmi_processor_handle, amdsmi_fw_info_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_vbios_info_p)(amdsmi_processor_handle, amdsmi_vbios_info_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_device_uuid_p)(amdsmi_processor_handle, unsigned int *, char *);
#ifdef AMDSMI_HAVE_ENUMERATION_INFO
extern amdsmi_status_t (*amdsmi_get_gpu_enumeration_info_p)(amdsmi_processor_handle, amdsmi_enumeration_info_t *);
#endif
extern amdsmi_status_t (*amdsmi_get_gpu_vendor_name_p)(amdsmi_processor_handle, char *, size_t);
extern amdsmi_status_t (*amdsmi_get_gpu_vram_vendor_p)(amdsmi_processor_handle, char *, uint32_t);
extern amdsmi_status_t (*amdsmi_get_gpu_subsystem_name_p)(amdsmi_processor_handle, char *, size_t);
extern amdsmi_status_t (*amdsmi_get_link_metrics_p)(amdsmi_processor_handle, amdsmi_link_metrics_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_process_list_p)(amdsmi_processor_handle, uint32_t *, amdsmi_proc_info_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_ecc_enabled_p)(amdsmi_processor_handle, uint64_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_total_ecc_count_p)(amdsmi_processor_handle, amdsmi_error_count_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_ecc_count_p)(amdsmi_processor_handle, amdsmi_gpu_block_t, amdsmi_error_count_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_ecc_status_p)(amdsmi_processor_handle, amdsmi_gpu_block_t, amdsmi_ras_err_state_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_compute_partition_p)(amdsmi_processor_handle, char *, uint32_t);
extern amdsmi_status_t (*amdsmi_get_gpu_memory_partition_p)(amdsmi_processor_handle, char *, uint32_t);
extern amdsmi_status_t (*amdsmi_get_gpu_accelerator_partition_profile_p)(amdsmi_processor_handle, amdsmi_accelerator_partition_profile_t *, uint32_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_id_p)(amdsmi_processor_handle, uint16_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_revision_p)(amdsmi_processor_handle, uint16_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_subsystem_id_p)(amdsmi_processor_handle, uint16_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_pci_bandwidth_p)(amdsmi_processor_handle, amdsmi_pcie_bandwidth_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_bdf_id_p)(amdsmi_processor_handle, uint64_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_topo_numa_affinity_p)(amdsmi_processor_handle, int32_t *);
extern amdsmi_status_t (*amdsmi_get_energy_count_p)(amdsmi_processor_handle, uint64_t *, float *, uint64_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_power_profile_presets_p)(amdsmi_processor_handle, uint32_t, amdsmi_power_profile_status_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_cache_info_p)(amdsmi_processor_handle, amdsmi_gpu_cache_info_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_mem_overdrive_level_p)(amdsmi_processor_handle, uint32_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_od_volt_curve_regions_p)(amdsmi_processor_handle, uint32_t *, amdsmi_freq_volt_region_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_od_volt_info_p)(amdsmi_processor_handle, amdsmi_od_volt_freq_data_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_overdrive_level_p)(amdsmi_processor_handle, uint32_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_perf_level_p)(amdsmi_processor_handle, amdsmi_dev_perf_level_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_pm_metrics_info_p)(amdsmi_processor_handle, amdsmi_name_value_t **, uint32_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_ras_feature_info_p)(amdsmi_processor_handle, amdsmi_ras_feature_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_reg_table_info_p)(amdsmi_processor_handle, amdsmi_reg_type_t, amdsmi_name_value_t **, uint32_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_volt_metric_p)(amdsmi_processor_handle, amdsmi_voltage_type_t, amdsmi_voltage_metric_t, int64_t *);
extern amdsmi_status_t (*amdsmi_get_gpu_vram_info_p)(amdsmi_processor_handle, amdsmi_vram_info_t *);
extern amdsmi_status_t (*amdsmi_get_pcie_info_p)(amdsmi_processor_handle, amdsmi_pcie_info_t *);
extern amdsmi_status_t (*amdsmi_get_processor_count_from_handles_p)(amdsmi_processor_handle *, uint32_t *, uint32_t *, uint32_t *, uint32_t *);
extern amdsmi_status_t (*amdsmi_get_soc_pstate_p)(amdsmi_processor_handle, amdsmi_dpm_policy_t *);
extern amdsmi_status_t (*amdsmi_get_xgmi_plpd_p)(amdsmi_processor_handle, amdsmi_dpm_policy_t *);

#ifndef AMDSMI_DISABLE_ESMI
extern amdsmi_status_t (*amdsmi_get_cpu_handles_p)(uint32_t *, amdsmi_processor_handle *);
extern amdsmi_status_t (*amdsmi_get_cpucore_handles_p)(uint32_t *, amdsmi_processor_handle *);
extern amdsmi_status_t (*amdsmi_get_cpu_socket_power_p)(amdsmi_processor_handle, uint32_t *);
extern amdsmi_status_t (*amdsmi_get_cpu_socket_power_cap_p)(amdsmi_processor_handle, uint32_t *);
extern amdsmi_status_t (*amdsmi_get_cpu_socket_power_cap_max_p)(amdsmi_processor_handle, uint32_t *);
extern amdsmi_status_t (*amdsmi_get_cpu_core_energy_p)(amdsmi_processor_handle, uint64_t *);
extern amdsmi_status_t (*amdsmi_get_cpu_socket_energy_p)(amdsmi_processor_handle, uint64_t *);
extern amdsmi_status_t (*amdsmi_get_cpu_smu_fw_version_p)(amdsmi_processor_handle, amdsmi_smu_fw_version_t *);
extern amdsmi_status_t (*amdsmi_get_threads_per_core_p)(uint32_t *);
extern amdsmi_status_t (*amdsmi_get_cpu_family_p)(uint32_t *);
extern amdsmi_status_t (*amdsmi_get_cpu_model_p)(uint32_t *);
extern amdsmi_status_t (*amdsmi_get_cpu_core_boostlimit_p)(amdsmi_processor_handle, uint32_t *);
extern amdsmi_status_t (*amdsmi_get_cpu_socket_current_active_freq_limit_p)(amdsmi_processor_handle, uint16_t *, char **);
extern amdsmi_status_t (*amdsmi_get_cpu_socket_freq_range_p)(amdsmi_processor_handle, uint16_t *, uint16_t *);
extern amdsmi_status_t (*amdsmi_get_cpu_core_current_freq_limit_p)(amdsmi_processor_handle, uint32_t *);
extern amdsmi_status_t (*amdsmi_get_minmax_bandwidth_between_processors_p)(amdsmi_processor_handle, amdsmi_processor_handle, uint64_t *, uint64_t *);
extern amdsmi_status_t (*amdsmi_get_cpu_dimm_temp_range_and_refresh_rate_p)(amdsmi_processor_handle, uint8_t, amdsmi_temp_range_refresh_rate_t *);
extern amdsmi_status_t (*amdsmi_get_cpu_dimm_power_consumption_p)(amdsmi_processor_handle, uint8_t, amdsmi_dimm_power_t *);
extern amdsmi_status_t (*amdsmi_get_cpu_dimm_thermal_sensor_p)(amdsmi_processor_handle, uint8_t, amdsmi_dimm_thermal_t *);
#endif

/* Accessor prototypes */
int access_amdsmi_temp_metric(int mode, void *arg);
int access_amdsmi_fan_speed(int mode, void *arg);
int access_amdsmi_fan_rpms(int mode, void *arg);
int access_amdsmi_mem_total(int mode, void *arg);
int access_amdsmi_mem_usage(int mode, void *arg);
int access_amdsmi_power_cap(int mode, void *arg);
int access_amdsmi_power_cap_range(int mode, void *arg);
int access_amdsmi_power_average(int mode, void *arg);
int access_amdsmi_pci_throughput(int mode, void *arg);
int access_amdsmi_pci_replay_counter(int mode, void *arg);
int access_amdsmi_clk_freq(int mode, void *arg);
int access_amdsmi_gpu_metrics(int mode, void *arg);
int access_amdsmi_gpu_info(int mode, void *arg);
int access_amdsmi_gpu_activity(int mode, void *arg);
int access_amdsmi_fan_speed_max(int mode, void *arg);
int access_amdsmi_pci_bandwidth(int mode, void *arg);
int access_amdsmi_energy_count(int mode, void *arg);
int access_amdsmi_power_profile_status(int mode, void *arg);
int access_amdsmi_uuid_hash(int mode, void *arg);
int access_amdsmi_gpu_string_hash(int mode, void *arg);
int access_amdsmi_asic_info(int mode, void *arg);
int access_amdsmi_link_metrics(int mode, void *arg);
int access_amdsmi_process_count(int mode, void *arg);
int access_amdsmi_ecc_total(int mode, void *arg);
#ifdef AMDSMI_HAVE_ENUMERATION_INFO
int access_amdsmi_enumeration_info(int mode, void *arg);
#endif
int access_amdsmi_ecc_enabled_mask(int mode, void *arg);
int access_amdsmi_compute_partition_hash(int mode, void *arg);
int access_amdsmi_memory_partition_hash(int mode, void *arg);
int access_amdsmi_accelerator_num_partitions(int mode, void *arg);
int access_amdsmi_lib_version(int mode, void *arg);
int access_amdsmi_cache_size(int mode, void *arg);
int access_amdsmi_overdrive_level(int mode, void *arg);
int access_amdsmi_mem_overdrive_level(int mode, void *arg);
int access_amdsmi_od_volt_regions_count(int mode, void *arg);
int access_amdsmi_perf_level(int mode, void *arg);
int access_amdsmi_pm_metrics_count(int mode, void *arg);
int access_amdsmi_ras_ecc_schema(int mode, void *arg);
int access_amdsmi_reg_count(int mode, void *arg);
int access_amdsmi_voltage(int mode, void *arg);
int access_amdsmi_vram_width(int mode, void *arg);
int access_amdsmi_soc_pstate_id(int mode, void *arg);
int access_amdsmi_xgmi_plpd_id(int mode, void *arg);

#ifndef AMDSMI_DISABLE_ESMI
int access_amdsmi_cpu_socket_power(int mode, void *arg);
int access_amdsmi_cpu_socket_energy(int mode, void *arg);
int access_amdsmi_cpu_socket_freq_limit(int mode, void *arg);
int access_amdsmi_cpu_socket_freq_range(int mode, void *arg);
int access_amdsmi_cpu_power_cap(int mode, void *arg);
int access_amdsmi_cpu_core_energy(int mode, void *arg);
int access_amdsmi_cpu_core_freq_limit(int mode, void *arg);
int access_amdsmi_cpu_core_boostlimit(int mode, void *arg);
int access_amdsmi_dimm_temp(int mode, void *arg);
int access_amdsmi_dimm_power(int mode, void *arg);
int access_amdsmi_dimm_range_refresh(int mode, void *arg);
int access_amdsmi_threads_per_core(int mode, void *arg);
int access_amdsmi_cpu_family(int mode, void *arg);
int access_amdsmi_cpu_model(int mode, void *arg);
int access_amdsmi_smu_fw_version(int mode, void *arg);
int access_amdsmi_xgmi_bandwidth(int mode, void *arg);
#endif

#endif /* __AMDS_PRIV_H__ */
