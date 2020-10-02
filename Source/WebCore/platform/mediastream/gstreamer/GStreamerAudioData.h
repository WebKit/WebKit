/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Author: Thibault Saunier <tsaunier@igalia.com>
 * Author: Alejandro G. Castro <alex@igalia.com>
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

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)

#include "GRefPtrGStreamer.h"
#include "PlatformAudioData.h"

#include <gst/audio/audio.h>

namespace WebCore {
class GStreamerAudioData final : public PlatformAudioData {
public:
    GStreamerAudioData(GRefPtr<GstSample>&& sample, GstAudioInfo info)
        : m_sample(WTFMove(sample))
        , m_audioInfo(info)
    {
    }

    GStreamerAudioData(GRefPtr<GstSample>&& sample)
        : m_sample(WTFMove(sample))
    {
        gst_audio_info_from_caps(&m_audioInfo, gst_sample_get_caps(m_sample.get()));
    }

    GstSample* getSample() { return m_sample.get(); }
    GstAudioInfo getAudioInfo() {return m_audioInfo; }

private:
    Kind kind() const { return Kind::GStreamerAudioData; }
    GRefPtr<GstSample> m_sample;
    GRefPtr<GstCaps> m_caps;

    GstAudioInfo m_audioInfo;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
