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

#include "FlexFormattingUtils.h"
#include "FlexRect.h"
#include "InlineRect.h"
#include "LayoutBoxGeometry.h"
#include "LayoutChildIterator.h"
#include "LayoutContext.h"
#include "LayoutState.h"
#include "LengthFunctions.h"
#include "RenderStyleInlines.h"
#include <wtf/FixedVector.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(FlexFormattingContext);

FlexFormattingContext::FlexFormattingContext(const ElementBox& flexBox, LayoutState& globalLayoutState)
    : m_flexBox(flexBox)
    , m_globalLayoutState(globalLayoutState)
    , m_flexFormattingUtils(*this)
    , m_integrationUtils(globalLayoutState)
{
}

void FlexFormattingContext::layout(const ConstraintsForFlexContent& constraints)
{
    auto logicalFlexItems = convertFlexItemsToLogicalSpace(constraints);
    auto flexItemRects = FlexLayout { *this }.layout(constraints, logicalFlexItems);
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

    auto convertVisualToLogical = [&] {
        auto direction = root().style().flexDirection();
        auto previousLogicalOrder = std::optional<int> { };
        auto flexContainerMainInnerSize = constraints.mainAxis().availableSize;

        for (auto* flexItem = root().firstInFlowChild(); flexItem; flexItem = flexItem->nextInFlowSibling()) {
            auto& flexItemGeometry = m_globalLayoutState.geometryForBox(*flexItem);
            auto& style = flexItem->style();
            auto mainAxis = LogicalFlexItem::MainAxisGeometry { };
            auto crossAxis = LogicalFlexItem::CrossAxisGeometry { };

            switch (direction) {
            case FlexDirection::Row:
            case FlexDirection::RowReverse: {
                if (style.width().isSpecified())
                    mainAxis.size = valueForLength(style.width(), *flexContainerMainInnerSize);

                if (style.flexBasis().isAuto()) {
                    // Auto keyword retrieves the value of the main size property as the used flex-basis.
                    // If that value is itself auto, then the used value is content.
                    mainAxis.definiteFlexBasis = mainAxis.size;
                } else if (!style.flexBasis().isIntrinsic())
                    mainAxis.definiteFlexBasis = valueForLength(style.flexBasis(), *flexContainerMainInnerSize);
                if (style.minWidth().isSpecified())
                    mainAxis.minimumSize = valueForLength(style.minWidth(), *flexContainerMainInnerSize);
                if (style.maxWidth().isSpecified())
                    mainAxis.maximumSize = valueForLength(style.maxWidth(), *flexContainerMainInnerSize);

                auto marginStart = [&] {
                    if (direction == FlexDirection::Row)
                        return style.marginStart().isAuto() ? std::nullopt : std::make_optional(flexItemGeometry.marginStart());
                    return style.marginEnd().isAuto() ? std::nullopt : std::make_optional(flexItemGeometry.marginEnd());
                };
                auto marginEnd = [&] {
                    if (direction == FlexDirection::Row)
                        return style.marginEnd().isAuto() ? std::nullopt : std::make_optional(flexItemGeometry.marginEnd());
                    return style.marginStart().isAuto() ? std::nullopt : std::make_optional(flexItemGeometry.marginStart());
                };
                mainAxis.marginStart = marginStart();
                mainAxis.marginEnd = marginEnd();
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
                    crossAxis.maximumSize = valueForLength(style.maxHeight(), constraints.crossAxis().availableSize.value_or(0));
                if (style.minHeight().isSpecified())
                    crossAxis.minimumSize = valueForLength(style.minHeight(), constraints.crossAxis().availableSize.value_or(0));
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
    auto logicalWidth = logicalRects.last().right() - logicalRects.first().left();
    auto& flexBoxStyle = root().style();
    auto flexDirection = flexBoxStyle.flexDirection();
    auto isMainAxisParallelWithInlineAxis = FlexFormattingUtils::isMainAxisParallelWithInlineAxis(root());
    auto flexContainerContentBoxPosition = LayoutPoint { constraints.mainAxis().startPosition, constraints.crossAxis().startPosition };

    auto flexBoxLogicalHeightForWarpReverse = [&]() -> std::optional<LayoutUnit> {
        if (flexBoxStyle.flexWrap() != FlexWrap::Reverse)
            return { };
        if (!isMainAxisParallelWithInlineAxis) {
            // We always have a valid horizontal constraint for column logical height.
            return constraints.mainAxis().availableSize;
        }

        // Let's use the bottom of the content if flex box does not have a definite height.
        return constraints.crossAxis().availableSize.value_or(logicalRects.last().bottom());
    }();

    for (size_t index = 0; index < logicalFlexItemList.size(); ++index) {
        auto& logicalFlexItem = logicalFlexItemList[index];
        auto& flexItemGeometry = geometryForFlexItem(logicalFlexItem.layoutBox());
        auto logicalRect = [&] {
            // Note that flex rects are inner size based.
            if (!flexBoxLogicalHeightForWarpReverse)
                return logicalRects[index];
            auto rect = logicalRects[index];
            auto adjustedLogicalBorderBoxTop = *flexBoxLogicalHeightForWarpReverse - (rect.bottom() - flexItemGeometry.marginBefore());
            if (logicalFlexItem.isContentBoxBased())
                adjustedLogicalBorderBoxTop -= flexItemGeometry.verticalBorderAndPadding();
            rect.setTop(adjustedLogicalBorderBoxTop);
            return rect;
        }();

        auto borderBoxTopLeft = LayoutPoint { };
        switch (flexDirection) {
        case FlexDirection::Row: {
            borderBoxTopLeft = { flexContainerContentBoxPosition.x() + logicalRect.left(), flexContainerContentBoxPosition.y() + logicalRect.top() };
            break;
        }
        case FlexDirection::RowReverse:
            borderBoxTopLeft = { constraints.mainAxis().startPosition + *constraints.mainAxis().availableSize - logicalRect.right(), flexContainerContentBoxPosition.y() + logicalRect.top() };
            if (logicalFlexItem.isContentBoxBased())
                borderBoxTopLeft.move({ -flexItemGeometry.horizontalBorderAndPadding(), 0_lu });
            break;
        case FlexDirection::Column: {
            auto flippedTopLeft = FloatPoint { logicalRect.top(), logicalRect.left() };
            borderBoxTopLeft = { flexContainerContentBoxPosition.x() + flippedTopLeft.x(), flexContainerContentBoxPosition.y() + flippedTopLeft.y() };
            break;
        }
        case FlexDirection::ColumnReverse: {
            auto visualBottom = flexContainerContentBoxPosition.y() + constraints.crossAxis().availableSize.value_or(logicalWidth);
            borderBoxTopLeft = { flexContainerContentBoxPosition.x() + logicalRect.top(), visualBottom - logicalRect.right() };
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        flexItemGeometry.setTopLeft(borderBoxTopLeft);

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

const BoxGeometry& FlexFormattingContext::geometryForFlexItem(const Box& flexItem) const
{
    ASSERT(flexItem.isFlexItem());
    return m_globalLayoutState.geometryForBox(flexItem);
}

BoxGeometry& FlexFormattingContext::geometryForFlexItem(const Box& flexItem)
{
    ASSERT(flexItem.isFlexItem());
    return m_globalLayoutState.ensureGeometryForBox(flexItem);
}


}
}
