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
#include "LayoutIntegrationBlockLayout.h"

#include "AXObjectCache.h"
#include "BlockFormattingContext.h"
#include "BlockFormattingUtils.h"
#include "BlockIntegrationUtils.h"
#include "LayoutIntegrationBlockCoverage.h"
#include "LayoutIntegrationLineLayout.h"
#include "LayoutRepainter.h"
#include "LocalFrameViewInlines.h"
#include "LocalFrameViewLayoutContext.h"
#include "Logging.h"
#include "RenderBlockFlow.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderDescendantIterator.h"
#include "RenderElementInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderLayer.h"
#include "RenderLayoutState.h"
#include "RenderTableCell.h"
#include "RenderView.h"
#include "ScrollbarUpdateScope.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace LayoutIntegration {

BlockLayout::BlockLayout(RenderBlockFlow& rootRenderer)
    : m_rootRenderer(rootRenderer)
{
}

// The block size the percentage block sizes of the children of the root resolve against (RenderBox::computePercentageLogicalHeight). In
// quirks mode that is past the containing blocks with an auto block size, without the margins, borders and padding of the body and the
// root element (quirks §3.5).
static std::optional<LayoutUnit> contentBoxBlockSizeForPercentages(const RenderBlockFlow& rootRenderer)
{
    auto isPerpendicular = [&](const RenderBlock& containingBlock) {
        return rootRenderer.isHorizontalWritingMode() != containingBlock.isHorizontalWritingMode();
    };
    if (!rootRenderer.skipContainingBlockForPercentHeightCalculation(rootRenderer, false))
        return rootRenderer.availableLogicalHeightForPercentageComputation();
    CheckedPtr<const RenderBox> containingBlockChild = &rootRenderer;
    CheckedPtr<const RenderBlock> containingBlock = &rootRenderer;
    auto skippedMarginsBordersAndPadding = 0_lu;
    while (containingBlock && !is<RenderView>(*containingBlock) && rootRenderer.skipContainingBlockForPercentHeightCalculation(*containingBlock, isPerpendicular(*containingBlock))) {
        if (containingBlock->isBody() || containingBlock->isDocumentElementRenderer()) {
            auto writingMode = containingBlock->writingMode();
            skippedMarginsBordersAndPadding += containingBlock->marginBefore(writingMode) + containingBlock->marginAfter(writingMode) + containingBlock->borderAndPaddingLogicalHeight();
        }
        containingBlockChild = containingBlock;
        containingBlock = containingBlock->containingBlock();
    }
    if (!containingBlock || is<RenderTableCell>(*containingBlock))
        return { };
    if (is<RenderView>(*containingBlock) && rootRenderer.view().frameView().isAutoSizeEnabled())
        return { };
    auto blockSize = isPerpendicular(*containingBlock) ? std::optional { containingBlockChild->containingBlockLogicalWidthForContent() } : containingBlock->availableLogicalHeightForPercentageComputation();
    // A flex container stretching its items knows its block size before it has one (RenderFlexibleBox::isInCrossAxisStretchLayout).
    if (CheckedPtr flexContainer = dynamicDowncast<RenderFlexibleBox>(*containingBlock); !blockSize && flexContainer && flexContainer->isInCrossAxisStretchLayout())
        blockSize = flexContainer->contentBoxLogicalHeight();
    return blockSize.transform([&](auto blockSize) {
        return std::max(0_lu, blockSize - skippedMarginsBordersAndPadding);
    });
}

static Layout::ConstraintsForBlockContent constraintsForBlockContent(const RenderBlockFlow& rootRenderer, RelayoutChildren relayoutChildren)
{
    // The root lives in its parent's formatting context, which has already decided its size.
    auto contentBoxLogicalLeft = rootRenderer.borderAndPaddingStart();
    // The scrollbar in the inline axis sits between the border and the padding: the vertical one, on the left or on the right, in horizontal
    // writing modes and the horizontal one, at the bottom, in vertical writing modes. The block formatting context measures from the inline
    // start, so it counts when it is on that side, like RenderBlockFlow::determineLogicalLeftPositionForChild, which measures from the logical
    // left (top in vertical writing modes) and clamps it to the space it has.
    auto writingMode = rootRenderer.writingMode();
    auto scrollbarIsOnLogicalLeft = writingMode.isHorizontal() && rootRenderer.shouldPlaceVerticalScrollbarOnLeft();
    if (scrollbarIsOnLogicalLeft == writingMode.isLogicalLeftInlineStart())
        contentBoxLogicalLeft += std::min<LayoutUnit>(rootRenderer.scrollbarLogicalWidth(), std::max(0_lu, rootRenderer.logicalWidth() - rootRenderer.borderAndPaddingLogicalWidth()));
    auto horizontalConstraints = Layout::HorizontalConstraints { contentBoxLogicalLeft, rootRenderer.contentBoxLogicalWidth() };
    return { { horizontalConstraints, rootRenderer.borderAndPaddingBefore() }, contentBoxBlockSizeForPercentages(rootRenderer), rootRenderer.availableLogicalHeight(AvailableLogicalHeightType::ExcludeMarginBorderPadding), relayoutChildren == RelayoutChildren::Yes };
}

void BlockLayout::layout(RelayoutChildren relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom)
{
    auto paintInvalidation = PaintInvalidation { };
    auto integrationUtils = BlockIntegrationUtils { paintInvalidation };
    auto blockLayoutState = Layout::BlockLayoutState { { } };
    auto result = Layout::BlockFormattingContext { rootRenderer(), integrationUtils, blockLayoutState }.layout(constraintsForBlockContent(rootRenderer(), relayoutChildren), BlockIntegrationUtils::rootGeometry(rootRenderer()));
    LOG_WITH_STREAM(Layout, stream << "BlockLayout for root (" << &rootRenderer() << "): " << result);

    updateRenderers(result, paintInvalidation);
    // Like updateRenderers for the block containers in the root: the inline formatting context took the overflow of the block-level boxes in
    // the root's inline content from their renderers, before they had their new geometry.
    if (CheckedPtr lineLayout = rootRenderer().inlineLayout(); lineLayout && lineLayout->hasBlocks())
        lineLayout->updateOverflow();
    // RenderBlockFlow::layoutBlock resolves the used block size of the root from this.
    rootRenderer().setLogicalHeight(rootRenderer().borderAndPaddingLogicalHeight() + rootRenderer().scrollbarLogicalHeight() + result.intrinsicContentBlockSize);
    // Like RenderBlockFlow::layoutInlineContent for the inline content of the root.
    if (auto damagedInlineContent = paintInvalidation.damagedInlineContentLogicalTopAndBottom(rootRenderer(), rootRenderer().logicalHeight())) {
        repaintLogicalTop = damagedInlineContent->logicalTop;
        repaintLogicalBottom = damagedInlineContent->logicalBottom;
        if (CheckedPtr cache = protect(rootRenderer().document())->existingAXObjectCache())
            cache->onLaidOutInlineContent(rootRenderer());
    }
}

std::pair<LayoutUnit, LayoutUnit> BlockLayout::computeIntrinsicInlineSizes(const RenderBlockFlow& rootRenderer)
{
    auto paintInvalidation = PaintInvalidation { };
    auto integrationUtils = BlockIntegrationUtils { paintInvalidation };
    auto blockLayoutState = Layout::BlockLayoutState { { } };
    auto sizes = Layout::BlockFormattingContext { rootRenderer, integrationUtils, blockLayoutState }.computeIntrinsicInlineSizes(rootRenderer);

    // The render tree stops invalidating intrinsic sizes at a box whose intrinsic sizes are already invalid, expecting its ancestors to be
    // invalid too. The block formatting context does not compute the render tree's intrinsic sizes of the boxes in the root, so those that
    // are invalid stay invalid, while the root's become valid: let invalidation go past them (RenderObject::invalidateContentLogicalWidths).
    SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef mutableRootRenderer = const_cast<RenderBlockFlow&>(rootRenderer);
    for (CheckedRef descendant : descendantsOfType<RenderElement>(mutableRootRenderer.get())) {
        if (!descendant->hasInvalidContentLogicalWidths())
            continue;
        descendant->clearContentLogicalWidthsInvalidation();
        descendant->invalidateContentLogicalWidths(MarkingBehavior::OnlyThis);
    }
    return { sizes.minimum, sizes.maximum };
}

bool BlockLayout::contentDependsOnBlockSize(const RenderBlockFlow& rootRenderer)
{
    auto writingMode = rootRenderer.writingMode();
    // Percentage block sizes resolve against the content box of the root through inline boxes and anonymous block boxes (CSS 2.2 §9.2.1.1,
    // §10.5). Any other box gives its content a block size of its own, which depends on the root's only when it is a percentage itself.
    // Out-of-flow boxes resolve theirs against the padding box of their containing block, which is definite either way.
    auto hasPercentageBlockSizeContent = [&] {
        for (CheckedPtr<const RenderObject> descendant = rootRenderer.firstChild(); descendant;) {
            CheckedPtr box = dynamicDowncast<RenderBox>(*descendant);
            if (box && !box->isOutOfFlowPositioned() && Layout::BlockFormattingUtils::hasPercentageBlockSize(*box, writingMode))
                return true;
            auto passesBlockSizeThrough = descendant->isInlineBox() || (box && box->isAnonymousBlock());
            descendant = passesBlockSizeThrough ? descendant->nextInPreOrder(&rootRenderer) : descendant->nextInPreOrderAfterChildren(&rootRenderer);
        }
        return false;
    };
    return hasPercentageBlockSizeContent() && canUseForBlockLayout(rootRenderer);
}

void BlockLayout::layoutOutOfFlowBoxes(const RenderBlock& containingBlock)
{
    auto paintInvalidation = PaintInvalidation { };
    layoutOutOfFlowBoxes(containingBlock, paintInvalidation);
}

// The padding box of a block in the coordinates its children are positioned in (RenderBox::setLogicalLocation): from its left edge (top in
// vertical writing modes) in the inline axis, and from its block-start edge in the block axis, which is where the render tree measures block
// positions from in a flipped block flow direction too (RenderBox::flipForWritingModeForChild).
static LayoutRect logicalPaddingBoxRect(const RenderBox& box)
{
    auto writingMode = box.writingMode();
    auto paddingBox = box.paddingBoxRect();
    auto borderBoxSize = box.borderBoxSize();
    if (!writingMode.isHorizontal()) {
        paddingBox = paddingBox.transposedRect();
        borderBoxSize = borderBoxSize.transposedSize();
    }
    if (writingMode.isBlockFlipped())
        paddingBox.setY(borderBoxSize.height() - paddingBox.maxY());
    return paddingBox;
}

// The static position the in-flow layout of its parent gave the box (RenderLayer::staticInlinePosition, staticBlockPosition), relative to the
// border box of its block container, moved through the positions of the boxes in between, relative offsets included (like PositionedLayoutConstraints::computedInlineStaticDistance)
// to the padding box of its containing block. Coverage keeps those boxes in the containing block's writing mode.
static Layout::OutOfFlowGeometry::StaticPosition staticPosition(const RenderBox& outOfFlowBox, const RenderBlock& containingBlock)
{
    CheckedPtr<const RenderElement> staticPositionBlock = outOfFlowBox.parent();
    while (staticPositionBlock->isInlineBox())
        staticPositionBlock = staticPositionBlock->parent();
    CheckedRef blockContainer = downcast<RenderBlock>(*staticPositionBlock);
    CheckedRef layer = *outOfFlowBox.layer();
    auto position = LayoutPoint { blockContainer->writingMode().isLogicalLeftInlineStart() ? layer->staticInlinePosition() : blockContainer->logicalWidth() - layer->staticInlinePosition(), layer->staticBlockPosition() };
    // An out-of-flow box between them is positioned in its own containing block, which is where container() goes.
    for (CheckedPtr<const RenderElement> box = blockContainer.ptr(); box != &containingBlock; box = box->container()) {
        if (CheckedPtr renderBox = dynamicDowncast<RenderBox>(*box)) {
            position.move(renderBox->logicalLeft(), renderBox->logicalTop());
            auto inFlowOffset = renderBox->offsetForInFlowPosition();
            position.move(renderBox->isHorizontalWritingMode() ? inFlowOffset : inFlowOffset.transposedSize());
        }
    }
    auto paddingBox = logicalPaddingBoxRect(containingBlock);
    auto inlinePosition = containingBlock.writingMode().isLogicalLeftInlineStart() ? position.x() - paddingBox.x() : paddingBox.maxX() - position.x();
    return { inlinePosition, position.y() - paddingBox.y(), blockContainer->writingMode().isLogicalLeftInlineStart() != containingBlock.writingMode().isLogicalLeftInlineStart() };
}

// Like RenderBlock::layoutOutOfFlowBoxes once the containing block has its size: the out-of-flow boxes BlockLayout::updateRenderers registered
// on it (RenderBlock::outOfFlowBoxes()).
void BlockLayout::layoutOutOfFlowBoxes(const RenderBlock& containingBlock, PaintInvalidation& paintInvalidation)
{
    auto* outOfFlowBoxes = containingBlock.outOfFlowBoxes();
    if (!outOfFlowBoxes)
        return;
    auto paddingBox = logicalPaddingBoxRect(containingBlock);
    auto paddingBoxInlineStart = containingBlock.writingMode().isLogicalLeftInlineStart() ? paddingBox.x() : containingBlock.logicalWidth() - paddingBox.maxX();
    auto outOfFlowContainingBlock = Layout::OutOfFlowContainingBlock { containingBlock, paddingBoxInlineStart, paddingBox.y(), paddingBox.width(), paddingBox.height() };
    auto integrationUtils = BlockIntegrationUtils { paintInvalidation };
    auto blockLayoutState = Layout::BlockLayoutState { { } };
    // Laying out a box registers the out-of-flow boxes in it, which can make the list grow while it is iterated (like in RenderBlock::layoutOutOfFlowBoxes).
    for (CheckedRef outOfFlowBox : *outOfFlowBoxes) {
        auto result = Layout::BlockFormattingContext { rootRenderer(), integrationUtils, blockLayoutState }.layoutOutOfFlowBox(outOfFlowBox, outOfFlowContainingBlock, staticPosition(outOfFlowBox, containingBlock));
        LOG_WITH_STREAM(Layout, stream << "BlockLayout for out-of-flow box (" << outOfFlowBox.ptr() << "): " << result);
        updateRenderers(result, paintInvalidation);
    }
    SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef mutableContainingBlock = const_cast<RenderBlock&>(containingBlock);
    // The transforms of the boxes that depend on the size of their containing block (offset-path), before the overflow they extend.
    mutableContainingBlock->updateOutOfFlowDescendantTransformsAfterLayout();
    // Fixed positioned boxes do not scroll with the content, so they add no scrollable overflow (RenderBlock::addOverflowFromOutOfFlowBoxes).
    for (CheckedRef outOfFlowBox : *outOfFlowBoxes) {
        if (outOfFlowBox->isAbsolutelyPositioned())
            mutableContainingBlock->addOverflowFromContainedBox(outOfFlowBox);
    }
}

void BlockLayout::updateRenderers(const Layout::BlockLayoutResult& result, PaintInvalidation& paintInvalidation)
{
    // This is the only place where the block formatting context's renderers are written, and the only place where its logical
    // coordinates (from the inline start of each containing block) are mapped to the render tree's.
    auto mutableRenderer = [](const CheckedRef<const RenderBox>& renderer) -> CheckedRef<RenderBox> {
        SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef mutableRenderer = const_cast<RenderBox&>(renderer.get());
        return mutableRenderer;
    };

    // The containing block of an in-flow block-level box is its parent, or the block container whose inline content it is in.
    auto containingBlockOf = [](const RenderBox& box) -> const RenderBox& {
        if (box.isOutOfFlowPositioned() || box.parent()->isInlineBox())
            return *box.containingBlock();
        return downcast<RenderBox>(*box.parent());
    };
    // The paint invalidation is what RenderBlockFlow::layoutBlockChild and layoutBlock do, from the geometry before anything is written.
    struct PreviousState {
        LayoutRect borderBoxRect;
        bool everHadLayout { false };
    };
    auto previousStates = Vector<PreviousState> { };
    auto layoutRepainters = Vector<LayoutRepainter> { };
    previousStates.reserveInitialCapacity(result.boxes.size());
    layoutRepainters.reserveInitialCapacity(result.boxes.size());
    for (auto& box : result.boxes) {
        CheckedRef renderer = mutableRenderer(box.renderer);
        previousStates.append({ renderer->borderBoxRectInContainer(), renderer->everHadLayout() });
        // The formatting context of a box laid out through the render tree invalidated it when it laid it out.
        layoutRepainters.append(LayoutRepainter { renderer, box.isLaidOutThroughRenderTree ? std::optional { LayoutRepainter::CheckForRepaint::No } : std::nullopt });
    }

    for (auto& box : result.boxes) {
        CheckedRef renderer = mutableRenderer(box.renderer);
        auto& geometry = box.geometry;
        // Margins follow the direction of the containing block.
        auto containingBlockWritingMode = containingBlockOf(renderer).writingMode();
        renderer->setMarginStart(geometry.marginStart(), containingBlockWritingMode);
        renderer->setMarginEnd(geometry.marginEnd(), containingBlockWritingMode);
        renderer->setMarginBefore(geometry.marginBefore(), containingBlockWritingMode);
        renderer->setMarginAfter(geometry.marginAfter(), containingBlockWritingMode);
        // Sizes and positions are in the writing mode of the containing block, which an orthogonal box does not share.
        auto borderBoxSize = LayoutSize { geometry.borderBoxWidth(), geometry.borderBoxHeight() };
        renderer->setBorderBoxSize(containingBlockWritingMode.isHorizontal() ? borderBoxSize : borderBoxSize.transposedSize());
        // What RenderBlockFlow::layoutBlock leaves for the parent to collapse with, when the parent is laid out by the render tree path
        // and this box is not laid out again. A box laid out through the render tree has them from its own layout, in its own writing
        // mode, which an orthogonal box does not share with the block formatting context.
        if (CheckedPtr blockFlow = box.isLaidOutThroughRenderTree ? nullptr : dynamicDowncast<RenderBlockFlow>(renderer)) {
            blockFlow->setMaxMarginBeforeValues(box.collapsedMarginsBefore.positive, box.collapsedMarginsBefore.negative);
            blockFlow->setMaxMarginAfterValues(box.collapsedMarginsAfter.positive, box.collapsedMarginsAfter.negative);
            blockFlow->setHasMarginBeforeQuirk(box.marginBeforeIsQuirky);
            blockFlow->setHasMarginAfterQuirk(box.marginAfterIsQuirky);
        }
    }

    // The render tree measures the logical left from the left (top in vertical writing modes) edge of the containing block in
    // either direction, so placing a box in a right-to-left containing block needs the containing block's final width.
    for (auto& box : result.boxes) {
        CheckedRef renderer = mutableRenderer(box.renderer);
        auto& geometry = box.geometry;
        CheckedRef containingBlock = containingBlockOf(renderer);
        auto logicalLeft = Layout::BoxGeometry::borderBoxLeft(geometry);
        if (!containingBlock->writingMode().isLogicalLeftInlineStart())
            logicalLeft = containingBlock->logicalWidth() - logicalLeft - geometry.borderBoxWidth();
        auto location = LayoutPoint { logicalLeft, Layout::BoxGeometry::borderBoxTop(geometry) };
        renderer->setLocation(containingBlock->writingMode().isHorizontal() ? location : location.transposedPoint());
    }

    // What RenderBlockFlow::adjustOutOfFlowBlock and layoutBlockChildren do for an out-of-flow child: its layer keeps the static position, and its
    // containing block registers it, to lay it out once it has its size.
    for (auto& outOfFlowBox : result.outOfFlowBoxes) {
        CheckedRef renderer = mutableRenderer(outOfFlowBox.renderer);
        CheckedRef layer = *renderer->layer();
        if (layer->staticBlockPosition() != outOfFlowBox.staticBlockPosition) {
            layer->setStaticBlockPosition(outOfFlowBox.staticBlockPosition);
            CheckedRef style = renderer->style();
            if (style->hasStaticBlockPosition(renderer->parent()->isHorizontalWritingMode()))
                renderer->setChildNeedsLayout(MarkingBehavior::OnlyThis);
        }
        layer->setStaticInlinePosition(outOfFlowBox.staticInlinePosition);
        CheckedPtr containingBlock = renderer->containingBlock();
        containingBlock->addOutOfFlowBox(renderer);
    }

    // How far each box and its containing block moved within the root. While a box is laid out, the render tree's layout delta holds
    // that offset so that its paint invalidation sees it where it was. In reverse post-order the containing block comes before the box.
    auto locationChanges = Vector<LayoutSize>(result.boxes.size());
    auto containingBlockLocationChanges = Vector<LayoutSize>(result.boxes.size());
    auto containingBlockIndexes = Vector<size_t> { };
    for (auto boxIndex = result.boxes.size(); boxIndex--;) {
        CheckedRef renderer = result.boxes[boxIndex].renderer;
        while (!containingBlockIndexes.isEmpty() && result.boxes[containingBlockIndexes.last()].renderer.ptr() != &containingBlockOf(renderer))
            containingBlockIndexes.removeLast();
        containingBlockLocationChanges[boxIndex] = containingBlockIndexes.isEmpty() ? LayoutSize { } : locationChanges[containingBlockIndexes.last()];
        locationChanges[boxIndex] = containingBlockLocationChanges[boxIndex] + (previousStates[boxIndex].borderBoxRect.location() - renderer->location());
        containingBlockIndexes.append(boxIndex);
    }

    CheckedRef layoutContext = rootRenderer().view().frameView().layoutContext();
    // The layout state has the paint offset of the root only, not of the boxes inside.
    auto layoutStateDisabler = LayoutStateDisabler { layoutContext.get() };
    auto invalidateWithLayoutDelta = [&](LayoutSize layoutDelta, NOESCAPE auto&& invalidate) {
        layoutContext->addLayoutDelta(layoutDelta);
        invalidate();
        layoutContext->addLayoutDelta(-layoutDelta);
    };

    // Descendants come before their ancestors, which the overflow of the ancestors needs.
    for (size_t boxIndex = 0; boxIndex < result.boxes.size(); ++boxIndex) {
        auto& box = result.boxes[boxIndex];
        CheckedRef renderer = mutableRenderer(box.renderer);
        // The formatting context of a box laid out through the render tree did what follows for it.
        CheckedPtr blockFlow = box.isLaidOutThroughRenderTree ? nullptr : dynamicDowncast<RenderBlockFlow>(renderer);
        if (blockFlow) {
            blockFlow->clearOverflow();
            // Like RenderBlockFlow::layoutBlock, the content area ends where the content does, not where the box does.
            auto contentArea = blockFlow->flippedContentBoxRect();
            auto contentBlockEnd = blockFlow->borderAndPaddingBefore() + box.intrinsicContentBlockSize;
            auto isHorizontalWritingMode = blockFlow->writingMode().isHorizontal();
            if (isHorizontalWritingMode)
                contentArea.shiftMaxYEdgeTo(contentBlockEnd);
            else
                contentArea.shiftMaxXEdgeTo(contentBlockEnd);
            // The transforms of the children that depend on the size of their containing block (offset-path), before the overflow they extend.
            blockFlow->updateInFlowDescendantTransformsAfterLayout();
            // The inline formatting context took the overflow of the block-level boxes in the inline content from their renderers, before they
            // had their new geometry.
            if (CheckedPtr lineLayout = blockFlow->inlineLayout(); lineLayout && lineLayout->hasBlocks())
                lineLayout->updateOverflow();
            blockFlow->computeInFlowOverflow(contentArea, isHorizontalWritingMode ? RenderBox::ComputeOverflowOptions::MarginsExtendContentAreaX : RenderBox::ComputeOverflowOptions::MarginsExtendContentAreaY);
            // Like RenderBlockFlow::layoutBlock, once the box has its size and its in-flow overflow.
            invalidateWithLayoutDelta(locationChanges[boxIndex], [&] {
                layoutOutOfFlowBoxes(*blockFlow, paintInvalidation);
            });
        }
        // Like RenderBlockFlow::layoutBlock: the transform depends on the size of the box (transform-origin).
        renderer->updateLayerTransform();

        // What RenderBlockFlow::layoutBlock invalidates: changes inside the box, at the place it had before it moved.
        auto damagedInlineContent = std::optional<PaintInvalidation::LogicalTopAndBottom> { };
        if (blockFlow) {
            damagedInlineContent = paintInvalidation.damagedInlineContentLogicalTopAndBottom(*blockFlow, box.geometry.verticalBorderAndPadding() + box.intrinsicContentBlockSize);
            if (CheckedPtr cache = protect(blockFlow->document())->existingAXObjectCache(); cache && damagedInlineContent)
                cache->onLaidOutInlineContent(*blockFlow);
        }
        invalidateWithLayoutDelta(locationChanges[boxIndex], [&] {
            auto didInvalidateWholeBox = layoutRepainters[boxIndex].repaintAfterLayout();
            if (damagedInlineContent && !didInvalidateWholeBox)
                downcast<RenderBlockFlow>(renderer).repaintInlineContentAfterLayout(damagedInlineContent->logicalTop, damagedInlineContent->logicalBottom);
        });
        // What RenderBlock::layout and RenderBlock::recomputeLogicalWidth do once the box is laid out. The scroll size and position of a
        // scroll container follow its new overflow; coverage keeps out the ones whose scrollbars would come or go with it.
        if (blockFlow) {
            blockFlow->updateScrollInfoAfterLayout();
            blockFlow->setShouldForceRelayoutChildren(false);
        }
        renderer->invalidateBackgroundObscurationStatus();
        renderer->setScrollAnchoringSuppressionStyleChanged(false);
        renderer->clearNeedsLayout();

        // What RenderBlockFlow::layoutBlockChild invalidates: the box if it is new or moved.
        if (!renderer->checkForRepaintDuringLayout())
            continue;
        auto& previousState = previousStates[boxIndex];
        invalidateWithLayoutDelta(containingBlockLocationChanges[boxIndex], [&] {
            if (!previousState.everHadLayout)
                renderer->repaint();
            else if (!renderer->parent()->selfNeedsLayout())
                renderer->repaintDuringLayoutIfMoved(previousState.borderBoxRect);
        });
    }
}

} // namespace LayoutIntegration
} // namespace WebCore
