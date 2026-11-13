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

class BlockItem;
class FloatingContext;

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
        // Its own formatting context laid it out through the render tree, including its overflow and its out-of-flow boxes.
        bool isLaidOutThroughRenderTree { false };
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
    // The in-flow block-level boxes that follow the clamp point of a line-clamp container: invisible boxes, which take no space, and which are
    // not laid out, nor is their content (css-overflow-4 §5.3.1).
    Vector<CheckedRef<const RenderBox>> invisibleBoxes;
    // The content box block size of the root from its content (CSS 2.2 §10.6.7).
    LayoutUnit intrinsicContentBlockSize;
};

// The containing block of an out-of-flow box (CSS 2.2 §10.1, css-position-3 §2.1): the padding box of a block, or the rectangle a positioned
// inline box forms, relative to the border box of the block, or of the block container whose inline content the inline box is in. The inline
// axis runs from its inline start (in its direction), the block axis from its block start.
struct OutOfFlowContainingBlock {
    WritingMode writingMode;
    bool isScrollContainer { false };
    LayoutUnit inlineStart;
    LayoutUnit blockStart;
    LayoutUnit inlineSize;
    LayoutUnit blockSize;
    std::optional<LayoutRect> scrollableContainingBlock { };
    std::optional<LayoutUnit> fillAvailableBlockSize { };
    std::optional<LayoutRect> fixedPositionOverflowArea { };
};

// A block-level box in the inline content of a block container (CSS 2.2 §9.2.1.1), which the block formatting context lays out when the
// inline formatting context reaches it.
struct BlockLevelBoxInInlineContent {
    // Relative to the border box of the block container.
    BoxGeometry geometry;
    // Where the content after the box starts, above the margins that are still open, from the top of the block container's border box.
    LayoutUnit logicalBottom;
    // The margins still open, which move the next line down unless they collapse with the block container's block-start margin.
    BlockLayoutState::MarginState marginState;
    // The lines the box and its content took off the line-clamp budget.
    size_t lineCount { 0 };
};

// Context only: what it changes as it lays out its content is in the block layout state, so the context can be built again from the root,
// the integration utils and that state.
class BlockFormattingContext {
    WTF_MAKE_TZONE_ALLOCATED(BlockFormattingContext);
public:
    BlockFormattingContext(const RenderBlockFlow& root, LayoutIntegration::BlockIntegrationUtils&, BlockLayoutState&);

    // The root's geometry is the one the formatting context it is in gave it.
    // A root the block formatting context it is in lays out gets the line grid of its content from there.
    BlockLayoutResult layout(const ConstraintsForBlockContent&, const BoxGeometry& rootGeometry, const RootLineGrid&);
    // A block-level box in the inline content of the innermost block container being laid out, on the line the inline formatting context
    // reached it on, below lineCount lines with content, of which lineCountForLineClamp count towards the line-clamp budget with the lines of
    // the block-level boxes before it. It and its content join the result. The floats the inline formatting context placed so far, in the
    // coordinates of the block container's border box, are the ones the box and the lines after it avoid.
    BlockLevelBoxInInlineContent layoutBlockLevelBoxInInlineContent(const RenderBox&, LayoutUnit lineLogicalTop, size_t lineCount, size_t lineCountForLineClamp, PlacedFloats& inlineContentFloats, BlockLayoutResult&);
    // The min-content and max-content inline sizes of the content of a block container (css-sizing-3 §5.1), but no min-content size for a
    // marquee that scrolls its content horizontally (RenderBlockFlow::computeIntrinsicLogicalWidths). The percentage block sizes in it resolve
    // against its definite content box block size.
    IntrinsicWidthConstraints computeIntrinsicInlineSizes(const RenderBlockFlow& blockContainer, std::optional<LayoutUnit> contentBoxBlockSize) const;
    // An out-of-flow box and its content, in its containing block that has its size (css-position-3 §4), and against its default anchor
    // (css-anchor-position-1 §3). The box establishes an independent formatting context for its content.
    BlockLayoutResult layoutOutOfFlowBox(const RenderBox&, const OutOfFlowContainingBlock&, const OutOfFlowGeometry::StaticPosition&, const std::optional<OutOfFlowGeometry::DefaultAnchor>&);

private:
    IntrinsicWidthConstraints computeContentIntrinsicInlineSizes(const RenderBlockFlow&, std::optional<LayoutUnit> contentBoxBlockSize) const;
    void layoutBlockItems(const Vector<BlockItem>&, BlockLayoutResult&);
    bool isAfterClampPoint() const;
    TextBoxTrimState textBoxTrimStateFor(const RenderBlockFlow&, const TextBoxTrimState& ancestorState) const;
    void startTextBoxTrim(const RenderBlockFlow&, ContainingBlockState&);
    void endTextBoxTrim(const RenderBlockFlow&, ContainingBlockState&);
    void placeChildrenAtTrimmedBlockEnd(BlockEndChildren&&, BlockLayoutResult&);
    void handleBlockLevelBoxEnd(const RenderBox&);
    void handleBlockContainerStart(const RenderBox&);
    void handleEmptyEditableContent(const RenderBox& blockContainer, ContainingBlockState&);
    void handleBlockContainerEnd(const RenderBox&, BlockLayoutResult&);
    static bool startsLineClamp(const RenderBox& blockContainer);
    bool isLaidOutWithoutLineClamp(const RenderBox& blockContainer) const;
    void startLineClamp(const RenderBox& blockContainer);
    void endLineClamp(const RenderBox& blockContainer);
    void handleInlineContent(const RenderBox& blockContainer, BlockLayoutResult&);
    void handleIndependentFormattingContextRoot(const RenderBox&, BlockLayoutResult&);
    LayoutUnit layoutIndependentFormattingContextThroughRenderTree(const RenderBox&, ContainingBlockState& boxState, std::optional<LayoutUnit> borderBoxInlineSize = { }, std::optional<LayoutSize> placedBorderBoxLogicalLeftAndTopInRoot = { });
    BlockLayoutResult layoutOutOfFlowIndependentFormattingContext(const RenderBox&, const OutOfFlowContainingBlock&, const OutOfFlowGeometry::StaticPosition&, const std::optional<OutOfFlowGeometry::DefaultAnchor>&);
    void handleFloat(const RenderBox&, BlockLayoutResult&);
    template<typename LayoutContent> std::optional<LayoutUnit> placeBesideFloats(const RenderBox&, ContainingBlockState& boxState, LayoutUnit marginsBeforeLogicalBottom, const LayoutContent&);
    void handleOutOfFlowBox(const RenderBox&, BlockLayoutResult&);

    ContainingBlockState containingBlockStateForRoot(const ConstraintsForBlockContent&, const BoxGeometry& rootGeometry) const;
    ContainingBlockState containingBlockStateFor(const RenderBox& blockContainer, const BoxGeometry&, const ContainingBlockState& parentState) const;
    std::optional<ContainingBlockState::LineGrid> lineGridFor(const RenderBox& blockContainer, const ContainingBlockState& parentState) const;
    std::optional<BlockLayoutState::LineGrid> lineGridForInlineContent(const ContainingBlockState&);
    std::optional<ContainingBlockState::LineGrid> lineGridForNestedRoot(const ContainingBlockState& rootState);
    RootLineGrid rootLineGridForNestedRoot(const ContainingBlockState& rootState);
    std::pair<LayoutSize, LayoutUnit> lineGridBorderBoxOffsetAndBorderAndPaddingBefore(const ContainingBlockState::LineGrid&);
    LayoutSize logicalLeftAndTopInRoot(LayoutUnit inlineStartInRoot, LayoutUnit borderBoxInlineSize, LayoutUnit logicalTopInRoot) const;
    std::optional<LineGridInLayoutState> lineGridInLayoutState(const ContainingBlockState&, LayoutSize borderBoxOffset);
    std::optional<LineGridInLayoutState> lineGridForBoxLaidOutThroughRenderTree(const RenderBox&, const ContainingBlockState&, LayoutSize borderBoxOffset);
    LayoutSize borderBoxLogicalLeftAndTopInRoot(const ContainingBlockState&);

    BoxGeometry computeBorderAndPadding(const RenderBox&, LayoutUnit containingBlockInlineSize) const;
    LayoutUnit scrollbarInlineStartSpace(const RenderBox&, const BoxGeometry&) const;
    void computeInlineSizeAndMargins(const RenderBox&, const ContainingBlockState&, BoxGeometry&, std::optional<LayoutUnit> availableInlineSize = { }) const;
    void computeLogicalLeft(const RenderBox&, const ContainingBlockState&, BoxGeometry&) const;
    ConstraintsForInlineContent constraintsForInlineContent(const RenderBox& blockContainer, const ContainingBlockState& blockContainerState) const;
    ContainingBlockState containingBlockStateAtBlockStart(const RenderBox&) const;
    ContainingBlockState computeContainingBlockStateAtBlockStart(const RenderBox&);
    std::optional<LayoutUnit> stretchFitBorderBoxBlockSize(const RenderBox&, const BoxGeometry&, const ContainingBlockState&) const;
    LayoutUnit orthogonalFlowAvailableInlineSpace(const ContainingBlockState&) const;
    void computeBlockSize(const RenderBox&, LayoutUnit intrinsicContentBlockSize, const ContainingBlockState&, BoxGeometry&) const;
    LayoutUnit viewportStretchedBorderBoxBlockSize(LayoutUnit collapsedMarginBefore, LayoutUnit collapsedMarginAfter, const ContainingBlockState&) const;
    void placeInContainingBlock(const RenderBox&, ContainingBlockState&& blockContainerState, BlockLayoutResult&);
    BlockLayoutResult layoutContent(const ConstraintsForBlockContent&, const BoxGeometry& rootGeometry);
    struct BorderBoxLogicalTops {
        Vector<LayoutUnit> borderBoxLogicalTops;
        Vector<bool> hasClearance;
    };
    void resolveBorderBoxLogicalTops(ContainingBlockState& blockContainerState);
    void placePendingFloats(LayoutUnit borderBoxLogicalTop, const RenderBox* blockContainer = nullptr);
    FloatingContext floatingContextForSidesOf(const RenderBox&, const PlacedFloats&) const;
    std::optional<ContainingBlockState::Clearance> clearanceAtBlockStart(const RenderBox&) const;
    void placeCollapsedThroughBoxWithClearance(const RenderBox&, ContainingBlockState&&, std::span<const ContainingBlockState* const> statesWaitingForTop, const BorderBoxLogicalTops&, BlockLayoutResult&);
    LayoutSize borderBoxOffsetInRoot(const ContainingBlockState&);
    void addFloats(PlacedFloats::List&&, const ContainingBlockState&);
    void addInlineContentFloats(const PlacedFloats::List&);
    BorderBoxLogicalTops borderBoxLogicalTopsWaitingForTop(std::span<const ContainingBlockState* const> statesWaitingForTop, LayoutUnit marginsLogicalBottom) const;
    Vector<const ContainingBlockState*> statesWaitingForTop() const;
    void placePendingFloats(std::span<const ContainingBlockState* const> statesWaitingForTop, const BorderBoxLogicalTops&);
    struct HypotheticalPosition {
        LayoutUnit borderBoxLogicalTop;
        Vector<std::pair<CheckedRef<const RenderBox>, LayoutUnit>> blockContainerLogicalTops;
    };
    HypotheticalPosition hypotheticalPosition() const;
    PlacedFloats floatsAtHypotheticalPosition() const;

    const RenderBlockFlow& root() const { return m_root; }
    LayoutIntegration::BlockIntegrationUtils& integrationUtils() const { return m_integrationUtils; }

    const CheckedRef<const RenderBlockFlow> m_root;
    LayoutIntegration::BlockIntegrationUtils& m_integrationUtils;
    BlockLayoutState& m_blockLayoutState;
    // The tops the block containers with pending floats got, which the content is laid out again with when the floats would have gone
    // elsewhere beside other floats from there.
    Vector<std::pair<CheckedRef<const RenderBox>, LayoutUnit>> m_resolvedBorderBoxLogicalTops;
    bool m_needsLayoutWithResolvedBorderBoxLogicalTops { false };
    bool m_isLayingOutWithResolvedBorderBoxLogicalTops { false };
    RootLineGrid m_rootLineGrid;
    // The block containers whose content ended at the clamp point of their own line-clamp budget, laid out again without it, and how many of
    // them are around the content laid out now, whose inline content is laid out again too.
    Vector<CheckedRef<const RenderBox>> m_blockContainersLaidOutWithoutLineClamp;
    unsigned m_lineClampContainersLaidOutAgainCount { 0 };
};

WTF::TextStream& operator<<(WTF::TextStream&, const BlockLayoutResult&);

}
}
