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
    sizeAndPlaceFlexItems(downcast<ConstraintsForFlexContent>(constraints));
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

void FlexFormattingContext::sizeAndPlaceFlexItems(const ConstraintsForFlexContent& constraints)
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

FlexLayout::LogicalFlexItems FlexFormattingContext::convertFlexItemsToLogicalSpace()
{
    struct FlexItem {
        FlexRect marginRect;
        int logicalOrder { 0 };
    };

    auto& formattingState = this->formattingState();
    Vector<FlexItem> flexItemList;
    auto flexItemsNeedReordering = false;

    auto convertVisualToLogical = [&] {
        auto direction = root().style().flexDirection();
        auto previousLogicalOrder = std::optional<int> { };

        for (auto* flexItem = root().firstInFlowChild(); flexItem; flexItem = flexItem->nextInFlowSibling()) {
            auto& flexItemGeometry = formattingState.boxGeometry(*flexItem);
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
            auto flexItemOrder = flexItem->style().order();
            flexItemsNeedReordering = flexItemsNeedReordering || flexItemOrder != previousLogicalOrder.value_or(0);
            previousLogicalOrder = flexItemOrder;

            flexItemList.append({ { logicalSize }, flexItemOrder });
        }
    };
    convertVisualToLogical();

    auto reorderFlexItemsIfApplicable = [&] {
        if (!flexItemsNeedReordering)
            return;

        std::stable_sort(flexItemList.begin(), flexItemList.end(), [&] (auto& a, auto& b) {
            return a.logicalOrder < b.logicalOrder;
        });
    };
    reorderFlexItemsIfApplicable();

    auto logicalFlexItemList = FlexLayout::LogicalFlexItems(flexItemList.size());
    auto* layoutBox = root().firstInFlowChild();
    for (size_t index = 0; index < flexItemList.size(); ++index) {
        logicalFlexItemList[index] = { flexItemList[index].marginRect, downcast<ContainerBox>(layoutBox) };
        layoutBox = layoutBox->nextInFlowSibling();
    }
    return logicalFlexItemList;
}

void FlexFormattingContext::setFlexItemsGeometry(const FlexLayout::LogicalFlexItems& logicalFlexItemList, const ConstraintsForFlexContent& constraints)
{
    auto& formattingState = this->formattingState();
    auto logicalWidth = logicalFlexItemList.last().marginRect.right() - logicalFlexItemList.first().marginRect.left();
    auto direction = root().style().flexDirection();
    for (auto& logicalFlexItem : logicalFlexItemList) {
        auto& flexItemGeometry = formattingState.boxGeometry(*logicalFlexItem.layoutBox);
        auto topLeft = LayoutPoint { };

        switch (direction) {
        case FlexDirection::Row:
            topLeft = { constraints.horizontal().logicalLeft + logicalFlexItem.marginRect.left(), constraints.logicalTop() + logicalFlexItem.marginRect.top() };
            break;
        case FlexDirection::RowReverse:
            topLeft = { constraints.horizontal().logicalRight() - logicalFlexItem.marginRect.right(), constraints.logicalTop() + logicalFlexItem.marginRect.top() };
            break;
        case FlexDirection::Column: {
            auto flippedTopLeft = logicalFlexItem.marginRect.topLeft().transposedPoint();
            topLeft = { constraints.horizontal().logicalLeft + flippedTopLeft.x(), constraints.logicalTop() + flippedTopLeft.y() };
            break;
        }
        case FlexDirection::ColumnReverse:
            topLeft = { constraints.horizontal().logicalLeft + logicalFlexItem.marginRect.top(), constraints.logicalTop() + logicalWidth - logicalFlexItem.marginRect.right() };
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        flexItemGeometry.setLogicalTopLeft(topLeft);
        if (direction == FlexDirection::Row || direction == FlexDirection::RowReverse) {
            flexItemGeometry.setContentBoxWidth(logicalFlexItem.marginRect.width() - flexItemGeometry.horizontalMarginBorderAndPadding());
            flexItemGeometry.setContentBoxHeight(logicalFlexItem.marginRect.height() - flexItemGeometry.verticalMarginBorderAndPadding());
        } else {
            flexItemGeometry.setContentBoxWidth(logicalFlexItem.marginRect.height() - flexItemGeometry.horizontalMarginBorderAndPadding());
            flexItemGeometry.setContentBoxHeight(logicalFlexItem.marginRect.width() - flexItemGeometry.verticalMarginBorderAndPadding());
        }
    }
}

void FlexFormattingContext::layoutInFlowContentForIntegration(const ConstraintsForInFlowContent& constraints)
{
    auto flexConstraints = downcast<ConstraintsForFlexContent>(constraints);
    auto logicalFlexItems = convertFlexItemsToLogicalSpace();
    auto flexLayout = FlexLayout { formattingState(), root().style() };
    flexLayout.layout(flexConstraints, logicalFlexItems);
    setFlexItemsGeometry(logicalFlexItems, flexConstraints);
}

IntrinsicWidthConstraints FlexFormattingContext::computedIntrinsicWidthConstraintsForIntegration()
{
    return { };
}

}
}

#endif
