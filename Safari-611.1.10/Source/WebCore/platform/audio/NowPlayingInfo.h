/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "MediaSessionIdentifier.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

struct NowPlayingInfo {
    String title;
    String sourceApplicationIdentifier;
    double duration { 0 };
    double currentTime { 0 };
    bool supportsSeeking { false };
    MediaSessionIdentifier uniqueIdentifier;
    bool isPlaying { false };
    bool allowsNowPlayingControlsVisibility { false };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<NowPlayingInfo> decode(Decoder&);
};

template<class Encoder> inline void NowPlayingInfo::encode(Encoder& encoder) const
{
    encoder << title << sourceApplicationIdentifier << duration << currentTime << supportsSeeking << uniqueIdentifier << isPlaying << allowsNowPlayingControlsVisibility;
}

template<class Decoder> inline Optional<NowPlayingInfo> NowPlayingInfo::decode(Decoder& decoder)
{
    String title;
    if (!decoder.decode(title))
        return { };

    String sourceApplicationIdentifier;
    if (!decoder.decode(sourceApplicationIdentifier))
        return { };

    double duration;
    if (!decoder.decode(duration))
        return { };

    double currentTime;
    if (!decoder.decode(currentTime))
        return { };

    bool supportsSeeking;
    if (!decoder.decode(supportsSeeking))
        return { };

    MediaSessionIdentifier uniqueIdentifier;
    if (!decoder.decode(uniqueIdentifier))
        return { };

    bool isPlaying;
    if (!decoder.decode(isPlaying))
        return { };

    bool allowsNowPlayingControlsVisibility;
    if (!decoder.decode(allowsNowPlayingControlsVisibility))
        return { };

    return NowPlayingInfo { WTFMove(title), WTFMove(sourceApplicationIdentifier), duration, currentTime, supportsSeeking, uniqueIdentifier, isPlaying, allowsNowPlayingControlsVisibility };
}

} // namespace WebCore
