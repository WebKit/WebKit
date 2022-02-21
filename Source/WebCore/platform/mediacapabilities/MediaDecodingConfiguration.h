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

#include "MediaConfiguration.h"
#include "MediaDecodingType.h"

namespace WebCore {

struct MediaDecodingConfiguration : MediaConfiguration {
    MediaDecodingType type;

    bool canExposeVP9 { true };

    MediaDecodingConfiguration isolatedCopy() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<MediaDecodingConfiguration> decode(Decoder&);
};

inline MediaDecodingConfiguration MediaDecodingConfiguration::isolatedCopy() const
{
    return { MediaConfiguration::isolatedCopy(), type, canExposeVP9 };
}

template<class Encoder>
void MediaDecodingConfiguration::encode(Encoder& encoder) const
{
    MediaConfiguration::encode(encoder);
    encoder << type << canExposeVP9;
}

template<class Decoder>
std::optional<MediaDecodingConfiguration> MediaDecodingConfiguration::decode(Decoder& decoder)
{
    auto mediaConfiguration = MediaConfiguration::decode(decoder);
    if (!mediaConfiguration)
        return std::nullopt;

    std::optional<MediaDecodingType> type;
    decoder >> type;
    if (!type)
        return std::nullopt;

    std::optional<bool> canExposeVP9;
    decoder >> canExposeVP9;
    if (!canExposeVP9)
        return std::nullopt;

    return {{
        *mediaConfiguration,
        *type,
        *canExposeVP9
    }};
}

} // namespace WebCore

