/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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

#include "Color.h"
#include "FloatSize.h"

namespace WTF {
class TextStream;
}

namespace WebCore {

struct GraphicsStyleDropShadow {
    FloatSize offset;
    FloatSize radius;
    Color color;
    
    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<GraphicsStyleDropShadow> decode(Decoder&);
};

inline bool operator==(const GraphicsStyleDropShadow& a, const GraphicsStyleDropShadow& b)
{
    return a.offset == b.offset && a.radius == b.radius && a.color == b.color;
}

using GraphicsStyle = std::variant<
    GraphicsStyleDropShadow
>;

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const GraphicsStyleDropShadow&);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const GraphicsStyle&);

template<class Encoder>
void GraphicsStyleDropShadow::encode(Encoder& encoder) const
{
    encoder << offset;
    encoder << radius;
    encoder << color;
}

template<class Decoder>
std::optional<GraphicsStyleDropShadow> GraphicsStyleDropShadow::decode(Decoder& decoder)
{
    std::optional<FloatSize> offset;
    decoder >> offset;
    if (!offset)
        return std::nullopt;

    std::optional<FloatSize> radius;
    decoder >> radius;
    if (!radius)
        return std::nullopt;

    std::optional<Color> color;
    decoder >> color;
    if (!color)
        return std::nullopt;

    return GraphicsStyleDropShadow { *offset, *radius, *color };
}

} // namespace WebCore
