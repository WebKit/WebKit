/*
 * Copyright (C) 2010 Alex Milowski (alex@milowski.com). All rights reserved.
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

#ifndef RenderMathMLOperator_h
#define RenderMathMLOperator_h

#if ENABLE(MATHML)

#include "Font.h"
#include "GlyphPage.h"
#include "MathMLElement.h"
#include "MathMLOperatorDictionary.h"
#include "OpenTypeMathData.h"
#include "RenderMathMLToken.h"

namespace WebCore {

class RenderMathMLOperator : public RenderMathMLToken {
public:
    RenderMathMLOperator(MathMLElement&, Ref<RenderStyle>&&);
    RenderMathMLOperator(Document&, Ref<RenderStyle>&&, const String& operatorString, MathMLOperatorDictionary::Form, unsigned short flags = 0);

    virtual void stretchTo(LayoutUnit heightAboveBaseline, LayoutUnit depthBelowBaseline);
    void stretchTo(LayoutUnit width);
    LayoutUnit stretchSize() const { return m_isVertical ? m_stretchHeightAboveBaseline + m_stretchDepthBelowBaseline : m_stretchWidth; }
    void resetStretchSize();
    
    bool hasOperatorFlag(MathMLOperatorDictionary::Flag flag) const { return m_operatorFlags & flag; }
    // FIXME: The displaystyle property is not implemented (https://bugs.webkit.org/show_bug.cgi?id=118737).
    bool isLargeOperatorInDisplayStyle() const { return !hasOperatorFlag(MathMLOperatorDictionary::Stretchy) && hasOperatorFlag(MathMLOperatorDictionary::LargeOp); }
    bool isVertical() const { return m_isVertical; }

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) final;
    void updateStyle() final;

    void paint(PaintInfo&, const LayoutPoint&) override;

    void updateTokenContent(const String& operatorString);
    void updateTokenContent() final;
    void updateOperatorProperties();
    void updateFromElement() final;
    LayoutUnit trailingSpaceError();

protected:
    virtual void setOperatorProperties();
    void computePreferredLogicalWidths() override;
    void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;
    float advanceForGlyph(const GlyphData&) const;
    void setLeadingSpace(LayoutUnit leadingSpace) { m_leadingSpace = leadingSpace; }
    void setTrailingSpace(LayoutUnit trailingSpace) { m_trailingSpace = trailingSpace; }
    UChar textContent() const { return m_textContent; }

private:
    enum DrawMode {
        DrawNormal, DrawSizeVariant, DrawGlyphAssembly
    };

    class StretchyData {
    public:
        DrawMode mode() const { return m_mode; }
        GlyphData variant() const { return m_data[0]; }
        GlyphData top() const { return m_data[0]; }
        GlyphData extension() const { return m_data[1]; }
        GlyphData bottom() const { return m_data[2]; }
        GlyphData middle() const { return m_data[3]; }
        GlyphData left() const { return m_data[2]; }
        GlyphData right() const { return m_data[0]; }

        void setNormalMode()
        {
            m_mode = DrawNormal;
        }
        void setSizeVariantMode(const GlyphData& variant)
        {
            m_mode = DrawSizeVariant;
            m_data[0] = variant;
        }
        void setGlyphAssemblyMode(const GlyphData& top, const GlyphData& extension, const GlyphData& bottom, const GlyphData& middle)
        {
            m_mode = DrawGlyphAssembly;
            m_data[0] = top;
            m_data[1] = extension;
            m_data[2] = bottom;
            m_data[3] = middle;
        }
        StretchyData()
            : m_mode(DrawNormal) { }
        StretchyData(const StretchyData& data)
        {
            switch (data.m_mode) {
            case DrawNormal:
                setNormalMode();
                break;
            case DrawSizeVariant:
                setSizeVariantMode(data.variant());
                break;
            case DrawGlyphAssembly:
                setGlyphAssemblyMode(data.top(), data.extension(), data.bottom(), data.middle());
                break;
            }
        }

    private:
        DrawMode m_mode;
        // FIXME: For OpenType fonts with a MATH table all the glyphs are from the same font, so we would only need to store the glyph indices here.
        GlyphData m_data[4];
    };

    const char* renderName() const override { return isAnonymous() ? "RenderMathMLOperator (anonymous)" : "RenderMathMLOperator"; }
    void paintChildren(PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect) final;
    bool isRenderMathMLOperator() const final { return true; }
    // The following operators are invisible: U+2061 FUNCTION APPLICATION, U+2062 INVISIBLE TIMES, U+2063 INVISIBLE SEPARATOR, U+2064 INVISIBLE PLUS.
    bool isInvisibleOperator() const { return 0x2061 <= m_textContent && m_textContent <= 0x2064; }
    bool isChildAllowed(const RenderObject&, const RenderStyle&) const final;

    Optional<int> firstLineBaseline() const final;
    RenderMathMLOperator* unembellishedOperator() final { return this; }
    void rebuildTokenContent(const String& operatorString);

    bool shouldAllowStretching() const;

    FloatRect boundsForGlyph(const GlyphData&) const;
    float heightForGlyph(const GlyphData&) const;

    bool getGlyphAssemblyFallBack(Vector<OpenTypeMathData::AssemblyPart>, StretchyData&) const;
    StretchyData getDisplayStyleLargeOperator(UChar) const;
    StretchyData findStretchyData(UChar, float* maximumGlyphWidth);

    enum GlyphPaintTrimming {
        TrimTop,
        TrimBottom,
        TrimTopAndBottom,
        TrimLeft,
        TrimRight,
        TrimLeftAndRight
    };

    LayoutRect paintGlyph(PaintInfo&, const GlyphData&, const LayoutPoint& origin, GlyphPaintTrimming);
    void fillWithVerticalExtensionGlyph(PaintInfo&, const LayoutPoint& from, const LayoutPoint& to);
    void fillWithHorizontalExtensionGlyph(PaintInfo&, const LayoutPoint& from, const LayoutPoint& to);
    void paintVerticalGlyphAssembly(PaintInfo&, const LayoutPoint&);
    void paintHorizontalGlyphAssembly(PaintInfo&, const LayoutPoint&);
    void setOperatorFlagFromAttribute(MathMLOperatorDictionary::Flag, const QualifiedName&);
    void setOperatorFlagFromAttributeValue(MathMLOperatorDictionary::Flag, const AtomicString& attributeValue);
    void setOperatorPropertiesFromOpDictEntry(const MathMLOperatorDictionary::Entry*);

    LayoutUnit m_stretchHeightAboveBaseline;
    LayoutUnit m_stretchDepthBelowBaseline;
    LayoutUnit m_stretchWidth;

    UChar m_textContent;
    bool m_isVertical;
    MathMLOperatorDictionary::Form m_operatorForm;
    unsigned short m_operatorFlags;
    LayoutUnit m_leadingSpace;
    LayoutUnit m_trailingSpace;
    LayoutUnit m_minSize;
    LayoutUnit m_maxSize;
    StretchyData m_stretchyData;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMathMLOperator, isRenderMathMLOperator())

#endif // ENABLE(MATHML)
#endif // RenderMathMLOperator_h
