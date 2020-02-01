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
#include <wtf/Optional.h>

namespace WebCore {

struct CompositionHighlight {
    CompositionHighlight() = default;
    CompositionHighlight(unsigned startOffset, unsigned endOffset, const Color& c)
        : startOffset(startOffset)
        , endOffset(endOffset)
        , color(c)
    {
    }

    unsigned startOffset { 0 };
    unsigned endOffset { 0 };
    Color color { Color::compositionFill };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<CompositionHighlight> decode(Decoder&);
};

template<class Encoder>
void CompositionHighlight::encode(Encoder& encoder) const
{
    encoder << startOffset;
    encoder << endOffset;
    encoder << color;
}

template<class Decoder>
Optional<CompositionHighlight> CompositionHighlight::decode(Decoder& decoder)
{
    Optional<unsigned> startOffset;
    decoder >> startOffset;
    if (!startOffset)
        return WTF::nullopt;

    Optional<unsigned> endOffset;
    decoder >> endOffset;
    if (!endOffset)
        return WTF::nullopt;

    Optional<Color> color;
    decoder >> color;
    if (!color)
        return WTF::nullopt;

    return {{ *startOffset, *endOffset, *color }};
}

} // namespace WebCore
