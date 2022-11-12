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
#include "FormattingGeometry.h"

#include "BlockFormattingState.h"
#include "FlexFormattingState.h"
#include "FloatingContext.h"
#include "FloatingState.h"
#include "FormattingQuirks.h"
#include "InlineFormattingState.h"
#include "LayoutContainingBlockChainIterator.h"
#include "LayoutContext.h"
#include "LayoutInitialContainingBlock.h"
#include "Logging.h"
#include "TableFormattingState.h"

namespace WebCore {
namespace Layout {

static inline bool isHeightAuto(const Box& layoutBox)
{
    // 10.5 Content height: the 'height' property
    //
    // The percentage is calculated with respect to the height of the generated box's containing block.
    // If the height of the containing block is not specified explicitly (i.e., it depends on content height),
    // and this element is not absolutely positioned, the used height is calculated as if 'auto' was specified.

    auto height = layoutBox.style().logicalHeight();
    if (height.isAuto())
        return true;

    if (height.isPercent()) {
        if (layoutBox.isOutOfFlowPositioned())
            return false;

        return !FormattingContext::containingBlock(layoutBox).style().logicalHeight().isFixed();
    }

    return false;
}

FormattingGeometry::FormattingGeometry(const FormattingContext& formattingContext)
    : m_formattingContext(formattingContext)
{
}

std::optional<LayoutUnit> FormattingGeometry::computedHeightValue(const Box& layoutBox, HeightType heightType, std::optional<LayoutUnit> containingBlockHeight) const
{
    auto& style = layoutBox.style();
    auto height = heightType == HeightType::Normal ? style.logicalHeight() : heightType == HeightType::Min ? style.logicalMinHeight() : style.logicalMaxHeight();
    if (height.isUndefined() || height.isAuto() || height.isMaxContent() || height.isMinContent() || height.isFitContent())
        return { };

    if (height.isFixed())
        return LayoutUnit { height.value() };

    if (!containingBlockHeight) {
        if (layoutState().inQuirksMode())
            containingBlockHeight = formattingContext().formattingQuirks().heightValueOfNearestContainingBlockWithFixedHeight(layoutBox);
        else {
            auto nonAnonymousContainingBlockLogicalHeight = [&]() -> Length {
                // When the block level box is a direct child of an inline level box (<span><div></div></span>) and we wrap it into a continuation,
                // the containing block (anonymous wrapper) is not the box we need to check for fixed height.
                for (auto& containingBlock : containingBlockChain(layoutBox)) {
                    if (containingBlock.isAnonymous())
                        continue;
                    return containingBlock.style().logicalHeight();
                }
                ASSERT_NOT_REACHED();
                return { };
            };
            containingBlockHeight = fixedValue(nonAnonymousContainingBlockLogicalHeight());
        }
    }

    if (!containingBlockHeight)
        return { };

    return valueForLength(height, *containingBlockHeight);
}

std::optional<LayoutUnit> FormattingGeometry::computedHeight(const Box& layoutBox, std::optional<LayoutUnit> containingBlockHeight) const
{
    if (auto height = computedHeightValue(layoutBox, HeightType::Normal, containingBlockHeight)) {
        if (layoutBox.style().boxSizing() == BoxSizing::ContentBox)
            return height;
        auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
        return *height - (boxGeometry.verticalBorder() + boxGeometry.verticalPadding().value_or(0));
    }
    return { };
}

std::optional<LayoutUnit> FormattingGeometry::computedWidthValue(const Box& layoutBox, WidthType widthType, LayoutUnit containingBlockWidth) const
{
    // Applies to: all elements except non-replaced inlines (out-of-flow check is required for positioned <br> as for some reason we don't blockify them).
    ASSERT(!layoutBox.isInlineBox() || layoutBox.isOutOfFlowPositioned());

    auto width = [&] {
        auto& style = layoutBox.style();
        switch (widthType) {
        case WidthType::Normal:
            return style.logicalWidth();
        case WidthType::Min:
            return style.logicalMinWidth();
        case WidthType::Max:
            return style.logicalMaxWidth();
        }
        ASSERT_NOT_REACHED();
        return style.logicalWidth();
    }();
    if (auto computedValue = this->computedValue(width, containingBlockWidth))
        return computedValue;

    if (width.isMinContent() || width.isMaxContent() || width.isFitContent()) {
        if (!is<ElementBox>(layoutBox))
            return { };
        auto& elementBox = downcast<ElementBox>(layoutBox);
        // FIXME: Consider splitting up computedIntrinsicWidthConstraints so that we could computed the min and max values separately.
        auto intrinsicWidthConstraints = [&] {
            if (!elementBox.hasInFlowOrFloatingChild())
                return IntrinsicWidthConstraints { 0_lu, containingBlockWidth };
            ASSERT(elementBox.establishesFormattingContext());
            auto& layoutState = this->layoutState();
            if (layoutState.hasFormattingState(elementBox)) {
                if (auto intrinsicWidthConstraints = layoutState.formattingStateForFormattingContext(elementBox).intrinsicWidthConstraints())
                    return *intrinsicWidthConstraints;
            }
            return LayoutContext::createFormattingContext(elementBox, const_cast<LayoutState&>(layoutState))->computedIntrinsicWidthConstraints();
        }();
        if (width.isMinContent())
            return intrinsicWidthConstraints.minimum;
        if (width.isMaxContent())
            return intrinsicWidthConstraints.maximum;
        ASSERT(width.isFitContent());
        // If the available space in a given axis is definite, equal to min(max-content size,
        // max(min-content size, stretch-fit size)). Otherwise, equal to the max-content size in that axis.
        // FIXME: We don't yet have indefinite available size.
        return std::min(intrinsicWidthConstraints.maximum, std::max(intrinsicWidthConstraints.minimum, containingBlockWidth));
    }
    return { };
}

std::optional<LayoutUnit> FormattingGeometry::computedWidth(const Box& layoutBox, LayoutUnit containingBlockWidth) const
{
    if (auto computedWidth = computedWidthValue(layoutBox, WidthType::Normal, containingBlockWidth)) {
        auto& style = layoutBox.style();
        // Non-quantitative values such as auto and min-content are not influenced by the box-sizing property.
        if (style.boxSizing() == BoxSizing::ContentBox || style.width().isIntrinsicOrAuto())
            return computedWidth;
        auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
        return *computedWidth - (boxGeometry.horizontalBorder() + boxGeometry.horizontalPadding().value_or(0));
    }
    return { };
}

LayoutUnit FormattingGeometry::contentHeightForFormattingContextRoot(const ElementBox& formattingContextRoot) const
{
    ASSERT(formattingContextRoot.establishesFormattingContext());
    ASSERT(isHeightAuto(formattingContextRoot) || formattingContextRoot.establishesTableFormattingContext() || formattingContextRoot.isTableCell());
    auto usedContentHeight = LayoutUnit { }; 
    auto hasContent = formattingContextRoot.hasInFlowOrFloatingChild();
    // The used height of the containment box is determined as if performing a normal layout of the box, except that it is treated as having no content.
    auto shouldIgnoreContent = formattingContextRoot.isSizeContainmentBox();
    if (hasContent && !shouldIgnoreContent)
        usedContentHeight = LayoutContext::createFormattingContext(formattingContextRoot, const_cast<LayoutState&>(layoutState()))->usedContentHeight();
    return usedContentHeight;
}

std::optional<LayoutUnit> FormattingGeometry::computedValue(const Length& geometryProperty, LayoutUnit containingBlockWidth) const
{
    //  In general, the computed value resolves the specified value as far as possible without laying out the content.
    if (geometryProperty.isFixed() || geometryProperty.isPercent() || geometryProperty.isCalculated())
        return valueForLength(geometryProperty, containingBlockWidth);
    return { };
}

std::optional<LayoutUnit> FormattingGeometry::fixedValue(const Length& geometryProperty) const
{
    if (!geometryProperty.isFixed())
        return { };
    return LayoutUnit { geometryProperty.value() };
}

// https://www.w3.org/TR/CSS22/visudet.html#min-max-heights
// Specifies a percentage for determining the used value. The percentage is calculated with respect to the height of the generated box's containing block.
// If the height of the containing block is not specified explicitly (i.e., it depends on content height), and this element is not absolutely positioned,
// the percentage value is treated as '0' (for 'min-height') or 'none' (for 'max-height').
std::optional<LayoutUnit> FormattingGeometry::computedMaxHeight(const Box& layoutBox, std::optional<LayoutUnit> containingBlockHeight) const
{
    return computedHeightValue(layoutBox, HeightType::Max, containingBlockHeight);
}

std::optional<LayoutUnit> FormattingGeometry::computedMinHeight(const Box& layoutBox, std::optional<LayoutUnit> containingBlockHeight) const
{
    return computedHeightValue(layoutBox, HeightType::Min, containingBlockHeight);
}

std::optional<LayoutUnit> FormattingGeometry::computedMinWidth(const Box& layoutBox, LayoutUnit containingBlockWidth) const
{
    return computedWidthValue(layoutBox, WidthType::Min, containingBlockWidth);
}

std::optional<LayoutUnit> FormattingGeometry::computedMaxWidth(const Box& layoutBox, LayoutUnit containingBlockWidth) const
{
    return computedWidthValue(layoutBox, WidthType::Max, containingBlockWidth);
}

LayoutUnit FormattingGeometry::staticVerticalPositionForOutOfFlowPositioned(const Box& layoutBox, const VerticalConstraints& verticalConstraints) const
{
    ASSERT(layoutBox.isOutOfFlowPositioned());

    // For the purposes of this section and the next, the term "static position" (of an element) refers, roughly, to the position an element would have
    // had in the normal flow. More precisely, the static position for 'top' is the distance from the top edge of the containing block to the top margin
    // edge of a hypothetical box that would have been the first box of the element if its specified 'position' value had been 'static' and its specified
    // 'float' had been 'none' and its specified 'clear' had been 'none'. (Note that due to the rules in section 9.7 this might require also assuming a different
    // computed value for 'display'.) The value is negative if the hypothetical box is above the containing block.

    // Start with this box's border box offset from the parent's border box.
    auto& formattingContext = this->formattingContext();
    auto top = LayoutUnit { };
    if (layoutBox.previousInFlowSibling() && layoutBox.previousInFlowSibling()->isBlockLevelBox()) {
        // Add sibling offset
        auto& previousInFlowSibling = *layoutBox.previousInFlowSibling();
        auto& previousInFlowBoxGeometry = formattingContext.geometryForBox(previousInFlowSibling, FormattingContext::EscapeReason::OutOfFlowBoxNeedsInFlowGeometry);
        auto usedVerticalMarginForPreviousBox = downcast<BlockFormattingState>(formattingContext.formattingState()).usedVerticalMargin(previousInFlowSibling);

        top += BoxGeometry::borderBoxRect(previousInFlowBoxGeometry).bottom() + usedVerticalMarginForPreviousBox.nonCollapsedValues.after;
    } else
        top = formattingContext.geometryForBox(layoutBox.parent(), FormattingContext::EscapeReason::OutOfFlowBoxNeedsInFlowGeometry).contentBoxTop();

    // Resolve top all the way up to the containing block.
    auto& containingBlock = FormattingContext::containingBlock(layoutBox);
    // Start with the parent since we pretend that this box is normal flow.
    for (auto* ancestor = &layoutBox.parent(); ancestor != &containingBlock; ancestor = &FormattingContext::containingBlock(*ancestor)) {
        auto& boxGeometry = formattingContext.geometryForBox(*ancestor, FormattingContext::EscapeReason::OutOfFlowBoxNeedsInFlowGeometry);
        // BoxGeometry::top is the border box top position in its containing block's coordinate system.
        top += BoxGeometry::borderBoxTop(boxGeometry);
        ASSERT(!ancestor->isPositioned() || layoutBox.isFixedPositioned());
    }
    // Move the static position relative to the padding box. This is very specific to abolutely positioned boxes.
    return top - verticalConstraints.logicalTop;
}

LayoutUnit FormattingGeometry::staticHorizontalPositionForOutOfFlowPositioned(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints) const
{
    ASSERT(layoutBox.isOutOfFlowPositioned());
    // See staticVerticalPositionForOutOfFlowPositioned for the definition of the static position.

    // Start with this box's border box offset from the parent's border box.
    auto& formattingContext = this->formattingContext();
    auto left = formattingContext.geometryForBox(layoutBox.parent(), FormattingContext::EscapeReason::OutOfFlowBoxNeedsInFlowGeometry).contentBoxLeft();

    // Resolve left all the way up to the containing block.
    auto& containingBlock = FormattingContext::containingBlock(layoutBox);
    // Start with the parent since we pretend that this box is normal flow.
    for (auto* ancestor = &layoutBox.parent(); ancestor != &containingBlock; ancestor = &FormattingContext::containingBlock(*ancestor)) {
        auto& boxGeometry = formattingContext.geometryForBox(*ancestor, FormattingContext::EscapeReason::OutOfFlowBoxNeedsInFlowGeometry);
        // BoxGeometry::left is the border box left position in its containing block's coordinate system.
        left += BoxGeometry::borderBoxLeft(boxGeometry);
        ASSERT(!ancestor->isPositioned() || layoutBox.isFixedPositioned());
    }
    // Move the static position relative to the padding box. This is very specific to abolutely positioned boxes.
    return left - horizontalConstraints.logicalLeft;
}

LayoutUnit FormattingGeometry::shrinkToFitWidth(const Box& formattingContextRoot, LayoutUnit availableWidth) const
{
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Width] -> shrink to fit -> unsupported -> width(" << LayoutUnit { } << "px) layoutBox: " << &formattingContextRoot << ")");
    ASSERT(formattingContextRoot.establishesFormattingContext());

    // Calculation of the shrink-to-fit width is similar to calculating the width of a table cell using the automatic table layout algorithm.
    // Roughly: calculate the preferred width by formatting the content without breaking lines other than where explicit line breaks occur,
    // and also calculate the preferred minimum width, e.g., by trying all possible line breaks. CSS 2.2 does not define the exact algorithm.
    // Thirdly, find the available width: in this case, this is the width of the containing block minus the used values of 'margin-left', 'border-left-width',
    // 'padding-left', 'padding-right', 'border-right-width', 'margin-right', and the widths of any relevant scroll bars.

    // Then the shrink-to-fit width is: min(max(preferred minimum width, available width), preferred width).
    auto hasContent = is<ElementBox>(formattingContextRoot) && downcast<ElementBox>(formattingContextRoot).hasInFlowOrFloatingChild();
    // The used width of the containment box is determined as if performing a normal layout of the box, except that it is treated as having no content.
    auto shouldIgnoreContent = formattingContextRoot.isSizeContainmentBox();
    if (!hasContent || shouldIgnoreContent)
        return { };

    auto computedIntrinsicWidthConstraints = [&] {
        auto& layoutState = this->layoutState();
        auto& root = downcast<ElementBox>(formattingContextRoot);
        if (layoutState.hasFormattingState(root)) {
            if (auto intrinsicWidthConstraints = layoutState.formattingStateForFormattingContext(root).intrinsicWidthConstraints())
                return *intrinsicWidthConstraints;
        }
        return LayoutContext::createFormattingContext(root, const_cast<LayoutState&>(layoutState))->computedIntrinsicWidthConstraints();
    }();
    return std::min(std::max(computedIntrinsicWidthConstraints.minimum, availableWidth), computedIntrinsicWidthConstraints.maximum);
}

VerticalGeometry FormattingGeometry::outOfFlowNonReplacedVerticalGeometry(const ElementBox& layoutBox, const HorizontalConstraints& horizontalConstraints, const VerticalConstraints& verticalConstraints, const OverriddenVerticalValues& overriddenVerticalValues) const
{
    ASSERT(layoutBox.isOutOfFlowPositioned() && !layoutBox.isReplacedBox());

    // 10.6.4 Absolutely positioned, non-replaced elements
    //
    // For absolutely positioned elements, the used values of the vertical dimensions must satisfy this constraint:
    // 'top' + 'margin-top' + 'border-top-width' + 'padding-top' + 'height' + 'padding-bottom' + 'border-bottom-width' + 'margin-bottom' + 'bottom'
    // = height of containing block

    // If all three of 'top', 'height', and 'bottom' are auto, set 'top' to the static position and apply rule number three below.

    // If none of the three are 'auto': If both 'margin-top' and 'margin-bottom' are 'auto', solve the equation under the extra
    // constraint that the two margins get equal values. If one of 'margin-top' or 'margin-bottom' is 'auto', solve the equation for that value.
    // If the values are over-constrained, ignore the value for 'bottom' and solve for that value.

    // Otherwise, pick the one of the following six rules that applies.

    // 1. 'top' and 'height' are 'auto' and 'bottom' is not 'auto', then the height is based on the content per 10.6.7,
    //     set 'auto' values for 'margin-top' and 'margin-bottom' to 0, and solve for 'top'
    // 2. 'top' and 'bottom' are 'auto' and 'height' is not 'auto', then set 'top' to the static position, set 'auto' values for
    //    'margin-top' and 'margin-bottom' to 0, and solve for 'bottom'
    // 3. 'height' and 'bottom' are 'auto' and 'top' is not 'auto', then the height is based on the content per 10.6.7, set 'auto'
    //     values for 'margin-top' and 'margin-bottom' to 0, and solve for 'bottom'
    // 4. 'top' is 'auto', 'height' and 'bottom' are not 'auto', then set 'auto' values for 'margin-top' and 'margin-bottom' to 0, and solve for 'top'
    // 5. 'height' is 'auto', 'top' and 'bottom' are not 'auto', then 'auto' values for 'margin-top' and 'margin-bottom' are set to 0 and solve for 'height'
    // 6. 'bottom' is 'auto', 'top' and 'height' are not 'auto', then set 'auto' values for 'margin-top' and 'margin-bottom' to 0 and solve for 'bottom'

    auto& formattingContext = this->formattingContext();
    auto& style = layoutBox.style();
    auto& boxGeometry = formattingContext.geometryForBox(layoutBox);
    auto containingBlockHeight = verticalConstraints.logicalHeight;
    auto containingBlockWidth = horizontalConstraints.logicalWidth;

    auto top = computedValue(style.logicalTop(), containingBlockWidth);
    auto bottom = computedValue(style.logicalBottom(), containingBlockWidth);
    auto height = overriddenVerticalValues.height ? overriddenVerticalValues.height.value() : computedHeight(layoutBox, containingBlockHeight);
    auto computedVerticalMargin = FormattingGeometry::computedVerticalMargin(layoutBox, horizontalConstraints);
    UsedVerticalMargin::NonCollapsedValues usedVerticalMargin; 
    auto paddingTop = boxGeometry.paddingBefore().value_or(0);
    auto paddingBottom = boxGeometry.paddingAfter().value_or(0);
    auto borderTop = boxGeometry.borderBefore();
    auto borderBottom = boxGeometry.borderAfter();

    if (!top && !height && !bottom)
        top = staticVerticalPositionForOutOfFlowPositioned(layoutBox, verticalConstraints);

    if (top && height && bottom) {
        if (!computedVerticalMargin.before && !computedVerticalMargin.after) {
            auto marginBeforeAndAfter = containingBlockHeight - (*top + borderTop + paddingTop + *height + paddingBottom + borderBottom + *bottom);
            usedVerticalMargin = { marginBeforeAndAfter / 2, marginBeforeAndAfter / 2 };
        } else if (!computedVerticalMargin.before) {
            usedVerticalMargin.after = *computedVerticalMargin.after;
            usedVerticalMargin.before = containingBlockHeight - (*top + borderTop + paddingTop + *height + paddingBottom + borderBottom + usedVerticalMargin.after + *bottom);
        } else if (!computedVerticalMargin.after) {
            usedVerticalMargin.before = *computedVerticalMargin.before;
            usedVerticalMargin.after = containingBlockHeight - (*top + usedVerticalMargin.before + borderTop + paddingTop + *height + paddingBottom + borderBottom + *bottom);
        } else
            usedVerticalMargin = { *computedVerticalMargin.before, *computedVerticalMargin.after };
        // Over-constrained?
        auto boxHeight = *top + usedVerticalMargin.before + borderTop + paddingTop + *height + paddingBottom + borderBottom + usedVerticalMargin.after + *bottom;
        if (boxHeight != containingBlockHeight)
            bottom = containingBlockHeight - (*top + usedVerticalMargin.before + borderTop + paddingTop + *height + paddingBottom + borderBottom + usedVerticalMargin.after);
    }

    if (!top && !height && bottom) {
        // #1
        height = contentHeightForFormattingContextRoot(layoutBox);
        usedVerticalMargin = { computedVerticalMargin.before.value_or(0), computedVerticalMargin.after.value_or(0) };
        top = containingBlockHeight - (usedVerticalMargin.before + borderTop + paddingTop + *height + paddingBottom + borderBottom + usedVerticalMargin.after + *bottom); 
    }

    if (!top && !bottom && height) {
        // #2
        top = staticVerticalPositionForOutOfFlowPositioned(layoutBox, verticalConstraints);
        usedVerticalMargin = { computedVerticalMargin.before.value_or(0), computedVerticalMargin.after.value_or(0) };
        bottom = containingBlockHeight - (*top + usedVerticalMargin.before + borderTop + paddingTop + *height + paddingBottom + borderBottom + usedVerticalMargin.after);
    }

    if (!height && !bottom && top) {
        // #3
        height = contentHeightForFormattingContextRoot(layoutBox);
        usedVerticalMargin = { computedVerticalMargin.before.value_or(0), computedVerticalMargin.after.value_or(0) };
        bottom = containingBlockHeight - (*top + usedVerticalMargin.before + borderTop + paddingTop + *height + paddingBottom + borderBottom + usedVerticalMargin.after);
    }

    if (!top && height && bottom) {
        // #4
        usedVerticalMargin = { computedVerticalMargin.before.value_or(0), computedVerticalMargin.after.value_or(0) };
        top = containingBlockHeight - (usedVerticalMargin.before + borderTop + paddingTop + *height + paddingBottom + borderBottom + usedVerticalMargin.after + *bottom);
    }

    if (!height && top && bottom) {
        // #5
        usedVerticalMargin = { computedVerticalMargin.before.value_or(0), computedVerticalMargin.after.value_or(0) };
        height = containingBlockHeight - (*top + usedVerticalMargin.before + borderTop + paddingTop + paddingBottom + borderBottom + usedVerticalMargin.after + *bottom);
    }

    if (!bottom && top && height) {
        // #6
        usedVerticalMargin = { computedVerticalMargin.before.value_or(0), computedVerticalMargin.after.value_or(0) };
        bottom = containingBlockHeight - (*top + usedVerticalMargin.before + borderTop + paddingTop + *height + paddingBottom + borderBottom + usedVerticalMargin.after);
    }

    ASSERT(top);
    ASSERT(bottom);
    ASSERT(height);

    // For out-of-flow elements the containing block is formed by the padding edge of the ancestor.
    // At this point the positioned value is in the coordinate system of the padding box. Let's convert it to border box coordinate system.
    auto containingBlockPaddingVerticalEdge = verticalConstraints.logicalTop;
    *top += containingBlockPaddingVerticalEdge;
    *bottom += containingBlockPaddingVerticalEdge;

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Position][Height][Margin] -> out-of-flow non-replaced -> top(" << *top << "px) bottom("  << *bottom << "px) height(" << *height << "px) margin(" << usedVerticalMargin.before << "px, "  << usedVerticalMargin.after << "px) layoutBox(" << &layoutBox << ")");
    return { *top, *bottom, { *height, usedVerticalMargin } };
}

HorizontalGeometry FormattingGeometry::outOfFlowNonReplacedHorizontalGeometry(const ElementBox& layoutBox, const HorizontalConstraints& horizontalConstraints, const OverriddenHorizontalValues& overriddenHorizontalValues) const
{
    ASSERT(layoutBox.isOutOfFlowPositioned() && !layoutBox.isReplacedBox());
    
    // 10.3.7 Absolutely positioned, non-replaced elements
    //
    // 'left' + 'margin-left' + 'border-left-width' + 'padding-left' + 'width' + 'padding-right' + 'border-right-width' + 'margin-right' + 'right'
    // = width of containing block

    // If all three of 'left', 'width', and 'right' are 'auto': First set any 'auto' values for 'margin-left' and 'margin-right' to 0.
    // Then, if the 'direction' property of the element establishing the static-position containing block is 'ltr' set 'left' to the static
    // position and apply rule number three below; otherwise, set 'right' to the static position and apply rule number one below.
    //
    // If none of the three is 'auto': If both 'margin-left' and 'margin-right' are 'auto', solve the equation under the extra constraint that the two margins get equal values,
    // unless this would make them negative, in which case when direction of the containing block is 'ltr' ('rtl'), set 'margin-left' ('margin-right') to zero and
    // solve for 'margin-right' ('margin-left'). If one of 'margin-left' or 'margin-right' is 'auto', solve the equation for that value.
    // If the values are over-constrained, ignore the value for 'left' (in case the 'direction' property of the containing block is 'rtl') or 'right'
    // (in case 'direction' is 'ltr') and solve for that value.
    //
    // Otherwise, set 'auto' values for 'margin-left' and 'margin-right' to 0, and pick the one of the following six rules that applies.
    //
    // 1. 'left' and 'width' are 'auto' and 'right' is not 'auto', then the width is shrink-to-fit. Then solve for 'left'
    // 2. 'left' and 'right' are 'auto' and 'width' is not 'auto', then if the 'direction' property of the element establishing the static-position 
    //    containing block is 'ltr' set 'left' to the static position, otherwise set 'right' to the static position.
    //    Then solve for 'left' (if 'direction is 'rtl') or 'right' (if 'direction' is 'ltr').
    // 3. 'width' and 'right' are 'auto' and 'left' is not 'auto', then the width is shrink-to-fit . Then solve for 'right'
    // 4. 'left' is 'auto', 'width' and 'right' are not 'auto', then solve for 'left'
    // 5. 'width' is 'auto', 'left' and 'right' are not 'auto', then solve for 'width'
    // 6. 'right' is 'auto', 'left' and 'width' are not 'auto', then solve for 'right'

    auto& formattingContext = this->formattingContext();
    auto& style = layoutBox.style();
    auto& boxGeometry = formattingContext.geometryForBox(layoutBox);
    auto containingBlockWidth = horizontalConstraints.logicalWidth;
    auto isLeftToRightDirection = FormattingContext::containingBlock(layoutBox).style().isLeftToRightDirection();
    
    auto left = computedValue(style.logicalLeft(), containingBlockWidth);
    auto right = computedValue(style.logicalRight(), containingBlockWidth);
    auto width = overriddenHorizontalValues.width ? overriddenHorizontalValues.width : computedWidth(layoutBox, containingBlockWidth);
    auto computedHorizontalMargin = FormattingGeometry::computedHorizontalMargin(layoutBox, horizontalConstraints);
    UsedHorizontalMargin usedHorizontalMargin;
    auto paddingLeft = boxGeometry.paddingStart().value_or(0);
    auto paddingRight = boxGeometry.paddingEnd().value_or(0);
    auto borderLeft = boxGeometry.borderStart();
    auto borderRight = boxGeometry.borderEnd();
    if (!left && !width && !right) {
        // If all three of 'left', 'width', and 'right' are 'auto': First set any 'auto' values for 'margin-left' and 'margin-right' to 0.
        // Then, if the 'direction' property of the element establishing the static-position containing block is 'ltr' set 'left' to the static
        // position and apply rule number three below; otherwise, set 'right' to the static position and apply rule number one below.
        usedHorizontalMargin = { computedHorizontalMargin.start.value_or(0), computedHorizontalMargin.end.value_or(0) };

        auto staticHorizontalPosition = staticHorizontalPositionForOutOfFlowPositioned(layoutBox, horizontalConstraints);
        if (isLeftToRightDirection)
            left = staticHorizontalPosition;
        else
            right = staticHorizontalPosition;
    } else if (left && width && right) {
        // If none of the three is 'auto': If both 'margin-left' and 'margin-right' are 'auto', solve the equation under the extra constraint that the two margins get equal values,
        // unless this would make them negative, in which case when direction of the containing block is 'ltr' ('rtl'), set 'margin-left' ('margin-right') to zero and
        // solve for 'margin-right' ('margin-left'). If one of 'margin-left' or 'margin-right' is 'auto', solve the equation for that value.
        // If the values are over-constrained, ignore the value for 'left' (in case the 'direction' property of the containing block is 'rtl') or 'right'
        // (in case 'direction' is 'ltr') and solve for that value.
        if (!computedHorizontalMargin.start && !computedHorizontalMargin.end) {
            auto marginStartAndEnd = containingBlockWidth - (*left + borderLeft + paddingLeft + *width + paddingRight + borderRight + *right);
            if (marginStartAndEnd >= 0)
                usedHorizontalMargin = { marginStartAndEnd / 2, marginStartAndEnd / 2 };
            else {
                if (isLeftToRightDirection) {
                    usedHorizontalMargin.start = 0_lu;
                    usedHorizontalMargin.end = containingBlockWidth - (*left + usedHorizontalMargin.start + borderLeft + paddingLeft + *width + paddingRight + borderRight + *right);
                } else {
                    usedHorizontalMargin.end = 0_lu;
                    usedHorizontalMargin.start = containingBlockWidth - (*left + borderLeft + paddingLeft + *width + paddingRight + borderRight + usedHorizontalMargin.end + *right);
                }
            }
        } else if (!computedHorizontalMargin.start) {
            usedHorizontalMargin.end = *computedHorizontalMargin.end;
            usedHorizontalMargin.start = containingBlockWidth - (*left + borderLeft + paddingLeft + *width + paddingRight + borderRight + usedHorizontalMargin.end + *right);
        } else if (!computedHorizontalMargin.end) {
            usedHorizontalMargin.start = *computedHorizontalMargin.start;
            usedHorizontalMargin.end = containingBlockWidth - (*left + usedHorizontalMargin.start + borderLeft + paddingLeft + *width + paddingRight + borderRight + *right);
        } else {
            usedHorizontalMargin = { *computedHorizontalMargin.start, *computedHorizontalMargin.end };
            // Overconstrained? Ignore right (left).
            if (isLeftToRightDirection)
                right = containingBlockWidth - (usedHorizontalMargin.start + *left + borderLeft + paddingLeft + *width + paddingRight + borderRight + usedHorizontalMargin.end);
            else
                left = containingBlockWidth - (usedHorizontalMargin.start + borderLeft + paddingLeft + *width + paddingRight + borderRight + usedHorizontalMargin.end + *right);
        }
    } else {
        // Otherwise, set 'auto' values for 'margin-left' and 'margin-right' to 0, and pick the one of the following six rules that applies.
        usedHorizontalMargin = { computedHorizontalMargin.start.value_or(0), computedHorizontalMargin.end.value_or(0) };
    }

    if (!left && !width && right) {
        // #1
        // Calculate the available width by solving for 'width' after setting 'left' (in case 1) to 0
        left = LayoutUnit { 0 };
        auto availableWidth = containingBlockWidth - (*left + usedHorizontalMargin.start + borderLeft + paddingLeft + paddingRight + borderRight + usedHorizontalMargin.end + *right);
        width = shrinkToFitWidth(layoutBox, availableWidth);
        left = containingBlockWidth - (usedHorizontalMargin.start + borderLeft + paddingLeft + *width + paddingRight  + borderRight + usedHorizontalMargin.end + *right);
    } else if (!left && !right && width) {
        // #2
        auto staticHorizontalPosition = staticHorizontalPositionForOutOfFlowPositioned(layoutBox, horizontalConstraints);
        if (isLeftToRightDirection) {
            left = staticHorizontalPosition;
            right = containingBlockWidth - (*left + usedHorizontalMargin.start + borderLeft + paddingLeft + *width + paddingRight + borderRight + usedHorizontalMargin.end);
        } else {
            right = staticHorizontalPosition;
            left = containingBlockWidth - (usedHorizontalMargin.start + borderLeft + paddingLeft + *width + paddingRight + borderRight + usedHorizontalMargin.end + *right);
        }
    } else if (!width && !right && left) {
        // #3
        // Calculate the available width by solving for 'width' after setting 'right' (in case 3) to 0
        right = LayoutUnit { 0 };
        auto availableWidth = containingBlockWidth - (*left + usedHorizontalMargin.start + borderLeft + paddingLeft + paddingRight + borderRight + usedHorizontalMargin.end + *right);
        width = shrinkToFitWidth(layoutBox, availableWidth);
        right = containingBlockWidth - (*left + usedHorizontalMargin.start + borderLeft + paddingLeft + *width + paddingRight + borderRight + usedHorizontalMargin.end);
    } else if (!left && width && right) {
        // #4
        left = containingBlockWidth - (usedHorizontalMargin.start + borderLeft + paddingLeft + *width + paddingRight + borderRight + usedHorizontalMargin.end + *right);
    } else if (!width && left && right) {
        // #5
        width = containingBlockWidth - (*left + usedHorizontalMargin.start + borderLeft + paddingLeft + paddingRight + borderRight + usedHorizontalMargin.end + *right);
    } else if (!right && left && width) {
        // #6
        right = containingBlockWidth - (*left + usedHorizontalMargin.start + borderLeft + paddingLeft + *width + paddingRight + borderRight + usedHorizontalMargin.end);
    }

    ASSERT(left);
    ASSERT(right);
    ASSERT(width);

    // For out-of-flow elements the containing block is formed by the padding edge of the ancestor.
    // At this point the positioned value is in the coordinate system of the padding box. Let's convert it to border box coordinate system.
    auto containingBlockPaddingVerticalEdge = horizontalConstraints.logicalLeft;
    *left += containingBlockPaddingVerticalEdge;
    *right += containingBlockPaddingVerticalEdge;

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Position][Width][Margin] -> out-of-flow non-replaced -> left(" << *left << "px) right("  << *right << "px) width(" << *width << "px) margin(" << usedHorizontalMargin.start << "px, "  << usedHorizontalMargin.end << "px) layoutBox(" << &layoutBox << ")");
    return { *left, *right, { *width, usedHorizontalMargin } };
}

VerticalGeometry FormattingGeometry::outOfFlowReplacedVerticalGeometry(const ElementBox& replacedBox, const HorizontalConstraints& horizontalConstraints, const VerticalConstraints& verticalConstraints, const OverriddenVerticalValues& overriddenVerticalValues) const
{
    ASSERT(replacedBox.isOutOfFlowPositioned());

    // 10.6.5 Absolutely positioned, replaced elements
    //
    // The used value of 'height' is determined as for inline replaced elements.
    // If 'margin-top' or 'margin-bottom' is specified as 'auto' its used value is determined by the rules below.
    // 1. If both 'top' and 'bottom' have the value 'auto', replace 'top' with the element's static position.
    // 2. If 'bottom' is 'auto', replace any 'auto' on 'margin-top' or 'margin-bottom' with '0'.
    // 3. If at this point both 'margin-top' and 'margin-bottom' are still 'auto', solve the equation under the extra constraint that the two margins must get equal values.
    // 4. If at this point there is only one 'auto' left, solve the equation for that value.
    // 5. If at this point the values are over-constrained, ignore the value for 'bottom' and solve for that value.

    auto& formattingContext = this->formattingContext();
    auto& style = replacedBox.style();
    auto& boxGeometry = formattingContext.geometryForBox(replacedBox);
    auto containingBlockHeight = verticalConstraints.logicalHeight;
    auto containingBlockWidth = horizontalConstraints.logicalWidth;

    auto top = computedValue(style.logicalTop(), containingBlockWidth);
    auto bottom = computedValue(style.logicalBottom(), containingBlockWidth);
    auto height = inlineReplacedContentHeightAndMargin(replacedBox, horizontalConstraints, verticalConstraints, overriddenVerticalValues).contentHeight;
    auto computedVerticalMargin = FormattingGeometry::computedVerticalMargin(replacedBox, horizontalConstraints);
    std::optional<LayoutUnit> usedMarginBefore = computedVerticalMargin.before;
    std::optional<LayoutUnit> usedMarginAfter = computedVerticalMargin.after;
    auto paddingTop = boxGeometry.paddingBefore().value_or(0);
    auto paddingBottom = boxGeometry.paddingAfter().value_or(0);
    auto borderTop = boxGeometry.borderBefore();
    auto borderBottom = boxGeometry.borderAfter();

    if (!top && !bottom) {
        // #1
        top = staticVerticalPositionForOutOfFlowPositioned(replacedBox, verticalConstraints);
    }

    if (!bottom) {
        // #2
        usedMarginBefore = computedVerticalMargin.before.value_or(0);
        usedMarginAfter = usedMarginBefore;
    }

    if (!usedMarginBefore && !usedMarginAfter) {
        // #3
        auto marginBeforeAndAfter = containingBlockHeight - (*top + borderTop + paddingTop + height + paddingBottom + borderBottom + *bottom);
        usedMarginBefore = marginBeforeAndAfter / 2;
        usedMarginAfter = usedMarginBefore;
    }

    // #4
    if (!top)
        top = containingBlockHeight - (*usedMarginBefore + borderTop + paddingTop + height + paddingBottom + borderBottom + *usedMarginAfter + *bottom);

    if (!bottom)
        bottom = containingBlockHeight - (*top + *usedMarginBefore + borderTop + paddingTop + height + paddingBottom + borderBottom + *usedMarginAfter);

    if (!usedMarginBefore)
        usedMarginBefore = containingBlockHeight - (*top + borderTop + paddingTop + height + paddingBottom + borderBottom + *usedMarginAfter + *bottom);

    if (!usedMarginAfter)
        usedMarginAfter = containingBlockHeight - (*top + *usedMarginBefore + borderTop + paddingTop + height + paddingBottom + borderBottom + *bottom);

    // #5
    auto boxHeight = *top + *usedMarginBefore + borderTop + paddingTop + height + paddingBottom + borderBottom + *usedMarginAfter + *bottom;
    if (boxHeight > containingBlockHeight)
        bottom = containingBlockHeight - (*top + *usedMarginBefore + borderTop + paddingTop + height + paddingBottom + borderBottom + *usedMarginAfter); 

    // For out-of-flow elements the containing block is formed by the padding edge of the ancestor.
    // At this point the positioned value is in the coordinate system of the padding box. Let's convert it to border box coordinate system.
    auto containingBlockPaddingVerticalEdge = verticalConstraints.logicalTop;
    *top += containingBlockPaddingVerticalEdge;
    *bottom += containingBlockPaddingVerticalEdge;

    ASSERT(top);
    ASSERT(bottom);
    ASSERT(usedMarginBefore);
    ASSERT(usedMarginAfter);
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Position][Height][Margin] -> out-of-flow replaced -> top(" << *top << "px) bottom("  << *bottom << "px) height(" << height << "px) margin(" << *usedMarginBefore << "px, "  << *usedMarginAfter << "px) layoutBox(" << &replacedBox << ")");
    return { *top, *bottom, { height, { *usedMarginBefore, *usedMarginAfter } } };
}

HorizontalGeometry FormattingGeometry::outOfFlowReplacedHorizontalGeometry(const ElementBox& replacedBox, const HorizontalConstraints& horizontalConstraints, const VerticalConstraints& verticalConstraints, const OverriddenHorizontalValues& overriddenHorizontalValues) const
{
    ASSERT(replacedBox.isOutOfFlowPositioned());

    // 10.3.8 Absolutely positioned, replaced elements
    // In this case, section 10.3.7 applies up through and including the constraint equation, but the rest of section 10.3.7 is replaced by the following rules:
    //
    // The used value of 'width' is determined as for inline replaced elements. If 'margin-left' or 'margin-right' is specified as 'auto' its used value is determined by the rules below.
    // 1. If both 'left' and 'right' have the value 'auto', then if the 'direction' property of the element establishing the static-position containing block is 'ltr',
    //   set 'left' to the static position; else if 'direction' is 'rtl', set 'right' to the static position.
    // 2. If 'left' or 'right' are 'auto', replace any 'auto' on 'margin-left' or 'margin-right' with '0'.
    // 3. If at this point both 'margin-left' and 'margin-right' are still 'auto', solve the equation under the extra constraint that the two margins must get equal values,
    //   unless this would make them negative, in which case when the direction of the containing block is 'ltr' ('rtl'), set 'margin-left' ('margin-right') to zero and
    //   solve for 'margin-right' ('margin-left').
    // 4. If at this point there is an 'auto' left, solve the equation for that value.
    // 5. If at this point the values are over-constrained, ignore the value for either 'left' (in case the 'direction' property of the containing block is 'rtl') or
    //   'right' (in case 'direction' is 'ltr') and solve for that value.

    auto& formattingContext = this->formattingContext();
    auto& style = replacedBox.style();
    auto& boxGeometry = formattingContext.geometryForBox(replacedBox);
    auto containingBlockWidth = horizontalConstraints.logicalWidth;
    auto isLeftToRightDirection = FormattingContext::containingBlock(replacedBox).style().isLeftToRightDirection();

    auto left = computedValue(style.logicalLeft(), containingBlockWidth);
    auto right = computedValue(style.logicalRight(), containingBlockWidth);
    auto computedHorizontalMargin = FormattingGeometry::computedHorizontalMargin(replacedBox, horizontalConstraints);
    std::optional<LayoutUnit> usedMarginStart = computedHorizontalMargin.start;
    std::optional<LayoutUnit> usedMarginEnd = computedHorizontalMargin.end;
    auto width = inlineReplacedContentWidthAndMargin(replacedBox, horizontalConstraints, verticalConstraints, overriddenHorizontalValues).contentWidth;
    auto paddingLeft = boxGeometry.paddingStart().value_or(0);
    auto paddingRight = boxGeometry.paddingEnd().value_or(0);
    auto borderLeft = boxGeometry.borderStart();
    auto borderRight = boxGeometry.borderEnd();

    if (!left && !right) {
        // #1
        auto staticHorizontalPosition = staticHorizontalPositionForOutOfFlowPositioned(replacedBox, horizontalConstraints);
        if (isLeftToRightDirection)
            left = staticHorizontalPosition;
        else
            right = staticHorizontalPosition;
    }

    if (!left || !right) {
        // #2
        usedMarginStart = computedHorizontalMargin.start.value_or(0);
        usedMarginEnd = computedHorizontalMargin.end.value_or(0);
    }

    if (!usedMarginStart && !usedMarginEnd) {
        // #3
        auto marginStartAndEnd = containingBlockWidth - (*left + borderLeft + paddingLeft + width + paddingRight + borderRight + *right);
        if (marginStartAndEnd >= 0) {
            usedMarginStart = marginStartAndEnd / 2;
            usedMarginEnd = usedMarginStart;
        } else {
            if (isLeftToRightDirection) {
                usedMarginStart = 0_lu;
                usedMarginEnd = containingBlockWidth - (*left + *usedMarginStart + borderLeft + paddingLeft + width + paddingRight + borderRight + *right);
            } else {
                usedMarginEnd = 0_lu;
                usedMarginStart = containingBlockWidth - (*left + borderLeft + paddingLeft + width + paddingRight + borderRight + *usedMarginEnd + *right);
            }
        }
    }

    // #4
    if (!left)
        left = containingBlockWidth - (*usedMarginStart + borderLeft + paddingLeft + width + paddingRight + borderRight + *usedMarginEnd + *right);

    if (!right)
        right = containingBlockWidth - (*left + *usedMarginStart + borderLeft + paddingLeft + width + paddingRight + borderRight + *usedMarginEnd);

    if (!usedMarginStart)
        usedMarginStart = containingBlockWidth - (*left + borderLeft + paddingLeft + width + paddingRight + borderRight + *usedMarginEnd + *right);

    if (!usedMarginEnd)
        usedMarginEnd = containingBlockWidth - (*left + *usedMarginStart + borderLeft + paddingLeft + width + paddingRight + borderRight + *right);

    auto boxWidth = (*left + *usedMarginStart + borderLeft + paddingLeft + width + paddingRight + borderRight + *usedMarginEnd + *right);
    if (boxWidth > containingBlockWidth) {
        // #5 Over-constrained?
        if (isLeftToRightDirection)
            right = containingBlockWidth - (*left + *usedMarginStart + borderLeft + paddingLeft + width + paddingRight + borderRight + *usedMarginEnd);
        else
            left = containingBlockWidth - (*usedMarginStart + borderLeft + paddingLeft + width + paddingRight + borderRight + *usedMarginEnd + *right);
    }

    ASSERT(left);
    ASSERT(right);
    ASSERT(usedMarginStart);
    ASSERT(usedMarginEnd);

    // For out-of-flow elements the containing block is formed by the padding edge of the ancestor.
    // At this point the positioned value is in the coordinate system of the padding box. Let's convert it to border box coordinate system.
    auto containingBlockPaddingVerticalEdge = horizontalConstraints.logicalLeft;
    *left += containingBlockPaddingVerticalEdge;
    *right += containingBlockPaddingVerticalEdge;

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Position][Width][Margin] -> out-of-flow replaced -> left(" << *left << "px) right("  << *right << "px) width(" << width << "px) margin(" << *usedMarginStart << "px, "  << *usedMarginEnd << "px) layoutBox(" << &replacedBox << ")");
    return { *left, *right, { width, { *usedMarginStart, *usedMarginEnd } } };
}

ContentHeightAndMargin FormattingGeometry::complicatedCases(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints, const OverriddenVerticalValues& overriddenVerticalValues) const
{
    ASSERT(!layoutBox.isReplacedBox());
    // TODO: Use complicated-case for document renderer for now (see BlockFormattingGeometry::inFlowHeightAndMargin).
    ASSERT((layoutBox.isBlockLevelBox() && layoutBox.isInFlow() && !layoutBox.isOverflowVisible()) || layoutBox.isInlineBlockBox() || layoutBox.isFloatingPositioned() || layoutBox.isDocumentBox() || layoutBox.isTableBox());

    // 10.6.6 Complicated cases
    //
    // Block-level, non-replaced elements in normal flow when 'overflow' does not compute to 'visible' (except if the 'overflow' property's value has been propagated to the viewport).
    // 'Inline-block', non-replaced elements.
    // Floating, non-replaced elements.
    //
    // 1. If 'margin-top', or 'margin-bottom' are 'auto', their used value is 0.
    // 2. If 'height' is 'auto', the height depends on the element's descendants per 10.6.7.

    auto height = overriddenVerticalValues.height ? overriddenVerticalValues.height.value() : computedHeight(layoutBox);
    auto computedVerticalMargin = FormattingGeometry::computedVerticalMargin(layoutBox, horizontalConstraints);
    // #1
    auto usedVerticalMargin = UsedVerticalMargin::NonCollapsedValues { computedVerticalMargin.before.value_or(0), computedVerticalMargin.after.value_or(0) }; 
    // #2
    if (!height) {
        ASSERT(isHeightAuto(layoutBox));
        if (!is<ElementBox>(layoutBox) || !downcast<ElementBox>(layoutBox).hasInFlowOrFloatingChild())
            height = 0_lu;
        else if (layoutBox.isDocumentBox() && !layoutBox.establishesFormattingContext()) {
            auto& documentBox = downcast<ElementBox>(layoutBox);
            auto top = BoxGeometry::marginBoxRect(formattingContext().geometryForBox(*documentBox.firstInFlowChild())).top();
            auto bottom = BoxGeometry::marginBoxRect(formattingContext().geometryForBox(*documentBox.lastInFlowChild())).bottom();
            // This is a special (quirk?) behavior since the document box is not a formatting context root and
            // all the float boxes end up at the ICB level.
            auto& initialContainingBlock = FormattingContext::initialContainingBlock(documentBox);
            auto floatingContext = FloatingContext { formattingContext(), downcast<BlockFormattingState>(layoutState().formattingStateForFormattingContext(initialContainingBlock)).floatingState() };
            if (auto floatBottom = floatingContext.bottom()) {
                bottom = std::max<LayoutUnit>(*floatBottom, bottom);
                auto floatTop = floatingContext.top();
                ASSERT(floatTop);
                top = std::min<LayoutUnit>(*floatTop, top);
            }
            height = bottom - top;
        } else {
            ASSERT(layoutBox.establishesFormattingContext());
            height = contentHeightForFormattingContextRoot(downcast<ElementBox>(layoutBox));
        }
    }

    ASSERT(height);

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Height][Margin] -> floating non-replaced -> height(" << *height << "px) margin(" << usedVerticalMargin.before << "px, " << usedVerticalMargin.after << "px) -> layoutBox(" << &layoutBox << ")");
    return ContentHeightAndMargin { *height, usedVerticalMargin };
}

ContentWidthAndMargin FormattingGeometry::floatingNonReplacedContentWidthAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints, const OverriddenHorizontalValues& overriddenHorizontalValues) const
{
    ASSERT(layoutBox.isFloatingPositioned() && !layoutBox.isReplacedBox());

    // 10.3.5 Floating, non-replaced elements
    //
    // 1. If 'margin-left', or 'margin-right' are computed as 'auto', their used value is '0'.
    // 2. If 'width' is computed as 'auto', the used value is the "shrink-to-fit" width.

    auto computedHorizontalMargin = FormattingGeometry::computedHorizontalMargin(layoutBox, horizontalConstraints);

    // #1
    auto usedHorizontallMargin = UsedHorizontalMargin { computedHorizontalMargin.start.value_or(0), computedHorizontalMargin.end.value_or(0) };
    // #2
    auto width = overriddenHorizontalValues.width ? overriddenHorizontalValues.width : computedWidth(layoutBox, horizontalConstraints.logicalWidth);
    if (!width)
        width = shrinkToFitWidth(layoutBox, horizontalConstraints.logicalWidth);

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Width][Margin] -> floating non-replaced -> width(" << *width << "px) margin(" << usedHorizontallMargin.start << "px, " << usedHorizontallMargin.end << "px) -> layoutBox(" << &layoutBox << ")");
    return ContentWidthAndMargin { *width, usedHorizontallMargin };
}

ContentHeightAndMargin FormattingGeometry::floatingReplacedContentHeightAndMargin(const ElementBox& replacedBox, const HorizontalConstraints& horizontalConstraints, const OverriddenVerticalValues& overriddenVerticalValues) const
{
    ASSERT(replacedBox.isFloatingPositioned());

    // 10.6.2 Inline replaced elements, block-level replaced elements in normal flow, 'inline-block'
    // replaced elements in normal flow and floating replaced elements
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Height][Margin] -> floating replaced -> redirected to inline replaced");
    return inlineReplacedContentHeightAndMargin(replacedBox, horizontalConstraints, { }, overriddenVerticalValues);
}

ContentWidthAndMargin FormattingGeometry::floatingReplacedContentWidthAndMargin(const ElementBox& replacedBox, const HorizontalConstraints& horizontalConstraints, const OverriddenHorizontalValues& overriddenHorizontalValues) const
{
    ASSERT(replacedBox.isFloatingPositioned());

    // 10.3.6 Floating, replaced elements
    //
    // 1. If 'margin-left' or 'margin-right' are computed as 'auto', their used value is '0'.
    // 2. The used value of 'width' is determined as for inline replaced elements.
    auto computedHorizontalMargin = FormattingGeometry::computedHorizontalMargin(replacedBox, horizontalConstraints);

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Height][Margin] -> floating replaced -> redirected to inline replaced");
    auto usedMargin = UsedHorizontalMargin { computedHorizontalMargin.start.value_or(0), computedHorizontalMargin.end.value_or(0) };
    return inlineReplacedContentWidthAndMargin(replacedBox, horizontalConstraints, { }, { overriddenHorizontalValues.width, usedMargin });
}

VerticalGeometry FormattingGeometry::outOfFlowVerticalGeometry(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints, const VerticalConstraints& verticalConstraints, const OverriddenVerticalValues& overriddenVerticalValues) const
{
    ASSERT(layoutBox.isOutOfFlowPositioned());

    if (!layoutBox.isReplacedBox())
        return outOfFlowNonReplacedVerticalGeometry(downcast<ElementBox>(layoutBox), horizontalConstraints, verticalConstraints, overriddenVerticalValues);
    return outOfFlowReplacedVerticalGeometry(downcast<ElementBox>(layoutBox), horizontalConstraints, verticalConstraints, overriddenVerticalValues);
}

HorizontalGeometry FormattingGeometry::outOfFlowHorizontalGeometry(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints, const VerticalConstraints& verticalConstraints, const OverriddenHorizontalValues& overriddenHorizontalValues) const
{
    ASSERT(layoutBox.isOutOfFlowPositioned());

    if (!layoutBox.isReplacedBox())
        return outOfFlowNonReplacedHorizontalGeometry(downcast<ElementBox>(layoutBox), horizontalConstraints, overriddenHorizontalValues);
    return outOfFlowReplacedHorizontalGeometry(downcast<ElementBox>(layoutBox), horizontalConstraints, verticalConstraints, overriddenHorizontalValues);
}

ContentHeightAndMargin FormattingGeometry::floatingContentHeightAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints, const OverriddenVerticalValues& overriddenVerticalValues) const
{
    ASSERT(layoutBox.isFloatingPositioned());

    if (!layoutBox.isReplacedBox())
        return complicatedCases(layoutBox, horizontalConstraints, overriddenVerticalValues);
    return floatingReplacedContentHeightAndMargin(downcast<ElementBox>(layoutBox), horizontalConstraints, overriddenVerticalValues);
}

ContentWidthAndMargin FormattingGeometry::floatingContentWidthAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints, const OverriddenHorizontalValues& overriddenHorizontalValues) const
{
    ASSERT(layoutBox.isFloatingPositioned());

    if (!layoutBox.isReplacedBox())
        return floatingNonReplacedContentWidthAndMargin(layoutBox, horizontalConstraints, overriddenHorizontalValues);
    return floatingReplacedContentWidthAndMargin(downcast<ElementBox>(layoutBox), horizontalConstraints, overriddenHorizontalValues);
}

ContentHeightAndMargin FormattingGeometry::inlineReplacedContentHeightAndMargin(const ElementBox& replacedBox, const HorizontalConstraints& horizontalConstraints, std::optional<VerticalConstraints> verticalConstraints, const OverriddenVerticalValues& overriddenVerticalValues) const
{
    // 10.6.2 Inline replaced elements, block-level replaced elements in normal flow, 'inline-block' replaced elements in normal flow and floating replaced elements
    //
    // 1. If 'margin-top', or 'margin-bottom' are 'auto', their used value is 0.
    // 2. If 'height' and 'width' both have computed values of 'auto' and the element also has an intrinsic height, then that intrinsic height is the used value of 'height'.
    // 3. Otherwise, if 'height' has a computed value of 'auto', and the element has an intrinsic ratio then the used value of 'height' is:
    //    (used width) / (intrinsic ratio)
    // 4. Otherwise, if 'height' has a computed value of 'auto', and the element has an intrinsic height, then that intrinsic height is the used value of 'height'.
    // 5. Otherwise, if 'height' has a computed value of 'auto', but none of the conditions above are met, then the used value of 'height' must be set to
    //    the height of the largest rectangle that has a 2:1 ratio, has a height not greater than 150px, and has a width not greater than the device width.

    // #1
    auto& formattingContext = this->formattingContext();
    auto computedVerticalMargin = FormattingGeometry::computedVerticalMargin(replacedBox, horizontalConstraints);
    auto usedVerticalMargin = UsedVerticalMargin::NonCollapsedValues { computedVerticalMargin.before.value_or(0), computedVerticalMargin.after.value_or(0) };
    auto& style = replacedBox.style();

    auto height = overriddenVerticalValues.height ? overriddenVerticalValues.height.value() : computedHeight(replacedBox, verticalConstraints ? std::optional<LayoutUnit>(verticalConstraints->logicalHeight) : std::nullopt);
    auto heightIsAuto = !overriddenVerticalValues.height && isHeightAuto(replacedBox);
    auto widthIsAuto = style.logicalWidth().isAuto();

    if (heightIsAuto && widthIsAuto && replacedBox.hasIntrinsicHeight()) {
        // #2
        height = replacedBox.intrinsicHeight();
    } else if (heightIsAuto && replacedBox.hasIntrinsicRatio()) {
        // #3
        auto usedWidth = formattingContext.geometryForBox(replacedBox).contentBoxWidth();
        height = usedWidth / replacedBox.intrinsicRatio();
    } else if (heightIsAuto && replacedBox.hasIntrinsicHeight()) {
        // #4
        height = replacedBox.intrinsicHeight();
    } else if (heightIsAuto) {
        // #5
        height = { 150 };
    }

    ASSERT(height);

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Height][Margin] -> inflow replaced -> height(" << *height << "px) margin(" << usedVerticalMargin.before << "px, " << usedVerticalMargin.after << "px) -> layoutBox(" << &replacedBox << ")");
    return { *height, usedVerticalMargin };
}

ContentWidthAndMargin FormattingGeometry::inlineReplacedContentWidthAndMargin(const ElementBox& replacedBox, const HorizontalConstraints& horizontalConstraints, std::optional<VerticalConstraints> verticalConstraints, const OverriddenHorizontalValues& overriddenHorizontalValues) const
{
    // 10.3.2 Inline, replaced elements
    //
    // A computed value of 'auto' for 'margin-left' or 'margin-right' becomes a used value of '0'.
    //
    // 1. If 'height' and 'width' both have computed values of 'auto' and the element also has an intrinsic width, then that intrinsic width is the used value of 'width'.
    //
    // 2. If 'height' and 'width' both have computed values of 'auto' and the element has no intrinsic width, but does have an intrinsic height and intrinsic ratio;
    //    or if 'width' has a computed value of 'auto', 'height' has some other computed value, and the element does have an intrinsic ratio;
    //    then the used value of 'width' is: (used height) * (intrinsic ratio)
    //
    // 3. If 'height' and 'width' both have computed values of 'auto' and the element has an intrinsic ratio but no intrinsic height or width,
    //    then the used value of 'width' is undefined in CSS 2.2. However, it is suggested that, if the containing block's width does not itself depend on the replaced
    //    element's width, then the used value of 'width' is calculated from the constraint equation used for block-level, non-replaced elements in normal flow.
    //
    // 4. Otherwise, if 'width' has a computed value of 'auto', and the element has an intrinsic width, then that intrinsic width is the used value of 'width'.
    //
    // 5. Otherwise, if 'width' has a computed value of 'auto', but none of the conditions above are met, then the used value of 'width' becomes 300px.
    //    If 300px is too wide to fit the device, UAs should use the width of the largest rectangle that has a 2:1 ratio and fits the device instead.

    auto computedHorizontalMargin = FormattingGeometry::computedHorizontalMargin(replacedBox, horizontalConstraints);

    auto usedMarginStart = [&] {
        if (overriddenHorizontalValues.margin)
            return overriddenHorizontalValues.margin->start;
        return computedHorizontalMargin.start.value_or(0_lu);
    };

    auto usedMarginEnd = [&] {
        if (overriddenHorizontalValues.margin)
            return overriddenHorizontalValues.margin->end;
        return computedHorizontalMargin.end.value_or(0_lu);
    };

    auto width = overriddenHorizontalValues.width ? overriddenHorizontalValues.width : computedWidth(replacedBox, horizontalConstraints.logicalWidth);
    auto heightIsAuto = isHeightAuto(replacedBox);
    auto height = computedHeight(replacedBox, verticalConstraints ? std::optional<LayoutUnit>(verticalConstraints->logicalHeight) : std::nullopt);

    if (!width && heightIsAuto && replacedBox.hasIntrinsicWidth()) {
        // #1
        width = replacedBox.intrinsicWidth();
    } else if ((!width && heightIsAuto && !replacedBox.hasIntrinsicWidth() && replacedBox.hasIntrinsicHeight() && replacedBox.hasIntrinsicRatio())
        || (!width && height && replacedBox.hasIntrinsicRatio())) {
        // #2
        width = height.value_or(replacedBox.hasIntrinsicHeight()) * replacedBox.intrinsicRatio();
    } else if (!width && heightIsAuto && replacedBox.hasIntrinsicRatio() && !replacedBox.hasIntrinsicWidth() && !replacedBox.hasIntrinsicHeight()) {
        // #3
        // FIXME: undefined but surely doable.
        ASSERT_NOT_IMPLEMENTED_YET();
    } else if (!width && replacedBox.hasIntrinsicWidth()) {
        // #4
        width = replacedBox.intrinsicWidth();
    } else if (!width) {
        // #5
        width = { 300 };
    }

    ASSERT(width);

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Width][Margin] -> inflow replaced -> width(" << *width << "px) margin(" << usedMarginStart() << "px, " << usedMarginEnd() << "px) -> layoutBox(" << &replacedBox << ")");
    return { *width, { usedMarginStart(), usedMarginEnd() } };
}

LayoutSize FormattingGeometry::inFlowPositionedPositionOffset(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints) const
{
    ASSERT(layoutBox.isInFlowPositioned());

    // 9.4.3 Relative positioning
    //
    // The 'top' and 'bottom' properties move relatively positioned element(s) up or down without changing their size.
    // Top' moves the boxes down, and 'bottom' moves them up. Since boxes are not split or stretched as a result of 'top' or 'bottom', the used values are always: top = -bottom.
    //
    // 1. If both are 'auto', their used values are both '0'.
    // 2. If one of them is 'auto', it becomes the negative of the other.
    // 3. If neither is 'auto', 'bottom' is ignored (i.e., the used value of 'bottom' will be minus the value of 'top').

    auto& style = layoutBox.style();
    auto containingBlockWidth = horizontalConstraints.logicalWidth;

    auto top = computedValue(style.logicalTop(), containingBlockWidth);
    auto bottom = computedValue(style.logicalBottom(), containingBlockWidth);

    if (!top && !bottom) {
        // #1
        top = bottom = { 0 };
    } else if (!top) {
        // #2
        top = -*bottom;
    } else if (!bottom) {
        // #3
        bottom = -*top;
    } else {
        // #4
        bottom = std::nullopt;
    }

    // For relatively positioned elements, 'left' and 'right' move the box(es) horizontally, without changing their size.
    // 'Left' moves the boxes to the right, and 'right' moves them to the left.
    // Since boxes are not split or stretched as a result of 'left' or 'right', the used values are always: left = -right.
    //
    // 1. If both 'left' and 'right' are 'auto' (their initial values), the used values are '0' (i.e., the boxes stay in their original position).
    // 2. If 'left' is 'auto', its used value is minus the value of 'right' (i.e., the boxes move to the left by the value of 'right').
    // 3. If 'right' is specified as 'auto', its used value is minus the value of 'left'.
    // 4. If neither 'left' nor 'right' is 'auto', the position is over-constrained, and one of them has to be ignored.
    //    If the 'direction' property of the containing block is 'ltr', the value of 'left' wins and 'right' becomes -'left'.
    //    If 'direction' of the containing block is 'rtl', 'right' wins and 'left' is ignored.

    auto left = computedValue(style.logicalLeft(), containingBlockWidth);
    auto right = computedValue(style.logicalRight(), containingBlockWidth);

    if (!left && !right) {
        // #1
        left = right = { 0 };
    } else if (!left) {
        // #2
        left = -*right;
    } else if (!right) {
        // #3
        right = -*left;
    } else {
        // #4
        auto isLeftToRightDirection = FormattingContext::containingBlock(layoutBox).style().isLeftToRightDirection();
        if (isLeftToRightDirection)
            right = -*left;
        else
            left = std::nullopt;
    }

    ASSERT(!bottom || *top == -*bottom);
    ASSERT(!left || *left == -*right);

    auto topPositionOffset = *top;
    auto leftPositionOffset = left.value_or(-*right);

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Position] -> positioned inflow -> top offset(" << topPositionOffset << "px) left offset(" << leftPositionOffset << "px) layoutBox(" << &layoutBox << ")");
    return { leftPositionOffset, topPositionOffset };
}

inline static WritingMode usedWritingMode(const Box& layoutBox)
{
    // https://www.w3.org/TR/css-writing-modes-4/#logical-direction-layout
    // Flow-relative directions are calculated with respect to the writing mode of the containing block of the box.
    // For inline-level boxes, the writing mode of the parent box is used instead.
    return layoutBox.isInlineLevelBox() ? layoutBox.parent().style().writingMode() : FormattingContext::containingBlock(layoutBox).style().writingMode();
}

Edges FormattingGeometry::computedBorder(const Box& layoutBox) const
{
    auto& style = layoutBox.style();
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Border] -> layoutBox: " << &layoutBox);
    return {
        { LayoutUnit(style.borderLeftWidth()), LayoutUnit(style.borderRightWidth()) },
        { LayoutUnit(style.borderTopWidth()), LayoutUnit(style.borderBottomWidth()) }
    };
}

std::optional<Edges> FormattingGeometry::computedPadding(const Box& layoutBox, const LayoutUnit containingBlockWidth) const
{
    if (!layoutBox.isPaddingApplicable())
        return std::nullopt;

    auto& style = layoutBox.style();
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Padding] -> layoutBox: " << &layoutBox);
    return Edges {
        { valueForLength(style.paddingStart(), containingBlockWidth), valueForLength(style.paddingEnd(), containingBlockWidth) },
        { valueForLength(style.paddingBefore(), containingBlockWidth), valueForLength(style.paddingAfter(), containingBlockWidth) }
    };
}

ComputedHorizontalMargin FormattingGeometry::computedHorizontalMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints) const
{
    auto& style = layoutBox.style();
    auto containingBlockWidth = horizontalConstraints.logicalWidth;
    if (isHorizontalWritingMode(usedWritingMode(layoutBox)))
        return { computedValue(style.marginLeft(), containingBlockWidth), computedValue(style.marginRight(), containingBlockWidth) };
    return { computedValue(style.marginTop(), containingBlockWidth), computedValue(style.marginBottom(), containingBlockWidth) };
}

ComputedVerticalMargin FormattingGeometry::computedVerticalMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints) const
{
    auto& style = layoutBox.style();
    auto containingBlockWidth = horizontalConstraints.logicalWidth;
    if (isHorizontalWritingMode(usedWritingMode(layoutBox)))
        return { computedValue(style.marginTop(), containingBlockWidth), computedValue(style.marginBottom(), containingBlockWidth) };
    return { computedValue(style.marginLeft(), containingBlockWidth), computedValue(style.marginRight(), containingBlockWidth) };
}

IntrinsicWidthConstraints FormattingGeometry::constrainByMinMaxWidth(const Box& layoutBox, IntrinsicWidthConstraints intrinsicWidth) const
{
    auto& style = layoutBox.style();
    auto minWidth = fixedValue(style.logicalMinWidth());
    auto maxWidth = fixedValue(style.logicalMaxWidth());
    if (!minWidth && !maxWidth)
        return intrinsicWidth;

    if (maxWidth) {
        intrinsicWidth.minimum = std::min(*maxWidth, intrinsicWidth.minimum);
        intrinsicWidth.maximum = std::min(*maxWidth, intrinsicWidth.maximum);
    }

    if (minWidth) {
        intrinsicWidth.minimum = std::max(*minWidth, intrinsicWidth.minimum);
        intrinsicWidth.maximum = std::max(*minWidth, intrinsicWidth.maximum);
    }

    ASSERT(intrinsicWidth.minimum <= intrinsicWidth.maximum);
    return intrinsicWidth;
}

ConstraintsForOutOfFlowContent FormattingGeometry::constraintsForOutOfFlowContent(const ElementBox& elementBox) const
{
    auto& boxGeometry = formattingContext().geometryForBox(elementBox);
    return {
        { boxGeometry.paddingBoxLeft(), boxGeometry.paddingBoxWidth() },
        { boxGeometry.paddingBoxTop(), boxGeometry.paddingBoxHeight() },
        boxGeometry.contentBoxWidth() };
}

ConstraintsForInFlowContent FormattingGeometry::constraintsForInFlowContent(const ElementBox& elementBox, std::optional<FormattingContext::EscapeReason> escapeReason) const
{
    auto& boxGeometry = formattingContext().geometryForBox(elementBox, escapeReason);
    return { { boxGeometry.contentBoxLeft(), boxGeometry.contentBoxWidth() }, boxGeometry.contentBoxTop() };
}

}
}
