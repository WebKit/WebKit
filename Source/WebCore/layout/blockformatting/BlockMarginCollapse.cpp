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

static bool hasBorder(const BorderValue& borderValue)
{
    if (borderValue.style() == BorderStyle::None || borderValue.style() == BorderStyle::Hidden)
        return false;
    return !!borderValue.width();
}

static bool hasPadding(const Length& paddingValue)
{
    // FIXME: Check if percent value needs to be resolved.
    return !paddingValue.isZero();
}

static bool hasBorderBefore(const Box& layoutBox)
{
    return hasBorder(layoutBox.style().borderBefore());
}

static bool hasBorderAfter(const Box& layoutBox)
{
    return hasBorder(layoutBox.style().borderAfter());
}

static bool hasPaddingBefore(const Box& layoutBox)
{
    return hasPadding(layoutBox.style().paddingBefore());
}

static bool hasPaddingAfter(const Box& layoutBox)
{
    return hasPadding(layoutBox.style().paddingAfter());
}

static bool hasClearance(const Box& layoutBox)
{
    if (!layoutBox.hasFloatClear())
        return false;
    // FIXME
    return false;
}

static bool establishesBlockFormattingContext(const Box& layoutBox)
{
    // WebKit treats the document element renderer as a block formatting context root. It probably only impacts margin collapsing, so let's not do
    // a layout wide quirk on this for now.
    if (layoutBox.isDocumentBox())
        return true;
    return layoutBox.establishesBlockFormattingContext();
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

LayoutUnit BlockFormattingContext::Geometry::MarginCollapse::computedNonCollapsedMarginBefore(const LayoutState& layoutState, const Box& layoutBox)
{
    ASSERT(layoutBox.isBlockLevelBox());

    return computedNonCollapsedVerticalMarginValue(layoutState, layoutBox).before;
}

LayoutUnit BlockFormattingContext::Geometry::MarginCollapse::computedNonCollapsedMarginAfter(const LayoutState& layoutState, const Box& layoutBox)
{
    ASSERT(layoutBox.isBlockLevelBox());

    return computedNonCollapsedVerticalMarginValue(layoutState, layoutBox).after;
}

LayoutUnit BlockFormattingContext::Geometry::MarginCollapse::nonCollapsedMarginBefore(const LayoutState& layoutState, const Box& layoutBox)
{
    ASSERT(layoutBox.isBlockLevelBox());

    // Non collapsed margin top includes collapsed margin from inflow first child.
    return marginValue(computedNonCollapsedMarginBefore(layoutState, layoutBox), collapsedMarginBeforeFromFirstChild(layoutState, layoutBox));
}

LayoutUnit BlockFormattingContext::Geometry::MarginCollapse::nonCollapsedMarginAfter(const LayoutState& layoutState, const Box& layoutBox)
{
    ASSERT(layoutBox.isBlockLevelBox());

    // Non collapsed margin bottom includes collapsed margin from inflow last child.
    return marginValue(computedNonCollapsedMarginAfter(layoutState, layoutBox), collapsedMarginAfterFromLastChild(layoutState, layoutBox));
}

LayoutUnit BlockFormattingContext::Geometry::MarginCollapse::collapsedMarginBeforeFromFirstChild(const LayoutState& layoutState, const Box& layoutBox)
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
    if (!marginBeforeCollapsesWithParentMarginBefore(layoutState, firstInFlowChild))
        return 0;
    // Collect collapsed margin top recursively.
    return marginValue(computedNonCollapsedMarginBefore(layoutState, firstInFlowChild), collapsedMarginBeforeFromFirstChild(layoutState, firstInFlowChild));
}

LayoutUnit BlockFormattingContext::Geometry::MarginCollapse::collapsedMarginAfterFromLastChild(const LayoutState& layoutState, const Box& layoutBox)
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
    if (!marginAfterCollapsesWithParentMarginAfter(lastInFlowChild))
        return 0;

    // Collect collapsed margin bottom recursively.
    return marginValue(computedNonCollapsedMarginAfter(layoutState, lastInFlowChild), collapsedMarginAfterFromLastChild(layoutState, lastInFlowChild));
}

bool BlockFormattingContext::Geometry::MarginCollapse::marginBeforeCollapsesWithParentMarginAfter(const Box&)
{
    return false;
}

bool BlockFormattingContext::Geometry::MarginCollapse::marginBeforeCollapsesWithParentMarginBefore(const LayoutState& layoutState, const Box& layoutBox)
{
    // The first inflow child could propagate its top margin to parent.
    // https://www.w3.org/TR/CSS21/box.html#collapsing-margins
    if (layoutBox.isAnonymous())
        return false;

    ASSERT(layoutBox.isBlockLevelBox());

    // Margins between a floated box and any other box do not collapse.
    if (layoutBox.isFloatingPositioned())
        return false;

    // Margins of absolutely positioned boxes do not collapse.
    if (layoutBox.isOutOfFlowPositioned())
        return false;

    // Margins of inline-block boxes do not collapse.
    if (layoutBox.isInlineBlockBox())
        return false;

    // Only the first inlflow child collapses with parent.
    if (layoutBox.previousInFlowSibling())
        return false;

    auto& parent = *layoutBox.parent();
    // Margins of elements that establish new block formatting contexts do not collapse with their in-flow children
    if (establishesBlockFormattingContext(parent))
        return false;

    if (hasBorderBefore(parent))
        return false;

    if (hasPaddingBefore(parent))
        return false;

    // ...and the child has no clearance.
    if (hasClearance(layoutBox))
        return false;

    if (BlockFormattingContext::Quirks::shouldIgnoreMarginBefore(layoutState, layoutBox))
        return false;

    return true;
}

bool BlockFormattingContext::Geometry::MarginCollapse::marginAfterCollapsesWithSiblingMarginBeforeWithClearance(const Box&)
{
    return false;
}

bool BlockFormattingContext::Geometry::MarginCollapse::marginAfterCollapsesWithParentMarginBefore(const Box&)
{
    return false;
}

bool BlockFormattingContext::Geometry::MarginCollapse::marginAfterCollapsesWithParentMarginAfter(const Box& layoutBox)
{
    if (layoutBox.isAnonymous())
        return false;

    ASSERT(layoutBox.isBlockLevelBox());

    // Margins between a floated box and any other box do not collapse.
    if (layoutBox.isFloatingPositioned())
        return false;

    // Margins of absolutely positioned boxes do not collapse.
    if (layoutBox.isOutOfFlowPositioned())
        return false;

    // Margins of inline-block boxes do not collapse.
    if (layoutBox.isInlineBlockBox())
        return false;

    // Only the last inlflow child collapses with parent.
    if (layoutBox.nextInFlowSibling())
        return false;

    auto& parent = *layoutBox.parent();
    // Margins of elements that establish new block formatting contexts do not collapse with their in-flow children.
    if (establishesBlockFormattingContext(parent))
        return false;

    // The bottom margin of an in-flow block box with a 'height' of 'auto' collapses with its last in-flow block-level child's bottom margin, if:
    if (!parent.style().height().isAuto())
        return false;

    // the box has no bottom padding, and
    if (hasPaddingBefore(parent))
        return false;

    // the box has no bottom border, and
    if (hasBorderBefore(parent))
        return false;

    // the child's bottom margin neither collapses with a top margin that has clearance...
    if (marginAfterCollapsesWithSiblingMarginBeforeWithClearance(layoutBox))
        return false;

    // nor (if the box's min-height is non-zero) with the box's top margin.
    auto computedMinHeight = parent.style().logicalMinHeight();
    if (!computedMinHeight.isAuto() && computedMinHeight.value() && marginAfterCollapsesWithParentMarginBefore(layoutBox))
        return false;

    return true;
}

bool BlockFormattingContext::Geometry::MarginCollapse::marginBeforeCollapsesWithPreviousSibling(const Box& layoutBox)
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

bool BlockFormattingContext::Geometry::MarginCollapse::marginAfterCollapsesWithNextSibling(const Box& layoutBox)
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

bool BlockFormattingContext::Geometry::MarginCollapse::marginsCollapseThrough(const Box& layoutBox)
{
    ASSERT(layoutBox.isBlockLevelBox());

    // If the top and bottom margins of a box are adjoining, then it is possible for margins to collapse through it.
    if (hasBorderBefore(layoutBox) || hasBorderAfter(layoutBox))
        return false;

    if (hasPaddingBefore(layoutBox) || hasPaddingAfter(layoutBox))
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

LayoutUnit BlockFormattingContext::Geometry::MarginCollapse::marginBefore(const LayoutState& layoutState, const Box& layoutBox)
{
    if (layoutBox.isAnonymous())
        return 0;

    ASSERT(layoutBox.isBlockLevelBox());

    // TODO: take _hasAdjoiningMarginBeforeAndAfter() into account.
    if (marginBeforeCollapsesWithParentMarginBefore(layoutState, layoutBox))
        return 0;

    // FIXME: Find out the logic behind this.
    if (BlockFormattingContext::Quirks::shouldIgnoreMarginBefore(layoutState, layoutBox))
        return 0;

    if (!marginBeforeCollapsesWithPreviousSibling(layoutBox)) {
        if (!marginsCollapseThrough(layoutBox))
            return nonCollapsedMarginBefore(layoutState, layoutBox);
        // Compute the collapsed through value.
        auto marginBefore = nonCollapsedMarginBefore(layoutState, layoutBox);
        auto marginAfter = nonCollapsedMarginAfter(layoutState, layoutBox); 
        return marginValue(marginBefore, marginAfter);
    }

    // The bottom margin of an in-flow block-level element always collapses with the top margin of its next in-flow block-level sibling,
    // unless that sibling has clearance.
    auto* previousInFlowSibling = layoutBox.previousInFlowSibling();
    if (!previousInFlowSibling)
        return nonCollapsedMarginBefore(layoutState, layoutBox);

    auto previousSiblingMarginAfter = nonCollapsedMarginAfter(layoutState, *previousInFlowSibling);
    auto marginBefore = nonCollapsedMarginBefore(layoutState, layoutBox);
    return marginValue(marginBefore, previousSiblingMarginAfter);
}

LayoutUnit BlockFormattingContext::Geometry::MarginCollapse::marginAfter(const LayoutState& layoutState, const Box& layoutBox)
{
    if (layoutBox.isAnonymous())
        return 0;

    ASSERT(layoutBox.isBlockLevelBox());

    // TODO: take _hasAdjoiningMarginBeforeAndBottom() into account.
    if (marginAfterCollapsesWithParentMarginAfter(layoutBox))
        return 0;

    if (marginsCollapseThrough(layoutBox))
        return 0;

    // Floats and out of flow positioned boxes do not collapse their margins.
    if (!marginAfterCollapsesWithNextSibling(layoutBox))
        return nonCollapsedMarginAfter(layoutState, layoutBox);

    // The bottom margin of an in-flow block-level element always collapses with the top margin of its next in-flow block-level sibling,
    // unless that sibling has clearance.
    if (layoutBox.nextInFlowSibling())
        return 0;
    return nonCollapsedMarginAfter(layoutState, layoutBox);
}

}
}
#endif
