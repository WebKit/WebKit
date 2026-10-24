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
#include "RenderBlockFlow.h"
#include "RenderBoxInlines.h"
#include "RenderView.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StylePrimitiveNumericTypes+EvaluationMinimum.h"
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
    // FIXME: Keep the block items across layouts and invalidate them on tree and formatting context participation changes.
    auto blockItems = BlockItemsBuilder { root() }.build();

    auto result = BlockLayoutResult { };
    m_blockLayoutState.containingBlockStates().append(containingBlockStateForRoot(constraints));
    m_blockLayoutState.marginCollapse().startBlockFormattingContextRoot(m_blockLayoutState.containingBlockStates().last());
    // The margins of a box that establishes a block formatting context do not collapse with its children's (CSS 2.2 §8.3.1).
    m_blockLayoutState.marginCollapse().resetAdjoiningMargins(m_blockLayoutState.containingBlockStates().last().contentBoxLogicalTop());

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
            handleInlineContent(box);
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
    auto& containingBlockState = m_blockLayoutState.containingBlockStates().last();
    auto geometry = computeBorderAndPadding(blockContainer, containingBlockState.inlineRange.size());
    computeInlineSizeAndMargins(blockContainer, containingBlockState, geometry);
    computeLogicalLeft(containingBlockState, geometry);
    // FIXME: Clearance separates the block-start margin from the margins before it (CSS 2.2 §9.5.2).
    auto blockContainerState = containingBlockStateFor(blockContainer, geometry, containingBlockState);
    m_blockLayoutState.marginCollapse().startBlockContainer(blockContainerState, containingBlockState, geometry.marginBefore());
    if (geometry.borderAndPaddingBefore())
        resolveBorderBoxLogicalTops(blockContainerState);
    m_blockLayoutState.containingBlockStates().append(WTF::move(blockContainerState));
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
        return BlockMarginCollapse::marginsCollapseThrough(blockContainer, blockContainerState.geometry, containingBlockState.contentBoxBlockSize, root().writingMode());
    };
    if (!blockContainerState.borderBoxLogicalTop && !marginsCollapseThrough())
        resolveBorderBoxLogicalTops(blockContainerState);
    placeInContainingBlock(blockContainer, WTF::move(blockContainerState), result);
}

void BlockFormattingContext::handleInlineContent(const RenderBox& blockContainer)
{
    auto& blockContainerState = m_blockLayoutState.containingBlockStates().last();
    ASSERT(blockContainerState.blockContainer.ptr() == &blockContainer);
    // FIXME: Floats need the block container's top before the inline content is laid out; decide whether it has line boxes from its
    // inline items then.
    auto inlineContentLayoutResult = integrationUtils().layoutInlineContent(downcast<RenderBlockFlow>(blockContainer), constraintsForInlineContent(blockContainer, blockContainerState), blockContainerState.geometry, m_blockLayoutState);
    if (!inlineContentLayoutResult.hasLineBoxes)
        return;
    // The first line box separates the block-start margin from what follows (CSS 2.2 §8.3.1).
    if (!blockContainerState.borderBoxLogicalTop)
        resolveBorderBoxLogicalTops(blockContainerState);
    m_blockLayoutState.marginCollapse().resetAdjoiningMargins(blockContainerState.contentBoxLogicalTop() + inlineContentLayoutResult.contentBlockSize);
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
        .margins = { }
    };
}

ContainingBlockState BlockFormattingContext::containingBlockStateFor(const RenderBox& blockContainer, const BoxGeometry& geometry, const ContainingBlockState& parentState) const
{
    return {
        .blockContainer = blockContainer,
        .geometry = geometry,
        .inlineRange = { geometry.borderAndPaddingStart(), geometry.contentBoxWidth() },
        .borderBoxLogicalTop = { },
        .borderAndPaddingBefore = geometry.borderAndPaddingBefore(),
        .contentBoxBlockSize = BlockFormattingUtils::definiteContentBoxBlockSize(blockContainer, geometry, parentState.contentBoxBlockSize, root().writingMode()),
        .margins = { }
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

    auto contentInlineSizes = [&] -> std::optional<IntrinsicWidthConstraints> {
        if (!BlockFormattingUtils::hasIntrinsicInlineSize(box, writingMode))
            return { };
        return computeIntrinsicInlineSizes(downcast<RenderBlockFlow>(box));
    }();
    CheckedRef containingBlockStyle = containingBlockState.blockContainer->style();
    auto [borderBoxInlineSize, marginStart, marginEnd] = BlockFormattingUtils::inlineSizeAndMargins(box, geometry.horizontalBorderAndPadding(), containingBlockInlineSize, containingBlockStyle, contentInlineSizes);
    geometry.setContentBoxWidth(borderBoxInlineSize - geometry.horizontalBorderAndPadding());
    // Unlike border and padding, start and end margins follow the direction of the containing block.
    geometry.setHorizontalMargin({ marginStart, marginEnd });
}

// Goes through the block items of the block container's content the way layout does, collecting the outer min-content and max-content
// contributions of the boxes in it (§5.2).
IntrinsicWidthConstraints BlockFormattingContext::computeIntrinsicInlineSizes(const RenderBlockFlow& blockContainer) const
{
    if (blockContainer.childrenInline())
        return blockContainer.firstChild() ? LayoutIntegration::BlockIntegrationUtils::inlineContentIntrinsicInlineSizes(blockContainer) : IntrinsicWidthConstraints { };

    auto writingMode = root().writingMode();
    // The outer size: the inline size, clamped by the min and max inline sizes, plus border, padding and margins, where auto margins and
    // percentages, which are cyclic here, count as zero and percentage inline sizes as auto (§5.2.1). Coverage keeps out percentage padding,
    // which the render tree path resolves against the containing block's current inline size.
    auto outerInlineSizes = [&](const RenderBox& box, IntrinsicWidthConstraints contentSizes) -> IntrinsicWidthConstraints {
        CheckedRef style = box.style();
        auto zoom = style->usedZoomForLength();
        auto borderAndPadding = computeBorderAndPadding(box, 0_lu).horizontalBorderAndPadding();
        // A size under a min-content and under a max-content constraint: a fixed size, or the content's for the sizing keywords.
        auto contentBoxSizes = [&](auto& size) -> std::optional<IntrinsicWidthConstraints> {
            if (size.isFixed()) {
                auto value = Style::evaluate<LayoutUnit>(size, 0_lu, zoom);
                value = std::max(0_lu, style->boxSizing() == BoxSizing::BorderBox ? value - borderAndPadding : value);
                return IntrinsicWidthConstraints { value, value };
            }
            if (size.isMinContent())
                return IntrinsicWidthConstraints { contentSizes.minimum, contentSizes.minimum };
            if (size.isMaxContent())
                return IntrinsicWidthConstraints { contentSizes.maximum, contentSizes.maximum };
            if (size.isFitContent())
                return contentSizes;
            return { };
        };
        auto [minimum, maximum] = contentSizes;
        if (auto sizes = contentBoxSizes(style->logicalWidth(writingMode)))
            std::tie(minimum, maximum) = std::pair { sizes->minimum, sizes->maximum };
        if (auto sizes = contentBoxSizes(style->logicalMaxWidth(writingMode))) {
            minimum = std::min(minimum, sizes->minimum);
            maximum = std::min(maximum, sizes->maximum);
        }
        if (auto sizes = contentBoxSizes(style->logicalMinWidth(writingMode))) {
            minimum = std::max(minimum, sizes->minimum);
            maximum = std::max(maximum, sizes->maximum);
        }
        auto margin = [&](auto& margin) {
            return margin.isAuto() ? 0_lu : Style::evaluateMinimum<LayoutUnit>(margin, 0_lu, zoom);
        };
        auto outerSpace = borderAndPadding + margin(style->marginStart(writingMode)) + margin(style->marginEnd(writingMode));
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
            addContribution(blockContainerSizes.last(), outerInlineSizes(box, contentSizes));
            break;
        }
        }
    }
    ASSERT(blockContainerSizes.size() == 1);
    auto sizes = blockContainerSizes.first();
    sizes.minimum = std::max(0_lu, sizes.minimum);
    sizes.maximum = std::max(sizes.minimum, sizes.maximum);
    return sizes;
}

void BlockFormattingContext::computeLogicalLeft(const ContainingBlockState& containingBlockState, BoxGeometry& geometry) const
{
    // The box is placed from the inline start of its containing block, so an over-constrained box ignores its end margin (CSS 2.2 §10.3.3).
    geometry.setLeft(containingBlockState.inlineRange.min() + geometry.marginStart());
}

void BlockFormattingContext::computeBlockSize(const RenderBox& box, LayoutUnit intrinsicContentBlockSize, const ContainingBlockState& containingBlockState, BoxGeometry& geometry) const
{
    auto borderAndPaddingBlockSize = geometry.verticalBorderAndPadding();
    auto blockSize = BlockFormattingUtils::borderBoxBlockSize(box, borderAndPaddingBlockSize, intrinsicContentBlockSize, containingBlockState.contentBoxBlockSize, root().writingMode());
    if (auto aspectRatioBlockSize = BlockFormattingUtils::aspectRatioBorderBoxBlockSize(box, geometry.borderBoxWidth(), geometry.horizontalBorderAndPadding(), borderAndPaddingBlockSize, root().writingMode())) {
        blockSize = *aspectRatioBlockSize;
        // The automatic minimum block size of a box that is not a scroll container is its content's (css-sizing-4 §5.1).
        if (box.effectiveOverflowBlockDirection() == Overflow::Visible)
            blockSize = std::max(blockSize, borderAndPaddingBlockSize + intrinsicContentBlockSize);
    }
    geometry.setContentBoxHeight(blockSize - borderAndPaddingBlockSize);
}

ConstraintsForInlineContent BlockFormattingContext::constraintsForInlineContent(const RenderBox& blockContainer, const ContainingBlockState& blockContainerState) const
{
    // Like the line boxes the inline formatting context produces, these are relative to the border box of the block container, in its own direction.
    auto& geometry = blockContainerState.geometry;
    auto horizontalConstraints = HorizontalConstraints { blockContainerState.inlineRange.min(), blockContainerState.inlineRange.size() };
    auto visualLeft = blockContainer.writingMode().isLogicalLeftInlineStart() ? geometry.borderAndPaddingStart() : geometry.borderAndPaddingEnd();
    // The border box height is only needed in vertical writing modes.
    return { { horizontalConstraints, blockContainerState.borderAndPaddingBefore }, visualLeft, LayoutSize { geometry.borderBoxWidth(), 0_lu } };
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
        result.boxes.append({ box, WTF::move(geometry), { }, collapsedMarginsBefore, collapsedMarginsAfter, blockContainerState.margins.collapsedMarginsBeforeAreQuirky(), box.style().marginAfter().hasQuirk() });
        return;
    }

    // The content ends at the bottom border edge of the last in-flow child whose margins do not collapse with the box's block-end margin,
    // or below the collapsed margin after it when that margin does not collapse with the box's (CSS 2.2 §10.6.3).
    CheckedRef style = box.style();
    auto writingMode = root().writingMode();
    auto marginAfterCollapsesWithChildren = !geometry.borderAndPaddingAfter() && BlockFormattingUtils::blockSizeBehavesAsAuto(style->logicalHeight(writingMode), containingBlockState.contentBoxBlockSize);
    // A quirk container leaves out a quirky margin after its last child (RenderBlockFlow::handleAfterSideOfBlock).
    auto contentBoxLogicalBottom = marginAfterCollapsesWithChildren || (BlockMarginCollapse::isQuirkContainer(box) && m_blockLayoutState.marginCollapse().endsWithQuirk()) ? m_blockLayoutState.marginCollapse().logicalTop() : m_blockLayoutState.marginCollapse().collapsedLogicalBottom();
    // Negative margins may pull that above the content box top.
    auto intrinsicContentBlockSize = std::max(0_lu, contentBoxLogicalBottom - blockContainerState.contentBoxLogicalTop());
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
    result.boxes.append({ box, WTF::move(geometry), intrinsicContentBlockSize, collapsedMarginsBefore, m_blockLayoutState.marginCollapse().adjoiningMargins(), blockContainerState.margins.collapsedMarginsBeforeAreQuirky(), marginAfterIsQuirky });
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
