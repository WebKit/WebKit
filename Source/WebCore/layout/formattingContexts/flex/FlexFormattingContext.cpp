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

LayoutUnit FlexFormattingContext::computeAvailableLogicalVerticalSpace(LogicalFlexItems& logicalFlexItemList, const ConstraintsForFlexContent& flexConstraints) const
{
    auto availableLogicalVerticalSpaceFromConstraint = [&] {
        auto flexDirection = root().style().flexDirection();
        auto flexDirectionIsInlineAxis = flexDirection == FlexDirection::Row || flexDirection == FlexDirection::RowReverse;
        if (flexDirectionIsInlineAxis)
            return flexConstraints.availableVerticalSpace();
        return std::optional<LayoutUnit> { flexConstraints.horizontal().logicalWidth };
    };

    if (auto availableSpace = availableLogicalVerticalSpaceFromConstraint())
        return *availableSpace;

    auto availableSpace = LayoutUnit { };
    for (auto& logicalFlexItem : logicalFlexItemList)
        availableSpace = std::max(availableSpace, logicalFlexItem.rect.height());
    return availableSpace;
}

LayoutUnit FlexFormattingContext::computeAvailableLogicalHorizontalSpace(LogicalFlexItems& logicalFlexItemList, const ConstraintsForFlexContent& flexConstraints) const
{
    auto availableLogicalHorizontalSpaceFromConstraint = [&] {
        auto flexDirection = root().style().flexDirection();
        auto flexDirectionIsInlineAxis = flexDirection == FlexDirection::Row || flexDirection == FlexDirection::RowReverse;
        if (flexDirectionIsInlineAxis)
            return std::optional<LayoutUnit> { flexConstraints.horizontal().logicalWidth }; 
        return flexConstraints.availableVerticalSpace();
    };

    if (auto availableSpace = availableLogicalHorizontalSpaceFromConstraint())
        return *availableSpace;

    auto availableSpace = LayoutUnit { };
    for (auto& logicalFlexItem : logicalFlexItemList)
        availableSpace += logicalFlexItem.rect.width();
    return availableSpace;
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

void FlexFormattingContext::setFlexItemsGeometry(const LogicalFlexItems& logicalFlexItemList, const ConstraintsForFlexContent& constraints)
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
        if (direction == FlexDirection::Row || direction == FlexDirection::RowReverse) {
            flexItemGeometry.setContentBoxWidth(logicalFlexItem.rect.width() - flexItemGeometry.horizontalMarginBorderAndPadding());
            flexItemGeometry.setContentBoxHeight(logicalFlexItem.rect.height() - flexItemGeometry.verticalMarginBorderAndPadding());
        } else {
            flexItemGeometry.setContentBoxWidth(logicalFlexItem.rect.height() - flexItemGeometry.horizontalMarginBorderAndPadding());
            flexItemGeometry.setContentBoxHeight(logicalFlexItem.rect.width() - flexItemGeometry.verticalMarginBorderAndPadding());
        }
    }
}

void FlexFormattingContext::computeLogicalWidthForShrinkingFlexItems(LogicalFlexItems& logicalFlexItemList, LayoutUnit availableSpace)
{
    auto& formattingState = this->formattingState();

    auto totalShrink = 0.f;
    auto totalFlexibleSpace = LayoutUnit { };
    auto flexShrinkBase = 0.f;

    struct ShrinkingFlexItem {
        float flexShrink { 0 };
        LayoutUnit minimumSize;
        LayoutUnit flexBasis;
        LogicalFlexItem& flexItem;
        bool isFrozen { false };
    };
    Vector<ShrinkingFlexItem> shrinkingItems;

    auto computeTotalShrinkAndOverflowingSpace = [&] {
        // Collect flex items with non-zero flex-shrink value. flex-shrink: 0 flex items
        // don't participate in content flexing.
        for (auto& flexItem : logicalFlexItemList) {
            auto& style = flexItem.layoutBox->style();
            auto baseSize = style.flexBasis().isFixed() ? LayoutUnit { style.flexBasis().value() } : flexItem.rect.width();
            if (auto shrinkValue = style.flexShrink()) {
                auto flexShrink = shrinkValue * baseSize;
                shrinkingItems.append({ flexShrink, formattingState.intrinsicWidthConstraintsForBox(*flexItem.layoutBox)->minimum, baseSize, flexItem, { } });
                totalShrink += flexShrink;
                totalFlexibleSpace += baseSize;
            } else
                availableSpace -= baseSize;
        }
        if (totalShrink)
            flexShrinkBase = (totalFlexibleSpace - availableSpace) / totalShrink;
    };
    computeTotalShrinkAndOverflowingSpace();

    auto adjustShrinkBase = [&] {
        // Now that we know how much each flex item needs to be shrunk, let's check
        // if they hit their minimum content width (i.e. whether they can be sized that small).
        while (true) {
            auto didFreeze = false;
            for (auto& shirinkingFlex : shrinkingItems) {
                auto flexedSize = shirinkingFlex.flexBasis - (shirinkingFlex.flexShrink * flexShrinkBase);
                if (!shirinkingFlex.isFrozen && shirinkingFlex.minimumSize > flexedSize) {
                    shirinkingFlex.isFrozen = true;
                    didFreeze = true;
                    totalShrink -= shirinkingFlex.flexShrink;
                    totalFlexibleSpace -= shirinkingFlex.flexBasis;
                    availableSpace -= shirinkingFlex.minimumSize;
                }
            }
            if (!didFreeze)
                break;
            flexShrinkBase = totalShrink ? (totalFlexibleSpace - availableSpace) / totalShrink : 0.f;
        }
    };
    adjustShrinkBase();

    auto computeLogicalWidth = [&] {
        // Adjust the total grow width by the overflow value (shrink) except when min content with disagrees.
        for (auto& shirinkingFlex : shrinkingItems) {
            auto flexedSize = LayoutUnit { shirinkingFlex.flexBasis - (shirinkingFlex.flexShrink * flexShrinkBase) };
            shirinkingFlex.flexItem.rect.setWidth(std::max(shirinkingFlex.minimumSize, flexedSize));
        }
    };
    computeLogicalWidth();
}

void FlexFormattingContext::computeLogicalWidthForStretchingFlexItems(LogicalFlexItems& logicalFlexItemList, LayoutUnit availableSpace)
{
    auto& formattingState = this->formattingState();

    auto totalFlexibleSpace = LayoutUnit { };
    auto totalGrowth = 0.f;
    auto flexGrowBase = 0.f;
    struct StrechingFlexItem {
        float flexGrow { 0 };
        LayoutUnit minimumSize;
        LayoutUnit flexBasis;
        LogicalFlexItem& flexItem;
        bool isFrozen { false };
    };
    Vector<StrechingFlexItem> stretchingItems;

    auto computeTotalGrowthAndFlexibleSpace = [&] {
        // Collect flex items with non-zero flex-grow value. flex-grow: 0 (initial) flex items
        // don't participate in available space distribution.
        for (auto& flexItem : logicalFlexItemList) {
            auto& style = flexItem.layoutBox->style();
            auto baseSize = style.flexBasis().isFixed() ? LayoutUnit { style.flexBasis().value() } : flexItem.rect.width();
            if (auto growValue = style.flexGrow()) {
                auto flexGrow = growValue * baseSize;
                stretchingItems.append({ flexGrow, formattingState.intrinsicWidthConstraintsForBox(*flexItem.layoutBox)->minimum, baseSize, flexItem, { } });
                totalGrowth += flexGrow;
                totalFlexibleSpace += baseSize;
            } else
                availableSpace -= baseSize;
        }
        if (totalGrowth)
            flexGrowBase = (availableSpace - totalFlexibleSpace) / totalGrowth;
    };
    computeTotalGrowthAndFlexibleSpace();
    if (!totalGrowth)
        return;

    auto adjustGrowthBase = [&] {
        // This is where we compute how much space the flexing boxes take up if we just
        // let them flex by their flex-grow value. Note that we can't size them below their minimum content width.
        // Such flex items are removed from the final overflow distribution.
        auto accumulatedWidth = LayoutUnit { };
        while (true) {
            auto didFreeze = false;
            for (auto& stretchingFlex : stretchingItems) {
                auto flexedSize = stretchingFlex.flexBasis + stretchingFlex.flexGrow * flexGrowBase;
                if (stretchingFlex.minimumSize >= flexedSize) {
                    stretchingFlex.isFrozen = true;
                    didFreeze = true;
                    totalGrowth -= stretchingFlex.flexGrow;
                    totalFlexibleSpace -= stretchingFlex.flexBasis;
                    availableSpace -= stretchingFlex.minimumSize;
                }
            }
            if (!didFreeze)
                break;
            flexGrowBase = totalGrowth ? (totalFlexibleSpace - availableSpace) / totalGrowth : 0.f;
        }
    };
    adjustGrowthBase();

    auto computeLogicalWidth = [&] {
        // Adjust the total grow width by the overflow value (shrink) except when min content with disagrees.
        for (auto& stretchingFlex : stretchingItems) {
            auto flexedSize = LayoutUnit { stretchingFlex.flexBasis + (stretchingFlex.flexGrow * flexGrowBase) };
            stretchingFlex.flexItem.rect.setWidth(std::max(stretchingFlex.minimumSize, flexedSize));
        }
    };
    computeLogicalWidth();
}

void FlexFormattingContext::computeLogicalWidthForFlexItems(LogicalFlexItems& logicalFlexItemList, LayoutUnit availableSpace)
{
    auto contentLogicalWidth = [&] {
        auto logicalWidth = LayoutUnit { };
        for (auto& logicalFlexItem : logicalFlexItemList)
            logicalWidth += logicalFlexItem.rect.width();
        return logicalWidth;
    }();

    if (availableSpace > contentLogicalWidth)
        computeLogicalWidthForStretchingFlexItems(logicalFlexItemList, availableSpace);
    else if (availableSpace < contentLogicalWidth)
        computeLogicalWidthForShrinkingFlexItems(logicalFlexItemList, availableSpace);
}

void FlexFormattingContext::computeLogicalHeightForFlexItems(LogicalFlexItems& logicalFlexItemList, LayoutUnit availableSpace)
{
    auto alignItems = root().style().alignItems();

    for (auto& logicalFlexItem : logicalFlexItemList) {
        auto& height = logicalFlexItem.layoutBox->style().height();
        if (!height.isAuto())
            continue;
        switch (alignItems.position()) {
        case ItemPosition::Normal:
        case ItemPosition::Stretch:
            logicalFlexItem.rect.setHeight(availableSpace);
            break;
        case ItemPosition::Center:
        case ItemPosition::Start:
        case ItemPosition::FlexStart:
        case ItemPosition::End:
        case ItemPosition::FlexEnd:
            break;
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
    }
}

void FlexFormattingContext::alignFlexItems(LogicalFlexItems& logicalFlexItemList, LayoutUnit availableSpace)
{
    // FIXME: Check if height computation and vertical alignment should merge.
    auto flexBoxAlignItems = root().style().alignItems();

    for (auto& logicalFlexItem : logicalFlexItemList) {
        auto& flexItemAlignSelf = logicalFlexItem.layoutBox->style().alignSelf();
        auto alignValue = flexItemAlignSelf.position() != ItemPosition::Auto ? flexItemAlignSelf : flexBoxAlignItems;
        switch (alignValue.position()) {
        case ItemPosition::Normal:
        case ItemPosition::Stretch:
            logicalFlexItem.rect.setTop({ });
            break;
        case ItemPosition::Center:
            logicalFlexItem.rect.setTop({ availableSpace / 2 -  logicalFlexItem.rect.height() / 2 });
            break;
        case ItemPosition::Start:
        case ItemPosition::FlexStart:
            logicalFlexItem.rect.setTop({ });
            break;
        case ItemPosition::End:
        case ItemPosition::FlexEnd:
            logicalFlexItem.rect.setTop({ availableSpace - logicalFlexItem.rect.height() });
            break;
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
    }
}

void FlexFormattingContext::justifyFlexItems(LogicalFlexItems& logicalFlexItemList, LayoutUnit availableSpace)
{
    auto justifyContent = root().style().justifyContent();
    // FIXME: Make this optional.
    auto contentLogicalWidth = [&] {
        auto logicalWidth = LayoutUnit { };
        for (auto& logicalFlexItem : logicalFlexItemList)
            logicalWidth += logicalFlexItem.rect.width();
        return logicalWidth;
    }();

    auto initialOffset = [&] {
        switch (justifyContent.distribution()) {
        case ContentDistribution::Default:
            // Fall back to justifyContent.position() 
            break;
        case ContentDistribution::SpaceBetween:
            return LayoutUnit { };
        case ContentDistribution::SpaceAround:
            return (availableSpace - contentLogicalWidth) / logicalFlexItemList.size() / 2; 
        case ContentDistribution::SpaceEvenly:
            return (availableSpace - contentLogicalWidth) / (logicalFlexItemList.size() + 1);
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }

        switch (justifyContent.position()) {
        case ContentPosition::Normal:
        case ContentPosition::Start:
        case ContentPosition::FlexStart:
        case ContentPosition::Left:
            return LayoutUnit { };
        case ContentPosition::End:
        case ContentPosition::FlexEnd:
        case ContentPosition::Right:
            return availableSpace - contentLogicalWidth;
        case ContentPosition::Center:
            return availableSpace / 2 - contentLogicalWidth / 2;
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
        ASSERT_NOT_REACHED();
        return LayoutUnit { };
    };

    auto gapBetweenItems = [&] {
        switch (justifyContent.distribution()) {
        case ContentDistribution::Default:
            return LayoutUnit { };
        case ContentDistribution::SpaceBetween:
            if (logicalFlexItemList.size() == 1)
                return LayoutUnit { };
            return (availableSpace - contentLogicalWidth) / (logicalFlexItemList.size() - 1); 
        case ContentDistribution::SpaceAround:
            return (availableSpace - contentLogicalWidth) / logicalFlexItemList.size(); 
        case ContentDistribution::SpaceEvenly:
            return (availableSpace - contentLogicalWidth) / (logicalFlexItemList.size() + 1);
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
        ASSERT_NOT_REACHED();
        return LayoutUnit { };
    };

    auto logicalLeft = initialOffset();
    auto gap = gapBetweenItems();
    for (auto& logicalFlexItem : logicalFlexItemList) {
        logicalFlexItem.rect.setLeft(logicalLeft);
        logicalLeft = logicalFlexItem.rect.right() + gap;
    }
}

void FlexFormattingContext::layoutInFlowContentForIntegration(const ConstraintsForInFlowContent& constraints)
{
    auto logicalFlexItemList = convertFlexItemsToLogicalSpace();
    auto flexConstraints = downcast<ConstraintsForFlexContent>(constraints);

    auto availableLogicalHorizontalSpace = computeAvailableLogicalHorizontalSpace(logicalFlexItemList, flexConstraints);
    computeLogicalWidthForFlexItems(logicalFlexItemList, availableLogicalHorizontalSpace);

    auto availableLogicalVerticalSpace = computeAvailableLogicalVerticalSpace(logicalFlexItemList, flexConstraints);
    computeLogicalHeightForFlexItems(logicalFlexItemList, availableLogicalVerticalSpace);
    alignFlexItems(logicalFlexItemList, availableLogicalVerticalSpace);
    justifyFlexItems(logicalFlexItemList, availableLogicalHorizontalSpace);

    setFlexItemsGeometry(logicalFlexItemList, flexConstraints);
}

IntrinsicWidthConstraints FlexFormattingContext::computedIntrinsicWidthConstraintsForIntegration()
{
    return { };
}

}
}

#endif
