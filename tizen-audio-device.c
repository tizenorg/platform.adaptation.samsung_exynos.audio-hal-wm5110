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

/* #define DEBUG_TIMING */

static device_type_t outDeviceTypes[] = {
    { AUDIO_DEVICE_OUT_SPEAKER, "Speaker" },
    { AUDIO_DEVICE_OUT_RECEIVER, "Earpiece" },
    { AUDIO_DEVICE_OUT_JACK, "Headphones" },
    { AUDIO_DEVICE_OUT_BT_SCO, "Bluetooth" },
    { AUDIO_DEVICE_OUT_AUX, "Line" },
    { AUDIO_DEVICE_OUT_HDMI, "HDMI" },
    { 0, 0 },
};

static device_type_t inDeviceTypes[] = {
    { AUDIO_DEVICE_IN_MAIN_MIC, "MainMic" },
    { AUDIO_DEVICE_IN_SUB_MIC, "SubMic" },
    { AUDIO_DEVICE_IN_JACK, "HeadsetMic" },
    { AUDIO_DEVICE_IN_BT_SCO, "BT Mic" },
    { 0, 0 },
};

static uint32_t convert_device_string_to_enum(const char* device_str, uint32_t direction)
{
    uint32_t device = 0;

    if (!strncmp(device_str,"builtin-speaker", MAX_NAME_LEN)) {
        device = AUDIO_DEVICE_OUT_SPEAKER;
    } else if (!strncmp(device_str,"builtin-receiver", MAX_NAME_LEN)) {
        device = AUDIO_DEVICE_OUT_RECEIVER;
    } else if ((!strncmp(device_str,"audio-jack", MAX_NAME_LEN)) && (direction == AUDIO_DIRECTION_OUT)) {
        device = AUDIO_DEVICE_OUT_JACK;
    } else if ((!strncmp(device_str,"bt", MAX_NAME_LEN)) && (direction == AUDIO_DIRECTION_OUT)) {
        device = AUDIO_DEVICE_OUT_BT_SCO;
    } else if (!strncmp(device_str,"aux", MAX_NAME_LEN)) {
        device = AUDIO_DEVICE_OUT_AUX;
    } else if (!strncmp(device_str,"hdmi", MAX_NAME_LEN)) {
        device = AUDIO_DEVICE_OUT_HDMI;
    } else if ((!strncmp(device_str,"builtin-mic", MAX_NAME_LEN))) {
        device = AUDIO_DEVICE_IN_MAIN_MIC;
    /* To Do : SUB_MIC */
    } else if ((!strncmp(device_str,"audio-jack", MAX_NAME_LEN)) && (direction == AUDIO_DIRECTION_IN)) {
        device = AUDIO_DEVICE_IN_JACK;
    } else if ((!strncmp(device_str,"bt", MAX_NAME_LEN)) && (direction == AUDIO_DIRECTION_IN)) {
        device = AUDIO_DEVICE_IN_BT_SCO;
    } else {
        device = AUDIO_DEVICE_NONE;
    }
    AUDIO_LOG_INFO("device type(%s), enum(0x%x)", device_str, device);
    return device;
}

static audio_return_t set_devices(audio_mgr_t *am, const char *verb, device_info_t *devices, uint32_t num_of_devices)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    uint32_t new_device = 0;
    const char *active_devices[MAX_DEVICES] = {NULL,};
    int i = 0, j = 0, dev_idx = 0;

    if (num_of_devices > MAX_DEVICES) {
        num_of_devices = MAX_DEVICES;
        AUDIO_LOG_ERROR("error: num_of_devices");
        return AUDIO_ERR_PARAMETER;
    }

    if ((devices[0].direction == AUDIO_DIRECTION_OUT) && am->device.active_in) {
        /* check the active in devices */
        for (j = 0; j < inDeviceTypes[j].type; j++) {
            if (((am->device.active_in & (~0x80000000)) & inDeviceTypes[j].type))
                active_devices[dev_idx++] = inDeviceTypes[j].name;
        }
    } else if ((devices[0].direction == AUDIO_DIRECTION_IN) && am->device.active_out) {
        /* check the active out devices */
        for (j = 0; j < outDeviceTypes[j].type; j++) {
            if (am->device.active_out & outDeviceTypes[j].type)
                active_devices[dev_idx++] = outDeviceTypes[j].name;
        }
    }

    for (i = 0; i < num_of_devices; i++) {
        new_device = convert_device_string_to_enum(devices[i].type, devices[i].direction);
        if (new_device & 0x80000000) {
            for (j = 0; j < inDeviceTypes[j].type; j++) {
                if (new_device == inDeviceTypes[j].type) {
                    active_devices[dev_idx++] = inDeviceTypes[j].name;
                    am->device.active_in |= new_device;
                }
            }
        } else {
            for (j = 0; j < outDeviceTypes[j].type; j++) {
                if (new_device == outDeviceTypes[j].type) {
                    active_devices[dev_idx++] = outDeviceTypes[j].name;
                    am->device.active_out |= new_device;
                }
            }
        }
    }

    if (active_devices[0] == NULL) {
        AUDIO_LOG_ERROR("Failed to set device: active device is NULL");
        return AUDIO_ERR_PARAMETER;
    }

    audio_ret = _audio_ucm_set_devices(am, verb, active_devices);
    if (audio_ret) {
        AUDIO_LOG_ERROR("Failed to set device: error = %d", audio_ret);
        return audio_ret;
    }
    return audio_ret;

}

audio_return_t _audio_device_init (audio_mgr_t *am)
{
    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);

    am->device.active_in = 0x0;
    am->device.active_out = 0x0;
    am->device.route_flag = AUDIO_ROUTE_FLAG_NONE;
    am->device.pcm_in = NULL;
    am->device.pcm_out = NULL;
    am->device.mode = VERB_NORMAL;
    pthread_mutex_init(&am->device.pcm_lock, NULL);
    am->device.pcm_count = 0;

    return AUDIO_RET_OK;
}

audio_return_t _audio_device_deinit (audio_mgr_t *am)
{
    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);

    return AUDIO_RET_OK;
}

static audio_return_t _do_route_ap_playback_capture (audio_mgr_t *am, audio_route_info_t *route_info)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    device_info_t *devices = route_info->device_infos;
    const char *verb = NULL;

    /* To Do: Set modifiers */
    /* int mod_idx = 0; */
    /* const char *modifiers[MAX_MODIFIERS] = {NULL,}; */

    verb = AUDIO_USE_CASE_VERB_HIFI;
    AUDIO_LOG_INFO("do_route_ap_playback_capture++ ");
    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);

    audio_ret = set_devices(am, verb, devices, route_info->num_of_devices);
    if (audio_ret) {
        AUDIO_LOG_ERROR("Failed to set devices: error = 0x%x", audio_ret);
        return audio_ret;
    }
    am->device.mode = VERB_NORMAL;

    /* To Do: Set modifiers */
    /*
    if (!strncmp("voice_recognition", route_info->role, MAX_NAME_LEN)) {
        modifiers[mod_idx++] = AUDIO_USE_CASE_MODIFIER_VOICESEARCH;
    } else if ((!strncmp("alarm", route_info->role, MAX_NAME_LEN))||(!strncmp("notifiication", route_info->role, MAX_NAME_LEN))) {
        if (am->device.active_out &= AUDIO_DEVICE_OUT_JACK)
            modifiers[mod_idx++] = AUDIO_USE_CASE_MODIFIER_DUAL_MEDIA;
        else
            modifiers[mod_idx++] = AUDIO_USE_CASE_MODIFIER_MEDIA;
    } else if (!strncmp("ringtone", route_info->role, MAX_NAME_LEN)) {
        if (am->device.active_out &= AUDIO_DEVICE_OUT_JACK)
            modifiers[mod_idx++] = AUDIO_USE_CASE_MODIFIER_DUAL_RINGTONE;
        else
            modifiers[mod_idx++] = AUDIO_USE_CASE_MODIFIER_RINGTONE;
    } else {
        if (am->device.active_in)
            modifiers[mod_idx++] = AUDIO_USE_CASE_MODIFIER_CAMCORDING;
        else
            modifiers[mod_idx++] = AUDIO_USE_CASE_MODIFIER_MEDIA;
    }
    audio_ret = _audio_ucm_set_modifiers (am, verb, modifiers);
    */

    return audio_ret;
}
audio_return_t _do_route_voicecall (audio_mgr_t *am, device_info_t *devices, int32_t num_of_devices)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    const char *verb = NULL;
    verb = AUDIO_USE_CASE_VERB_VOICECALL;

    AUDIO_LOG_INFO("do_route_voicecall++");
    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);

    audio_ret = set_devices(am, verb, devices, num_of_devices);
    if (audio_ret) {
        AUDIO_LOG_ERROR("Failed to set devices: error = 0x%x", audio_ret);
        return audio_ret;
    }
    /* FIXME. Get network info and configure rate in pcm device */
    am->device.mode = VERB_CALL;
    if (am->device.active_out && am->device.active_in)
        _voice_pcm_open(am);

    return audio_ret;
}
audio_return_t _do_route_voip (audio_mgr_t *am, device_info_t *devices, int32_t num_of_devices)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    const char *verb = NULL;
    const char *active_devices[MAX_DEVICES] = {NULL,};
    verb = AUDIO_USE_CASE_VERB_HIFI;

    AUDIO_LOG_INFO("do_route_voip++");
    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);
    audio_ret = set_devices(am, verb, devices, num_of_devices);
    if (audio_ret) {
        AUDIO_LOG_ERROR("Failed to set devices: error = 0x%x", audio_ret);
        return audio_ret;
    }
    /* FIXME. If necessary, set VERB_VOIP */
    am->device.mode = VERB_NORMAL;
    if (active_devices == NULL) {
        AUDIO_LOG_ERROR("Failed to set device: active device is NULL");
        return AUDIO_ERR_PARAMETER;
    }

    /* TO DO: Set modifiers */
    return audio_ret;
}

audio_return_t _do_route_reset (audio_mgr_t *am, uint32_t direction)
{
    audio_return_t audio_ret = AUDIO_RET_OK;

    /* FIXME: If you need to reset, set verb inactive */
    /* const char *verb = NULL; */
    /* verb = AUDIO_USE_CASE_VERB_INACTIVE; */

    AUDIO_LOG_INFO("do_route_reset++, direction(%p)", direction);
    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);

    if (direction == AUDIO_DIRECTION_OUT) {
        am->device.active_out &= 0x0;
    } else {
        am->device.active_in &= 0x0;
    }
    if (am->device.mode == VERB_CALL) {
        _voice_pcm_close(am, direction);
    }
    /* TO DO: Set Inactive */
    return audio_ret;
}

audio_return_t audio_do_route (void *userdata, audio_route_info_t *info)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    audio_mgr_t *am = (audio_mgr_t *)userdata;
    device_info_t *devices = info->device_infos;

    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);

    AUDIO_LOG_INFO("role:%s", info->role);

    if (!strncmp("call-voice", info->role, MAX_NAME_LEN)) {
        audio_ret = _do_route_voicecall(am, devices, info->num_of_devices);
        if (AUDIO_IS_ERROR(audio_ret)) {
            AUDIO_LOG_WARN("set voicecall route return 0x%x", audio_ret);
        }
    } else if (!strncmp("voip", info->role, MAX_NAME_LEN)) {
        audio_ret = _do_route_voip(am, devices, info->num_of_devices);
        if (AUDIO_IS_ERROR(audio_ret)) {
            AUDIO_LOG_WARN("set voip route return 0x%x", audio_ret);
        }
    } else if (!strncmp("reset", info->role, MAX_NAME_LEN)) {
        audio_ret = _do_route_reset(am, devices->direction);
        if (AUDIO_IS_ERROR(audio_ret)) {
            AUDIO_LOG_WARN("set reset return 0x%x", audio_ret);
        }
    } else {
        /* need to prepare for "alarm","notification","emergency","voice-information","voice-recognition","ringtone" */
        audio_ret = _do_route_ap_playback_capture(am, info);

        if (AUDIO_IS_ERROR(audio_ret)) {
            AUDIO_LOG_WARN("set playback route return 0x%x", audio_ret);
        }
    }
    return audio_ret;
}

audio_return_t audio_update_stream_connection_info (void *userdata, audio_stream_info_t *info, uint32_t is_connected)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    audio_mgr_t *am = (audio_mgr_t *)userdata;

    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);

    AUDIO_LOG_INFO("role:%s, direction:%u, idx:%u, is_connected:%d", info->role, info->direction, info->idx, is_connected);

    return audio_ret;
}

audio_return_t audio_update_route_option (void *userdata, audio_route_option_t *option)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    audio_mgr_t *am = (audio_mgr_t *)userdata;

    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);

    AUDIO_LOG_INFO("role:%s, name:%s, value:%d", option->role, option->name, option->value);

    return audio_ret;
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

int _voice_pcm_close (audio_mgr_t *am, uint32_t direction)
{
    AUDIO_LOG_INFO("close voice pcm handles");

    if (am->device.pcm_out && (direction == AUDIO_DIRECTION_OUT)) {
        audio_alsa_pcm_close((void *)am, am->device.pcm_out);
        am->device.pcm_out = NULL;
        AUDIO_LOG_INFO("voice pcm_out handle close success");
    } else if (am->device.pcm_in && (direction == AUDIO_DIRECTION_IN)) {
        audio_alsa_pcm_close((void *)am, am->device.pcm_in);
        am->device.pcm_in = NULL;
        AUDIO_LOG_INFO("voice pcm_in handle close success");
    }

    return 0;
}

#ifdef __USE_TINYALSA__
static struct pcm *__tinyalsa_open_device (audio_pcm_sample_spec_t *ss, size_t period_size, size_t period_count, uint32_t direction)
{
    struct pcm *pcm = NULL;
    struct pcm_config config;

    config.channels          = ss->channels;
    config.rate              = ss->rate;
    config.period_size       = period_size;
    config.period_count      = period_count;
    config.format            = ss->format;
    config.start_threshold   = 1024;
    config.stop_threshold    = 0xFFFFFFFF;
    config.silence_threshold = 0;

    AUDIO_LOG_INFO("channels %d, rate %d, format %d, period_size %d, period_count %d", ss->channels, ss->rate, ss->format, period_size, period_count);

    pcm = pcm_open((direction == AUDIO_DIRECTION_OUT) ? PLAYBACK_CARD_ID : CAPTURE_CARD_ID,
                   (direction == AUDIO_DIRECTION_OUT) ? PLAYBACK_PCM_DEVICE_ID : CAPTURE_PCM_DEVICE_ID,
                   (direction == AUDIO_DIRECTION_OUT) ? PCM_OUT : PCM_IN,
                   &config);
    if (!pcm || !pcm_is_ready(pcm)) {
        AUDIO_LOG_ERROR("Unable to open device (%s)", pcm_get_error(pcm));
        pcm_close(pcm);
        return NULL;
    }

    return pcm;
}
#endif

audio_return_t audio_pcm_open (void *userdata, void **pcm_handle, void *sample_spec, uint32_t direction)
{
#ifdef __USE_TINYALSA__
    audio_mgr_t *am = (audio_mgr_t *)userdata;
    audio_pcm_sample_spec_t *ss = (audio_pcm_sample_spec_t *)sample_spec;
    size_t period_size, buffer_size;
    int err;

    ss->format = _convert_format((audio_sample_format_t)ss->format);
    period_size = (direction == AUDIO_DIRECTION_OUT) ? PERIODSZ_PLAYBACK : PERIODSZ_CAPTURE;
    buffer_size = (direction == AUDIO_DIRECTION_OUT) ? BUFFERSZ_PLAYBACK : BUFFERSZ_CAPTURE;

    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);

    *pcm_handle = __tinyalsa_open_device(ss, period_size, (buffer_size / period_size), direction);
    if (*pcm_handle == NULL) {
        AUDIO_LOG_ERROR("Error opening PCM device");
        return AUDIO_ERR_RESOURCE;
    }

    if ((err = pcm_prepare((struct pcm *)*pcm_handle)) != 0) {
        AUDIO_LOG_ERROR("Error prepare PCM device : %d", err);
    }

    am->device.pcm_count++;
    AUDIO_LOG_INFO("Opening PCM handle 0x%x", *pcm_handle);
#else  /* alsa-lib */
    char *device_name = NULL;
    audio_mgr_t *am = (audio_mgr_t *)userdata;
    audio_pcm_sample_spec_t *ss = (audio_pcm_sample_spec_t *)sample_spec;
    int err, mode;
    uint8_t use_mmap = 0;
    snd_pcm_uframes_t period_size, buffer_size;
    snd_pcm_uframes_t avail_min = 1024;

    ss->format = _convert_format((audio_sample_format_t)ss->format);
    mode =  SND_PCM_NONBLOCK | SND_PCM_NO_AUTO_RESAMPLE | SND_PCM_NO_AUTO_CHANNELS | SND_PCM_NO_AUTO_FORMAT;
    period_size = (direction == AUDIO_DIRECTION_OUT) ? PERIODSZ_PLAYBACK : PERIODSZ_CAPTURE;
    buffer_size = (direction == AUDIO_DIRECTION_OUT) ? BUFFERSZ_PLAYBACK : BUFFERSZ_CAPTURE;

    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);
    if(direction == AUDIO_DIRECTION_OUT)
        device_name = PLAYBACK_PCM_DEVICE;
    else if (direction == AUDIO_DIRECTION_IN)
        device_name = CAPTURE_PCM_DEVICE;
    else {
        AUDIO_LOG_ERROR("Error get device_name, direction : %d", direction);
        return AUDIO_ERR_RESOURCE;
    }

    if ((err = snd_pcm_open((snd_pcm_t **)pcm_handle, device_name, (direction == AUDIO_DIRECTION_OUT) ? SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE, mode)) < 0) {
        AUDIO_LOG_ERROR("Error opening PCM device %s : %s", device_name, snd_strerror(err));
        pthread_mutex_unlock(&am->device.pcm_lock);
        return AUDIO_ERR_RESOURCE;
    }

    if ((err = _audio_pcm_set_hw_params((snd_pcm_t *)*pcm_handle, ss, &use_mmap, &period_size, &buffer_size)) != AUDIO_RET_OK) {
        AUDIO_LOG_ERROR("Failed to set pcm hw parameters : %s", snd_strerror(err));
        return AUDIO_ERR_RESOURCE;
    }
    AUDIO_LOG_INFO("setting avail_min : %lu", (unsigned long) avail_min);
    if ((err = _audio_pcm_set_sw_params((snd_pcm_t *)*pcm_handle, avail_min, 0)) < 0) {
        AUDIO_LOG_ERROR("Failed to set pcm sw parameters : %s", snd_strerror(err));
        return AUDIO_ERR_RESOURCE;
    }

    if ((err = snd_pcm_prepare((snd_pcm_t *)*pcm_handle)) != 0) {
        AUDIO_LOG_ERROR("Error prepare PCM device : %d", err);
    }

    am->device.pcm_count++;
    AUDIO_LOG_INFO("Opening PCM handle 0x%x, PCM device %s", *pcm_handle, device_name);
#endif

    return AUDIO_RET_OK;
}

audio_return_t audio_pcm_start (void *userdata, void *pcm_handle)
{
    int err;

#ifdef __USE_TINYALSA__
    if ((err = pcm_start(pcm_handle)) < 0) {
        AUDIO_LOG_ERROR("Error starting PCM handle : %d", err);
        return AUDIO_ERR_RESOURCE;
    }
#else  /* alsa-lib */
    if ((err = snd_pcm_start(pcm_handle)) < 0) {
        AUDIO_LOG_ERROR("Error starting PCM handle : %s", snd_strerror(err));
        return AUDIO_ERR_RESOURCE;
    }
#endif

    AUDIO_LOG_INFO("PCM handle 0x%x start", pcm_handle);
    return AUDIO_RET_OK;
}

audio_return_t audio_pcm_stop (void *userdata, void *pcm_handle)
{
    int err;

#ifdef __USE_TINYALSA__
    if ((err = pcm_stop(pcm_handle)) < 0) {
        AUDIO_LOG_ERROR("Error stopping PCM handle : %d", err);
        return AUDIO_ERR_RESOURCE;
    }
#else  /* alsa-lib */
    if ((err = snd_pcm_drop(pcm_handle)) < 0) {
        AUDIO_LOG_ERROR("Error stopping PCM handle : %s", snd_strerror(err));
        return AUDIO_ERR_RESOURCE;
    }
#endif

    AUDIO_LOG_INFO("PCM handle 0x%x stop", pcm_handle);
    return AUDIO_RET_OK;
}

audio_return_t audio_pcm_close (void *userdata, void *pcm_handle)
{
    audio_mgr_t *am = (audio_mgr_t *)userdata;
    int err;

    AUDIO_LOG_INFO("Try to close PCM handle 0x%x", pcm_handle);

#ifdef __USE_TINYALSA__
    if ((err = pcm_close(pcm_handle)) < 0) {
        AUDIO_LOG_ERROR("Error closing PCM handle : %d", err);
        return AUDIO_ERR_RESOURCE;
    }
#else  /* alsa-lib */
    if ((err = snd_pcm_close(pcm_handle)) < 0) {
        AUDIO_LOG_ERROR("Error closing PCM handle : %s", snd_strerror(err));
        pthread_mutex_unlock(&am->device.pcm_lock);
        return AUDIO_ERR_RESOURCE;
    }
#endif

    pcm_handle = NULL;
    am->device.pcm_count--;
    AUDIO_LOG_INFO("PCM handle close success (count:%d)", am->device.pcm_count);

    return AUDIO_RET_OK;
}

audio_return_t audio_pcm_avail (void *userdata, void *pcm_handle, uint32_t *avail)
{
#ifdef __USE_TINYALSA__
    struct timespec tspec;
    unsigned int frames_avail = 0;
    int err;

    AUDIO_RETURN_VAL_IF_FAIL(pcm_handle, AUDIO_ERR_PARAMETER);

    err = pcm_get_htimestamp(pcm_handle, &frames_avail, &tspec);
    if (err < 0) {
        AUDIO_LOG_ERROR("Could not get avail and timespec");
        return AUDIO_ERR_IOCTL;
    }

#ifdef DEBUG_TIMING
    AUDIO_LOG_DEBUG("avail = %d", frames_avail);
#endif

    *avail = (uint32_t)frames_avail;
#else  /* alsa-lib */
    snd_pcm_sframes_t frames_avail;

    AUDIO_RETURN_VAL_IF_FAIL(pcm_handle, AUDIO_ERR_PARAMETER);

    if ((frames_avail = snd_pcm_avail(pcm_handle)) < 0) {
        AUDIO_LOG_ERROR("Could not get avail");
        return AUDIO_ERR_IOCTL;
    }

#ifdef DEBUG_TIMING
    AUDIO_LOG_DEBUG("avail = %d", frames_avail);
#endif

    *avail = (uint32_t)frames_avail;
#endif

    return AUDIO_RET_OK;
}

audio_return_t audio_pcm_write (void *userdata, void *pcm_handle, const void *buffer, uint32_t frames)
{
#ifdef __USE_TINYALSA__
    int err;

    AUDIO_RETURN_VAL_IF_FAIL(pcm_handle, AUDIO_ERR_PARAMETER);

    err = pcm_write(pcm_handle, buffer, pcm_frames_to_bytes(pcm_handle, (unsigned int)frames));
    if (err < 0) {
        AUDIO_LOG_ERROR("Failed to write pcm : %d", err);
        return AUDIO_ERR_IOCTL;
    }

#ifdef DEBUG_TIMING
    AUDIO_LOG_DEBUG("audio_pcm_write = %d", frames);
#endif
#else  /* alsa-lib */
    snd_pcm_sframes_t frames_written;

    AUDIO_RETURN_VAL_IF_FAIL(pcm_handle, AUDIO_ERR_PARAMETER);

    frames_written = snd_pcm_writei(pcm_handle, buffer, (snd_pcm_uframes_t) frames);
    if (frames_written < 0) {
        AUDIO_LOG_ERROR("Failed to write pcm : %d", frames_written);
        return AUDIO_ERR_IOCTL;
    }

#ifdef DEBUG_TIMING
    AUDIO_LOG_DEBUG("audio_pcm_write = (%d / %d)", frames_written, frames);
#endif
#endif

    return AUDIO_RET_OK;
}

audio_return_t audio_pcm_read (void *userdata, void *pcm_handle, void *buffer, uint32_t frames)
{
#ifdef __USE_TINYALSA__
    int err;

    AUDIO_RETURN_VAL_IF_FAIL(pcm_handle, AUDIO_ERR_PARAMETER);

    err = pcm_read(pcm_handle, buffer, pcm_frames_to_bytes(pcm_handle, (unsigned int)frames));
    if (err < 0) {
        AUDIO_LOG_ERROR("Failed to read pcm : %d", err);
        return AUDIO_ERR_IOCTL;
    }

#ifdef DEBUG_TIMING
    AUDIO_LOG_DEBUG("audio_pcm_read = %d", frames);
#endif
#else  /* alsa-lib */
    snd_pcm_sframes_t frames_read;

    AUDIO_RETURN_VAL_IF_FAIL(pcm_handle, AUDIO_ERR_PARAMETER);

    frames_read = snd_pcm_readi(pcm_handle, buffer, (snd_pcm_uframes_t)frames);
    if (frames_read < 0) {
        AUDIO_LOG_ERROR("Failed to read pcm : %d", frames_read);
        return AUDIO_ERR_IOCTL;
    }

#ifdef DEBUG_TIMING
    AUDIO_LOG_DEBUG("audio_pcm_read = (%d / %d)", frames_read, frames);
#endif
#endif

    return AUDIO_RET_OK;
}

audio_return_t audio_pcm_get_fd(void *userdata, void *pcm_handle, int *fd)
{
    AUDIO_RETURN_VAL_IF_FAIL(pcm_handle, AUDIO_ERR_PARAMETER);
#ifdef __USE_TINYALSA__
    *fd = _pcm_poll_descriptor((struct pcm *)pcm_handle);
#else  /* alsa-lib */
    *fd = _snd_pcm_poll_descriptor((snd_pcm_t *)pcm_handle);
#endif
    return AUDIO_RET_OK;
}
audio_return_t audio_pcm_recover(void *userdata, void *pcm_handle, int err)
{
    // TODO:
    AUDIO_LOG_DEBUG("audio_pcm_recover");
    return AUDIO_RET_OK;
}

audio_return_t audio_pcm_get_params(void *userdata, void *pcm_handle, uint32_t direction, void **sample_spec, uint32_t *frag_size, uint32_t *nfrags)
{
    audio_mgr_t *am = (audio_mgr_t *)userdata;
    audio_pcm_sample_spec_t *ss = (audio_pcm_sample_spec_t *)*sample_spec;

    if (direction == AUDIO_DIRECTION_OUT) {
        *frag_size = PERIODSZ_PLAYBACK * _audio_sample_size(ss->format) * ss->channels;
        *nfrags = BUFFERSZ_PLAYBACK / PERIODSZ_PLAYBACK;
    } else if (direction == AUDIO_DIRECTION_IN) {
        *frag_size = PERIODSZ_CAPTURE * _audio_sample_size(ss->format) * ss->channels;
        *nfrags = BUFFERSZ_CAPTURE / PERIODSZ_CAPTURE;
    } else {
        AUDIO_LOG_DEBUG("Parameters are invalid");
        return AUDIO_ERR_PARAMETER;
    }

    AUDIO_LOG_DEBUG("audio_pcm_get_params : format=%d, channels=%d, rate=%d, frag_size=%d, nfrags=%d", ss->format, ss->channels, ss->rate, *frag_size, *nfrags);
    return AUDIO_RET_OK;
}

audio_return_t audio_pcm_set_params(void *userdata, void *pcm_handle, uint32_t direction, void *sample_spec, uint32_t frag_size, uint32_t nfrags)
{
    // TODO:
    AUDIO_LOG_DEBUG("audio_pcm_set_params");
    return AUDIO_RET_OK;
}
