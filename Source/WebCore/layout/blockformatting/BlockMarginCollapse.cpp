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
#include "BlockMarginCollapse.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutUnit.h"
#include "RenderStyle.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(BlockMarginCollapse);

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

BlockMarginCollapse::BlockMarginCollapse()
{
}

LayoutUnit BlockMarginCollapse::marginTop(const Box& layoutBox) const
{
    if (layoutBox.isAnonymous())
        return 0;
    
    // TODO: take _hasAdjoiningMarginTopAndBottom() into account.
    if (isMarginTopCollapsedWithParent(layoutBox))
        return 0;
    
    // Floats and out of flow positioned boxes do not collapse their margins.
    if (!isMarginTopCollapsedWithSibling(layoutBox))
        return nonCollapsedMarginTop(layoutBox);
    
    // The bottom margin of an in-flow block-level element always collapses with the top margin of its next in-flow block-level sibling,
    // unless that sibling has clearance.
    auto* previousInFlowSibling = layoutBox.previousInFlowSibling();
    if (!previousInFlowSibling)
        return nonCollapsedMarginTop(layoutBox);

    auto previousSiblingMarginBottom = nonCollapsedMarginBottom(*previousInFlowSibling);
    auto marginTop = nonCollapsedMarginTop(layoutBox);
    return marginValue(marginTop, previousSiblingMarginBottom);
}

LayoutUnit BlockMarginCollapse::marginBottom(const Box& layoutBox) const
{
    if (layoutBox.isAnonymous())
        return 0;

    // TODO: take _hasAdjoiningMarginTopAndBottom() into account.
    if (isMarginBottomCollapsedWithParent(layoutBox))
        return 0;

    // Floats and out of flow positioned boxes do not collapse their margins.
    if (!isMarginBottomCollapsedWithSibling(layoutBox))
        return nonCollapsedMarginBottom(layoutBox);
    
    // The bottom margin of an in-flow block-level element always collapses with the top margin of its next in-flow block-level sibling,
    // unless that sibling has clearance.
    if (layoutBox.nextInFlowSibling())
        return 0;
    return nonCollapsedMarginBottom(layoutBox);
}

bool BlockMarginCollapse::isMarginTopCollapsedWithSibling(const Box& layoutBox) const
{
    if (layoutBox.isFloatingPositioned())
        return false;

    if (!layoutBox.isPositioned() || layoutBox.isInFlowPositioned())
        return true;

    // Out of flow positioned.
    ASSERT(layoutBox.isOutOfFlowPositioned());
    return layoutBox.style().top().isAuto();
}

bool BlockMarginCollapse::isMarginBottomCollapsedWithSibling(const Box& layoutBox) const
{
    if (layoutBox.isFloatingPositioned())
        return false;

    if (!layoutBox.isPositioned() || layoutBox.isInFlowPositioned())
        return true;

    // Out of flow positioned.
    ASSERT(layoutBox.isOutOfFlowPositioned());
    return layoutBox.style().bottom().isAuto();
}

bool BlockMarginCollapse::isMarginTopCollapsedWithParent(const Box& layoutBox) const
{
    // The first inflow child could propagate its top margin to parent.
    // https://www.w3.org/TR/CSS21/box.html#collapsing-margins
    if (layoutBox.isAnonymous())
        return false;

    if (layoutBox.isFloatingOrOutOfFlowPositioned())
        return false;

    // We never margin collapse the initial containing block.
    ASSERT(layoutBox.parent());
    auto& parent = *layoutBox.parent();
    // Is this box the first inlflow child?
    if (parent.firstInFlowChild() != &layoutBox)
        return false;

    if (parent.establishesBlockFormattingContext())
        return false;

    // Margins of the root element's box do not collapse.
    if (parent.isInitialContainingBlock())
        return false;

    if (!parent.style().borderTop().nonZero())
        return false;

    if (!parent.style().paddingTop().isZero())
        return false;

    return true;
}

bool BlockMarginCollapse::isMarginBottomCollapsedWithParent(const Box& layoutBox) const
{
    // last inflow box to parent.
    // https://www.w3.org/TR/CSS21/box.html#collapsing-margins
    if (layoutBox.isAnonymous())
        return false;

    if (layoutBox.isFloatingOrOutOfFlowPositioned())
        return false;

    // We never margin collapse the initial containing block.
    ASSERT(layoutBox.parent());
    auto& parent = *layoutBox.parent();
    // Is this the last inlflow child?
    if (parent.lastInFlowChild() != &layoutBox)
        return false;

    if (parent.establishesBlockFormattingContext())
        return false;

    // Margins of the root element's box do not collapse.
    if (parent.isInitialContainingBlock())
        return false;

    if (!parent.style().borderTop().nonZero())
        return false;

    if (!parent.style().paddingTop().isZero())
        return false;

    if (!parent.style().height().isAuto())
        return false;

    return true;
}

LayoutUnit BlockMarginCollapse::nonCollapsedMarginTop(const Box& layoutBox) const
{
    // Non collapsed margin top includes collapsed margin from inflow first child.
    return marginValue(layoutBox.style().marginTop().value(), collapsedMarginTopFromFirstChild(layoutBox));
}

LayoutUnit BlockMarginCollapse::nonCollapsedMarginBottom(const Box& layoutBox) const
{
    // Non collapsed margin bottom includes collapsed margin from inflow last child.
    return marginValue(layoutBox.style().marginBottom().value(), collapsedMarginBottomFromLastChild(layoutBox));
}

LayoutUnit BlockMarginCollapse::collapsedMarginTopFromFirstChild(const Box& layoutBox) const
{
    // Check if the first child collapses its margin top.
    if (!is<Container>(layoutBox) || !downcast<Container>(layoutBox).hasInFlowChild())
        return 0;

    auto& firstInFlowChild = *downcast<Container>(layoutBox).firstInFlowChild();
    if (!isMarginTopCollapsedWithParent(firstInFlowChild))
        return 0;

    // Collect collapsed margin top recursively.
    return marginValue(firstInFlowChild.style().marginTop().value(), collapsedMarginTopFromFirstChild(firstInFlowChild));
}

LayoutUnit BlockMarginCollapse::collapsedMarginBottomFromLastChild(const Box& layoutBox) const
{
    // Check if the last child propagates its margin bottom.
    if (!is<Container>(layoutBox) || !downcast<Container>(layoutBox).hasInFlowChild())
        return 0;

    auto& lastInFlowChild = *downcast<Container>(layoutBox).lastInFlowChild();
    if (!isMarginBottomCollapsedWithParent(lastInFlowChild))
        return 0;

    // Collect collapsed margin bottom recursively.
    return marginValue(lastInFlowChild.style().marginBottom().value(), collapsedMarginBottomFromLastChild(lastInFlowChild));
}

bool BlockMarginCollapse::hasAdjoiningMarginTopAndBottom(const Box&) const
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
}

}
}
#endif
