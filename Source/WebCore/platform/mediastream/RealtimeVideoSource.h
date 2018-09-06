/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "FontCascade.h"
#include "ImageBuffer.h"
#include "MediaSample.h"
#include "RealtimeMediaSource.h"
#include <wtf/Lock.h>
#include <wtf/RunLoop.h>

namespace WebCore {

struct FrameRateRange;
struct VideoPreset;

class RealtimeVideoSource : public RealtimeMediaSource {
public:
    virtual ~RealtimeVideoSource();

protected:
    RealtimeVideoSource(const String& id, const String& name);

    void prepareToProduceData();
    bool supportsSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double>) final;
    void setSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double>) final;

    using VideoPresets = Vector<VideoPreset>;
    void setSupportedPresets(VideoPresets&&);

    struct CaptureSizeAndFrameRate {
        IntSize size;
        double frameRate;
    };
    std::optional<CaptureSizeAndFrameRate> bestSupportedSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double>);

    void addSupportedCapabilities(RealtimeMediaSourceCapabilities&) const;

    void setDefaultSize(const IntSize& size) { m_defaultSize = size; }

    double observedFrameRate() const { return m_observedFrameRate; }

    void dispatchMediaSampleToObservers(MediaSample&);

private:

    VideoPresets m_presets;
    Deque<double> m_observedFrameTimeStamps;
    double m_observedFrameRate { 0 };
    IntSize m_defaultSize;
};

struct FrameRateRange {
    double minimum;
    double maximum;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<FrameRateRange> decode(Decoder&);
};

template<class Encoder>
void FrameRateRange::encode(Encoder& encoder) const
{
    encoder << minimum;
    encoder << maximum;
}

template <class Decoder>
std::optional<FrameRateRange> FrameRateRange::decode(Decoder& decoder)
{
    std::optional<double> minimum;
    decoder >> minimum;
    if (!minimum)
        return std::nullopt;

    std::optional<double> maximum;
    decoder >> maximum;
    if (!maximum)
        return std::nullopt;

    return FrameRateRange { *minimum, *maximum };
}

struct VideoPreset {
    IntSize size;
    Vector<FrameRateRange> frameRateRanges;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<VideoPreset> decode(Decoder&);
};

template<class Encoder>
void VideoPreset::encode(Encoder& encoder) const
{
    encoder << size;
    encoder << frameRateRanges;
}

template <class Decoder>
std::optional<VideoPreset> VideoPreset::decode(Decoder& decoder)
{
    std::optional<IntSize> size;
    decoder >> size;
    if (!size)
        return std::nullopt;

    std::optional<Vector<FrameRateRange>> frameRateRanges;
    decoder >> frameRateRanges;
    if (!frameRateRanges)
        return std::nullopt;

    return VideoPreset { *size, *frameRateRanges };
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

