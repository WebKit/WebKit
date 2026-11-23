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
#include "BlockFormattingContext.h"

#include "BlockFormattingUtils.h"
#include "BlockIntegrationUtils.h"
#include "BlockItemsBuilder.h"
#include "BlockStepSizing.h"
#include "Element.h"
#include "FloatingContext.h"
#include "FontCascadeInlines.h"
#include "InlineContentConstrainer.h"
#include "LayoutIntegrationBlockCoverage.h"
#include "LineClampUpdater.h"
#include "LocalFrameViewInlines.h"
#include "LocalFrameViewLayoutContext.h"
#include "OutOfFlowGeometry.h"
#include "RenderBlockFlow.h"
#include "RenderBoxInlines.h"
#include "RenderDescendantIterator.h"
#include "RenderElementStyleInlines.h"
#include "RenderListItem.h"
#include "RenderMultiColumnSet.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderObjectInlines.h"
#include "RenderTable.h"
#include "RenderTableCell.h"
#include "RenderView.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StylePrimitiveNumericTypes+EvaluationMinimum.h"
#include "StyleUnevaluatedCalcSize.h"
#include "UsedStyle.h"
#include <ranges>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_TZONE_ALLOCATED_IMPL(BlockBreakToken);
WTF_MAKE_TZONE_ALLOCATED_IMPL(BlockFormattingContext);

static BlockFormattingUtils::ContainingBlockIsTableCell containingBlockIsTableCell(const ContainingBlockState& containingBlockState)
{
    return containingBlockState.blockContainer->isRenderTableCell() ? BlockFormattingUtils::ContainingBlockIsTableCell::Yes : BlockFormattingUtils::ContainingBlockIsTableCell::No;
}

// What the percentage block sizes of a box resolve against: the content box block size of its containing block. A table cell is a root
// whose content box block size is the one the row gives it (constraintsForBlockContent); before it has one, a child that scrolls its
// overflow gets zero when the cell or the table has a block size (RenderBox::computePercentageLogicalHeight, tableCellShouldHaveZeroInitialSize).
static std::optional<LayoutUnit> percentageBasis(const RenderBox& box, const ContainingBlockState& containingBlockState)
{
    CheckedPtr tableCell = dynamicDowncast<RenderTableCell>(containingBlockState.blockContainer);
    if (!tableCell || containingBlockState.contentBoxBlockSize || !box.scrollsOverflowY())
        return containingBlockState.contentBoxBlockSize;
    auto& cellBlockSize = tableCell->style().logicalHeight();
    if (tableCell->table()->style().logicalHeight().isAuto() && (cellBlockSize.isAuto() || cellBlockSize.isPercentOrCalculated()))
        return { };
    return 0_lu;
}

// The content box block size the percentage block sizes in the content of a box resolve against while its inline sizes are measured, but
// not one a preferred aspect ratio would give it from the inline size being measured (RenderBlock::availableLogicalHeightForPercentageComputation).
static std::optional<LayoutUnit> contentBoxBlockSizeWhileMeasured(const RenderBox& box, const BoxGeometry& geometry, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, const StretchFitBorderBoxBlockSizes& stretchFitBorderBoxBlockSizes, WritingMode writingMode, BlockFormattingUtils::ContainingBlockIsTableCell containingBlockIsTableCell = BlockFormattingUtils::ContainingBlockIsTableCell::No)
{
    CheckedRef style = box.style();
    if (style->aspectRatio().hasRatio() && style->logicalHeight(writingMode).isAuto())
        return { };
    return BlockFormattingUtils::definiteContentBoxBlockSize(box, geometry, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSizes, writingMode, containingBlockIsTableCell);
}

// What stretch block sizes resolve against. The children of a table cell stretch to the block size the cell has from its style when that is
// fixed, not to the one the row gives it, which only percentages resolve against (RenderBox::isBlockSizeResolvableForStretch), and like those
// without the intrinsic padding of its vertical alignment.
static std::optional<LayoutUnit> stretchFitBasis(const ContainingBlockState& containingBlockState)
{
    CheckedPtr tableCell = dynamicDowncast<RenderTableCell>(containingBlockState.blockContainer);
    if (!tableCell)
        return BlockFormattingUtils::stretchFitBasis(containingBlockState.blockContainer, containingBlockState.contentBoxBlockSize);
    if (!tableCell->style().logicalHeight().isFixed())
        return { };
    auto& geometry = containingBlockState.geometry;
    auto borderAndPaddingBlockSize = geometry.verticalBorderAndPadding() - tableCell->intrinsicPaddingBefore() - tableCell->intrinsicPaddingAfter();
    auto borderBoxBlockSize = BlockFormattingUtils::borderBoxBlockSize(*tableCell, borderAndPaddingBlockSize, { }, { }, { }, tableCell->writingMode());
    return std::max(0_lu, borderBoxBlockSize - borderAndPaddingBlockSize - tableCell->scrollbarLogicalHeight());
}

// A box that continues in this fragmentainer from its start, without its block-start margin, border and padding (box-decoration-break: slice),
// with what the fragmentainers before took of it (css-break-3 §4): its content starts below the rest of its block-start border and padding
// when they crossed the end of the fragmentainer before. With box-decoration-break: clone the fragment has its block-start border and padding,
// but not its margin, which is truncated (css-break-3 §5.2, §5.4).
static void continueFromFragmentainerBefore(ContainingBlockState& boxState, const ContainingBlockState::ConsumedBlockSize& consumedBlockSize)
{
    auto& geometry = boxState.geometry;
    geometry.setVerticalMargin({ { }, geometry.marginAfter() });
    if (!consumedBlockSize.clonesDecorations) {
        geometry.setVerticalBorder({ { }, geometry.borderAfter() });
        geometry.setVerticalPadding({ { }, geometry.paddingAfter() });
    }
    boxState.borderAndPaddingBefore = consumedBlockSize.borderAndPaddingBeforeLeft();
    boxState.blockEndChildren.contentBoxLogicalTop = boxState.borderAndPaddingBefore;
    boxState.consumedBlockSize = consumedBlockSize;
}

BlockFormattingContext::BlockFormattingContext(const RenderBlockFlow& root, LayoutIntegration::BlockIntegrationUtils& integrationUtils, BlockLayoutState& blockLayoutState)
    : m_root(root)
    , m_integrationUtils(integrationUtils)
    , m_blockLayoutState(blockLayoutState)
{
}

BlockLayoutResult BlockFormattingContext::layout(const ConstraintsForBlockContent& constraints, const BoxGeometry& rootGeometry, const RootLineGrid& rootLineGrid, std::optional<FragmentainerConstraints> fragmentainer, const BlockBreakToken* breakToken)
{
    m_rootLineGrid = rootLineGrid;
    m_fragmentainer = fragmentainer;
    m_incomingBreakToken = breakToken;
    LayoutIntegration::BlockIntegrationUtils::layoutOutsideListMarker(root(), constraints.relayoutChildren());
    // The line of its own the marker of a list item gets in quirks mode goes above its content, in its first fragment, and its content block
    // size includes it (RenderListItem::layoutExcludedChildren).
    auto ownLineBlockSize = breakToken ? 0_lu : LayoutIntegration::BlockIntegrationUtils::outsideListMarkerOwnLineBlockSize(root());
    auto contentConstraints = !ownLineBlockSize ? constraints : ConstraintsForBlockContent { { constraints.horizontal(), constraints.logicalTop() + ownLineBlockSize }, constraints.contentBoxBlockSize(), constraints.fillAvailableBlockSize(), constraints.replacedElementAvailableBlockSize(), constraints.relayoutChildren() };
    auto layoutRootContent = [&] {
        auto result = layoutContent(contentConstraints, rootGeometry);
        result.intrinsicContentBlockSize += ownLineBlockSize;
        return result;
    };
    // Floats placed from a provisional top beside or below floats from before may go elsewhere from the top the margins give their block
    // container: the content is laid out again with those tops, like the render tree path lays out a block that moved while its floats were
    // placed again (RenderBlockFlow::layoutBlockChild).
    auto initialBlockLayoutState = m_blockLayoutState;
    auto initialLegacyLineClamp = LayoutIntegration::BlockIntegrationUtils::legacyLineClamp(root());
    auto result = layoutRootContent();
    if (!m_needsLayoutWithResolvedBorderBoxLogicalTops)
        return result;
    m_needsLayoutWithResolvedBorderBoxLogicalTops = false;
    m_isLayingOutWithResolvedBorderBoxLogicalTops = true;
    m_blockLayoutState = WTF::move(initialBlockLayoutState);
    LayoutIntegration::BlockIntegrationUtils::restoreLegacyLineClamp(root(), initialLegacyLineClamp);
    return layoutRootContent();
}

static bool hasForcedBreakAfterContent(const RenderBox& root, FragmentainerConstraints::Type);

BlockLayoutResult BlockFormattingContext::layoutContent(const ConstraintsForBlockContent& constraints, const BoxGeometry& rootGeometry)
{
    ASSERT(m_blockLayoutState.containingBlockStates().isEmpty());
    auto result = BlockLayoutResult { };
    m_fragmentainerHasContent = m_fragmentainer && m_fragmentainer->hasContentBefore;
    m_marginsAdjoinUnforcedBreak = m_incomingBreakToken && !m_incomingBreakToken->isForced;
    m_unsplittableBoxesLaidOutWithBreaks.clear();
    m_pushedFloats.clear();
    m_continuingFloats.clear();
    m_floatContents.clear();
    m_floatsInFragmentainer.clear();
    m_floatBoxCount = 0;
    m_spaceShortage = { };
    m_unbreakableBlockSize = { };
    m_parallelFlowBreakTokens.clear();
    m_parallelFlowBreak = std::nullopt;
    m_parallelFlow = std::nullopt;
    m_inlineBreakTokenToResume = nullptr;
    m_openBlockFormattingContextRootToResume = nullptr;
    m_blockStepExtraSpaceToResume = std::nullopt;
    m_rootBlockStepExtraSpace = std::nullopt;
    m_blockStepExtraSpaceAtBreak = std::nullopt;
    m_blockLevelBoxBreakTokenToResume = nullptr;
    m_blockLevelBoxBreakToken = nullptr;
    m_columnSpannerBreakInBlockLevelBox = std::nullopt;
    m_forcedBreakAfterBlockLevelBox = false;
    m_contentEndsWithMonolithicOverflow = false;
    m_blockLayoutState.containingBlockStates().append(containingBlockStateForRoot(constraints, rootGeometry));
    // The root continues from the fragmentainer before without its block-start border and padding (box-decoration-break: slice).
    if (m_incomingBreakToken)
        continueFromFragmentainerBefore(m_blockLayoutState.containingBlockStates().last(), m_incomingBreakToken->rootConsumedBlockSize);
    m_blockLayoutState.containingBlockStates().last().lineGrid = m_rootLineGrid.lineGrid;
    m_blockLayoutState.marginCollapse().startBlockFormattingContextRoot(m_blockLayoutState.containingBlockStates().last());
    // The margins of a box that establishes a block formatting context do not collapse with its children's (CSS 2.2 §8.3.1).
    m_blockLayoutState.marginCollapse().resetAdjoiningMargins(m_blockLayoutState.containingBlockStates().last().contentBoxLogicalTop());
    // margin-trim discards the margins at the block-start and block-end edges of the content (css-box-4 §3.3.1), which a fragment of the
    // root after the first one does not start with.
    auto marginTrim = root().style().marginTrim();
    if (marginTrim.contains(Style::MarginTrimSide::BlockStart) && !m_incomingBreakToken)
        m_blockLayoutState.marginCollapse().setTrimsMarginsAtBlockStartEdge(true);

    // FIXME: Keep the block items across layouts and invalidate them on tree and formatting context participation changes.
    auto blockItems = BlockItemList { };
    if (!root().childrenInline())
        blockItems = BlockItemsBuilder { root() }.build();
    auto startIndex = size_t { 0 };
    if (m_incomingBreakToken) {
        placeFloatsFromFragmentainerBefore(result);
        // The content overflowing block containers with a block size of their own goes first, and the content after them from the start of the
        // fragmentainer too.
        for (auto& parallelFlow : m_incomingBreakToken->parallelFlows)
            layoutParallelFlow(blockItems, parallelFlow, result);
        result.hasOnlyParallelFlows = m_incomingBreakToken->hasOnlyParallelFlows;
        m_fragmentainerHasContent = m_fragmentainer->hasContentBefore;
        m_marginsAdjoinUnforcedBreak = !m_incomingBreakToken->isForced;
        m_inlineBreakTokenToResume = m_incomingBreakToken->inlineBreakToken ? &*m_incomingBreakToken->inlineBreakToken : nullptr;
        m_blockLevelBoxBreakTokenToResume = m_incomingBreakToken->blockLevelBoxBreakToken.get();
        m_openBlockFormattingContextRootToResume = m_incomingBreakToken->openBlockFormattingContextRoot ? &*m_incomingBreakToken->openBlockFormattingContextRoot : nullptr;
        resumeAfterBreak(*m_incomingBreakToken);
        startIndex = m_incomingBreakToken->blockItemIndex;
        // Content from the fragmentainer before overflows into this one: the content continues below it, which a break before the next box
        // may follow.
        if (auto blockOffset = m_incomingBreakToken->blockOffset) {
            m_blockLayoutState.marginCollapse().resetAdjoiningMargins(blockOffset, m_incomingBreakToken->marginsAfterOverflow);
            m_fragmentainerHasContent = true;
            // The margins after it do not adjoin the break, which is before it.
            m_marginsAdjoinUnforcedBreak = false;
        }
    }
    // A root with only inline-level content establishes an inline formatting context for it (CSS Display 3), whose lines that do not fit
    // continue in the next fragmentainer. The content is one block item, after which only the floats that went to the next fragmentainer
    // continue.
    auto blockItemCount = root().childrenInline() ? 1 : blockItems.size();
    if (root().childrenInline()) {
        if (!startIndex) {
            // Below content before it, its first lines may go to the next fragmentainer with it to keep orphans and widows (css-break-3 §4.4).
            auto inlineContentBreak = handleInlineContent(root(), result, !m_incomingBreakToken && m_fragmentainerHasContent);
            auto isForcedBreak = std::exchange(m_forcedBreakAfterBlockLevelBox, false);
            if (auto columnSpannerBreak = std::exchange(m_columnSpannerBreakInBlockLevelBox, std::nullopt))
                endMultiColumnLineInInlineContent(*columnSpannerBreak, WTF::move(inlineContentBreak), 0, result);
            else if (inlineContentBreak) {
                result.isBreakBeforeContent = !m_incomingBreakToken && !inlineContentBreak->breaksInside;
                auto& breakToken = endFragmentainer(0, isForcedBreak, result);
                breakToken.inlineBreakToken = WTF::move(inlineContentBreak->breakToken);
                breakToken.blockLevelBoxBreakToken = WTF::move(inlineContentBreak->blockLevelBoxBreakToken);
            }
        }
    } else
        layoutBlockItems(blockItems, result, startIndex);
    if (m_fragmentainer && !result.breakToken && (!m_pushedFloats.isEmpty() || !m_continuingFloats.isEmpty()))
        endFragmentainer(blockItemCount, false, result);
    // The floats that go to the next fragmentainer have nothing in this one, nor does their content.
    auto isInPushedFloat = [&](const RenderBox& box) {
        return m_pushedFloats.containsIf([&](auto& pushedFloat) {
            return pushedFloat.box.ptr() == &box || box.isDescendantOf(pushedFloat.box.ptr());
        });
    };
    result.boxes.removeAllMatching([&](auto& box) {
        return isInPushedFloat(box.renderer);
    });
    result.outOfFlowBoxes.removeAllMatching([&](auto& outOfFlowBox) {
        return isInPushedFloat(outOfFlowBox.renderer);
    });

    ASSERT(m_blockLayoutState.containingBlockStates().size() == 1);
    result.spaceShortage = m_spaceShortage;
    result.unbreakableBlockSize = m_unbreakableBlockSize;
    // The content continues in the next fragmentainer: the root's fragment fills this one (css-break-3 §4.1). The root ends in it when only
    // the content overflowing block containers with a block size of their own does.
    auto parallelFlowsBreakToken = std::optional<BlockBreakToken> { };
    if (!result.breakToken && !m_parallelFlowBreakTokens.isEmpty()) {
        auto blockSize = m_fragmentainer->availableBlockSize();
        parallelFlowsBreakToken = BlockBreakToken { blockItemCount, { }, { }, false, { }, false };
        parallelFlowsBreakToken->overflowingFloats = floatsOverflowingFragmentainer(blockSize);
        parallelFlowsBreakToken->parallelFlows = std::exchange(m_parallelFlowBreakTokens, { });
        parallelFlowsBreakToken->hasOnlyParallelFlows = true;
        result.fragmentainerBlockSize = blockSize;
    }
    if (result.breakToken) {
        auto& rootState = m_blockLayoutState.containingBlockStates().last();
        result.intrinsicContentBlockSize = std::max(0_lu, result.fragmentainerBlockSize - rootState.contentBoxLogicalTop());
        m_blockLayoutState.containingBlockStates().clear();
        return result;
    }
    // The content of the root ends at the bottom margin edge of its last in-flow child (CSS 2.2 §10.6.7), except for a quirky one in a
    // quirk container. Negative margins may pull that above its content box top.
    auto& rootState = m_blockLayoutState.containingBlockStates().last();
    if (marginTrim.contains(Style::MarginTrimSide::BlockEnd)) {
        m_blockLayoutState.marginCollapse().resetAdjoiningMargins(m_blockLayoutState.marginCollapse().logicalTop());
        placeChildrenAtTrimmedBlockEnd(WTF::move(rootState.blockEndChildren), result);
    }
    m_blockLayoutState.marginCollapse().endBlockFormattingContextRoot(rootState);
    auto contentLogicalBottom = BlockMarginCollapse::isQuirkContainer(root()) && m_blockLayoutState.marginCollapse().endsWithQuirk() ? m_blockLayoutState.marginCollapse().logicalTop() : m_blockLayoutState.marginCollapse().collapsedLogicalBottom();
    // The content of a block formatting context root also ends below the bottom margin edge of its floats, except in a line-clamp container
    // (CSS 2.2 §10.6.7, RenderBlockFlow::layoutBlock).
    if (auto* placedFloats = m_blockLayoutState.placedFloats(); placedFloats && !root().establishesLineClampContainer()) {
        if (auto floatsLogicalBottom = placedFloats->lowestPositionOnBlockAxis())
            contentLogicalBottom = std::max(contentLogicalBottom, *floatsLogicalBottom);
    }
    if (m_fragmentainer && m_fragmentainer->appliesForcedBreakAfterContent && m_fragmentainer->blockSize && hasForcedBreakAfterContent(root(), m_fragmentainer->breakType()))
        contentLogicalBottom = std::max(contentLogicalBottom, m_fragmentainer->availableBlockSize());
    result.intrinsicContentBlockSize = std::max(0_lu, contentLogicalBottom - rootState.contentBoxLogicalTop());
    addLineGridOfOutOfFlowContainingBlock(root(), rootState, result);
    m_blockLayoutState.containingBlockStates().clear();
    result.breakToken = WTF::move(parallelFlowsBreakToken);
    return result;
}

// A box that cannot be split across fragmentainers (css-break-3 §4.4): monolithic content, and a box with break-inside: avoid, or the avoid
// value of the fragmentainer's type (RenderBlock::childBoxIsUnsplittableForFragmentation).
static bool isUnsplittableForPagination(const RenderBox& box, FragmentainerConstraints::Type type)
{
    auto breakInside = box.style().breakInside();
    return box.isUnsplittableForPagination() || breakInside == BreakInside::Avoid || breakInside == (type == FragmentainerConstraints::Type::Page ? BreakInside::AvoidPage : BreakInside::AvoidColumn);
}

// A monolithic box in the content of a multi-column flow, which a column paints whole, past its end when it does not fit, where it takes no
// room in the columns after it (css-multicol-1 §8.1, LocalFrameViewLayoutContext::paintsColumnsFromFragments). Not one in a page yet, nor in
// a box the render tree lays out in the columns, which paint it sliced, like the render tree.
bool BlockFormattingContext::paintsMonolithicOverflow(const RenderBox& box) const
{
    return m_fragmentainer && m_fragmentainer->type == FragmentainerConstraints::Type::Column && root().isRenderMultiColumnFlow() && box.isUnsplittableForPagination();
}

// A block formatting context root the block formatting context lays out, whose content breaks like that of a block container (css-break-3 §4.1),
// unless it is monolithic.
static bool hasFragmentableContent(const RenderBox& box)
{
    CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(box);
    return blockContainer && !LayoutIntegration::BlockIntegrationUtils::laysOutThroughRenderTree(*blockContainer) && !box.isUnsplittableForPagination();
}

// A block size of its own, which the content does not give it, and which the content may overflow.
static bool hasBlockSizeOfItsOwn(const RenderBox& box)
{
    CheckedRef style = box.style();
    auto writingMode = box.writingMode();
    return !style->logicalHeight(writingMode).isAuto() || !style->logicalMaxHeight(writingMode).isNone();
}

// A table, a flex container (-webkit-box included), a grid container or a MathML box, whose content the render tree breaks across the
// fragmentainers itself, from where the box goes (LayoutIntegration::BlockIntegrationUtils::layoutIndependentFormattingContext), unless it is
// monolithic, and so does a multi-column container in columns, and a column set or a frameset, whose columns or frames the fragmentainers
// slice where it is, like the content of an empty block container with a block size.
static bool hasContentFragmentedByRenderTree(const RenderBox& box, const std::optional<FragmentainerConstraints>& fragmentainer)
{
    if (!fragmentainer || box.isUnsplittableForPagination())
        return false;
    if (LayoutIntegration::isLaidOutByOwnRenderTreeLayout(box) || box.isRenderMultiColumnSet() || box.isRenderFrameSet())
        return true;
    CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(box);
    if (!blockFlow)
        return false;
    if (blockFlow->multiColumnFlow())
        return fragmentainer->type == FragmentainerConstraints::Type::Column;
    // So does a block formatting context root the block formatting context lays out through the render tree, in the normal flow or floating,
    // unless the block formatting context lays out the content of the float (BlockFormattingContext::laysOutFloatContent), and a progress or
    // meter element.
    if (blockFlow->isFloating() && LayoutIntegration::canUseForFloatContentLayout(*blockFlow))
        return false;
    auto isLaidOutAsRoot = blockFlow->createsNewFormattingContext() || BlockFormattingUtils::isFormControlBlockContainer(*blockFlow);
    return isLaidOutAsRoot && !blockFlow->isOutOfFlowPositioned() && LayoutIntegration::BlockIntegrationUtils::laysOutThroughRenderTree(*blockFlow);
}

// Nothing in a block container that cannot be split breaks while it may fit in a fragmentainer as a whole (css-break-3 §4.4): it goes to the
// next one, or, when it does not fit where it starts, it is laid out again breaking inside like any box, and so is what continues of it in the
// fragmentainers after (RenderBlockFlow::adjustBlockChildForPagination moves the box after its content is laid out, unless it does not fit).
bool BlockFormattingContext::breaksInside(std::span<const ContainingBlockState> containingBlockStates) const
{
    return std::ranges::all_of(containingBlockStates, [&](auto& state) {
        return state.consumedBlockSize || !isUnsplittableForPagination(state.blockContainer, m_fragmentainer->breakType()) || m_unsplittableBoxesLaidOutWithBreaks.containsIf([&](auto& box) {
            return box.ptr() == state.blockContainer.ptr();
        });
    });
}

// One whose content the render tree breaks, unless it avoids breaks inside while it fits in a fragmentainer, like a flex container, which the
// render tree moves to the next one like a box that cannot be split (RenderBlockFlow::adjustForUnsplittableChild).
bool BlockFormattingContext::breaksContentThroughRenderTree(const RenderBox& box) const
{
    if (!hasContentFragmentedByRenderTree(box, m_fragmentainer))
        return false;
    auto avoidsBreaksInside = isUnsplittableForPagination(box, m_fragmentainer->breakType()) || box.type() == RenderObject::Type::FlexibleBox;
    return !avoidsBreaksInside || m_unsplittableBoxesLaidOutWithBreaks.containsIf([&](auto& laidOutWithBreaks) {
        return laidOutWithBreaks.ptr() == &box;
    });
}

// The break-before of a block-level box and the break-after of the in-flow box before it, where the break-before of a first in-flow child and
// the break-after of a last one propagate to their parent (css-break-3 §3.1), but not those of flex and grid items, which flex and grid layout
// apply (css-flexbox-1 §12), in the render tree here, nor those of the content of monolithic boxes, which does not break (css-break-3 §4.1).
// Forced breaks do not propagate to a layout containment box (css-contain-2 §3.2), nor do they have a break point between it and its first
// in-flow child to go to (css-break-3 §4.1).
enum class BreakValues : bool { All, Forced };
static bool keepsBreaksInside(const RenderBox& container, BreakValues values)
{
    return container.isFlexibleBoxIncludingDeprecated() || container.isRenderGrid() || container.isUnsplittableForPagination() || (values == BreakValues::Forced && container.shouldApplyLayoutContainment());
}

template<typename Matches>
static bool hasBreakValueBefore(const RenderBox& box, BreakValues values, const Matches& matches)
{
    auto keepsBreaksInside = [&](const RenderBox& container) {
        return WebCore::Layout::keepsBreaksInside(container, values);
    };
    for (CheckedPtr<const RenderBox> child = &box; !child->previousInFlowSibling();) {
        CheckedPtr parent = dynamicDowncast<RenderBox>(child->parent());
        if (!parent)
            break;
        if (keepsBreaksInside(*parent))
            return false;
        child = WTF::move(parent);
    }
    for (CheckedPtr<const RenderBox> first = &box; first; first = keepsBreaksInside(*first) ? nullptr : dynamicDowncast<RenderBox>(first->firstInFlowChild())) {
        if (matches(first->style().breakBefore()))
            return true;
    }
    for (CheckedPtr last = dynamicDowncast<RenderBox>(box.previousInFlowSibling()); last; last = keepsBreaksInside(*last) ? nullptr : dynamicDowncast<RenderBox>(last->lastInFlowChild())) {
        if (matches(last->style().breakAfter()))
            return true;
    }
    return false;
}

// A page break in pages, and a column break in columns (css-break-3 §3.1). Columns are the only fragmentainers of their content, also in
// pages, where the page break values do nothing, like in the render tree (RenderBlockFlow::applyBeforeBreak). In pages, a change of page
// value forces one too (css-page-3, Using named pages).
static bool hasForcedBreakBefore(const RenderBox& box, FragmentainerConstraints::Type type)
{
    if (type == FragmentainerConstraints::Type::Page) {
        if (CheckedPtr parent = dynamicDowncast<RenderBlockFlow>(box.parent()); parent && BlockFormattingUtils::pageValuesForceBreakBefore(*parent, box))
            return true;
    }
    return hasBreakValueBefore(box, BreakValues::Forced, [&](BreakBetween value) {
        return type == FragmentainerConstraints::Type::Page ? alwaysPageBreak(value) : value == BreakBetween::Column;
    });
}

// The break-after of a box, and that of its last in-flow box, which propagates to it (css-break-3 §3.1).
static bool hasForcedBreakAfter(const RenderBox& box, FragmentainerConstraints::Type type)
{
    for (CheckedPtr last = &box; last; last = keepsBreaksInside(*last, BreakValues::Forced) ? nullptr : dynamicDowncast<RenderBox>(last->lastInFlowChild())) {
        auto value = last->style().breakAfter();
        if (type == FragmentainerConstraints::Type::Page ? alwaysPageBreak(value) : value == BreakBetween::Column)
            return true;
    }
    return false;
}

static bool hasForcedBreakAfterContent(const RenderBox& root, FragmentainerConstraints::Type type)
{
    CheckedPtr last = dynamicDowncast<RenderBox>(root.lastInFlowChild());
    return last && hasForcedBreakAfter(*last, type);
}

// Rule 1 of css-break-3 §4.4: no unforced break between boxes whose break-before or break-after avoid it.
static bool avoidsBreakBefore(const RenderBox& box, FragmentainerConstraints::Type type)
{
    return hasBreakValueBefore(box, BreakValues::All, [&](BreakBetween value) {
        return value == BreakBetween::Avoid || value == (type == FragmentainerConstraints::Type::Page ? BreakBetween::AvoidPage : BreakBetween::AvoidColumn);
    });
}

// A block container with box-decoration-break: clone, each fragment of which has its border and padding, also at a break (css-break-3 §5.4).
static bool clonesDecorations(const ContainingBlockState& state)
{
    return state.blockContainer->style().boxDecorationBreak() == BoxDecorationBreak::Clone && !state.isLaidOutThroughRenderTree;
}

// What the fragmentainers before took of a box, and this one too, which takes a fragment of fragmentBlockSize of its border box.
static ContainingBlockState::ConsumedBlockSize consumedBlockSizeThrough(const std::optional<ContainingBlockState::ConsumedBlockSize>& consumedBefore, LayoutUnit borderAndPaddingBefore, LayoutUnit fragmentBlockSize)
{
    auto consumed = consumedBefore.value_or(ContainingBlockState::ConsumedBlockSize { { }, { }, borderAndPaddingBefore });
    return { consumed.borderBox + fragmentBlockSize, consumed.contentBox + std::max(0_lu, fragmentBlockSize - consumed.borderAndPaddingBeforeLeft()), consumed.borderAndPaddingBefore };
}

// The same for a block container, whose fragment has, when it clones its decorations, the block-start border and padding it has in this
// fragmentainer and the block-end ones it clones at the break (clonedBlockEnd) around its content.
static ContainingBlockState::ConsumedBlockSize consumedBlockSizeThrough(const ContainingBlockState& state, LayoutUnit fragmentBlockSize, LayoutUnit clonedBlockEnd)
{
    if (!clonesDecorations(state))
        return consumedBlockSizeThrough(state.consumedBlockSize, state.geometry.borderAndPaddingBefore(), fragmentBlockSize);
    auto consumed = state.consumedBlockSize.value_or(ContainingBlockState::ConsumedBlockSize { { }, { }, state.geometry.borderAndPaddingBefore(), true });
    auto fragmentContentBlockSize = std::max(0_lu, fragmentBlockSize - state.geometry.borderAndPaddingBefore() - clonedBlockEnd);
    return { consumed.borderBox + fragmentBlockSize, consumed.contentBox + fragmentContentBlockSize, consumed.borderAndPaddingBefore, true };
}

// The fragment of a box that continues in the next fragmentainer fills the rest of this one, without its block-end margin, border and padding
// (box-decoration-break: slice, css-break-3 §4.1), or with its block-end border and padding when it clones them there.
static void sliceAtFragmentainerEnd(BoxGeometry& geometry, LayoutUnit fragmentBlockSize, bool clonesBlockEnd = false)
{
    geometry.setVerticalMargin({ geometry.marginBefore(), { } });
    if (!clonesBlockEnd) {
        geometry.setVerticalBorder({ geometry.borderBefore(), { } });
        geometry.setVerticalPadding({ geometry.paddingBefore(), { } });
    }
    geometry.setContentBoxHeight(std::max(0_lu, fragmentBlockSize - geometry.verticalBorderAndPadding() - geometry.verticalSpaceForScrollbar()));
}

// "The effect is as though the element were rendered with no breaks present, and then sliced by the breaks afterward" (box-decoration-break:
// slice, css-break-3 §5.4): after a break inside its block-end padding and border, the fragment of a box has the part of them the fragmentainers
// before did not take, border first.
static void sliceBlockEndAfterBreak(BoxGeometry& geometry, LayoutUnit fragmentBlockSize)
{
    auto blockEndLeft = std::max(0_lu, fragmentBlockSize - geometry.borderAndPaddingBefore());
    if (blockEndLeft >= geometry.borderAndPaddingAfter())
        return;
    auto borderAfter = std::min(geometry.borderAfter(), blockEndLeft);
    geometry.setVerticalBorder({ geometry.borderBefore(), borderAfter });
    geometry.setVerticalPadding({ geometry.paddingBefore(), blockEndLeft - borderAfter });
}

// The block containers open at the break continue in this fragmentainer from its start, with their text-box-trim, which trims the end of their
// last formatted line, and the start of their first one only when it is still to come.
void BlockFormattingContext::resumeAfterBreak(const BlockBreakToken& breakToken)
{
    auto& containingBlockStates = m_blockLayoutState.containingBlockStates();
    auto withFirstFormattedLineTrimmedFromBreak = [&](TextBoxTrimState state) {
        state.trimsFirstFormattedLine = state.trimsFirstFormattedLine && breakToken.trimsFirstFormattedLine;
        return state;
    };
    m_blockLayoutState.setTextBoxTrimState(withFirstFormattedLineTrimmedFromBreak(m_blockLayoutState.textBoxTrimState()));
    m_blockStepExtraSpaceToResume = breakToken.blockStepExtraSpace;
    for (auto& openBlockContainer : breakToken.openBlockContainers) {
        CheckedRef blockContainer = openBlockContainer.blockContainer;
        auto blockContainerState = containingBlockStateAtBlockStart(blockContainer);
        // Its block-start border and padding, which the first fragment has, keep the start trim of an ancestor from the content after it.
        startTextBoxTrim(downcast<RenderBlockFlow>(blockContainer), blockContainerState);
        m_blockLayoutState.setTextBoxTrimState(withFirstFormattedLineTrimmedFromBreak(m_blockLayoutState.textBoxTrimState()));
        continueFromFragmentainerBefore(blockContainerState, openBlockContainer.consumedBlockSize);
        // A fragment that clones them leaves out its block-start border and padding when they would leave no room for its content: "if more
        // room is still needed, cloned margins/border/padding are truncated at the block-start side as well" (css-break-3 §4.4).
        auto contentLogicalTop = containingBlockStates.last().contentBoxLogicalTop();
        if (openBlockContainer.consumedBlockSize.clonesDecorations && contentLogicalTop + blockContainerState.borderAndPaddingBefore >= m_fragmentainer->availableBlockSize()) {
            auto& geometry = blockContainerState.geometry;
            geometry.setVerticalBorder({ { }, geometry.borderAfter() });
            geometry.setVerticalPadding({ { }, geometry.paddingAfter() });
            blockContainerState.borderAndPaddingBefore = { };
            blockContainerState.blockEndChildren.contentBoxLogicalTop = { };
        }
        // It starts at the start of the content box of the one around it, below the block-start border and padding a fragment that clones
        // them has.
        m_blockLayoutState.marginCollapse().resetAdjoiningMargins(contentLogicalTop);
        m_blockLayoutState.marginCollapse().startBlockContainer(blockContainerState, containingBlockStates.last(), { });
        resolveBorderBoxLogicalTops(blockContainerState);
        containingBlockStates.append(WTF::move(blockContainerState));
    }
}

// The content overflowing a block container with a block size of its own continues from the start of the fragmentainer, in the block container
// and its ancestors, which ended in a fragmentainer before and have no fragment in this one, up to the end of the block container
// (css-break-3 §2.1).
void BlockFormattingContext::layoutParallelFlow(const BlockItemList& rootBlockItems, const BlockBreakToken& breakToken, BlockLayoutResult& result)
{
    // One in a block-level box in inline content continues in the box's own block items (layoutBlockLevelBoxInInlineContent).
    CheckedPtr blockLevelBox = breakToken.parallelFlowBlockLevelBox;
    auto blockLevelBoxItems = blockLevelBox ? BlockItemsBuilder { root() }.buildForBlockLevelBoxInInlineContent(*blockLevelBox) : BlockItemList { };
    auto& blockItems = blockLevelBox ? blockLevelBoxItems : rootBlockItems;
    auto& containingBlockStates = m_blockLayoutState.containingBlockStates();
    ASSERT(containingBlockStates.size() == 1);
    auto marginCollapse = m_blockLayoutState.marginCollapse();
    auto textBoxTrimState = m_blockLayoutState.textBoxTrimState();
    m_fragmentainerHasContent = m_fragmentainer->hasContentBefore;
    m_marginsAdjoinUnforcedBreak = !breakToken.isForced;
    m_inlineBreakTokenToResume = breakToken.inlineBreakToken ? &*breakToken.inlineBreakToken : nullptr;
    m_blockLevelBoxBreakTokenToResume = breakToken.blockLevelBoxBreakToken.get();
    m_openBlockFormattingContextRootToResume = breakToken.openBlockFormattingContextRoot ? &*breakToken.openBlockFormattingContextRoot : nullptr;
    resumeAfterBreak(breakToken);
    CheckedRef blockContainer = *breakToken.parallelFlowBlockContainer;
    // The one of a block formatting context root continues in the root, which ended in a fragmentainer before and has no fragment in this one
    // either (addParallelFlowOfRoot).
    auto& startItem = blockItems[breakToken.blockItemIndex];
    auto isInRoot = startItem.type() == BlockItem::Type::IndependentFormattingContextRoot && &startItem.renderer() == blockContainer.ptr();
    auto outsideStateCount = size_t { 1 };
    if (isInRoot)
        outsideStateCount = containingBlockStates.size() + 1;
    else {
        while (containingBlockStates[outsideStateCount - 1].blockContainer.ptr() != blockContainer.ptr())
            ++outsideStateCount;
    }
    m_parallelFlow = ParallelFlow { blockContainer, outsideStateCount, blockLevelBox };
    if (auto blockOffset = breakToken.blockOffset) {
        m_blockLayoutState.marginCollapse().resetAdjoiningMargins(blockOffset, breakToken.marginsAfterOverflow);
        m_fragmentainerHasContent = true;
        // The margins after it do not adjoin the break, which is before it.
        m_marginsAdjoinUnforcedBreak = false;
    }
    if (isInRoot) {
        auto boxCount = result.boxes.size();
        layoutBlockItems(blockItems, result, breakToken.blockItemIndex, breakToken.blockItemIndex + 1);
        result.boxes.removeFirstMatching([&](auto& box) {
            return box.renderer.ptr() == blockContainer.ptr();
        }, boxCount);
    } else {
        auto endIndex = breakToken.blockItemIndex;
        while (blockItems[endIndex].type() != BlockItem::Type::BlockContainerEnd || &blockItems[endIndex].renderer() != blockContainer.ptr())
            ++endIndex;
        layoutBlockItems(blockItems, result, breakToken.blockItemIndex, endIndex);
    }
    m_parallelFlow = std::nullopt;
    containingBlockStates.shrink(1);
    m_blockLayoutState.marginCollapse() = WTF::move(marginCollapse);
    m_blockLayoutState.setTextBoxTrimState(WTF::move(textBoxTrimState));
}

// The flows parallel to the content after a block formatting context root whose own content ended in the fragmentainer, of the content
// overflowing the block containers with a block size of their own in it, continue in the next fragmentainer in the root, from its block item,
// in its ancestors, which have no fragment there either (layoutParallelFlow).
void BlockFormattingContext::addParallelFlowOfRoot(const RenderBox& root, size_t blockItemIndex, const BoxGeometry& rootGeometry, LayoutUnit rootLogicalTop, BlockBreakToken&& contentBreakToken)
{
    auto blockSize = m_fragmentainer->availableBlockSize();
    auto& containingBlockStates = m_blockLayoutState.containingBlockStates();
    auto breakToken = BlockBreakToken { blockItemIndex, { }, { }, false, { }, false };
    auto fragmentEnds = fragmentEndsOfOpenBlockContainers(blockSize);
    for (size_t index = 1; index < containingBlockStates.size(); ++index) {
        auto& state = containingBlockStates[index];
        auto borderBoxLogicalTop = state.borderBoxLogicalTop.value_or(0_lu);
        breakToken.openBlockContainers.append({ state.blockContainer, consumedBlockSizeThrough(state, std::max(0_lu, fragmentEnds[index].fragmentEnd - borderBoxLogicalTop), fragmentEnds[index].clonedBlockEnd) });
    }
    breakToken.parallelFlowBlockContainer = &root;
    auto consumedBlockSize = consumedBlockSizeThrough(std::nullopt, rootGeometry.borderAndPaddingBefore(), std::max(0_lu, blockSize - rootLogicalTop));
    breakToken.openBlockFormattingContextRoot = BlockBreakToken::OpenBlockFormattingContextRoot { consumedBlockSize, rootGeometry, makeUnique<BlockBreakToken>(WTF::move(contentBreakToken)), { } };
    m_parallelFlowBreakTokens.append(makeUniqueRef<BlockBreakToken>(WTF::move(breakToken)));
}

// Where the fragment of each block container open in the walk ends when the fragmentainer ends: above the block-end border and padding the
// fragments of the block containers around it that clone them have there (css-break-3 §5.4), but not those of the block containers outside a
// parallel flow, which have no fragment in this fragmentainer, nor those of one whose block size of its own ends it in this fragmentainer,
// whose block-end border and padding are not at a break, and which the content overflowing it goes past. A fragment leaves out the ones that
// would leave no room for its content, which then could not go on in the next fragmentainer: "if more room is still needed, cloned
// margins/border/padding at the block-end side are truncated" (css-break-3 §4.4). The last entry is where the content of the innermost one ends.
auto BlockFormattingContext::fragmentEndsOfOpenBlockContainers(LayoutUnit fragmentainerEnd) const -> Vector<FragmentEnd>
{
    auto& containingBlockStates = m_blockLayoutState.containingBlockStates();
    auto firstStateIndex = m_parallelFlow ? std::max<size_t>(1, m_parallelFlow->outsideStateCount) : 1;
    auto fragmentEnds = Vector<FragmentEnd>(containingBlockStates.size() + 1);
    for (size_t index = 0; index < containingBlockStates.size(); ++index) {
        fragmentEnds[index].fragmentEnd = fragmentainerEnd;
        auto& state = containingBlockStates[index];
        if (index < firstStateIndex || !clonesDecorations(state))
            continue;
        auto borderBoxLogicalTop = state.borderBoxLogicalTop.value_or(0_lu);
        if (auto blockSizeLeft = blockSizeLeftOfItsOwn(state, containingBlockStates[index - 1]); blockSizeLeft && state.borderBoxLogicalTop && borderBoxLogicalTop + *blockSizeLeft <= fragmentainerEnd)
            continue;
        auto clonedBlockEnd = state.geometry.borderAndPaddingAfter();
        if (fragmentainerEnd - clonedBlockEnd <= borderBoxLogicalTop + state.borderAndPaddingBefore)
            continue;
        fragmentEnds[index].clonedBlockEnd = clonedBlockEnd;
        fragmentainerEnd -= clonedBlockEnd;
    }
    fragmentEnds.last().fragmentEnd = fragmentainerEnd;
    return fragmentEnds;
}

// The end of the fragmentainer for the content at the current position of the walk, in the block containers open there.
LayoutUnit BlockFormattingContext::fragmentainerEndForContent() const
{
    return fragmentEndsOfOpenBlockContainers(m_fragmentainer->availableBlockSize()).last().fragmentEnd;
}

// What a block size leaves of a block container that clones its block-start and block-end border and padding (css-break-3 §5.4), each of its
// fragments having all of them: of its content box, or, with box-sizing: border-box and a block size of its own, of its border box, which the
// cloned border and padding of the fragments before took their share of.
static LayoutUnit clonedFragmentBlockSize(const RenderBox& blockContainer, const BoxGeometry& wholeGeometry, const ContainingBlockState::ConsumedBlockSize& consumedBlockSize)
{
    auto borderAndPadding = wholeGeometry.verticalBorderAndPadding();
    if (blockContainer.style().boxSizing() == BoxSizing::BorderBox && hasBlockSizeOfItsOwn(blockContainer))
        return std::max(borderAndPadding, wholeGeometry.borderBoxHeight() - consumedBlockSize.borderBox);
    return borderAndPadding + std::max(0_lu, wholeGeometry.contentBoxHeight() - consumedBlockSize.contentBox);
}

// A block container with a block size of its own, which its content does not give it, ends where that block size puts it, also when its
// content goes on past the end of the fragmentainer.
// What the block size of its own leaves of a block container after the fragmentainers before, with its content going on past the end of this
// one.
std::optional<LayoutUnit> BlockFormattingContext::blockSizeLeftOfItsOwn(const ContainingBlockState& blockContainerState, const ContainingBlockState& parentState) const
{
    CheckedRef blockContainer = blockContainerState.blockContainer;
    if (!hasBlockSizeOfItsOwn(blockContainer) || blockContainerState.isLaidOutThroughRenderTree)
        return { };
    auto contentBlockSizePastEnd = LayoutUnit::max() / 4;
    auto geometry = blockContainerState.geometry;
    if (auto& consumedBlockSize = blockContainerState.consumedBlockSize; consumedBlockSize && consumedBlockSize->clonesDecorations) {
        computeBlockSize(blockContainer, contentBlockSizePastEnd + consumedBlockSize->contentBox, parentState, geometry);
        return clonedFragmentBlockSize(blockContainer, geometry, *consumedBlockSize);
    }
    auto consumedBorderBoxBlockSize = 0_lu;
    if (auto& consumedBlockSize = blockContainerState.consumedBlockSize) {
        geometry.setVerticalPadding({ consumedBlockSize->borderAndPaddingBefore, geometry.paddingAfter() });
        contentBlockSizePastEnd += consumedBlockSize->contentBox;
        consumedBorderBoxBlockSize = consumedBlockSize->borderBox;
    }
    computeBlockSize(blockContainer, contentBlockSizePastEnd, parentState, geometry);
    return std::max(0_lu, geometry.borderBoxHeight() - consumedBorderBoxBlockSize);
}

// The column spanner the multi-column line before this one ended at, whose placeholder and the ones before it the walk has passed.
const RenderBox* BlockFormattingContext::lastColumnSpannerPlaceholder() const
{
    if (!m_incomingBreakToken)
        return nullptr;
    return m_incomingBreakToken->columnSpannerPlaceholder ? m_incomingBreakToken->columnSpannerPlaceholder.get() : m_incomingBreakToken->lastColumnSpannerPlaceholder.get();
}

// Not one with a column spanner still to come in its content, which ends the multi-column line: the content after the block container comes
// after the spanner (css-multicol-1 §6.1), not alongside the overflow.
bool BlockFormattingContext::endsInFragmentainer(const ContainingBlockState& blockContainerState, const ContainingBlockState& parentState, LayoutUnit fragmentainerEnd) const
{
    auto blockSizeLeft = blockSizeLeftOfItsOwn(blockContainerState, parentState);
    if (!blockSizeLeft || *blockContainerState.borderBoxLogicalTop + *blockSizeLeft > fragmentainerEnd)
        return false;
    // The placeholders in the content come in tree order, those up to the last one the walk passed behind it.
    CheckedRef blockContainer = blockContainerState.blockContainer;
    CheckedPtr passedPlaceholder = lastColumnSpannerPlaceholder();
    auto isBehind = passedPlaceholder && passedPlaceholder->isDescendantOf(blockContainer.ptr());
    for (auto& placeholder : descendantsOfType<RenderMultiColumnSpannerPlaceholder>(blockContainer.get())) {
        if (!isBehind)
            return false;
        isBehind = &placeholder != passedPlaceholder.get();
    }
    return true;
}

static PlacedFloats::Item movedFloat(const PlacedFloats::Item&, LayoutSize offset, const Box* layoutBox);

// The floats taller than the rest of the fragmentainer, which overflow into the next one, in its coordinates.
PlacedFloats::List BlockFormattingContext::floatsOverflowingFragmentainer(LayoutUnit fragmentainerBlockSize) const
{
    auto floats = PlacedFloats::List { };
    if (auto* placedFloats = m_blockLayoutState.placedFloats()) {
        for (auto& floatItem : placedFloats->list()) {
            if (floatItem.absoluteRectWithMargin().bottom() > fragmentainerBlockSize)
                floats.append(movedFloat(floatItem, { 0_lu, -fragmentainerBlockSize }, floatItem.layoutBox()));
        }
    }
    return floats;
}

// The walk stops at a break: the block containers open there fill the rest of the fragmentainer, and the next fragmentainer resumes from the
// block item and those block containers (css-break-3 §4.1). A column of a multi-column line that ends at a column spanner ends where its
// content does, and so does a fragmentainer that ends inside a block formatting context root, where the root's content ends in it. A break in
// the content of a block container with a block size of its own whose end is in the fragmentainer, the innermost one, only ends the flow of
// its content, which continues in the next fragmentainer, and the walk goes on from the block container's end (css-break-3 §2.1), where the
// content after it may break in another one around it.
BlockBreakToken& BlockFormattingContext::endFragmentainer(size_t blockItemIndex, bool isForced, BlockLayoutResult& result, std::optional<LayoutUnit> contentLogicalBottom, std::optional<LayoutUnit> innermostContentBoxLogicalBottom)
{
    auto blockSize = contentLogicalBottom.value_or(m_fragmentainer->blockSize ? m_fragmentainer->availableBlockSize() : m_blockLayoutState.marginCollapse().logicalTop());
    // Not below monolithic content, whose overflow takes no room in the next fragmentainer.
    auto blockOffset = contentLogicalBottom || m_contentEndsWithMonolithicOverflow ? 0_lu : std::max(0_lu, m_blockLayoutState.marginCollapse().logicalTop() - blockSize);
    auto breakToken = BlockBreakToken { blockItemIndex, { }, { }, isForced, blockOffset, m_blockLayoutState.textBoxTrimState().trimsFirstFormattedLine };
    breakToken.lastColumnSpannerPlaceholder = lastColumnSpannerPlaceholder();
    if (blockOffset)
        breakToken.marginsAfterOverflow = m_blockLayoutState.marginCollapse().adjoiningMargins();
    if (m_blockStepExtraSpaceAtBreak && m_blockStepExtraSpaceAtBreak->first == blockItemIndex)
        breakToken.blockStepExtraSpace = m_blockStepExtraSpaceAtBreak->second;
    auto& containingBlockStates = m_blockLayoutState.containingBlockStates();
    // The fragment of a block container fills the rest of the fragmentainer, above the cloned block-end border and padding of those around it,
    // but no more than its block size of its own leaves, which one with a column spanner in it that ends there does not take a parallel flow
    // for (endsInFragmentainer).
    auto fragmentEnds = fragmentEndsOfOpenBlockContainers(blockSize);
    auto fragmentBlockSize = [&](size_t stateIndex, const ContainingBlockState& state, const ContainingBlockState& parentState, LayoutUnit borderBoxLogicalTop) {
        auto fragmentBlockSize = std::max(0_lu, fragmentEnds[stateIndex].fragmentEnd - borderBoxLogicalTop);
        if (auto blockSizeLeft = blockSizeLeftOfItsOwn(state, parentState))
            return std::min(fragmentBlockSize, *blockSizeLeft);
        return fragmentBlockSize;
    };
    auto consumedBlockSize = [&](size_t stateIndex, const ContainingBlockState& state, const ContainingBlockState& parentState, LayoutUnit borderBoxLogicalTop) {
        return consumedBlockSizeThrough(state, fragmentBlockSize(stateIndex, state, parentState, borderBoxLogicalTop), fragmentEnds[stateIndex].clonedBlockEnd);
    };
    // In a parallel flow, the block container it is the content of and its ancestors have no fragment in this fragmentainer. In the walk of a
    // block-level box in inline content, the walk around it goes on in the inline content, which ends the fragmentainer with the line of the
    // box (InlineBreakToken::continuesBlockContent).
    auto outsideStateCount = m_parallelFlow ? m_parallelFlow->outsideStateCount : 1;
    auto walkStateCount = m_blockLevelBoxOutsideStateCount.value_or(1);
    // The innermost one resolves those of the ones around it waiting for theirs.
    if (containingBlockStates.size() > 1 && !containingBlockStates.last().borderBoxLogicalTop)
        resolveBorderBoxLogicalTops(containingBlockStates.last());
    // In the walk of a block-level box in inline content, only the box and the boxes in it, whose overflow goes on alongside the inline content
    // after the box.
    auto parallelFlowStateIndex = std::optional<size_t> { };
    if (m_fragmentainer->blockSize) {
        auto lowestStateIndex = std::max(outsideStateCount, m_blockLevelBoxOutsideStateCount.value_or(0));
        for (auto index = containingBlockStates.size(); index-- > lowestStateIndex && !parallelFlowStateIndex;) {
            if (endsInFragmentainer(containingBlockStates[index], containingBlockStates[index - 1], fragmentEnds[index].fragmentEnd))
                parallelFlowStateIndex = index;
        }
    }
    // The innermost one ends its content box above the end of the fragmentainer when only its block-end padding and border cross it, which
    // take the rest of the fragmentainer, not its content, unless it clones them, which its fragment has besides its content.
    auto blockEndInFragmentainer = innermostContentBoxLogicalBottom && !fragmentEnds[containingBlockStates.size() - 1].clonedBlockEnd ? std::max(0_lu, blockSize - *innermostContentBoxLogicalBottom) : 0_lu;
    while (containingBlockStates.size() > (parallelFlowStateIndex ? *parallelFlowStateIndex + 1 : walkStateCount)) {
        auto stateIndex = containingBlockStates.size() - 1;
        auto blockContainerState = containingBlockStates.takeLast();
        auto& containingBlockState = containingBlockStates.last();
        auto borderBoxLogicalTop = *blockContainerState.borderBoxLogicalTop;
        auto consumed = consumedBlockSize(stateIndex, blockContainerState, containingBlockState, borderBoxLogicalTop);
        consumed.contentBox -= std::min(consumed.contentBox, std::exchange(blockEndInFragmentainer, 0_lu));
        breakToken.openBlockContainers.insert(0, { blockContainerState.blockContainer, consumed });
        // The walk goes on without them: what their starts set up for their content ends with them.
        if (parallelFlowStateIndex)
            endBlockContainerContent(blockContainerState.blockContainer, blockContainerState);
        if (containingBlockStates.size() < outsideStateCount)
            continue;
        auto geometry = blockContainerState.geometry;
        sliceAtFragmentainerEnd(geometry, fragmentBlockSize(stateIndex, blockContainerState, containingBlockState, borderBoxLogicalTop), !!fragmentEnds[stateIndex].clonedBlockEnd);
        geometry.setTop(borderBoxLogicalTop - containingBlockState.borderBoxLogicalTop.value_or(0_lu));
        result.boxes.append({ blockContainerState.blockContainer, WTF::move(geometry), { }, blockContainerState.margins.collapsedMarginsBefore(), { }, blockContainerState.margins.collapsedMarginsBeforeAreQuirky(), false, false });
    }
    if (parallelFlowStateIndex) {
        // They have no fragment in the parallel flow, nor the block-start border and padding of one.
        for (auto index = *parallelFlowStateIndex; index; --index) {
            auto& state = containingBlockStates[index];
            auto consumed = consumedBlockSize(index, state, containingBlockStates[index - 1], *state.borderBoxLogicalTop);
            consumed.clonesDecorations = false;
            breakToken.openBlockContainers.insert(0, { state.blockContainer, consumed });
        }
        CheckedRef blockContainer = containingBlockStates[*parallelFlowStateIndex].blockContainer;
        breakToken.parallelFlowBlockContainer = blockContainer.ptr();
        if (m_blockLevelBoxOutsideStateCount)
            breakToken.parallelFlowBlockLevelBox = containingBlockStates[*m_blockLevelBoxOutsideStateCount].blockContainer.ptr();
        else if (m_parallelFlow)
            breakToken.parallelFlowBlockLevelBox = m_parallelFlow->blockLevelBox;
        // Its content goes on past the end of the fragmentainer, and the margins adjoining the break are truncated.
        m_blockLayoutState.marginCollapse().resetAdjoiningMargins(std::max(m_blockLayoutState.marginCollapse().logicalTop(), blockSize));
        m_parallelFlowBreak = ParallelFlowBreak { blockContainer, blockItemIndex };
        m_parallelFlowBreakTokens.append(makeUniqueRef<BlockBreakToken>(WTF::move(breakToken)));
        return m_parallelFlowBreakTokens.last();
    }
    if (m_blockLevelBoxOutsideStateCount) {
        m_blockLevelBoxBreakToken = makeUnique<BlockBreakToken>(WTF::move(breakToken));
        return *m_blockLevelBoxBreakToken;
    }
    if (m_parallelFlow) {
        breakToken.parallelFlowBlockContainer = m_parallelFlow->blockContainer.ptr();
        breakToken.parallelFlowBlockLevelBox = m_parallelFlow->blockLevelBox;
        m_parallelFlowBreakTokens.append(makeUniqueRef<BlockBreakToken>(WTF::move(breakToken)));
        return m_parallelFlowBreakTokens.last();
    }
    result.fragmentainerBlockSize = blockSize;
    breakToken.rootConsumedBlockSize = consumedBlockSizeThrough(containingBlockStates.last().consumedBlockSize, containingBlockStates.last().geometry.borderAndPaddingBefore(), std::max(0_lu, blockSize));
    breakToken.pushedFloats = m_pushedFloats;
    breakToken.overflowingFloats = floatsOverflowingFragmentainer(blockSize);
    // The floats that go to the next fragmentainer or continue there go in the fragments of the block containers around them that continue
    // there.
    auto openBlockContainersAround = [&](const RenderBox& floatBox) {
        auto openBlockContainers = Vector<BlockBreakToken::OpenBlockContainer> { };
        for (auto& openBlockContainer : breakToken.openBlockContainers) {
            if (floatBox.isDescendantOf(openBlockContainer.blockContainer.ptr()))
                openBlockContainers.append(openBlockContainer);
        }
        return openBlockContainers;
    };
    for (auto& pushedFloat : breakToken.pushedFloats)
        pushedFloat.openBlockContainers = openBlockContainersAround(pushedFloat.box);
    for (auto& continuingFloat : m_continuingFloats) {
        continuingFloat.openBlockContainers = openBlockContainersAround(continuingFloat.box);
        breakToken.overflowingFloats.removeAllMatching([&](auto& floatItem) {
            return floatItem.layoutBox() == &continuingFloat.box->layoutBox();
        });
    }
    breakToken.continuingFloats = std::exchange(m_continuingFloats, { });
    breakToken.parallelFlows = std::exchange(m_parallelFlowBreakTokens, { });
    result.breakToken = WTF::move(breakToken);
    return *result.breakToken;
}

// The multi-column line ends at a column spanner in a block-level box in inline content, after the line of the box, where the inline content
// continues in the next one, or after the inline content, when the spanner ended it.
void BlockFormattingContext::endMultiColumnLineInInlineContent(const ColumnSpannerBreak& columnSpannerBreak, std::optional<InlineContentBreak>&& inlineContentBreak, size_t inlineContentIndex, BlockLayoutResult& result)
{
    auto& breakToken = endFragmentainer(inlineContentBreak ? inlineContentIndex : inlineContentIndex + 1, true, result, columnSpannerBreak.multiColumnLineLogicalBottom);
    breakToken.columnSpannerPlaceholder = columnSpannerBreak.placeholder.ptr();
    if (!inlineContentBreak)
        return;
    breakToken.inlineBreakToken = WTF::move(inlineContentBreak->breakToken);
    breakToken.blockLevelBoxBreakToken = WTF::move(inlineContentBreak->blockLevelBoxBreakToken);
}

// Each multi-column line of a multi-column container is a block-level box of its own (css-multicol-1 §2), whose content ends below the content
// before the spanner, its floats included, like the content of a block formatting context root (CSS 2.2 §10.6.7), which the clear: both of
// the placeholder does in the render tree path (RenderMultiColumnSpannerPlaceholder::createAnonymous). The placeholder sits there, without a
// block size.
LayoutUnit BlockFormattingContext::placeColumnSpannerPlaceholder(const RenderBox& placeholder, BlockLayoutResult& result)
{
    auto& containingBlockState = m_blockLayoutState.containingBlockStates().last();
    if (!containingBlockState.borderBoxLogicalTop)
        resolveBorderBoxLogicalTops(containingBlockState);
    auto logicalBottom = m_blockLayoutState.marginCollapse().collapsedLogicalBottom();
    if (auto* placedFloats = m_blockLayoutState.placedFloats()) {
        if (auto floatsLogicalBottom = placedFloats->lowestPositionOnBlockAxis())
            logicalBottom = std::max(logicalBottom, *floatsLogicalBottom);
    }
    // It has no size: the render tree does not lay it out.
    auto geometry = BoxGeometry { };
    geometry.setTopLeft({ containingBlockState.inlineRange.min(), logicalBottom - *containingBlockState.borderBoxLogicalTop });
    geometry.setContentBoxSize({ });
    result.boxes.append({ placeholder, WTF::move(geometry), { }, { }, { }, false, false, false });
    return logicalBottom;
}

void BlockFormattingContext::layoutBlockItems(const BlockItemList& blockItems, BlockLayoutResult& result, size_t startIndex, std::optional<size_t> endIndex)
{
    // The content of a block container with a line-clamp budget of its own that ends at its clamp point is laid out again without the budget,
    // from the state the block formatting context had at its start (RenderBlockFlow::layoutInFlowChildren).
    struct ClampedContent {
        CheckedRef<const RenderBox> blockContainer;
        size_t startIndex { 0 };
        BlockLayoutState blockLayoutState;
        size_t boxCount { 0 };
        size_t outOfFlowBoxCount { 0 };
        size_t invisibleBoxCount { 0 };
    };
    auto clampedContents = Vector<ClampedContent> { };
    auto blockContainerEndIndex = [&](size_t startIndex) {
        auto index = startIndex + 1;
        while (blockItems[index].type() != BlockItem::Type::BlockContainerEnd || &blockItems[index].renderer() != &blockItems[startIndex].renderer())
            ++index;
        return index;
    };
    // Fragmentation: where the walk was at the start of each block container open in this fragmentainer, and before the box it lays out, to go
    // back to when the box does not fit (css-break-3 §4).
    struct BreakPoint {
        CheckedRef<const RenderBox> box;
        size_t index { 0 };
        BlockLayoutState blockLayoutState;
        size_t boxCount { 0 };
        size_t outOfFlowBoxCount { 0 };
        size_t invisibleBoxCount { 0 };
        size_t inlineContentFragmentCount { 0 };
        size_t pushedFloatCount { 0 };
        size_t continuingFloatCount { 0 };
        size_t floatBoxCount { 0 };
        size_t parallelFlowBreakTokenCount { 0 };
        bool fragmentainerHadContent { false };
        bool marginsAdjoinedUnforcedBreak { false };
    };
    auto blockContainerBreakPoints = Vector<BreakPoint> { };
    auto breakPointAt = [&](const RenderBox& box, size_t index) {
        return BreakPoint { box, index, m_blockLayoutState, result.boxes.size(), result.outOfFlowBoxes.size(), result.invisibleBoxes.size(), result.inlineContentFragments.size(), m_pushedFloats.size(), m_continuingFloats.size(), m_floatBoxCount, m_parallelFlowBreakTokens.size(), m_fragmentainerHasContent, m_marginsAdjoinUnforcedBreak };
    };
    auto goBackTo = [&](BreakPoint& breakPoint) {
        m_blockLayoutState = WTF::move(breakPoint.blockLayoutState);
        result.boxes.shrink(breakPoint.boxCount);
        result.outOfFlowBoxes.shrink(breakPoint.outOfFlowBoxCount);
        result.invisibleBoxes.shrink(breakPoint.invisibleBoxCount);
        result.inlineContentFragments.shrink(breakPoint.inlineContentFragmentCount);
        m_pushedFloats.shrink(breakPoint.pushedFloatCount);
        m_continuingFloats.shrink(breakPoint.continuingFloatCount);
        m_floatBoxCount = breakPoint.floatBoxCount;
        m_parallelFlowBreakTokens.shrink(breakPoint.parallelFlowBreakTokenCount);
        m_fragmentainerHasContent = breakPoint.fragmentainerHadContent;
        m_marginsAdjoinUnforcedBreak = breakPoint.marginsAdjoinedUnforcedBreak;
    };
    // An unforced break before the first box in a block container goes before the block container: there is no break point between the
    // content edge and the first box without a gap between them (css-break-3 §4.1, RenderBlockFlow::adjustBlockChildForPagination propagates
    // the strut), also past floats before the box, which take no space and go with it, unless the box went below floats, clearing them or
    // avoiding them: that leaves a gap, where a break may go (RenderBlockFlow::adjustBlockChildForPagination propagates the strut of a child
    // at the top of the block only). This is the outermost block container that starts at the break point, if any.
    auto startsBelowContentEdge = [&](const RenderBox& child, const RenderBox& container) {
        auto& states = m_blockLayoutState.containingBlockStates();
        auto stateOf = [&](const RenderBox& box) -> const ContainingBlockState* {
            auto index = states.findIf([&](auto& state) {
                return state.blockContainer.ptr() == &box;
            });
            return index == notFound ? nullptr : &states[index];
        };
        auto* containerState = stateOf(container);
        if (!containerState)
            return false;
        // A block container still open, whose top is in the root's coordinates, or the box laid out last, relative to its containing block.
        if (auto* childState = stateOf(child))
            return childState->borderBoxLogicalTop && containerState->borderBoxLogicalTop && *childState->borderBoxLogicalTop - childState->geometry.marginBefore() > containerState->contentBoxLogicalTop();
        if (!result.boxes.isEmpty() && result.boxes.last().renderer.ptr() == &child) {
            auto& geometry = result.boxes.last().geometry;
            return BoxGeometry::borderBoxTop(geometry) - geometry.marginBefore() > containerState->borderAndPaddingBefore;
        }
        // Before its layout, where clearance would take it below them.
        return child.style().clear() != Clear::None;
    };
    auto propagatedBreakPointIndex = [&](BreakPoint& breakPoint) {
        auto propagatedIndex = std::optional<size_t> { };
        CheckedPtr containerBox = breakPoint.blockLayoutState.containingBlockStates().last().blockContainer.ptr();
        auto* childBreakPoint = &breakPoint;
        for (auto index = blockContainerBreakPoints.size(); index--;) {
            auto& containerBreakPoint = blockContainerBreakPoints[index];
            auto hasBoxesBefore = containerBreakPoint.boxCount - containerBreakPoint.floatBoxCount != breakPoint.boxCount - breakPoint.floatBoxCount;
            auto* placedFloats = m_blockLayoutState.placedFloats();
            auto hasFloats = containerBreakPoint.floatBoxCount != childBreakPoint->floatBoxCount || (placedFloats && !placedFloats->isEmpty()) || !m_pushedFloats.isEmpty();
            auto isBelowFloatsBefore = hasFloats && startsBelowContentEdge(childBreakPoint->box, containerBreakPoint.box);
            if (containerBox.get() != containerBreakPoint.box.ptr() || hasBoxesBefore || isBelowFloatsBefore || !containerBreakPoint.fragmentainerHadContent)
                break;
            propagatedIndex = index;
            childBreakPoint = &containerBreakPoint;
            containerBox = containerBreakPoint.blockLayoutState.containingBlockStates().last().blockContainer.ptr();
        }
        return propagatedIndex;
    };
    // The last break point before a block-level box that rule 1 allows, where a break it does not allow goes instead (css-break-3 §4.4).
    auto lastAllowedBreakPoint = std::optional<BreakPoint> { };
    auto isBlockLevelBoxStart = [&](size_t index) {
        return blockItems[index].type() == BlockItem::Type::BlockContainerStart || blockItems[index].type() == BlockItem::Type::IndependentFormattingContextRoot;
    };
    // After a break in the content of a block container with a block size of its own whose end is in the fragmentainer (endFragmentainer),
    // the walk goes on from the end of the block container, without the break points in its content.
    auto parallelFlowEndIndex = [&] -> std::optional<size_t> {
        auto parallelFlowBreak = std::exchange(m_parallelFlowBreak, std::nullopt);
        if (!parallelFlowBreak)
            return std::nullopt;
        CheckedRef blockContainer = parallelFlowBreak->blockContainer;
        auto index = parallelFlowBreak->blockItemIndex;
        while (blockItems[index].type() != BlockItem::Type::BlockContainerEnd || &blockItems[index].renderer() != blockContainer.ptr())
            ++index;
        while (!blockContainerBreakPoints.isEmpty() && blockContainerBreakPoints.last().box.ptr() != blockContainer.ptr())
            blockContainerBreakPoints.removeLast();
        auto blockContainerStartIndex = blockContainerBreakPoints.isEmpty() ? 0 : blockContainerBreakPoints.last().index;
        if (lastAllowedBreakPoint && lastAllowedBreakPoint->index > blockContainerStartIndex)
            lastAllowedBreakPoint = std::nullopt;
        return index;
    };
    auto breakBefore = [&](BreakPoint&& breakPoint, bool isForced) -> std::optional<size_t> {
        if (!isForced) {
            if (auto propagatedIndex = propagatedBreakPointIndex(breakPoint)) {
                breakPoint = WTF::move(blockContainerBreakPoints[*propagatedIndex]);
                blockContainerBreakPoints.shrink(*propagatedIndex);
            }
            if (isBlockLevelBoxStart(breakPoint.index) && avoidsBreakBefore(breakPoint.box, m_fragmentainer->breakType()) && lastAllowedBreakPoint)
                breakPoint = *std::exchange(lastAllowedBreakPoint, std::nullopt);
        }
        auto index = breakPoint.index;
        goBackTo(breakPoint);
        endFragmentainer(index, isForced, result);
        if (auto endIndex = parallelFlowEndIndex())
            return endIndex;
        if (!m_blockLevelBoxOutsideStateCount)
            result.isBreakBeforeContent = !m_incomingBreakToken && !index;
        return std::nullopt;
    };
    // A box below the end of the fragmentainer goes to the next one, also with no content before it when its block-start margins cross the end:
    // they adjoin the break, and are truncated in the next one (css-break-3 §5.2).
    auto isBelowFragmentainerEnd = [](BreakPoint& breakPoint, LayoutUnit logicalTop, LayoutUnit blockSize) {
        return logicalTop >= blockSize && (breakPoint.fragmentainerHadContent || breakPoint.blockLayoutState.marginCollapse().logicalTop() < blockSize);
    };
    // The border box of the box last added to the result, in the root's block coordinates.
    auto lastBoxLogicalTopAndBottom = [&] {
        auto& geometry = result.boxes.last().geometry;
        auto logicalTop = m_blockLayoutState.containingBlockStates().last().borderBoxLogicalTop.value_or(0_lu) + BoxGeometry::borderBoxTop(geometry);
        return std::pair { logicalTop, logicalTop + geometry.borderBoxHeight() };
    };

    for (size_t index = startIndex; index < endIndex.value_or(blockItems.size()); ++index) {
        auto& blockItem = blockItems[index];
        CheckedRef box = blockItem.renderer();
        // The content of a list item with an outside marker may hold the marker's first formatted line, also inside a box laid out on its own.
        auto listItemsWithOutsideMarkerScope = ListItemOutsideMarkerScope { root().view().frameView().layoutContext(), LayoutIntegration::BlockIntegrationUtils::listItemsWithOutsideMarker(m_blockLayoutState.containingBlockStates()) };
        // The states of the block containers below the root around the block item, the one it ends included.
        auto containingBlockStates = m_blockLayoutState.containingBlockStates().span().subspan(1);
        // The multi-column line ends there, unless floats that went to the next column or continue there, or content past the end of this one,
        // take it on into the next column. The margins after a spanner stay, like those at the start of a block formatting context root.
        if (blockItem.type() == BlockItem::Type::ColumnSpannerPlaceholder) {
            auto breakPoint = breakPointAt(box, index);
            auto multiColumnLineLogicalBottom = placeColumnSpannerPlaceholder(box, result);
            if (!m_pushedFloats.isEmpty() || hasFloatsContinuingInNextFragmentainer() || (breakPoint.fragmentainerHadContent && multiColumnLineLogicalBottom > fragmentainerEndForContent())) {
                if (auto endIndex = breakBefore(WTF::move(breakPoint), false)) {
                    index = *endIndex - 1;
                    continue;
                }
                return;
            }
            auto& breakToken = endFragmentainer(index + 1, true, result, multiColumnLineLogicalBottom);
            breakToken.columnSpannerPlaceholder = box.ptr();
            if (m_blockLevelBoxOutsideStateCount)
                m_columnSpannerBreakInBlockLevelBox = ColumnSpannerBreak { box, multiColumnLineLogicalBottom };
            // The content overflowing block containers with a block size of their own goes on in more columns of the column set past its last
            // one, where it is in the strip of the columns, not after the spanner (RenderMultiColumnSet::columnIndexAtOffset).
            if (m_fragmentainer->blockSize && !breakToken.parallelFlows.isEmpty()) {
                auto parallelFlowsBreakToken = makeUnique<BlockBreakToken>(BlockBreakToken { blockItems.size(), { }, { }, false, { }, false });
                parallelFlowsBreakToken->parallelFlows = std::exchange(breakToken.parallelFlows, { });
                parallelFlowsBreakToken->hasOnlyParallelFlows = true;
                breakToken.parallelFlowsPastLastColumn = WTF::move(parallelFlowsBreakToken);
            }
            ASSERT(!m_parallelFlowBreak);
            return;
        }
        if (m_fragmentainer && (blockItem.type() == BlockItem::Type::BlockContainerStart || blockItem.type() == BlockItem::Type::IndependentFormattingContextRoot) && breaksInside(containingBlockStates)) {
            // A box that clears floats that went to the next fragmentainer goes there too, below them, but not one that clears only the other
            // side (CSS 2.2 §9.5.2).
            // Not where no content took block size in the fragmentainer yet, zero-height boxes included, like in the render tree (the next page
            // after the top of a page is that page, RenderBlockFlow::nextPageLogicalTop).
            auto isForcedBreak = m_fragmentainerHasContent && m_fragmentainer->appliesForcedBreaks && hasForcedBreakBefore(box, m_fragmentainer->breakType())
                && m_blockLayoutState.marginCollapse().logicalTop() > -m_fragmentainer->rootBlockOffset;
            auto clearsPushedFloat = [&] {
                auto clear = box->usedStyle().clear();
                return m_pushedFloats.containsIf([&](auto& pushedFloat) {
                    return clear == UsedClear::Both || (clear == UsedClear::Left) == (pushedFloat.box->usedStyle().floating() == UsedFloat::Left);
                });
            };
            if (isForcedBreak || (box->usedStyle().clear() != UsedClear::None && clearsPushedFloat())) {
                if (auto endIndex = breakBefore(breakPointAt(box, index), isForcedBreak)) {
                    index = *endIndex - 1;
                    continue;
                }
                return;
            }
            auto breakPoint = breakPointAt(box, index);
            if (m_fragmentainerHasContent && !propagatedBreakPointIndex(breakPoint) && !avoidsBreakBefore(box, m_fragmentainer->breakType()))
                lastAllowedBreakPoint = breakPoint;
            if (blockItem.type() == BlockItem::Type::BlockContainerStart) {
                blockContainerBreakPoints.append(WTF::move(breakPoint));
                handleBlockContainerStart(box);
                // Its block-start border and padding separate the margins inside it from the break.
                if (m_blockLayoutState.containingBlockStates().last().geometry.borderAndPaddingBefore())
                    m_marginsAdjoinUnforcedBreak = false;
                continue;
            }
            auto* resumedRoot = m_openBlockFormattingContextRootToResume;
            auto rootContentBreak = handleIndependentFormattingContextRoot(box, result);
            // The extra space of the block step of the root goes to the next fragmentainer with it.
            auto breakBeforeRoot = [&, blockStepExtraSpace = std::exchange(m_rootBlockStepExtraSpace, std::nullopt)](BreakPoint&& breakPoint) {
                if (blockStepExtraSpace)
                    m_blockStepExtraSpaceAtBreak = std::pair { index, *blockStepExtraSpace };
                auto endIndex = breakBefore(WTF::move(breakPoint), false);
                m_blockStepExtraSpaceAtBreak = std::nullopt;
                return endIndex;
            };
            handleBlockLevelBoxEnd(box);
            auto [logicalTop, logicalBottom] = lastBoxLogicalTopAndBottom();
            auto blockSize = fragmentainerEndForContent();
            if (rootContentBreak) {
                // A block formatting context root whose content breaks goes to the next fragmentainer when none of its content fits below
                // the content before it, and otherwise breaks inside like a block container, when its content does, or when it crosses the
                // end of the fragmentainer (css-break-3 §4.1).
                if (isBelowFragmentainerEnd(breakPoint, logicalTop, blockSize) || (breakPoint.fragmentainerHadContent && rootContentBreak->isBreakBeforeContent)) {
                    if (auto endIndex = breakBeforeRoot(WTF::move(breakPoint))) {
                        index = *endIndex - 1;
                        continue;
                    }
                    return;
                }
                auto& geometry = result.boxes.last().geometry;
                auto& contentBreakToken = rootContentBreak->breakToken;
                // Only the content overflowing block containers with a block size of their own in it goes on past the end of the fragmentainer:
                // the root ends where its content does, and that content is a flow parallel to the content after the root (css-break-3 §2.1).
                if (contentBreakToken && contentBreakToken->hasOnlyParallelFlows) {
                    addParallelFlowOfRoot(box, index, geometry, logicalTop, WTF::move(*contentBreakToken));
                    contentBreakToken = std::nullopt;
                }
                if (contentBreakToken || (logicalBottom > blockSize && logicalTop < blockSize && logicalBottom - geometry.borderAfter() - geometry.paddingAfter() > logicalTop)) {
                    auto fragmentainerEnd = contentBreakToken ? logicalTop + rootContentBreak->fragmentainerBlockSize : blockSize;
                    auto openRoot = BlockBreakToken::OpenBlockFormattingContextRoot { consumedBlockSizeThrough(resumedRoot ? std::optional { resumedRoot->consumedBlockSize } : std::nullopt, geometry.borderAndPaddingBefore(), fragmentainerEnd - logicalTop), geometry, { }, { } };
                    auto isForced = contentBreakToken && contentBreakToken->isForced;
                    if (contentBreakToken)
                        openRoot.contentBreakToken = makeUnique<BlockBreakToken>(WTF::move(*contentBreakToken));
                    else {
                        // Its content continues below the rest of its block-start border and padding.
                        auto contentLogicalTop = logicalTop + (resumedRoot ? resumedRoot->consumedBlockSize.borderAndPaddingBeforeLeft() : geometry.borderAndPaddingBefore());
                        auto contentLogicalBottom = contentLogicalTop + result.boxes.last().intrinsicContentBlockSize;
                        openRoot.overflowingContentBlockSize = std::max(0_lu, contentLogicalBottom - std::max(blockSize, contentLogicalTop));
                    }
                    sliceAtFragmentainerEnd(geometry, fragmentainerEnd - logicalTop);
                    endFragmentainer(index, isForced, result, fragmentainerEnd).openBlockFormattingContextRoot = WTF::move(openRoot);
                    if (auto endIndex = parallelFlowEndIndex()) {
                        index = *endIndex - 1;
                        continue;
                    }
                    return;
                }
                m_fragmentainerHasContent = true;
                m_marginsAdjoinUnforcedBreak = false;
                continue;
            }
            // A box whose content the render tree broke across the pages crosses their ends where it is, and the content after it goes below it,
            // unless its first lines do not fit below the content before it, for which the render tree gives it a pagination strut that takes it
            // to the next fragmentainer (RenderBlockFlow::adjustLinePositionForPagination and adjustBlockChildForPagination).
            if (breaksContentThroughRenderTree(box) && logicalTop < blockSize) {
                if (CheckedPtr block = dynamicDowncast<RenderBlock>(box); block && block->paginationStrut() && breakPoint.fragmentainerHadContent) {
                    if (auto endIndex = breakBeforeRoot(WTF::move(breakPoint))) {
                        index = *endIndex - 1;
                        continue;
                    }
                    return;
                }
                m_fragmentainerHasContent = true;
                m_marginsAdjoinUnforcedBreak = false;
                continue;
            }
            // Content that does not fit goes to the next fragmentainer, unless it is taller than one, but not monolithic content in a column,
            // which overflows the next one less from its start, and paints whole there (css-break-3 §4.4, paintsColumnsFromFragments).
            // A flex container the render tree moves like content that cannot be split does not make balanced columns as tall as it is, which only
            // such content does (RenderBlockFlow::adjustForUnsplittableChild updates the minimum page height for it alone).
            if (box->type() != RenderObject::Type::FlexibleBox || isUnsplittableForPagination(box, m_fragmentainer->breakType()))
                addUnbreakableBlockSize(logicalBottom - logicalTop);
            if (isBelowFragmentainerEnd(breakPoint, logicalTop, blockSize) || (breakPoint.fragmentainerHadContent && logicalBottom > blockSize && (logicalBottom - logicalTop <= m_fragmentainer->wholeBlockSize() || paintsMonolithicOverflow(box)))) {
                addSpaceShortage(logicalBottom - blockSize);
                if (auto endIndex = breakBeforeRoot(WTF::move(breakPoint))) {
                    index = *endIndex - 1;
                    continue;
                }
                return;
            }
            // A block formatting context root that only avoids breaks inside it is laid out again breaking inside, like a block container,
            // when it does not fit where it starts.
            if (logicalBottom > blockSize && (hasFragmentableContent(box) || (hasContentFragmentedByRenderTree(box, m_fragmentainer) && !breaksContentThroughRenderTree(box)))) {
                m_unsplittableBoxesLaidOutWithBreaks.append(box);
                goBackTo(breakPoint);
                // The loop moves on to the box again.
                --index;
                continue;
            }
            if (logicalTop < blockSize && logicalBottom > blockSize && paintsMonolithicOverflow(box)) {
                m_contentEndsWithMonolithicOverflow = true;
                result.boxesPastFragmentainerEnd.append(box);
            }
            m_fragmentainerHasContent = true;
            m_marginsAdjoinUnforcedBreak = false;
            continue;
        }
        if (m_fragmentainer && blockItem.type() == BlockItem::Type::BlockContainerEnd && breaksInside(containingBlockStates.first(containingBlockStates.size() - 1))) {
            auto endBreakPoint = breakPointAt(box, index);
            auto startBreakPoint = !blockContainerBreakPoints.isEmpty() && blockContainerBreakPoints.last().box.ptr() == box.ptr() ? std::optional { blockContainerBreakPoints.takeLast() } : std::nullopt;
            auto isUnsplittable = !breaksInside(containingBlockStates.last(1));
            handleBlockContainerEnd(box, result);
            handleBlockLevelBoxEnd(box);
            auto [logicalTop, logicalBottom] = lastBoxLogicalTopAndBottom();
            auto blockSize = fragmentainerEndForContent();
            if (isUnsplittable)
                addUnbreakableBlockSize(logicalBottom - logicalTop);
            auto canBreakBefore = startBreakPoint && startBreakPoint->fragmentainerHadContent;
            if ((startBreakPoint && isBelowFragmentainerEnd(*startBreakPoint, logicalTop, blockSize)) || (canBreakBefore && logicalBottom > blockSize && isUnsplittable && (logicalBottom - logicalTop <= m_fragmentainer->wholeBlockSize() || paintsMonolithicOverflow(box)))) {
                addSpaceShortage(logicalBottom - blockSize);
                if (auto endIndex = breakBefore(WTF::move(*startBreakPoint), false)) {
                    index = *endIndex - 1;
                    continue;
                }
                return;
            }
            // Not one with size containment, which is monolithic (css-contain-2 §3.2): it has no possible break points (css-break-3 §4.1).
            if (logicalBottom > blockSize && isUnsplittable && startBreakPoint && !box->isUnsplittableForPagination()) {
                m_unsplittableBoxesLaidOutWithBreaks.append(box);
                auto startIndex = startBreakPoint->index;
                goBackTo(*startBreakPoint);
                // The loop moves on to the block container's start.
                index = startIndex - 1;
                continue;
            }
            // A box that crosses the end of the fragmentainer breaks inside: its fragment fills the rest of it, and the rest of the box continues
            // in the next one (css-break-3 §4.1), also when only its block-end padding and border cross the end, which the break slices, but not
            // when they are all of the box that is left, which overflow the fragmentainer.
            auto& geometry = result.boxes.last().geometry;
            auto contentBoxLogicalBottom = logicalBottom - geometry.borderAndPaddingAfter();
            // One that clones them breaks only inside its content box, whose fragments have them all, while it has content left.
            auto breakableLogicalTop = box->style().boxDecorationBreak() == BoxDecorationBreak::Clone ? logicalTop + geometry.borderAndPaddingBefore() : logicalTop;
            // Not one whose content box ends past the end of the fragmentainer only because monolithic content at its start overflows it: the box
            // ends with it, and the content after it goes to the next one.
            auto contentOverflowsFragmentainer = m_contentEndsWithMonolithicOverflow && contentBoxLogicalBottom > blockSize && !hasBlockSizeOfItsOwn(box) && geometry.contentBoxHeight() <= result.boxes.last().intrinsicContentBlockSize;
            if (logicalBottom > blockSize && logicalTop < blockSize && contentBoxLogicalBottom > breakableLogicalTop && !isUnsplittable && !contentOverflowsFragmentainer) {
                addSpaceShortage(logicalBottom - blockSize);
                goBackTo(endBreakPoint);
                endFragmentainer(index, false, result, { }, contentBoxLogicalBottom);
                if (auto endIndex = parallelFlowEndIndex()) {
                    index = *endIndex - 1;
                    continue;
                }
                return;
            }
            if (logicalTop < blockSize && logicalBottom > blockSize && ((isUnsplittable && paintsMonolithicOverflow(box)) || contentOverflowsFragmentainer)) {
                m_contentEndsWithMonolithicOverflow = true;
                result.boxesPastFragmentainerEnd.append(box);
            }
            if (logicalBottom > logicalTop)
                m_fragmentainerHasContent = true;
            m_marginsAdjoinUnforcedBreak = m_marginsAdjoinUnforcedBreak && logicalBottom <= logicalTop;
            continue;
        }
        if (m_fragmentainer && blockItem.type() == BlockItem::Type::InlineContent) {
            auto breakPoint = breakPointAt(box, index);
            auto* resumedInlineBreakToken = m_inlineBreakTokenToResume;
            // The lines may go to the next fragmentainer with their block container to keep orphans and widows, when rule 1 allows a break
            // before it, or before a box before it (css-break-3 §4.4).
            auto propagatedIndex = propagatedBreakPointIndex(breakPoint);
            auto isBreakBeforeAllowed = !resumedInlineBreakToken && propagatedIndex && (!avoidsBreakBefore(blockContainerBreakPoints[*propagatedIndex].box, m_fragmentainer->breakType()) || lastAllowedBreakPoint);
            auto inlineContentBreak = handleInlineContent(box, result, isBreakBeforeAllowed);
            auto isForcedBreak = std::exchange(m_forcedBreakAfterBlockLevelBox, false);
            if (auto columnSpannerBreak = std::exchange(m_columnSpannerBreakInBlockLevelBox, std::nullopt)) {
                endMultiColumnLineInInlineContent(*columnSpannerBreak, WTF::move(inlineContentBreak), index, result);
                return;
            }
            if (!inlineContentBreak)
                continue;
            if (inlineContentBreak->breaksInside) {
                auto& breakToken = endFragmentainer(index, isForcedBreak, result);
                breakToken.inlineBreakToken = WTF::move(inlineContentBreak->breakToken);
                breakToken.blockLevelBoxBreakToken = WTF::move(inlineContentBreak->blockLevelBoxBreakToken);
            } else if (!resumedInlineBreakToken) {
                // A first line that does not fit takes the block container with it, like a first box
                // (RenderBlockFlow::computeLineAdjustmentForPagination sets the strut on the block container).
                if (auto endIndex = breakBefore(WTF::move(breakPoint), false)) {
                    index = *endIndex - 1;
                    continue;
                }
                return;
            } else {
                // The block-level box a break was in has content at the start of the fragmentainer, which stays.
                ASSERT(!resumedInlineBreakToken->continuesBlockContent);
                goBackTo(breakPoint);
                endFragmentainer(index, false, result).inlineBreakToken = *resumedInlineBreakToken;
            }
            if (auto endIndex = parallelFlowEndIndex()) {
                index = *endIndex - 1;
                continue;
            }
            return;
        }
        // So are the floats among them, which the inline formatting context hides in inline content.
        auto isBlockLevelBoxStart = blockItem.type() == BlockItem::Type::BlockContainerStart || blockItem.type() == BlockItem::Type::IndependentFormattingContextRoot || blockItem.type() == BlockItem::Type::Float;
        if (isBlockLevelBoxStart && isAfterClampPoint()) {
            // Neither it nor anything in it is laid out (LineClampUpdater::skipLayoutForForcedHidden).
            result.invisibleBoxes.append(box);
            if (blockItem.type() == BlockItem::Type::BlockContainerStart)
                index = blockContainerEndIndex(index);
            continue;
        }
        // A box an earlier clamp point hid is visible again.
        if (isBlockLevelBoxStart && box->isForceHiddenByLineClamp())
            LayoutIntegration::BlockIntegrationUtils::showBoxHiddenByLineClamp(box);

        switch (blockItem.type()) {
        case BlockItem::Type::BlockContainerStart:
            if (startsLineClamp(box) && !isLaidOutWithoutLineClamp(box))
                clampedContents.append({ box, index, m_blockLayoutState, result.boxes.size(), result.outOfFlowBoxes.size(), result.invisibleBoxes.size() });
            handleBlockContainerStart(box);
            break;
        case BlockItem::Type::BlockContainerEnd:
            if (!clampedContents.isEmpty() && clampedContents.last().blockContainer.ptr() == box.ptr()) {
                auto clampedContent = clampedContents.takeLast();
                if (LineClampUpdater::contentFitsWithinMaximumLines(downcast<RenderBlockFlow>(box))) {
                    m_blockLayoutState = WTF::move(clampedContent.blockLayoutState);
                    result.boxes.shrink(clampedContent.boxCount);
                    result.outOfFlowBoxes.shrink(clampedContent.outOfFlowBoxCount);
                    result.invisibleBoxes.shrink(clampedContent.invisibleBoxCount);
                    m_blockContainersLaidOutWithoutLineClamp.append(box);
                    // The loop moves on to the block container's start.
                    index = clampedContent.startIndex - 1;
                    continue;
                }
            }
            handleBlockContainerEnd(box, result);
            handleBlockLevelBoxEnd(box);
            break;
        case BlockItem::Type::InlineContent:
            handleInlineContent(box, result);
            break;
        case BlockItem::Type::IndependentFormattingContextRoot:
            handleIndependentFormattingContextRoot(box, result);
            handleBlockLevelBoxEnd(box);
            break;
        case BlockItem::Type::Float:
            handleFloat(box, result);
            break;
        case BlockItem::Type::OutOfFlowBox:
            handleOutOfFlowBox(box, result);
            break;
        case BlockItem::Type::ColumnSpannerPlaceholder:
            ASSERT_NOT_REACHED();
            break;
        }
    }
}

// max-lines gives the content of a block container a line-clamp budget of its own, which -webkit-line-clamp on a block container does not,
// and nor does max-lines in the content of a -webkit-box with -webkit-line-clamp, where only a line-clamp container, an independent formatting
// context the block formatting context does not lay out, clamps (LineClampUpdater).
bool BlockFormattingContext::startsLineClamp(const RenderBox& blockContainer) const
{
    CheckedRef style = blockContainer.style();
    if (auto lineClamp = m_blockLayoutState.lineClamp(); lineClamp && lineClamp->isLegacy)
        return false;
    return !style->maxLines().isAuto() && style->overflowContinue() != OverflowContinue::WebkitLegacy;
}

bool BlockFormattingContext::isLaidOutWithoutLineClamp(const RenderBox& blockContainer) const
{
    return m_blockContainersLaidOutWithoutLineClamp.containsIf([&](auto& laidOutBlockContainer) {
        return laidOutBlockContainer.ptr() == &blockContainer;
    });
}

// Until the block container's end, where the lines of its inline content come off the budget around it, unless its content fit in its own
// budget and was laid out again without it (LineClampUpdater::resetLineClamp).
void BlockFormattingContext::startLineClamp(const RenderBox& blockContainer)
{
    if (!startsLineClamp(blockContainer))
        return;
    m_blockLayoutState.lineClampsToRestore().append({ blockContainer, m_blockLayoutState.lineClamp(), m_blockLayoutState.clampAfterBox() });
    m_blockLayoutState.setClampAfterBox(nullptr);
    if (isLaidOutWithoutLineClamp(blockContainer)) {
        m_blockLayoutState.setLineClamp({ });
        ++m_lineClampContainersLaidOutAgainCount;
        return;
    }
    auto maximumLines = static_cast<size_t>(blockContainer.style().maxLines().tryValue()->value);
    CheckedRef block = downcast<RenderBlock>(blockContainer);
    m_blockLayoutState.setLineClamp(BlockLayoutState::LineClamp { maximumLines, LineClampUpdater::shouldDiscardOverflow(block), false, false, false, LineClampUpdater::hasTextBoxTrimEnd(block) });
}

void BlockFormattingContext::endLineClamp(const RenderBox& blockContainer)
{
    auto& lineClampsToRestore = m_blockLayoutState.lineClampsToRestore();
    if (lineClampsToRestore.isEmpty() || lineClampsToRestore.last().blockContainer.ptr() != &blockContainer)
        return;
    auto lineClampToRestore = lineClampsToRestore.takeLast();
    auto lineClamp = lineClampToRestore.lineClamp;
    if (isLaidOutWithoutLineClamp(blockContainer)) {
        ASSERT(m_lineClampContainersLaidOutAgainCount);
        --m_lineClampContainersLaidOutAgainCount;
    } else if (lineClamp) {
        CheckedRef blockFlow = downcast<RenderBlockFlow>(blockContainer);
        auto lineCount = blockFlow->childrenInline() ? static_cast<size_t>(blockFlow->lineCount()) : 0;
        lineClamp->maximumLines -= std::min(lineClamp->maximumLines, lineCount);
        if (auto contentLineClamp = m_blockLayoutState.lineClamp())
            lineClamp->hasConstrainedTextWrapContent |= contentLineClamp->hasConstrainedTextWrapContent;
    }
    m_blockLayoutState.setLineClamp(lineClamp);
    m_blockLayoutState.setClampAfterBox(lineClampToRestore.clampAfterBox.get());
}

// "Any in-flow or floating boxes that follow the clamp point in the box tree" are invisible boxes (css-overflow-4 §5.3.1): the content before
// them took all the lines of the line-clamp budget.
bool BlockFormattingContext::isAfterClampPoint() const
{
    auto lineClamp = m_blockLayoutState.lineClamp();
    return lineClamp && lineClamp->shouldDiscardOverflow && !lineClamp->maximumLines;
}

// The clamp point may be right after a block-level box, which no line box precedes (LineClampUpdater::autoClampPoints).
void BlockFormattingContext::handleBlockLevelBoxEnd(const RenderBox& box)
{
    if (m_blockLayoutState.clampAfterBox() != &box)
        return;
    auto lineClamp = *m_blockLayoutState.lineClamp();
    m_blockLayoutState.setLineClamp(BlockLayoutState::LineClamp { 0, lineClamp.shouldDiscardOverflow, false, false, false, lineClamp.shouldTrimEndAtClampPoint });
    m_blockLayoutState.setClampAfterBox(nullptr);
}

// The root's floats for placing a float or a box with clear, whose inline-start and inline-end values resolve against the inline direction of
// its containing block (UsedStyle::floating), which the floating context takes from its root (FloatingContext::isStartPositioned). Positions
// stay in the root's coordinates.
FloatingContext BlockFormattingContext::floatingContextForSidesOf(const RenderBox& box, const PlacedFloats& floats) const
{
    CheckedRef containingBlock = *box.containingBlock();
    return { containingBlock->layoutBox(), floats, { }, m_blockLayoutState.containingBlockStates().first().geometry.borderBoxWidth() };
}

// Before its block-start margin joins the adjoining margins: clearance is decided once the margins give the block container its top, its
// hypothetical position (CSS 2.2 §9.5.2).
std::optional<ContainingBlockState::Clearance> BlockFormattingContext::clearanceAtBlockStart(const RenderBox& blockContainer) const
{
    auto* placedFloats = m_blockLayoutState.placedFloats();
    if (!blockContainer.layoutBox().hasFloatClear() || !placedFloats || placedFloats->isEmpty())
        return { };
    // The floats waiting for their block container's top are where that top would be now, and go with it.
    auto& pendingFloats = m_blockLayoutState.pendingFloats();
    auto blockEndOfFloatsToClear = [&](NOESCAPE const Function<bool(const PlacedFloats::Item&)>& includesFloat) -> std::optional<LayoutUnit> {
        auto floats = PlacedFloats { root().layoutBox() };
        for (auto& floatItem : placedFloats->list()) {
            if (includesFloat(floatItem))
                floats.add(floatItem);
        }
        if (floats.isEmpty())
            return { };
        return floatingContextForSidesOf(blockContainer, floats).blockEndOfFloatsToClear(blockContainer.layoutBox());
    };
    auto isPending = [&](auto& floatItem) {
        return pendingFloats.containsIf([&](auto& pending) {
            return pending.floatBoxes.containsIf([&](auto& floatBox) {
                return floatBox.ptr() == floatItem.layoutBox();
            });
        });
    };
    auto clearance = ContainingBlockState::Clearance { .marginsBeforeLogicalBottom = m_blockLayoutState.marginCollapse().collapsedLogicalBottomAtContent() };
    if (auto floatsLogicalBottom = blockEndOfFloatsToClear([&](auto& floatItem) { return !isPending(floatItem); }))
        clearance.floatsLogicalBottom = *floatsLogicalBottom;
    for (auto& pending : pendingFloats) {
        auto pendingFloatsLogicalBottom = blockEndOfFloatsToClear([&](auto& floatItem) {
            return pending.floatBoxes.containsIf([&](auto& floatBox) {
                return floatBox.ptr() == floatItem.layoutBox();
            });
        });
        if (pendingFloatsLogicalBottom)
            clearance.pendingFloats.append({ pending.blockContainer, *pendingFloatsLogicalBottom - pending.provisionalBorderBoxLogicalTop });
    }
    if (clearance.floatsLogicalBottom == LayoutUnit::min() && clearance.pendingFloats.isEmpty())
        return { };
    return clearance;
}

void BlockFormattingContext::handleBlockContainerStart(const RenderBox& blockContainer)
{
    LayoutIntegration::BlockIntegrationUtils::invalidateContentInlineSizesBeforeLayout(blockContainer, m_blockLayoutState.containingBlockStates().last().relayoutChildren);
    auto clearance = clearanceAtBlockStart(blockContainer);
    auto blockContainerState = containingBlockStateAtBlockStart(blockContainer);
    // The descendants marked for a new basis lose the intrinsic inline sizes that depend on it (through aspect ratios), which the inline
    // size may come from (RenderBlockFlow::layoutBlock marks them before it computes the width).
    if (LayoutIntegration::BlockIntegrationUtils::markPercentageHeightDescendantsForLayout(downcast<RenderBlockFlow>(blockContainer), blockContainerState.contentBoxBlockSize))
        blockContainerState = containingBlockStateAtBlockStart(blockContainer);
    m_blockLayoutState.marginCollapse().startBlockContainer(blockContainerState, m_blockLayoutState.containingBlockStates().last(), blockContainerState.geometry.marginBefore());
    blockContainerState.clearance = clearance;
    LayoutIntegration::BlockIntegrationUtils::layoutOutsideListMarker(blockContainer, blockContainerState.relayoutChildren);
    // The line of its own the marker of a list item gets in quirks mode is room above its block children that its block size includes, like
    // block-start padding (RenderListItem::layoutExcludedChildren).
    if (auto ownLineBlockSize = LayoutIntegration::BlockIntegrationUtils::outsideListMarkerOwnLineBlockSize(blockContainer)) {
        auto& geometry = blockContainerState.geometry;
        geometry.setVerticalPadding({ geometry.paddingBefore() + ownLineBlockSize, geometry.paddingAfter() });
        blockContainerState.borderAndPaddingBefore += ownLineBlockSize;
    }
    if (blockContainerState.geometry.borderAndPaddingBefore())
        resolveBorderBoxLogicalTops(blockContainerState);
    startTextBoxTrim(downcast<RenderBlockFlow>(blockContainer), blockContainerState);
    startLineClamp(blockContainer);
    // Trim-end on the line-clamp container or on a block between it and this one applies to the last line before the clamp point, which may be
    // in this block, unless block-end padding or border is in between (css-inline-3 §6.2, LineClampUpdater).
    if (auto lineClamp = m_blockLayoutState.lineClamp(); lineClamp && !lineClamp->isLegacy) {
        blockContainerState.trimsEndAtClampPointToRestore = lineClamp->shouldTrimEndAtClampPoint;
        lineClamp->shouldTrimEndAtClampPoint = LineClampUpdater::shouldTrimEndAtClampPoint(downcast<RenderBlock>(blockContainer), lineClamp->shouldTrimEndAtClampPoint);
        m_blockLayoutState.setLineClamp(lineClamp);
    }
    // Its margin-trim discards the margins at the block-start edge of its content, its first child's block-start margin and the ones that
    // collapse with it (css-box-4 §3.3.1).
    auto marginTrim = blockContainer.style().marginTrim();
    if (marginTrim.contains(Style::MarginTrimSide::BlockStart)) {
        blockContainerState.trimsMarginsAtBlockStartEdgeToRestore = m_blockLayoutState.marginCollapse().trimsMarginsAtBlockStartEdge();
        m_blockLayoutState.marginCollapse().setTrimsMarginsAtBlockStartEdge(true);
    }
    if (marginTrim.contains(Style::MarginTrimSide::BlockEnd) && !blockContainerState.borderBoxLogicalTop)
        blockContainerState.adjoiningMarginsAtContentStart = m_blockLayoutState.marginCollapse().adjoiningMargins();
    m_blockLayoutState.containingBlockStates().append(WTF::move(blockContainerState));
    if (!blockContainer.firstChild())
        handleEmptyEditableContent(blockContainer, m_blockLayoutState.containingBlockStates().last());
}

// The content of an empty editable block container is as tall as a line, for the caret (RenderBlock::hasLineIfEmpty), and separates its margins
// like a line box does (CSS 2.2 §8.3.1).
void BlockFormattingContext::handleEmptyEditableContent(const RenderBox& blockContainer, ContainingBlockState& blockContainerState)
{
    auto contentBlockSize = BlockFormattingUtils::emptyEditableContentBlockSize(blockContainer);
    if (!contentBlockSize)
        return;
    if (!blockContainerState.borderBoxLogicalTop)
        resolveBorderBoxLogicalTops(blockContainerState);
    m_blockLayoutState.marginCollapse().resetAdjoiningMargins(blockContainerState.contentBoxLogicalTop() + *contentBlockSize);
}

// The text-box-trim of a block container applies to its content (TextBoxTrimmer). A block-start border or padding keeps the start trim of
// an ancestor from it, and from the content after it.
TextBoxTrimState BlockFormattingContext::textBoxTrimStateFor(const RenderBlockFlow& blockContainer, const TextBoxTrimState& ancestorState) const
{
    auto textBoxTrim = blockContainer.style().textBoxTrim();
    auto trimsEnd = textBoxTrim == TextBoxTrim::TrimEnd || textBoxTrim == TextBoxTrim::TrimBoth;
    // The last formatted line is in its own inline content, unless its first layout tells it is in block-level content, a block-level box on
    // the last line included.
    CheckedPtr<const RenderBlockFlow> lastFormattedLineRoot;
    if (trimsEnd) {
        lastFormattedLineRoot = m_blockLayoutState.lastFormattedLineRoot(blockContainer);
        if (!lastFormattedLineRoot && blockContainer.childrenInline())
            lastFormattedLineRoot = &blockContainer;
    }
    return {
        textBoxTrim == TextBoxTrim::TrimStart || textBoxTrim == TextBoxTrim::TrimBoth || ancestorState.trimsFirstFormattedLine,
        lastFormattedLineRoot ? lastFormattedLineRoot : ancestorState.lastFormattedLineRoot
    };
}

void BlockFormattingContext::startTextBoxTrim(const RenderBlockFlow& blockContainer, ContainingBlockState& blockContainerState)
{
    auto state = m_blockLayoutState.textBoxTrimState();
    if (state.trimsFirstFormattedLine && blockContainerState.geometry.borderAndPaddingBefore())
        state.trimsFirstFormattedLine = false;
    if (blockContainer.style().textBoxTrim() == TextBoxTrim::None) {
        m_blockLayoutState.setTextBoxTrimState(WTF::move(state));
        return;
    }
    blockContainerState.textBoxTrimStateToRestore = state;
    m_blockLayoutState.setTextBoxTrimState(textBoxTrimStateFor(blockContainer, state));
}

void BlockFormattingContext::endTextBoxTrim(const RenderBlockFlow& blockContainer, ContainingBlockState& blockContainerState)
{
    if (auto& state = blockContainerState.textBoxTrimStateToRestore) {
        m_blockLayoutState.setTextBoxTrimState(WTF::move(*state));
        return;
    }
    // Only the first formatted line gets its start trimmed, which a ruby annotation or an anonymous block without content on its first line
    // is not part of.
    auto state = m_blockLayoutState.textBoxTrimState();
    if (!state.trimsFirstFormattedLine || blockContainer.style().display() == Style::DisplayType::RubyText || LayoutIntegration::BlockIntegrationUtils::isAnonymousBlockWithoutContentfulFirstLine(blockContainer))
        return;
    state.trimsFirstFormattedLine = false;
    m_blockLayoutState.setTextBoxTrimState(WTF::move(state));
}

// What the start of a block container set up for its content ends with it.
void BlockFormattingContext::endBlockContainerContent(const RenderBox& blockContainer, ContainingBlockState& blockContainerState)
{
    endTextBoxTrim(downcast<RenderBlockFlow>(blockContainer), blockContainerState);
    if (auto trimsEnd = blockContainerState.trimsEndAtClampPointToRestore; trimsEnd && m_blockLayoutState.lineClamp()) {
        auto lineClamp = m_blockLayoutState.lineClamp();
        lineClamp->shouldTrimEndAtClampPoint = *trimsEnd;
        m_blockLayoutState.setLineClamp(lineClamp);
    }
    endLineClamp(blockContainer);
    if (auto trimsMargins = blockContainerState.trimsMarginsAtBlockStartEdgeToRestore) {
        auto& marginCollapse = m_blockLayoutState.marginCollapse();
        marginCollapse.setTrimsMarginsAtBlockStartEdge(*trimsMargins && marginCollapse.trimsMarginsAtBlockStartEdge());
    }
}

void BlockFormattingContext::handleBlockContainerEnd(const RenderBox& blockContainer, BlockLayoutResult& result)
{
    auto blockContainerState = m_blockLayoutState.containingBlockStates().takeLast();
    ASSERT(blockContainerState.blockContainer.ptr() == &blockContainer);
    endBlockContainerContent(blockContainer, blockContainerState);
    addLineGridOfOutOfFlowContainingBlock(blockContainer, blockContainerState, result);
    auto& containingBlockState = m_blockLayoutState.containingBlockStates().last();
    m_blockLayoutState.marginCollapse().endBlockContainer(blockContainerState, containingBlockState);
    // Nothing in its content separated the block-start margin from the block-end margin. Unless the box's own height, block-end
    // border or padding does, the margins collapse through it.
    auto marginsCollapseThrough = [&] {
        if (blockContainer.stretchesToViewport()) {
            // Quirks mode: an empty body still stretches to the viewport (RenderBlock::isSelfCollapsingBlock checks the height it gets).
            auto margins = m_blockLayoutState.marginCollapse().adjoiningMargins();
            margins.add(blockContainerState.geometry.marginAfter());
            if (viewportStretchedBorderBoxBlockSize(blockContainerState.margins.collapsedMarginsBefore().collapsedValue(), margins.collapsedValue(), containingBlockState) > 0)
                return false;
        }
        return BlockMarginCollapse::marginsCollapseThrough(blockContainer, blockContainerState.geometry, percentageBasis(blockContainer, containingBlockState), stretchFitBorderBoxBlockSizes(blockContainerState.geometry, containingBlockState), root().writingMode());
    };
    if (!blockContainerState.borderBoxLogicalTop && !marginsCollapseThrough())
        resolveBorderBoxLogicalTops(blockContainerState);
    placeInContainingBlock(blockContainer, WTF::move(blockContainerState), result);
}

// The start in the root's logical coordinates of an inline range of a block container's, from the inline start of its border box in its own
// direction, and the other way around: mirrored in its border box when its inline direction is the opposite of the root's.
static LayoutUnit inlineStartInRoot(const ContainingBlockState& blockContainerState, LayoutUnit start, LayoutUnit size)
{
    if (!blockContainerState.hasInlineDirectionOppositeToRoot)
        return blockContainerState.borderBoxLogicalLeftInRoot + start;
    return blockContainerState.borderBoxLogicalLeftInRoot + blockContainerState.geometry.borderBoxWidth() - start - size;
}

static LayoutUnit inlineStartInBlockContainer(const ContainingBlockState& blockContainerState, LayoutUnit startInRoot, LayoutUnit size)
{
    if (!blockContainerState.hasInlineDirectionOppositeToRoot)
        return startInRoot - blockContainerState.borderBoxLogicalLeftInRoot;
    return blockContainerState.borderBoxLogicalLeftInRoot + blockContainerState.geometry.borderBoxWidth() - startInRoot - size;
}

// The part of the content box of a block container that the floats leave between two block positions in the root, in its own coordinates.
static std::pair<LayoutUnit, LayoutUnit> spaceBesideFloats(const FloatingContext& floatingContext, const ContainingBlockState& blockContainerState, LayoutUnit top, LayoutUnit bottom)
{
    auto constraints = floatingContext.constraints(top, bottom, FloatingContext::MayBeAboveLastFloat::Yes);
    auto start = blockContainerState.inlineRange.min();
    auto end = blockContainerState.inlineRange.max();
    // The floats at the start of the root are at the end of a block container whose inline direction is the opposite.
    auto startEdge = constraints.start ? std::optional { inlineStartInBlockContainer(blockContainerState, constraints.start->x, 0_lu) } : std::nullopt;
    auto endEdge = constraints.end ? std::optional { inlineStartInBlockContainer(blockContainerState, constraints.end->x, 0_lu) } : std::nullopt;
    if (blockContainerState.hasInlineDirectionOppositeToRoot)
        std::swap(startEdge, endEdge);
    if (startEdge)
        start = std::max(start, *startEdge);
    if (endEdge)
        end = std::min(end, *endEdge);
    return { start, end };
}

// A float in the coordinates of a border box of this inline size whose inline direction is the opposite: its start and end, and its margins,
// swap sides.
static PlacedFloats::Item mirroredFloat(const PlacedFloats::Item& floatItem, LayoutUnit borderBoxInlineSize)
{
    auto localGeometry = floatItem.boxGeometry();
    auto absoluteGeometry = BoxGeometry { localGeometry };
    absoluteGeometry.setHorizontalMargin({ localGeometry.marginEnd(), localGeometry.marginStart() });
    auto borderBox = floatItem.absoluteBorderBoxRect();
    absoluteGeometry.setTopLeft({ borderBoxInlineSize - borderBox.left() - borderBox.width(), borderBox.top() });
    auto position = floatItem.isStartPositioned() ? PlacedFloats::Item::Position::End : PlacedFloats::Item::Position::Start;
    if (CheckedPtr layoutBox = floatItem.layoutBox())
        return { *layoutBox, position, absoluteGeometry, BoxGeometry::borderBoxTopLeft(localGeometry), floatItem.placedByLine() };
    return { position, absoluteGeometry, BoxGeometry::borderBoxTopLeft(localGeometry), RefPtr { floatItem.shape() } };
}

// A float in the coordinates of a border box offset by this from the one it is in; without its box, as a float from outside the inline
// formatting context it goes to (PlacedFloats::Item::isInFormattingContextOf).
static PlacedFloats::Item movedFloat(const PlacedFloats::Item& floatItem, LayoutSize offset, const Box* layoutBox)
{
    auto localGeometry = floatItem.boxGeometry();
    auto absoluteGeometry = BoxGeometry { localGeometry };
    absoluteGeometry.setTopLeft(floatItem.absoluteBorderBoxRect().topLeft() + offset);
    auto position = floatItem.isStartPositioned() ? PlacedFloats::Item::Position::Start : PlacedFloats::Item::Position::End;
    if (layoutBox)
        return { *layoutBox, position, absoluteGeometry, BoxGeometry::borderBoxTopLeft(localGeometry), floatItem.placedByLine() };
    return { position, absoluteGeometry, BoxGeometry::borderBoxTopLeft(localGeometry), RefPtr { floatItem.shape() } };
}

static PlacedFloats::Item floatWithoutShape(const PlacedFloats::Item& floatItem)
{
    auto localGeometry = floatItem.boxGeometry();
    auto absoluteGeometry = BoxGeometry { localGeometry };
    absoluteGeometry.setTopLeft(floatItem.absoluteBorderBoxRect().topLeft());
    auto position = floatItem.isStartPositioned() ? PlacedFloats::Item::Position::Start : PlacedFloats::Item::Position::End;
    return { position, absoluteGeometry, BoxGeometry::borderBoxTopLeft(localGeometry), nullptr };
}

// A float from outside the inline content it intrudes into, in the coordinates of the border box of the content's block container, which is
// offset by this from the root's. The render tree path gives the inline formatting context such floats from its float lists with their edges
// along the physical vertical axis floored (LineLayout::preparePlacedFloats), and lines go beside them the same way here.
static PlacedFloats::Item intrudingFloat(const PlacedFloats::Item& floatItem, const ContainingBlockState& blockContainerState, LayoutSize offset, WritingMode writingMode, bool isInContent)
{
    auto geometry = floatItem.boxGeometry();
    auto localTopLeft = BoxGeometry::borderBoxTopLeft(geometry);
    auto borderBoxTopLeft = floatItem.absoluteBorderBoxRect().topLeft() + offset;
    // Not those of the content itself that continue from the fragmentainer before, which the render tree path does not list as intruding.
    if (isInContent) {
        geometry.setTopLeft(borderBoxTopLeft);
        auto position = floatItem.isStartPositioned() ? PlacedFloats::Item::Position::Start : PlacedFloats::Item::Position::End;
        auto floatInBlockContainer = PlacedFloats::Item { position, geometry, localTopLeft, RefPtr { floatItem.shape() } };
        if (blockContainerState.hasInlineDirectionOppositeToRoot)
            return mirroredFloat(floatInBlockContainer, blockContainerState.geometry.borderBoxWidth());
        return floatInBlockContainer;
    }
    if (writingMode.isHorizontal()) {
        auto marginBoxTop = LayoutUnit { (borderBoxTopLeft.y() - geometry.marginBefore()).floor() };
        auto marginBoxBottom = LayoutUnit { (borderBoxTopLeft.y() + geometry.borderBoxHeight() + geometry.marginAfter()).floor() };
        geometry.setVerticalMargin({ geometry.marginBefore(), marginBoxBottom - marginBoxTop - geometry.marginBefore() - geometry.borderBoxHeight() });
        borderBoxTopLeft.setY(marginBoxTop + geometry.marginBefore());
    } else {
        auto marginBoxLeft = LayoutUnit { (borderBoxTopLeft.x() - geometry.marginStart()).floor() };
        auto marginBoxRight = LayoutUnit { (borderBoxTopLeft.x() + geometry.borderBoxWidth() + geometry.marginEnd()).floor() };
        geometry.setHorizontalMargin({ geometry.marginStart(), marginBoxRight - marginBoxLeft - geometry.marginStart() - geometry.borderBoxWidth() });
        borderBoxTopLeft.setX(marginBoxLeft + geometry.marginStart());
    }
    geometry.setTopLeft(borderBoxTopLeft);
    auto position = floatItem.isStartPositioned() ? PlacedFloats::Item::Position::Start : PlacedFloats::Item::Position::End;
    auto floatInBlockContainer = PlacedFloats::Item { position, geometry, localTopLeft, RefPtr { floatItem.shape() } };
    // The inline content of a block container is in its own inline direction.
    if (blockContainerState.hasInlineDirectionOppositeToRoot)
        return mirroredFloat(floatInBlockContainer, blockContainerState.geometry.borderBoxWidth());
    return floatInBlockContainer;
}

// The border box of a block container being laid out in the root, from its top when it is known, and otherwise from where the margins collapsed
// so far put it: the floats placed from there wait for it (placePendingFloats).
LayoutSize BlockFormattingContext::borderBoxOffsetInRoot(const ContainingBlockState& blockContainerState)
{
    auto borderBoxLogicalTop = [&] {
        if (auto borderBoxLogicalTop = blockContainerState.borderBoxLogicalTop)
            return *borderBoxLogicalTop;
        for (auto& [blockContainer, resolvedBorderBoxLogicalTop] : m_resolvedBorderBoxLogicalTops) {
            if (blockContainer.ptr() == blockContainerState.blockContainer.ptr())
                return resolvedBorderBoxLogicalTop;
        }
        return m_blockLayoutState.marginCollapse().collapsedLogicalBottom();
    };
    return { blockContainerState.borderBoxLogicalLeftInRoot, borderBoxLogicalTop() };
}

// The floats placed in a block container, in the coordinates of the root, that the block formatting context does not have yet. While the
// block container's top is not known, they wait for it.
void BlockFormattingContext::addFloats(PlacedFloats::List&& floats, const ContainingBlockState& blockContainerState)
{
    auto& placedFloats = m_blockLayoutState.ensurePlacedFloats(root().layoutBox());
    auto& pendingFloats = m_blockLayoutState.pendingFloats();
    auto pendingFloatCount = size_t { 0 };
    for (auto& pending : pendingFloats)
        pendingFloatCount += pending.floatBoxes.size();
    auto isPlacedAgainstOtherFloats = placedFloats.list().size() > pendingFloatCount;
    auto isPlacedAgainstOtherPendingFloats = pendingFloats.containsIf([&](auto& pending) {
        return pending.blockContainer.ptr() != blockContainerState.blockContainer.ptr();
    });
    auto addedFloats = Vector<CheckedRef<const Box>> { };
    for (auto& floatItem : floats) {
        CheckedPtr floatBox = floatItem.layoutBox();
        if (!floatBox)
            continue;
        auto isPlaced = placedFloats.list().containsIf([&](auto& placedFloat) {
            return placedFloat.layoutBox() == floatBox.get();
        });
        if (isPlaced)
            continue;
        placedFloats.add(WTF::move(floatItem));
        addedFloats.append(*floatBox);
    }
    if (!blockContainerState.borderBoxLogicalTop && !addedFloats.isEmpty())
        pendingFloats.append({ blockContainerState.blockContainer, WTF::move(addedFloats), borderBoxOffsetInRoot(blockContainerState).height(), isPlacedAgainstOtherFloats, isPlacedAgainstOtherPendingFloats });
}

// The floats in the inline content of the innermost block container, in the coordinates of its border box: the content placed them before a
// block-level box in it, or after the last one.
PlacedFloats::Item BlockFormattingContext::inlineContentFloatInRoot(const PlacedFloats::Item& floatItem)
{
    auto& blockContainerState = m_blockLayoutState.containingBlockStates().last();
    auto offsetInRoot = borderBoxOffsetInRoot(blockContainerState);
    if (blockContainerState.hasInlineDirectionOppositeToRoot)
        return movedFloat(mirroredFloat(floatItem, blockContainerState.geometry.borderBoxWidth()), offsetInRoot, floatItem.layoutBox());
    return movedFloat(floatItem, offsetInRoot, floatItem.layoutBox());
}

void BlockFormattingContext::addInlineContentFloats(const PlacedFloats::List& floats)
{
    addFloats(WTF::map(floats, [&](auto& floatItem) {
        return inlineContentFloatInRoot(floatItem);
    }), m_blockLayoutState.containingBlockStates().last());
}

auto BlockFormattingContext::handleInlineContent(const RenderBox& blockContainer, BlockLayoutResult& result, bool isBreakBeforeAllowed) -> std::optional<InlineContentBreak>
{
    auto& containingBlockStates = m_blockLayoutState.containingBlockStates();
    auto& blockContainerState = containingBlockStates.last();
    ASSERT(blockContainerState.blockContainer.ptr() == &blockContainer);
    // The block-level boxes in the content add states of their own while it is laid out, which can move the block container's state, and the
    // geometry with it.
    auto geometry = blockContainerState.geometry;
    // The start of its first formatted line, and the end of the last one, are trimmed, the other way around on lines the block flow inverts.
    auto& textBoxTrimState = m_blockLayoutState.textBoxTrimState();
    auto textBoxTrim = BlockLayoutState::TextBoxTrim { };
    auto isLineInverted = blockContainer.writingMode().isLineInverted();
    if (textBoxTrimState.trimsFirstFormattedLine)
        textBoxTrim.add(isLineInverted ? BlockLayoutState::TextBoxTrimSide::End : BlockLayoutState::TextBoxTrimSide::Start);
    if (textBoxTrimState.lastFormattedLineRoot == &blockContainer)
        textBoxTrim.add(isLineInverted ? BlockLayoutState::TextBoxTrimSide::Start : BlockLayoutState::TextBoxTrimSide::End);
    m_blockLayoutState.setTextBoxTrim(textBoxTrim);
    m_blockLayoutState.setTextBoxEdge(blockContainer.style().textBoxEdge());
    // While the line-clamp container looks for its auto clamp point, its content is laid out as if 'text-wrap-style' were 'stable', and it
    // lays out again what that constrained (RenderBlockFlow::layoutInFlowChildren).
    if (auto lineClamp = m_blockLayoutState.lineClamp(); lineClamp && lineClamp->isStableTextWrap && InlineContentConstrainer::constrainsContent(blockContainer.style())) {
        lineClamp->hasConstrainedTextWrapContent = true;
        m_blockLayoutState.setLineClamp(lineClamp);
    }
    // The floats that intrude into the content and those in it are placed from the block container's border box in the root, whose top the
    // line boxes at the start of the content resolve before it is laid out. Without them the margins stay open, for a block-level box in the
    // content too, and the floats go where the block container's top would be if they did not (CSS 2.2 §8.3.1, §9.5.1).
    CheckedRef inlineContentRoot = downcast<RenderBlockFlow>(blockContainer);
    auto* placedFloats = m_blockLayoutState.placedFloats();
    auto hasFloats = (placedFloats && !placedFloats->isEmpty()) || LayoutIntegration::BlockIntegrationUtils::inlineContentHasFloats(inlineContentRoot);
    auto offsetInRoot = LayoutSize { };
    auto intrudingFloats = PlacedFloats::List { };
    if (hasFloats) {
        if (!blockContainerState.borderBoxLogicalTop && LayoutIntegration::BlockIntegrationUtils::inlineContentMakesLineBoxesBeforeBlockLevelBoxes(inlineContentRoot))
            resolveBorderBoxLogicalTops(blockContainerState);
        offsetInRoot = borderBoxOffsetInRoot(blockContainerState);
        if (placedFloats) {
            for (auto& floatItem : placedFloats->list()) {
                CheckedPtr floatBox = floatItem.layoutBox();
                auto isInContent = floatBox && floatBox->rendererForIntegration()->isDescendantOf(inlineContentRoot.ptr());
                intrudingFloats.append(intrudingFloat(floatItem, blockContainerState, -offsetInRoot, root().writingMode(), isInContent));
            }
        }
    }
    // The lines snap to the line grid from the block container's border box, from where the margins so far put it while its top is not known
    // (only floats would give it clearance).
    m_blockLayoutState.setLineGrid(lineGridForInlineContent(blockContainerState, !!m_inlineBreakTokenToResume));
    // An initial letter in the content clears the ones that intrude into it from before, which the inline formatting context gets as the
    // bottom of the lowest one from the top of the content (RenderBlockFlow::lowestInitialLetterLogicalBottom).
    auto intrusiveInitialLetterLogicalBottom = std::optional<LayoutUnit> { };
    if (placedFloats) {
        for (auto& floatItem : placedFloats->list()) {
            CheckedPtr floatBox = floatItem.layoutBox();
            if (!floatBox || floatBox->style().pseudoElementType() != PseudoElementType::FirstLetter || floatBox->style().initialLetter().drop() <= 0)
                continue;
            auto marginBoxLogicalBottom = floatItem.absoluteRectWithMargin().bottom();
            if (marginBoxLogicalBottom <= offsetInRoot.height())
                continue;
            auto logicalBottom = marginBoxLogicalBottom - offsetInRoot.height() - blockContainerState.borderAndPaddingBefore;
            intrusiveInitialLetterLogicalBottom = std::max(intrusiveInitialLetterLogicalBottom.value_or(logicalBottom), logicalBottom);
        }
    }
    m_blockLayoutState.setIntrusiveInitialLetterLogicalBottom(intrusiveInitialLetterLogicalBottom);
    // -webkit-line-clamp gives the content what is left of the count of lines in the -webkit-box, which the boxes the render tree lays out
    // add to as well (BlockIntegrationUtils::legacyLineClampMaximumLines).
    auto isInLegacyLineClampContext = m_blockLayoutState.lineClamp() && m_blockLayoutState.lineClamp()->isLegacy;
    if (isInLegacyLineClampContext) {
        if (auto maximumLines = LayoutIntegration::BlockIntegrationUtils::legacyLineClampMaximumLines(inlineContentRoot))
            m_blockLayoutState.setLineClamp(BlockLayoutState::LineClamp { *maximumLines, false, true });
    }
    // The lines that do not fit in the fragmentainer continue in the next one (css-break-3 §4.1), from where they stopped in the one before,
    // below what monolithic content from there overflows into this one.
    auto constraints = constraintsForInlineContent(blockContainer, blockContainerState);
    auto* inlineBreakToken = std::exchange(m_inlineBreakTokenToResume, nullptr);
    // The lines of the fragmentainers before took their part of the line-clamp budget already, which the inline formatting context counts
    // again from the start of the content.
    auto lineCountBeforeBreak = inlineBreakToken && inlineBreakToken->previousLine ? inlineBreakToken->previousLine->lineIndex + 1 : 0;
    if (auto lineClamp = m_blockLayoutState.lineClamp(); lineClamp && lineCountBeforeBreak) {
        lineClamp->maximumLines += lineCountBeforeBreak;
        m_blockLayoutState.setLineClamp(lineClamp);
    }
    // Content that starts with a block-level box, whose block-start margin may still collapse with the block container's, breaks from the top
    // the block container got the first time the content was laid out (m_resolvedBorderBoxLogicalTops).
    auto isFragmented = m_fragmentainer && breaksInside(containingBlockStates.span().subspan(1));
    auto resolvedBorderBoxLogicalTop = std::optional<LayoutUnit> { };
    auto needsFragmentainerEndFromResolvedTop = false;
    if (isFragmented && !LayoutIntegration::BlockIntegrationUtils::inlineContentMakesLineBoxesBeforeBlockLevelBoxes(inlineContentRoot)) {
        isFragmented = false;
        if (LayoutIntegration::BlockIntegrationUtils::inlineContentStartsWithBlockLevelBox(inlineContentRoot)) {
            auto index = m_resolvedBorderBoxLogicalTops.findIf([&](auto& entry) {
                return entry.first.ptr() == &blockContainer;
            });
            if (blockContainerState.borderBoxLogicalTop)
                isFragmented = true;
            else if (index != notFound) {
                resolvedBorderBoxLogicalTop = m_resolvedBorderBoxLogicalTops[index].second;
                isFragmented = true;
            } else
                needsFragmentainerEndFromResolvedTop = true;
        }
    }
    if (isFragmented) {
        if (!blockContainerState.borderBoxLogicalTop && !resolvedBorderBoxLogicalTop)
            resolveBorderBoxLogicalTops(blockContainerState);
        // The lines from the fragmentainers before count from where they stopped.
        if (inlineBreakToken)
            blockContainerState.lineCountThroughLastBlockLevelBox = inlineBreakToken->previousLine ? inlineBreakToken->previousLine->lineIndex + 1 : 0;
        if (inlineBreakToken && m_incomingBreakToken->blockOffset > blockContainerState.contentBoxLogicalTop()) {
            auto logicalTop = constraints.logicalTop() + m_incomingBreakToken->blockOffset - blockContainerState.contentBoxLogicalTop();
            constraints = { { constraints.horizontal(), logicalTop }, constraints.visualLeft(), constraints.formattingRootBorderBoxSize() };
        }
        // orphans and widows constrain the breaks only when set (RenderBlockFlow::computeLineAdjustmentForPagination).
        auto lineCount = [](const auto& value) -> size_t {
            auto integer = value.tryValue();
            return integer ? integer->value : 1;
        };
        CheckedRef style = blockContainer.style();
        auto borderBoxLogicalTop = blockContainerState.borderBoxLogicalTop.value_or(resolvedBorderBoxLogicalTop.value_or(0_lu));
        // The first line is not at the start of the fragmentainer below the block-start margins, borders and padding of the boxes that start
        // in it, where it goes to the next one when it does not fit, like the render tree moves it (RenderBlockFlow::computeLineAdjustmentForPagination),
        // unless those end past the end of the fragmentainer, which the block container breaks at itself.
        auto contentLogicalTop = borderBoxLogicalTop + blockContainerState.borderAndPaddingBefore;
        auto hasContentBefore = m_fragmentainerHasContent || (!m_fragmentainer->keepsContentAtStart && contentLogicalTop > -m_fragmentainer->rootBlockOffset && contentLogicalTop < m_fragmentainer->availableBlockSize());
        using ForcedBreaks = ConstraintsForInlineContent::FragmentainerEnd::ForcedBreaks;
        auto forcedBreaks = !m_fragmentainer->appliesForcedBreaks ? ForcedBreaks::None : m_fragmentainer->breakType() == FragmentainerConstraints::Type::Page ? ForcedBreaks::Page : ForcedBreaks::Column;
        constraints.setFragmentainerEnd(ConstraintsForInlineContent::FragmentainerEnd { fragmentainerEndForContent() - borderBoxLogicalTop, hasContentBefore, m_fragmentainer->wholeBlockSize(), isBreakBeforeAllowed, lineCount(style->orphans()), lineCount(style->widows()), !m_pushedFloats.isEmpty(), forcedBreaks });
    } else
        ASSERT(!inlineBreakToken);
    auto floatsInFragmentainerBeforeContent = m_floatsInFragmentainer.size();
    auto listItemsWithOutsideMarkerScope = ListItemOutsideMarkerScope { root().view().frameView().layoutContext(), LayoutIntegration::BlockIntegrationUtils::listItemsWithOutsideMarker(m_blockLayoutState.containingBlockStates()) };
    auto inlineContentLayoutResult = integrationUtils().layoutInlineContent(inlineContentRoot, constraints, geometry, blockContainerState.contentBoxBlockSize, blockContainerState.fillAvailableBlockSize, blockContainerState.replacedElementAvailableBlockSize, orthogonalFlowAvailableInlineSpace(blockContainerState), blockContainerState.relayoutChildren || m_isLayingOutWithResolvedBorderBoxLogicalTops || m_lineClampContainersLaidOutAgainCount, m_blockLayoutState, intrudingFloats, lineGridInLayoutState(blockContainerState, borderBoxLogicalLeftAndTopInRoot(blockContainerState)), *this, result, inlineBreakToken);
    // Its lines are relative to the fragment of the block container, also when they start in one after its first.
    if (inlineBreakToken || blockContainerState.consumedBlockSize)
        result.inlineContentFragments.append({ inlineContentRoot, inlineContentLayoutResult.firstLineIndex });
    // The floats the inline formatting context placed whose content the block formatting context laid out join the result with that content,
    // where they went, but not those that go to the next fragmentainer, which it lays out again there.
    for (auto& floatItem : inlineContentLayoutResult.floats) {
        CheckedPtr floatBox = floatItem.layoutBox();
        auto index = floatBox ? m_inlineContentFloatResults.findIf([&](auto& entry) {
            return &entry.first->layoutBox() == floatBox.get();
        }) : notFound;
        if (index == notFound)
            continue;
        CheckedRef box = m_inlineContentFloatResults[index].first;
        auto& floatContentLayout = m_inlineContentFloatResults[index].second;
        auto startsInNextFragmentainer = !!floatContentLayout.offsetPastFragmentainerEnd;
        if (auto* content = floatContent(*floatBox))
            continueFloat(box.get(), BlockBreakToken::PushedFloat::Content { content->content }, floatContentLayout, floatItem.boxGeometry(), inlineContentFloatInRoot(floatItem), containingBlockForFloats(containingBlockStates.last()));
        if (startsInNextFragmentainer)
            continue;
        auto contentResult = std::exchange(floatContentLayout.result, { });
        auto intrinsicContentBlockSize = contentResult.intrinsicContentBlockSize;
        appendFloatContentResult(WTF::move(contentResult), result);
        result.boxes.append({ box, floatItem.boxGeometry(), intrinsicContentBlockSize, { }, { }, false, false, false });
    }
    m_inlineContentFloatResults.clear();
    // The block-level box the break is in, if any, continues in the next fragmentainer.
    m_blockLevelBoxBreakTokenToResume = nullptr;
    auto blockLevelBoxBreakToken = std::exchange(m_blockLevelBoxBreakToken, nullptr);
    if (!inlineContentLayoutResult.breakToken || !inlineContentLayoutResult.breakToken->continuesBlockContent)
        blockLevelBoxBreakToken = nullptr;
    // Its content is laid out again with the fragmentainer end from the top it got.
    if (needsFragmentainerEndFromResolvedTop) {
        m_resolvedBorderBoxLogicalTops.append({ blockContainer, borderBoxOffsetInRoot(containingBlockStates.last()).height() });
        m_needsLayoutWithResolvedBorderBoxLogicalTops = true;
    }
    addUnbreakableBlockSize(inlineContentLayoutResult.unbreakableLinesBlockSize);
    for (auto& floatItem : inlineContentLayoutResult.pushedFloats) {
        auto* content = floatContent(*floatItem.layoutBox());
        auto floatInFragmentainer = m_floatsInFragmentainer.reverseFindIf([&](auto& entry) {
            return entry.box.ptr() == floatItem.layoutBox();
        });
        auto renderTreeLayout = floatInFragmentainer != notFound ? m_floatsInFragmentainer[floatInFragmentainer].renderTreeLayout : std::nullopt;
        m_pushedFloats.append({ downcast<RenderBox>(*floatItem.layoutBox()->rendererForIntegration()), inlineContentFloatInRoot(floatItem), content ? std::optional { content->content } : std::nullopt, containingBlockForFloats(blockContainerState), WTF::move(renderTreeLayout) });
    }
    auto inlineContentBreak = std::optional<InlineContentBreak> { };
    if (inlineContentLayoutResult.breakToken) {
        addSpaceShortage(LayoutUnit { inlineContentLayoutResult.breakToken->spaceShortage });
        // Its line boxes are content in the fragmentainer, and so are the floats it placed, which stay there, while the lines that fit neither
        // beside nor below them go to the next one: there is a break between a float and the in-flow content after it (css-break-3 §4.1). So
        // does a first line the floats beside the start of the content leave no room for, which goes below them: the gap above it is where the
        // break goes (class C), later than the one before the block container, and the render tree keeps the block container there too (the
        // first line takes the pagination strut, RenderBlockFlow::computeLineAdjustmentForPagination).
        auto startsBelowFloats = [&] {
            if (inlineContentLayoutResult.hasLineBoxes || inlineBreakToken)
                return false;
            auto contentLogicalTop = constraints.logicalTop();
            auto contentLogicalLeft = constraints.horizontal().logicalLeft;
            auto contentLogicalRight = contentLogicalLeft + constraints.horizontal().logicalWidth;
            return intrudingFloats.containsIf([&](auto& floatItem) {
                auto marginBox = floatItem.absoluteRectWithMargin();
                return marginBox.top() <= contentLogicalTop && marginBox.bottom() > contentLogicalTop && marginBox.left() < contentLogicalRight && marginBox.right() > contentLogicalLeft;
            });
        };
        auto breaksInside = inlineContentLayoutResult.hasLineBoxes || !inlineContentLayoutResult.floats.isEmpty() || startsBelowFloats();
        inlineContentBreak = InlineContentBreak { WTF::move(*inlineContentLayoutResult.breakToken), breaksInside, WTF::move(blockLevelBoxBreakToken) };
        if (!breaksInside)
            return inlineContentBreak;
    }
    if (m_fragmentainer && inlineContentLayoutResult.hasLineBoxes) {
        m_fragmentainerHasContent = true;
        m_marginsAdjoinUnforcedBreak = false;
    }
    addInlineContentFloats(inlineContentLayoutResult.floats);
    if (m_fragmentainer) {
        for (auto& floatItem : inlineContentLayoutResult.floats) {
            // Not one this layout of the content laid out through the render tree already (layoutFloatInInlineContentThroughRenderTree).
            auto isInFragmentainer = std::ranges::any_of(m_floatsInFragmentainer.subspan(floatsInFragmentainerBeforeContent), [&](auto& entry) {
                return entry.box.ptr() == floatItem.layoutBox();
            });
            if (!isInFragmentainer)
                m_floatsInFragmentainer.append({ *floatItem.layoutBox(), containingBlockForFloats(blockContainerState) });
        }
    }
    // The inline formatting context does not break content without line boxes: its floats go to the next fragmentainer from here when they do
    // not fit, once the block container's top is known (placePendingFloats otherwise).
    if (m_fragmentainer && !constraints.fragmentainerEnd() && containingBlockStates.last().borderBoxLogicalTop) {
        pushFloatsThatDoNotFit(WTF::map(inlineContentLayoutResult.floats, [](auto& floatItem) {
            return CheckedRef { *floatItem.layoutBox() };
        }));
    }
    // The floats of a line-clamp container are clipped to its block-end content edge, those the inline formatting context hid after the
    // clamp point included.
    if (auto lineClamp = m_blockLayoutState.lineClamp(); lineClamp && lineClamp->shouldDiscardOverflow) {
        for (auto& floatItem : inlineContentLayoutResult.floats) {
            if (CheckedPtr floatBox = floatItem.layoutBox())
                LayoutIntegration::BlockIntegrationUtils::setFloatClippedByLineClamp(downcast<RenderBox>(*floatBox->rendererForIntegration()), true);
        }
    }
    if (isInLegacyLineClampContext) {
        auto consumedContentBlockSize = blockContainerState.consumedBlockSize ? blockContainerState.consumedBlockSize->contentBox : 0_lu;
        if (auto clampedContentBlockSize = LayoutIntegration::BlockIntegrationUtils::updateLegacyLineClampAfterInlineContent(inlineContentRoot, lineCountBeforeBreak, consumedContentBlockSize))
            inlineContentLayoutResult.contentBlockSize = *clampedContentBlockSize;
    } else if (auto lineClamp = m_blockLayoutState.lineClamp()) {
        // The content takes its lines off the line-clamp budget (LineClampUpdater).
        lineClamp->maximumLines -= std::min(lineClamp->maximumLines, inlineContentLayoutResult.lineCount);
        m_blockLayoutState.setLineClamp(lineClamp);
    }
    auto& blockContainerStateAfterLayout = containingBlockStates.last();
    auto& marginCollapse = m_blockLayoutState.marginCollapse();
    if (auto& margins = inlineContentLayoutResult.marginsOfKeptBlockLevelBoxes) {
        // The content separates the block-start margin, with the margins at its start, from the margins at its end.
        if (!blockContainerStateAfterLayout.borderBoxLogicalTop) {
            marginCollapse.addKeptContentMarginsBefore(blockContainerStateAfterLayout, margins->before);
            resolveBorderBoxLogicalTops(blockContainerStateAfterLayout);
        }
        auto contentLogicalBottom = blockContainerStateAfterLayout.contentBoxLogicalTop() + inlineContentLayoutResult.contentBlockSize;
        if (inlineContentLayoutResult.endsWithBlockLevelBox)
            marginCollapse.resetAdjoiningMargins(contentLogicalBottom, margins->after, margins->afterAreQuirky);
        else
            marginCollapse.resetAdjoiningMarginsAfterLineBoxes(contentLogicalBottom);
        return inlineContentBreak;
    }
    // Content that ends with a block-level box leaves the margins after it open (CSS 2.2 §8.3.1).
    if (!inlineContentLayoutResult.hasLineBoxes || inlineContentLayoutResult.endsWithBlockLevelBox)
        return inlineContentBreak;
    blockContainerStateAfterLayout.blockEndChildren = { { }, { }, blockContainerStateAfterLayout.borderAndPaddingBefore };
    // The first line box separates the block-start margin from what follows (CSS 2.2 §8.3.1).
    if (!blockContainerStateAfterLayout.borderBoxLogicalTop)
        resolveBorderBoxLogicalTops(blockContainerStateAfterLayout);
    marginCollapse.resetAdjoiningMarginsAfterLineBoxes(blockContainerStateAfterLayout.contentBoxLogicalTop() + inlineContentLayoutResult.contentBlockSize);
    return inlineContentBreak;
}

// A block-level box in inline content is laid out like a child of the block container, between the line boxes before and after it (CSS 2.2
// §9.2.1.1).
BlockLevelBoxInInlineContent BlockFormattingContext::layoutBlockLevelBoxInInlineContent(const RenderBox& box, LayoutUnit lineLogicalTop, size_t lineCount, size_t lineCountForLineClamp, PlacedFloats& inlineContentFloats, BlockLayoutResult& result)
{
    auto& containingBlockStates = m_blockLayoutState.containingBlockStates();
    auto& blockContainerState = containingBlockStates.last();
    ASSERT(blockContainerState.blockContainer.ptr() == box.containingBlock());
    // The line boxes before it separate its block-start margin from the margins before them (CSS 2.2 §8.3.1), and are content before it in
    // the fragmentainer, which a break before it needs.
    if (lineCount > blockContainerState.lineCountThroughLastBlockLevelBox) {
        if (!blockContainerState.borderBoxLogicalTop)
            resolveBorderBoxLogicalTops(blockContainerState);
        m_blockLayoutState.marginCollapse().separateMarginsAtLineBox(*blockContainerState.borderBoxLogicalTop + lineLogicalTop);
        if (m_fragmentainer)
            m_fragmentainerHasContent = true;
    }
    // The box avoids the floats the lines before it placed (RenderBlockFlow::layoutBlockChildFromInlineLayout).
    addInlineContentFloats(inlineContentFloats.list());
    // Its own line is one more line with content.
    blockContainerState.lineCountThroughLastBlockLevelBox = lineCount + 1;
    // The inline content keeps its text-box-trim. A box after the first formatted line does not trim its start (TextBoxTrimStartDisabler).
    auto inlineContentTextBoxTrim = m_blockLayoutState.textBoxTrim();
    auto inlineContentTextBoxEdge = m_blockLayoutState.textBoxEdge();
    auto textBoxTrimStateToRestore = std::optional<TextBoxTrimState> { };
    if (lineCount && m_blockLayoutState.textBoxTrimState().trimsFirstFormattedLine) {
        textBoxTrimStateToRestore = m_blockLayoutState.textBoxTrimState();
        m_blockLayoutState.setTextBoxTrimState({ false, textBoxTrimStateToRestore->lastFormattedLineRoot });
    }
    // The inline formatting context counts its lines against the line-clamp budget it started with, and the box takes its own off what is left
    // of it (layoutWithFormattingContextForBlockInInline).
    auto lineClamp = m_blockLayoutState.lineClamp();
    auto remainingLines = lineClamp ? lineClamp->maximumLines - std::min(lineClamp->maximumLines, lineCountForLineClamp) : 0;
    if (lineClamp) {
        auto lineClampForBox = *lineClamp;
        lineClampForBox.maximumLines = remainingLines;
        m_blockLayoutState.setLineClamp(lineClampForBox);
    }
    auto stateCount = containingBlockStates.size();
    // In fragmented content, its content breaks at the end of the fragmentainer, which then ends with its line, and continues on the first
    // line of the next one, from where it stopped (InlineBreakToken::continuesBlockContent).
    auto blockItems = BlockItemsBuilder { root() }.buildForBlockLevelBoxInInlineContent(box);
    auto blockLevelBoxOutsideStateCount = std::exchange(m_blockLevelBoxOutsideStateCount, stateCount);
    auto startIndex = size_t { 0 };
    if (auto* breakToken = std::exchange(m_blockLevelBoxBreakTokenToResume, nullptr)) {
        m_marginsAdjoinUnforcedBreak = !breakToken->isForced;
        m_inlineBreakTokenToResume = breakToken->inlineBreakToken ? &*breakToken->inlineBreakToken : nullptr;
        m_blockLevelBoxBreakTokenToResume = breakToken->blockLevelBoxBreakToken.get();
        m_openBlockFormattingContextRootToResume = breakToken->openBlockFormattingContextRoot ? &*breakToken->openBlockFormattingContextRoot : nullptr;
        resumeAfterBreak(*breakToken);
        startIndex = breakToken->blockItemIndex;
        if (auto blockOffset = breakToken->blockOffset) {
            m_blockLayoutState.marginCollapse().resetAdjoiningMargins(blockOffset, breakToken->marginsAfterOverflow);
            m_fragmentainerHasContent = true;
            // The margins after it do not adjoin the break, which is before it.
            m_marginsAdjoinUnforcedBreak = false;
        }
    }
    layoutBlockItems(blockItems, result, startIndex);
    m_blockLevelBoxOutsideStateCount = blockLevelBoxOutsideStateCount;
    ASSERT_UNUSED(stateCount, containingBlockStates.size() == stateCount);
    // A break before the box takes its line to the next fragmentainer, where it is laid out (the inline formatting context breaks before
    // the line).
    if (m_blockLevelBoxBreakToken && !m_blockLevelBoxBreakToken->blockItemIndex && m_blockLevelBoxBreakToken->openBlockContainers.isEmpty() && !m_blockLevelBoxBreakToken->openBlockFormattingContextRoot) {
        m_blockLevelBoxBreakToken = nullptr;
        m_blockLayoutState.setTextBoxTrim(inlineContentTextBoxTrim);
        m_blockLayoutState.setTextBoxEdge(inlineContentTextBoxEdge);
        if (textBoxTrimStateToRestore)
            m_blockLayoutState.setTextBoxTrimState(WTF::move(*textBoxTrimStateToRestore));
        return { { }, lineLogicalTop, { }, 0, true, false };
    }
    // A column spanner that ends the box ends the fragmentainer after the line of the box, where the box has nothing left.
    auto breaksAfter = m_blockLevelBoxBreakToken && m_blockLevelBoxBreakToken->columnSpannerPlaceholder && m_blockLevelBoxBreakToken->blockItemIndex == blockItems.size() && m_blockLevelBoxBreakToken->openBlockContainers.isEmpty();
    if (breaksAfter)
        m_blockLevelBoxBreakToken = nullptr;
    // So does a forced break after it, which is one between the block box it is in and that of the content after it (CSS 2.2 §9.2.1.1,
    // css-break-3 §3.1).
    if (!m_blockLevelBoxBreakToken && m_fragmentainer && m_fragmentainer->appliesForcedBreaks && hasForcedBreakAfter(box, m_fragmentainer->breakType())) {
        breaksAfter = true;
        m_forcedBreakAfterBlockLevelBox = true;
    }
    auto breaksInside = !!m_blockLevelBoxBreakToken;
    ASSERT(result.boxes.last().renderer.ptr() == &box);
    m_blockLayoutState.setTextBoxTrim(inlineContentTextBoxTrim);
    m_blockLayoutState.setTextBoxEdge(inlineContentTextBoxEdge);
    if (textBoxTrimStateToRestore)
        m_blockLayoutState.setTextBoxTrimState(WTF::move(*textBoxTrimStateToRestore));
    auto boxLineCount = size_t { 0 };
    if (lineClamp) {
        auto lineClampAfterBox = *m_blockLayoutState.lineClamp();
        boxLineCount = remainingLines - std::min(remainingLines, lineClampAfterBox.maximumLines);
        lineClamp->hasConstrainedTextWrapContent |= lineClampAfterBox.hasConstrainedTextWrapContent;
        m_blockLayoutState.setLineClamp(lineClamp);
    }

    // Without a top, the block container's margins still collapse with what is after the box, which collapsed through.
    auto& blockContainerStateAfterLayout = containingBlockStates.last();
    // The lines after the box snap to the line grid from where the block container is now, which the inline content of the box changed.
    m_blockLayoutState.setLineGrid(lineGridForInlineContent(blockContainerStateAfterLayout, false));
    // The lines after the box avoid the floats in it too, and the ones from outside the block container, from where its top is now.
    auto offsetInRoot = borderBoxOffsetInRoot(blockContainerStateAfterLayout);
    auto floats = PlacedFloats::List { };
    for (auto& floatItem : m_blockLayoutState.ensurePlacedFloats(root().layoutBox()).list()) {
        auto inlineContentFloat = inlineContentFloats.list().findIf([&](auto& inlineContentFloat) {
            return inlineContentFloat.layoutBox() && inlineContentFloat.layoutBox() == floatItem.layoutBox();
        });
        floats.append(inlineContentFloat != notFound ? inlineContentFloats.list()[inlineContentFloat] : intrudingFloat(floatItem, blockContainerStateAfterLayout, -offsetInRoot, root().writingMode(), false));
    }
    inlineContentFloats.clear();
    for (auto& floatItem : floats)
        inlineContentFloats.add(WTF::move(floatItem));
    auto& marginCollapse = m_blockLayoutState.marginCollapse();
    auto& geometry = result.boxes.last().geometry;
    // Its fragment fills the rest of the fragmentainer, and the margins adjoining the break are truncated (css-break-3 §5.2).
    if (breaksInside || breaksAfter)
        return { geometry, BoxGeometry::borderBoxTop(geometry) + geometry.borderBoxHeight(), { }, boxLineCount, false, breaksInside, breaksAfter };
    auto marginsCollapseWithBlockContainerTop = !blockContainerStateAfterLayout.borderBoxLogicalTop;
    auto marginState = BlockLayoutState::MarginState { };
    marginState.adjoiningMargins = marginCollapse.adjoiningMargins();
    marginState.atBlockContainerStart = marginsCollapseWithBlockContainerTop;
    marginState.blockContainerMarginBeforeCollapsesWithContent = marginsCollapseWithBlockContainerTop;
    return {
        geometry,
        marginsCollapseWithBlockContainerTop ? lineLogicalTop : marginCollapse.logicalTop() - *blockContainerStateAfterLayout.borderBoxLogicalTop,
        marginState,
        boxLineCount
    };
}

// block-step-size rounds the margin box block size up to a multiple of the step, the extra space going to the margins (css-rhythm-1 §2,
// RenderBlockFlow::performBlockStepSizing). Only the margin box insertion is supported, as on the render tree path.
static std::optional<LayoutUnit> blockStepSize(const RenderBox& box)
{
    CheckedRef style = box.style();
    auto blockStepSize = style->blockStepSize().tryLength();
    if (!blockStepSize || !BlockStepSizing::childHasSupportedStyle(style.get()))
        return { };
    return Style::evaluate<LayoutUnit>(*blockStepSize, style->usedZoomForLength());
}

// Like RenderBlockFlow::clearFloatsIfNeeded, which gives a box clearance once its margins collapsed, and takes them out of its parent's
// block-start margin then: the outermost of the block containers waiting for their top that has clear goes below the floats when its
// hypothetical position, where the margins put it, is above them, without the margins of a block container inside it that goes below floats
// too (CSS 2.2 §9.5.2, §8.3.1). The ones around it go to where the margins before it put them, and the ones inside it go with it, or
// below the floats they clear from there, where the floats waiting for a block container's top are where that block container's top is.
auto BlockFormattingContext::borderBoxLogicalTopsWaitingForTop(std::span<const ContainingBlockState* const> statesWaitingForTop, LayoutUnit marginsLogicalBottom) const -> BorderBoxLogicalTops
{
    auto count = statesWaitingForTop.size();
    auto tops = BorderBoxLogicalTops { };
    tops.borderBoxLogicalTops.fill(marginsLogicalBottom, count);
    tops.hasClearance.fill(false, count);
    auto outermostClearedIndex = std::optional<size_t> { };
    auto hypotheticalBorderBoxLogicalTop = marginsLogicalBottom;
    for (size_t index = 0; index < count; ++index) {
        auto& clearance = statesWaitingForTop[index]->clearance;
        if (!clearance || hypotheticalBorderBoxLogicalTop >= clearance->floatsLogicalBottomFor(hypotheticalBorderBoxLogicalTop))
            continue;
        outermostClearedIndex = index;
        hypotheticalBorderBoxLogicalTop = clearance->marginsBeforeLogicalBottom;
    }
    if (!outermostClearedIndex)
        return tops;

    auto& clearance = *statesWaitingForTop[*outermostClearedIndex]->clearance;
    auto containingBlocksLogicalTop = clearance.marginsBeforeLogicalBottom;
    auto borderBoxLogicalTop = clearance.floatsLogicalBottomFor(containingBlocksLogicalTop);
    for (auto index = *outermostClearedIndex + 1; index < count; ++index)
        tops.borderBoxLogicalTops[index] = containingBlocksLogicalTop;
    tops.borderBoxLogicalTops[*outermostClearedIndex] = borderBoxLogicalTop;
    tops.hasClearance[*outermostClearedIndex] = true;
    for (auto index = *outermostClearedIndex; index--;) {
        if (auto& innerClearance = statesWaitingForTop[index]->clearance) {
            auto floatsLogicalBottom = innerClearance->floatsLogicalBottomFor([&](const RenderBox& blockContainer) {
                for (auto outerIndex = index + 1; outerIndex < count; ++outerIndex) {
                    CheckedRef outerBlockContainer = statesWaitingForTop[outerIndex]->blockContainer;
                    if (&blockContainer == outerBlockContainer.ptr() || blockContainer.isDescendantOf(outerBlockContainer.ptr()))
                        return tops.borderBoxLogicalTops[outerIndex];
                }
                return containingBlocksLogicalTop;
            });
            if (borderBoxLogicalTop < floatsLogicalBottom) {
                borderBoxLogicalTop = floatsLogicalBottom;
                tops.hasClearance[index] = true;
            }
        }
        tops.borderBoxLogicalTops[index] = borderBoxLogicalTop;
    }
    return tops;
}

// The block containers waiting for their top, from the innermost one.
Vector<const ContainingBlockState*> BlockFormattingContext::statesWaitingForTop() const
{
    auto states = Vector<const ContainingBlockState*> { };
    for (auto& containingBlockState : m_blockLayoutState.containingBlockStates() | std::views::reverse) {
        if (containingBlockState.borderBoxLogicalTop)
            break;
        states.append(&containingBlockState);
    }
    return states;
}

// Where resolveBorderBoxLogicalTops would put the next box, without clearance of its own, and the block containers waiting for their top. A
// box that separates margins ends the block-start sides of the quirk containers, whose margins then count, the box's own included.
auto BlockFormattingContext::hypotheticalPosition(EndsQuirkContainerBlockStartSides endsQuirkContainerBlockStartSides) const -> HypotheticalPosition
{
    auto& marginCollapse = m_blockLayoutState.marginCollapse();
    auto marginsLogicalBottom = endsQuirkContainerBlockStartSides == EndsQuirkContainerBlockStartSides::Yes ? marginCollapse.collapsedLogicalBottomAtContent() : marginCollapse.collapsedLogicalBottom();
    auto states = statesWaitingForTop();
    auto tops = borderBoxLogicalTopsWaitingForTop(states, marginsLogicalBottom);
    auto position = HypotheticalPosition { states.isEmpty() ? marginsLogicalBottom : tops.borderBoxLogicalTops.first(), { } };
    for (size_t index = 0; index < states.size(); ++index)
        position.blockContainerLogicalTops.append({ states[index]->blockContainer, tops.borderBoxLogicalTops[index] });
    return position;
}

// The floats placed so far, where the ones waiting for their block container's top are where that top would be at the hypothetical position
// of the next box.
PlacedFloats BlockFormattingContext::floatsAtHypotheticalPosition(const HypotheticalPosition& position) const
{
    auto& pendingFloats = m_blockLayoutState.pendingFloats();
    auto floats = PlacedFloats { root().layoutBox() };
    for (auto& floatItem : m_blockLayoutState.placedFloats()->list()) {
        auto pending = pendingFloats.findIf([&](auto& pending) {
            return pending.floatBoxes.containsIf([&](auto& floatBox) {
                return floatBox.ptr() == floatItem.layoutBox();
            });
        });
        if (pending == notFound) {
            floats.add(floatItem);
            continue;
        }
        // They go with the innermost block container around them that waits for its top (placePendingFloats).
        CheckedRef blockContainer = pendingFloats[pending].blockContainer;
        auto borderBoxLogicalTop = position.borderBoxLogicalTop;
        for (auto& [outerBlockContainer, outerBorderBoxLogicalTop] : position.blockContainerLogicalTops) {
            if (blockContainer.ptr() == outerBlockContainer.ptr() || blockContainer->isDescendantOf(outerBlockContainer.ptr())) {
                borderBoxLogicalTop = outerBorderBoxLogicalTop;
                break;
            }
        }
        floats.add(movedFloat(floatItem, { 0_lu, borderBoxLogicalTop - pendingFloats[pending].provisionalBorderBoxLogicalTop }, floatItem.layoutBox()));
    }
    return floats;
}

// The content box of a border box, which the scrollbars take space from, up to all of the padding box (RenderBox::paddingBoxWidth). The
// border box stays the sum of its parts, which a padding the box does not use (a table with collapsed borders) leaves negative.
static void setContentBoxInlineSize(BoxGeometry& geometry, LayoutUnit borderBoxInlineSize)
{
    auto scrollbarInlineSize = std::min(geometry.horizontalSpaceForScrollbar(), std::max(0_lu, borderBoxInlineSize - geometry.horizontalBorder()));
    geometry.setHorizontalSpaceForScrollbar(scrollbarInlineSize);
    geometry.setContentBoxWidth(borderBoxInlineSize - geometry.horizontalBorderAndPadding() - scrollbarInlineSize);
}

static void setContentBoxBlockSize(BoxGeometry& geometry, LayoutUnit borderBoxBlockSize)
{
    auto scrollbarBlockSize = std::min(geometry.verticalSpaceForScrollbar(), std::max(0_lu, borderBoxBlockSize - geometry.verticalBorder()));
    geometry.setVerticalSpaceForScrollbar(scrollbarBlockSize);
    geometry.setContentBoxHeight(borderBoxBlockSize - geometry.verticalBorderAndPadding() - scrollbarBlockSize);
}

static bool decidesItsOwnInlineSize(const RenderBox&, WritingMode);

// The inline size a box whose own formatting context decides it gets from the space beside the floats, which the block formatting context
// computes for it: an auto one, which stretches into the space or fits the content into it (RenderBox::shrinkToAvoidFloats), and stretch
// (RenderBox::computeSizingKeywordLogicalWidthUsing). Not the one of a table, which shrinks into the space itself, or of a replaced element.
static bool hasInlineSizeFromSpaceBesideFloats(const RenderBox& box, WritingMode writingMode)
{
    // An orthogonal box gets the size in the root's inline axis from its own layout, its block size.
    if (is<RenderTable>(box) || box.isRenderReplaced() || box.writingMode().isOrthogonal(writingMode))
        return false;
    CheckedRef style = box.style();
    return style->logicalWidth(writingMode).isAuto() || style->logicalWidth(writingMode).isStretch() || style->logicalMinWidth(writingMode).isStretch() || style->logicalMaxWidth(writingMode).isStretch();
}

// The stretch inline size, min inline size or max inline size of a block-level replaced element, which its own formatting context resolves to the
// stretch-fit inline size the block formatting context gives it, the one in the space beside the floats (RenderReplaced::computeReplacedLogicalWidthUsing).
// Not -webkit-fill-available, which takes the containing block's, nor in an orthogonal flow, where the floats are in the box's block axis.
static bool hasStretchFitInlineSizeBesideFloats(const RenderBox& box, WritingMode writingMode)
{
    if (!(box.isRenderReplaced() || box.isRenderOrLegacyRenderSVGRoot()) || box.isFloating() || box.writingMode().isOrthogonal(writingMode))
        return false;
    CheckedRef style = box.style();
    auto isStretch = [](auto& size) {
        return size.isStretch() && !size.isFillAvailable();
    };
    return isStretch(style->logicalWidth(writingMode)) || isStretch(style->logicalMinWidth(writingMode)) || isStretch(style->logicalMaxWidth(writingMode));
}

// The stretch-fit inline size of a border box in a space beside the floats in the content box of the containing block: a float may sit in a
// positive margin, and a negative one adds to it (RenderBox::shrinkLogicalWidthToAvoidFloats).
static LayoutUnit stretchFitBorderBoxInlineSizeInSpace(LayoutUnit spaceStart, LayoutUnit spaceEnd, LayoutUnit contentBoxStart, LayoutUnit contentBoxEnd, LayoutUnit marginStart, LayoutUnit marginEnd)
{
    auto marginInFloats = [](LayoutUnit margin, LayoutUnit floatsInlineSize) {
        return std::min(std::max(0_lu, margin), floatsInlineSize);
    };
    return spaceEnd - spaceStart - marginStart - marginEnd + marginInFloats(marginStart, spaceStart - contentBoxStart) + marginInFloats(marginEnd, contentBoxEnd - spaceEnd);
}

// A box that establishes a formatting context of its own, a table or a block-level replaced element goes beside the floats before it without
// overlapping their margin boxes, or below them (CSS 2.2 §9.5): like RenderBlockFlow::getClearDelta, from where the margins put it, its
// hypothetical position, and then at the bottom of one float after the other, until it fits the space the floats leave beside its border box.
// An auto inline size stretches into that space (css-sizing-3 §4.2), and the block size depends on it, so the box is laid out again when the
// space is narrower below the top: at most twice for each position. A box that goes below its hypothetical position has clearance: its margins
// no longer collapse with the ones before it (CSS 2.2 §8.3.1, RenderBlockFlow::clearFloatsIfNeeded). Returns the block size of its content.
template<typename LayoutContent>
std::optional<LayoutUnit> BlockFormattingContext::placeBesideFloats(const RenderBox& box, ContainingBlockState& boxState, LayoutUnit marginsBeforeLogicalBottom, const LayoutContent& layoutContent)
{
    auto& containingBlockState = m_blockLayoutState.containingBlockStates().last();
    // The box separates margins, which puts it where the margins kept at the block-start side of a quirk container count when one of them, its
    // own too, is not quirky (RenderBlockFlow::collapseMarginsWithChildInfo).
    auto hypotheticalPosition = this->hypotheticalPosition(EndsQuirkContainerBlockStartSides::Yes);
    auto hypotheticalBorderBoxLogicalTop = hypotheticalPosition.borderBoxLogicalTop;
    auto& pendingFloats = m_blockLayoutState.pendingFloats();
    auto floats = floatsAtHypotheticalPosition(hypotheticalPosition);
    // It goes beside the margin boxes of the floats, not their shapes, which only line boxes wrap around (csswg-drafts issue 1970).
    auto marginBoxFloats = PlacedFloats { root().layoutBox() };
    for (auto& floatItem : floats.list())
        marginBoxFloats.add(floatWithoutShape(floatItem));
    auto& rootState = m_blockLayoutState.containingBlockStates().first();
    auto floatingContext = FloatingContext { root().layoutBox(), marginBoxFloats, { }, rootState.geometry.borderBoxWidth() };

    // The space the floats leave in the content box of the containing block, in its coordinates.
    auto contentBoxStart = containingBlockState.inlineRange.min();
    auto contentBoxEnd = containingBlockState.inlineRange.max();
    auto spaceBetween = [&](LayoutUnit top, LayoutUnit bottom) {
        auto [spaceStart, spaceEnd] = spaceBesideFloats(floatingContext, containingBlockState, top, bottom);
        return spaceOnLineGridColumns(containingBlockState, spaceStart, spaceEnd);
    };

    // Its inline size and margins in a space, and where its border box starts in it, like RenderBlockFlow::determineLogicalLeftPositionForChild:
    // auto margins share the space (RenderBox::computeLogicalWidth with containingBlockAvailableLineWidth), and otherwise a float may sit in
    // a positive start margin, and a negative one pulls the box back only where no float is. A stretch-fit size, of an auto inline size too,
    // goes to the end of the space, or to a positive end margin, and negative margins add to it (RenderBox::shrinkLogicalWidthToAvoidFloats),
    // while percentages still refer to the containing block.
    CheckedRef style = box.style();
    auto writingMode = root().writingMode();
    CheckedRef containingBlockStyle = containingBlockState.blockContainer->style();
    auto& marginStartLength = style->marginStart(containingBlockStyle->writingMode());
    auto& marginEndLength = style->marginEnd(containingBlockStyle->writingMode());
    auto hasAutoInlineMargin = marginStartLength.isAuto() || marginEndLength.isAuto();
    auto marginFor = [&](auto& length) {
        return length.isAuto() ? 0_lu : Style::evaluateMinimum<LayoutUnit>(length, containingBlockState.inlineRange.size(), style->usedZoomForLength());
    };
    auto marginStart = hasAutoInlineMargin ? 0_lu : marginFor(marginStartLength);
    auto marginEnd = hasAutoInlineMargin ? 0_lu : marginFor(marginEndLength);
    CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(box);
    auto isLaidOutThroughRenderTree = !blockContainer || LayoutIntegration::BlockIntegrationUtils::laysOutThroughRenderTree(*blockContainer);
    auto hasOwnInlineSize = isLaidOutThroughRenderTree && decidesItsOwnInlineSize(box, writingMode);
    // A table with an auto inline size is no wider than the space, which it floors to its min-content size itself (RenderTable::updateLogicalWidth
    // with shrinkLogicalWidthToAvoidFloats), so it keeps the inline size it has in the containing block when that fits.
    auto shrinksIntoSpace = is<RenderTable>(box) && box.shrinkToAvoidFloats();
    auto hasInlineSizeFromSpace = hasOwnInlineSize && hasInlineSizeFromSpaceBesideFloats(box, writingMode);
    // A replaced element takes the stretch-fit inline size in the space from where it starts, down its block size when that does not depend on
    // its inline size, with the margins it has without the floats (RenderReplaced::computeReplacedLogicalWidthUsing).
    auto hasStretchFitInlineSizeFromSpace = hasStretchFitInlineSizeBesideFloats(box, writingMode);
    auto narrowsDownBlockSize = !hasStretchFitInlineSizeFromSpace || !BlockFormattingUtils::blockSizeBehavesAsAuto(style->logicalHeight(writingMode), containingBlockState.contentBoxBlockSize);
    // The auto margins of an orthogonal box, which are in its block axis, share the containing block, not the space beside the floats
    // (RenderBox::computeLogicalHeight with a perpendicular containing block).
    auto autoMarginsInlineSize = [&](LayoutUnit spaceInlineSize) {
        return box.writingMode().isOrthogonal(writingMode) ? containingBlockState.inlineRange.size() : spaceInlineSize;
    };
    struct Placement {
        BoxGeometry geometry;
        LayoutUnit spaceStart;
        LayoutUnit spaceEnd;
        std::optional<LayoutUnit> borderBoxInlineSize { };
        std::optional<LayoutUnit> stretchFitBorderBoxInlineSize { };
    };
    auto laidOutState = std::optional<ContainingBlockState> { };
    auto laidOutContentBlockSize = std::optional<LayoutUnit> { };
    auto laidOutGivenBorderBoxInlineSize = std::optional<LayoutUnit> { };
    auto laidOutGivenStretchFitBorderBoxInlineSize = std::optional<LayoutUnit> { };
    // The content of a box in a line grid snaps to it from where the box is, and the content of a box in a fragmentainer breaks where its end
    // is from there, so a box that would go elsewhere is laid out again there.
    auto contentDependsOnPosition = (containingBlockState.lineGrid && !box.isUnsplittableForPagination()) || (m_fragmentainer && hasFragmentableContent(box)) || hasContentFragmentedByRenderTree(box, m_fragmentainer);
    auto layoutBorderBoxLogicalTop = hypotheticalBorderBoxLogicalTop;
    auto laidOutBorderBoxLogicalTop = std::optional<LayoutUnit> { };
    auto containingBlockBorderBoxInlineSize = std::optional<LayoutUnit> { };
    // An auto start margin is in the space, and so is the start margin -webkit-center centers the box with, a float may sit in a positive one,
    // and a negative one pulls the box back only where no float is (RenderBlockFlow::determineLogicalLeftPositionForChild).
    auto borderBoxStartInSpace = [&](LayoutUnit spaceStart, const BoxGeometry& geometry) {
        if (marginStartLength.isAuto() || containingBlockStyle->textAlign() == Style::TextAlign::WebKitCenter)
            return spaceStart + geometry.marginStart();
        if (spaceStart > contentBoxStart)
            return std::max(contentBoxStart + geometry.marginStart(), spaceStart);
        return contentBoxStart + geometry.marginStart();
    };
    auto placementIn = [&](LayoutUnit spaceStart, LayoutUnit spaceEnd) {
        auto geometry = boxState.geometry;
        auto stretchFitInlineSize = stretchFitBorderBoxInlineSizeInSpace(spaceStart, spaceEnd, contentBoxStart, contentBoxEnd, marginStart, marginEnd);
        if (!hasOwnInlineSize) {
            computeInlineSizeAndMargins(box, containingBlockState, geometry, stretchFitInlineSize + marginStart + marginEnd);
            // The margins that align it share the space, which a float in a margin does not add to (RenderBox::computeInlineDirectionMargins
            // with containingBlockAvailableLineWidth).
            auto [alignedMarginStart, alignedMarginEnd] = BlockFormattingUtils::inlineMargins(box, geometry.borderBoxWidth(), containingBlockState.inlineRange.size(), containingBlockStyle, autoMarginsInlineSize(spaceEnd - spaceStart));
            geometry.setHorizontalMargin({ alignedMarginStart, alignedMarginEnd });
            // Where it goes in the space, which -webkit-line-align puts its lines on the columns of the line grid from.
            geometry.setLeft(borderBoxStartInSpace(spaceStart, geometry) + BlockFormattingUtils::justifySelfOffset(box, geometry.borderBoxWidth(), geometry.marginStart(), geometry.marginEnd(), containingBlockState.inlineRange.size(), containingBlockStyle));
            return Placement { WTF::move(geometry), spaceStart, spaceEnd };
        }
        auto borderBoxInlineSize = std::optional<LayoutUnit> { };
        if (shrinksIntoSpace && containingBlockBorderBoxInlineSize && stretchFitInlineSize < *containingBlockBorderBoxInlineSize)
            borderBoxInlineSize = stretchFitInlineSize;
        else if (hasInlineSizeFromSpace) {
            // Its own formatting context would take the space beside the floats from the containing block, where the block formatting
            // context keeps them.
            computeInlineSizeAndMargins(box, containingBlockState, geometry, stretchFitInlineSize + marginStart + marginEnd);
            borderBoxInlineSize = geometry.borderBoxWidth();
        }
        auto stretchFitInlineSizeFromSpace = std::optional<LayoutUnit> { };
        if (hasStretchFitInlineSizeFromSpace)
            stretchFitInlineSizeFromSpace = stretchFitBorderBoxInlineSizeInSpace(spaceStart, spaceEnd, contentBoxStart, contentBoxEnd, marginFor(marginStartLength), marginFor(marginEndLength));
        return Placement { WTF::move(geometry), spaceStart, spaceEnd, borderBoxInlineSize, stretchFitInlineSizeFromSpace };
    };
    // The box laid out for a placement, and its border box block size.
    auto layoutFor = [&](const Placement& placement) {
        auto hasInlineSize = [&] {
            if (hasOwnInlineSize)
                return laidOutGivenBorderBoxInlineSize == placement.borderBoxInlineSize && laidOutGivenStretchFitBorderBoxInlineSize == placement.stretchFitBorderBoxInlineSize;
            return laidOutState->geometry.borderBoxWidth() == placement.geometry.borderBoxWidth();
        };
        if (laidOutState && hasInlineSize() && (!contentDependsOnPosition || laidOutBorderBoxLogicalTop == layoutBorderBoxLogicalTop))
            return laidOutState->geometry.borderBoxHeight();
        auto state = containingBlockStateFor(box, placement.geometry, containingBlockState);
        state.margins = boxState.margins;
        if (contentDependsOnPosition)
            state.borderBoxLogicalTop = layoutBorderBoxLogicalTop;
        laidOutContentBlockSize = layoutContent(state, placement.borderBoxInlineSize, placement.stretchFitBorderBoxInlineSize).first;
        state.borderBoxLogicalTop = { };
        laidOutBorderBoxLogicalTop = layoutBorderBoxLogicalTop;
        laidOutGivenBorderBoxInlineSize = placement.borderBoxInlineSize;
        laidOutGivenStretchFitBorderBoxInlineSize = placement.stretchFitBorderBoxInlineSize;
        if (!state.isLaidOutThroughRenderTree)
            computeBlockSize(box, laidOutContentBlockSize.value_or(0_lu), containingBlockState, state.geometry);
        laidOutState = WTF::move(state);
        return laidOutState->geometry.borderBoxHeight();
    };
    // Whether the border box fits the space beside the floats at a position, for its whole block size, which it is laid out for. Floats
    // further down narrow that space, which an auto inline size then stretches into, once more at most.
    auto placementAt = [&](LayoutUnit top) -> std::pair<Placement, bool> {
        layoutBorderBoxLogicalTop = top;
        auto [spaceStart, spaceEnd] = spaceBetween(top, top);
        auto placement = placementIn(spaceStart, spaceEnd);
        auto blockSize = layoutFor(placement);
        for (auto attempt = 0; narrowsDownBlockSize && attempt < 2; ++attempt) {
            auto [rangeStart, rangeEnd] = spaceBetween(top, top + blockSize);
            if (rangeStart == placement.spaceStart && rangeEnd == placement.spaceEnd)
                break;
            placement = placementIn(rangeStart, rangeEnd);
            blockSize = layoutFor(placement);
        }
        std::tie(placement.spaceStart, placement.spaceEnd) = spaceBetween(top, top + blockSize);
        if (placement.spaceStart == contentBoxStart && placement.spaceEnd == contentBoxEnd)
            return { WTF::move(placement), true };
        // Beside a float at the end, the start margin takes the part of the space that the floats at the start leave in it. Floats past the
        // content box leave no space, which an empty box still fits (RenderBlock::availableLogicalWidthForLine).
        auto spaceInlineSize = std::max(0_lu, placement.spaceEnd - placement.spaceStart);
        auto borderBoxInlineSize = laidOutState->geometry.borderBoxWidth();
        if (placement.spaceEnd >= contentBoxEnd)
            return { WTF::move(placement), borderBoxInlineSize <= spaceInlineSize };
        auto usedMarginStart = !hasAutoInlineMargin ? marginStart : hasOwnInlineSize ? BlockFormattingUtils::inlineMargins(box, borderBoxInlineSize, autoMarginsInlineSize(spaceInlineSize), containingBlockStyle).first : placement.geometry.marginStart();
        auto floatsAtStart = placement.spaceStart - contentBoxStart;
        auto marginStartInSpace = std::max(usedMarginStart, floatsAtStart) - floatsAtStart;
        return { WTF::move(placement), marginStartInSpace + borderBoxInlineSize <= spaceInlineSize };
    };
    auto nextFloatLogicalBottom = [&](LayoutUnit top) -> std::optional<LayoutUnit> {
        auto bottom = std::optional<LayoutUnit> { };
        for (auto& floatItem : floats.list()) {
            auto floatBottom = floatItem.absoluteRectWithMargin().bottom();
            if (floatBottom > top && (!bottom || floatBottom < *bottom))
                bottom = floatBottom;
        }
        return bottom;
    };

    if (shrinksIntoSpace) {
        layoutFor(placementIn(contentBoxStart, contentBoxEnd));
        containingBlockBorderBoxInlineSize = laidOutState->geometry.borderBoxWidth();
    }

    auto borderBoxLogicalTop = hypotheticalBorderBoxLogicalTop;
    // Its clear comes first (RenderBlockFlow::getClearDelta).
    if (box.layoutBox().hasFloatClear()) {
        if (auto floatsLogicalBottom = floatingContextForSidesOf(box, floats).blockEndOfFloatsToClear(box.layoutBox()))
            borderBoxLogicalTop = std::max(borderBoxLogicalTop, *floatsLogicalBottom);
    }
    auto [placement, fits] = placementAt(borderBoxLogicalTop);
    while (!fits) {
        auto nextTop = nextFloatLogicalBottom(borderBoxLogicalTop);
        if (!nextTop)
            break;
        borderBoxLogicalTop = *nextTop;
        std::tie(placement, fits) = placementAt(borderBoxLogicalTop);
    }
    layoutFor(placement);
    boxState = WTF::move(*laidOutState);
    auto& geometry = boxState.geometry;
    // The layout for an earlier space of the same inline size stays, with the margins in this one.
    if (!hasOwnInlineSize)
        geometry.setHorizontalMargin({ placement.geometry.marginStart(), placement.geometry.marginEnd() });
    else {
        // Its own formatting context decided its inline size (layoutIndependentFormattingContextThroughRenderTree): the margins that align it
        // are in the space.
        auto [usedMarginStart, usedMarginEnd] = BlockFormattingUtils::inlineMargins(box, geometry.borderBoxWidth(), containingBlockState.inlineRange.size(), containingBlockStyle, autoMarginsInlineSize(placement.spaceEnd - placement.spaceStart));
        geometry.setHorizontalMargin({ usedMarginStart, usedMarginEnd });
    }
    // Its end margin still takes what makes the equality true in the containing block, past the floats (RenderBox::computeLogicalWidth).
    if (containingBlockState.inlineRange.size() && BlockFormattingUtils::ignoresEndMarginWhenOverConstrained(box, containingBlockStyle))
        geometry.setHorizontalMargin({ geometry.marginStart(), containingBlockState.inlineRange.size() - geometry.borderBoxWidth() - geometry.marginStart() });
    // justify-self moves it from there by the free space in the containing block, not in the space beside the floats.
    geometry.setLeft(borderBoxStartInSpace(placement.spaceStart, geometry) + BlockFormattingUtils::justifySelfOffset(box, geometry.borderBoxWidth(), geometry.marginStart(), geometry.marginEnd(), containingBlockState.inlineRange.size(), containingBlockStyle));
    // Below the hypothetical position the floats put it, as clearance would (resolveBorderBoxLogicalTops).
    if (borderBoxLogicalTop > hypotheticalBorderBoxLogicalTop) {
        if (pendingFloats.isEmpty())
            boxState.clearance = ContainingBlockState::Clearance { borderBoxLogicalTop, { }, marginsBeforeLogicalBottom };
        else
            boxState.clearance = ContainingBlockState::Clearance { LayoutUnit::min(), { { containingBlockState.blockContainer, borderBoxLogicalTop - hypotheticalBorderBoxLogicalTop } }, marginsBeforeLogicalBottom };
    }
    return laidOutContentBlockSize;
}

auto BlockFormattingContext::handleIndependentFormattingContextRoot(const RenderBox& box, BlockLayoutResult& result) -> std::optional<RootContentBreak>
{
    auto rootState = containingBlockStateAtBlockStart(box);
    auto* openRoot = std::exchange(m_openBlockFormattingContextRootToResume, nullptr);
    auto resumedBlockStepExtraSpace = std::exchange(m_blockStepExtraSpaceToResume, std::nullopt);
    if (openRoot) {
        // It keeps the inline position and size it has beside the floats in the fragmentainers before, like in the render tree, where it
        // has one for all its fragments.
        auto geometry = rootState.geometry;
        geometry.setHorizontalMargin({ openRoot->geometry.marginStart(), openRoot->geometry.marginEnd() });
        geometry.setContentBoxWidth(openRoot->geometry.contentBoxWidth());
        geometry.setLeft(BoxGeometry::borderBoxLeft(openRoot->geometry));
        rootState = containingBlockStateFor(box, geometry, m_blockLayoutState.containingBlockStates().last());
        continueFromFragmentainerBefore(rootState, openRoot->consumedBlockSize);
    }
    // Its content breaks in the fragmentainer below the content before it, from where it stopped in the fragmentainer before.
    auto fragmentsContent = m_fragmentainer && hasFragmentableContent(box) && breaksInside(std::span { &rootState, 1 });
    auto rootContentBreak = fragmentsContent ? std::optional { RootContentBreak { } } : std::nullopt;
    auto rootSpaceShortage = std::optional<LayoutUnit> { };
    auto rootUnbreakableBlockSize = LayoutUnit { };
    // Its margins do not collapse with its children's, nor through it, even when it is empty (CSS 2.2 §8.3.1). With a block step, they
    // collapse once the block size is known, which its content, in a formatting context of its own, does not depend on the position for, and
    // so do those of a fieldset, once its own layout has placed its legend, whose block-start margin it takes (RenderBlock::marginBefore).
    auto stepSize = blockStepSize(box);
    // When its content breaks, the render tree rounds its margin box as it is where the margins of its style put it, breaks included, and puts
    // it where the margins with the extra space put it, where it is laid out again, and where its first line may not fit, which takes it to
    // the next fragmentainer, with the extra space in its block-end margin (RenderBlockFlow::layoutBlockChild). css-rhythm-1 §2 rounds each
    // fragment instead.
    auto stepsAcrossBreaks = stepSize && m_fragmentainer && breaksInside(std::span { &rootState, 1 }) && (hasFragmentableContent(box) || breaksContentThroughRenderTree(box));
    // A column set, with no margins, separates the margins around it unless it has no columns, when they collapse through it like through an
    // empty block container, as those of adjacent column spanners do (css-multicol-1 §6.1).
    auto collapsesMarginsOnceLaidOut = (stepSize && !stepsAcrossBreaks) || box.isFieldset() || box.isRenderMultiColumnSet();
    auto measuresBlockSizeAcrossBreaks = false;
    auto contentBlockSizeAcrossBreaks = std::optional<LayoutUnit> { };

    // A block formatting context root the block formatting context lays out adds the boxes in its content to the result, which another layout
    // of it replaces.
    auto resultBoxCount = result.boxes.size();
    auto resultOutOfFlowBoxCount = result.outOfFlowBoxes.size();
    auto resultInlineContentFragmentCount = result.inlineContentFragments.size();
    auto layoutContent = [&](ContainingBlockState& state, std::optional<LayoutUnit> borderBoxInlineSize, std::optional<LayoutUnit> stretchFitBorderBoxInlineSize) {
        result.boxes.shrink(resultBoxCount);
        result.outOfFlowBoxes.shrink(resultOutOfFlowBoxCount);
        result.inlineContentFragments.shrink(resultInlineContentFragmentCount);
        auto contentBlockSize = std::optional<LayoutUnit> { };
        auto legendMarginBefore = LayoutUnit { };
        CheckedPtr root = dynamicDowncast<RenderBlockFlow>(box);
        if (CheckedPtr columnSet = dynamicDowncast<RenderMultiColumnSet>(box)) {
            // Its columns, as tall as column balancing or the multi-column container makes them, give it its block size
            // (RenderMultiColumnSet::computeLogicalHeight).
            contentBlockSize = columnSet->availableColumnHeight();
        } else if (!root || LayoutIntegration::BlockIntegrationUtils::laysOutThroughRenderTree(*root)) {
            legendMarginBefore = layoutIndependentFormattingContextThroughRenderTree(box, m_blockLayoutState.containingBlockStates().last(), state, borderBoxInlineSize, stretchFitBorderBoxInlineSize);
            contentBlockSize = state.geometry.contentBoxHeight();
        } else if (openRoot && !openRoot->contentBreakToken) {
            // Only the rest of its block size continues from the fragmentainer before, below its content that overflows into this one.
            contentBlockSize = openRoot->overflowingContentBlockSize;
        } else if (!root->firstChild())
            contentBlockSize = BlockFormattingUtils::emptyEditableContentBlockSize(box);
        else if (root->layoutContext().isSkippedContentRootForLayout(*root)) {
            // content-visibility skips its content, which stays as it is, and size containment sizes the box (RenderBlockFlow::layoutInFlowChildren).
            contentBlockSize = 0_lu;
        } else {
            LayoutIntegration::BlockIntegrationUtils::markPercentageHeightDescendantsForLayout(*root, state.contentBoxBlockSize);
            // Its content is in a formatting context of its own; a block formatting context gets a block layout state of its own, with its own
            // text-box-trim only, whose start trim a block-start border or padding keeps from the content after it too, and the last formatted
            // lines of the boxes with trim-end in it.
            auto blockLayoutState = BlockLayoutState { { } };
            blockLayoutState.setLastFormattedLineRoots(Vector { m_blockLayoutState.lastFormattedLineRoots() });
            // -webkit-line-clamp counts the lines in it too, unlike line-clamp (LineClampUpdater).
            if (auto lineClamp = m_blockLayoutState.lineClamp(); lineClamp && lineClamp->isLegacy)
                blockLayoutState.setLineClamp(lineClamp);
            if (root->style().textBoxTrim() != TextBoxTrim::None) {
                auto textBoxTrimState = m_blockLayoutState.textBoxTrimState();
                if (textBoxTrimState.trimsFirstFormattedLine && state.geometry.borderAndPaddingBefore()) {
                    textBoxTrimState.trimsFirstFormattedLine = false;
                    m_blockLayoutState.setTextBoxTrimState(textBoxTrimState);
                }
                blockLayoutState.setTextBoxTrimState(textBoxTrimStateFor(*root, textBoxTrimState));
            }
            auto horizontalConstraints = HorizontalConstraints { state.inlineRange.min(), state.inlineRange.size() };
            auto initialBlockLayoutState = blockLayoutState;
            // The render tree path shifts the content instead of laying it out again, which would count its lines again.
            auto initialLegacyLineClamp = LayoutIntegration::BlockIntegrationUtils::legacyLineClamp(box);
            auto layoutRootContent = [&](LayoutUnit shift, std::optional<LayoutUnit> borderBoxStripLogicalTop = { }) {
                auto constraints = ConstraintsForBlockContent { { horizontalConstraints, state.borderAndPaddingBefore + shift }, state.contentBoxBlockSize, state.fillAvailableBlockSize, state.replacedElementAvailableBlockSize, state.relayoutChildren };
                auto fragmentainer = std::optional<FragmentainerConstraints> { };
                if (fragmentsContent) {
                    auto rootBlockOffset = m_fragmentainer->rootBlockOffset + *state.borderBoxLogicalTop;
                    // The content of one with a block size of its own is a flow parallel to the content after it (css-break-3 §2.1), which
                    // breaks across the fragmentainers from where the root starts, also past where the root ends, while the root breaks like a
                    // block container of that block size.
                    if (auto* fragmentedContentLayout = integrationUtils().fragmentedContentLayout(); fragmentedContentLayout && (borderBoxStripLogicalTop || hasBlockSizeOfItsOwn(box)))
                        return (*fragmentedContentLayout)(*root, blockLayoutState, constraints, state.geometry, rootLineGridForNestedRoot(state), borderBoxStripLogicalTop.value_or(m_fragmentainer->stripLogicalTop + rootBlockOffset), m_fragmentainer->appliesForcedBreaks);
                    fragmentainer = FragmentainerConstraints { m_fragmentainer->blockSize, m_fragmentainer->type, rootBlockOffset, m_fragmentainerHasContent, m_fragmentainer->appliesForcedBreaks, false, m_fragmentainer->stripLogicalTop };
                    fragmentainer->breaksLikePages = m_fragmentainer->breaksLikePages;
                }
                return BlockFormattingContext { *root, integrationUtils(), blockLayoutState }.layout(constraints, state.geometry, rootLineGridForNestedRoot(state), fragmentainer, openRoot ? openRoot->contentBreakToken.get() : nullptr);
            };
            auto rootResult = layoutRootContent({ });
            // Whether none of the content fits below the content before the root, which does not depend on where align-content moves it.
            auto isBreakBeforeContent = rootResult.isBreakBeforeContent;
            // align-content moves the content as a unit in the block size the root gets from that of its content, where it is laid out again,
            // but not one at least as tall as a fragmentainer, like the render tree does (RenderBlockFlow::layoutBlock).
            if (!root->style().alignContent().isNormal()) {
                auto geometry = state.geometry;
                computeBlockSize(box, rootResult.intrinsicContentBlockSize, m_blockLayoutState.containingBlockStates().last(), geometry);
                auto isAligned = !m_fragmentainer || geometry.borderBoxHeight() < m_fragmentainer->wholeBlockSize();
                if (auto shift = isAligned ? BlockFormattingUtils::alignContentShift(box, geometry.contentBoxHeight() - rootResult.intrinsicContentBlockSize) : 0_lu) {
                    blockLayoutState = initialBlockLayoutState;
                    LayoutIntegration::BlockIntegrationUtils::restoreLegacyLineClamp(box, initialLegacyLineClamp);
                    rootResult = layoutRootContent(shift);
                }
            }
            // The block size its content gives it across the breaks, which the render tree has for the box (RenderBlockFlow::layoutBlockChild),
            // from the start of the next fragmentainer when none of its content fits below the content before it, which a pagination strut
            // takes it to (RenderBlockFlow::adjustLinePositionForPagination).
            if (measuresBlockSizeAcrossBreaks) {
                contentBlockSizeAcrossBreaks = std::nullopt;
                auto borderBoxStripLogicalTop = std::optional<LayoutUnit> { };
                if (isBreakBeforeContent && m_fragmentainerHasContent && m_fragmentainer->blockSize)
                    borderBoxStripLogicalTop = m_fragmentainer->stripLogicalTop + *m_fragmentainer->blockSize;
                else if (rootResult.breakToken && !rootResult.breakToken->hasOnlyParallelFlows)
                    borderBoxStripLogicalTop = m_fragmentainer->stripLogicalTop + m_fragmentainer->rootBlockOffset + *state.borderBoxLogicalTop;
                if (borderBoxStripLogicalTop) {
                    blockLayoutState = initialBlockLayoutState;
                    contentBlockSizeAcrossBreaks = layoutRootContent({ }, borderBoxStripLogicalTop).intrinsicContentBlockSize;
                }
            }
            result.boxes.appendVector(WTF::move(rootResult.boxes));
            result.fragmentedBoxes.appendVector(WTF::move(rootResult.fragmentedBoxes));
            contentBlockSize = rootResult.intrinsicContentBlockSize;
            result.outOfFlowBoxes.appendVector(WTF::move(rootResult.outOfFlowBoxes));
            result.outOfFlowContainingBlockLineGrids.appendVector(WTF::move(rootResult.outOfFlowContainingBlockLineGrids));
            if (fragmentsContent) {
                result.inlineContentFragments.appendVector(WTF::move(rootResult.inlineContentFragments));
                rootContentBreak = RootContentBreak { WTF::move(rootResult.breakToken), isBreakBeforeContent, rootResult.fragmentainerBlockSize };
                rootSpaceShortage = rootResult.spaceShortage;
                rootUnbreakableBlockSize = rootResult.unbreakableBlockSize;
            }
        }
        return std::pair { contentBlockSize, legendMarginBefore };
    };

    auto contentBlockSize = std::optional<LayoutUnit> { };
    auto legendMarginBefore = LayoutUnit { };
    auto isLaidOut = false;
    auto* placedFloats = m_blockLayoutState.placedFloats();
    auto isAfterFloats = !openRoot && placedFloats && !placedFloats->isEmpty();
    auto blockStepExtraSpace = std::optional<LayoutUnit> { };
    if (stepsAcrossBreaks) {
        auto& geometry = rootState.geometry;
        if (openRoot)
            geometry.setVerticalMargin({ geometry.marginBefore(), openRoot->geometry.marginAfter() });
        else {
            blockStepExtraSpace = resumedBlockStepExtraSpace;
            if (!blockStepExtraSpace) {
                auto initialBlockLayoutState = m_blockLayoutState;
                auto initialRootState = rootState;
                auto marginsBeforeLogicalBottom = m_blockLayoutState.marginCollapse().collapsedLogicalBottomAtContent();
                m_blockLayoutState.marginCollapse().startBlockContainer(rootState, m_blockLayoutState.containingBlockStates().last(), geometry.marginBefore());
                measuresBlockSizeAcrossBreaks = true;
                if (isAfterFloats)
                    contentBlockSize = placeBesideFloats(box, rootState, marginsBeforeLogicalBottom, layoutContent);
                else {
                    resolveBorderBoxLogicalTops(rootState);
                    contentBlockSize = layoutContent(rootState, { }, { }).first;
                }
                measuresBlockSizeAcrossBreaks = false;
                auto borderBoxBlockSize = rootState.geometry.borderBoxHeight();
                if (!rootState.isLaidOutThroughRenderTree) {
                    auto measuredGeometry = rootState.geometry;
                    computeBlockSize(box, contentBlockSizeAcrossBreaks.value_or(contentBlockSize.value_or(0_lu)), m_blockLayoutState.containingBlockStates().last(), measuredGeometry);
                    borderBoxBlockSize = measuredGeometry.borderBoxHeight();
                }
                blockStepExtraSpace = BlockStepSizing::computeExtraSpace(*stepSize, initialRootState.geometry.marginBefore() + borderBoxBlockSize + initialRootState.geometry.marginAfter());
                m_blockLayoutState = WTF::move(initialBlockLayoutState);
                rootState = WTF::move(initialRootState);
            }
            // Not in a block-start margin truncated at an unforced break.
            if (blockStepExtraSpace) {
                auto halfExtraSpace = *blockStepExtraSpace / 2;
                geometry.setVerticalMargin({ geometry.marginBefore() + (m_marginsAdjoinUnforcedBreak ? 0_lu : halfExtraSpace), geometry.marginAfter() + halfExtraSpace });
            }
        }
    }
    auto stepMarginBefore = rootState.geometry.marginBefore();
    auto stepMarginAfter = rootState.geometry.marginAfter();
    if (!collapsesMarginsOnceLaidOut) {
        auto marginsBeforeLogicalBottom = m_blockLayoutState.marginCollapse().collapsedLogicalBottomAtContent();
        m_blockLayoutState.marginCollapse().startBlockContainer(rootState, m_blockLayoutState.containingBlockStates().last(), rootState.geometry.marginBefore());
        if (isAfterFloats) {
            contentBlockSize = placeBesideFloats(box, rootState, marginsBeforeLogicalBottom, layoutContent);
            isLaidOut = true;
        }
        resolveBorderBoxLogicalTops(rootState);
    } else if (isAfterFloats) {
        // Like RenderBlockFlow::layoutBlockChild, it is laid out beside the floats where the margins of its style put it first, then its margins
        // take what that layout adds to them and it goes beside the floats from where they put it, laid out again for the space there.
        auto initialBlockLayoutState = m_blockLayoutState;
        auto initialRootState = rootState;
        auto layoutContentKeepingLegendMargin = [&](ContainingBlockState& state, std::optional<LayoutUnit> borderBoxInlineSize, std::optional<LayoutUnit> stretchFitBorderBoxInlineSize) {
            auto contentLayoutResult = layoutContent(state, borderBoxInlineSize, stretchFitBorderBoxInlineSize);
            legendMarginBefore = contentLayoutResult.second;
            return contentLayoutResult;
        };
        m_blockLayoutState.marginCollapse().startBlockContainer(rootState, m_blockLayoutState.containingBlockStates().last(), rootState.geometry.marginBefore());
        placeBesideFloats(box, rootState, m_blockLayoutState.marginCollapse().collapsedLogicalBottomAtContent(), layoutContentKeepingLegendMargin);
        auto marginBefore = initialRootState.geometry.marginBefore();
        auto marginAfter = initialRootState.geometry.marginAfter();
        if (stepSize) {
            auto halfExtraSpace = BlockStepSizing::computeExtraSpace(*stepSize, marginBefore + rootState.geometry.borderBoxHeight() + marginAfter) / 2;
            marginBefore += halfExtraSpace;
            marginAfter += halfExtraSpace;
        }
        m_blockLayoutState = WTF::move(initialBlockLayoutState);
        rootState = WTF::move(initialRootState);
        auto marginsBeforeLogicalBottom = m_blockLayoutState.marginCollapse().collapsedLogicalBottomAtContent();
        m_blockLayoutState.marginCollapse().startBlockContainer(rootState, m_blockLayoutState.containingBlockStates().last(), marginBefore + legendMarginBefore);
        contentBlockSize = placeBesideFloats(box, rootState, marginsBeforeLogicalBottom, layoutContent);
        rootState.geometry.setVerticalMargin({ marginBefore, marginAfter });
        resolveBorderBoxLogicalTops(rootState);
        isLaidOut = true;
    }
    if (!isLaidOut)
        std::tie(contentBlockSize, legendMarginBefore) = layoutContent(rootState, { }, { });

    if (stepsAcrossBreaks) {
        rootState.geometry.setVerticalMargin({ stepMarginBefore, stepMarginAfter });
        m_rootBlockStepExtraSpace = blockStepExtraSpace;
    } else if (stepSize && !isAfterFloats) {
        auto& geometry = rootState.geometry;
        if (!rootState.isLaidOutThroughRenderTree)
            computeBlockSize(box, contentBlockSize.value_or(0_lu), m_blockLayoutState.containingBlockStates().last(), geometry);
        auto halfExtraSpace = BlockStepSizing::computeExtraSpace(*stepSize, geometry.marginBefore() + geometry.borderBoxHeight() + geometry.marginAfter()) / 2;
        geometry.setVerticalMargin({ geometry.marginBefore() + halfExtraSpace, geometry.marginAfter() + halfExtraSpace });
        m_blockLayoutState.marginCollapse().startBlockContainer(rootState, m_blockLayoutState.containingBlockStates().last(), geometry.marginBefore());
        resolveBorderBoxLogicalTops(rootState);
    } else if (collapsesMarginsOnceLaidOut && !isAfterFloats) {
        m_blockLayoutState.marginCollapse().startBlockContainer(rootState, m_blockLayoutState.containingBlockStates().last(), rootState.geometry.marginBefore() + legendMarginBefore);
        if (!box.isRenderMultiColumnSet() || contentBlockSize.value_or(0_lu))
            resolveBorderBoxLogicalTops(rootState);
    }
    if (contentBlockSize && rootState.borderBoxLogicalTop)
        m_blockLayoutState.marginCollapse().resetAdjoiningMargins(rootState.contentBoxLogicalTop() + *contentBlockSize);
    m_blockLayoutState.marginCollapse().endBlockContainer(rootState, m_blockLayoutState.containingBlockStates().last());
    placeInContainingBlock(box, WTF::move(rootState), result);
    if (rootSpaceShortage)
        addSpaceShortage(*rootSpaceShortage);
    addUnbreakableBlockSize(rootUnbreakableBlockSize);
    return rootContentBreak;
}

// The inline size of a box laid out through the render tree that its own formatting context decides: of a table (CSS 2.2 §17.5.2), of a
// replaced element (§10.3.4), of a frameset, which takes the size of the viewport (RenderFrameSet::layout), of a MathML box, which takes the
// width of its content, or centers it in the stretch-fit inline size with display: block (RenderMathMLRow::layoutBlock,
// RenderMathMLMath::layoutBlock), of a box that fits its content, like a float or a button (RenderBox::sizesLogicalWidthToFitContent), or
// that has a sizing keyword inline size, which it resolves with its own intrinsic sizes (css-sizing-3 §5.1), and of an orthogonal box, for
// which it is the block size (css-writing-modes-4 §7.3).
static bool decidesItsOwnInlineSize(const RenderBox& box, WritingMode writingMode)
{
#if ENABLE(MATHML)
    if (box.isRenderMathMLBlock())
        return true;
#endif
    return is<RenderTable>(box) || box.isRenderReplaced() || box.isRenderOrLegacyRenderSVGRoot() || box.isRenderFrameSet() || box.sizesLogicalWidthToFitContent() || BlockFormattingUtils::hasIntrinsicInlineSize(box, writingMode)
        || box.writingMode().isOrthogonal(writingMode) || box.style().aspectRatio().hasRatio();
}

// Everything that belongs to the block formatting context is computed here: the margins, the position, the containing block and, for a box
// whose inline size its own formatting context does not decide, or the stretch inline sizes of a replaced element, the stretch-fit inline size. The rest comes back from the box's own formatting
// context, laid out through the render tree, which lays out its content: the inline size it decides (decidesItsOwnInlineSize), unless it is
// a table that shrinks into the space beside floats (placeBesideFloats), the block size, border and padding, and what it adds to the
// block-start margin, which collapses with the others once the box is laid out.
LayoutUnit BlockFormattingContext::layoutIndependentFormattingContextThroughRenderTree(const RenderBox& box, const ContainingBlockState& containingBlockState, ContainingBlockState& boxState, std::optional<LayoutUnit> borderBoxInlineSize, std::optional<LayoutUnit> stretchFitBorderBoxInlineSize, std::optional<LayoutSize> placedBorderBoxLogicalLeftAndTopInRoot, const BlockBreakToken::PushedFloat::RenderTreeLayout* pushedFloat)
{
    auto containingBlockInlineSize = containingBlockState.inlineRange.size();
    auto& geometry = boxState.geometry;
    auto containingBlockBlockSize = [&] -> std::optional<LayoutUnit> {
        if (box.writingMode().isOrthogonal(root().writingMode()))
            return orthogonalFlowAvailableInlineSpace(containingBlockState);
        // In quirks mode a stretch block size, which takes the block size of the containing block like a percentage, does not look past it.
        if (box.document().inQuirksMode() && BlockFormattingUtils::hasOnlyStretchBlockSizes(box))
            return stretchFitBasis(containingBlockState);
        if (box.isRenderReplaced() && box.document().inQuirksMode())
            return containingBlockState.replacedElementAvailableBlockSize;
        return containingBlockState.contentBoxBlockSize;
    };
    // The one in the content box of the containing block, away from floats.
    auto stretchFitInlineSize = [&] -> std::optional<LayoutUnit> {
        if (stretchFitBorderBoxInlineSize || !hasStretchFitInlineSizeBesideFloats(box, root().writingMode()))
            return stretchFitBorderBoxInlineSize;
        CheckedRef style = box.style();
        CheckedRef containingBlockStyle = containingBlockState.blockContainer->style();
        auto margin = [&](auto& length) {
            return length.isAuto() ? 0_lu : Style::evaluateMinimum<LayoutUnit>(length, containingBlockInlineSize, style->usedZoomForLength());
        };
        auto containingBlockWritingMode = containingBlockStyle->writingMode();
        return containingBlockInlineSize - margin(style->marginStart(containingBlockWritingMode)) - margin(style->marginEnd(containingBlockWritingMode));
    };
    auto constraints = ConstraintsForIndependentFormattingContext {
        .containingBlockInlineSize = containingBlockInlineSize,
        .containingBlockBlockSize = containingBlockBlockSize(),
        .relayoutChildren = containingBlockState.relayoutChildren,
        .borderBoxInlineSize = decidesItsOwnInlineSize(box, root().writingMode()) ? borderBoxInlineSize : std::optional { geometry.borderBoxWidth() },
        .stretchFitBorderBoxInlineSize = stretchFitInlineSize(),
        .marginBefore = geometry.marginBefore(),
        .marginAfter = geometry.marginAfter(),
        .containingBlockMarginBefore = containingBlockState.geometry.marginBefore(),
        .containingBlockMarginAfter = containingBlockState.geometry.marginAfter(),
        // Percentages that look past the box to a table cell behave as auto (RenderBox::computePercentageLogicalHeight).
        .contentPercentageBasis = containingBlockIsTableCell(containingBlockState) == BlockFormattingUtils::ContainingBlockIsTableCell::Yes ? std::nullopt : containingBlockState.contentBoxBlockSize,
        .containingBlockAvailableBlockSize = containingBlockState.fillAvailableBlockSize,
        .containingBlockAvailableBlockSizeIncludingMarginBorderPadding = containingBlockState.replacedElementAvailableBlockSize
    };
    // Its content snaps to the line grid around it from where the box is (RenderBlockFlow::layoutBlockChild places a block before its layout).
    auto offsetInRoot = [&] {
        if (placedBorderBoxLogicalLeftAndTopInRoot)
            return *placedBorderBoxLogicalLeftAndTopInRoot;
        return logicalLeftAndTopInRoot(inlineStartInRoot(containingBlockState, BoxGeometry::borderBoxLeft(geometry), geometry.borderBoxWidth()), geometry.borderBoxWidth(), borderBoxOffsetInRoot(boxState).height());
    };
    // And its content breaks across the pages from there, also that of a flex container, which only avoids breaks inside it where it starts
    // while it fits in a fragmentainer (RenderBlockFlow::adjustForUnsplittableChild paginates its content from where it is).
    if (hasContentFragmentedByRenderTree(box, m_fragmentainer))
        constraints.borderBoxStripLogicalTop = m_fragmentainer->stripLogicalTop + m_fragmentainer->rootBlockOffset + offsetInRoot().height();
    auto lineGrid = std::optional<LineGridInLayoutState> { };
    auto layout = [&] {
        // The containing block of a float that went to the next fragmentainer is not in the states there yet, where the line grids stay.
        if (pushedFloat) {
            lineGrid = pushedFloat->lineGrid;
            if (lineGrid)
                lineGrid->boxBorderBoxOffset = offsetInLayoutState(offsetInRoot());
        } else
            lineGrid = lineGridForBoxLaidOutThroughRenderTree(box, containingBlockState, offsetInRoot());
        auto layoutResult = integrationUtils().layoutIndependentFormattingContext(box, constraints, lineGrid);
        geometry.setBorder(layoutResult.border);
        geometry.setPadding(layoutResult.padding);
        setContentBoxInlineSize(geometry, layoutResult.borderBoxInlineSize);
        setContentBoxBlockSize(geometry, layoutResult.borderBoxBlockSize);
        CheckedRef containingBlockStyle = containingBlockState.blockContainer->style();
        auto [marginStart, marginEnd] = BlockFormattingUtils::inlineMargins(box, layoutResult.borderBoxInlineSize, containingBlockInlineSize, containingBlockStyle);
        geometry.setHorizontalMargin({ marginStart, marginEnd });
        computeLogicalLeft(box, containingBlockState, geometry);
        return layoutResult.legendMarginBefore;
    };
    auto legendMarginBefore = layout();
    boxState.borderAndPaddingBefore = geometry.borderAndPaddingBefore();
    boxState.isLaidOutThroughRenderTree = true;
    return legendMarginBefore;
}

// The block-axis margins, border, padding and content box block size of a fragment of a box.
static void setBlockAxisGeometry(BoxGeometry& geometry, const BoxGeometry& fragmentGeometry)
{
    geometry.setVerticalMargin({ fragmentGeometry.marginBefore(), fragmentGeometry.marginAfter() });
    geometry.setVerticalBorder({ fragmentGeometry.borderBefore(), fragmentGeometry.borderAfter() });
    geometry.setVerticalPadding({ fragmentGeometry.paddingBefore(), fragmentGeometry.paddingAfter() });
    geometry.setContentBoxHeight(fragmentGeometry.contentBoxHeight());
}

void BlockFormattingContext::handleFloat(const RenderBox& box, BlockLayoutResult& result)
{
    ASSERT(m_blockLayoutState.containingBlockStates().last().blockContainer.ptr() == box.parent());
    // A float before the clamp point of a line-clamp container is clipped to its block-end content edge (css-overflow-4 §5.3.1).
    auto lineClamp = m_blockLayoutState.lineClamp();
    LayoutIntegration::BlockIntegrationUtils::setFloatClippedByLineClamp(box, lineClamp && lineClamp->shouldDiscardOverflow);
    // Its own formatting context lays it out at its shrink-to-fit inline size (CSS 2.2 §10.3.5), unless the block formatting context lays out
    // its content, once it knows where the float goes.
    auto boxState = containingBlockStateAtBlockStart(box);
    auto& containingBlockState = m_blockLayoutState.containingBlockStates().last();
    auto laysOutContent = laysOutFloatContent(box);
    if (!laysOutContent)
        layoutIndependentFormattingContextThroughRenderTree(box, containingBlockState, boxState);
    auto& geometry = boxState.geometry;
    computeFloatMargins(box, containingBlockState, geometry);

    // It goes no higher than where it would be if it were not floating, below the margins before it, unless they still collapse with the
    // containing block's top, which it then goes to (CSS 2.2 §9.5.1 rules 4 and 5, RenderBlockFlow::adjustFloatingBlock).
    auto containingBlockOffsetInRoot = borderBoxOffsetInRoot(containingBlockState);
    auto marginBoxLogicalTop = containingBlockState.borderBoxLogicalTop ? m_blockLayoutState.marginCollapse().collapsedLogicalBottom() - *containingBlockState.borderBoxLogicalTop : containingBlockState.borderAndPaddingBefore;
    geometry.setTop(marginBoxLogicalTop + geometry.marginBefore());
    auto& placedFloats = m_blockLayoutState.ensurePlacedFloats(root().layoutBox());
    auto floatingContext = floatingContextForSidesOf(box, placedFloats);
    // In the root's logical coordinates, which mirror those of a containing block whose inline direction is the opposite.
    auto isOpposite = containingBlockState.hasInlineDirectionOppositeToRoot;
    auto containingBlockInlineSize = containingBlockState.geometry.borderBoxWidth();
    auto containingBlock = containingBlockForFloats(containingBlockState);
    auto geometryInRoot = geometry;
    if (isOpposite)
        geometryInRoot.setHorizontalMargin({ geometry.marginEnd(), geometry.marginStart() });
    geometryInRoot.setTopLeft(floatingContext.positionForFloat(box.layoutBox(), geometryInRoot, containingBlock));
    // -webkit-line-align puts it against the edges of the space beside the floats on the columns of the line grid, lower down while its margin
    // box, no wider than the content box, does not fit between them (RenderBlockFlow::computeLogicalLocationForFloat).
    if (containingBlockState.lineGrid && containingBlockState.blockContainer->style().lineAlign() != LineAlign::None) {
        auto marginBoxLogicalTop = containingBlockOffsetInRoot.height() + BoxGeometry::borderBoxTop(geometryInRoot) - geometry.marginBefore();
        auto spaceAt = [&](LayoutUnit top) {
            auto [spaceStart, spaceEnd] = spaceBesideFloats(floatingContext, containingBlockState, top, top);
            return spaceOnLineGridColumns(containingBlockState, spaceStart, spaceEnd);
        };
        auto marginBoxInlineSize = std::min(geometry.marginBoxWidth(), containingBlockState.inlineRange.size());
        auto [spaceStart, spaceEnd] = spaceAt(marginBoxLogicalTop);
        while (spaceEnd - spaceStart < marginBoxInlineSize) {
            auto nextFloatLogicalBottom = std::optional<LayoutUnit> { };
            for (auto& floatItem : placedFloats.list()) {
                auto floatLogicalBottom = floatItem.absoluteRectWithMargin().bottom();
                if (floatLogicalBottom > marginBoxLogicalTop && (!nextFloatLogicalBottom || floatLogicalBottom < *nextFloatLogicalBottom))
                    nextFloatLogicalBottom = floatLogicalBottom;
            }
            if (!nextFloatLogicalBottom)
                break;
            marginBoxLogicalTop = *nextFloatLogicalBottom;
            std::tie(spaceStart, spaceEnd) = spaceAt(marginBoxLogicalTop);
        }
        auto isAtInlineStart = (box.usedStyle().floating() == UsedFloat::Left) == containingBlockState.blockContainer->writingMode().isLogicalLeftInlineStart();
        auto borderBoxStart = (isAtInlineStart ? spaceStart : spaceEnd - marginBoxInlineSize) + geometry.marginStart();
        geometryInRoot.setTopLeft({ isOpposite ? containingBlockInlineSize - borderBoxStart - geometry.borderBoxWidth() : borderBoxStart, marginBoxLogicalTop - containingBlockOffsetInRoot.height() + geometry.marginBefore() });
    }
    // Its content snaps to the line grid around it from where it goes (RenderBlockFlow::positionNewFloats places a float before its layout),
    // where it stays, and the render tree breaks it across the fragmentainers from there.
    auto contentIsFragmentedByRenderTree = !laysOutContent && hasContentFragmentedByRenderTree(box, m_fragmentainer);
    if (!laysOutContent && (contentIsFragmentedByRenderTree || lineGridForBoxLaidOutThroughRenderTree(box, containingBlockState, { }))) {
        auto placedOffsetInRoot = logicalLeftAndTopInRoot(containingBlockOffsetInRoot.width() + BoxGeometry::borderBoxLeft(geometryInRoot), geometry.borderBoxWidth(), containingBlockOffsetInRoot.height() + BoxGeometry::borderBoxTop(geometryInRoot));
        auto placedGeometry = geometry;
        layoutIndependentFormattingContextThroughRenderTree(box, containingBlockState, boxState, { }, { }, placedOffsetInRoot);
        geometry.setHorizontalMargin({ placedGeometry.marginStart(), placedGeometry.marginEnd() });
        geometry.setVerticalMargin({ placedGeometry.marginBefore(), placedGeometry.marginAfter() });
        auto topLeft = BoxGeometry::borderBoxTopLeft(geometryInRoot);
        geometryInRoot.setContentBoxSize(geometry.contentBoxSize());
        geometryInRoot.setTopLeft(topLeft);
    }
    auto contentResult = BlockLayoutResult { };
    auto floatContentLayout = std::optional<FloatContentLayout> { };
    auto floatContent = std::optional<BlockBreakToken::PushedFloat::Content> { };
    if (laysOutContent) {
        auto content = BlockBreakToken::PushedFloat::Content { containingBlockState, boxState, breaksInside(m_blockLayoutState.containingBlockStates().span().subspan(1)) };
        auto borderBoxLogicalTopInRoot = containingBlockOffsetInRoot.height() + BoxGeometry::borderBoxTop(geometryInRoot);
        setLineGridOfFloatContent(content, logicalLeftAndTopInRoot(containingBlockOffsetInRoot.width() + BoxGeometry::borderBoxLeft(geometryInRoot), geometry.borderBoxWidth(), borderBoxLogicalTopInRoot));
        floatContentLayout = layoutFloatContent(downcast<RenderBlockFlow>(box), content, borderBoxLogicalTopInRoot);
        contentResult = std::exchange(floatContentLayout->result, { });
        setBlockAxisGeometry(geometry, floatContentLayout->fragmentGeometry);
        setBlockAxisGeometry(geometryInRoot, floatContentLayout->fragmentGeometry);
        m_floatContents.removeFirstMatching([&](auto& floatContent) {
            return floatContent.box.ptr() == &box;
        });
        floatContent = content;
        m_floatContents.append({ box, WTF::move(content), contentResult.isBreakBeforeContent });
        LayoutIntegration::BlockIntegrationUtils::setShapeOutside(box, geometry, containingBlockState.blockContainer->writingMode(), containingBlockState.inlineRange.size());
    }
    if (m_fragmentainer) {
        auto renderTreeLayout = std::optional<BlockBreakToken::PushedFloat::RenderTreeLayout> { };
        if (contentIsFragmentedByRenderTree) {
            auto placedOffsetInRoot = logicalLeftAndTopInRoot(containingBlockOffsetInRoot.width() + BoxGeometry::borderBoxLeft(geometryInRoot), geometry.borderBoxWidth(), containingBlockOffsetInRoot.height() + BoxGeometry::borderBoxTop(geometryInRoot));
            renderTreeLayout = { containingBlockState, boxState, lineGridForBoxLaidOutThroughRenderTree(box, containingBlockState, placedOffsetInRoot) };
        }
        m_floatsInFragmentainer.append({ box.layoutBox(), containingBlock, WTF::move(renderTreeLayout) });
    }
    auto floatItem = floatingContext.makeFloatItem(box.layoutBox(), geometryInRoot, containingBlock);
    auto borderBoxLeft = BoxGeometry::borderBoxLeft(geometryInRoot);
    geometry.setTopLeft({ isOpposite ? containingBlockInlineSize - borderBoxLeft - geometry.borderBoxWidth() : borderBoxLeft, BoxGeometry::borderBoxTop(geometryInRoot) });
    auto startsInNextFragmentainer = floatContentLayout && floatContentLayout->offsetPastFragmentainerEnd;
    if (floatContentLayout)
        continueFloat(box, WTF::move(*floatContent), *floatContentLayout, geometry, floatItem, containingBlock);
    addFloats({ WTF::move(floatItem) }, containingBlockState);
    // One that does not fit in the fragmentainer goes to the next one, once the top of its containing block is known (placePendingFloats).
    if (containingBlockState.borderBoxLogicalTop)
        pushFloatsThatDoNotFit(Vector<CheckedRef<const Box>> { box.layoutBox() });
    if (startsInNextFragmentainer)
        return;
    m_floatBoxCount += contentResult.boxes.size() + 1;
    auto intrinsicContentBlockSize = contentResult.intrinsicContentBlockSize;
    appendFloatContentResult(WTF::move(contentResult), result);
    result.boxes.append({ box, WTF::move(geometry), intrinsicContentBlockSize, { }, { }, false, false, !laysOutContent });
}

// The content of a float in this fragmentainer joins the result, where the fragmentainer breaks like the content around the float, and column
// balancing makes room for it.
void BlockFormattingContext::appendFloatContentResult(BlockLayoutResult&& contentResult, BlockLayoutResult& result)
{
    result.boxes.appendVector(WTF::move(contentResult.boxes));
    result.outOfFlowBoxes.appendVector(WTF::move(contentResult.outOfFlowBoxes));
    result.outOfFlowContainingBlockLineGrids.appendVector(WTF::move(contentResult.outOfFlowContainingBlockLineGrids));
    result.inlineContentFragments.appendVector(WTF::move(contentResult.inlineContentFragments));
    result.fragmentedBoxes.appendVector(WTF::move(contentResult.fragmentedBoxes));
    if (contentResult.spaceShortage)
        addSpaceShortage(*contentResult.spaceShortage);
    addUnbreakableBlockSize(contentResult.unbreakableBlockSize);
}

// Its auto margins are zero, and none of its margins collapse or fill the containing block (CSS 2.2 §10.3.5, §10.6.6).
void BlockFormattingContext::computeFloatMargins(const RenderBox& box, const ContainingBlockState& containingBlockState, BoxGeometry& geometry) const
{
    CheckedRef style = box.style();
    auto writingMode = containingBlockState.blockContainer->writingMode();
    auto margin = [&](auto& length) {
        return length.isAuto() ? 0_lu : Style::evaluateMinimum<LayoutUnit>(length, containingBlockState.inlineRange.size(), style->usedZoomForLength());
    };
    geometry.setHorizontalMargin({ margin(style->marginStart(writingMode)), margin(style->marginEnd(writingMode)) });
    geometry.setVerticalMargin({ margin(style->marginBefore(writingMode)), margin(style->marginAfter(writingMode)) });
}

std::optional<BoxGeometry> BlockFormattingContext::geometryForFloatInInlineContent(const RenderBox& box) const
{
    if (!laysOutFloatContent(box))
        return { };
    auto boxState = containingBlockStateAtBlockStart(box);
    computeFloatMargins(box, m_blockLayoutState.containingBlockStates().last(), boxState.geometry);
    return boxState.geometry;
}

std::optional<bool> BlockFormattingContext::layoutContentOfFloatInInlineContent(const RenderBox& box, BoxGeometry& geometry)
{
    auto& containingBlockStates = m_blockLayoutState.containingBlockStates();
    auto& containingBlockState = containingBlockStates.last();
    ASSERT(containingBlockState.blockContainer.ptr() == box.containingBlock());
    if (!laysOutFloatContent(box))
        return layoutFloatInInlineContentThroughRenderTree(box, geometry);
    // The layout of its content gives the box the geometry of the root of that content, in the layout state the inline formatting context
    // reads this geometry from too, which keeps where the box went and its margins.
    auto placedGeometry = geometry;
    auto content = BlockBreakToken::PushedFloat::Content { containingBlockState, containingBlockStateFor(box, placedGeometry, containingBlockState), breaksInside(containingBlockStates.span().subspan(1)) };
    auto containingBlockOffsetInRoot = borderBoxOffsetInRoot(containingBlockState);
    auto borderBoxLogicalTopInRoot = borderBoxLogicalLeftAndTopInRoot(containingBlockState).height() + BoxGeometry::borderBoxTop(placedGeometry);
    auto inlineStart = containingBlockState.hasInlineDirectionOppositeToRoot ? containingBlockState.geometry.borderBoxWidth() - BoxGeometry::borderBoxLeft(placedGeometry) - placedGeometry.borderBoxWidth() : BoxGeometry::borderBoxLeft(placedGeometry);
    setLineGridOfFloatContent(content, logicalLeftAndTopInRoot(containingBlockOffsetInRoot.width() + inlineStart, placedGeometry.borderBoxWidth(), borderBoxLogicalTopInRoot));
    auto floatContentLayout = layoutFloatContent(downcast<RenderBlockFlow>(box), content, borderBoxLogicalTopInRoot);
    setBlockAxisGeometry(placedGeometry, floatContentLayout.fragmentGeometry);
    geometry = placedGeometry;
    LayoutIntegration::BlockIntegrationUtils::setShapeOutside(box, placedGeometry, containingBlockState.blockContainer->writingMode(), containingBlockState.inlineRange.size());
    auto isBreakBeforeContent = floatContentLayout.result.isBreakBeforeContent;
    m_floatContents.removeFirstMatching([&](auto& floatContent) {
        return floatContent.box.ptr() == &box;
    });
    m_floatContents.append({ box, WTF::move(content), isBreakBeforeContent });
    m_inlineContentFloatResults.removeFirstMatching([&](auto& entry) {
        return entry.first.ptr() == &box;
    });
    m_inlineContentFloatResults.append({ box, WTF::move(floatContentLayout) });
    if (goesWholeToNextFragmentainer(box, placedGeometry.marginBoxHeight()))
        return { };
    return isBreakBeforeContent;
}

// The render tree breaks the content of a table, a flex container or a grid container that floats across the fragmentainers from where the
// inline formatting context placed it, like a float in block-level content (handleFloat), where it lays it out again, and it stays there, a
// flex container included, which the render tree only moves to the next fragmentainer in block-level content (RenderBlockFlow::positionNewFloats).
std::optional<bool> BlockFormattingContext::layoutFloatInInlineContentThroughRenderTree(const RenderBox& box, BoxGeometry& geometry)
{
    if (!hasContentFragmentedByRenderTree(box, m_fragmentainer))
        return { };
    auto& containingBlockState = m_blockLayoutState.containingBlockStates().last();
    auto boxState = containingBlockStateFor(box, geometry, containingBlockState);
    auto containingBlockOffsetInRoot = borderBoxOffsetInRoot(containingBlockState);
    auto inlineStart = containingBlockState.hasInlineDirectionOppositeToRoot ? containingBlockState.geometry.borderBoxWidth() - BoxGeometry::borderBoxLeft(geometry) - geometry.borderBoxWidth() : BoxGeometry::borderBoxLeft(geometry);
    auto placedOffsetInRoot = logicalLeftAndTopInRoot(containingBlockOffsetInRoot.width() + inlineStart, geometry.borderBoxWidth(), borderBoxLogicalLeftAndTopInRoot(containingBlockState).height() + BoxGeometry::borderBoxTop(geometry));
    // Its layout gives it the geometry of its renderer, which is not where the inline formatting context placed it.
    auto placedGeometry = geometry;
    layoutIndependentFormattingContextThroughRenderTree(box, containingBlockState, boxState, { }, { }, placedOffsetInRoot);
    placedGeometry.setContentBoxHeight(boxState.geometry.contentBoxHeight());
    geometry = placedGeometry;
    auto lineGrid = lineGridForBoxLaidOutThroughRenderTree(box, containingBlockState, placedOffsetInRoot);
    m_floatsInFragmentainer.append({ box.layoutBox(), containingBlockForFloats(containingBlockState), BlockBreakToken::PushedFloat::RenderTreeLayout { containingBlockState, WTF::move(boxState), WTF::move(lineGrid) } });
    return false;
}

// The content of a float the block formatting context lays out snaps to the line grid around it from where the float goes, where it stays
// when the float goes to the next fragmentainer (RenderBlockFlow::positionNewFloats places a float before its layout).
void BlockFormattingContext::setLineGridOfFloatContent(BlockBreakToken::PushedFloat::Content& content, LayoutSize borderBoxOffsetInRoot)
{
    content.lineGrid = rootLineGridForNestedRoot(content.floatState, borderBoxOffsetInRoot);
    content.lineGridBorderBoxStripLogicalTop = m_fragmentainer->stripLogicalTop + m_fragmentainer->rootBlockOffset + borderBoxOffsetInRoot.height();
}

// In fragmented content, the block formatting context lays out the content of a float that can be split, which breaks as a flow of its own
// (css-break-3 §2.1), where the render tree lays it out as monolithic content (BlockIntegrationUtils::layoutIndependentFormattingContext).
bool BlockFormattingContext::laysOutFloatContent(const RenderBox& box) const
{
    if (!m_fragmentainer || box.isUnsplittableForPagination() || !integrationUtils().fragmentedContentLayout())
        return false;
    CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(box);
    return blockContainer && LayoutIntegration::canUseForFloatContentLayout(*blockContainer);
}

// The content of a float is a block formatting context of its own (CSS 2.2 §9.4.1), at the inline size that fits it (§10.3.5), and the float
// gets its block size from it (§10.6.7). Unless the float is in a box that cannot be split, it breaks like a block container in the fragment
// of its containing block, one fragmentainer at a time from where the float goes, without the forced breaks of the boxes in the normal flow
// (css-break-3 §2.1, §3.1, like RenderBlockFlow::applyBeforeBreak): in this one up to where the content of its containing block ends, above
// the cloned block-end border and padding of the block containers around it (§5.4), and its own when it clones them, from where it
// continues from the one before (contentBreakToken, none when only its block size of its own is left).
auto BlockFormattingContext::layoutFloatContent(const RenderBlockFlow& box, BlockBreakToken::PushedFloat::Content& content, LayoutUnit borderBoxLogicalTopInRoot, const BlockBreakToken::ContinuingFloat* continuingFloat, bool mayStartInNextFragmentainer) -> FloatContentLayout
{
    auto* contentBreakToken = continuingFloat ? continuingFloat->contentBreakToken.get() : nullptr;
    auto hasContentLeft = !continuingFloat || continuingFloat->hasContentLeft;
    auto contentBlockSizeLeft = continuingFloat ? continuingFloat->contentBlockSizeLeft : 0_lu;
    auto& floatState = content.floatState;
    LayoutIntegration::BlockIntegrationUtils::markPercentageHeightDescendantsForLayout(box, floatState.contentBoxBlockSize);
    auto isContinuation = !!floatState.consumedBlockSize;
    // With a block layout state of its own and its own text-box-trim only, like an out-of-flow box (layoutOutOfFlowBox).
    auto blockLayoutState = BlockLayoutState { { } };
    blockLayoutState.setLastFormattedLineRoots(Vector { m_blockLayoutState.lastFormattedLineRoots() });
    if (box.style().textBoxTrim() != TextBoxTrim::None)
        blockLayoutState.setTextBoxTrimState(textBoxTrimStateFor(box, { }));
    auto horizontalConstraints = HorizontalConstraints { floatState.inlineRange.min(), floatState.inlineRange.size() };
    auto initialBlockLayoutState = blockLayoutState;
    auto borderBoxStripLogicalTop = m_fragmentainer->stripLogicalTop + m_fragmentainer->rootBlockOffset + borderBoxLogicalTopInRoot;
    // The line grid stays where it is when the float goes to the next fragmentainer, and its rows start again at the top of the fragment where
    // the float continues, below the block-start border and padding of the box that establishes it, but not one around the fragmented flow.
    auto lineGrid = content.lineGrid;
    if (auto offset = borderBoxStripLogicalTop - content.lineGridBorderBoxStripLogicalTop) {
        if (lineGrid.lineGrid && lineGrid.lineGrid->outsideRoot) {
            if (!isContinuation)
                lineGrid.lineGrid->outsideRoot->borderBoxOffset.shrink(0_lu, offset);
            else if (!isAroundFragmentedFlow(lineGrid.lineGrid->blockContainer, box))
                lineGrid.lineGrid->outsideRoot->borderBoxOffset.setHeight(0_lu);
        }
        lineGrid.borderBoxOffsetInLayoutState.expand(0_lu, offset);
    }
    auto constraintsForContent = [&](LayoutUnit shift) {
        return ConstraintsForBlockContent { { horizontalConstraints, floatState.borderAndPaddingBefore + shift }, floatState.contentBoxBlockSize, floatState.fillAvailableBlockSize, floatState.replacedElementAvailableBlockSize, floatState.relayoutChildren };
    };
    // One that avoids breaks inside goes to the next fragmentainer whole when it fits in one (goesWholeToNextFragmentainer), and breaks only
    // when it is taller (css-break-3 §4.4).
    auto breaksInside = content.breaks;
    if (breaksInside && m_fragmentainer->blockSize && !isContinuation && isUnsplittableForPagination(box, m_fragmentainer->breakType())) {
        auto measuringBlockLayoutState = initialBlockLayoutState;
        auto wholeGeometry = floatState.geometry;
        auto wholeResult = BlockFormattingContext { box, integrationUtils(), measuringBlockLayoutState }.layout(constraintsForContent({ }), wholeGeometry, lineGrid);
        computeBlockSize(box, wholeResult.intrinsicContentBlockSize, content.containingBlockState, wholeGeometry);
        breaksInside = !goesWholeToNextFragmentainer(box, wholeGeometry.marginBoxHeight());
    }
    // In content past the end of the fragmentainer, below block-start border and padding of a box around it that crossed the end, which the
    // strip of fragmentainers breaks, its content breaks across the fragmentainers from where it is in the strip, like that content.
    auto& containingBlockState = m_blockLayoutState.containingBlockStates().last();
    auto isPastFragmentainerEnd = breaksInside && m_fragmentainer->blockSize && !isContinuation && borderBoxOffsetInRoot(containingBlockState).height() + containingBlockState.borderAndPaddingBefore >= m_fragmentainer->availableBlockSize();
    auto breaks = breaksInside && m_fragmentainer->blockSize && !isPastFragmentainerEnd;
    auto fragmentEnd = fragmentainerEndForContent();
    auto clonedBlockEnd = breaks && clonesDecorations(floatState) ? floatState.geometry.borderAndPaddingAfter() : 0_lu;
    auto layoutContent = [&](LayoutUnit shift) {
        auto constraints = constraintsForContent(shift);
        if (!breaksInside)
            return BlockFormattingContext { box, integrationUtils(), blockLayoutState }.layout(constraints, floatState.geometry, lineGrid);
        if (isPastFragmentainerEnd)
            return (*integrationUtils().fragmentedContentLayout())(box, blockLayoutState, constraints, floatState.geometry, lineGrid, borderBoxStripLogicalTop, false);
        auto rootBlockOffset = m_fragmentainer->rootBlockOffset + borderBoxLogicalTopInRoot;
        auto blockSize = m_fragmentainer->blockSize ? std::optional { m_fragmentainer->rootBlockOffset + fragmentEnd - clonedBlockEnd } : std::nullopt;
        auto fragmentainer = FragmentainerConstraints { blockSize, m_fragmentainer->type, rootBlockOffset, !isContinuation && rootBlockOffset > 0, false, false, m_fragmentainer->stripLogicalTop };
        fragmentainer.breaksLikePages = m_fragmentainer->breaksLikePages;
        return BlockFormattingContext { box, integrationUtils(), blockLayoutState }.layout(constraints, floatState.geometry, lineGrid, fragmentainer, contentBreakToken);
    };
    // One that goes past the end of the fragmentainer starts in the next one, where its content goes, like in the strip of fragmentainers,
    // unless nothing is above it in this one, which the content of each fragmentainer makes progress through (css-break-3 §4.4).
    auto startsPastFragmentainerEnd = breaks && !isContinuation && mayStartInNextFragmentainer && borderBoxLogicalTopInRoot >= fragmentEnd;
    auto result = hasContentLeft && !startsPastFragmentainerEnd ? layoutContent({ }) : BlockLayoutResult { };
    if (!hasContentLeft)
        result.intrinsicContentBlockSize = contentBlockSizeLeft;
    computeBlockSize(box, result.intrinsicContentBlockSize, content.containingBlockState, floatState.geometry);
    // align-content moves the content as a unit in the block size the float gets from that of its content, where it is laid out again, but
    // not in one at least as tall as a fragmentainer, like the render tree does (RenderBlockFlow::layoutBlock), nor in a fragment after the first.
    auto contentShift = 0_lu;
    if (hasContentLeft && !isContinuation && !box.style().alignContent().isNormal() && floatState.geometry.borderBoxHeight() < m_fragmentainer->wholeBlockSize()) {
        if (auto shift = BlockFormattingUtils::alignContentShift(box, floatState.geometry.contentBoxHeight() - result.intrinsicContentBlockSize)) {
            blockLayoutState = initialBlockLayoutState;
            contentShift = shift;
            result = layoutContent(shift);
        }
    }
    auto fragmentGeometry = floatState.geometry;
    computeBlockSizeAfterBreaks(box, floatState, result.intrinsicContentBlockSize, content.containingBlockState, fragmentGeometry);
    auto floatContentLayout = FloatContentLayout { { }, fragmentGeometry, { }, { }, false, { }, { }, { } };
    if (startsPastFragmentainerEnd)
        floatContentLayout.offsetPastFragmentainerEnd = borderBoxLogicalTopInRoot - fragmentEnd;
    // It continues in the next fragmentainer when its content does, or its block size of its own crosses the end of this one, unless none of
    // its content fits here, where it goes to the next one whole, or this one has no room for a fragment after its first, which then takes
    // all of what its block size leaves. Its fragment fills the rest of this one, but no more than its block size of its own leaves.
    auto fragmentBlockSize = std::max(0_lu, fragmentEnd - borderBoxLogicalTopInRoot);
    auto crossesFragmentainerEnd = borderBoxLogicalTopInRoot + fragmentGeometry.borderBoxHeight() > fragmentEnd && (fragmentBlockSize || !isContinuation);
    if (breaks && !result.isBreakBeforeContent && (startsPastFragmentainerEnd || result.breakToken || crossesFragmentainerEnd)) {
        // The content beside it and after it avoids and clears all of it, which goes on past the end of the fragmentainer, where its margin box
        // ends in the strip of fragmentainers, which its content laid out across them from where the float goes tells, like the render tree
        // has it, and which this fragment's margin box ends at then. Its content is laid out in this fragmentainer again after that, which the
        // renderers of its content keep.
        auto marginBoxStripLogicalBottom = continuingFloat ? continuingFloat->marginBoxStripLogicalBottom : 0_lu;
        if (!continuingFloat) {
            auto wholeBlockLayoutState = initialBlockLayoutState;
            auto wholeGeometry = floatState.geometry;
            auto wholeResult = (*integrationUtils().fragmentedContentLayout())(box, wholeBlockLayoutState, constraintsForContent(contentShift), wholeGeometry, lineGrid, borderBoxStripLogicalTop, false);
            computeBlockSize(box, wholeResult.intrinsicContentBlockSize, content.containingBlockState, wholeGeometry);
            marginBoxStripLogicalBottom = borderBoxStripLogicalTop + wholeGeometry.borderBoxHeight() + wholeGeometry.marginAfter();
            if (hasContentLeft && !startsPastFragmentainerEnd) {
                blockLayoutState = initialBlockLayoutState;
                result = layoutContent(contentShift);
            }
        }
        if (auto blockSizeLeft = blockSizeLeftOfItsOwn(floatState, content.containingBlockState))
            fragmentBlockSize = std::min(fragmentBlockSize, *blockSizeLeft);
        floatContentLayout.fragmentGeometry = floatState.geometry;
        sliceAtFragmentainerEnd(floatContentLayout.fragmentGeometry, fragmentBlockSize, !!clonedBlockEnd);
        floatContentLayout.consumedBlockSize = consumedBlockSizeThrough(floatState, fragmentBlockSize, clonedBlockEnd);
        if (auto contentBreakToken = std::exchange(result.breakToken, std::nullopt))
            floatContentLayout.contentBreakToken = makeUnique<BlockBreakToken>(WTF::move(*contentBreakToken));
        floatContentLayout.hasContentLeft = hasContentLeft && (startsPastFragmentainerEnd || floatContentLayout.contentBreakToken);
        if (!floatContentLayout.hasContentLeft) {
            auto contentBlockSize = floatState.consumedBlockSize.value_or(ContainingBlockState::ConsumedBlockSize { }).contentBox + result.intrinsicContentBlockSize;
            floatContentLayout.contentBlockSizeLeft = std::max(0_lu, contentBlockSize - floatContentLayout.consumedBlockSize->contentBox);
        }
        floatContentLayout.marginBoxStripLogicalBottom = marginBoxStripLogicalBottom;
        auto& fragment = floatContentLayout.fragmentGeometry;
        auto fragmentMarginBoxStripLogicalBottom = borderBoxStripLogicalTop + fragment.borderBoxHeight() + fragment.marginAfter();
        if (floatContentLayout.marginBoxStripLogicalBottom > fragmentMarginBoxStripLogicalBottom)
            fragment.setVerticalMargin({ fragment.marginBefore(), floatContentLayout.marginBoxStripLogicalBottom - borderBoxStripLogicalTop - fragment.borderBoxHeight() });
        else
            floatContentLayout.marginBoxStripLogicalBottom = fragmentMarginBoxStripLogicalBottom;
    }
    floatContentLayout.result = WTF::move(result);
    return floatContentLayout;
}

// A float that continues from the fragmentainer before goes on at its start, before the content there (css-break-3 §2.1), in the fragments of
// the block containers around it that continue here, below the block-start border and padding of those that clone theirs (§5.4), at the start
// of the content box of the innermost one, where its inline position stays, and its content goes up to where the content of that one ends.
// The block containers around a float from the fragmentainer before that continue in this one, which it goes in, resumed for it only.
void BlockFormattingContext::withOpenBlockContainers(const Vector<BlockBreakToken::OpenBlockContainer>& openBlockContainers, NOESCAPE const Function<void()>& function)
{
    auto& containingBlockStates = m_blockLayoutState.containingBlockStates();
    auto stateCount = containingBlockStates.size();
    auto marginCollapse = m_blockLayoutState.marginCollapse();
    auto textBoxTrimState = m_blockLayoutState.textBoxTrimState();
    auto blockStepExtraSpaceToResume = m_blockStepExtraSpaceToResume;
    auto breakToken = BlockBreakToken { 0, { }, { }, false, { }, false };
    breakToken.openBlockContainers = openBlockContainers;
    resumeAfterBreak(breakToken);
    function();
    containingBlockStates.shrink(stateCount);
    m_blockLayoutState.marginCollapse() = WTF::move(marginCollapse);
    m_blockLayoutState.setTextBoxTrimState(WTF::move(textBoxTrimState));
    m_blockStepExtraSpaceToResume = blockStepExtraSpaceToResume;
}

// The containing block of a float from the fragmentainer before, here or at the start of the fragmentainer when it has no fragment here.
FloatingContext::ContainingBlock BlockFormattingContext::containingBlockForFloatFromFragmentainerBefore(const ContainingBlockState& containingBlockStateBefore, const FloatingContext::ContainingBlock& containingBlockBefore)
{
    for (auto& state : m_blockLayoutState.containingBlockStates()) {
        if (state.blockContainer.ptr() == containingBlockStateBefore.blockContainer.ptr())
            return containingBlockForFloats(state);
    }
    return { { containingBlockBefore.borderBoxTopLeft.x(), 0_lu }, containingBlockBefore.contentBox };
}

void BlockFormattingContext::layoutContinuingFloat(const BlockBreakToken::ContinuingFloat& continuingFloat, BlockLayoutResult& result)
{
    CheckedRef box = continuingFloat.box;
    auto content = continuingFloat.content;
    auto& floatItemBefore = continuingFloat.floatItem;
    auto& placedFloats = m_blockLayoutState.ensurePlacedFloats(root().layoutBox());
    auto floatingContext = floatingContextForSidesOf(box, placedFloats);
    auto geometryInRoot = floatItemBefore.boxGeometry();
    geometryInRoot.setHorizontalMargin(floatItemBefore.inlineAxisMargin());
    geometryInRoot.setVerticalMargin({ { }, geometryInRoot.marginAfter() });
    auto containingBlock = FloatingContext::ContainingBlock { };
    auto floatContentLayout = FloatContentLayout { };
    withOpenBlockContainers(continuingFloat.openBlockContainers, [&] {
        containingBlock = containingBlockForFloatFromFragmentainerBefore(content.containingBlockState, continuingFloat.containingBlock);
        auto contentLogicalTop = m_blockLayoutState.containingBlockStates().last().contentBoxLogicalTop();
        geometryInRoot.setTopLeft({ floatItemBefore.absoluteBorderBoxRect().left() - containingBlock.borderBoxTopLeft.x(), contentLogicalTop - containingBlock.borderBoxTopLeft.y() });
        // One that starts here goes as far below the start as it was past the end of the fragmentainer before, which the rest of the block-start
        // border and padding that crossed it may take already, beside or below the floats here, without its block-start margin, which the break
        // truncates (css-break-3 §5.2), and it may start past the end of this one too when floats are above it.
        auto mayStartInNextFragmentainer = false;
        if (auto offset = continuingFloat.offsetPastFragmentainerEnd) {
            geometryInRoot.setTop(std::max(contentLogicalTop, *offset) - containingBlock.borderBoxTopLeft.y());
            geometryInRoot.setTopLeft(floatingContext.positionForFloat(box->layoutBox(), geometryInRoot, containingBlock));
            auto& floatGeometry = content.floatState.geometry;
            floatGeometry.setVerticalMargin({ { }, floatGeometry.marginAfter() });
            content.floatState.consumedBlockSize = std::nullopt;
            mayStartInNextFragmentainer = BoxGeometry::borderBoxTop(geometryInRoot) > contentLogicalTop - containingBlock.borderBoxTopLeft.y();
        } else
            continueFromFragmentainerBefore(content.floatState, *content.floatState.consumedBlockSize);
        auto borderBoxLogicalTopInRoot = containingBlock.borderBoxTopLeft.y() + BoxGeometry::borderBoxTop(geometryInRoot);
        floatContentLayout = layoutFloatContent(downcast<RenderBlockFlow>(box), content, borderBoxLogicalTopInRoot, &continuingFloat, mayStartInNextFragmentainer);
    });
    setBlockAxisGeometry(geometryInRoot, floatContentLayout.fragmentGeometry);
    auto floatItem = floatingContext.makeFloatItem(box->layoutBox(), geometryInRoot, containingBlock);
    // Relative to its containing block, in its inline direction, or to the root when that has no fragment here.
    auto geometry = continuingFloat.geometry;
    setBlockAxisGeometry(geometry, floatContentLayout.fragmentGeometry);
    auto borderBoxLeft = BoxGeometry::borderBoxLeft(geometryInRoot);
    auto& containingBlockState = content.containingBlockState;
    geometry.setTopLeft({ containingBlockState.hasInlineDirectionOppositeToRoot ? containingBlockState.geometry.borderBoxWidth() - borderBoxLeft - geometry.borderBoxWidth() : borderBoxLeft, BoxGeometry::borderBoxTop(geometryInRoot) });
    auto contentResult = std::exchange(floatContentLayout.result, { });
    auto startsInNextFragmentainer = !!floatContentLayout.offsetPastFragmentainerEnd;
    continueFloat(box, WTF::move(content), floatContentLayout, geometry, floatItem, containingBlock);
    placedFloats.add(WTF::move(floatItem));
    if (startsInNextFragmentainer)
        return;
    auto intrinsicContentBlockSize = contentResult.intrinsicContentBlockSize;
    appendFloatContentResult(WTF::move(contentResult), result);
    result.boxes.append({ box, WTF::move(geometry), intrinsicContentBlockSize, { }, { }, false, false, false });
}

bool BlockFormattingContext::hasFloatsContinuingInNextFragmentainer() const
{
    return !m_continuingFloats.isEmpty() || m_inlineContentFloatResults.containsIf([](auto& entry) {
        return entry.second.consumedBlockSize && !entry.second.offsetPastFragmentainerEnd;
    });
}

// A float whose content or block size of its own continues in the next fragmentainer goes on there (layoutContinuingFloat), unless it goes
// there whole (pushFloatsThatDoNotFit).
void BlockFormattingContext::continueFloat(const RenderBox& box, BlockBreakToken::PushedFloat::Content&& content, FloatContentLayout& floatContentLayout, const BoxGeometry& geometry, const PlacedFloats::Item& floatItem, const FloatingContext::ContainingBlock& containingBlock)
{
    m_continuingFloats.removeFirstMatching([&](auto& continuingFloat) {
        return continuingFloat.box.ptr() == &box;
    });
    if (!floatContentLayout.consumedBlockSize)
        return;
    content.floatState.consumedBlockSize = floatContentLayout.consumedBlockSize;
    m_continuingFloats.append({ box, { }, WTF::move(content), geometry, floatItem, containingBlock, std::exchange(floatContentLayout.contentBreakToken, nullptr), floatContentLayout.hasContentLeft, floatContentLayout.contentBlockSizeLeft, floatContentLayout.marginBoxStripLogicalBottom, floatContentLayout.offsetPastFragmentainerEnd });
}

const BlockFormattingContext::FloatContent* BlockFormattingContext::floatContent(const Box& floatBox) const
{
    auto index = m_floatContents.findIf([&](auto& floatContent) {
        return &floatContent.box->layoutBox() == &floatBox;
    });
    return index == notFound ? nullptr : &m_floatContents[index];
}

void BlockFormattingContext::handleOutOfFlowBox(const RenderBox& box, BlockLayoutResult& result)
{
    auto& parentState = m_blockLayoutState.containingBlockStates().last();
    ASSERT(parentState.blockContainer.ptr() == box.parent());
    CheckedRef parentStyle = parentState.blockContainer->style();
    auto isLeftToRight = parentStyle->writingMode().isBidiLTR();
    // Boxes in the top layer use the initial containing block as their static position rectangle (css-position-4 §2.1).
    if (RefPtr element = box.element(); element && element->isInTopLayer()) [[unlikely]] {
        result.outOfFlowBoxes.append({ box, { }, { } });
        return;
    }

    // Like RenderBlockFlow::adjustOutOfFlowBlock: where the block-start margin edge of the box would be with position: static. While the
    // parent still waits for its top, that is its content box top, which its block-start margins collapse with.
    auto staticBlockPosition = parentState.borderBoxLogicalTop ? m_blockLayoutState.marginCollapse().collapsedLogicalBottom() - *parentState.borderBoxLogicalTop : parentState.borderAndPaddingBefore;

    // A box that was inline-level before it got blockified goes where text-align puts the start of a line, beside the floats at its static
    // position (RenderBlockFlow::staticInlinePositionForOriginalDisplayInline).
    auto lineRange = [&] {
        auto& inlineRange = parentState.inlineRange;
        auto* placedFloats = m_blockLayoutState.placedFloats();
        auto lineStart = inlineRange.min();
        auto lineEnd = inlineRange.max();
        if (placedFloats && !placedFloats->isEmpty()) {
            auto& rootState = m_blockLayoutState.containingBlockStates().first();
            auto hypotheticalPosition = this->hypotheticalPosition();
            auto floats = floatsAtHypotheticalPosition(hypotheticalPosition);
            auto staticBlockPositionInRoot = hypotheticalPosition.borderBoxLogicalTop;
            auto floatingContext = FloatingContext { root().layoutBox(), floats, { }, rootState.geometry.borderBoxWidth() };
            std::tie(lineStart, lineEnd) = spaceBesideFloats(floatingContext, parentState, staticBlockPositionInRoot, staticBlockPositionInRoot);
        }
        std::tie(lineStart, lineEnd) = spaceOnLineGridColumns(parentState, lineStart, lineEnd);
        return LayoutRange { lineStart, lineEnd - lineStart };
    };
    auto staticInlinePosition = [&] {
        if (!box.style().originalDisplay().isInlineType())
            return parentState.inlineRange.min();
        auto inlineRange = lineRange();
        switch (parentStyle->textAlign()) {
        case Style::TextAlign::Left:
        case Style::TextAlign::WebKitLeft:
            return isLeftToRight ? inlineRange.min() : inlineRange.max();
        case Style::TextAlign::Right:
        case Style::TextAlign::WebKitRight:
            return isLeftToRight ? inlineRange.max() : inlineRange.min();
        case Style::TextAlign::Center:
        case Style::TextAlign::WebKitCenter:
            return inlineRange.min() + inlineRange.size() / 2;
        case Style::TextAlign::End:
            return inlineRange.max();
        case Style::TextAlign::Justify:
        case Style::TextAlign::Start:
            break;
        }
        return inlineRange.min();
    };
    result.outOfFlowBoxes.append({ box, staticInlinePosition(), staticBlockPosition });
}

// The result is in the logical coordinates of the containing block, whose inline axis is the box's block axis in an orthogonal flow. Border and
// padding keep the box's own sides, and the scrollbars their physical axes.
static void transposeToContainingBlockAxes(BoxGeometry& geometry, OutOfFlowGeometry::AxisGeometry& inlineAxis, OutOfFlowGeometry::AxisGeometry& blockAxis)
{
    std::swap(inlineAxis, blockAxis);
    geometry.setBorder({ { geometry.borderBefore(), geometry.borderAfter() }, { geometry.borderStart(), geometry.borderEnd() } });
    geometry.setPadding({ { geometry.paddingBefore(), geometry.paddingAfter() }, { geometry.paddingStart(), geometry.paddingEnd() } });
    geometry.setContentBoxSize({ geometry.contentBoxHeight(), geometry.contentBoxWidth() });
    geometry.setSpaceForScrollbar({ geometry.verticalSpaceForScrollbar(), geometry.horizontalSpaceForScrollbar() });
}

// A replaced element, a table, a flex container, a WebVTT cue box, a form control or a block container whose content the block formatting
// context does not lay out as a nested root, whose own formatting context lays it out through the render tree; the block formatting context
// places it, and sizes it when it is neither replaced nor a table (css-position-3 §4.3), from its content when its block size is auto.
BlockLayoutResult BlockFormattingContext::layoutOutOfFlowIndependentFormattingContext(const RenderBox& root, const OutOfFlowContainingBlock& containingBlock, const OutOfFlowGeometry::StaticPosition& staticPosition, const std::optional<OutOfFlowGeometry::DefaultAnchor>& defaultAnchor, const std::optional<LineGridInLayoutState>& containingBlockLineGrid)
{
    auto geometry = computeBorderAndPadding(root, containingBlock.inlineSize);
    auto outOfFlowGeometry = OutOfFlowGeometry { root, { containingBlock.inlineSize, containingBlock.blockSize, containingBlock.writingMode, containingBlock.isScrollContainer, containingBlock.scrollableContainingBlock, containingBlock.fillAvailableBlockSize, containingBlock.fixedPositionOverflowArea }, staticPosition, defaultAnchor, geometry.horizontalBorderAndPadding(), geometry.verticalBorderAndPadding() };
    // In content broken into fragmentainers, the render tree breaks the content of the box from where it goes, unless it cannot be split or is a
    // multi-column container, which the render tree does not break across pages, and lays it out again where it goes when its estimate of
    // where that is was wrong, like for a box whose block-end edge places it (RenderBlock::layoutOutOfFlowBox).
    CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(root);
    auto breaksContent = containingBlock.borderBoxStripLogicalTop && !root.isUnsplittableForPagination() && !root.isRenderVTTCue() && !(blockFlow && blockFlow->multiColumnFlow());
    auto borderBoxStripLogicalTop = [&](LayoutUnit borderBoxBlockStart) -> std::optional<LayoutUnit> {
        if (!breaksContent)
            return { };
        return *containingBlock.borderBoxStripLogicalTop + containingBlock.blockStart + borderBoxBlockStart;
    };
    auto [inlineAxis, blockAxis] = [&] -> std::pair<OutOfFlowGeometry::AxisGeometry, OutOfFlowGeometry::AxisGeometry> {
        // A replaced element from its natural size and aspect ratio (CSS 2.2 §10.3.8, §10.6.5), and a table from its columns, which do not
        // stretch it (css-position-3 §4.3, RenderTable::updateLogicalWidth).
        if (root.isRenderReplaced() || root.isRenderOrLegacyRenderSVGRoot() || is<RenderTable>(root)) {
            // The content of a table snaps to the line grid around it from where it would go with no size (RenderBlock::layoutOutOfFlowBox).
            auto estimatedBorderBoxBlockStart = outOfFlowGeometry.computeReplacedBlockAxisGeometry({ }).borderBoxStart;
            auto lineGrid = lineGridForOutOfFlowBox(root, containingBlock, containingBlockLineGrid, containingBlock.inlineStart + outOfFlowGeometry.computeReplacedInlineAxisGeometry({ }).borderBoxStart, containingBlock.blockStart + estimatedBorderBoxBlockStart, { });
            auto borderBoxSize = integrationUtils().layoutOutOfFlowIndependentFormattingContext(root, { }, { }, { }, lineGrid, borderBoxStripLogicalTop(estimatedBorderBoxBlockStart));
            auto blockAxis = outOfFlowGeometry.computeReplacedBlockAxisGeometry(borderBoxSize.height());
            if (breaksContent && blockAxis.borderBoxStart != estimatedBorderBoxBlockStart) {
                borderBoxSize = integrationUtils().layoutOutOfFlowIndependentFormattingContext(root, { }, { }, { }, lineGrid, borderBoxStripLogicalTop(blockAxis.borderBoxStart));
                blockAxis = outOfFlowGeometry.computeReplacedBlockAxisGeometry(borderBoxSize.height());
            }
            return { outOfFlowGeometry.computeReplacedInlineAxisGeometry(borderBoxSize.width()), blockAxis };
        }
        auto contentInlineSizes = std::optional<IntrinsicWidthConstraints> { };
        if (outOfFlowGeometry.needsContentInlineSizes())
            contentInlineSizes = LayoutIntegration::BlockIntegrationUtils::independentFormattingContextIntrinsicInlineSizes(root, geometry.horizontalBorderAndPadding());
        auto inlineAxis = outOfFlowGeometry.computeInlineAxisGeometry(contentInlineSizes);
        auto borderBoxBlockSize = outOfFlowGeometry.definiteContentBoxBlockSize().transform([&](auto contentBoxBlockSize) {
            return contentBoxBlockSize + geometry.verticalBorderAndPadding();
        });
        // A WebVTT cue box moves itself once laid out (RenderVTTCue::layout: the cue box positioning of the WebVTT rendering rules), from the
        // inline position the block formatting context gives it and the block position its own layout gives it.
        if (root.isRenderVTTCue()) {
            auto borderBoxSize = integrationUtils().layoutOutOfFlowIndependentFormattingContext(root, inlineAxis.borderBoxSize, { }, containingBlock.inlineStart + inlineAxis.borderBoxStart);
            return { inlineAxis, outOfFlowGeometry.computeBlockAxisGeometry(borderBoxSize.height() - geometry.verticalBorderAndPadding(), inlineAxis.borderBoxSize) };
        }
        // Its content snaps to the line grid around it from where it goes before it is laid out (RenderBlock::layoutOutOfFlowBox).
        auto borderBoxBlockStart = outOfFlowGeometry.computeBlockAxisGeometry(borderBoxBlockSize.value_or(geometry.verticalBorderAndPadding()) - geometry.verticalBorderAndPadding(), inlineAxis.borderBoxSize).borderBoxStart;
        auto lineGrid = lineGridForOutOfFlowBox(root, containingBlock, containingBlockLineGrid, containingBlock.inlineStart + inlineAxis.borderBoxStart, containingBlock.blockStart + borderBoxBlockStart, inlineAxis.borderBoxSize);
        auto borderBoxSize = integrationUtils().layoutOutOfFlowIndependentFormattingContext(root, inlineAxis.borderBoxSize, borderBoxBlockSize, { }, lineGrid, borderBoxStripLogicalTop(borderBoxBlockStart));
        auto blockAxis = outOfFlowGeometry.computeBlockAxisGeometry(borderBoxSize.height() - geometry.verticalBorderAndPadding(), inlineAxis.borderBoxSize);
        if (breaksContent && blockAxis.borderBoxStart != borderBoxBlockStart) {
            borderBoxSize = integrationUtils().layoutOutOfFlowIndependentFormattingContext(root, inlineAxis.borderBoxSize, borderBoxBlockSize, { }, lineGrid, borderBoxStripLogicalTop(blockAxis.borderBoxStart));
            blockAxis = outOfFlowGeometry.computeBlockAxisGeometry(borderBoxSize.height() - geometry.verticalBorderAndPadding(), inlineAxis.borderBoxSize);
        }
        return { inlineAxis, blockAxis };
    }();
    setContentBoxInlineSize(geometry, inlineAxis.borderBoxSize);
    setContentBoxBlockSize(geometry, blockAxis.borderBoxSize);
    // In the logical coordinates of the containing block, like layoutOutOfFlowBox.
    if (outOfFlowGeometry.isOrthogonal())
        transposeToContainingBlockAxes(geometry, inlineAxis, blockAxis);
    geometry.setHorizontalMargin({ inlineAxis.marginStart, inlineAxis.marginEnd });
    geometry.setLeft(containingBlock.inlineStart + inlineAxis.borderBoxStart);
    geometry.setVerticalMargin({ blockAxis.marginStart, blockAxis.marginEnd });
    geometry.setTop(containingBlock.blockStart + blockAxis.borderBoxStart);
    // And it stays where its own layout leaves it.
    if (root.isRenderVTTCue()) {
        auto location = LayoutIntegration::BlockIntegrationUtils::borderBoxLogicalLocation(root);
        geometry.setLeft(location.x());
        geometry.setTop(location.y());
    }

    auto margins = [](LayoutUnit margin) {
        auto adjoiningMargins = AdjoiningMargins { };
        adjoiningMargins.add(margin);
        return adjoiningMargins;
    };
    auto result = BlockLayoutResult { };
    result.boxes.append({ root, geometry, geometry.contentBoxHeight(), margins(geometry.marginBefore()), margins(geometry.marginAfter()), false, false, true });
    return result;
}

BlockLayoutResult BlockFormattingContext::layoutOutOfFlowBox(const RenderBox& outOfFlowBox, const OutOfFlowContainingBlock& containingBlock, const OutOfFlowGeometry::StaticPosition& staticPosition, const std::optional<OutOfFlowGeometry::DefaultAnchor>& defaultAnchor, const std::optional<LineGridInLayoutState>& containingBlockLineGrid, const FragmentedOutOfFlowContentLayout* fragmentedContentLayout)
{
    if (!is<RenderBlockFlow>(outOfFlowBox) || outOfFlowBox.isRenderVTTCue() || LayoutIntegration::BlockIntegrationUtils::laysOutThroughRenderTree(outOfFlowBox))
        return layoutOutOfFlowIndependentFormattingContext(outOfFlowBox, containingBlock, staticPosition, defaultAnchor, containingBlockLineGrid);
    auto result = BlockLayoutResult { };
    CheckedRef box = downcast<RenderBlockFlow>(outOfFlowBox);
    // In the box's own writing mode, which its content is laid out in.
    auto geometry = computeBorderAndPadding(box, containingBlock.inlineSize);
    auto outOfFlowGeometry = OutOfFlowGeometry { box, { containingBlock.inlineSize, containingBlock.blockSize, containingBlock.writingMode, containingBlock.isScrollContainer, containingBlock.scrollableContainingBlock, containingBlock.fillAvailableBlockSize, containingBlock.fixedPositionOverflowArea }, staticPosition, defaultAnchor, geometry.horizontalBorderAndPadding(), geometry.verticalBorderAndPadding() };

    // The box establishes an independent formatting context for its content, a block formatting context with a block layout state of its own,
    // in the box's writing mode, with the last formatted lines of the boxes with trim-end in it.
    auto blockLayoutState = BlockLayoutState { { } };
    blockLayoutState.setLastFormattedLineRoots(Vector { m_blockLayoutState.lastFormattedLineRoots() });
    auto contentFormattingContext = BlockFormattingContext { box, integrationUtils(), blockLayoutState };
    // Its own text-box-trim only: its lines are not the first or last formatted line of the boxes it is in, which only look into their in-flow
    // children (css-pseudo-4 §2.1.1). The render tree path gives it a start trim an ancestor has not used yet (TextBoxTrimmer).
    if (box->style().textBoxTrim() != TextBoxTrim::None)
        blockLayoutState.setTextBoxTrimState(contentFormattingContext.textBoxTrimStateFor(box, { }));
    // The percentage block sizes in its content resolve against the block size its insets or its own block size give it, when that does not
    // depend on the inline size being measured.
    auto contentInlineSizes = std::optional<IntrinsicWidthConstraints> { };
    if (outOfFlowGeometry.needsContentInlineSizes()) {
        auto measuredContentBoxBlockSize = outOfFlowGeometry.definiteContentBoxBlockSize().transform([&](auto contentBoxBlockSize) {
            return std::max(0_lu, contentBoxBlockSize - geometry.verticalSpaceForScrollbar());
        });
        contentInlineSizes = contentFormattingContext.computeIntrinsicInlineSizes(box, measuredContentBoxBlockSize);
    }
    // Its scrollbars take part in the sizes of its content (RenderBlock::computeIntrinsicLogicalWidths).
    if (contentInlineSizes) {
        auto scrollbarInlineSize = LayoutUnit { box->intrinsicScrollbarLogicalWidthIncludingGutter() };
        contentInlineSizes = IntrinsicWidthConstraints { contentInlineSizes->minimum + scrollbarInlineSize, contentInlineSizes->maximum + scrollbarInlineSize };
    }
    auto inlineAxis = outOfFlowGeometry.computeInlineAxisGeometry(contentInlineSizes);
    setContentBoxInlineSize(geometry, inlineAxis.borderBoxSize);
    auto contentBoxBlockSize = outOfFlowGeometry.definiteContentBoxBlockSize(inlineAxis.borderBoxSize).transform([&](auto contentBoxBlockSize) {
        return std::max(0_lu, contentBoxBlockSize - geometry.verticalSpaceForScrollbar());
    });

    auto intrinsicContentBlockSize = LayoutUnit { };
    // Where the content broke across the fragmentainers from.
    auto fragmentedContentBorderBoxBlockStart = std::optional<LayoutUnit> { };
    // content-visibility skips the content of a box, which stays as it is (RenderBlockFlow::layoutInFlowChildren).
    if (box->firstChild() && !box->layoutContext().isSkippedContentRootForLayout(box)) {
        LayoutIntegration::BlockIntegrationUtils::markPercentageHeightDescendantsForLayout(box, contentBoxBlockSize);
        auto horizontalConstraints = HorizontalConstraints { geometry.borderAndPaddingStart() + scrollbarInlineStartSpace(box, geometry), geometry.contentBoxWidth() };
        // The available block size of its content is its own definite block size, and otherwise the one of its containing block, less its block
        // margins, border and padding for -webkit-fill-available (RenderBox::availableLogicalHeightUsing).
        auto fillAvailableBlockSize = contentBoxBlockSize.value_or([&] {
            auto writingMode = box->writingMode();
            auto margin = [&](auto& margin) {
                return margin.isAuto() ? 0_lu : Style::evaluateMinimum<LayoutUnit>(margin, containingBlock.inlineSize, box->style().usedZoomForLength());
            };
            return containingBlock.fillAvailableBlockSize.value_or(0_lu) - margin(box->style().marginBefore(writingMode)) - margin(box->style().marginAfter(writingMode)) - geometry.verticalBorderAndPadding();
        }());
        auto replacedElementAvailableBlockSize = contentBoxBlockSize.value_or(containingBlock.availableBlockSizeIncludingMarginBorderPadding.value_or(0_lu));
        auto relayoutChildren = LayoutIntegration::BlockIntegrationUtils::contentBoxInlineSizeChanged(box, geometry.contentBoxWidth());
        // Its content snaps to the line grid around it from where it goes before it is laid out, without content, like the render tree path,
        // which lays it out again where it goes but keeps the lines (RenderBlock::layoutOutOfFlowBox).
        auto borderBoxBlockStart = outOfFlowGeometry.computeBlockAxisGeometry(geometry.verticalSpaceForScrollbar(), inlineAxis.borderBoxSize).borderBoxStart;
        auto lineGrid = lineGridForOutOfFlowBox(box, containingBlock, containingBlockLineGrid, containingBlock.inlineStart + inlineAxis.borderBoxStart, containingBlock.blockStart + borderBoxBlockStart, inlineAxis.borderBoxSize);
        auto rootLineGrid = rootLineGridForOutOfFlowBox(box, lineGrid);
        auto contentConstraints = ConstraintsForBlockContent { { horizontalConstraints, geometry.borderAndPaddingBefore() }, contentBoxBlockSize, fillAvailableBlockSize, replacedElementAvailableBlockSize, relayoutChildren };
        auto contentResult = [&] {
            if (!fragmentedContentLayout)
                return contentFormattingContext.layout(contentConstraints, geometry, rootLineGrid);
            // In fragmented content, from where it goes. When that depends on its block size, it is where the whole content puts it, which is
            // right unless the box crosses the end of a fragmentainer: breaking its content there makes it taller, which would move it and change
            // where its content breaks (css-break-3 §5.5 does not require that to be right).
            fragmentedContentBorderBoxBlockStart = borderBoxBlockStart;
            if (outOfFlowGeometry.blockAxisBorderBoxStartDependsOnSize()) {
                auto wholeContentBlockLayoutState = blockLayoutState;
                auto wholeContentBlockSize = BlockFormattingContext { box, integrationUtils(), wholeContentBlockLayoutState }.layout(contentConstraints, geometry, rootLineGrid).intrinsicContentBlockSize;
                fragmentedContentBorderBoxBlockStart = outOfFlowGeometry.computeBlockAxisGeometry(wholeContentBlockSize + geometry.verticalSpaceForScrollbar(), inlineAxis.borderBoxSize).borderBoxStart;
            }
            return (*fragmentedContentLayout)(box, blockLayoutState, contentConstraints, geometry, rootLineGrid, containingBlock.blockStart + *fragmentedContentBorderBoxBlockStart);
        }();
        intrinsicContentBlockSize = contentResult.intrinsicContentBlockSize;
        result.boxes.appendVector(WTF::move(contentResult.boxes));
        result.outOfFlowBoxes.appendVector(WTF::move(contentResult.outOfFlowBoxes));
        result.outOfFlowContainingBlockLineGrids.appendVector(WTF::move(contentResult.outOfFlowContainingBlockLineGrids));
    } else if (auto contentBlockSize = BlockFormattingUtils::emptyEditableContentBlockSize(box))
        intrinsicContentBlockSize = *contentBlockSize;
    // Size containment sizes the box as if it had no content (css-contain-2 §3.2, RenderBox::overrideLogicalHeightForSizeContainment).
    if (box->shouldApplySizeContainment())
        intrinsicContentBlockSize = box->explicitIntrinsicInnerLogicalHeight().value_or(0_lu);

    auto blockAxis = outOfFlowGeometry.computeBlockAxisGeometry(intrinsicContentBlockSize + geometry.verticalSpaceForScrollbar(), inlineAxis.borderBoxSize);
    setContentBoxBlockSize(geometry, blockAxis.borderBoxSize);

    if (outOfFlowGeometry.isOrthogonal())
        transposeToContainingBlockAxes(geometry, inlineAxis, blockAxis);
    geometry.setHorizontalMargin({ inlineAxis.marginStart, inlineAxis.marginEnd });
    geometry.setLeft(containingBlock.inlineStart + inlineAxis.borderBoxStart);
    geometry.setVerticalMargin({ blockAxis.marginStart, blockAxis.marginEnd });
    // A box whose content broke across the fragmentainers stays where it broke from, where the fragments merge into the strip.
    geometry.setTop(containingBlock.blockStart + fragmentedContentBorderBoxBlockStart.value_or(blockAxis.borderBoxStart));

    auto margins = [](LayoutUnit margin) {
        auto adjoiningMargins = AdjoiningMargins { };
        adjoiningMargins.add(margin);
        return adjoiningMargins;
    };
    result.boxes.append({ box, geometry, intrinsicContentBlockSize, margins(geometry.marginBefore()), margins(geometry.marginAfter()), false, false, false });
    return result;
}

ContainingBlockState BlockFormattingContext::containingBlockStateForRoot(const ConstraintsForBlockContent& constraints, const BoxGeometry& rootGeometry) const
{
    auto horizontalConstraints = constraints.horizontal();
    return {
        .blockContainer = root(),
        .geometry = rootGeometry,
        .inlineRange = { horizontalConstraints.logicalLeft, horizontalConstraints.logicalWidth },
        .borderBoxLogicalLeftInRoot = { },
        .hasInlineDirectionOppositeToRoot = false,
        .borderBoxLogicalTop = 0_lu,
        .borderAndPaddingBefore = constraints.logicalTop(),
        .contentBoxBlockSize = constraints.contentBoxBlockSize(),
        .fillAvailableBlockSize = constraints.fillAvailableBlockSize(),
        .replacedElementAvailableBlockSize = constraints.replacedElementAvailableBlockSize(),
        .margins = { },
        .isLaidOutThroughRenderTree = false,
        .relayoutChildren = constraints.relayoutChildren(),
        .lineCountThroughLastBlockLevelBox = 0,
        .blockEndChildren = { { }, { }, constraints.logicalTop() }
    };
}

ContainingBlockState BlockFormattingContext::containingBlockStateFor(const RenderBox& blockContainer, const BoxGeometry& geometry, const ContainingBlockState& parentState) const
{
    auto contentBoxBlockSize = BlockFormattingUtils::definiteContentBoxBlockSize(blockContainer, geometry, percentageBasis(blockContainer, parentState), stretchFitBorderBoxBlockSizes(geometry, parentState), root().writingMode(), containingBlockIsTableCell(parentState));
    return {
        .blockContainer = blockContainer,
        .geometry = geometry,
        .inlineRange = { geometry.borderAndPaddingStart() + scrollbarInlineStartSpace(blockContainer, geometry), geometry.contentBoxWidth() },
        .borderBoxLogicalLeftInRoot = inlineStartInRoot(parentState, BoxGeometry::borderBoxLeft(geometry), geometry.borderBoxWidth()),
        .hasInlineDirectionOppositeToRoot = blockContainer.writingMode().isLogicalLeftInlineStart() != root().writingMode().isLogicalLeftInlineStart(),
        .borderBoxLogicalTop = { },
        .borderAndPaddingBefore = geometry.borderAndPaddingBefore(),
        .contentBoxBlockSize = contentBoxBlockSize,
        .fillAvailableBlockSize = BlockFormattingUtils::availableBlockSize(blockContainer, geometry, contentBoxBlockSize, parentState.contentBoxBlockSize, parentState.fillAvailableBlockSize, stretchFitBorderBoxBlockSizes(geometry, parentState), root().writingMode(), AvailableLogicalHeightType::ExcludeMarginBorderPadding),
        .replacedElementAvailableBlockSize = BlockFormattingUtils::availableBlockSize(blockContainer, geometry, contentBoxBlockSize, parentState.contentBoxBlockSize, parentState.replacedElementAvailableBlockSize, stretchFitBorderBoxBlockSizes(geometry, parentState), root().writingMode(), AvailableLogicalHeightType::IncludeMarginBorderPadding),
        .margins = { },
        .isLaidOutThroughRenderTree = false,
        // -webkit-line-clamp counts the lines of everything in it again (RenderBlockFlow::layoutBlock).
        .relayoutChildren = LayoutIntegration::BlockIntegrationUtils::contentBoxInlineSizeChanged(blockContainer, geometry.contentBoxWidth()) || (m_blockLayoutState.lineClamp() && m_blockLayoutState.lineClamp()->isLegacy),
        .lineCountThroughLastBlockLevelBox = 0,
        .blockEndChildren = { { }, { }, geometry.borderAndPaddingBefore() },
        .lineGrid = lineGridFor(blockContainer, parentState)
    };
}

// Like RenderLayoutState::propagateLineGridInfo and establishLineGrid: the line grid around a box that cannot be split across pages does not
// reach its content, and a block container with a -webkit-line-grid name snaps its content to the nearest line grid around it with that
// name, or establishes one.
std::optional<ContainingBlockState::LineGrid> BlockFormattingContext::lineGridFor(const RenderBox& blockContainer, const ContainingBlockState& parentState) const
{
    auto lineGrid = blockContainer.isUnsplittableForPagination() ? std::nullopt : parentState.lineGrid;
    CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(blockContainer);
    auto& name = blockContainer.style().lineGrid();
    if (!blockFlow || name.isNone())
        return lineGrid;
    auto establishedLineGrid = ContainingBlockState::LineGrid { *blockFlow, { } };
    if (!lineGrid)
        return establishedLineGrid;
    if (lineGrid->blockContainer->style().lineGrid() == name)
        return lineGrid;
    CheckedPtr<const RenderBlockFlow> currentGridContainer = lineGrid->blockContainer.ptr();
    for (auto& state : m_blockLayoutState.containingBlockStates() | std::views::reverse) {
        if (state.lineGrid && state.lineGrid->blockContainer.ptr() == currentGridContainer)
            continue;
        if (!state.lineGrid)
            return establishedLineGrid;
        currentGridContainer = state.lineGrid->blockContainer.ptr();
        if (currentGridContainer->style().lineGrid() == name)
            return state.lineGrid;
    }
    if (m_rootLineGrid.isInLayoutStateOfRoot) {
        if (auto lineGridOutsideRoot = LayoutIntegration::BlockIntegrationUtils::lineGridOutsideRoot(root(), name, *currentGridContainer))
            return lineGridOutsideRoot;
        return establishedLineGrid;
    }
    // Those around a root that the block formatting context around it lays out.
    auto& lineGridsAroundRoot = m_rootLineGrid.lineGridsAroundRoot;
    auto index = lineGridsAroundRoot.reverseFindIf([&](auto& lineGridAround) {
        return lineGridAround.blockContainer->style().lineGrid() == name;
    });
    if (index == notFound)
        return establishedLineGrid;
    auto& [gridContainer, borderBoxOffsetInLayoutState] = lineGridsAroundRoot[index];
    auto logicalBorderBoxOffset = root().writingMode().isHorizontal() ? borderBoxOffsetInLayoutState : borderBoxOffsetInLayoutState.transposedSize();
    return ContainingBlockState::LineGrid { gridContainer, ContainingBlockState::LineGrid::OutsideRoot { logicalBorderBoxOffset - m_rootLineGrid.borderBoxOffsetInLayoutState, gridContainer->borderAndPaddingBefore() } };
}

// From the left of the root's border box, like the offsets of the render tree layout state (RenderLayoutState::layoutOffset).
LayoutSize BlockFormattingContext::logicalLeftAndTopInRoot(LayoutUnit inlineStartInRoot, LayoutUnit borderBoxInlineSize, LayoutUnit logicalTopInRoot) const
{
    if (root().writingMode().isLogicalLeftInlineStart())
        return { inlineStartInRoot, logicalTopInRoot };
    return { m_blockLayoutState.containingBlockStates().first().geometry.borderBoxWidth() - inlineStartInRoot - borderBoxInlineSize, logicalTopInRoot };
}

LayoutSize BlockFormattingContext::borderBoxLogicalLeftAndTopInRoot(const ContainingBlockState& blockContainerState)
{
    if (&blockContainerState == &m_blockLayoutState.containingBlockStates().first())
        return { };
    auto offsetInRoot = borderBoxOffsetInRoot(blockContainerState);
    return logicalLeftAndTopInRoot(offsetInRoot.width(), blockContainerState.geometry.borderBoxWidth(), offsetInRoot.height());
}

// The line grid of the content of a block container, and the ones around it, from a box's border box in the root, in the coordinates of the render
// tree layout state.
// In the coordinates of the render tree's layout state, from the logical left.
LayoutSize BlockFormattingContext::offsetInLayoutState(LayoutSize offsetInRoot) const
{
    auto offset = m_rootLineGrid.borderBoxOffsetInLayoutState + offsetInRoot;
    return root().writingMode().isHorizontal() ? offset : offset.transposedSize();
}

std::optional<LineGridInLayoutState> BlockFormattingContext::lineGridInLayoutState(const ContainingBlockState& containingBlockState, LayoutSize borderBoxOffset)
{
    if (!containingBlockState.lineGrid)
        return { };
    // From the containing block to the root, which those around the root follow, unless one in between has none, which ends the search for a
    // name. The containing block is in the containing block states, or the state of a box being laid out whose parent is the last one.
    auto& containingBlockStates = m_blockLayoutState.containingBlockStates();
    auto isInContainingBlockStates = containingBlockStates.containsIf([&](auto& state) {
        return &state == &containingBlockState;
    });
    auto states = Vector<const ContainingBlockState*> { };
    if (!isInContainingBlockStates)
        states.append(&containingBlockState);
    auto skipsInnerStates = isInContainingBlockStates;
    for (auto& state : containingBlockStates | std::views::reverse) {
        if (skipsInnerStates) {
            if (&state != &containingBlockState)
                continue;
            skipsInnerStates = false;
        }
        states.append(&state);
    }
    auto innerStateCount = size_t { 0 };
    while (innerStateCount < states.size() && states[innerStateCount]->lineGrid)
        ++innerStateCount;
    auto lineGrids = Vector<LineGridInLayoutState::LineGrid> { };
    auto addLineGrid = [&](const RenderBlockFlow& blockContainer, LayoutSize offset) {
        if (lineGrids.isEmpty() || lineGrids.last().blockContainer.ptr() != &blockContainer)
            lineGrids.append({ blockContainer, offset });
    };
    if (innerStateCount == states.size()) {
        for (auto& lineGrid : m_rootLineGrid.lineGridsAroundRoot)
            addLineGrid(lineGrid.blockContainer, lineGrid.borderBoxOffset);
    }
    // The block container that establishes a line grid in the root is in the states, which may not be in the containing block states anymore.
    auto gridContainerOffset = [&](const ContainingBlockState::LineGrid& lineGrid) {
        if (lineGrid.outsideRoot)
            return lineGrid.outsideRoot->borderBoxOffset;
        for (auto* state : states) {
            if (state->blockContainer.ptr() == lineGrid.blockContainer.ptr())
                return borderBoxLogicalLeftAndTopInRoot(*state);
        }
        return lineGridBorderBoxOffsetAndBorderAndPaddingBefore(lineGrid).first;
    };
    for (auto index = innerStateCount; index--;) {
        auto& lineGrid = *states[index]->lineGrid;
        addLineGrid(lineGrid.blockContainer, offsetInLayoutState(gridContainerOffset(lineGrid)));
    }
    return LineGridInLayoutState { WTF::move(lineGrids), offsetInLayoutState(borderBoxOffset) };
}

// -webkit-line-align puts the edges of the space the block-level boxes in a block container go in, and the out-of-flow boxes that were inline-level,
// on the columns of the line grid of its content, from the border box of the block container that establishes it, the start pushed in and the end
// pulled back to the nearest column (RenderBlock::adjustLogicalLeftOffsetForLine, adjustLogicalRightOffsetForLine).
std::pair<LayoutUnit, LayoutUnit> BlockFormattingContext::spaceOnLineGridColumns(const ContainingBlockState& blockContainerState, LayoutUnit start, LayoutUnit end)
{
    auto& lineGrid = blockContainerState.lineGrid;
    CheckedRef blockContainer = blockContainerState.blockContainer;
    if (!lineGrid || blockContainer->style().lineAlign() == LineAlign::None || lineGrid->blockContainer->writingMode().computedWritingMode() != blockContainer->writingMode().computedWritingMode())
        return { start, end };
    float columnWidth = lineGrid->blockContainer->style().fontCascade().primaryFont().maxCharWidth();
    if (!columnWidth)
        return { start, end };
    auto offset = borderBoxLogicalLeftAndTopInRoot(blockContainerState).width() - lineGridBorderBoxOffsetAndBorderAndPaddingBefore(*lineGrid).first.width();
    // From the left of the block container's border box, like the columns.
    auto isLeftToRight = blockContainer->writingMode().isLogicalLeftInlineStart();
    auto borderBoxInlineSize = blockContainerState.geometry.borderBoxWidth();
    auto left = isLeftToRight ? start : borderBoxInlineSize - end;
    auto right = isLeftToRight ? end : borderBoxInlineSize - start;
    left += fmodf(columnWidth - fmodf((left + offset).toFloat(), columnWidth), columnWidth);
    right -= ceilf(fmodf(fmodf((right + offset).toFloat(), columnWidth), columnWidth));
    if (isLeftToRight)
        return { left, right };
    return { borderBoxInlineSize - right, borderBoxInlineSize - left };
}

// A box the render tree lays out on its own gets the line grid around it from the layout state of its containing block, which the block
// formatting context lays out, unless it cannot be split across pages (RenderLayoutState::propagateLineGridInfo).
std::optional<LineGridInLayoutState> BlockFormattingContext::lineGridForBoxLaidOutThroughRenderTree(const RenderBox& box, const ContainingBlockState& containingBlockState, LayoutSize borderBoxOffset)
{
    if (box.isUnsplittableForPagination())
        return { };
    return lineGridInLayoutState(containingBlockState, borderBoxOffset);
}

std::pair<LayoutSize, LayoutUnit> BlockFormattingContext::lineGridBorderBoxOffsetAndBorderAndPaddingBefore(const ContainingBlockState::LineGrid& lineGrid)
{
    if (auto& outsideRoot = lineGrid.outsideRoot)
        return { outsideRoot->borderBoxOffset, outsideRoot->borderAndPaddingBefore };
    auto& containingBlockStates = m_blockLayoutState.containingBlockStates();
    auto gridContainerState = containingBlockStates.findIf([&](auto& state) {
        return state.blockContainer.ptr() == lineGrid.blockContainer.ptr();
    });
    if (gridContainerState == notFound) {
        ASSERT_NOT_REACHED();
        return { };
    }
    return { borderBoxLogicalLeftAndTopInRoot(containingBlockStates[gridContainerState]), containingBlockStates[gridContainerState].borderAndPaddingBefore };
}

// A block formatting context root the block formatting context lays out snaps its content to the line grid from its border box, where the
// margins put it before it is laid out, and so does the content of a float the block formatting context lays out, from where the float goes.
std::optional<ContainingBlockState::LineGrid> BlockFormattingContext::lineGridForNestedRoot(const ContainingBlockState& rootState, LayoutSize borderBoxOffsetInRoot)
{
    auto& lineGrid = rootState.lineGrid;
    if (!lineGrid)
        return { };
    if (lineGrid->blockContainer.ptr() == rootState.blockContainer.ptr())
        return ContainingBlockState::LineGrid { lineGrid->blockContainer, { } };
    auto [gridOffset, borderAndPaddingBefore] = lineGridBorderBoxOffsetAndBorderAndPaddingBefore(*lineGrid);
    return ContainingBlockState::LineGrid { lineGrid->blockContainer, ContainingBlockState::LineGrid::OutsideRoot { gridOffset - borderBoxOffsetInRoot, borderAndPaddingBefore } };
}

// The line grid of the content of a block formatting context root the block formatting context lays out, or of a float, and the ones around it.
RootLineGrid BlockFormattingContext::rootLineGridForNestedRoot(const ContainingBlockState& rootState, std::optional<LayoutSize> borderBoxOffsetInRoot)
{
    auto offsetInRoot = borderBoxOffsetInRoot.value_or(borderBoxLogicalLeftAndTopInRoot(rootState));
    auto lineGridsAround = lineGridInLayoutState(m_blockLayoutState.containingBlockStates().last(), offsetInRoot);
    return {
        lineGridForNestedRoot(rootState, offsetInRoot),
        m_rootLineGrid.borderBoxOffsetInLayoutState + offsetInRoot,
        false,
        lineGridsAround ? WTF::move(lineGridsAround->lineGrids) : Vector<LineGridInLayoutState::LineGrid> { }
    };
}

// The line grid of the content of a block container that out-of-flow boxes in the content may have as their containing block, which they are
// laid out with once the content is: one that is positioned, or the one around a positioned inline box (RenderObject::containingBlock).
void BlockFormattingContext::addLineGridOfOutOfFlowContainingBlock(const RenderBox& blockContainer, const ContainingBlockState& blockContainerState, BlockLayoutResult& result)
{
    if (!blockContainerState.lineGrid)
        return;
    if (auto lineGrid = lineGridInLayoutState(blockContainerState, borderBoxLogicalLeftAndTopInRoot(blockContainerState)))
        result.outOfFlowContainingBlockLineGrids.append({ blockContainer, WTF::move(*lineGrid) });
}

// An out-of-flow box gets the line grid of the content of its containing block unless it cannot be split across pages, from where it goes in its
// containing block, in the coordinates of the render tree layout state (RenderLayoutState::computeOffsets).
std::optional<LineGridInLayoutState> BlockFormattingContext::lineGridForOutOfFlowBox(const RenderBox& box, const OutOfFlowContainingBlock& containingBlock, const std::optional<LineGridInLayoutState>& containingBlockLineGrid, LayoutUnit borderBoxInlineStart, LayoutUnit borderBoxBlockStart, LayoutUnit borderBoxInlineSize) const
{
    if (!containingBlockLineGrid || box.isUnsplittableForPagination())
        return { };
    // From the left of the border box of the containing block, which has its size.
    CheckedRef containingBlockRenderer = *box.containingBlock();
    auto logicalLeft = containingBlock.writingMode.isLogicalLeftInlineStart() ? borderBoxInlineStart : containingBlockRenderer->logicalWidth() - borderBoxInlineStart - borderBoxInlineSize;
    auto offset = LayoutSize { logicalLeft, borderBoxBlockStart };
    if (!containingBlock.writingMode.isHorizontal())
        offset = offset.transposedSize();
    return LineGridInLayoutState { containingBlockLineGrid->lineGrids, containingBlockLineGrid->boxBorderBoxOffset + offset };
}

// The line grid an out-of-flow box whose content the block formatting context lays out snaps its content to: the one around it, or the nearest
// one around it with the name the box has, unless the box establishes that one (RenderLayoutState::establishLineGrid).
RootLineGrid BlockFormattingContext::rootLineGridForOutOfFlowBox(const RenderBox& box, const std::optional<LineGridInLayoutState>& lineGrid) const
{
    CheckedRef blockFlow = downcast<RenderBlockFlow>(box);
    CheckedRef style = box.style();
    auto& name = style->lineGrid();
    auto establishedLineGrid = name.isNone() ? std::nullopt : std::optional { ContainingBlockState::LineGrid { blockFlow, { } } };
    if (!lineGrid || lineGrid->lineGrids.isEmpty())
        return { establishedLineGrid, { }, false, { } };
    auto isHorizontal = box.writingMode().isHorizontal();
    auto logicalOffset = [&](LayoutSize offset) {
        return isHorizontal ? offset : offset.transposedSize();
    };
    auto borderBoxOffset = logicalOffset(lineGrid->boxBorderBoxOffset);
    auto lineGridsAround = lineGrid->lineGrids;
    auto index = lineGridsAround.size() - 1;
    if (!name.isNone() && lineGridsAround[index].blockContainer->style().lineGrid() != name) {
        index = lineGridsAround.reverseFindIf([&](auto& lineGridAround) {
            return lineGridAround.blockContainer->style().lineGrid() == name;
        });
        if (index == notFound)
            return { establishedLineGrid, borderBoxOffset, false, WTF::move(lineGridsAround) };
        auto namedLineGrid = lineGridsAround[index];
        lineGridsAround.append(WTF::move(namedLineGrid));
        index = lineGridsAround.size() - 1;
    }
    CheckedRef gridContainer = lineGridsAround[index].blockContainer;
    auto outsideRoot = ContainingBlockState::LineGrid::OutsideRoot { logicalOffset(lineGridsAround[index].borderBoxOffset) - borderBoxOffset, gridContainer->borderAndPaddingBefore() };
    return { ContainingBlockState::LineGrid { gridContainer, outsideRoot }, borderBoxOffset, false, WTF::move(lineGridsAround) };
}

bool BlockFormattingContext::isAroundFragmentedFlow(const RenderBox& gridContainer, const RenderBox& box)
{
    auto* fragmentedFlow = box.enclosingFragmentedFlow();
    return fragmentedFlow && gridContainer.enclosingFragmentedFlow() != fragmentedFlow;
}

// What the inline formatting context snaps the lines of a block container to (RenderBlockFlow's lineGrid): the line grid in the same writing
// mode, from the offset of the block container's border box to the one of the block container that establishes it.
std::optional<BlockLayoutState::LineGrid> BlockFormattingContext::lineGridForInlineContent(const ContainingBlockState& blockContainerState, bool continuesFromBreak)
{
    auto& lineGrid = blockContainerState.lineGrid;
    if (!lineGrid)
        return { };
    CheckedRef gridContainer = lineGrid->blockContainer;
    if (gridContainer->writingMode().computedWritingMode() != blockContainerState.blockContainer->writingMode().computedWritingMode())
        return { };
    // The rows start at the content box of the block container that establishes the line grid. Its fragments after the first have no
    // block-start border and padding, from where the rows start at the top of the fragmentainer, except for lines that continue from the
    // fragmentainer before, which start below the block-start border and padding there, like the render tree snaps the first line after a
    // break (computeFirstLineSnapAdjustment) and moves the lines after it with it.
    auto [gridOffset, topRowOffset] = lineGridBorderBoxOffsetAndBorderAndPaddingBefore(*lineGrid);
    CheckedRef gridStyle = gridContainer->style();
    auto rowHeight = LayoutUnit::fromFloatCeil(gridStyle->usedLineHeight());
    // So do those of one around the root that starts above the fragmentainer, like the render tree snaps lines from the top of the page when it
    // is below the first row (RenderBlockFlow's pageLogicalTopForOffset in lineGrid), except one around the multi-column flow the root is in,
    // whose rows start at the first one below the top of each column, which keeps them where they are in the first one
    // (RenderLayoutState::computeLineGridPaginationOrigin).
    if (lineGrid->outsideRoot && m_fragmentainer) {
        auto fragmentainerTop = -m_fragmentainer->rootBlockOffset;
        if (!isAroundFragmentedFlow(gridContainer, root())) {
            if (gridOffset.height() < fragmentainerTop)
                gridOffset.setHeight(fragmentainerTop);
        } else if (auto firstRowTop = gridOffset.height() + topRowOffset; firstRowTop < fragmentainerTop && roundToInt(rowHeight)) {
            gridOffset.setHeight(fragmentainerTop + rowHeight - roundToInt(fragmentainerTop - firstRowTop) % roundToInt(rowHeight));
            topRowOffset = 0_lu;
        }
    }
    if (continuesFromBreak && !lineGrid->outsideRoot) {
        auto& containingBlockStates = m_blockLayoutState.containingBlockStates();
        auto gridContainerState = containingBlockStates.findIf([&](auto& state) {
            return state.blockContainer.ptr() == lineGrid->blockContainer.ptr();
        });
        if (gridContainerState != notFound && containingBlockStates[gridContainerState].consumedBlockSize)
            topRowOffset = containingBlockStates[gridContainerState].consumedBlockSize->borderAndPaddingBefore;
    }
    Ref primaryFont = gridStyle->fontCascade().primaryFont();
    auto columnWidth = primaryFont->maxCharWidth();
    return BlockLayoutState::LineGrid { borderBoxLogicalLeftAndTopInRoot(blockContainerState), gridOffset, columnWidth, rowHeight, topRowOffset, WTF::move(primaryFont), { }, { } };
}

BoxGeometry BlockFormattingContext::computeBorderAndPadding(const RenderBox& box, LayoutUnit containingBlockInlineSize) const
{
    CheckedRef style = box.style();
    // Start and end follow the box's own direction. A box whose writing mode differs from its parent's establishes
    // a new block formatting context, so the block flow direction is the root's.
    auto writingMode = box.writingMode();

    auto geometry = BoxGeometry { };
    auto usedStyle = UsedStyle { box };
    geometry.setBorder({
        { usedStyle.borderWidthStart(), usedStyle.borderWidthEnd() },
        { usedStyle.borderWidthBefore(), usedStyle.borderWidthAfter() }
    });
    // Percentages refer to the inline size of the containing block, on every side (CSS 2.2 §8.4).
    auto padding = [&](auto& paddingEdge) {
        return Style::evaluateMinimum<LayoutUnit>(paddingEdge, containingBlockInlineSize, style->usedZoomForLength());
    };
    geometry.setPadding({
        { padding(style->paddingStart(writingMode)), padding(style->paddingEnd(writingMode)) },
        { padding(style->paddingBefore(writingMode)), padding(style->paddingAfter(writingMode)) }
    });
    // The scrollbars of a scroll container take space from its padding box, between it and the border, which its content box gives up
    // (RenderBox::verticalScrollbarWidth, horizontalScrollbarHeight), twice in the inline axis with scrollbar-gutter: stable both-edges
    // (RenderBox::contentBoxWidth).
    if (box.hasNonVisibleOverflow()) {
        auto spaceForScrollbars = LayoutSize { box.verticalScrollbarWidth(), box.horizontalScrollbarHeight() };
        if (!writingMode.isHorizontal())
            spaceForScrollbars = spaceForScrollbars.transposedSize();
        if (style->scrollbarGutter().isStableBothEdges())
            spaceForScrollbars.setWidth(spaceForScrollbars.width() * 2);
        geometry.setSpaceForScrollbar(spaceForScrollbars);
    }
    return geometry;
}

// Where the content box of a scroll container starts in its padding box: past a vertical scrollbar on its inline-start side, or past the
// gutter on that side with scrollbar-gutter: stable both-edges (RenderBlockFlow::determineLogicalLeftPositionForChild).
LayoutUnit BlockFormattingContext::scrollbarInlineStartSpace(const RenderBox& box, const BoxGeometry& geometry) const
{
    auto inlineSpace = geometry.horizontalSpaceForScrollbar();
    if (!inlineSpace)
        return { };
    if (box.style().scrollbarGutter().isStableBothEdges())
        return inlineSpace / 2;
    // In the box's own direction, which border and padding follow.
    auto writingMode = box.writingMode();
    auto isOnLogicalLeft = writingMode.isHorizontal() && box.shouldPlaceVerticalScrollbarOnLeft();
    return isOnLogicalLeft == writingMode.isLogicalLeftInlineStart() ? inlineSpace : 0_lu;
}

void BlockFormattingContext::computeInlineSizeAndMargins(const RenderBox& box, const ContainingBlockState& containingBlockState, BoxGeometry& geometry, std::optional<LayoutUnit> availableInlineSize) const
{
    CheckedRef style = box.style();
    auto writingMode = root().writingMode();
    auto containingBlockInlineSize = containingBlockState.inlineRange.size();
    // Percentages refer to the inline size of the containing block, and auto margins are zero in the block direction (CSS 2.2 §8.3, §10.6.3).
    auto blockDirectionMargin = [&](auto& marginEdge) {
        return marginEdge.isAuto() ? 0_lu : Style::evaluateMinimum<LayoutUnit>(marginEdge, containingBlockInlineSize, style->usedZoomForLength());
    };
    geometry.setVerticalMargin({ blockDirectionMargin(style->marginBefore(writingMode)), blockDirectionMargin(style->marginAfter(writingMode)) });
    // The inline size and margins come once its own formatting context has decided the inline size (layoutIndependentFormattingContextThroughRenderTree).
    CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(box);
    auto isLaidOutThroughRenderTree = !blockContainer || (BlockFormattingUtils::establishesIndependentFormattingContext(*blockContainer) && LayoutIntegration::BlockIntegrationUtils::laysOutThroughRenderTree(*blockContainer) && !laysOutFloatContent(box));
    if (isLaidOutThroughRenderTree && decidesItsOwnInlineSize(box, writingMode) && !(availableInlineSize && hasInlineSizeFromSpaceBesideFloats(box, writingMode)))
        return;

    CheckedRef containingBlockStyle = containingBlockState.blockContainer->style();
    // A preferred aspect ratio gives an auto inline size from a definite block size, and the min-content and max-content sizes of the box,
    // floored by the min-content size of its content when its min inline size is auto (RenderBox::applyAutomaticContentBasedMinimumSize).
    auto horizontalBorderAndPadding = geometry.horizontalBorderAndPadding();
    auto aspectRatioBorderBoxInlineSize = BlockFormattingUtils::aspectRatioBorderBoxInlineSize(box, horizontalBorderAndPadding, geometry.verticalBorderAndPadding(), containingBlockState.contentBoxBlockSize, stretchFitBorderBoxBlockSizes(geometry, containingBlockState), writingMode);
    auto contentInlineSizes = [&] -> std::optional<IntrinsicWidthConstraints> {
        auto fitsContent = style->logicalWidth(writingMode).isAuto() && BlockFormattingUtils::autoInlineSizeFitsContent(box, containingBlockStyle);
        if (!BlockFormattingUtils::hasIntrinsicInlineSize(box, writingMode) && !fitsContent && !BlockFormattingUtils::hasAspectRatioAutomaticMinimumInlineSize(box, aspectRatioBorderBoxInlineSize, writingMode))
            return { };
        auto sizes = [&] {
            // Its own formatting context measures a box it lays out, with its scrollbars.
            if (isLaidOutThroughRenderTree)
                return LayoutIntegration::BlockIntegrationUtils::independentFormattingContextIntrinsicInlineSizes(box, horizontalBorderAndPadding);
            // The scrollbars that come or go in its content change these (RenderBlock::layout).
            integrationUtils().trackScrollbarChangesForContentSizes();
            auto contentBoxBlockSize = contentBoxBlockSizeWhileMeasured(box, geometry, percentageBasis(box, containingBlockState), stretchFitBorderBoxBlockSizes(geometry, containingBlockState), writingMode, containingBlockIsTableCell(containingBlockState));
            auto sizes = computeIntrinsicInlineSizes(*blockContainer, contentBoxBlockSize);
            // With its scrollbars (RenderBlockFlow::computeIntrinsicLogicalWidths).
            auto scrollbarInlineSize = LayoutUnit { box.intrinsicScrollbarLogicalWidthIncludingGutter() };
            return IntrinsicWidthConstraints { sizes.minimum + scrollbarInlineSize, sizes.maximum + scrollbarInlineSize };
        }();
        if (!aspectRatioBorderBoxInlineSize)
            return sizes;
        return BlockFormattingUtils::aspectRatioContentInlineSizes(box, std::max(0_lu, *aspectRatioBorderBoxInlineSize - horizontalBorderAndPadding), sizes, horizontalBorderAndPadding, writingMode);
    }();
    // They apply to an inline size that is not a length or a percentage.
    auto aspectRatioInlineSizeLimits = std::optional<std::pair<LayoutUnit, LayoutUnit>> { };
    if (!style->logicalWidth(writingMode).isSpecified())
        aspectRatioInlineSizeLimits = BlockFormattingUtils::aspectRatioInlineSizeLimits(box, geometry.horizontalBorderAndPadding(), geometry.verticalBorderAndPadding(), containingBlockState.contentBoxBlockSize, stretchFitBorderBoxBlockSizes(geometry, containingBlockState), writingMode);
    auto [borderBoxInlineSize, marginStart, marginEnd] = BlockFormattingUtils::inlineSizeAndMargins(box, geometry.horizontalBorderAndPadding(), containingBlockInlineSize, containingBlockStyle, contentInlineSizes, aspectRatioInlineSizeLimits, aspectRatioBorderBoxInlineSize, availableInlineSize);
    setContentBoxInlineSize(geometry, borderBoxInlineSize);
    // Unlike border and padding, start and end margins follow the direction of the containing block.
    geometry.setHorizontalMargin({ marginStart, marginEnd });
}

IntrinsicWidthConstraints BlockFormattingContext::computeIntrinsicInlineSizes(const RenderBlockFlow& blockContainer, std::optional<LayoutUnit> contentBoxBlockSize) const
{
    auto sizes = computeContentIntrinsicInlineSizes(blockContainer, contentBoxBlockSize);
    if (BlockFormattingUtils::isScrollingHorizontalMarquee(blockContainer))
        sizes.minimum = { };
    return sizes;
}

// A box with inline size containment measures as if it had no content, by contain-intrinsic-inline-size (css-contain-2 §3.3,
// RenderBlockFlow::computeIntrinsicLogicalWidths).
static std::optional<IntrinsicWidthConstraints> containedIntrinsicInlineSizes(const RenderBox& box)
{
    if (!box.shouldApplySizeOrInlineSizeContainment())
        return { };
    auto inlineSize = box.explicitIntrinsicInnerLogicalWidth().value_or(0_lu);
    return IntrinsicWidthConstraints { inlineSize, inlineSize };
}

// Goes through the block items of the block container's content the way layout does, collecting the outer min-content and max-content
// contributions of the boxes in it (§5.2).
IntrinsicWidthConstraints BlockFormattingContext::computeContentIntrinsicInlineSizes(const RenderBlockFlow& blockContainer, std::optional<LayoutUnit> contentBoxBlockSize) const
{
    if (auto sizes = containedIntrinsicInlineSizes(blockContainer))
        return *sizes;
    if (blockContainer.childrenInline())
        return blockContainer.firstChild() ? LayoutIntegration::BlockIntegrationUtils::inlineContentIntrinsicInlineSizes(blockContainer) : IntrinsicWidthConstraints { };

    auto writingMode = root().writingMode();
    // The outer size: the inline size, clamped by the min and max inline sizes, plus border, padding and margins, where auto margins and
    // percentages, which are cyclic here, count as zero and percentage inline sizes as auto (§5.2.1). The render tree path resolves percentage
    // padding against the inline size its containing block got from the last layout instead, so the box grows every time it measures it again.
    auto inlineMargins = [&](const RenderBox& box) {
        CheckedRef style = box.style();
        auto margin = [&](auto& margin) {
            return margin.isAuto() ? 0_lu : Style::evaluateMinimum<LayoutUnit>(margin, 0_lu, style->usedZoomForLength());
        };
        return std::pair { margin(style->marginStart(writingMode)), margin(style->marginEnd(writingMode)) };
    };
    auto outerInlineSizes = [&](const RenderBox& box, IntrinsicWidthConstraints contentSizes, std::optional<LayoutUnit> containingBlockContentBoxBlockSize) -> IntrinsicWidthConstraints {
        CheckedRef style = box.style();
        // The scrollbars of a scroll container take part in the sizes of its content (RenderBlockFlow::computeIntrinsicLogicalWidths).
        auto scrollbarInlineSize = LayoutUnit { box.intrinsicScrollbarLogicalWidthIncludingGutter() };
        contentSizes = { contentSizes.minimum + scrollbarInlineSize, contentSizes.maximum + scrollbarInlineSize };
        auto zoom = style->usedZoomForLength();
        auto borderAndPadding = computeBorderAndPadding(box, 0_lu).horizontalBorderAndPadding();
        auto verticalBorderAndPadding = computeBorderAndPadding(box, 0_lu).verticalBorderAndPadding();
        // A preferred aspect ratio gives the min-content and max-content sizes of the box from a definite block size, floored by the min-content
        // size of its content when its min inline size is auto (RenderBox::applyAutomaticContentBasedMinimumSize).
        auto aspectRatioBorderBoxInlineSize = BlockFormattingUtils::aspectRatioBorderBoxInlineSize(box, borderAndPadding, verticalBorderAndPadding, containingBlockContentBoxBlockSize, { }, writingMode);
        auto boxSizes = [&] {
            if (!aspectRatioBorderBoxInlineSize)
                return contentSizes;
            return BlockFormattingUtils::aspectRatioContentInlineSizes(box, std::max(0_lu, *aspectRatioBorderBoxInlineSize - borderAndPadding), contentSizes, borderAndPadding, writingMode);
        }();
        // A calc-size() resolves its calculation with the sizes of its basis, where percentages are cyclic (RenderBlock::logicalWidthContributionsForSize).
        auto calcSizeContentBoxSizes = [&](auto& size, IntrinsicWidthConstraints basisSizes) {
            auto calcSize = size.calcSize();
            auto resolve = [&](LayoutUnit contentBoxSize) {
                return BlockFormattingUtils::calcSizeBorderBoxSize(calcSize, style.get(), contentBoxSize + borderAndPadding, borderAndPadding, 0_lu) - borderAndPadding;
            };
            return IntrinsicWidthConstraints { resolve(basisSizes.minimum), resolve(basisSizes.maximum) };
        };
        // A fixed size, or a calc-size() without a percentage in its basis, which would make it behave like that percentage (css-values-5 §11).
        auto isFixed = [](auto& size) {
            return size.isFixed() || (size.isCalcSize() && size.isSpecified() && !size.calcSize().basisHasPercentage());
        };
        // A size under a min-content and under a max-content constraint: a fixed size, or the box's own for the sizing keywords and for auto,
        // which stretch behaves as here (css-sizing-4 §6.1), when they are the basis of a calc-size().
        // A stretch() sizes the margin box, whatever the box-sizing, its margins resolved against zero here (css-sizing-4 §3.1).
        auto stretchFunctionContentBoxSize = [&](LayoutUnit value) {
            auto [marginStart, marginEnd] = inlineMargins(box);
            return std::max(0_lu, value - marginStart - marginEnd - borderAndPadding);
        };
        auto contentBoxSizes = [&](auto& size, IntrinsicWidthConstraints ownSizes, std::optional<IntrinsicWidthConstraints> autoSizes) -> std::optional<IntrinsicWidthConstraints> {
            // A fit-content() clamps the stretch() of a fixed argument between the box's own sizes. A percentage argument is cyclic here, where
            // it is fit-content, and max-content as a maximum and min-content as a minimum (css-sizing-4 §5.2.1).
            if (size.isFitContentFunction()) {
                if (auto fixedArgument = size.fitContentArgument().tryFixed()) {
                    auto value = std::clamp(stretchFunctionContentBoxSize(LayoutUnit { fixedArgument->resolveZoom(zoom) }), ownSizes.minimum, ownSizes.maximum);
                    return IntrinsicWidthConstraints { value, value };
                }
                if constexpr (std::same_as<std::remove_cvref_t<decltype(size)>, Style::MaximumSize>)
                    return IntrinsicWidthConstraints { ownSizes.maximum, ownSizes.maximum };
                if constexpr (std::same_as<std::remove_cvref_t<decltype(size)>, Style::MinimumSize>)
                    return IntrinsicWidthConstraints { ownSizes.minimum, ownSizes.minimum };
                return ownSizes;
            }
            if (isFixed(size)) {
                auto value = Style::evaluate<LayoutUnit>(size, 0_lu, zoom);
                if (size.isStretchFunction())
                    value = stretchFunctionContentBoxSize(value);
                else
                    value = std::max(0_lu, style->boxSizing() == BoxSizing::BorderBox ? value - borderAndPadding : value);
                return IntrinsicWidthConstraints { value, value };
            }
            auto keywordSizes = [&] -> std::optional<IntrinsicWidthConstraints> {
                if (size.isMinContent())
                    return IntrinsicWidthConstraints { ownSizes.minimum, ownSizes.minimum };
                if (size.isMaxContent())
                    return IntrinsicWidthConstraints { ownSizes.maximum, ownSizes.maximum };
                if (size.isFitContent())
                    return ownSizes;
                return { };
            }();
            if (!size.isCalcSize() || size.isSpecified())
                return keywordSizes;
            return (keywordSizes ? keywordSizes : autoSizes).transform([&](auto basisSizes) {
                return calcSizeContentBoxSizes(size, basisSizes);
            });
        };
        // An inline size that is not fixed is the one the aspect ratio gives (RenderBlock::computePreferredLogicalWidths), the basis of a calc-size().
        auto& inlineSize = style->logicalWidth(writingMode);
        auto [minimum, maximum] = boxSizes;
        if (isFixed(inlineSize) || !aspectRatioBorderBoxInlineSize) {
            if (auto sizes = contentBoxSizes(inlineSize, boxSizes, boxSizes))
                std::tie(minimum, maximum) = std::pair { sizes->minimum, sizes->maximum };
        } else if (inlineSize.isCalcSize() && !inlineSize.isSpecified()) {
            auto sizes = calcSizeContentBoxSizes(inlineSize, boxSizes);
            std::tie(minimum, maximum) = std::pair { sizes.minimum, sizes.maximum };
        }
        // A preferred aspect ratio that gives the auto block size transfers the definite min and max block sizes (RenderBox::applyTransferredMinMaxSizesFromAspectRatio).
        // The sizing keywords of the min and max inline sizes are the box's own sizes with them, which an indefinite maximum does not cap
        // (css-sizing-4 §5.3, RenderBox::constrainIntrinsicLogicalWidthsByMinMax).
        auto transferredBoxSizes = boxSizes;
        if (!isFixed(inlineSize) && style->logicalHeight(writingMode).isAuto()) {
            if (auto limits = BlockFormattingUtils::aspectRatioInlineSizeLimits(box, borderAndPadding, verticalBorderAndPadding, containingBlockContentBoxBlockSize, { }, writingMode)) {
                auto transferredMinimum = std::max(0_lu, limits->first - borderAndPadding);
                auto transferredMaximum = limits->second == LayoutUnit::max() ? LayoutUnit::max() : std::max(0_lu, limits->second - borderAndPadding);
                minimum = std::clamp(minimum, transferredMinimum, transferredMaximum);
                maximum = std::clamp(maximum, transferredMinimum, transferredMaximum);
                transferredBoxSizes = { std::clamp(boxSizes.minimum, transferredMinimum, transferredMaximum), std::clamp(boxSizes.maximum, transferredMinimum, transferredMaximum) };
            }
        }
        if (auto sizes = contentBoxSizes(style->logicalMaxWidth(writingMode), transferredBoxSizes, { })) {
            minimum = std::min(minimum, sizes->minimum);
            maximum = std::min(maximum, sizes->maximum);
        }
        // The automatic minimum is zero, unless a preferred aspect ratio gives the inline size, where it is in the box's sizes already.
        auto automaticMinimumSizes = std::optional<IntrinsicWidthConstraints> { };
        if (!BlockFormattingUtils::hasAspectRatioAutomaticMinimumInlineSize(box, aspectRatioBorderBoxInlineSize, writingMode))
            automaticMinimumSizes = IntrinsicWidthConstraints { };
        if (auto sizes = contentBoxSizes(style->logicalMinWidth(writingMode), transferredBoxSizes, automaticMinimumSizes)) {
            minimum = std::max(minimum, sizes->minimum);
            maximum = std::max(maximum, sizes->maximum);
        }
        auto [marginStart, marginEnd] = inlineMargins(box);
        auto outerSpace = borderAndPadding + marginStart + marginEnd;
        return { minimum + outerSpace, maximum + outerSpace };
    };

    // Each block container collects the contributions of its children between its start and end. In the max-content inline size, the floats
    // among them go side by side, and so does a box after them that avoids floats (CSS 2.2 §9.5), with its content when it is inline, while
    // another box goes below them (RenderBlock::computeBlockIntrinsicLogicalWidths), an output or a legend outside a fieldset too.
    // The percentage block sizes in the content of a block container resolve against its content box block size, which comes down from the
    // block container being measured.
    struct BlockContainerSizes {
        CheckedRef<const RenderBox> blockContainer;
        std::optional<LayoutUnit> contentBoxBlockSize;
        IntrinsicWidthConstraints sizes { };
        LayoutUnit leftFloatsMaximum { };
        LayoutUnit rightFloatsMaximum { };
    };

    auto addContribution = [&](BlockContainerSizes& container, const RenderBox& box, IntrinsicWidthConstraints contribution) {
        auto avoidsFloats = BlockFormattingUtils::establishesIndependentFormattingContext(box) || (box.isAnonymousBlock() && box.childrenInline());
        auto& sizes = container.sizes;
        if (box.isFloating() || avoidsFloats) {
            auto floatsMaximum = container.leftFloatsMaximum + container.rightFloatsMaximum;
            auto clear = box.usedStyle().clear();
            if (clear == UsedClear::Left || clear == UsedClear::Both) {
                sizes.maximum = std::max(sizes.maximum, floatsMaximum);
                container.leftFloatsMaximum = { };
            }
            if (clear == UsedClear::Right || clear == UsedClear::Both) {
                sizes.maximum = std::max(sizes.maximum, floatsMaximum);
                container.rightFloatsMaximum = { };
            }
        }
        sizes.minimum = std::max(sizes.minimum, contribution.minimum);
        if (box.isFloating()) {
            (box.usedStyle().floating() == UsedFloat::Left ? container.leftFloatsMaximum : container.rightFloatsMaximum) += contribution.maximum;
            return;
        }
        auto maximum = contribution.maximum;
        if (avoidsFloats) {
            // A float may sit in a positive margin, and a negative one overlaps it. The margins go on the sides the direction of the containing
            // block of the block container being measured gives them, as on the render tree path.
            auto [marginStart, marginEnd] = inlineMargins(box);
            CheckedPtr containingBlock = container.blockContainer->containingBlock();
            auto isLeftToRight = !containingBlock || containingBlock->writingMode().isLogicalLeftInlineStart();
            auto [marginLeft, marginRight] = isLeftToRight ? std::pair { marginStart, marginEnd } : std::pair { marginEnd, marginStart };
            auto besideFloats = [](LayoutUnit floatsMaximum, LayoutUnit margin) {
                return margin > 0 ? std::max(floatsMaximum, margin) : floatsMaximum + margin;
            };
            maximum = contribution.maximum - marginStart - marginEnd + besideFloats(container.leftFloatsMaximum, marginLeft) + besideFloats(container.rightFloatsMaximum, marginRight);
            maximum = std::max(maximum, container.leftFloatsMaximum + container.rightFloatsMaximum);
        } else
            sizes.maximum = std::max(sizes.maximum, container.leftFloatsMaximum + container.rightFloatsMaximum);
        container.leftFloatsMaximum = { };
        container.rightFloatsMaximum = { };
        sizes.maximum = std::max(sizes.maximum, maximum);
    };
    auto contentSizes = [](BlockContainerSizes&& container) {
        auto sizes = container.sizes;
        sizes.maximum = std::max(sizes.maximum, container.leftFloatsMaximum + container.rightFloatsMaximum);
        return sizes;
    };
    auto withMargins = [&](const RenderBox& box, IntrinsicWidthConstraints contributions) -> IntrinsicWidthConstraints {
        auto [marginStart, marginEnd] = inlineMargins(box);
        return { contributions.minimum + marginStart + marginEnd, contributions.maximum + marginStart + marginEnd };
    };

    auto blockContainerSizes = Vector<BlockContainerSizes> { { blockContainer, contentBoxBlockSize } };
    auto contentBoxBlockSizeOf = [&](const RenderBox& box) {
        return contentBoxBlockSizeWhileMeasured(box, computeBorderAndPadding(box, 0_lu), blockContainerSizes.last().contentBoxBlockSize, { }, writingMode);
    };
    for (auto& blockItem : BlockItemsBuilder { blockContainer }.build()) {
        CheckedRef box = blockItem.renderer();
        switch (blockItem.type()) {
        case BlockItem::Type::BlockContainerStart:
            blockContainerSizes.append({ box, contentBoxBlockSizeOf(box) });
            break;
        case BlockItem::Type::InlineContent: {
            auto inlineContentSizes = LayoutIntegration::BlockIntegrationUtils::inlineContentIntrinsicInlineSizes(downcast<RenderBlockFlow>(box));
            auto& sizes = blockContainerSizes.last().sizes;
            sizes.minimum = std::max(sizes.minimum, inlineContentSizes.minimum);
            sizes.maximum = std::max(sizes.maximum, inlineContentSizes.maximum);
            break;
        }
        case BlockItem::Type::BlockContainerEnd: {
            auto sizes = contentSizes(blockContainerSizes.takeLast());
            if (auto containedSizes = containedIntrinsicInlineSizes(box))
                sizes = *containedSizes;
            addContribution(blockContainerSizes.last(), box, outerInlineSizes(box, sizes, blockContainerSizes.last().contentBoxBlockSize));
            break;
        }
        case BlockItem::Type::IndependentFormattingContextRoot:
            if (LayoutIntegration::BlockIntegrationUtils::laysOutThroughRenderTree(box)) {
                // Its own formatting context measures a box it lays out, against the size of the containing block only where coverage keeps
                // it out (canComputeContentInlineSizes).
                addContribution(blockContainerSizes.last(), box, withMargins(box, LayoutIntegration::BlockIntegrationUtils::independentFormattingContextIntrinsicInlineSizeContributions(box)));
                break;
            }
            addContribution(blockContainerSizes.last(), box, outerInlineSizes(box, computeIntrinsicInlineSizes(downcast<RenderBlockFlow>(box), contentBoxBlockSizeOf(box)), blockContainerSizes.last().contentBoxBlockSize));
            break;
        case BlockItem::Type::Float:
            // Its own formatting context lays it out, and measures it (handleFloat).
            addContribution(blockContainerSizes.last(), box, withMargins(box, LayoutIntegration::BlockIntegrationUtils::independentFormattingContextIntrinsicInlineSizeContributions(box)));
            break;
        case BlockItem::Type::OutOfFlowBox:
            // Out-of-flow boxes do not contribute to the intrinsic sizes of their parent (css-sizing-3 §5.2).
            break;
        case BlockItem::Type::ColumnSpannerPlaceholder:
            // The multi-column container measures the spanner, and the floats before the placeholder do not go beside the ones after it, which it
            // clears (RenderMultiColumnSpannerPlaceholder::createAnonymous).
            addContribution(blockContainerSizes.last(), box, { });
            break;
        }
    }
    ASSERT(blockContainerSizes.size() == 1);
    auto sizes = contentSizes(blockContainerSizes.takeLast());
    sizes.minimum = std::max(0_lu, sizes.minimum);
    sizes.maximum = std::max(sizes.minimum, sizes.maximum);
    return sizes;
}

void BlockFormattingContext::computeLogicalLeft(const RenderBox& box, const ContainingBlockState& containingBlockState, BoxGeometry& geometry) const
{
    // The box is placed from the inline start of its containing block, so an over-constrained box ignores its end margin (CSS 2.2 §10.3.3),
    // and justify-self moves it in the space its margin box leaves.
    CheckedRef containingBlockStyle = containingBlockState.blockContainer->style();
    auto justifySelfOffset = BlockFormattingUtils::justifySelfOffset(box, geometry.borderBoxWidth(), geometry.marginStart(), geometry.marginEnd(), containingBlockState.inlineRange.size(), containingBlockStyle);
    geometry.setLeft(containingBlockState.inlineRange.min() + geometry.marginStart() + justifySelfOffset);
}

// The containing block's definite content box block size less the margins, where a margin that collapses with the containing block's, which has no
// border or padding on that side and is not a block formatting context root, counts as zero (css-sizing-4 §6.1).
StretchFitBorderBoxBlockSizes BlockFormattingContext::stretchFitBorderBoxBlockSizes(const BoxGeometry& geometry, const ContainingBlockState& containingBlockState) const
{
    auto sizes = StretchFitBorderBoxBlockSizes { .fillAvailable = containingBlockState.fillAvailableBlockSize, .stretchFunctionMargins = geometry.marginBefore() + geometry.marginAfter() };
    auto containingBlockContentBoxBlockSize = stretchFitBasis(containingBlockState);
    if (!containingBlockContentBoxBlockSize)
        return sizes;
    auto marginBefore = geometry.marginBefore();
    auto marginAfter = geometry.marginAfter();
    // The margins of a block-level box in inline content stay (RenderBox's marginForStretchInParentBlockAxis).
    CheckedPtr containingBlock = dynamicDowncast<RenderBlockFlow>(containingBlockState.blockContainer);
    if (containingBlock && !containingBlock->createsNewFormattingContext() && !containingBlock->childrenInline()) {
        auto& containingBlockGeometry = containingBlockState.geometry;
        if (!containingBlockGeometry.borderAndPaddingBefore())
            marginBefore = { };
        if (!containingBlockGeometry.borderAndPaddingAfter())
            marginAfter = { };
    }
    sizes.stretch = std::max(0_lu, *containingBlockContentBoxBlockSize - marginBefore - marginAfter);
    return sizes;
}

// A box that continues from the fragmentainers before gets what its block size leaves after them (css-break-3 §4.1), with all of its block-start
// and block-end border and padding when it clones them (css-break-3 §5.4).
void BlockFormattingContext::computeBlockSizeAfterBreaks(const RenderBox& box, const ContainingBlockState& boxState, LayoutUnit intrinsicContentBlockSize, const ContainingBlockState& containingBlockState, BoxGeometry& geometry) const
{
    auto& consumedBlockSize = boxState.consumedBlockSize;
    if (!consumedBlockSize || boxState.isLaidOutThroughRenderTree)
        return;
    auto wholeGeometry = geometry;
    if (consumedBlockSize->clonesDecorations) {
        computeBlockSize(box, consumedBlockSize->contentBox + intrinsicContentBlockSize, containingBlockState, wholeGeometry);
        geometry.setContentBoxHeight(std::max(0_lu, clonedFragmentBlockSize(box, wholeGeometry, *consumedBlockSize) - geometry.verticalBorderAndPadding() - geometry.verticalSpaceForScrollbar()));
        return;
    }
    wholeGeometry.setVerticalPadding({ consumedBlockSize->borderAndPaddingBefore, geometry.paddingAfter() });
    computeBlockSize(box, consumedBlockSize->contentBox + intrinsicContentBlockSize, containingBlockState, wholeGeometry);
    auto fragmentBlockSize = std::max(0_lu, wholeGeometry.borderBoxHeight() - consumedBlockSize->borderBox);
    sliceBlockEndAfterBreak(geometry, fragmentBlockSize);
    geometry.setContentBoxHeight(std::max(0_lu, fragmentBlockSize - geometry.verticalBorderAndPadding() - geometry.verticalSpaceForScrollbar()));
}

void BlockFormattingContext::computeBlockSize(const RenderBox& box, LayoutUnit intrinsicContentBlockSize, const ContainingBlockState& containingBlockState, BoxGeometry& geometry) const
{
    // A box with size containment is sized as if it had no content, by contain-intrinsic-block-size (css-contain-2 §3.2,
    // RenderBox::overrideLogicalHeightForSizeContainment).
    if (box.shouldApplySizeContainment())
        intrinsicContentBlockSize = box.explicitIntrinsicInnerLogicalHeight().value_or(0_lu);
    auto borderAndPaddingBlockSize = geometry.verticalBorderAndPadding();
    // An auto block size takes the scrollbar in the block axis too (RenderBlockFlow::layoutBlock).
    auto scrollbarBlockSize = geometry.verticalSpaceForScrollbar();
    auto blockSize = BlockFormattingUtils::borderBoxBlockSize(box, borderAndPaddingBlockSize, intrinsicContentBlockSize + scrollbarBlockSize, percentageBasis(box, containingBlockState), stretchFitBorderBoxBlockSizes(geometry, containingBlockState), root().writingMode(), containingBlockIsTableCell(containingBlockState));
    if (auto aspectRatioBlockSize = BlockFormattingUtils::aspectRatioBorderBoxBlockSize(box, geometry.borderBoxWidth(), geometry.horizontalBorderAndPadding(), borderAndPaddingBlockSize, containingBlockState.contentBoxBlockSize, stretchFitBorderBoxBlockSizes(geometry, containingBlockState), root().writingMode(), intrinsicContentBlockSize))
        blockSize = *aspectRatioBlockSize;
    setContentBoxBlockSize(geometry, blockSize);
}

ConstraintsForInlineContent BlockFormattingContext::constraintsForInlineContent(const RenderBox& blockContainer, const ContainingBlockState& blockContainerState) const
{
    // Like the line boxes the inline formatting context produces, these are relative to the border box of the block container, in its own direction.
    auto& geometry = blockContainerState.geometry;
    auto horizontalConstraints = HorizontalConstraints { blockContainerState.inlineRange.min(), blockContainerState.inlineRange.size() };
    auto visualLeft = blockContainer.writingMode().isLogicalLeftInlineStart() ? geometry.borderAndPaddingStart() : geometry.borderAndPaddingEnd();
    // The vertical scrollbar of a scroll container sits between its left border and padding (LayoutIntegration::BoxGeometryUpdater::formattingContextConstraints),
    // which is in the block axis in vertical writing modes, and so does one of the gutters with scrollbar-gutter: stable both-edges.
    if (blockContainer.style().scrollbarGutter().isStableBothEdges())
        visualLeft += geometry.horizontalSpaceForScrollbar() / 2;
    else if (blockContainer.shouldPlaceVerticalScrollbarOnLeft() && blockContainer.writingMode().isHorizontal())
        visualLeft += geometry.horizontalSpaceForScrollbar();
    // The physical size of the border box, of which the inline formatting context only needs the inline size (InlineDisplayLineBuilder's
    // mapLineRectLogicalToVisual).
    auto borderBoxSize = blockContainer.writingMode().isHorizontal() ? LayoutSize { geometry.borderBoxWidth(), 0_lu } : LayoutSize { 0_lu, geometry.borderBoxWidth() };
    return { { horizontalConstraints, blockContainerState.borderAndPaddingBefore }, visualLeft, borderBoxSize };
}

ContainingBlockState BlockFormattingContext::containingBlockStateAtBlockStart(const RenderBox& box) const
{
    auto& containingBlockState = m_blockLayoutState.containingBlockStates().last();
    auto geometry = computeBorderAndPadding(box, containingBlockState.inlineRange.size());
    computeInlineSizeAndMargins(box, containingBlockState, geometry);
    // The margins adjoining an unforced break are truncated (css-break-3 §5.2).
    if (m_marginsAdjoinUnforcedBreak)
        geometry.setVerticalMargin({ { }, geometry.marginAfter() });
    computeLogicalLeft(box, containingBlockState, geometry);
    return containingBlockStateFor(box, geometry, containingBlockState);
}

// Quirks mode: body is at least as tall as the viewport leaves it inside html (RenderBox::computeLogicalHeight).
// The available inline space of an orthogonal box, the block size of its containing block (css-writing-modes-4 §7.3.1). The initial
// containing block's size stands in for an indefinite one, and caps it when it is not auto, or in quirks mode, where an auto one is the
// available block size of the containing block like for percentages (RenderBox::perpendicularContainingBlockLogicalHeight).
LayoutUnit BlockFormattingContext::orthogonalFlowAvailableInlineSpace(const ContainingBlockState& containingBlockState) const
{
    CheckedRef containingBlock = containingBlockState.blockContainer;
    CheckedRef style = containingBlock->style();
    auto writingMode = root().writingMode();
    auto& blockSize = style->logicalHeight(writingMode);
    if (blockSize.isFixed())
        return containingBlockState.fillAvailableBlockSize;
    // The size the parent formatting context gives the root, like the render tree's overriding size.
    if (containingBlock.ptr() == &root() && root().overridingBorderBoxLogicalHeight())
        return containingBlockState.contentBoxBlockSize.value_or(containingBlockState.fillAvailableBlockSize);
    auto viewSize = root().view().frameView().layoutSize();
    auto initialContainingBlockSize = LayoutUnit { writingMode.isHorizontal() ? viewSize.height() : viewSize.width() };
    auto& maximumBlockSize = style->logicalMaxHeight(writingMode);
    // A multi-column flow gets its block size from its columns (RenderBox::perpendicularContainingBlockLogicalHeight).
    auto isIndefinite = blockSize.isAuto() && (maximumBlockSize.isNone() || maximumBlockSize.isMaxContent()) && !containingBlock->document().inQuirksMode() && !style->aspectRatio().hasRatio() && !is<RenderMultiColumnFlow>(containingBlock);
    if (isIndefinite)
        return initialContainingBlockSize;
    // A preferred aspect ratio gives an auto block size from the inline size, in quirks mode too.
    auto availableBlockSize = containingBlockState.fillAvailableBlockSize;
    if (style->aspectRatio().hasRatio()) {
        auto& geometry = containingBlockState.geometry;
        auto borderBoxBlockSize = (blockSize.isAuto() || blockSize.isIntrinsic()) ? BlockFormattingUtils::aspectRatioBorderBoxBlockSize(containingBlock, geometry.borderBoxWidth(), geometry.horizontalBorderAndPadding(), geometry.verticalBorderAndPadding(), { }, { }, writingMode) : std::nullopt;
        if (auto contentBoxBlockSize = borderBoxBlockSize ? std::optional { *borderBoxBlockSize - geometry.verticalBorderAndPadding() } : containingBlockState.contentBoxBlockSize)
            availableBlockSize = *contentBoxBlockSize;
    }
    return std::min(availableBlockSize, initialContainingBlockSize);
}

// Quirks mode: the root element and the body stretch to the viewport less their margins and the margins, borders and padding of their containing
// block, the view, which has none, or the root element (RenderBox::computeLogicalHeight). Those of the root element are in its geometry before
// its renderer has them.
LayoutUnit BlockFormattingContext::viewportStretchedBorderBoxBlockSize(LayoutUnit collapsedMarginBefore, LayoutUnit collapsedMarginAfter, const ContainingBlockState& containingBlockState) const
{
    auto& containingBlock = containingBlockState.geometry;
    auto spaceAround = collapsedMarginBefore + collapsedMarginAfter + containingBlock.marginBefore() + containingBlock.marginAfter() + containingBlock.verticalBorderAndPadding();
    CheckedRef view = root().view();
    return view->pageOrViewLogicalHeight() - spaceAround;
}

// A box with clear whose margins collapse through it, like RenderBlockFlow::clearFloatsIfNeeded: it goes to the bottom of the floats, the
// margins before it give the containing blocks waiting for their top theirs, and its margins collapse with the ones after it from there,
// though not with its containing block's block-end margin (CSS 2.2 §8.3.1, §9.5.2).
void BlockFormattingContext::placeCollapsedThroughBoxWithClearance(const RenderBox& box, ContainingBlockState&& blockContainerState, std::span<const ContainingBlockState* const> statesWaitingForTop, const BorderBoxLogicalTops& tops, BlockLayoutResult& result)
{
    auto& containingBlockState = m_blockLayoutState.containingBlockStates().last();
    auto floatsLogicalBottom = tops.borderBoxLogicalTops.first();
    if (!containingBlockState.borderBoxLogicalTop) {
        m_blockLayoutState.marginCollapse().endQuirkContainerBlockStartSide(containingBlockState);
        auto index = size_t { 1 };
        for (auto& state : m_blockLayoutState.containingBlockStates() | std::views::reverse) {
            if (state.borderBoxLogicalTop)
                break;
            state.borderBoxLogicalTop = tops.borderBoxLogicalTops[index];
            if (tops.hasClearance[index])
                BlockMarginCollapse::separateFromContainingBlockMarginBefore(state);
            state.clearance = { };
            ++index;
        }
    }
    placePendingFloats(statesWaitingForTop, tops);
    placePendingFloats(*containingBlockState.borderBoxLogicalTop);

    auto& geometry = blockContainerState.geometry;
    computeBlockSize(box, { }, containingBlockState, geometry);
    geometry.setTop(floatsLogicalBottom - *containingBlockState.borderBoxLogicalTop);
    auto margins = blockContainerState.margins.collapsedMarginsBefore();
    margins.add(geometry.marginAfter());
    // They start above it by its collapsed block-start margin, which the clearance includes.
    m_blockLayoutState.marginCollapse().resetAdjoiningMargins(floatsLogicalBottom - blockContainerState.margins.collapsedMarginsBefore().collapsedValue(), margins);
    containingBlockState.endsWithClearedBoxCollapsedThrough = true;
    auto collapsedMarginsAfter = AdjoiningMargins { };
    collapsedMarginsAfter.add(geometry.marginAfter());
    result.boxes.append({ box, WTF::move(geometry), { }, blockContainerState.margins.collapsedMarginsBefore(), collapsedMarginsAfter, blockContainerState.margins.collapsedMarginsBeforeAreQuirky(), box.style().marginAfter().hasQuirk(), false });
}

void BlockFormattingContext::placeInContainingBlock(const RenderBox& box, ContainingBlockState&& blockContainerState, BlockLayoutResult& result)
{
    auto& containingBlockState = m_blockLayoutState.containingBlockStates().last();
    // A box is placed once its containing block's top is known, and the result is relative to that top, not to the root's.
    ASSERT(containingBlockState.borderBoxLogicalTop || !blockContainerState.borderBoxLogicalTop);
    auto& geometry = blockContainerState.geometry;
    auto& collapsedMarginsBefore = blockContainerState.margins.collapsedMarginsBefore();

    if (!blockContainerState.borderBoxLogicalTop && blockContainerState.clearance) {
        // With the containing blocks waiting for their top, which may have clearance of their own.
        auto statesWaitingForTop = this->statesWaitingForTop();
        statesWaitingForTop.insert(0, &blockContainerState);
        auto tops = borderBoxLogicalTopsWaitingForTop(statesWaitingForTop, m_blockLayoutState.marginCollapse().collapsedLogicalBottomForCollapsedThroughBox());
        if (tops.hasClearance.first())
            return placeCollapsedThroughBoxWithClearance(box, WTF::move(blockContainerState), statesWaitingForTop, tops, result);
    }
    if (!blockContainerState.borderBoxLogicalTop) {
        // Its margin-trim at the block-end edge discards the margins of its content, which all collapse with its last child's block-end margin.
        if (auto& margins = blockContainerState.adjoiningMarginsAtContentStart)
            m_blockLayoutState.marginCollapse().resetAdjoiningMargins(m_blockLayoutState.marginCollapse().logicalTop(), *margins);
        // The margins collapse through the box, which has no height and leaves them open. Its top border edge is its parent's when they
        // include the parent's block-start margin, and otherwise where it would be with a non-zero bottom border (CSS 2.2 §8.3.1).
        computeBlockSize(box, { }, containingBlockState, geometry);
        ASSERT(!geometry.borderBoxHeight());
        if (containingBlockState.borderBoxLogicalTop) {
            auto borderBoxLogicalTop = m_blockLayoutState.marginCollapse().collapsedLogicalBottomForCollapsedThroughBox();
            geometry.setTop(borderBoxLogicalTop - *containingBlockState.borderBoxLogicalTop);
            // The floats waiting for a top are in the box or in the boxes the margins collapsed through inside it.
            placePendingFloats(borderBoxLogicalTop);
        } else
            geometry.setTop({ });
        m_blockLayoutState.marginCollapse().addCollapsedThroughBlockEndMargin(blockContainerState, containingBlockState, geometry.marginAfter());
        auto collapsedMarginsAfter = AdjoiningMargins { };
        collapsedMarginsAfter.add(geometry.marginAfter());
        result.boxes.append({ box, WTF::move(geometry), { }, collapsedMarginsBefore, collapsedMarginsAfter, blockContainerState.margins.collapsedMarginsBeforeAreQuirky(), box.style().marginAfter().hasQuirk(), false });
        containingBlockState.blockEndChildren.collapsedThroughChildren.append(result.boxes.size() - 1);
        return;
    }

    containingBlockState.endsWithClearedBoxCollapsedThrough = false;
    // The content ends at the bottom border edge of the last in-flow child whose margins do not collapse with the box's block-end margin,
    // or below the collapsed margin after it when that margin does not collapse with the box's (CSS 2.2 §10.6.3).
    CheckedRef style = box.style();
    auto writingMode = root().writingMode();
    // A box that is not a block container establishes a formatting context of its own.
    CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(box);
    auto establishesBlockFormattingContext = [&] {
        return !blockContainer || blockContainer->createsNewFormattingContext();
    };
    // Its margin-trim discards the margins at the block-end edge of its content, its last child's block-end margin and the ones that collapse
    // with it (css-box-4 §3.3.1).
    auto trimsMarginsAtBlockEnd = !establishesBlockFormattingContext() && style->marginTrim().contains(Style::MarginTrimSide::BlockEnd);
    if (trimsMarginsAtBlockEnd) {
        m_blockLayoutState.marginCollapse().resetAdjoiningMargins(m_blockLayoutState.marginCollapse().logicalTop());
        placeChildrenAtTrimmedBlockEnd(WTF::move(blockContainerState.blockEndChildren), result);
    }
    auto marginAfterCollapsesWithChildren = !establishesBlockFormattingContext() && !geometry.borderAndPaddingAfter() && BlockFormattingUtils::blockSizeBehavesAsAuto(style->logicalHeight(writingMode), containingBlockState.contentBoxBlockSize)
        && !blockContainerState.endsWithClearedBoxCollapsedThrough;
    // A quirk container leaves out a quirky margin after its last child (RenderBlockFlow::handleAfterSideOfBlock).
    auto contentBoxLogicalBottom = marginAfterCollapsesWithChildren || (BlockMarginCollapse::isQuirkContainer(box) && m_blockLayoutState.marginCollapse().endsWithQuirk()) ? m_blockLayoutState.marginCollapse().logicalTop() : m_blockLayoutState.marginCollapse().collapsedLogicalBottom();
    // Negative margins may pull that above the content box top.
    auto intrinsicContentBlockSize = std::max(0_lu, contentBoxLogicalBottom - blockContainerState.contentBoxLogicalTop());
    // A box laid out through the render tree has its block size from its own formatting context already.
    if (!blockContainerState.isLaidOutThroughRenderTree)
        computeBlockSize(box, intrinsicContentBlockSize, containingBlockState, geometry);
    computeBlockSizeAfterBreaks(box, blockContainerState, intrinsicContentBlockSize, containingBlockState, geometry);
    if (box.stretchesToViewport()) {
        auto collapsedMarginAfter = geometry.marginAfter();
        if (marginAfterCollapsesWithChildren) {
            auto margins = m_blockLayoutState.marginCollapse().adjoiningMargins();
            margins.add(geometry.marginAfter());
            collapsedMarginAfter = margins.collapsedValue();
        }
        auto stretchedBorderBoxBlockSize = viewportStretchedBorderBoxBlockSize(collapsedMarginsBefore.collapsedValue(), collapsedMarginAfter, containingBlockState);
        // A fragment after the first gets what the stretched block size leaves after the fragmentainers before.
        if (auto& consumedBlockSize = blockContainerState.consumedBlockSize)
            stretchedBorderBoxBlockSize -= consumedBlockSize->borderBox;
        geometry.setContentBoxHeight(std::max(geometry.contentBoxHeight(), stretchedBorderBoxBlockSize - geometry.verticalBorderAndPadding()));
    }

    // WebKit compatibility (RenderBlockFlow::layoutBlock): the block-end margin still collapses with the last child's when min-height is
    // not zero, unless it made the box taller than its content.
    auto minimumBlockSizeSeparatesMargins = style->logicalHeight(writingMode).isAuto() && !style->logicalMinHeight(writingMode).isPossiblyZero() && geometry.contentBoxHeight() > intrinsicContentBlockSize;
    // What follows is placed below the bottom border edge, also when min/max-height moved that away from where the content ends.
    auto borderBoxLogicalTop = *blockContainerState.borderBoxLogicalTop;
    auto marginAfterIsQuirky = m_blockLayoutState.marginCollapse().addBlockEndMargin(blockContainerState, geometry.marginAfter(), style->marginAfter().hasQuirk(), marginAfterCollapsesWithChildren && !minimumBlockSizeSeparatesMargins, borderBoxLogicalTop + geometry.borderBoxHeight());

    geometry.setTop(borderBoxLogicalTop - *containingBlockState.borderBoxLogicalTop);
    result.boxes.append({ box, WTF::move(geometry), intrinsicContentBlockSize, collapsedMarginsBefore, m_blockLayoutState.marginCollapse().adjoiningMargins(), blockContainerState.margins.collapsedMarginsBeforeAreQuirky(), marginAfterIsQuirky, blockContainerState.isLaidOutThroughRenderTree });
    auto boxIndex = result.boxes.size() - 1;
    containingBlockState.blockEndChildren = { { }, boxIndex, containingBlockState.blockEndChildren.contentBoxLogicalTop };
    // Its block-end margin collapses with the margins at the block-end edge of its content, which margin-trim at the block-end edge of an ancestor
    // discards too.
    auto ancestorTrimsMarginsAtBlockEnd = std::ranges::any_of(m_blockLayoutState.containingBlockStates(), [](auto& state) {
        return state.blockContainer->style().marginTrim().contains(Style::MarginTrimSide::BlockEnd);
    });
    if (marginAfterCollapsesWithChildren && !trimsMarginsAtBlockEnd && ancestorTrimsMarginsAtBlockEnd)
        m_blockLayoutState.blockEndChildren().append({ boxIndex, WTF::move(blockContainerState.blockEndChildren) });
}

// margin-trim at the block-end edge of a block container discards the margins that placed the children whose margins collapse through them
// at the end of its content, also in its last child when its block-end margin collapses with them: they go to the bottom border edge of the
// child before them (CSS 2.2 §8.3.1, css-box-4 §3.3.1).
void BlockFormattingContext::placeChildrenAtTrimmedBlockEnd(BlockEndChildren&& children, BlockLayoutResult& result)
{
    auto logicalTop = children.contentBoxLogicalTop;
    if (children.lastChild) {
        auto& lastChildGeometry = result.boxes[*children.lastChild].geometry;
        logicalTop = BoxGeometry::borderBoxTop(lastChildGeometry) + lastChildGeometry.borderBoxHeight();
    }
    for (auto index : children.collapsedThroughChildren) {
        auto& box = result.boxes[index];
        moveFloatsInside(box.renderer, logicalTop - BoxGeometry::borderBoxTop(box.geometry));
        box.geometry.setTop(logicalTop);
    }
    if (!children.lastChild)
        return;
    auto& blockEndChildren = m_blockLayoutState.blockEndChildren();
    auto lastChildEntry = blockEndChildren.findIf([&](auto& entry) {
        return entry.first == *children.lastChild;
    });
    if (lastChildEntry == notFound)
        return;
    auto lastChildChildren = WTF::move(blockEndChildren[lastChildEntry].second);
    blockEndChildren.removeAt(lastChildEntry);
    placeChildrenAtTrimmedBlockEnd(WTF::move(lastChildChildren), result);
}

// The floats in a box that moves in the block axis go with it, where the content after it avoids them (placeChildrenAtTrimmedBlockEnd).
void BlockFormattingContext::moveFloatsInside(const RenderBox& box, LayoutUnit blockOffset)
{
    auto* placedFloats = m_blockLayoutState.placedFloats();
    if (!blockOffset || !placedFloats)
        return;
    auto floats = PlacedFloats { root().layoutBox() };
    for (auto& floatItem : placedFloats->list()) {
        CheckedPtr floatBox = floatItem.layoutBox();
        if (floatBox && floatBox->rendererForIntegration()->isDescendantOf(&box))
            floats.add(movedFloat(floatItem, { 0_lu, blockOffset }, floatBox.get()));
        else
            floats.add(floatItem);
    }
    m_blockLayoutState.ensurePlacedFloats(root().layoutBox()) = WTF::move(floats);
}

// The block container separates the adjoining margins from the margins that follow (CSS 2.2 §8.3.1), so they collapse into one margin. It
// ends at the top border edge of the block container and of every block container still waiting for its top (which have no block-start
// border or padding, so their margins were part of it). The margins that follow start at the block container's content box top.
void BlockFormattingContext::resolveBorderBoxLogicalTops(ContainingBlockState& blockContainerState)
{
    ASSERT(!blockContainerState.borderBoxLogicalTop);
    m_blockLayoutState.marginCollapse().endQuirkContainerBlockStartSide(blockContainerState);
    auto borderBoxLogicalTop = m_blockLayoutState.marginCollapse().collapsedLogicalBottom();
    // The block containers waiting for their top, from the innermost one. It is still on the stack when its line boxes separate the margins.
    auto statesWaitingForTop = Vector<ContainingBlockState*> { };
    auto& containingBlockStates = m_blockLayoutState.containingBlockStates();
    if (containingBlockStates.isEmpty() || &containingBlockStates.last() != &blockContainerState)
        statesWaitingForTop.append(&blockContainerState);
    for (auto& containingBlockState : containingBlockStates | std::views::reverse) {
        if (containingBlockState.borderBoxLogicalTop)
            break;
        statesWaitingForTop.append(&containingBlockState);
    }
    auto constStatesWaitingForTop = statesWaitingForTop.map([](auto* state) -> const ContainingBlockState* {
        return state;
    });
    auto tops = borderBoxLogicalTopsWaitingForTop(constStatesWaitingForTop, borderBoxLogicalTop);
    for (size_t index = 0; index < statesWaitingForTop.size(); ++index) {
        auto& state = *statesWaitingForTop[index];
        state.borderBoxLogicalTop = tops.borderBoxLogicalTops[index];
        // Its margins no longer collapse with the ones before them.
        if (tops.hasClearance[index])
            BlockMarginCollapse::separateFromContainingBlockMarginBefore(state);
        state.clearance = { };
    }
    m_blockLayoutState.marginCollapse().resetAdjoiningMargins(blockContainerState.contentBoxLogicalTop());
    placePendingFloats(constStatesWaitingForTop, tops);
}

// The floats waiting for a top are in block containers whose margins collapsed through them into these, and go with the innermost one around
// them.
void BlockFormattingContext::placePendingFloats(std::span<const ContainingBlockState* const> statesWaitingForTop, const BorderBoxLogicalTops& tops)
{
    for (size_t index = 0; index < statesWaitingForTop.size(); ++index)
        placePendingFloats(tops.borderBoxLogicalTops[index], statesWaitingForTop[index]->blockContainer.ptr());
}

void BlockFormattingContext::placePendingFloats(LayoutUnit borderBoxLogicalTop, const RenderBox* blockContainer)
{
    auto& pendingFloats = m_blockLayoutState.pendingFloats();
    if (pendingFloats.isEmpty())
        return;
    auto& placedFloats = m_blockLayoutState.ensurePlacedFloats(root().layoutBox());
    auto floats = placedFloats.list();
    auto didMoveFloats = false;
    // Only the ones in the block container and inside it when there is one.
    auto pendingFloatsToPlace = Vector<BlockLayoutState::PendingFloats> { };
    pendingFloats.removeAllMatching([&](auto& pending) {
        if (blockContainer && pending.blockContainer.ptr() != blockContainer && !pending.blockContainer->isDescendantOf(blockContainer))
            return false;
        pendingFloatsToPlace.append(WTF::move(pending));
        return true;
    });
    for (auto& pending : pendingFloatsToPlace) {
        auto offset = LayoutSize { 0_lu, borderBoxLogicalTop - pending.provisionalBorderBoxLogicalTop };
        if (offset.isZero())
            continue;
        auto resolvedBorderBoxLogicalTop = m_resolvedBorderBoxLogicalTops.findIf([&](auto& entry) {
            return entry.first.ptr() == pending.blockContainer.ptr();
        });
        if (resolvedBorderBoxLogicalTop == notFound) {
            m_resolvedBorderBoxLogicalTops.append({ pending.blockContainer, borderBoxLogicalTop });
            // The content of a float that breaks across the fragmentainers broke from the provisional top.
            auto hasBrokenFloatContent = pending.floatBoxes.containsIf([&](auto& floatBox) {
                auto* content = floatContent(floatBox);
                return (content && content->content.breaks) || hasContentFragmentedByRenderTree(downcast<RenderBox>(*floatBox->rendererForIntegration()), m_fragmentainer);
            });
            m_needsLayoutWithResolvedBorderBoxLogicalTops |= pending.isPlacedAgainstOtherFloats || pending.isPlacedAgainstOtherPendingFloats || hasBrokenFloatContent;
        }
        for (auto& floatItem : floats) {
            auto isPending = pending.floatBoxes.containsIf([&](auto& floatBox) {
                return floatBox.ptr() == floatItem.layoutBox();
            });
            if (isPending)
                floatItem = movedFloat(floatItem, offset, floatItem.layoutBox());
        }
        didMoveFloats = true;
    }
    if (didMoveFloats) {
        placedFloats.clear();
        for (auto& floatItem : floats)
            placedFloats.add(WTF::move(floatItem));
    }
    for (auto& pending : pendingFloatsToPlace)
        pushFloatsThatDoNotFit(pending.floatBoxes);
}

// A float that does not fit in the fragmentainer goes to the start of the next one, unless it is at the start of this one or taller than one,
// and so do the floats after it, which go no higher (CSS 2.2 §9.5.1 rule 5, RenderBlockFlow::positionNewFloats moves the margin box of an
// unsplittable float to the next page).
bool BlockFormattingContext::pushesFloat(const PlacedFloats::Item& floatItem) const
{
    if (!m_fragmentainer || !breaksInside(m_blockLayoutState.containingBlockStates().span().subspan(1)))
        return false;
    if (!m_pushedFloats.isEmpty())
        return true;
    // Not one with only the block-start border and padding of the boxes around it, or other floats, above it in the fragmentainer, like
    // monolithic content at its start (RenderBlockFlow::positionNewFloats leaves it there).
    if (!m_fragmentainerHasContent && !m_fragmentainer->hasContentBefore)
        return false;
    // Forced breaks apply to floats in the normal flow of the root too (css-break-3 §3.1), but not where no content took block size in the
    // fragmentainer yet.
    auto marginBox = floatItem.absoluteRectWithMargin();
    if (m_fragmentainer->appliesForcedBreaks && LayoutUnit { marginBox.top() } > -m_fragmentainer->rootBlockOffset) {
        CheckedRef box = downcast<RenderBox>(*floatItem.layoutBox()->rendererForIntegration());
        auto breakBefore = box->style().breakBefore();
        if (m_fragmentainer->breakType() == FragmentainerConstraints::Type::Page ? alwaysPageBreak(breakBefore) : breakBefore == BreakBetween::Column)
            return true;
    }
    // One whose content breaks goes when none of it fits, at the break before the float (css-break-3 §4.1), like the render tree moves it with
    // the pagination strut of its first line (RenderBlockFlow::positionNewFloats).
    if (auto* content = floatContent(*floatItem.layoutBox()); content && content->content.breaks && !goesWholeToNextFragmentainer(content->box, marginBox.height()))
        return content->isBreakBeforeContent;
    // One whose content the render tree broke crosses the ends of the fragmentainers where it is, unless none of it fit, for which the render
    // tree gives a block container a pagination strut that takes the float to the next fragmentainer (RenderBlockFlow::positionNewFloats), but
    // not in inline content, whose floats the render tree's line layout keeps where they are.
    if (CheckedRef box = downcast<RenderBox>(*floatItem.layoutBox()->rendererForIntegration()); breaksContentThroughRenderTree(box)) {
        CheckedPtr block = dynamicDowncast<RenderBlock>(box);
        return block && block->paginationStrut() && !box->containingBlock()->childrenInline();
    }
    return marginBox.bottom() > fragmentainerEndForContent() && marginBox.height() <= m_fragmentainer->wholeBlockSize();
}

// A float that avoids breaks inside goes to the next fragmentainer whole when it does not fit, like content that cannot be split, unless it is
// taller than a fragmentainer, where it breaks anyway (css-break-3 §4.4, RenderBlockFlow::adjustForUnsplittableChild).
bool BlockFormattingContext::goesWholeToNextFragmentainer(const RenderBox& floatBox, LayoutUnit marginBoxBlockSize) const
{
    return isUnsplittableForPagination(floatBox, m_fragmentainer->breakType()) && marginBoxBlockSize <= m_fragmentainer->wholeBlockSize();
}

void BlockFormattingContext::addSpaceShortage(LayoutUnit spaceShortage)
{
    if (spaceShortage > 0)
        m_spaceShortage = std::min(m_spaceShortage.value_or(spaceShortage), spaceShortage);
}

// In the logical coordinates of the root, which mirror those of a containing block whose inline direction is the opposite.
FloatingContext::ContainingBlock BlockFormattingContext::containingBlockForFloats(const ContainingBlockState& containingBlockState)
{
    auto contentBoxStart = containingBlockState.hasInlineDirectionOppositeToRoot ? containingBlockState.geometry.borderBoxWidth() - containingBlockState.inlineRange.max() : containingBlockState.inlineRange.min();
    return { toLayoutPoint(borderBoxOffsetInRoot(containingBlockState)), { contentBoxStart, containingBlockState.inlineRange.size() } };
}

void BlockFormattingContext::pushFloatsThatDoNotFit(std::span<const CheckedRef<const Box>> floatBoxes)
{
    if (!m_fragmentainer || !m_blockLayoutState.placedFloats())
        return;
    auto& placedFloats = m_blockLayoutState.ensurePlacedFloats(root().layoutBox());
    for (auto& floatBox : floatBoxes) {
        auto index = placedFloats.list().findIf([&](auto& floatItem) {
            return floatItem.layoutBox() == floatBox.ptr();
        });
        if (index == notFound)
            continue;
        auto* content = floatContent(floatBox);
        if ((!content || !content->content.breaks) && !breaksContentThroughRenderTree(downcast<RenderBox>(*floatBox->rendererForIntegration())))
            addUnbreakableBlockSize(LayoutUnit { placedFloats.list()[index].absoluteRectWithMargin().height() });
        if (!pushesFloat(placedFloats.list()[index]))
            continue;
        auto floatInFragmentainer = m_floatsInFragmentainer.reverseFindIf([&](auto& entry) {
            return entry.box.ptr() == floatBox.ptr();
        });
        ASSERT(floatInFragmentainer != notFound);
        if (floatInFragmentainer == notFound)
            continue;
        auto& placedFloat = m_floatsInFragmentainer[floatInFragmentainer];
        m_pushedFloats.append({ downcast<RenderBox>(*floatBox->rendererForIntegration()), placedFloats.list()[index], content ? std::optional { content->content } : std::nullopt, placedFloat.containingBlock, placedFloat.renderTreeLayout });
        m_continuingFloats.removeFirstMatching([&](auto& continuingFloat) {
            return &continuingFloat.box->layoutBox() == floatBox.ptr();
        });
        placedFloats.remove(floatBox);
    }
}

// The floats from the fragmentainer before: the ones that overflow into this one, and the ones that did not fit there, at the start of this
// one in their order, beside the floats there, where their containing block starts too when it continues here.
void BlockFormattingContext::placeFloatsFromFragmentainerBefore(BlockLayoutResult& result)
{
    if (m_incomingBreakToken->overflowingFloats.isEmpty() && m_incomingBreakToken->continuingFloats.isEmpty() && m_incomingBreakToken->pushedFloats.isEmpty())
        return;
    auto& placedFloats = m_blockLayoutState.ensurePlacedFloats(root().layoutBox());
    for (auto& floatItem : m_incomingBreakToken->overflowingFloats)
        placedFloats.add(floatItem);
    for (auto& continuingFloat : m_incomingBreakToken->continuingFloats)
        layoutContinuingFloat(continuingFloat, result);
    for (auto& pushedFloat : m_incomingBreakToken->pushedFloats) {
        // As high as it fits beside the floats at the start of this fragmentainer, like the render tree places it again from the top of the
        // next page (RenderBlockFlow::positionNewFloats), below the block-start border and padding of the block containers around it that
        // continue here and clone theirs.
        auto geometry = pushedFloat.floatItem.boxGeometry();
        auto containingBlock = FloatingContext::ContainingBlock { };
        auto floatingContext = floatingContextForSidesOf(pushedFloat.box, placedFloats);
        // Its content breaks from where it goes now.
        auto contentResult = BlockLayoutResult { };
        auto floatContentLayout = std::optional<FloatContentLayout> { };
        withOpenBlockContainers(pushedFloat.openBlockContainers, [&] {
            containingBlock = pushedFloat.content ? containingBlockForFloatFromFragmentainerBefore(pushedFloat.content->containingBlockState, pushedFloat.containingBlock) : FloatingContext::ContainingBlock { { pushedFloat.containingBlock.borderBoxTopLeft.x(), 0_lu }, pushedFloat.containingBlock.contentBox };
            auto contentLogicalTop = m_blockLayoutState.containingBlockStates().last().contentBoxLogicalTop();
            geometry.setTopLeft({ BoxGeometry::borderBoxLeft(geometry), contentLogicalTop - containingBlock.borderBoxTopLeft.y() + geometry.marginBefore() });
            geometry.setTopLeft(floatingContext.positionForFloat(pushedFloat.box->layoutBox(), geometry, containingBlock));
            auto borderBoxLogicalTop = containingBlock.borderBoxTopLeft.y() + BoxGeometry::borderBoxTop(geometry);
            if (auto content = pushedFloat.content) {
                floatContentLayout = layoutFloatContent(downcast<RenderBlockFlow>(pushedFloat.box), *content, borderBoxLogicalTop);
                contentResult = std::exchange(floatContentLayout->result, { });
                setBlockAxisGeometry(geometry, floatContentLayout->fragmentGeometry);
                LayoutIntegration::BlockIntegrationUtils::setShapeOutside(pushedFloat.box, geometry, content->containingBlockState.blockContainer->writingMode(), content->containingBlockState.inlineRange.size());
                m_floatContents.append({ pushedFloat.box, WTF::move(*content), false });
            } else if (auto& renderTreeLayout = pushedFloat.renderTreeLayout) {
                auto floatState = renderTreeLayout->floatState;
                auto placedOffsetInRoot = logicalLeftAndTopInRoot(containingBlock.borderBoxTopLeft.x() + BoxGeometry::borderBoxLeft(geometry), geometry.borderBoxWidth(), borderBoxLogicalTop);
                layoutIndependentFormattingContextThroughRenderTree(pushedFloat.box, renderTreeLayout->containingBlockState, floatState, { }, { }, placedOffsetInRoot, &*renderTreeLayout);
                geometry.setContentBoxHeight(floatState.geometry.contentBoxHeight());
            }
        });
        auto floatItem = floatingContext.makeFloatItem(pushedFloat.box->layoutBox(), geometry, containingBlock);
        if (floatContentLayout)
            continueFloat(pushedFloat.box.get(), BlockBreakToken::PushedFloat::Content { *pushedFloat.content }, *floatContentLayout, geometry, floatItem, containingBlock);
        placedFloats.add(WTF::move(floatItem));
        auto intrinsicContentBlockSize = contentResult.intrinsicContentBlockSize;
        appendFloatContentResult(WTF::move(contentResult), result);
        result.boxes.append({ pushedFloat.box, WTF::move(geometry), intrinsicContentBlockSize, { }, { }, false, false, !pushedFloat.content });
    }
}

TextStream& operator<<(TextStream& stream, const BlockLayoutResult& result)
{
    stream << "intrinsic content block size " << result.intrinsicContentBlockSize;
    for (auto& box : result.boxes) {
        auto borderBox = BoxGeometry::borderBoxRect(box.geometry);
        CheckedRef renderer = box.renderer;
        stream.nextLine();
        stream << renderer->renderName() << " at (" << borderBox.left() << "," << borderBox.top() << ") size " << borderBox.width() << "x" << borderBox.height()
            << " margins (" << box.geometry.marginBefore() << " " << box.geometry.marginEnd() << " " << box.geometry.marginAfter() << " " << box.geometry.marginStart() << ")";
    }
    return stream;
}

}
}
