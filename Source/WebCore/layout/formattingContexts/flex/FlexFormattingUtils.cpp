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
#include "FlexFormattingUtils.h"

#include "FlexFormattingContext.h"
#include "LayoutContext.h"
#include "LogicalFlexItem.h"
#include "RenderStyleInlines.h"

namespace WebCore {
namespace Layout {

FlexFormattingUtils::FlexFormattingUtils(const FlexFormattingContext& flexFormattingContext)
    : m_flexFormattingContext(flexFormattingContext)
{
}

bool FlexFormattingUtils::isMainAxisParallelWithInlineAxis(const ElementBox& flexContainer)
{
    ASSERT(flexContainer.isFlexBox());
    auto& flexContainerStyle = flexContainer.style();
    auto isHorizontalWritingMode = flexContainerStyle.writingMode().isHorizontal();
    auto flexDirection = flexContainerStyle.flexDirection();
    return (isHorizontalWritingMode && (flexDirection == FlexDirection::Row || flexDirection == FlexDirection::RowReverse)) || (!isHorizontalWritingMode && (flexDirection == FlexDirection::Column || flexDirection == FlexDirection::ColumnReverse));
}

bool FlexFormattingUtils::isMainAxisParallelWithLeftRightAxis(const ElementBox& flexContainer)
{
    // Currently, the only case where the property’s axis is not parallel with either left↔right axis is in a column flexbox.
    // https://drafts.csswg.org/css-align/#positional-values
    ASSERT(flexContainer.isFlexBox());
    auto flexDirection = flexContainer.style().flexDirection();
    return flexDirection == FlexDirection::Row || flexDirection == FlexDirection::RowReverse;
}

bool FlexFormattingUtils::isInlineDirectionRTL(const ElementBox& flexContainer)
{
    ASSERT(flexContainer.isFlexBox());
    return !flexContainer.writingMode().isLogicalLeftInlineStart();
}

bool FlexFormattingUtils::isMainReversedToContentDirection(const ElementBox& flexContainer)
{
    ASSERT(flexContainer.isFlexBox());
    auto flexDirection = flexContainer.style().flexDirection();
    return flexDirection == FlexDirection::RowReverse || flexDirection == FlexDirection::ColumnReverse;
}

bool FlexFormattingUtils::areFlexLinesReversedInCrossAxis(const ElementBox& flexContainer)
{
    ASSERT(flexContainer.isFlexBox());
    return flexContainer.style().flexWrap() == FlexWrap::Reverse;
}

// The column-gap property specifies spacing between "columns", separating boxes in the container's inline axis similar to inline-axis margin;
// while row-gap indicates spacing between "rows". separating boxes in the container's block axis.
// horizontal row    : column gap
// vertical   row    : column gap
// horizontal column : row gap
// vertical   column : row gap
LayoutUnit FlexFormattingUtils::mainAxisGapValue(const ElementBox& flexContainer, LayoutUnit flexContainerContentBoxWidth)
{
    ASSERT(flexContainer.isFlexBox());
    auto flexDirection = flexContainer.style().flexDirection();
    auto isMainAxisInlineAxis = flexDirection == FlexDirection::Row || flexDirection == FlexDirection::RowReverse;
    auto& gapValue = isMainAxisInlineAxis ? flexContainer.style().columnGap() : flexContainer.style().rowGap();
    if (gapValue.isNormal())
        return { };
    return valueForLength(gapValue.length(), flexContainerContentBoxWidth);
}

LayoutUnit FlexFormattingUtils::crossAxisGapValue(const ElementBox& flexContainer, LayoutUnit flexContainerContentBoxHeight)
{
    ASSERT(flexContainer.isFlexBox());
    auto flexDirection = flexContainer.style().flexDirection();
    auto isMainAxisInlineAxis = flexDirection == FlexDirection::Row || flexDirection == FlexDirection::RowReverse;
    auto& gapValue = isMainAxisInlineAxis ? flexContainer.style().rowGap() : flexContainer.style().columnGap();
    if (gapValue.isNormal())
        return { };
    return valueForLength(gapValue.length(), flexContainerContentBoxHeight);
}

// flex container  direction  flex item    main axis size
// horizontal      row        horizontal : width
// vertical        column     horizontal : width
// vertical        row        vertical   : width
// horizontal      column     vertical   : width
//
// horizontal      row        vertical   : height
// horizontal      column     horizontal : height
// vertical        row        horizontal : height
// vertical        column     vertical   : height
LayoutUnit FlexFormattingUtils::usedMinimumSizeInMainAxis(const LogicalFlexItem& flexItem) const
{
    if (auto mainAxisMinimumWidth = flexItem.mainAxis().minimumSize)
        return *mainAxisMinimumWidth;

    auto& flexContainer = formattingContext().root();
    auto& flexItemBox = downcast<ElementBox>(flexItem.layoutBox());
    auto isMainAxisParallelWithInlineAxis = this->isMainAxisParallelWithInlineAxis(flexContainer);

    auto minimumContentSize = LayoutUnit { };
    auto shouldUseFlexItemContentWidth =
        (isMainAxisParallelWithInlineAxis && flexItem.writingMode().isHorizontal())
        || (!isMainAxisParallelWithInlineAxis && flexItem.writingMode().isVertical());
    minimumContentSize = shouldUseFlexItemContentWidth ? formattingContext().integrationUtils().minContentWidth(flexItemBox) : formattingContext().integrationUtils().minContentHeight(flexItemBox);
    if (auto mainAxisWidth = flexItem.mainAxis().size)
        minimumContentSize = std::min(*mainAxisWidth, minimumContentSize);
    return minimumContentSize;
}

std::optional<LayoutUnit> FlexFormattingUtils::usedMaximumSizeInMainAxis(const LogicalFlexItem& flexItem) const
{
    // Initial value of 'max-width: none' computes to used 'infinite'
    return flexItem.mainAxis().maximumSize;
}

LayoutUnit FlexFormattingUtils::usedMaxContentSizeInMainAxis(const LogicalFlexItem& flexItem) const
{
    auto& flexContainer = formattingContext().root();
    auto& flexItemBox = downcast<ElementBox>(flexItem.layoutBox());
    auto isMainAxisParallelWithInlineAxis = this->isMainAxisParallelWithInlineAxis(flexContainer);

    auto contentSize = LayoutUnit { };
    auto shouldUseFlexItemContentWidth =
        (isMainAxisParallelWithInlineAxis && flexItem.writingMode().isHorizontal())
        || (!isMainAxisParallelWithInlineAxis && flexItem.writingMode().isVertical());
    if (shouldUseFlexItemContentWidth)
        contentSize = formattingContext().integrationUtils().maxContentWidth(flexItemBox);
    else {
        formattingContext().integrationUtils().layoutWithFormattingContextForBox(flexItemBox);
        auto isOrthogonal = flexContainer.writingMode().isOrthogonal(flexItem.writingMode());
        contentSize = !isOrthogonal ? formattingContext().geometryForFlexItem(flexItemBox).contentBoxHeight() : formattingContext().geometryForFlexItem(flexItemBox).contentBoxWidth();
    }

    if (!flexItem.isContentBoxBased())
        contentSize += flexItem.mainAxis().borderAndPadding;
    return contentSize;
}

LayoutUnit FlexFormattingUtils::usedSizeInCrossAxis(const LogicalFlexItem& flexItem, LayoutUnit maxAxisConstraint) const
{
    if (auto definiteSize = flexItem.crossAxis().definiteSize)
        return *definiteSize;

    auto& flexContainer = formattingContext().root();
    auto& flexItemBox = flexItem.layoutBox();
    auto isMainAxisParallelWithInlineAxis = this->isMainAxisParallelWithInlineAxis(flexContainer);

    auto widtConstraintForLayout = isMainAxisParallelWithInlineAxis ? std::make_optional(maxAxisConstraint) : std::nullopt;
    auto shouldUseFlexItemContentHeight =
        (isMainAxisParallelWithInlineAxis && flexItem.writingMode().isHorizontal())
        || (!isMainAxisParallelWithInlineAxis && flexItem.writingMode().isVertical());
    formattingContext().integrationUtils().layoutWithFormattingContextForBox(downcast<ElementBox>(flexItemBox), widtConstraintForLayout);
    auto crossSize = shouldUseFlexItemContentHeight ? formattingContext().geometryForFlexItem(flexItemBox).contentBoxHeight() : formattingContext().geometryForFlexItem(flexItemBox).contentBoxWidth();
    if (!flexItem.isContentBoxBased())
        crossSize += flexItem.crossAxis().borderAndPadding;
    return crossSize;
}

}
}

