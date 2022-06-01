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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FlexRect.h"
#include "LayoutContext.h"

namespace WebCore {
namespace Layout {

FlexLayout::FlexLayout(const FlexFormattingState& formattingState, const RenderStyle& flexBoxStyle)
    : m_formattingState(formattingState)
    , m_flexBoxStyle(flexBoxStyle)
{
}

LayoutUnit FlexLayout::computeAvailableLogicalVerticalSpace(LogicalFlexItems& flexItems, const ConstraintsForFlexContent& flexConstraints) const
{
    auto availableLogicalVerticalSpaceFromConstraint = [&] {
        auto flexDirection = flexBoxStyle().flexDirection();
        auto flexDirectionIsInlineAxis = flexDirection == FlexDirection::Row || flexDirection == FlexDirection::RowReverse;
        if (flexDirectionIsInlineAxis)
            return flexConstraints.availableVerticalSpace();
        return std::optional<LayoutUnit> { flexConstraints.horizontal().logicalWidth };
    };

    if (auto availableSpace = availableLogicalVerticalSpaceFromConstraint())
        return *availableSpace;

    auto availableSpace = LayoutUnit { };
    for (auto& flexItem : flexItems)
        availableSpace = std::max(availableSpace, flexItem.marginRect.height());
    return availableSpace;
}

LayoutUnit FlexLayout::computeAvailableLogicalHorizontalSpace(LogicalFlexItems& flexItems, const ConstraintsForFlexContent& flexConstraints) const
{
    auto availableLogicalHorizontalSpaceFromConstraint = [&] {
        auto flexDirection = flexBoxStyle().flexDirection();
        auto flexDirectionIsInlineAxis = flexDirection == FlexDirection::Row || flexDirection == FlexDirection::RowReverse;
        if (flexDirectionIsInlineAxis)
            return std::optional<LayoutUnit> { flexConstraints.horizontal().logicalWidth }; 
        return flexConstraints.availableVerticalSpace();
    };

    if (auto availableSpace = availableLogicalHorizontalSpaceFromConstraint())
        return *availableSpace;

    auto availableSpace = LayoutUnit { };
    for (auto& flexItem : flexItems)
        availableSpace += flexItem.marginRect.width();
    return availableSpace;
}

void FlexLayout::computeLogicalWidthForShrinkingFlexItems(LogicalFlexItems& flexItems, LayoutUnit availableSpace)
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
        for (auto& flexItem : flexItems) {
            auto& style = flexItem.layoutBox->style();
            auto baseSize = style.flexBasis().isFixed() ? LayoutUnit { style.flexBasis().value() } : flexItem.marginRect.width();
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
            shirinkingFlex.flexItem.marginRect.setWidth(std::max(shirinkingFlex.minimumSize, flexedSize));
        }
    };
    computeLogicalWidth();
}

void FlexLayout::computeLogicalWidthForStretchingFlexItems(LogicalFlexItems& flexItems, LayoutUnit availableSpace)
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
        for (auto& flexItem : flexItems) {
            auto& style = flexItem.layoutBox->style();
            auto baseSize = style.flexBasis().isFixed() ? LayoutUnit { style.flexBasis().value() } : flexItem.marginRect.width();
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
            stretchingFlex.flexItem.marginRect.setWidth(std::max(stretchingFlex.minimumSize, flexedSize));
        }
    };
    computeLogicalWidth();
}

void FlexLayout::computeLogicalWidthForFlexItems(LogicalFlexItems& flexItems, LayoutUnit availableSpace)
{
    auto contentLogicalWidth = [&] {
        auto logicalWidth = LayoutUnit { };
        for (auto& flexItem : flexItems)
            logicalWidth += flexItem.marginRect.width();
        return logicalWidth;
    }();

    if (availableSpace > contentLogicalWidth)
        computeLogicalWidthForStretchingFlexItems(flexItems, availableSpace);
    else if (availableSpace < contentLogicalWidth)
        computeLogicalWidthForShrinkingFlexItems(flexItems, availableSpace);
}

void FlexLayout::computeLogicalHeightForFlexItems(LogicalFlexItems& flexItems, LayoutUnit availableSpace)
{
    auto alignItems = flexBoxStyle().alignItems();

    for (auto& flexItem : flexItems) {
        auto& height = flexItem.layoutBox->style().height();
        if (!height.isAuto())
            continue;
        switch (alignItems.position()) {
        case ItemPosition::Normal:
        case ItemPosition::Stretch:
            flexItem.marginRect.setHeight(availableSpace);
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

void FlexLayout::alignFlexItems(LogicalFlexItems& flexItems, LayoutUnit availableSpace)
{
    // FIXME: Check if height computation and vertical alignment should merge.
    auto flexBoxAlignItems = flexBoxStyle().alignItems();

    for (auto& flexItem : flexItems) {
        auto& flexItemAlignSelf = flexItem.layoutBox->style().alignSelf();
        auto alignValue = flexItemAlignSelf.position() != ItemPosition::Auto ? flexItemAlignSelf : flexBoxAlignItems;
        switch (alignValue.position()) {
        case ItemPosition::Normal:
        case ItemPosition::Stretch:
            flexItem.marginRect.setTop({ });
            break;
        case ItemPosition::Center:
            flexItem.marginRect.setTop({ availableSpace / 2 -  flexItem.marginRect.height() / 2 });
            break;
        case ItemPosition::Start:
        case ItemPosition::FlexStart:
            flexItem.marginRect.setTop({ });
            break;
        case ItemPosition::End:
        case ItemPosition::FlexEnd:
            flexItem.marginRect.setTop({ availableSpace - flexItem.marginRect.height() });
            break;
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
    }
}

void FlexLayout::justifyFlexItems(LogicalFlexItems& flexItems, LayoutUnit availableSpace)
{
    auto justifyContent = flexBoxStyle().justifyContent();
    // FIXME: Make this optional.
    auto contentLogicalWidth = [&] {
        auto logicalWidth = LayoutUnit { };
        for (auto& flexItem : flexItems)
            logicalWidth += flexItem.marginRect.width();
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
            return (availableSpace - contentLogicalWidth) / flexItems.size() / 2; 
        case ContentDistribution::SpaceEvenly:
            return (availableSpace - contentLogicalWidth) / (flexItems.size() + 1);
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
            if (flexItems.size() == 1)
                return LayoutUnit { };
            return (availableSpace - contentLogicalWidth) / (flexItems.size() - 1); 
        case ContentDistribution::SpaceAround:
            return (availableSpace - contentLogicalWidth) / flexItems.size(); 
        case ContentDistribution::SpaceEvenly:
            return (availableSpace - contentLogicalWidth) / (flexItems.size() + 1);
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
        ASSERT_NOT_REACHED();
        return LayoutUnit { };
    };

    auto logicalLeft = initialOffset();
    auto gap = gapBetweenItems();
    for (auto& flexItem : flexItems) {
        flexItem.marginRect.setLeft(logicalLeft);
        logicalLeft = flexItem.marginRect.right() + gap;
    }
}

void FlexLayout::layout(const ConstraintsForFlexContent& constraints, LogicalFlexItems& flexItems)
{
    auto availableLogicalHorizontalSpace = computeAvailableLogicalHorizontalSpace(flexItems, constraints);
    computeLogicalWidthForFlexItems(flexItems, availableLogicalHorizontalSpace);

    auto availableLogicalVerticalSpace = computeAvailableLogicalVerticalSpace(flexItems, constraints);
    computeLogicalHeightForFlexItems(flexItems, availableLogicalVerticalSpace);
    alignFlexItems(flexItems, availableLogicalVerticalSpace);
    justifyFlexItems(flexItems, availableLogicalHorizontalSpace);
}

}
}

#endif
