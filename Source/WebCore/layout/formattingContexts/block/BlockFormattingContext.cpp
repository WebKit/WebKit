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
#include "BlockItemsBuilder.h"
#include "RenderBlockFlow.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StylePrimitiveNumericTypes+EvaluationMinimum.h"
#include "UsedStyle.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_TZONE_ALLOCATED_IMPL(BlockFormattingContext);

BlockFormattingContext::BlockFormattingContext(const RenderBlockFlow& root, BlockLayoutState& blockLayoutState)
    : m_root(root)
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

    for (auto& blockItem : blockItems) {
        CheckedRef box = blockItem.renderer();
        switch (blockItem.type()) {
        case BlockItem::Type::BlockContainerStart:
            handleBlockContainerStart(box);
            break;
        case BlockItem::Type::BlockContainerEnd:
            handleBlockContainerEnd(box, result);
            break;
        }
    }

    ASSERT(m_blockLayoutState.containingBlockStates().size() == 1);
    result.intrinsicContentBlockSize = m_blockLayoutState.containingBlockStates().last().intrinsicContentBlockSize();
    m_blockLayoutState.containingBlockStates().clear();
    return result;
}

void BlockFormattingContext::handleBlockContainerStart(const RenderBox& blockContainer)
{
    auto& containingBlockState = m_blockLayoutState.containingBlockStates().last();
    auto geometry = computeBorderAndPadding(blockContainer, containingBlockState);
    computeInlineSizeAndMargins(blockContainer, containingBlockState, geometry);
    computeStaticPosition(containingBlockState, geometry);
    m_blockLayoutState.containingBlockStates().append(containingBlockStateFor(blockContainer, geometry, containingBlockState));
}

void BlockFormattingContext::handleBlockContainerEnd(const RenderBox& blockContainer, BlockLayoutResult& result)
{
    auto blockContainerState = m_blockLayoutState.containingBlockStates().takeLast();
    ASSERT(blockContainerState.blockContainer.ptr() == &blockContainer);
    auto& containingBlockState = m_blockLayoutState.containingBlockStates().last();
    auto intrinsicContentBlockSize = blockContainerState.intrinsicContentBlockSize();
    computeBlockSize(blockContainer, intrinsicContentBlockSize, containingBlockState, blockContainerState.geometry);
    placeInContainingBlock(blockContainer, WTF::move(blockContainerState.geometry), intrinsicContentBlockSize, containingBlockState, result);
}

ContainingBlockState BlockFormattingContext::containingBlockStateForRoot(const ConstraintsForBlockContent& constraints) const
{
    auto horizontalConstraints = constraints.horizontal();
    return {
        .blockContainer = root(),
        .geometry = { },
        .inlineRange = { horizontalConstraints.logicalLeft, horizontalConstraints.logicalWidth },
        .borderBoxLogicalTop = { },
        .contentBoxLogicalTop = constraints.logicalTop(),
        .contentBoxBlockSize = constraints.contentBoxBlockSize(),
        .lastInFlowChildMarginBoxLogicalBottom = { }
    };
}

ContainingBlockState BlockFormattingContext::containingBlockStateFor(const RenderBox& blockContainer, const BoxGeometry& geometry, const ContainingBlockState& parentState) const
{
    auto borderBoxLogicalTop = BoxGeometry::borderBoxTop(geometry);
    return {
        .blockContainer = blockContainer,
        .geometry = geometry,
        .inlineRange = { geometry.borderAndPaddingStart(), geometry.contentBoxWidth() },
        .borderBoxLogicalTop = borderBoxLogicalTop,
        .contentBoxLogicalTop = borderBoxLogicalTop + geometry.borderAndPaddingBefore(),
        .contentBoxBlockSize = BlockFormattingUtils::definiteContentBoxBlockSize(blockContainer, geometry, parentState.contentBoxBlockSize, root().writingMode()),
        .lastInFlowChildMarginBoxLogicalBottom = { }
    };
}

BoxGeometry BlockFormattingContext::computeBorderAndPadding(const RenderBox& box, const ContainingBlockState& containingBlockState) const
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
        return Style::evaluateMinimum<LayoutUnit>(paddingEdge, containingBlockState.inlineRange.size(), style->usedZoomForLength());
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

    CheckedRef containingBlockStyle = containingBlockState.blockContainer->style();
    auto [borderBoxInlineSize, marginStart, marginEnd] = BlockFormattingUtils::inlineSizeAndMargins(box, geometry.horizontalBorderAndPadding(), containingBlockInlineSize, containingBlockStyle);
    geometry.setContentBoxWidth(borderBoxInlineSize - geometry.horizontalBorderAndPadding());
    // Unlike border and padding, start and end margins follow the direction of the containing block.
    geometry.setHorizontalMargin({ marginStart, marginEnd });
}

void BlockFormattingContext::computeStaticPosition(const ContainingBlockState& containingBlockState, BoxGeometry& geometry) const
{
    // The box is placed from the inline start of its containing block, so an over-constrained box ignores its end margin (CSS 2.2 §10.3.3).
    // FIXME: Margins do not collapse yet (CSS 2.2 §8.3.1); the margin box starts where the previous sibling's ends.
    geometry.setTopLeft({ containingBlockState.inlineRange.min() + geometry.marginStart(), containingBlockState.logicalTopForNextChild() + geometry.marginBefore() });
}

void BlockFormattingContext::computeBlockSize(const RenderBox& box, LayoutUnit intrinsicContentBlockSize, const ContainingBlockState& containingBlockState, BoxGeometry& geometry) const
{
    auto borderAndPaddingBlockSize = geometry.verticalBorderAndPadding();
    auto blockSize = BlockFormattingUtils::borderBoxBlockSize(box, borderAndPaddingBlockSize, intrinsicContentBlockSize, containingBlockState.contentBoxBlockSize, root().writingMode());
    geometry.setContentBoxHeight(blockSize - borderAndPaddingBlockSize);
}

void BlockFormattingContext::placeInContainingBlock(const RenderBox& box, BoxGeometry geometry, LayoutUnit intrinsicContentBlockSize, ContainingBlockState& containingBlockState, BlockLayoutResult& result) const
{
    auto borderBoxLogicalTop = BoxGeometry::borderBoxTop(geometry);
    containingBlockState.lastInFlowChildMarginBoxLogicalBottom = borderBoxLogicalTop + geometry.borderBoxHeight() + geometry.marginAfter();
    // The result is relative to the border box of the containing block, not to the root.
    geometry.setTop(borderBoxLogicalTop - containingBlockState.borderBoxLogicalTop);
    result.boxes.append({ box, WTF::move(geometry), intrinsicContentBlockSize });
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
