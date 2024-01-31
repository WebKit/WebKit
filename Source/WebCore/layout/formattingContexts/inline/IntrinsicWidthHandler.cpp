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

static bool isBoxEligibleForNonLineBuilderMinimumWidth(const ElementBox& box)
{
    // Note that hanging trailing content needs line builder (combination of wrapping is allowed but whitespace is preserved).
    auto& style = box.style();
    return TextUtil::isWrappingAllowed(style) && (style.lineBreak() == LineBreak::Anywhere || style.wordBreak() == WordBreak::BreakAll || style.wordBreak() == WordBreak::BreakWord) && style.whiteSpaceCollapse() != WhiteSpaceCollapse::Preserve;
}

static bool isContentEligibleForNonLineBuilderMaximumWidth(const ElementBox& rootBox, const InlineItemList& inlineItemList)
{
    return inlineItemList.size() == 1 && inlineItemList[0].isText() && !downcast<InlineTextItem>(inlineItemList[0]).isWhitespace() && rootBox.style().textIndent() == RenderStyle::initialTextIndent();
}

static bool isSubtreeEligibleForNonLineBuilderMinimumWidth(const ElementBox& root)
{
    auto isSimpleBreakableContent = isBoxEligibleForNonLineBuilderMinimumWidth(root);
    for (auto* child = root.firstChild(); child && isSimpleBreakableContent; child = child->nextSibling()) {
        if (child->isFloatingPositioned()) {
            isSimpleBreakableContent = false;
            break;
        }
        auto isInlineBoxWithInlineContent = child->isInlineBox() && !child->isInlineTextBox() && !child->isLineBreakBox();
        if (isInlineBoxWithInlineContent)
            isSimpleBreakableContent = isSubtreeEligibleForNonLineBuilderMinimumWidth(downcast<ElementBox>(*child));
    }
    return isSimpleBreakableContent;
}

static bool isContentEligibleForNonLineBuilderMinimumWidth(const ElementBox& rootBox, bool mayUseSimplifiedTextOnlyInlineLayout)
{
    return (mayUseSimplifiedTextOnlyInlineLayout && isBoxEligibleForNonLineBuilderMinimumWidth(rootBox)) || (!mayUseSimplifiedTextOnlyInlineLayout && isSubtreeEligibleForNonLineBuilderMinimumWidth(rootBox));
}

static bool mayUseContentWidthBetweenLineBreaksAsMaximumSize(const ElementBox& rootBox, const InlineItemList& inlineItemList)
{
    if (!TextUtil::shouldPreserveSpacesAndTabs(rootBox))
        return false;
    for (auto& inlineItem : inlineItemList) {
        if (inlineItem.isText() && !downcast<InlineTextItem>(inlineItem).width()) {
            // We can't accumulate individual inline items when width depends on position (e.g. tab).
            return false;
        }
    }
    return true;
}

IntrinsicWidthHandler::IntrinsicWidthHandler(InlineFormattingContext& inlineFormattingContext, const InlineContentCache::InlineItems& inlineItems)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_inlineItems(inlineItems)
{
    auto initializeRangeAndTextOnlyBuilderEligibility = [&] {
        m_inlineItemRange = { 0, inlineItems.content().size() };
        m_mayUseSimplifiedTextOnlyInlineLayoutInRange = TextOnlySimpleLineBuilder::isEligibleForSimplifiedInlineLayoutByStyle(root());
        if (!m_mayUseSimplifiedTextOnlyInlineLayoutInRange)
            return;

        m_mayUseSimplifiedTextOnlyInlineLayoutInRange = inlineItems.hasTextAndLineBreakOnlyContent() && !inlineItems.requiresVisualReordering();
        if (!m_mayUseSimplifiedTextOnlyInlineLayoutInRange)
            return;
        // Non-bidi text only content maybe nested inside inline boxes e.g. <div>simple text</div>, <div><span>simple text inside inline box</span></div> or
        // <div>some text<span>and some more inside inline box</span></div>
        auto inlineBoxCount = inlineItems.inlineBoxCount();
        if (!inlineBoxCount)
            return;

        auto& inlineItemList = inlineItems.content();
        auto inlineBoxStartAndEndInlineItemsCount = 2 * inlineBoxCount;
        ASSERT(inlineBoxStartAndEndInlineItemsCount <= inlineItemList.size());

        m_mayUseSimplifiedTextOnlyInlineLayoutInRange = inlineBoxStartAndEndInlineItemsCount < inlineItemList.size();
        if (!m_mayUseSimplifiedTextOnlyInlineLayoutInRange)
            return;

        for (size_t index = 0; index < inlineBoxCount; ++index) {
            auto& inlineItem = inlineItemList[index];
            auto isNestingInlineBox = inlineItem.isInlineBoxStart() && inlineItemList[inlineItems.size() - 1 - index].isInlineBoxEnd();
            m_mayUseSimplifiedTextOnlyInlineLayoutInRange = isNestingInlineBox && !formattingContext().geometryForBox(inlineItem.layoutBox()).horizontalMarginBorderAndPadding();
            if (!m_mayUseSimplifiedTextOnlyInlineLayoutInRange)
                return;
        }
        m_inlineItemRange = { inlineBoxCount, inlineItemList.size() - inlineBoxCount };
    };
    initializeRangeAndTextOnlyBuilderEligibility();
}

InlineLayoutUnit IntrinsicWidthHandler::minimumContentSize()
{
    auto minimumContentSize = InlineLayoutUnit { };

    if (isContentEligibleForNonLineBuilderMinimumWidth(root(), m_mayUseSimplifiedTextOnlyInlineLayoutInRange))
        minimumContentSize = simplifiedMinimumWidth(root());
    else if (m_mayUseSimplifiedTextOnlyInlineLayoutInRange) {
        auto simplifiedLineBuilder = TextOnlySimpleLineBuilder { formattingContext(), { }, inlineItemList() };
        minimumContentSize = computedIntrinsicWidthForConstraint(IntrinsicWidthMode::Minimum, simplifiedLineBuilder, MayCacheLayoutResult::No);
    } else {
        auto lineBuilder = LineBuilder { formattingContext(), { }, inlineItemList() };
        minimumContentSize = computedIntrinsicWidthForConstraint(IntrinsicWidthMode::Minimum, lineBuilder, MayCacheLayoutResult::No);
    }

    return minimumContentSize;
}

InlineLayoutUnit IntrinsicWidthHandler::maximumContentSize()
{
    auto mayCacheLayoutResult = m_mayUseSimplifiedTextOnlyInlineLayoutInRange && !m_inlineItemRange.startIndex() ? MayCacheLayoutResult::Yes : MayCacheLayoutResult::No;
    auto maximumContentSize = InlineLayoutUnit { };

    if (isContentEligibleForNonLineBuilderMaximumWidth(root(), inlineItemList()))
        maximumContentSize = simplifiedMaximumWidth(mayCacheLayoutResult);
    else if (m_mayUseSimplifiedTextOnlyInlineLayoutInRange) {
        if (m_maximumContentWidthBetweenLineBreaks && mayUseContentWidthBetweenLineBreaksAsMaximumSize(root(), inlineItemList())) {
            maximumContentSize = *m_maximumContentWidthBetweenLineBreaks;
#ifndef NDEBUG
            auto simplifiedLineBuilder = TextOnlySimpleLineBuilder { formattingContext(), { }, inlineItemList() };
            ASSERT(std::abs(maximumContentSize - computedIntrinsicWidthForConstraint(IntrinsicWidthMode::Maximum, simplifiedLineBuilder, MayCacheLayoutResult::No)) < 1);
#endif
        } else {
            auto simplifiedLineBuilder = TextOnlySimpleLineBuilder { formattingContext(), { }, inlineItemList() };
            maximumContentSize = computedIntrinsicWidthForConstraint(IntrinsicWidthMode::Maximum, simplifiedLineBuilder, mayCacheLayoutResult);
        }
    } else {
        auto lineBuilder = LineBuilder { formattingContext(), { }, inlineItemList() };
        maximumContentSize = computedIntrinsicWidthForConstraint(IntrinsicWidthMode::Maximum, lineBuilder, mayCacheLayoutResult);
    }

    return maximumContentSize;
}

InlineLayoutUnit IntrinsicWidthHandler::computedIntrinsicWidthForConstraint(IntrinsicWidthMode intrinsicWidthMode, AbstractLineBuilder& lineBuilder, MayCacheLayoutResult mayCacheLayoutResult)
{
    auto horizontalConstraints = HorizontalConstraints { };
    if (intrinsicWidthMode == IntrinsicWidthMode::Maximum)
        horizontalConstraints.logicalWidth = maxInlineLayoutUnit();
    auto layoutRange = m_inlineItemRange;
    if (layoutRange.isEmpty())
        return { };

    auto maximumContentWidth = InlineLayoutUnit { };
    struct ContentWidthBetweenLineBreaks {
        InlineLayoutUnit maximum { };
        InlineLayoutUnit current { };
    };
    auto contentWidthBetweenLineBreaks = ContentWidthBetweenLineBreaks { };
    auto previousLineEnd = std::optional<InlineItemPosition> { };
    auto previousLine = std::optional<PreviousLine> { };
    auto lineIndex = 0lu;
    lineBuilder.setIntrinsicWidthMode(intrinsicWidthMode);

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

        auto lineEndsWithLineBreak = !lineLayoutResult.inlineContent.isEmpty() && lineLayoutResult.inlineContent.last().isLineBreak();
        auto lineContentLogicalWidth = lineLayoutResult.lineGeometry.logicalTopLeft.x() + lineLayoutResult.contentGeometry.logicalWidth + floatContentWidth();
        maximumContentWidth = std::max(maximumContentWidth, lineContentLogicalWidth);
        contentWidthBetweenLineBreaks.current += (lineContentLogicalWidth + lineLayoutResult.hangingContent.logicalWidth);
        if (lineEndsWithLineBreak)
            contentWidthBetweenLineBreaks = { std::max(contentWidthBetweenLineBreaks.maximum, contentWidthBetweenLineBreaks.current), { } };

        layoutRange.start = InlineFormattingUtils::leadingInlineItemPositionForNextLine(lineLayoutResult.inlineItemRange.end, previousLineEnd, !lineLayoutResult.floatContent.hasIntrusiveFloat.isEmpty(), layoutRange.end);
        if (layoutRange.isEmpty()) {
            auto cacheLineBreakingResultForSubsequentLayoutIfApplicable = [&] {
                m_maximumIntrinsicWidthResultForSingleLine = { };
                if (mayCacheLayoutResult == MayCacheLayoutResult::No)
                    return;
                m_maximumIntrinsicWidthResultForSingleLine = WTFMove(lineLayoutResult);
            };
            cacheLineBreakingResultForSubsequentLayoutIfApplicable();
            break;
        }

        // Support single line only.
        mayCacheLayoutResult = MayCacheLayoutResult::No;
        previousLineEnd = layoutRange.start;
        auto hasSeenInlineContent = previousLine ? previousLine->hasInlineContent || !lineLayoutResult.inlineContent.isEmpty() : !lineLayoutResult.inlineContent.isEmpty();
        previousLine = PreviousLine { lineIndex++, lineLayoutResult.contentGeometry.trailingOverflowingContentWidth, lineEndsWithLineBreak, hasSeenInlineContent, { }, WTFMove(lineLayoutResult.floatContent.suspendedFloats) };
    }
    m_maximumContentWidthBetweenLineBreaks = std::max(contentWidthBetweenLineBreaks.current, contentWidthBetweenLineBreaks.maximum);
    return maximumContentWidth;
}

InlineLayoutUnit IntrinsicWidthHandler::simplifiedMinimumWidth(const ElementBox& root) const
{
    auto maximumWidth = InlineLayoutUnit { };

    for (auto* child = root.firstChild(); child; child = child->nextInFlowSibling()) {
        if (child->isInlineTextBox()) {
            auto& inlineTextBox = downcast<InlineTextBox>(*child);
            auto& fontCascade = inlineTextBox.style().fontCascade();
            auto contentLength = inlineTextBox.content().length();
            size_t index = 0;
            while (index < contentLength) {
                auto characterLength = TextUtil::firstUserPerceivedCharacterLength(inlineTextBox, index, contentLength - index);
                ASSERT(characterLength);
                maximumWidth = std::max(maximumWidth, TextUtil::width(inlineTextBox, fontCascade, index, index + characterLength, { }, TextUtil::UseTrailingWhitespaceMeasuringOptimization::No));
                index += characterLength;
            }
            continue;
        }
        if (child->isAtomicInlineLevelBox() || child->isReplacedBox()) {
            maximumWidth = std::max<InlineLayoutUnit>(maximumWidth, formattingContext().geometryForBox(*child).marginBoxWidth());
            continue;
        }
        auto isInlineBoxWithInlineContent = child->isInlineBox() && !child->isLineBreakBox();
        if (isInlineBoxWithInlineContent) {
            auto& boxGeometry = formattingContext().geometryForBox(*child);
            maximumWidth = std::max(maximumWidth, std::max<InlineLayoutUnit>(boxGeometry.marginBorderAndPaddingStart(), boxGeometry.marginBorderAndPaddingEnd()));
            maximumWidth = std::max(maximumWidth, simplifiedMinimumWidth(downcast<ElementBox>(*child)));
            continue;
        }
    }
    return maximumWidth;
}

InlineLayoutUnit IntrinsicWidthHandler::simplifiedMaximumWidth(MayCacheLayoutResult mayCacheLayoutResult)
{
    ASSERT(root().firstChild() && root().firstChild() == root().lastChild());
    auto& inlineTextItem = downcast<InlineTextItem>(inlineItemList()[0]);
    auto& style = inlineTextItem.firstLineStyle();

    auto contentLogicalWidth = TextUtil::width(inlineTextItem, style.fontCascade(), { });
    if (mayCacheLayoutResult == MayCacheLayoutResult::No)
        return contentLogicalWidth;

    auto line = Line { formattingContext() };
    line.initialize({ }, true);
    line.appendTextFast(inlineTextItem, style, contentLogicalWidth);
    auto lineContent = line.close();
    ASSERT(contentLogicalWidth == lineContent.contentLogicalWidth);
    m_maximumIntrinsicWidthResultForSingleLine = LineLayoutResult { { 0, 1 }, WTFMove(lineContent.runs), { }, { { }, lineContent.contentLogicalWidth, lineContent.contentLogicalRight, { } } };
    return contentLogicalWidth;
}

InlineFormattingContext& IntrinsicWidthHandler::formattingContext()
{
    return m_inlineFormattingContext;
}

const InlineFormattingContext& IntrinsicWidthHandler::formattingContext() const
{
    return m_inlineFormattingContext;
}

const ElementBox& IntrinsicWidthHandler::root() const
{
    return m_inlineFormattingContext.root();
}

}
}

