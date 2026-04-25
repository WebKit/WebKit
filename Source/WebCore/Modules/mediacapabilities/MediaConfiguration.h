/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "AudioConfiguration.h"
#include "PlatformMediaConfiguration.h"
#include "VideoConfiguration.h"
#include <wtf/CrossThreadCopier.h>

namespace WebCore {

struct MediaConfiguration {
    std::optional<VideoConfiguration> video;
    std::optional<AudioConfiguration> audio;

    MediaConfiguration isolatedCopy() const &;
    MediaConfiguration isolatedCopy() &&;
};

inline MediaConfiguration MediaConfiguration::isolatedCopy() const &
{
    return {
        crossThreadCopy(video),
        crossThreadCopy(audio),
    };
}

inline MediaConfiguration MediaConfiguration::isolatedCopy() &&
{
    return {
        crossThreadCopy(WTF::move(video)),
        crossThreadCopy(WTF::move(audio)),
    };
}

inline PlatformMediaConfiguration toPlatform(MediaConfiguration&& value)
{
    return {
        value.video ? std::optional { toPlatform(WTF::move(*value.video)) } : std::nullopt,
        value.audio ? std::optional { toPlatform(WTF::move(*value.audio)) } : std::nullopt,
        { },
        { },
    };
}

inline MediaConfiguration fromPlatform(PlatformMediaConfiguration&& value)
{
    return {
        value.video ? std::optional { fromPlatform(WTF::move(*value.video)) } : std::nullopt,
        value.audio ? std::optional { fromPlatform(WTF::move(*value.audio)) } : std::nullopt,
    };
}

} // namespace WebCore

