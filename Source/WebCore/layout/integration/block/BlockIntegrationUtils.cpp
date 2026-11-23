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
#include "InlineIteratorBox.h"
#include "InlineIteratorLineBox.h"
#include "InlineIteratorLineBoxInlines.h"
#include "InlineWalker.h"
#include "LayoutBoxGeometry.h"
#include "LayoutIntegrationBlockCoverage.h"
#include "LayoutIntegrationLineLayout.h"
#include "LineClampUpdater.h"
#include "LocalFrameViewInlines.h"
#include "LocalFrameViewLayoutContext.h"
#include "RenderBlockFlow.h"
#include "RenderBlockFlowInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderElementInlines.h"
#include "RenderObjectInlines.h"
#include "RenderHTMLCanvas.h"
#include "RenderLayer.h"
#include "RenderLayoutState.h"
#include "RenderLineBreak.h"
#include "RenderListItem.h"
#include "RenderText.h"
#include "RenderView.h"
#include "ShapeOutsideInfo.h"
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
    auto result = [&] -> BlockIntegrationUtils::InlineContentLayoutResult {
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
    }();
    result.lineCount = lineLayout.lineCount();
    return result;
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

// The floats that intruded into the content when it was last laid out are not the ones that intrude into it now.
static bool intrudingFloatsChanged(const LineLayout& lineLayout, const Layout::PlacedFloats::List& intrudingFloats)
{
    auto previousIntrudingFloats = lineLayout.intrudingFloats();
    if (previousIntrudingFloats.size() != intrudingFloats.size())
        return true;
    for (size_t index = 0; index < intrudingFloats.size(); ++index) {
        auto& previousFloat = previousIntrudingFloats[index];
        auto& intrudingFloat = intrudingFloats[index];
        auto previousRect = previousFloat.absoluteRectWithMargin();
        auto rect = intrudingFloat.absoluteRectWithMargin();
        if (previousFloat.isStartPositioned() != intrudingFloat.isStartPositioned() || previousRect.topLeft() != rect.topLeft() || previousRect.size() != rect.size() || previousFloat.shape() != intrudingFloat.shape())
            return true;
    }
    return false;
}

static bool inlineContentConstraintsChanged(const LineLayout& lineLayout, const Layout::ConstraintsForInlineContent& constraints)
{
    auto& previousConstraints = lineLayout.inlineContentConstraints();
    return !previousConstraints
        || previousConstraints->horizontal().logicalLeft != constraints.horizontal().logicalLeft
        || previousConstraints->horizontal().logicalWidth != constraints.horizontal().logicalWidth
        || previousConstraints->logicalTop() != constraints.logicalTop()
        || previousConstraints->visualLeft() != constraints.visualLeft()
        || previousConstraints->formattingRootBorderBoxSize() != constraints.formattingRootBorderBoxSize()
        || previousConstraints->fragmentainerEnd();
}

// The percentages in the inline padding of a box laid out through the render tree, in content whose intrinsic sizes the block formatting
// context computes, are cyclic and resolve against zero (css-sizing-3 §5.2.1), like those of the block containers it measures itself: the
// box's own formatting context measures it as if its containing block had no inline size, then again for its layout. The render tree path
// resolves them against the inline size its containing block got from the last layout, which makes the content wider each time it measures it.
class CyclicPercentagePaddingScope {
public:
    void add(const RenderBox& box)
    {
        CheckedRef style = box.style();
        auto writingMode = box.writingMode();
        if (!style->paddingStart(writingMode).isPercentOrCalculated() && !style->paddingEnd(writingMode).isPercentOrCalculated())
            return;
        // Only the box is written, which is measured again from its style when it is laid out.
        SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef mutableBox = const_cast<RenderBox&>(box);
        m_boxes.append({ mutableBox, box.overridingContainingBlockContentLogicalWidth() });
        mutableBox->setOverridingContainingBlockContentLogicalWidth(0_lu);
        mutableBox->invalidateContentLogicalWidths(MarkingBehavior::OnlyThis);
    }

    ~CyclicPercentagePaddingScope()
    {
        for (auto& [box, overridingContainingBlockContentLogicalWidth] : m_boxes) {
            if (overridingContainingBlockContentLogicalWidth)
                box->setOverridingContainingBlockContentLogicalWidth(*overridingContainingBlockContentLogicalWidth);
            else
                box->clearOverridingContainingBlockContentLogicalWidth();
            box->invalidateContentLogicalWidths(MarkingBehavior::OnlyThis);
        }
    }

private:
    Vector<std::pair<CheckedRef<RenderBox>, std::optional<RenderBox::ContainingBlockContentSize>>> m_boxes;
};

Layout::IntrinsicWidthConstraints BlockIntegrationUtils::inlineContentIntrinsicInlineSizes(const RenderBlockFlow& blockContainer)
{
    // The inline formatting context measures the atomic inline-level boxes and the floats in the content through the render tree.
    auto cyclicPercentagePaddingScope = CyclicPercentagePaddingScope { };
    for (CheckedPtr<const RenderObject> descendant = blockContainer.firstChild(); descendant;) {
        CheckedPtr box = dynamicDowncast<RenderBox>(*descendant);
        if (!box) {
            descendant = descendant->nextInPreOrder(&blockContainer);
            continue;
        }
        if (!box->isOutOfFlowPositioned())
            cyclicPercentagePaddingScope.add(*box);
        descendant = descendant->nextInPreOrderAfterChildren(&blockContainer);
    }
    auto [minimum, maximum] = blockContainer.computeInlineIntrinsicLogicalWidths();
    return { minimum, maximum };
}

// A canvas lays out its drawable content in an anonymous block (RenderHTMLCanvas::layout), whose containing block is the canvas's.
static RenderBlock* drawableContentBlock(RenderBox& box)
{
    CheckedPtr canvas = dynamicDowncast<RenderHTMLCanvas>(box);
    return canvas ? canvas->innerRenderer() : nullptr;
}

// A table cell gives the percentage block sizes inside it, also the ones that look through anonymous blocks to it, what the table gives it,
// which the render tree reads from the cell (RenderBox::computePercentageLogicalHeight, RenderReplaced::computeReplacedLogicalHeightUsing).
static bool percentagesResolveAgainstTableCell(const RenderBlock& containingBlock)
{
    CheckedPtr ancestor = &containingBlock;
    while (ancestor && ancestor->isAnonymousBlock())
        ancestor = ancestor->containingBlock();
    return ancestor && ancestor->isRenderTableCell();
}

// The available inline space of an orthogonal box can be the size of the initial containing block, which the view lays the box out again for
// when it changes, like RenderBox::perpendicularContainingBlockLogicalHeight registers it.
static void registerOrthogonalBoxForLayout(RenderBox& box)
{
    box.view().addPercentHeightDescendant(box);
}

// The layout states of the block containers around a box the render tree lays out on its own that have none of their own, with the line grids
// around it, the last one being the one of its containing block, from which the box goes where the block formatting context puts it.
static void pushLineGridLayoutStates(LineGridLayoutStateMaintainer& lineGridLayoutStates, RenderBox& containingBlock, const Layout::LineGridInLayoutState& lineGrid, LayoutSize containingBlockLayoutOffset)
{
    for (size_t index = 0; index < lineGrid.lineGrids.size(); ++index) {
        auto& [blockContainer, borderBoxOffset] = lineGrid.lineGrids[index];
        SUPPRESS_MEMORY_UNSAFE_CAST auto& gridContainer = const_cast<RenderBlockFlow&>(blockContainer.get());
        auto isContainingBlock = index == lineGrid.lineGrids.size() - 1;
        lineGridLayoutStates.push(containingBlock, isContainingBlock ? containingBlockLayoutOffset : borderBoxOffset, gridContainer, borderBoxOffset);
    }
}

BlockIntegrationUtils::IndependentFormattingContextLayoutResult BlockIntegrationUtils::layoutIndependentFormattingContext(const RenderBox& root, const Layout::ConstraintsForIndependentFormattingContext& constraints, const std::optional<Layout::LineGridInLayoutState>& lineGrid)
{
    // Only the root and its content are written here, by the root's own formatting context, but for the block-direction margins the block
    // formatting context gives it, and to a quirks mode body those of the root element, which nothing else in the block formatting context reads.
    SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef box = const_cast<RenderBox&>(root);
    // Its parent, or the block container whose inline content it is in.
    CheckedRef containingBlock = downcast<RenderBlockFlow>(*box->containingBlock());

    // Like RenderBlockFlow::layoutBlockChildren, which lays a child out again when the inline size of its containing block changed, once: in
    // content that breaks into fragmentainers, laying a box out again where this layout laid it out would lose the pagination strut its first
    // lines gave it, which its children, not laid out again, do not give it again (RenderBlockFlow::adjustBlockChildForPagination).
    if (!m_fragmentainerOffsetsOfLaidOutBoxes.contains(box))
        containingBlock->updateBlockChildDirtyBitsBeforeLayout(constraints.relayoutChildren ? RelayoutChildren::Yes : RelayoutChildren::No, box);
    // A -webkit-fill-available block size resolves against the available block size of the containing block, which the render tree does not
    // lay the box out again for when it changes.
    if (Layout::BlockFormattingUtils::hasFillAvailableBlockSize(box, box->writingMode()))
        box->setChildNeedsLayout(MarkingBehavior::OnlyThis);
    if (constraints.borderBoxInlineSize) {
        if (box->logicalWidth() != *constraints.borderBoxInlineSize)
            box->setChildNeedsLayout(MarkingBehavior::OnlyThis);
        box->setOverridingBorderBoxLogicalWidth(*constraints.borderBoxInlineSize);
    }
    // The block formatting context writes the geometry of the containing block once all of it is laid out. The render tree takes a containing
    // block block size, definite or not, as the box's own (RenderBox::hasAutoHeightOrContainingBlockWithAutoHeight), so only a box with a
    // percentage or stretch block size, which resolves against it, gets one. An orthogonal box resolves that against the inline size of the containing
    // block instead (RenderBox::computePercentageLogicalHeight), and takes the block size as its available inline space
    // (RenderBox::perpendicularContainingBlockLogicalHeight).
    auto containingBlockWritingMode = containingBlock->writingMode();
    // Like RenderBlockFlow::layoutBlockChild, before the root's own layout reads them. They follow the writing mode of the containing block.
    box->setMarginBefore(constraints.marginBefore, containingBlockWritingMode);
    box->setMarginAfter(constraints.marginAfter, containingBlockWritingMode);
    // A quirks mode body stretches to the viewport less the margins of the root element too, which it reads from the root element's renderer
    // (RenderBox::computeLogicalHeight), before the block formatting context that lays out the root element gives them to it.
    if (box->stretchesToViewport() && box->isBody()) {
        containingBlock->setMarginBefore(constraints.containingBlockMarginBefore, containingBlockWritingMode);
        containingBlock->setMarginAfter(constraints.containingBlockMarginAfter, containingBlockWritingMode);
    }
    auto isOrthogonal = box->writingMode().isOrthogonal(containingBlockWritingMode);
    box->setOverridingContainingBlockContentLogicalWidth(constraints.containingBlockInlineSize);
    if (isOrthogonal || (!percentagesResolveAgainstTableCell(containingBlock) && Layout::BlockFormattingUtils::hasPercentageBlockSize(box, box->writingMode())))
        box->setOverridingContainingBlockContentLogicalHeight(constraints.containingBlockBlockSize);
    if (isOrthogonal)
        registerOrthogonalBoxForLayout(box);
    CheckedPtr drawableContent = drawableContentBlock(box);
    if (drawableContent)
        drawableContent->setOverridingContainingBlockContentLogicalWidth(constraints.containingBlockInlineSize);
    CheckedRef layoutContext = box->view().frameView().layoutContext();
    // Its stretch-fit inline size comes from the floats the block formatting context places, which the render tree does not lay it out again for.
    if (constraints.stretchFitBorderBoxInlineSize) {
        box->setChildNeedsLayout(MarkingBehavior::OnlyThis);
        layoutContext->setStretchFitInlineSize(box, *constraints.stretchFitBorderBoxInlineSize);
    }
    // Its content looks past an orthogonal box to the inline size of the containing block instead (RenderBox::computePercentageLogicalHeight).
    if (!isOrthogonal) {
        auto containingBlockBlockSizes = LocalFrameViewLayoutContext::ContainingBlockBlockSizes { constraints.contentPercentageBasis, constraints.containingBlockAvailableBlockSize, constraints.containingBlockAvailableBlockSizeIncludingMarginBorderPadding };
        layoutContext->setContainingBlockBlockSizes(box, containingBlockBlockSizes);
        if (drawableContent)
            layoutContext->setContainingBlockBlockSizes(*drawableContent, containingBlockBlockSizes);
    }
    {
        // The block formatting context breaks the content around the box into fragmentainers when its root is paginated (BlockLayout::layout),
        // and keeps the box whole, like monolithic content (css-break-3 §4.1), unless the box's own formatting context breaks its content across
        // the pages from where the box goes.
        auto* rootLayoutState = layoutContext->layoutState();
        auto isMonolithic = rootLayoutState && rootLayoutState->isPaginated() && !constraints.borderBoxStripLogicalTop;
        if (isMonolithic)
            layoutContext->addMonolithicBox(box);
        // The layout state has the paint offset of the block formatting context root only, not of the boxes inside, and the line grid of the
        // containing block's content, from where the box goes, which it is laid out for every time.
        auto layoutStateDisabler = LayoutStateDisabler { layoutContext.get() };
        auto lineGridLayoutStates = LineGridLayoutStateMaintainer { layoutContext.get() };
        if (lineGrid) {
            // Its content snaps to them from where it goes, which it is laid out again for when it moves on them, but not where it already
            // was, which would lose the pagination strut its first lines gave it (RenderBlockFlow::adjustBlockChildForPagination).
            auto lineGridOffsets = lineGrid->lineGrids.map([&](auto& grid) {
                auto offset = grid.borderBoxOffset - lineGrid->boxBorderBoxOffset;
                return containingBlockWritingMode.isHorizontal() ? offset.height() : offset.width();
            });
            auto addResult = m_lineGridOffsetsOfLaidOutBoxes.add(box, lineGridOffsets);
            if (addResult.isNewEntry || addResult.iterator->value != lineGridOffsets) {
                addResult.iterator->value = WTF::move(lineGridOffsets);
                box->setNeedsLayout(MarkingBehavior::OnlyThis);
            }
            pushLineGridLayoutStates(lineGridLayoutStates, containingBlock, *lineGrid, lineGrid->boxBorderBoxOffset - box->locationOffset());
        }
        // The pages it breaks into are from the layout state of its containing block, which has none of its own, where the box goes before the
        // block formatting context puts it there, and which it is laid out again for when that is on another page (RenderBlockFlow::layoutBlockChild).
        // In a multi-column flow the render tree finds it from where the boxes around it are instead, up to the flow
        // (RenderFragmentedFlow::offsetFromLogicalTopOfFirstFragment), which the layout state of the flow paginates from already.
        auto paginationLayoutState = std::optional<LayoutStateMaintainer> { };
        auto isInFragmentedFlow = constraints.borderBoxStripLogicalTop && box->enclosingFragmentedFlow();
        if (isInFragmentedFlow)
            layoutContext->setOffsetInFragmentedFlow(box, *constraints.borderBoxStripLogicalTop);
        else if (constraints.borderBoxStripLogicalTop && !lineGrid) {
            auto blockOffset = *constraints.borderBoxStripLogicalTop - containingBlock->logicalTopForChild(box);
            paginationLayoutState.emplace(containingBlock, containingBlock->writingMode().isHorizontal() ? LayoutSize { 0_lu, blockOffset } : LayoutSize { blockOffset, 0_lu });
        }
        // The render tree does not lay out again the content of a box it broke across the fragmentainers, or not, from somewhere else, like
        // the flex items of a flex container that moved (RenderBlock::markForPaginationRelayoutIfNeeded is only for block containers), nor
        // that of one the block formatting context first laid out whole and then breaking inside, where it does not fit.
        if (rootLayoutState && rootLayoutState->isPaginated() && !box->isUnsplittableForPagination()) {
            auto addResult = m_fragmentainerOffsetsOfLaidOutBoxes.add(box, constraints.borderBoxStripLogicalTop);
            if (addResult.isNewEntry || addResult.iterator->value != constraints.borderBoxStripLogicalTop) {
                addResult.iterator->value = constraints.borderBoxStripLogicalTop;
                box->setNeedsLayout(MarkingBehavior::OnlyThis);
                for (CheckedRef descendant : descendantsOfType<RenderBox>(box.get()))
                    descendant->setNeedsLayout(MarkingBehavior::OnlyThis);
            }
        }
        box->layoutIfNeeded();
        if (isInFragmentedFlow)
            layoutContext->clearOffsetInFragmentedFlow(box);
        if (isMonolithic)
            layoutContext->removeMonolithicBox(box);
    }
    if (!isOrthogonal) {
        layoutContext->clearContainingBlockBlockSizes(box);
        if (drawableContent)
            layoutContext->clearContainingBlockBlockSizes(*drawableContent);
    }
    if (constraints.stretchFitBorderBoxInlineSize)
        layoutContext->clearStretchFitInlineSize(box);
    // Like BoxGeometryUpdater for a float in inline content: the shape the lines beside it wrap around, whose shape-margin percentage needs the
    // inline size of the containing block (ShapeOutsideInfo).
    if (box->isFloating()) {
        auto* shapeOutsideInfo = box->shapeOutsideInfo();
        box->layoutBox().setShape(shapeOutsideInfo ? &shapeOutsideInfo->computedShape() : nullptr);
    }
    box->clearOverridingContainingBlockContentSize();
    if (drawableContent)
        drawableContent->clearOverridingContainingBlockContentSize();
    if (constraints.borderBoxInlineSize)
        box->clearOverridingBorderBoxLogicalWidth();

    auto borderBoxSize = containingBlockWritingMode.isHorizontal() ? box->borderBoxSize() : box->borderBoxSize().transposedSize();
    auto border = box->borderWidths();
    auto padding = box->padding();
    return {
        borderBoxSize.width(),
        borderBoxSize.height(),
        { { border.start(containingBlockWritingMode), border.end(containingBlockWritingMode) }, { border.before(containingBlockWritingMode), border.after(containingBlockWritingMode) } },
        { { padding.start(containingBlockWritingMode), padding.end(containingBlockWritingMode) }, { padding.before(containingBlockWritingMode), padding.after(containingBlockWritingMode) } },
        is<RenderBlock>(box) ? downcast<RenderBlock>(box).intrinsicMarginBeforeForFieldset() : 0_lu
    };
}

LayoutSize BlockIntegrationUtils::layoutOutOfFlowIndependentFormattingContext(const RenderBox& root, std::optional<LayoutUnit> borderBoxInlineSize, std::optional<LayoutUnit> borderBoxBlockSize, std::optional<LayoutUnit> borderBoxInlinePosition, const std::optional<Layout::LineGridInLayoutState>& lineGrid, std::optional<LayoutUnit> borderBoxStripLogicalTop)
{
    SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef box = const_cast<RenderBox&>(root);
    // Its size may depend on the size of its containing block, which the block formatting context has just laid out.
    box->setChildNeedsLayout(MarkingBehavior::OnlyThis);
    // With an overriding inline size the render tree keeps a box where it is in the inline axis (RenderBox::computeLogicalWidth), so one
    // that moves itself during its layout starts from where BlockLayout::updateRenderers would put it.
    if (borderBoxInlinePosition) {
        CheckedRef containingBlock = *box->containingBlock();
        auto writingMode = containingBlock->writingMode();
        auto inlineSize = borderBoxInlineSize.value_or(writingMode.isHorizontal() ? box->borderBoxWidth() : box->borderBoxHeight());
        auto logicalLeft = writingMode.isLogicalLeftInlineStart() ? *borderBoxInlinePosition : containingBlock->logicalWidth() - *borderBoxInlinePosition - inlineSize;
        if (writingMode.isHorizontal())
            box->setX(logicalLeft);
        else
            box->setY(logicalLeft);
    }
    if (borderBoxInlineSize)
        box->setOverridingBorderBoxLogicalWidth(*borderBoxInlineSize);
    if (borderBoxBlockSize)
        box->setOverridingBorderBoxLogicalHeight(*borderBoxBlockSize);
    {
        // The layout state has the paint offset of the block formatting context root only, not of the boxes inside, and the line grid of the
        // containing block's content, from where the box goes.
        CheckedRef layoutContext = box->view().frameView().layoutContext();
        auto layoutStateDisabler = LayoutStateDisabler { layoutContext.get() };
        auto lineGridLayoutStates = LineGridLayoutStateMaintainer { layoutContext.get() };
        if (lineGrid) {
            box->setNeedsLayout(MarkingBehavior::OnlyThis);
            // The layout state of a box whose containing block is an inline box is moved by the start of the inline box too
            // (RenderLayoutState::computeOffsets).
            auto offset = lineGrid->boxBorderBoxOffset - box->locationOffset();
            if (CheckedPtr inlineBox = dynamicDowncast<RenderBoxModelObject>(box->container()); inlineBox && inlineBox->isInlineBox())
                offset -= toLayoutSize(inlineBox->blockStartRelativeFirstFragmentBorderBoxRect().location());
            pushLineGridLayoutStates(lineGridLayoutStates, *box->containingBlock(), *lineGrid, offset);
        }
        // Its content breaks across the pages from where the block formatting context puts it, like that of a box in the normal flow
        // (layoutIndependentFormattingContext), which the render tree lays out again when it moves (RenderBlock::layoutOutOfFlowBox).
        auto paginationLayoutState = std::optional<LayoutStateMaintainer> { };
        auto isInFragmentedFlow = borderBoxStripLogicalTop && box->enclosingFragmentedFlow();
        if (isInFragmentedFlow)
            layoutContext->setOffsetInFragmentedFlow(box, *borderBoxStripLogicalTop);
        else if (borderBoxStripLogicalTop && !lineGrid) {
            CheckedRef containingBlock = *box->containingBlock();
            auto blockOffset = *borderBoxStripLogicalTop - containingBlock->logicalTopForChild(box);
            paginationLayoutState.emplace(containingBlock, containingBlock->writingMode().isHorizontal() ? LayoutSize { 0_lu, blockOffset } : LayoutSize { blockOffset, 0_lu });
        }
        if (borderBoxStripLogicalTop) {
            auto addResult = m_fragmentainerOffsetsOfLaidOutBoxes.add(box, borderBoxStripLogicalTop);
            if (addResult.isNewEntry || addResult.iterator->value != borderBoxStripLogicalTop) {
                addResult.iterator->value = borderBoxStripLogicalTop;
                for (CheckedRef descendant : descendantsOfType<RenderBox>(box.get()))
                    descendant->setNeedsLayout(MarkingBehavior::OnlyThis);
            }
        }
        box->layoutIfNeeded();
        if (isInFragmentedFlow)
            layoutContext->clearOffsetInFragmentedFlow(box);
    }
    if (borderBoxInlineSize)
        box->clearOverridingBorderBoxLogicalWidth();
    if (borderBoxBlockSize)
        box->clearOverridingBorderBoxLogicalHeight();
    return { box->logicalWidth(), box->logicalHeight() };
}

LayoutPoint BlockIntegrationUtils::borderBoxLogicalLocation(const RenderBox& box)
{
    CheckedRef containingBlock = *box.containingBlock();
    auto writingMode = containingBlock->writingMode();
    auto location = writingMode.isHorizontal() ? box.location() : box.location().transposedPoint();
    if (!writingMode.isLogicalLeftInlineStart())
        location.setX(containingBlock->logicalWidth() - location.x() - (writingMode.isHorizontal() ? box.borderBoxWidth() : box.borderBoxHeight()));
    return location;
}

Layout::IntrinsicWidthConstraints BlockIntegrationUtils::independentFormattingContextIntrinsicInlineSizes(const RenderBox& box, LayoutUnit borderAndPadding)
{
    // Like RenderBox::computeOutOfFlowPositionedLogicalWidthUsing: the contributions for an automatic fit-content size, and for the sizing
    // keywords, the legacy ones included, the intrinsic sizes of the content (RenderBox::computeIntrinsicKeywordLogicalWidths), as the
    // contributions apply them.
    if (!Layout::BlockFormattingUtils::hasIntrinsicInlineSize(box, box.writingMode()))
        return { box.minContentLogicalWidthContribution() - borderAndPadding, box.maxContentLogicalWidthContribution() - borderAndPadding };
    auto intrinsicWidthComputationScope = IntrinsicLogicalWidthComputationScope { box.view().frameView().layoutContext(), box };
    auto [minimum, maximum] = box.computeIntrinsicLogicalWidths();
    return { minimum, maximum };
}

Layout::IntrinsicWidthConstraints BlockIntegrationUtils::independentFormattingContextIntrinsicInlineSizeContributions(const RenderBox& box)
{
    // An orthogonal box contributes the block size its formatting context lays it out at (RenderBlock::computeBlockIntrinsicLogicalWidths).
    // The out-of-flow boxes in it do not change that size, so it does not lay it out for them, which would invalidate all of it
    // (RenderBox::computeIntrinsicLogicalHeight).
    if (CheckedPtr containingBlock = box.containingBlock(); containingBlock && containingBlock->writingMode().isOrthogonal(box.writingMode())) {
        if (!box.selfNeedsLayout() && !box.normalChildNeedsLayout())
            return { box.logicalHeight(), box.logicalHeight() };
        SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef mutableBox = const_cast<RenderBox&>(box);
        auto blockSize = mutableBox->computeIntrinsicLogicalHeight();
        return { blockSize, blockSize };
    }
    auto cyclicPercentagePaddingScope = CyclicPercentagePaddingScope { };
    cyclicPercentagePaddingScope.add(box);
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

// Like layoutIndependentFormattingContext does from the renderer once the render tree laid the float out (ShapeOutsideInfo), with the geometry
// the block formatting context gives the float before it writes it to the renderer.
void BlockIntegrationUtils::setShapeOutside(const RenderBox& floatBox, const Layout::BoxGeometry& geometry, WritingMode containingBlockWritingMode, LayoutUnit containingBlockContentInlineSize)
{
    SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef box = const_cast<RenderBox&>(floatBox);
    if (!box->shapeOutsideInfo()) {
        box->layoutBox().setShape(nullptr);
        return;
    }
    auto borderBoxSize = LayoutSize { geometry.borderBoxWidth(), geometry.borderBoxHeight() };
    auto margins = LayoutBoxExtent { };
    margins.setStart(geometry.marginStart(), containingBlockWritingMode);
    margins.setEnd(geometry.marginEnd(), containingBlockWritingMode);
    margins.setBefore(geometry.marginBefore(), containingBlockWritingMode);
    margins.setAfter(geometry.marginAfter(), containingBlockWritingMode);
    box->layoutBox().setShape(makeShapeForShapeOutside(box, { containingBlockWritingMode.isHorizontal() ? borderBoxSize : borderBoxSize.transposedSize(), margins, containingBlockContentInlineSize }));
}

Layout::BoxGeometry BlockIntegrationUtils::rootGeometry(const RenderBlockFlow& root)
{
    // In the root's writing mode, which is its content's. An atomic inline-level root shares its geometry with the inline formatting context it
    // is in, which places it with its margins, so these are the ones LayoutIntegration::BoxGeometryUpdater::setFormattingContextRootGeometry
    // gives it: from style, before the root's own layout computes its block-direction ones, and line-relative in the block axis, from the
    // line-over side (BoxGeometryUpdater::verticalLogicalMargin).
    CheckedRef style = root.style();
    auto writingMode = root.writingMode();
    // The view has no containing block, nor margins.
    auto containingBlockInlineSize = root.isRenderView() ? 0_lu : root.containingBlockLogicalWidthForContent();
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
    // The physical borders on the root's own sides, not the collapsed border halves of a table cell, which are in the writing mode of the table
    // (RenderTableCell::borderStart).
    auto borderWidths = root.borderWidths();
    geometry.setBorder({ { borderWidths.start(writingMode), borderWidths.end(writingMode) }, { borderWidths.before(writingMode), borderWidths.after(writingMode) } });
    geometry.setPadding({ { root.paddingStart(), root.paddingEnd() }, { root.paddingBefore(), root.paddingAfter() } });
    geometry.setContentBoxWidth(root.contentBoxLogicalWidth());
    // Scrollbars take the space they need from the padding box, up to all of it (LayoutIntegration::BoxGeometryUpdater's scrollbarLogicalSize).
    auto paddingBoxIncludingScrollbar = root.paddingBoxRectIncludingScrollbar();
    auto spaceForScrollbar = LayoutSize { std::max(0_lu, paddingBoxIncludingScrollbar.width() - root.paddingBoxWidth()), std::max(0_lu, paddingBoxIncludingScrollbar.height() - root.paddingBoxHeight()) };
    if (!writingMode.isHorizontal())
        spaceForScrollbar = spaceForScrollbar.transposedSize();
    // scrollbar-gutter: stable both-edges takes the same space on the other side too (RenderBox::contentBoxWidth).
    if (style->scrollbarGutter().isStableBothEdges())
        spaceForScrollbar.setWidth(spaceForScrollbar.width() * 2);
    geometry.setHorizontalSpaceForScrollbar(spaceForScrollbar.width());
    geometry.setVerticalSpaceForScrollbar(spaceForScrollbar.height());
    return geometry;
}

// Like RenderBlock::layout for a box whose inline size comes from its content (sizesAffectedByScrollbarsForSubtreeRoot): the scroll containers
// laid out after it hand their scrollbar changes to the root (RelayoutScopeForScrollbarChange), which lays its content out again with them.
void BlockIntegrationUtils::trackScrollbarChangesForContentSizes()
{
    if (!m_rootForScrollbarChanges || m_scrollbarChangesForContentSizes)
        return;
    CheckedRef layoutContext = m_rootForScrollbarChanges->layoutContext();
    if (layoutContext->subtreeScrollbarChangesState() || m_rootForScrollbarChanges->writingMode().computedTextDirection() == TextDirection::RTL)
        return;
    m_scrollbarChangesForContentSizes.emplace(layoutContext.get(), *m_rootForScrollbarChanges, EnumSet { LogicalBoxAxis::Inline });
}

Vector<RendererScrollbarChange> BlockIntegrationUtils::takeScrollbarChangesForContentSizes()
{
    if (!m_scrollbarChangesForContentSizes)
        return { };
    auto changes = WTF::move(m_rootForScrollbarChanges->layoutContext().subtreeScrollbarChangesState()->rendererScrollbarChanges);
    m_scrollbarChangesForContentSizes.reset();
    return changes;
}

bool BlockIntegrationUtils::contentBoxInlineSizeChanged(const RenderBox& box, LayoutUnit contentBoxInlineSize)
{
    if (box.contentBoxLogicalWidth() != contentBoxInlineSize)
        return true;
    CheckedPtr block = dynamicDowncast<RenderBlock>(box);
    return block && block->hasBorderOrPaddingLogicalWidthChanged();
}

bool BlockIntegrationUtils::markPercentageHeightDescendantsForLayout(const RenderBlockFlow& blockContainer, std::optional<LayoutUnit> percentageBasis)
{
    // Only the dirty bits of this block container and of the boxes laid out through the render tree inside it are written, and the basis they
    // were marked for, which the render tree path keeps for its next layout of the block container. RenderBlockFlow::layoutBlock lays out its
    // content right after; the block container is marked too when they are, so that its inline content, which lays out those boxes, is not
    // kept as it is (layoutInlineContent).
    SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef mutableBlockContainer = const_cast<RenderBlockFlow&>(blockContainer);
    if (!mutableBlockContainer->dirtyForLayoutFromPercentageHeightDescendantsIfBasisChanged(percentageBasis))
        return false;
    mutableBlockContainer->setChildNeedsLayout(MarkingBehavior::OnlyThis);
    return true;
}

void BlockIntegrationUtils::layoutOutsideListMarker(const RenderBox& listItem, bool relayoutChildren)
{
    // Only the marker and the list item's record of it are written.
    if (CheckedPtr item = dynamicDowncast<RenderListItem>(listItem); item && item->markerBox())
        SUPPRESS_MEMORY_UNSAFE_CAST const_cast<RenderListItem&>(*item).layoutOutsideMarker(relayoutChildren ? RelayoutChildren::Yes : RelayoutChildren::No);
}

void BlockIntegrationUtils::placeOutsideListMarker(const RenderBox& listItem)
{
    if (CheckedPtr item = dynamicDowncast<RenderListItem>(listItem); item && item->markerBox())
        SUPPRESS_MEMORY_UNSAFE_CAST const_cast<RenderListItem&>(*item).placeOutsideMarkerAfterLayout();
}

LayoutUnit BlockIntegrationUtils::outsideListMarkerOwnLineBlockSize(const RenderBox& box)
{
    CheckedPtr listItem = dynamicDowncast<RenderListItem>(box);
    return listItem ? listItem->markerOwnLineBlockSize() : 0_lu;
}

Vector<CheckedRef<RenderListItem>> BlockIntegrationUtils::listItemsWithOutsideMarker(const Vector<Layout::ContainingBlockState>& containingBlockStates)
{
    // From the outermost one, like nested RenderListItem::layoutBlock calls, leaving out the ones already there, like the root's.
    auto listItems = Vector<CheckedRef<RenderListItem>> { };
    for (auto& state : containingBlockStates) {
        CheckedPtr listItem = dynamicDowncast<RenderListItem>(state.blockContainer);
        if (!listItem || !listItem->markerBox())
            continue;
        if (listItem->view().frameView().layoutContext().listItemsWithOutsideMarker().containsIf([&](auto& item) { return item.get() == listItem.get(); }))
            continue;
        // The layout context lists them while their content is laid out; only that list is written.
        SUPPRESS_MEMORY_UNSAFE_CAST listItems.append(const_cast<RenderListItem&>(*listItem));
    }
    return listItems;
}

void BlockIntegrationUtils::invalidateContentInlineSizesBeforeLayout(const RenderBox& box, bool relayoutChildren)
{
    // Only the intrinsic sizes of the box are written, like RenderBlock::updateBlockChildDirtyBitsBeforeLayout when its containing block lays
    // out its children again: with percentage padding, or a percentage block size that its aspect ratio transfers, they depend on that layout.
    if (!relayoutChildren || !box.shouldInvalidateContentWidths())
        return;
    SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef mutableBox = const_cast<RenderBox&>(box);
    mutableBox->invalidateContentLogicalWidths(MarkingBehavior::OnlyThis);
}

bool BlockIntegrationUtils::isAnonymousBlockWithoutContentfulFirstLine(const RenderBlockFlow& blockContainer)
{
    if (!blockContainer.isAnonymousBlock())
        return false;
    auto firstLineBox = InlineIterator::firstLineBoxFor(blockContainer);
    return !firstLineBox || !firstLineBox->lineLeftmostLeafBox();
}

void BlockIntegrationUtils::showBoxHiddenByLineClamp(const RenderBox& box)
{
    // Only the box and its content are written, which the block formatting context lays out right after, like RenderBlockFlow::layoutBlockChild
    // (LineClampUpdater::setIsForcedHidden marks them for layout).
    SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef mutableBox = const_cast<RenderBox&>(box);
    LineClampUpdater::setIsForcedHidden(mutableBox, false);
}

void BlockIntegrationUtils::setFloatClippedByLineClamp(const RenderBox& floatBox, bool isClipped)
{
    // Only the float is written, which paints clipped to the block-end content edge of its line-clamp container (LineClampUpdater::blockEndClipRect),
    // like RenderBlockFlow::layoutBlockChildren sets it.
    SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef mutableFloatBox = const_cast<RenderBox&>(floatBox);
    mutableFloatBox->setIsClippedByLineClamp(isClipped);
}

std::optional<size_t> BlockIntegrationUtils::legacyLineClampMaximumLines(const RenderBlockFlow& blockContainer)
{
    auto legacyLineClamp = BlockIntegrationUtils::legacyLineClamp(blockContainer);
    if (!legacyLineClamp)
        return { };
    // Past the count, the lines left wrap around to more than any content has, so the content after the clamp point is not truncated.
    return legacyLineClamp->maximumLineCount - legacyLineClamp->currentLineCount;
}

// From the left of the root's border box, in the root's logical coordinates, which are the ones of the line grid around it: a writing mode
// root cannot be split across pages, and the line grid around it does not reach its content (RenderBox::isUnsplittableForPagination).
static LayoutSize lineGridOffsetFromRoot(const RenderLayoutState& rootLayoutState, LayoutSize lineGridOffset, const RenderBlockFlow& root)
{
    auto offset = lineGridOffset - rootLayoutState.layoutOffset();
    return root.writingMode().isHorizontal() ? offset : offset.transposedSize();
}

Layout::RootLineGrid BlockIntegrationUtils::lineGridForRoot(const RenderBlockFlow& root)
{
    // The root pushes a layout state of its own when there is a line grid around it or it has a -webkit-line-grid name
    // (LocalFrameViewLayoutContext::pushLayoutState).
    auto* layoutState = root.view().frameView().layoutContext().layoutState();
    if (!layoutState)
        return { { }, { }, true };
    auto borderBoxOffsetInLayoutState = root.writingMode().isHorizontal() ? layoutState->layoutOffset() : layoutState->layoutOffset().transposedSize();
    if (!layoutState->lineGrid()) {
        // So it does in fragmented content, where the boxes the render tree lays out in a line grid inside the root snap their lines from the
        // top of the page they start on, which does not have the offset of the layout state (RenderBlockFlow's pageLogicalTopForOffset in
        // lineGrid).
        auto pushesLayoutState = root.enclosingFragmentedFlow() || layoutState->isPaginated();
        return { { }, pushesLayoutState ? borderBoxOffsetInLayoutState : LayoutSize { }, true };
    }
    ASSERT(layoutState->renderer() == &root);
    CheckedRef gridContainer = *layoutState->lineGrid();
    if (gridContainer.ptr() == &root)
        return { Layout::ContainingBlockState::LineGrid { gridContainer.get(), { } }, borderBoxOffsetInLayoutState, true };
    auto outsideRoot = Layout::ContainingBlockState::LineGrid::OutsideRoot { lineGridOffsetFromRoot(*layoutState, layoutState->lineGridOffset(), root), gridContainer->borderAndPaddingBefore() };
    return { Layout::ContainingBlockState::LineGrid { gridContainer.get(), outsideRoot }, borderBoxOffsetInLayoutState, true };
}

// Like RenderLayoutState::establishLineGrid, through the line grids in the layout states of the boxes around the root, past the one of the root.
std::optional<Layout::ContainingBlockState::LineGrid> BlockIntegrationUtils::lineGridOutsideRoot(const RenderBlockFlow& root, const Style::WebkitLineGrid& name, const RenderBlockFlow& rootGridContainer)
{
    auto& layoutStateStack = root.view().frameView().layoutContext().layoutStateStack();
    if (layoutStateStack.isEmpty() || layoutStateStack.last()->lineGrid() != &rootGridContainer)
        return { };
    auto& rootLayoutState = *layoutStateStack.last();
    CheckedPtr<const RenderBlockFlow> currentGridContainer = &rootGridContainer;
    for (auto& layoutState : layoutStateStack | std::views::reverse) {
        if (layoutState->lineGrid() == currentGridContainer.get())
            continue;
        currentGridContainer = layoutState->lineGrid();
        if (!currentGridContainer)
            return { };
        if (currentGridContainer->style().lineGrid() == name)
            return Layout::ContainingBlockState::LineGrid { *currentGridContainer, Layout::ContainingBlockState::LineGrid::OutsideRoot { lineGridOffsetFromRoot(rootLayoutState, layoutState->lineGridOffset(), root), currentGridContainer->borderAndPaddingBefore() } };
    }
    return { };
}

std::optional<RenderLayoutState::LegacyLineClamp> BlockIntegrationUtils::legacyLineClamp(const RenderBox& box)
{
    auto* layoutState = box.view().frameView().layoutContext().layoutState();
    return layoutState ? layoutState->legacyLineClamp() : std::nullopt;
}

void BlockIntegrationUtils::restoreLegacyLineClamp(const RenderBox& box, const std::optional<RenderLayoutState::LegacyLineClamp>& legacyLineClamp)
{
    if (legacyLineClamp)
        box.view().frameView().layoutContext().layoutState()->setLegacyLineClamp(legacyLineClamp);
}

std::optional<LayoutUnit> BlockIntegrationUtils::updateLegacyLineClampAfterInlineContent(const RenderBlockFlow& blockContainer, size_t lineCountBeforeBreak, LayoutUnit consumedContentBlockSize)
{
    auto& layoutState = *blockContainer.view().frameView().layoutContext().layoutState();
    CheckedPtr inlineLayout = blockContainer.inlineLayout();
    auto legacyLineClamp = layoutState.legacyLineClamp();
    if (!legacyLineClamp || !inlineLayout || blockContainer.isFloatingOrOutOfFlowPositioned())
        return { };

    // Not the lines of the fragmentainers before, which it counted there.
    legacyLineClamp->currentLineCount += inlineLayout->lineCount() - std::min<size_t>(inlineLayout->lineCount(), lineCountBeforeBreak);
    // The -webkit-box clamps at the first one only.
    if (legacyLineClamp->clampedRenderer) {
        layoutState.setLegacyLineClamp(*legacyLineClamp);
        return { };
    }
    auto clampedContentBlockSize = [&] -> std::optional<LayoutUnit> {
        if (auto clampedContentBlockSize = inlineLayout->clampedContentLogicalHeight())
            return clampedContentBlockSize;
        // The content may end right at the clamp point.
        if (legacyLineClamp->currentLineCount == legacyLineClamp->maximumLineCount)
            return LayoutUnit { inlineLayout->contentLogicalHeight() };
        return { };
    }();
    if (clampedContentBlockSize) {
        // From the start of the content, which the fragmentainers before took a part of.
        legacyLineClamp->clampedContentLogicalHeight = consumedContentBlockSize + *clampedContentBlockSize;
        legacyLineClamp->clampedRenderer = &blockContainer;
    }
    layoutState.setLegacyLineClamp(*legacyLineClamp);
    return clampedContentBlockSize;
}

bool BlockIntegrationUtils::laysOutThroughRenderTree(const RenderBox& box)
{
    // FIXME: A block formatting context root whose content the block formatting context cannot lay out yet runs render tree block layout
    // (RenderBlockFlow::layoutBlock). It goes back to the block formatting context as coverage grows.
    CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(box);
    return !blockContainer || !canUseForNestedBlockLayout(*blockContainer);
}

bool BlockIntegrationUtils::inlineContentHasFloats(const RenderBlockFlow& blockContainer)
{
    for (CheckedPtr<const RenderObject> descendant = blockContainer.firstChild(); descendant;) {
        if (descendant->isFloating())
            return true;
        if (is<RenderBox>(*descendant)) {
            descendant = descendant->nextInPreOrderAfterChildren(&blockContainer);
            continue;
        }
        descendant = descendant->nextInPreOrder(&blockContainer);
    }
    return false;
}

enum class InlineContentStart : uint8_t { LineBoxes, BlockLevelBox, None };
static InlineContentStart inlineContentStart(const RenderBlockFlow& blockContainer)
{
    for (CheckedPtr<const RenderObject> descendant = blockContainer.firstChild(); descendant;) {
        if (descendant->isFloatingOrOutOfFlowPositioned()) {
            descendant = descendant->nextInPreOrderAfterChildren(&blockContainer);
            continue;
        }
        if (CheckedPtr text = dynamicDowncast<RenderText>(*descendant)) {
            if (!text->containsOnlyCollapsibleWhitespace())
                return InlineContentStart::LineBoxes;
        } else if (CheckedPtr lineBreak = dynamicDowncast<RenderLineBreak>(*descendant)) {
            if (!lineBreak->isWBR())
                return InlineContentStart::LineBoxes;
        } else if (CheckedPtr box = dynamicDowncast<RenderBox>(*descendant)) {
            // The block-start margin of a block-level box can still collapse with the block container's.
            return box->isBlockLevelBox() ? InlineContentStart::BlockLevelBox : InlineContentStart::LineBoxes;
        } else if (descendant->isInlineBox()) {
            CheckedRef inlineBox = downcast<RenderBoxModelObject>(*descendant);
            CheckedRef style = inlineBox->style();
            auto writingMode = blockContainer.writingMode();
            auto hasInlineAxisMargin = [&](auto& margin) {
                return !margin.isAuto() && !margin.isPossiblyZero();
            };
            if (hasInlineAxisMargin(style->marginStart(writingMode)) || hasInlineAxisMargin(style->marginEnd(writingMode)) || inlineBox->borderStart() || inlineBox->borderEnd() || !style->paddingStart(writingMode).isPossiblyZero() || !style->paddingEnd(writingMode).isPossiblyZero())
                return InlineContentStart::LineBoxes;
        }
        descendant = descendant->nextInPreOrder(&blockContainer);
    }
    return InlineContentStart::None;
}

bool BlockIntegrationUtils::inlineContentMakesLineBoxesBeforeBlockLevelBoxes(const RenderBlockFlow& blockContainer)
{
    return inlineContentStart(blockContainer) == InlineContentStart::LineBoxes;
}

bool BlockIntegrationUtils::inlineContentStartsWithBlockLevelBox(const RenderBlockFlow& blockContainer)
{
    return inlineContentStart(blockContainer) == InlineContentStart::BlockLevelBox;
}

BlockIntegrationUtils::InlineContentLayoutResult BlockIntegrationUtils::layoutInlineContent(const RenderBlockFlow& blockContainer, const Layout::ConstraintsForInlineContent& constraints, const Layout::BoxGeometry& geometry, std::optional<LayoutUnit> contentBoxBlockSize, LayoutUnit fillAvailableBlockSize, LayoutUnit replacedElementAvailableBlockSize, LayoutUnit orthogonalFlowAvailableInlineSpace, bool relayoutChildren, const Layout::BlockLayoutState& blockLayoutState, const Layout::PlacedFloats::List& intrudingFloats, const std::optional<Layout::LineGridInLayoutState>& contentLineGrid, Layout::BlockFormattingContext& blockFormattingContext, Layout::BlockLayoutResult& blockLayoutResult, const Layout::InlineBreakToken* breakToken)
{
    // Only the line layout of this block container and its inline content are written here, and nothing else in the block formatting
    // context reads them.
    auto& inlineContentRoot = const_cast<RenderBlockFlow&>(blockContainer);

    // Like RenderBlockFlow::layoutBlockChild, which only lays out a block that needs it, and RenderBlock::recomputeLogicalWidth, which
    // lays its content out again when its width changed: otherwise the lines stay as they are. A block whose content only needs its overflow
    // recomputed or its out-of-flow boxes laid out keeps them too (RenderBlock::canPerformSimplifiedLayout). Content takes its lines off the
    // line-clamp budget as it gets laid out, and what the content before it left of the budget may have changed: RenderBlockFlow::layoutInFlowChildren
    // lays out all the content of a line-clamp container's formatting context again. Lines that snap to a line grid go where the block container
    // is now, unlike on the render tree path, which keeps them where they were (RenderBlock::adjustLogicalLeftOffsetForLine).
    auto isInLineClampContext = !!blockLayoutState.lineClamp();
    CheckedRef style = inlineContentRoot.style();
    auto snapsToLineGrid = blockLayoutState.lineGrid() && (style->lineSnap() != LineSnap::None || style->lineAlign() != LineAlign::None);
    // Lines laid out into fragmentainers are where their fragmentainer put them, and are laid out again (inlineContentConstraintsChanged).
    auto isFragmenting = !!constraints.fragmentainerEnd();
    // Not held on to: content of out-of-flow boxes only drops the line layout below.
    auto wasFragmented = [&] {
        CheckedPtr previousLineLayout = inlineContentRoot.inlineLayout();
        return previousLineLayout && previousLineLayout->inlineContentConstraints() && previousLineLayout->inlineContentConstraints()->fragmentainerEnd();
    }();
    auto contentNeedsLayout = relayoutChildren || isFragmenting || isInLineClampContext || snapsToLineGrid || inlineContentRoot.selfNeedsLayout() || inlineContentRoot.normalChildNeedsLayout() || inlineContentRoot.outOfFlowChildNeedsStaticPositionLayout();
    if (CheckedPtr lineLayout = inlineContentRoot.inlineLayout(); lineLayout && !contentNeedsLayout && lineLayout->hasContent() && !inlineContentConstraintsChanged(*lineLayout, constraints) && !intrudingFloatsChanged(*lineLayout, intrudingFloats)) {
        auto keptContentLayoutResult = lineLayout->hasBlocks() ? keptInlineContentWithBlockLevelBoxesLayoutResult(inlineContentRoot, *lineLayout) : std::optional { inlineContentLayoutResult(*lineLayout) };
        if (keptContentLayoutResult) {
            if (inlineContentRoot.needsSimplifiedNormalFlowLayout())
                inlineContentRoot.simplifiedNormalFlowLayout();
            keptContentLayoutResult->floats = lineLayout->floatsInContent();
            return *keptContentLayoutResult;
        }
    }

    // Like RenderBlockFlow::layoutInlineContent, which lays the content out from scratch when the box's width changed (RenderBlock::recomputeLogicalWidth).
    // The renderer of the root has its new geometry already, and RenderBlock::recomputeLogicalWidth told its layout about the change.
    auto invalidateChildren = relayoutChildren || ((isFragmenting || wasFragmented) && !breakToken) || isInLineClampContext || snapsToLineGrid || inlineContentRoot.logicalWidth() != geometry.borderBoxWidth() || inlineContentRoot.hasBorderOrPaddingLogicalWidthChanged();
    auto inlineContentStatus = inlineContentRoot.markInlineContentDirtyForLayout(invalidateChildren ? RelayoutChildren::Yes : RelayoutChildren::No);
    if (inlineContentStatus.hasSimpleOutOfFlowContentOnly) {
        // Like RenderBlockFlow::layoutInlineContent: content of out-of-flow boxes only has no lines, and the static position of the boxes is
        // where the content starts, at the start of the content box unless align-content moves it (RenderBlockFlow::setStaticPositionsForSimpleOutOfFlowContent).
        inlineContentRoot.m_lineLayout = nullptr;
        for (auto walker = InlineWalker(inlineContentRoot); !walker.atEnd(); walker.advance()) {
            CheckedRef layer = *downcast<RenderBox>(*walker.current()).layer();
            layer->setStaticInlinePosition(geometry.borderAndPaddingStart());
            layer->setStaticBlockPosition(constraints.logicalTop());
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
    CheckedRef layoutContext = inlineContentRoot.view().frameView().layoutContext();
    // Percentages that look past a box to a table cell behave as auto (RenderBox::computePercentageLogicalHeight).
    auto containingBlockBlockSizes = LocalFrameViewLayoutContext::ContainingBlockBlockSizes { percentagesResolveAgainstTableCell(inlineContentRoot) ? std::nullopt : contentBoxBlockSize, fillAvailableBlockSize, replacedElementAvailableBlockSize };
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
        // Like layoutIndependentFormattingContext. The content of an out-of-flow box looks past it to its own containing block.
        auto isOrthogonal = box->writingMode().isOrthogonal(inlineContentRoot.writingMode());
        if (isOrthogonal) {
            box->setOverridingContainingBlockContentLogicalHeight(orthogonalFlowAvailableInlineSpace);
            registerOrthogonalBoxForLayout(*box);
        } else if (!percentagesResolveAgainstTableCell(inlineContentRoot) && Layout::BlockFormattingUtils::hasPercentageBlockSize(*box, box->writingMode())) {
            auto containingBlockBlockSize = [&] -> std::optional<LayoutUnit> {
                // In quirks mode a stretch block size does not look past the containing block, unlike a percentage.
                if (box->document().inQuirksMode() && Layout::BlockFormattingUtils::hasOnlyStretchBlockSizes(*box))
                    return Layout::BlockFormattingUtils::stretchFitBasis(inlineContentRoot, contentBoxBlockSize);
                if (box->isRenderReplaced() && box->document().inQuirksMode())
                    return replacedElementAvailableBlockSize;
                return contentBoxBlockSize;
            };
            box->setOverridingContainingBlockContentLogicalHeight(containingBlockBlockSize());
        }
        if (!isOrthogonal && !box->isOutOfFlowPositioned())
            layoutContext->setContainingBlockBlockSizes(*box, containingBlockBlockSizes);
        atomicInlineLevelBoxes.append(*box);
        if (CheckedPtr drawableContent = drawableContentBlock(*box)) {
            drawableContent->setOverridingContainingBlockContentLogicalWidth(constraints.horizontal().logicalWidth);
            if (!isOrthogonal)
                layoutContext->setContainingBlockBlockSizes(*drawableContent, containingBlockBlockSizes);
        }
        descendant = descendant->nextInPreOrderAfterChildren(&inlineContentRoot);
    }
    // The floats whose content the block formatting context lays out have the geometry it gives them, not that of their renderers, which no
    // layout gave a size, also when the inline formatting context places them again from a line before without measuring them
    // (IntegrationUtils::layoutWithFormattingContextForBox).
    for (auto& box : atomicInlineLevelBoxes) {
        if (!box->isFloating())
            continue;
        if (auto floatGeometry = blockFormattingContext.geometryForFloatInInlineContent(box))
            inlineContentRoot.view().layoutState().ensureGeometryForBox(box->layoutBox()) = *floatGeometry;
    }
    auto firstLineIndex = size_t { 0 };
    auto outgoingBreakToken = std::optional<Layout::InlineBreakToken> { };
    auto pushedFloats = Layout::PlacedFloats::List { };
    {
        // The layout state has the paint offset of the block formatting context root only, not of the boxes inside.
        auto layoutStateDisabler = LayoutStateDisabler { layoutContext.get() };
        // The block formatting context needs the margins of every block-level box in the content, so it lays out all the lines again
        // (like RenderBlockFlow::layoutInlineContent when one of them is dirty).
        auto hasBlockLevelBoxes = inlineContentStatus.hasDirtyInFlowBlockLevelElement || lineLayout->hasBlocks();
        // The lines before the damage would need the floats in the content placed before them, which only the render tree's float lists keep.
        auto hasFloats = !intrudingFloats.isEmpty() || inlineContentHasFloats(inlineContentRoot);
        auto forceFullLayout = invalidateChildren || hasBlockLevelBoxes || hasFloats || isFragmenting ? LineLayout::ForceFullLayout::Yes : LineLayout::ForceFullLayout::No;
        // After all the lines of the fragmentainers before, a last one with nothing but the root inline box too, like one with only floats
        // (LineLayout::lineCount leaves that out).
        if (breakToken && breakToken->previousLine)
            firstLineIndex = breakToken->previousLine->lineIndex + 1;
        auto layoutResult = lineLayout->layout(blockLayoutState, blockFormattingContext, blockLayoutResult, intrudingFloats, forceFullLayout, breakToken);
        outgoingBreakToken = WTF::move(layoutResult.breakToken);
        pushedFloats = WTF::move(layoutResult.pushedFloats);
        auto damagedRect = layoutResult.repaintRect;
        // The inline formatting context lays the floats in the content out before it places them, and their content snaps to the line grid
        // from where they go (RenderBlockFlow::positionNewFloats places a float before its layout), where they are laid out again, and the
        // content with the block sizes that gives them, unless block-level boxes in it took part in the block formatting context already.
        if (contentLineGrid) {
            auto layoutFloatsWhereTheyGo = [&] {
                auto floatSizeChanged = false;
                for (auto& box : atomicInlineLevelBoxes) {
                    // Not one whose content the block formatting context laid out where the float goes (BlockFormattingContext::setLineGridOfFloatContent).
                    if (!box->isFloating() || box->isUnsplittableForPagination() || blockFormattingContext.geometryForFloatInInlineContent(box))
                        continue;
                    auto borderBoxSize = box->borderBoxRect().size();
                    auto lineGridLayoutStates = LineGridLayoutStateMaintainer { layoutContext.get() };
                    pushLineGridLayoutStates(lineGridLayoutStates, inlineContentRoot, *contentLineGrid, contentLineGrid->boxBorderBoxOffset);
                    box->setNeedsLayout(MarkingBehavior::OnlyThis);
                    box->layoutIfNeeded();
                    floatSizeChanged |= box->borderBoxRect().size() != borderBoxSize;
                }
                return floatSizeChanged;
            };
            // The inline formatting context lays them out again in its own layout state.
            if (layoutFloatsWhereTheyGo() && !lineLayout->hasBlocks()) {
                auto contentDamagedRect = lineLayout->layout(blockLayoutState, blockFormattingContext, blockLayoutResult, intrudingFloats, LineLayout::ForceFullLayout::Yes).repaintRect;
                if (damagedRect && contentDamagedRect)
                    damagedRect->unite(*contentDamagedRect);
                else if (contentDamagedRect)
                    damagedRect = contentDamagedRect;
                layoutFloatsWhereTheyGo();
            }
        }
        paintInvalidationScope.setDamagedRect(damagedRect);
    }
    for (auto& box : atomicInlineLevelBoxes) {
        box->clearOverridingContainingBlockContentSize();
        layoutContext->clearContainingBlockBlockSizes(box);
        if (CheckedPtr drawableContent = drawableContentBlock(box)) {
            drawableContent->clearOverridingContainingBlockContentSize();
            layoutContext->clearContainingBlockBlockSizes(*drawableContent);
        }
    }
    auto result = inlineContentLayoutResult(lineLayout.get());
    result.floats = lineLayout->floatsInContent();
    result.pushedFloats = WTF::move(pushedFloats);
    // Content that breaks across fragmentainers is the lines this one took, from the top of the content box.
    if (breakToken || outgoingBreakToken) {
        result.hasLineBoxes = lineLayout->lineCount() > firstLineIndex;
        result.contentBlockSize = result.hasLineBoxes ? std::max(0_lu, lineLayout->lastLineLogicalBottom() - geometry.borderAndPaddingBefore()) : 0_lu;
        result.breakToken = WTF::move(outgoingBreakToken);
        result.firstLineIndex = firstLineIndex;
    }
    if (auto fragmentainerEnd = constraints.fragmentainerEnd())
        result.unbreakableLinesBlockSize = lineLayout->tallestLinesBlockSize(firstLineIndex, std::max(fragmentainerEnd->orphans, fragmentainerEnd->widows));
    return result;
}

} // namespace LayoutIntegration
} // namespace WebCore
