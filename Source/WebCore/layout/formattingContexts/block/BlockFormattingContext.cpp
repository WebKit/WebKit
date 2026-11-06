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
        auto isBlockLevelBoxStart = blockItem.type() == BlockItem::Type::BlockContainerStart || blockItem.type() == BlockItem::Type::IndependentFormattingContextRoot;
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

void BlockFormattingContext::handleBlockContainerStart(const RenderBox& blockContainer)
{
    LayoutIntegration::BlockIntegrationUtils::markPercentageHeightDescendantsForLayout(downcast<RenderBlockFlow>(blockContainer));
    auto blockContainerState = computeContainingBlockStateAtBlockStart(blockContainer);
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
    // FIXME: Floats need the block container's top before the inline content is laid out; decide whether it has line boxes from its
    // inline items then.
    auto inlineContentLayoutResult = integrationUtils().layoutInlineContent(downcast<RenderBlockFlow>(blockContainer), constraintsForInlineContent(blockContainer, blockContainerState), geometry, blockContainerState.contentBoxBlockSize, blockContainerState.replacedElementAvailableBlockSize, orthogonalFlowAvailableInlineSpace(blockContainerState), blockContainerState.relayoutChildren, m_blockLayoutState, *this, result);
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
BlockLevelBoxInInlineContent BlockFormattingContext::layoutBlockLevelBoxInInlineContent(const RenderBox& box, LayoutUnit lineLogicalTop, size_t lineCount, size_t lineCountForLineClamp, BlockLayoutResult& result)
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

void BlockFormattingContext::handleIndependentFormattingContextRoot(const RenderBox& box, BlockLayoutResult& result)
{
    auto rootState = containingBlockStateAtBlockStart(box);
    // Its margins do not collapse with its children's, nor through it, even when it is empty (CSS 2.2 §8.3.1). With a block step, they
    // collapse once the block size is known, which its content, in a formatting context of its own, does not depend on the position for.
    auto stepSize = blockStepSize(box);
    if (!stepSize) {
        m_blockLayoutState.marginCollapse().startBlockContainer(rootState, m_blockLayoutState.containingBlockStates().last(), rootState.geometry.marginBefore());
        resolveBorderBoxLogicalTops(rootState);
    }

    auto contentBlockSize = std::optional<LayoutUnit> { };
    CheckedPtr root = dynamicDowncast<RenderBlockFlow>(box);
    if (!root || LayoutIntegration::BlockIntegrationUtils::laysOutThroughRenderTree(*root)) {
        layoutIndependentFormattingContextThroughRenderTree(box, rootState);
        contentBlockSize = rootState.geometry.contentBoxHeight();
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
            auto state = m_blockLayoutState.textBoxTrimState();
            if (state.trimsFirstFormattedLine && rootState.geometry.borderAndPaddingBefore()) {
                state.trimsFirstFormattedLine = false;
                m_blockLayoutState.setTextBoxTrimState(state);
            }
            blockLayoutState.setTextBoxTrimState(textBoxTrimStateFor(*root, state));
        }
        auto horizontalConstraints = HorizontalConstraints { rootState.inlineRange.min(), rootState.inlineRange.size() };
        auto rootResult = BlockFormattingContext { *root, integrationUtils(), blockLayoutState }.layout({ { horizontalConstraints, rootState.borderAndPaddingBefore }, rootState.contentBoxBlockSize, rootState.fillAvailableBlockSize, rootState.replacedElementAvailableBlockSize, rootState.relayoutChildren }, rootState.geometry);
        result.boxes.appendVector(WTF::move(rootResult.boxes));
        contentBlockSize = rootResult.intrinsicContentBlockSize;
        result.outOfFlowBoxes.appendVector(WTF::move(rootResult.outOfFlowBoxes));
    }

    if (stepSize) {
        auto& geometry = rootState.geometry;
        if (!rootState.isLaidOutThroughRenderTree)
            computeBlockSize(box, contentBlockSize.value_or(0_lu), m_blockLayoutState.containingBlockStates().last(), geometry);
        auto halfExtraSpace = BlockStepSizing::computeExtraSpace(*stepSize, geometry.marginBefore() + geometry.borderBoxHeight() + geometry.marginAfter()) / 2;
        geometry.setVerticalMargin({ geometry.marginBefore() + halfExtraSpace, geometry.marginAfter() + halfExtraSpace });
        m_blockLayoutState.marginCollapse().startBlockContainer(rootState, m_blockLayoutState.containingBlockStates().last(), geometry.marginBefore());
        resolveBorderBoxLogicalTops(rootState);
    }
    if (contentBlockSize)
        m_blockLayoutState.marginCollapse().resetAdjoiningMargins(rootState.contentBoxLogicalTop() + *contentBlockSize);
    m_blockLayoutState.marginCollapse().endBlockContainer(rootState, m_blockLayoutState.containingBlockStates().last());
    placeInContainingBlock(box, WTF::move(rootState), result);
}

// The inline size of a box laid out through the render tree that its own formatting context decides: of a table (CSS 2.2 §17.5.2), of a
// replaced element (§10.3.4), of a frameset, which takes the size of the viewport (RenderFrameSet::layout), of a box that fits its content,
// like a button (RenderBox::sizesLogicalWidthToFitContent), or that has a sizing keyword inline size, which it resolves with its own
// intrinsic sizes (css-sizing-3 §5.1), and of an orthogonal box, for which it is the block size (css-writing-modes-4 §7.3).
static bool decidesItsOwnInlineSize(const RenderBox& box, WritingMode writingMode)
{
    return is<RenderTable>(box) || box.isRenderReplaced() || box.isRenderOrLegacyRenderSVGRoot() || box.isRenderFrameSet() || box.sizesLogicalWidthToFitContent() || BlockFormattingUtils::hasIntrinsicInlineSize(box, writingMode)
        || box.writingMode().isOrthogonal(writingMode) || box.style().aspectRatio().hasRatio();
}

// Everything that belongs to the block formatting context is computed here: the margins, the position, the containing block and, for a box
// whose inline size its own formatting context does not decide, the stretch-fit inline size. The rest comes back from the box's own formatting
// context, laid out through the render tree, which lays out its content: the inline size it decides (decidesItsOwnInlineSize), the block size,
// border and padding.
void BlockFormattingContext::layoutIndependentFormattingContextThroughRenderTree(const RenderBox& box, ContainingBlockState& boxState)
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
        .borderBoxInlineSize = decidesItsOwnInlineSize(box, root().writingMode()) ? std::nullopt : std::optional { geometry.borderBoxWidth() }
    };
    auto layoutResult = integrationUtils().layoutIndependentFormattingContext(box, constraints);

    geometry.setBorder(layoutResult.border);
    geometry.setPadding(layoutResult.padding);
    geometry.setContentBoxWidth(layoutResult.borderBoxInlineSize - geometry.horizontalBorderAndPadding());
    geometry.setContentBoxHeight(layoutResult.borderBoxBlockSize - geometry.verticalBorderAndPadding());
    CheckedRef containingBlockStyle = containingBlockState.blockContainer->style();
    auto [marginStart, marginEnd] = BlockFormattingUtils::inlineMargins(box, layoutResult.borderBoxInlineSize, containingBlockInlineSize, containingBlockStyle);
    geometry.setHorizontalMargin({ marginStart, marginEnd });
    computeLogicalLeft(box, containingBlockState, geometry);
    boxState.borderAndPaddingBefore = geometry.borderAndPaddingBefore();
    boxState.isLaidOutThroughRenderTree = true;
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

    // A box that was inline-level before it got blockified goes where text-align puts the start of a line (RenderBlockFlow::staticInlinePositionForOriginalDisplayInline).
    auto& inlineRange = parentState.inlineRange;
    auto staticInlinePosition = [&] {
        if (!box.style().originalDisplay().isInlineType())
            return inlineRange.min();
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

// A replaced element or a flex container, whose own formatting context lays it out through the render tree; the block formatting context
// places it, and sizes the flex container (css-position-3 §4.3), whose block size comes from its content when it is auto.
BlockLayoutResult BlockFormattingContext::layoutOutOfFlowIndependentFormattingContext(const RenderBox& root, const OutOfFlowContainingBlock& containingBlock, const OutOfFlowGeometry::StaticPosition& staticPosition, const std::optional<OutOfFlowGeometry::DefaultAnchor>& defaultAnchor)
{
    auto geometry = computeBorderAndPadding(root, containingBlock.inlineSize);
    auto outOfFlowGeometry = OutOfFlowGeometry { root, { containingBlock.inlineSize, containingBlock.blockSize, containingBlock.writingMode, containingBlock.isScrollContainer, containingBlock.scrollableContainingBlock, containingBlock.fillAvailableBlockSize }, staticPosition, defaultAnchor, geometry.horizontalBorderAndPadding(), geometry.verticalBorderAndPadding() };
    auto [inlineAxis, blockAxis] = [&] -> std::pair<OutOfFlowGeometry::AxisGeometry, OutOfFlowGeometry::AxisGeometry> {
        if (root.isRenderReplaced() || root.isRenderOrLegacyRenderSVGRoot()) {
            // From its natural size and aspect ratio (CSS 2.2 §10.3.8, §10.6.5).
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
        auto borderBoxSize = integrationUtils().layoutOutOfFlowIndependentFormattingContext(root, inlineAxis.borderBoxSize, borderBoxBlockSize);
        return { inlineAxis, outOfFlowGeometry.computeBlockAxisGeometry(borderBoxSize.height() - geometry.verticalBorderAndPadding(), inlineAxis.borderBoxSize) };
    }();
    geometry.setContentBoxWidth(inlineAxis.borderBoxSize - geometry.horizontalBorderAndPadding());
    geometry.setHorizontalMargin({ inlineAxis.marginStart, inlineAxis.marginEnd });
    geometry.setLeft(containingBlock.inlineStart + inlineAxis.borderBoxStart);
    geometry.setContentBoxHeight(blockAxis.borderBoxSize - geometry.verticalBorderAndPadding());
    geometry.setVerticalMargin({ blockAxis.marginStart, blockAxis.marginEnd });
    geometry.setTop(containingBlock.blockStart + blockAxis.borderBoxStart);

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
    if (!is<RenderBlockFlow>(outOfFlowBox))
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
    auto contentInlineSizes = outOfFlowGeometry.needsContentInlineSizes() ? std::optional { contentFormattingContext.computeIntrinsicInlineSizes(box) } : std::nullopt;
    auto inlineAxis = outOfFlowGeometry.computeInlineAxisGeometry(contentInlineSizes);
    geometry.setContentBoxWidth(inlineAxis.borderBoxSize - geometry.horizontalBorderAndPadding());
    auto contentBoxBlockSize = outOfFlowGeometry.definiteContentBoxBlockSize(inlineAxis.borderBoxSize);

    auto intrinsicContentBlockSize = LayoutUnit { };
    if (box->firstChild()) {
        auto horizontalConstraints = HorizontalConstraints { geometry.borderAndPaddingStart(), geometry.contentBoxWidth() };
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

    auto blockAxis = outOfFlowGeometry.computeBlockAxisGeometry(intrinsicContentBlockSize, inlineAxis.borderBoxSize);
    geometry.setContentBoxHeight(blockAxis.borderBoxSize - geometry.verticalBorderAndPadding());

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
        .inlineRange = { geometry.borderAndPaddingStart(), geometry.contentBoxWidth() },
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
    return geometry;
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

void BlockFormattingContext::computeInlineSizeAndMargins(const RenderBox& box, const ContainingBlockState& containingBlockState, BoxGeometry& geometry) const
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
    if (isLaidOutThroughRenderTree && decidesItsOwnInlineSize(box, writingMode))
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
        auto sizes = computeIntrinsicInlineSizes(*blockContainer);
        if (!aspectRatioBorderBoxInlineSize)
            return sizes;
        return aspectRatioContentInlineSizes(box, std::max(0_lu, *aspectRatioBorderBoxInlineSize - horizontalBorderAndPadding), sizes, horizontalBorderAndPadding, writingMode);
    }();
    // They apply to an inline size that is not a length or a percentage.
    auto aspectRatioInlineSizeLimits = std::optional<std::pair<LayoutUnit, LayoutUnit>> { };
    if (!style->logicalWidth(writingMode).isSpecified())
        aspectRatioInlineSizeLimits = BlockFormattingUtils::aspectRatioInlineSizeLimits(box, geometry.horizontalBorderAndPadding(), geometry.verticalBorderAndPadding(), containingBlockState.contentBoxBlockSize, stretchFitBorderBoxBlockSize(box, geometry, containingBlockState), writingMode);
    auto [borderBoxInlineSize, marginStart, marginEnd] = BlockFormattingUtils::inlineSizeAndMargins(box, geometry.horizontalBorderAndPadding(), containingBlockInlineSize, containingBlockStyle, contentInlineSizes, aspectRatioInlineSizeLimits, aspectRatioBorderBoxInlineSize);
    geometry.setContentBoxWidth(borderBoxInlineSize - geometry.horizontalBorderAndPadding());
    // Unlike border and padding, start and end margins follow the direction of the containing block.
    geometry.setHorizontalMargin({ marginStart, marginEnd });
}

// Goes through the block items of the block container's content the way layout does, collecting the outer min-content and max-content
// contributions of the boxes in it (§5.2).
// A box with inline size containment measures as if it had no content, by contain-intrinsic-inline-size (css-contain-2 §3.3,
// RenderBlockFlow::computeIntrinsicLogicalWidths).
static std::optional<IntrinsicWidthConstraints> containedIntrinsicInlineSizes(const RenderBox& box)
{
    if (!box.shouldApplySizeOrInlineSizeContainment())
        return { };
    auto inlineSize = box.explicitIntrinsicInnerLogicalWidth().value_or(0_lu);
    return IntrinsicWidthConstraints { inlineSize, inlineSize };
}

IntrinsicWidthConstraints BlockFormattingContext::computeIntrinsicInlineSizes(const RenderBlockFlow& blockContainer) const
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
        return margin(style->marginStart(writingMode)) + margin(style->marginEnd(writingMode));
    };
    auto outerInlineSizes = [&](const RenderBox& box, IntrinsicWidthConstraints contentSizes) -> IntrinsicWidthConstraints {
        CheckedRef style = box.style();
        auto zoom = style->usedZoomForLength();
        auto borderAndPadding = computeBorderAndPadding(box, 0_lu).horizontalBorderAndPadding();
        auto verticalBorderAndPadding = computeBorderAndPadding(box, 0_lu).verticalBorderAndPadding();
        // A preferred aspect ratio gives the min-content and max-content sizes of the box from a fixed block size, floored by the min-content
        // size of its content when its min inline size is auto (RenderBox::applyAutomaticContentBasedMinimumSize); coverage keeps out the other
        // definite block sizes, which resolve against the containing block.
        auto aspectRatioBorderBoxInlineSize = BlockFormattingUtils::aspectRatioBorderBoxInlineSize(box, borderAndPadding, verticalBorderAndPadding, { }, { }, writingMode);
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
        // A preferred aspect ratio that gives the auto block size transfers the fixed min and max block sizes (RenderBox::applyTransferredMinMaxSizesFromAspectRatio);
        // coverage keeps out the others, which resolve against the containing block. The sizing keywords of the min and max inline sizes are the
        // box's own sizes with them, which an indefinite maximum does not cap (css-sizing-4 §5.3, RenderBox::constrainIntrinsicLogicalWidthsByMinMax).
        auto transferredBoxSizes = boxSizes;
        if (!isFixed(inlineSize) && style->logicalHeight(writingMode).isAuto()) {
            if (auto limits = BlockFormattingUtils::aspectRatioInlineSizeLimits(box, borderAndPadding, verticalBorderAndPadding, { }, { }, writingMode)) {
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
        auto outerSpace = borderAndPadding + inlineMargins(box);
        return { minimum + outerSpace, maximum + outerSpace };
    };
    auto addContribution = [](IntrinsicWidthConstraints& containerSizes, IntrinsicWidthConstraints contribution) {
        containerSizes.minimum = std::max(containerSizes.minimum, contribution.minimum);
        containerSizes.maximum = std::max(containerSizes.maximum, contribution.maximum);
    };

    // Each block container collects the contributions of its children between its start and end.
    auto blockContainerSizes = Vector<IntrinsicWidthConstraints> { { } };
    for (auto& blockItem : BlockItemsBuilder { blockContainer }.build()) {
        CheckedRef box = blockItem.renderer();
        switch (blockItem.type()) {
        case BlockItem::Type::BlockContainerStart:
            blockContainerSizes.constructAndAppend();
            break;
        case BlockItem::Type::InlineContent:
            addContribution(blockContainerSizes.last(), LayoutIntegration::BlockIntegrationUtils::inlineContentIntrinsicInlineSizes(downcast<RenderBlockFlow>(box)));
            break;
        case BlockItem::Type::BlockContainerEnd: {
            auto contentSizes = blockContainerSizes.takeLast();
            if (auto sizes = containedIntrinsicInlineSizes(box))
                contentSizes = *sizes;
            addContribution(blockContainerSizes.last(), outerInlineSizes(box, contentSizes));
            break;
        }
        case BlockItem::Type::IndependentFormattingContextRoot:
            if (LayoutIntegration::BlockIntegrationUtils::laysOutThroughRenderTree(box)) {
                // Its own formatting context measures a box it lays out, against the size of the containing block only where coverage keeps
                // it out (canComputeContentInlineSizes).
                auto contributions = LayoutIntegration::BlockIntegrationUtils::independentFormattingContextIntrinsicInlineSizeContributions(box);
                auto margins = inlineMargins(box);
                addContribution(blockContainerSizes.last(), { contributions.minimum + margins, contributions.maximum + margins });
                break;
            }
            addContribution(blockContainerSizes.last(), outerInlineSizes(box, computeIntrinsicInlineSizes(downcast<RenderBlockFlow>(box))));
            break;
        case BlockItem::Type::OutOfFlowBox:
            // Out-of-flow boxes do not contribute to the intrinsic sizes of their parent (css-sizing-3 §5.2).
            break;
        }
    }
    ASSERT(blockContainerSizes.size() == 1);
    auto sizes = blockContainerSizes.first();
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
    auto blockSize = BlockFormattingUtils::borderBoxBlockSize(box, borderAndPaddingBlockSize, intrinsicContentBlockSize, percentageBasis(box, containingBlockState), stretchFitBorderBoxBlockSize(box, geometry, containingBlockState), root().writingMode(), containingBlockIsTableCell(containingBlockState));
    if (auto aspectRatioBlockSize = BlockFormattingUtils::aspectRatioBorderBoxBlockSize(box, geometry.borderBoxWidth(), geometry.horizontalBorderAndPadding(), borderAndPaddingBlockSize, containingBlockState.contentBoxBlockSize, stretchFitBorderBoxBlockSize(box, geometry, containingBlockState), root().writingMode(), intrinsicContentBlockSize))
        blockSize = *aspectRatioBlockSize;
    geometry.setContentBoxHeight(blockSize - borderAndPaddingBlockSize);
}

ConstraintsForInlineContent BlockFormattingContext::constraintsForInlineContent(const RenderBox& blockContainer, const ContainingBlockState& blockContainerState) const
{
    // Like the line boxes the inline formatting context produces, these are relative to the border box of the block container, in its own direction.
    auto& geometry = blockContainerState.geometry;
    auto horizontalConstraints = HorizontalConstraints { blockContainerState.inlineRange.min(), blockContainerState.inlineRange.size() };
    auto visualLeft = blockContainer.writingMode().isLogicalLeftInlineStart() ? geometry.borderAndPaddingStart() : geometry.borderAndPaddingEnd();
    // The vertical scrollbar of a root sits between its left border and padding (LayoutIntegration::BoxGeometryUpdater::formattingContextConstraints),
    // which is in the block axis in vertical writing modes.
    if (blockContainer.shouldPlaceVerticalScrollbarOnLeft() && blockContainer.writingMode().isHorizontal())
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
    // FIXME: Clearance separates the block-start margin from the margins before it (CSS 2.2 §9.5.2).
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

void BlockFormattingContext::placeInContainingBlock(const RenderBox& box, ContainingBlockState&& blockContainerState, BlockLayoutResult& result)
{
    auto& containingBlockState = m_blockLayoutState.containingBlockStates().last();
    // A box is placed once its containing block's top is known, and the result is relative to that top, not to the root's.
    ASSERT(containingBlockState.borderBoxLogicalTop || !blockContainerState.borderBoxLogicalTop);
    auto& geometry = blockContainerState.geometry;
    auto& collapsedMarginsBefore = blockContainerState.margins.collapsedMarginsBefore();

    if (!blockContainerState.borderBoxLogicalTop) {
        // Its margin-trim at the block-end edge discards the margins of its content, which all collapse with its last child's block-end margin.
        if (auto& margins = blockContainerState.adjoiningMarginsAtContentStart)
            m_blockLayoutState.marginCollapse().resetAdjoiningMargins(m_blockLayoutState.marginCollapse().logicalTop(), *margins);
        // The margins collapse through the box, which has no height and leaves them open. Its top border edge is its parent's when they
        // include the parent's block-start margin, and otherwise where it would be with a non-zero bottom border (CSS 2.2 §8.3.1).
        computeBlockSize(box, { }, containingBlockState, geometry);
        ASSERT(!geometry.borderBoxHeight());
        if (containingBlockState.borderBoxLogicalTop)
            geometry.setTop(m_blockLayoutState.marginCollapse().collapsedLogicalBottomForCollapsedThroughBox() - *containingBlockState.borderBoxLogicalTop);
        else
            geometry.setTop({ });
        m_blockLayoutState.marginCollapse().addCollapsedThroughBlockEndMargin(blockContainerState, containingBlockState, geometry.marginAfter());
        auto collapsedMarginsAfter = AdjoiningMargins { };
        collapsedMarginsAfter.add(geometry.marginAfter());
        result.boxes.append({ box, WTF::move(geometry), { }, collapsedMarginsBefore, collapsedMarginsAfter, blockContainerState.margins.collapsedMarginsBeforeAreQuirky(), box.style().marginAfter().hasQuirk(), false });
        containingBlockState.blockEndChildren.collapsedThroughChildren.append(result.boxes.size() - 1);
        return;
    }

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
    auto marginAfterCollapsesWithChildren = !establishesBlockFormattingContext() && !geometry.borderAndPaddingAfter() && BlockFormattingUtils::blockSizeBehavesAsAuto(style->logicalHeight(writingMode), containingBlockState.contentBoxBlockSize);
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
    for (auto& containingBlockState : m_blockLayoutState.containingBlockStates() | std::views::reverse) {
        if (containingBlockState.borderBoxLogicalTop)
            break;
        containingBlockState.borderBoxLogicalTop = borderBoxLogicalTop;
    }
    // After the loop: the block container is still on the stack when its line boxes separate the margins, and its top would stop the loop.
    blockContainerState.borderBoxLogicalTop = borderBoxLogicalTop;
    m_blockLayoutState.marginCollapse().resetAdjoiningMargins(blockContainerState.contentBoxLogicalTop());
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
