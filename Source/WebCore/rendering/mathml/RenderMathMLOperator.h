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

#include "GlyphPage.h"
#include "MathMLElement.h"
#include "RenderMathMLToken.h"

namespace WebCore {
    
namespace MathMLOperatorDictionary {

enum Form { Infix, Prefix, Postfix };
enum Flag {
    Accent = 0x1, // FIXME: This must be used to implement accentunder/accent on munderover (https://bugs.webkit.org/show_bug.cgi?id=124826).
    Fence = 0x2, // This has no visual effect but allows to expose semantic information via the accessibility tree.
    LargeOp = 0x4, // FIXME: This must be used to implement displaystyle (https://bugs.webkit.org/show_bug.cgi?id=118737)
    MovableLimits = 0x8, // FIXME: This must be used to implement displaystyle  (https://bugs.webkit.org/show_bug.cgi?id=118737).
    Separator = 0x10, // This has no visual effect but allows to expose semantic information via the accessibility tree.
    Stretchy = 0x20,
    Symmetric = 0x40
};
struct Entry {
    UChar character;
    Form form;
    unsigned short lspace;
    unsigned short rspace;
    unsigned short flags;
};

}

class RenderMathMLOperator final : public RenderMathMLToken {
public:
    RenderMathMLOperator(MathMLElement&, PassRef<RenderStyle>);
    RenderMathMLOperator(Document&, PassRef<RenderStyle>, const String& operatorString, MathMLOperatorDictionary::Form, MathMLOperatorDictionary::Flag);

    void stretchTo(LayoutUnit heightAboveBaseline, LayoutUnit depthBelowBaseline);
    LayoutUnit stretchSize() const { return m_stretchHeightAboveBaseline + m_stretchDepthBelowBaseline; }
    
    bool hasOperatorFlag(MathMLOperatorDictionary::Flag flag) const { return m_operatorFlags & flag; }

    void updateStyle() override final;

    void paint(PaintInfo&, const LayoutPoint&);

    void updateTokenContent(const String& operatorString);
    void updateTokenContent() override final;
    void updateOperatorProperties();

private:
    virtual const char* renderName() const override { return isAnonymous() ? "RenderMathMLOperator (anonymous)" : "RenderMathMLOperator"; }
    virtual void paintChildren(PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect) override;
    virtual bool isRenderMathMLOperator() const override { return true; }
    bool isFencedOperator() { return isAnonymous(); }
    // The following operators are invisible: U+2061 FUNCTION APPLICATION, U+2062 INVISIBLE TIMES, U+2063 INVISIBLE SEPARATOR, U+2064 INVISIBLE PLUS.
    bool isInvisibleOperator() const { return 0x2061 <= m_operator && m_operator <= 0x2064; }
    virtual bool isChildAllowed(const RenderObject&, const RenderStyle&) const override;
    virtual void computePreferredLogicalWidths() override;
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;
    virtual int firstLineBaseline() const override;
    virtual RenderMathMLOperator* unembellishedOperator() override { return this; }
    void rebuildTokenContent(const String& operatorString);
    void updateFromElement() override;

    bool shouldAllowStretching(UChar& characterForStretching);

    FloatRect boundsForGlyph(const GlyphData&);
    float heightForGlyph(const GlyphData&);
    float advanceForGlyph(const GlyphData&);

    // FIXME: DrawSizeVariant is not implemented yet.
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
    StretchyData findStretchyData(UChar, float* maximumGlyphWidth);
    
    enum GlyphPaintTrimming {
        TrimTop,
        TrimBottom,
        TrimTopAndBottom,
    };

    LayoutRect paintGlyph(PaintInfo&, const GlyphData&, const LayoutPoint& origin, GlyphPaintTrimming);
    void fillWithExtensionGlyph(PaintInfo&, const LayoutPoint& from, const LayoutPoint& to);

    LayoutUnit m_stretchHeightAboveBaseline;
    LayoutUnit m_stretchDepthBelowBaseline;

    UChar m_operator;
    StretchyData m_stretchyData;
    MathMLOperatorDictionary::Form m_operatorForm;
    unsigned short m_operatorFlags;
    LayoutUnit m_leadingSpace;
    LayoutUnit m_trailingSpace;
    LayoutUnit m_minSize;
    LayoutUnit m_maxSize;

    void setOperatorFlagFromAttribute(MathMLOperatorDictionary::Flag, const QualifiedName&);
    void setOperatorPropertiesFromOpDictEntry(const MathMLOperatorDictionary::Entry*);
    void SetOperatorProperties();
};

RENDER_OBJECT_TYPE_CASTS(RenderMathMLOperator, isRenderMathMLOperator())

}

#endif // ENABLE(MATHML)
#endif // RenderMathMLOperator_h
