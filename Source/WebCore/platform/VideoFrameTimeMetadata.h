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

#include <optional>
#include <wtf/Seconds.h>

namespace WebCore {

struct VideoFrameTimeMetadata {
    std::optional<double> processingDuration;

    std::optional<Seconds> captureTime;
    std::optional<Seconds> receiveTime;
    std::optional<unsigned> rtpTimestamp;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<VideoFrameTimeMetadata> decode(Decoder&);
};

template<class Encoder>
inline void VideoFrameTimeMetadata::encode(Encoder& encoder) const
{
    encoder << processingDuration << captureTime << receiveTime << rtpTimestamp;
}

template<class Decoder>
inline std::optional<VideoFrameTimeMetadata> VideoFrameTimeMetadata::decode(Decoder& decoder)
{
    std::optional<std::optional<double>> processingDuration;
    decoder >> processingDuration;
    if (!processingDuration)
        return std::nullopt;

    std::optional<std::optional<Seconds>> captureTime;
    decoder >> captureTime;
    if (!captureTime)
        return std::nullopt;

    std::optional<std::optional<Seconds>> receiveTime;
    decoder >> receiveTime;
    if (!receiveTime)
        return std::nullopt;

    std::optional<std::optional<unsigned>> rtpTimestamp;
    decoder >> rtpTimestamp;
    if (!rtpTimestamp)
        return std::nullopt;

    return VideoFrameTimeMetadata { *processingDuration, *captureTime, *receiveTime, *rtpTimestamp };
}

}
