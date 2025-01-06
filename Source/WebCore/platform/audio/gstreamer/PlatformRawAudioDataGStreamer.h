/*
 * Copyright (C) 2023 Igalia S.L
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

#include "PlatformRawAudioData.h"

#if ENABLE(WEB_CODECS) && USE(GSTREAMER)

#include "GRefPtrGStreamer.h"
#include <gst/audio/audio.h>

namespace WebCore {

class PlatformRawAudioDataGStreamer final : public PlatformRawAudioData {
public:
    static Ref<PlatformRawAudioData> create(GRefPtr<GstSample>&& sample)
    {
        return adoptRef(*new PlatformRawAudioDataGStreamer(WTFMove(sample)));
    }

    AudioSampleFormat format() const final;
    size_t sampleRate() const final;
    size_t numberOfChannels() const final;
    size_t numberOfFrames() const final;
    std::optional<uint64_t> duration() const final;
    int64_t timestamp() const final;

    size_t memoryCost() const final;

    constexpr MediaPlatformType platformType() const final { return MediaPlatformType::GStreamer; }

    GstSample* sample() const { return m_sample.get(); }
    const GstAudioInfo* info() const { return &m_info; }

private:
    PlatformRawAudioDataGStreamer(GRefPtr<GstSample>&&);

    GRefPtr<GstSample> m_sample;
    GstAudioInfo m_info;
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::PlatformRawAudioDataGStreamer)
static bool isType(const WebCore::PlatformRawAudioData& data) { return data.platformType() == WebCore::MediaPlatformType::GStreamer; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WEB_CODECS) && USE(GSTREAMER)
