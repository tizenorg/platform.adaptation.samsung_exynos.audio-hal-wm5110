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

#include "tizen-audio-internal.h"

static const char *__get_session_string_by_idx (uint32_t session_idx)
{
    switch (session_idx) {
    case AUDIO_SESSION_MEDIA:                       return "media";
    case AUDIO_SESSION_VOICECALL:                   return "voicecall";
    case AUDIO_SESSION_VIDEOCALL:                   return "videocall";
    case AUDIO_SESSION_VOIP:                        return "voip";
    case AUDIO_SESSION_FMRADIO:                     return "fmradio";
    case AUDIO_SESSION_CAMCORDER:                   return "camcorder";
    case AUDIO_SESSION_NOTIFICATION:                return "notification";
    case AUDIO_SESSION_ALARM:                       return "alarm";
    case AUDIO_SESSION_EMERGENCY:                   return "emergency";
    case AUDIO_SESSION_VOICE_RECOGNITION:           return "voice_recognition";
    default:                                        return "invalid";
    }
}

static const char *__get_subsession_string_by_idx (uint32_t subsession_idx)
{
    switch (subsession_idx) {
    case AUDIO_SUBSESSION_NONE:                     return "none";
    case AUDIO_SUBSESSION_VOICE:                    return "voice";
    case AUDIO_SUBSESSION_RINGTONE:                 return "ringtone";
    case AUDIO_SUBSESSION_MEDIA:                    return "media";
    case AUDIO_SUBSESSION_INIT:                     return "init";
    case AUDIO_SUBSESSION_VR_NORMAL:                return "vr_normal";
    case AUDIO_SUBSESSION_VR_DRIVE:                 return "vr_drive";
    case AUDIO_SUBSESSION_STEREO_REC:               return "stereo_rec";
    case AUDIO_SUBSESSION_MONO_REC:                 return "mono_rec";
    default:                                        return "invalid";
    }
}

static const char * __get_sessin_cmd_string (uint32_t cmd)
{
    switch (cmd) {
    case AUDIO_SESSION_CMD_START:                   return "start";
    case AUDIO_SESSION_CMD_SUBSESSION:              return "subsession";
    case AUDIO_SESSION_CMD_END:                     return "end";
    default:                                        return "invalid";
    }
}

audio_return_t _audio_session_init (audio_mgr_t *am)
{
    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);

    am->session.session = AUDIO_SESSION_MEDIA;
    am->session.subsession = AUDIO_SUBSESSION_NONE;
    am->session.is_recording = 0;
    am->session.is_radio_on = 0;
    am->session.is_call_session = 0;

    return AUDIO_RET_OK;
}

audio_return_t _audio_session_deinit (audio_mgr_t *am)
{
    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);

    return AUDIO_RET_OK;
}

audio_return_t audio_set_session (void *userdata, uint32_t session, uint32_t subsession, uint32_t cmd)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    audio_mgr_t *am = (audio_mgr_t *)userdata;
    uint32_t prev_subsession = am->session.subsession;

    AUDIO_RETURN_VAL_IF_FAIL(am, AUDIO_ERR_PARAMETER);

    AUDIO_LOG_INFO("session %s:%s(%s)->%s(%s)", __get_sessin_cmd_string(cmd),
        __get_session_string_by_idx(am->session.session), __get_subsession_string_by_idx(am->session.subsession),
        __get_session_string_by_idx(session), __get_subsession_string_by_idx(subsession));

    if (cmd == AUDIO_SESSION_CMD_START) {
        if (am->session.is_call_session) {
            AUDIO_LOG_ERROR("call active its not possible to have any other session start now");
            return audio_ret;
        }
        am->session.session = session;
        am->session.subsession = subsession;

        if ((session == AUDIO_SESSION_VIDEOCALL) ||
            (session == AUDIO_SESSION_VOICECALL) ||
            (session == AUDIO_SESSION_VOIP)) {
            AUDIO_LOG_INFO("set call session");
            am->session.is_call_session = 1;
        }

    } else if (cmd == AUDIO_SESSION_CMD_END) {

        if ((session == AUDIO_SESSION_VIDEOCALL) ||
            (session == AUDIO_SESSION_VOICECALL) ||
            (session == AUDIO_SESSION_VOIP)) {
            if(am->device.pcm_out || am->device.pcm_in) {
                _voice_pcm_close(am, 1);
            }
            AUDIO_LOG_INFO("unset call session");
            am->session.is_call_session = 0;
        }

        if (am->session.is_call_session) {
            AUDIO_LOG_ERROR("call active its not possible to have any other session end now");
            return audio_ret;
        }

        if (session == AUDIO_SESSION_MEDIA && (prev_subsession == AUDIO_SUBSESSION_STEREO_REC || AUDIO_SUBSESSION_MONO_REC)) {
            am->session.is_recording = 0;
        }
        am->session.session = AUDIO_SESSION_MEDIA;
        am->session.subsession = AUDIO_SUBSESSION_NONE;
    } else if (cmd == AUDIO_SESSION_CMD_SUBSESSION) {

        if (am->session.is_call_session) {
            if ((subsession != AUDIO_SUBSESSION_VOICE) &&
                (subsession != AUDIO_SUBSESSION_MEDIA) &&
                (subsession != AUDIO_SUBSESSION_RINGTONE)) {
                AUDIO_LOG_ERROR("call active we can only have one of AUDIO_SUBSESSION_VOICE AUDIO_SUBSESSION_MEDIA AUDIO_SUBSESSION_RINGTONE as a sub-session");
                return audio_ret;
            }
            if (prev_subsession != AUDIO_SUBSESSION_VOICE && subsession == AUDIO_SUBSESSION_VOICE){
                if(!am->device.pcm_out || !am->device.pcm_in) {
                    _voice_pcm_open(am);
                }
            } else if (prev_subsession == AUDIO_SUBSESSION_VOICE && subsession != AUDIO_SUBSESSION_VOICE) {
                if(am->device.pcm_out || am->device.pcm_in) {
                    _voice_pcm_close(am, 1);
                }
            }
        }
        am->session.subsession = subsession;
        if (prev_subsession != subsession && subsession == AUDIO_SUBSESSION_VOICE) {
            am->session.is_recording = 0;
        }

        if (subsession == AUDIO_SUBSESSION_STEREO_REC || subsession == AUDIO_SUBSESSION_MONO_REC) {
            am->session.is_recording = 1;
        } else if (am->session.is_recording == 1 && subsession == AUDIO_SUBSESSION_INIT) {
            am->session.is_recording = 0;
        }
    }

    return audio_ret;
}
