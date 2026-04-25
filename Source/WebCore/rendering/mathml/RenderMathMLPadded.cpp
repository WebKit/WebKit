/*
 * Copyright (C) 2016 Igalia S.L. All rights reserved.
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "RenderMathMLPadded.h"

#if ENABLE(MATHML)

#include "RenderMathMLBlockInlines.h"
#include <cmath>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderMathMLPadded);

RenderMathMLPadded::RenderMathMLPadded(MathMLPaddedElement& element, RenderStyle&& style)
    : RenderMathMLRow(Type::MathMLPadded, element, WTF::move(style))
{
    ASSERT(isRenderMathMLPadded());
}

RenderMathMLPadded::~RenderMathMLPadded() = default;

MathMLPaddedElement& RenderMathMLPadded::element() const
{
    return static_cast<MathMLPaddedElement&>(nodeForNonAnonymous());
}

LayoutUnit RenderMathMLPadded::voffset() const
{
    return toUserUnits(element().voffset(), style(), 0);
}

LayoutUnit RenderMathMLPadded::lspace() const
{
    // FIXME: Negative lspace values are not supported yet (https://bugs.webkit.org/show_bug.cgi?id=85730).
    return std::max(0_lu, toUserUnits(element().lspace(), style(), 0));
}

LayoutUnit RenderMathMLPadded::mpaddedWidth(LayoutUnit contentWidth) const
{
    auto& widthAttr = element().width();
    // If parsing failed (attribute not set) or it's a percentage, use the content width as default.
    if (widthAttr.type == MathMLElement::LengthType::ParsingFailed ||  widthAttr.type == MathMLElement::LengthType::Percentage)
        return contentWidth;
    return std::max(0_lu, toUserUnits(widthAttr, style(), 0));
}

LayoutUnit RenderMathMLPadded::mpaddedHeight(LayoutUnit contentHeight) const
{
    auto& heightAttr = element().height();
    // If parsing failed (attribute not set) or it's a percentage, use the content height as default.
    if (heightAttr.type == MathMLElement::LengthType::ParsingFailed ||  heightAttr.type == MathMLElement::LengthType::Percentage)
        return contentHeight;
    return std::max(0_lu, toUserUnits(heightAttr, style(), 0));
}

LayoutUnit RenderMathMLPadded::mpaddedDepth(LayoutUnit contentDepth) const
{
    auto& depthAttr = element().depth();
    // If parsing failed (attribute not set) or it's a percentage, use the content depth as default.
    if (depthAttr.type == MathMLElement::LengthType::ParsingFailed ||  depthAttr.type == MathMLElement::LengthType::Percentage)
        return contentDepth;
    return std::max(0_lu, toUserUnits(depthAttr, style(), 0));
}

void RenderMathMLPadded::computePreferredLogicalWidths()
{
    ASSERT(needsPreferredLogicalWidthsUpdate());

    // Only the width attribute should modify the width.
    // We parse it using the preferred width of the content as its default value.
    LayoutUnit preferredWidth = preferredLogicalWidthOfRowItems();
    preferredWidth = mpaddedWidth(preferredWidth);
    m_maxPreferredLogicalWidth = m_minPreferredLogicalWidth = preferredWidth;

    auto sizes = sizeAppliedToMathContent(LayoutPhase::CalculatePreferredLogicalWidth);
    applySizeToMathContent(LayoutPhase::CalculatePreferredLogicalWidth, sizes);

    adjustPreferredLogicalWidthsForBorderAndPadding();

    clearNeedsPreferredWidthsUpdate();
}

void RenderMathMLPadded::layoutBlock(RelayoutChildren relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    insertPositionedChildrenIntoContainingBlock();

    if (relayoutChildren == RelayoutChildren::No && simplifiedLayout())
        return;

    layoutFloatingChildren();

    recomputeLogicalWidth();
    computeAndSetBlockDirectionMarginsOfChildren();

    // We first layout our children as a normal <mrow> element.
    LayoutUnit contentWidth, contentAscent, contentDescent;
    stretchVerticalOperatorsAndLayoutChildren();
    getContentBoundingBox(contentWidth, contentAscent, contentDescent);
    layoutRowItems(contentWidth, contentAscent);

    // We parse the mpadded attributes using the content metrics as the default value.
    LayoutUnit width = mpaddedWidth(contentWidth);
    LayoutUnit ascent = mpaddedHeight(contentAscent);
    LayoutUnit descent = mpaddedDepth(contentDescent);

    auto inlineShift = style().writingMode().inlineDirection() == FlowDirection::RightToLeft ? (width - contentWidth - lspace()) : lspace();

    // Align children on the new baseline and shift them by (lspace, -voffset)
    shiftInFlowChildren(inlineShift, ascent - contentAscent - voffset());

    // Set the final metrics.
    setLogicalWidth(width);
    setLogicalHeight(ascent + descent);

    auto sizes = sizeAppliedToMathContent(LayoutPhase::Layout);
    auto shift = applySizeToMathContent(LayoutPhase::Layout, sizes);
    shiftInFlowChildren(shift, 0);

    adjustLayoutForBorderAndPadding();

    layoutOutOfFlowBoxes(relayoutChildren);
}

std::optional<LayoutUnit> RenderMathMLPadded::firstLineBaseline() const
{
    // We try and calculate the baseline from the position of the first child.
    LayoutUnit ascent;
    if (CheckedPtr baselineChild = firstInFlowChildBox())
        ascent = ascentForChild(*baselineChild) + baselineChild->logicalTop() + voffset();
    else
        ascent = mpaddedHeight(0);
    return ascent;
}

}

#endif
