/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "BlockIntegrationUtils.h"

#include "InlineFormattingConstraints.h"
#include "LayoutBoxGeometry.h"
#include "LayoutIntegrationLineLayout.h"
#include "RenderBlockFlow.h"
#include "RenderBlockFlowInlines.h"

namespace WebCore {
namespace LayoutIntegration {

PaintInvalidation::InlineContentScope::InlineContentScope(PaintInvalidation& paintInvalidation, RenderBlockFlow& blockContainer, bool isFullLayout)
    : m_paintInvalidation(paintInvalidation)
    , m_blockContainer(blockContainer)
    , m_isFullLayout(isFullLayout)
    , m_contentTopAndBottomBeforeLayout(blockContainer.inlineContentTopAndBottomIncludingInkOverflow())
{
}

PaintInvalidation::InlineContentScope::~InlineContentScope()
{
    m_paintInvalidation.m_inlineContent.set(m_blockContainer.get(), InlineContent { m_isFullLayout, m_damagedRect, m_contentTopAndBottomBeforeLayout });
}

auto PaintInvalidation::damagedInlineContentLogicalTopAndBottom(RenderBlockFlow& blockContainer, LayoutUnit borderBoxBlockSizeFromContent) -> std::optional<LogicalTopAndBottom>
{
    auto inlineContent = m_inlineContent.takeOptional(blockContainer);
    if (!inlineContent)
        return { };

    auto logicalHeight = blockContainer.logicalHeight();
    blockContainer.setLogicalHeight(borderBoxBlockSizeFromContent);
    auto damaged = LogicalTopAndBottom { };
    blockContainer.updateRepaintTopAndBottomAfterLayout(inlineContent->isFullLayout ? RelayoutChildren::Yes : RelayoutChildren::No, inlineContent->damagedRect, inlineContent->contentTopAndBottomBeforeLayout, damaged.logicalTop, damaged.logicalBottom);
    blockContainer.setLogicalHeight(logicalHeight);
    return damaged;
}

BlockIntegrationUtils::BlockIntegrationUtils(PaintInvalidation& paintInvalidation)
    : m_paintInvalidation(paintInvalidation)
{
}

static BlockIntegrationUtils::InlineContentLayoutResult inlineContentLayoutResult(const LineLayout& lineLayout)
{
    if (lineLayout.isSelfCollapsingContent())
        return { { }, false };
    return { lineLayout.hasContentfulInlineOrBlockLine() ? lineLayout.contentLogicalHeight() : 0_lu, true };
}

static bool inlineContentConstraintsChanged(const LineLayout& lineLayout, const Layout::ConstraintsForInlineContent& constraints)
{
    auto& previousConstraints = lineLayout.inlineContentConstraints();
    return !previousConstraints
        || previousConstraints->horizontal().logicalLeft != constraints.horizontal().logicalLeft
        || previousConstraints->horizontal().logicalWidth != constraints.horizontal().logicalWidth
        || previousConstraints->logicalTop() != constraints.logicalTop()
        || previousConstraints->visualLeft() != constraints.visualLeft()
        || previousConstraints->formattingRootBorderBoxSize() != constraints.formattingRootBorderBoxSize();
}

BlockIntegrationUtils::InlineContentLayoutResult BlockIntegrationUtils::layoutInlineContent(const RenderBlockFlow& blockContainer, const Layout::ConstraintsForInlineContent& constraints, const Layout::BoxGeometry& geometry, const Layout::BlockLayoutState& blockLayoutState)
{
    // Only the line layout of this block container and its inline content are written here, and nothing else in the block formatting
    // context reads them.
    auto& inlineContentRoot = const_cast<RenderBlockFlow&>(blockContainer);

    // Like RenderBlockFlow::layoutBlockChild, which only lays out a block that needs it, and RenderBlock::recomputeLogicalWidth, which
    // lays its content out again when its width changed: otherwise the lines stay as they are. A block whose content only needs its overflow
    // recomputed or its out-of-flow boxes laid out keeps them too (RenderBlock::canPerformSimplifiedLayout).
    auto contentNeedsLayout = inlineContentRoot.selfNeedsLayout() || inlineContentRoot.normalChildNeedsLayout() || inlineContentRoot.outOfFlowChildNeedsStaticPositionLayout();
    if (CheckedPtr lineLayout = inlineContentRoot.inlineLayout(); lineLayout && !contentNeedsLayout && lineLayout->hasContent() && !inlineContentConstraintsChanged(*lineLayout, constraints)) {
        if (inlineContentRoot.needsSimplifiedNormalFlowLayout())
            inlineContentRoot.simplifiedNormalFlowLayout();
        return inlineContentLayoutResult(*lineLayout);
    }

    // Like RenderBlockFlow::layoutInlineContent, which lays the content out from scratch when the box's width changed (RenderBlock::recomputeLogicalWidth).
    auto invalidateChildren = inlineContentRoot.logicalWidth() != geometry.borderBoxWidth() || inlineContentRoot.hasBorderOrPaddingLogicalWidthChanged();
    inlineContentRoot.markInlineContentDirtyForLayout(invalidateChildren ? RelayoutChildren::Yes : RelayoutChildren::No);
    inlineContentRoot.computeAndSetLineLayoutPath();
    auto paintInvalidationScope = PaintInvalidation::InlineContentScope { paintInvalidation(), inlineContentRoot, invalidateChildren };
    inlineContentRoot.m_previousInlineLayoutContentTopAndBottomIncludingInkOverflow = { };

    if (!inlineContentRoot.inlineLayout())
        inlineContentRoot.m_lineLayout = makeUnique<LineLayout>(inlineContentRoot);
    CheckedRef lineLayout = *inlineContentRoot.inlineLayout();
    lineLayout->updateFormattingContextGeometries(constraints, geometry);
    // Without block-in-inline no margins cross into the inline content, so the margin state of the block layout state stays empty.
    paintInvalidationScope.setDamagedRect(lineLayout->layout(blockLayoutState, invalidateChildren ? LineLayout::ForceFullLayout::Yes : LineLayout::ForceFullLayout::No).repaintRect);
    return inlineContentLayoutResult(lineLayout.get());
}

} // namespace LayoutIntegration
} // namespace WebCore
