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
#include "FloatingContext.h"
#include "InlineFormattingConstraints.h"
#include "InlineLineTypes.h"
#include "OutOfFlowGeometry.h"
#include <WebCore/AdjoiningMargins.h>
#include <WebCore/LayoutBoxGeometry.h>
#include <wtf/CheckedRef.h>
#include <wtf/Function.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>
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

// The fragmentainer the content is laid out into, one at a time (css-break-3 §2): a page, or a column of a multi-column container.
struct FragmentainerConstraints {
    enum class Type : bool { Page, Column };
    // None when column balancing lays the content out first, before it has a column block size (RenderMultiColumnSet::prepareForLayout):
    // only forced breaks end the fragmentainer then, where its content ends.
    std::optional<LayoutUnit> blockSize;
    Type type { Type::Page };
    // Where the border box of a block formatting context root laid out by the block formatting context it is in starts in the fragmentainer,
    // and whether content before it took space there, which a break before the root's content, the root with it, needs: content at the start
    // of the fragmentainer stays, even when it does not fit (css-break-3 §4.4).
    LayoutUnit rootBlockOffset;
    bool hasContentBefore { false };
    // Only boxes in the normal flow of the fragmentation root must apply forced breaks (css-break-3 §3.1): the content of an out-of-flow box
    // does not, like in the render tree (RenderBlockFlow::applyBeforeBreak).
    bool appliesForcedBreaks { true };
    // A root the render tree lays out in the fragmentainers ends with the fragmentainer after a forced break-after on its last in-flow box,
    // which the content after the root starts after (RenderBlockFlow::applyAfterBreak), unlike the root element or a multi-column flow, which
    // get no empty fragmentainer for it.
    bool appliesForcedBreakAfterContent { false };
    // Where the fragmentainer starts in the strip of the root whose content breaks into fragmentainers (LayoutIntegration::BlockLayout::layout),
    // which the boxes the render tree lays out and breaks the content of go into the pages from.
    LayoutUnit stripLogicalTop;
    // The content of a root the render tree lays out stays where it starts in the fragmentainer even when none of it fits there, where a
    // pagination strut moves the root instead (RenderBlockFlow::adjustLinePositionForPagination leaves its first line in place).
    bool keepsContentAtStart { false };
    // The columns of a paginated view take the break values of pages, like in the render tree (RenderMultiColumnFlow::shouldCheckColumnBreaks).
    bool breaksLikePages { false };

    // The page or column that break values name for the fragmentainer (css-break-3 §3.1).
    Type breakType() const { return breaksLikePages ? Type::Page : type; }
    // The end of the fragmentainer, from the root's border box top.
    LayoutUnit availableBlockSize() const { return blockSize ? *blockSize - rootBlockOffset : LayoutUnit::max(); }
    // Content taller than the fragmentainer overflows it wherever it goes (css-break-3 §4.4).
    LayoutUnit wholeBlockSize() const { return blockSize.value_or(LayoutUnit::max()); }
};

// Where the walk of a fragmentainer stopped, which the walk of the next fragmentainer resumes from (css-break-3 §4): the block item to resume
// at, and the block containers open there below the root, outermost first, with the part of them the fragmentainers before took.
struct BlockBreakToken {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(BlockBreakToken);
    struct OpenBlockContainer {
        CheckedRef<const RenderBox> blockContainer;
        ContainingBlockState::ConsumedBlockSize consumedBlockSize;
    };
    size_t blockItemIndex { 0 };
    Vector<OpenBlockContainer> openBlockContainers;
    ContainingBlockState::ConsumedBlockSize rootConsumedBlockSize;
    // The margins after a forced break stay, and those adjoining an unforced break are truncated (css-break-3 §5.2).
    bool isForced { false };
    // Where the content continues in the next fragmentainer: below what of the content of this one overflows into it, like that of a flex
    // container its own formatting context breaks, but not monolithic content, which takes no room there (css-multicol-1 §8.1).
    LayoutUnit blockOffset;
    // Whether text-box-trim still trims the start of the first formatted line at the break, which the block containers resumed after it keep
    // only then (css-inline-3 §6.2).
    bool trimsFirstFormattedLine { false };
    // The line the inline content of the block item continues from, when the break is between its lines.
    std::optional<InlineBreakToken> inlineBreakToken;
    // The floats that did not fit in the fragmentainer, which go to the start of the next one in their order, and those taller than it, which
    // overflow into the next one, in its coordinates: floats break apart from the content beside them (css-break-3 §2.1).
    struct PushedFloat {
        CheckedRef<const RenderBox> box;
        PlacedFloats::Item floatItem;
        // A float whose content the block formatting context lays out (BlockFormattingContext::handleFloat), which it lays out again where
        // the float goes: the state of its containing block and its own, and whether its content breaks.
        struct Content {
            ContainingBlockState containingBlockState;
            ContainingBlockState floatState;
            bool breaks { false };
            // The line grid its content snaps to, from where the float went first, in the strip of fragmentainers.
            RootLineGrid lineGrid { };
            LayoutUnit lineGridBorderBoxStripLogicalTop { };
        };
        std::optional<Content> content;
        // Its containing block, which it goes beside the floats in again at the start of the next fragmentainer.
        FloatingContext::ContainingBlock containingBlock;
        // A float whose content the render tree breaks from where it goes (BlockFormattingContext::handleFloat), which it lays out again there:
        // the state of its containing block and its own.
        struct RenderTreeLayout {
            ContainingBlockState containingBlockState;
            ContainingBlockState floatState;
            // The line grids around it where it went first, which stay where they are when it goes to the next fragmentainer.
            std::optional<LineGridInLayoutState> lineGrid { };
        };
        std::optional<RenderTreeLayout> renderTreeLayout;
        // The block containers around it open at the break, outermost first, whose fragments in the next fragmentainer it goes in, below the
        // block-start border and padding of those that clone theirs (css-break-3 §5.4).
        Vector<OpenBlockContainer> openBlockContainers { };
    };
    Vector<PushedFloat> pushedFloats;
    PlacedFloats::List overflowingFloats;
    // The floats whose content the block formatting context lays out that continue in the next fragmentainer, which they break across like
    // block containers (css-break-3 §2.1), before the content there: the block containers around them open at the break, outermost first,
    // whose fragments there they continue in, below the block-start border and padding of those that clone theirs (§5.4), their own state, with
    // what the fragmentainers took of them, their geometry in their containing block and their float in the root, whose inline position they
    // keep, and where their content continues, unless only the rest of their block size does.
    struct ContinuingFloat {
        CheckedRef<const RenderBox> box;
        Vector<OpenBlockContainer> openBlockContainers;
        PushedFloat::Content content;
        BoxGeometry geometry;
        PlacedFloats::Item floatItem;
        FloatingContext::ContainingBlock containingBlock;
        std::unique_ptr<BlockBreakToken> contentBreakToken;
        bool hasContentLeft { true };
        // What of the block size its laid out content gave it the fragmentainers have not taken yet, when only that is left.
        LayoutUnit contentBlockSizeLeft;
        // Where its margin box ends in the strip of fragmentainers, which the content beside it and after it avoids and clears.
        LayoutUnit marginBoxStripLogicalBottom;
        // One below the floats before it that starts past the end of the fragmentainer, as far as this, starts in the next one, beside or
        // below what continues of them there, like in the strip of fragmentainers.
        std::optional<LayoutUnit> offsetPastFragmentainerEnd;
    };
    Vector<ContinuingFloat> continuingFloats;
    // The placeholder of the column spanner the multi-column line ended at, after which the content continues in the next multi-column line.
    CheckedPtr<const RenderBox> columnSpannerPlaceholder;
    // The placeholder of the column spanner the last multi-column line before this one ended at.
    CheckedPtr<const RenderBox> lastColumnSpannerPlaceholder;
    // The block formatting context root at the block item, when the break is inside it: the part of it the fragmentainers before took, the
    // geometry of its fragment in the one before, whose inline position and size it keeps, and where its content continues, unless only the
    // rest of its block size does, below what monolithic content in it overflows into the next fragmentainer.
    struct OpenBlockFormattingContextRoot {
        ContainingBlockState::ConsumedBlockSize consumedBlockSize;
        BoxGeometry geometry;
        std::unique_ptr<BlockBreakToken> contentBreakToken;
        LayoutUnit overflowingContentBlockSize;
    };
    std::optional<OpenBlockFormattingContextRoot> openBlockFormattingContextRoot;
    // The content overflowing a block container with a block size of its own is a flow parallel to the content after the box, which breaks
    // apart from it (css-break-3 §2.1): where each of those flows continues, which the next fragmentainer lays out from its start before the
    // content after them, and, in one of those, the block container it is the content of, which it ends at.
    Vector<UniqueRef<BlockBreakToken>> parallelFlows;
    CheckedPtr<const RenderBox> parallelFlowBlockContainer;
    // The block-level box in inline content the block container is in, whose own block items the flow continues in.
    CheckedPtr<const RenderBox> parallelFlowBlockLevelBox;
    // The root's content ended in this fragmentainer: the next ones only have those flows.
    bool hasOnlyParallelFlows { false };
    // Where the walk of the block-level box in the inline content at the break continues, when the break is inside that box
    // (InlineBreakToken::continuesBlockContent).
    std::unique_ptr<BlockBreakToken> blockLevelBoxBreakToken;
    // The margins after the content that overflows into the next fragmentainer, below which the content continues there (blockOffset), and
    // which the margins of the content after it collapse with.
    AdjoiningMargins marginsAfterOverflow;
    // At a column spanner, the flows parallel to the content after them that go on past the end of the multi-column line, in more columns of
    // its column set, before the multi-column line after the spanner.
    std::unique_ptr<BlockBreakToken> parallelFlowsPastLastColumn;
    // The extra space of the block step of the box at the block item, which the fragmentainer before rounded its margin box with
    // (BlockFormattingContext::handleIndependentFormattingContextRoot).
    std::optional<LayoutUnit> blockStepExtraSpace;
};

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
    // The line grids of the content of the containing blocks of out-of-flow boxes in the content, from their border boxes, which the out-of-flow
    // boxes, laid out once the content is (BlockLayout::layoutOutOfFlowBoxes), snap their content to.
    Vector<std::pair<CheckedRef<const RenderBox>, LineGridInLayoutState>> outOfFlowContainingBlockLineGrids;
    // The boxes broken across fragmentainers whose fragments are not slices of the rect the strip of fragmentainers has for them
    // (LayoutIntegration::BlockLayout's mergeFragments): those with box-decoration-break: clone, each fragment of which has the box's
    // decorations (css-break-3 §5.4), and those whose fragments have gaps between them, in one that clones its decorations, which are slices
    // of the unbroken box.
    struct BoxFragment {
        // From the block-start edge of the box's border box.
        LayoutUnit blockOffset;
        LayoutUnit blockSize;
        // Where in the strip of fragmentainers its block-start edge is, in the fragmentainer it was laid out in, which tells a column the
        // fragments it paints.
        LayoutUnit fragmentainerPosition;
    };
    struct FragmentedBox {
        CheckedRef<const RenderBox> renderer;
        bool clonesDecorations { false };
        Vector<BoxFragment> fragments;
    };
    Vector<FragmentedBox> fragmentedBoxes;
    // The monolithic boxes that the fragmentainer they start at the start of is not tall enough for, which overflow its block-end edge, and the
    // block containers that end with them, which a column paints whole, past its block-end edge, not sliced across the columns after it
    // (css-multicol-1 §8.1).
    Vector<CheckedRef<const RenderBox>> boxesPastFragmentainerEnd;
    // Those of the fragments of a multi-column flow, with where in the strip of columns their column is (LayoutIntegration::BlockLayout's
    // mergeFragments).
    Vector<std::pair<CheckedRef<const RenderBox>, LayoutUnit>> boxesPastColumnEnds;
    // Where the content continues in the next fragmentainer, when it did not all fit in this one (css-break-3 §4).
    std::optional<BlockBreakToken> breakToken;
    // None of the content fit below the content before the root in the fragmentainer (FragmentainerConstraints::hasContentBefore): it all
    // goes to the next one, and the root with it.
    bool isBreakBeforeContent { false };
    // The block containers whose inline content continues from the fragmentainer before, and the first of the lines they took in this one,
    // which follow those of the fragmentainers before.
    struct InlineContentFragment {
        CheckedRef<const RenderBlockFlow> blockContainer;
        size_t firstLineIndex { 0 };
    };
    Vector<InlineContentFragment> inlineContentFragments;
    // The block size of the fragmentainer, which one without a block size of its own takes from where its content ends at a forced break.
    LayoutUnit fragmentainerBlockSize;
    // What column balancing picks the next column block size from (RenderMultiColumnSet::calculateBalancedHeight): the least block space
    // past the end of the fragmentainer that a line or a box that cannot break that did not fit, or a box that crossed it, needed, and the
    // block size of the tallest content that cannot break.
    std::optional<LayoutUnit> spaceShortage;
    LayoutUnit unbreakableBlockSize;
    // The root's content ended in a fragmentainer before: this one only has content that overflows block containers with a block size of
    // their own (BlockBreakToken::parallelFlows).
    bool hasOnlyParallelFlows { false };
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
    std::optional<LayoutUnit> availableBlockSizeIncludingMarginBorderPadding { };
    std::optional<LayoutRect> fixedPositionOverflowArea { };
    // In content broken into fragmentainers: where the border box of the containing block starts in the strip of fragmentainers.
    std::optional<LayoutUnit> borderBoxStripLogicalTop { };
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
    // In fragmented content: the box goes to the next fragmentainer with its line, without its layout in this one, or it breaks inside at the
    // end of this one, where its line ends, or the fragmentainer ends after its line, at a column spanner it ends with.
    bool breaksBefore { false };
    bool breaksInside { false };
    bool breaksAfter { false };
};

// Context only: what it changes as it lays out its content is in the block layout state, so the context can be built again from the root,
// the integration utils and that state.
class BlockFormattingContext {
    WTF_MAKE_TZONE_ALLOCATED(BlockFormattingContext);
public:
    BlockFormattingContext(const RenderBlockFlow& root, LayoutIntegration::BlockIntegrationUtils&, BlockLayoutState&);

    // The root's geometry is the one the formatting context it is in gave it.
    // A root the block formatting context it is in lays out gets the line grid of its content from there.
    // Into a fragmentainer, from where the walk of the one before stopped, the result says where to continue (css-break-3 §4).
    BlockLayoutResult layout(const ConstraintsForBlockContent&, const BoxGeometry& rootGeometry, const RootLineGrid&, std::optional<FragmentainerConstraints> = { }, const BlockBreakToken* = nullptr);
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
    // In fragmented content, the content of a block container fragments across the fragmentainers its containing block is in (css-break-3
    // §5.5): fragmentedContentLayout lays it out one fragmentainer at a time from where the border box starts in the containing block, from
    // the block layout state its context starts from, and merges the fragments.
    // A float in the inline content of a block container being laid out, whose content the block formatting context lays out, in fragmented
    // content: its geometry before the inline formatting context places it, without the block size its content gives it, and then the layout
    // of its content from where it went, in the coordinates of the block container's border box, which gives it its block size, and tells
    // whether none of its content fits there. None for a float the render tree lays out, or one that goes to the next fragmentainer like
    // content that cannot be split when it does not fit (goesWholeToNextFragmentainer).
    std::optional<BoxGeometry> geometryForFloatInInlineContent(const RenderBox&) const;
    std::optional<bool> layoutContentOfFloatInInlineContent(const RenderBox&, BoxGeometry&);
    // Whether a box that establishes a line grid is around the multi-column flow a box is in, whose columns keep its rows where they are.
    static bool isAroundFragmentedFlow(const RenderBox& gridContainer, const RenderBox&);

    using FragmentedOutOfFlowContentLayout = Function<BlockLayoutResult(const RenderBlockFlow&, const BlockLayoutState&, const ConstraintsForBlockContent&, const BoxGeometry&, const RootLineGrid&, LayoutUnit borderBoxBlockStart)>;
    BlockLayoutResult layoutOutOfFlowBox(const RenderBox&, const OutOfFlowContainingBlock&, const OutOfFlowGeometry::StaticPosition&, const std::optional<OutOfFlowGeometry::DefaultAnchor>&, const std::optional<LineGridInLayoutState>& containingBlockLineGrid, const FragmentedOutOfFlowContentLayout* fragmentedContentLayout = nullptr);

private:
    IntrinsicWidthConstraints computeContentIntrinsicInlineSizes(const RenderBlockFlow&, std::optional<LayoutUnit> contentBoxBlockSize) const;
    void layoutBlockItems(const Vector<BlockItem>&, BlockLayoutResult&, size_t startIndex = 0, std::optional<size_t> endIndex = { });
    void resumeAfterBreak(const BlockBreakToken&);
    void layoutParallelFlow(const Vector<BlockItem>&, const BlockBreakToken&, BlockLayoutResult&);
    BlockBreakToken& endFragmentainer(size_t blockItemIndex, bool isForced, BlockLayoutResult&, std::optional<LayoutUnit> contentLogicalBottom = { }, std::optional<LayoutUnit> innermostContentBoxLogicalBottom = { });
    std::optional<LayoutUnit> blockSizeLeftOfItsOwn(const ContainingBlockState& blockContainerState, const ContainingBlockState& parentState) const;
    const RenderBox* lastColumnSpannerPlaceholder() const;
    bool endsInFragmentainer(const ContainingBlockState& blockContainerState, const ContainingBlockState& parentState, LayoutUnit fragmentainerEnd) const;
    struct FragmentEnd {
        LayoutUnit fragmentEnd;
        // The block-end border and padding the fragment clones at the break, which its content ends above.
        LayoutUnit clonedBlockEnd;
    };
    Vector<FragmentEnd> fragmentEndsOfOpenBlockContainers(LayoutUnit fragmentainerEnd) const;
    LayoutUnit fragmentainerEndForContent() const;
    PlacedFloats::List floatsOverflowingFragmentainer(LayoutUnit fragmentainerBlockSize) const;
    LayoutUnit placeColumnSpannerPlaceholder(const RenderBox&, BlockLayoutResult&);
    bool isAfterClampPoint() const;
    TextBoxTrimState textBoxTrimStateFor(const RenderBlockFlow&, const TextBoxTrimState& ancestorState) const;
    void startTextBoxTrim(const RenderBlockFlow&, ContainingBlockState&);
    void endBlockContainerContent(const RenderBox&, ContainingBlockState&);
    void endTextBoxTrim(const RenderBlockFlow&, ContainingBlockState&);
    void placeChildrenAtTrimmedBlockEnd(BlockEndChildren&&, BlockLayoutResult&);
    void moveFloatsInside(const RenderBox&, LayoutUnit blockOffset);
    void handleBlockLevelBoxEnd(const RenderBox&);
    void handleBlockContainerStart(const RenderBox&);
    void handleEmptyEditableContent(const RenderBox& blockContainer, ContainingBlockState&);
    void handleBlockContainerEnd(const RenderBox&, BlockLayoutResult&);
    bool startsLineClamp(const RenderBox& blockContainer) const;
    bool isLaidOutWithoutLineClamp(const RenderBox& blockContainer) const;
    void startLineClamp(const RenderBox& blockContainer);
    void endLineClamp(const RenderBox& blockContainer);
    // In a fragmentainer, where the content continues in the next one when its lines did not all fit, and whether the break is inside the
    // block container, when any did, which they may not to keep orphans and widows when a break before the block container is allowed, a
    // float it placed did, or floats took the first line below them.
    struct InlineContentBreak {
        InlineBreakToken breakToken;
        bool breaksInside { false };
        std::unique_ptr<BlockBreakToken> blockLevelBoxBreakToken;
    };
    std::optional<InlineContentBreak> handleInlineContent(const RenderBox& blockContainer, BlockLayoutResult&, bool isBreakBeforeAllowed = false);
    struct ColumnSpannerBreak;
    void endMultiColumnLineInInlineContent(const ColumnSpannerBreak&, std::optional<InlineContentBreak>&&, size_t inlineContentIndex, BlockLayoutResult&);
    bool breaksInside(std::span<const ContainingBlockState>) const;
    bool breaksContentThroughRenderTree(const RenderBox&) const;
    bool paintsMonolithicOverflow(const RenderBox&) const;
    bool pushesFloat(const PlacedFloats::Item&) const;
    bool goesWholeToNextFragmentainer(const RenderBox& floatBox, LayoutUnit marginBoxBlockSize) const;
    void addParallelFlowOfRoot(const RenderBox& root, size_t blockItemIndex, const BoxGeometry& rootGeometry, LayoutUnit rootLogicalTop, BlockBreakToken&& contentBreakToken);
    void addSpaceShortage(LayoutUnit);
    void addUnbreakableBlockSize(LayoutUnit blockSize) { m_unbreakableBlockSize = std::max(m_unbreakableBlockSize, blockSize); }
    FloatingContext::ContainingBlock containingBlockForFloats(const ContainingBlockState&);
    void pushFloatsThatDoNotFit(std::span<const CheckedRef<const Box>> floatBoxes);
    void placeFloatsFromFragmentainerBefore(BlockLayoutResult&);
    // Where the content of a block formatting context root whose content breaks stopped in the fragmentainer, and where it ends there, from the
    // root's border box top (BlockLayoutResult::breakToken, isBreakBeforeContent and fragmentainerBlockSize).
    struct RootContentBreak {
        std::optional<BlockBreakToken> breakToken;
        bool isBreakBeforeContent { false };
        LayoutUnit fragmentainerBlockSize;
    };
    std::optional<RootContentBreak> handleIndependentFormattingContextRoot(const RenderBox&, BlockLayoutResult&);
    LayoutUnit layoutIndependentFormattingContextThroughRenderTree(const RenderBox&, const ContainingBlockState& containingBlockState, ContainingBlockState& boxState, std::optional<LayoutUnit> borderBoxInlineSize = { }, std::optional<LayoutUnit> stretchFitBorderBoxInlineSize = { }, std::optional<LayoutSize> placedBorderBoxLogicalLeftAndTopInRoot = { }, const BlockBreakToken::PushedFloat::RenderTreeLayout* pushedFloat = nullptr);
    BlockLayoutResult layoutOutOfFlowIndependentFormattingContext(const RenderBox&, const OutOfFlowContainingBlock&, const OutOfFlowGeometry::StaticPosition&, const std::optional<OutOfFlowGeometry::DefaultAnchor>&, const std::optional<LineGridInLayoutState>& containingBlockLineGrid);
    std::optional<LineGridInLayoutState> lineGridForOutOfFlowBox(const RenderBox&, const OutOfFlowContainingBlock&, const std::optional<LineGridInLayoutState>& containingBlockLineGrid, LayoutUnit borderBoxInlineStart, LayoutUnit borderBoxBlockStart, LayoutUnit borderBoxInlineSize) const;
    RootLineGrid rootLineGridForOutOfFlowBox(const RenderBox&, const std::optional<LineGridInLayoutState>&) const;
    void addLineGridOfOutOfFlowContainingBlock(const RenderBox& blockContainer, const ContainingBlockState&, BlockLayoutResult&);
    void handleFloat(const RenderBox&, BlockLayoutResult&);
    bool laysOutFloatContent(const RenderBox&) const;
    std::optional<bool> layoutFloatInInlineContentThroughRenderTree(const RenderBox&, BoxGeometry&);
    void computeFloatMargins(const RenderBox&, const ContainingBlockState&, BoxGeometry&) const;
    // The layout of the content of a float in the fragmentainer (layoutFloatContent): the block-axis geometry of the float's fragment in it, and,
    // when the float continues in the next one, what the fragmentainers took of it and where its content continues.
    struct FloatContentLayout {
        BlockLayoutResult result;
        BoxGeometry fragmentGeometry;
        std::optional<ContainingBlockState::ConsumedBlockSize> consumedBlockSize;
        std::unique_ptr<BlockBreakToken> contentBreakToken;
        bool hasContentLeft { false };
        LayoutUnit contentBlockSizeLeft;
        LayoutUnit marginBoxStripLogicalBottom;
        std::optional<LayoutUnit> offsetPastFragmentainerEnd;
    };
    FloatContentLayout layoutFloatContent(const RenderBlockFlow&, BlockBreakToken::PushedFloat::Content&, LayoutUnit borderBoxLogicalTopInRoot, const BlockBreakToken::ContinuingFloat* = nullptr, bool mayStartInNextFragmentainer = true);
    void continueFloat(const RenderBox&, BlockBreakToken::PushedFloat::Content&&, FloatContentLayout&, const BoxGeometry&, const PlacedFloats::Item&, const FloatingContext::ContainingBlock&);
    void layoutContinuingFloat(const BlockBreakToken::ContinuingFloat&, BlockLayoutResult&);
    void appendFloatContentResult(BlockLayoutResult&&, BlockLayoutResult&);
    // Also those the inline formatting context laying out the content placed so far.
    bool hasFloatsContinuingInNextFragmentainer() const;
    void withOpenBlockContainers(const Vector<BlockBreakToken::OpenBlockContainer>&, NOESCAPE const Function<void()>&);
    FloatingContext::ContainingBlock containingBlockForFloatFromFragmentainerBefore(const ContainingBlockState&, const FloatingContext::ContainingBlock&);
    void computeBlockSizeAfterBreaks(const RenderBox&, const ContainingBlockState&, LayoutUnit intrinsicContentBlockSize, const ContainingBlockState&, BoxGeometry&) const;
    template<typename LayoutContent> std::optional<LayoutUnit> placeBesideFloats(const RenderBox&, ContainingBlockState& boxState, LayoutUnit marginsBeforeLogicalBottom, const LayoutContent&);
    void handleOutOfFlowBox(const RenderBox&, BlockLayoutResult&);

    ContainingBlockState containingBlockStateForRoot(const ConstraintsForBlockContent&, const BoxGeometry& rootGeometry) const;
    ContainingBlockState containingBlockStateFor(const RenderBox& blockContainer, const BoxGeometry&, const ContainingBlockState& parentState) const;
    std::optional<ContainingBlockState::LineGrid> lineGridFor(const RenderBox& blockContainer, const ContainingBlockState& parentState) const;
    std::optional<BlockLayoutState::LineGrid> lineGridForInlineContent(const ContainingBlockState&, bool continuesFromBreak);
    std::optional<ContainingBlockState::LineGrid> lineGridForNestedRoot(const ContainingBlockState& rootState, LayoutSize borderBoxOffsetInRoot);
    RootLineGrid rootLineGridForNestedRoot(const ContainingBlockState& rootState, std::optional<LayoutSize> borderBoxOffsetInRoot = { });
    void setLineGridOfFloatContent(BlockBreakToken::PushedFloat::Content&, LayoutSize borderBoxOffsetInRoot);
    std::pair<LayoutSize, LayoutUnit> lineGridBorderBoxOffsetAndBorderAndPaddingBefore(const ContainingBlockState::LineGrid&);
    LayoutSize logicalLeftAndTopInRoot(LayoutUnit inlineStartInRoot, LayoutUnit borderBoxInlineSize, LayoutUnit logicalTopInRoot) const;
    std::optional<LineGridInLayoutState> lineGridInLayoutState(const ContainingBlockState&, LayoutSize borderBoxOffset);
    LayoutSize offsetInLayoutState(LayoutSize offsetInRoot) const;
    std::pair<LayoutUnit, LayoutUnit> spaceOnLineGridColumns(const ContainingBlockState&, LayoutUnit start, LayoutUnit end);
    std::optional<LineGridInLayoutState> lineGridForBoxLaidOutThroughRenderTree(const RenderBox&, const ContainingBlockState&, LayoutSize borderBoxOffset);
    LayoutSize borderBoxLogicalLeftAndTopInRoot(const ContainingBlockState&);

    BoxGeometry computeBorderAndPadding(const RenderBox&, LayoutUnit containingBlockInlineSize) const;
    LayoutUnit scrollbarInlineStartSpace(const RenderBox&, const BoxGeometry&) const;
    void computeInlineSizeAndMargins(const RenderBox&, const ContainingBlockState&, BoxGeometry&, std::optional<LayoutUnit> availableInlineSize = { }) const;
    void computeLogicalLeft(const RenderBox&, const ContainingBlockState&, BoxGeometry&) const;
    ConstraintsForInlineContent constraintsForInlineContent(const RenderBox& blockContainer, const ContainingBlockState& blockContainerState) const;
    ContainingBlockState containingBlockStateAtBlockStart(const RenderBox&) const;
    StretchFitBorderBoxBlockSizes stretchFitBorderBoxBlockSizes(const BoxGeometry&, const ContainingBlockState&) const;
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
    PlacedFloats::Item inlineContentFloatInRoot(const PlacedFloats::Item&);
    void addInlineContentFloats(const PlacedFloats::List&);
    BorderBoxLogicalTops borderBoxLogicalTopsWaitingForTop(std::span<const ContainingBlockState* const> statesWaitingForTop, LayoutUnit marginsLogicalBottom) const;
    Vector<const ContainingBlockState*> statesWaitingForTop() const;
    void placePendingFloats(std::span<const ContainingBlockState* const> statesWaitingForTop, const BorderBoxLogicalTops&);
    struct HypotheticalPosition {
        LayoutUnit borderBoxLogicalTop;
        Vector<std::pair<CheckedRef<const RenderBox>, LayoutUnit>> blockContainerLogicalTops;
    };
    enum class EndsQuirkContainerBlockStartSides : bool { No, Yes };
    HypotheticalPosition hypotheticalPosition(EndsQuirkContainerBlockStartSides = EndsQuirkContainerBlockStartSides::No) const;
    PlacedFloats floatsAtHypotheticalPosition(const HypotheticalPosition&) const;

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
    // The fragmentainer the walk lays the content out into, and where the walk of the one before stopped.
    std::optional<FragmentainerConstraints> m_fragmentainer;
    const BlockBreakToken* m_incomingBreakToken { nullptr };
    // The line the inline content at the start of the fragmentainer continues from, until it does.
    const InlineBreakToken* m_inlineBreakTokenToResume { nullptr };
    // The block formatting context root that continues at the start of the fragmentainer, until it does.
    const BlockBreakToken::OpenBlockFormattingContextRoot* m_openBlockFormattingContextRootToResume { nullptr };
    // The extra space of the block step of the box at the start of the fragmentainer, until it is laid out, that of the block formatting
    // context root laid out last, and that of the box the walk breaks before, at its block item.
    std::optional<LayoutUnit> m_blockStepExtraSpaceToResume;
    std::optional<LayoutUnit> m_rootBlockStepExtraSpace;
    std::optional<std::pair<size_t, LayoutUnit>> m_blockStepExtraSpaceAtBreak;
    // The block-level box in the inline content that continues at the start of the fragmentainer, until it does, and, while the walk lays out
    // such a box, how many of the containing block states are those of the walk around it, and where its walk stopped at a break inside it.
    const BlockBreakToken* m_blockLevelBoxBreakTokenToResume { nullptr };
    std::optional<size_t> m_blockLevelBoxOutsideStateCount;
    std::unique_ptr<BlockBreakToken> m_blockLevelBoxBreakToken;
    // The column spanner the multi-column line ended at in the walk of a block-level box in inline content, which ends the fragmentainer
    // with the line of the box, and where the line ends in the root.
    struct ColumnSpannerBreak {
        CheckedRef<const RenderBox> placeholder;
        LayoutUnit multiColumnLineLogicalBottom;
    };
    std::optional<ColumnSpannerBreak> m_columnSpannerBreakInBlockLevelBox;
    // The inline content ends the fragmentainer after the line of a block-level box with a forced break after it.
    bool m_forcedBreakAfterBlockLevelBox { false };
    // The content in the fragmentainer ends with a monolithic box at its start that overflows its end, which takes no room in the next one
    // (css-multicol-1 §8.1), and which the block containers around it end with, overflowing it too.
    bool m_contentEndsWithMonolithicOverflow { false };
    // Something took space in the fragmentainer, which a break before the next box needs: content at its start stays, even when it does
    // not fit (css-break-3 §4.4).
    bool m_fragmentainerHasContent { false };
    // The block-start margins of the boxes at the start of the fragmentainer after an unforced break are truncated (css-break-3 §5.2).
    bool m_marginsAdjoinUnforcedBreak { false };
    // The boxes that cannot be split, which did not fit where they start in the fragmentainer, laid out again breaking inside.
    Vector<CheckedRef<const RenderBox>> m_unsplittableBoxesLaidOutWithBreaks;
    // The floats that do not fit in the fragmentainer, which go to the next one, and the containing blocks of the floats placed in it.
    Vector<BlockBreakToken::PushedFloat> m_pushedFloats;
    struct FloatInFragmentainer {
        CheckedRef<const Box> box;
        FloatingContext::ContainingBlock containingBlock;
        std::optional<BlockBreakToken::PushedFloat::RenderTreeLayout> renderTreeLayout;
    };
    Vector<FloatInFragmentainer> m_floatsInFragmentainer;
    // The floats whose content the walk laid out in the fragmentainer, and whether none of it fit in this one, below the content before it.
    struct FloatContent {
        CheckedRef<const RenderBox> box;
        BlockBreakToken::PushedFloat::Content content;
        bool isBreakBeforeContent { false };
    };
    const FloatContent* floatContent(const Box&) const;
    Vector<FloatContent> m_floatContents;
    // The layout of the content of the floats the inline formatting context places, which joins the result with the floats it placed.
    Vector<std::pair<CheckedRef<const RenderBox>, FloatContentLayout>> m_inlineContentFloatResults;
    // The floats placed in the fragmentainer whose content, or block size of their own, continues in the next one (BlockBreakToken::continuingFloats).
    Vector<BlockBreakToken::ContinuingFloat> m_continuingFloats;
    // How many of the boxes in the result are floats among block-level boxes and their content, which take no space.
    size_t m_floatBoxCount { 0 };
    // Where the flows parallel to the content after block containers with a block size of their own continue in the next fragmentainer, and
    // the block container whose content broke last, whose end the walk goes on from, with the block item it broke at.
    Vector<UniqueRef<BlockBreakToken>> m_parallelFlowBreakTokens;
    struct ParallelFlowBreak {
        CheckedRef<const RenderBox> blockContainer;
        size_t blockItemIndex { 0 };
    };
    std::optional<ParallelFlowBreak> m_parallelFlowBreak;
    // The parallel flow the walk lays out, and how many of the containing block states, the root's included, are outside it: those of the
    // block container it is the content of and of its ancestors, which ended in a fragmentainer before.
    struct ParallelFlow {
        CheckedRef<const RenderBox> blockContainer;
        size_t outsideStateCount { 0 };
        CheckedPtr<const RenderBox> blockLevelBox;
    };
    std::optional<ParallelFlow> m_parallelFlow;
    // What column balancing picks the next column block size from (BlockLayoutResult::spaceShortage and unbreakableBlockSize).
    std::optional<LayoutUnit> m_spaceShortage;
    LayoutUnit m_unbreakableBlockSize;
};

WTF::TextStream& operator<<(WTF::TextStream&, const BlockLayoutResult&);

}
}
