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
#include <cmath>

namespace WebCore {

using namespace MathMLNames;

RenderMathMLFraction::RenderMathMLFraction(MathMLInlineContainerElement& element, RenderStyle&& style)
    : RenderMathMLBlock(element, WTFMove(style))
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
    auto* child = firstChildBox();
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

    // We try and read constants to draw the fraction from the OpenType MATH and use fallback values otherwise.
    // We also parse presentation attributes of the <mfrac> element.

    // We first determine the default line thickness.
    const auto& primaryFont = style().fontCascade().primaryFont();
    const auto* mathData = style().fontCascade().primaryFont().mathData();
    if (mathData)
        m_defaultLineThickness = mathData->getMathConstant(primaryFont, OpenTypeMathData::FractionRuleThickness);
    else
        m_defaultLineThickness = ruleThicknessFallback();

    // Next, we parse the linethickness attribute.
    // The MathML3 recommendation states that "medium" is the default thickness.
    // However, it only states that "thin" and "thick" are respectively thiner and thicker.
    // The MathML in HTML5 implementation note suggests 50% and 200% and these values are also used in Gecko.
    String thickness = element().getAttribute(MathMLNames::linethicknessAttr);
    if (equalLettersIgnoringASCIICase(thickness, "thin"))
        m_lineThickness = m_defaultLineThickness / 2;
    else if (equalLettersIgnoringASCIICase(thickness, "medium"))
        m_lineThickness = m_defaultLineThickness;
    else if (equalLettersIgnoringASCIICase(thickness, "thick"))
        m_lineThickness = m_defaultLineThickness * 2;
    else {
        // Parse the thickness using m_defaultLineThickness as the default value.
        m_lineThickness = m_defaultLineThickness;
        parseMathMLLength(thickness, m_lineThickness, &style(), false);
    }

    // We now know whether we should layout as a normal fraction or as a stack (fraction without bar) and so determine the relevant constants.
    // FIXME: If m_lineThickness == 0, we should read Stack* parameters. See http://wkb.ug/122297
    bool display = mathMLStyle()->displayStyle();
    if (mathData) {
        m_numeratorGapMin = mathData->getMathConstant(primaryFont, display ? OpenTypeMathData::FractionNumDisplayStyleGapMin : OpenTypeMathData::FractionNumeratorGapMin);
        m_denominatorGapMin = mathData->getMathConstant(primaryFont, display ? OpenTypeMathData::FractionDenomDisplayStyleGapMin : OpenTypeMathData::FractionDenominatorGapMin);
        m_numeratorMinShiftUp = mathData->getMathConstant(primaryFont, display ? OpenTypeMathData::FractionNumeratorDisplayStyleShiftUp : OpenTypeMathData::FractionNumeratorShiftUp);
        m_denominatorMinShiftDown = mathData->getMathConstant(primaryFont, display ? OpenTypeMathData::FractionDenominatorDisplayStyleShiftDown : OpenTypeMathData::FractionDenominatorShiftDown);
    } else {
        // The MATH table specification suggests default rule thickness or (in displaystyle) 3 times default rule thickness for the gaps.
        m_numeratorGapMin = m_denominatorGapMin = display ? 3 * ruleThicknessFallback() : ruleThicknessFallback();

        // The MATH table specification does not suggest any values for shifts, so we leave them at zero.
        m_numeratorMinShiftUp = m_denominatorMinShiftDown = 0;
    }

    // Parse alignment attributes.
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

    numerator().layoutIfNeeded();
    denominator().layoutIfNeeded();

    setLogicalWidth(std::max(numerator().logicalWidth(), denominator().logicalWidth()));

    LayoutUnit verticalOffset = 0; // This is the top of the renderer.
    LayoutPoint numeratorLocation(horizontalOffset(numerator(), m_numeratorAlign), verticalOffset);
    numerator().setLocation(numeratorLocation);

    LayoutUnit numeratorAscent = ascentForChild(numerator());
    verticalOffset += std::max(numerator().logicalHeight() + m_numeratorGapMin + m_lineThickness / 2, numeratorAscent + m_numeratorMinShiftUp); // This is the middle of the fraction bar.
    m_ascent = verticalOffset + mathAxisHeight();

    LayoutUnit denominatorAscent = ascentForChild(denominator());
    LayoutUnit denominatorDescent = denominator().logicalHeight() - denominatorAscent;
    verticalOffset += std::max(m_lineThickness / 2 + m_denominatorGapMin, m_denominatorMinShiftDown - denominatorAscent);
    LayoutPoint denominatorLocation(horizontalOffset(denominator(), m_denominatorAlign), verticalOffset);
    denominator().setLocation(denominatorLocation);

    verticalOffset = std::max(verticalOffset + denominator().logicalHeight(), m_ascent + denominatorDescent); // This is the bottom of our renderer.
    setLogicalHeight(verticalOffset);

    clearNeedsLayout();
}

void RenderMathMLFraction::paint(PaintInfo& info, const LayoutPoint& paintOffset)
{
    RenderMathMLBlock::paint(info, paintOffset);
    if (info.context().paintingDisabled() || info.phase != PaintPhaseForeground || style().visibility() != VISIBLE || !isValid())
        return;

    IntPoint adjustedPaintOffset = roundedIntPoint(paintOffset + location() + LayoutPoint(0, m_ascent - mathAxisHeight()));

    GraphicsContextStateSaver stateSaver(info.context());

    info.context().setStrokeThickness(m_lineThickness);
    info.context().setStrokeStyle(SolidStroke);
    info.context().setStrokeColor(style().visitedDependentColor(CSSPropertyColor));
    info.context().drawLine(adjustedPaintOffset, roundedIntPoint(LayoutPoint(adjustedPaintOffset.x() + logicalWidth(), adjustedPaintOffset.y())));
}

Optional<int> RenderMathMLFraction::firstLineBaseline() const
{
    if (isValid())
        return Optional<int>(std::lround(static_cast<float>(m_ascent)));
    return RenderMathMLBlock::firstLineBaseline();
}

}

#endif // ENABLE(MATHML)
