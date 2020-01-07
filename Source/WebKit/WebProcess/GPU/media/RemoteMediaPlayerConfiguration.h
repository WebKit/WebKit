/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include <WebCore/MediaPlayerEnums.h>
#include <WebCore/PlatformTimeRanges.h>
#include <wtf/MediaTime.h>

namespace WebKit {

struct RemoteMediaPlayerConfiguration {
    String engineDescription;
    bool supportsScanning { false };
    bool supportsFullscreen { false };
    bool supportsPictureInPicture { false };
    bool supportsAcceleratedRendering { false };
    bool canPlayToWirelessPlaybackTarget { false };
    bool shouldIgnoreIntrinsicSize { false };

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << engineDescription;
        encoder << supportsScanning;
        encoder << supportsFullscreen;
        encoder << supportsPictureInPicture;
        encoder << supportsAcceleratedRendering;
        encoder << canPlayToWirelessPlaybackTarget;
        encoder << shouldIgnoreIntrinsicSize;
    }

    template <class Decoder>
    static Optional<RemoteMediaPlayerConfiguration> decode(Decoder& decoder)
    {
        Optional<String> engineDescription;
        decoder >> engineDescription;
        if (!engineDescription)
            return WTF::nullopt;

        Optional<bool> supportsScanning;
        decoder >> supportsScanning;
        if (!supportsScanning)
            return WTF::nullopt;

        Optional<bool> supportsFullscreen;
        decoder >> supportsFullscreen;
        if (!supportsFullscreen)
            return WTF::nullopt;

        Optional<bool> supportsPictureInPicture;
        decoder >> supportsPictureInPicture;
        if (!supportsPictureInPicture)
            return WTF::nullopt;

        Optional<bool> supportsAcceleratedRendering;
        decoder >> supportsAcceleratedRendering;
        if (!supportsAcceleratedRendering)
            return WTF::nullopt;

        Optional<bool> canPlayToWirelessPlaybackTarget;
        decoder >> canPlayToWirelessPlaybackTarget;
        if (!canPlayToWirelessPlaybackTarget)
            return WTF::nullopt;

        Optional<bool> shouldIgnoreIntrinsicSize;
        decoder >> shouldIgnoreIntrinsicSize;
        if (!shouldIgnoreIntrinsicSize)
            return WTF::nullopt;

        return {{
            WTFMove(*engineDescription),
            *supportsScanning,
            *supportsPictureInPicture,
            *supportsAcceleratedRendering,
            *canPlayToWirelessPlaybackTarget,
            *shouldIgnoreIntrinsicSize,
        }};
    }
};

} // namespace WebKit

#endif
