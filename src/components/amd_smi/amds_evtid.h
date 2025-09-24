#ifndef __AMDS_EVTID_H__
#define __AMDS_EVTID_H__

#include <stdint.h>

/* Event identifier encoding format (64-bit):
 * |        unused       | dev  | ql |   nameid   |
 * |---------------------|------|----|------------|
 * unused    : (64 - DEVICE_WIDTH - QLMASK_WIDTH - NAMEID_WIDTH) bits
 * device    : DEVICE_WIDTH bits  (device index qualifier)
 * qlmask    : QLMASK_WIDTH bits  (qualifier flags)
 * nameid    : NAMEID_WIDTH bits  (base event index)
 */
#define EVENTS_WIDTH (sizeof(uint64_t) * 8)
#define DEVICE_WIDTH (7)
#define NAMEID_WIDTH (12)
#define QLMASK_WIDTH (2)
#define UNUSED_WIDTH (EVENTS_WIDTH - DEVICE_WIDTH - NAMEID_WIDTH - QLMASK_WIDTH)
#define DEVICE_SHIFT (EVENTS_WIDTH - UNUSED_WIDTH - DEVICE_WIDTH)
#define QLMASK_SHIFT (DEVICE_SHIFT - QLMASK_WIDTH)
#define NAMEID_SHIFT (QLMASK_SHIFT - NAMEID_WIDTH)
#define DEVICE_MASK  ((0xFFFFFFFFFFFFFFFFULL >> (EVENTS_WIDTH - DEVICE_WIDTH)) << DEVICE_SHIFT)
#define QLMASK_MASK  ((0xFFFFFFFFFFFFFFFFULL >> (EVENTS_WIDTH - QLMASK_WIDTH)) << QLMASK_SHIFT)
#define NAMEID_MASK  ((0xFFFFFFFFFFFFFFFFULL >> (EVENTS_WIDTH - NAMEID_WIDTH)) << NAMEID_SHIFT)
#define DEVICE_FLAG  (0x2)

typedef struct {
  int device;
  int flags;
  int nameid;
} amds_evtinfo_t;

int amds_evt_id_create(const amds_evtinfo_t *info, uint64_t *event_id);
int amds_evt_id_to_info(uint64_t event_id, amds_evtinfo_t *info);

#endif /* __AMDS_EVTID_H__ */
