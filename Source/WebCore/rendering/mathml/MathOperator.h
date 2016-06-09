/*
 * Copyright (C) 2016 Igalia S.L. All rights reserved.
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

#ifndef MathOperator_h
#define MathOperator_h

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
    MathOperator() { }
    enum class Type { UndefinedOperator, DisplayOperator, VerticalOperator, HorizontalOperator };
    void setOperator(UChar baseCharacter, Type);

    LayoutUnit italicCorrection() const { return m_italicCorrection; }

    bool isStretched() const { return m_stretchType != StretchType::Unstretched; }
    void unstretch() { m_stretchType = StretchType::Unstretched; }

    // FIXME: All the code below should be private when it is no longer needed by RenderMathMLOperator (http://webkit.org/b/152244).

    struct GlyphAssemblyData {
        GlyphData topOrRight;
        GlyphData extension;
        GlyphData bottomOrLeft;
        GlyphData middle;
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
    bool getBaseGlyph(const RenderStyle&, GlyphData&) const;
    void setSizeVariant(const GlyphData&);
    void setGlyphAssembly(const GlyphAssemblyData&);
    void calculateDisplayStyleLargeOperator(const RenderStyle&);
    void calculateStretchyData(const RenderStyle&, float* maximumGlyphWidth, LayoutUnit targetSize = 0);
    bool calculateGlyphAssemblyFallback(const RenderStyle&, const Vector<OpenTypeMathData::AssemblyPart>&, GlyphAssemblyData&) const;

    LayoutRect paintGlyph(const RenderStyle&, PaintInfo&, const GlyphData&, const LayoutPoint& origin, GlyphPaintTrimming);
    void fillWithVerticalExtensionGlyph(const RenderStyle&, PaintInfo&, const LayoutPoint& from, const LayoutPoint& to);
    void fillWithHorizontalExtensionGlyph(const RenderStyle&, PaintInfo&, const LayoutPoint& from, const LayoutPoint& to);
    void paintVerticalGlyphAssembly(const RenderStyle&, PaintInfo&, const LayoutPoint&);
    void paintHorizontalGlyphAssembly(const RenderStyle&, PaintInfo&, const LayoutPoint&);

    UChar m_baseCharacter = 0;
    Type m_operatorType { Type::UndefinedOperator };
    StretchType m_stretchType = StretchType::Unstretched;
    union {
        GlyphData m_variant;
        GlyphAssemblyData m_assembly;
    };
    LayoutUnit m_width = 0;
    LayoutUnit m_ascent = 0;
    LayoutUnit m_descent = 0;
    LayoutUnit m_italicCorrection = 0;
};

}

#endif // ENABLE(MATHML)
#endif // MathOperator_h
