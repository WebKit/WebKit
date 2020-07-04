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
#include "DisplayBox.h"
#include "FormattingContext.h"
#include "InlineFormattingState.h"
#include "LayoutChildIterator.h"
#include "LayoutContext.h"
#include "LayoutInitialContainingBlock.h"
#include "LayoutReplacedBox.h"
#include "Logging.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

ContentHeightAndMargin BlockFormattingContext::Geometry::inFlowNonReplacedHeightAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints, const OverrideVerticalValues& overrideVerticalValues)
{
    ASSERT(layoutBox.isInFlow() && !layoutBox.isReplacedBox());
    ASSERT(layoutBox.isOverflowVisible());

    auto compute = [&](const auto& overrideVerticalValues) -> ContentHeightAndMargin {

        // 10.6.3 Block-level non-replaced elements in normal flow when 'overflow' computes to 'visible'
        //
        // If 'margin-top', or 'margin-bottom' are 'auto', their used value is 0.
        // If 'height' is 'auto', the height depends on whether the element has any block-level children and whether it has padding or borders:
        // The element's height is the distance from its top content edge to the first applicable of the following:
        // 1. the bottom edge of the last line box, if the box establishes a inline formatting context with one or more lines
        // 2. the bottom edge of the bottom (possibly collapsed) margin of its last in-flow child, if the child's bottom margin
        //    does not collapse with the element's bottom margin
        // 3. the bottom border edge of the last in-flow child whose top margin doesn't collapse with the element's bottom margin
        // 4. zero, otherwise
        // Only children in the normal flow are taken into account (i.e., floating boxes and absolutely positioned boxes are ignored,
        // and relatively positioned boxes are considered without their offset). Note that the child box may be an anonymous block box.

        auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
        auto computedVerticalMargin = Geometry::computedVerticalMargin(layoutBox, horizontalConstraints);
        auto nonCollapsedMargin = UsedVerticalMargin::NonCollapsedValues { computedVerticalMargin.before.valueOr(0), computedVerticalMargin.after.valueOr(0) }; 
        auto borderAndPaddingTop = boxGeometry.borderTop() + boxGeometry.paddingTop().valueOr(0);
        auto height = overrideVerticalValues.height ? overrideVerticalValues.height.value() : computedHeight(layoutBox);

        if (height)
            return { *height, nonCollapsedMargin };

        if (!is<ContainerBox>(layoutBox) || !downcast<ContainerBox>(layoutBox).hasInFlowChild())
            return { 0, nonCollapsedMargin };

        // 1. the bottom edge of the last line box, if the box establishes a inline formatting context with one or more lines
        auto& layoutContainer = downcast<ContainerBox>(layoutBox);
        if (layoutContainer.establishesInlineFormattingContext()) {
            auto& lineBoxes = layoutState().establishedInlineFormattingState(layoutContainer).displayInlineContent()->lineBoxes;
            // Even empty containers generate one line. 
            ASSERT(!lineBoxes.isEmpty());
            return { toLayoutUnit(lineBoxes.last().bottom()) - borderAndPaddingTop, nonCollapsedMargin };
        }

        // 2. the bottom edge of the bottom (possibly collapsed) margin of its last in-flow child, if the child's bottom margin...
        auto* lastInFlowChild = layoutContainer.lastInFlowChild();
        ASSERT(lastInFlowChild);
        if (!formattingContext().marginCollapse().marginAfterCollapsesWithParentMarginAfter(*lastInFlowChild)) {
            auto& lastInFlowBoxGeometry = formattingContext().geometryForBox(*lastInFlowChild);
            auto bottomEdgeOfBottomMargin = lastInFlowBoxGeometry.bottom() + (lastInFlowBoxGeometry.hasCollapsedThroughMargin() ? LayoutUnit() : lastInFlowBoxGeometry.marginAfter()); 
            return { bottomEdgeOfBottomMargin - borderAndPaddingTop, nonCollapsedMargin };
        }

        // 3. the bottom border edge of the last in-flow child whose top margin doesn't collapse with the element's bottom margin
        auto* inFlowChild = lastInFlowChild;
        while (inFlowChild && formattingContext().marginCollapse().marginBeforeCollapsesWithParentMarginAfter(*inFlowChild))
            inFlowChild = inFlowChild->previousInFlowSibling();
        if (inFlowChild) {
            auto& inFlowDisplayBoxGeometry = formattingContext().geometryForBox(*inFlowChild);
            return { inFlowDisplayBoxGeometry.top() + inFlowDisplayBoxGeometry.borderBox().height() - borderAndPaddingTop, nonCollapsedMargin };
        }

        // 4. zero, otherwise
        return { 0, nonCollapsedMargin };
    };

    // 10.6.7 'Auto' heights for block formatting context roots
    auto isAutoHeight = !overrideVerticalValues.height && !computedHeight(layoutBox);
    if (isAutoHeight && layoutBox.establishesBlockFormattingContext())
        return compute( OverrideVerticalValues { contentHeightForFormattingContextRoot(layoutBox) });
    return compute(overrideVerticalValues);
}

ContentWidthAndMargin BlockFormattingContext::Geometry::inFlowNonReplacedWidthAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints, const OverrideHorizontalValues& overrideHorizontalValues)
{
    ASSERT(layoutBox.isInFlow());

    auto compute = [&]() {

        // 10.3.3 Block-level, non-replaced elements in normal flow
        //
        // The following constraints must hold among the used values of the other properties:
        // 'margin-left' + 'border-left-width' + 'padding-left' + 'width' + 'padding-right' + 'border-right-width' + 'margin-right' = width of containing block
        //
        // 1. If 'width' is not 'auto' and 'border-left-width' + 'padding-left' + 'width' + 'padding-right' + 'border-right-width' 
        //    (plus any of 'margin-left' or 'margin-right' that are not 'auto') is larger than the width of the containing block, then
        //    any 'auto' values for 'margin-left' or 'margin-right' are, for the following rules, treated as zero.
        //
        // 2. If all of the above have a computed value other than 'auto', the values are said to be "over-constrained" and one of the used values will
        //    have to be different from its computed value. If the 'direction' property of the containing block has the value 'ltr', the specified value
        //    of 'margin-right' is ignored and the value is calculated so as to make the equality true. If the value of 'direction' is 'rtl',
        //    this happens to 'margin-left' instead.
        //
        // 3. If there is exactly one value specified as 'auto', its used value follows from the equality.
        //
        // 4. If 'width' is set to 'auto', any other 'auto' values become '0' and 'width' follows from the resulting equality.
        //
        // 5. If both 'margin-left' and 'margin-right' are 'auto', their used values are equal. This horizontally centers the element with respect to the
        //    edges of the containing block.

        auto containingBlockWidth = horizontalConstraints.logicalWidth;
        auto& containingBlockStyle = layoutBox.containingBlock().style();
        auto& boxGeometry = formattingContext().geometryForBox(layoutBox);

        auto width = overrideHorizontalValues.width ? overrideHorizontalValues.width : computedWidth(layoutBox, containingBlockWidth);
        auto computedHorizontalMargin = Geometry::computedHorizontalMargin(layoutBox, horizontalConstraints);
        UsedHorizontalMargin usedHorizontalMargin;
        auto borderLeft = boxGeometry.borderLeft();
        auto borderRight = boxGeometry.borderRight();
        auto paddingLeft = boxGeometry.paddingLeft().valueOr(0);
        auto paddingRight = boxGeometry.paddingRight().valueOr(0);

        // #1
        if (width) {
            auto horizontalSpaceForMargin = containingBlockWidth - (computedHorizontalMargin.start.valueOr(0) + borderLeft + paddingLeft + *width + paddingRight + borderRight + computedHorizontalMargin.end.valueOr(0));
            if (horizontalSpaceForMargin < 0)
                usedHorizontalMargin = { computedHorizontalMargin.start.valueOr(0), computedHorizontalMargin.end.valueOr(0) };
        }

        // #2
        if (width && computedHorizontalMargin.start && computedHorizontalMargin.end) {
            if (containingBlockStyle.isLeftToRightDirection()) {
                usedHorizontalMargin.start = *computedHorizontalMargin.start;
                usedHorizontalMargin.end = containingBlockWidth - (usedHorizontalMargin.start + borderLeft + paddingLeft + *width + paddingRight + borderRight);
            } else {
                usedHorizontalMargin.end = *computedHorizontalMargin.end;
                usedHorizontalMargin.start = containingBlockWidth - (borderLeft + paddingLeft + *width + paddingRight + borderRight + usedHorizontalMargin.end);
            }
        }

        // #3
        if (!computedHorizontalMargin.start && width && computedHorizontalMargin.end) {
            usedHorizontalMargin.end = *computedHorizontalMargin.end;
            usedHorizontalMargin.start = containingBlockWidth - (borderLeft + paddingLeft  + *width + paddingRight + borderRight + usedHorizontalMargin.end);
        } else if (computedHorizontalMargin.start && !width && computedHorizontalMargin.end) {
            usedHorizontalMargin = { *computedHorizontalMargin.start, *computedHorizontalMargin.end };
            width = containingBlockWidth - (usedHorizontalMargin.start + borderLeft + paddingLeft + paddingRight + borderRight + usedHorizontalMargin.end);
        } else if (computedHorizontalMargin.start && width && !computedHorizontalMargin.end) {
            usedHorizontalMargin.start = *computedHorizontalMargin.start;
            usedHorizontalMargin.end = containingBlockWidth - (usedHorizontalMargin.start + borderLeft + paddingLeft + *width + paddingRight + borderRight);
        }

        // #4
        if (!width) {
            usedHorizontalMargin = { computedHorizontalMargin.start.valueOr(0), computedHorizontalMargin.end.valueOr(0) };
            width = containingBlockWidth - (usedHorizontalMargin.start + borderLeft + paddingLeft + paddingRight + borderRight + usedHorizontalMargin.end);
        }

        // #5
        if (!computedHorizontalMargin.start && !computedHorizontalMargin.end) {
            auto horizontalSpaceForMargin = containingBlockWidth - (borderLeft + paddingLeft  + *width + paddingRight + borderRight);
            usedHorizontalMargin = { horizontalSpaceForMargin / 2, horizontalSpaceForMargin / 2 };
        }

        auto shouldApplyCenterAlignForBlockContent = containingBlockStyle.textAlign() == TextAlignMode::WebKitCenter && (computedHorizontalMargin.start || computedHorizontalMargin.end);
        if (shouldApplyCenterAlignForBlockContent) {
            auto borderBoxWidth = (borderLeft + paddingLeft  + *width + paddingRight + borderRight);
            auto marginStart = computedHorizontalMargin.start.valueOr(0);
            auto marginEnd = computedHorizontalMargin.end.valueOr(0);
            auto centeredLogicalLeftForMarginBox = std::max((containingBlockWidth - borderBoxWidth - marginStart - marginEnd) / 2, 0_lu);
            usedHorizontalMargin.start = centeredLogicalLeftForMarginBox + marginStart;
            usedHorizontalMargin.end = containingBlockWidth - borderBoxWidth - marginStart + marginEnd;
        }
        ASSERT(width);

        return ContentWidthAndMargin { *width, usedHorizontalMargin };
    };

    auto contentWidthAndMargin = compute();
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Width][Margin] -> inflow non-replaced -> width(" << contentWidthAndMargin.contentWidth << "px) margin(" << contentWidthAndMargin.usedMargin.start << "px, " << contentWidthAndMargin.usedMargin.end << "px) -> layoutBox(" << &layoutBox << ")");
    return contentWidthAndMargin;
}

ContentWidthAndMargin BlockFormattingContext::Geometry::inFlowReplacedWidthAndMargin(const ReplacedBox& replacedBox, const HorizontalConstraints& horizontalConstraints, const OverrideHorizontalValues& overrideHorizontalValues)
{
    ASSERT(replacedBox.isInFlow());

    // 10.3.4 Block-level, replaced elements in normal flow
    //
    // 1. The used value of 'width' is determined as for inline replaced elements.
    // 2. Then the rules for non-replaced block-level elements are applied to determine the margins.

    // #1
    auto usedWidth = inlineReplacedWidthAndMargin(replacedBox, horizontalConstraints, { }, overrideHorizontalValues).contentWidth;
    // #2
    auto nonReplacedWidthAndMargin = inFlowNonReplacedWidthAndMargin(replacedBox, horizontalConstraints, OverrideHorizontalValues { usedWidth, overrideHorizontalValues.margin });

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Width][Margin] -> inflow replaced -> width(" << usedWidth  << "px) margin(" << nonReplacedWidthAndMargin.usedMargin.start << "px, " << nonReplacedWidthAndMargin.usedMargin.end << "px) -> layoutBox(" << &replacedBox << ")");
    return { usedWidth, nonReplacedWidthAndMargin.usedMargin };
}

LayoutUnit BlockFormattingContext::Geometry::staticVerticalPosition(const Box& layoutBox, const VerticalConstraints& verticalConstraints) const
{
    // https://www.w3.org/TR/CSS22/visuren.html#block-formatting
    // In a block formatting context, boxes are laid out one after the other, vertically, beginning at the top of a containing block.
    // The vertical distance between two sibling boxes is determined by the 'margin' properties.
    // Vertical margins between adjacent block-level boxes in a block formatting context collapse.
    if (auto* previousInFlowSibling = layoutBox.previousInFlowSibling()) {
        auto& previousInFlowBoxGeometry = formattingContext().geometryForBox(*previousInFlowSibling);
        return previousInFlowBoxGeometry.bottom() + previousInFlowBoxGeometry.marginAfter();
    }
    return verticalConstraints.logicalTop;
}

LayoutUnit BlockFormattingContext::Geometry::staticHorizontalPosition(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints) const
{
    // https://www.w3.org/TR/CSS22/visuren.html#block-formatting
    // In a block formatting context, each box's left outer edge touches the left edge of the containing block (for right-to-left formatting, right edges touch).
    return horizontalConstraints.logicalLeft + formattingContext().geometryForBox(layoutBox).marginStart();
}

Point BlockFormattingContext::Geometry::staticPosition(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints, const VerticalConstraints& verticalConstraints) const
{
    return { staticHorizontalPosition(layoutBox, horizontalConstraints), staticVerticalPosition(layoutBox, verticalConstraints) };
}

ContentHeightAndMargin BlockFormattingContext::Geometry::inFlowHeightAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints, const OverrideVerticalValues& overrideVerticalValues)
{
    ASSERT(layoutBox.isInFlow());

    // 10.6.2 Inline replaced elements, block-level replaced elements in normal flow, 'inline-block'
    // replaced elements in normal flow and floating replaced elements
    if (layoutBox.isReplacedBox())
        return inlineReplacedHeightAndMargin(downcast<ReplacedBox>(layoutBox), horizontalConstraints, { }, overrideVerticalValues);

    ContentHeightAndMargin contentHeightAndMargin;
    if (layoutBox.isOverflowVisible() && !layoutBox.isDocumentBox()) {
        // TODO: Figure out the case for the document element. Let's just complicated-case it for now.
        contentHeightAndMargin = inFlowNonReplacedHeightAndMargin(layoutBox, horizontalConstraints, overrideVerticalValues);
    } else {
        // 10.6.6 Complicated cases
        // Block-level, non-replaced elements in normal flow when 'overflow' does not compute to 'visible' (except if the 'overflow' property's value has been propagated to the viewport).
        contentHeightAndMargin = complicatedCases(layoutBox, horizontalConstraints, overrideVerticalValues);
    }

    auto quirks = formattingContext().quirks();
    if (!quirks.needsStretching(layoutBox))
        return contentHeightAndMargin;

    contentHeightAndMargin.contentHeight = quirks.stretchedInFlowHeight(layoutBox, contentHeightAndMargin);

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Height][Margin] -> inflow non-replaced -> streched to viewport -> height(" << contentHeightAndMargin.contentHeight << "px) margin(" << contentHeightAndMargin.nonCollapsedMargin.before << "px, " << contentHeightAndMargin.nonCollapsedMargin.after << "px) -> layoutBox(" << &layoutBox << ")");
    return contentHeightAndMargin;
}

ContentWidthAndMargin BlockFormattingContext::Geometry::inFlowWidthAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints, const OverrideHorizontalValues& overrideHorizontalValues)
{
    ASSERT(layoutBox.isInFlow());

    if (!layoutBox.isReplacedBox())
        return inFlowNonReplacedWidthAndMargin(layoutBox, horizontalConstraints, overrideHorizontalValues);
    return inFlowReplacedWidthAndMargin(downcast<ReplacedBox>(layoutBox), horizontalConstraints, overrideHorizontalValues);
}

ContentWidthAndMargin BlockFormattingContext::Geometry::computedWidthAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints, Optional<LayoutUnit> availableWidthFloatAvoider)
{
    auto compute = [&] (auto constraintsForWidth, Optional<LayoutUnit> usedWidth) {
        if (layoutBox.isFloatingPositioned())
            return floatingWidthAndMargin(layoutBox, constraintsForWidth, { usedWidth, { } });

        if (layoutBox.isInFlow())
            return inFlowWidthAndMargin(layoutBox, constraintsForWidth, { usedWidth, { } });

        ASSERT_NOT_REACHED();
        return ContentWidthAndMargin { };
    };

    auto horizontalConstraintsForWidth = horizontalConstraints;
    if (layoutBox.style().logicalWidth().isAuto() && availableWidthFloatAvoider) {
        // While the non-auto width values should all be resolved against the containing block's width, when
        // the width is auto the available horizontal space is shrunk by neighboring floats.
        horizontalConstraintsForWidth.logicalWidth = *availableWidthFloatAvoider;
    }
    auto contentWidthAndMargin = compute(horizontalConstraintsForWidth, { });
    auto availableWidth = horizontalConstraints.logicalWidth;
    if (auto maxWidth = computedMaxWidth(layoutBox, availableWidth)) {
        auto maxWidthAndMargin = compute(horizontalConstraints, maxWidth);
        if (contentWidthAndMargin.contentWidth > maxWidthAndMargin.contentWidth)
            contentWidthAndMargin = maxWidthAndMargin;
    }

    auto minWidth = computedMinWidth(layoutBox, availableWidth).valueOr(0);
    auto minWidthAndMargin = compute(horizontalConstraints, minWidth);
    if (contentWidthAndMargin.contentWidth < minWidthAndMargin.contentWidth)
        contentWidthAndMargin = minWidthAndMargin;
    return contentWidthAndMargin;
}

FormattingContext::IntrinsicWidthConstraints BlockFormattingContext::Geometry::intrinsicWidthConstraints(const Box& layoutBox)
{
    auto fixedMarginBorderAndPadding = [&](auto& layoutBox) {
        auto& style = layoutBox.style();
        return fixedValue(style.marginStart()).valueOr(0)
            + LayoutUnit { style.borderLeftWidth() }
            + fixedValue(style.paddingLeft()).valueOr(0)
            + fixedValue(style.paddingRight()).valueOr(0)
            + LayoutUnit { style.borderRightWidth() }
            + fixedValue(style.marginEnd()).valueOr(0);
    };

    auto computedIntrinsicWidthConstraints = [&]() -> IntrinsicWidthConstraints {
        auto logicalWidth = layoutBox.style().logicalWidth();
        // Minimum/maximum width can't be depending on the containing block's width.
        auto needsResolvedContainingBlockWidth = logicalWidth.isCalculated() || logicalWidth.isPercent() || logicalWidth.isRelative();
        if (needsResolvedContainingBlockWidth)
            return { };

        if (auto width = fixedValue(logicalWidth))
            return { *width, *width };

        if (layoutBox.isReplacedBox()) {
            auto& replacedBox = downcast<ReplacedBox>(layoutBox);
            if (replacedBox.hasIntrinsicWidth()) {
                auto replacedWidth = replacedBox.intrinsicWidth();
                return { replacedWidth, replacedWidth };
            }
            return { };
        }

        if (!is<ContainerBox>(layoutBox) || !downcast<ContainerBox>(layoutBox).hasInFlowOrFloatingChild())
            return { };

        if (layoutBox.establishesFormattingContext()) {
            auto intrinsicWidthConstraints = LayoutContext::createFormattingContext(downcast<ContainerBox>(layoutBox), layoutState())->computedIntrinsicWidthConstraints();
            if (logicalWidth.isMinContent())
                return { intrinsicWidthConstraints.minimum, intrinsicWidthConstraints.minimum };
            if (logicalWidth.isMaxContent())
                return { intrinsicWidthConstraints.maximum, intrinsicWidthConstraints.maximum };
            return intrinsicWidthConstraints;
        }

        auto intrinsicWidthConstraints = IntrinsicWidthConstraints { };
        auto& formattingState = layoutState().formattingStateForBox(layoutBox);
        for (auto& child : childrenOfType<Box>(downcast<ContainerBox>(layoutBox))) {
            if (child.isOutOfFlowPositioned())
                continue;
            auto childIntrinsicWidthConstraints = formattingState.intrinsicWidthConstraintsForBox(child);
            ASSERT(childIntrinsicWidthConstraints);

            intrinsicWidthConstraints.minimum = std::max(intrinsicWidthConstraints.minimum, childIntrinsicWidthConstraints->minimum);
            intrinsicWidthConstraints.maximum = std::max(intrinsicWidthConstraints.maximum, childIntrinsicWidthConstraints->maximum);
        }
        return intrinsicWidthConstraints;
    };
    // FIXME Check for box-sizing: border-box;
    auto intrinsicWidthConstraints = constrainByMinMaxWidth(layoutBox, computedIntrinsicWidthConstraints());
    intrinsicWidthConstraints.expand(fixedMarginBorderAndPadding(layoutBox));
    return intrinsicWidthConstraints;
}

}
}

#endif
