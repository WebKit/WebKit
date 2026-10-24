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

// https://drafts.csswg.org/css-grid-2/#item-margins
// Percent/Calc padding and sizing resolves against the gridAreaInlineSize, not the block size of the grid container.
static LayoutUnit inlineWidthForGridItemWithGridArea(const LayoutState& layoutState, const ElementBox& box, LayoutIntegration::LogicalWidthType logicalWidthType, std::optional<LayoutUnit> gridAreaInlineSize)
{
    ASSERT(box.isGridItem());
    CheckedRef renderer = downcast<RenderBox>(*box.rendererForIntegration());

    // RenderGrid sets the grid item renderer's grid area content logical width during
    // item placement which is present until we have resolved the actual grid area
    // inline size.
    renderer->setOverridingContainingBlockContentLogicalWidth(gridAreaInlineSize.value_or(0_lu));
    renderer->invalidateContentLogicalWidths(MarkingBehavior::MarkOnlyThis);

    auto width = layoutState.logicalWidthWithFormattingContextForBox(box, logicalWidthType);

    renderer->clearOverridingContainingBlockContentSize();
    renderer->invalidateContentLogicalWidths(MarkingBehavior::MarkOnlyThis);

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

std::pair<LayoutUnit, LayoutUnit> IntegrationUtils::borderAndPaddingForGridItem(const ElementBox& box, std::optional<LayoutUnit> gridAreaInlineSize) const
{
    ASSERT(box.isGridItem());
    CheckedRef renderer = downcast<RenderBox>(*box.rendererForIntegration());

    // The inline grid area is indefinite while sizing columns, so the item's cyclic percentage padding resolves against zero.
    renderer->setOverridingContainingBlockContentLogicalWidth(gridAreaInlineSize.value_or(0_lu));
    auto inlineBorderAndPadding = renderer->borderAndPaddingLogicalWidth();
    auto blockBorderAndPadding = renderer->borderAndPaddingLogicalHeight();
    renderer->clearOverridingContainingBlockContentSize();

    return { inlineBorderAndPadding, blockBorderAndPadding };
}

void IntegrationUtils::layoutGridItem(const ElementBox& box, std::optional<LayoutUnit> overridingBorderBoxLogicalWidth, std::optional<LayoutUnit> overridingBorderBoxLogicalHeight, LayoutUnit gridAreaInlineSize) const
{
    ASSERT(box.isGridItem());
    LayoutIntegration::layoutGridItemWithFormattingContext(box, overridingBorderBoxLogicalWidth, overridingBorderBoxLogicalHeight, gridAreaInlineSize, const_cast<LayoutState&>(m_globalLayoutState.get()));
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

// Max width for grid items is resolved against the gridAreaInlineSize, not the block size of the grid container.
LayoutUnit IntegrationUtils::maxContentWidthForGridItem(const ElementBox& box, std::optional<LayoutUnit> gridAreaInlineSize) const
{
    ASSERT(box.isGridItem());
    return inlineWidthForGridItemWithGridArea(m_globalLayoutState, box, LayoutIntegration::LogicalWidthType::MaxContent, gridAreaInlineSize);
}

// Min width for grid items is resolved against the gridAreaInlineSize, not the block size of the grid container.
LayoutUnit IntegrationUtils::minContentWidthForGridItem(const ElementBox& box, std::optional<LayoutUnit> gridAreaInlineSize) const
{
    ASSERT(box.isGridItem());
    return inlineWidthForGridItemWithGridArea(m_globalLayoutState, box, LayoutIntegration::LogicalWidthType::MinContent, gridAreaInlineSize);
}

LayoutUnit IntegrationUtils::minContentHeight(const ElementBox& box) const
{
    ASSERT(box.isFlexItem());
    return m_globalLayoutState->logicalHeightWithFormattingContextForBox(box, LayoutIntegration::LogicalHeightType::MinContent);
}

static LayoutUnit blockSizeForGridItem(const LayoutState& layoutState, const ElementBox& box, LayoutUnit gridAreaInlineSize, LayoutIntegration::LogicalHeightType logicalHeightType)
{
    ASSERT(box.isGridItem());
    CheckedRef renderer = downcast<RenderBox>(*box.rendererForIntegration());

    switch (logicalHeightType) {
    case LayoutIntegration::LogicalHeightType::MinContent:
    case LayoutIntegration::LogicalHeightType::MaxContent:
    case LayoutIntegration::LogicalHeightType::MinContentContribution:
    case LayoutIntegration::LogicalHeightType::MaxContentContribution: {
        renderer->setNeedsLayout(MarkingBehavior::MarkOnlyThis);

        LayoutIntegration::layoutGridItemWithFormattingContext(box, { }, { }, gridAreaInlineSize, const_cast<LayoutState&>(layoutState));

        return layoutState.geometryForBox(box).borderBoxHeight();
    }
    }

    ASSERT_NOT_REACHED();
    return { };
}

LayoutUnit IntegrationUtils::minContentHeightForGridItem(const ElementBox& box, LayoutUnit gridAreaInlineSize) const
{
    ASSERT(box.isGridItem());
    return blockSizeForGridItem(m_globalLayoutState, box, gridAreaInlineSize, LayoutIntegration::LogicalHeightType::MinContent);
}

LayoutUnit IntegrationUtils::maxContentHeightForGridItem(const ElementBox& box, LayoutUnit gridAreaInlineSize) const
{
    ASSERT(box.isGridItem());
    return blockSizeForGridItem(m_globalLayoutState, box, gridAreaInlineSize, LayoutIntegration::LogicalHeightType::MaxContent);
}

LayoutUnit IntegrationUtils::minContentContributionHeightForGridItem(const ElementBox& box, LayoutUnit gridAreaInlineSize) const
{
    ASSERT(box.isGridItem());
    return blockSizeForGridItem(m_globalLayoutState, box, gridAreaInlineSize, LayoutIntegration::LogicalHeightType::MinContentContribution);
}

LayoutUnit IntegrationUtils::maxContentContributionHeightForGridItem(const ElementBox& box, LayoutUnit gridAreaInlineSize) const
{
    ASSERT(box.isGridItem());
    return blockSizeForGridItem(m_globalLayoutState, box, gridAreaInlineSize, LayoutIntegration::LogicalHeightType::MaxContentContribution);
}

LayoutUnit IntegrationUtils::minContentLogicalWidthContribution(const ElementBox& box) const
{
    if (box.isGridItem())
        return inlineWidthForGridItemWithGridArea(m_globalLayoutState, box, LayoutIntegration::LogicalWidthType::MinContentContribution, { });
    return m_globalLayoutState->logicalWidthWithFormattingContextForBox(box, LayoutIntegration::LogicalWidthType::MinContentContribution);
}

LayoutUnit IntegrationUtils::maxContentLogicalWidthContribution(const ElementBox& box) const
{
    if (box.isGridItem())
        return inlineWidthForGridItemWithGridArea(m_globalLayoutState, box, LayoutIntegration::LogicalWidthType::MaxContentContribution, { });
    return m_globalLayoutState->logicalWidthWithFormattingContextForBox(box, LayoutIntegration::LogicalWidthType::MaxContentContribution);
}

void IntegrationUtils::layoutWithFormattingContextForBlockInInline(const ElementBox& block, LayoutPoint blockLineLogicalTopLeft, const InlineLayoutState& inlineLayoutState) const
{
    ASSERT(block.isBlockLevelBox());
    m_globalLayoutState->layoutWithFormattingContextForBlockInInline(block, blockLineLogicalTopLeft, inlineLayoutState);
}

Layout::BlockLayoutState::MarginState IntegrationUtils::toMarginState(const RenderBlockFlow::MarginInfo& marginInfo)
{
    auto marginState = BlockLayoutState::MarginState { };
    marginState.adjoiningMargins = { .positive = marginInfo.positiveMargin(), .negative = marginInfo.negativeMargin() };
    marginState.marginBeforeIncludedInClearance = marginInfo.marginBeforeWithClearance();
    marginState.atBlockContainerStart = marginInfo.atBeforeSideOfBlock();
    marginState.blockContainerMarginBeforeCollapsesWithContent = marginInfo.canCollapseMarginBeforeWithChildren();
    marginState.blockContainerMarginAfterCollapsesWithContent = marginInfo.canCollapseMarginAfterWithChildren();
    marginState.marginBeforeIsQuirky = marginInfo.hasMarginBeforeQuirk();
    marginState.marginAfterIsQuirky = marginInfo.hasMarginAfterQuirk();
    marginState.marginBeforeQuirkIsDetermined = marginInfo.determinedMarginBeforeQuirk();
    return marginState;
}

void IntegrationUtils::updateMarginInfo(RenderBlockFlow::MarginInfo& marginInfo, const BlockLayoutState::MarginState& marginState)
{
    ASSERT(marginInfo.canCollapseMarginBeforeWithChildren() == marginState.blockContainerMarginBeforeCollapsesWithContent);
    marginInfo.setMargin(marginState.adjoiningMargins.positive, marginState.adjoiningMargins.negative);
    marginInfo.setMarginBeforeWithClearance(marginState.marginBeforeIncludedInClearance);
    marginInfo.setAtBeforeSideOfBlock(marginState.atBlockContainerStart);
    marginInfo.setCanCollapseMarginAfterWithChildren(marginState.blockContainerMarginAfterCollapsesWithContent);
    marginInfo.setHasMarginBeforeQuirk(marginState.marginBeforeIsQuirky);
    marginInfo.setHasMarginAfterQuirk(marginState.marginAfterIsQuirky);
    marginInfo.setDeterminedMarginBeforeQuirk(marginState.marginBeforeQuirkIsDetermined);
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
        // Invert verticalLogicalMargin() and convert to unflipped coords.
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

