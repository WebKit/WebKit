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

FlexFormattingContext::FlexFormattingContext(const ElementBox& formattingContextRoot, FlexFormattingState& formattingState)
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
    for (auto& flexItem : childrenOfType<ElementBox>(root())) {
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
    for (auto& flexItem : childrenOfType<ElementBox>(root())) {
        if (formattingState.intrinsicWidthConstraintsForBox(flexItem))
            continue;
        formattingState.setIntrinsicWidthConstraintsForBox(flexItem, formattingGeometry.intrinsicWidthConstraints(flexItem));
    }
}

FlexLayout::LogicalFlexItems FlexFormattingContext::convertFlexItemsToLogicalSpace(const ConstraintsForFlexContent& constraints)
{
    struct FlexItem {
        LayoutSize marginBoxSize;
        int logicalOrder { 0 };
        CheckedPtr<const ElementBox> layoutBox;
    };

    auto& formattingState = this->formattingState();
    Vector<FlexItem> flexItemList;
    auto flexItemsNeedReordering = false;

    auto convertVisualToLogical = [&] {
        auto direction = root().style().flexDirection();
        auto previousLogicalOrder = std::optional<int> { };

        for (auto* flexItem = root().firstInFlowChild(); flexItem; flexItem = flexItem->nextInFlowSibling()) {
            auto& flexItemGeometry = formattingState.boxGeometry(*flexItem);
            auto& flexItemStyle = flexItem->style();
            auto logicalSize = LayoutSize { };
            auto flexBasis = flexItemStyle.flexBasis().isAuto() ? std::nullopt : std::make_optional(valueForLength(flexItemStyle.flexBasis(), constraints.horizontal().logicalWidth));

            switch (direction) {
            case FlexDirection::Row:
            case FlexDirection::RowReverse: {
                auto contentBoxLogicalWidth = flexBasis.value_or(flexItemGeometry.contentBoxWidth());
                logicalSize = { flexItemGeometry.horizontalMarginBorderAndPadding() + contentBoxLogicalWidth, flexItemGeometry.marginBoxHeight() };
                break;
            }
            case FlexDirection::Column:
            case FlexDirection::ColumnReverse: {
                auto contentBoxLogicalWidth = flexBasis.value_or(flexItemGeometry.contentBoxHeight());
                logicalSize = { flexItemGeometry.verticalMarginBorderAndPadding() + contentBoxLogicalWidth, flexItemGeometry.marginBoxWidth() };
                break;
            }
            default:
                ASSERT_NOT_REACHED();
                break;
            }
            auto flexItemOrder = flexItemStyle.order();
            flexItemsNeedReordering = flexItemsNeedReordering || flexItemOrder != previousLogicalOrder.value_or(0);
            previousLogicalOrder = flexItemOrder;

            flexItemList.append({ logicalSize, flexItemOrder, downcast<ElementBox>(flexItem) });
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
        auto logicalTypeValues = [&]() -> FlexLayout::LogicalFlexItem::LogicalTypes {
            auto& style = layoutBox.style();
            if (flexDirectionIsInlineAxis) {
                return { style.width().type()
                    , style.height().type()
                    , style.marginStart().type()
                    , style.marginEnd().type()
                    , style.marginBefore().type()
                    , style.marginAfter().type()
                };
            }
            return { style.height().type()
                , style.width().type()
                , style.marginBefore().type()
                , style.marginAfter().type()
                , style.marginStart().type()
                , style.marginEnd().type()
            };
        };
        logicalFlexItemList[index] = { flexItemList[index].marginBoxSize
            , logicalTypeValues()
            , *formattingState.intrinsicWidthConstraintsForBox(layoutBox)
            , layoutBox };
    }
    return logicalFlexItemList;
}

static inline BoxGeometry::HorizontalMargin horizontalMargin(const FlexLayout::FlexItemRect::AutoMargin autoMargin, BoxGeometry::HorizontalMargin computedMargin, FlexDirection flexDirection)
{
    auto marginValue = BoxGeometry::HorizontalMargin { };
    switch (flexDirection) {
    case FlexDirection::Row:
        marginValue = { autoMargin.left.value_or(computedMargin.start), autoMargin.right.value_or(computedMargin.end) };
        break;
    case FlexDirection::RowReverse:
        marginValue = { autoMargin.right.value_or(computedMargin.start), autoMargin.left.value_or(computedMargin.end) };
        break;
    case FlexDirection::Column:
        marginValue = { autoMargin.top.value_or(computedMargin.start), autoMargin.bottom.value_or(computedMargin.end) };
        break;
    case FlexDirection::ColumnReverse:
        marginValue = { autoMargin.top.value_or(computedMargin.start), autoMargin.bottom.value_or(computedMargin.end) };
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return marginValue;
}

static inline BoxGeometry::VerticalMargin verticalMargin(const FlexLayout::FlexItemRect::AutoMargin autoMargin, BoxGeometry::VerticalMargin computedMargin, FlexDirection flexDirection)
{
    auto marginValue = BoxGeometry::VerticalMargin { };
    switch (flexDirection) {
    case FlexDirection::Row:
        marginValue = { autoMargin.top.value_or(computedMargin.before), autoMargin.bottom.value_or(computedMargin.after) };
        break;
    case FlexDirection::RowReverse:
        marginValue = { autoMargin.top.value_or(computedMargin.before), autoMargin.bottom.value_or(computedMargin.after) };
        break;
    case FlexDirection::Column:
        marginValue = { autoMargin.left.value_or(computedMargin.before), autoMargin.right.value_or(computedMargin.after) };
        break;
    case FlexDirection::ColumnReverse:
        marginValue = { autoMargin.right.value_or(computedMargin.before), autoMargin.left.value_or(computedMargin.after) };
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return marginValue;
}

void FlexFormattingContext::setFlexItemsGeometry(const FlexLayout::LogicalFlexItems& logicalFlexItemList, const FlexLayout::LogicalFlexItemRects& logicalRects, const ConstraintsForFlexContent& constraints)
{
    auto& formattingState = this->formattingState();
    auto logicalWidth = logicalRects.last().marginRect.right() - logicalRects.first().marginRect.left();
    auto& flexBoxStyle = root().style();
    auto flexDirection = flexBoxStyle.flexDirection();
    auto isMainAxisParallelWithInlineAxis = FlexFormattingGeometry::isMainAxisParallelWithInlineAxis(root());
    auto flexBoxLogicalHeightForWarpReserve = [&]() -> std::optional<LayoutUnit> {
        if (flexBoxStyle.flexWrap() != FlexWrap::Reverse)
            return { };
        if (!isMainAxisParallelWithInlineAxis) {
            // We always have a valid horizontal constraint for column logical height.
            return constraints.horizontal().logicalWidth;
        }

        // Let's use the bottom of the content if flex box does not have a definite height.
        return constraints.availableVerticalSpace().value_or(logicalRects.last().marginRect.bottom());
    }();

    for (size_t index = 0; index < logicalFlexItemList.size(); ++index) {
        auto& logicalFlexItem = logicalFlexItemList[index];
        auto& flexItemGeometry = formattingState.boxGeometry(logicalFlexItem.layoutBox());
        auto borderBoxTopLeft = LayoutPoint { };
        auto logicalRect = logicalRects[index].marginRect;
        auto usedHorizontalMargin = horizontalMargin(logicalRects[index].autoMargin, flexItemGeometry.horizontalMargin(), flexDirection);
        auto usedVerticalMargin = verticalMargin(logicalRects[index].autoMargin, flexItemGeometry.verticalMargin(), flexDirection);
        auto adjustedLogicalTop = !flexBoxLogicalHeightForWarpReserve ? logicalRect.top() : *flexBoxLogicalHeightForWarpReserve - logicalRect.bottom();

        switch (flexDirection) {
        case FlexDirection::Row: {
            borderBoxTopLeft = {
                constraints.horizontal().logicalLeft + logicalRect.left() + usedHorizontalMargin.start,
                constraints.logicalTop() + adjustedLogicalTop + usedVerticalMargin.before
            };
            break;
        }
        case FlexDirection::RowReverse:
            borderBoxTopLeft = {
                constraints.horizontal().logicalRight() - logicalRect.right() + usedHorizontalMargin.start,
                constraints.logicalTop() + adjustedLogicalTop + usedVerticalMargin.before
            };
            break;
        case FlexDirection::Column: {
            auto flippedTopLeft = FloatPoint { adjustedLogicalTop, logicalRect.left() };
            borderBoxTopLeft = {
                constraints.horizontal().logicalLeft + flippedTopLeft.x() + usedHorizontalMargin.start,
                constraints.logicalTop() + flippedTopLeft.y() + usedVerticalMargin.before
            };
            break;
        }
        case FlexDirection::ColumnReverse: {
            auto visualBottom = constraints.logicalTop() + constraints.availableVerticalSpace().value_or(logicalWidth);
            borderBoxTopLeft = {
                constraints.horizontal().logicalLeft + adjustedLogicalTop + usedHorizontalMargin.start,
                visualBottom - logicalRect.right() + usedVerticalMargin.before
            };
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        flexItemGeometry.setLogicalTopLeft(borderBoxTopLeft);

        auto horizontalMarginBorderAndPadding = usedHorizontalMargin.start + flexItemGeometry.horizontalBorderAndPadding() + usedHorizontalMargin.end;
        auto verticallMarginBorderAndPadding = usedVerticalMargin.before + flexItemGeometry.verticalBorderAndPadding() + usedVerticalMargin.after;
        if (isMainAxisParallelWithInlineAxis) {
            flexItemGeometry.setContentBoxWidth(logicalRect.width() - horizontalMarginBorderAndPadding);
            flexItemGeometry.setContentBoxHeight(logicalRect.height() - verticallMarginBorderAndPadding);
        } else {
            flexItemGeometry.setContentBoxWidth(logicalRect.height() - horizontalMarginBorderAndPadding);
            flexItemGeometry.setContentBoxHeight(logicalRect.width() - verticallMarginBorderAndPadding);
        }
    }
}

void FlexFormattingContext::layoutInFlowContentForIntegration(const ConstraintsForFlexContent& constraints)
{
    auto logicalFlexItems = convertFlexItemsToLogicalSpace(constraints);
    auto flexLayout = FlexLayout { root() };

    auto logicalFlexConstraints = [&] {
        auto flexDirection = root().style().flexDirection();
        auto flexDirectionIsInlineAxis = flexDirection == FlexDirection::Row || flexDirection == FlexDirection::RowReverse;
        auto logicalVerticalSpace = flexDirectionIsInlineAxis ? constraints.availableVerticalSpace() : std::make_optional(constraints.horizontal().logicalWidth);
        auto logicalHorizontalSpace = flexDirectionIsInlineAxis ? std::make_optional(constraints.horizontal().logicalWidth) : constraints.availableVerticalSpace();
        auto logicalMinimumHorizontalSpace = flexDirectionIsInlineAxis ? std::nullopt : constraints.minimumVerticalSpace();
        return FlexLayout::LogicalConstraints { logicalVerticalSpace, { logicalHorizontalSpace, logicalMinimumHorizontalSpace } };
    };

    auto flexItemRects = flexLayout.layout(logicalFlexConstraints(), logicalFlexItems);
    setFlexItemsGeometry(logicalFlexItems, flexItemRects, constraints);
}

IntrinsicWidthConstraints FlexFormattingContext::computedIntrinsicWidthConstraintsForIntegration()
{
    return { };
}

}
}

