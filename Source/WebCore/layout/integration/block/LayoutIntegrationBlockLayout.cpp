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
#include "BlockIntegrationUtils.h"
#include "LayoutRepainter.h"
#include "LocalFrameViewInlines.h"
#include "LocalFrameViewLayoutContext.h"
#include "Logging.h"
#include "RenderBlockFlow.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderDescendantIterator.h"
#include "RenderLayoutState.h"
#include "RenderView.h"
#include "ScrollbarUpdateScope.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace LayoutIntegration {

BlockLayout::BlockLayout(RenderBlockFlow& rootRenderer)
    : m_rootRenderer(rootRenderer)
{
}

static Layout::ConstraintsForBlockContent constraintsForBlockContent(const RenderBlockFlow& rootRenderer)
{
    // The root lives in its parent's formatting context, which has already decided its size.
    auto contentBoxLogicalLeft = rootRenderer.borderAndPaddingStart();
    // The vertical scrollbar sits between the border and the padding, on the inline start side when it is on the left of a left-to-right
    // box or on the right of a right-to-left one (like RenderBlockFlow::determineLogicalLeftPositionForChild, it is clamped to the space it has).
    // In vertical writing modes, it is in the block axis.
    if (rootRenderer.isHorizontalWritingMode() && rootRenderer.shouldPlaceVerticalScrollbarOnLeft() == rootRenderer.writingMode().isLogicalLeftInlineStart())
        contentBoxLogicalLeft += std::min<LayoutUnit>(rootRenderer.verticalScrollbarWidth(), std::max(0_lu, rootRenderer.logicalWidth() - rootRenderer.borderAndPaddingLogicalWidth()));
    auto horizontalConstraints = Layout::HorizontalConstraints { contentBoxLogicalLeft, rootRenderer.contentBoxLogicalWidth() };
    return { { horizontalConstraints, rootRenderer.borderAndPaddingBefore() }, rootRenderer.availableLogicalHeightForPercentageComputation() };
}

void BlockLayout::layout()
{
    auto paintInvalidation = PaintInvalidation { };
    auto integrationUtils = BlockIntegrationUtils { paintInvalidation };
    auto blockLayoutState = Layout::BlockLayoutState { { } };
    auto result = Layout::BlockFormattingContext { rootRenderer(), integrationUtils, blockLayoutState }.layout(constraintsForBlockContent(rootRenderer()));
    LOG_WITH_STREAM(Layout, stream << "BlockLayout for root (" << &rootRenderer() << "): " << result);

    updateRenderers(result, paintInvalidation);
    // RenderBlockFlow::layoutBlock resolves the used block size of the root from this.
    rootRenderer().setLogicalHeight(rootRenderer().borderAndPaddingLogicalHeight() + rootRenderer().scrollbarLogicalHeight() + result.intrinsicContentBlockSize);
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

void BlockLayout::updateRenderers(const Layout::BlockLayoutResult& result, PaintInvalidation& paintInvalidation)
{
    // This is the only place where the block formatting context's renderers are written, and the only place where its logical
    // coordinates (from the inline start of each containing block) are mapped to the render tree's.
    auto mutableRenderer = [](const Layout::BlockLayoutResult::Box& box) -> CheckedRef<RenderBox> {
        SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef renderer = const_cast<RenderBox&>(box.renderer.get());
        return renderer;
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
        CheckedRef renderer = mutableRenderer(box);
        previousStates.append({ renderer->borderBoxRectInContainer(), renderer->everHadLayout() });
        layoutRepainters.append(LayoutRepainter { renderer });
    }

    for (auto& box : result.boxes) {
        CheckedRef renderer = mutableRenderer(box);
        auto& geometry = box.geometry;
        // Margins follow the direction of the containing block, which is the parent of an in-flow block-level box.
        auto containingBlockWritingMode = renderer->parent()->writingMode();
        renderer->setMarginStart(geometry.marginStart(), containingBlockWritingMode);
        renderer->setMarginEnd(geometry.marginEnd(), containingBlockWritingMode);
        renderer->setMarginBefore(geometry.marginBefore(), containingBlockWritingMode);
        renderer->setMarginAfter(geometry.marginAfter(), containingBlockWritingMode);
        renderer->setLogicalWidth(geometry.borderBoxWidth());
        renderer->setLogicalHeight(geometry.borderBoxHeight());
        // What RenderBlockFlow::layoutBlock leaves for the parent to collapse with, when the parent is laid out by the render tree path
        // and this box is not laid out again.
        if (CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(renderer)) {
            blockFlow->setMaxMarginBeforeValues(box.collapsedMarginsBefore.positive, box.collapsedMarginsBefore.negative);
            blockFlow->setMaxMarginAfterValues(box.collapsedMarginsAfter.positive, box.collapsedMarginsAfter.negative);
            blockFlow->setHasMarginBeforeQuirk(box.marginBeforeIsQuirky);
            blockFlow->setHasMarginAfterQuirk(box.marginAfterIsQuirky);
        }
    }

    // The render tree measures the logical left from the left (top in vertical writing modes) edge of the containing block in
    // either direction, so placing a box in a right-to-left containing block needs the containing block's final width.
    for (auto& box : result.boxes) {
        CheckedRef renderer = mutableRenderer(box);
        auto& geometry = box.geometry;
        CheckedRef containingBlock = downcast<RenderBox>(*renderer->parent());
        auto logicalLeft = Layout::BoxGeometry::borderBoxLeft(geometry);
        if (!containingBlock->writingMode().isLogicalLeftInlineStart())
            logicalLeft = containingBlock->logicalWidth() - logicalLeft - geometry.borderBoxWidth();
        renderer->setLogicalLocation({ logicalLeft, Layout::BoxGeometry::borderBoxTop(geometry) });
    }

    // How far each box and its containing block moved within the root. While a box is laid out, the render tree's layout delta holds
    // that offset so that its paint invalidation sees it where it was. In reverse post-order the containing block comes before the box.
    auto locationChanges = Vector<LayoutSize>(result.boxes.size());
    auto containingBlockLocationChanges = Vector<LayoutSize>(result.boxes.size());
    auto containingBlockIndexes = Vector<size_t> { };
    for (auto boxIndex = result.boxes.size(); boxIndex--;) {
        CheckedRef renderer = result.boxes[boxIndex].renderer;
        while (!containingBlockIndexes.isEmpty() && result.boxes[containingBlockIndexes.last()].renderer.ptr() != renderer->parent())
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
        CheckedRef renderer = mutableRenderer(box);
        if (CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(renderer)) {
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
            blockFlow->computeInFlowOverflow(contentArea, isHorizontalWritingMode ? RenderBox::ComputeOverflowOptions::MarginsExtendContentAreaX : RenderBox::ComputeOverflowOptions::MarginsExtendContentAreaY);
        }
        // Like RenderBlockFlow::layoutBlock: the transform depends on the size of the box (transform-origin).
        renderer->updateLayerTransform();

        // What RenderBlockFlow::layoutBlock invalidates: changes inside the box, at the place it had before it moved.
        auto damagedInlineContent = std::optional<PaintInvalidation::LogicalTopAndBottom> { };
        if (CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(renderer)) {
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
        if (CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(renderer)) {
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
