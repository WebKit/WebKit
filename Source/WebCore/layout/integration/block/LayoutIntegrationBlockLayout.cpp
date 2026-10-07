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

#include "BlockFormattingContext.h"
#include "LayoutRepainter.h"
#include "LocalFrameViewInlines.h"
#include "LocalFrameViewLayoutContext.h"
#include "Logging.h"
#include "RenderBlockFlow.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderLayoutState.h"
#include "RenderView.h"
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
    auto blockLayoutState = Layout::BlockLayoutState { { } };
    auto result = Layout::BlockFormattingContext { rootRenderer(), blockLayoutState }.layout(constraintsForBlockContent(rootRenderer()));
    LOG_WITH_STREAM(Layout, stream << "BlockLayout for root (" << &rootRenderer() << "): " << result);

    updateRenderers(result);
    // RenderBlockFlow::layoutBlock resolves the used block size of the root from this.
    rootRenderer().setLogicalHeight(rootRenderer().borderAndPaddingLogicalHeight() + rootRenderer().scrollbarLogicalHeight() + result.intrinsicContentBlockSize);
}

void BlockLayout::updateRenderers(const Layout::BlockLayoutResult& result)
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
            blockFlow->computeInFlowOverflow(contentArea, isHorizontalWritingMode ? RenderBox::ComputeOverflowOptions::MarginsExtendContentAreaX : RenderBox::ComputeOverflowOptions::MarginsExtendContentAreaY);
        }

        // What RenderBlockFlow::layoutBlock invalidates: changes inside the box, at the place it had before it moved.
        invalidateWithLayoutDelta(locationChanges[boxIndex], [&] {
            layoutRepainters[boxIndex].repaintAfterLayout();
        });
        // What RenderBlock::layout does once the box is laid out.
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
