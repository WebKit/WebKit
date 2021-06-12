/*
 * Copyright (C) 2007-2018 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO)

namespace WebCore {

struct VideoPlaybackQualityMetrics {
    uint32_t totalVideoFrames { 0 };
    uint32_t droppedVideoFrames { 0 };
    uint32_t corruptedVideoFrames { 0 };
    double totalFrameDelay { 0 };
    uint32_t displayCompositedVideoFrames { 0 };

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << totalVideoFrames;
        encoder << droppedVideoFrames;
        encoder << corruptedVideoFrames;
        encoder << totalFrameDelay;
        encoder << displayCompositedVideoFrames;
    }

    template <class Decoder>
    static std::optional<VideoPlaybackQualityMetrics> decode(Decoder& decoder)
    {
        std::optional<uint32_t> totalVideoFrames;
        decoder >> totalVideoFrames;
        if (!totalVideoFrames)
            return std::nullopt;

        std::optional<uint32_t> droppedVideoFrames;
        decoder >> droppedVideoFrames;
        if (!droppedVideoFrames)
            return std::nullopt;

        std::optional<uint32_t> corruptedVideoFrames;
        decoder >> corruptedVideoFrames;
        if (!corruptedVideoFrames)
            return std::nullopt;

        std::optional<double> totalFrameDelay;
        decoder >> totalFrameDelay;
        if (!totalFrameDelay)
            return std::nullopt;

        std::optional<uint32_t> displayCompositedVideoFrames;
        decoder >> displayCompositedVideoFrames;
        if (!displayCompositedVideoFrames)
            return std::nullopt;

        return {{ *totalVideoFrames, *droppedVideoFrames, *corruptedVideoFrames, *totalFrameDelay, *displayCompositedVideoFrames }};
    }
};

} // namespace WebCore

#endif // ENABLE(VIDEO)
