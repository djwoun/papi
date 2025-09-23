/**
 * @file    amds_evtapi.c
 * @author  Dong Jun Woun
 *          djwoun@gmail.com
 *
 *  AMD SMI native event API (enumeration, name<->code conversion, descriptors).
 *  This version implements device qualifiers compatible with the ROCm component:
 *   - EventCode packs { device (6 bits), qlmask (1 bit), nameid }.
 *   - Every event requires a :device=<id> qualifier when selecting it by name.
 *   - Valid device ids for a metric are discovered from the event table and
 *     exposed in the long description ("Mandatory device qualifier [ids]").
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "papi.h"
#include "htable.h"

#include "amds.h"
#include "amds_priv.h"

typedef struct {
    int device;
    int flags;
    int nameid;
} amds_event_info_t;

static int amds_strip_device_qualifier_impl(const char *name, char *base, int len);
int amds_strip_device_qualifier_(const char *name, char *base, int len);

static inline int amds_evt_id_to_info(unsigned int event_id, amds_event_info_t *info)
{
    if (!info) return PAPI_EINVAL;
    info->device =  (int)((event_id & AMDS_DEVICE_MASK) >> AMDS_DEVICE_SHIFT);
    info->flags  =  (int)((event_id & AMDS_QLMASK_MASK) >> AMDS_QLMASK_SHIFT);
    info->nameid =  (int)((event_id & AMDS_NAMEID_MASK) >> AMDS_NAMEID_SHIFT);

    native_event_table_t *ntv = amds_get_ntv_table();
    if (!ntv) return PAPI_ECMP;

    if (info->nameid < 0 || info->nameid >= ntv->count) return PAPI_ENOEVNT;
    int devcnt = amds_get_device_count();
    if (info->device < 0 || info->device >= 64 || info->device >= devcnt) return PAPI_ENOEVNT;
    if ((info->flags & AMDS_DEVICE_FLAG) == 0 && info->device > 0) return PAPI_ENOEVNT;

    char base[PAPI_MAX_STR_LEN] = {0};
    if (amds_strip_device_qualifier_(amds_get_ntv_table()->events[info->nameid].name, base, (int)sizeof(base)) != PAPI_OK)
        return PAPI_ENOEVNT;
    int ok = 0;
    for (int i = 0; i < ntv->count; ++i) {
        if (ntv->events[i].device != info->device) continue;
        char b2[PAPI_MAX_STR_LEN] = {0};
        if (amds_strip_device_qualifier_(ntv->events[i].name, b2, (int)sizeof(b2)) != PAPI_OK) continue;
        if (strcmp(base, b2) == 0) { ok = 1; break; }
    }
    return ok ? PAPI_OK : PAPI_ENOEVNT;
}

static inline int amds_evt_id_create(const amds_event_info_t *info, unsigned int *event_id)
{
    if (!info || !event_id) return PAPI_EINVAL;
    unsigned int id = 0;
    id |= ((unsigned int)info->device  << AMDS_DEVICE_SHIFT);
    id |= ((unsigned int)info->flags   << AMDS_QLMASK_SHIFT);
    id |= ((unsigned int)info->nameid  << AMDS_NAMEID_SHIFT);
    *event_id = id;
    return PAPI_OK;
}

static int amds_strip_device_qualifier_impl(const char *name, char *base, int len)
{
    if (!name || !base || len <= 0) return PAPI_EINVAL;
    const char *p = strstr(name, ":device=");
    if (!p) {
        if ((int)strlen(name) >= len) return PAPI_EBUF;
        strncpy(base, name, (size_t)len);
        return PAPI_OK;
    }
    const char *head = name;
    const char *digits = p + 8;
    while (*digits >= '0' && *digits <= '9') digits++;
    int need_colon = 0;
    if (*digits == ',') { digits++; need_colon = 1; }
    int pos = 0;
    int head_len = (int)(p - head);
    if (head_len >= len) return PAPI_EBUF;
    memcpy(base + pos, head, (size_t)head_len);
    pos += head_len;
    if (need_colon && *digits != '\0') {
        if (pos + 1 >= len) return PAPI_EBUF;
        base[pos++] = ':';
    }
    int rem = (int)strlen(digits);
    if (pos + rem >= len) rem = len - 1 - pos;
    if (rem > 0) memcpy(base + pos, digits, (size_t)rem);
    pos += rem;
    base[pos] = '\0';
    return PAPI_OK;
}

int amds_strip_device_qualifier_(const char *name, char *base, int len)
{
    return amds_strip_device_qualifier_impl(name, base, len);
}

static int amds_collect_supported_devices(const char *base, char *out, int outlen)
{
    if (!base || !out || outlen <= 0) return PAPI_EINVAL;
    native_event_table_t *ntv = amds_get_ntv_table();
    if (!ntv) return PAPI_ECMP;
    unsigned long long seen = 0;
    int pos = 0;
    for (int i = 0; i < ntv->count; ++i) {
        char b2[PAPI_MAX_STR_LEN] = {0};
        if (amds_strip_device_qualifier_impl(ntv->events[i].name, b2, (int)sizeof(b2)) != PAPI_OK) continue;
        if (strcmp(base, b2) != 0) continue;
        int d = ntv->events[i].device;
        if (d < 0 || d >= 64) continue;
        if ((seen >> d) & 1ULL) continue;
        seen |= (1ULL << d);
        int n = snprintf(out + pos, (size_t)(outlen - pos), "%d,", d);
        if (n < 0 || n >= outlen - pos) { out[outlen-1] = 0; break; }
        pos += n;
    }
    if (pos > 0) out[pos-1] = '\0'; else out[0] = '\0';
    return PAPI_OK;
}

static int amds_name_to_device(const char *name, int *device)
{
    if (!name || !device) return PAPI_EINVAL;
    const char *p = strstr(name, ":device=");
    if (!p) return PAPI_ENOEVNT;
    const char *digits = p + 8;
    if (!isdigit((unsigned char)*digits)) return PAPI_ENOEVNT;
    char *endptr = NULL;
    long v = strtol(digits, &endptr, 10);
    if (endptr == digits) return PAPI_ENOEVNT;
    if (v < 0 || v >= 64) return PAPI_ENOEVNT;
    *device = (int)v;
    if (strstr(digits, ":device=")) return PAPI_ENOEVNT;
    return PAPI_OK;
}

int amds_evt_enum(unsigned int *EventCode, int modifier)
{
    native_event_table_t *ntv = amds_get_ntv_table();
    if (!EventCode) return PAPI_EINVAL;
    if (!ntv || ntv->count <= 0) return PAPI_ENOEVNT;
    amds_event_info_t info;
    switch (modifier) {
        case PAPI_ENUM_FIRST:
            info.device = 0; info.flags = 0; info.nameid = 0;
            return amds_evt_id_create(&info, EventCode);
        case PAPI_ENUM_EVENTS:
        {
            if (amds_evt_id_to_info(*EventCode, &info) != PAPI_OK) return PAPI_EINVAL;
            char base_cur[PAPI_MAX_STR_LEN] = {0};
            if (amds_strip_device_qualifier_impl(ntv->events[info.nameid].name, base_cur, (int)sizeof(base_cur)) != PAPI_OK)
                return PAPI_EINVAL;
            int next = -1;
            for (int i = info.nameid + 1; i < ntv->count; ++i) {
                char base_i[PAPI_MAX_STR_LEN] = {0};
                if (amds_strip_device_qualifier_impl(ntv->events[i].name, base_i, (int)sizeof(base_i)) != PAPI_OK) continue;
                if (strcmp(base_cur, base_i) != 0) { next = i; break; }
            }
            if (next < 0) return PAPI_ENOEVNT;
            info.device = 0; info.flags = 0; info.nameid = next;
            return amds_evt_id_create(&info, EventCode);
        }
        case PAPI_NTV_ENUM_UMASKS:
        {
            if (amds_evt_id_to_info(*EventCode, &info) != PAPI_OK) return PAPI_EINVAL;
            if ((info.flags & AMDS_DEVICE_FLAG) == 0) {
                info.flags = AMDS_DEVICE_FLAG;
                return amds_evt_id_create(&info, EventCode);
            }
            return PAPI_ENOEVNT;
        }
        default:
            return PAPI_EINVAL;
    }
}

int amds_evt_code_to_name(unsigned int EventCode, char *name, int len)
{
    if (!name || len <= 0) return PAPI_EINVAL;
    native_event_table_t *ntv = amds_get_ntv_table();
    if (!ntv) return PAPI_ECMP;
    amds_event_info_t info;
    int ret = amds_evt_id_to_info(EventCode, &info);
    if (ret != PAPI_OK) return ret;
    char base[PAPI_MAX_STR_LEN] = {0};
    ret = amds_strip_device_qualifier_impl(ntv->events[info.nameid].name, base, (int)sizeof(base));
    if (ret != PAPI_OK) return ret;
    if (info.flags & AMDS_DEVICE_FLAG) snprintf(name, (size_t)len, "%s:device=%d", base, info.device);
    else snprintf(name, (size_t)len, "%s", base);
    return PAPI_OK;
}

int amds_evt_name_to_code(const char *name, unsigned int *EventCode)
{
    if (!name || !EventCode) return PAPI_EINVAL;
    native_event_table_t *ntv = amds_get_ntv_table();
    if (!ntv) return PAPI_ECMP;
    int device;
    int ret = amds_name_to_device(name, &device);
    if (ret != PAPI_OK) return ret;
    char base[PAPI_MAX_STR_LEN] = {0};
    ret = amds_strip_device_qualifier_impl(name, base, (int)sizeof(base));
    if (ret != PAPI_OK) return ret;
    int nameid = -1;
    for (int i = 0; i < ntv->count; ++i) {
        char b2[PAPI_MAX_STR_LEN] = {0};
        if (amds_strip_device_qualifier_impl(ntv->events[i].name, b2, (int)sizeof(b2)) != PAPI_OK) continue;
        if (strcmp(base, b2) == 0) { nameid = i; break; }
    }
    if (nameid < 0) return PAPI_ENOEVNT;
    amds_event_info_t info = { device, AMDS_DEVICE_FLAG, nameid };
    ret = amds_evt_id_create(&info, EventCode);
    if (ret != PAPI_OK) return ret;
    return amds_evt_id_to_info(*EventCode, &info);
}

int amds_evt_code_to_descr(unsigned int EventCode, char *descr, int len)
{
    if (!descr || len <= 0) return PAPI_EINVAL;
    native_event_table_t *ntv = amds_get_ntv_table();
    if (!ntv) return PAPI_ECMP;
    amds_event_info_t info;
    int ret = amds_evt_id_to_info(EventCode, &info);
    if (ret != PAPI_OK) return ret;
    snprintf(descr, (size_t)len, "%s", ntv->events[info.nameid].descr);
    return PAPI_OK;
}

int amds_evt_code_to_info(unsigned int EventCode, PAPI_event_info_t *info)
{
    if (!info) return PAPI_EINVAL;
    native_event_table_t *ntv = amds_get_ntv_table();
    if (!ntv) return PAPI_ECMP;
    amds_event_info_t inf;
    int ret = amds_evt_id_to_info(EventCode, &inf);
    if (ret != PAPI_OK) return ret;
    char base[PAPI_MAX_STR_LEN] = {0};
    ret = amds_strip_device_qualifier_impl(ntv->events[inf.nameid].name, base, (int)sizeof(base));
    if (ret != PAPI_OK) return ret;
    char devices[PAPI_MAX_STR_LEN] = {0};
    amds_collect_supported_devices(base, devices, (int)sizeof(devices));
    if (devices[0] == '\0') return PAPI_ENOEVNT;
    switch (inf.flags) {
        case 0:
            snprintf(info->symbol, sizeof(info->symbol), "%s", base);
            snprintf(info->long_descr, sizeof(info->long_descr), "%s", ntv->events[inf.nameid].descr);
            break;
        case AMDS_DEVICE_FLAG:
            snprintf(info->symbol, sizeof(info->symbol), "%s:device=%d", base, inf.device);
            snprintf(info->long_descr, sizeof(info->long_descr), "%s, masks:Mandatory device qualifier [%s]",
                     ntv->events[inf.nameid].descr, devices);
            break;
        default:
            return PAPI_EINVAL;
    }
    return PAPI_OK;
}

