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

#include "InlineFormattingContext.h"
#include "InlineLineBuilder.h"
#include "LayoutElementBox.h"
#include "RenderStyleInlines.h"
#include "TextOnlySimpleLineBuilder.h"

namespace WebCore {
namespace Layout {

static bool isEligibleForNonLineBuilderProcess(const RenderStyle& style)
{
    // Note that hanging trailing content needs line builder (combination of wrapping is allowed but whitespace is preserved).
    return TextUtil::isWrappingAllowed(style) && (style.lineBreak() == LineBreak::Anywhere || style.wordBreak() == WordBreak::BreakAll || style.wordBreak() == WordBreak::BreakWord) && style.whiteSpaceCollapse() != WhiteSpaceCollapse::Preserve;
}

IntrinsicWidthHandler::IntrinsicWidthHandler(InlineFormattingContext& inlineFormattingContext, const InlineItemList& inlineItemList, bool mayUseSimplifiedTextOnlyInlineLayout)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_inlineItemList(inlineItemList)
    , m_mayUseSimplifiedTextOnlyInlineLayout(mayUseSimplifiedTextOnlyInlineLayout)
{
}

IntrinsicWidthConstraints IntrinsicWidthHandler::computedIntrinsicSizes()
{
    auto computedIntrinsicValue = [&](auto intrinsicWidthMode, auto& inlineBuilder, MayCacheLayoutResult mayCacheLayoutResult = MayCacheLayoutResult::No) {
        inlineBuilder.setIntrinsicWidthMode(intrinsicWidthMode);
        return ceiledLayoutUnit(computedIntrinsicWidthForConstraint(intrinsicWidthMode, inlineBuilder, mayCacheLayoutResult));
    };

    if (m_mayUseSimplifiedTextOnlyInlineLayout) {
        auto simplifiedLineBuilder = TextOnlySimpleLineBuilder { formattingContext(), { }, m_inlineItemList };
        auto minimumWidth = isEligibleForNonLineBuilderProcess(rootStyle()) ? ceiledLayoutUnit(simplifiedMinimumWidth()) : computedIntrinsicValue(IntrinsicWidthMode::Minimum, simplifiedLineBuilder);
        return { minimumWidth, computedIntrinsicValue(IntrinsicWidthMode::Maximum, simplifiedLineBuilder, MayCacheLayoutResult::Yes) };
    }

    auto lineBuilder = LineBuilder { formattingContext(), { }, m_inlineItemList };
    return { computedIntrinsicValue(IntrinsicWidthMode::Minimum, lineBuilder), computedIntrinsicValue(IntrinsicWidthMode::Maximum, lineBuilder) };
}

LayoutUnit IntrinsicWidthHandler::maximumContentSize()
{
    auto lineBuilder = LineBuilder { formattingContext(), { }, m_inlineItemList };
    lineBuilder.setIntrinsicWidthMode(IntrinsicWidthMode::Maximum);

    return ceiledLayoutUnit(computedIntrinsicWidthForConstraint(IntrinsicWidthMode::Maximum, lineBuilder));
}

InlineLayoutUnit IntrinsicWidthHandler::computedIntrinsicWidthForConstraint(IntrinsicWidthMode intrinsicWidthMode, AbstractLineBuilder& lineBuilder, MayCacheLayoutResult mayCacheLayoutResult)
{
    auto horizontalConstraints = HorizontalConstraints { };
    if (intrinsicWidthMode == IntrinsicWidthMode::Maximum)
        horizontalConstraints.logicalWidth = maxInlineLayoutUnit();
    auto layoutRange = InlineItemRange { 0 , m_inlineItemList.size() };
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

        layoutRange.start = InlineFormattingUtils::leadingInlineItemPositionForNextLine(lineLayoutResult.inlineItemRange.end, previousLineEnd, layoutRange.end);
        if (layoutRange.isEmpty()) {
            auto cacheLineBreakingResultForSubsequentLayoutIfApplicable = [&] {
                m_maximumIntrinsicWidthResultForSingleLine = { };
                if (mayCacheLayoutResult == MayCacheLayoutResult::No)
                    return;
                m_maximumIntrinsicWidthResultForSingleLine = LineBreakingResult { ceiledLayoutUnit(maximumContentWidth), WTFMove(lineLayoutResult) };
            };
            cacheLineBreakingResultForSubsequentLayoutIfApplicable();
            break;
        }

        // Support single line only.
        mayCacheLayoutResult = MayCacheLayoutResult::No;
        previousLineEnd = layoutRange.start;
        auto hasSeenInlineContent = previousLine ? previousLine->hasInlineContent || !lineLayoutResult.inlineContent.isEmpty() : !lineLayoutResult.inlineContent.isEmpty();
        previousLine = PreviousLine { lineIndex++, lineLayoutResult.contentGeometry.trailingOverflowingContentWidth, !lineLayoutResult.inlineContent.isEmpty() && lineLayoutResult.inlineContent.last().isLineBreak(), hasSeenInlineContent, { }, WTFMove(lineLayoutResult.floatContent.suspendedFloats) };
    }
    return maximumContentWidth;
}

InlineLayoutUnit IntrinsicWidthHandler::simplifiedMinimumWidth() const
{
    auto& fontCascade = rootStyle().fontCascade();

    auto maximumWidth = InlineLayoutUnit { };
    for (auto& inlineItem : m_inlineItemList) {
        if (inlineItem.isText()) {
            auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
            auto contentLength = inlineTextItem.length();
            size_t index = 0;
            while (index < contentLength) {
                auto characterIndex = inlineTextItem.start() + index;
                auto characterLength = TextUtil::firstUserPerceivedCharacterLength(inlineTextItem.inlineTextBox(), characterIndex, contentLength - index);
                ASSERT(characterLength);
                if (characterIndex + characterLength > inlineTextItem.end()) {
                    // grapheme clusters could span across multiple adjacent inline text items.
                    maximumWidth = std::max(maximumWidth, TextUtil::width(inlineTextItem.inlineTextBox(), fontCascade, characterIndex, characterIndex + characterLength, { }, TextUtil::UseTrailingWhitespaceMeasuringOptimization::No));
                } else
                    maximumWidth = std::max(maximumWidth, TextUtil::width(inlineTextItem, fontCascade, characterIndex, characterIndex + characterLength, { }, TextUtil::UseTrailingWhitespaceMeasuringOptimization::No));
                index += characterLength;
            }
            continue;
        }
        ASSERT(inlineItem.isLineBreak());
    }
    return maximumWidth;
}

InlineFormattingContext& IntrinsicWidthHandler::formattingContext()
{
    return m_inlineFormattingContext;
}

const InlineFormattingContext& IntrinsicWidthHandler::formattingContext() const
{
    return m_inlineFormattingContext;
}

const RenderStyle& IntrinsicWidthHandler::rootStyle() const
{
    return m_inlineFormattingContext.root().style();
}

}
}

