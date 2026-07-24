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
#include "RenderObject.h"
#include "RenderObjectInlines.h"

namespace WebCore {
namespace Layout {

// https://drafts.csswg.org/css-grid-2/#item-margins
// Percent/Calc padding and sizing resolves against the gridAreaInlineSize, not the block size of the grid container.
static LayoutUnit inlineWidthForGridItemWithGridArea(const LayoutState& layoutState, const ElementBox& box, LayoutIntegration::LogicalWidthType logicalWidthType, LayoutUnit gridAreaInlineSize)
{
    ASSERT(box.isGridItem());
    CheckedRef renderer = downcast<RenderBox>(*box.rendererForIntegration());

    renderer->setGridAreaContentLogicalWidth(gridAreaInlineSize);
    renderer->invalidateContentLogicalWidths(MarkingBehavior::MarkOnlyThis);

    auto width = layoutState.logicalWidthWithFormattingContextForBox(box, logicalWidthType);

    renderer->clearGridAreaContentSize();
    renderer->invalidateContentLogicalWidths(MarkingBehavior::MarkOnlyThis);

    return width;
}

IntegrationUtils::IntegrationUtils(const LayoutState& globalLayoutState)
    : m_globalLayoutState(globalLayoutState)
{
}

void IntegrationUtils::layoutWithFormattingContextForBox(const ElementBox& box, std::optional<LayoutUnit> widthConstraint, std::optional<LayoutUnit> heightConstraint) const
{
    m_globalLayoutState->layoutWithFormattingContextForBox(box, widthConstraint, heightConstraint);
}

std::pair<LayoutUnit, LayoutUnit> IntegrationUtils::borderAndPaddingForGridItem(const ElementBox& box, LayoutUnit gridAreaInlineSize) const
{
    ASSERT(box.isGridItem());
    CheckedRef renderer = downcast<RenderBox>(*box.rendererForIntegration());

    renderer->setGridAreaContentLogicalWidth(gridAreaInlineSize);
    auto inlineBorderAndPadding = renderer->borderAndPaddingLogicalWidth();
    auto blockBorderAndPadding = renderer->borderAndPaddingLogicalHeight();
    renderer->clearGridAreaContentSize();

    return { inlineBorderAndPadding, blockBorderAndPadding };
}

void IntegrationUtils::layoutGridItem(const ElementBox& box, std::optional<LayoutUnit> widthConstraint, std::optional<LayoutUnit> heightConstraint, LayoutUnit gridAreaInlineSize) const
{
    ASSERT(box.isGridItem());
    LayoutIntegration::layoutGridItemWithFormattingContext(box, widthConstraint, heightConstraint, gridAreaInlineSize, const_cast<LayoutState&>(m_globalLayoutState.get()));
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
LayoutUnit IntegrationUtils::maxContentWidthForGridItem(const ElementBox& box, LayoutUnit gridAreaInlineSize) const
{
    ASSERT(box.isGridItem());
    return inlineWidthForGridItemWithGridArea(m_globalLayoutState, box, LayoutIntegration::LogicalWidthType::MaxContent, gridAreaInlineSize);
}

// Min width for grid items is resolved against the gridAreaInlineSize, not the block size of the grid container.
LayoutUnit IntegrationUtils::minContentWidthForGridItem(const ElementBox& box, LayoutUnit gridAreaInlineSize) const
{
    ASSERT(box.isGridItem());
    return inlineWidthForGridItemWithGridArea(m_globalLayoutState, box, LayoutIntegration::LogicalWidthType::MinContent, gridAreaInlineSize);
}

LayoutUnit IntegrationUtils::minContentHeight(const ElementBox& box) const
{
    ASSERT(box.isFlexItem());
    return m_globalLayoutState->logicalHeightWithFormattingContextForBox(box, LayoutIntegration::LogicalHeightType::MinContent);
}

static LayoutUnit blockSizeForGridItem(const LayoutState& layoutState, const ElementBox& box, LayoutUnit inlineAxisConstraint, LayoutIntegration::LogicalHeightType logicalHeightType)
{
    ASSERT(box.isGridItem());
    CheckedRef renderer = downcast<RenderBox>(*box.rendererForIntegration());

    switch (logicalHeightType) {
    case LayoutIntegration::LogicalHeightType::MinContent:
    case LayoutIntegration::LogicalHeightType::MaxContent:
    case LayoutIntegration::LogicalHeightType::MinContentContribution:
    case LayoutIntegration::LogicalHeightType::MaxContentContribution: {
        renderer->setGridAreaContentLogicalWidth(inlineAxisConstraint);
        renderer->setNeedsLayout(MarkingBehavior::MarkOnlyThis);

        layoutState.layoutWithFormattingContextForBox(box, { }, { });

        renderer->clearGridAreaContentSize();

        return layoutState.geometryForBox(box).borderBoxHeight();
    }
    }

    ASSERT_NOT_REACHED();
    return { };
}

LayoutUnit IntegrationUtils::minContentHeightForGridItem(const ElementBox& box, LayoutUnit inlineAxisConstraint) const
{
    ASSERT(box.isGridItem());
    return blockSizeForGridItem(m_globalLayoutState, box, inlineAxisConstraint, LayoutIntegration::LogicalHeightType::MinContent);
}

LayoutUnit IntegrationUtils::maxContentHeightForGridItem(const ElementBox& box, LayoutUnit inlineAxisConstraint) const
{
    ASSERT(box.isGridItem());
    return blockSizeForGridItem(m_globalLayoutState, box, inlineAxisConstraint, LayoutIntegration::LogicalHeightType::MaxContent);
}

LayoutUnit IntegrationUtils::minContentContributionHeightForGridItem(const ElementBox& box, LayoutUnit inlineAxisConstraint) const
{
    ASSERT(box.isGridItem());
    return blockSizeForGridItem(m_globalLayoutState, box, inlineAxisConstraint, LayoutIntegration::LogicalHeightType::MinContentContribution);
}

LayoutUnit IntegrationUtils::maxContentContributionHeightForGridItem(const ElementBox& box, LayoutUnit inlineAxisConstraint) const
{
    ASSERT(box.isGridItem());
    return blockSizeForGridItem(m_globalLayoutState, box, inlineAxisConstraint, LayoutIntegration::LogicalHeightType::MaxContentContribution);
}

LayoutUnit IntegrationUtils::minContentLogicalWidthContribution(const ElementBox& box) const
{
    ASSERT(box.isGridItem());
    return inlineWidthForGridItemWithGridArea(m_globalLayoutState, box, LayoutIntegration::LogicalWidthType::MinContentContribution, 0_lu);
}


LayoutUnit IntegrationUtils::maxContentLogicalWidthContribution(const ElementBox& box) const
{
    ASSERT(box.isGridItem());
    return inlineWidthForGridItemWithGridArea(m_globalLayoutState, box, LayoutIntegration::LogicalWidthType::MaxContentContribution, 0_lu);
}

void IntegrationUtils::layoutWithFormattingContextForBlockInInline(const ElementBox& block, LayoutPoint blockLineLogicalTopLeft, const InlineLayoutState& inlineLayoutState) const
{
    ASSERT(block.isBlockLevelBox());
    m_globalLayoutState->layoutWithFormattingContextForBlockInInline(block, blockLineLogicalTopLeft, inlineLayoutState);
}

Layout::BlockLayoutState::MarginState IntegrationUtils::toMarginState(const RenderBlockFlow::MarginInfo& marginInfo)
{
    return { marginInfo.canCollapseWithChildren(), marginInfo.canCollapseMarginBeforeWithChildren(), marginInfo.canCollapseMarginAfterWithChildren(), marginInfo.quirkContainer(), marginInfo.atBeforeSideOfBlock(), marginInfo.atAfterSideOfBlock(), marginInfo.hasMarginBeforeQuirk(), marginInfo.hasMarginAfterQuirk(), marginInfo.determinedMarginBeforeQuirk(), marginInfo.positiveMargin(), marginInfo.negativeMargin() };
}

RenderBlockFlow::MarginInfo IntegrationUtils::toMarginInfo(const Layout::BlockLayoutState::MarginState& marginState)
{
    return { marginState.canCollapseWithChildren, marginState.canCollapseMarginBeforeWithChildren, marginState.canCollapseMarginAfterWithChildren, marginState.quirkContainer, marginState.atBeforeSideOfBlock, marginState.atAfterSideOfBlock, marginState.hasMarginBeforeQuirk, marginState.hasMarginAfterQuirk, marginState.determinedMarginBeforeQuirk, marginState.positiveMargin, marginState.negativeMargin };
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

