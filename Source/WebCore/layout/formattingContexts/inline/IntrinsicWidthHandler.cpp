/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "IntrinsicWidthHandler.h"

#include "FloatingContext.h"
#include "FloatingState.h"
#include "InlineFormattingContext.h"
#include "InlineLineBuilder.h"
#include "LayoutElementBox.h"
#include "TextOnlySimpleLineBuilder.h"

namespace WebCore {
namespace Layout {

IntrinsicWidthHandler::IntrinsicWidthHandler(const InlineFormattingContext& inlineFormattingContext)
    : m_inlineFormattingContext(inlineFormattingContext)
{
}

IntrinsicWidthConstraints IntrinsicWidthHandler::computedIntrinsicSizes()
{
    auto& inlineFormattingState = formattingState();

    auto computedIntrinsicValue = [&](auto intrinsicWidthMode, auto& inlineBuilder, MayCacheLayoutResult mayCacheLayoutResult = MayCacheLayoutResult::No) {
        inlineBuilder.setIntrinsicWidthMode(intrinsicWidthMode);
        return ceiledLayoutUnit(computedIntrinsicWidthForConstraint(intrinsicWidthMode, inlineBuilder, mayCacheLayoutResult));
    };

    auto intrinsicSizes = IntrinsicWidthConstraints { };
    if (TextOnlySimpleLineBuilder::isEligibleForSimplifiedTextOnlyInlineLayout(root(), inlineFormattingState)) {
        auto simplifiedLineBuilder = TextOnlySimpleLineBuilder { formattingContext(), { }, inlineFormattingState.inlineItems() };
        intrinsicSizes = { computedIntrinsicValue(IntrinsicWidthMode::Minimum, simplifiedLineBuilder), computedIntrinsicValue(IntrinsicWidthMode::Maximum, simplifiedLineBuilder, TextOnlySimpleLineBuilder::hasIntrinsicWidthSpecificStyle(root().style()) ? MayCacheLayoutResult::No : MayCacheLayoutResult::Yes) };
    } else {
        auto floatingState = FloatingState { root() };
        auto parentBlockLayoutState = BlockLayoutState { floatingState, { } };
        auto inlineLayoutState = InlineLayoutState { parentBlockLayoutState, { } };
        auto lineBuilder = LineBuilder { formattingContext(), inlineLayoutState, floatingState, { }, inlineFormattingState.inlineItems() };
        intrinsicSizes = { computedIntrinsicValue(IntrinsicWidthMode::Minimum, lineBuilder), computedIntrinsicValue(IntrinsicWidthMode::Maximum, lineBuilder) };
    }
    return intrinsicSizes;
}

LayoutUnit IntrinsicWidthHandler::maximumContentSize()
{
    auto& inlineFormattingState = formattingState();
    auto floatingState = FloatingState { root() };
    auto parentBlockLayoutState = BlockLayoutState { floatingState, { } };
    auto inlineLayoutState = InlineLayoutState { parentBlockLayoutState, { } };
    auto lineBuilder = LineBuilder { formattingContext(), inlineLayoutState, floatingState, { }, inlineFormattingState.inlineItems() };
    lineBuilder.setIntrinsicWidthMode(IntrinsicWidthMode::Maximum);

    return ceiledLayoutUnit(computedIntrinsicWidthForConstraint(IntrinsicWidthMode::Maximum, lineBuilder));
}

InlineLayoutUnit IntrinsicWidthHandler::computedIntrinsicWidthForConstraint(IntrinsicWidthMode intrinsicWidthMode, AbstractLineBuilder& lineBuilder, MayCacheLayoutResult mayCacheLayoutResult)
{
    auto horizontalConstraints = HorizontalConstraints { };
    if (intrinsicWidthMode == IntrinsicWidthMode::Maximum)
        horizontalConstraints.logicalWidth = maxInlineLayoutUnit();
    auto& inlineItems = formattingState().inlineItems();
    auto layoutRange = InlineItemRange { 0 , inlineItems.size() };
    auto maximumContentWidth = InlineLayoutUnit { };
    auto previousLineEnd = std::optional<InlineItemPosition> { };
    auto previousLine = std::optional<PreviousLine> { };
    auto lineIndex = 0lu;

    while (true) {
        auto lineLayoutResult = lineBuilder.layoutInlineContent({ layoutRange, { 0.f, 0.f, horizontalConstraints.logicalWidth, 0.f } }, previousLine);
        auto floatContentWidth = [&] {
            auto leftWidth = LayoutUnit { };
            auto rightWidth = LayoutUnit { };
            for (auto& floatItem : lineLayoutResult.floatContent.placedFloats) {
                mayCacheLayoutResult = MayCacheLayoutResult::No;
                auto marginBoxRect = BoxGeometry::marginBoxRect(floatItem.boxGeometry());
                if (floatItem.isLeftPositioned())
                    leftWidth = std::max(leftWidth, marginBoxRect.right());
                else
                    rightWidth = std::max(rightWidth, horizontalConstraints.logicalWidth - marginBoxRect.left());
            }
            return InlineLayoutUnit { leftWidth + rightWidth };
        };
        maximumContentWidth = std::max(maximumContentWidth, lineLayoutResult.lineGeometry.logicalTopLeft.x() + lineLayoutResult.contentGeometry.logicalWidth + floatContentWidth());

        layoutRange.start = InlineFormattingGeometry::leadingInlineItemPositionForNextLine(lineLayoutResult.inlineItemRange.end, previousLineEnd, layoutRange.end);
        if (layoutRange.isEmpty()) {
            auto shouldCacheLineBreakingResultForSubsequentLayout = !lineIndex && mayCacheLayoutResult == MayCacheLayoutResult::Yes;
            if (shouldCacheLineBreakingResultForSubsequentLayout)
                m_maximumIntrinsicWidthResultForSingleLine = LineBreakingResult { ceiledLayoutUnit(maximumContentWidth), WTFMove(lineLayoutResult) };
            break;
        }

        previousLineEnd = layoutRange.start;
        previousLine = PreviousLine { lineIndex++, lineLayoutResult.contentGeometry.trailingOverflowingContentWidth, { }, { }, WTFMove(lineLayoutResult.floatContent.suspendedFloats) };
    }
    return maximumContentWidth;
}

const InlineFormattingContext& IntrinsicWidthHandler::formattingContext() const
{
    return m_inlineFormattingContext;
}

const InlineFormattingState& IntrinsicWidthHandler::formattingState() const
{
    return m_inlineFormattingContext.formattingState();
}

const ElementBox& IntrinsicWidthHandler::root() const
{
    return m_inlineFormattingContext.root();
}

}
}

