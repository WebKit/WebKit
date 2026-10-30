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
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePrimitiveNumericTypes+EvaluationMinimum.h"

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
        CheckedRef blockContainer = lineLayout.flow();
        if (auto contentBlockSize = Layout::BlockFormattingUtils::emptyEditableContentBlockSize(blockContainer))
            return { *contentBlockSize, true };
    }
    if (lineLayout.blockLevelBoxAtEnd())
        return { { }, true, true };
    if (lineLayout.isSelfCollapsingContent())
        return { { }, false };
    return { lineLayout.hasContentfulInlineOrBlockLine() ? lineLayout.contentLogicalHeight() : 0_lu, true };
}

// Like RenderBlockFlow::layoutBlockChild with a child that does not need layout: the block-level boxes in lines kept from their last layout keep
// the margins it gave them (RenderBlockFlow::marginValuesForChild). Content whose margins all collapse through it, or that ends with a box whose
// margins collapse through it, joins its margins with the ones before it, and is laid out again.
std::optional<BlockIntegrationUtils::InlineContentLayoutResult> BlockIntegrationUtils::keptInlineContentWithBlockLevelBoxesLayoutResult(const RenderBlockFlow& blockContainer, const LineLayout& lineLayout)
{
    if (lineLayout.isSelfCollapsingContent())
        return { };
    auto result = inlineContentLayoutResult(lineLayout);
    auto marginsBefore = Layout::AdjoiningMargins { blockContainer.maxPositiveMarginBefore(), blockContainer.maxNegativeMarginBefore() };
    auto marginsAfter = Layout::AdjoiningMargins { };
    auto marginsAfterAreQuirky = false;
    if (CheckedPtr box = lineLayout.blockLevelBoxAtEnd()) {
        CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(*box);
        if (!blockFlow || box->isSelfCollapsingBlock())
            return { };
        result.contentBlockSize = blockFlow->logicalBottom() - blockContainer.borderAndPaddingBefore();
        marginsAfter = { blockFlow->maxPositiveMarginAfter(), blockFlow->maxNegativeMarginAfter() };
        marginsAfterAreQuirky = blockFlow->hasMarginAfterQuirk();
    }
    result.marginsOfKeptBlockLevelBoxes = { marginsBefore, marginsAfter, marginsAfterAreQuirky };
    return result;
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
    auto [minimum, maximum] = blockContainer.computeInlineIntrinsicLogicalWidths();
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
            box->setChildNeedsLayout(MarkingBehavior::OnlyThis);
        box->setOverridingBorderBoxLogicalWidth(*constraints.borderBoxInlineSize);
    }
    // The block formatting context writes the geometry of the containing block once all of it is laid out. The render tree takes a containing
    // block block size, definite or not, as the box's own (RenderBox::hasAutoHeightOrContainingBlockWithAutoHeight), so only a box with a
    // percentage or stretch block size, which resolves against it, gets one. An orthogonal box resolves that against the inline size of the containing
    // block instead (RenderBox::computePercentageLogicalHeight), and takes its available inline space from the containing block's style
    // (RenderBox::perpendicularContainingBlockLogicalHeight, coverage).
    auto containingBlockWritingMode = containingBlock->writingMode();
    auto isOrthogonal = box->writingMode().isOrthogonal(containingBlockWritingMode);
    box->setOverridingContainingBlockContentLogicalWidth(constraints.containingBlockInlineSize);
    if (!isOrthogonal && Layout::BlockFormattingUtils::hasPercentageBlockSize(box, box->writingMode()))
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

    auto borderBoxSize = containingBlockWritingMode.isHorizontal() ? box->borderBoxSize() : box->borderBoxSize().transposedSize();
    auto border = box->borderWidths();
    auto padding = box->padding();
    return {
        borderBoxSize.width(),
        borderBoxSize.height(),
        { { border.start(containingBlockWritingMode), border.end(containingBlockWritingMode) }, { border.before(containingBlockWritingMode), border.after(containingBlockWritingMode) } },
        { { padding.start(containingBlockWritingMode), padding.end(containingBlockWritingMode) }, { padding.before(containingBlockWritingMode), padding.after(containingBlockWritingMode) } }
    };
}

LayoutSize BlockIntegrationUtils::layoutOutOfFlowIndependentFormattingContext(const RenderBox& root, std::optional<LayoutUnit> borderBoxInlineSize, std::optional<LayoutUnit> borderBoxBlockSize)
{
    SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef box = const_cast<RenderBox&>(root);
    // Its size may depend on the size of its containing block, which the block formatting context has just laid out.
    box->setChildNeedsLayout(MarkingBehavior::OnlyThis);
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

Layout::IntrinsicWidthConstraints BlockIntegrationUtils::independentFormattingContextIntrinsicInlineSizeContributions(const RenderBox& box)
{
    // Like RenderBlock::computeChildIntrinsicLogicalWidths: a min-content or max-content inline size of a block makes both contributions
    // that size (css-sizing-3 §5.2.1).
    auto contributions = Layout::IntrinsicWidthConstraints { box.minContentLogicalWidthContribution(), box.maxContentLogicalWidthContribution() };
    if (is<RenderBlock>(box)) {
        auto& inlineSize = box.style().logicalWidth();
        if (inlineSize.isMaxContent())
            contributions.minimum = contributions.maximum;
        else if (inlineSize.isMinContent())
            contributions.maximum = contributions.minimum;
    }
    return contributions;
}

Layout::BoxGeometry BlockIntegrationUtils::rootGeometry(const RenderBlockFlow& root)
{
    // In the root's writing mode, which is its content's. An atomic inline-level root shares its geometry with the inline formatting context it
    // is in, which places it with its margins, so these are the ones LayoutIntegration::BoxGeometryUpdater::setFormattingContextRootGeometry
    // gives it: from style, before the root's own layout computes its block-direction ones, and line-relative in the block axis, from the
    // line-over side (BoxGeometryUpdater::verticalLogicalMargin).
    CheckedRef style = root.style();
    auto writingMode = root.writingMode();
    auto containingBlockInlineSize = root.containingBlockLogicalWidthForContent();
    auto margin = [&](auto& margin) {
        return margin.isAuto() ? 0_lu : Style::evaluateMinimum<LayoutUnit>(margin, containingBlockInlineSize, style->usedZoomForLength());
    };
    auto marginLineOver = margin(style->marginBefore(writingMode));
    auto marginLineUnder = margin(style->marginAfter(writingMode));
    if (writingMode.isLineInverted())
        std::swap(marginLineOver, marginLineUnder);
    auto geometry = Layout::BoxGeometry { };
    geometry.setHorizontalMargin({ margin(style->marginStart(writingMode)), margin(style->marginEnd(writingMode)) });
    geometry.setVerticalMargin({ marginLineOver, marginLineUnder });
    geometry.setBorder({ { root.borderStart(), root.borderEnd() }, { root.borderBefore(), root.borderAfter() } });
    geometry.setPadding({ { root.paddingStart(), root.paddingEnd() }, { root.paddingBefore(), root.paddingAfter() } });
    geometry.setContentBoxWidth(root.contentBoxLogicalWidth());
    // Scrollbars take the space they need from the padding box, up to all of it (LayoutIntegration::BoxGeometryUpdater's scrollbarLogicalSize).
    auto paddingBoxIncludingScrollbar = root.paddingBoxRectIncludingScrollbar();
    auto spaceForScrollbar = LayoutSize { std::max(0_lu, paddingBoxIncludingScrollbar.width() - root.paddingBoxWidth()), std::max(0_lu, paddingBoxIncludingScrollbar.height() - root.paddingBoxHeight()) };
    if (!writingMode.isHorizontal())
        spaceForScrollbar = spaceForScrollbar.transposedSize();
    geometry.setHorizontalSpaceForScrollbar(spaceForScrollbar.width());
    geometry.setVerticalSpaceForScrollbar(spaceForScrollbar.height());
    return geometry;
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
    // Only the dirty bits of this block container and of the boxes laid out through the render tree inside it are written. RenderBlockFlow::layoutBlock
    // lays out its content right after; the block container is marked too, so that its inline content, which lays out those boxes, is not kept
    // as it is (layoutInlineContent).
    SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef mutableBlockContainer = const_cast<RenderBlockFlow&>(blockContainer);
    auto* descendants = mutableBlockContainer->percentHeightDescendants();
    if (!descendants || descendants->isEmptyIgnoringNullReferences())
        return;
    mutableBlockContainer->dirtyForLayoutFromPercentageHeightDescendants();
    mutableBlockContainer->setChildNeedsLayout(MarkingBehavior::OnlyThis);
}

bool BlockIntegrationUtils::laysOutThroughRenderTree(const RenderBox& box)
{
    // FIXME: A block formatting context root whose content the block formatting context cannot lay out yet runs render tree block layout
    // (RenderBlockFlow::layoutBlock). It goes back to the block formatting context as coverage grows.
    CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(box);
    return !blockContainer || !canUseForNestedBlockLayout(*blockContainer);
}

BlockIntegrationUtils::InlineContentLayoutResult BlockIntegrationUtils::layoutInlineContent(const RenderBlockFlow& blockContainer, const Layout::ConstraintsForInlineContent& constraints, const Layout::BoxGeometry& geometry, std::optional<LayoutUnit> contentBoxBlockSize, bool relayoutChildren, const Layout::BlockLayoutState& blockLayoutState, Layout::BlockFormattingContext& blockFormattingContext, Layout::BlockLayoutResult& blockLayoutResult)
{
    // Only the line layout of this block container and its inline content are written here, and nothing else in the block formatting
    // context reads them.
    auto& inlineContentRoot = const_cast<RenderBlockFlow&>(blockContainer);

    // Like RenderBlockFlow::layoutBlockChild, which only lays out a block that needs it, and RenderBlock::recomputeLogicalWidth, which
    // lays its content out again when its width changed: otherwise the lines stay as they are. A block whose content only needs its overflow
    // recomputed or its out-of-flow boxes laid out keeps them too (RenderBlock::canPerformSimplifiedLayout).
    auto contentNeedsLayout = inlineContentRoot.selfNeedsLayout() || inlineContentRoot.normalChildNeedsLayout() || inlineContentRoot.outOfFlowChildNeedsStaticPositionLayout();
    if (CheckedPtr lineLayout = inlineContentRoot.inlineLayout(); lineLayout && !contentNeedsLayout && lineLayout->hasContent() && !inlineContentConstraintsChanged(*lineLayout, constraints)) {
        auto keptContentLayoutResult = lineLayout->hasBlocks() ? keptInlineContentWithBlockLevelBoxesLayoutResult(inlineContentRoot, *lineLayout) : std::optional { inlineContentLayoutResult(*lineLayout) };
        if (keptContentLayoutResult) {
            if (inlineContentRoot.needsSimplifiedNormalFlowLayout())
                inlineContentRoot.simplifiedNormalFlowLayout();
            return *keptContentLayoutResult;
        }
    }

    // Like RenderBlockFlow::layoutInlineContent, which lays the content out from scratch when the box's width changed (RenderBlock::recomputeLogicalWidth).
    // The renderer of the root has its new geometry already, and RenderBlock::recomputeLogicalWidth told its layout about the change.
    auto invalidateChildren = relayoutChildren || inlineContentRoot.logicalWidth() != geometry.borderBoxWidth() || inlineContentRoot.hasBorderOrPaddingLogicalWidthChanged();
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
    // block, this block container, which the block formatting context writes once all of it is laid out. Like a leaf, only a box with a
    // percentage block size gets its block size (layoutIndependentFormattingContext).
    auto atomicInlineLevelBoxes = Vector<CheckedRef<RenderBox>> { };
    for (CheckedPtr<RenderObject> descendant = inlineContentRoot.firstChild(); descendant;) {
        CheckedPtr box = dynamicDowncast<RenderBox>(*descendant);
        if (!box) {
            descendant = descendant->nextInPreOrder(&inlineContentRoot);
            continue;
        }
        // The block formatting context lays out the in-flow block-level boxes.
        if (box->isInFlow() && box->isBlockLevelBox()) {
            descendant = descendant->nextInPreOrderAfterChildren(&inlineContentRoot);
            continue;
        }
        box->setOverridingContainingBlockContentLogicalWidth(constraints.horizontal().logicalWidth);
        if (!box->writingMode().isOrthogonal(inlineContentRoot.writingMode()) && Layout::BlockFormattingUtils::hasPercentageBlockSize(*box, box->writingMode()))
            box->setOverridingContainingBlockContentLogicalHeight(contentBoxBlockSize);
        atomicInlineLevelBoxes.append(*box);
        descendant = descendant->nextInPreOrderAfterChildren(&inlineContentRoot);
    }
    {
        // The layout state has the paint offset of the block formatting context root only, not of the boxes inside.
        CheckedRef layoutContext = inlineContentRoot.view().frameView().layoutContext();
        auto layoutStateDisabler = LayoutStateDisabler { layoutContext.get() };
        // The block formatting context needs the margins of every block-level box in the content, so it lays out all the lines again
        // (like RenderBlockFlow::layoutInlineContent when one of them is dirty).
        auto hasBlockLevelBoxes = inlineContentStatus.hasDirtyInFlowBlockLevelElement || lineLayout->hasBlocks();
        auto forceFullLayout = invalidateChildren || hasBlockLevelBoxes ? LineLayout::ForceFullLayout::Yes : LineLayout::ForceFullLayout::No;
        paintInvalidationScope.setDamagedRect(lineLayout->layout(blockLayoutState, blockFormattingContext, blockLayoutResult, forceFullLayout).repaintRect);
    }
    for (auto& box : atomicInlineLevelBoxes)
        box->clearOverridingContainingBlockContentSize();
    return inlineContentLayoutResult(lineLayout.get());
}

} // namespace LayoutIntegration
} // namespace WebCore
