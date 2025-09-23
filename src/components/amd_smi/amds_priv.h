/**
 * @file    amds_priv.h
 * @author  Dong Jun Woun 
 *          djwoun@gmail.com
 *
 */

#ifndef __AMDS_PRIV_H__
#define __AMDS_PRIV_H__

#define AMDSMI_DISABLE_ESMI

#include <amd_smi/amdsmi.h>
#include <stdint.h>

/* -----------------------------------------------------------------------
 * Event identifier encoding format (amd_smi):
 * +-----------------------------+--------+--------+
 * |            unused           | device | qlmask | nameid
 * +-----------------------------+--------+--------+
 * device : 6 bits ([0 - 63] devices)
 * qlmask : 1 bit  (qualifier mask; only 'device' supported)
 * nameid : remaining lower bits (index into native event table)
 * ---------------------------------------------------------------------*/
#ifndef AMDS_EVENTS_WIDTH
#define AMDS_EVENTS_WIDTH (sizeof(unsigned int) * 8)
#endif
#define AMDS_DEVICE_WIDTH (6)
#define AMDS_QLMASK_WIDTH (1)
#define AMDS_NAMEID_WIDTH (AMDS_EVENTS_WIDTH - AMDS_DEVICE_WIDTH - AMDS_QLMASK_WIDTH)
#define AMDS_UNUSED_WIDTH (AMDS_EVENTS_WIDTH - AMDS_DEVICE_WIDTH - AMDS_QLMASK_WIDTH - AMDS_NAMEID_WIDTH)

#define AMDS_DEVICE_SHIFT (AMDS_EVENTS_WIDTH - AMDS_UNUSED_WIDTH - AMDS_DEVICE_WIDTH)
#define AMDS_QLMASK_SHIFT (AMDS_DEVICE_SHIFT   - AMDS_QLMASK_WIDTH)
#define AMDS_NAMEID_SHIFT (AMDS_QLMASK_SHIFT   - AMDS_NAMEID_WIDTH)

#define AMDS_DEVICE_MASK  (((unsigned int)0xFFFFFFFFu >> (AMDS_EVENTS_WIDTH - AMDS_DEVICE_WIDTH)) << AMDS_DEVICE_SHIFT)
#define AMDS_QLMASK_MASK  (((unsigned int)0xFFFFFFFFu >> (AMDS_EVENTS_WIDTH - AMDS_QLMASK_WIDTH)) << AMDS_QLMASK_SHIFT)
#define AMDS_NAMEID_MASK  (((unsigned int)0xFFFFFFFFu >> (AMDS_EVENTS_WIDTH - AMDS_NAMEID_WIDTH)) << AMDS_NAMEID_SHIFT)

/* Qualifier flags (qlmask bitfield) */
#define AMDS_DEVICE_FLAG  (0x1u)

#ifndef AMDSMI_LIB_VERSION_MAJOR
#define AMDSMI_LIB_VERSION_MAJOR 0
#endif

/* Access mode used by accessors */
typedef enum {
  PAPI_MODE_READ = 1,
  PAPI_MODE_WRITE,
  PAPI_MODE_RDWR,
} rocs_access_mode_e;

typedef int (*amds_accessor_t)(int mode, void *arg);

/* Native event descriptor */
typedef struct native_event {
  unsigned int id;
  char *name, *descr;
  int32_t device;
  uint64_t value;
  uint32_t mode, variant, subvariant;
  void *priv;
  int (*open_func)(struct native_event *);
  int (*close_func)(struct native_event *);
  int (*start_func)(struct native_event *);
  int (*stop_func)(struct native_event *);
  amds_accessor_t access_func;
} native_event_t;

typedef struct native_event_table {
  native_event_t *events;
  int count;
} native_event_table_t;

/* Forward accessors defined in amds.c */
native_event_table_t *amds_get_ntv_table(void);
int32_t              amds_get_device_count(void);
void                *amds_get_htable(void);
uint32_t             amds_get_lib_major(void);
amdsmi_processor_handle *amds_get_device_handles(void);
int32_t                 amds_get_gpu_count(void);
int32_t                 amds_get_cpu_count(void);
amdsmi_processor_handle **amds_get_cpu_core_handles(void);
uint32_t                *amds_get_cores_per_socket(void);

#ifndef AMDS_PRIV_IMPL
#define device_handles (amds_get_device_handles())
#define device_count (amds_get_device_count())
#define gpu_count (amds_get_gpu_count())
#define cpu_count (amds_get_cpu_count())
#define cpu_core_handles (amds_get_cpu_core_handles())
#define cores_per_socket (amds_get_cores_per_socket())
#define htable (amds_get_htable())
#define ntv_table_p (amds_get_ntv_table())
#define amdsmi_lib_major (amds_get_lib_major())
#endif

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
int access_amdsmi_clock_info(int mode, void *arg);
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
int access_amdsmi_link_weight(int mode, void *arg);
int access_amdsmi_link_type(int mode, void *arg);
int access_amdsmi_p2p_status(int mode, void *arg);
int access_amdsmi_p2p_accessible(int mode, void *arg);
int access_amdsmi_link_topology_nearest(int mode, void *arg);
int access_amdsmi_topo_numa(int mode, void *arg);
int access_amdsmi_device_bdf(int mode, void *arg);
int access_amdsmi_kfd_info(int mode, void *arg);
int access_amdsmi_xgmi_info(int mode, void *arg);
int access_amdsmi_process_info(int mode, void *arg);
int access_amdsmi_ecc_total(int mode, void *arg);
int access_amdsmi_ecc_block(int mode, void *arg);
int access_amdsmi_ecc_status(int mode, void *arg);
int access_amdsmi_ecc_enabled_mask(int mode, void *arg);
int access_amdsmi_compute_partition_hash(int mode, void *arg);
int access_amdsmi_memory_partition_hash(int mode, void *arg);
int access_amdsmi_memory_reserved_pages(int mode, void *arg);
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
int access_amdsmi_pm_enabled(int mode, void *arg);
int access_amdsmi_ras_ecc_schema(int mode, void *arg);
int access_amdsmi_ras_eeprom_version(int mode, void *arg);
int access_amdsmi_ras_eeprom_validate(int mode, void *arg);
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
int access_amdsmi_metrics_header_info(int mode, void *arg);
int access_amdsmi_xgmi_error_status(int mode, void *arg);
int access_amdsmi_xgmi_plpd_id(int mode, void *arg);
int access_amdsmi_xgmi_plpd_supported(int mode, void *arg);
int access_amdsmi_process_isolation(int mode, void *arg);
int access_amdsmi_xcd_counter(int mode, void *arg);
int access_amdsmi_board_serial_hash(int mode, void *arg);
int access_amdsmi_fw_version(int mode, void *arg);
int access_amdsmi_bad_page_count(int mode, void *arg);
int access_amdsmi_bad_page_threshold(int mode, void *arg);
int access_amdsmi_bad_page_record(int mode, void *arg);
int access_amdsmi_power_sensor(int mode, void *arg);
int access_amdsmi_pcie_info(int mode, void *arg);
int access_amdsmi_event_notification(int mode, void *arg);
int access_amdsmi_xgmi_bandwidth(int mode, void *arg);
int access_amdsmi_utilization_count(int mode, void *arg);
int access_amdsmi_violation_status(int mode, void *arg);

/* Consolidated AMDSMI_LIB_VERSION_MAJOR >= 25 block */
#if AMDSMI_LIB_VERSION_MAJOR >= 25
int access_amdsmi_enumeration_info(int mode, void *arg);
int access_amdsmi_memory_partition_config(int mode, void *arg);
int access_amdsmi_xgmi_link_status(int mode, void *arg);
int access_amdsmi_vram_max_bandwidth(int mode, void *arg);
#endif

#ifndef AMDSMI_DISABLE_ESMI
int access_amdsmi_cpu_socket_power(int mode, void *arg);
int access_amdsmi_cpu_socket_energy(int mode, void *arg);
int access_amdsmi_cpu_socket_freq_limit(int mode, void *arg);
int access_amdsmi_cpu_socket_freq_range(int mode, void *arg);
int access_amdsmi_cpu_power_cap(int mode, void *arg);
int access_amdsmi_cpu_core_energy(int mode, void *arg);
int access_amdsmi_cpu_core_freq_limit(int mode, void *arg);
int access_amdsmi_cpu_core_boostlimit(int mode, void *arg);
int access_amdsmi_cpu_cclk_limit(int mode, void *arg);
int access_amdsmi_cpu_io_bw(int mode, void *arg);
int access_amdsmi_cpu_xgmi_bw(int mode, void *arg);
int access_amdsmi_cpu_ddr_bw(int mode, void *arg);
int access_amdsmi_cpu_fclk_mclk(int mode, void *arg);
int access_amdsmi_cpu_hsmp_driver_version(int mode, void *arg);
int access_amdsmi_cpu_hsmp_proto_ver(int mode, void *arg);
int access_amdsmi_cpu_prochot_status(int mode, void *arg);
int access_amdsmi_cpu_svi_power(int mode, void *arg);
int access_amdsmi_dimm_temp(int mode, void *arg);
int access_amdsmi_dimm_power(int mode, void *arg);
int access_amdsmi_dimm_range_refresh(int mode, void *arg);
int access_amdsmi_threads_per_core(int mode, void *arg);
int access_amdsmi_cpu_family(int mode, void *arg);
int access_amdsmi_cpu_model(int mode, void *arg);
int access_amdsmi_smu_fw_version(int mode, void *arg);
#endif

#endif /* __AMDS_PRIV_H__ */
