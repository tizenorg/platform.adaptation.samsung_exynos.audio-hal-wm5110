/*
 * audio-hal
 *
 * Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <vconf.h>
#include <iniparser.h>

#include "tizen-audio-internal.h"

#define VOLUME_INI_DEFAULT_PATH     "/usr/etc/mmfw_audio_volume.ini"
#define VOLUME_INI_TEMP_PATH        "/opt/system/mmfw_audio_volume.ini"
#define VOLUME_VALUE_MAX            (1.0f)
#define GAIN_VALUE_MAX              (1.0f)

enum {
    STREAM_DEVICE_SPEAKER,
    STREAM_DEVICE_HEADSET,
    STREAM_DEVICE_BLUETOOTH,
    STREAM_DEVICE_HDMI,
    STREAM_DEVICE_DOCK,
    STREAM_DEVICE_MAX,
};

static const char *g_volume_vconf[AUDIO_VOLUME_TYPE_MAX] = {
    "file/private/sound/volume/system",         /* AUDIO_VOLUME_TYPE_SYSTEM */
    "file/private/sound/volume/notification",   /* AUDIO_VOLUME_TYPE_NOTIFICATION */
    "file/private/sound/volume/alarm",          /* AUDIO_VOLUME_TYPE_ALARM */
    "file/private/sound/volume/ringtone",       /* AUDIO_VOLUME_TYPE_RINGTONE */
    "file/private/sound/volume/media",          /* AUDIO_VOLUME_TYPE_MEDIA */
    "file/private/sound/volume/call",           /* AUDIO_VOLUME_TYPE_CALL */
    "file/private/sound/volume/voip",           /* AUDIO_VOLUME_TYPE_VOIP */
    "file/private/sound/volume/voice",          /* AUDIO_VOLUME_TYPE_VOICE */
    "file/private/sound/volume/fixed",          /* AUDIO_VOLUME_TYPE_FIXED */
};

static inline uint8_t __get_volume_dev_index(audio_mgr_t *am, uint32_t volume_type)
{

    switch (am->device.active_out) {
        case AUDIO_DEVICE_OUT_SPEAKER:          return AUDIO_VOLUME_DEVICE_SPEAKER;
        case AUDIO_DEVICE_OUT_RECEIVER:         return AUDIO_VOLUME_DEVICE_RECEIVER;
        case AUDIO_DEVICE_OUT_WIRED_ACCESSORY:  return AUDIO_VOLUME_DEVICE_EARJACK;
        case AUDIO_DEVICE_OUT_BT_SCO:           return AUDIO_VOLUME_DEVICE_BT_SCO;
        case AUDIO_DEVICE_OUT_BT_A2DP:          return AUDIO_VOLUME_DEVICE_BT_A2DP;
        case AUDIO_DEVICE_OUT_DOCK:             return AUDIO_VOLUME_DEVICE_DOCK;
        case AUDIO_DEVICE_OUT_HDMI:             return AUDIO_VOLUME_DEVICE_HDMI;
        case AUDIO_DEVICE_OUT_MIRRORING:        return AUDIO_VOLUME_DEVICE_MIRRORING;
        case AUDIO_DEVICE_OUT_USB_AUDIO:        return AUDIO_VOLUME_DEVICE_USB;
        case AUDIO_DEVICE_OUT_MULTIMEDIA_DOCK:  return AUDIO_VOLUME_DEVICE_MULTIMEDIA_DOCK;
        default:                                return AUDIO_VOLUME_DEVICE_SPEAKER;
    }
}

static const uint8_t __get_stream_dev_index (uint32_t device_out)
{
    switch (device_out) {
    case AUDIO_DEVICE_OUT_SPEAKER:          return STREAM_DEVICE_SPEAKER;
    case AUDIO_DEVICE_OUT_RECEIVER:         return STREAM_DEVICE_SPEAKER;
    case AUDIO_DEVICE_OUT_WIRED_ACCESSORY:  return STREAM_DEVICE_HEADSET;
    case AUDIO_DEVICE_OUT_BT_SCO:           return STREAM_DEVICE_BLUETOOTH;
    case AUDIO_DEVICE_OUT_BT_A2DP:          return STREAM_DEVICE_BLUETOOTH;
    case AUDIO_DEVICE_OUT_DOCK:             return STREAM_DEVICE_DOCK;
    case AUDIO_DEVICE_OUT_HDMI:             return STREAM_DEVICE_HDMI;
    case AUDIO_DEVICE_OUT_MIRRORING:        return STREAM_DEVICE_SPEAKER;
    case AUDIO_DEVICE_OUT_USB_AUDIO:        return STREAM_DEVICE_SPEAKER;
    case AUDIO_DEVICE_OUT_MULTIMEDIA_DOCK:  return STREAM_DEVICE_DOCK;
    default:
        AUDIO_LOG_DEBUG("invalid device_out:%d", device_out);
        break;
    }

    return STREAM_DEVICE_SPEAKER;
}

static const char *__get_device_string_by_idx (uint32_t dev_idx)
{
    switch (dev_idx) {
    case STREAM_DEVICE_SPEAKER:             return "speaker";
    case STREAM_DEVICE_HEADSET:             return "headset";
    case STREAM_DEVICE_BLUETOOTH:           return "btheadset";
    case STREAM_DEVICE_HDMI:                return "hdmi";
    case STREAM_DEVICE_DOCK:                return "dock";
    default:                                return "invalid";
    }
}

static const char *__get_volume_type_string_by_idx (uint32_t vol_type_idx)
{
    switch (vol_type_idx) {
    case AUDIO_VOLUME_TYPE_SYSTEM:          return "system";
    case AUDIO_VOLUME_TYPE_NOTIFICATION:    return "notification";
    case AUDIO_VOLUME_TYPE_ALARM:           return "alarm";
    case AUDIO_VOLUME_TYPE_RINGTONE:        return "ringtone";
    case AUDIO_VOLUME_TYPE_MEDIA:           return "media";
    case AUDIO_VOLUME_TYPE_CALL:            return "call";
    case AUDIO_VOLUME_TYPE_VOIP:            return "voip";
    case AUDIO_VOLUME_TYPE_VOICE:           return "voice";
    case AUDIO_VOLUME_TYPE_FIXED:           return "fixed";
    default:                                return "invalid";
    }
}

static const char *__get_gain_type_string_by_idx (uint32_t gain_type_idx)
{
    switch (gain_type_idx) {
    case AUDIO_GAIN_TYPE_DEFAULT:           return "default";
    case AUDIO_GAIN_TYPE_DIALER:            return "dialer";
    case AUDIO_GAIN_TYPE_TOUCH:             return "touch";
    case AUDIO_GAIN_TYPE_AF:                return "af";
    case AUDIO_GAIN_TYPE_SHUTTER1:          return "shutter1";
    case AUDIO_GAIN_TYPE_SHUTTER2:          return "shutter2";
    case AUDIO_GAIN_TYPE_CAMCODING:         return "camcording";
    case AUDIO_GAIN_TYPE_MIDI:              return "midi";
    case AUDIO_GAIN_TYPE_BOOTING:           return "booting";
    case AUDIO_GAIN_TYPE_VIDEO:             return "video";
    case AUDIO_GAIN_TYPE_TTS:               return "tts";
    default:                                return "invalid";
    }
}

static void __dump_info(char *dump, audio_info_t *info)
{
    int len;
    char name[64] = { '\0', };

    if (info->device.api == AUDIO_DEVICE_API_ALSA) {
        len = snprintf(dump, AUDIO_DUMP_STR_LEN, "device:alsa(%d.%d)", info->device.alsa.card_idx, info->device.alsa.device_idx);
    } else if (info->device.api == AUDIO_DEVICE_API_ALSA) {
        len = snprintf(dump, AUDIO_DUMP_STR_LEN, "device:bluez(%s,nrec:%d)", info->device.bluez.protocol, info->device.bluez.nrec);
    } else {
        len = snprintf(dump, AUDIO_DUMP_STR_LEN, "device:unknown");
    }

    if (len > 0)
        dump += len;

    strncpy(name, info->stream.name ? info->stream.name : "null", sizeof(name)-1);
    len = snprintf(dump, AUDIO_DUMP_STR_LEN, "stream:%s(%dhz,%dch,vol:%s,gain:%s)",
        name, info->stream.samplerate, info->stream.channels,
        __get_volume_type_string_by_idx(info->stream.volume_type), __get_gain_type_string_by_idx(info->stream.gain_type));

    if (len > 0)
        dump += len;

    *dump = '\0';
}

static void __dump_tb (audio_mgr_t *am)
{
    audio_volume_gain_table_t *volume_gain_table = am->stream.volume_gain_table;
    uint32_t dev_idx, vol_type_idx, vol_level_idx, gain_type_idx;
    const char *gain_type_str[] = {
        "def",          /* AUDIO_GAIN_TYPE_DEFAULT */
        "dial",         /* AUDIO_GAIN_TYPE_DIALER */
        "touch",        /* AUDIO_GAIN_TYPE_TOUCH */
        "af",           /* AUDIO_GAIN_TYPE_AF */
        "shut1",        /* AUDIO_GAIN_TYPE_SHUTTER1 */
        "shut2",        /* AUDIO_GAIN_TYPE_SHUTTER2 */
        "cam",          /* AUDIO_GAIN_TYPE_CAMCODING */
        "midi",         /* AUDIO_GAIN_TYPE_MIDI */
        "boot",         /* AUDIO_GAIN_TYPE_BOOTING */
        "video",        /* AUDIO_GAIN_TYPE_VIDEO */
        "tts",          /* AUDIO_GAIN_TYPE_TTS */
    };
    char dump_str[AUDIO_DUMP_STR_LEN], *dump_str_ptr;

    /* Dump volume table */
    AUDIO_LOG_INFO("<<<<< volume table >>>>>");


        for (vol_type_idx = 0; vol_type_idx < AUDIO_VOLUME_TYPE_MAX; vol_type_idx++) {
            const char *vol_type_str = __get_volume_type_string_by_idx(vol_type_idx);

            dump_str_ptr = &dump_str[0];
            memset(dump_str, 0x00, sizeof(char) * sizeof(dump_str));
            snprintf(dump_str_ptr, 8, "%6s:", vol_type_str);
            dump_str_ptr += strlen(dump_str_ptr);

            for (vol_level_idx = 0; vol_level_idx < volume_gain_table->volume_level_max[vol_type_idx]; vol_level_idx++) {
                snprintf(dump_str_ptr, 6, "%01.2f ", volume_gain_table->volume[vol_type_idx][vol_level_idx]);
                dump_str_ptr += strlen(dump_str_ptr);
            }
            AUDIO_LOG_INFO("%s", dump_str);
        }


    /* Dump gain table */
    AUDIO_LOG_INFO("<<<<< gain table >>>>>");

    dump_str_ptr = &dump_str[0];
    memset(dump_str, 0x00, sizeof(char) * sizeof(dump_str));

    snprintf(dump_str_ptr, 11, "%10s", " ");
    dump_str_ptr += strlen(dump_str_ptr);

    for (gain_type_idx = 0; gain_type_idx < AUDIO_GAIN_TYPE_MAX; gain_type_idx++) {
        snprintf(dump_str_ptr, 7, "%5s ", gain_type_str[gain_type_idx]);
        dump_str_ptr += strlen(dump_str_ptr);
    }
    AUDIO_LOG_INFO("%s", dump_str);


        dump_str_ptr = &dump_str[0];
        memset(dump_str, 0x00, sizeof(char) * sizeof(dump_str));

        dump_str_ptr += strlen(dump_str_ptr);

        for (gain_type_idx = 0; gain_type_idx < AUDIO_GAIN_TYPE_MAX; gain_type_idx++) {
            snprintf(dump_str_ptr, 7, "%01.3f ", volume_gain_table->gain[gain_type_idx]);
            dump_str_ptr += strlen(dump_str_ptr);
        }
        AUDIO_LOG_INFO("%s", dump_str);

}

static audio_return_t __load_volume_gain_table_from_ini (audio_mgr_t *am)
{
    dictionary * dict = NULL;
    uint32_t vol_type_idx, vol_level_idx, gain_type_idx;
    audio_volume_gain_table_t *volume_gain_table = am->stream.volume_gain_table;
    int size = 0;
    const char delimiter[] = ", ";
    char *key, *list_str, *token, *ptr = NULL;
    const char *section = "volumes";

    dict = iniparser_load(VOLUME_INI_TEMP_PATH);
    if (!dict) {
        AUDIO_LOG_DEBUG("Use default volume&gain ini file");
        dict = iniparser_load(VOLUME_INI_DEFAULT_PATH);
        if (!dict) {
            AUDIO_LOG_WARN("Loading volume&gain table from ini file failed");
            return AUDIO_ERR_UNDEFINED;
        }
    }

        /* Load volume table */
        for (vol_type_idx = 0; vol_type_idx < AUDIO_VOLUME_TYPE_MAX; vol_type_idx++) {
            const char *vol_type_str = __get_volume_type_string_by_idx(vol_type_idx);

            volume_gain_table->volume_level_max[vol_type_idx] = 0;
        size = strlen(section) + strlen(vol_type_str) + 2;
            key = malloc(size);
            if (key) {
            snprintf(key, size, "%s:%s", section, vol_type_str);
                list_str = iniparser_getstring(dict, key, NULL);
                if (list_str) {
                    token = strtok_r(list_str, delimiter, &ptr);
                    while (token) {
                        /* convert dB volume to linear volume */
                        double vol_value = 0.0f;
                        if(strncmp(token, "0", strlen(token)))
                            vol_value = pow(10.0, (atof(token) - 100) / 20.0);
                        volume_gain_table->volume[vol_type_idx][volume_gain_table->volume_level_max[vol_type_idx]++] = vol_value;
                        token = strtok_r(NULL, delimiter, &ptr);
                    }
                } else {
                     volume_gain_table->volume_level_max[vol_type_idx] = 1;
                    for (vol_level_idx = 0; vol_level_idx < AUDIO_VOLUME_LEVEL_MAX; vol_level_idx++) {
                        volume_gain_table->volume[vol_type_idx][vol_level_idx] = VOLUME_VALUE_MAX;
                    }
                }
                free(key);
            }
        }

        /* Load gain table */
                volume_gain_table->gain[AUDIO_GAIN_TYPE_DEFAULT] = GAIN_VALUE_MAX;
                for (gain_type_idx = AUDIO_GAIN_TYPE_DEFAULT + 1; gain_type_idx < AUDIO_GAIN_TYPE_MAX; gain_type_idx++) {
            const char *gain_type_str = __get_gain_type_string_by_idx(gain_type_idx);

        size = strlen(section) + strlen("gain") + strlen(gain_type_str) + 3;
            key = malloc(size);
            if (key) {
            snprintf(key, size, "%s:gain_%s", section, gain_type_str);
                token = iniparser_getstring(dict, key, NULL);
                    if (token) {
                        volume_gain_table->gain[gain_type_idx] = atof(token);
                    } else {
                        volume_gain_table->gain[gain_type_idx] = GAIN_VALUE_MAX;
                    }
                free(key);
            } else {
                    volume_gain_table->gain[gain_type_idx] = GAIN_VALUE_MAX;
            }
        }

    iniparser_freedict(dict);

    __dump_tb(am);

    return AUDIO_RET_OK;
}

audio_return_t _audio_stream_init (audio_mgr_t *am)
{
    int i, j, val;
    audio_return_t audio_ret = AUDIO_RET_OK;
    int init_value[AUDIO_VOLUME_TYPE_MAX] = { 9, 11, 7, 11, 7, 4, 4, 7, 4, 0 };

    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);

    for (i = 0; i < AUDIO_VOLUME_TYPE_MAX; i++) {
        am->stream.volume_level[i] = init_value[i];
    }

    for (i = 0; i < AUDIO_VOLUME_TYPE_MAX; i++) {
        /* Get volume value string from VCONF */
        if(vconf_get_int(g_volume_vconf[i], &val) < 0) {
            AUDIO_LOG_ERROR("vconf_get_int(%s) failed", g_volume_vconf[i]);
            continue;
        }

        AUDIO_LOG_INFO("read vconf. %s = %d", g_volume_vconf[i], val);

        am->stream.volume_level[i] = val;
    }

    if (!(am->stream.volume_gain_table = malloc(sizeof(audio_volume_gain_table_t)))) {
        AUDIO_LOG_ERROR("volume_gain_table malloc failed");
        return AUDIO_ERR_RESOURCE;
    }

    audio_ret = __load_volume_gain_table_from_ini(am);
    if(audio_ret != AUDIO_RET_OK) {
        AUDIO_LOG_ERROR("gain table load error");
        return AUDIO_ERR_UNDEFINED;
    }

    return audio_ret;
}

audio_return_t _audio_stream_deinit (audio_mgr_t *am)
{
    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);

    if (am->stream.volume_gain_table) {
        free(am->stream.volume_gain_table);
        am->stream.volume_gain_table = NULL;
    }

    return AUDIO_RET_OK;
}

audio_return_t audio_get_volume_level_max (void *userdata, uint32_t volume_type, uint32_t *level)
{
    audio_mgr_t *am = (audio_mgr_t *)userdata;
    audio_volume_gain_table_t *volume_gain_table;

    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);
    AUDIO_RETURN_VAL_IF_FAIL(am->stream.volume_gain_table, AUDIO_ERR_PARAMETER);

    /* Get max volume level by device & type */
    volume_gain_table = am->stream.volume_gain_table;
    *level = volume_gain_table->volume_level_max[volume_type];

    AUDIO_LOG_DEBUG("get_volume_level_max:%s=>%d", __get_volume_type_string_by_idx(volume_type), *level);

    return AUDIO_RET_OK;
}

audio_return_t audio_get_volume_level (void *userdata, uint32_t volume_type, uint32_t *level)
{
    audio_mgr_t *am = (audio_mgr_t *)userdata;

    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);

    *level = am->stream.volume_level[volume_type];

    return AUDIO_RET_OK;
}

audio_return_t audio_get_volume_value (void *userdata, audio_info_t *info, uint32_t volume_type, uint32_t level, double *value)
{
    if (info) {
        audio_mgr_t *am = (audio_mgr_t *)userdata;
        audio_volume_gain_table_t *volume_gain_table;
        char dump_str[AUDIO_DUMP_STR_LEN];

        AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);
        AUDIO_RETURN_VAL_IF_FAIL(am->stream.volume_gain_table, AUDIO_ERR_PARAMETER);
        __dump_info(&dump_str[0], info);
        /* Get basic volume by device & type & level */
        volume_gain_table = am->stream.volume_gain_table;
        if (volume_gain_table->volume_level_max[volume_type] < level)
            *value = VOLUME_VALUE_MAX;
        else
            *value = volume_gain_table->volume[volume_type][level];
        *value *= volume_gain_table->gain[info->stream.gain_type];

        AUDIO_LOG_DEBUG("get_volume_value:%d(%s)=>%f %s", level, __get_volume_type_string_by_idx(volume_type), *value, &dump_str[0]);
    }

    return AUDIO_RET_OK;
}

audio_return_t audio_set_volume_level (void *userdata, audio_info_t *info, uint32_t volume_type, uint32_t level)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    audio_mgr_t *am = (audio_mgr_t *)userdata;

    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);

    if (info == NULL) {

        /* Update volume level */
        am->stream.volume_level[volume_type] = level;
        AUDIO_LOG_INFO("set_volume_level:session(%d), %d(%s)", am->session.session, level, __get_volume_type_string_by_idx(volume_type));
    }

    return audio_ret;
}

audio_return_t audio_get_gain_value (void *userdata, audio_info_t *info, uint32_t volume_type, double *value)
{
    audio_mgr_t *am = (audio_mgr_t *)userdata;
    audio_volume_gain_table_t *volume_gain_table;

    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);
    AUDIO_RETURN_VAL_IF_FAIL(am->stream.volume_gain_table, AUDIO_ERR_PARAMETER);

    if (info != NULL) {
        volume_gain_table = am->stream.volume_gain_table;
        *value = volume_gain_table->gain[info->stream.gain_type];
    }

    return AUDIO_RET_OK;
}

audio_return_t audio_get_mute (void *userdata, audio_info_t *info, uint32_t volume_type, uint32_t direction, uint32_t *mute)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    audio_mgr_t *am = (audio_mgr_t *)userdata;

    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);

    /* TODO. Not implemented */

    return audio_ret;
}

audio_return_t audio_set_mute (void *userdata, audio_info_t *info, uint32_t volume_type, uint32_t direction, uint32_t mute)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    audio_mgr_t *am = (audio_mgr_t *)userdata;

    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);
    /* TODO. Not implemented */

    return audio_ret;
}
