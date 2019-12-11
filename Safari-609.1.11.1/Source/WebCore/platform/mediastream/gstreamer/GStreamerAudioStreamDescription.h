/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)

#include "AudioStreamDescription.h"
#include "GStreamerCommon.h"
#include <gst/audio/audio.h>

namespace WebCore {

class GStreamerAudioStreamDescription final: public AudioStreamDescription {
public:
    GStreamerAudioStreamDescription(GstAudioInfo info)
        : m_info(info)
        , m_caps(adoptGRef(gst_audio_info_to_caps(&m_info)))
    {
    }

    GStreamerAudioStreamDescription(GstAudioInfo *info)
        : m_info(*info)
        , m_caps(adoptGRef(gst_audio_info_to_caps(&m_info)))
    {
    }

    GStreamerAudioStreamDescription()
    {
        gst_audio_info_init(&m_info);
    }

    WEBCORE_EXPORT ~GStreamerAudioStreamDescription() { };

    const PlatformDescription& platformDescription() const
    {
        m_platformDescription = { PlatformDescription::GStreamerAudioStreamDescription, reinterpret_cast<const AudioStreamBasicDescription*>(&m_info) };

        return m_platformDescription;
    }

    WEBCORE_EXPORT PCMFormat format() const final {
        switch (GST_AUDIO_INFO_FORMAT(&m_info)) {
        case GST_AUDIO_FORMAT_S16LE:
        case GST_AUDIO_FORMAT_S16BE:
            return Int16;
        case GST_AUDIO_FORMAT_S32LE:
        case GST_AUDIO_FORMAT_S32BE:
            return Int32;
        case GST_AUDIO_FORMAT_F32LE:
        case GST_AUDIO_FORMAT_F32BE:
            return Float32;
        case GST_AUDIO_FORMAT_F64LE:
        case GST_AUDIO_FORMAT_F64BE:
            return Float64;
        default:
            break;
        }
        return None;
    }

    double sampleRate() const final { return GST_AUDIO_INFO_RATE(&m_info); }
    bool isPCM() const final { return format() != None; }
    bool isInterleaved() const final { return GST_AUDIO_INFO_LAYOUT(&m_info) == GST_AUDIO_LAYOUT_INTERLEAVED; }
    bool isSignedInteger() const final { return GST_AUDIO_INFO_IS_INTEGER(&m_info); }
    bool isNativeEndian() const final { return GST_AUDIO_INFO_ENDIANNESS(&m_info) == G_BYTE_ORDER; }
    bool isFloat() const final { return GST_AUDIO_INFO_IS_FLOAT(&m_info); }
    int bytesPerFrame() { return GST_AUDIO_INFO_BPF(&m_info);  }

    uint32_t numberOfInterleavedChannels() const final { return isInterleaved() ? GST_AUDIO_INFO_CHANNELS(&m_info) : TRUE; }
    uint32_t numberOfChannelStreams() const final { return GST_AUDIO_INFO_CHANNELS(&m_info); }
    uint32_t numberOfChannels() const final { return GST_AUDIO_INFO_CHANNELS(&m_info); }
    uint32_t sampleWordSize() const final { return GST_AUDIO_INFO_BPS(&m_info); }

    bool operator==(const GStreamerAudioStreamDescription& other) { return gst_audio_info_is_equal(&m_info, &other.m_info); }
    bool operator!=(const GStreamerAudioStreamDescription& other) { return !operator == (other); }

    GstCaps* caps() { return m_caps.get(); }
    GstAudioInfo* getInfo() { return &m_info; }

private:
    GstAudioInfo m_info;
    GRefPtr<GstCaps> m_caps;
    mutable PlatformDescription m_platformDescription;
};

} // WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
