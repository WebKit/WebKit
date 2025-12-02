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
#include "LayoutIntegrationFormattingContextLayout.h"
#include "LayoutState.h"
#include "RenderObject.h"

namespace WebCore {
namespace Layout {

IntegrationUtils::IntegrationUtils(const LayoutState& globalLayoutState)
    : m_globalLayoutState(globalLayoutState)
{
}

void IntegrationUtils::layoutWithFormattingContextForBox(const ElementBox& box, std::optional<LayoutUnit> widthConstraint, std::optional<LayoutUnit> heightConstraint) const
{
    m_globalLayoutState->layoutWithFormattingContextForBox(box, widthConstraint, heightConstraint);
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

LayoutUnit IntegrationUtils::preferredMinWidth(const ElementBox& box) const
{
    ASSERT(box.isGridItem());
    return m_globalLayoutState->logicalWidthWithFormattingContextForBox(box, LayoutIntegration::LogicalWidthType::PreferredMinimum);
}


LayoutUnit IntegrationUtils::preferredMaxWidth(const ElementBox& box) const
{
    ASSERT(box.isGridItem());
    return m_globalLayoutState->logicalWidthWithFormattingContextForBox(box, LayoutIntegration::LogicalWidthType::PreferredMaximum);
}

void IntegrationUtils::layoutWithFormattingContextForBlockInInline(const ElementBox& block, LayoutPoint blockLogicalTopLeft, const InlineLayoutState& inlineLayoutState) const
{
    ASSERT(block.isBlockLevelBox());
    m_globalLayoutState->layoutWithFormattingContextForBlockInInline(block, blockLogicalTopLeft, inlineLayoutState);
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

