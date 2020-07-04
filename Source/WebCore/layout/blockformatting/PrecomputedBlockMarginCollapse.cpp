/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "BlockFormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "BlockFormattingState.h"
#include "LayoutBox.h"
#include "LayoutContainerBox.h"
#include "LayoutState.h"
#include "LayoutUnit.h"
#include "RenderStyle.h"

namespace WebCore {
namespace Layout {

UsedVerticalMargin::PositiveAndNegativePair::Values BlockFormattingContext::MarginCollapse::precomputedPositiveNegativeValues(const Box& layoutBox) const
{
    auto& blockFormattingState = downcast<BlockFormattingState>(layoutState().formattingStateForBox(layoutBox));
    if (blockFormattingState.hasUsedVerticalMargin(layoutBox))
        return blockFormattingState.usedVerticalMargin(layoutBox).positiveAndNegativeValues.before;

    auto geometry = formattingContext().geometry();
    auto horizontalConstraints = geometry.constraintsForInFlowContent(layoutBox.containingBlock()).horizontal;
    auto computedVerticalMargin = geometry.computedVerticalMargin(layoutBox, horizontalConstraints);
    auto nonCollapsedMargin = UsedVerticalMargin::NonCollapsedValues { computedVerticalMargin.before.valueOr(0), computedVerticalMargin.after.valueOr(0) };
    return precomputedPositiveNegativeMarginBefore(layoutBox, nonCollapsedMargin);
}

UsedVerticalMargin::PositiveAndNegativePair::Values BlockFormattingContext::MarginCollapse::precomputedPositiveNegativeMarginBefore(const Box& layoutBox, UsedVerticalMargin::NonCollapsedValues nonCollapsedValues) const
{
    auto firstChildCollapsedMarginBefore = [&]() -> UsedVerticalMargin::PositiveAndNegativePair::Values {
        if (!marginBeforeCollapsesWithFirstInFlowChildMarginBefore(layoutBox))
            return { };
        return precomputedPositiveNegativeValues(*downcast<ContainerBox>(layoutBox).firstInFlowChild());
    };

    auto previouSiblingCollapsedMarginAfter = [&]() -> UsedVerticalMargin::PositiveAndNegativePair::Values {
        if (!marginBeforeCollapsesWithPreviousSiblingMarginAfter(layoutBox))
            return { };
        auto& previousInFlowSibling = *layoutBox.previousInFlowSibling();
        auto& blockFormattingState = downcast<BlockFormattingState>(layoutState().formattingStateForBox(previousInFlowSibling));
        return blockFormattingState.usedVerticalMargin(previousInFlowSibling).positiveAndNegativeValues.after;
    };

    // 1. Gather positive and negative margin values from first child if margins are adjoining.
    // 2. Gather positive and negative margin values from previous inflow sibling if margins are adjoining.
    // 3. Compute min/max positive and negative collapsed margin values using non-collpased computed margin before.
    auto collapsedMarginBefore = computedPositiveAndNegativeMargin(firstChildCollapsedMarginBefore(), previouSiblingCollapsedMarginAfter());
    if (collapsedMarginBefore.isQuirk && formattingContext().quirks().shouldIgnoreCollapsedQuirkMargin(layoutBox))
        collapsedMarginBefore = { };

    auto nonCollapsedBefore = UsedVerticalMargin::PositiveAndNegativePair::Values { };
    if (nonCollapsedValues.before > 0)
        nonCollapsedBefore = { nonCollapsedValues.before, { }, layoutBox.style().hasMarginBeforeQuirk() };
    else if (nonCollapsedValues.before < 0)
        nonCollapsedBefore = { { }, nonCollapsedValues.before, layoutBox.style().hasMarginBeforeQuirk() };

    return computedPositiveAndNegativeMargin(collapsedMarginBefore, nonCollapsedBefore);
}

PrecomputedMarginBefore BlockFormattingContext::MarginCollapse::precomputedMarginBefore(const Box& layoutBox, UsedVerticalMargin::NonCollapsedValues usedNonCollapsedMargin)
{
    ASSERT(layoutBox.isBlockLevelBox());
    // Don't pre-compute vertical margins for out of flow boxes.
    ASSERT(layoutBox.isInFlow() || layoutBox.isFloatingPositioned());
    ASSERT(!layoutBox.isReplacedBox());

    auto positiveNegativeMarginBefore = precomputedPositiveNegativeMarginBefore(layoutBox, usedNonCollapsedMargin);
    auto collapsedMarginBefore = marginValue(positiveNegativeMarginBefore);
    return { usedNonCollapsedMargin.before, collapsedMarginBefore, positiveNegativeMarginBefore };
}

}
}
#endif
