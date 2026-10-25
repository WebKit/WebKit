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

#include <WebCore/LayoutUnit.h>
#include <wtf/CheckedRef.h>

namespace WebCore {

enum class RelayoutChildren : bool;

class RenderBlock;
class RenderBlockFlow;

namespace Layout {
struct BlockLayoutResult;
}

namespace LayoutIntegration {

class PaintInvalidation;

class BlockLayout {
public:
    explicit BlockLayout(RenderBlockFlow&);

    // The root's inline content that changed, between repaintLogicalTop and repaintLogicalBottom, is invalidated once the root has its
    // overflow (RenderBlockFlow::layoutBlock, repaintInlineContentAfterLayout).
    void layout(RelayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom);
    // The min-content and max-content inline sizes of the root's content (css-sizing-3 §5.1), from the block formatting context that lays it out.
    static std::pair<LayoutUnit, LayoutUnit> computeIntrinsicInlineSizes(const RenderBlockFlow&);
    // Whether a new definite block size of the root changes the layout of its content, like the one flex and grid layout give an item they stretch
    // (css-flexbox-1 §9.4 step 11). The render tree path registers percentage height descendants for that; the block formatting context does not.
    // False for a root the block formatting context does not lay out.
    static bool contentDependsOnBlockSize(const RenderBlockFlow&);
    // The out-of-flow boxes registered on the root or on a block container inside it, once their containing block has its size
    // (RenderBlockFlow::layoutBlock, RenderBlock::simplifiedLayout).
    void layoutOutOfFlowBoxes(const RenderBlock& containingBlock);

private:
    void updateRenderers(const Layout::BlockLayoutResult&, PaintInvalidation&);
    void layoutOutOfFlowBoxes(const RenderBlock& containingBlock, PaintInvalidation&);

    RenderBlockFlow& rootRenderer() const { return m_rootRenderer; }

    const CheckedRef<RenderBlockFlow> m_rootRenderer;
};

} // namespace LayoutIntegration
} // namespace WebCore
