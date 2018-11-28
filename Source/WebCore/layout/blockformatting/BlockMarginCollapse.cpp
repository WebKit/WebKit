/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutUnit.h"
#include "RenderStyle.h"

namespace WebCore {
namespace Layout {

static bool isQuirkContainer(const Box& layoutBox)
{
    return layoutBox.isBodyBox() || layoutBox.isDocumentBox() || layoutBox.isTableCell();
}

static bool hasMarginTopQuirkValue(const Box& layoutBox)
{
    return layoutBox.style().hasMarginBeforeQuirk();
}

static bool shouldIgnoreMarginTopInQuirkContext(const LayoutState& layoutState, const Box& layoutBox)
{
    if (!layoutBox.parent())
        return false;
    return layoutState.inQuirksMode() && isQuirkContainer(*layoutBox.parent()) && hasMarginTopQuirkValue(layoutBox);
}

static LayoutUnit marginValue(LayoutUnit currentMarginValue, LayoutUnit candidateMarginValue)
{
    if (!candidateMarginValue)
        return currentMarginValue;
    if (!currentMarginValue)
        return candidateMarginValue;
    // Both margins are positive.
    if (candidateMarginValue > 0 && currentMarginValue > 0)
        return std::max(candidateMarginValue, currentMarginValue);
    // Both margins are negative.
    if (candidateMarginValue < 0 && currentMarginValue < 0)
        return 0 - std::max(std::abs(candidateMarginValue.toFloat()), std::abs(currentMarginValue.toFloat()));
    // One of the margins is negative.
    return currentMarginValue + candidateMarginValue;
}

static bool isMarginTopCollapsedWithSibling(const Box& layoutBox)
{
    ASSERT(layoutBox.isBlockLevelBox());

    if (layoutBox.isFloatingPositioned())
        return false;

    if (!layoutBox.isPositioned() || layoutBox.isInFlowPositioned())
        return true;

    // Out of flow positioned.
    ASSERT(layoutBox.isOutOfFlowPositioned());
    return layoutBox.style().top().isAuto();
}

static bool isMarginBottomCollapsedWithSibling(const Box& layoutBox)
{
    ASSERT(layoutBox.isBlockLevelBox());

    if (layoutBox.isFloatingPositioned())
        return false;

    if (!layoutBox.isPositioned() || layoutBox.isInFlowPositioned())
        return true;

    // Out of flow positioned.
    ASSERT(layoutBox.isOutOfFlowPositioned());
    return layoutBox.style().bottom().isAuto();
}

static bool isMarginTopCollapsedWithParent(const LayoutState& layoutState, const Box& layoutBox)
{
    // The first inflow child could propagate its top margin to parent.
    // https://www.w3.org/TR/CSS21/box.html#collapsing-margins
    if (layoutBox.isAnonymous())
        return false;

    ASSERT(layoutBox.isBlockLevelBox());

    if (layoutBox.isFloatingOrOutOfFlowPositioned())
        return false;

    // We never margin collapse the initial containing block.
    ASSERT(layoutBox.parent());
    auto& parent = *layoutBox.parent();
    // Only the first inlflow child collapses with parent.
    if (layoutBox.previousInFlowSibling())
        return false;

    if (parent.establishesBlockFormattingContext())
        return false;

    // Margins of the root element's box do not collapse.
    if (parent.isDocumentBox() || parent.isInitialContainingBlock())
        return false;

    auto& parentDisplayBox = layoutState.displayBoxForLayoutBox(parent);
    if (parentDisplayBox.borderTop())
        return false;

    if (parentDisplayBox.paddingTop().value_or(0))
        return false;

    if (shouldIgnoreMarginTopInQuirkContext(layoutState, layoutBox))
        return false;

    return true;
}

static bool isMarginBottomCollapsedThrough(const LayoutState& layoutState, const Box& layoutBox)
{
    ASSERT(layoutBox.isBlockLevelBox());

    // If the top and bottom margins of a box are adjoining, then it is possible for margins to collapse through it.
    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);

    if (displayBox.borderTop() || displayBox.borderBottom())
        return false;

    if (displayBox.paddingTop().value_or(0) || displayBox.paddingBottom().value_or(0))
        return false;

    if (!layoutBox.style().height().isAuto() || !layoutBox.style().minHeight().isAuto())
        return false;

    if (!is<Container>(layoutBox))
        return true;

    auto& container = downcast<Container>(layoutBox);
    if (container.hasInFlowOrFloatingChild())
        return false;

    return true;
}

LayoutUnit BlockFormattingContext::Geometry::MarginCollapse::collapsedMarginTopFromFirstChild(const LayoutState& layoutState, const Box& layoutBox)
{
    ASSERT(layoutBox.isBlockLevelBox());

    // Check if the first child collapses its margin top.
    if (!is<Container>(layoutBox) || !downcast<Container>(layoutBox).hasInFlowChild())
        return 0;

    // Do not collapse margin with a box from a non-block formatting context <div><span>foobar</span></div>.
    if (layoutBox.establishesFormattingContext() && !layoutBox.establishesBlockFormattingContextOnly())
        return 0;

    // FIXME: Take collapsed through margin into account.
    auto& firstInFlowChild = *downcast<Container>(layoutBox).firstInFlowChild();
    if (!isMarginTopCollapsedWithParent(layoutState, firstInFlowChild))
        return 0;
    // Collect collapsed margin top recursively.
    return marginValue(computedNonCollapsedMarginTop(layoutState, firstInFlowChild), collapsedMarginTopFromFirstChild(layoutState, firstInFlowChild));
}

LayoutUnit BlockFormattingContext::Geometry::MarginCollapse::nonCollapsedMarginTop(const LayoutState& layoutState, const Box& layoutBox)
{
    ASSERT(layoutBox.isBlockLevelBox());

    // Non collapsed margin top includes collapsed margin from inflow first child.
    return marginValue(computedNonCollapsedMarginTop(layoutState, layoutBox), collapsedMarginTopFromFirstChild(layoutState, layoutBox));
}

/*static bool hasAdjoiningMarginTopAndBottom(const Box&)
{
    // Two margins are adjoining if and only if:
    // 1. both belong to in-flow block-level boxes that participate in the same block formatting context
    // 2. no line boxes, no clearance, no padding and no border separate them (Note that certain zero-height line boxes (see 9.4.2) are ignored for this purpose.)
    // 3. both belong to vertically-adjacent box edges, i.e. form one of the following pairs:
    //        top margin of a box and top margin of its first in-flow child
    //        bottom margin of box and top margin of its next in-flow following sibling
    //        bottom margin of a last in-flow child and bottom margin of its parent if the parent has 'auto' computed height
    //        top and bottom margins of a box that does not establish a new block formatting context and that has zero computed 'min-height',
    //        zero or 'auto' computed 'height', and no in-flow children
    // A collapsed margin is considered adjoining to another margin if any of its component margins is adjoining to that margin.
    return false;
}*/
LayoutUnit BlockFormattingContext::Geometry::MarginCollapse::computedNonCollapsedMarginTop(const LayoutState& layoutState, const Box& layoutBox)
{
    ASSERT(layoutBox.isBlockLevelBox());

    return computedNonCollapsedVerticalMarginValue(layoutState, layoutBox).top;
}

LayoutUnit BlockFormattingContext::Geometry::MarginCollapse::computedNonCollapsedMarginBottom(const LayoutState& layoutState, const Box& layoutBox)
{
    ASSERT(layoutBox.isBlockLevelBox());

    return computedNonCollapsedVerticalMarginValue(layoutState, layoutBox).bottom;
}

LayoutUnit BlockFormattingContext::Geometry::MarginCollapse::marginTop(const LayoutState& layoutState, const Box& layoutBox)
{
    if (layoutBox.isAnonymous())
        return 0;

    ASSERT(layoutBox.isBlockLevelBox());

    // TODO: take _hasAdjoiningMarginTopAndBottom() into account.
    if (isMarginTopCollapsedWithParent(layoutState, layoutBox))
        return 0;

    // FIXME: Find out the logic behind this.
    if (shouldIgnoreMarginTopInQuirkContext(layoutState, layoutBox))
        return 0;

    if (!isMarginTopCollapsedWithSibling(layoutBox)) {
        if (!isMarginBottomCollapsedThrough(layoutState, layoutBox))
            return nonCollapsedMarginTop(layoutState, layoutBox);
        // Compute the collapsed through value.
        auto marginTop = nonCollapsedMarginTop(layoutState, layoutBox);
        auto marginBottom = nonCollapsedMarginBottom(layoutState, layoutBox); 
        return marginValue(marginTop, marginBottom);
    }

    // The bottom margin of an in-flow block-level element always collapses with the top margin of its next in-flow block-level sibling,
    // unless that sibling has clearance.
    auto* previousInFlowSibling = layoutBox.previousInFlowSibling();
    if (!previousInFlowSibling)
        return nonCollapsedMarginTop(layoutState, layoutBox);

    auto previousSiblingMarginBottom = nonCollapsedMarginBottom(layoutState, *previousInFlowSibling);
    auto marginTop = nonCollapsedMarginTop(layoutState, layoutBox);
    return marginValue(marginTop, previousSiblingMarginBottom);
}

LayoutUnit BlockFormattingContext::Geometry::MarginCollapse::marginBottom(const LayoutState& layoutState, const Box& layoutBox)
{
    if (layoutBox.isAnonymous())
        return 0;

    ASSERT(layoutBox.isBlockLevelBox());

    // TODO: take _hasAdjoiningMarginTopAndBottom() into account.
    if (isMarginBottomCollapsedWithParent(layoutState, layoutBox))
        return 0;

    if (isMarginBottomCollapsedThrough(layoutState, layoutBox))
        return 0;

    // Floats and out of flow positioned boxes do not collapse their margins.
    if (!isMarginBottomCollapsedWithSibling(layoutBox))
        return nonCollapsedMarginBottom(layoutState, layoutBox);

    // The bottom margin of an in-flow block-level element always collapses with the top margin of its next in-flow block-level sibling,
    // unless that sibling has clearance.
    if (layoutBox.nextInFlowSibling())
        return 0;
    return nonCollapsedMarginBottom(layoutState, layoutBox);
}

bool BlockFormattingContext::Geometry::MarginCollapse::isMarginBottomCollapsedWithParent(const LayoutState& layoutState, const Box& layoutBox)
{
    // last inflow box to parent.
    // https://www.w3.org/TR/CSS21/box.html#collapsing-margins
    if (layoutBox.isAnonymous())
        return false;

    ASSERT(layoutBox.isBlockLevelBox());

    if (layoutBox.isFloatingOrOutOfFlowPositioned())
        return false;

    if (isMarginBottomCollapsedThrough(layoutState, layoutBox))
        return false;

    // We never margin collapse the initial containing block.
    ASSERT(layoutBox.parent());
    auto& parent = *layoutBox.parent();
    // Only the last inlflow child collapses with parent.
    if (layoutBox.nextInFlowSibling())
        return false;

    if (parent.establishesBlockFormattingContext())
        return false;

    // Margins of the root element's box do not collapse.
    if (parent.isDocumentBox() || parent.isInitialContainingBlock())
        return false;

    auto& parentDisplayBox = layoutState.displayBoxForLayoutBox(parent);
    if (parentDisplayBox.borderTop())
        return false;

    if (parentDisplayBox.paddingTop().value_or(0))
        return false;

    if (!parent.style().height().isAuto())
        return false;

    return true;
}

bool BlockFormattingContext::Geometry::MarginCollapse::isMarginTopCollapsedWithParentMarginBottom(const Box&)
{
    return false;
}

LayoutUnit BlockFormattingContext::Geometry::MarginCollapse::collapsedMarginBottomFromLastChild(const LayoutState& layoutState, const Box& layoutBox)
{
    ASSERT(layoutBox.isBlockLevelBox());

    // Check if the last child propagates its margin bottom.
    if (!is<Container>(layoutBox) || !downcast<Container>(layoutBox).hasInFlowChild())
        return 0;

    // Do not collapse margin with a box from a non-block formatting context <div><span>foobar</span></div>.
    if (layoutBox.establishesFormattingContext() && !layoutBox.establishesBlockFormattingContextOnly())
        return 0;

    // FIXME: Check for collapsed through margin.
    auto& lastInFlowChild = *downcast<Container>(layoutBox).lastInFlowChild();
    if (!isMarginBottomCollapsedWithParent(layoutState, lastInFlowChild))
        return 0;

    // Collect collapsed margin bottom recursively.
    return marginValue(computedNonCollapsedMarginBottom(layoutState, lastInFlowChild), collapsedMarginBottomFromLastChild(layoutState, lastInFlowChild));
}

LayoutUnit BlockFormattingContext::Geometry::MarginCollapse::nonCollapsedMarginBottom(const LayoutState& layoutState, const Box& layoutBox)
{
    ASSERT(layoutBox.isBlockLevelBox());

    // Non collapsed margin bottom includes collapsed margin from inflow last child.
    return marginValue(computedNonCollapsedMarginBottom(layoutState, layoutBox), collapsedMarginBottomFromLastChild(layoutState, layoutBox));
}

}
}
#endif
