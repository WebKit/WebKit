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
#include "RenderMathMLBlockInlines.h"
#include <cmath>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderMathMLFraction);

RenderMathMLFraction::RenderMathMLFraction(MathMLFractionElement& element, RenderStyle&& style)
    : RenderMathMLRow(Type::MathMLFraction, element, WTFMove(style))
{
    ASSERT(isRenderMathMLFraction());
}

RenderMathMLFraction::~RenderMathMLFraction() = default;

bool RenderMathMLFraction::isValid() const
{
    // Verify whether the list of children is valid:
    // <mfrac> numerator denominator </mfrac>
    auto* child = firstInFlowChildBox();
    if (!child)
        return false;
    child = child->nextInFlowSiblingBox();
    return child && !child->nextInFlowSiblingBox();
}

RenderBox& RenderMathMLFraction::numerator() const
{
    ASSERT(isValid());
    return *firstInFlowChildBox();
}

RenderBox& RenderMathMLFraction::denominator() const
{
    ASSERT(isValid());
    return *firstInFlowChildBox()->nextInFlowSiblingBox();
}

LayoutUnit RenderMathMLFraction::defaultLineThickness() const
{
    const Ref primaryFont = style().fontCascade().primaryFont();
    if (RefPtr mathData = primaryFont->mathData())
        return LayoutUnit(mathData->getMathConstant(primaryFont, OpenTypeMathData::FractionRuleThickness));
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
    // See https://mathml-refresh.github.io/mathml-core/#fraction-with-nonzero-line-thickness.

    ASSERT(lineThickness());
    FractionParameters parameters;
    LayoutUnit numeratorGapMin, denominatorGapMin, numeratorMinShiftUp, denominatorMinShiftDown;

    // We try and read constants to draw the fraction from the OpenType MATH and use fallback values otherwise.
    bool display = style().mathStyle() == MathStyle::Normal;
    const Ref primaryFont = style().fontCascade().primaryFont();
    if (RefPtr mathData = primaryFont->mathData()) {
        numeratorGapMin = mathData->getMathConstant(primaryFont, display ? OpenTypeMathData::FractionNumDisplayStyleGapMin : OpenTypeMathData::FractionNumeratorGapMin);
        denominatorGapMin = mathData->getMathConstant(primaryFont, display ? OpenTypeMathData::FractionDenomDisplayStyleGapMin : OpenTypeMathData::FractionDenominatorGapMin);
        numeratorMinShiftUp = mathData->getMathConstant(primaryFont, display ? OpenTypeMathData::FractionNumeratorDisplayStyleShiftUp : OpenTypeMathData::FractionNumeratorShiftUp);
        denominatorMinShiftDown = mathData->getMathConstant(primaryFont, display ? OpenTypeMathData::FractionDenominatorDisplayStyleShiftDown : OpenTypeMathData::FractionDenominatorShiftDown);
    } else {
        // The MATH table specification suggests default rule thickness or (in displaystyle) 3 times default rule thickness for the gaps.
        numeratorGapMin = display ? 3 * ruleThicknessFallback() : ruleThicknessFallback();
        denominatorGapMin = numeratorGapMin;

        // The MATH table specification does not suggest any values for shifts, so we leave them at zero.
        numeratorMinShiftUp = 0;
        denominatorMinShiftDown = 0;
    }

    // Adjust fraction shifts to satisfy min gaps.
    LayoutUnit numeratorAscent = ascentForChild(numerator()) + numerator().marginBefore();
    LayoutUnit numeratorDescent = numerator().logicalHeight() + numerator().marginLogicalHeight() - numeratorAscent;
    LayoutUnit denominatorAscent = ascentForChild(denominator()) + denominator().marginBefore();
    LayoutUnit thickness = lineThickness();
    parameters.numeratorShiftUp = std::max(numeratorMinShiftUp, mathAxisHeight() + thickness / 2 + numeratorGapMin + numeratorDescent);
    parameters.denominatorShiftDown = std::max(denominatorMinShiftDown, thickness / 2 + denominatorGapMin + denominatorAscent - mathAxisHeight());

    return parameters;
}

RenderMathMLFraction::FractionParameters RenderMathMLFraction::stackParameters() const
{
    // See https://mathml-refresh.github.io/mathml-core/#fraction-with-zero-line-thickness.

    ASSERT(!lineThickness());
    ASSERT(isValid());
    FractionParameters parameters;
    LayoutUnit gapMin;
    
    // We try and read constants to draw the stack from the OpenType MATH and use fallback values otherwise.
    bool display = style().mathStyle() == MathStyle::Normal;
    const Ref primaryFont = style().fontCascade().primaryFont();
    if (RefPtr mathData = primaryFont->mathData()) {
        gapMin = mathData->getMathConstant(primaryFont, display ? OpenTypeMathData::StackDisplayStyleGapMin : OpenTypeMathData::StackGapMin);
        parameters.numeratorShiftUp = mathData->getMathConstant(primaryFont, display ? OpenTypeMathData::StackTopDisplayStyleShiftUp : OpenTypeMathData::StackTopShiftUp);
        parameters.denominatorShiftDown = mathData->getMathConstant(primaryFont, display ? OpenTypeMathData::StackBottomDisplayStyleShiftDown : OpenTypeMathData::StackBottomShiftDown);
    } else {
        // We use the values suggested in the MATH table specification.
        gapMin = display ? 7 * ruleThicknessFallback() : 3 * ruleThicknessFallback();

        // The MATH table specification does not suggest any values for shifts, so we leave them at zero.
        parameters.numeratorShiftUp = 0;
        parameters.denominatorShiftDown = 0;
    }

    // Adjust fraction shifts to satisfy min gaps.
    LayoutUnit numeratorAscent = ascentForChild(numerator()) + numerator().marginBefore();
    LayoutUnit numeratorDescent = numerator().logicalHeight() + numerator().marginLogicalHeight() - numeratorAscent;
    LayoutUnit denominatorAscent = ascentForChild(denominator()) + denominator().marginBefore();
    LayoutUnit gap = parameters.numeratorShiftUp - numeratorDescent + parameters.denominatorShiftDown - denominatorAscent;
    if (gap < gapMin) {
        LayoutUnit delta = (gapMin - gap) / 2;
        parameters.numeratorShiftUp += delta;
        parameters.denominatorShiftDown += delta;
    }

    return parameters;
}

RenderMathMLOperator* RenderMathMLFraction::unembellishedOperator() const
{
    if (!isValid())
        return RenderMathMLRow::unembellishedOperator();

    auto* mathMLBlock = dynamicDowncast<RenderMathMLBlock>(numerator());
    return mathMLBlock ? mathMLBlock->unembellishedOperator() : nullptr;
}

void RenderMathMLFraction::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    if (!isValid()) {
        RenderMathMLRow::computePreferredLogicalWidths();
        return;
    }

    LayoutUnit numeratorWidth = numerator().maxPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(numerator());
    LayoutUnit denominatorWidth = denominator().maxPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(denominator());
    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = std::max(numeratorWidth, denominatorWidth);

    auto sizes = sizeAppliedToMathContent(LayoutPhase::CalculatePreferredLogicalWidth);
    applySizeToMathContent(LayoutPhase::CalculatePreferredLogicalWidth, sizes);

    adjustPreferredLogicalWidthsForBorderAndPadding();

    setPreferredLogicalWidthsDirty(false);
}

LayoutUnit RenderMathMLFraction::horizontalOffset(RenderBox& child, MathMLFractionElement::FractionAlignment align) const
{
    LayoutUnit contentBoxInlineSize = logicalWidth();
    LayoutUnit childMarginBoxInlineSize = child.marginStart() + child.logicalWidth() + child.marginEnd();
    switch (align) {
    case MathMLFractionElement::FractionAlignmentRight:
        return LayoutUnit(contentBoxInlineSize - childMarginBoxInlineSize);
    case MathMLFractionElement::FractionAlignmentCenter:
        return LayoutUnit((contentBoxInlineSize - childMarginBoxInlineSize) / 2);
    case MathMLFractionElement::FractionAlignmentLeft:
        return 0_lu;
    }

    ASSERT_NOT_REACHED();
    return 0_lu;
}

LayoutUnit RenderMathMLFraction::fractionAscent() const
{
    ASSERT(isValid());

    LayoutUnit numeratorAscent = ascentForChild(numerator()) + numerator().marginBefore();
    if (LayoutUnit thickness = lineThickness())
        return std::max(mathAxisHeight() + thickness / 2, numeratorAscent + fractionParameters().numeratorShiftUp);

    return numeratorAscent + stackParameters().numeratorShiftUp;
}

void RenderMathMLFraction::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    insertPositionedChildrenIntoContainingBlock();

    if (!relayoutChildren && simplifiedLayout())
        return;

    if (!isValid()) {
        RenderMathMLRow::layoutBlock(relayoutChildren);
        return;
    }

    layoutFloatingChildren();

    recomputeLogicalWidth();

    numerator().layoutIfNeeded();
    denominator().layoutIfNeeded();
    computeAndSetBlockDirectionMarginsOfChildren();

    LayoutUnit numeratorMarginBoxInlineSize = numerator().marginStart() + numerator().logicalWidth() + numerator().marginEnd();
    LayoutUnit denominatorMarginBoxInlineSize = denominator().marginStart() + denominator().logicalWidth() + denominator().marginEnd();
    setLogicalWidth(std::max(numeratorMarginBoxInlineSize, denominatorMarginBoxInlineSize));

    LayoutUnit verticalOffset = 0; // This is the top of the renderer.
    verticalOffset += numerator().marginBefore();
    LayoutPoint numeratorLocation(numerator().marginLeft() + horizontalOffset(numerator(), element().numeratorAlignment()), verticalOffset);
    numerator().setLocation(numeratorLocation);

    LayoutUnit denominatorAscent = ascentForChild(denominator()) + denominator().marginBefore();
    verticalOffset = fractionAscent();
    FractionParameters parameters = lineThickness() ? fractionParameters() : stackParameters();
    verticalOffset += parameters.denominatorShiftDown - denominatorAscent;

    verticalOffset += denominator().marginBefore();
    LayoutPoint denominatorLocation(denominator().marginLeft() + horizontalOffset(denominator(), element().denominatorAlignment()), verticalOffset);
    denominator().setLocation(denominatorLocation);

    verticalOffset += denominator().logicalHeight() + denominator().marginAfter(); // This is the bottom of our renderer.

    setLogicalHeight(verticalOffset);

    auto sizes = sizeAppliedToMathContent(LayoutPhase::Layout);
    auto shift = applySizeToMathContent(LayoutPhase::Layout, sizes);
    shiftInFlowChildren(shift, 0);

    adjustLayoutForBorderAndPadding();

    layoutPositionedObjects(relayoutChildren);

    updateScrollInfoAfterLayout();

    clearNeedsLayout();
}

void RenderMathMLFraction::paint(PaintInfo& info, const LayoutPoint& paintOffset)
{
    RenderMathMLRow::paint(info, paintOffset);
    LayoutUnit thickness = lineThickness();
    if (info.context().paintingDisabled() || info.phase != PaintPhase::Foreground || style().usedVisibility() != Visibility::Visible || !isValid() || !thickness)
        return;

    LayoutUnit borderAndPaddingLeft = writingMode().isBidiLTR() ? borderAndPaddingStart() : borderAndPaddingEnd();
    IntPoint adjustedPaintOffset = roundedIntPoint(paintOffset + location() + LayoutPoint(borderAndPaddingLeft, borderAndPaddingBefore() + fractionAscent() - mathAxisHeight()));

    GraphicsContextStateSaver stateSaver(info.context());

    info.context().setStrokeThickness(thickness);
    info.context().setStrokeStyle(StrokeStyle::SolidStroke);
    info.context().setStrokeColor(style().visitedDependentColorWithColorFilter(CSSPropertyColor));
    // MathML Core says the fraction bar takes the full width of the content box.
    info.context().drawLine(adjustedPaintOffset, roundedIntPoint(LayoutPoint(adjustedPaintOffset.x() + logicalWidth() - borderAndPaddingLogicalWidth(), LayoutUnit(adjustedPaintOffset.y()))));
}

std::optional<LayoutUnit> RenderMathMLFraction::firstLineBaseline() const
{
    if (isValid())
        return LayoutUnit { roundf(static_cast<float>(borderAndPaddingBefore() + fractionAscent())) };
    return RenderMathMLRow::firstLineBaseline();
}

}

#endif // ENABLE(MATHML)
