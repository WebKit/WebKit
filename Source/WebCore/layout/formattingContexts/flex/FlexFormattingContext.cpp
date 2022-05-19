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
#include "FlexFormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FlexFormattingGeometry.h"
#include "FlexFormattingState.h"
#include "FlexRect.h"
#include "InlineRect.h"
#include "LayoutBoxGeometry.h"
#include "LayoutChildIterator.h"
#include "LayoutContext.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(FlexFormattingContext);

FlexFormattingContext::FlexFormattingContext(const ContainerBox& formattingContextRoot, FlexFormattingState& formattingState)
    : FormattingContext(formattingContextRoot, formattingState)
    , m_flexFormattingGeometry(*this)
    , m_flexFormattingQuirks(*this)
{
}

void FlexFormattingContext::layoutInFlowContent(const ConstraintsForInFlowContent& constraints)
{
    computeIntrinsicWidthConstraintsForFlexItems();
    sizeAndPlaceFlexItems(constraints);
}

LayoutUnit FlexFormattingContext::usedContentHeight() const
{
    auto& lines = formattingState().lines();
    return LayoutUnit { lines.last().bottom() - lines.first().top() };
}

IntrinsicWidthConstraints FlexFormattingContext::computedIntrinsicWidthConstraints()
{
    return { };
}

void FlexFormattingContext::sizeAndPlaceFlexItems(const ConstraintsForInFlowContent& constraints)
{
    auto& formattingState = this->formattingState();
    auto& formattingGeometry = this->formattingGeometry();
    auto flexItemMainAxisStart = constraints.horizontal().logicalLeft;
    auto flexItemMainAxisEnd = flexItemMainAxisStart;
    auto flexItemCrosAxisStart = constraints.logicalTop();
    auto flexItemCrosAxisEnd = flexItemCrosAxisStart;
    for (auto& flexItem : childrenOfType<ContainerBox>(root())) {
        ASSERT(flexItem.establishesFormattingContext());
        // FIXME: This is just a simple, let's layout the flex items and place them next to each other setup.
        auto intrinsicWidths = formattingState.intrinsicWidthConstraintsForBox(flexItem);
        auto flexItemLogicalWidth = std::min(std::max(intrinsicWidths->minimum, constraints.horizontal().logicalWidth), intrinsicWidths->maximum);
        auto flexItemConstraints = ConstraintsForInFlowContent { { { }, flexItemLogicalWidth }, { } };

        LayoutContext::createFormattingContext(flexItem, layoutState())->layoutInFlowContent(flexItemConstraints);

        auto computeFlexItemGeometry = [&] {
            auto& flexItemGeometry = formattingState.boxGeometry(flexItem);

            flexItemGeometry.setLogicalTopLeft(LayoutPoint { flexItemMainAxisEnd, flexItemCrosAxisStart });

            flexItemGeometry.setBorder(formattingGeometry.computedBorder(flexItem));
            flexItemGeometry.setPadding(formattingGeometry.computedPadding(flexItem, constraints.horizontal().logicalWidth));

            auto computedHorizontalMargin = formattingGeometry.computedHorizontalMargin(flexItem, constraints.horizontal());
            flexItemGeometry.setHorizontalMargin({ computedHorizontalMargin.start.value_or(0_lu), computedHorizontalMargin.end.value_or(0_lu) });

            auto computedVerticalMargin = formattingGeometry.computedVerticalMargin(flexItem, constraints.horizontal());
            flexItemGeometry.setVerticalMargin({ computedVerticalMargin.before.value_or(0_lu), computedVerticalMargin.after.value_or(0_lu) });

            flexItemGeometry.setContentBoxHeight(formattingGeometry.contentHeightForFormattingContextRoot(flexItem));
            flexItemGeometry.setContentBoxWidth(flexItemLogicalWidth);
            flexItemMainAxisEnd= BoxGeometry::borderBoxRect(flexItemGeometry).right();
            flexItemCrosAxisEnd = std::max(flexItemCrosAxisEnd, BoxGeometry::borderBoxRect(flexItemGeometry).bottom());
        };
        computeFlexItemGeometry();
    }
    auto flexLine = InlineRect { flexItemCrosAxisStart, flexItemMainAxisStart, flexItemMainAxisEnd - flexItemMainAxisStart, flexItemCrosAxisEnd - flexItemCrosAxisStart };
    formattingState.addLine(flexLine);
}

void FlexFormattingContext::computeIntrinsicWidthConstraintsForFlexItems()
{
    auto& formattingState = this->formattingState();
    auto& formattingGeometry = this->formattingGeometry();
    for (auto& flexItem : childrenOfType<ContainerBox>(root())) {
        if (formattingState.intrinsicWidthConstraintsForBox(flexItem))
            continue;
        formattingState.setIntrinsicWidthConstraintsForBox(flexItem, formattingGeometry.intrinsicWidthConstraints(flexItem));
    }
}

FlexFormattingContext::LogicalFlexItems FlexFormattingContext::convertFlexItemsToLogicalSpace()
{
    auto& formattingState = this->formattingState();
    LogicalFlexItems logicalFlexItemList;
    auto flexItemsNeedReordering = false;

    auto convertVisualToLogical = [&] {
        auto direction = root().style().flexDirection();
        auto previousLogicalOrder = std::optional<int> { };

        for (auto& flexItem : childrenOfType<ContainerBox>(root())) {
            auto& flexItemGeometry = formattingState.boxGeometry(flexItem);
            auto logicalSize = LayoutSize { };

            switch (direction) {
            case FlexDirection::Row:
            case FlexDirection::RowReverse:
                logicalSize = { flexItemGeometry.marginBoxWidth(), flexItemGeometry.marginBoxHeight() };
                break;
            case FlexDirection::Column:
            case FlexDirection::ColumnReverse:
                logicalSize = { flexItemGeometry.marginBoxHeight(), flexItemGeometry.marginBoxWidth() };
                break;
            default:
                ASSERT_NOT_REACHED();
                break;
            }
            auto flexItemOrder = flexItem.style().order();
            flexItemsNeedReordering = flexItemsNeedReordering || flexItemOrder != previousLogicalOrder.value_or(0);
            previousLogicalOrder = flexItemOrder;

            logicalFlexItemList.append({ { logicalSize }, flexItemOrder, &flexItem });
        }
    };
    convertVisualToLogical();

    auto reorderFlexItemsIfApplicable = [&] {
        if (!flexItemsNeedReordering)
            return;

        std::stable_sort(logicalFlexItemList.begin(), logicalFlexItemList.end(), [&] (auto& a, auto& b) {
            return a.logicalOrder < b.logicalOrder;
        });
    };
    reorderFlexItemsIfApplicable();

    return logicalFlexItemList;
}

void FlexFormattingContext::setFlexItemsGeometry(const LogicalFlexItems& logicalFlexItemList, const ConstraintsForInFlowContent& constraints)
{
    auto& formattingState = this->formattingState();
    auto logicalWidth = logicalFlexItemList.last().rect.right() - logicalFlexItemList.first().rect.left();
    auto direction = root().style().flexDirection();
    for (auto& logicalFlexItem : logicalFlexItemList) {
        auto& flexItemGeometry = formattingState.boxGeometry(*logicalFlexItem.layoutBox);
        auto topLeft = LayoutPoint { };

        switch (direction) {
        case FlexDirection::Row:
            topLeft = { constraints.horizontal().logicalLeft + logicalFlexItem.rect.left(), constraints.logicalTop() + logicalFlexItem.rect.top() };
            break;
        case FlexDirection::RowReverse:
            topLeft = { constraints.horizontal().logicalRight() - logicalFlexItem.rect.right(), constraints.logicalTop() + logicalFlexItem.rect.top() };
            break;
        case FlexDirection::Column: {
            auto flippedTopLeft = logicalFlexItem.rect.topLeft().transposedPoint();
            topLeft = { constraints.horizontal().logicalLeft + flippedTopLeft.x(), constraints.logicalTop() + flippedTopLeft.y() };
            break;
        }
        case FlexDirection::ColumnReverse:
            topLeft = { constraints.horizontal().logicalLeft + logicalFlexItem.rect.top(), constraints.logicalTop() + logicalWidth - logicalFlexItem.rect.right() };
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        flexItemGeometry.setLogicalTopLeft(topLeft);
        if (direction == FlexDirection::Row || direction == FlexDirection::RowReverse)
            flexItemGeometry.setContentBoxWidth(logicalFlexItem.rect.width() - flexItemGeometry.horizontalMarginBorderAndPadding());
        else
            flexItemGeometry.setContentBoxHeight(logicalFlexItem.rect.width() - flexItemGeometry.verticalMarginBorderAndPadding());
    }
}

void FlexFormattingContext::layoutInFlowContentForIntegration(const ConstraintsForInFlowContent& constraints)
{
    auto logicalFlexItemList = convertFlexItemsToLogicalSpace();

    auto totalGrowth = 0.f;
    auto totalFixedWidth = LayoutUnit { };

    for (auto& logicalFlexItem : logicalFlexItemList) {
        totalGrowth += logicalFlexItem.layoutBox->style().flexGrow();
        // FIXME: Use min/max here.
        totalFixedWidth += logicalFlexItem.rect.width();
    }

    auto logicalLeft = LayoutUnit { };
    auto logicalTop = LayoutUnit { };
    auto availableWidth = constraints.horizontal().logicalWidth;
    auto flexibleWidth = availableWidth - totalFixedWidth;

    for (auto& logicalFlexItem : logicalFlexItemList) {
        logicalFlexItem.rect.setTopLeft({ logicalLeft, logicalTop });
        logicalLeft = logicalFlexItem.rect.right();
        auto growFlexItemIfApplicable = [&] {
            if (flexibleWidth <= 0)
                return;
            auto grow = logicalFlexItem.layoutBox->style().flexGrow();
            if (!grow)
                return;
            // This value specifies the flex grow factor, which determines how much the flex item will grow relative to the
            // rest of the flex items in the flex container when positive free space is distributed.
            logicalFlexItem.rect.setWidth(LayoutUnit { availableWidth * grow / totalGrowth });
            // FIXME: constrain logical width on min width.
        };
        growFlexItemIfApplicable();
    }
    setFlexItemsGeometry(logicalFlexItemList, constraints);
}

IntrinsicWidthConstraints FlexFormattingContext::computedIntrinsicWidthConstraintsForIntegration()
{
    return { };
}

}
}

#endif
