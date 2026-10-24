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

#include "BlockFormattingContext.h"
#include "BlockFormattingUtils.h"
#include "InlineFormattingConstraints.h"
#include "InlineWalker.h"
#include "LayoutBoxGeometry.h"
#include "LayoutIntegrationBlockCoverage.h"
#include "LayoutIntegrationLineLayout.h"
#include "LocalFrameViewInlines.h"
#include "RenderBlockFlow.h"
#include "RenderBlockFlowInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderLayer.h"
#include "RenderLayoutState.h"
#include "RenderView.h"

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
    // Like RenderBlockFlow::layoutInlineContent: inline content without a contentful line in an editable block container is as tall as an empty one.
    if (!lineLayout.hasContentfulInlineOrBlockLine()) {
        if (auto contentBlockSize = Layout::BlockFormattingUtils::emptyEditableContentBlockSize(lineLayout.flow()))
            return { *contentBlockSize, true };
    }
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

Layout::IntrinsicWidthConstraints BlockIntegrationUtils::inlineContentIntrinsicInlineSizes(const RenderBlockFlow& blockContainer)
{
    // Coverage keeps out the inline content the inline formatting context cannot measure (LineLayout::canUseForIntrinsicWidthComputation).
    auto minimum = LayoutUnit { };
    auto maximum = LayoutUnit { };
    auto didCompute = const_cast<RenderBlockFlow&>(blockContainer).tryComputeIntrinsicLogicalWidthsUsingInlinePath(minimum, maximum);
    ASSERT_UNUSED(didCompute, didCompute);
    return { minimum, maximum };
}

BlockIntegrationUtils::IndependentFormattingContextLayoutResult BlockIntegrationUtils::layoutIndependentFormattingContext(const RenderBox& root, const Layout::ConstraintsForIndependentFormattingContext& constraints)
{
    // Only the root and its content are written here, by the root's own formatting context, and nothing else in the block formatting context
    // reads them.
    SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef box = const_cast<RenderBox&>(root);
    CheckedRef containingBlock = downcast<RenderBlockFlow>(*box->parent());

    // Like RenderBlockFlow::layoutBlockChildren, which lays a child out again when the inline size of its containing block changed.
    containingBlock->updateBlockChildDirtyBitsBeforeLayout(constraints.relayoutChildren ? RelayoutChildren::Yes : RelayoutChildren::No, box);
    if (constraints.borderBoxInlineSize) {
        if (box->logicalWidth() != *constraints.borderBoxInlineSize)
            box->setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
        box->setOverridingBorderBoxLogicalWidth(*constraints.borderBoxInlineSize);
    }
    // The block formatting context writes the geometry of the containing block once all of it is laid out. The render tree takes a containing
    // block block size, definite or not, as the box's own (RenderBox::hasAutoHeightOrContainingBlockWithAutoHeight), so only a box with a
    // percentage block size, which resolves against it, gets one.
    box->setOverridingContainingBlockContentLogicalWidth(constraints.containingBlockInlineSize);
    if (Layout::BlockFormattingUtils::hasPercentageBlockSize(box, box->writingMode()))
        box->setOverridingContainingBlockContentLogicalHeight(constraints.containingBlockBlockSize);
    {
        // The layout state has the paint offset of the block formatting context root only, not of the boxes inside.
        CheckedRef layoutContext = box->view().frameView().layoutContext();
        auto layoutStateDisabler = LayoutStateDisabler { layoutContext.get() };
        box->layoutIfNeeded();
    }
    box->clearOverridingContainingBlockContentSize();
    if (constraints.borderBoxInlineSize)
        box->clearOverridingBorderBoxLogicalWidth();

    return {
        box->logicalWidth(),
        box->logicalHeight(),
        { { box->borderStart(), box->borderEnd() }, { box->borderBefore(), box->borderAfter() } },
        { { box->paddingStart(), box->paddingEnd() }, { box->paddingBefore(), box->paddingAfter() } }
    };
}

LayoutSize BlockIntegrationUtils::layoutOutOfFlowIndependentFormattingContext(const RenderBox& root, std::optional<LayoutUnit> borderBoxInlineSize, std::optional<LayoutUnit> borderBoxBlockSize)
{
    SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef box = const_cast<RenderBox&>(root);
    // Its size may depend on the size of its containing block, which the block formatting context has just laid out.
    box->setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
    if (borderBoxInlineSize)
        box->setOverridingBorderBoxLogicalWidth(*borderBoxInlineSize);
    if (borderBoxBlockSize)
        box->setOverridingBorderBoxLogicalHeight(*borderBoxBlockSize);
    {
        // The layout state has the paint offset of the block formatting context root only, not of the boxes inside.
        CheckedRef layoutContext = box->view().frameView().layoutContext();
        auto layoutStateDisabler = LayoutStateDisabler { layoutContext.get() };
        box->layoutIfNeeded();
    }
    if (borderBoxInlineSize)
        box->clearOverridingBorderBoxLogicalWidth();
    if (borderBoxBlockSize)
        box->clearOverridingBorderBoxLogicalHeight();
    return { box->logicalWidth(), box->logicalHeight() };
}

Layout::IntrinsicWidthConstraints BlockIntegrationUtils::independentFormattingContextIntrinsicInlineSizes(const RenderBox& box, LayoutUnit borderAndPadding)
{
    // Like RenderBox::computeOutOfFlowPositionedLogicalWidthUsing for a fit-content size.
    return { box.minContentLogicalWidthContribution() - borderAndPadding, box.maxContentLogicalWidthContribution() - borderAndPadding };
}

bool BlockIntegrationUtils::contentBoxInlineSizeChanged(const RenderBox& box, LayoutUnit contentBoxInlineSize)
{
    if (box.contentBoxLogicalWidth() != contentBoxInlineSize)
        return true;
    CheckedPtr block = dynamicDowncast<RenderBlock>(box);
    return block && block->hasBorderOrPaddingLogicalWidthChanged();
}

void BlockIntegrationUtils::markPercentageHeightDescendantsForLayout(const RenderBlockFlow& blockContainer)
{
    // Only the dirty bits of boxes laid out through the render tree inside this block container are written.
    SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef mutableBlockContainer = const_cast<RenderBlockFlow&>(blockContainer);
    mutableBlockContainer->dirtyForLayoutFromPercentageHeightDescendants();
}

bool BlockIntegrationUtils::laysOutThroughRenderTree(const RenderBox& box)
{
    // FIXME: A block formatting context root whose content the block formatting context cannot lay out yet runs render tree block layout
    // (RenderBlockFlow::layoutBlock). It goes back to the block formatting context as coverage grows.
    CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(box);
    return !blockContainer || !canUseForNestedBlockLayout(*blockContainer);
}

BlockIntegrationUtils::InlineContentLayoutResult BlockIntegrationUtils::layoutInlineContent(const RenderBlockFlow& blockContainer, const Layout::ConstraintsForInlineContent& constraints, const Layout::BoxGeometry& geometry, std::optional<LayoutUnit> contentBoxBlockSize, const Layout::BlockLayoutState& blockLayoutState)
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
    auto inlineContentStatus = inlineContentRoot.markInlineContentDirtyForLayout(invalidateChildren ? RelayoutChildren::Yes : RelayoutChildren::No);
    if (inlineContentStatus.hasSimpleOutOfFlowContentOnly) {
        // Like RenderBlockFlow::layoutInlineContent: content of out-of-flow boxes only has no lines, and the static position of the boxes is
        // the start of the content box (RenderBlockFlow::setStaticPositionsForSimpleOutOfFlowContent).
        inlineContentRoot.m_lineLayout = nullptr;
        for (auto walker = InlineWalker(inlineContentRoot); !walker.atEnd(); walker.advance()) {
            CheckedRef layer = *downcast<RenderBox>(*walker.current()).layer();
            layer->setStaticInlinePosition(geometry.borderAndPaddingStart());
            layer->setStaticBlockPosition(geometry.borderAndPaddingBefore());
        }
        return { };
    }
    inlineContentRoot.computeAndSetLineLayoutPath();
    auto paintInvalidationScope = PaintInvalidation::InlineContentScope { paintInvalidation(), inlineContentRoot, invalidateChildren };
    inlineContentRoot.m_previousInlineLayoutContentTopAndBottomIncludingInkOverflow = { };

    if (!inlineContentRoot.inlineLayout())
        inlineContentRoot.m_lineLayout = makeUnique<LineLayout>(inlineContentRoot);
    CheckedRef lineLayout = *inlineContentRoot.inlineLayout();
    lineLayout->updateFormattingContextGeometries(constraints, geometry);
    // The inline formatting context lays out the atomic inline-level boxes through the render tree, and they read the size of their containing
    // block, this block container, which the block formatting context writes once all of it is laid out.
    auto atomicInlineLevelBoxes = Vector<CheckedRef<RenderBox>> { };
    for (CheckedPtr<RenderObject> descendant = inlineContentRoot.firstChild(); descendant;) {
        CheckedPtr box = dynamicDowncast<RenderBox>(*descendant);
        if (!box) {
            descendant = descendant->nextInPreOrder(&inlineContentRoot);
            continue;
        }
        box->setOverridingContainingBlockContentLogicalWidth(constraints.horizontal().logicalWidth);
        box->setOverridingContainingBlockContentLogicalHeight(contentBoxBlockSize);
        atomicInlineLevelBoxes.append(*box);
        descendant = descendant->nextInPreOrderAfterChildren(&inlineContentRoot);
    }
    {
        // The layout state has the paint offset of the block formatting context root only, not of the boxes inside.
        CheckedRef layoutContext = inlineContentRoot.view().frameView().layoutContext();
        auto layoutStateDisabler = LayoutStateDisabler { layoutContext.get() };
        // Without block-in-inline no margins cross into the inline content, so the margin state of the block layout state stays empty.
        paintInvalidationScope.setDamagedRect(lineLayout->layout(blockLayoutState, invalidateChildren ? LineLayout::ForceFullLayout::Yes : LineLayout::ForceFullLayout::No).repaintRect);
    }
    for (auto& box : atomicInlineLevelBoxes)
        box->clearOverridingContainingBlockContentSize();
    return inlineContentLayoutResult(lineLayout.get());
}

} // namespace LayoutIntegration
} // namespace WebCore
