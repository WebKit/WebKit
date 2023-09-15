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
#include "LengthFunctions.h"
#include "RenderStyleInlines.h"
#include <wtf/FixedVector.h>
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

void FlexFormattingContext::layout(const ConstraintsForFlexContent& constraints)
{
    auto logicalFlexItems = convertFlexItemsToLogicalSpace(constraints);
    auto flexLayout = FlexLayout { *this };

    auto logicalFlexConstraints = [&] {
        auto flexDirection = root().style().flexDirection();
        auto flexDirectionIsInlineAxis = flexDirection == FlexDirection::Row || flexDirection == FlexDirection::RowReverse;
        auto logicalVerticalSpace = flexDirectionIsInlineAxis ? constraints.availableVerticalSpace() : std::make_optional(constraints.horizontal().logicalWidth);
        auto logicalHorizontalSpace = flexDirectionIsInlineAxis ? std::make_optional(constraints.horizontal().logicalWidth) : constraints.availableVerticalSpace();

        return FlexLayout::LogicalConstraints { { logicalHorizontalSpace, { }, { }, { }, { }, { } }, { logicalVerticalSpace, { }, { }, { }, { }, { } } };
    };

    auto flexItemRects = flexLayout.layout(logicalFlexConstraints(), logicalFlexItems);
    setFlexItemsGeometry(logicalFlexItems, flexItemRects, constraints);
}

IntrinsicWidthConstraints FlexFormattingContext::computedIntrinsicWidthConstraints()
{
    return { };
}

FlexLayout::LogicalFlexItems FlexFormattingContext::convertFlexItemsToLogicalSpace(const ConstraintsForFlexContent& constraints)
{
    struct FlexItem {
        LogicalFlexItem::MainAxisGeometry mainAxis;
        LogicalFlexItem::CrossAxisGeometry crossAxis;
        int logicalOrder { 0 };
        CheckedPtr<const ElementBox> layoutBox;
    };

    Vector<FlexItem> flexItemList;
    auto flexItemsNeedReordering = false;
    auto& formattingState = this->formattingState();

    auto convertVisualToLogical = [&] {
        auto direction = root().style().flexDirection();
        auto previousLogicalOrder = std::optional<int> { };

        for (auto* flexItem = root().firstInFlowChild(); flexItem; flexItem = flexItem->nextInFlowSibling()) {
            auto& flexItemGeometry = formattingState.boxGeometry(*flexItem);
            auto& style = flexItem->style();
            auto mainAxis = LogicalFlexItem::MainAxisGeometry { };
            auto crossAxis = LogicalFlexItem::CrossAxisGeometry { };

            switch (direction) {
            case FlexDirection::Row:
            case FlexDirection::RowReverse: {
                if (style.flexBasis().isAuto()) {
                    // Auto keyword retrieves the value of the main size property as the used flex-basis.
                    // If that value is itself auto, then the used value is content.
                    if (!style.width().isAuto())
                        mainAxis.definiteFlexBasis = valueForLength(style.width(), constraints.horizontal().logicalWidth);
                } else if (!style.flexBasis().isContent())
                    mainAxis.definiteFlexBasis = valueForLength(style.flexBasis(), constraints.horizontal().logicalWidth);
                if (style.width().isSpecified()) {
                    // FIXME: Add support for non-fixed content and clamp them to min/max.
                    mainAxis.minimumUsedSize = valueForLength(style.width(), constraints.horizontal().logicalWidth);
                    mainAxis.maximumUsedSize = mainAxis.minimumUsedSize;
                }
                if (!style.marginStart().isAuto())
                    mainAxis.marginStart = flexItemGeometry.marginStart();
                if (!style.marginEnd().isAuto())
                    mainAxis.marginEnd = flexItemGeometry.marginEnd();
                mainAxis.borderAndPadding = flexItemGeometry.horizontalBorderAndPadding();

                if (!style.marginBefore().isAuto())
                    crossAxis.marginStart = flexItemGeometry.marginBefore();
                if (!style.marginAfter().isAuto())
                    crossAxis.marginEnd = flexItemGeometry.marginAfter();
                auto& height = style.height();
                crossAxis.hasSizeAuto = height.isAuto();
                if (height.isFixed())
                    crossAxis.definiteSize = height.value();
                if (style.maxHeight().isSpecified())
                    crossAxis.maximumSize = valueForLength(style.maxHeight(), constraints.availableVerticalSpace().value_or(0));
                if (style.minHeight().isSpecified())
                    crossAxis.minimumSize = valueForLength(style.minHeight(), constraints.availableVerticalSpace().value_or(0));
                crossAxis.borderAndPadding = flexItemGeometry.verticalBorderAndPadding();
                break;
            }
            case FlexDirection::Column:
            case FlexDirection::ColumnReverse: {
                break;
            }
            default:
                ASSERT_NOT_REACHED();
                break;
            }
            auto flexItemOrder = style.order();
            flexItemsNeedReordering = flexItemsNeedReordering || flexItemOrder != previousLogicalOrder.value_or(0);
            previousLogicalOrder = flexItemOrder;

            flexItemList.append({ mainAxis, crossAxis, flexItemOrder, downcast<ElementBox>(flexItem) });
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
    for (size_t index = 0; index < flexItemList.size(); ++index) {
        auto& flexItem = flexItemList[index];
        logicalFlexItemList[index] = { *flexItem.layoutBox
            , flexItem.mainAxis
            , flexItem.crossAxis
            , false
            , false
        };
    }
    return logicalFlexItemList;
}

void FlexFormattingContext::setFlexItemsGeometry(const FlexLayout::LogicalFlexItems& logicalFlexItemList, const FlexLayout::LogicalFlexItemRects& logicalRects, const ConstraintsForFlexContent& constraints)
{
    auto& formattingState = this->formattingState();
    auto logicalWidth = logicalRects.last().right() - logicalRects.first().left();
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
        return constraints.availableVerticalSpace().value_or(logicalRects.last().bottom());
    }();

    for (size_t index = 0; index < logicalFlexItemList.size(); ++index) {
        auto& logicalFlexItem = logicalFlexItemList[index];
        auto& flexItemGeometry = formattingState.boxGeometry(logicalFlexItem.layoutBox());
        auto borderBoxTopLeft = LayoutPoint { };
        auto logicalRect = logicalRects[index];
        auto adjustedLogicalTop = !flexBoxLogicalHeightForWarpReserve ? logicalRect.top() : *flexBoxLogicalHeightForWarpReserve - logicalRect.bottom();

        switch (flexDirection) {
        case FlexDirection::Row: {
            borderBoxTopLeft = { constraints.horizontal().logicalLeft + logicalRect.left(), constraints.logicalTop() + adjustedLogicalTop };
            break;
        }
        case FlexDirection::RowReverse:
            borderBoxTopLeft = { constraints.horizontal().logicalRight() - logicalRect.right(), constraints.logicalTop() + adjustedLogicalTop };
            break;
        case FlexDirection::Column: {
            auto flippedTopLeft = FloatPoint { adjustedLogicalTop, logicalRect.left() };
            borderBoxTopLeft = { constraints.horizontal().logicalLeft + flippedTopLeft.x(), constraints.logicalTop() + flippedTopLeft.y() };
            break;
        }
        case FlexDirection::ColumnReverse: {
            auto visualBottom = constraints.logicalTop() + constraints.availableVerticalSpace().value_or(logicalWidth);
            borderBoxTopLeft = { constraints.horizontal().logicalLeft + adjustedLogicalTop, visualBottom - logicalRect.right() };
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        flexItemGeometry.setLogicalTopLeft(borderBoxTopLeft);

        auto contentBoxWidth = isMainAxisParallelWithInlineAxis ? logicalRect.width() : logicalRect.height();
        auto contentBoxHeight = isMainAxisParallelWithInlineAxis ? logicalRect.height() : logicalRect.width();
        if (!logicalFlexItem.isContentBoxBased()) {
            contentBoxWidth -= flexItemGeometry.horizontalBorderAndPadding();
            contentBoxHeight -= flexItemGeometry.verticalBorderAndPadding();
        }
        flexItemGeometry.setContentBoxWidth(contentBoxWidth);
        flexItemGeometry.setContentBoxHeight(contentBoxHeight);
    }
}

}
}
