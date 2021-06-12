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

#if ENABLE(GPU_PROCESS)

#include <WebCore/VP9Utilities.h>

namespace WebKit {

struct MediaOverridesForTesting {
    std::optional<bool> systemHasAC;
    std::optional<bool> systemHasBattery;

    std::optional<bool> vp9HardwareDecoderDisabled;
    std::optional<WebCore::ScreenDataOverrides> vp9ScreenSizeAndScale;

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << systemHasAC;
        encoder << systemHasBattery;
        encoder << vp9HardwareDecoderDisabled;
        encoder << vp9ScreenSizeAndScale;
    }

    template <class Decoder>
    static std::optional<MediaOverridesForTesting> decode(Decoder& decoder)
    {
        std::optional<std::optional<bool>> systemHasAC;
        decoder >> systemHasAC;
        if (!systemHasAC)
            return std::nullopt;

        std::optional<std::optional<bool>> systemHasBattery;
        decoder >> systemHasBattery;
        if (!systemHasBattery)
            return std::nullopt;

        std::optional<std::optional<bool>> vp9HardwareDecoderDisabled;
        decoder >> vp9HardwareDecoderDisabled;
        if (!vp9HardwareDecoderDisabled)
            return std::nullopt;

        std::optional<std::optional<WebCore::ScreenDataOverrides>> vp9ScreenSizeAndScale;
        decoder >> vp9ScreenSizeAndScale;
        if (!vp9ScreenSizeAndScale)
            return std::nullopt;

        return {{
            *systemHasAC,
            *systemHasBattery,
            *vp9HardwareDecoderDisabled,
            *vp9ScreenSizeAndScale,
        }};
    }
};

} // namespace WebKit

#endif
