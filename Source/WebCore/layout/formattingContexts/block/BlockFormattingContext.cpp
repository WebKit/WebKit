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
#include "Element.h"
#include "OutOfFlowGeometry.h"
#include "RenderBlockFlow.h"
#include "RenderBoxInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderTable.h"
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

BlockFormattingContext::BlockFormattingContext(const RenderBlockFlow& root, LayoutIntegration::BlockIntegrationUtils& integrationUtils, BlockLayoutState& blockLayoutState)
    : m_root(root)
    , m_integrationUtils(integrationUtils)
    , m_blockLayoutState(blockLayoutState)
{
}

BlockLayoutResult BlockFormattingContext::layout(const ConstraintsForBlockContent& constraints)
{
    ASSERT(m_blockLayoutState.containingBlockStates().isEmpty());
    auto result = BlockLayoutResult { };
    m_blockLayoutState.containingBlockStates().append(containingBlockStateForRoot(constraints));
    m_blockLayoutState.marginCollapse().startBlockFormattingContextRoot(m_blockLayoutState.containingBlockStates().last());
    // The margins of a box that establishes a block formatting context do not collapse with its children's (CSS 2.2 §8.3.1).
    m_blockLayoutState.marginCollapse().resetAdjoiningMargins(m_blockLayoutState.containingBlockStates().last().contentBoxLogicalTop());

    // FIXME: Keep the block items across layouts and invalidate them on tree and formatting context participation changes.
    auto blockItems = BlockItemList { };
    if (root().childrenInline()) {
        // A root with only inline-level content establishes an inline formatting context for it (CSS Display 3), in the geometry its parent
        // formatting context gave it.
        auto& rootState = m_blockLayoutState.containingBlockStates().last();
        rootState.geometry = LayoutIntegration::BlockIntegrationUtils::rootGeometry(root());
        handleInlineContent(root(), rootState, m_blockLayoutState);
    } else
        blockItems = BlockItemsBuilder { root() }.build();

    for (auto& blockItem : blockItems) {
        CheckedRef box = blockItem.renderer();
        switch (blockItem.type()) {
        case BlockItem::Type::BlockContainerStart:
            handleBlockContainerStart(box);
            break;
        case BlockItem::Type::BlockContainerEnd:
            handleBlockContainerEnd(box, result);
            break;
        case BlockItem::Type::InlineContent:
            handleInlineContent(box, m_blockLayoutState.containingBlockStates().last(), m_blockLayoutState);
            break;
        case BlockItem::Type::IndependentFormattingContextRoot:
            handleIndependentFormattingContextRoot(box, result);
            break;
        case BlockItem::Type::OutOfFlowBox:
            handleOutOfFlowBox(box, result);
            break;
        }
    }

    ASSERT(m_blockLayoutState.containingBlockStates().size() == 1);
    // The content of the root ends at the bottom margin edge of its last in-flow child (CSS 2.2 §10.6.7), except for a quirky one in a
    // quirk container. Negative margins may pull that above its content box top.
    auto& rootState = m_blockLayoutState.containingBlockStates().last();
    m_blockLayoutState.marginCollapse().endBlockFormattingContextRoot(rootState);
    auto contentLogicalBottom = BlockMarginCollapse::isQuirkContainer(root()) && m_blockLayoutState.marginCollapse().endsWithQuirk() ? m_blockLayoutState.marginCollapse().logicalTop() : m_blockLayoutState.marginCollapse().collapsedLogicalBottom();
    result.intrinsicContentBlockSize = std::max(0_lu, contentLogicalBottom - rootState.contentBoxLogicalTop());
    m_blockLayoutState.containingBlockStates().clear();
    return result;
}

void BlockFormattingContext::handleBlockContainerStart(const RenderBox& blockContainer)
{
    LayoutIntegration::BlockIntegrationUtils::markPercentageHeightDescendantsForLayout(downcast<RenderBlockFlow>(blockContainer));
    auto blockContainerState = computeContainingBlockStateAtBlockStart(blockContainer);
    if (blockContainerState.geometry.borderAndPaddingBefore())
        resolveBorderBoxLogicalTops(blockContainerState);
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

void BlockFormattingContext::handleBlockContainerEnd(const RenderBox& blockContainer, BlockLayoutResult& result)
{
    auto blockContainerState = m_blockLayoutState.containingBlockStates().takeLast();
    ASSERT(blockContainerState.blockContainer.ptr() == &blockContainer);
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
        return BlockMarginCollapse::marginsCollapseThrough(blockContainer, blockContainerState.geometry, containingBlockState.contentBoxBlockSize, stretchFitBorderBoxBlockSize(blockContainer, blockContainerState.geometry, containingBlockState), root().writingMode());
    };
    if (!blockContainerState.borderBoxLogicalTop && !marginsCollapseThrough())
        resolveBorderBoxLogicalTops(blockContainerState);
    placeInContainingBlock(blockContainer, WTF::move(blockContainerState), result);
}

void BlockFormattingContext::handleInlineContent(const RenderBox& blockContainer, ContainingBlockState& blockContainerState, const BlockLayoutState& inlineContentBlockLayoutState)
{
    ASSERT(blockContainerState.blockContainer.ptr() == &blockContainer);
    // FIXME: Floats need the block container's top before the inline content is laid out; decide whether it has line boxes from its
    // inline items then.
    auto inlineContentLayoutResult = integrationUtils().layoutInlineContent(downcast<RenderBlockFlow>(blockContainer), constraintsForInlineContent(blockContainer, blockContainerState), blockContainerState.geometry, blockContainerState.contentBoxBlockSize, blockContainerState.relayoutChildren, inlineContentBlockLayoutState);
    if (!inlineContentLayoutResult.hasLineBoxes)
        return;
    // The first line box separates the block-start margin from what follows (CSS 2.2 §8.3.1).
    if (!blockContainerState.borderBoxLogicalTop)
        resolveBorderBoxLogicalTops(blockContainerState);
    m_blockLayoutState.marginCollapse().resetAdjoiningMarginsAfterLineBoxes(blockContainerState.contentBoxLogicalTop() + inlineContentLayoutResult.contentBlockSize);
}

void BlockFormattingContext::handleIndependentFormattingContextRoot(const RenderBox& box, BlockLayoutResult& result)
{
    auto rootState = computeContainingBlockStateAtBlockStart(box);
    // Its margins do not collapse with its children's, nor through it, even when it is empty (CSS 2.2 §8.3.1).
    resolveBorderBoxLogicalTops(rootState);

    CheckedPtr root = dynamicDowncast<RenderBlockFlow>(box);
    // Its content is in a formatting context of its own; a block formatting context gets a block layout state of its own.
    auto blockLayoutState = BlockLayoutState { { } };
    if (!root || LayoutIntegration::BlockIntegrationUtils::laysOutThroughRenderTree(*root))
        layoutIndependentFormattingContextThroughRenderTree(box, rootState);
    else if (!root->firstChild())
        handleEmptyEditableContent(box, rootState);
    else if (root->childrenInline()) {
        LayoutIntegration::BlockIntegrationUtils::markPercentageHeightDescendantsForLayout(*root);
        // A block container with only inline-level content establishes an inline formatting context for it (CSS Display 3).
        handleInlineContent(box, rootState, blockLayoutState);
    } else {
        LayoutIntegration::BlockIntegrationUtils::markPercentageHeightDescendantsForLayout(*root);
        auto horizontalConstraints = HorizontalConstraints { rootState.inlineRange.min(), rootState.inlineRange.size() };
        auto rootResult = BlockFormattingContext { *root, integrationUtils(), blockLayoutState }.layout({ { horizontalConstraints, rootState.borderAndPaddingBefore }, rootState.contentBoxBlockSize, rootState.fillAvailableBlockSize, rootState.relayoutChildren });
        result.boxes.appendVector(WTF::move(rootResult.boxes));
        m_blockLayoutState.marginCollapse().resetAdjoiningMargins(rootState.contentBoxLogicalTop() + rootResult.intrinsicContentBlockSize);
        result.outOfFlowBoxes.appendVector(WTF::move(rootResult.outOfFlowBoxes));
    }
    m_blockLayoutState.marginCollapse().endBlockContainer(rootState, m_blockLayoutState.containingBlockStates().last());
    placeInContainingBlock(box, WTF::move(rootState), result);
}

// The inline size of a box laid out through the render tree that its own formatting context decides: of a table (CSS 2.2 §17.5.2), of a
// replaced element (§10.3.4), of a box that fits its content, like a button (RenderBox::sizesLogicalWidthToFitContent), or that has a
// sizing keyword inline size, which it resolves with its own intrinsic sizes (css-sizing-3 §5.1), and of an orthogonal box, for which it is
// the block size (css-writing-modes-4 §7.3).
static bool decidesItsOwnInlineSize(const RenderBox& box, WritingMode writingMode)
{
    return is<RenderTable>(box) || box.isRenderReplaced() || box.isRenderOrLegacyRenderSVGRoot() || box.sizesLogicalWidthToFitContent() || BlockFormattingUtils::hasIntrinsicInlineSize(box, writingMode)
        || box.writingMode().isOrthogonal(writingMode);
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
    auto constraints = ConstraintsForIndependentFormattingContext {
        .containingBlockInlineSize = containingBlockInlineSize,
        .containingBlockBlockSize = containingBlockState.contentBoxBlockSize,
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
    m_blockLayoutState.marginCollapse().resetAdjoiningMargins(boxState.contentBoxLogicalTop() + geometry.contentBoxHeight());
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
BlockLayoutResult BlockFormattingContext::layoutOutOfFlowIndependentFormattingContext(const RenderBox& root, const OutOfFlowContainingBlock& containingBlock, const OutOfFlowGeometry::StaticPosition& staticPosition)
{
    auto geometry = computeBorderAndPadding(root, containingBlock.inlineSize);
    auto outOfFlowGeometry = OutOfFlowGeometry { root, { containingBlock.inlineSize, containingBlock.blockSize, containingBlock.box->writingMode() }, staticPosition, geometry.horizontalBorderAndPadding(), geometry.verticalBorderAndPadding() };
    auto [inlineAxis, blockAxis] = [&] -> std::pair<OutOfFlowGeometry::AxisGeometry, OutOfFlowGeometry::AxisGeometry> {
        if (root.isRenderReplaced() || root.isRenderOrLegacyRenderSVGRoot()) {
            // From its natural size and aspect ratio (CSS 2.2 §10.3.8, §10.6.5).
            auto borderBoxSize = integrationUtils().layoutOutOfFlowIndependentFormattingContext(root);
            return { outOfFlowGeometry.computeReplacedInlineAxisGeometry(borderBoxSize.width()), outOfFlowGeometry.computeReplacedBlockAxisGeometry(borderBoxSize.height()) };
        }
        auto contentInlineSizes = std::optional<IntrinsicWidthConstraints> { };
        if (outOfFlowGeometry.hasFitContentInlineSize())
            contentInlineSizes = LayoutIntegration::BlockIntegrationUtils::independentFormattingContextIntrinsicInlineSizes(root, geometry.horizontalBorderAndPadding());
        auto inlineAxis = outOfFlowGeometry.computeInlineAxisGeometry(contentInlineSizes);
        auto borderBoxBlockSize = outOfFlowGeometry.definiteContentBoxBlockSize().transform([&](auto contentBoxBlockSize) {
            return contentBoxBlockSize + geometry.verticalBorderAndPadding();
        });
        auto borderBoxSize = integrationUtils().layoutOutOfFlowIndependentFormattingContext(root, inlineAxis.borderBoxSize, borderBoxBlockSize);
        return { inlineAxis, outOfFlowGeometry.computeBlockAxisGeometry(borderBoxSize.height() - geometry.verticalBorderAndPadding()) };
    }();
    geometry.setContentBoxWidth(inlineAxis.borderBoxSize - geometry.horizontalBorderAndPadding());
    geometry.setHorizontalMargin({ inlineAxis.marginStart, inlineAxis.marginEnd });
    geometry.setLeft(containingBlock.paddingBoxInlineStart + inlineAxis.borderBoxStart);
    geometry.setContentBoxHeight(blockAxis.borderBoxSize - geometry.verticalBorderAndPadding());
    geometry.setVerticalMargin({ blockAxis.marginStart, blockAxis.marginEnd });
    geometry.setTop(containingBlock.paddingBoxBlockStart + blockAxis.borderBoxStart);

    auto margins = [](LayoutUnit margin) {
        auto adjoiningMargins = AdjoiningMargins { };
        adjoiningMargins.add(margin);
        return adjoiningMargins;
    };
    auto result = BlockLayoutResult { };
    result.boxes.append({ root, geometry, geometry.contentBoxHeight(), margins(geometry.marginBefore()), margins(geometry.marginAfter()), false, false, true });
    return result;
}

BlockLayoutResult BlockFormattingContext::layoutOutOfFlowBox(const RenderBox& outOfFlowBox, const OutOfFlowContainingBlock& containingBlock, const OutOfFlowGeometry::StaticPosition& staticPosition)
{
    if (!is<RenderBlockFlow>(outOfFlowBox))
        return layoutOutOfFlowIndependentFormattingContext(outOfFlowBox, containingBlock, staticPosition);
    auto result = BlockLayoutResult { };
    CheckedRef box = downcast<RenderBlockFlow>(outOfFlowBox);
    auto geometry = computeBorderAndPadding(box, containingBlock.inlineSize);
    auto outOfFlowGeometry = OutOfFlowGeometry { box, { containingBlock.inlineSize, containingBlock.blockSize, containingBlock.box->writingMode() }, staticPosition, geometry.horizontalBorderAndPadding(), geometry.verticalBorderAndPadding() };

    auto contentInlineSizes = outOfFlowGeometry.hasFitContentInlineSize() ? std::optional { computeIntrinsicInlineSizes(box) } : std::nullopt;
    auto inlineAxis = outOfFlowGeometry.computeInlineAxisGeometry(contentInlineSizes);
    geometry.setContentBoxWidth(inlineAxis.borderBoxSize - geometry.horizontalBorderAndPadding());
    geometry.setHorizontalMargin({ inlineAxis.marginStart, inlineAxis.marginEnd });
    geometry.setLeft(containingBlock.paddingBoxInlineStart + inlineAxis.borderBoxStart);
    auto contentBoxBlockSize = outOfFlowGeometry.definiteContentBoxBlockSize();

    // The box establishes an independent formatting context for its content.
    auto boxState = ContainingBlockState {
        .blockContainer = box,
        .geometry = geometry,
        .inlineRange = { geometry.borderAndPaddingStart(), geometry.contentBoxWidth() },
        .borderBoxLogicalTop = 0_lu,
        .borderAndPaddingBefore = geometry.borderAndPaddingBefore(),
        .contentBoxBlockSize = contentBoxBlockSize,
        // Coverage keeps out the -webkit-fill-available block sizes inside.
        .fillAvailableBlockSize = contentBoxBlockSize.value_or(0_lu),
        .margins = { },
        .isLaidOutThroughRenderTree = false,
        .relayoutChildren = LayoutIntegration::BlockIntegrationUtils::contentBoxInlineSizeChanged(box, geometry.contentBoxWidth())
    };
    auto intrinsicContentBlockSize = LayoutUnit { };
    if (box->firstChild()) {
        // Its content is in a block formatting context of its own, with a block layout state of its own.
        auto blockLayoutState = BlockLayoutState { { } };
        if (box->childrenInline()) {
            auto inlineContentLayoutResult = integrationUtils().layoutInlineContent(box.get(), constraintsForInlineContent(box, boxState), geometry, boxState.contentBoxBlockSize, boxState.relayoutChildren, blockLayoutState);
            if (inlineContentLayoutResult.hasLineBoxes)
                intrinsicContentBlockSize = inlineContentLayoutResult.contentBlockSize;
        } else {
            auto horizontalConstraints = HorizontalConstraints { boxState.inlineRange.min(), boxState.inlineRange.size() };
            auto contentResult = BlockFormattingContext { box, integrationUtils(), blockLayoutState }.layout({ { horizontalConstraints, boxState.borderAndPaddingBefore }, contentBoxBlockSize, boxState.fillAvailableBlockSize, boxState.relayoutChildren });
            intrinsicContentBlockSize = contentResult.intrinsicContentBlockSize;
            result.boxes.appendVector(WTF::move(contentResult.boxes));
            result.outOfFlowBoxes.appendVector(WTF::move(contentResult.outOfFlowBoxes));
        }
    } else if (auto contentBlockSize = BlockFormattingUtils::emptyEditableContentBlockSize(box))
        intrinsicContentBlockSize = *contentBlockSize;

    auto blockAxis = outOfFlowGeometry.computeBlockAxisGeometry(intrinsicContentBlockSize);
    geometry.setContentBoxHeight(blockAxis.borderBoxSize - geometry.verticalBorderAndPadding());
    geometry.setVerticalMargin({ blockAxis.marginStart, blockAxis.marginEnd });
    geometry.setTop(containingBlock.paddingBoxBlockStart + blockAxis.borderBoxStart);

    auto margins = [](LayoutUnit margin) {
        auto adjoiningMargins = AdjoiningMargins { };
        adjoiningMargins.add(margin);
        return adjoiningMargins;
    };
    result.boxes.append({ box, geometry, intrinsicContentBlockSize, margins(geometry.marginBefore()), margins(geometry.marginAfter()), false, false, false });
    return result;
}

ContainingBlockState BlockFormattingContext::containingBlockStateForRoot(const ConstraintsForBlockContent& constraints) const
{
    auto horizontalConstraints = constraints.horizontal();
    return {
        .blockContainer = root(),
        .geometry = { },
        .inlineRange = { horizontalConstraints.logicalLeft, horizontalConstraints.logicalWidth },
        .borderBoxLogicalTop = 0_lu,
        .borderAndPaddingBefore = constraints.logicalTop(),
        .contentBoxBlockSize = constraints.contentBoxBlockSize(),
        .fillAvailableBlockSize = constraints.fillAvailableBlockSize(),
        .margins = { },
        .isLaidOutThroughRenderTree = false,
        .relayoutChildren = constraints.relayoutChildren()
    };
}

ContainingBlockState BlockFormattingContext::containingBlockStateFor(const RenderBox& blockContainer, const BoxGeometry& geometry, const ContainingBlockState& parentState) const
{
    auto contentBoxBlockSize = BlockFormattingUtils::definiteContentBoxBlockSize(blockContainer, geometry, parentState.contentBoxBlockSize, stretchFitBorderBoxBlockSize(blockContainer, geometry, parentState), root().writingMode());
    return {
        .blockContainer = blockContainer,
        .geometry = geometry,
        .inlineRange = { geometry.borderAndPaddingStart(), geometry.contentBoxWidth() },
        .borderBoxLogicalTop = { },
        .borderAndPaddingBefore = geometry.borderAndPaddingBefore(),
        .contentBoxBlockSize = contentBoxBlockSize,
        .fillAvailableBlockSize = BlockFormattingUtils::fillAvailableBlockSize(blockContainer, geometry, contentBoxBlockSize, parentState.contentBoxBlockSize, parentState.fillAvailableBlockSize, stretchFitBorderBoxBlockSize(blockContainer, geometry, parentState), root().writingMode()),
        .margins = { },
        .isLaidOutThroughRenderTree = false,
        .relayoutChildren = LayoutIntegration::BlockIntegrationUtils::contentBoxInlineSizeChanged(blockContainer, geometry.contentBoxWidth())
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
// sizes of its content when its min inline size is auto, through the calculation of a calc-size() there, where percentages are cyclic
// (RenderBox::computeIntrinsicKeywordLogicalWidths, applyAutomaticContentBasedMinimumSize).
static IntrinsicWidthConstraints aspectRatioContentInlineSizes(const RenderBox& box, LayoutUnit aspectRatioContentBoxInlineSize, IntrinsicWidthConstraints contentSizes, LayoutUnit borderAndPaddingInlineSize, WritingMode writingMode)
{
    CheckedRef style = box.style();
    auto& minimumSize = style->logicalMinWidth(writingMode);
    if (!minimumSize.isAuto() || !box.firstChild())
        return { aspectRatioContentBoxInlineSize, aspectRatioContentBoxInlineSize };
    if (minimumSize.isCalcSize()) {
        auto calcSize = minimumSize.calcSize();
        auto resolve = [&](LayoutUnit contentBoxInlineSize) {
            return BlockFormattingUtils::calcSizeBorderBoxSize(calcSize, style.get(), contentBoxInlineSize + borderAndPaddingInlineSize, borderAndPaddingInlineSize, 0_lu) - borderAndPaddingInlineSize;
        };
        contentSizes = { resolve(contentSizes.minimum), resolve(contentSizes.maximum) };
    }
    return { std::max(aspectRatioContentBoxInlineSize, contentSizes.minimum), std::max(aspectRatioContentBoxInlineSize, contentSizes.maximum) };
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
    auto isLaidOutThroughRenderTree = !blockContainer || (blockContainer->createsNewFormattingContext() && LayoutIntegration::BlockIntegrationUtils::laysOutThroughRenderTree(*blockContainer));
    if (isLaidOutThroughRenderTree && decidesItsOwnInlineSize(box, writingMode))
        return;

    CheckedRef containingBlockStyle = containingBlockState.blockContainer->style();
    // A preferred aspect ratio gives an auto inline size from a definite block size, and the min-content and max-content sizes of the box,
    // floored by those of its content when its min inline size is auto (RenderBox::computeIntrinsicKeywordLogicalWidths).
    auto horizontalBorderAndPadding = geometry.horizontalBorderAndPadding();
    auto aspectRatioBorderBoxInlineSize = BlockFormattingUtils::aspectRatioBorderBoxInlineSize(box, horizontalBorderAndPadding, geometry.verticalBorderAndPadding(), containingBlockState.contentBoxBlockSize, stretchFitBorderBoxBlockSize(box, geometry, containingBlockState), writingMode);
    auto contentInlineSizes = [&] -> std::optional<IntrinsicWidthConstraints> {
        auto fitsContent = style->logicalWidth(writingMode).isAuto() && BlockFormattingUtils::isAlignedByJustifySelf(box, containingBlockStyle);
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
        // A preferred aspect ratio gives the min-content and max-content sizes of the box from a fixed block size, floored by those of its
        // content when its min inline size is auto (RenderBox::computeIntrinsicKeywordLogicalWidths); coverage keeps out the other definite
        // block sizes, which resolve against the containing block.
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
    if (!containingBlockState.contentBoxBlockSize)
        return { };
    auto marginBefore = geometry.marginBefore();
    auto marginAfter = geometry.marginAfter();
    CheckedPtr containingBlock = dynamicDowncast<RenderBlockFlow>(containingBlockState.blockContainer);
    if (containingBlock && !containingBlock->createsNewFormattingContext()) {
        auto& containingBlockGeometry = containingBlockState.geometry;
        if (!containingBlockGeometry.borderAndPaddingBefore())
            marginBefore = { };
        if (!containingBlockGeometry.borderAndPaddingAfter())
            marginAfter = { };
    }
    return std::max(0_lu, *containingBlockState.contentBoxBlockSize - marginBefore - marginAfter);
}

void BlockFormattingContext::computeBlockSize(const RenderBox& box, LayoutUnit intrinsicContentBlockSize, const ContainingBlockState& containingBlockState, BoxGeometry& geometry) const
{
    // A box with size containment is sized as if it had no content, by contain-intrinsic-block-size (css-contain-2 §3.2,
    // RenderBox::overrideLogicalHeightForSizeContainment).
    if (box.shouldApplySizeContainment())
        intrinsicContentBlockSize = box.explicitIntrinsicInnerLogicalHeight().value_or(0_lu);
    auto borderAndPaddingBlockSize = geometry.verticalBorderAndPadding();
    auto blockSize = BlockFormattingUtils::borderBoxBlockSize(box, borderAndPaddingBlockSize, intrinsicContentBlockSize, containingBlockState.contentBoxBlockSize, stretchFitBorderBoxBlockSize(box, geometry, containingBlockState), root().writingMode());
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

ContainingBlockState BlockFormattingContext::computeContainingBlockStateAtBlockStart(const RenderBox& box)
{
    auto& containingBlockState = m_blockLayoutState.containingBlockStates().last();
    auto geometry = computeBorderAndPadding(box, containingBlockState.inlineRange.size());
    computeInlineSizeAndMargins(box, containingBlockState, geometry);
    computeLogicalLeft(box, containingBlockState, geometry);
    // FIXME: Clearance separates the block-start margin from the margins before it (CSS 2.2 §9.5.2).
    auto blockContainerState = containingBlockStateFor(box, geometry, containingBlockState);
    m_blockLayoutState.marginCollapse().startBlockContainer(blockContainerState, containingBlockState, geometry.marginBefore());
    return blockContainerState;
}

// Quirks mode: body is at least as tall as the viewport leaves it inside html (RenderBox::computeLogicalHeight).
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
    auto marginAfterIsQuirky = m_blockLayoutState.marginCollapse().addBlockEndMargin(geometry.marginAfter(), style->marginAfter().hasQuirk(), marginAfterCollapsesWithChildren && !minimumBlockSizeSeparatesMargins, borderBoxLogicalTop + geometry.borderBoxHeight());

    geometry.setTop(borderBoxLogicalTop - *containingBlockState.borderBoxLogicalTop);
    result.boxes.append({ box, WTF::move(geometry), intrinsicContentBlockSize, collapsedMarginsBefore, m_blockLayoutState.marginCollapse().adjoiningMargins(), blockContainerState.margins.collapsedMarginsBeforeAreQuirky(), marginAfterIsQuirky, blockContainerState.isLaidOutThroughRenderTree });
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
