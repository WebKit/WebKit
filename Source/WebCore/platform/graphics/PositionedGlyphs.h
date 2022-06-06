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

#include "GlyphBufferMembers.h"
#include "TextFlags.h"

namespace WebCore {

class Font;

struct PositionedGlyphs {
    PositionedGlyphs(Vector<GlyphBufferGlyph>&&, Vector<GlyphBufferAdvance>&&, const FloatPoint&, FontSmoothingMode);
    PositionedGlyphs(const PositionedGlyphs&) = default;
    PositionedGlyphs(PositionedGlyphs&&) = default;
    PositionedGlyphs& operator=(const PositionedGlyphs&) = default;

    Vector<GlyphBufferGlyph> glyphs;
    Vector<GlyphBufferAdvance> advances;
    FloatPoint localAnchor;
    FontSmoothingMode smoothingMode;

    FloatRect computeBounds(const Font&) const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<PositionedGlyphs> decode(Decoder&);
};

inline PositionedGlyphs::PositionedGlyphs(Vector<GlyphBufferGlyph>&& glyphs, Vector<GlyphBufferAdvance>&& advances, const FloatPoint& localAnchor, FontSmoothingMode smoothingMode)
    : glyphs(WTFMove(glyphs))
    , advances(WTFMove(advances))
    , localAnchor(localAnchor)
    , smoothingMode(smoothingMode)
{
}

template<class Encoder>
void PositionedGlyphs::encode(Encoder& encoder) const
{
    encoder << glyphs;
    encoder << advances;
    encoder << localAnchor;
    encoder << smoothingMode;
}

template<class Decoder>
std::optional<PositionedGlyphs> PositionedGlyphs::decode(Decoder& decoder)
{
    std::optional<Vector<GlyphBufferGlyph>> glyphs;
    decoder >> glyphs;
    if (!glyphs)
        return std::nullopt;

    std::optional<Vector<GlyphBufferAdvance>> advances;
    decoder >> advances;
    if (!advances)
        return std::nullopt;

    if (glyphs->size() != advances->size())
        return std::nullopt;

    std::optional<FloatPoint> localAnchor;
    decoder >> localAnchor;
    if (!localAnchor)
        return std::nullopt;

    std::optional<FontSmoothingMode> smoothingMode;
    decoder >> smoothingMode;
    if (!smoothingMode)
        return std::nullopt;

    return { { WTFMove(*glyphs), WTFMove(*advances), *localAnchor, *smoothingMode } };
}

} // namespace WebCore
