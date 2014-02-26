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

#include "MathMLElement.h"
#include "RenderMathMLBlock.h"
#include <wtf/unicode/CharacterNames.h>

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
    // FIXME: spacing around <mo> operators is not implemented yet (https://bugs.webkit.org/show_bug.cgi?id=115787).
    unsigned short lspace;
    unsigned short rspace;
    unsigned short flags;
};

}

class RenderMathMLOperator final : public RenderMathMLBlock {
public:
    RenderMathMLOperator(MathMLElement&, PassRef<RenderStyle>);
    RenderMathMLOperator(MathMLElement&, PassRef<RenderStyle>, UChar operatorChar, MathMLOperatorDictionary::Form, MathMLOperatorDictionary::Flag);

    MathMLElement& element() { return toMathMLElement(nodeForNonAnonymous()); }

    void stretchTo(LayoutUnit heightAboveBaseline, LayoutUnit depthBelowBaseline);
    LayoutUnit stretchSize() const { return m_stretchHeightAboveBaseline + m_stretchDepthBelowBaseline; }
    
    bool hasOperatorFlag(MathMLOperatorDictionary::Flag flag) const { return m_operatorFlags & flag; }

    void updateStyle();

    void paint(PaintInfo&, const LayoutPoint&);

    struct StretchyCharacter {
        UChar character;
        UChar topGlyph;
        UChar extensionGlyph;
        UChar bottomGlyph;
        UChar middleGlyph;
    };

    virtual void updateFromElement() override;

private:
    virtual const char* renderName() const override { return isAnonymous() ? "RenderMathMLOperator (anonymous)" : "RenderMathMLOperator"; }
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;
    virtual void paintChildren(PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect) override;
    virtual bool isRenderMathMLOperator() const override { return true; }
    virtual bool isChildAllowed(const RenderObject&, const RenderStyle&) const override;
    virtual void computePreferredLogicalWidths() override;
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;
    virtual int firstLineBaseline() const override;
    virtual RenderMathMLOperator* unembellishedOperator() override { return this; }

    bool shouldAllowStretching(UChar& characterForStretching);
    StretchyCharacter* findAcceptableStretchyCharacter(UChar);

    FloatRect glyphBoundsForCharacter(UChar);
    float glyphHeightForCharacter(UChar);
    float advanceForCharacter(UChar);

    enum CharacterPaintTrimming {
        TrimTop,
        TrimBottom,
        TrimTopAndBottom,
    };

    LayoutRect paintCharacter(PaintInfo&, UChar, const LayoutPoint& origin, CharacterPaintTrimming);
    void fillWithExtensionGlyph(PaintInfo&, const LayoutPoint& from, const LayoutPoint& to);

    LayoutUnit m_stretchHeightAboveBaseline;
    LayoutUnit m_stretchDepthBelowBaseline;
    bool m_isStretched;

    UChar m_operator;
    StretchyCharacter* m_stretchyCharacter;
    bool m_isFencedOperator;
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
