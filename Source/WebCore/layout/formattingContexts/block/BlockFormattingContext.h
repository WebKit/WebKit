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

#pragma once

#include "BlockFormattingConstraints.h"
#include "BlockFormattingUtils.h"
#include "BlockLayoutState.h"
#include "InlineFormattingConstraints.h"
#include "OutOfFlowGeometry.h"
#include <WebCore/AdjoiningMargins.h>
#include <WebCore/LayoutBoxGeometry.h>
#include <wtf/CheckedRef.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {

class RenderBlockFlow;
class RenderBox;

namespace LayoutIntegration {
class BlockIntegrationUtils;
}

namespace Layout {

struct BlockLayoutResult {
    struct Box {
        CheckedRef<const RenderBox> renderer;
        // In the logical coordinates of the containing block, relative to its border box: the inline axis runs from its inline start
        // (in its direction), the block axis from its block start. Margins follow the containing block's direction, border and padding the box's.
        BoxGeometry geometry;
        // The content box block size the box would have with an auto height (CSS 2.2 §10.6.3).
        LayoutUnit intrinsicContentBlockSize;
        // The margins that collapse with each block-axis edge of the box, its own included, and whether they are quirky: a parent laid out
        // by the render tree path reads them from a box it does not lay out again (RenderBlockFlow::maxPositiveMarginBefore and friends).
        AdjoiningMargins collapsedMarginsBefore;
        AdjoiningMargins collapsedMarginsAfter;
        bool marginBeforeIsQuirky { false };
        bool marginAfterIsQuirky { false };
    };
    // Post-order: a box is added once its content is laid out, after its descendants.
    Vector<Box> boxes;
    // An absolutely or fixed positioned box in the content, which BlockLayout::updateRenderers registers on its containing block
    // (RenderBlock::addOutOfFlowBox), and which its containing block lays out once it has its size.
    struct OutOfFlowBox {
        CheckedRef<const RenderBox> renderer;
        // Where the margin box of the box would start with position: static (CSS 2.2 §10.3.7, §10.6.4), relative to the border box of the
        // block container it would be in: the inline axis runs from its inline start (in its direction), the block axis from its block start.
        LayoutUnit staticInlinePosition;
        LayoutUnit staticBlockPosition;
    };
    // In tree order.
    Vector<OutOfFlowBox> outOfFlowBoxes;
    // The content box block size of the root from its content (CSS 2.2 §10.6.7).
    LayoutUnit intrinsicContentBlockSize;
};

// The padding box of the containing block of an out-of-flow box (CSS 2.2 §10.1), relative to its border box: the inline axis runs from its
// inline start (in its direction), the block axis from its block start.
struct OutOfFlowContainingBlock {
    CheckedRef<const RenderBox> box;
    LayoutUnit paddingBoxInlineStart;
    LayoutUnit paddingBoxBlockStart;
    LayoutUnit inlineSize;
    LayoutUnit blockSize;
};

// Context only: what it changes as it lays out its content is in the block layout state, so the context can be built again from the root,
// the integration utils and that state.
class BlockFormattingContext {
    WTF_MAKE_TZONE_ALLOCATED(BlockFormattingContext);
public:
    BlockFormattingContext(const RenderBlockFlow& root, LayoutIntegration::BlockIntegrationUtils&, BlockLayoutState&);

    BlockLayoutResult layout(const ConstraintsForBlockContent&);
    // The min-content and max-content inline sizes of the content of a block container (css-sizing-3 §5.1).
    IntrinsicWidthConstraints computeIntrinsicInlineSizes(const RenderBlockFlow& blockContainer) const;
    // An out-of-flow box and its content, in its containing block that has its size (css-position-3 §4). The box establishes an independent
    // formatting context for its content.
    BlockLayoutResult layoutOutOfFlowBox(const RenderBox&, const OutOfFlowContainingBlock&, const OutOfFlowGeometry::StaticPosition&);

private:
    void handleBlockContainerStart(const RenderBox&);
    void handleBlockContainerEnd(const RenderBox&, BlockLayoutResult&);
    void handleInlineContent(const RenderBox& blockContainer, ContainingBlockState& blockContainerState, const BlockLayoutState& inlineContentBlockLayoutState);
    void handleIndependentFormattingContextRoot(const RenderBox&, BlockLayoutResult&);
    void handleOutOfFlowBox(const RenderBox&, BlockLayoutResult&);

    ContainingBlockState containingBlockStateForRoot(const ConstraintsForBlockContent&) const;
    ContainingBlockState containingBlockStateFor(const RenderBox& blockContainer, const BoxGeometry&, const ContainingBlockState& parentState) const;

    BoxGeometry computeBorderAndPadding(const RenderBox&, LayoutUnit containingBlockInlineSize) const;
    void computeInlineSizeAndMargins(const RenderBox&, const ContainingBlockState&, BoxGeometry&) const;
    void computeLogicalLeft(const ContainingBlockState&, BoxGeometry&) const;
    ConstraintsForInlineContent constraintsForInlineContent(const RenderBox& blockContainer, const ContainingBlockState& blockContainerState) const;
    ContainingBlockState computeContainingBlockStateAtBlockStart(const RenderBox&);
    void computeBlockSize(const RenderBox&, LayoutUnit intrinsicContentBlockSize, const ContainingBlockState&, BoxGeometry&) const;
    LayoutUnit viewportStretchedBorderBoxBlockSize(LayoutUnit collapsedMarginBefore, LayoutUnit collapsedMarginAfter) const;
    void placeInContainingBlock(const RenderBox&, ContainingBlockState&& blockContainerState, BlockLayoutResult&);
    void resolveBorderBoxLogicalTops(ContainingBlockState& blockContainerState);

    const RenderBlockFlow& root() const { return m_root; }
    LayoutIntegration::BlockIntegrationUtils& integrationUtils() const { return m_integrationUtils; }

    const CheckedRef<const RenderBlockFlow> m_root;
    LayoutIntegration::BlockIntegrationUtils& m_integrationUtils;
    BlockLayoutState& m_blockLayoutState;
};

WTF::TextStream& operator<<(WTF::TextStream&, const BlockLayoutResult&);

}
}
