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
#include <string.h>
#include <stdbool.h>

#include "tizen-audio-internal.h"

audio_return_t _audio_device_init (audio_mgr_t *am)
{
    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);

    am->device.active_in = AUDIO_DEVICE_IN_NONE;
    am->device.active_out = AUDIO_DEVICE_OUT_NONE;
    am->device.route_flag = AUDIO_ROUTE_FLAG_NONE;
    am->device.pcm_in = NULL;
    am->device.pcm_out = NULL;
    pthread_mutex_init(&am->device.pcm_lock, NULL);
    am->device.pcm_count = 0;

    return AUDIO_RET_OK;
}

audio_return_t _audio_device_deinit (audio_mgr_t *am)
{
    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);

    return AUDIO_RET_OK;
}

static void __load_n_open_device_with_params (audio_mgr_t *am, audio_device_info_t *device_info, int load_only)
{
    audio_device_param_info_t params[AUDIO_DEVICE_PARAM_MAX];
    int dev_param_count = 0;

    AUDIO_RETURN_IF_FAIL(am);
    AUDIO_RETURN_IF_FAIL(device_info);
    AUDIO_RETURN_IF_FAIL(am->cb_intf.load_device);
    AUDIO_RETURN_IF_FAIL(am->cb_intf.open_device);

    memset(&params[0], 0, sizeof(audio_device_param_info_t) * AUDIO_DEVICE_PARAM_MAX);

    if (device_info->api == AUDIO_DEVICE_API_ALSA) {
        device_info->name = malloc(strlen(device_info->alsa.card_name) + 6); /* 6 = "hw: ,idx */
        sprintf(device_info->name, "hw:%s,%d", device_info->alsa.card_name, device_info->alsa.device_idx);
        if (device_info->direction == AUDIO_DIRECTION_OUT) {
            /* ALSA playback */
            switch (device_info->alsa.device_idx) {
                /* default device */
                case 0:
                case 3:
                    device_info->is_default_device = 1;
                    params[dev_param_count].param = AUDIO_DEVICE_PARAM_SUSPEND_TIMEOUT;
                    params[dev_param_count++].u32_v = 1;
                    params[dev_param_count].param = AUDIO_DEVICE_PARAM_TSCHED_BUF_SIZE;
                    params[dev_param_count++].u32_v = 35280;
                    AUDIO_LOG_INFO("HiFi Device");
                    break;
                /* HDMI device */
                case 1:
                    /* VOICE PCM. */
                    break;
                default:
                    AUDIO_LOG_INFO("Unknown Playback Device");
                    break;
            }
        } else if (device_info->direction == AUDIO_DIRECTION_IN) {
            /* ALSA capture */
            switch (device_info->alsa.device_idx) {
                /* default device */
                case 0:
                    device_info->is_default_device = 1;
                    /* use mmap */
                    params[dev_param_count].param = AUDIO_DEVICE_PARAM_USE_MMAP;
                    params[dev_param_count++].u32_v = 1;
                    params[dev_param_count].param = AUDIO_DEVICE_PARAM_SAMPLERATE;
                    params[dev_param_count++].u32_v = 48000;
                    params[dev_param_count].param = AUDIO_DEVICE_PARAM_ALTERNATE_RATE;
                    params[dev_param_count++].u32_v = 48000;
                    break;
                default:
                    AUDIO_LOG_INFO("Unknown Capture Device");
                    break;
            }
        }

        AUDIO_LOG_INFO("open alsa %s device hw:%s,%d", (device_info->direction == AUDIO_DIRECTION_IN) ? "capture" : "playback",
        device_info->alsa.card_name, device_info->alsa.device_idx);
    }

    if (load_only) {
        am->cb_intf.load_device(am->platform_data, device_info, &params[0]);
    } else {
        am->cb_intf.open_device(am->platform_data, device_info, &params[0]);
    }
}

static audio_return_t __set_route_ap_playback_capture (audio_mgr_t *am, uint32_t device_in, uint32_t device_out, uint32_t route_flag)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    int dev_idx = 0;
    const char *verb = NULL;
    const char *devices[MAX_DEVICES] = {NULL,};
    const char *modifiers[MAX_MODIFIERS] = {NULL,};

    verb = AUDIO_USE_CASE_VERB_HIFI;

    if (route_flag & AUDIO_ROUTE_FLAG_MUTE_POLICY) {
        devices[dev_idx++] = AUDIO_USE_CASE_DEV_HEADSET;
    } else if (route_flag & AUDIO_ROUTE_FLAG_DUAL_OUT) {
        devices[dev_idx++] = AUDIO_USE_CASE_DEV_SPEAKER;
        devices[dev_idx++] = AUDIO_USE_CASE_DEV_HEADSET;
        if (device_out == AUDIO_DEVICE_OUT_MIRRORING) {
            AUDIO_LOG_INFO("Skip WFD enable during DUAL path");
        }
    } else {
        switch (device_out) {
            case AUDIO_DEVICE_OUT_SPEAKER:
                 devices[dev_idx++] = AUDIO_USE_CASE_DEV_SPEAKER;
                 break;
            case AUDIO_DEVICE_OUT_RECEIVER:
                devices[dev_idx++] = AUDIO_USE_CASE_DEV_HANDSET;
                break;
            case AUDIO_DEVICE_OUT_WIRED_ACCESSORY:
                devices[dev_idx++] = AUDIO_USE_CASE_DEV_HEADSET;
                break;
            /* even BT SCO is opened by call app, we cannot use BT SCO on HiFi verb */
            case AUDIO_DEVICE_OUT_BT_SCO:
                devices[dev_idx++] = AUDIO_USE_CASE_DEV_SPEAKER;
                break;
            default:
                break;
        }
    }

    if (am->session.is_radio_on == 0 || am->session.is_recording == 1) {
        switch (device_in) {
            case AUDIO_DEVICE_IN_MIC:
                devices[dev_idx++] = AUDIO_USE_CASE_DEV_MAIN_MIC;
                break;
            case AUDIO_DEVICE_IN_WIRED_ACCESSORY:
                devices[dev_idx++] = AUDIO_USE_CASE_DEV_HEADSET_MIC;
                break;
            /* even BT SCO is opened by call app, we cannot use BT SCO on HiFi verb */
            case AUDIO_DEVICE_IN_BT_SCO:
                devices[dev_idx++] = AUDIO_USE_CASE_DEV_MAIN_MIC;
                break;
            default:
                break;
        }
    }

    /* TODO. Handle voice recognition when seperate devices are available */
    audio_ret = _audio_ucm_update_use_case(am, verb, devices, modifiers);
    if (AUDIO_IS_ERROR(audio_ret)) {
        return audio_ret;
    }
    return AUDIO_RET_OK;
}

audio_return_t _set_route_voicecall (audio_mgr_t *am, uint32_t device_in, uint32_t device_out, uint32_t route_flag)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    int dev_idx = 0;
    const char *verb = NULL;
    const char *devices[MAX_DEVICES] = {NULL,};

    verb = AUDIO_USE_CASE_VERB_VOICECALL;

    switch (device_out) {
        case AUDIO_DEVICE_OUT_SPEAKER:
            /* FIXME: WB handling is needed */
            devices[dev_idx++] = AUDIO_USE_CASE_DEV_SPEAKER;
            break;
        case AUDIO_DEVICE_OUT_RECEIVER:
            /* FIXME: WB handling is needed */
            devices[dev_idx++] = AUDIO_USE_CASE_DEV_HANDSET;
            break;
        case AUDIO_DEVICE_OUT_WIRED_ACCESSORY:
            devices[dev_idx++] = AUDIO_USE_CASE_DEV_HEADSET;
            break;
        case AUDIO_DEVICE_OUT_BT_SCO:
            devices[dev_idx++] = AUDIO_USE_CASE_DEV_BT_HEADSET;
            break;
        default:
            break;
    }

    switch (device_in) {
        case AUDIO_DEVICE_IN_MIC:
            devices[dev_idx++] = AUDIO_USE_CASE_DEV_MAIN_MIC;
            break;
        case AUDIO_DEVICE_IN_WIRED_ACCESSORY:
            devices[dev_idx++] = AUDIO_USE_CASE_DEV_HEADSET_MIC;
            break;
        default:
            break;
    }

    /* FIXME. Get network info and configure rate in pcm device */
    audio_ret = _audio_ucm_update_use_case(am, verb, devices, NULL);

    return audio_ret;
}

static audio_return_t __set_route_voip (audio_mgr_t *am, uint32_t device_in, uint32_t device_out, uint32_t route_flag)
{
    int dev_idx = 0;
    const char *verb = NULL;
    const char *devices[MAX_DEVICES] = {NULL,};

    verb = AUDIO_USE_CASE_VERB_HIFI; /* Modify later to use VIRTUALAUDIO to enable echo cancellation */

    switch (device_out) {
        case AUDIO_DEVICE_OUT_SPEAKER:
            devices[dev_idx++] = AUDIO_USE_CASE_DEV_SPEAKER;
            break;
        case AUDIO_DEVICE_OUT_RECEIVER:
            devices[dev_idx++] = AUDIO_USE_CASE_DEV_HANDSET;
            break;
        case AUDIO_DEVICE_OUT_WIRED_ACCESSORY:
            devices[dev_idx++] = AUDIO_USE_CASE_DEV_HEADSET;
            break;
        case AUDIO_DEVICE_OUT_BT_SCO:
            devices[dev_idx++] = AUDIO_USE_CASE_DEV_BT_HEADSET;
            break;
        default:
            break;
    }

    switch (device_in) {
        case AUDIO_DEVICE_IN_MIC:
            devices[dev_idx++] = AUDIO_USE_CASE_DEV_MAIN_MIC;
            break;
        case AUDIO_DEVICE_IN_WIRED_ACCESSORY:
            devices[dev_idx++] = AUDIO_USE_CASE_DEV_HEADSET_MIC;
            break;
        case AUDIO_DEVICE_IN_BT_SCO:
            devices[dev_idx++] = AUDIO_USE_CASE_DEV_BT_MIC;
            break;
        default:
            break;
    }

    return _audio_ucm_update_use_case(am, verb, devices, NULL);
}

audio_return_t _reset_route (audio_mgr_t *am, int need_inactive)
{
    const char *devices[MAX_DEVICES] = {NULL,};
    const char *modifiers[MAX_MODIFIERS] = {NULL,};

    if(need_inactive) {
        _audio_ucm_update_use_case(am, AUDIO_USE_CASE_VERB_INACTIVE, devices, modifiers);
    }
    _audio_ucm_update_use_case(am, AUDIO_USE_CASE_VERB_HIFI, devices, modifiers);
    __set_route_ap_playback_capture(am, am->device.active_in, am->device.active_out, 0);

    return AUDIO_RET_OK;
}

audio_return_t audio_set_route (void *userdata, uint32_t session, uint32_t subsession, uint32_t device_in, uint32_t device_out, uint32_t route_flag)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    audio_mgr_t *am = (audio_mgr_t *)userdata;
    int i, dev_info_count = 0;
    audio_device_info_t device_info_list[AUDIO_DEVICE_INFO_LIST_MAX];

    am->device.active_in = device_in;
    am->device.active_out = device_out;
    am->device.route_flag = route_flag;

    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);

    AUDIO_LOG_INFO("session:%d,%d in:%d out:%d flag:0x%x", session, subsession, device_in, device_out, route_flag);

	if ((session == AUDIO_SESSION_VOICECALL) && (subsession == AUDIO_SUBSESSION_VOICE)) {
        audio_ret = _set_route_voicecall(am, device_in, device_out, route_flag);
        if (AUDIO_IS_ERROR(audio_ret)) {
            AUDIO_LOG_WARN("set voicecall route return 0x%x", audio_ret);
        }
    } else if ((session == AUDIO_SESSION_VOIP) && (subsession == AUDIO_SUBSESSION_VOICE)) {
        audio_ret = __set_route_voip(am, device_in, device_out, route_flag);
        if (AUDIO_IS_ERROR(audio_ret)) {
            AUDIO_LOG_WARN("set voip route return 0x%x", audio_ret);
        }
    } else {
        audio_ret = __set_route_ap_playback_capture(am, device_in, device_out, route_flag);
        if (AUDIO_IS_ERROR(audio_ret)) {
            AUDIO_LOG_WARN("set playback route return 0x%x", audio_ret);
        }
    }

    if (!AUDIO_IS_ERROR(audio_ret)) {
        memset((void *)&device_info_list[0], 0, sizeof(audio_device_info_t) * AUDIO_DEVICE_INFO_LIST_MAX);
        /* fill device params & open device */
        dev_info_count = _audio_ucm_fill_device_info_list(am, &device_info_list[0], NULL);
        for (i = 0; i < dev_info_count; i++) {
            __load_n_open_device_with_params(am, &device_info_list[i], 0);
            }
        }
    return AUDIO_RET_OK;
}

audio_return_t audio_alsa_pcm_open (void *userdata, void **pcm_handle, char *device_name, uint32_t direction, int mode)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    audio_mgr_t *am = (audio_mgr_t *)userdata;
    int err;

    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);

//    pthread_mutex_lock(&am->device.pcm_lock);
   if ((err = snd_pcm_open((snd_pcm_t **)pcm_handle, device_name, (direction == AUDIO_DIRECTION_OUT) ? SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE, mode)) < 0) {
        AUDIO_LOG_ERROR("Error opening PCM device %s: %s", device_name, snd_strerror(err));
        pthread_mutex_unlock(&am->device.pcm_lock);
        return AUDIO_ERR_RESOURCE;
    }
    am->device.pcm_count++;
    AUDIO_LOG_INFO("PCM handle 0x%x(%s,%s) opened(count:%d)", *pcm_handle, device_name, (direction == AUDIO_DIRECTION_OUT) ? "playback" : "capture", am->device.pcm_count);
//    pthread_mutex_unlock(&am->device.pcm_lock);

    return audio_ret;
}

audio_return_t audio_alsa_pcm_close (void *userdata, void *pcm_handle)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    audio_mgr_t *am = (audio_mgr_t *)userdata;
    int err;

    AUDIO_LOG_INFO("Try to close PCM handle 0x%x", pcm_handle);
//    pthread_mutex_lock(&am->device.pcm_lock);
    if ((err = snd_pcm_close(pcm_handle)) < 0) {
        AUDIO_LOG_ERROR("Error closing PCM handle : %s", snd_strerror(err));
        pthread_mutex_unlock(&am->device.pcm_lock);
        return AUDIO_ERR_RESOURCE;
    }

    am->device.pcm_count--;
    AUDIO_LOG_INFO("PCM handle close success (count:%d)", am->device.pcm_count);
//    pthread_mutex_unlock(&am->device.pcm_lock);

    return audio_ret;
}
static int __voice_pcm_set_params (audio_mgr_t *am, snd_pcm_t *pcm)
{
    snd_pcm_hw_params_t *params = NULL;
    int err = 0;
    unsigned int val = 0;

    /* Skip parameter setting to null device. */
    if (snd_pcm_type(pcm) == SND_PCM_TYPE_NULL)
        return AUDIO_ERR_IOCTL;

    /* Allocate a hardware parameters object. */
    snd_pcm_hw_params_alloca(&params);

    /* Fill it in with default values. */
    if (snd_pcm_hw_params_any(pcm, params) < 0) {
        AUDIO_LOG_ERROR("snd_pcm_hw_params_any() : failed! - %s\n", snd_strerror(err));
        goto error;
    }

    /* Set the desired hardware parameters. */
    /* Interleaved mode */
    err = snd_pcm_hw_params_set_access(pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        AUDIO_LOG_ERROR("snd_pcm_hw_params_set_access() : failed! - %s\n", snd_strerror(err));
        goto error;
    }
    err = snd_pcm_hw_params_set_rate(pcm, params, (am->device.route_flag & AUDIO_ROUTE_FLAG_NETWORK_WB) ? 16000 : 8000, 0);
    if (err < 0) {
        AUDIO_LOG_ERROR("snd_pcm_hw_params_set_rate() : failed! - %s\n", snd_strerror(err));
    }
    err = snd_pcm_hw_params(pcm, params);
    if (err < 0) {
        AUDIO_LOG_ERROR("snd_pcm_hw_params() : failed! - %s\n", snd_strerror(err));
        goto error;
    }

    /* Dump current param */
    snd_pcm_hw_params_get_access(params, (snd_pcm_access_t *) &val);
    AUDIO_LOG_DEBUG("access type = %s\n", snd_pcm_access_name((snd_pcm_access_t)val));

    snd_pcm_hw_params_get_format(params, (snd_pcm_format_t*)&val);
    AUDIO_LOG_DEBUG("format = '%s' (%s)\n",
                    snd_pcm_format_name((snd_pcm_format_t)val),
                    snd_pcm_format_description((snd_pcm_format_t)val));

    snd_pcm_hw_params_get_subformat(params, (snd_pcm_subformat_t *)&val);
    AUDIO_LOG_DEBUG("subformat = '%s' (%s)\n",
                    snd_pcm_subformat_name((snd_pcm_subformat_t)val),
                    snd_pcm_subformat_description((snd_pcm_subformat_t)val));

    snd_pcm_hw_params_get_channels(params, &val);
    AUDIO_LOG_DEBUG("channels = %d\n", val);

    return 0;

error:
    return -1;
}

int _voice_pcm_open (audio_mgr_t *am)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    int ret = 0;

    AUDIO_LOG_INFO("open voice pcm handles");

    /* Get playback voice-pcm from ucm conf. Open and set-params */
    if ((audio_ret = audio_alsa_pcm_open((void *)am, (void **)&am->device.pcm_out, VOICE_PCM_DEVICE, AUDIO_DIRECTION_OUT, 0)) < 0) {
        AUDIO_LOG_ERROR("snd_pcm_open for %s failed. %x", VOICE_PCM_DEVICE, audio_ret);
        return AUDIO_ERR_IOCTL;
    }
    ret = __voice_pcm_set_params(am, am->device.pcm_out);

    AUDIO_LOG_INFO("pcm playback device open success device(%s)", VOICE_PCM_DEVICE);

    /* Get capture voice-pcm from ucm conf. Open and set-params */
    if ((audio_ret = audio_alsa_pcm_open((void *)am, (void **)&am->device.pcm_in, VOICE_PCM_DEVICE, AUDIO_DIRECTION_IN, 0)) < 0) {
        AUDIO_LOG_ERROR("snd_pcm_open for %s failed. %x", VOICE_PCM_DEVICE, audio_ret);
        return AUDIO_ERR_IOCTL;
    }
    ret = __voice_pcm_set_params(am, am->device.pcm_in);

    AUDIO_LOG_INFO("pcm captures device open success device(%s)", VOICE_PCM_DEVICE);

    return ret;
}

int _voice_pcm_close (audio_mgr_t *am,int reset)
{
    AUDIO_LOG_INFO("close voice pcm handles");

    if (am->device.pcm_out) {
        audio_alsa_pcm_close((void *)am, am->device.pcm_out);
        am->device.pcm_out = NULL;
        AUDIO_LOG_INFO("pcm playback device close");
    }

    if (am->device.pcm_in) {
        audio_alsa_pcm_close((void *)am, am->device.pcm_in);
        am->device.pcm_in = NULL;
        AUDIO_LOG_INFO("pcm capture device close");
    }
    if (reset)
        _reset_route(am, 1);

    return 0;
}
