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

#include "BlockFormattingState.h"
#include "FloatingState.h"
#include "InlineFormattingState.h"
#include "LayoutBox.h"
#include "LayoutContainerBox.h"
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

static bool establishesBlockFormattingContext(const Box& layoutBox)
{
    // WebKit treats the document element renderer as a block formatting context root. It probably only impacts margin collapsing, so let's not do
    // a layout wide quirk on this for now.
    if (layoutBox.isDocumentBox())
        return true;
    return layoutBox.establishesBlockFormattingContext();
}

bool BlockFormattingContext::MarginCollapse::hasClearance(const Box& layoutBox) const
{
    if (!layoutBox.hasFloatClear())
        return false;
    // FIXME: precomputedVerticalPositionForFormattingRoot logic ends up calling into this function when the layoutBox (first inflow child) has
    // not been laid out.
    if (!layoutState().hasDisplayBox(layoutBox))
        return false;
    return formattingContext().geometryForBox(layoutBox).hasClearance();
}

bool BlockFormattingContext::MarginCollapse::marginBeforeCollapsesWithParentMarginAfter(const Box& layoutBox) const
{
    // 1. This is the last in-flow child and its margins collapse through and the margin after collapses with parent's margin after or
    // 2. This box's margin after collapses with the next sibling's margin before and that sibling collapses through and
    // we can get to the last in-flow child like that.
    auto* lastInFlowChild = layoutBox.containingBlock().lastInFlowChild();
    for (auto* currentBox = &layoutBox; currentBox; currentBox = currentBox->nextInFlowSibling()) {
        if (!marginsCollapseThrough(*currentBox))
            return false;
        if (currentBox == lastInFlowChild)
            return marginAfterCollapsesWithParentMarginAfter(*currentBox); 
        if (!marginAfterCollapsesWithNextSiblingMarginBefore(*currentBox))
            return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool BlockFormattingContext::MarginCollapse::marginBeforeCollapsesWithParentMarginBefore(const Box& layoutBox) const
{
    // The first inflow child could propagate its top margin to parent.
    // https://www.w3.org/TR/CSS21/box.html#collapsing-margins
    ASSERT(layoutBox.isBlockLevelBox());

    if (formattingContext().quirks().shouldCollapseMarginBeforeWithParentMarginBefore(layoutBox))
        return true;

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

    auto& containingBlock = layoutBox.containingBlock();
    // Margins of elements that establish new block formatting contexts do not collapse with their in-flow children
    if (establishesBlockFormattingContext(containingBlock))
        return false;

    if (hasBorderBefore(containingBlock))
        return false;

    if (hasPaddingBefore(containingBlock))
        return false;

    // ...and the child has no clearance.
    if (hasClearance(layoutBox))
        return false;

    return true;
}

bool BlockFormattingContext::MarginCollapse::marginBeforeCollapsesWithPreviousSiblingMarginAfter(const Box& layoutBox) const
{
    ASSERT(layoutBox.isBlockLevelBox());

    if (!layoutBox.previousInFlowSibling())
        return false;

    auto& previousInFlowSibling = *layoutBox.previousInFlowSibling();
    // Margins between a floated box and any other box do not collapse.
    if (layoutBox.isFloatingPositioned() || previousInFlowSibling.isFloatingPositioned())
        return false;

    // Margins of absolutely positioned boxes do not collapse.
    if ((layoutBox.isOutOfFlowPositioned() && !layoutBox.style().top().isAuto())
        || (previousInFlowSibling.isOutOfFlowPositioned() && !previousInFlowSibling.style().bottom().isAuto()))
        return false;

    // Margins of inline-block boxes do not collapse.
    if (layoutBox.isInlineBlockBox() || previousInFlowSibling.isInlineBlockBox())
        return false;

    // The bottom margin of an in-flow block-level element always collapses with the top margin of
    // its next in-flow block-level sibling, unless that sibling has clearance.
    if (hasClearance(layoutBox))
        return false;

    return true;
}

bool BlockFormattingContext::MarginCollapse::marginBeforeCollapsesWithFirstInFlowChildMarginBefore(const Box& layoutBox) const
{
    ASSERT(layoutBox.isBlockLevelBox());
    // Margins of elements that establish new block formatting contexts do not collapse with their in-flow children.
    if (establishesBlockFormattingContext(layoutBox))
        return false;

    // The top margin of an in-flow block element collapses with its first in-flow block-level
    // child's top margin if the element has no top border...
    if (hasBorderBefore(layoutBox))
        return false;

    // ...no top padding
    if (hasPaddingBefore(layoutBox))
        return false;

    if (!is<ContainerBox>(layoutBox) || !downcast<ContainerBox>(layoutBox).hasInFlowChild())
        return false;

    auto& firstInFlowChild = *downcast<ContainerBox>(layoutBox).firstInFlowChild();
    if (!firstInFlowChild.isBlockLevelBox())
        return false;

    // ...and the child has no clearance.
    if (hasClearance(firstInFlowChild))
        return false;

    // Margins of inline-block boxes do not collapse.
    if (firstInFlowChild.isInlineBlockBox())
        return false;

    return true;
}

bool BlockFormattingContext::MarginCollapse::marginAfterCollapsesWithSiblingMarginBeforeWithClearance(const Box& layoutBox) const
{
    // If the top and bottom margins of an element with clearance are adjoining, its margins collapse with the adjoining margins
    // of following siblings but that resulting margin does not collapse with the bottom margin of the parent block.
    if (!marginsCollapseThrough(layoutBox))
        return false;

    for (auto* previousSibling = layoutBox.previousInFlowSibling(); previousSibling; previousSibling = previousSibling->previousInFlowSibling()) {
        if (!marginsCollapseThrough(*previousSibling))
            return false;
        if (hasClearance(*previousSibling))
            return true;
    }
    return false;
}

bool BlockFormattingContext::MarginCollapse::marginAfterCollapsesWithParentMarginBefore(const Box& layoutBox) const
{
    // 1. This is the first in-flow child and its margins collapse through and the margin before collapses with parent's margin before or
    // 2. This box's margin before collapses with the previous sibling's margin after and that sibling collapses through and
    // we can get to the first in-flow child like that.
    auto* firstInFlowChild = layoutBox.containingBlock().firstInFlowChild();
    for (auto* currentBox = &layoutBox; currentBox; currentBox = currentBox->previousInFlowSibling()) {
        if (!marginsCollapseThrough(*currentBox))
            return false;
        if (currentBox == firstInFlowChild)
            return marginBeforeCollapsesWithParentMarginBefore(*currentBox); 
        if (!marginBeforeCollapsesWithPreviousSiblingMarginAfter(*currentBox))
            return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool BlockFormattingContext::MarginCollapse::marginAfterCollapsesWithParentMarginAfter(const Box& layoutBox) const
{
    ASSERT(layoutBox.isBlockLevelBox());

    if (formattingContext().quirks().shouldCollapseMarginAfterWithParentMarginAfter(layoutBox))
        return true;

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

    auto& containingBlock = layoutBox.containingBlock();
    // Margins of elements that establish new block formatting contexts do not collapse with their in-flow children.
    if (establishesBlockFormattingContext(containingBlock))
        return false;

    // The bottom margin of an in-flow block box with a 'height' of 'auto' collapses with its last in-flow block-level child's bottom margin, if:
    if (!containingBlock.style().height().isAuto())
        return false;

    // the box has no bottom padding, and
    if (hasPaddingAfter(containingBlock))
        return false;

    // the box has no bottom border, and
    if (hasBorderAfter(containingBlock))
        return false;

    // the child's bottom margin neither collapses with a top margin that has clearance...
    if (marginAfterCollapsesWithSiblingMarginBeforeWithClearance(layoutBox))
        return false;

    // nor (if the box's min-height is non-zero) with the box's top margin.
    auto computedMinHeight = containingBlock.style().logicalMinHeight();
    if (!computedMinHeight.isAuto() && computedMinHeight.value() && marginAfterCollapsesWithParentMarginBefore(layoutBox))
        return false;

    return true;
}

bool BlockFormattingContext::MarginCollapse::marginAfterCollapsesWithLastInFlowChildMarginAfter(const Box& layoutBox) const
{
    ASSERT(layoutBox.isBlockLevelBox());

    // Margins of elements that establish new block formatting contexts do not collapse with their in-flow children.
    if (establishesBlockFormattingContext(layoutBox))
        return false;

    if (!is<ContainerBox>(layoutBox) || !downcast<ContainerBox>(layoutBox).hasInFlowChild())
        return false;

    auto& lastInFlowChild = *downcast<ContainerBox>(layoutBox).lastInFlowChild();
    if (!lastInFlowChild.isBlockLevelBox())
        return false;

    // The bottom margin of an in-flow block box with a 'height' of 'auto' collapses with its last in-flow block-level child's bottom margin, if:
    if (!layoutBox.style().height().isAuto())
        return false;

    // the box has no bottom padding, and
    if (hasPaddingAfter(layoutBox))
        return false;

    // the box has no bottom border, and
    if (hasBorderAfter(layoutBox))
        return false;

    // the child's bottom margin neither collapses with a top margin that has clearance...
    if (marginAfterCollapsesWithSiblingMarginBeforeWithClearance(lastInFlowChild))
        return false;

    // nor (if the box's min-height is non-zero) with the box's top margin.
    auto computedMinHeight = layoutBox.style().logicalMinHeight();
    if (!computedMinHeight.isAuto() && computedMinHeight.value()
        && (marginAfterCollapsesWithParentMarginBefore(lastInFlowChild) || hasClearance(lastInFlowChild)))
        return false;

    // Margins of inline-block boxes do not collapse.
    if (lastInFlowChild.isInlineBlockBox())
        return false;

    // This is a quirk behavior: When the margin after of the last inflow child (or a previous sibling with collapsed through margins)
    // collapses with a quirk parent's the margin before, then the same margin after does not collapses with the parent's margin after.
    if (formattingContext().quirks().shouldIgnoreCollapsedQuirkMargin(layoutBox) && marginAfterCollapsesWithParentMarginBefore(lastInFlowChild))
        return false;

    return true;
}

bool BlockFormattingContext::MarginCollapse::marginAfterCollapsesWithNextSiblingMarginBefore(const Box& layoutBox) const
{
    ASSERT(layoutBox.isBlockLevelBox());

    if (!layoutBox.nextInFlowSibling())
        return false;

    return marginBeforeCollapsesWithPreviousSiblingMarginAfter(*layoutBox.nextInFlowSibling());
}

bool BlockFormattingContext::MarginCollapse::marginsCollapseThrough(const Box& layoutBox) const
{
    ASSERT(layoutBox.isBlockLevelBox());

    // A box's own margins collapse if the 'min-height' property is zero, and it has neither top or bottom borders nor top or bottom padding,
    // and it has a 'height' of either 0 or 'auto', and it does not contain a line box, and all of its in-flow children's margins (if any) collapse.
    if (hasBorderBefore(layoutBox) || hasBorderAfter(layoutBox))
        return false;

    if (hasPaddingBefore(layoutBox) || hasPaddingAfter(layoutBox))
        return false;

    // Margins are not adjoining when the box has clearance.
    if (hasClearance(layoutBox))
        return false;

    auto& style = layoutBox.style();
    auto computedHeightValueIsZero = style.height().isFixed() && !style.height().value();
    if (!(style.height().isAuto() || computedHeightValueIsZero))
        return false;

    // FIXME: Check for computed 0 height.
    if (!style.minHeight().isAuto())
        return false;

    // FIXME: Block replaced boxes clearly don't collapse through their margins, but I couldn't find it in the spec yet (and no, it's not a quirk).
    if (layoutBox.isReplacedBox())
        return false;

    if (!is<ContainerBox>(layoutBox))
        return true;

    if (!downcast<ContainerBox>(layoutBox).hasInFlowChild())
        return !establishesBlockFormattingContext(layoutBox);

    if (layoutBox.establishesFormattingContext()) {
        if (layoutBox.establishesInlineFormattingContext()) {
            auto& layoutState = this->layoutState();
            // If we get here through margin estimation, we don't necessarily have an actual state for this layout box since
            // we haven't started laying it out yet.
            auto& containerBox = downcast<ContainerBox>(layoutBox);
            if (!layoutState.hasInlineFormattingState(containerBox))
                return false;

            auto isConsideredEmpty = [&] {
                auto& formattingState = layoutState.establishedInlineFormattingState(containerBox);
                if (auto* inlineContent = formattingState.displayInlineContent(); inlineContent && !inlineContent->lines.isEmpty())
                    return false;
                // Any float box in this formatting context prevents collapsing through.
                auto& floats = formattingState.floatingState().floats();
                for (auto& floatItem : floats) {
                    if (floatItem.isInFormattingContextOf(containerBox))
                        return false;
                }
                return true;
            };
            return isConsideredEmpty();
        }

        // A root of a non-inline formatting context (table, flex etc) with inflow descendants should not collapse through.
        return false;
    }

    for (auto* inflowChild = downcast<ContainerBox>(layoutBox).firstInFlowOrFloatingChild(); inflowChild; inflowChild = inflowChild->nextInFlowOrFloatingSibling()) {
        if (establishesBlockFormattingContext(*inflowChild))
            return false;
        if (!marginsCollapseThrough(*inflowChild))
            return false;
    }
    return true;
}

UsedVerticalMargin::PositiveAndNegativePair::Values BlockFormattingContext::MarginCollapse::computedPositiveAndNegativeMargin(UsedVerticalMargin::PositiveAndNegativePair::Values a, UsedVerticalMargin::PositiveAndNegativePair::Values b) const
{
    UsedVerticalMargin::PositiveAndNegativePair::Values computedValues;
    if (a.positive && b.positive)
        computedValues.positive = std::max(*a.positive, *b.positive);
    else
        computedValues.positive = a.positive ? a.positive : b.positive;

    if (a.negative && b.negative)
        computedValues.negative = std::min(*a.negative, *b.negative);
    else
        computedValues.negative = a.negative ? a.negative : b.negative;

    if (a.isNonZero() && b.isNonZero())
        computedValues.isQuirk = a.isQuirk || b.isQuirk;
    else if (a.isNonZero())
        computedValues.isQuirk = a.isQuirk;
    else
        computedValues.isQuirk = b.isQuirk;

    return computedValues;
}

Optional<LayoutUnit> BlockFormattingContext::MarginCollapse::marginValue(UsedVerticalMargin::PositiveAndNegativePair::Values marginValues) const
{
    // When two or more margins collapse, the resulting margin width is the maximum of the collapsing margins' widths.
    // In the case of negative margins, the maximum of the absolute values of the negative adjoining margins is deducted from the maximum
    // of the positive adjoining margins. If there are no positive margins, the maximum of the absolute values of the adjoining margins is deducted from zero.
    if (!marginValues.negative)
        return marginValues.positive;

    if (!marginValues.positive)
        return marginValues.negative;

    return *marginValues.positive + *marginValues.negative;
}

void BlockFormattingContext::MarginCollapse::updateMarginAfterForPreviousSibling(BlockFormattingContext& blockFormattingContext, const MarginCollapse& marginCollapse, const Box& layoutBox)
{
    // 1. Get the margin before value from the next in-flow sibling. This is the same as this box's margin after value now since they are collapsed.
    // 2. Update the collapsed margin after value as well as the positive/negative cache.
    // 3. Check if the box's margins collapse through.
    // 4. If so, update the positive/negative cache.
    // 5. In case of collapsed through margins check if the before margin collapes with the previous inflow sibling's after margin.
    // 6. If so, jump to #2.
    // 7. No need to propagate to parent because its margin is not computed yet (pre-computed at most).
    auto* currentBox = &layoutBox;
    auto& blockFormattingState = blockFormattingContext.formattingState();
    while (marginCollapse.marginBeforeCollapsesWithPreviousSiblingMarginAfter(*currentBox)) {
        auto& previousSibling = *currentBox->previousInFlowSibling();
        auto previousSiblingVerticalMargin = blockFormattingState.usedVerticalMargin(previousSibling);

        auto collapsedVerticalMarginBefore = previousSiblingVerticalMargin.collapsedValues.before;
        auto collapsedVerticalMarginAfter = blockFormattingContext.geometryForBox(*currentBox).marginBefore();

        auto marginsCollapseThrough = marginCollapse.marginsCollapseThrough(previousSibling);
        if (marginsCollapseThrough)
            collapsedVerticalMarginBefore = collapsedVerticalMarginAfter;

        // Update positive/negative cache.
        auto previousSiblingPositiveNegativeMargin = blockFormattingState.usedVerticalMargin(previousSibling).positiveAndNegativeValues;
        auto positiveNegativeMarginBefore = blockFormattingState.usedVerticalMargin(*currentBox).positiveAndNegativeValues.before;

        auto adjustedPreviousSiblingVerticalMargin = previousSiblingVerticalMargin;
        adjustedPreviousSiblingVerticalMargin.positiveAndNegativeValues.after = marginCollapse.computedPositiveAndNegativeMargin(positiveNegativeMarginBefore, previousSiblingPositiveNegativeMargin.after);
        if (marginsCollapseThrough) {
            adjustedPreviousSiblingVerticalMargin.positiveAndNegativeValues.before = marginCollapse.computedPositiveAndNegativeMargin(previousSiblingPositiveNegativeMargin.before, adjustedPreviousSiblingVerticalMargin.positiveAndNegativeValues.after);
            adjustedPreviousSiblingVerticalMargin.positiveAndNegativeValues.after = adjustedPreviousSiblingVerticalMargin.positiveAndNegativeValues.before;
        }
        blockFormattingState.setUsedVerticalMargin(previousSibling, adjustedPreviousSiblingVerticalMargin);

        if (!marginsCollapseThrough)
            break;

        currentBox = &previousSibling;
    }
}

UsedVerticalMargin::PositiveAndNegativePair::Values BlockFormattingContext::MarginCollapse::positiveNegativeValues(const Box& layoutBox, MarginType marginType) const
{
    auto& blockFormattingState = downcast<BlockFormattingState>(layoutState().formattingStateForBox(layoutBox));
    // By the time we get here in BFC layout to gather positive and negative margin values for either a previous sibling or a child box,
    // we mush have computed and cached those values.
    ASSERT(blockFormattingState.hasUsedVerticalMargin(layoutBox));
    auto positiveAndNegativeVerticalMargin = blockFormattingState.usedVerticalMargin(layoutBox).positiveAndNegativeValues;
    return marginType == MarginType::Before ? positiveAndNegativeVerticalMargin.before : positiveAndNegativeVerticalMargin.after; 
}

UsedVerticalMargin::PositiveAndNegativePair::Values BlockFormattingContext::MarginCollapse::positiveNegativeMarginBefore(const Box& layoutBox, UsedVerticalMargin::NonCollapsedValues nonCollapsedValues) const
{
    auto firstChildCollapsedMarginBefore = [&]() -> UsedVerticalMargin::PositiveAndNegativePair::Values {
        if (!marginBeforeCollapsesWithFirstInFlowChildMarginBefore(layoutBox))
            return { };
        return positiveNegativeValues(*downcast<ContainerBox>(layoutBox).firstInFlowChild(), MarginType::Before);
    };

    auto previouSiblingCollapsedMarginAfter = [&]() -> UsedVerticalMargin::PositiveAndNegativePair::Values {
        if (!marginBeforeCollapsesWithPreviousSiblingMarginAfter(layoutBox))
            return { };
        return positiveNegativeValues(*layoutBox.previousInFlowSibling(), MarginType::After);
    };

    // 1. Gather positive and negative margin values from first child if margins are adjoining.
    // 2. Gather positive and negative margin values from previous inflow sibling if margins are adjoining.
    // 3. Compute min/max positive and negative collapsed margin values using non-collpased computed margin before.
    auto collapsedMarginBefore = computedPositiveAndNegativeMargin(firstChildCollapsedMarginBefore(), previouSiblingCollapsedMarginAfter());
    if (collapsedMarginBefore.isQuirk && formattingContext().quirks().shouldIgnoreCollapsedQuirkMargin(layoutBox))
        collapsedMarginBefore = { };

    UsedVerticalMargin::PositiveAndNegativePair::Values nonCollapsedBefore;
    if (nonCollapsedValues.before > 0)
        nonCollapsedBefore = { nonCollapsedValues.before, { }, layoutBox.style().hasMarginBeforeQuirk() };
    else if (nonCollapsedValues.before < 0)
        nonCollapsedBefore = { { }, nonCollapsedValues.before, layoutBox.style().hasMarginBeforeQuirk() };

    return computedPositiveAndNegativeMargin(collapsedMarginBefore, nonCollapsedBefore);
}

UsedVerticalMargin::PositiveAndNegativePair::Values BlockFormattingContext::MarginCollapse::positiveNegativeMarginAfter(const Box& layoutBox, UsedVerticalMargin::NonCollapsedValues nonCollapsedValues) const
{
    auto lastChildCollapsedMarginAfter = [&]() -> UsedVerticalMargin::PositiveAndNegativePair::Values {
        if (!marginAfterCollapsesWithLastInFlowChildMarginAfter(layoutBox))
            return { };
        return positiveNegativeValues(*downcast<ContainerBox>(layoutBox).lastInFlowChild(), MarginType::After);
    };

    // We don't know yet the margin before value of the next sibling. Let's just pretend it does not have one and 
    // update it later when we compute the next sibling's margin before. See updateMarginAfterForPreviousSibling.
    UsedVerticalMargin::PositiveAndNegativePair::Values nonCollapsedAfter;
    if (nonCollapsedValues.after > 0)
        nonCollapsedAfter = { nonCollapsedValues.after, { }, layoutBox.style().hasMarginAfterQuirk() };
    else if (nonCollapsedValues.after < 0)
        nonCollapsedAfter = { { }, nonCollapsedValues.after, layoutBox.style().hasMarginAfterQuirk() };

    return computedPositiveAndNegativeMargin(lastChildCollapsedMarginAfter(), nonCollapsedAfter);
}

LayoutUnit BlockFormattingContext::MarginCollapse::marginBeforeIgnoringCollapsingThrough(const Box& layoutBox, UsedVerticalMargin::NonCollapsedValues nonCollapsedValues)
{
    ASSERT(layoutBox.isBlockLevelBox());
    return marginValue(positiveNegativeMarginBefore(layoutBox, nonCollapsedValues)).valueOr(nonCollapsedValues.before);
}

UsedVerticalMargin BlockFormattingContext::MarginCollapse::collapsedVerticalValues(const Box& layoutBox, UsedVerticalMargin::NonCollapsedValues nonCollapsedValues)
{
    ASSERT(layoutBox.isBlockLevelBox());
    // 1. Get min/max margin top values from the first in-flow child if we are collapsing margin top with it.
    // 2. Get min/max margin top values from the previous in-flow sibling, if we are collapsing margin top with it.
    // 3. Get this layout box's computed margin top value.
    // 4. Update the min/max value and compute the final margin.
    auto positiveAndNegativeVerticalMargin = UsedVerticalMargin::PositiveAndNegativePair { this->positiveNegativeMarginBefore(layoutBox, nonCollapsedValues), this->positiveNegativeMarginAfter(layoutBox, nonCollapsedValues) };

    auto marginsCollapseThrough = this->marginsCollapseThrough(layoutBox);
    if (marginsCollapseThrough) {
        positiveAndNegativeVerticalMargin.before = computedPositiveAndNegativeMargin(positiveAndNegativeVerticalMargin.before, positiveAndNegativeVerticalMargin.after);
        positiveAndNegativeVerticalMargin.after = positiveAndNegativeVerticalMargin.before;
    }

    auto hasCollapsedMarginBefore = marginBeforeCollapsesWithFirstInFlowChildMarginBefore(layoutBox) || marginBeforeCollapsesWithPreviousSiblingMarginAfter(layoutBox);
    auto hasCollapsedMarginAfter = marginAfterCollapsesWithLastInFlowChildMarginAfter(layoutBox);
    auto usedVerticalMargin = UsedVerticalMargin { nonCollapsedValues, { }, positiveAndNegativeVerticalMargin };

    if ((hasCollapsedMarginBefore && hasCollapsedMarginAfter) || marginsCollapseThrough)
        usedVerticalMargin.collapsedValues = { marginValue(positiveAndNegativeVerticalMargin.before), marginValue(positiveAndNegativeVerticalMargin.after), marginsCollapseThrough };
    else if (hasCollapsedMarginBefore)
        usedVerticalMargin.collapsedValues = { marginValue(positiveAndNegativeVerticalMargin.before), { }, false };
    else if (hasCollapsedMarginAfter)
        usedVerticalMargin.collapsedValues = { { }, marginValue(positiveAndNegativeVerticalMargin.after), false };
    return usedVerticalMargin;
}

}
}
#endif
