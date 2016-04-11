/*
 * Copyright (C) 2009 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2010 François Sausset (sausset@gmail.com). All rights reserved.
 * Copyright (C) 2016 Igalia S.L.
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

#include "config.h"

#if ENABLE(MATHML)

#include "RenderMathMLFraction.h"

#include "GraphicsContext.h"
#include "MathMLNames.h"
#include "PaintInfo.h"

namespace WebCore {
    
using namespace MathMLNames;

// FIXME: The "MathML in HTML5" implementation note suggests the values 50% for "thin" and 200% for "thick" (http://webkit.org/b/155639).
static const float gLineThin = 0.33f;
static const float gLineMedium = 1.f;
static const float gLineThick = 3.f;
static const float gFractionBarWidth = 0.05f;

// FIXME: We should read the gaps and shifts from the MATH table (http://webkit.org/b/155639).
static const float gNumeratorGap = 0.2f;
static const float gDenominatorGap = 0.2f;

RenderMathMLFraction::RenderMathMLFraction(MathMLInlineContainerElement& element, Ref<RenderStyle>&& style)
    : RenderMathMLBlock(element, WTFMove(style))
    , m_defaultLineThickness(1)
    , m_lineThickness(0)
    , m_numeratorAlign(FractionAlignmentCenter)
    , m_denominatorAlign(FractionAlignmentCenter)
{
}

RenderMathMLFraction::FractionAlignment RenderMathMLFraction::parseAlignmentAttribute(const String& value)
{
    if (equalLettersIgnoringASCIICase(value, "left"))
        return FractionAlignmentLeft;
    if (equalLettersIgnoringASCIICase(value, "right"))
        return FractionAlignmentRight;
    return FractionAlignmentCenter;
}

bool RenderMathMLFraction::isValid() const
{
    // Verify whether the list of children is valid:
    // <mfrac> numerator denominator </mfrac>
    RenderBox* child = firstChildBox();
    if (!child)
        return false;
    child = child->nextSiblingBox();
    return child && !child->nextSiblingBox();
}

RenderBox& RenderMathMLFraction::numerator() const
{
    ASSERT(isValid());
    return *firstChildBox();
}

RenderBox& RenderMathMLFraction::denominator() const
{
    ASSERT(isValid());
    return *firstChildBox()->nextSiblingBox();
}

void RenderMathMLFraction::updateFromElement()
{
    if (isEmpty())
        return;

    // We first determine the default line thickness.
    const auto& primaryFont = style().fontCascade().primaryFont();
    if (auto* mathData = style().fontCascade().primaryFont().mathData())
        m_defaultLineThickness = mathData->getMathConstant(primaryFont, OpenTypeMathData::FractionRuleThickness);
    else
        m_defaultLineThickness = gFractionBarWidth * style().fontSize();

    // Next, we parse the linethickness attribute.
    String thickness = element().getAttribute(MathMLNames::linethicknessAttr);
    if (equalLettersIgnoringASCIICase(thickness, "thin"))
        m_lineThickness = m_defaultLineThickness * gLineThin;
    else if (equalLettersIgnoringASCIICase(thickness, "medium"))
        m_lineThickness = m_defaultLineThickness * gLineMedium;
    else if (equalLettersIgnoringASCIICase(thickness, "thick"))
        m_lineThickness = m_defaultLineThickness * gLineThick;
    else {
        // Parse the thickness using m_defaultLineThickness as the default value.
        m_lineThickness = m_defaultLineThickness;
        parseMathMLLength(thickness, m_lineThickness, &style(), false);
    }

    m_numeratorAlign = parseAlignmentAttribute(element().getAttribute(MathMLNames::numalignAttr));
    m_denominatorAlign = parseAlignmentAttribute(element().getAttribute(MathMLNames::denomalignAttr));
}

void RenderMathMLFraction::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderMathMLBlock::styleDidChange(diff, oldStyle);
    
    updateFromElement();
}

RenderMathMLOperator* RenderMathMLFraction::unembellishedOperator()
{
    if (!isValid() || !is<RenderMathMLBlock>(numerator()))
        return nullptr;

    return downcast<RenderMathMLBlock>(numerator()).unembellishedOperator();
}

void RenderMathMLFraction::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = 0;

    if (isValid()) {
        LayoutUnit numeratorWidth = numerator().maxPreferredLogicalWidth();
        LayoutUnit denominatorWidth = denominator().maxPreferredLogicalWidth();
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = std::max(numeratorWidth, denominatorWidth);
    }

    setPreferredLogicalWidthsDirty(false);
}

LayoutUnit RenderMathMLFraction::horizontalOffset(RenderBox& child, FractionAlignment align)
{
    switch (align) {
    case FractionAlignmentRight:
        return LayoutUnit(logicalWidth() - child.logicalWidth());
    case FractionAlignmentCenter:
        return LayoutUnit((logicalWidth() - child.logicalWidth()) / 2);
    case FractionAlignmentLeft:
        return LayoutUnit(0);
    }

    ASSERT_NOT_REACHED();
    return LayoutUnit(0);
}

void RenderMathMLFraction::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    // FIXME: We should be able to remove this.
    updateFromElement();

    if (!relayoutChildren && simplifiedLayout())
        return;

    if (!isValid()) {
        setLogicalWidth(0);
        setLogicalHeight(0);
        clearNeedsLayout();
        return;
    }

    LayoutUnit verticalOffset = 0;

    numerator().layoutIfNeeded();
    denominator().layoutIfNeeded();

    setLogicalWidth(std::max<LayoutUnit>(numerator().logicalWidth(), denominator().logicalWidth()));

    LayoutPoint numeratorLocation(horizontalOffset(numerator(), m_numeratorAlign), verticalOffset);
    numerator().setLocation(numeratorLocation);

    verticalOffset += numerator().logicalHeight() + gNumeratorGap * style().fontSize() + m_lineThickness + gDenominatorGap * style().fontSize();

    LayoutPoint denominatorLocation(horizontalOffset(denominator(), m_denominatorAlign), verticalOffset);
    denominator().setLocation(denominatorLocation);

    setLogicalHeight(verticalOffset + denominator().logicalHeight());

    clearNeedsLayout();
}

void RenderMathMLFraction::paint(PaintInfo& info, const LayoutPoint& paintOffset)
{
    RenderMathMLBlock::paint(info, paintOffset);
    if (info.context().paintingDisabled() || info.phase != PaintPhaseForeground || style().visibility() != VISIBLE || !isValid())
        return;

    IntPoint adjustedPaintOffset = roundedIntPoint(paintOffset + location() + LayoutPoint(0, numerator().logicalHeight() + gNumeratorGap * style().fontSize() + m_lineThickness / 2));
    
    GraphicsContextStateSaver stateSaver(info.context());
    
    info.context().setStrokeThickness(m_lineThickness);
    info.context().setStrokeStyle(SolidStroke);
    info.context().setStrokeColor(style().visitedDependentColor(CSSPropertyColor));
    info.context().drawLine(adjustedPaintOffset, roundedIntPoint(LayoutPoint(adjustedPaintOffset.x() + logicalWidth(), adjustedPaintOffset.y())));
}

Optional<int> RenderMathMLFraction::firstLineBaseline() const
{
    if (isValid()) {
        LayoutUnit axisHeight = mathAxisHeight();
        return Optional<int>(numerator().logicalHeight() + gNumeratorGap * style().fontSize() + static_cast<int>(lroundf(m_lineThickness / 2 + axisHeight)));
    }
    return RenderMathMLBlock::firstLineBaseline();
}

void RenderMathMLFraction::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForChild, bool usePrintRect)
{
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (!paintChild(*child, paintInfo, paintOffset, paintInfoForChild, usePrintRect, PaintAsInlineBlock))
            return;
    }
}

}

#endif // ENABLE(MATHML)
