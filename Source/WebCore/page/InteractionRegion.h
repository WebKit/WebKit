/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "FloatRect.h"

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebCore {

class Page;

struct InteractionRegion {
    Vector<FloatRect> rectsInContentCoordinates;
    bool isInline { false };
    bool hasLightBackground { false };
    float borderRadius { 0 };
    
    WEBCORE_EXPORT ~InteractionRegion();
    
    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<InteractionRegion> decode(Decoder&);
};

WEBCORE_EXPORT Vector<InteractionRegion> interactionRegions(Page&, FloatRect rectInContentCoordinates);

template<class Encoder>
void InteractionRegion::encode(Encoder& encoder) const
{
    encoder << rectsInContentCoordinates;
    encoder << isInline;
    encoder << hasLightBackground;
    encoder << borderRadius;
}

template<class Decoder>
std::optional<InteractionRegion> InteractionRegion::decode(Decoder& decoder)
{
    std::optional<Vector<FloatRect>> rectsInContentCoordinates;
    decoder >> rectsInContentCoordinates;
    if (!rectsInContentCoordinates)
        return std::nullopt;
    
    std::optional<bool> isInline;
    decoder >> isInline;
    if (!isInline)
        return std::nullopt;
    
    std::optional<bool> hasLightBackground;
    decoder >> hasLightBackground;
    if (!hasLightBackground)
        return std::nullopt;
    
    std::optional<float> borderRadius;
    decoder >> borderRadius;
    if (!borderRadius)
        return std::nullopt;

    return { {
        WTFMove(*rectsInContentCoordinates),
        WTFMove(*isInline),
        WTFMove(*hasLightBackground),
        WTFMove(*borderRadius)
    } };
}

}
