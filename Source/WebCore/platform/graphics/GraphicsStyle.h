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
#include <wtf/Vector.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

struct GraphicsDropShadow {
    FloatSize offset;
    FloatSize radius;
    Color color;
    
    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<GraphicsDropShadow> decode(Decoder&);
};

inline bool operator==(const GraphicsDropShadow& a, const GraphicsDropShadow& b)
{
    return a.offset == b.offset && a.radius == b.radius && a.color == b.color;
}

struct GraphicsGaussianBlur {
    FloatSize radius;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<GraphicsGaussianBlur> decode(Decoder&);
};

inline bool operator==(const GraphicsGaussianBlur& a, const GraphicsGaussianBlur& b)
{
    return a.radius == b.radius;
}

struct GraphicsColorMatrix {
    std::array<float, 20> values;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<GraphicsColorMatrix> decode(Decoder&);
};

inline bool operator==(const GraphicsColorMatrix& a, const GraphicsColorMatrix& b)
{
    return a.values == b.values;
}

using GraphicsStyle = std::variant<
    GraphicsDropShadow,
    GraphicsGaussianBlur,
    GraphicsColorMatrix
>;

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const GraphicsDropShadow&);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const GraphicsGaussianBlur&);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const GraphicsColorMatrix&);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const GraphicsStyle&);

template<class Encoder>
void GraphicsDropShadow::encode(Encoder& encoder) const
{
    encoder << offset;
    encoder << radius;
    encoder << color;
}

template<class Decoder>
std::optional<GraphicsDropShadow> GraphicsDropShadow::decode(Decoder& decoder)
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

    return GraphicsDropShadow { *offset, *radius, *color };
}

template<class Encoder>
void GraphicsGaussianBlur::encode(Encoder& encoder) const
{
    encoder << radius;
}

template<class Decoder>
std::optional<GraphicsGaussianBlur> GraphicsGaussianBlur::decode(Decoder& decoder)
{
    std::optional<FloatSize> radius;
    decoder >> radius;
    if (!radius)
        return std::nullopt;

    return GraphicsGaussianBlur { *radius };
}

template<class Encoder>
void GraphicsColorMatrix::encode(Encoder& encoder) const
{
    encoder << values;
}

template<class Decoder>
std::optional<GraphicsColorMatrix> GraphicsColorMatrix::decode(Decoder& decoder)
{
    std::optional<std::array<float, 20>> values;
    decoder >> values;
    if (!values)
        return std::nullopt;

    return GraphicsColorMatrix { WTFMove(*values) };
}

} // namespace WebCore
