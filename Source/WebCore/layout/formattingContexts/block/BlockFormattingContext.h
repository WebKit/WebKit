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
#include "BlockLayoutState.h"
#include <WebCore/AdjoiningMargins.h>
#include <WebCore/LayoutBoxGeometry.h>
#include <wtf/CheckedRef.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {

class RenderBlockFlow;
class RenderBox;

namespace Layout {

struct BlockLayoutResult {
    struct Box {
        CheckedRef<const RenderBox> renderer;
        // In the logical coordinates of the containing block, relative to its border box: the inline axis runs from its inline start
        // (in its direction), the block axis from its block start. Margins follow the containing block's direction, border and padding the box's.
        BoxGeometry geometry;
        // The content box block size the box would have with an auto height (CSS 2.2 §10.6.3).
        LayoutUnit intrinsicContentBlockSize;
        // The margins that collapse with each block-axis edge of the box, its own included: a parent laid out by the render tree path
        // reads them from a box it does not lay out again (RenderBlockFlow::maxPositiveMarginBefore and friends).
        AdjoiningMargins collapsedMarginsBefore;
        AdjoiningMargins collapsedMarginsAfter;
    };
    // Post-order: a box is added once its content is laid out, after its descendants.
    Vector<Box> boxes;
    LayoutUnit intrinsicContentBlockSize;
};

// Context only: what it changes as it lays out its content is in the block layout state, so the context can be built again from the root
// and that state.
class BlockFormattingContext {
    WTF_MAKE_TZONE_ALLOCATED(BlockFormattingContext);
public:
    BlockFormattingContext(const RenderBlockFlow& root, BlockLayoutState&);

    BlockLayoutResult layout(const ConstraintsForBlockContent&);

private:
    void handleBlockContainerStart(const RenderBox&);
    void handleBlockContainerEnd(const RenderBox&, BlockLayoutResult&);

    ContainingBlockState containingBlockStateForRoot(const ConstraintsForBlockContent&) const;
    ContainingBlockState containingBlockStateFor(const RenderBox& blockContainer, const BoxGeometry&, const ContainingBlockState& parentState) const;

    BoxGeometry computeBorderAndPadding(const RenderBox&, const ContainingBlockState&) const;
    void computeInlineSizeAndMargins(const RenderBox&, const ContainingBlockState&, BoxGeometry&) const;
    void computeLogicalLeft(const ContainingBlockState&, BoxGeometry&) const;
    void computeBlockSize(const RenderBox&, LayoutUnit intrinsicContentBlockSize, const ContainingBlockState&, BoxGeometry&) const;
    void placeInContainingBlock(const RenderBox&, ContainingBlockState&& blockContainerState, BlockLayoutResult&);
    void resolveBorderBoxLogicalTops(ContainingBlockState& blockContainerState);

    const RenderBlockFlow& root() const { return m_root; }

    const CheckedRef<const RenderBlockFlow> m_root;
    BlockLayoutState& m_blockLayoutState;
};

WTF::TextStream& operator<<(WTF::TextStream&, const BlockLayoutResult&);

}
}
