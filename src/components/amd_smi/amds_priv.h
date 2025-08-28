#ifndef __AMDS_PRIV_H__
#define __AMDS_PRIV_H__

#define AMDSMI_DISABLE_ESMI

#include "amdsmi.h"
#include <stdint.h>

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
#include "amds_funcs.h"
#define DECLARE_AMDSMI(name, ret, args) extern ret(*name) args;
AMD_SMI_GPU_FUNCTIONS(DECLARE_AMDSMI)
#ifndef AMDSMI_DISABLE_ESMI
AMD_SMI_CPU_FUNCTIONS(DECLARE_AMDSMI)
#endif
#undef DECLARE_AMDSMI

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
int access_amdsmi_enumeration_info(int mode, void *arg);
int access_amdsmi_asic_info(int mode, void *arg);
int access_amdsmi_link_metrics(int mode, void *arg);
int access_amdsmi_process_info(int mode, void *arg);
int access_amdsmi_ecc_total(int mode, void *arg);
int access_amdsmi_ecc_block(int mode, void *arg);
int access_amdsmi_ecc_status(int mode, void *arg);
int access_amdsmi_ecc_enabled_mask(int mode, void *arg);
int access_amdsmi_compute_partition_hash(int mode, void *arg);
int access_amdsmi_memory_partition_hash(int mode, void *arg);
int access_amdsmi_accelerator_num_partitions(int mode, void *arg);
int access_amdsmi_lib_version(int mode, void *arg);
int access_amdsmi_cache_stat(int mode, void *arg);
int access_amdsmi_overdrive_level(int mode, void *arg);
int access_amdsmi_mem_overdrive_level(int mode, void *arg);
int access_amdsmi_od_volt_regions_count(int mode, void *arg);
int access_amdsmi_od_volt_curve_range(int mode, void *arg);
int access_amdsmi_od_volt_info(int mode, void *arg);
int access_amdsmi_perf_level(int mode, void *arg);
int access_amdsmi_pm_metrics_count(int mode, void *arg);
int access_amdsmi_pm_metric_value(int mode, void *arg);
int access_amdsmi_ras_ecc_schema(int mode, void *arg);
int access_amdsmi_ras_eeprom_version(int mode, void *arg);
int access_amdsmi_ras_block_state(int mode, void *arg);
int access_amdsmi_reg_count(int mode, void *arg);
int access_amdsmi_reg_value(int mode, void *arg);
int access_amdsmi_voltage(int mode, void *arg);
int access_amdsmi_vram_width(int mode, void *arg);
int access_amdsmi_vram_size(int mode, void *arg);
int access_amdsmi_vram_type(int mode, void *arg);
int access_amdsmi_vram_vendor(int mode, void *arg);
int access_amdsmi_vram_usage(int mode, void *arg);
int access_amdsmi_soc_pstate_id(int mode, void *arg);
int access_amdsmi_soc_pstate_supported(int mode, void *arg);
int access_amdsmi_xgmi_plpd_id(int mode, void *arg);
int access_amdsmi_xgmi_plpd_supported(int mode, void *arg);
int access_amdsmi_process_isolation(int mode, void *arg);
int access_amdsmi_xcd_counter(int mode, void *arg);
int access_amdsmi_board_serial_hash(int mode, void *arg);
int access_amdsmi_vram_max_bandwidth(int mode, void *arg);
int access_amdsmi_fw_version(int mode, void *arg);
int access_amdsmi_bad_page_count(int mode, void *arg);
int access_amdsmi_bad_page_threshold(int mode, void *arg);
int access_amdsmi_bad_page_record(int mode, void *arg);
int access_amdsmi_power_sensor(int mode, void *arg);
int access_amdsmi_pcie_info(int mode, void *arg);
int access_amdsmi_event_notification(int mode, void *arg);

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
