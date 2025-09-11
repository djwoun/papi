#include "amds.h"
#include "amds_priv.h"
#include "htable.h"
#include "papi.h"
#include <string.h>
/* Event enumeration: iterate over native events */
int amds_evt_enum(unsigned int *EventCode, int modifier) {
  if (modifier == PAPI_ENUM_FIRST) {
    if (ntv_table_p->count == 0) {
      return PAPI_ENOEVNT;
    }
    *EventCode = 0;
    return PAPI_OK;
  } else if (modifier == PAPI_ENUM_EVENTS) {
    if (*EventCode + 1 < (unsigned int)ntv_table_p->count) {
      *EventCode = *EventCode + 1;
      return PAPI_OK;
    } else {
      return PAPI_ENOEVNT;
    }
  }
  return PAPI_EINVAL;
}
int amds_evt_code_to_name(unsigned int EventCode, char *name, int len) {
  if (EventCode >= (unsigned int)ntv_table_p->count) {
    return PAPI_EINVAL;
  }
  strncpy(name, ntv_table_p->events[EventCode].name, len);
  name[len-1] = '\0';
  return PAPI_OK;
}
int amds_evt_name_to_code(const char *name, unsigned int *EventCode) {
  native_event_t *event = NULL;
  int hret = htable_find(htable, name, (void **)&event);
  if (hret != HTABLE_SUCCESS) {
    return (hret == HTABLE_ENOVAL) ? PAPI_ENOEVNT : PAPI_ECMP;
  }
  *EventCode = event->id;
  return PAPI_OK;
}
int amds_evt_code_to_descr(unsigned int EventCode, char *descr, int len) {
  if (EventCode >= (unsigned int)ntv_table_p->count) {
    return PAPI_EINVAL;
  }
  strncpy(descr, ntv_table_p->events[EventCode].descr, len);
  descr[len-1] = '\0';
  return PAPI_OK;
}
