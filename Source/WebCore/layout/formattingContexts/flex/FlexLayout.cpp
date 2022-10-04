/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "FlexLayout.h"

#include "FlexFormattingContext.h"
#include "FlexRect.h"
#include "LayoutContext.h"

namespace WebCore {
namespace Layout {

FlexLayout::FlexLayout(const ElementBox& flexBox)
    : m_flexBox(flexBox)
{
}

FlexLayout::LineHeightList FlexLayout::computeAvailableLogicalVerticalSpace(const LogicalFlexItems& flexItems, const WrappingPositions& wrappingIndexList, const LogicalConstraints& flexConstraints) const
{
    auto lineHeightList = LineHeightList(wrappingIndexList.size());
    auto lineRange = Range<size_t> { };
    auto accumulatedContentHeight = LayoutUnit { };
    for (size_t index = 0; index < wrappingIndexList.size(); ++index) {
        lineRange = { lineRange.end(), wrappingIndexList[index] };
        auto contentHeightForRange = [&] {
            auto contentHeight = LayoutUnit { };
            for (auto flexIndex = lineRange.begin(); flexIndex < lineRange.end(); ++flexIndex)
                contentHeight = std::max(contentHeight, flexItems[flexIndex].height());
            return contentHeight;
        };
        lineHeightList[index] = contentHeightForRange();
        accumulatedContentHeight += lineHeightList[index];
    }

    if (flexConstraints.verticalSpace && accumulatedContentHeight < *flexConstraints.verticalSpace) {
        auto extraSpacePerLine = (*flexConstraints.verticalSpace - accumulatedContentHeight) / lineHeightList.size();
        for (size_t index = 0; index < lineHeightList.size(); ++index)
            lineHeightList[index] += extraSpacePerLine;
    }
    return lineHeightList;
}

LayoutUnit FlexLayout::computeAvailableLogicalHorizontalSpace(const LogicalFlexItems& flexItems, const LogicalConstraints& flexConstraints) const
{
    if (flexConstraints.horizontalSpace.available)
        return *flexConstraints.horizontalSpace.available;

    auto contentLogicalWidth = LayoutUnit { };
    for (auto& flexItem : flexItems)
        contentLogicalWidth += flexItem.width();
    return std::max(contentLogicalWidth, flexConstraints.horizontalSpace.minimum.value_or(0_lu));
}

FlexLayout::WrappingPositions FlexLayout::computeWrappingPositions(const LogicalFlexItems& flexItems, LayoutUnit availableSpace) const
{
    auto wrappingPositions = WrappingPositions();

    switch (flexBoxStyle().flexWrap()) {
    case FlexWrap::NoWrap:
        wrappingPositions.append(flexItems.size());
        break;
    case FlexWrap::Wrap:
    case FlexWrap::Reverse: {
        auto accumulatedWidth = LayoutUnit { };
        size_t lastWrapIndex = 0;
        for (size_t index = 0; index < flexItems.size(); ++index) {
            auto flexItemWidth = flexItems[index].width();
            auto isFlexLineEmpty = index == lastWrapIndex;
            if (isFlexLineEmpty || accumulatedWidth + flexItemWidth <= availableSpace) {
                accumulatedWidth += flexItemWidth;
                continue;
            }
            accumulatedWidth = flexItemWidth;
            wrappingPositions.append(index);
            lastWrapIndex = index;
        }
        wrappingPositions.append(flexItems.size());
        break;
    }
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return wrappingPositions;
}

void FlexLayout::computeLogicalWidthForShrinkingFlexItems(const LogicalFlexItems& flexItems, const LineRange& lineRange, LayoutUnit availableSpace, LogicalFlexItemRects& flexRects)
{
    auto totalShrink = 0.f;
    auto totalFlexibleSpace = LayoutUnit { };
    auto flexShrinkBase = 0.f;

    struct ShrinkingFlexItem {
        float flexShrink { 0 };
        const LogicalFlexItem& flexItem;
        bool isFrozen { false };
    };
    Vector<ShrinkingFlexItem> shrinkingItems;

    auto computeTotalShrinkAndOverflowingSpace = [&] {
        // Collect flex items with non-zero flex-shrink value. flex-shrink: 0 flex items
        // don't participate in content flexing.
        for (size_t index = lineRange.begin(); index < lineRange.end(); ++index) {
            auto& flexItem = flexItems[index];
            auto& style = flexItem.style();
            auto baseSize = flexItem.flexBasis();
            if (auto shrinkValue = style.flexShrink()) {
                auto flexShrink = shrinkValue * baseSize;
                shrinkingItems.append({ flexShrink, flexItem, { } });
                totalShrink += flexShrink;
                totalFlexibleSpace += baseSize;
            } else {
                shrinkingItems.append({ { }, flexItem, true });
                availableSpace -= baseSize;
            }
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
                auto& flexItem = shirinkingFlex.flexItem;
                auto baseSize = flexItem.flexBasis();
                auto flexedSize = baseSize - (shirinkingFlex.flexShrink * flexShrinkBase);
                if (!shirinkingFlex.isFrozen && flexItem.minimumSize() > flexedSize) {
                    shirinkingFlex.isFrozen = true;
                    didFreeze = true;
                    totalShrink -= shirinkingFlex.flexShrink;
                    totalFlexibleSpace -= baseSize;
                    availableSpace -= flexItem.minimumSize();
                }
            }
            if (!didFreeze)
                break;
            flexShrinkBase = totalShrink ? (totalFlexibleSpace - availableSpace) / totalShrink : 1.f;
        }
    };
    adjustShrinkBase();

    auto computeLogicalWidth = [&] {
        // Adjust the total grow width by the overflow value (shrink) except when min content with disagrees.
        for (size_t index = 0; index < shrinkingItems.size(); ++index) {
            auto& shirinkingFlex = shrinkingItems[index];
            auto flexedSize = LayoutUnit { shirinkingFlex.flexItem.flexBasis() - (shirinkingFlex.flexShrink * flexShrinkBase) };
            flexRects[lineRange.begin() + index]().setWidth(std::max(shirinkingFlex.flexItem.minimumSize(), flexedSize));
        }
    };
    computeLogicalWidth();
}

void FlexLayout::computeLogicalWidthForStretchingFlexItems(const LogicalFlexItems& flexItems, const LineRange& lineRange, LayoutUnit availableSpace, LogicalFlexItemRects& flexRects)
{
    auto totalFlexibleSpace = LayoutUnit { };
    auto totalGrowth = 0.f;
    auto flexGrowBase = 0.f;
    struct FlexItem {
        float flexGrow { 0 };
        const LogicalFlexItem& logicalFlexItem;
        std::optional<LayoutUnit> frozenSize;
    };
    Vector<FlexItem> resolvedItems;
    resolvedItems.reserveInitialCapacity(flexItems.size());

    auto computeTotalGrowthAndFlexibleSpace = [&] {
        // Collect flex items with non-zero flex-grow value. flex-grow: 0 (initial) flex items
        // don't participate in available space distribution.
        for (size_t index = lineRange.begin(); index < lineRange.end(); ++index) {
            auto& flexItem = flexItems[index];
            if (auto growValue = flexItem.style().flexGrow()) {
                resolvedItems.append({ growValue, flexItem, { } });
                totalGrowth += growValue;
                totalFlexibleSpace += flexItem.flexBasis();
            } else {
                resolvedItems.append({ { }, flexItem, flexItem.flexBasis() });
                availableSpace -= flexItem.flexBasis();
            }
        }
        if (totalGrowth)
            flexGrowBase = (availableSpace - totalFlexibleSpace) / totalGrowth;
    };
    computeTotalGrowthAndFlexibleSpace();

    auto adjustGrowthBase = [&] {
        // This is where we compute how much space the flexing boxes take up if we just
        // let them flex by their flex-grow value. Note that we can't size them below their minimum content width.
        // Such flex items are removed from the final overflow distribution.
        while (true) {
            auto didFreeze = false;
            for (auto& resolvedFlexItem : resolvedItems) {
                if (resolvedFlexItem.frozenSize.has_value())
                    continue;
                auto& flexItem = resolvedFlexItem.logicalFlexItem;
                auto baseSize = flexItem.flexBasis();
                auto flexedSize = baseSize + resolvedFlexItem.flexGrow * flexGrowBase;
                auto belowMinimumSize = flexedSize < flexItem.minimumSize();
                auto aboveMaximumSize = flexedSize > flexItem.maximumSize();
                if (flexedSize && (belowMinimumSize || aboveMaximumSize)) {
                    didFreeze = true;
                    totalGrowth -= resolvedFlexItem.flexGrow;
                    totalFlexibleSpace -= baseSize;
                    resolvedFlexItem.frozenSize = belowMinimumSize ? flexItem.minimumSize() : flexItem.maximumSize();
                    availableSpace -= *resolvedFlexItem.frozenSize;
                }
            }
            if (!didFreeze)
                break;
            flexGrowBase = totalGrowth ? (availableSpace - totalFlexibleSpace) / totalGrowth : 0.f;
        }
    };
    adjustGrowthBase();

    auto computeLogicalWidth = [&] {
        // Adjust the total grow width by the overflow value (shrink) except when min content width disagrees.
        for (size_t index = 0; index < resolvedItems.size(); ++index) {
            auto& resolvedFlexItem = resolvedItems[index];
            if (resolvedFlexItem.frozenSize) {
                flexRects[lineRange.begin() + index]().setWidth(*resolvedFlexItem.frozenSize);
                continue;
            }
            auto flexedSize = LayoutUnit { resolvedFlexItem.logicalFlexItem.flexBasis() + (resolvedFlexItem.flexGrow * flexGrowBase) };
            flexRects[lineRange.begin() + index]().setWidth(flexedSize);
        }
    };
    computeLogicalWidth();
}

void FlexLayout::computeLogicalWidthForFlexItems(const LogicalFlexItems& flexItems, const LineRange& lineRange, LayoutUnit availableSpace, LogicalFlexItemRects& flexRects)
{
    auto contentLogicalWidth = [&] {
        auto logicalWidth = LayoutUnit { };
        for (size_t index = lineRange.begin(); index < lineRange.end(); ++index)
            logicalWidth += flexItems[index].width();
        return logicalWidth;
    }();
    if (availableSpace > contentLogicalWidth)
        computeLogicalWidthForStretchingFlexItems(flexItems, lineRange, availableSpace, flexRects);
    else if (availableSpace < contentLogicalWidth)
        computeLogicalWidthForShrinkingFlexItems(flexItems, lineRange, availableSpace, flexRects);
    else {
        for (size_t index = lineRange.begin(); index < lineRange.end(); ++index)
            flexRects[index]().setWidth(flexItems[index].width());
    }
}

void FlexLayout::computeLogicalHeightForFlexItems(const LogicalFlexItems& flexItems, const LineRange& lineRange, LayoutUnit availableSpace, LogicalFlexItemRects& flexRects)
{
    auto flexBoxAlignItems = flexBoxStyle().alignItems();

    for (size_t index = lineRange.begin(); index < lineRange.end(); ++index) {
        auto& flexItem = flexItems[index];
        if (!flexItem.isHeightAuto()) {
            flexRects[index]().setHeight(flexItem.height());
            continue;
        }
        auto& flexItemAlignSelf = flexItem.style().alignSelf();
        auto alignValue = flexItemAlignSelf.position() != ItemPosition::Auto ? flexItemAlignSelf : flexBoxAlignItems;
        switch (alignValue.position()) {
        case ItemPosition::Normal:
        case ItemPosition::Stretch:
            flexRects[index]().setHeight(availableSpace);
            break;
        case ItemPosition::Center:
        case ItemPosition::Start:
        case ItemPosition::FlexStart:
        case ItemPosition::End:
        case ItemPosition::FlexEnd:
            flexRects[index]().setHeight(flexItem.height());
            break;
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
    }
}

void FlexLayout::distributeMarginAutoInMainAxis(const LogicalFlexItems& flexItems, const LineRange& lineRange, LayoutUnit availableSpace, LogicalFlexItemRects& flexRects)
{
    if (availableSpace <= 0)
        return;

    Vector<size_t> boxesWithMarginAuto;
    boxesWithMarginAuto.reserveInitialCapacity(flexItems.size());

    auto logicalWidth = LayoutUnit { };
    size_t autoMarginCount = 0;
    for (size_t index = lineRange.begin(); index < lineRange.end(); ++index) {
        auto& flexItem = flexItems[index];

        if (flexItem.hasAutoMarginLeft() || flexItem.hasAutoMarginRight()) {
            if (flexItem.hasAutoMarginLeft())
                ++autoMarginCount;
            if (flexItem.hasAutoMarginRight())
                ++autoMarginCount;
            boxesWithMarginAuto.append(index);
        }
        logicalWidth += flexRects[index]().width();
    }

    if (!autoMarginCount) {
        ASSERT(boxesWithMarginAuto.isEmpty());
        return;
    }

    auto extraMargin = std::max(0_lu, availableSpace - logicalWidth) / autoMarginCount;
    if (!extraMargin)
        return;

    for (auto index : boxesWithMarginAuto) {
        auto& flexRect = flexRects[index];
        auto& flexItem = flexItems[index];

        if (flexItem.hasAutoMarginLeft())
            flexRect.autoMargin.left = extraMargin;
        if (flexItem.hasAutoMarginRight())
            flexRect.autoMargin.right = extraMargin;
        flexRect.marginRect.setWidth(flexRect.marginRect.width() + flexRect.autoMargin.left.value_or(0_lu) + flexRect.autoMargin.right.value_or(0_lu));
    }
}

void FlexLayout::distributeMarginAutoInCrossAxis(const LogicalFlexItems& flexItems, const LineRange& lineRange, LayoutUnit availableSpace, LogicalFlexItemRects& flexRects)
{
    if (availableSpace <= 0)
        return;

    for (size_t index = lineRange.begin(); index < lineRange.end(); ++index) {
        auto& flexItem = flexItems[index];
        auto& flexRect = flexRects[index];

        auto hasAutoMarginTop = flexItem.hasAutoMarginTop();
        auto hasAutoMarginBottom = flexItem.hasAutoMarginBottom();
        if (hasAutoMarginTop && hasAutoMarginBottom) {
            auto marginValue = std::max(0_lu, (availableSpace - flexRect.marginRect.height()) / 2);
            flexRect.autoMargin.top = marginValue;
            flexRect.autoMargin.bottom = marginValue;
        } else if (hasAutoMarginTop)
            flexRect.autoMargin.top = std::max(0_lu, availableSpace - flexRect.marginRect.height());
        else if (hasAutoMarginBottom)
            flexRect.autoMargin.bottom = std::max(0_lu, availableSpace - flexRect.marginRect.height());
        else
            continue;
        flexRect.marginRect.setHeight(flexRect.marginRect.height() + flexRect.autoMargin.top.value_or(0_lu) + flexRect.autoMargin.bottom.value_or(0_lu));
    }
}

void FlexLayout::alignFlexItems(const LogicalFlexItems& flexItems, const LineRange& lineRange, VerticalConstraints constraints, LogicalFlexItemRects& flexRects)
{
    // FIXME: Check if height computation and vertical alignment should merge.
    auto availableSpace = constraints.logicalHeight;
    auto lineTop = constraints.logicalTop;
    auto flexBoxAlignItems = flexBoxStyle().alignItems();

    for (size_t index = lineRange.begin(); index < lineRange.end(); ++index) {
        auto& flexItem = flexItems[index];
        auto& flexItemAlignSelf = flexItem.style().alignSelf();
        auto alignValue = flexItemAlignSelf.position() != ItemPosition::Auto ? flexItemAlignSelf : flexBoxAlignItems;
        switch (alignValue.position()) {
        case ItemPosition::Normal:
        case ItemPosition::Stretch:
            flexRects[index]().setTop(lineTop);
            break;
        case ItemPosition::Center:
            flexRects[index]().setTop({ lineTop + (availableSpace / 2 -  flexItem.height() / 2) });
            break;
        case ItemPosition::Start:
        case ItemPosition::FlexStart:
            flexRects[index]().setTop(lineTop);
            break;
        case ItemPosition::End:
        case ItemPosition::FlexEnd:
            flexRects[index]().setTop({ lineTop + availableSpace - flexItem.height() });
            break;
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
    }
}

void FlexLayout::justifyFlexItems(const LogicalFlexItems& flexItems, const LineRange& lineRange, LayoutUnit availableSpace, LogicalFlexItemRects& flexRects)
{
    auto justifyContent = flexBoxStyle().justifyContent();
    // FIXME: Make this optional.
    auto contentLogicalWidth = [&] {
        auto logicalWidth = LayoutUnit { };
        for (size_t index = lineRange.begin(); index < lineRange.end(); ++index)
            logicalWidth += flexItems[index].width();
        return logicalWidth;
    }();

    auto initialOffset = [&] {
        switch (justifyContent.distribution()) {
        case ContentDistribution::Default:
            // Fall back to justifyContent.position() 
            break;
        case ContentDistribution::SpaceBetween:
            return LayoutUnit { };
        case ContentDistribution::SpaceAround: {
            auto itemCount = availableSpace > contentLogicalWidth ? lineRange.distance() : 1;
            return (availableSpace - contentLogicalWidth) / itemCount / 2;
        }
        case ContentDistribution::SpaceEvenly: {
            auto gapCount = availableSpace > contentLogicalWidth ? lineRange.distance() + 1 : 2;
            return (availableSpace - contentLogicalWidth) / gapCount;
        }
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }

        auto positionalAlignment = [&] {
            auto positionalAlignmentValue = justifyContent.position();
            if (!FlexFormattingGeometry::isMainAxisParallelWithInlineAxis(flexBox()) && (positionalAlignmentValue == ContentPosition::Left || positionalAlignmentValue == ContentPosition::Right))
                positionalAlignmentValue = ContentPosition::Start;
            return positionalAlignmentValue;
        };

        switch (positionalAlignment()) {
        // logical alignments
        case ContentPosition::Normal:
        case ContentPosition::FlexStart:
            return LayoutUnit { };
        case ContentPosition::FlexEnd:
            return availableSpace - contentLogicalWidth;
        case ContentPosition::Center:
            return availableSpace / 2 - contentLogicalWidth / 2;
        // non-logical alignments
        case ContentPosition::Left:
        case ContentPosition::Start:
            if (FlexFormattingGeometry::isReversedToContentDirection(flexBox()))
                return availableSpace - contentLogicalWidth;
            return LayoutUnit { };
        case ContentPosition::Right:
        case ContentPosition::End:
            if (FlexFormattingGeometry::isReversedToContentDirection(flexBox()))
                return LayoutUnit { };
            return availableSpace - contentLogicalWidth;
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
            if (lineRange.distance() == 1)
                return LayoutUnit { };
            return std::max(0_lu, availableSpace - contentLogicalWidth) / (lineRange.distance() - 1);
        case ContentDistribution::SpaceAround:
            return std::max(0_lu, availableSpace - contentLogicalWidth) / lineRange.distance();
        case ContentDistribution::SpaceEvenly:
            return std::max(0_lu, availableSpace - contentLogicalWidth) / (lineRange.distance() + 1);
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
        ASSERT_NOT_REACHED();
        return LayoutUnit { };
    };

    auto logicalLeft = initialOffset();
    auto gap = gapBetweenItems();
    for (size_t index = lineRange.begin(); index < lineRange.end(); ++index) {
        flexRects[index]().setLeft(logicalLeft);
        logicalLeft = flexRects[index]().right() + gap;
    }
}

FlexLayout::LogicalFlexItemRects FlexLayout::layout(const LogicalConstraints& constraints, const LogicalFlexItems& flexItems)
{
    auto flexRects = LogicalFlexItemRects(flexItems.size());

    auto availableLogicalHorizontalSpace = computeAvailableLogicalHorizontalSpace(flexItems, constraints);
    auto wrappingIndexList = computeWrappingPositions(flexItems, availableLogicalHorizontalSpace);
    auto lineHeightList = computeAvailableLogicalVerticalSpace(flexItems, wrappingIndexList, constraints);

    auto lineRange = Range<size_t> { };
    auto lineTop = LayoutUnit { };
    for (size_t index = 0; index < wrappingIndexList.size(); ++index) {
        lineRange = { lineRange.end(), wrappingIndexList[index] };

        auto performMainAxisLayout = [&] {
            computeLogicalWidthForFlexItems(flexItems, lineRange, availableLogicalHorizontalSpace, flexRects);
            distributeMarginAutoInMainAxis(flexItems, lineRange, availableLogicalHorizontalSpace, flexRects);
            justifyFlexItems(flexItems, lineRange, availableLogicalHorizontalSpace, flexRects);
        };
        performMainAxisLayout();

        auto performCrossAxisLayout = [&] {
            auto availableLogicalVerticalSpace = lineHeightList[index];
            computeLogicalHeightForFlexItems(flexItems, lineRange, availableLogicalVerticalSpace, flexRects);
            distributeMarginAutoInCrossAxis(flexItems, lineRange, availableLogicalVerticalSpace, flexRects);
            alignFlexItems(flexItems, lineRange, { lineTop, availableLogicalVerticalSpace }, flexRects);
            lineTop += availableLogicalVerticalSpace;
        };
        performCrossAxisLayout();
    }
    return flexRects;
}

}
}

