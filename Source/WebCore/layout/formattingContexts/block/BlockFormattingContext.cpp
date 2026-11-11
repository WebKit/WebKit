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
#include "InlineContentConstrainer.h"
#include "LocalFrameViewInlines.h"
#include "OutOfFlowGeometry.h"
#include "RenderBlockFlow.h"
#include "RenderBoxInlines.h"
#include "RenderElementStyleInlines.h"
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
static std::optional<LayoutUnit> contentBoxBlockSizeWhileMeasured(const RenderBox& box, const BoxGeometry& geometry, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, std::optional<LayoutUnit> stretchFitBorderBoxBlockSize, WritingMode writingMode, BlockFormattingUtils::ContainingBlockIsTableCell containingBlockIsTableCell = BlockFormattingUtils::ContainingBlockIsTableCell::No)
{
    CheckedRef style = box.style();
    if (style->aspectRatio().hasRatio() && style->logicalHeight(writingMode).isAuto())
        return { };
    return BlockFormattingUtils::definiteContentBoxBlockSize(box, geometry, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSize, writingMode, containingBlockIsTableCell);
}

// What stretch block sizes resolve against. The children of a table cell stretch to the block size the cell has from its style when that is
// fixed, not to the one the row gives it, which only percentages resolve against (RenderBox::isBlockSizeResolvableForStretch), and like those
// without the intrinsic padding of its vertical alignment.
static std::optional<LayoutUnit> stretchFitBasis(const ContainingBlockState& containingBlockState)
{
    CheckedPtr tableCell = dynamicDowncast<RenderTableCell>(containingBlockState.blockContainer);
    if (!tableCell)
        return containingBlockState.contentBoxBlockSize;
    if (!tableCell->style().logicalHeight().isFixed())
        return { };
    auto& geometry = containingBlockState.geometry;
    auto borderAndPaddingBlockSize = geometry.verticalBorderAndPadding() - tableCell->intrinsicPaddingBefore() - tableCell->intrinsicPaddingAfter();
    auto borderBoxBlockSize = BlockFormattingUtils::borderBoxBlockSize(*tableCell, borderAndPaddingBlockSize, { }, { }, { }, tableCell->writingMode());
    return std::max(0_lu, borderBoxBlockSize - borderAndPaddingBlockSize - tableCell->scrollbarLogicalHeight());
}

BlockFormattingContext::BlockFormattingContext(const RenderBlockFlow& root, LayoutIntegration::BlockIntegrationUtils& integrationUtils, BlockLayoutState& blockLayoutState)
    : m_root(root)
    , m_integrationUtils(integrationUtils)
    , m_blockLayoutState(blockLayoutState)
{
}

BlockLayoutResult BlockFormattingContext::layout(const ConstraintsForBlockContent& constraints, const BoxGeometry& rootGeometry)
{
    // Floats placed from a provisional top beside or below floats from before may go elsewhere from the top the margins give their block
    // container: the content is laid out again with those tops, like the render tree path lays out a block that moved while its floats were
    // placed again (RenderBlockFlow::layoutBlockChild).
    auto initialBlockLayoutState = m_blockLayoutState;
    auto result = layoutContent(constraints, rootGeometry);
    if (!m_needsLayoutWithResolvedBorderBoxLogicalTops)
        return result;
    m_needsLayoutWithResolvedBorderBoxLogicalTops = false;
    m_isLayingOutWithResolvedBorderBoxLogicalTops = true;
    m_blockLayoutState = WTF::move(initialBlockLayoutState);
    return layoutContent(constraints, rootGeometry);
}

BlockLayoutResult BlockFormattingContext::layoutContent(const ConstraintsForBlockContent& constraints, const BoxGeometry& rootGeometry)
{
    ASSERT(m_blockLayoutState.containingBlockStates().isEmpty());
    auto result = BlockLayoutResult { };
    m_blockLayoutState.containingBlockStates().append(containingBlockStateForRoot(constraints, rootGeometry));
    m_blockLayoutState.marginCollapse().startBlockFormattingContextRoot(m_blockLayoutState.containingBlockStates().last());
    // The margins of a box that establishes a block formatting context do not collapse with its children's (CSS 2.2 §8.3.1).
    m_blockLayoutState.marginCollapse().resetAdjoiningMargins(m_blockLayoutState.containingBlockStates().last().contentBoxLogicalTop());
    // margin-trim discards the margins at the block-start and block-end edges of the content (css-box-4 §3.3.1).
    auto marginTrim = root().style().marginTrim();
    if (marginTrim.contains(Style::MarginTrimSide::BlockStart))
        m_blockLayoutState.marginCollapse().setTrimsMarginsAtBlockStartEdge(true);

    // FIXME: Keep the block items across layouts and invalidate them on tree and formatting context participation changes.
    auto blockItems = BlockItemList { };
    // A root with only inline-level content establishes an inline formatting context for it (CSS Display 3).
    if (root().childrenInline())
        handleInlineContent(root(), result);
    else
        blockItems = BlockItemsBuilder { root() }.build();
    layoutBlockItems(blockItems, result);

    ASSERT(m_blockLayoutState.containingBlockStates().size() == 1);
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
    result.intrinsicContentBlockSize = std::max(0_lu, contentLogicalBottom - rootState.contentBoxLogicalTop());
    m_blockLayoutState.containingBlockStates().clear();
    return result;
}

void BlockFormattingContext::layoutBlockItems(const BlockItemList& blockItems, BlockLayoutResult& result)
{
    auto blockContainerEndIndex = [&](size_t startIndex) {
        auto index = startIndex + 1;
        while (blockItems[index].type() != BlockItem::Type::BlockContainerEnd || &blockItems[index].renderer() != &blockItems[startIndex].renderer())
            ++index;
        return index;
    };
    for (size_t index = 0; index < blockItems.size(); ++index) {
        auto& blockItem = blockItems[index];
        CheckedRef box = blockItem.renderer();
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
            handleBlockContainerStart(box);
            break;
        case BlockItem::Type::BlockContainerEnd:
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
        }
    }
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
    m_blockLayoutState.setLineClamp(BlockLayoutState::LineClamp { 0, m_blockLayoutState.lineClamp()->shouldDiscardOverflow, false });
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
    LayoutIntegration::BlockIntegrationUtils::markPercentageHeightDescendantsForLayout(downcast<RenderBlockFlow>(blockContainer));
    LayoutIntegration::BlockIntegrationUtils::invalidateContentInlineSizesBeforeLayout(blockContainer, m_blockLayoutState.containingBlockStates().last().relayoutChildren);
    auto clearance = clearanceAtBlockStart(blockContainer);
    auto blockContainerState = computeContainingBlockStateAtBlockStart(blockContainer);
    blockContainerState.clearance = clearance;
    if (blockContainerState.geometry.borderAndPaddingBefore())
        resolveBorderBoxLogicalTops(blockContainerState);
    startTextBoxTrim(downcast<RenderBlockFlow>(blockContainer), blockContainerState);
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

void BlockFormattingContext::handleBlockContainerEnd(const RenderBox& blockContainer, BlockLayoutResult& result)
{
    auto blockContainerState = m_blockLayoutState.containingBlockStates().takeLast();
    ASSERT(blockContainerState.blockContainer.ptr() == &blockContainer);
    endTextBoxTrim(downcast<RenderBlockFlow>(blockContainer), blockContainerState);
    if (auto trimsMargins = blockContainerState.trimsMarginsAtBlockStartEdgeToRestore) {
        auto& marginCollapse = m_blockLayoutState.marginCollapse();
        marginCollapse.setTrimsMarginsAtBlockStartEdge(*trimsMargins && marginCollapse.trimsMarginsAtBlockStartEdge());
    }
    auto& containingBlockState = m_blockLayoutState.containingBlockStates().last();
    m_blockLayoutState.marginCollapse().endBlockContainer(blockContainerState, containingBlockState);
    // Nothing in its content separated the block-start margin from the block-end margin. Unless the box's own height, block-end
    // border or padding does, the margins collapse through it.
    auto marginsCollapseThrough = [&] {
        if (blockContainer.stretchesToViewport()) {
            // Quirks mode: an empty body still stretches to the viewport (RenderBlock::isSelfCollapsingBlock checks the height it gets).
            auto margins = m_blockLayoutState.marginCollapse().adjoiningMargins();
            margins.add(blockContainerState.geometry.marginAfter());
            if (viewportStretchedBorderBoxBlockSize(blockContainerState.margins.collapsedMarginsBefore().collapsedValue(), margins.collapsedValue()) > 0)
                return false;
        }
        return BlockMarginCollapse::marginsCollapseThrough(blockContainer, blockContainerState.geometry, percentageBasis(blockContainer, containingBlockState), stretchFitBorderBoxBlockSize(blockContainer, blockContainerState.geometry, containingBlockState), root().writingMode());
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
static PlacedFloats::Item intrudingFloat(const PlacedFloats::Item& floatItem, const ContainingBlockState& blockContainerState, LayoutSize offset, WritingMode writingMode)
{
    auto geometry = floatItem.boxGeometry();
    auto localTopLeft = BoxGeometry::borderBoxTopLeft(geometry);
    auto borderBoxTopLeft = floatItem.absoluteBorderBoxRect().topLeft() + offset;
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
void BlockFormattingContext::addInlineContentFloats(const PlacedFloats::List& floats)
{
    auto& blockContainerState = m_blockLayoutState.containingBlockStates().last();
    auto offsetInRoot = borderBoxOffsetInRoot(blockContainerState);
    addFloats(WTF::map(floats, [&](auto& floatItem) {
        if (blockContainerState.hasInlineDirectionOppositeToRoot)
            return movedFloat(mirroredFloat(floatItem, blockContainerState.geometry.borderBoxWidth()), offsetInRoot, floatItem.layoutBox());
        return movedFloat(floatItem, offsetInRoot, floatItem.layoutBox());
    }), blockContainerState);
}

void BlockFormattingContext::handleInlineContent(const RenderBox& blockContainer, BlockLayoutResult& result)
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
            for (auto& floatItem : placedFloats->list())
                intrudingFloats.append(intrudingFloat(floatItem, blockContainerState, -offsetInRoot, root().writingMode()));
        }
    }
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
    auto inlineContentLayoutResult = integrationUtils().layoutInlineContent(inlineContentRoot, constraintsForInlineContent(blockContainer, blockContainerState), geometry, blockContainerState.contentBoxBlockSize, blockContainerState.replacedElementAvailableBlockSize, orthogonalFlowAvailableInlineSpace(blockContainerState), blockContainerState.relayoutChildren || m_isLayingOutWithResolvedBorderBoxLogicalTops, m_blockLayoutState, intrudingFloats, *this, result);
    addInlineContentFloats(inlineContentLayoutResult.floats);
    // The floats of a line-clamp container are clipped to its block-end content edge, those the inline formatting context hid after the
    // clamp point included.
    if (auto lineClamp = m_blockLayoutState.lineClamp(); lineClamp && lineClamp->shouldDiscardOverflow) {
        for (auto& floatItem : inlineContentLayoutResult.floats) {
            if (CheckedPtr floatBox = floatItem.layoutBox())
                LayoutIntegration::BlockIntegrationUtils::setFloatClippedByLineClamp(downcast<RenderBox>(*floatBox->rendererForIntegration()), true);
        }
    }
    // The content takes its lines off the line-clamp budget (LineClampUpdater).
    if (auto lineClamp = m_blockLayoutState.lineClamp()) {
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
        return;
    }
    // Content that ends with a block-level box leaves the margins after it open (CSS 2.2 §8.3.1).
    if (!inlineContentLayoutResult.hasLineBoxes || inlineContentLayoutResult.endsWithBlockLevelBox)
        return;
    blockContainerStateAfterLayout.blockEndChildren = { { }, { }, blockContainerStateAfterLayout.borderAndPaddingBefore };
    // The first line box separates the block-start margin from what follows (CSS 2.2 §8.3.1).
    if (!blockContainerStateAfterLayout.borderBoxLogicalTop)
        resolveBorderBoxLogicalTops(blockContainerStateAfterLayout);
    marginCollapse.resetAdjoiningMarginsAfterLineBoxes(blockContainerStateAfterLayout.contentBoxLogicalTop() + inlineContentLayoutResult.contentBlockSize);
}

// A block-level box in inline content is laid out like a child of the block container, between the line boxes before and after it (CSS 2.2
// §9.2.1.1).
BlockLevelBoxInInlineContent BlockFormattingContext::layoutBlockLevelBoxInInlineContent(const RenderBox& box, LayoutUnit lineLogicalTop, size_t lineCount, size_t lineCountForLineClamp, PlacedFloats& inlineContentFloats, BlockLayoutResult& result)
{
    auto& containingBlockStates = m_blockLayoutState.containingBlockStates();
    auto& blockContainerState = containingBlockStates.last();
    ASSERT(blockContainerState.blockContainer.ptr() == box.containingBlock());
    // The line boxes before it separate its block-start margin from the margins before them (CSS 2.2 §8.3.1).
    if (lineCount > blockContainerState.lineCountThroughLastBlockLevelBox) {
        if (!blockContainerState.borderBoxLogicalTop)
            resolveBorderBoxLogicalTops(blockContainerState);
        m_blockLayoutState.marginCollapse().separateMarginsAtLineBox(*blockContainerState.borderBoxLogicalTop + lineLogicalTop);
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
    layoutBlockItems(BlockItemsBuilder { root() }.buildForBlockLevelBoxInInlineContent(box), result);
    ASSERT_UNUSED(stateCount, containingBlockStates.size() == stateCount);
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
    // The lines after the box avoid the floats in it too, and the ones from outside the block container, from where its top is now.
    auto offsetInRoot = borderBoxOffsetInRoot(blockContainerStateAfterLayout);
    auto floats = PlacedFloats::List { };
    for (auto& floatItem : m_blockLayoutState.ensurePlacedFloats(root().layoutBox()).list()) {
        auto inlineContentFloat = inlineContentFloats.list().findIf([&](auto& inlineContentFloat) {
            return inlineContentFloat.layoutBox() && inlineContentFloat.layoutBox() == floatItem.layoutBox();
        });
        floats.append(inlineContentFloat != notFound ? inlineContentFloats.list()[inlineContentFloat] : intrudingFloat(floatItem, blockContainerStateAfterLayout, -offsetInRoot, root().writingMode()));
    }
    inlineContentFloats.clear();
    for (auto& floatItem : floats)
        inlineContentFloats.add(WTF::move(floatItem));
    auto& marginCollapse = m_blockLayoutState.marginCollapse();
    auto marginsCollapseWithBlockContainerTop = !blockContainerStateAfterLayout.borderBoxLogicalTop;
    auto marginState = BlockLayoutState::MarginState { };
    marginState.adjoiningMargins = marginCollapse.adjoiningMargins();
    marginState.atBlockContainerStart = marginsCollapseWithBlockContainerTop;
    marginState.blockContainerMarginBeforeCollapsesWithContent = marginsCollapseWithBlockContainerTop;
    return {
        result.boxes.last().geometry,
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

// Where resolveBorderBoxLogicalTops would put the next box, without clearance of its own, and the block containers waiting for their top.
auto BlockFormattingContext::hypotheticalPosition() const -> HypotheticalPosition
{
    auto marginsLogicalBottom = m_blockLayoutState.marginCollapse().collapsedLogicalBottom();
    auto states = statesWaitingForTop();
    auto tops = borderBoxLogicalTopsWaitingForTop(states, marginsLogicalBottom);
    auto position = HypotheticalPosition { states.isEmpty() ? marginsLogicalBottom : tops.borderBoxLogicalTops.first(), { } };
    for (size_t index = 0; index < states.size(); ++index)
        position.blockContainerLogicalTops.append({ states[index]->blockContainer, tops.borderBoxLogicalTops[index] });
    return position;
}

// The floats placed so far, where the ones waiting for their block container's top are where that top would be at the hypothetical position
// of the next box.
PlacedFloats BlockFormattingContext::floatsAtHypotheticalPosition() const
{
    auto position = hypotheticalPosition();
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
    auto hypotheticalBorderBoxLogicalTop = hypotheticalPosition().borderBoxLogicalTop;
    auto& pendingFloats = m_blockLayoutState.pendingFloats();
    auto floats = floatsAtHypotheticalPosition();
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
        return spaceBesideFloats(floatingContext, containingBlockState, top, bottom);
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
    auto marginStart = hasAutoInlineMargin ? 0_lu : Style::evaluateMinimum<LayoutUnit>(marginStartLength, containingBlockState.inlineRange.size(), style->usedZoomForLength());
    auto marginEnd = hasAutoInlineMargin ? 0_lu : Style::evaluateMinimum<LayoutUnit>(marginEndLength, containingBlockState.inlineRange.size(), style->usedZoomForLength());
    CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(box);
    auto isLaidOutThroughRenderTree = !blockContainer || LayoutIntegration::BlockIntegrationUtils::laysOutThroughRenderTree(*blockContainer);
    auto hasOwnInlineSize = isLaidOutThroughRenderTree && decidesItsOwnInlineSize(box, writingMode);
    // A table with an auto inline size is no wider than the space, which it floors to its min-content size itself (RenderTable::updateLogicalWidth
    // with shrinkLogicalWidthToAvoidFloats), so it keeps the inline size it has in the containing block when that fits.
    auto shrinksIntoSpace = is<RenderTable>(box) && box.shrinkToAvoidFloats();
    auto hasInlineSizeFromSpace = hasOwnInlineSize && hasInlineSizeFromSpaceBesideFloats(box, writingMode);
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
    };
    auto laidOutState = std::optional<ContainingBlockState> { };
    auto laidOutContentBlockSize = std::optional<LayoutUnit> { };
    auto laidOutGivenBorderBoxInlineSize = std::optional<LayoutUnit> { };
    auto containingBlockBorderBoxInlineSize = std::optional<LayoutUnit> { };
    auto placementIn = [&](LayoutUnit spaceStart, LayoutUnit spaceEnd) {
        auto geometry = boxState.geometry;
        auto marginInFloats = [](LayoutUnit margin, LayoutUnit floatsInlineSize) {
            return std::min(std::max(0_lu, margin), floatsInlineSize);
        };
        auto stretchFitInlineSize = spaceEnd - spaceStart - marginStart - marginEnd + marginInFloats(marginStart, spaceStart - contentBoxStart) + marginInFloats(marginEnd, contentBoxEnd - spaceEnd);
        if (!hasOwnInlineSize) {
            computeInlineSizeAndMargins(box, containingBlockState, geometry, stretchFitInlineSize + marginStart + marginEnd);
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
        return Placement { WTF::move(geometry), spaceStart, spaceEnd, borderBoxInlineSize };
    };
    // The box laid out for a placement, and its border box block size.
    auto layoutFor = [&](const Placement& placement) {
        auto hasInlineSize = [&] {
            if (hasOwnInlineSize)
                return laidOutGivenBorderBoxInlineSize == placement.borderBoxInlineSize;
            return laidOutState->geometry.borderBoxWidth() == placement.geometry.borderBoxWidth();
        };
        if (laidOutState && hasInlineSize())
            return laidOutState->geometry.borderBoxHeight();
        auto state = containingBlockStateFor(box, placement.geometry, containingBlockState);
        state.margins = boxState.margins;
        laidOutContentBlockSize = layoutContent(state, placement.borderBoxInlineSize).first;
        laidOutGivenBorderBoxInlineSize = placement.borderBoxInlineSize;
        if (!state.isLaidOutThroughRenderTree)
            computeBlockSize(box, laidOutContentBlockSize.value_or(0_lu), containingBlockState, state.geometry);
        laidOutState = WTF::move(state);
        return laidOutState->geometry.borderBoxHeight();
    };
    // Whether the border box fits the space beside the floats at a position, for its whole block size, which it is laid out for. Floats
    // further down narrow that space, which an auto inline size then stretches into, once more at most.
    auto placementAt = [&](LayoutUnit top) -> std::pair<Placement, bool> {
        auto [spaceStart, spaceEnd] = spaceBetween(top, top);
        auto placement = placementIn(spaceStart, spaceEnd);
        auto blockSize = layoutFor(placement);
        for (auto attempt = 0; attempt < 2; ++attempt) {
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
        // Its own formatting context decided its inline size (layoutIndependentFormattingContextThroughRenderTree): the margins are in the space.
        auto spaceInlineSize = hasAutoInlineMargin ? autoMarginsInlineSize(placement.spaceEnd - placement.spaceStart) : containingBlockState.inlineRange.size();
        auto [usedMarginStart, usedMarginEnd] = BlockFormattingUtils::inlineMargins(box, geometry.borderBoxWidth(), spaceInlineSize, containingBlockStyle);
        geometry.setHorizontalMargin({ hasAutoInlineMargin ? usedMarginStart : marginStart, hasAutoInlineMargin ? usedMarginEnd : marginEnd });
    }
    // Its end margin still takes what makes the equality true in the containing block, past the floats (RenderBox::computeLogicalWidth).
    if (containingBlockState.inlineRange.size() && BlockFormattingUtils::ignoresEndMarginWhenOverConstrained(box, containingBlockStyle))
        geometry.setHorizontalMargin({ geometry.marginStart(), containingBlockState.inlineRange.size() - geometry.borderBoxWidth() - geometry.marginStart() });
    // An auto start margin is in the space, a float may sit in a positive one, and a negative one pulls the box back only where no float is
    // (RenderBlockFlow::determineLogicalLeftPositionForChild).
    auto borderBoxStart = [&] {
        if (marginStartLength.isAuto())
            return placement.spaceStart + geometry.marginStart();
        if (placement.spaceStart > contentBoxStart)
            return std::max(contentBoxStart + geometry.marginStart(), placement.spaceStart);
        return contentBoxStart + geometry.marginStart();
    }();
    // justify-self moves it from there by the free space in the containing block, not in the space beside the floats.
    geometry.setLeft(borderBoxStart + BlockFormattingUtils::justifySelfOffset(box, geometry.borderBoxWidth(), geometry.marginStart(), geometry.marginEnd(), containingBlockState.inlineRange.size(), containingBlockStyle));
    // Below the hypothetical position the floats put it, as clearance would (resolveBorderBoxLogicalTops).
    if (borderBoxLogicalTop > hypotheticalBorderBoxLogicalTop) {
        if (pendingFloats.isEmpty())
            boxState.clearance = ContainingBlockState::Clearance { borderBoxLogicalTop, { }, marginsBeforeLogicalBottom };
        else
            boxState.clearance = ContainingBlockState::Clearance { LayoutUnit::min(), { { containingBlockState.blockContainer, borderBoxLogicalTop - hypotheticalBorderBoxLogicalTop } }, marginsBeforeLogicalBottom };
    }
    return laidOutContentBlockSize;
}

void BlockFormattingContext::handleIndependentFormattingContextRoot(const RenderBox& box, BlockLayoutResult& result)
{
    auto rootState = containingBlockStateAtBlockStart(box);
    // Its margins do not collapse with its children's, nor through it, even when it is empty (CSS 2.2 §8.3.1). With a block step, they
    // collapse once the block size is known, which its content, in a formatting context of its own, does not depend on the position for, and
    // so do those of a fieldset, once its own layout has placed its legend, whose block-start margin it takes (RenderBlock::marginBefore).
    auto stepSize = blockStepSize(box);
    auto collapsesMarginsOnceLaidOut = stepSize || box.isFieldset();

    // A block formatting context root the block formatting context lays out adds the boxes in its content to the result, which another layout
    // of it replaces.
    auto resultBoxCount = result.boxes.size();
    auto resultOutOfFlowBoxCount = result.outOfFlowBoxes.size();
    auto layoutContent = [&](ContainingBlockState& state, std::optional<LayoutUnit> borderBoxInlineSize) {
        result.boxes.shrink(resultBoxCount);
        result.outOfFlowBoxes.shrink(resultOutOfFlowBoxCount);
        auto contentBlockSize = std::optional<LayoutUnit> { };
        auto legendMarginBefore = LayoutUnit { };
        CheckedPtr root = dynamicDowncast<RenderBlockFlow>(box);
        if (!root || LayoutIntegration::BlockIntegrationUtils::laysOutThroughRenderTree(*root)) {
            legendMarginBefore = layoutIndependentFormattingContextThroughRenderTree(box, state, borderBoxInlineSize);
            contentBlockSize = state.geometry.contentBoxHeight();
        } else if (!root->firstChild())
            contentBlockSize = BlockFormattingUtils::emptyEditableContentBlockSize(box);
        else {
            LayoutIntegration::BlockIntegrationUtils::markPercentageHeightDescendantsForLayout(*root);
            // Its content is in a formatting context of its own; a block formatting context gets a block layout state of its own, with its own
            // text-box-trim only, whose start trim a block-start border or padding keeps from the content after it too, and the last formatted
            // lines of the boxes with trim-end in it.
            auto blockLayoutState = BlockLayoutState { { } };
            blockLayoutState.setLastFormattedLineRoots(Vector { m_blockLayoutState.lastFormattedLineRoots() });
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
            auto layoutRootContent = [&](LayoutUnit shift) {
                return BlockFormattingContext { *root, integrationUtils(), blockLayoutState }.layout({ { horizontalConstraints, state.borderAndPaddingBefore + shift }, state.contentBoxBlockSize, state.fillAvailableBlockSize, state.replacedElementAvailableBlockSize, state.relayoutChildren }, state.geometry);
            };
            auto rootResult = layoutRootContent({ });
            // align-content moves the content as a unit in the block size the root gets from that of its content, where it is laid out again.
            if (!root->style().alignContent().isNormal()) {
                auto geometry = state.geometry;
                computeBlockSize(box, rootResult.intrinsicContentBlockSize, m_blockLayoutState.containingBlockStates().last(), geometry);
                if (auto shift = BlockFormattingUtils::alignContentShift(box, geometry.contentBoxHeight() - rootResult.intrinsicContentBlockSize)) {
                    blockLayoutState = WTF::move(initialBlockLayoutState);
                    rootResult = layoutRootContent(shift);
                }
            }
            result.boxes.appendVector(WTF::move(rootResult.boxes));
            contentBlockSize = rootResult.intrinsicContentBlockSize;
            result.outOfFlowBoxes.appendVector(WTF::move(rootResult.outOfFlowBoxes));
        }
        return std::pair { contentBlockSize, legendMarginBefore };
    };

    auto contentBlockSize = std::optional<LayoutUnit> { };
    auto legendMarginBefore = LayoutUnit { };
    auto isLaidOut = false;
    auto* placedFloats = m_blockLayoutState.placedFloats();
    auto isAfterFloats = placedFloats && !placedFloats->isEmpty();
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
        auto layoutContentKeepingLegendMargin = [&](ContainingBlockState& state, std::optional<LayoutUnit> borderBoxInlineSize) {
            auto contentLayoutResult = layoutContent(state, borderBoxInlineSize);
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
        std::tie(contentBlockSize, legendMarginBefore) = layoutContent(rootState, { });

    if (stepSize && !isAfterFloats) {
        auto& geometry = rootState.geometry;
        if (!rootState.isLaidOutThroughRenderTree)
            computeBlockSize(box, contentBlockSize.value_or(0_lu), m_blockLayoutState.containingBlockStates().last(), geometry);
        auto halfExtraSpace = BlockStepSizing::computeExtraSpace(*stepSize, geometry.marginBefore() + geometry.borderBoxHeight() + geometry.marginAfter()) / 2;
        geometry.setVerticalMargin({ geometry.marginBefore() + halfExtraSpace, geometry.marginAfter() + halfExtraSpace });
        m_blockLayoutState.marginCollapse().startBlockContainer(rootState, m_blockLayoutState.containingBlockStates().last(), geometry.marginBefore());
        resolveBorderBoxLogicalTops(rootState);
    } else if (collapsesMarginsOnceLaidOut && !isAfterFloats) {
        m_blockLayoutState.marginCollapse().startBlockContainer(rootState, m_blockLayoutState.containingBlockStates().last(), rootState.geometry.marginBefore() + legendMarginBefore);
        resolveBorderBoxLogicalTops(rootState);
    }
    if (contentBlockSize)
        m_blockLayoutState.marginCollapse().resetAdjoiningMargins(rootState.contentBoxLogicalTop() + *contentBlockSize);
    m_blockLayoutState.marginCollapse().endBlockContainer(rootState, m_blockLayoutState.containingBlockStates().last());
    placeInContainingBlock(box, WTF::move(rootState), result);
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
// whose inline size its own formatting context does not decide, the stretch-fit inline size. The rest comes back from the box's own formatting
// context, laid out through the render tree, which lays out its content: the inline size it decides (decidesItsOwnInlineSize), unless it is
// a table that shrinks into the space beside floats (placeBesideFloats), the block size, border and padding, and what it adds to the
// block-start margin, which collapses with the others once the box is laid out.
LayoutUnit BlockFormattingContext::layoutIndependentFormattingContextThroughRenderTree(const RenderBox& box, ContainingBlockState& boxState, std::optional<LayoutUnit> borderBoxInlineSize)
{
    auto& containingBlockState = m_blockLayoutState.containingBlockStates().last();
    auto containingBlockInlineSize = containingBlockState.inlineRange.size();
    auto& geometry = boxState.geometry;
    auto containingBlockBlockSize = [&] -> std::optional<LayoutUnit> {
        if (box.writingMode().isOrthogonal(root().writingMode()))
            return orthogonalFlowAvailableInlineSpace(containingBlockState);
        if (box.isRenderReplaced() && box.document().inQuirksMode())
            return containingBlockState.replacedElementAvailableBlockSize;
        return containingBlockState.contentBoxBlockSize;
    };
    auto constraints = ConstraintsForIndependentFormattingContext {
        .containingBlockInlineSize = containingBlockInlineSize,
        .containingBlockBlockSize = containingBlockBlockSize(),
        .relayoutChildren = containingBlockState.relayoutChildren,
        .borderBoxInlineSize = decidesItsOwnInlineSize(box, root().writingMode()) ? borderBoxInlineSize : std::optional { geometry.borderBoxWidth() },
        .marginBefore = geometry.marginBefore(),
        .marginAfter = geometry.marginAfter()
    };
    auto layoutResult = integrationUtils().layoutIndependentFormattingContext(box, constraints);

    geometry.setBorder(layoutResult.border);
    geometry.setPadding(layoutResult.padding);
    setContentBoxInlineSize(geometry, layoutResult.borderBoxInlineSize);
    setContentBoxBlockSize(geometry, layoutResult.borderBoxBlockSize);
    CheckedRef containingBlockStyle = containingBlockState.blockContainer->style();
    auto [marginStart, marginEnd] = BlockFormattingUtils::inlineMargins(box, layoutResult.borderBoxInlineSize, containingBlockInlineSize, containingBlockStyle);
    geometry.setHorizontalMargin({ marginStart, marginEnd });
    computeLogicalLeft(box, containingBlockState, geometry);
    boxState.borderAndPaddingBefore = geometry.borderAndPaddingBefore();
    boxState.isLaidOutThroughRenderTree = true;
    return layoutResult.legendMarginBefore;
}

void BlockFormattingContext::handleFloat(const RenderBox& box, BlockLayoutResult& result)
{
    ASSERT(m_blockLayoutState.containingBlockStates().last().blockContainer.ptr() == box.parent());
    // A float before the clamp point of a line-clamp container is clipped to its block-end content edge (css-overflow-4 §5.3.1).
    auto lineClamp = m_blockLayoutState.lineClamp();
    LayoutIntegration::BlockIntegrationUtils::setFloatClippedByLineClamp(box, lineClamp && lineClamp->shouldDiscardOverflow);
    // Its own formatting context lays it out at its shrink-to-fit inline size (CSS 2.2 §10.3.5).
    auto boxState = containingBlockStateAtBlockStart(box);
    layoutIndependentFormattingContextThroughRenderTree(box, boxState);
    auto& geometry = boxState.geometry;
    auto& containingBlockState = m_blockLayoutState.containingBlockStates().last();
    // Its auto margins are zero, and none of its margins collapse or fill the containing block (CSS 2.2 §10.3.5, §10.6.6).
    CheckedRef style = box.style();
    auto writingMode = containingBlockState.blockContainer->writingMode();
    auto margin = [&](auto& length) {
        return length.isAuto() ? 0_lu : Style::evaluateMinimum<LayoutUnit>(length, containingBlockState.inlineRange.size(), style->usedZoomForLength());
    };
    geometry.setHorizontalMargin({ margin(style->marginStart(writingMode)), margin(style->marginEnd(writingMode)) });
    geometry.setVerticalMargin({ margin(style->marginBefore(writingMode)), margin(style->marginAfter(writingMode)) });

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
    auto contentBoxStart = isOpposite ? containingBlockInlineSize - containingBlockState.inlineRange.max() : containingBlockState.inlineRange.min();
    auto containingBlock = FloatingContext::ContainingBlock { toLayoutPoint(containingBlockOffsetInRoot), { contentBoxStart, containingBlockState.inlineRange.size() } };
    auto geometryInRoot = geometry;
    if (isOpposite)
        geometryInRoot.setHorizontalMargin({ geometry.marginEnd(), geometry.marginStart() });
    geometryInRoot.setTopLeft(floatingContext.positionForFloat(box.layoutBox(), geometryInRoot, containingBlock));
    addFloats({ floatingContext.makeFloatItem(box.layoutBox(), geometryInRoot, containingBlock) }, containingBlockState);
    auto borderBoxLeft = BoxGeometry::borderBoxLeft(geometryInRoot);
    geometry.setTopLeft({ isOpposite ? containingBlockInlineSize - borderBoxLeft - geometry.borderBoxWidth() : borderBoxLeft, BoxGeometry::borderBoxTop(geometryInRoot) });
    result.boxes.append({ box, WTF::move(geometry), { }, { }, { }, false, false, true });
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
        if (!placedFloats || placedFloats->isEmpty())
            return inlineRange;
        auto& rootState = m_blockLayoutState.containingBlockStates().first();
        auto floats = floatsAtHypotheticalPosition();
        auto staticBlockPositionInRoot = hypotheticalPosition().borderBoxLogicalTop;
        auto floatingContext = FloatingContext { root().layoutBox(), floats, { }, rootState.geometry.borderBoxWidth() };
        auto [lineStart, lineEnd] = spaceBesideFloats(floatingContext, parentState, staticBlockPositionInRoot, staticBlockPositionInRoot);
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

// A replaced element, a table, a flex container, a WebVTT cue box, a form control or a block container whose content the block formatting
// context does not lay out as a nested root, whose own formatting context lays it out through the render tree; the block formatting context
// places it, and sizes it when it is neither replaced nor a table (css-position-3 §4.3), from its content when its block size is auto.
BlockLayoutResult BlockFormattingContext::layoutOutOfFlowIndependentFormattingContext(const RenderBox& root, const OutOfFlowContainingBlock& containingBlock, const OutOfFlowGeometry::StaticPosition& staticPosition, const std::optional<OutOfFlowGeometry::DefaultAnchor>& defaultAnchor)
{
    auto geometry = computeBorderAndPadding(root, containingBlock.inlineSize);
    auto outOfFlowGeometry = OutOfFlowGeometry { root, { containingBlock.inlineSize, containingBlock.blockSize, containingBlock.writingMode, containingBlock.isScrollContainer, containingBlock.scrollableContainingBlock, containingBlock.fillAvailableBlockSize }, staticPosition, defaultAnchor, geometry.horizontalBorderAndPadding(), geometry.verticalBorderAndPadding() };
    auto [inlineAxis, blockAxis] = [&] -> std::pair<OutOfFlowGeometry::AxisGeometry, OutOfFlowGeometry::AxisGeometry> {
        // A replaced element from its natural size and aspect ratio (CSS 2.2 §10.3.8, §10.6.5), and a table from its columns, which do not
        // stretch it (css-position-3 §4.3, RenderTable::updateLogicalWidth).
        if (root.isRenderReplaced() || root.isRenderOrLegacyRenderSVGRoot() || is<RenderTable>(root)) {
            auto borderBoxSize = integrationUtils().layoutOutOfFlowIndependentFormattingContext(root);
            return { outOfFlowGeometry.computeReplacedInlineAxisGeometry(borderBoxSize.width()), outOfFlowGeometry.computeReplacedBlockAxisGeometry(borderBoxSize.height()) };
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
        auto borderBoxSize = integrationUtils().layoutOutOfFlowIndependentFormattingContext(root, inlineAxis.borderBoxSize, borderBoxBlockSize);
        return { inlineAxis, outOfFlowGeometry.computeBlockAxisGeometry(borderBoxSize.height() - geometry.verticalBorderAndPadding(), inlineAxis.borderBoxSize) };
    }();
    setContentBoxInlineSize(geometry, inlineAxis.borderBoxSize);
    geometry.setHorizontalMargin({ inlineAxis.marginStart, inlineAxis.marginEnd });
    geometry.setLeft(containingBlock.inlineStart + inlineAxis.borderBoxStart);
    setContentBoxBlockSize(geometry, blockAxis.borderBoxSize);
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

BlockLayoutResult BlockFormattingContext::layoutOutOfFlowBox(const RenderBox& outOfFlowBox, const OutOfFlowContainingBlock& containingBlock, const OutOfFlowGeometry::StaticPosition& staticPosition, const std::optional<OutOfFlowGeometry::DefaultAnchor>& defaultAnchor)
{
    if (!is<RenderBlockFlow>(outOfFlowBox) || outOfFlowBox.isRenderVTTCue() || LayoutIntegration::BlockIntegrationUtils::laysOutThroughRenderTree(outOfFlowBox))
        return layoutOutOfFlowIndependentFormattingContext(outOfFlowBox, containingBlock, staticPosition, defaultAnchor);
    auto result = BlockLayoutResult { };
    CheckedRef box = downcast<RenderBlockFlow>(outOfFlowBox);
    // In the box's own writing mode, which its content is laid out in.
    auto geometry = computeBorderAndPadding(box, containingBlock.inlineSize);
    auto outOfFlowGeometry = OutOfFlowGeometry { box, { containingBlock.inlineSize, containingBlock.blockSize, containingBlock.writingMode, containingBlock.isScrollContainer, containingBlock.scrollableContainingBlock, containingBlock.fillAvailableBlockSize }, staticPosition, defaultAnchor, geometry.horizontalBorderAndPadding(), geometry.verticalBorderAndPadding() };

    // The box establishes an independent formatting context for its content, a block formatting context with a block layout state of its own,
    // in the box's writing mode, with the last formatted lines of the boxes with trim-end in it.
    auto blockLayoutState = BlockLayoutState { { } };
    blockLayoutState.setLastFormattedLineRoots(Vector { m_blockLayoutState.lastFormattedLineRoots() });
    auto contentFormattingContext = BlockFormattingContext { box, integrationUtils(), blockLayoutState };
    // Its own text-box-trim only: its lines are not the first or last formatted line of the boxes it is in, which only look into their in-flow
    // children (css-pseudo-4 §2.1.1). The render tree path gives it a start trim an ancestor has not used yet (TextBoxTrimmer).
    if (box->style().textBoxTrim() != TextBoxTrim::None)
        blockLayoutState.setTextBoxTrimState(contentFormattingContext.textBoxTrimStateFor(box, { }));
    auto contentInlineSizes = outOfFlowGeometry.needsContentInlineSizes() ? std::optional { contentFormattingContext.computeIntrinsicInlineSizes(box, { }) } : std::nullopt;
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
    if (box->firstChild()) {
        auto horizontalConstraints = HorizontalConstraints { geometry.borderAndPaddingStart() + scrollbarInlineStartSpace(box, geometry), geometry.contentBoxWidth() };
        // Coverage keeps out the -webkit-fill-available block sizes inside, and the percentage block sizes of replaced elements in quirks mode.
        auto availableBlockSize = contentBoxBlockSize.value_or(0_lu);
        auto relayoutChildren = LayoutIntegration::BlockIntegrationUtils::contentBoxInlineSizeChanged(box, geometry.contentBoxWidth());
        auto contentResult = contentFormattingContext.layout({ { horizontalConstraints, geometry.borderAndPaddingBefore() }, contentBoxBlockSize, availableBlockSize, availableBlockSize, relayoutChildren }, geometry);
        intrinsicContentBlockSize = contentResult.intrinsicContentBlockSize;
        result.boxes.appendVector(WTF::move(contentResult.boxes));
        result.outOfFlowBoxes.appendVector(WTF::move(contentResult.outOfFlowBoxes));
    } else if (auto contentBlockSize = BlockFormattingUtils::emptyEditableContentBlockSize(box))
        intrinsicContentBlockSize = *contentBlockSize;
    // Size containment sizes the box as if it had no content (css-contain-2 §3.2, RenderBox::overrideLogicalHeightForSizeContainment).
    if (box->shouldApplySizeContainment())
        intrinsicContentBlockSize = box->explicitIntrinsicInnerLogicalHeight().value_or(0_lu);

    auto blockAxis = outOfFlowGeometry.computeBlockAxisGeometry(intrinsicContentBlockSize + geometry.verticalSpaceForScrollbar(), inlineAxis.borderBoxSize);
    setContentBoxBlockSize(geometry, blockAxis.borderBoxSize);

    // The result is in the logical coordinates of the containing block, whose inline axis is the box's block axis in an orthogonal flow. Border
    // and padding keep the box's own sides.
    if (outOfFlowGeometry.isOrthogonal()) {
        std::swap(inlineAxis, blockAxis);
        geometry.setBorder({ { geometry.borderBefore(), geometry.borderAfter() }, { geometry.borderStart(), geometry.borderEnd() } });
        geometry.setPadding({ { geometry.paddingBefore(), geometry.paddingAfter() }, { geometry.paddingStart(), geometry.paddingEnd() } });
        geometry.setContentBoxSize({ geometry.contentBoxHeight(), geometry.contentBoxWidth() });
    }
    geometry.setHorizontalMargin({ inlineAxis.marginStart, inlineAxis.marginEnd });
    geometry.setLeft(containingBlock.inlineStart + inlineAxis.borderBoxStart);
    geometry.setVerticalMargin({ blockAxis.marginStart, blockAxis.marginEnd });
    geometry.setTop(containingBlock.blockStart + blockAxis.borderBoxStart);

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
    auto contentBoxBlockSize = BlockFormattingUtils::definiteContentBoxBlockSize(blockContainer, geometry, percentageBasis(blockContainer, parentState), stretchFitBorderBoxBlockSize(blockContainer, geometry, parentState), root().writingMode(), containingBlockIsTableCell(parentState));
    return {
        .blockContainer = blockContainer,
        .geometry = geometry,
        .inlineRange = { geometry.borderAndPaddingStart() + scrollbarInlineStartSpace(blockContainer, geometry), geometry.contentBoxWidth() },
        .borderBoxLogicalLeftInRoot = inlineStartInRoot(parentState, BoxGeometry::borderBoxLeft(geometry), geometry.borderBoxWidth()),
        .hasInlineDirectionOppositeToRoot = blockContainer.writingMode().isLogicalLeftInlineStart() != root().writingMode().isLogicalLeftInlineStart(),
        .borderBoxLogicalTop = { },
        .borderAndPaddingBefore = geometry.borderAndPaddingBefore(),
        .contentBoxBlockSize = contentBoxBlockSize,
        .fillAvailableBlockSize = BlockFormattingUtils::availableBlockSize(blockContainer, geometry, contentBoxBlockSize, parentState.contentBoxBlockSize, parentState.fillAvailableBlockSize, stretchFitBorderBoxBlockSize(blockContainer, geometry, parentState), root().writingMode(), AvailableLogicalHeightType::ExcludeMarginBorderPadding),
        .replacedElementAvailableBlockSize = BlockFormattingUtils::availableBlockSize(blockContainer, geometry, contentBoxBlockSize, parentState.contentBoxBlockSize, parentState.replacedElementAvailableBlockSize, stretchFitBorderBoxBlockSize(blockContainer, geometry, parentState), root().writingMode(), AvailableLogicalHeightType::IncludeMarginBorderPadding),
        .margins = { },
        .isLaidOutThroughRenderTree = false,
        .relayoutChildren = LayoutIntegration::BlockIntegrationUtils::contentBoxInlineSizeChanged(blockContainer, geometry.contentBoxWidth()),
        .lineCountThroughLastBlockLevelBox = 0,
        .blockEndChildren = { { }, { }, geometry.borderAndPaddingBefore() }
    };
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

// The min-content and max-content sizes of a box whose preferred aspect ratio gives its inline size: the size the ratio gives, floored by the
// min-content size of its content when its min inline size is auto, the automatic minimum, through the calculation of a calc-size() there,
// where percentages are cyclic (RenderBox::computeIntrinsicKeywordLogicalWidths, applyAutomaticContentBasedMinimumSize).
static IntrinsicWidthConstraints aspectRatioContentInlineSizes(const RenderBox& box, LayoutUnit aspectRatioContentBoxInlineSize, IntrinsicWidthConstraints contentSizes, LayoutUnit borderAndPaddingInlineSize, WritingMode writingMode)
{
    CheckedRef style = box.style();
    auto& minimumSize = style->logicalMinWidth(writingMode);
    if (!minimumSize.isAuto() || !box.firstChild())
        return { aspectRatioContentBoxInlineSize, aspectRatioContentBoxInlineSize };
    auto contentMinimum = contentSizes.minimum;
    if (minimumSize.isCalcSize())
        contentMinimum = BlockFormattingUtils::calcSizeBorderBoxSize(minimumSize.calcSize(), style.get(), contentMinimum + borderAndPaddingInlineSize, borderAndPaddingInlineSize, 0_lu) - borderAndPaddingInlineSize;
    auto minimum = std::max(aspectRatioContentBoxInlineSize, contentMinimum);
    return { minimum, minimum };
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
    auto isLaidOutThroughRenderTree = !blockContainer || (BlockFormattingUtils::establishesIndependentFormattingContext(*blockContainer) && LayoutIntegration::BlockIntegrationUtils::laysOutThroughRenderTree(*blockContainer));
    if (isLaidOutThroughRenderTree && decidesItsOwnInlineSize(box, writingMode) && !(availableInlineSize && hasInlineSizeFromSpaceBesideFloats(box, writingMode)))
        return;

    CheckedRef containingBlockStyle = containingBlockState.blockContainer->style();
    // A preferred aspect ratio gives an auto inline size from a definite block size, and the min-content and max-content sizes of the box,
    // floored by the min-content size of its content when its min inline size is auto (RenderBox::applyAutomaticContentBasedMinimumSize).
    auto horizontalBorderAndPadding = geometry.horizontalBorderAndPadding();
    auto aspectRatioBorderBoxInlineSize = BlockFormattingUtils::aspectRatioBorderBoxInlineSize(box, horizontalBorderAndPadding, geometry.verticalBorderAndPadding(), containingBlockState.contentBoxBlockSize, stretchFitBorderBoxBlockSize(box, geometry, containingBlockState), writingMode);
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
            auto contentBoxBlockSize = contentBoxBlockSizeWhileMeasured(box, geometry, percentageBasis(box, containingBlockState), stretchFitBorderBoxBlockSize(box, geometry, containingBlockState), writingMode, containingBlockIsTableCell(containingBlockState));
            auto sizes = computeIntrinsicInlineSizes(*blockContainer, contentBoxBlockSize);
            // With its scrollbars (RenderBlockFlow::computeIntrinsicLogicalWidths).
            auto scrollbarInlineSize = LayoutUnit { box.intrinsicScrollbarLogicalWidthIncludingGutter() };
            return IntrinsicWidthConstraints { sizes.minimum + scrollbarInlineSize, sizes.maximum + scrollbarInlineSize };
        }();
        if (!aspectRatioBorderBoxInlineSize)
            return sizes;
        return aspectRatioContentInlineSizes(box, std::max(0_lu, *aspectRatioBorderBoxInlineSize - horizontalBorderAndPadding), sizes, horizontalBorderAndPadding, writingMode);
    }();
    // They apply to an inline size that is not a length or a percentage.
    auto aspectRatioInlineSizeLimits = std::optional<std::pair<LayoutUnit, LayoutUnit>> { };
    if (!style->logicalWidth(writingMode).isSpecified())
        aspectRatioInlineSizeLimits = BlockFormattingUtils::aspectRatioInlineSizeLimits(box, geometry.horizontalBorderAndPadding(), geometry.verticalBorderAndPadding(), containingBlockState.contentBoxBlockSize, stretchFitBorderBoxBlockSize(box, geometry, containingBlockState), writingMode);
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
    // percentages, which are cyclic here, count as zero and percentage inline sizes as auto (§5.2.1). Coverage keeps out percentage padding,
    // which the render tree path resolves against the containing block's current inline size.
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
            return aspectRatioContentInlineSizes(box, std::max(0_lu, *aspectRatioBorderBoxInlineSize - borderAndPadding), contentSizes, borderAndPadding, writingMode);
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
        auto contentBoxSizes = [&](auto& size, IntrinsicWidthConstraints ownSizes, std::optional<IntrinsicWidthConstraints> autoSizes) -> std::optional<IntrinsicWidthConstraints> {
            if (isFixed(size)) {
                auto value = Style::evaluate<LayoutUnit>(size, 0_lu, zoom);
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
    // among them go side by side, and so does a box after them that avoids floats, with its content when it is inline, while another box
    // goes below them (RenderBlock::computeBlockIntrinsicLogicalWidths).
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
        auto avoidsFloats = box.avoidsFloats() || (box.isAnonymousBlock() && box.childrenInline());
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
std::optional<LayoutUnit> BlockFormattingContext::stretchFitBorderBoxBlockSize(const RenderBox& box, const BoxGeometry& geometry, const ContainingBlockState& containingBlockState) const
{
    // -webkit-fill-available resolves to what it did before the stretch keyword: the available block size, margins included (RenderBox::computeSizingKeywordLogicalContentHeightUsing).
    if (BlockFormattingUtils::hasFillAvailableBlockSize(box, root().writingMode()))
        return containingBlockState.fillAvailableBlockSize;
    auto containingBlockContentBoxBlockSize = stretchFitBasis(containingBlockState);
    if (!containingBlockContentBoxBlockSize)
        return { };
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
    return std::max(0_lu, *containingBlockContentBoxBlockSize - marginBefore - marginAfter);
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
    auto blockSize = BlockFormattingUtils::borderBoxBlockSize(box, borderAndPaddingBlockSize, intrinsicContentBlockSize + scrollbarBlockSize, percentageBasis(box, containingBlockState), stretchFitBorderBoxBlockSize(box, geometry, containingBlockState), root().writingMode(), containingBlockIsTableCell(containingBlockState));
    if (auto aspectRatioBlockSize = BlockFormattingUtils::aspectRatioBorderBoxBlockSize(box, geometry.borderBoxWidth(), geometry.horizontalBorderAndPadding(), borderAndPaddingBlockSize, containingBlockState.contentBoxBlockSize, stretchFitBorderBoxBlockSize(box, geometry, containingBlockState), root().writingMode(), intrinsicContentBlockSize))
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
    computeLogicalLeft(box, containingBlockState, geometry);
    return containingBlockStateFor(box, geometry, containingBlockState);
}

ContainingBlockState BlockFormattingContext::computeContainingBlockStateAtBlockStart(const RenderBox& box)
{
    auto blockContainerState = containingBlockStateAtBlockStart(box);
    m_blockLayoutState.marginCollapse().startBlockContainer(blockContainerState, m_blockLayoutState.containingBlockStates().last(), blockContainerState.geometry.marginBefore());
    return blockContainerState;
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
    auto isIndefinite = blockSize.isAuto() && (maximumBlockSize.isNone() || maximumBlockSize.isMaxContent()) && !containingBlock->document().inQuirksMode() && !style->aspectRatio().hasRatio();
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

LayoutUnit BlockFormattingContext::viewportStretchedBorderBoxBlockSize(LayoutUnit collapsedMarginBefore, LayoutUnit collapsedMarginAfter) const
{
    auto& html = root();
    auto spaceAround = collapsedMarginBefore + collapsedMarginAfter + html.marginBefore(html.writingMode()) + html.marginAfter(html.writingMode()) + html.borderAndPaddingLogicalHeight();
    CheckedRef view = html.view();
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
    if (box.stretchesToViewport()) {
        auto collapsedMarginAfter = geometry.marginAfter();
        if (marginAfterCollapsesWithChildren) {
            auto margins = m_blockLayoutState.marginCollapse().adjoiningMargins();
            margins.add(geometry.marginAfter());
            collapsedMarginAfter = margins.collapsedValue();
        }
        geometry.setContentBoxHeight(std::max(geometry.contentBoxHeight(), viewportStretchedBorderBoxBlockSize(collapsedMarginsBefore.collapsedValue(), collapsedMarginAfter) - geometry.verticalBorderAndPadding()));
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
    for (auto index : children.collapsedThroughChildren)
        result.boxes[index].geometry.setTop(logicalTop);
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
            m_needsLayoutWithResolvedBorderBoxLogicalTops |= pending.isPlacedAgainstOtherFloats || pending.isPlacedAgainstOtherPendingFloats;
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
    if (!didMoveFloats)
        return;
    placedFloats.clear();
    for (auto& floatItem : floats)
        placedFloats.add(WTF::move(floatItem));
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
