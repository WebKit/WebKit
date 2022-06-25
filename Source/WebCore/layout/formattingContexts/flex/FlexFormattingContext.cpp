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
    auto contentTop = LayoutUnit::max();
    auto contentBottom = LayoutUnit::min();
    for (auto& flexItem : childrenOfType<Box>(root())) {
        auto marginBox = Layout::BoxGeometry::marginBoxRect(geometryForBox(flexItem));
        contentTop = std::min(contentTop, marginBox.top());
        contentBottom = std::max(contentBottom, marginBox.bottom());
    }
    return std::max(0_lu, contentBottom - contentTop);
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

std::optional<LayoutUnit> FlexFormattingContext::computedAutoMarginValueForFlexItems(const ConstraintsForFlexContent& constraints)
{
    auto flexDirection = root().style().flexDirection();
    auto flexDirectionIsInlineAxis = flexDirection == FlexDirection::Row || flexDirection == FlexDirection::RowReverse;
    auto availableSpace = flexDirectionIsInlineAxis ? std::make_optional(constraints.horizontal().logicalWidth) : constraints.availableVerticalSpace();
    if (!availableSpace)
        return { };

    size_t autoMarginCount = 0;
    auto logicalWidth = LayoutUnit { };

    for (auto* flexItem = root().firstInFlowChild(); flexItem; flexItem = flexItem->nextInFlowSibling()) {
        auto& flexItemStyle = flexItem->style();
        auto hasAutoMarginStart = flexDirectionIsInlineAxis ? flexItemStyle.marginStart().isAuto() : flexItemStyle.marginBefore().isAuto();
        auto hasAutoMarginEnd = flexDirectionIsInlineAxis ? flexItemStyle.marginEnd().isAuto() : flexItemStyle.marginAfter().isAuto();
        if (hasAutoMarginStart)
            ++autoMarginCount;
        if (hasAutoMarginEnd)
            ++autoMarginCount;

        auto& flexItemGeometry = formattingState().boxGeometry(*flexItem);
        logicalWidth += flexDirectionIsInlineAxis ? flexItemGeometry.marginBoxWidth() : flexItemGeometry.marginBoxHeight();
    }
    if (autoMarginCount)
        return std::max(0_lu, *availableSpace - logicalWidth) / autoMarginCount;
    return { };
}

FlexLayout::LogicalFlexItems FlexFormattingContext::convertFlexItemsToLogicalSpace(const ConstraintsForFlexContent& constraints)
{
    struct FlexItem {
        FlexRect marginRect;
        int logicalOrder { 0 };
        CheckedPtr<const ContainerBox> layoutBox;
    };

    auto& formattingState = this->formattingState();
    Vector<FlexItem> flexItemList;
    auto flexItemsNeedReordering = false;
    auto autoMarginValue = computedAutoMarginValueForFlexItems(constraints);

    auto convertVisualToLogical = [&] {
        auto direction = root().style().flexDirection();
        auto previousLogicalOrder = std::optional<int> { };

        for (auto* flexItem = root().firstInFlowChild(); flexItem; flexItem = flexItem->nextInFlowSibling()) {
            auto& flexItemGeometry = formattingState.boxGeometry(*flexItem);
            auto& flexItemStyle = flexItem->style();
            auto logicalSize = LayoutSize { };

            switch (direction) {
            case FlexDirection::Row:
            case FlexDirection::RowReverse: {
                auto hasAutoMarginStart = flexItemStyle.marginStart().isAuto();
                auto hasAutoMarginEnd = flexItemStyle.marginEnd().isAuto();
                if (autoMarginValue && (hasAutoMarginStart || hasAutoMarginEnd)) {
                    auto horizontalMargin = flexItemGeometry.horizontalMargin();
                    horizontalMargin = { hasAutoMarginStart ? *autoMarginValue : horizontalMargin.start, hasAutoMarginEnd ? *autoMarginValue : horizontalMargin.end };
                    flexItemGeometry.setHorizontalMargin(horizontalMargin);
                }
                logicalSize = { flexItemGeometry.marginBoxWidth(), flexItemGeometry.marginBoxHeight() };
                break;
            }
            case FlexDirection::Column:
            case FlexDirection::ColumnReverse: {
                auto hasAutoMarginBefore = flexItemStyle.marginBefore().isAuto();
                auto hasAutoMarginAfter = flexItemStyle.marginAfter().isAuto();
                if (autoMarginValue && (hasAutoMarginBefore || hasAutoMarginAfter)) {
                    auto verticalMargin = flexItemGeometry.verticalMargin();
                    verticalMargin = { hasAutoMarginBefore ? *autoMarginValue : verticalMargin.before, hasAutoMarginAfter ? *autoMarginValue : verticalMargin.after };
                    flexItemGeometry.setVerticalMargin(verticalMargin);
                }
                logicalSize = { flexItemGeometry.marginBoxHeight(), flexItemGeometry.marginBoxWidth() };
                break;
            }
            default:
                ASSERT_NOT_REACHED();
                break;
            }
            auto flexItemOrder = flexItemStyle.order();
            flexItemsNeedReordering = flexItemsNeedReordering || flexItemOrder != previousLogicalOrder.value_or(0);
            previousLogicalOrder = flexItemOrder;

            flexItemList.append({ { logicalSize }, flexItemOrder, downcast<ContainerBox>(flexItem) });
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

    auto flexDirection = root().style().flexDirection();
    auto flexDirectionIsInlineAxis = flexDirection == FlexDirection::Row || flexDirection == FlexDirection::RowReverse;
    auto logicalFlexItemList = FlexLayout::LogicalFlexItems(flexItemList.size());
    for (size_t index = 0; index < flexItemList.size(); ++index) {
        auto& layoutBox = *flexItemList[index].layoutBox;
        auto& style = layoutBox.style();
        auto logicalWidthType = flexDirectionIsInlineAxis ? style.width().type() : style.height().type();
        auto logicalHeightType = flexDirectionIsInlineAxis ? style.height().type() : style.width().type();
        logicalFlexItemList[index] = { flexItemList[index].marginRect
            , logicalWidthType
            , logicalHeightType
            , *formattingState.intrinsicWidthConstraintsForBox(layoutBox)
            , layoutBox };
    }
    return logicalFlexItemList;
}

void FlexFormattingContext::setFlexItemsGeometry(const FlexLayout::LogicalFlexItems& logicalFlexItemList, const ConstraintsForFlexContent& constraints)
{
    auto& formattingState = this->formattingState();
    auto logicalWidth = logicalFlexItemList.last().right() - logicalFlexItemList.first().left();
    auto direction = root().style().flexDirection();
    for (auto& logicalFlexItem : logicalFlexItemList) {
        auto& flexItemGeometry = formattingState.boxGeometry(logicalFlexItem.layoutBox());
        auto borderBoxTopLeft = LayoutPoint { };

        switch (direction) {
        case FlexDirection::Row:
            borderBoxTopLeft = {
                constraints.horizontal().logicalLeft + logicalFlexItem.left() + flexItemGeometry.marginStart(),
                constraints.logicalTop() + logicalFlexItem.top()
            };
            break;
        case FlexDirection::RowReverse:
            borderBoxTopLeft = {
                constraints.horizontal().logicalRight() - logicalFlexItem.right() + flexItemGeometry.marginStart(),
                constraints.logicalTop() + logicalFlexItem.top()
            };
            break;
        case FlexDirection::Column: {
            auto flippedTopLeft = logicalFlexItem.topLeft().transposedPoint();
            borderBoxTopLeft = {
                constraints.horizontal().logicalLeft + flippedTopLeft.x(),
                constraints.logicalTop() + flippedTopLeft.y() + flexItemGeometry.marginBefore()
            };
            break;
        }
        case FlexDirection::ColumnReverse: {
            auto visualBottom = constraints.logicalTop() + constraints.availableVerticalSpace().value_or(logicalWidth);
            borderBoxTopLeft = {
                constraints.horizontal().logicalLeft + logicalFlexItem.top(),
                visualBottom - logicalFlexItem.right() + flexItemGeometry.marginBefore()
            };
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        flexItemGeometry.setLogicalTopLeft(borderBoxTopLeft);
        if (direction == FlexDirection::Row || direction == FlexDirection::RowReverse) {
            flexItemGeometry.setContentBoxWidth(logicalFlexItem.width() - flexItemGeometry.horizontalMarginBorderAndPadding());
            flexItemGeometry.setContentBoxHeight(logicalFlexItem.height() - flexItemGeometry.verticalMarginBorderAndPadding());
        } else {
            flexItemGeometry.setContentBoxWidth(logicalFlexItem.height() - flexItemGeometry.horizontalMarginBorderAndPadding());
            flexItemGeometry.setContentBoxHeight(logicalFlexItem.width() - flexItemGeometry.verticalMarginBorderAndPadding());
        }
    }
}

void FlexFormattingContext::layoutInFlowContentForIntegration(const ConstraintsForFlexContent& constraints)
{
    auto logicalFlexItems = convertFlexItemsToLogicalSpace(constraints);
    auto flexLayout = FlexLayout { root().style() };

    auto logicalFlexConstraints = [&] {
        auto flexDirection = root().style().flexDirection();
        auto flexDirectionIsInlineAxis = flexDirection == FlexDirection::Row || flexDirection == FlexDirection::RowReverse;
        auto logicalVerticalSapce = flexDirectionIsInlineAxis ? constraints.availableVerticalSpace() : std::make_optional(constraints.horizontal().logicalWidth);
        auto logicalHorizontalSapce = flexDirectionIsInlineAxis ? std::make_optional(constraints.horizontal().logicalWidth) : constraints.availableVerticalSpace();
        return FlexLayout::LogicalConstraints { logicalVerticalSapce, logicalHorizontalSapce };
    };

    flexLayout.layout(logicalFlexConstraints(), logicalFlexItems);
    setFlexItemsGeometry(logicalFlexItems, constraints);
}

IntrinsicWidthConstraints FlexFormattingContext::computedIntrinsicWidthConstraintsForIntegration()
{
    return { };
}

}
}

#endif
