/*
 * Copyright (C) 2016 Igalia S.L. All rights reserved.
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MATHML)

#include "GlyphPage.h"
#include "LayoutUnit.h"
#include "OpenTypeMathData.h"
#include "PaintInfo.h"
#include <unicode/utypes.h>

namespace WebCore {

class RenderStyle;

class MathOperator {
public:
    MathOperator();
    enum class Type { NormalOperator, DisplayOperator, VerticalOperator, HorizontalOperator };
    void setOperator(const RenderStyle&, UChar32 baseCharacter, Type);
    void reset(const RenderStyle&);

    LayoutUnit width() const { return m_width; }
    LayoutUnit maxPreferredWidth() const { return m_maxPreferredWidth; }
    LayoutUnit ascent() const { return m_ascent; }
    LayoutUnit descent() const { return m_descent; }
    LayoutUnit italicCorrection() const { return m_italicCorrection; }

    void stretchTo(const RenderStyle&, LayoutUnit width);

    void paint(const RenderStyle&, PaintInfo&, const LayoutPoint&);

private:
    struct GlyphAssemblyData {
        UChar32 topOrRightCodePoint { 0 };
        Glyph topOrRightFallbackGlyph { 0 };
        UChar32 extensionCodePoint { 0 };
        Glyph extensionFallbackGlyph { 0 };
        UChar32 bottomOrLeftCodePoint { 0 };
        Glyph bottomOrLeftFallbackGlyph { 0 };
        UChar32 middleCodePoint { 0 };
        Glyph middleFallbackGlyph { 0 };

        bool hasExtension() const { return extensionCodePoint || extensionFallbackGlyph; }
        bool hasMiddle() const { return middleCodePoint || middleFallbackGlyph; }
        void initialize();
    };
    enum class StretchType { Unstretched, SizeVariant, GlyphAssembly };
    enum GlyphPaintTrimming {
        TrimTop,
        TrimBottom,
        TrimTopAndBottom,
        TrimLeft,
        TrimRight,
        TrimLeftAndRight
    };

    LayoutUnit stretchSize() const;
    bool getGlyph(const RenderStyle&, UChar32 character, GlyphData&) const;
    bool getBaseGlyph(const RenderStyle& style, GlyphData& baseGlyph) const { return getGlyph(style, m_baseCharacter, baseGlyph); }
    void setSizeVariant(const GlyphData&);
    void setGlyphAssembly(const RenderStyle&, const GlyphAssemblyData&);
    void getMathVariantsWithFallback(const RenderStyle&, bool isVertical, Vector<Glyph>&, Vector<OpenTypeMathData::AssemblyPart>&);
    void calculateDisplayStyleLargeOperator(const RenderStyle&);
    void calculateStretchyData(const RenderStyle&, bool calculateMaxPreferredWidth, LayoutUnit targetSize = 0_lu);
    bool calculateGlyphAssemblyFallback(const Vector<OpenTypeMathData::AssemblyPart>&, GlyphAssemblyData&) const;

    LayoutRect paintGlyph(const RenderStyle&, PaintInfo&, const GlyphData&, const LayoutPoint& origin, GlyphPaintTrimming);
    void fillWithVerticalExtensionGlyph(const RenderStyle&, PaintInfo&, const LayoutPoint& from, const LayoutPoint& to);
    void fillWithHorizontalExtensionGlyph(const RenderStyle&, PaintInfo&, const LayoutPoint& from, const LayoutPoint& to);
    void paintVerticalGlyphAssembly(const RenderStyle&, PaintInfo&, const LayoutPoint&);
    void paintHorizontalGlyphAssembly(const RenderStyle&, PaintInfo&, const LayoutPoint&);

    UChar32 m_baseCharacter { 0 };
    Type m_operatorType { Type::NormalOperator };
    StretchType m_stretchType { StretchType::Unstretched };
    union {
        Glyph m_variantGlyph;
        GlyphAssemblyData m_assembly;
    };
    LayoutUnit m_maxPreferredWidth { 0 };
    LayoutUnit m_width { 0 };
    LayoutUnit m_ascent { 0 };
    LayoutUnit m_descent { 0 };
    LayoutUnit m_italicCorrection { 0 };
    float m_radicalVerticalScale { 1 };
};

}

#endif // ENABLE(MATHML)
