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
#include "AnchorPositionEvaluator.h"
#include "BlockFormattingContext.h"
#include "BlockFormattingUtils.h"
#include "BlockIntegrationUtils.h"
#include "FlexFormattingUtils.h"
#include "FloatingObjects.h"
#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorInlineBox.h"
#include "LayoutIntegrationBlockCoverage.h"
#include "LayoutIntegrationLineLayout.h"
#include "LayoutIntegrationPagination.h"
#include "LayoutRepainter.h"
#include "LineClampUpdater.h"
#include "LocalFrameViewInlines.h"
#include "LocalFrameViewLayoutContext.h"
#include "Logging.h"
#include "PositionedLayoutConstraints.h"
#include "RenderBlockFlow.h"
#include "RenderBlockInlines.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderDescendantIterator.h"
#include "RenderElementInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderGrid.h"
#include "RenderLayer.h"
#include "RenderLayerScrollableArea.h"
#include "RenderLayoutState.h"
#include "RenderMultiColumnFlow.h"
#include "RenderMultiColumnSet.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderObjectInlines.h"
#include "RenderTableCell.h"
#include "RenderView.h"
#include "ScrollbarUpdateScope.h"
#include "SubtreeScrollbarChangesState.h"
#include "TextBoxTrimmer.h"
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
    // A table cell gives its children the block size the row gives it, once it has one, not its own (RenderTableSection::relayoutCellIfFlexed).
    if (CheckedPtr tableCell = dynamicDowncast<RenderTableCell>(rootRenderer)) {
        if (tableCell->style().logicalHeight().isPercentOrCalculated() && tableCell->table()->style().logicalHeight().isAuto())
            return { };
        return tableCell->overridingBorderBoxLogicalHeight().transform([&](auto borderBoxBlockSize) {
            return borderBoxBlockSize - tableCell->computedCSSPaddingBefore() - tableCell->computedCSSPaddingAfter() - tableCell->borderLogicalHeight() - tableCell->scrollbarLogicalHeight();
        });
    }
    auto isPerpendicular = [&](const RenderBlock& containingBlock) {
        return rootRenderer.isHorizontalWritingMode() != containingBlock.isHorizontalWritingMode();
    };
    // An auto-sized view has no definite block size: its percentages behave as auto (RenderBox::computePercentageLogicalHeight).
    if (is<RenderView>(rootRenderer) && rootRenderer.view().frameView().isAutoSizeEnabled())
        return { };
    if (!rootRenderer.skipContainingBlockForPercentHeightCalculation(rootRenderer, false))
        return rootRenderer.availableLogicalHeightForPercentageComputation();
    CheckedPtr<const RenderBox> containingBlockChild = &rootRenderer;
    CheckedPtr<const RenderBlock> containingBlock = &rootRenderer;
    auto skippedMarginsBordersAndPadding = 0_lu;
    CheckedRef layoutContext = rootRenderer.view().frameView().layoutContext();
    while (containingBlock && !is<RenderView>(*containingBlock) && rootRenderer.skipContainingBlockForPercentHeightCalculation(*containingBlock, isPerpendicular(*containingBlock))) {
        if (containingBlock->isBody() || containingBlock->isDocumentElementRenderer()) {
            auto writingMode = containingBlock->writingMode();
            skippedMarginsBordersAndPadding += containingBlock->marginBefore(writingMode) + containingBlock->marginAfter(writingMode) + containingBlock->borderAndPaddingLogicalHeight();
        }
        // Past a box that a block formatting context lays out through the render tree, what it gives the box's content
        // (LocalFrameViewLayoutContext::ContainingBlockBlockSizes).
        if (auto containingBlockBlockSizes = layoutContext->containingBlockBlockSizes(*containingBlock)) {
            return containingBlockBlockSizes->percentageBasis.transform([&](auto blockSize) {
                return std::max(0_lu, blockSize - skippedMarginsBordersAndPadding);
            });
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

// While the root's intrinsic inline sizes are measured. A root that gets its block size from the inline size being measured, through its preferred
// aspect ratio, has none for its content yet (css-sizing-4 §4.2): one with an automatic block size and inline size and no stretch from its container,
// which normal is not for a grid item with a preferred aspect ratio (css-grid-1 §6.2), whatever block size the render tree's grid gives it while it
// measures the root again, which is the size of its row (GridTrackSizingAlgorithm).
static std::optional<LayoutUnit> contentBoxBlockSizeForPercentagesWhileMeasured(const RenderBlockFlow& rootRenderer)
{
    auto blockSizeDependsOnMeasuredInlineSize = rootRenderer.shouldComputeLogicalHeightFromAspectRatio() && !rootRenderer.style().logicalWidth().isSpecified()
        && !rootRenderer.hasStretchedLogicalHeight(rootRenderer.isGridItem() ? StretchingMode::Explicit : StretchingMode::Normal);
    if (blockSizeDependsOnMeasuredInlineSize)
        return { };
    return contentBoxBlockSizeForPercentages(rootRenderer);
}

static Layout::ConstraintsForBlockContent constraintsForBlockContent(const RenderBlockFlow& rootRenderer, RelayoutChildren relayoutChildren)
{
    // The root lives in its parent's formatting context, which has already decided its size. WebKit compatibility: the collapsed border halves
    // of a table cell are in the writing mode of the table (RenderTableCell::borderStart). Block content starts after those on the cell's own
    // inline-start and block-start sides (RenderBlockFlow::determineLogicalLeftPositionForChild), inline content after the physical ones on
    // those sides (BoxGeometryUpdater::setFormattingContextRootGeometry).
    auto writingMode = rootRenderer.writingMode();
    auto borderWidths = rootRenderer.borderWidths();
    auto borderStart = rootRenderer.childrenInline() ? borderWidths.start(writingMode) : rootRenderer.borderStart();
    auto borderBefore = rootRenderer.childrenInline() ? borderWidths.before(writingMode) : rootRenderer.borderBefore();
    auto contentBoxLogicalLeft = borderStart + rootRenderer.paddingStart();
    // The scrollbar in the inline axis sits between the border and the padding: the vertical one, on the left or on the right, in horizontal
    // writing modes and the horizontal one, at the bottom, in vertical writing modes. The block formatting context measures from the inline
    // start, so it counts when it is on that side, like RenderBlockFlow::determineLogicalLeftPositionForChild, which measures from the logical
    // left (top in vertical writing modes) and clamps it to the space it has.
    // With scrollbar-gutter: stable both-edges, a gutter is on that side either way.
    auto scrollbarIsOnLogicalLeft = writingMode.isHorizontal() && rootRenderer.shouldPlaceVerticalScrollbarOnLeft();
    if (rootRenderer.style().scrollbarGutter().isStableBothEdges() || scrollbarIsOnLogicalLeft == writingMode.isLogicalLeftInlineStart())
        contentBoxLogicalLeft += std::min<LayoutUnit>(rootRenderer.scrollbarLogicalWidth(), std::max(0_lu, rootRenderer.logicalWidth() - rootRenderer.borderAndPaddingLogicalWidth()));
    auto horizontalConstraints = Layout::HorizontalConstraints { contentBoxLogicalLeft, rootRenderer.contentBoxLogicalWidth() };
    return { { horizontalConstraints, borderBefore + rootRenderer.paddingBefore() }, contentBoxBlockSizeForPercentages(rootRenderer), rootRenderer.availableLogicalHeight(AvailableLogicalHeightType::ExcludeMarginBorderPadding),
        rootRenderer.availableLogicalHeight(AvailableLogicalHeightType::IncludeMarginBorderPadding), relayoutChildren == RelayoutChildren::Yes };
}

// The last formatted line of a box with trim-end and block-level content, a block-level box on the last line of its inline content included,
// is only known once the content is laid out, which it is again with it (RenderBlockFlow::layoutInFlowChildren). That inline content is laid
// out again (TextBoxTrimmer::lastInlineFormattingContextRootForTrimEnd).
static Layout::BlockLayoutState::LastFormattedLineRoots lastFormattedLineRootsForTrimEnd(const RenderBlockFlow* rootRenderer, const Layout::BlockLayoutResult& result)
{
    auto lastFormattedLineRoots = Layout::BlockLayoutState::LastFormattedLineRoots { };
    auto addLastFormattedLineRoot = [&](const RenderBox& box) {
        CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(box);
        if (!blockContainer)
            return;
        if (CheckedPtr lastFormattedLineRoot = TextBoxTrimmer::lastInlineFormattingContextRootForTrimEnd(*blockContainer)) {
            // The inline content of the block containers on the way, which may lay it out as a block-level box in it, is not kept either.
            for (CheckedPtr<RenderBlock> ancestor = lastFormattedLineRoot; ancestor && ancestor != blockContainer; ancestor = ancestor->containingBlock())
                ancestor->setNeedsLayout(MarkingBehavior::OnlyThis);
            SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef mutableBlockContainer = const_cast<RenderBlockFlow&>(*blockContainer);
            mutableBlockContainer->setNeedsLayout(MarkingBehavior::OnlyThis);
            lastFormattedLineRoots.append({ box, *lastFormattedLineRoot });
        }
    };
    if (rootRenderer)
        addLastFormattedLineRoot(*rootRenderer);
    for (auto& box : result.boxes) {
        if (!box.isLaidOutThroughRenderTree)
            addLastFormattedLineRoot(box.renderer);
    }
    return lastFormattedLineRoots;
}

// The content is laid out one page or column at a time (css-break-3 §2). The render tree keeps one rect per box in its strip of pages or
// columns, from the top of the box's first fragment to the bottom of its last one, each fragment moved by where its page starts in the strip.
// The fragmentainer of each fragment painting reads is where it is in the strip of the fragmented flow, from where the root starts in it.
static Layout::BlockLayoutResult mergeFragments(const RenderBlockFlow& root, const Vector<Layout::BlockLayoutResult>& pageResults, const Vector<LayoutUnit>& pageStripLogicalTops, LayoutUnit stripLogicalTopOfRoot = { }, bool isColumnContent = false)
{
    struct MergedBox {
        Layout::BlockLayoutResult::Box box;
        LayoutUnit stripLogicalTop;
        LayoutUnit stripLogicalBottom;
        std::pair<size_t, size_t> lastOccurrence;
    };
    auto mergedBoxes = Vector<MergedBox> { };
    auto mergedBoxIndexes = HashMap<const RenderBox*, size_t> { };
    // Where each fragment after the first of a block container with inline content starts in the strip, and the first of its lines.
    struct InlineContentFragment {
        size_t firstLineIndex { 0 };
        LayoutUnit stripLogicalTop;
    };
    auto inlineContentFragments = HashMap<const RenderBlockFlow*, Vector<InlineContentFragment>> { };
    // Where each fragment of a box is in the strip, and its block size, which painting gives the decorations of a box with
    // box-decoration-break: clone (css-break-3 §5.4), and of one whose fragments have gaps between them in the strip.
    auto boxFragments = HashMap<const RenderBox*, Vector<Layout::BlockLayoutResult::BoxFragment>> { };
    // In tree order, with where the static position is in the strip.
    auto outOfFlowBoxes = Vector<std::pair<Layout::BlockLayoutResult::OutOfFlowBox, LayoutUnit>> { };
    // The boxes past the end of the column they are in, monolithic ones and those that end with them, with where that column is in the strip,
    // and the boxes in more than one fragment, which a later fragment of a box around them laid them out in again.
    auto monolithicBoxesPastColumnEnds = Vector<std::pair<CheckedRef<const RenderBox>, LayoutUnit>> { };
    auto boxesInMoreThanOneFragment = HashSet<const RenderBox*> { };
    auto containingBlockOf = [](const RenderBox& box) -> const RenderBox* {
        return box.parent()->isInlineBox() ? box.containingBlock() : dynamicDowncast<RenderBox>(*box.parent());
    };
    for (size_t page = 0; page < pageResults.size(); ++page) {
        auto& pageResult = pageResults[page];
        auto pageStripLogicalTop = pageStripLogicalTops[page];
        if (isColumnContent) {
            for (auto& box : pageResult.boxesPastFragmentainerEnd)
                monolithicBoxesPastColumnEnds.append({ box, stripLogicalTopOfRoot + pageStripLogicalTop });
        }
        // In the root's block coordinates of the page, where each box is relative to the fragment of its containing block on the page.
        auto logicalTopsOnPage = HashMap<const RenderBox*, LayoutUnit> { };
        for (size_t index = pageResult.boxes.size(); index--;) {
            auto& box = pageResult.boxes[index];
            CheckedPtr containingBlock = containingBlockOf(box.renderer);
            // A float that went to a page after the one its containing block ends on is placed from the top of the page.
            auto containingBlockLogicalTop = containingBlock == &root ? 0_lu : logicalTopsOnPage.getOptional(containingBlock.get()).value_or(0_lu);
            auto logicalTop = containingBlockLogicalTop + Layout::BoxGeometry::borderBoxTop(box.geometry);
            logicalTopsOnPage.set(box.renderer.ptr(), logicalTop);
            auto stripLogicalTop = pageStripLogicalTop + logicalTop;
            auto stripLogicalBottom = stripLogicalTop + box.geometry.borderBoxHeight();
            if (!box.isLaidOutThroughRenderTree && box.geometry.borderBoxHeight()) {
                // A fragment a negative margin pulls above its fragmentainer is still in it.
                boxFragments.ensure(box.renderer.ptr(), [] {
                    return Vector<Layout::BlockLayoutResult::BoxFragment> { };
                }).iterator->value.append({ stripLogicalTop, box.geometry.borderBoxHeight(), stripLogicalTopOfRoot + std::max(stripLogicalTop, pageStripLogicalTop) });
            }
            auto addResult = mergedBoxIndexes.add(box.renderer.ptr(), mergedBoxes.size());
            if (addResult.isNewEntry) {
                mergedBoxes.append({ box, stripLogicalTop, stripLogicalBottom, { page, index } });
                continue;
            }
            boxesInMoreThanOneFragment.add(box.renderer.ptr());
            // A later fragment: the box ends there, with its block-end border, padding and margin.
            auto& merged = mergedBoxes[addResult.iterator->value];
            auto& geometry = merged.box.geometry;
            geometry.setVerticalBorder({ geometry.borderBefore(), box.geometry.borderAfter() });
            geometry.setVerticalPadding({ geometry.paddingBefore(), box.geometry.paddingAfter() });
            geometry.setVerticalMargin({ geometry.marginBefore(), box.geometry.marginAfter() });
            merged.box.collapsedMarginsAfter = box.collapsedMarginsAfter;
            merged.box.marginAfterIsQuirky = box.marginAfterIsQuirky;
            // An empty one does not move the end of a box whose block size of its own the fragments before used up, and whose content goes on
            // past them (BlockFormattingContext::blockSizeLeftOfItsOwn).
            if (box.geometry.borderBoxHeight())
                merged.stripLogicalBottom = stripLogicalBottom;
            merged.lastOccurrence = { page, index };
        }
        // The static position of an out-of-flow box is relative to the fragment of its parent on the page, and the content of a block container
        // that continues past its end, on a page after the one it ends on, relative to the top of the page.
        for (auto& outOfFlowBox : pageResult.outOfFlowBoxes) {
            CheckedRef parent = downcast<RenderBox>(*outOfFlowBox.renderer->parent());
            auto parentLogicalTop = parent.ptr() == &root ? 0_lu : logicalTopsOnPage.getOptional(parent.ptr()).value_or(0_lu);
            outOfFlowBoxes.append({ outOfFlowBox, pageStripLogicalTop + parentLogicalTop + outOfFlowBox.staticBlockPosition });
        }
        for (auto& fragment : pageResult.inlineContentFragments) {
            auto stripLogicalTop = pageStripLogicalTop + (fragment.blockContainer.ptr() == &root ? 0_lu : logicalTopsOnPage.getOptional(fragment.blockContainer.ptr()).value_or(0_lu));
            inlineContentFragments.ensure(fragment.blockContainer.ptr(), [] {
                return Vector<InlineContentFragment> { };
            }).iterator->value.append({ fragment.firstLineIndex, stripLogicalTop });
        }
    }
    // The lines of each fragment are relative to the fragment's border box, and move to where it is in the strip, relative to the first one.
    for (auto& [blockContainer, fragments] : inlineContentFragments) {
        CheckedRef lineLayout = *const_cast<RenderBlockFlow&>(*blockContainer).inlineLayout();
        auto blockContainerStripLogicalTop = blockContainer == &root ? 0_lu : mergedBoxes[mergedBoxIndexes.get(blockContainer)].stripLogicalTop;
        // All its lines, a last one with no content too (LineLayout::lineCount leaves that out), which moves with the fragment before it.
        auto lastLine = InlineIterator::lastLineBoxFor(*blockContainer);
        auto adjustments = Vector<LineAdjustment>(lastLine ? lastLine->lineIndex() + 1 : 0);
        for (size_t index = 0; index < fragments.size(); ++index) {
            auto endLineIndex = index + 1 < fragments.size() ? fragments[index + 1].firstLineIndex : adjustments.size();
            for (auto lineIndex = fragments[index].firstLineIndex; lineIndex < endLineIndex; ++lineIndex)
                adjustments[lineIndex] = { fragments[index].stripLogicalTop - blockContainerStripLogicalTop, lineIndex == fragments[index].firstLineIndex };
        }
        lineLayout->moveLinesInBlockDirection(adjustments);
    }
    // Post-order: a box goes after its descendants, also after a float that went to a page after the one its containing block ends on.
    auto depths = HashMap<const RenderBox*, size_t> { };
    for (auto& merged : mergedBoxes) {
        auto depth = size_t { 0 };
        for (CheckedPtr containingBlock = containingBlockOf(merged.box.renderer); containingBlock && containingBlock != &root; containingBlock = containingBlockOf(*containingBlock)) {
            ++depth;
            auto& mergedContainingBlock = mergedBoxes[mergedBoxIndexes.get(containingBlock.get())];
            mergedContainingBlock.lastOccurrence = std::max(mergedContainingBlock.lastOccurrence, merged.lastOccurrence);
        }
        depths.set(merged.box.renderer.ptr(), depth);
    }
    auto result = Layout::BlockLayoutResult { };
    auto orderedBoxes = mergedBoxes;
    std::ranges::sort(orderedBoxes, [&](auto& a, auto& b) {
        if (a.lastOccurrence != b.lastOccurrence)
            return a.lastOccurrence < b.lastOccurrence;
        return depths.get(a.box.renderer.ptr()) > depths.get(b.box.renderer.ptr());
    });
    for (auto& merged : orderedBoxes) {
        auto box = merged.box;
        CheckedPtr containingBlock = containingBlockOf(box.renderer);
        auto containingBlockStripLogicalTop = containingBlock == &root ? 0_lu : mergedBoxes[mergedBoxIndexes.get(containingBlock.get())].stripLogicalTop;
        auto& geometry = box.geometry;
        geometry.setContentBoxHeight(std::max(0_lu, merged.stripLogicalBottom - merged.stripLogicalTop - geometry.verticalBorderAndPadding() - geometry.verticalSpaceForScrollbar()));
        geometry.setTop(merged.stripLogicalTop - containingBlockStripLogicalTop);
        result.boxes.append(WTF::move(box));
    }
    for (auto& [outOfFlowBox, staticBlockPositionInStrip] : outOfFlowBoxes) {
        // A box in the top layer has the start of its parent for its static position wherever it is in the content, like in the render tree
        // (RenderBlockFlow::adjustOutOfFlowBlock), which no fragment moves.
        if (RefPtr element = outOfFlowBox.renderer->element(); element && element->isInTopLayer()) [[unlikely]] {
            result.outOfFlowBoxes.append(outOfFlowBox);
            continue;
        }
        CheckedRef parent = downcast<RenderBox>(*outOfFlowBox.renderer->parent());
        auto parentStripLogicalTop = parent.ptr() == &root ? 0_lu : mergedBoxes[mergedBoxIndexes.get(parent.ptr())].stripLogicalTop;
        result.outOfFlowBoxes.append({ outOfFlowBox.renderer, outOfFlowBox.staticInlinePosition, staticBlockPositionInStrip - parentStripLogicalTop });
    }
    for (auto& pageResult : pageResults) {
        result.invisibleBoxes.appendVector(pageResult.invisibleBoxes);
        result.fragmentedBoxes.appendVector(pageResult.fragmentedBoxes);
    }
    for (auto& [renderer, fragments] : boxFragments) {
        if (fragments.size() < 2)
            continue;
        auto clonesDecorations = renderer->style().boxDecorationBreak() == BoxDecorationBreak::Clone;
        // A fragment whose block-start border and padding cross the end of its fragmentainer goes past it, over the next one.
        auto hasGaps = std::ranges::adjacent_find(fragments, [](auto& fragment, auto& nextFragment) {
            return fragment.blockOffset + fragment.blockSize < nextFragment.blockOffset;
        }) != fragments.end();
        if (!clonesDecorations && !hasGaps)
            continue;
        // The fragments of one that slices its decorations that follow each other in the strip paint as one slice of the unbroken box.
        if (!clonesDecorations) {
            auto runs = Vector<Layout::BlockLayoutResult::BoxFragment> { };
            for (auto& fragment : fragments) {
                if (!runs.isEmpty() && runs.last().blockOffset + runs.last().blockSize >= fragment.blockOffset)
                    runs.last().blockSize = std::max(runs.last().blockSize, fragment.blockOffset + fragment.blockSize - runs.last().blockOffset);
                else
                    runs.append(fragment);
            }
            fragments = WTF::move(runs);
        }
        auto borderBoxStripLogicalTop = mergedBoxes[mergedBoxIndexes.get(renderer)].stripLogicalTop;
        for (auto& fragment : fragments)
            fragment.blockOffset -= borderBoxStripLogicalTop;
        result.fragmentedBoxes.append({ *renderer, clonesDecorations, WTF::move(fragments) });
    }
    for (auto& entry : monolithicBoxesPastColumnEnds) {
        if (!boxesInMoreThanOneFragment.contains(entry.first.ptr()))
            result.boxesPastColumnEnds.append(entry);
    }
    // The root's content ends in the last fragment that has it, before those with only the content overflowing block containers with a block
    // size of their own.
    auto lastPage = pageResults.size() - 1;
    while (lastPage && pageResults[lastPage].hasOnlyParallelFlows)
        --lastPage;
    auto lastPageStripLogicalTop = pageStripLogicalTops[lastPage];
    auto rootBorderAndPaddingBefore = root.borderAndPaddingBefore();
    result.intrinsicContentBlockSize = lastPage ? std::max(0_lu, lastPageStripLogicalTop + pageResults[lastPage].intrinsicContentBlockSize - rootBorderAndPaddingBefore) : pageResults[lastPage].intrinsicContentBlockSize;
    return result;
}

// Printing lays the root element out one page at a time, the first one with what is left of it below where the root starts on it, and a
// multi-column flow one column at a time (css-break-3 §2), whose block size comes from the column set of each multi-column line
// (columnBlockSize).
struct Fragmentation {
    Layout::FragmentainerConstraints::Type type;
    std::optional<LayoutUnit> firstFragmentainerBlockSize;
    std::optional<LayoutUnit> fragmentainerBlockSize;
    // The columns of a paginated view, whose break values are those of pages (RenderMultiColumnFlow::shouldCheckColumnBreaks).
    bool breaksLikePages { false };

    Layout::FragmentainerConstraints fragmentainer(Layout::FragmentainerConstraints&& fragmentainer) const
    {
        fragmentainer.breaksLikePages = breaksLikePages;
        return WTF::move(fragmentainer);
    }
};

static std::optional<Fragmentation> fragmentationForRoot(RenderBlockFlow& root, const RenderLayoutState* layoutState)
{
    if (CheckedPtr multiColumnFlow = dynamicDowncast<RenderMultiColumnFlow>(root))
        return Fragmentation { Layout::FragmentainerConstraints::Type::Column, { }, { }, !multiColumnFlow->shouldCheckColumnBreaks() };
    // A root the render tree lays out in a multi-column flow breaks across its columns, and reports what column balancing needs while they have
    // no block size yet (fragmentainerAtStripOffset), which the flow's layout state tells with a page height of its own once they have one
    // (RenderBlockFlow::checkForPaginationLogicalHeightChange), but not in a box that does not break, which takes that page height away
    // (RenderLayoutState::computePaginationInformation).
    if (CheckedPtr multiColumnFlow = dynamicDowncast<RenderMultiColumnFlow>(root.enclosingFragmentedFlow())) {
        if (!layoutState || !layoutState->isPaginated() || (!layoutState->pageLogicalHeight() && static_cast<const RenderFragmentedFlow&>(*multiColumnFlow).isPageLogicalHeightKnown()))
            return { };
        return Fragmentation { Layout::FragmentainerConstraints::Type::Column, { }, { }, !multiColumnFlow->shouldCheckColumnBreaks() };
    }
    auto pageLogicalHeight = layoutState && layoutState->isPaginated() ? layoutState->pageLogicalHeight() : 0_lu;
    if (!pageLogicalHeight)
        return { };
    auto offsetOnFirstPage = intMod(layoutState->pageLogicalOffset(&root, { }), pageLogicalHeight);
    return Fragmentation { Layout::FragmentainerConstraints::Type::Page, pageLogicalHeight - offsetOnFirstPage, pageLogicalHeight };
}

// The render tree's layout state has no column block size for the flow (RenderBlockFlow::checkForPaginationLogicalHeightChange); its column
// set does (RenderMultiColumnSet::prepareForLayout), but not yet when balancing lays the content out first. Neither does the multi-column line
// before a column spanner at the start of the flow, which has no column set, nor content. Nor do columns with a block size of 0, where the
// render tree does not break (RenderBlockFlow::pageLogicalHeightForOffset).
static std::optional<LayoutUnit> columnBlockSize(const RenderMultiColumnSet* columnSet)
{
    if (!columnSet || !columnSet->columnHeightComputed() || !columnSet->computedColumnHeight())
        return { };
    return columnSet->computedColumnHeight();
}

// The multi-column flow whose columns are the fragmentainers, the root, or the one the render tree lays the root out in.
static const RenderMultiColumnFlow* columnFlowOf(const RenderBlockFlow& root, const Fragmentation& fragmentation)
{
    if (fragmentation.type != Layout::FragmentainerConstraints::Type::Column)
        return nullptr;
    if (auto* multiColumnFlow = dynamicDowncast<RenderMultiColumnFlow>(root))
        return multiColumnFlow;
    return downcast<RenderMultiColumnFlow>(root.enclosingFragmentedFlow());
}

// The fragmentainer of the root's content that content starting at a block offset in the strip goes into, the one that starts there when one
// does: its block size, none while the columns have none (columnBlockSize), and how far into it the content starts, below other content.
static Layout::FragmentainerConstraints fragmentainerAtStripOffset(const RenderBlockFlow& root, const Fragmentation& fragmentation, LayoutUnit stripLogicalOffset, bool appliesForcedBreaks, bool appliesForcedBreakAfterContent = false)
{
    auto fragmentainer = [&](std::optional<LayoutUnit> blockSize, LayoutUnit offset) {
        return fragmentation.fragmentainer({ blockSize, fragmentation.type, offset, offset > 0, appliesForcedBreaks, appliesForcedBreakAfterContent, stripLogicalOffset - offset });
    };
    if (CheckedPtr<const RenderFragmentedFlow> multiColumnFlow = columnFlowOf(root, fragmentation)) {
        auto columnHeight = columnBlockSize(dynamicDowncast<RenderMultiColumnSet>(multiColumnFlow->fragmentAtBlockOffset(nullptr, stripLogicalOffset)));
        if (!columnHeight)
            return fragmentainer({ }, { });
        return fragmentainer(columnHeight, *columnHeight - multiColumnFlow->pageRemainingLogicalHeightForOffset(stripLogicalOffset, RenderBlockFlow::ExcludePageBoundary));
    }
    auto pageBlockSize = *fragmentation.fragmentainerBlockSize;
    auto offsetInPage = intMod(stripLogicalOffset - *fragmentation.firstFragmentainerBlockSize, pageBlockSize);
    if (offsetInPage < 0)
        offsetInPage += pageBlockSize;
    return fragmentainer(pageBlockSize, offsetInPage);
}

// Each multi-column line of a multi-column flow has a column set of its own, the first one, or the one after the column spanner the line before
// ended at (RenderTreeBuilder::MultiColumn::processPossibleSpannerDescendant).
struct ColumnFragment {
    CheckedPtr<RenderMultiColumnSet> columnSet;
    bool endsWithForcedBreak { false };
    bool endsWithColumnSpanner { false };
    // A column past the last one of its column set, which column balancing does not make room for.
    bool isPastLastColumn { false };
};

// Column balancing picks the next column block size of each column set from the forced breaks in its content, the least space shortage at its
// breaks and its tallest content that cannot break (RenderMultiColumnSet::recalculateColumnHeight), which the render tree path reports as it
// lays the content out (RenderBlockFlow::applyBeforeBreak, setPageBreak and updateMinimumPageHeight). It also gives each column set the part of
// the flow from the column spanner before it to the one after it (RenderMultiColumnFlow::fragmentedFlowDescendantBoxLaidOut), the flow itself
// giving the first one its start and the last one its end (RenderMultiColumnFlow::layout).
static void updateColumnSets(const Vector<Layout::BlockLayoutResult>& columnResults, const Vector<LayoutUnit>& stripLogicalTops, const Vector<ColumnFragment>& columnFragments)
{
    for (size_t index = 0; index < columnFragments.size(); ++index) {
        auto& columnFragment = columnFragments[index];
        auto& columnResult = columnResults[index];
        auto stripLogicalBottom = stripLogicalTops[index] + columnResult.fragmentainerBlockSize;
        CheckedPtr columnSet = columnFragment.columnSet;
        if (columnFragment.endsWithColumnSpanner) {
            if (columnSet)
                columnSet->setLogicalBottomInFragmentedFlow(stripLogicalBottom);
            auto nextIndex = index + 1;
            while (columnFragments[nextIndex].isPastLastColumn)
                ++nextIndex;
            if (CheckedPtr nextColumnSet = columnFragments[nextIndex].columnSet)
                nextColumnSet->setLogicalTopInFragmentedFlow(stripLogicalBottom);
        }
        if (!columnSet || !columnSet->requiresBalancing())
            continue;
        if (columnFragment.endsWithForcedBreak)
            columnSet->addForcedBreak(stripLogicalBottom);
        if (columnResult.spaceShortage)
            columnSet->recordSpaceShortage(*columnResult.spaceShortage);
        columnSet->updateMinimumColumnHeight(columnResult.unbreakableBlockSize);
    }
}

// A box with size containment is monolithic (css-contain-2 §3.2), and column balancing also makes room for the box and its scrollable
// overflow in the column it is in (RenderBlockFlow::adjustSizeContainmentChildForPagination), but not for a float, like the render tree, in the
// block axis of the columns, the root's, also for a box with a writing mode of its own. The render tree measures the
// room an in-flow box needs from where the box is before a break moves it to the next column, where it ends up here, which balances the
// columns more equally (css-multicol-1 §7.1).
static void updateSpaceShortageForSizeContainment(RenderFragmentedFlow& multiColumnFlow, const Layout::BlockLayoutResult& result)
{
    for (auto& box : result.boxes) {
        CheckedRef renderer = box.renderer;
        if (!renderer->shouldApplySizeContainment() || renderer->isFloating())
            continue;
        // Also when its containing block is an inline box, which places it from the inline box's first fragment.
        auto stripLogicalTop = renderer->offsetFromLogicalTopOfFirstPage();
        // In the block axis of the columns, from the block-start edge of the box, where the overflow, physical once out of the box's own
        // writing mode, ends on its other side in a flipped block flow direction.
        auto overflow = renderer->layoutOverflowRect();
        renderer->blockStartRelativeToPhysical(overflow);
        auto writingMode = multiColumnFlow.writingMode();
        auto borderBoxSize = renderer->borderBoxSize();
        auto boxBlockSize = writingMode.isHorizontal() ? borderBoxSize.height() : borderBoxSize.width();
        auto overflowBlockStart = writingMode.isHorizontal() ? overflow.y() : overflow.x();
        auto overflowBlockEnd = writingMode.isBlockFlipped() ? boxBlockSize - overflowBlockStart : (writingMode.isHorizontal() ? overflow.maxY() : overflow.maxX());
        auto blockSize = std::max(boxBlockSize, overflowBlockEnd);
        auto shortage = blockSize - multiColumnFlow.pageRemainingLogicalHeightForOffset(stripLogicalTop, RenderBlockFlow::ExcludePageBoundary);
        if (shortage > 0)
            multiColumnFlow.updateSpaceShortageForSizeContainment(nullptr, stripLogicalTop, shortage);
    }
}

// A line grid around a box that continues from the fragmentainer before has its rows start again at the top of this one, below the block-start
// border and padding of the box that establishes it, like the render tree snaps the first line after a break (computeFirstLineSnapAdjustment),
// but not one around the multi-column flow the box is in (BlockFormattingContext::lineGridForInlineContent).
static Layout::RootLineGrid lineGridAfterBreak(const Layout::RootLineGrid& lineGrid, LayoutUnit fragmentStripLogicalTop, const RenderBlockFlow& box)
{
    auto fragmentLineGrid = lineGrid;
    if (fragmentLineGrid.lineGrid && fragmentLineGrid.lineGrid->outsideRoot && !Layout::BlockFormattingContext::isAroundFragmentedFlow(fragmentLineGrid.lineGrid->blockContainer, box))
        fragmentLineGrid.lineGrid->outsideRoot->borderBoxOffset.setHeight(0_lu);
    fragmentLineGrid.borderBoxOffsetInLayoutState.expand(0_lu, fragmentStripLogicalTop);
    return fragmentLineGrid;
}

// The line-clamp budget goes on in the next fragmentainer with what the content of the fragmentainers before left of it.
static void continueLineClamp(Layout::BlockLayoutState& blockLayoutState, const Layout::BlockLayoutState& blockLayoutStateBefore)
{
    blockLayoutState.setLineClamp(blockLayoutStateBefore.lineClamp());
    blockLayoutState.setClampAfterBox(blockLayoutStateBefore.clampAfterBox());
}

// The content of a box that is a flow of its own in the root's content, that of an out-of-flow box or of a block formatting context root with a
// block size of its own (css-break-3 §2.1), one fragmentainer of the root at a time from where the box starts in the strip, merged into the
// strip like the root's content (BlockLayout::layout). Column balancing makes room for it too (RenderBlockFlow::setPageBreak and
// updateMinimumPageHeight), but for its forced breaks, which the box adds again each time it is laid out.
static Layout::BlockLayoutResult layoutAcrossFragmentainers(const RenderBlockFlow& fragmentationRoot, const Fragmentation& fragmentation, BlockIntegrationUtils& integrationUtils, const RenderBlockFlow& box, const Layout::BlockLayoutState& initialBlockLayoutState, const Layout::ConstraintsForBlockContent& constraints, const Layout::BoxGeometry& geometry, const Layout::RootLineGrid& lineGrid, LayoutUnit borderBoxStripLogicalTop, bool appliesForcedBreaks, bool appliesForcedBreakAfterContent = false, bool keepsContentAtStart = false, Layout::BlockLayoutState* lastBlockLayoutState = nullptr)
{
    auto blockLayoutStateBefore = initialBlockLayoutState;
    auto fragmentResults = Vector<Layout::BlockLayoutResult> { };
    auto fragmentStripLogicalTops = Vector<LayoutUnit> { };
    CheckedPtr<const RenderFragmentedFlow> multiColumnFlow = columnFlowOf(fragmentationRoot, fragmentation);
    auto columnFragments = Vector<ColumnFragment> { };
    auto columnStripLogicalTops = Vector<LayoutUnit> { };
    auto breakToken = std::optional<Layout::BlockBreakToken> { };
    do {
        fragmentStripLogicalTops.append(fragmentResults.isEmpty() ? 0_lu : fragmentStripLogicalTops.last() + fragmentResults.last().fragmentainerBlockSize);
        auto stripLogicalTop = borderBoxStripLogicalTop + fragmentStripLogicalTops.last();
        auto fragmentBlockLayoutState = initialBlockLayoutState;
        continueLineClamp(fragmentBlockLayoutState, blockLayoutStateBefore);
        auto fragmentainer = fragmentainerAtStripOffset(fragmentationRoot, fragmentation, stripLogicalTop, appliesForcedBreaks, appliesForcedBreakAfterContent);
        if (keepsContentAtStart && fragmentResults.isEmpty()) {
            fragmentainer.hasContentBefore = false;
            fragmentainer.keepsContentAtStart = true;
        }
        auto fragmentResult = Layout::BlockFormattingContext { box, integrationUtils, fragmentBlockLayoutState }.layout(constraints, geometry, fragmentResults.isEmpty() ? lineGrid : lineGridAfterBreak(lineGrid, fragmentStripLogicalTops.last(), box), fragmentainer, breakToken ? &*breakToken : nullptr);
        breakToken = std::exchange(fragmentResult.breakToken, std::nullopt);
        fragmentResults.append(WTF::move(fragmentResult));
        blockLayoutStateBefore = WTF::move(fragmentBlockLayoutState);
        if (multiColumnFlow) {
            columnFragments.append({ dynamicDowncast<RenderMultiColumnSet>(multiColumnFlow->fragmentAtBlockOffset(nullptr, stripLogicalTop)) });
            columnStripLogicalTops.append(stripLogicalTop);
        }
    } while (breakToken);
    updateColumnSets(fragmentResults, columnStripLogicalTops, columnFragments);
    if (lastBlockLayoutState)
        continueLineClamp(*lastBlockLayoutState, blockLayoutStateBefore);
    if (fragmentResults.size() == 1)
        return WTF::move(fragmentResults[0]);
    // None of the content fit in the first fragmentainer, below the content before the box there, which the box may go to the next one with.
    auto result = mergeFragments(box, fragmentResults, fragmentStripLogicalTops, borderBoxStripLogicalTop);
    result.isBreakBeforeContent = fragmentResults[0].isBreakBeforeContent;
    return result;
}

// A multi-column container lays out its multi-column flow before its content, the column sets and column spanners, at the start of its content
// box (RenderBlockFlow::layoutExcludedChildren), the column sets dropping the column block size balancing gave them before when it is first laid
// out (RenderMultiColumnSet::prepareForLayout). Whether it was laid out, which column balancing then makes room for.
static bool layoutMultiColumnFlow(RenderBlockFlow& container, RenderMultiColumnFlow& multiColumnFlow, RelayoutChildren relayoutChildren, bool isFirstLayout)
{
    auto constraints = constraintsForBlockContent(container, relayoutChildren);
    multiColumnFlow.setLogicalTop(constraints.logicalTop());
    if (relayoutChildren == RelayoutChildren::Yes)
        multiColumnFlow.setChildNeedsLayout(MarkingBehavior::OnlyThis);
    auto needsLayout = multiColumnFlow.needsLayout();
    if (needsLayout) {
        for (CheckedPtr columnSet = multiColumnFlow.firstMultiColumnSet(); columnSet; columnSet = columnSet->nextSiblingMultiColumnSet())
            columnSet->prepareForLayout(isFirstLayout);
        multiColumnFlow.invalidateFragments(MarkingBehavior::OnlyThis);
        multiColumnFlow.layout();
    }
    // Its inline size is that of a column, from the inline start of the content box.
    auto inlineStart = constraints.horizontal().logicalLeft;
    multiColumnFlow.setLogicalLeft(container.writingMode().isLogicalLeftInlineStart() ? inlineStart : container.logicalWidth() - inlineStart - multiColumnFlow.logicalWidth());
    return needsLayout;
}

void BlockLayout::layout(RelayoutChildren relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom)
{
    CheckedPtr multiColumnFlow = rootRenderer().multiColumnFlow();
    if (!multiColumnFlow)
        return layoutContent(relayoutChildren, repaintLogicalTop, repaintLogicalBottom);
    // Column balancing (css-multicol-1 §7.1): each column set that balances its columns takes a new column block size from what the content of
    // the flow reported as it was laid out (RenderMultiColumnSet::recalculateColumnHeight), and the flow and the container's content are laid out
    // again until none changes (RenderBlockFlow::relayoutForPagination).
    auto flowWasLaidOut = layoutMultiColumnFlow(rootRenderer(), *multiColumnFlow, relayoutChildren, true);
    layoutContent(relayoutChildren, repaintLogicalTop, repaintLogicalBottom);
    if (!flowWasLaidOut)
        return;
    for (auto isFirstPass = true; ; isFirstPass = false) {
        auto columnBlockSizeChanged = false;
        for (CheckedPtr columnSet = multiColumnFlow->firstMultiColumnSet(); columnSet; columnSet = columnSet->nextSiblingMultiColumnSet())
            columnBlockSizeChanged |= columnSet->recalculateColumnHeight(isFirstPass);
        if (!columnBlockSizeChanged)
            return;
        multiColumnFlow->setChildNeedsLayout(MarkingBehavior::OnlyThis);
        layoutMultiColumnFlow(rootRenderer(), *multiColumnFlow, RelayoutChildren::No, false);
        layoutContent(RelayoutChildren::No, repaintLogicalTop, repaintLogicalBottom);
    }
}

void BlockLayout::layoutContent(RelayoutChildren relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom)
{
    auto nestedBlockLayoutCoverageScope = NestedBlockLayoutCoverageScope { };
    auto paintInvalidation = PaintInvalidation { };
    auto integrationUtils = BlockIntegrationUtils { paintInvalidation };
    integrationUtils.setRootForScrollbarChanges(rootRenderer());
    // The line-clamp budget of the root's block formatting context: the root's LineClampUpdater sets it up and reads back what the content left
    // of it. The boxes in the content laid out through the render tree do not see it, since the lines of independent formatting contexts do not
    // count (css-overflow-4 §5.2).
    auto* renderTreeLayoutState = rootRenderer().view().frameView().layoutContext().layoutState();
    auto renderTreeLineClamp = renderTreeLayoutState ? renderTreeLayoutState->lineClamp() : std::nullopt;
    if (renderTreeLineClamp)
        renderTreeLayoutState->setLineClamp({ });
    // -webkit-line-clamp keeps its count of lines in the layout state, where the content adds its lines as it gets laid out, every time
    // (BlockIntegrationUtils::legacyLineClampMaximumLines).
    auto renderTreeLegacyLineClamp = renderTreeLayoutState ? renderTreeLayoutState->legacyLineClamp() : std::nullopt;
    // The text-box-trim the root's TextBoxTrimmer sets up for its content, which does not reach into the independent formatting contexts in it
    // either.
    CheckedRef layoutContext = rootRenderer().view().frameView().layoutContext();
    auto renderTreeTextBoxTrim = layoutContext->textBoxTrim();
    layoutContext->setTextBoxTrim({ });

    // The line grid around the root, which its layout state has too (RenderLayoutState::lineGrid).
    auto rootLineGrid = BlockIntegrationUtils::lineGridForRoot(rootRenderer());

    auto hasAutoScrollbarChanges = false;
    auto layoutContentOnce = [&](Layout::BlockLayoutState::LastFormattedLineRoots&& lastFormattedLineRoots) {
        auto blockLayoutState = Layout::BlockLayoutState { { } };
        auto pagesHaveFloats = false;
        if (renderTreeLegacyLineClamp) {
            renderTreeLayoutState->setLegacyLineClamp(renderTreeLegacyLineClamp);
            blockLayoutState.setLineClamp(Layout::BlockLayoutState::LineClamp { *BlockIntegrationUtils::legacyLineClampMaximumLines(rootRenderer()), false, true });
        } else if (renderTreeLineClamp) {
            blockLayoutState.setLineClamp(Layout::BlockLayoutState::LineClamp { renderTreeLineClamp->maximumLines, renderTreeLineClamp->shouldDiscardOverflow, false, renderTreeLineClamp->isStableTextWrap, renderTreeLineClamp->hasConstrainedTextWrapContent, renderTreeLineClamp->shouldTrimEndAtClampPoint });
            blockLayoutState.setClampAfterBox(renderTreeLineClamp->clampAfterBox.get());
        }
        if (renderTreeTextBoxTrim)
            blockLayoutState.setTextBoxTrimState({ renderTreeTextBoxTrim->trimFirstFormattedLine, renderTreeTextBoxTrim->lastFormattedLineRoot.get() });
        blockLayoutState.setLastFormattedLineRoots(WTF::move(lastFormattedLineRoots));
        if (CheckedPtr lastFormattedLineRoot = blockLayoutState.lastFormattedLineRoot(rootRenderer()))
            blockLayoutState.setTextBoxTrimState({ blockLayoutState.textBoxTrimState().trimsFirstFormattedLine, lastFormattedLineRoot.get() });
        auto initialBlockLayoutState = blockLayoutState;
        auto fragmentation = fragmentationForRoot(rootRenderer(), renderTreeLayoutState);
        // Forced breaks apply in the normal flow of the fragmentation root only (css-break-3 §3.1), not in a float or an out-of-flow box the
        // render tree lays out in the pages, like the render tree (inNormalFlow in RenderBlockFlow::applyBeforeBreak).
        auto appliesForcedBreaks = [&] {
            for (CheckedPtr<const RenderBlock> box = &rootRenderer(); box && !box->isRenderView() && !box->isRenderFragmentedFlow(); box = box->containingBlock()) {
                if (box->isFloatingOrOutOfFlowPositioned())
                    return false;
            }
            return true;
        }();
        auto layoutRootContent = [&](const Layout::ConstraintsForBlockContent& constraints) {
            if (!fragmentation)
                return Layout::BlockFormattingContext { rootRenderer(), integrationUtils, blockLayoutState }.layout(constraints, BlockIntegrationUtils::rootGeometry(rootRenderer()), rootLineGrid);
            auto pageResults = Vector<Layout::BlockLayoutResult> { };
            auto pageStripLogicalTops = Vector<LayoutUnit> { };
            auto rootContentLayout = BlockIntegrationUtils::FragmentedContentLayout { [&](const RenderBlockFlow& box, const Layout::BlockLayoutState& initialBlockLayoutState, const Layout::ConstraintsForBlockContent& constraints, const Layout::BoxGeometry& geometry, const Layout::RootLineGrid& lineGrid, LayoutUnit borderBoxStripLogicalTop, bool appliesForcedBreaks) {
                return layoutAcrossFragmentainers(rootRenderer(), *fragmentation, integrationUtils, box, initialBlockLayoutState, constraints, geometry, lineGrid, borderBoxStripLogicalTop, appliesForcedBreaks);
            } };
            integrationUtils.setFragmentedContentLayout(&rootContentLayout);
            // When none of its content fits below the content before it in its first page or column, a pagination strut takes it to the next
            // one, but not a table cell or an out-of-flow box (RenderBlockFlow::adjustLinePositionForPagination and adjustBlockChildForPagination).
            auto movesWithPaginationStrut = !rootRenderer().isRenderTableCell() && !rootRenderer().isOutOfFlowPositioned();
            // A block container moves it from where its content stays. A flex or grid container leaves it there, where its content goes to the
            // next column instead (css-break-3 §4.4), but in pages its first line stays across the break, like in the render tree, which gives
            // any block the strut (RenderBlockFlow::adjustLinePositionForPagination): a flex or grid container lays its items out before it
            // places them (FlexIntegrationUtils::layoutFlexItemWithMainSize), and a page break moved for where an item was before does not
            // move with it.
            CheckedPtr rootParent = rootRenderer().parent();
            auto keepsContentAtStartWhenMovedInColumns = movesWithPaginationStrut && rootParent && rootParent->isRenderBlockFlow();
            auto keepsContentAtStartWhenMovedInPages = movesWithPaginationStrut;
            CheckedPtr multiColumnFlow = dynamicDowncast<RenderMultiColumnFlow>(rootRenderer());
            // A root the render tree lays out in a multi-column flow breaks across the columns from where it starts in them, like an out-of-flow
            // box in the flow, whose columns column balancing makes room for it in.
            if (!multiColumnFlow && fragmentation->type == Layout::FragmentainerConstraints::Type::Column) {
                auto stripLogicalTop = rootRenderer().offsetFromLogicalTopOfFirstPage();
                auto layoutColumns = [&](bool keepsContentAtStart) {
                    return layoutAcrossFragmentainers(rootRenderer(), *fragmentation, integrationUtils, rootRenderer(), initialBlockLayoutState, constraints, BlockIntegrationUtils::rootGeometry(rootRenderer()), rootLineGrid, stripLogicalTop, appliesForcedBreaks, appliesForcedBreaks, keepsContentAtStart, &blockLayoutState);
                };
                auto result = layoutColumns(false);
                if (result.isBreakBeforeContent && movesWithPaginationStrut) {
                    rootRenderer().setPaginationStrut(fragmentainerAtStripOffset(rootRenderer(), *fragmentation, stripLogicalTop, appliesForcedBreaks).availableBlockSize());
                    if (keepsContentAtStartWhenMovedInColumns)
                        result = layoutColumns(true);
                }
                integrationUtils.setFragmentedContentLayout(nullptr);
                pagesHaveFloats |= std::ranges::any_of(result.boxes, [](auto& box) {
                    return box.renderer->isFloating();
                });
                if (rootRenderer().childrenInline())
                    rootRenderer().view().layoutState().ensureGeometryForBox(rootRenderer().layoutBox()) = BlockIntegrationUtils::rootGeometry(rootRenderer());
                return result;
            }
            auto columnFragments = Vector<ColumnFragment> { };
            auto layoutPages = [&](bool keepsContentAtStart) {
                pageResults.clear();
                pageStripLogicalTops.clear();
                columnFragments.clear();
                CheckedPtr columnSet = multiColumnFlow ? dynamicDowncast<RenderMultiColumnSet>(multiColumnFlow->firstColumnSetOrSpanner()) : nullptr;
                auto breakToken = std::optional<Layout::BlockBreakToken> { };
                auto nextPageStripLogicalTop = 0_lu;
                blockLayoutState = initialBlockLayoutState;
                do {
                    pageStripLogicalTops.append(nextPageStripLogicalTop);
                    auto pageBlockLayoutState = initialBlockLayoutState;
                    continueLineClamp(pageBlockLayoutState, blockLayoutState);
                    auto blockSize = multiColumnFlow ? columnBlockSize(columnSet.get()) : fragmentation->fragmentainerBlockSize;
                    // The root starts below the top of its first page, a whole page, where its content may break before its first line, like a table
                    // cell's (RenderBlockFlow::adjustLinePositionForPagination), and where content as tall as a page is not taller than one.
                    auto offsetInFirstPage = !multiColumnFlow && pageResults.isEmpty() ? *fragmentation->fragmentainerBlockSize - *fragmentation->firstFragmentainerBlockSize : 0_lu;
                    // Not in a table cell, which the render tree keeps to the height of its row (RenderTableSection::layoutRows).
                    auto appliesForcedBreakAfterContent = appliesForcedBreaks && !multiColumnFlow && !rootRenderer().isDocumentElementRenderer() && !rootRenderer().isRenderTableCell();
                    auto fragmentainer = fragmentation->fragmentainer({ blockSize, fragmentation->type, offsetInFirstPage, false, appliesForcedBreaks, appliesForcedBreakAfterContent, pageStripLogicalTops.last() - offsetInFirstPage, keepsContentAtStart && pageResults.isEmpty() });
                    auto pageResult = Layout::BlockFormattingContext { rootRenderer(), integrationUtils, pageBlockLayoutState }.layout(constraints, BlockIntegrationUtils::rootGeometry(rootRenderer()), pageResults.isEmpty() ? rootLineGrid : lineGridAfterBreak(rootLineGrid, pageStripLogicalTops.last(), rootRenderer()), fragmentainer, breakToken ? &*breakToken : nullptr);
                    pagesHaveFloats |= !!pageBlockLayoutState.placedFloats();
                    continueLineClamp(blockLayoutState, pageBlockLayoutState);
                    breakToken = std::exchange(pageResult.breakToken, std::nullopt);
                    nextPageStripLogicalTop = pageStripLogicalTops.last() + pageResult.fragmentainerBlockSize;
                    pageResults.append(WTF::move(pageResult));
                    if (!multiColumnFlow)
                        continue;
                    CheckedPtr columnSpannerPlaceholder = breakToken ? downcast<RenderMultiColumnSpannerPlaceholder>(breakToken->columnSpannerPlaceholder) : nullptr;
                    columnFragments.append({ columnSet, breakToken && breakToken->isForced, !!columnSpannerPlaceholder });
                    // The content overflowing block containers with a block size of their own in the multi-column line goes on in columns of its
                    // column set past its last one, from where the last one ends, before the multi-column line after the spanner.
                    if (auto parallelFlowsBreakToken = breakToken ? std::exchange(breakToken->parallelFlowsPastLastColumn, nullptr) : nullptr) {
                        auto columnBreakToken = std::optional { WTF::move(*parallelFlowsBreakToken) };
                        auto stripLogicalTop = pageStripLogicalTops.last() + *blockSize;
                        while (columnBreakToken) {
                            auto columnBlockLayoutState = initialBlockLayoutState;
                            continueLineClamp(columnBlockLayoutState, blockLayoutState);
                            auto columnFragmentainer = fragmentation->fragmentainer({ blockSize, fragmentation->type, { }, false, appliesForcedBreaks, false, stripLogicalTop });
                            auto columnResult = Layout::BlockFormattingContext { rootRenderer(), integrationUtils, columnBlockLayoutState }.layout(constraints, BlockIntegrationUtils::rootGeometry(rootRenderer()), rootLineGrid, columnFragmentainer, &*columnBreakToken);
                            pagesHaveFloats |= !!columnBlockLayoutState.placedFloats();
                            continueLineClamp(blockLayoutState, columnBlockLayoutState);
                            columnBreakToken = std::exchange(columnResult.breakToken, std::nullopt);
                            pageStripLogicalTops.append(stripLogicalTop);
                            stripLogicalTop += columnResult.fragmentainerBlockSize;
                            pageResults.append(WTF::move(columnResult));
                            columnFragments.append({ nullptr, false, false, true });
                        }
                    }
                    if (columnSpannerPlaceholder)
                        columnSet = dynamicDowncast<RenderMultiColumnSet>(columnSpannerPlaceholder->spanner()->nextSiblingBox());
                } while (breakToken);
            };
            layoutPages(false);
            if (!multiColumnFlow && pageResults[0].isBreakBeforeContent && fragmentation->firstFragmentainerBlockSize != fragmentation->fragmentainerBlockSize && movesWithPaginationStrut) {
                rootRenderer().setPaginationStrut(*fragmentation->firstFragmentainerBlockSize);
                if (keepsContentAtStartWhenMovedInPages)
                    layoutPages(true);
            }
            integrationUtils.setFragmentedContentLayout(nullptr);
            updateColumnSets(pageResults, pageStripLogicalTops, columnFragments);
            // The inline content of the root took the geometry of its last fragment, without the block-start border and padding of the first,
            // which the formatting context the root is in reads its geometry from, like a grid item's (BoxGeometryUpdater::updateBoxGeometryAfterIntegrationLayout).
            if (rootRenderer().childrenInline() && pageResults.size() > 1)
                rootRenderer().view().layoutState().ensureGeometryForBox(rootRenderer().layoutBox()) = BlockIntegrationUtils::rootGeometry(rootRenderer());
            return mergeFragments(rootRenderer(), pageResults, pageStripLogicalTops, { }, !!multiColumnFlow);
        };
        auto constraints = constraintsForBlockContent(rootRenderer(), relayoutChildren);
        auto result = layoutRootContent(constraints);
        // align-content moves the content as a unit in the block size the root gets from that of its content, where it is laid out again, but
        // not in a root at least as tall as a page, like the render tree does (RenderBlockFlow::layoutBlock).
        if (!rootRenderer().style().alignContent().isNormal()) {
            auto intrinsicBorderBoxBlockSize = rootRenderer().borderAndPaddingLogicalHeight() + rootRenderer().scrollbarLogicalHeight() + result.intrinsicContentBlockSize;
            auto borderBoxBlockSize = rootRenderer().computeLogicalHeight(intrinsicBorderBoxBlockSize, rootRenderer().logicalTop()).extent;
            auto isAligned = !fragmentation || borderBoxBlockSize < fragmentation->fragmentainerBlockSize;
            if (auto shift = isAligned ? Layout::BlockFormattingUtils::alignContentShift(rootRenderer(), borderBoxBlockSize - intrinsicBorderBoxBlockSize) : 0_lu) {
                blockLayoutState = initialBlockLayoutState;
                if (renderTreeLegacyLineClamp)
                    renderTreeLayoutState->setLegacyLineClamp(renderTreeLegacyLineClamp);
                result = layoutRootContent({ { constraints.horizontal(), constraints.logicalTop() + shift }, constraints.contentBoxBlockSize(), constraints.fillAvailableBlockSize(), constraints.replacedElementAvailableBlockSize(), constraints.relayoutChildren() });
            }
        }
        LOG_WITH_STREAM(Layout, stream << "BlockLayout for root (" << &rootRenderer() << "): " << result);
        if (auto lineClamp = blockLayoutState.lineClamp(); lineClamp && !lineClamp->isLegacy)
            renderTreeLayoutState->setLineClamp(RenderLayoutState::LineClamp { lineClamp->maximumLines, lineClamp->shouldDiscardOverflow, blockLayoutState.clampAfterBox(), lineClamp->isStableTextWrap, lineClamp->hasConstrainedTextWrapContent, lineClamp->shouldTrimEndAtClampPoint });
        else if (renderTreeLineClamp)
            renderTreeLayoutState->setLineClamp(renderTreeLineClamp);
        hasAutoScrollbarChanges |= updateRenderers(result, paintInvalidation, constraints.relayoutChildren() ? RelayoutChildren::Yes : RelayoutChildren::No);
        auto boxFragments = WTF::map(result.fragmentedBoxes, [](auto& box) {
            return std::pair { box.renderer, LocalFrameViewLayoutContext::BoxFragments { box.clonesDecorations, false, WTF::map(box.fragments, [](auto& fragment) {
                return LocalFrameViewLayoutContext::BlockFragment { fragment.blockOffset, fragment.blockSize, fragment.fragmentainerPosition };
            }), { } } };
        });
        for (auto& [box, fragmentainerPosition] : result.boxesPastColumnEnds)
            boxFragments.append({ box, LocalFrameViewLayoutContext::BoxFragments { false, false, { }, fragmentainerPosition } });
        layoutContext->setBoxFragments(rootRenderer(), WTF::move(boxFragments), fragmentation && fragmentation->type == Layout::FragmentainerConstraints::Type::Column);
        rootRenderer().m_floatingObjects = nullptr;
        if (blockLayoutState.placedFloats() || pagesHaveFloats)
            layoutContext->addRootWithFloatsPlacedByBlockFormattingContext(rootRenderer());
        return result;
    };
    auto result = layoutContentOnce({ });

    auto lastFormattedLineRoots = lastFormattedLineRootsForTrimEnd(&rootRenderer(), result);
    if (!lastFormattedLineRoots.isEmpty())
        result = layoutContentOnce(Layout::BlockLayoutState::LastFormattedLineRoots { lastFormattedLineRoots });
    // RenderBlock::layout lays a scroll container out again, once, when its auto scrollbars come or go (RelayoutScopeForScrollbarChange). The
    // ones the block formatting context lays out take their new scrollbars when it lays out the content again, and the ones in it keep theirs
    // then. So do the boxes it gave a size from their content, which they change (SubtreeScrollbarChangesHandler).
    for (auto& change : integrationUtils.takeScrollbarChangesForContentSizes()) {
        if (change.sizesAffectedFromScrollbarChanges.contains(LogicalBoxAxis::Block)) {
            change.renderer->setNeedsLayout(MarkingBehavior::OnlyThis);
            change.renderer->markContainingBlocksForLayout(&rootRenderer());
        }
        if (change.sizesAffectedFromScrollbarChanges.contains(LogicalBoxAxis::Inline))
            change.renderer->invalidateContentLogicalWidths(MarkingBehavior::ContainingBlockChain, &rootRenderer());
        hasAutoScrollbarChanges = true;
    }
    if (hasAutoScrollbarChanges)
        result = layoutContentOnce(WTF::move(lastFormattedLineRoots));
    layoutContext->setTextBoxTrim(renderTreeTextBoxTrim);

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
    auto nestedBlockLayoutCoverageScope = NestedBlockLayoutCoverageScope { };
    auto paintInvalidation = PaintInvalidation { };
    auto integrationUtils = BlockIntegrationUtils { paintInvalidation };
    auto blockLayoutState = Layout::BlockLayoutState { { } };
    // A fieldset's legend, which sits in its border and not in its content, floors them (RenderBlock::computeBlockIntrinsicLogicalWidths). This
    // takes the legend out of the content too, like RenderBlock::layoutExcludedChildren.
    auto [legendMinimum, legendMaximum] = rootRenderer.computeIntrinsicLogicalWidthsForFieldsetLegend();
    auto sizes = Layout::BlockFormattingContext { rootRenderer, integrationUtils, blockLayoutState }.computeIntrinsicInlineSizes(rootRenderer, contentBoxBlockSizeForPercentagesWhileMeasured(rootRenderer));
    sizes.minimum = std::max(sizes.minimum, legendMinimum);
    sizes.maximum = std::max(sizes.maximum, legendMaximum);
    // The multi-column flow of a multi-column container, which is out of its content too, adds its contributions like a child, which the
    // container then spreads across its columns (RenderBlock::computeBlockIntrinsicLogicalWidths, RenderBlockFlow::adjustIntrinsicLogicalWidthsForColumns).
    if (CheckedPtr multiColumnFlow = rootRenderer.multiColumnFlow()) {
        sizes.minimum = std::max(sizes.minimum, multiColumnFlow->minContentLogicalWidthContribution());
        sizes.maximum = std::max(sizes.maximum, multiColumnFlow->maxContentLogicalWidthContribution());
    }

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

void BlockLayout::invalidateIntrinsicInlineSizesIfPercentageBasisChanged(RenderBlockFlow& rootRenderer)
{
    if (!intrinsicInlineSizesDependOnContainerGivenBlockSize(rootRenderer) || !canUseForIntrinsicInlineSizes(rootRenderer))
        return;
    CheckedRef layoutContext = rootRenderer.view().frameView().layoutContext();
    auto percentageBasis = contentBoxBlockSizeForPercentagesWhileMeasured(rootRenderer);
    auto lastPercentageBasis = layoutContext->percentageBasisAtLastLayout(rootRenderer);
    layoutContext->setPercentageBasisAtLastLayout(rootRenderer, percentageBasis);
    if (lastPercentageBasis && *lastPercentageBasis == percentageBasis)
        return;
    rootRenderer.invalidateContentLogicalWidths();
}

void BlockLayout::prepareForRenderTreeLayout(RenderBlockFlow& rootRenderer)
{
    // The render tree paints the boxes it lays out as one fragment.
    rootRenderer.view().frameView().layoutContext().setBoxFragments(rootRenderer, { }, false);
    // The render tree's block layout takes the floats of the boxes it does not lay out again from its float lists, which the block formatting
    // context leaves empty (updateRenderers): it lays out all of the block formatting context the floats were placed in again.
    if (!rootRenderer.view().frameView().layoutContext().removeRootWithFloatsPlacedByBlockFormattingContext(rootRenderer))
        return;
    for (CheckedPtr<RenderObject> descendant = rootRenderer.firstChild(); descendant;) {
        if (CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(*descendant); blockContainer && !blockContainer->isFloatingOrOutOfFlowPositioned() && !blockContainer->createsNewFormattingContext()) {
            blockContainer->setChildNeedsLayout(MarkingBehavior::OnlyThis);
            descendant = descendant->nextInPreOrder(&rootRenderer);
            continue;
        }
        descendant = descendant->isInlineBox() ? descendant->nextInPreOrder(&rootRenderer) : descendant->nextInPreOrderAfterChildren(&rootRenderer);
    }
}

// The line grid of the content of a block container in a layout state of its own, which it pushes when there is a line grid around it
// (LocalFrameViewLayoutContext::pushLayoutState).
static std::optional<Layout::LineGridInLayoutState> lineGridInLayoutStateOf(const RenderBlockFlow& blockContainer)
{
    auto lineGrid = BlockIntegrationUtils::lineGridForRoot(blockContainer);
    if (!lineGrid.lineGrid)
        return { };
    auto inLayoutState = [&](LayoutSize offset) {
        return blockContainer.writingMode().isHorizontal() ? offset : offset.transposedSize();
    };
    auto& [gridContainer, outsideRoot] = *lineGrid.lineGrid;
    auto gridOffset = lineGrid.borderBoxOffsetInLayoutState + (outsideRoot ? outsideRoot->borderBoxOffset : LayoutSize { });
    return Layout::LineGridInLayoutState { { { gridContainer, inLayoutState(gridOffset) } }, inLayoutState(lineGrid.borderBoxOffsetInLayoutState) };
}

void BlockLayout::layoutOutOfFlowBoxes(const RenderBlock& containingBlock, RelayoutChildren relayoutChildren)
{
    auto nestedBlockLayoutCoverageScope = NestedBlockLayoutCoverageScope { };
    // A containing block that lays out only its out-of-flow boxes does it in its own layout state (RenderBlock::simplifiedLayout).
    if (CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(containingBlock); blockContainer && blockContainer != &rootRenderer()) {
        if (auto lineGrid = lineGridInLayoutStateOf(*blockContainer))
            m_outOfFlowContainingBlockLineGrids.append({ *blockContainer, WTF::move(*lineGrid) });
    }
    auto paintInvalidation = PaintInvalidation { };
    layoutOutOfFlowBoxes(containingBlock, paintInvalidation, relayoutChildren);
}

// The root of the block formatting context a block container the block formatting context laid out takes part in.
static const RenderBlockFlow& blockFormattingContextRootOf(const RenderBlockFlow& blockContainer)
{
    if (blockContainer.createsNewFormattingContext())
        return blockContainer;
    CheckedPtr containingBlock = dynamicDowncast<RenderBlockFlow>(blockContainer.containingBlock());
    return containingBlock ? blockFormattingContextRootOf(*containingBlock) : blockContainer;
}

// The padding box of a block in the coordinates its children are positioned in (RenderBox::setLogicalLocation): from its left edge (top in
// vertical writing modes) in the inline axis, and from its block-start edge in the block axis, which is where the render tree measures block
// positions from in a flipped block flow direction too (RenderBox::blockStartRelativeToPhysicalForChild).
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

// How a flex container aligns an out-of-flow child in its content box in its inline and block axes, as if it were its only flex item
// (css-flexbox-1 §4.1): justify-content in the main axis and align-self in the cross axis, like FlexLayout::staticMainAxisPositionForPositionedFlexItem
// and staticCrossAxisPositionForPositionedFlexItem.
using StaticPositionAlignment = Layout::OutOfFlowGeometry::StaticPosition::Alignment;
static std::pair<StaticPositionAlignment, StaticPositionAlignment> flexStaticPositionAlignments(const RenderFlexibleBox& flexBox, const RenderBox& outOfFlowBox)
{
    using Position = StaticPositionAlignment::Position;
    CheckedRef style = flexBox.style();
    auto isColumn = style->isColumnFlexDirection();
    auto mainAxisAlignment = [&] -> StaticPositionAlignment {
        auto rectangleSize = isColumn ? flexBox.contentBoxLogicalHeight() : flexBox.contentBoxLogicalWidth();
        auto justifyContent = style->justifyContent().resolve(FlexFormattingUtils::contentAlignmentNormalBehavior());
        // With no other flex items, space-around and space-evenly center it, overflowing or not.
        if (justifyContent.distribution() == ContentDistribution::SpaceAround || justifyContent.distribution() == ContentDistribution::SpaceEvenly)
            return { rectangleSize, Position::Center };
        // A reversed flex direction swaps the main-start and main-end edges, not start and end.
        auto isReverse = style->isReverseFlexDirection();
        auto isSafe = justifyContent.overflow() == OverflowAlignment::Safe;
        switch (FlexFormattingUtils::resolveLeftRightAlignment(justifyContent, style, isReverse)) {
        case ContentPosition::Center:
            return { rectangleSize, Position::Center, isSafe };
        case ContentPosition::Start:
            return { rectangleSize, Position::Start, isSafe };
        case ContentPosition::End:
            return { rectangleSize, Position::End, isSafe };
        case ContentPosition::FlexEnd:
            return { rectangleSize, isReverse ? Position::Start : Position::End, isSafe };
        default:
            return { rectangleSize, isReverse ? Position::End : Position::Start, isSafe };
        }
    };
    auto crossAxisAlignment = [&] -> StaticPositionAlignment {
        auto rectangleSize = isColumn ? flexBox.contentBoxLogicalWidth() : flexBox.contentBoxLogicalHeight();
        auto isSafe = outOfFlowBox.style().alignSelf().resolve(style.ptr()).overflow() == OverflowAlignment::Safe;
        // wrap-reverse makes the cross-start edge the end one, which alignmentForFlexItem accounts for in flex-start and flex-end.
        switch (FlexFormattingUtils::alignmentForFlexItem(outOfFlowBox)) {
        case ItemPosition::Center:
        case ItemPosition::AnchorCenter:
            return { rectangleSize, Position::Center, isSafe };
        case ItemPosition::FlexEnd:
        // Without a baseline-sharing group, last baseline alignment falls back to end alignment (css-align-3 §4.2).
        case ItemPosition::LastBaseline:
            return { rectangleSize, Position::End, isSafe };
        case ItemPosition::Stretch:
            return { rectangleSize, FlexFormattingUtils::isWrapReverse(flexBox) ? Position::End : Position::Start, isSafe };
        default:
            return { rectangleSize, Position::Start, isSafe };
        }
    };
    if (isColumn)
        return { crossAxisAlignment(), mainAxisAlignment() };
    return { mainAxisAlignment(), crossAxisAlignment() };
}

// How a grid container aligns an out-of-flow child whose containing block is outside it in the block axis: in its content box, by an align-self
// that centers it or puts it at the end, where it fits, only when all three are horizontal and left to right (PositionedLayoutConstraints::remainingSpaceForStaticAlignment).
static std::optional<StaticPositionAlignment> gridStaticPositionBlockAlignment(const RenderGrid& grid, const RenderBox& outOfFlowBox, const RenderBlock& containingBlock)
{
    auto isHorizontalLeftToRight = [](const RenderElement& renderer) {
        auto writingMode = renderer.writingMode();
        return writingMode.isHorizontal() && writingMode.isBidiLTR();
    };
    if (!isHorizontalLeftToRight(containingBlock) || !isHorizontalLeftToRight(grid) || !isHorizontalLeftToRight(outOfFlowBox))
        return { };
    auto alignSelf = outOfFlowBox.style().alignSelf().resolve(&grid.style());
    if (alignSelf.positionType() != ItemPositionType::NonLegacy || alignSelf.overflow() != OverflowAlignment::Default)
        return { };
    switch (alignSelf.position()) {
    case ItemPosition::Center:
        return StaticPositionAlignment { grid.contentBoxLogicalHeight(), StaticPositionAlignment::Position::Center, true };
    case ItemPosition::FlexEnd:
    case ItemPosition::SelfEnd:
    case ItemPosition::End:
        return StaticPositionAlignment { grid.contentBoxLogicalHeight(), StaticPositionAlignment::Position::End, true };
    default:
        return { };
    }
}

// A point in the border box of a box from the coordinates its children are positioned in (logicalPaddingBoxRect) to physical ones, and back.
static LayoutPoint physicalPoint(const RenderBox& box, LayoutPoint logicalPoint)
{
    auto writingMode = box.writingMode();
    auto borderBoxSize = box.borderBoxSize();
    if (writingMode.isHorizontal())
        return { logicalPoint.x(), writingMode.isBlockFlipped() ? borderBoxSize.height() - logicalPoint.y() : logicalPoint.y() };
    return { writingMode.isBlockFlipped() ? borderBoxSize.width() - logicalPoint.y() : logicalPoint.y(), logicalPoint.x() };
}

static LayoutPoint logicalPoint(const RenderBox& box, LayoutPoint physicalPoint)
{
    auto writingMode = box.writingMode();
    auto borderBoxSize = box.borderBoxSize();
    if (writingMode.isHorizontal())
        return { physicalPoint.x(), writingMode.isBlockFlipped() ? borderBoxSize.height() - physicalPoint.y() : physicalPoint.y() };
    return { physicalPoint.y(), writingMode.isBlockFlipped() ? borderBoxSize.width() - physicalPoint.x() : physicalPoint.x() };
}

// The box aligns to the start of the static-position rectangle in the writing mode of the static-position containing block (css-position-3
// §3.1), which is the end of the containing block's range in an axis where that start runs the other way.
static bool startsAtEnd(WritingMode staticPositionWritingMode, FlowDirection containingBlockDirection)
{
    auto isHorizontal = [](FlowDirection direction) {
        return direction == FlowDirection::LeftToRight || direction == FlowDirection::RightToLeft;
    };
    auto inlineDirection = staticPositionWritingMode.inlineDirection();
    auto direction = isHorizontal(inlineDirection) == isHorizontal(containingBlockDirection) ? inlineDirection : staticPositionWritingMode.blockDirection();
    return direction != containingBlockDirection;
}

// The static position the in-flow layout of its parent gave the box (RenderLayer::staticInlinePosition, staticBlockPosition), relative to the
// border box of its block container, moved through the physical positions of the boxes in between, relative offsets included (like
// PositionedLayoutConstraints::computedInlineStaticDistance), to the padding box of its containing block, whose writing mode they need not have.
static Layout::OutOfFlowGeometry::StaticPosition staticPosition(const RenderBox& outOfFlowBox, const RenderBlock& containingBlock)
{
    CheckedPtr<const RenderElement> staticPositionBlock = outOfFlowBox.parent();
    while (staticPositionBlock->isInlineBox())
        staticPositionBlock = staticPositionBlock->parent();
    CheckedRef blockContainer = downcast<RenderBlock>(*staticPositionBlock);
    auto staticPositionWritingMode = blockContainer->writingMode();
    // A flex container's is the start of its content box, which the layer has until the render tree path aligns the box there
    // (FlexLayout::setStaticPositionForPositionedLayout).
    CheckedPtr flexBox = dynamicDowncast<RenderFlexibleBox>(blockContainer);
    CheckedRef layer = *outOfFlowBox.layer();
    auto staticInlinePosition = flexBox ? flexBox->startOffsetForContent() : layer->staticInlinePosition();
    auto staticBlockPosition = flexBox ? flexBox->borderAndPaddingBefore() : layer->staticBlockPosition();
    auto position = physicalPoint(blockContainer, { staticPositionWritingMode.isLogicalLeftInlineStart() ? staticInlinePosition : blockContainer->logicalWidth() - staticInlinePosition, staticBlockPosition });
    // Each box is positioned in the next one container() goes to, from its block-start edge (RenderBox::blockStartRelativeToPhysicalForChild),
    // and an out-of-flow box between them from the border box of the first fragment of a positioned inline box
    // (RenderBoxModelObject::blockStartRelativeFirstFragmentBorderBoxRect).
    auto nextBox = [](const RenderElement& renderer) -> const RenderBox* {
        auto* container = renderer.container();
        while (!is<RenderBox>(*container))
            container = container->container();
        return downcast<RenderBox>(container);
    };
    for (CheckedPtr<const RenderBox> box = blockContainer.ptr(); box != &containingBlock; box = nextBox(*box)) {
        auto location = box->location();
        if (CheckedPtr inlineBox = box->isOutOfFlowPositioned() ? dynamicDowncast<RenderBoxModelObject>(box->container()) : nullptr; inlineBox && inlineBox->isInlineBox())
            location.moveBy(inlineBox->blockStartRelativeFirstFragmentBorderBoxRect().location());
        CheckedRef container = *nextBox(*box);
        if (container->writingMode().isBlockFlipped()) {
            if (container->isHorizontalWritingMode())
                location.setY(container->borderBoxHeight() - box->borderBoxHeight() - location.y());
            else
                location.setX(container->borderBoxWidth() - box->borderBoxWidth() - location.x());
        }
        position.moveBy(location);
        position.move(box->offsetForInFlowPosition());
    }
    auto writingMode = containingBlock.writingMode();
    position = logicalPoint(containingBlock, position);
    auto paddingBox = logicalPaddingBoxRect(containingBlock);
    auto inlinePosition = writingMode.isLogicalLeftInlineStart() ? position.x() - paddingBox.x() : paddingBox.maxX() - position.x();
    auto result = Layout::OutOfFlowGeometry::StaticPosition {
        .inlinePosition = inlinePosition,
        .blockPosition = position.y() - paddingBox.y(),
        // In the writing mode of its parent, an inline box with a direction of its own included (PositionedLayoutConstraints::computeStaticPosition).
        .inlinePositionIsMarginBoxEnd = startsAtEnd(outOfFlowBox.parent()->writingMode(), writingMode.inlineDirection()),
        .blockPositionIsMarginBoxEnd = startsAtEnd(outOfFlowBox.parent()->writingMode(), writingMode.blockDirection())
    };
    if (flexBox) {
        std::tie(result.inlineAlignment, result.blockAlignment) = flexStaticPositionAlignments(*flexBox, outOfFlowBox);
        // Its inline axis is the containing block's block axis in an orthogonal flow.
        if (staticPositionWritingMode.isOrthogonal(writingMode))
            std::swap(result.inlineAlignment, result.blockAlignment);
    } else if (CheckedPtr grid = dynamicDowncast<RenderGrid>(blockContainer))
        result.blockAlignment = gridStaticPositionBlockAlignment(*grid, outOfFlowBox, containingBlock);
    return result;
}

// The static-position rectangle in the inline axis that justify-self aligns a box whose parent is a block container in, from the static position
// relative to the containing block the box is laid out in, and the writing mode that resolves the alignment (css-position-3 §4.2). Like the render
// tree, an originally inline-level box and one orthogonal to its parent align from the static position without one, the latter in the writing mode
// of the containing block (PositionedLayoutConstraints::computeStaticPosition, staticPositionContainingBlockWritingMode).
static void setInlineStaticPositionRectangle(Layout::OutOfFlowGeometry::StaticPosition& staticPosition, const RenderBox& outOfFlowBox, const Layout::OutOfFlowContainingBlock& containingBlock)
{
    CheckedPtr parent = dynamicDowncast<RenderBox>(outOfFlowBox.parent());
    if (!parent || !parent->isBlockContainer())
        return;
    // A box in the top layer is laid out as if it were a sibling of the root element, in the initial containing block, or the viewport, which
    // has its size, when it is fixed positioned.
    if (RefPtr element = outOfFlowBox.element(); element && element->isInTopLayer()) [[unlikely]] {
        staticPosition.inlineRectangle = std::pair { 0_lu, containingBlock.inlineSize };
        return;
    }
    auto parentWritingMode = parent->writingMode();
    if (parentWritingMode.isOrthogonal(outOfFlowBox.writingMode()) || parentWritingMode.isOrthogonal(containingBlock.writingMode))
        return;
    staticPosition.inlineAlignmentWritingMode = parentWritingMode;
    if (outOfFlowBox.style().originalDisplay().isInlineType())
        return;
    auto size = parent->contentBoxLogicalWidth();
    auto start = staticPosition.inlinePositionIsMarginBoxEnd ? staticPosition.inlinePosition - size : staticPosition.inlinePosition;
    staticPosition.inlineRectangle = std::pair { start, start + size };
}

// The border box of the default anchor of a box that position-area or anchor-center positions against it, from the anchor's geometry in the
// render tree, which is committed by the time the containing block lays out its out-of-flow boxes (PositionedLayoutConstraints::captureAnchorGeometry).
static std::optional<Layout::OutOfFlowGeometry::DefaultAnchor> defaultAnchor(const RenderBox& outOfFlowBox, const RenderBlock& containingBlock)
{
    if (!Style::AnchorPositionEvaluator::isLayoutTimeAnchorPositioned(outOfFlowBox.style()))
        return { };
    CheckedPtr anchor = Style::AnchorPositionEvaluator::defaultAnchorForBox(outOfFlowBox);
    if (!anchor)
        return { };
    // Physical and relative to the border box of the containing block, scrolled to the start, but for a scrollbar on the left, which it takes
    // off for PositionedLayoutConstraints, whose containing block starts at the border.
    auto anchorRect = Style::AnchorPositionEvaluator::computeAnchorRectRelativeToContainingBlock(*anchor, containingBlock, outOfFlowBox);
    if (containingBlock.shouldPlaceVerticalScrollbarOnLeft())
        anchorRect.move(containingBlock.verticalScrollbarWidth(), 0);
    auto writingMode = containingBlock.writingMode();
    auto borderBoxSize = containingBlock.borderBoxSize();
    if (!writingMode.isHorizontal()) {
        anchorRect = anchorRect.transposedRect();
        borderBoxSize = borderBoxSize.transposedSize();
    }
    if (writingMode.isBlockFlipped())
        anchorRect.setY(borderBoxSize.height() - anchorRect.maxY());
    auto paddingBox = logicalPaddingBoxRect(containingBlock);
    auto inlineStart = writingMode.isLogicalLeftInlineStart() ? anchorRect.x() - paddingBox.x() : paddingBox.maxX() - anchorRect.maxX();
    return Layout::OutOfFlowGeometry::DefaultAnchor { inlineStart, anchorRect.width(), anchorRect.y() - paddingBox.y(), anchorRect.height() };
}

// The padding box of a scroll container extended to its scrollable overflow (css-position-4 §2.2), relative to its padding box. In the box's
// coordinates, which have the block start at the top or the left.
static std::optional<LayoutRect> scrollableContainingBlock(const RenderBlock& containingBlock)
{
    if (!containingBlock.hasRenderOverflow() || !containingBlock.hasPotentiallyScrollableOverflow())
        return { };
    auto rect = containingBlock.scrollablePaddingAreaOverflowRect();
    auto writingMode = containingBlock.writingMode();
    if (!writingMode.isHorizontal())
        rect = rect.transposedRect();
    auto paddingBox = logicalPaddingBoxRect(containingBlock);
    auto inlineStart = writingMode.isLogicalLeftInlineStart() ? rect.x() - paddingBox.x() : paddingBox.maxX() - rect.maxX();
    return LayoutRect { inlineStart, rect.y() - paddingBox.y(), rect.width(), rect.height() };
}

// The layout viewport and the scrollable overflow of the view, moved by how far the view is scrolled (PositionedLayoutConstraints::expandToScrollableArea),
// relative to the layout viewport.
static LayoutRect fixedPositionOverflowArea(const RenderView& renderView, const Layout::OutOfFlowContainingBlock& layoutViewport)
{
    auto writingMode = renderView.writingMode();
    auto area = renderView.scrollablePaddingAreaOverflowRect();
    area.uniteEvenIfEmpty(writingMode.isHorizontal() ? LayoutRect { 0_lu, 0_lu, layoutViewport.inlineSize, layoutViewport.blockSize } : LayoutRect { 0_lu, 0_lu, layoutViewport.blockSize, layoutViewport.inlineSize });
    CheckedRef frameView = renderView.frameView();
    area.moveBy(-frameView->constrainedScrollPosition(ScrollPosition(frameView->scrollPositionRespectingCustomFixedPosition())));
    if (!writingMode.isHorizontal())
        area = area.transposedRect();
    if (!writingMode.isLogicalLeftInlineStart())
        area.setX(layoutViewport.inlineSize - area.maxX());
    return area;
}

// The line grid of the content of the containing block, which the layout state of the root has for the root (RenderLayoutState::lineGrid).
std::optional<Layout::LineGridInLayoutState> BlockLayout::lineGridOfOutOfFlowContainingBlock(const RenderBlock& containingBlock) const
{
    auto index = m_outOfFlowContainingBlockLineGrids.findIf([&](auto& entry) {
        return entry.first.ptr() == &containingBlock;
    });
    if (index != notFound)
        return m_outOfFlowContainingBlockLineGrids[index].second;
    if (&containingBlock != &rootRenderer())
        return { };
    return lineGridInLayoutStateOf(rootRenderer());
}

// Like RenderBlock::layoutOutOfFlowBoxes once the containing block has its size: the out-of-flow boxes BlockLayout::updateRenderers registered
// on it (RenderBlock::outOfFlowBoxes()).
void BlockLayout::layoutOutOfFlowBoxes(const RenderBlock& containingBlock, PaintInvalidation& paintInvalidation, RelayoutChildren relayoutChildren)
{
    auto* outOfFlowBoxes = containingBlock.outOfFlowBoxes();
    // content-visibility skips the ones in a box whose content it skips (RenderBlock::layoutOutOfFlowBox), which the commit leaves as they are.
    if (!outOfFlowBoxes || containingBlock.layoutContext().isSkippedContentRootForLayout(containingBlock))
        return;
    auto writingMode = containingBlock.writingMode();
    auto paddingBox = logicalPaddingBoxRect(containingBlock);
    auto paddingBoxInlineStart = writingMode.isLogicalLeftInlineStart() ? paddingBox.x() : containingBlock.logicalWidth() - paddingBox.maxX();
    auto paddingBoxContainingBlock = Layout::OutOfFlowContainingBlock { writingMode, containingBlock.hasPotentiallyScrollableOverflow(), paddingBoxInlineStart, paddingBox.y(), paddingBox.width(), paddingBox.height(), scrollableContainingBlock(containingBlock) };
    // The containing block a positioned inline box forms (css-position-3 §2.1): from the start of the padding box of its first fragment to
    // the end of the padding box of its last one in the inline axis, which is the left one of the last line in right-to-left, and the padding
    // box of the bounding box of its fragments in the block axis (RenderBox::containingBlockRangeForPositioned, PositionedLayoutConstraints::fixupLogicalLeftPosition).
    auto inlineBoxContainingBlock = [&](const RenderBoxModelObject& inlineBox) {
        // The fragments are in the block container whose inline content the inline box is in, which may be an anonymous block inside the
        // containing block.
        CheckedRef inlineContentRoot = *inlineBox.containingBlock();
        auto inlineContentRootOffset = LayoutSize { };
        for (CheckedPtr<const RenderBlock> block = inlineContentRoot.ptr(); block && block != &containingBlock; block = block->containingBlock())
            inlineContentRootOffset += LayoutSize { block->logicalLeft(), block->logicalTop() };
        auto firstFragment = inlineBox.firstFragmentBorderBoxRect();
        if (writingMode.isBlockFlipped())
            inlineContentRoot->physicalToBlockStartRelative(firstFragment);
        auto firstFragmentLogicalTopLeft = writingMode.isHorizontal() ? firstFragment.location() : firstFragment.location().transposedPoint();
        auto leftFragmentLogicalLeft = firstFragmentLogicalTopLeft.x();
        if (!writingMode.isLogicalLeftInlineStart()) {
            if (auto fragment = InlineIterator::lineLeftmostInlineBoxFor(inlineBox)) {
                auto firstFragmentLeft = fragment->logicalLeftIgnoringInlineDirection();
                while (auto next = std::ranges::next(fragment))
                    fragment = next;
                // From the first fragment, by the distance to the last one, which the render tree rounds down with the position of the box in it.
                leftFragmentLogicalLeft += LayoutUnit::fromFloatFloor(fragment->logicalLeftIgnoringInlineDirection() - firstFragmentLeft);
            }
        }
        auto inlineSize = inlineBox.paddingBoxLogicalWidth();
        auto logicalLeft = inlineContentRootOffset.width() + leftFragmentLogicalLeft + inlineBox.borderLogicalLeft();
        auto inlineStart = writingMode.isLogicalLeftInlineStart() ? logicalLeft : containingBlock.logicalWidth() - logicalLeft - inlineSize;
        auto blockStart = inlineContentRootOffset.height() + firstFragmentLogicalTopLeft.y() + inlineBox.borderBefore();
        return Layout::OutOfFlowContainingBlock { writingMode, false, inlineStart, blockStart, inlineSize, inlineBox.paddingBoxLogicalHeight() };
    };
    // Where the border box of a replaced element is in the block that keeps the out-of-flow boxes inside it.
    auto replacedElementOffset = [&](const RenderReplaced& replaced) {
        auto offset = LayoutSize { replaced.logicalLeft(), replaced.logicalTop() };
        for (CheckedPtr<const RenderBlock> block = replaced.containingBlock(); block && block != &containingBlock; block = block->containingBlock())
            offset += LayoutSize { block->logicalLeft(), block->logicalTop() };
        return offset;
    };
    // The containing block a positioned replaced element with content of its own forms (a drawable canvas): its padding box, from where it is
    // in the block that keeps the box (RenderBox::containingBlockRangeForPositioned).
    auto replacedElementContainingBlock = [&](const RenderReplaced& replaced) {
        auto offset = replacedElementOffset(replaced);
        auto inlineSize = replaced.paddingBoxLogicalWidth();
        auto logicalLeft = offset.width() + replaced.borderLogicalLeft();
        auto inlineStart = writingMode.isLogicalLeftInlineStart() ? logicalLeft : containingBlock.logicalWidth() - logicalLeft - inlineSize;
        return Layout::OutOfFlowContainingBlock { writingMode, false, inlineStart, offset.height() + replaced.borderBefore(), inlineSize, replaced.paddingBoxLogicalHeight() };
    };
    auto integrationUtils = BlockIntegrationUtils { paintInvalidation };
    auto blockLayoutState = Layout::BlockLayoutState { { } };
    auto containingBlockLineGrid = lineGridOfOutOfFlowContainingBlock(containingBlock);
    // In content the root breaks into fragmentainers, the content of an out-of-flow block container fragments across the ones its containing
    // block is in (css-break-3 §5.5), from where the box starts in the strip, like the root's content (BlockLayout::layout), except inside a
    // box that cannot be split, which the render tree does not paginate (RenderLayoutState::computePaginationInformation).
    auto fragmentation = fragmentationForRoot(rootRenderer(), containingBlock.view().frameView().layoutContext().layoutState());
    // From where the root starts in the columns of the multi-column flow the render tree lays it out in.
    auto containingBlockStripLogicalTop = fragmentation && !is<RenderMultiColumnFlow>(rootRenderer()) && columnFlowOf(rootRenderer(), *fragmentation) ? rootRenderer().offsetFromLogicalTopOfFirstPage() : 0_lu;
    for (CheckedPtr<const RenderBlock> block = &containingBlock; fragmentation && block != &rootRenderer(); block = block->containingBlock()) {
        if (block->isUnsplittableForPagination())
            fragmentation = std::nullopt;
        containingBlockStripLogicalTop += block->logicalTop();
    }
    auto fragmentedContentLayout = Layout::BlockFormattingContext::FragmentedOutOfFlowContentLayout { [&](const RenderBlockFlow& box, const Layout::BlockLayoutState& initialBlockLayoutState, const Layout::ConstraintsForBlockContent& constraints, const Layout::BoxGeometry& geometry, const Layout::RootLineGrid& lineGrid, LayoutUnit borderBoxBlockStart) {
        return layoutAcrossFragmentainers(rootRenderer(), *fragmentation, integrationUtils, box, initialBlockLayoutState, constraints, geometry, lineGrid, containingBlockStripLogicalTop + borderBoxBlockStart, false);
    } };
    // And so does the content of the block formatting context roots with a block size of their own in it.
    auto rootContentLayout = BlockIntegrationUtils::FragmentedContentLayout { [&](const RenderBlockFlow& box, const Layout::BlockLayoutState& initialBlockLayoutState, const Layout::ConstraintsForBlockContent& constraints, const Layout::BoxGeometry& geometry, const Layout::RootLineGrid& lineGrid, LayoutUnit borderBoxStripLogicalTop, bool appliesForcedBreaks) {
        return layoutAcrossFragmentainers(rootRenderer(), *fragmentation, integrationUtils, box, initialBlockLayoutState, constraints, geometry, lineGrid, borderBoxStripLogicalTop, appliesForcedBreaks);
    } };
    if (fragmentation)
        integrationUtils.setFragmentedContentLayout(&rootContentLayout);
    // Laying out a box registers the out-of-flow boxes in it, which can make the list grow while it is iterated (like in RenderBlock::layoutOutOfFlowBoxes).
    for (CheckedRef outOfFlowBox : *outOfFlowBoxes) {
        // Like RenderElement::layoutIfNeeded, which captures how far scrolling between an anchor-positioned box and its default anchor moves
        // it when it lays the box out. Capturing again compares with the previous capture, which tells style resolution to resolve anchor()
        // and position-visibility again. The block formatting context places a box that position-area or anchor-center positions again also
        // when it does not need layout, from where its default anchor is now, which the capture has to follow.
        CheckedRef outOfFlowBoxStyle = outOfFlowBox->style();
        auto capturesScrollSnapshots = Style::AnchorPositionEvaluator::isAnchorPositioned(outOfFlowBoxStyle) && (outOfFlowBox->needsLayout() || Style::AnchorPositionEvaluator::isLayoutTimeAnchorPositioned(outOfFlowBoxStyle));
        auto outOfFlowContainingBlock = paddingBoxContainingBlock;
        if (fragmentation)
            outOfFlowContainingBlock.borderBoxStripLogicalTop = containingBlockStripLogicalTop;
        auto boxStaticPosition = staticPosition(outOfFlowBox, containingBlock);
        auto boxDefaultAnchor = defaultAnchor(outOfFlowBox, containingBlock);
        auto flipsInlineAxis = false;
        if (CheckedPtr container = outOfFlowBox->container(); container && (container->isInlineBox() || is<RenderReplaced>(*container))) {
            if (CheckedPtr replaced = dynamicDowncast<RenderReplaced>(*container))
                outOfFlowContainingBlock = replacedElementContainingBlock(*replaced);
            else
                outOfFlowContainingBlock = inlineBoxContainingBlock(downcast<RenderBoxModelObject>(*container));
            // The static position and the default anchor are relative to the padding box of the block.
            auto inlineOffset = outOfFlowContainingBlock.inlineStart - paddingBoxContainingBlock.inlineStart;
            auto blockOffset = outOfFlowContainingBlock.blockStart - paddingBoxContainingBlock.blockStart;
            boxStaticPosition.inlinePosition -= inlineOffset;
            boxStaticPosition.blockPosition -= blockOffset;
            if (boxDefaultAnchor) {
                boxDefaultAnchor->inlineStart -= inlineOffset;
                boxDefaultAnchor->blockStart -= blockOffset;
            }
            // One with a direction of its own gives it to the containing block (css-position-3 §2.1, PositionedLayoutConstraints'
            // m_containingWritingMode), which lays the box out from its own inline start, the other side of the block's.
            if (container->writingMode().isInlineOpposing(writingMode)) {
                flipsInlineAxis = true;
                outOfFlowContainingBlock.writingMode = container->writingMode();
                outOfFlowContainingBlock.inlineStart = containingBlock.logicalWidth() - outOfFlowContainingBlock.inlineStart - outOfFlowContainingBlock.inlineSize;
                boxStaticPosition.inlinePosition = outOfFlowContainingBlock.inlineSize - boxStaticPosition.inlinePosition;
                boxStaticPosition.inlinePositionIsMarginBoxEnd = !boxStaticPosition.inlinePositionIsMarginBoxEnd;
                if (boxDefaultAnchor)
                    boxDefaultAnchor->inlineStart = outOfFlowContainingBlock.inlineSize - boxDefaultAnchor->inlineStart - boxDefaultAnchor->inlineSize;
            }
        }
        // The view is the initial containing block of the absolutely positioned boxes it contains, which can overflow it where it scrolls
        // (PositionedLayoutConstraints::containerAllowsInfiniteOverflow), and gives the fixed positioned ones the layout viewport
        // (RenderBox::containingBlockRangeForPositioned), from its left (top in vertical writing modes).
        if (CheckedPtr renderView = dynamicDowncast<RenderView>(containingBlock)) {
            if (outOfFlowBox->isFixedPositioned()) {
                outOfFlowContainingBlock.inlineSize = renderView->clientLogicalWidthForFixedPosition();
                outOfFlowContainingBlock.blockSize = renderView->clientLogicalHeightForFixedPosition();
                if (!writingMode.isLogicalLeftInlineStart()) {
                    outOfFlowContainingBlock.inlineStart = renderView->logicalWidth() - outOfFlowContainingBlock.inlineSize;
                    boxStaticPosition.inlinePosition -= outOfFlowContainingBlock.inlineStart;
                    if (boxDefaultAnchor)
                        boxDefaultAnchor->inlineStart -= outOfFlowContainingBlock.inlineStart;
                }
                if (boxDefaultAnchor && renderView->hasRenderOverflow())
                    outOfFlowContainingBlock.fixedPositionOverflowArea = fixedPositionOverflowArea(*renderView, outOfFlowContainingBlock);
            } else
                outOfFlowContainingBlock.isScrollContainer = true;
        }
        setInlineStaticPositionRectangle(boxStaticPosition, outOfFlowBox, outOfFlowContainingBlock);
        // What -webkit-fill-available resolves to in the block axis: the available block size of the containing block, which is not the
        // inset-modified containing block (RenderBox::computeSizingKeywordLogicalContentHeightUsing), and what the content of a box with an
        // auto block size gets from it (RenderBox::availableLogicalHeightUsing).
        outOfFlowContainingBlock.fillAvailableBlockSize = containingBlock.availableLogicalHeight(AvailableLogicalHeightType::ExcludeMarginBorderPadding);
        outOfFlowContainingBlock.availableBlockSizeIncludingMarginBorderPadding = containingBlock.availableLogicalHeight(AvailableLogicalHeightType::IncludeMarginBorderPadding);
        auto layoutOutOfFlowBox = [&] {
            // Like LineClampUpdater, which keeps -webkit-line-clamp out of an out-of-flow block container.
            auto* renderTreeLayoutState = outOfFlowBox->view().frameView().layoutContext().layoutState();
            auto legacyLineClamp = renderTreeLayoutState && is<RenderBlockFlow>(outOfFlowBox) ? renderTreeLayoutState->legacyLineClamp() : std::nullopt;
            if (legacyLineClamp)
                renderTreeLayoutState->setLegacyLineClamp({ });
            // Nor does the content of an out-of-flow box that cannot be split break, which it lays out whole from where it starts.
            auto breaksContent = fragmentation && !outOfFlowBox->isUnsplittableForPagination();
            auto result = Layout::BlockFormattingContext { rootRenderer(), integrationUtils, blockLayoutState }.layoutOutOfFlowBox(outOfFlowBox, outOfFlowContainingBlock, boxStaticPosition, boxDefaultAnchor, containingBlockLineGrid, breaksContent ? &fragmentedContentLayout : nullptr);
            // Back to the inline start of the block, which the box is last in the result after its content.
            if (flipsInlineAxis) {
                auto& geometry = result.boxes.last().geometry;
                ASSERT(result.boxes.last().renderer.ptr() == outOfFlowBox.ptr());
                geometry.setLeft(containingBlock.logicalWidth() - Layout::BoxGeometry::borderBoxLeft(geometry) - geometry.borderBoxWidth());
                geometry.setHorizontalMargin({ geometry.marginEnd(), geometry.marginStart() });
            }
            // A box in a replaced element is in the coordinates of the replaced element, which positions it (RenderBox::location): from its left,
            // and its inline start in the geometry of right-to-left content.
            if (CheckedPtr replaced = dynamicDowncast<RenderReplaced>(outOfFlowBox->container())) {
                auto& geometry = result.boxes.last().geometry;
                auto offset = replacedElementOffset(*replaced);
                geometry.setLeft(Layout::BoxGeometry::borderBoxLeft(geometry) + (writingMode.isLogicalLeftInlineStart() ? -offset.width() : offset.width()));
                geometry.setTop(Layout::BoxGeometry::borderBoxTop(geometry) - offset.height());
            }
            if (legacyLineClamp)
                renderTreeLayoutState->setLegacyLineClamp(legacyLineClamp);
            LOG_WITH_STREAM(Layout, stream << "BlockLayout for out-of-flow box (" << outOfFlowBox.ptr() << "): " << result);
            auto hasAutoScrollbarChanges = updateRenderers(result, paintInvalidation, relayoutChildren);
            return std::pair { hasAutoScrollbarChanges, WTF::move(result) };
        };
        blockLayoutState.setLastFormattedLineRoots({ });
        auto [hasAutoScrollbarChanges, result] = layoutOutOfFlowBox();
        if (auto lastFormattedLineRoots = lastFormattedLineRootsForTrimEnd(nullptr, result); !lastFormattedLineRoots.isEmpty()) {
            blockLayoutState.setLastFormattedLineRoots(WTF::move(lastFormattedLineRoots));
            hasAutoScrollbarChanges |= layoutOutOfFlowBox().first;
        }
        // Once more when its auto scrollbars, or those of the scroll containers in it, come or go (RelayoutScopeForScrollbarChange).
        if (hasAutoScrollbarChanges)
            layoutOutOfFlowBox();
        if (capturesScrollSnapshots)
            Style::AnchorPositionEvaluator::captureScrollSnapshots(outOfFlowBox);
    }
    SUPPRESS_MEMORY_UNSAFE_CAST CheckedRef mutableContainingBlock = const_cast<RenderBlock&>(containingBlock);
    // The transforms of the boxes that depend on the size of their containing block (offset-path), before the overflow they extend.
    mutableContainingBlock->updateOutOfFlowDescendantTransformsAfterLayout();
    // Fixed positioned boxes do not scroll with the content, so they add no scrollable overflow, and a positioned inline box places the boxes from
    // its first fragment (RenderBlock::addOverflowFromOutOfFlowBoxes).
    downcast<RenderBlockFlow>(mutableContainingBlock).addOverflowFromOutOfFlowBoxes();
}

bool BlockLayout::updateRenderers(const Layout::BlockLayoutResult& result, PaintInvalidation& paintInvalidation, RelayoutChildren relayoutChildren)
{
    m_outOfFlowContainingBlockLineGrids.appendVector(result.outOfFlowContainingBlockLineGrids);
    auto hasAutoScrollbarChanges = false;
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
        bool needsLayout { false };
        LayoutRect contentBoxRect;
        LayoutRect scrollableOverflowRect;
    };
    auto previousStates = Vector<PreviousState> { };
    auto layoutRepainters = Vector<LayoutRepainter> { };
    // What RenderBlockFlow::layoutBlockChild does for a box after the clamp point: it hides it and its content, which it does not lay out.
    for (auto& invisibleBox : result.invisibleBoxes) {
        CheckedRef renderer = mutableRenderer(invisibleBox);
        LineClampUpdater::setIsForcedHidden(renderer, true);
        LineClampUpdater::skipLayoutForForcedHidden(renderer);
    }

    previousStates.reserveInitialCapacity(result.boxes.size());
    layoutRepainters.reserveInitialCapacity(result.boxes.size());
    for (auto& box : result.boxes) {
        CheckedRef renderer = mutableRenderer(box.renderer);
        previousStates.append({ renderer->borderBoxRectInContainer(), renderer->everHadLayout(), renderer->needsLayout(), renderer->contentBoxRect(), renderer->layoutOverflowRect() });
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
        // A column set takes the column inline size and count of its multi-column flow (RenderMultiColumnSet::updateLogicalWidth), also those
        // the multi-column container gives the flow without laying it out again, like when printing starts, where it has one column
        // (RenderBlockFlow::computeColumnCountAndWidth).
        if (CheckedPtr columnSet = dynamicDowncast<RenderMultiColumnSet>(renderer))
            columnSet->setComputedColumnWidthAndCount(columnSet->multiColumnFlow()->columnWidth(), columnSet->multiColumnFlow()->columnCount());
        // What RenderBlockFlow::layoutBlock leaves for the parent to collapse with, when the parent is laid out by the render tree path
        // and this box is not laid out again. A box laid out through the render tree has them from its own layout, in its own writing
        // mode, which an orthogonal box does not share with the block formatting context.
        if (CheckedPtr blockFlow = box.isLaidOutThroughRenderTree ? nullptr : dynamicDowncast<RenderBlockFlow>(renderer)) {
            // The floats the render tree path listed from its last layout of the box: the block formatting context placed them this time, and
            // they paint from the tree (RenderBlockFlow::paintFloats).
            blockFlow->m_floatingObjects = nullptr;
            if (!blockFlow->floatsPlacedByBlockFormattingContext().isEmpty())
                renderer->view().frameView().layoutContext().addRootWithFloatsPlacedByBlockFormattingContext(blockFormattingContextRootOf(*blockFlow));
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
        auto physicalLocation = containingBlock->writingMode().isHorizontal() ? location : location.transposedPoint();
        // The render tree places a box whose containing block is a positioned inline box from the border box of the first fragment of the
        // inline box, which is in the block container whose inline content the inline box is in
        // (RenderBoxModelObject::blockStartRelativeFirstFragmentBorderBoxRect).
        if (CheckedPtr container = renderer->isOutOfFlowPositioned() ? renderer->container() : nullptr; container && container->isInlineBox()) {
            for (CheckedPtr<const RenderBlock> block = container->containingBlock(); block && block != containingBlock.ptr(); block = block->containingBlock())
                physicalLocation -= toLayoutSize(block->location());
            physicalLocation -= toLayoutSize(downcast<RenderBoxModelObject>(*container).blockStartRelativeFirstFragmentBorderBoxRect().location());
        }
        renderer->setLocation(physicalLocation);
    }

    // Like RenderBlock::computeFragmentRangeForBoxChild for the boxes the render tree lays out in a fragmented flow (RenderBlockFlow::layoutBlockChild,
    // positionNewFloats, RenderBlock::layoutOutOfFlowBox): the column sets a box spans, which client rects and hit testing look up
    // (RenderFragmentedFlow::computedFragmentRangeForBox), and which are otherwise the one its containing block starts in, a set before a column
    // spanner for a box after it.
    for (auto& box : result.boxes) {
        CheckedRef renderer = box.renderer;
        CheckedPtr fragmentedFlow = renderer->enclosingFragmentedFlow();
        if (!fragmentedFlow || !fragmentedFlow->hasValidFragmentInfo())
            continue;
        auto blockStart = renderer->offsetFromLogicalTopOfFirstPage();
        auto blockEnd = blockStart + renderer->containingBlock()->logicalHeightForChild(renderer);
        fragmentedFlow->setFragmentRangeForBox(renderer, fragmentedFlow->fragmentAtBlockOffset(nullptr, blockStart, true), fragmentedFlow->fragmentAtBlockOffset(nullptr, blockEnd, true));
    }

    // What RenderBox::computePercentageLogicalHeight does for a box whose percentage block size resolves against a table cell, or looks past the
    // root: the containing block it resolves against registers it, which makes the table lay the cell out again once the row has its height
    // (RenderTableSection::relayoutCellIfFlexed), and lays it out again when that containing block lays out
    // (RenderBlockFlow::dirtyForLayoutFromPercentageHeightDescendantsIfBasisChanged). The block formatting context lays the others out
    // whenever the root lays out.
    for (auto& box : result.boxes) {
        CheckedRef renderer = mutableRenderer(box.renderer);
        if (box.isLaidOutThroughRenderTree || !Layout::BlockFormattingUtils::hasPercentageBlockSize(renderer, renderer->writingMode()))
            continue;
        auto looksPastRoot = false;
        CheckedPtr containingBlock = renderer->containingBlock();
        while (containingBlock && renderer->skipContainingBlockForPercentHeightCalculation(*containingBlock, renderer->isHorizontalWritingMode() != containingBlock->isHorizontalWritingMode())) {
            looksPastRoot |= containingBlock == &rootRenderer();
            containingBlock = containingBlock->containingBlock();
        }
        if (containingBlock && (looksPastRoot || is<RenderTableCell>(*containingBlock)))
            containingBlock->addPercentHeightDescendant(renderer);
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
    // Whether the containing block of each box lays out the box again, like RenderBlockFlow::layoutBlock: its in-flow children when its content
    // box moved or changed along its inline axis, and its out-of-flow ones when it changed at all (the root and the containing block of the
    // out-of-flow boxes outside the result say so).
    auto containingBlockRelaysOutChildren = Vector<bool>(result.boxes.size());
    auto containingBlockIndexes = Vector<size_t> { };
    for (auto boxIndex = result.boxes.size(); boxIndex--;) {
        CheckedRef renderer = result.boxes[boxIndex].renderer;
        while (!containingBlockIndexes.isEmpty() && result.boxes[containingBlockIndexes.last()].renderer.ptr() != &containingBlockOf(renderer))
            containingBlockIndexes.removeLast();
        containingBlockRelaysOutChildren[boxIndex] = [&] {
            if (containingBlockIndexes.isEmpty())
                return relayoutChildren == RelayoutChildren::Yes;
            auto previousContentBox = previousStates[containingBlockIndexes.last()].contentBoxRect;
            CheckedRef containingBlock = result.boxes[containingBlockIndexes.last()].renderer;
            auto contentBox = containingBlock->contentBoxRect();
            if (renderer->isOutOfFlowPositioned())
                return previousContentBox != contentBox;
            if (containingBlock->writingMode().isHorizontal())
                return previousContentBox.x() != contentBox.x() || previousContentBox.width() != contentBox.width();
            return previousContentBox.y() != contentBox.y() || previousContentBox.height() != contentBox.height();
        }();
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
            auto contentArea = blockFlow->blockStartRelativeContentBoxRect();
            auto contentBlockEnd = blockFlow->borderAndPaddingBefore() + box.intrinsicContentBlockSize;
            auto isHorizontalWritingMode = blockFlow->writingMode().isHorizontal();
            if (isHorizontalWritingMode)
                contentArea.shiftMaxYEdgeTo(contentBlockEnd);
            else
                contentArea.shiftMaxXEdgeTo(contentBlockEnd);
            // The transforms of the children that depend on the size of their containing block (offset-path), before the overflow they extend.
            blockFlow->updateInFlowDescendantTransformsAfterLayout();
            // The inline formatting context took the overflow of the block-level boxes in the inline content from their renderers, before they
            // had their new geometry. content-visibility leaves the content it skips as it is.
            if (CheckedPtr lineLayout = blockFlow->inlineLayout(); lineLayout && lineLayout->hasBlocks() && !blockFlow->layoutContext().isSkippedContentRootForLayout(*blockFlow))
                lineLayout->updateOverflow();
            blockFlow->computeInFlowOverflow(contentArea, isHorizontalWritingMode ? RenderBox::ComputeOverflowOptions::MarginsExtendContentAreaX : RenderBox::ComputeOverflowOptions::MarginsExtendContentAreaY);
            BlockIntegrationUtils::placeOutsideListMarker(*blockFlow);
            // Like RenderBlockFlow::layoutBlock, once the box has its size and its in-flow overflow. The render tree path lays them all out again
            // when its size changed.
            auto& previousState = previousStates[boxIndex];
            auto relaysOutOutOfFlowBoxes = blockFlow->isDocumentElementRenderer() || previousState.borderBoxRect.size() != renderer->borderBoxRectInContainer().size() || previousState.contentBoxRect != renderer->contentBoxRect();
            invalidateWithLayoutDelta(locationChanges[boxIndex], [&] {
                layoutOutOfFlowBoxes(*blockFlow, paintInvalidation, relaysOutOutOfFlowBoxes ? RelayoutChildren::Yes : RelayoutChildren::No);
            });
        }
        // Like RenderBlockFlow::layoutBlock: the transform depends on the size of the box (transform-origin).
        renderer->updateLayerTransform();

        // What RenderBlockFlow::layoutBlock invalidates: changes inside the box, at the place it had before it moved.
        auto damagedInlineContent = std::optional<PaintInvalidation::LogicalTopAndBottom> { };
        if (blockFlow) {
            // The geometry is in the writing mode of the containing block, whose inline axis is the box's block axis in an orthogonal flow.
            auto blockAxisBorderAndPadding = blockFlow->writingMode().isOrthogonal(containingBlockOf(*blockFlow).writingMode()) ? box.geometry.horizontalBorderAndPadding() : box.geometry.verticalBorderAndPadding();
            damagedInlineContent = paintInvalidation.damagedInlineContentLogicalTopAndBottom(*blockFlow, blockAxisBorderAndPadding + box.intrinsicContentBlockSize);
            if (CheckedPtr cache = protect(blockFlow->document())->existingAXObjectCache(); cache && damagedInlineContent)
                cache->onLaidOutInlineContent(*blockFlow);
        }
        invalidateWithLayoutDelta(locationChanges[boxIndex], [&] {
            auto didInvalidateWholeBox = layoutRepainters[boxIndex].repaintAfterLayout();
            if (damagedInlineContent && !didInvalidateWholeBox)
                downcast<RenderBlockFlow>(renderer).repaintInlineContentAfterLayout(damagedInlineContent->logicalTop, damagedInlineContent->logicalBottom);
        });
        // What RenderBlock::layout and RenderBlock::recomputeLogicalWidth do once the box is laid out. The scroll size and position of a
        // scroll container follow its new overflow, and so do its auto scrollbars, which lay it out again when they come or go, or the
        // ancestor whose size they change (RelayoutScopeForScrollbarChange). Not when the render tree path would not lay the box out: nothing in
        // it needed layout and its containing block does not lay out its children again, unless its size, its content box (which its scrollbars
        // take space from) or its scrollable overflow changed, or its scroll size is not that of its overflow anymore, which the layout its auto
        // scrollbars coming or going started changed, measured with the scrollbars before (RenderLayerScrollableArea::updateScrollInfoAfterLayout).
        // That keeps a scroll position that scrolling moved off its snap position there (ScrollableArea::resnapAfterLayout).
        if (blockFlow) {
            auto& previousState = previousStates[boxIndex];
            auto hasStaleScrollSize = [&] {
                CheckedPtr layer = blockFlow->layer();
                CheckedPtr scrollableArea = layer ? layer->scrollableArea() : nullptr;
                auto overflowRect = blockFlow->layoutOverflowRect();
                return scrollableArea && (scrollableArea->scrollWidth() != roundToInt(overflowRect.width()) || scrollableArea->scrollHeight() != roundToInt(overflowRect.height()));
            };
            auto mayHaveNewScrollInfo = previousState.needsLayout || containingBlockRelaysOutChildren[boxIndex] || previousState.borderBoxRect.size() != renderer->borderBoxRectInContainer().size()
                || previousState.contentBoxRect != renderer->contentBoxRect() || previousState.scrollableOverflowRect != renderer->layoutOverflowRect() || hasStaleScrollSize();
            if (auto scrollbarUpdate = mayHaveNewScrollInfo ? blockFlow->updateScrollInfoAfterLayout() : std::nullopt; scrollbarUpdate && !scrollbarUpdate->autoScrollbarChanges().isEmpty()) {
                auto& autoScrollbarChanges = scrollbarUpdate->autoScrollbarChanges();
                blockFlow->repaint();
                CheckedRef style = blockFlow->style();
                if (style->overflowX() == Overflow::Auto || style->overflowY() == Overflow::Auto) {
                    auto& subtreeScrollbarChangesState = blockFlow->layoutContext().subtreeScrollbarChangesState();
                    auto sizesAffected = subtreeScrollbarChangesState && subtreeScrollbarChangesState->isEligibleForScrollbarHandlingByAncestor(*blockFlow) ? subtreeScrollbarChangesState->sizesAffectedForSubtreeRootFromScrollbarChanges(*blockFlow, autoScrollbarChanges) : EnumSet<LogicalBoxAxis> { };
                    if (!sizesAffected.isEmpty())
                        subtreeScrollbarChangesState->addRendererWithScrollbarChange(*blockFlow, sizesAffected);
                    else
                        hasAutoScrollbarChanges = true;
                }
            }
            blockFlow->setShouldForceRelayoutChildren(false);
        }
        renderer->invalidateBackgroundObscurationStatus();
        renderer->setScrollAnchoringSuppressionStyleChanged(false);
        // Its skipped content has had its layout skipped, which lays it out once it is not skipped anymore (RenderBlockFlow::layoutInFlowChildren).
        if (blockFlow && blockFlow->layoutContext().isSkippedContentRootForLayout(*blockFlow))
            blockFlow->clearNeedsLayoutForSkippedContent();
        renderer->clearNeedsLayout();
        // What RenderMultiColumnSet::layout does once the column set has its position and size: the largest column block size the multi-column
        // container leaves it from there, and the fragments of the multi-column flow, once the last column set has them too.
        if (CheckedPtr columnSet = dynamicDowncast<RenderMultiColumnSet>(renderer))
            columnSet->updateAfterLayout();

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
    // Once the boxes have their overflow, and their containing blocks their positions.
    if (CheckedPtr multiColumnFlow = dynamicDowncast<RenderMultiColumnFlow>(rootRenderer()))
        updateSpaceShortageForSizeContainment(*multiColumnFlow, result);
    return hasAutoScrollbarChanges;
}

} // namespace LayoutIntegration
} // namespace WebCore
