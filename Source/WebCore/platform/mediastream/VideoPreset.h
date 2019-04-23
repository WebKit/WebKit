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

struct FrameRateRange {
    double minimum;
    double maximum;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<FrameRateRange> decode(Decoder&);
};

template<class Encoder>
void FrameRateRange::encode(Encoder& encoder) const
{
    encoder << minimum;
    encoder << maximum;
}

template <class Decoder>
Optional<FrameRateRange> FrameRateRange::decode(Decoder& decoder)
{
    Optional<double> minimum;
    decoder >> minimum;
    if (!minimum)
        return WTF::nullopt;

    Optional<double> maximum;
    decoder >> maximum;
    if (!maximum)
        return WTF::nullopt;

    return FrameRateRange { *minimum, *maximum };
}

struct VideoPresetData {
    IntSize size;
    Vector<FrameRateRange> frameRateRanges;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<VideoPresetData> decode(Decoder&);
};

template<class Encoder>
void VideoPresetData::encode(Encoder& encoder) const
{
    encoder << size;
    encoder << frameRateRanges;
}

template <class Decoder>
Optional<VideoPresetData> VideoPresetData::decode(Decoder& decoder)
{
    Optional<IntSize> size;
    decoder >> size;
    if (!size)
        return WTF::nullopt;

    Optional<Vector<FrameRateRange>> frameRateRanges;
    decoder >> frameRateRanges;
    if (!frameRateRanges)
        return WTF::nullopt;

    return VideoPresetData { *size, *frameRateRanges };
}

class VideoPreset : public RefCounted<VideoPreset> {
public:
    static Ref<VideoPreset> create(VideoPresetData&& data)
    {
        return adoptRef(*new VideoPreset(data.size, WTFMove(data.frameRateRanges), Base));
    }

    enum VideoPresetType {
        Base,
        AVCapture,
        GStreamer
    };

    IntSize size;
    Vector<FrameRateRange> frameRateRanges;
    VideoPresetType type;

protected:
    VideoPreset(IntSize size, Vector<FrameRateRange>&& frameRateRanges, VideoPresetType type)
        : size(size)
        , frameRateRanges(WTFMove(frameRateRanges))
        , type(type)
    {
    }
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::VideoPreset)
    static bool isType(const WebCore::VideoPreset& preset) { return preset.type == WebCore::VideoPreset::VideoPresetType::Base; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(MEDIA_STREAM)

