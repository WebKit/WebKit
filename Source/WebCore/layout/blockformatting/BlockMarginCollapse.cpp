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

#include "InlineFormattingState.h"
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

static bool hasClearance(const LayoutState& layoutState, const Box& layoutBox)
{
    if (!layoutBox.hasFloatClear())
        return false;
    return layoutState.displayBoxForLayoutBox(layoutBox).hasClearance();
}

static bool establishesBlockFormattingContext(const Box& layoutBox)
{
    // WebKit treats the document element renderer as a block formatting context root. It probably only impacts margin collapsing, so let's not do
    // a layout wide quirk on this for now.
    if (layoutBox.isDocumentBox())
        return true;
    return layoutBox.establishesBlockFormattingContext();
}

bool BlockFormattingContext::MarginCollapse::marginBeforeCollapsesWithParentMarginAfter(const LayoutState& layoutState, const Box& layoutBox)
{
    // 1. This is the last in-flow child and its margins collapse through and the margin after collapses with parent's margin after or
    // 2. This box's margin after collapses with the next sibling's margin before and that sibling collapses through and
    // we can get to the last in-flow child like that.
    auto* lastInFlowChild = layoutBox.parent()->lastInFlowChild();
    for (auto* currentBox = &layoutBox; currentBox; currentBox = currentBox->nextInFlowSibling()) {
        if (!marginsCollapseThrough(layoutState, *currentBox))
            return false;
        if (currentBox == lastInFlowChild)
            return marginAfterCollapsesWithParentMarginAfter(layoutState, *currentBox); 
        if (!marginAfterCollapsesWithNextSiblingMarginBefore(layoutState, *currentBox))
            return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool BlockFormattingContext::MarginCollapse::marginBeforeCollapsesWithParentMarginBefore(const LayoutState& layoutState, const Box& layoutBox)
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
    if (hasClearance(layoutState, layoutBox))
        return false;

    return true;
}

bool BlockFormattingContext::MarginCollapse::marginBeforeCollapsesWithPreviousSiblingMarginAfter(const LayoutState& layoutState, const Box& layoutBox)
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
    if (hasClearance(layoutState, layoutBox))
        return false;

    return true;
}

bool BlockFormattingContext::MarginCollapse::marginBeforeCollapsesWithFirstInFlowChildMarginBefore(const LayoutState& layoutState, const Box& layoutBox)
{
    if (layoutBox.isAnonymous())
        return false;

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

    if (!is<Container>(layoutBox) || !downcast<Container>(layoutBox).hasInFlowChild())
        return false;

    auto& firstInFlowChild = *downcast<Container>(layoutBox).firstInFlowChild();
    if (!firstInFlowChild.isBlockLevelBox())
        return false;

    // ...and the child has no clearance.
    if (hasClearance(layoutState, firstInFlowChild))
        return false;

    // Margins between a floated box and any other box do not collapse.
    if (firstInFlowChild.isFloatingPositioned())
        return false;

    // Margins of absolutely positioned boxes do not collapse.
    if (firstInFlowChild.isOutOfFlowPositioned())
        return false;

    // Margins of inline-block boxes do not collapse.
    if (firstInFlowChild.isInlineBlockBox())
        return false;

    return true;
}

bool BlockFormattingContext::MarginCollapse::marginAfterCollapsesWithSiblingMarginBeforeWithClearance(const LayoutState& layoutState, const Box& layoutBox)
{
    // If the top and bottom margins of an element with clearance are adjoining, its margins collapse with the adjoining margins
    // of following siblings but that resulting margin does not collapse with the bottom margin of the parent block.
    if (!marginsCollapseThrough(layoutState, layoutBox))
        return false;

    for (auto* previousSibling = layoutBox.previousInFlowSibling(); previousSibling; previousSibling = previousSibling->previousInFlowSibling()) {
        if (!marginsCollapseThrough(layoutState, *previousSibling))
            return false;
        if (hasClearance(layoutState, *previousSibling))
            return true;
    }
    return false;
}

bool BlockFormattingContext::MarginCollapse::marginAfterCollapsesWithParentMarginBefore(const LayoutState& layoutState, const Box& layoutBox)
{
    // 1. This is the first in-flow child and its margins collapse through and the margin before collapses with parent's margin before or
    // 2. This box's margin before collapses with the previous sibling's margin after and that sibling collapses through and
    // we can get to the first in-flow child like that.
    auto* firstInFlowChild = layoutBox.parent()->firstInFlowChild();
    for (auto* currentBox = &layoutBox; currentBox; currentBox = currentBox->previousInFlowSibling()) {
        if (!marginsCollapseThrough(layoutState, *currentBox))
            return false;
        if (currentBox == firstInFlowChild)
            return marginBeforeCollapsesWithParentMarginBefore(layoutState, *currentBox); 
        if (!marginBeforeCollapsesWithPreviousSiblingMarginAfter(layoutState, *currentBox))
            return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool BlockFormattingContext::MarginCollapse::marginAfterCollapsesWithParentMarginAfter(const LayoutState& layoutState, const Box& layoutBox)
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
    if (marginAfterCollapsesWithSiblingMarginBeforeWithClearance(layoutState, layoutBox))
        return false;

    // nor (if the box's min-height is non-zero) with the box's top margin.
    auto computedMinHeight = parent.style().logicalMinHeight();
    if (!computedMinHeight.isAuto() && computedMinHeight.value() && marginAfterCollapsesWithParentMarginBefore(layoutState, layoutBox))
        return false;

    return true;
}

bool BlockFormattingContext::MarginCollapse::marginAfterCollapsesWithLastInFlowChildMarginAfter(const LayoutState& layoutState, const Box& layoutBox)
{
    ASSERT(layoutBox.isBlockLevelBox());

    // Margins of elements that establish new block formatting contexts do not collapse with their in-flow children.
    if (establishesBlockFormattingContext(layoutBox))
        return false;

    if (!is<Container>(layoutBox) || !downcast<Container>(layoutBox).hasInFlowChild())
        return false;

    auto& lastInFlowChild = *downcast<Container>(layoutBox).lastInFlowChild();
    if (!lastInFlowChild.isBlockLevelBox())
        return false;

    // The bottom margin of an in-flow block box with a 'height' of 'auto' collapses with its last in-flow block-level child's bottom margin, if:
    if (!layoutBox.style().height().isAuto())
        return false;

    // the box has no bottom padding, and
    if (hasPaddingBefore(layoutBox))
        return false;

    // the box has no bottom border, and
    if (hasBorderBefore(layoutBox))
        return false;

    // the child's bottom margin neither collapses with a top margin that has clearance...
    if (marginAfterCollapsesWithSiblingMarginBeforeWithClearance(layoutState, lastInFlowChild))
        return false;

    // nor (if the box's min-height is non-zero) with the box's top margin.
    auto computedMinHeight = layoutBox.style().logicalMinHeight();
    if (!computedMinHeight.isAuto() && computedMinHeight.value()
        && (marginAfterCollapsesWithParentMarginBefore(layoutState, lastInFlowChild) || hasClearance(layoutState, lastInFlowChild)))
        return false;

    // Margins between a floated box and any other box do not collapse.
    if (lastInFlowChild.isFloatingPositioned())
        return false;

    // Margins of absolutely positioned boxes do not collapse.
    if (lastInFlowChild.isOutOfFlowPositioned())
        return false;

    // Margins of inline-block boxes do not collapse.
    if (lastInFlowChild.isInlineBlockBox())
        return false;

    return true;
}

bool BlockFormattingContext::MarginCollapse::marginAfterCollapsesWithNextSiblingMarginBefore(const LayoutState& layoutState, const Box& layoutBox)
{
    ASSERT(layoutBox.isBlockLevelBox());

    if (!layoutBox.nextInFlowSibling())
        return false;

    return marginBeforeCollapsesWithPreviousSiblingMarginAfter(layoutState, *layoutBox.nextInFlowSibling());
}

bool BlockFormattingContext::MarginCollapse::marginsCollapseThrough(const LayoutState& layoutState, const Box& layoutBox)
{
    ASSERT(layoutBox.isBlockLevelBox());

    // A box's own margins collapse if the 'min-height' property is zero, and it has neither top or bottom borders nor top or bottom padding,
    // and it has a 'height' of either 0 or 'auto', and it does not contain a line box, and all of its in-flow children's margins (if any) collapse.
    if (hasBorderBefore(layoutBox) || hasBorderAfter(layoutBox))
        return false;

    if (hasPaddingBefore(layoutBox) || hasPaddingAfter(layoutBox))
        return false;

    // FIXME: Check for computed 0 height.
    if (!layoutBox.style().height().isAuto())
        return false;

    // FIXME: Check for computed 0 height.
    if (!layoutBox.style().minHeight().isAuto())
        return false;

    if (!is<Container>(layoutBox))
        return true;

    if (!downcast<Container>(layoutBox).hasInFlowChild())
        return !establishesBlockFormattingContext(layoutBox);

    if (Quirks::needsStretching(layoutState, layoutBox))
        return false;

    if (layoutBox.establishesFormattingContext()) {
        if (layoutBox.establishesInlineFormattingContext()) {
            // If we get here through margin estimation, we don't necessarily have an actual state for this layout box since
            // we haven't started laying it out yet.
            if (!layoutState.hasFormattingState(layoutBox))
                return false;
            auto& formattingState = downcast<InlineFormattingState>(layoutState.establishedFormattingState(layoutBox));
            if (!formattingState.inlineRuns().isEmpty())
                return false;
            // Any float box in this formatting context prevents collapsing through.
            auto& floats = formattingState.floatingState().floats();
            for (auto& floatItem : floats) {
                if (floatItem.isDescendantOfFormattingRoot(downcast<Container>(layoutBox)))
                    return false;
            }
            return true;
        }

        if (establishesBlockFormattingContext(layoutBox))
            return false;
    }

    for (auto* inflowChild = downcast<Container>(layoutBox).firstInFlowOrFloatingChild(); inflowChild; inflowChild = inflowChild->nextInFlowOrFloatingSibling()) {
        if (establishesBlockFormattingContext(*inflowChild))
            return false;
        if (!marginsCollapseThrough(layoutState, *inflowChild))
            return false;
    }
    return true;
}

static PositiveAndNegativeVerticalMargin::Values computedPositiveAndNegativeMargin(PositiveAndNegativeVerticalMargin::Values a, PositiveAndNegativeVerticalMargin::Values b)
{
    PositiveAndNegativeVerticalMargin::Values computedValues;
    if (a.positive && b.positive)
        computedValues.positive = std::max(*a.positive, *b.positive);
    else
        computedValues.positive = a.positive ? a.positive : b.positive;

    if (a.negative && b.negative)
        computedValues.negative = std::min(*a.negative, *b.negative);
    else
        computedValues.negative = a.negative ? a.negative : b.negative;

    return computedValues;
}

static PositiveAndNegativeVerticalMargin::Values computedPositiveAndNegativeMargin(PositiveAndNegativeVerticalMargin::Values a, Optional<LayoutUnit> marginValue)
{
    if (!marginValue || !marginValue.value())
        return a;
    if (*marginValue > 0)
        return computedPositiveAndNegativeMargin(a, { marginValue, { } });
    if (*marginValue < 0)
        return computedPositiveAndNegativeMargin(a, { { }, marginValue });
    ASSERT_NOT_REACHED();
    return { };
}

static Optional<LayoutUnit> marginValue(PositiveAndNegativeVerticalMargin::Values marginValues)
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

void BlockFormattingContext::MarginCollapse::updateCollapsedMarginAfter(const LayoutState& layoutState, const Box& layoutBox, const UsedVerticalMargin& nextSiblingVerticalMargin)
{
    // 1. Get the margin before value from the next in-flow sibling. This is the same as this box's margin after value now since they are collapsed.
    // 2. Update the collapsed margin after value as well as the positive/negative cache.
    // 3. Check if the box's margins collapse through.
    // 4. If so, update the collapsed margin before value as well as the positive/negative cache.
    // 5. In case of collapsed through margins check if the before margin collapes with the previous inflow sibling's after margin.
    // 6. If so, jump to #2.
    if (!marginAfterCollapsesWithNextSiblingMarginBefore(layoutState, layoutBox))
        return;

    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);
    auto verticalMargin = displayBox.verticalMargin();
    auto verticalMarginAfter = nextSiblingVerticalMargin.before();
    Optional<LayoutUnit> verticalMarginBefore;

    auto marginsCollapseThrough = MarginCollapse::marginsCollapseThrough(layoutState, layoutBox);
    if (marginsCollapseThrough)
        verticalMarginBefore = verticalMarginAfter; 
    else if (verticalMargin.hasCollapsedValues())
        verticalMarginBefore = verticalMargin.collapsedValues().before;

    // Update vertical margin values.
    verticalMargin.setCollapsedValues({ verticalMarginBefore, verticalMarginAfter });
    displayBox.setVerticalMargin(verticalMargin);

    // Update positive/negative cache.
    auto& blockFormattingState = downcast<BlockFormattingState>(layoutState.formattingStateForBox(layoutBox));
    auto positiveNegativeMargin = blockFormattingState.positiveAndNegativeVerticalMargin(layoutBox);
    auto nextSiblingPositiveNegativeMarginBefore = blockFormattingState.positiveAndNegativeVerticalMargin(*layoutBox.nextInFlowSibling()).before;

    positiveNegativeMargin.after = computedPositiveAndNegativeMargin(nextSiblingPositiveNegativeMarginBefore, positiveNegativeMargin.after);
    if (marginsCollapseThrough) {
        positiveNegativeMargin.before = computedPositiveAndNegativeMargin(positiveNegativeMargin.before, positiveNegativeMargin.after);
        positiveNegativeMargin.after = positiveNegativeMargin.before; 
    }
    blockFormattingState.setPositiveAndNegativeVerticalMargin(layoutBox, positiveNegativeMargin);

    if (!marginsCollapseThrough || !marginBeforeCollapsesWithPreviousSiblingMarginAfter(layoutState, layoutBox))
        return;

    updateCollapsedMarginAfter(layoutState, *layoutBox.previousInFlowSibling(), verticalMargin);
}

PositiveAndNegativeVerticalMargin::Values BlockFormattingContext::MarginCollapse::positiveNegativeValues(const LayoutState& layoutState, const Box& layoutBox, MarginType marginType)
{
    auto& blockFormattingState = downcast<BlockFormattingState>(layoutState.formattingStateForBox(layoutBox));
    if (blockFormattingState.hasPositiveAndNegativeVerticalMargin(layoutBox)) {
        auto positiveAndNegativeVerticalMargin = blockFormattingState.positiveAndNegativeVerticalMargin(layoutBox);
        return marginType == MarginType::Before ? positiveAndNegativeVerticalMargin.before : positiveAndNegativeVerticalMargin.after; 
    }
    // This is the estimate path. We don't yet have positive/negative margin computed.
    auto computedVerticalMargin = Geometry::computedVerticalMargin(layoutState, layoutBox);
    auto nonCollapsedMargin = UsedVerticalMargin::NonCollapsedValues { computedVerticalMargin.before.valueOr(0), computedVerticalMargin.after.valueOr(0) }; 

    if (marginType == MarginType::Before)
        return positiveNegativeMarginBefore(layoutState, layoutBox, nonCollapsedMargin);
    return positiveNegativeMarginAfter(layoutState, layoutBox, nonCollapsedMargin);
}

PositiveAndNegativeVerticalMargin::Values BlockFormattingContext::MarginCollapse::positiveNegativeMarginBefore(const LayoutState& layoutState, const Box& layoutBox, const UsedVerticalMargin::NonCollapsedValues& nonCollapsedValues)
{
    auto firstChildCollapsedMarginBefore = [&]() -> PositiveAndNegativeVerticalMargin::Values {
        if (!is<Container>(layoutBox))
            return { };
        auto* firstInFlowChild = downcast<Container>(layoutBox).firstInFlowChild();
        if (!firstInFlowChild)
            return { };
        if (!marginBeforeCollapsesWithFirstInFlowChildMarginBefore(layoutState, layoutBox))
            return { };
        if (Quirks::shouldIgnoreMarginBefore(layoutState, *firstInFlowChild))
            return { };
        return positiveNegativeValues(layoutState, *firstInFlowChild, MarginType::Before);
    };

    auto previouSiblingCollapsedMarginAfter = [&]() -> PositiveAndNegativeVerticalMargin::Values {
        auto* previousInFlowSibling = layoutBox.previousInFlowSibling();
        if (!previousInFlowSibling)
            return { };
        if (!marginBeforeCollapsesWithPreviousSiblingMarginAfter(layoutState, layoutBox))
            return { };
        if (Quirks::shouldIgnoreMarginAfter(layoutState, *previousInFlowSibling))
            return { };
        return positiveNegativeValues(layoutState, *previousInFlowSibling, MarginType::After);
    };

    // 1. Gather positive and negative margin values from first child if margins are adjoining.
    // 2. Gather positive and negative margin values from previous inflow sibling if margins are adjoining.
    // 3. Compute min/max positive and negative collapsed margin values using non-collpased computed margin before.
    auto collapsedMarginBefore = computedPositiveAndNegativeMargin(firstChildCollapsedMarginBefore(), previouSiblingCollapsedMarginAfter());
    return computedPositiveAndNegativeMargin(collapsedMarginBefore, nonCollapsedValues.before);
}

PositiveAndNegativeVerticalMargin::Values BlockFormattingContext::MarginCollapse::positiveNegativeMarginAfter(const LayoutState& layoutState, const Box& layoutBox, const UsedVerticalMargin::NonCollapsedValues& nonCollapsedValues)
{
    auto lastChildCollapsedMarginAfter = [&]() -> PositiveAndNegativeVerticalMargin::Values {
        if (!is<Container>(layoutBox))
            return { };
        auto* lastInFlowChild = downcast<Container>(layoutBox).lastInFlowChild();
        if (!lastInFlowChild)
            return { };
        if (!marginAfterCollapsesWithLastInFlowChildMarginAfter(layoutState, layoutBox))
            return { };
        if (Quirks::shouldIgnoreMarginAfter(layoutState, *lastInFlowChild))
            return { };
        return positiveNegativeValues(layoutState, *lastInFlowChild, MarginType::After);
    };

    // We don't know yet the margin before value of the next sibling. Let's just pretend it does not have one and 
    // update it later when we compute the next sibling's margin before. See updateCollapsedMarginAfter.
    return computedPositiveAndNegativeMargin(lastChildCollapsedMarginAfter(), nonCollapsedValues.after);
}

EstimatedMarginBefore BlockFormattingContext::MarginCollapse::estimatedMarginBefore(const LayoutState& layoutState, const Box& layoutBox)
{
    if (layoutBox.isAnonymous())
        return { };

    ASSERT(layoutBox.isBlockLevelBox());
    // Can't estimate vertical margins for out of flow boxes (and we shouldn't need to do it for float boxes).
    ASSERT(layoutBox.isInFlow());
    ASSERT(!layoutBox.replaced());
    // Can't cross block formatting context boundary.
    ASSERT(!layoutBox.establishesBlockFormattingContext());
    
    auto computedVerticalMargin = Geometry::computedVerticalMargin(layoutState, layoutBox);
    auto nonCollapsedMargin = UsedVerticalMargin::NonCollapsedValues { computedVerticalMargin.before.valueOr(0), computedVerticalMargin.after.valueOr(0) };
    auto marginsCollapseThrough = MarginCollapse::marginsCollapseThrough(layoutState, layoutBox);
    auto positiveNegativeMarginBefore = MarginCollapse::positiveNegativeMarginBefore(layoutState, layoutBox, nonCollapsedMargin);

    auto collapsedMarginBefore = marginValue(!marginsCollapseThrough ? positiveNegativeMarginBefore
        : computedPositiveAndNegativeMargin(positiveNegativeMarginBefore, positiveNegativeMarginAfter(layoutState, layoutBox, nonCollapsedMargin)));

    return { nonCollapsedMargin.before, collapsedMarginBefore, marginsCollapseThrough };
}

LayoutUnit BlockFormattingContext::MarginCollapse::marginBeforeIgnoringCollapsingThrough(const LayoutState& layoutState, const Box& layoutBox, const UsedVerticalMargin::NonCollapsedValues& nonCollapsedValues)
{
    ASSERT(!layoutBox.isAnonymous());
    ASSERT(layoutBox.isBlockLevelBox());
    return marginValue(positiveNegativeMarginBefore(layoutState, layoutBox, nonCollapsedValues)).valueOr(nonCollapsedValues.before);
}

UsedVerticalMargin::CollapsedValues BlockFormattingContext::MarginCollapse::collapsedVerticalValues(const LayoutState& layoutState, const Box& layoutBox, const UsedVerticalMargin::NonCollapsedValues& nonCollapsedValues)
{
    if (layoutBox.isAnonymous())
        return { };

    ASSERT(layoutBox.isBlockLevelBox());
    // 1. Get min/max margin top values from the first in-flow child if we are collapsing margin top with it.
    // 2. Get min/max margin top values from the previous in-flow sibling, if we are collapsing margin top with it.
    // 3. Get this layout box's computed margin top value.
    // 4. Update the min/max value and compute the final margin. 
    auto positiveNegativeMarginBefore = MarginCollapse::positiveNegativeMarginBefore(layoutState, layoutBox, nonCollapsedValues);
    auto positiveNegativeMarginAfter = MarginCollapse::positiveNegativeMarginAfter(layoutState, layoutBox, nonCollapsedValues);

    auto marginsCollapseThrough = MarginCollapse::marginsCollapseThrough(layoutState, layoutBox); 
    if (marginsCollapseThrough) {
        positiveNegativeMarginBefore = computedPositiveAndNegativeMargin(positiveNegativeMarginBefore, positiveNegativeMarginAfter);
        positiveNegativeMarginAfter = positiveNegativeMarginBefore;         
    }

    auto& blockFormattingState = downcast<BlockFormattingState>(layoutState.formattingStateForBox(layoutBox));
    blockFormattingState.setPositiveAndNegativeVerticalMargin(layoutBox, { positiveNegativeMarginBefore, positiveNegativeMarginAfter });

    return { marginValue(positiveNegativeMarginBefore), marginValue(positiveNegativeMarginAfter), marginsCollapseThrough }; 
}

}
}
#endif
