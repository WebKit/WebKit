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
#include "RenderBlockFlow.h"
#include "RenderBlockFlowInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderElementInlines.h"
#include "RenderObjectInlines.h"
#include "RenderHTMLCanvas.h"
#include "RenderLayer.h"
#include "RenderLayoutState.h"
#include "RenderLineBreak.h"
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
        || previousConstraints->formattingRootBorderBoxSize() != constraints.formattingRootBorderBoxSize();
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

BlockIntegrationUtils::IndependentFormattingContextLayoutResult BlockIntegrationUtils::layoutIndependentFormattingContext(const RenderBox& root, const Layout::ConstraintsForIndependentFormattingContext& constraints)
{
    // Only the root and its content are written here, by the root's own formatting context, but for the block-direction margins the block
    // formatting context gives it, and nothing else in the block formatting context reads them.
    SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef box = const_cast<RenderBox&>(root);
    // Its parent, or the block container whose inline content it is in.
    CheckedRef containingBlock = downcast<RenderBlockFlow>(*box->containingBlock());

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
    // block instead (RenderBox::computePercentageLogicalHeight), and takes the block size as its available inline space
    // (RenderBox::perpendicularContainingBlockLogicalHeight).
    auto containingBlockWritingMode = containingBlock->writingMode();
    // Like RenderBlockFlow::layoutBlockChild, before the root's own layout reads them. They follow the writing mode of the containing block.
    box->setMarginBefore(constraints.marginBefore, containingBlockWritingMode);
    box->setMarginAfter(constraints.marginAfter, containingBlockWritingMode);
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
    // Its content looks past an orthogonal box to the inline size of the containing block instead (RenderBox::computePercentageLogicalHeight).
    if (!isOrthogonal) {
        auto containingBlockBlockSizes = LocalFrameViewLayoutContext::ContainingBlockBlockSizes { constraints.contentPercentageBasis, constraints.containingBlockAvailableBlockSize, constraints.containingBlockAvailableBlockSizeIncludingMarginBorderPadding };
        layoutContext->setContainingBlockBlockSizes(box, containingBlockBlockSizes);
        if (drawableContent)
            layoutContext->setContainingBlockBlockSizes(*drawableContent, containingBlockBlockSizes);
    }
    {
        // The layout state has the paint offset of the block formatting context root only, not of the boxes inside.
        auto layoutStateDisabler = LayoutStateDisabler { layoutContext.get() };
        box->layoutIfNeeded();
    }
    if (!isOrthogonal) {
        layoutContext->clearContainingBlockBlockSizes(box);
        if (drawableContent)
            layoutContext->clearContainingBlockBlockSizes(*drawableContent);
    }
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

LayoutSize BlockIntegrationUtils::layoutOutOfFlowIndependentFormattingContext(const RenderBox& root, std::optional<LayoutUnit> borderBoxInlineSize, std::optional<LayoutUnit> borderBoxBlockSize, std::optional<LayoutUnit> borderBoxInlinePosition)
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
    // keywords the intrinsic sizes of the content (RenderBox::computeIntrinsicKeywordLogicalWidths), as the contributions apply them.
    CheckedRef style = box.style();
    if (!style->logicalWidth().isIntrinsic() && !style->logicalMinWidth().isIntrinsic() && !style->logicalMaxWidth().isIntrinsic())
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

std::optional<LayoutUnit> BlockIntegrationUtils::updateLegacyLineClampAfterInlineContent(const RenderBlockFlow& blockContainer)
{
    auto& layoutState = *blockContainer.view().frameView().layoutContext().layoutState();
    CheckedPtr inlineLayout = blockContainer.inlineLayout();
    auto legacyLineClamp = layoutState.legacyLineClamp();
    if (!legacyLineClamp || !inlineLayout || blockContainer.isFloatingOrOutOfFlowPositioned())
        return { };

    legacyLineClamp->currentLineCount += inlineLayout->lineCount();
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
        legacyLineClamp->clampedContentLogicalHeight = clampedContentBlockSize;
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

bool BlockIntegrationUtils::inlineContentMakesLineBoxesBeforeBlockLevelBoxes(const RenderBlockFlow& blockContainer)
{
    for (CheckedPtr<const RenderObject> descendant = blockContainer.firstChild(); descendant;) {
        if (descendant->isFloatingOrOutOfFlowPositioned()) {
            descendant = descendant->nextInPreOrderAfterChildren(&blockContainer);
            continue;
        }
        if (CheckedPtr text = dynamicDowncast<RenderText>(*descendant)) {
            if (!text->containsOnlyCollapsibleWhitespace())
                return true;
        } else if (CheckedPtr lineBreak = dynamicDowncast<RenderLineBreak>(*descendant)) {
            if (!lineBreak->isWBR())
                return true;
        } else if (CheckedPtr box = dynamicDowncast<RenderBox>(*descendant)) {
            // The block-start margin of a block-level box can still collapse with the block container's.
            return !box->isBlockLevelBox();
        } else if (descendant->isInlineBox()) {
            CheckedRef inlineBox = downcast<RenderBoxModelObject>(*descendant);
            CheckedRef style = inlineBox->style();
            auto writingMode = blockContainer.writingMode();
            auto hasInlineAxisMargin = [&](auto& margin) {
                return !margin.isAuto() && !margin.isPossiblyZero();
            };
            if (hasInlineAxisMargin(style->marginStart(writingMode)) || hasInlineAxisMargin(style->marginEnd(writingMode)) || inlineBox->borderStart() || inlineBox->borderEnd() || !style->paddingStart(writingMode).isPossiblyZero() || !style->paddingEnd(writingMode).isPossiblyZero())
                return true;
        }
        descendant = descendant->nextInPreOrder(&blockContainer);
    }
    return false;
}

BlockIntegrationUtils::InlineContentLayoutResult BlockIntegrationUtils::layoutInlineContent(const RenderBlockFlow& blockContainer, const Layout::ConstraintsForInlineContent& constraints, const Layout::BoxGeometry& geometry, std::optional<LayoutUnit> contentBoxBlockSize, LayoutUnit fillAvailableBlockSize, LayoutUnit replacedElementAvailableBlockSize, LayoutUnit orthogonalFlowAvailableInlineSpace, bool relayoutChildren, const Layout::BlockLayoutState& blockLayoutState, const Layout::PlacedFloats::List& intrudingFloats, Layout::BlockFormattingContext& blockFormattingContext, Layout::BlockLayoutResult& blockLayoutResult)
{
    // Only the line layout of this block container and its inline content are written here, and nothing else in the block formatting
    // context reads them.
    auto& inlineContentRoot = const_cast<RenderBlockFlow&>(blockContainer);

    // Like RenderBlockFlow::layoutBlockChild, which only lays out a block that needs it, and RenderBlock::recomputeLogicalWidth, which
    // lays its content out again when its width changed: otherwise the lines stay as they are. A block whose content only needs its overflow
    // recomputed or its out-of-flow boxes laid out keeps them too (RenderBlock::canPerformSimplifiedLayout). Content takes its lines off the
    // line-clamp budget as it gets laid out, and what the content before it left of the budget may have changed: RenderBlockFlow::layoutInFlowChildren
    // lays out all the content of a line-clamp container's formatting context again.
    auto isInLineClampContext = !!blockLayoutState.lineClamp();
    auto contentNeedsLayout = relayoutChildren || isInLineClampContext || inlineContentRoot.selfNeedsLayout() || inlineContentRoot.normalChildNeedsLayout() || inlineContentRoot.outOfFlowChildNeedsStaticPositionLayout();
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
    auto invalidateChildren = relayoutChildren || isInLineClampContext || inlineContentRoot.logicalWidth() != geometry.borderBoxWidth() || inlineContentRoot.hasBorderOrPaddingLogicalWidthChanged();
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
        } else if (!percentagesResolveAgainstTableCell(inlineContentRoot) && Layout::BlockFormattingUtils::hasPercentageBlockSize(*box, box->writingMode()))
            box->setOverridingContainingBlockContentLogicalHeight(box->isRenderReplaced() && box->document().inQuirksMode() ? std::optional { replacedElementAvailableBlockSize } : contentBoxBlockSize);
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
    {
        // The layout state has the paint offset of the block formatting context root only, not of the boxes inside.
        auto layoutStateDisabler = LayoutStateDisabler { layoutContext.get() };
        // The block formatting context needs the margins of every block-level box in the content, so it lays out all the lines again
        // (like RenderBlockFlow::layoutInlineContent when one of them is dirty).
        auto hasBlockLevelBoxes = inlineContentStatus.hasDirtyInFlowBlockLevelElement || lineLayout->hasBlocks();
        // The lines before the damage would need the floats in the content placed before them, which only the render tree's float lists keep.
        auto hasFloats = !intrudingFloats.isEmpty() || inlineContentHasFloats(inlineContentRoot);
        auto forceFullLayout = invalidateChildren || hasBlockLevelBoxes || hasFloats ? LineLayout::ForceFullLayout::Yes : LineLayout::ForceFullLayout::No;
        paintInvalidationScope.setDamagedRect(lineLayout->layout(blockLayoutState, blockFormattingContext, blockLayoutResult, intrudingFloats, forceFullLayout).repaintRect);
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
    return result;
}

} // namespace LayoutIntegration
} // namespace WebCore
