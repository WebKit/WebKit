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
    auto flexDirection = flexContainer.style().flexDirection();
    return flexDirection == FlexDirection::Row || flexDirection == FlexDirection::RowReverse;
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

LayoutUnit FlexFormattingUtils::rowGapValue(const ElementBox& flexContainer, LayoutUnit flexContainerContentBoxHeight)
{
    ASSERT(flexContainer.isFlexBox());
    auto& rowGap = flexContainer.style().rowGap();
    if (rowGap.isNormal())
        return { };
    return valueForLength(rowGap.length(), flexContainerContentBoxHeight);
}

LayoutUnit FlexFormattingUtils::columnGapValue(const ElementBox& flexContainer, LayoutUnit flexContainerContentBoxWidth)
{
    ASSERT(flexContainer.isFlexBox());
    auto& columnGap = flexContainer.style().columnGap();
    if (columnGap.isNormal())
        return { };
    return valueForLength(columnGap.length(), flexContainerContentBoxWidth);
}

LayoutUnit FlexFormattingUtils::usedMinimumSizeInMainAxis(const LogicalFlexItem& flexItem) const
{
    if (auto mainAxisMinimumWidth = flexItem.mainAxis().minimumSize)
        return *mainAxisMinimumWidth;

    auto minimumContentSize = formattingContext().integrationUtils().minContentSize(downcast<ElementBox>(flexItem.layoutBox()));
    if (auto mainAxisWidth = flexItem.mainAxis().size)
        return std::min(*mainAxisWidth, minimumContentSize);

    return minimumContentSize;
}

std::optional<LayoutUnit> FlexFormattingUtils::usedMaximumSizeInMainAxis(const LogicalFlexItem& flexItem) const
{
    // Initial value of 'max-width: none' computes to used 'infinite'
    return flexItem.mainAxis().maximumSize;
}

LayoutUnit FlexFormattingUtils::usedMaxContentSizeInMainAxis(const LogicalFlexItem& flexItem) const
{
    auto isMainAxisParallelWithInlineAxis = this->isMainAxisParallelWithInlineAxis(formattingContext().root());
    auto& flexItemBox = downcast<ElementBox>(flexItem.layoutBox());

    auto contentSize = LayoutUnit { };
    if (isMainAxisParallelWithInlineAxis)
        contentSize = formattingContext().integrationUtils().maxContentSize(flexItemBox);
    else {
        formattingContext().integrationUtils().layoutWithFormattingContextForBox(flexItemBox);
        contentSize = formattingContext().geometryForFlexItem(flexItemBox).contentBoxHeight();
    }

    if (!flexItem.isContentBoxBased())
        contentSize += flexItem.mainAxis().borderAndPadding;
    return contentSize;
}

LayoutUnit FlexFormattingUtils::usedSizeInCrossAxis(const LogicalFlexItem& flexItem, LayoutUnit maxAxisConstraint) const
{
    if (auto definiteSize = flexItem.crossAxis().definiteSize)
        return *definiteSize;

    auto isMainAxisParallelWithInlineAxis = this->isMainAxisParallelWithInlineAxis(formattingContext().root());
    auto widtConstraintForLayout = isMainAxisParallelWithInlineAxis ? std::make_optional(maxAxisConstraint) : std::nullopt;
    auto& flexItemBox = flexItem.layoutBox();
    formattingContext().integrationUtils().layoutWithFormattingContextForBox(downcast<ElementBox>(flexItemBox), widtConstraintForLayout);
    auto crossSize = isMainAxisParallelWithInlineAxis ? formattingContext().geometryForFlexItem(flexItemBox).contentBoxHeight() : formattingContext().geometryForFlexItem(flexItemBox).contentBoxWidth();
    if (!flexItem.isContentBoxBased())
        crossSize += flexItem.crossAxis().borderAndPadding;
    return crossSize;
}

}
}

