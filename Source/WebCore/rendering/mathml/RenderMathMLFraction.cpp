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
#include "RenderMathMLFraction.h"

#if ENABLE(MATHML)

#include "GraphicsContext.h"
#include "MathMLFractionElement.h"
#include "PaintInfo.h"
#include <cmath>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderMathMLFraction);

RenderMathMLFraction::RenderMathMLFraction(MathMLFractionElement& element, RenderStyle&& style)
    : RenderMathMLBlock(element, WTFMove(style))
{
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

LayoutUnit RenderMathMLFraction::defaultLineThickness() const
{
    const auto& primaryFont = style().fontCascade().primaryFont();
    if (const auto* mathData = primaryFont.mathData())
        return mathData->getMathConstant(primaryFont, OpenTypeMathData::FractionRuleThickness);
    return ruleThicknessFallback();
}

LayoutUnit RenderMathMLFraction::lineThickness() const
{
    return std::max<LayoutUnit>(toUserUnits(element().lineThickness(), style(), defaultLineThickness()), 0);
}

float RenderMathMLFraction::relativeLineThickness() const
{
    if (LayoutUnit defaultThickness = defaultLineThickness())
        return lineThickness() / defaultThickness;
    return 0;
}

RenderMathMLFraction::FractionParameters RenderMathMLFraction::fractionParameters() const
{
    ASSERT(lineThickness());
    FractionParameters parameters;

    // We try and read constants to draw the fraction from the OpenType MATH and use fallback values otherwise.
    const auto& primaryFont = style().fontCascade().primaryFont();
    const auto* mathData = style().fontCascade().primaryFont().mathData();
    bool display = mathMLStyle().displayStyle();
    if (mathData) {
        parameters.numeratorGapMin = mathData->getMathConstant(primaryFont, display ? OpenTypeMathData::FractionNumDisplayStyleGapMin : OpenTypeMathData::FractionNumeratorGapMin);
        parameters.denominatorGapMin = mathData->getMathConstant(primaryFont, display ? OpenTypeMathData::FractionDenomDisplayStyleGapMin : OpenTypeMathData::FractionDenominatorGapMin);
        parameters.numeratorMinShiftUp = mathData->getMathConstant(primaryFont, display ? OpenTypeMathData::FractionNumeratorDisplayStyleShiftUp : OpenTypeMathData::FractionNumeratorShiftUp);
        parameters.denominatorMinShiftDown = mathData->getMathConstant(primaryFont, display ? OpenTypeMathData::FractionDenominatorDisplayStyleShiftDown : OpenTypeMathData::FractionDenominatorShiftDown);
    } else {
        // The MATH table specification suggests default rule thickness or (in displaystyle) 3 times default rule thickness for the gaps.
        parameters.numeratorGapMin = display ? 3 * ruleThicknessFallback() : ruleThicknessFallback();
        parameters.denominatorGapMin = parameters.numeratorGapMin;

        // The MATH table specification does not suggest any values for shifts, so we leave them at zero.
        parameters.numeratorMinShiftUp = 0;
        parameters.denominatorMinShiftDown = 0;
    }

    return parameters;
}

RenderMathMLFraction::StackParameters RenderMathMLFraction::stackParameters() const
{
    ASSERT(!lineThickness());
    ASSERT(isValid());
    StackParameters parameters;
    
    // We try and read constants to draw the stack from the OpenType MATH and use fallback values otherwise.
    const auto& primaryFont = style().fontCascade().primaryFont();
    const auto* mathData = style().fontCascade().primaryFont().mathData();
    bool display = mathMLStyle().displayStyle();
    if (mathData) {
        parameters.gapMin = mathData->getMathConstant(primaryFont, display ? OpenTypeMathData::StackDisplayStyleGapMin : OpenTypeMathData::StackGapMin);
        parameters.topShiftUp = mathData->getMathConstant(primaryFont, display ? OpenTypeMathData::StackTopDisplayStyleShiftUp : OpenTypeMathData::StackTopShiftUp);
        parameters.bottomShiftDown = mathData->getMathConstant(primaryFont, display ? OpenTypeMathData::StackBottomDisplayStyleShiftDown : OpenTypeMathData::StackBottomShiftDown);
    } else {
        // We use the values suggested in the MATH table specification.
        parameters.gapMin = display ? 7 * ruleThicknessFallback() : 3 * ruleThicknessFallback();

        // The MATH table specification does not suggest any values for shifts, so we leave them at zero.
        parameters.topShiftUp = 0;
        parameters.bottomShiftDown = 0;
    }

    LayoutUnit numeratorAscent = ascentForChild(numerator());
    LayoutUnit numeratorDescent = numerator().logicalHeight() - numeratorAscent;
    LayoutUnit denominatorAscent = ascentForChild(denominator());
    LayoutUnit gap = parameters.topShiftUp - numeratorDescent + parameters.bottomShiftDown - denominatorAscent;
    if (gap < parameters.gapMin) {
        // If the gap is not large enough, we increase the shifts by the same value.
        LayoutUnit delta = (parameters.gapMin - gap) / 2;
        parameters.topShiftUp += delta;
        parameters.bottomShiftDown += delta;
    }

    return parameters;
}

RenderMathMLOperator* RenderMathMLFraction::unembellishedOperator() const
{
    if (!isValid() || !is<RenderMathMLBlock>(numerator()))
        return nullptr;

    return downcast<RenderMathMLBlock>(numerator()).unembellishedOperator();
}

void RenderMathMLFraction::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    if (isValid()) {
        LayoutUnit numeratorWidth = numerator().maxPreferredLogicalWidth();
        LayoutUnit denominatorWidth = denominator().maxPreferredLogicalWidth();
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = std::max(numeratorWidth, denominatorWidth);
    }

    setPreferredLogicalWidthsDirty(false);
}

LayoutUnit RenderMathMLFraction::horizontalOffset(RenderBox& child, MathMLFractionElement::FractionAlignment align) const
{
    switch (align) {
    case MathMLFractionElement::FractionAlignmentRight:
        return LayoutUnit(logicalWidth() - child.logicalWidth());
    case MathMLFractionElement::FractionAlignmentCenter:
        return LayoutUnit((logicalWidth() - child.logicalWidth()) / 2);
    case MathMLFractionElement::FractionAlignmentLeft:
        return LayoutUnit(0);
    }

    ASSERT_NOT_REACHED();
    return LayoutUnit(0);
}

LayoutUnit RenderMathMLFraction::ascentOverHorizontalAxis() const
{
    ASSERT(isValid());

    LayoutUnit numeratorAscent = ascentForChild(numerator());
    if (LayoutUnit thickness = lineThickness()) {
        // For normal fraction layout, the axis is the middle of the fraction bar.
        FractionParameters parameters = fractionParameters();
        return std::max(numerator().logicalHeight() + parameters.numeratorGapMin + thickness / 2, numeratorAscent + parameters.numeratorMinShiftUp);
    }

    // For stack layout, the axis is the middle of the gap between numerator and denonimator.
    StackParameters parameters = stackParameters();
    return numeratorAscent + parameters.topShiftUp;
}

void RenderMathMLFraction::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    if (!isValid()) {
        layoutInvalidMarkup(relayoutChildren);
        return;
    }

    numerator().layoutIfNeeded();
    denominator().layoutIfNeeded();

    setLogicalWidth(std::max(numerator().logicalWidth(), denominator().logicalWidth()));

    LayoutUnit verticalOffset; // This is the top of the renderer.
    LayoutPoint numeratorLocation(horizontalOffset(numerator(), element().numeratorAlignment()), verticalOffset);
    numerator().setLocation(numeratorLocation);

    LayoutUnit denominatorAscent = ascentForChild(denominator());
    LayoutUnit denominatorDescent = denominator().logicalHeight() - denominatorAscent;
    LayoutUnit ascent = ascentOverHorizontalAxis();
    verticalOffset += ascent;
    if (LayoutUnit thickness = lineThickness()) {
        FractionParameters parameters = fractionParameters();
        verticalOffset += std::max(thickness / 2 + parameters.denominatorGapMin, parameters.denominatorMinShiftDown - denominatorAscent);
    } else {
        StackParameters parameters = stackParameters();
        verticalOffset += parameters.bottomShiftDown - denominatorAscent;
    }

    LayoutPoint denominatorLocation(horizontalOffset(denominator(), element().denominatorAlignment()), verticalOffset);
    denominator().setLocation(denominatorLocation);

    verticalOffset = std::max(verticalOffset + denominator().logicalHeight(), ascent + mathAxisHeight() + denominatorDescent); // This is the bottom of our renderer.
    setLogicalHeight(verticalOffset);

    layoutPositionedObjects(relayoutChildren);

    clearNeedsLayout();
}

void RenderMathMLFraction::paint(PaintInfo& info, const LayoutPoint& paintOffset)
{
    RenderMathMLBlock::paint(info, paintOffset);
    LayoutUnit thickness = lineThickness();
    if (info.context().paintingDisabled() || info.phase != PaintPhase::Foreground || style().visibility() != Visibility::Visible || !isValid() || !thickness)
        return;

    IntPoint adjustedPaintOffset = roundedIntPoint(paintOffset + location() + LayoutPoint(0, ascentOverHorizontalAxis()));

    GraphicsContextStateSaver stateSaver(info.context());

    info.context().setStrokeThickness(thickness);
    info.context().setStrokeStyle(SolidStroke);
    info.context().setStrokeColor(style().visitedDependentColorWithColorFilter(CSSPropertyColor));
    info.context().drawLine(adjustedPaintOffset, roundedIntPoint(LayoutPoint(adjustedPaintOffset.x() + logicalWidth(), adjustedPaintOffset.y())));
}

std::optional<int> RenderMathMLFraction::firstLineBaseline() const
{
    if (isValid())
        return std::optional<int>(std::lround(static_cast<float>(ascentOverHorizontalAxis() + mathAxisHeight())));
    return RenderMathMLBlock::firstLineBaseline();
}

}

#endif // ENABLE(MATHML)
