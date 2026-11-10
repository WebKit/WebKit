/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "LayoutIntegrationUtils.h"

#include "LayoutBox.h"
#include "LayoutBoxGeometry.h"
#include "LayoutIntegrationFormattingContextLayout.h"
#include "LayoutState.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderObject.h"
#include "RenderObjectInlines.h"

namespace WebCore {
namespace Layout {

const Style::ComputedStyle& IntegrationUtils::firstNonAnonymousAncestorStyle(const Box& box)
{
    CheckedPtr renderer = box.rendererForIntegration();
    if (auto* ancestor = renderer ? renderer->firstNonAnonymousAncestor() : nullptr)
        return ancestor->style();
    return box.style();
}

// The grid container's axes, not the item's own: an orthogonal item's inline axis is the container's block axis.
static GridItemBorderAndPadding borderAndPadding(const RenderBox& renderer, WritingMode containerWritingMode)
{
    if (containerWritingMode.isHorizontal())
        return { renderer.horizontalBorderAndPaddingExtent(), renderer.verticalBorderAndPaddingExtent() };
    return { renderer.verticalBorderAndPaddingExtent(), renderer.horizontalBorderAndPaddingExtent() };
}

// https://drafts.csswg.org/css-grid-2/#item-margins
// Percent/Calc padding and sizing resolves against the gridAreaInlineSize, not the block size of the grid container.
static LayoutUnit inlineWidthForGridItemWithGridArea(const LayoutState& layoutState, const ElementBox& box, LayoutIntegration::LogicalWidthType logicalWidthType, std::optional<LayoutUnit> gridAreaInlineSize,
    std::optional<LayoutUnit> gridAreaBlockSize, std::optional<LayoutUnit> overridingBorderBoxLogicalHeight)
{
    ASSERT(box.isGridItem());
    CheckedRef renderer = downcast<RenderBox>(*box.rendererForIntegration());

    // RenderGrid sets the grid item renderer's grid area content logical width during
    // item placement which is present until we have resolved the actual grid area
    // inline size.
    renderer->setGridAreaContentLogicalWidth(gridAreaInlineSize.value_or(0_lu));
    // A grid item measured before the rows are sized may have no grid area block size yet. Leave it
    // unset rather than set it to an indefinite size; see layoutGridItemWithFormattingContext().
    if (gridAreaBlockSize)
        renderer->setGridAreaContentLogicalHeight(*gridAreaBlockSize);
    // The block size the item is stretched to, which its inline size may be transferred from through
    // its preferred aspect ratio.
    if (overridingBorderBoxLogicalHeight)
        renderer->setOverridingBorderBoxLogicalHeight(*overridingBorderBoxLogicalHeight);
    renderer->invalidateContentLogicalWidths(MarkingBehavior::OnlyThis);

    auto width = layoutState.logicalWidthWithFormattingContextForBox(box, logicalWidthType);
    // The min-content and max-content widths exclude border and padding, while the contributions include them. Add them to the former,
    // under the same grid area basis while it is still set, so that every type returns a border-box size.
    // The item's own inline axis is the container's inline axis only for a parallel item; orthogonal items are rejected in inlineIntrinsicSizeForGridItem().
    if (logicalWidthType == LayoutIntegration::LogicalWidthType::MinContent || logicalWidthType == LayoutIntegration::LogicalWidthType::MaxContent)
        width += borderAndPadding(renderer.get(), box.parent().writingMode()).inlineAxis;

    if (overridingBorderBoxLogicalHeight)
        renderer->clearOverridingBorderBoxLogicalHeight();
    renderer->clearGridAreaContentSize();
    renderer->invalidateContentLogicalWidths(MarkingBehavior::OnlyThis);

    return width;
}

IntegrationUtils::IntegrationUtils(const LayoutState& globalLayoutState)
    : m_globalLayoutState(globalLayoutState)
{
}

void IntegrationUtils::layoutWithFormattingContextForBox(const ElementBox& box, std::optional<LayoutUnit> overridingBorderBoxLogicalWidth, std::optional<LayoutUnit> overridingBorderBoxLogicalHeight) const
{
    m_globalLayoutState->layoutWithFormattingContextForBox(box, overridingBorderBoxLogicalWidth, overridingBorderBoxLogicalHeight);
}

GridItemBorderAndPadding IntegrationUtils::borderAndPaddingForGridItem(const ElementBox& box, std::optional<LayoutUnit> gridAreaInlineSize) const
{
    ASSERT(box.isGridItem());
    CheckedRef renderer = downcast<RenderBox>(*box.rendererForIntegration());

    // The inline grid area is indefinite while sizing columns, so the item's cyclic percentage padding resolves against zero.
    renderer->setGridAreaContentLogicalWidth(gridAreaInlineSize.value_or(0_lu));
    auto gridItemBorderAndPadding = borderAndPadding(renderer.get(), box.parent().writingMode());
    renderer->clearGridAreaContentSize();

    return gridItemBorderAndPadding;
}

void IntegrationUtils::layoutGridItem(const ElementBox& box, GridItemOverridingBorderBoxSize overridingSize, LayoutUnit gridAreaInlineSize, std::optional<LayoutUnit> gridAreaBlockSize) const
{
    ASSERT(box.isGridItem());

    if (box.writingMode().isOrthogonal(box.parent().writingMode())) {
        // The renderer's overriding sizes are along the item's own axes, so the overriding size would be swapped here.
        // Orthogonal items stay on legacy through GridAvoidanceReason::GridItemIsOrthogonal.
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    LayoutIntegration::layoutGridItemWithFormattingContext(box, overridingSize.inlineAxis, overridingSize.blockAxis, gridAreaInlineSize, gridAreaBlockSize, const_cast<LayoutState&>(m_globalLayoutState.get()));
}

// The laid-out size comes from the renderer: BoxGeometry double-counts a fieldset's legend border and keeps a collapsed table's padding.
LayoutUnit IntegrationUtils::borderBoxSizeForGridItem(const ElementBox& box, LogicalBoxAxis axis) const
{
    ASSERT(box.isGridItem());
    CheckedRef renderer = downcast<RenderBox>(*box.rendererForIntegration());

    auto containerIsHorizontal = box.parent().writingMode().isHorizontal();
    return containerIsHorizontal == (axis == LogicalBoxAxis::Inline) ? renderer->borderBoxWidth() : renderer->borderBoxHeight();
}

std::optional<LayoutUnit> IntegrationUtils::firstLineBaselineForGridItem(const ElementBox& box) const
{
    ASSERT(box.isGridItem());
    return protect(downcast<RenderBox>(*box.rendererForIntegration()))->firstLineBaseline();
}

std::optional<LayoutUnit> IntegrationUtils::lastLineBaselineForGridItem(const ElementBox& box) const
{
    ASSERT(box.isGridItem());
    return protect(downcast<RenderBox>(*box.rendererForIntegration()))->lastLineBaseline();
}

LayoutUnit IntegrationUtils::maxContentWidth(const ElementBox& box) const
{
    ASSERT(box.isFlexItem());
    return m_globalLayoutState->logicalWidthWithFormattingContextForBox(box, LayoutIntegration::LogicalWidthType::MaxContent);
}

LayoutUnit IntegrationUtils::minContentWidth(const ElementBox& box) const
{
    ASSERT(box.isFlexItem());
    return m_globalLayoutState->logicalWidthWithFormattingContextForBox(box, LayoutIntegration::LogicalWidthType::MinContent);
}

LayoutUnit IntegrationUtils::minContentHeight(const ElementBox& box) const
{
    ASSERT(box.isFlexItem());
    return m_globalLayoutState->logicalHeightWithFormattingContextForBox(box, LayoutIntegration::LogicalHeightType::MinContent);
}

// The grid area inline size is the percentage basis for the item's padding and sizes (https://drafts.csswg.org/css-grid-2/#item-margins).
LayoutUnit IntegrationUtils::inlineIntrinsicSizeForGridItem(const ElementBox& box, GridItemIntrinsicSize intrinsicSize, std::optional<LayoutUnit> gridAreaInlineSize, std::optional<LayoutUnit> gridAreaBlockSize, std::optional<LayoutUnit> stretchedBlockSize) const
{
    ASSERT(box.isGridItem());

    if (box.writingMode().isOrthogonal(box.parent().writingMode())) {
        // The sizes below are along the item's own axes. Orthogonal items stay on legacy through GridAvoidanceReason::GridItemIsOrthogonal.
        ASSERT_NOT_IMPLEMENTED_YET();
        return { };
    }

    switch (intrinsicSize) {
    case GridItemIntrinsicSize::MinContent:
        return inlineWidthForGridItemWithGridArea(m_globalLayoutState, box, LayoutIntegration::LogicalWidthType::MinContent, gridAreaInlineSize, { }, { });
    case GridItemIntrinsicSize::MaxContent:
        return inlineWidthForGridItemWithGridArea(m_globalLayoutState, box, LayoutIntegration::LogicalWidthType::MaxContent, gridAreaInlineSize, { }, { });
    // The inline contributions are measured while sizing the columns, so the grid area inline size is not known yet.
    case GridItemIntrinsicSize::MinContentContribution:
        ASSERT(!gridAreaInlineSize);
        return inlineWidthForGridItemWithGridArea(m_globalLayoutState, box, LayoutIntegration::LogicalWidthType::MinContentContribution, { }, gridAreaBlockSize, stretchedBlockSize);
    case GridItemIntrinsicSize::MaxContentContribution:
        ASSERT(!gridAreaInlineSize);
        return inlineWidthForGridItemWithGridArea(m_globalLayoutState, box, LayoutIntegration::LogicalWidthType::MaxContentContribution, { }, gridAreaBlockSize, stretchedBlockSize);
    }
    ASSERT_NOT_REACHED();
    return { };
}

LayoutUnit IntegrationUtils::blockIntrinsicSizeForGridItem(const ElementBox& box, LayoutUnit gridAreaInlineSize) const
{
    ASSERT(box.isGridItem());

    if (box.writingMode().isOrthogonal(box.parent().writingMode())) {
        // The size below is along the item's own block axis. Orthogonal items stay on legacy through GridAvoidanceReason::GridItemIsOrthogonal.
        ASSERT_NOT_IMPLEMENTED_YET();
        return { };
    }

    // Every intrinsic kind of the block size (min-content, max-content and both contributions) is the size the item's layout produces, so there is
    // no kind to choose between.
    CheckedRef renderer = downcast<RenderBox>(*box.rendererForIntegration());
    renderer->setNeedsLayout(MarkingBehavior::OnlyThis);

    // The grid area block size is deliberately not set: the item is being measured to find the row size, so a percentage
    // height must not resolve against it (see layoutGridItemWithFormattingContext()).
    LayoutIntegration::layoutGridItemWithFormattingContext(box, { }, { }, gridAreaInlineSize, { }, const_cast<LayoutState&>(m_globalLayoutState.get()));

    return borderBoxSizeForGridItem(box, LogicalBoxAxis::Block);
}

LayoutUnit IntegrationUtils::minContentLogicalWidthContribution(const ElementBox& box) const
{
    ASSERT(!box.isGridItem());
    return m_globalLayoutState->logicalWidthWithFormattingContextForBox(box, LayoutIntegration::LogicalWidthType::MinContentContribution);
}

LayoutUnit IntegrationUtils::maxContentLogicalWidthContribution(const ElementBox& box) const
{
    ASSERT(!box.isGridItem());
    return m_globalLayoutState->logicalWidthWithFormattingContextForBox(box, LayoutIntegration::LogicalWidthType::MaxContentContribution);
}

void IntegrationUtils::layoutWithFormattingContextForBlockInInline(const ElementBox& block, LayoutPoint blockLineLogicalTopLeft, const InlineLayoutState& inlineLayoutState) const
{
    ASSERT(block.isBlockLevelBox());
    m_globalLayoutState->layoutWithFormattingContextForBlockInInline(block, blockLineLogicalTopLeft, inlineLayoutState);
}

Layout::BlockLayoutState::MarginState IntegrationUtils::toMarginState(const RenderBlockFlow::MarginInfo& marginInfo)
{
    return { marginInfo.canCollapseWithChildren(), marginInfo.canCollapseMarginBeforeWithChildren(), marginInfo.canCollapseMarginAfterWithChildren(), marginInfo.quirkContainer(), marginInfo.atBeforeSideOfBlock(), marginInfo.atAfterSideOfBlock(), marginInfo.hasMarginBeforeQuirk(), marginInfo.hasMarginAfterQuirk(), marginInfo.determinedMarginBeforeQuirk(), marginInfo.positiveMargin(), marginInfo.negativeMargin(), marginInfo.marginBeforeWithClearance() };
}

RenderBlockFlow::MarginInfo IntegrationUtils::toMarginInfo(const Layout::BlockLayoutState::MarginState& marginState)
{
    return { marginState.canCollapseWithChildren, marginState.canCollapseMarginBeforeWithChildren, marginState.canCollapseMarginAfterWithChildren, marginState.quirkContainer, marginState.atBeforeSideOfBlock, marginState.atAfterSideOfBlock, marginState.hasMarginBeforeQuirk, marginState.hasMarginAfterQuirk, marginState.determinedMarginBeforeQuirk, marginState.positiveMargin, marginState.negativeMargin, marginState.marginBeforeWithClearance };
}

std::pair<LayoutRect, LayoutRect> IntegrationUtils::toMarginAndBorderBoxVisualRect(const BoxGeometry& logicalGeometry, const LayoutSize& containerSize, WritingMode writingMode)
{
    // In certain writing modes, IFC gets the border box position wrong;
    // but the margin box is correct, so use it to derive the border box.
    auto marginBoxLogicalRect = BoxGeometry::marginBoxRect(logicalGeometry);
    auto containerLogicalWidth = writingMode.isHorizontal() ? containerSize.width() : containerSize.height();
    auto marginBoxLogicalX = writingMode.isInlineFlipped() ? containerLogicalWidth - marginBoxLogicalRect.right() : marginBoxLogicalRect.left();
    auto marginBoxVisualRect = writingMode.isHorizontal()
        ? LayoutRect { marginBoxLogicalX, marginBoxLogicalRect.top(), marginBoxLogicalRect.width(), marginBoxLogicalRect.height() }
        : LayoutRect { marginBoxLogicalRect.top(), marginBoxLogicalX, marginBoxLogicalRect.height(), marginBoxLogicalRect.width() };

    auto marginLeft = LayoutUnit { };
    auto marginTop = LayoutUnit { };
    auto marginWidth = LayoutUnit { };
    auto marginHeight = LayoutUnit { };

    if (writingMode.isHorizontal()) {
        marginLeft = writingMode.isInlineLeftToRight() ? logicalGeometry.marginStart() : logicalGeometry.marginEnd();
        marginTop = writingMode.isBlockTopToBottom() ? logicalGeometry.marginBefore() : logicalGeometry.marginAfter();
        marginWidth = logicalGeometry.marginStart() + logicalGeometry.marginEnd();
        marginHeight = logicalGeometry.marginBefore() + logicalGeometry.marginAfter();
    } else {
        // Invert verticalLogicalMargin() and convert to block-start relative coords.
        marginLeft = writingMode.isLineInverted() ? logicalGeometry.marginAfter() : logicalGeometry.marginBefore();
        marginTop = writingMode.isInlineTopToBottom() ? logicalGeometry.marginStart() : logicalGeometry.marginEnd();
        marginWidth = logicalGeometry.marginBefore() + logicalGeometry.marginAfter();
        marginHeight = logicalGeometry.marginStart() + logicalGeometry.marginEnd();
    }

    auto borderBoxVisualRect = marginBoxVisualRect;
    borderBoxVisualRect.expand(-marginWidth, -marginHeight);
    borderBoxVisualRect.move(marginLeft, marginTop);

    return { marginBoxVisualRect, borderBoxVisualRect };
}

}
}

