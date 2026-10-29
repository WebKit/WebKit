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

#include <WebCore/AdjoiningMargins.h>
#include <WebCore/LayoutBoxGeometry.h>
#include <WebCore/LayoutRect.h>
#include <WebCore/LayoutUnit.h>
#include <wtf/CheckedRef.h>
#include <wtf/WeakHashMap.h>

namespace WebCore {

class RenderBlockFlow;
class RenderBox;

namespace Layout {
class BlockFormattingContext;
class BlockLayoutState;
class BoxGeometry;
struct BlockLayoutResult;
struct ConstraintsForIndependentFormattingContext;
struct ConstraintsForInlineContent;
struct IntrinsicWidthConstraints;
}

namespace LayoutIntegration {

class LineLayout;

class PaintInvalidation {
    WTF_MAKE_NONCOPYABLE(PaintInvalidation);
public:
    PaintInvalidation() = default;

    class InlineContentScope {
        WTF_MAKE_NONCOPYABLE(InlineContentScope);
    public:
        InlineContentScope(PaintInvalidation&, RenderBlockFlow&, bool isFullLayout);
        ~InlineContentScope();

        void setDamagedRect(std::optional<LayoutRect> damagedRect) { m_damagedRect = damagedRect; }

    private:
        PaintInvalidation& m_paintInvalidation;
        CheckedRef<RenderBlockFlow> m_blockContainer;
        bool m_isFullLayout { false };
        std::pair<float, float> m_contentTopAndBottomBeforeLayout;
        std::optional<LayoutRect> m_damagedRect;
    };

    struct LogicalTopAndBottom {
        LayoutUnit logicalTop;
        LayoutUnit logicalBottom;
    };
    std::optional<LogicalTopAndBottom> damagedInlineContentLogicalTopAndBottom(RenderBlockFlow&, LayoutUnit borderBoxBlockSizeFromContent);

private:
    struct InlineContent {
        bool isFullLayout { false };
        std::optional<LayoutRect> damagedRect;
        std::pair<float, float> contentTopAndBottomBeforeLayout;
    };
    SingleThreadWeakHashMap<RenderBlockFlow, InlineContent> m_inlineContent;
};

class BlockIntegrationUtils {
public:
    explicit BlockIntegrationUtils(PaintInvalidation&);

    PaintInvalidation& paintInvalidation() const { return m_paintInvalidation; }

    struct InlineContentLayoutResult {
        // From the content box top to the bottom of the last line box (CSS 2.2 §10.6.3).
        LayoutUnit contentBlockSize;
        // Only line boxes separate margins: content whose line boxes are all phantom does not (CSS 2.2 §8.3.1, css-inline-3).
        bool hasLineBoxes { false };
        // The last line with content holds a block-level box, whose block formatting context keeps the margins after it open.
        bool endsWithBlockLevelBox { false };
        // Lines kept from their last layout with the block-level boxes in them, which keep the margins it gave them: those that collapsed with
        // the block container's block-start margin, and those still open after the box the content ends with, at its bottom border edge.
        struct MarginsOfKeptBlockLevelBoxes {
            Layout::AdjoiningMargins before;
            Layout::AdjoiningMargins after;
            bool afterAreQuirky { false };
        };
        std::optional<MarginsOfKeptBlockLevelBoxes> marginsOfKeptBlockLevelBoxes { };
    };
    // The content box block size is the percentage basis of the atomic inline-level boxes in it. Relayout children lays the lines out from
    // scratch, like when the inline size, border or padding of the block container changed. The block formatting context lays out the
    // block-level boxes in the content when the inline formatting context reaches them, and adds them to the result it is building.
    InlineContentLayoutResult layoutInlineContent(const RenderBlockFlow&, const Layout::ConstraintsForInlineContent&, const Layout::BoxGeometry&, std::optional<LayoutUnit> contentBoxBlockSize, bool relayoutChildren, const Layout::BlockLayoutState&, Layout::BlockFormattingContext&, Layout::BlockLayoutResult&);
    // The geometry its parent formatting context gave a block formatting context root, which its renderer has before its content is laid out.
    static Layout::BoxGeometry rootGeometry(const RenderBlockFlow&);
    // The min-content and max-content inline sizes of the inline content of a block container (css-sizing-3 §5.1), from the inline formatting
    // context's own layout (LineLayout::computeIntrinsicWidthConstraints).
    static Layout::IntrinsicWidthConstraints inlineContentIntrinsicInlineSizes(const RenderBlockFlow&);

    struct IndependentFormattingContextLayoutResult {
        LayoutUnit borderBoxInlineSize;
        LayoutUnit borderBoxBlockSize;
        // In the writing mode of the containing block, the block formatting context's.
        Layout::BoxGeometry::Edges border;
        Layout::BoxGeometry::Edges padding;
    };
    // An independent formatting context root the block formatting context does not lay out itself, such as a table or a flex container,
    // laid out by its own formatting context through the render tree (RenderBox::layoutIfNeeded). Its position and margins are the block
    // formatting context's, which BlockLayout::updateRenderers writes.
    IndependentFormattingContextLayoutResult layoutIndependentFormattingContext(const RenderBox&, const Layout::ConstraintsForIndependentFormattingContext&);
    // An out-of-flow replaced element or flex container, laid out by its own formatting context through the render tree once its containing
    // block has its size, at the border box size the block formatting context gives it: none for a replaced element, which sizes itself, and
    // no block size when it comes from the content. The border box size it gets; its position and margins are the block formatting context's.
    LayoutSize layoutOutOfFlowIndependentFormattingContext(const RenderBox&, std::optional<LayoutUnit> borderBoxInlineSize = { }, std::optional<LayoutUnit> borderBoxBlockSize = { });
    // The min-content and max-content inline sizes of the content box of a box whose own formatting context measures it through the render
    // tree (RenderBox::minContentLogicalWidthContribution).
    static Layout::IntrinsicWidthConstraints independentFormattingContextIntrinsicInlineSizes(const RenderBox&, LayoutUnit borderAndPadding);
    // The min-content and max-content contributions of the border box of such a box to the inline size of its containing block, its size
    // properties included.
    static Layout::IntrinsicWidthConstraints independentFormattingContextIntrinsicInlineSizeContributions(const RenderBox&);
    // Whether the content box inline size, border or padding of a box differ from its last layout, which its renderer still has
    // (RenderBlock::recomputeLogicalWidth).
    static bool contentBoxInlineSizeChanged(const RenderBox&, LayoutUnit contentBoxInlineSize);
    // Like RenderBlockFlow::layoutBlock: the boxes laid out through the render tree that resolve a percentage block size against the block
    // container are laid out again (RenderBlockFlow::dirtyForLayoutFromPercentageHeightDescendants).
    static void markPercentageHeightDescendantsForLayout(const RenderBlockFlow&);
    // A table, a flex or grid container, or a block formatting context root whose content the block formatting context cannot lay out (canUseForNestedBlockLayout).
    static bool laysOutThroughRenderTree(const RenderBox&);

private:
    static std::optional<InlineContentLayoutResult> keptInlineContentWithBlockLevelBoxesLayoutResult(const RenderBlockFlow&, const LineLayout&);
    PaintInvalidation& m_paintInvalidation;
};

} // namespace LayoutIntegration
} // namespace WebCore
