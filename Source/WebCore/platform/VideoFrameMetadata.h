/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(VIDEO)

#include <optional>

namespace WebCore {

struct VideoFrameMetadata {
    double presentationTime { 0 };
    double expectedDisplayTime { 0 };

    unsigned width { 0 };
    unsigned height { 0 };
    double mediaTime { 0 };

    unsigned presentedFrames { 0 };
    std::optional<double> processingDuration;

    std::optional<double> captureTime;
    std::optional<double> receiveTime;
    std::optional<unsigned> rtpTimestamp;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<VideoFrameMetadata> decode(Decoder&);
};

template<class Encoder>
inline void VideoFrameMetadata::encode(Encoder& encoder) const
{
    encoder << presentationTime << expectedDisplayTime << width << height << mediaTime << presentedFrames << processingDuration << captureTime << receiveTime << rtpTimestamp;
}

template<class Decoder>
inline std::optional<VideoFrameMetadata> VideoFrameMetadata::decode(Decoder& decoder)
{
    std::optional<double> presentationTime;
    decoder >> presentationTime;
    if (!presentationTime)
        return std::nullopt;

    std::optional<double> expectedDisplayTime;
    decoder >> expectedDisplayTime;
    if (!expectedDisplayTime)
        return std::nullopt;

    std::optional<unsigned> width;
    decoder >> width;
    if (!width)
        return std::nullopt;

    std::optional<unsigned> height;
    decoder >> height;
    if (!height)
        return std::nullopt;

    std::optional<double> mediaTime;
    decoder >> mediaTime;
    if (!mediaTime)
        return std::nullopt;

    std::optional<unsigned> presentedFrames;
    decoder >> presentedFrames;
    if (!presentedFrames)
        return std::nullopt;

    std::optional<std::optional<double>> processingDuration;
    decoder >> processingDuration;
    if (!processingDuration)
        return std::nullopt;

    std::optional<std::optional<double>> captureTime;
    decoder >> captureTime;
    if (!captureTime)
        return std::nullopt;

    std::optional<std::optional<double>> receiveTime;
    decoder >> receiveTime;
    if (!receiveTime)
        return std::nullopt;

    std::optional<std::optional<unsigned>> rtpTimestamp;
    decoder >> rtpTimestamp;
    if (!rtpTimestamp)
        return std::nullopt;

    return VideoFrameMetadata { *presentationTime, *expectedDisplayTime, *width, *height, *mediaTime, *presentedFrames, *processingDuration, *captureTime, *receiveTime, *rtpTimestamp };
}

}

#endif // ENABLE(VIDEO)
