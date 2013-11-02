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

#include "RenderMathMLBlock.h"
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {
    
class RenderMathMLOperator : public RenderMathMLBlock {
public:
    RenderMathMLOperator(Element*);
    RenderMathMLOperator(Element*, UChar operatorChar);

    virtual bool isRenderMathMLOperator() const { return true; }
    
    virtual bool isChildAllowed(RenderObject*, RenderStyle*) const;
    virtual void updateFromElement() OVERRIDE;
    virtual void computePreferredLogicalWidths() OVERRIDE;
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const OVERRIDE;
    
    virtual RenderMathMLOperator* unembellishedOperator() OVERRIDE { return this; }
    void stretchToHeight(int pixelHeight);
    int stretchHeight() const { return m_stretchHeight; }
    float expandedStretchHeight() const;
    
    virtual int firstLineBoxBaseline() const OVERRIDE;
    
    enum OperatorType { Default, Separator, Fence };
    void setOperatorType(OperatorType type) { m_operatorType = type; }
    OperatorType operatorType() const { return m_operatorType; }
  
    void updateStyle();

    void paint(PaintInfo&, const LayoutPoint&);

    struct StretchyCharacter {
        UChar character;
        UChar topGlyph;
        UChar extensionGlyph;
        UChar bottomGlyph;
        UChar middleGlyph;
    };

private:
    virtual const char* renderName() const { return isAnonymous() ? "RenderMathMLOperator (anonymous)" : "RenderMathMLOperator"; }
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) OVERRIDE;
    virtual void paintChildren(PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect) OVERRIDE;

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

    int m_stretchHeight;
    bool m_isStretched;

    UChar m_operator;
    OperatorType m_operatorType;
    StretchyCharacter* m_stretchyCharacter;
};

inline RenderMathMLOperator* toRenderMathMLOperator(RenderMathMLBlock* block)
{ 
    ASSERT_WITH_SECURITY_IMPLICATION(!block || block->isRenderMathMLOperator());
    return static_cast<RenderMathMLOperator*>(block);
}

inline const RenderMathMLOperator* toRenderMathMLOperator(const RenderMathMLBlock* block)
{ 
    ASSERT_WITH_SECURITY_IMPLICATION(!block || block->isRenderMathMLOperator());
    return static_cast<const RenderMathMLOperator*>(block);
}

// This will catch anyone doing an unnecessary cast.
void toRenderMathMLOperator(const RenderMathMLOperator*);

inline UChar convertHyphenMinusToMinusSign(UChar glyph)
{
    // When rendered as a mathematical operator, minus glyph should be larger.
    if (glyph == hyphenMinus)
        return minusSign;
    
    return glyph;
}

}

#endif // ENABLE(MATHML)
#endif // RenderMathMLOperator_h
