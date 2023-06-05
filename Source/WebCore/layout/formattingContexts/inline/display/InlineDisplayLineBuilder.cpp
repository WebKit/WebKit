/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "InlineDisplayLineBuilder.h"

#include "InlineDisplayContentBuilder.h"
#include "LayoutBoxGeometry.h"
#include "RenderStyleInlines.h"
#include "TextUtil.h"

namespace WebCore {
namespace Layout {

static InlineRect flipLogicalLineRectToVisualForWritingMode(const InlineRect& lineLogicalRect, WritingMode writingMode)
{
    switch (writingMode) {
    case WritingMode::TopToBottom:
        return lineLogicalRect;
    case WritingMode::LeftToRight:
    case WritingMode::RightToLeft:
        // See InlineFormattingGeometry for more info.
        return { lineLogicalRect.left(), lineLogicalRect.top(), lineLogicalRect.height(), lineLogicalRect.width() };
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return lineLogicalRect;
}

InlineDisplayLineBuilder::InlineDisplayLineBuilder(const InlineFormattingContext& inlineFormattingContext)
    : m_inlineFormattingContext(inlineFormattingContext)
{
}

InlineDisplayLineBuilder::EnclosingLineGeometry InlineDisplayLineBuilder::collectEnclosingLineGeometry(const LineBuilder::LayoutResult& lineLayoutResult, const LineBox& lineBox, const InlineRect& lineBoxRect) const
{
    auto& rootInlineBox = lineBox.rootInlineBox();
    auto initialEnclosingTopAndBottom = [&]() -> std::tuple<std::optional<InlineLayoutUnit>, std::optional<InlineLayoutUnit>>  {
        if (!lineBox.hasContent() || !rootInlineBox.hasContent())
            return { };
        return {
            lineBoxRect.top() + rootInlineBox.logicalTop() - rootInlineBox.annotationAbove().value_or(0.f),
            lineBoxRect.top() + rootInlineBox.logicalBottom() + rootInlineBox.annotationBelow().value_or(0.f)
        };
    };
    auto [enclosingTop, enclosingBottom] = initialEnclosingTopAndBottom();
    auto contentOverflowRect = [&]() -> InlineRect {
        auto rect = lineBoxRect;
        auto rootInlineBoxWidth = lineBox.logicalRectForRootInlineBox().width();
        auto isLeftToRightDirection = root().style().isLeftToRightDirection();
        if (lineLayoutResult.hangingContent.shouldContributeToScrollableOverflow)
            rect.expandHorizontally(lineLayoutResult.hangingContent.logicalWidth);
        else if (!isLeftToRightDirection) {
            // This is to balance hanging RTL trailing content. See LineBoxBuilder::build.
            rootInlineBoxWidth -= lineLayoutResult.hangingContent.logicalWidth;
        }
        auto rootInlineBoxHorizontalOverflow = rootInlineBoxWidth - rect.width();
        if (rootInlineBoxHorizontalOverflow > 0)
            isLeftToRightDirection ? rect.shiftRightBy(rootInlineBoxHorizontalOverflow) : rect.shiftLeftBy(-rootInlineBoxHorizontalOverflow);
        return rect;
    }();
    for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
        if (!inlineLevelBox.isAtomicInlineLevelBox() && !inlineLevelBox.isInlineBox() && !inlineLevelBox.isLineBreakBox())
            continue;

        auto& layoutBox = inlineLevelBox.layoutBox();
        auto borderBox = InlineRect { };

        if (inlineLevelBox.isAtomicInlineLevelBox()) {
            borderBox = lineBox.logicalBorderBoxForAtomicInlineLevelBox(layoutBox, formattingContext().geometryForBox(layoutBox));
            borderBox.moveBy(lineBoxRect.topLeft());
        } else if (inlineLevelBox.isInlineBox()) {
            auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
            auto isContentful = [&] {
                // In standards mode, inline boxes always start with an imaginary strut.
                return layoutState().inStandardsMode() || inlineLevelBox.hasContent() || boxGeometry.horizontalBorder() || (boxGeometry.horizontalPadding() && boxGeometry.horizontalPadding().value());
            };
            if (!isContentful())
                continue;
            borderBox = lineBox.logicalBorderBoxForInlineBox(layoutBox, boxGeometry);
            borderBox.moveBy(lineBoxRect.topLeft());
            // Collect scrollable overflow from inline boxes. All other inline level boxes (e.g atomic inline level boxes) stretch the line.
            if (lineBox.hasContent()) {
                // Empty lines (e.g. continuation pre/post blocks) don't expect scrollbar overflow.
                contentOverflowRect.expandVerticallyToContain(borderBox);
            }
        } else if (inlineLevelBox.isLineBreakBox()) {
            borderBox = lineBox.logicalBorderBoxForInlineBox(layoutBox, formattingContext().geometryForBox(layoutBox));
            borderBox.moveBy(lineBoxRect.topLeft());
        } else
            ASSERT_NOT_REACHED();

        auto adjustedBorderBoxTop = borderBox.top() - inlineLevelBox.annotationAbove().value_or(0.f);
        auto adjustedBorderBoxBottom = borderBox.bottom() + inlineLevelBox.annotationBelow().value_or(0.f);
        enclosingTop = std::min(enclosingTop.value_or(adjustedBorderBoxTop), adjustedBorderBoxTop);
        enclosingBottom = std::max(enclosingBottom.value_or(adjustedBorderBoxBottom), adjustedBorderBoxBottom);
    }
    return { { enclosingTop.value_or(lineBoxRect.top()), enclosingBottom.value_or(lineBoxRect.top()) }, contentOverflowRect };
}

InlineDisplay::Line InlineDisplayLineBuilder::build(const LineBuilder::LayoutResult& lineLayoutResult, const LineBox& lineBox, const ConstraintsForInlineContent& constraints, bool lineIsFullyTruncatedInBlockDirection) const
{
    auto& rootInlineBox = lineBox.rootInlineBox();
    auto isLeftToRightDirection = lineLayoutResult.directionality.inlineBaseDirection == TextDirection::LTR;
    auto lineBoxLogicalRect = lineBox.logicalRect();
    auto lineBoxVisualLeft = isLeftToRightDirection
        ? lineBoxLogicalRect.left()
        : InlineLayoutUnit { constraints.visualLeft() + constraints.horizontal().logicalWidth + constraints.horizontal().logicalLeft  } - lineBoxLogicalRect.right();

    auto rootInlineBoxRect = lineBox.logicalRectForRootInlineBox();
    auto contentVisualOffsetInInlineDirection = isLeftToRightDirection
        ? rootInlineBoxRect.left()
        : lineBoxLogicalRect.width() - lineLayoutResult.contentGeometry.logicalRightIncludingNegativeMargin; // Note that with hanging content lineLayoutResult.contentGeometry.logicalRight is not the same as rootLineBoxRect.right().

    auto lineBoxVisualRectInInlineDirection = InlineRect { lineBoxLogicalRect.top(), lineBoxVisualLeft, lineBoxLogicalRect.width(), lineBoxLogicalRect.height() };
    auto enclosingLineGeometry = collectEnclosingLineGeometry(lineLayoutResult, lineBox, lineBoxVisualRectInInlineDirection);

    auto writingMode = root().style().writingMode();
    return InlineDisplay::Line { lineBoxLogicalRect
        , flipLogicalLineRectToVisualForWritingMode(lineBoxVisualRectInInlineDirection, writingMode)
        , flipLogicalLineRectToVisualForWritingMode(enclosingLineGeometry.contentOverflowRect, writingMode)
        , enclosingLineGeometry.enclosingTopAndBottom
        , rootInlineBox.logicalTop() + rootInlineBox.ascent()
        , lineBox.baselineType()
        , rootInlineBoxRect.left()
        , contentVisualOffsetInInlineDirection
        , rootInlineBox.logicalWidth()
        , isLeftToRightDirection
        , rootInlineBox.layoutBox().style().isHorizontalWritingMode()
        , lineIsFullyTruncatedInBlockDirection
    };
}

static float truncateTextContentWithMismatchingDirection(InlineDisplay::Box& displayBox, float contentWidth, float availableWidthForContent, bool canFullyTruncate)
{
    // While truncation normally starts at the end of the content and progress backwards, with mismatching direction
    // we take a different approach and truncate content the other way around (i.e. ellipsis follows inline direction truncating the beginning of the content).
    // <div dir=rtl>some long content</div>
    // [...ng content]
    auto& inlineTextBox = downcast<InlineTextBox>(displayBox.layoutBox());
    auto& textContent = displayBox.text();

    auto availableWidthForTruncatedContent = contentWidth - availableWidthForContent;
    auto truncatedSide = TextUtil::breakWord(inlineTextBox, textContent.start(), textContent.length(), contentWidth, availableWidthForTruncatedContent, { }, displayBox.style().fontCascade());

    ASSERT(truncatedSide.length < textContent.length());
    auto visibleLength = textContent.length() - truncatedSide.length;
    auto visibleWidth = contentWidth - truncatedSide.logicalWidth;
    // TextUtil::breakWord never returns overflowing content (left side) which means the right side is normally wider (by one character) than what we need.
    auto visibleContentOverflows = truncatedSide.logicalWidth < availableWidthForTruncatedContent;
    if (!visibleContentOverflows) {
        textContent.setPartiallyVisibleContentLength(visibleLength);
        return visibleWidth;
    }

    auto wouldFullyTruncate = visibleLength <= 1;
    if (wouldFullyTruncate && canFullyTruncate) {
        displayBox.setIsFullyTruncated();
        return { };
    }

    visibleLength = !wouldFullyTruncate ? visibleLength - 1 : 1;
    textContent.setPartiallyVisibleContentLength(visibleLength);
    auto visibleStartPosition = textContent.end() - visibleLength;
    return TextUtil::width(inlineTextBox, displayBox.style().fontCascade(), visibleStartPosition, textContent.end(), { }, TextUtil::UseTrailingWhitespaceMeasuringOptimization::No);
}

static float truncate(InlineDisplay::Box& displayBox, float contentWidth, float availableWidthForContent, bool canFullyTruncate)
{
    if (displayBox.isText()) {
        if (!availableWidthForContent && canFullyTruncate) {
            displayBox.setIsFullyTruncated();
            return { };
        }
        auto contentDirection = displayBox.bidiLevel() % 2 ? TextDirection::RTL : TextDirection::LTR;
        if (displayBox.layoutBox().parent().style().direction() != contentDirection)
            return truncateTextContentWithMismatchingDirection(displayBox, contentWidth, availableWidthForContent, canFullyTruncate);

        auto& inlineTextBox = downcast<InlineTextBox>(displayBox.layoutBox());
        auto& textContent = displayBox.text();
        auto visibleSide = TextUtil::breakWord(inlineTextBox, textContent.start(), textContent.length(), contentWidth, availableWidthForContent, { }, displayBox.style().fontCascade());
        if (visibleSide.length) {
            textContent.setPartiallyVisibleContentLength(visibleSide.length);
            return visibleSide.logicalWidth;
        }
        if (canFullyTruncate) {
            displayBox.setIsFullyTruncated();
            return { };
        }
        auto firstCharacterLength = TextUtil::firstUserPerceivedCharacterLength(inlineTextBox, textContent.start(), textContent.length());
        auto firstCharacterWidth = TextUtil::width(inlineTextBox, displayBox.style().fontCascade(), textContent.start(), textContent.start() + firstCharacterLength, { }, TextUtil::UseTrailingWhitespaceMeasuringOptimization::No);
        textContent.setPartiallyVisibleContentLength(firstCharacterLength);
        return firstCharacterWidth;
    }

    if (canFullyTruncate) {
        // Atomic inline level boxes are never partially truncated.
        displayBox.setIsFullyTruncated();
        return { };
    }
    return contentWidth;
}

static float truncateOverflowingDisplayBoxes(const InlineDisplay::Line& displayLine, InlineDisplay::Boxes& boxes, float ellipsisWidth, const RenderStyle& rootStyle, LineEndingEllipsisPolicy lineEndingEllipsisPolicy)
{
    // We gotta truncate some runs.
    ASSERT(displayLine.lineBoxLogicalRect().x() + displayLine.contentLogicalLeft() + displayLine.contentLogicalWidth() + ellipsisWidth > displayLine.lineBoxLogicalRect().maxX());
    auto isHorizontal = rootStyle.isHorizontalWritingMode();
    auto left = [&] (auto& displayBox) {
        return isHorizontal ? displayBox.left() : displayBox.top();
    };
    auto right = [&] (auto& displayBox) {
        return isHorizontal ? displayBox.right() : displayBox.bottom();
    };
    auto width = [&] (auto& displayBox) {
        return isHorizontal ? displayBox.width() : displayBox.height();
    };
    // The logically first character or atomic inline-level element on a line must be clipped rather than ellipsed.
    auto isFirstContentRun = true;
    if (rootStyle.isLeftToRightDirection()) {
        auto visualRightForContentEnd = std::max(0.f, right(displayLine) - ellipsisWidth);
#if USE_FLOAT_AS_INLINE_LAYOUT_UNIT
        if (visualRightForContentEnd)
            visualRightForContentEnd += LayoutUnit::epsilon();
#endif
        auto truncateRight = std::optional<float> { };
        for (auto& displayBox : boxes) {
            if (displayBox.isInlineBox())
                continue;
            if (right(displayBox) > visualRightForContentEnd) {
                auto visibleWidth = truncate(displayBox, width(displayBox), std::max(0.f, visualRightForContentEnd - left(displayBox)), !isFirstContentRun);
                auto truncatePosition = left(displayBox) + visibleWidth;
                truncateRight = truncateRight.value_or(truncatePosition);
            }
            isFirstContentRun = false;
        }
        ASSERT_UNUSED(lineEndingEllipsisPolicy, lineEndingEllipsisPolicy != LineEndingEllipsisPolicy::WhenContentOverflowsInInlineDirection || truncateRight.has_value() || right(boxes.last()) == visualRightForContentEnd);
        return truncateRight.value_or(right(boxes.last()));
    }

    auto truncateLeft = std::optional<float> { };
    auto visualLeftForContentEnd = std::max(0.f, left(displayLine) + ellipsisWidth);
#if USE_FLOAT_AS_INLINE_LAYOUT_UNIT
    if (visualLeftForContentEnd)
        visualLeftForContentEnd -= LayoutUnit::epsilon();
#endif
    for (auto& displayBox : makeReversedRange(boxes)) {
        if (displayBox.isInlineBox())
            continue;
        if (left(displayBox) < visualLeftForContentEnd) {
            auto visibleWidth = truncate(displayBox, width(displayBox), std::max(0.f, right(displayBox) - visualLeftForContentEnd), !isFirstContentRun);
            auto truncatePosition = right(displayBox) - visibleWidth;
            truncateLeft = truncateLeft.value_or(truncatePosition);
        }
        isFirstContentRun = false;
    }
    ASSERT_UNUSED(lineEndingEllipsisPolicy, lineEndingEllipsisPolicy != LineEndingEllipsisPolicy::WhenContentOverflowsInInlineDirection || truncateLeft.has_value() || left(boxes.first()) == visualLeftForContentEnd);
    return truncateLeft.value_or(left(boxes.first())) - ellipsisWidth;
}

std::optional<FloatRect> InlineDisplayLineBuilder::trailingEllipsisVisualRectAfterTruncation(LineEndingEllipsisPolicy lineEndingEllipsisPolicy, const InlineDisplay::Line& displayLine, InlineDisplay::Boxes& displayBoxes, bool isLastLineWithInlineContent)
{
    if (displayBoxes.isEmpty())
        return { };

    auto contentNeedsEllipsis = [&] {
        switch (lineEndingEllipsisPolicy) {
        case LineEndingEllipsisPolicy::No:
            return false;
        case LineEndingEllipsisPolicy::WhenContentOverflowsInInlineDirection:
            return displayLine.contentLogicalWidth() > displayLine.lineBoxLogicalRect().width();
        case LineEndingEllipsisPolicy::WhenContentOverflowsInBlockDirection:
            if (isLastLineWithInlineContent)
                return false;
            FALLTHROUGH;
        case LineEndingEllipsisPolicy::Always:
            return true;
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    };
    if (!contentNeedsEllipsis())
        return { };

    ASSERT(displayBoxes[0].isRootInlineBox());
    auto& rootInlineBox = displayBoxes[0];
    auto& rootStyle = rootInlineBox.style();
    auto ellipsisWidth = rootStyle.fontCascade().width(TextUtil::ellipsisTextRun());

    auto contentNeedsTruncation = [&] {
        switch (lineEndingEllipsisPolicy) {
        case LineEndingEllipsisPolicy::WhenContentOverflowsInInlineDirection:
            ASSERT(displayLine.contentLogicalWidth() > displayLine.lineBoxLogicalRect().width());
            return true;
        case LineEndingEllipsisPolicy::WhenContentOverflowsInBlockDirection:
        case LineEndingEllipsisPolicy::Always:
            return displayLine.contentLogicalLeft() + displayLine.contentLogicalWidth() + ellipsisWidth > displayLine.lineBoxLogicalRect().maxX();
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    };

    auto ellipsisStart = 0.f;
    if (!contentNeedsTruncation()) {
        // The content does not overflow the line box. The ellipsis is supposed to be either visually trailing or leading depending on the inline direction.
        if (displayBoxes.size() > 1)
            ellipsisStart = rootStyle.isLeftToRightDirection() ? displayBoxes.last().right() : displayBoxes[1].left() - ellipsisWidth;
        else {
            // All we have is the root inline box.
            ellipsisStart = displayBoxes.first().left();
        }
    } else
        ellipsisStart = truncateOverflowingDisplayBoxes(displayLine, displayBoxes, ellipsisWidth, rootStyle, lineEndingEllipsisPolicy);

    if (rootStyle.isHorizontalWritingMode())
        return FloatRect { ellipsisStart, rootInlineBox.top(), ellipsisWidth, rootInlineBox.height() };
    return FloatRect { rootInlineBox.left(), ellipsisStart, rootInlineBox.width(), ellipsisWidth };
}

}
}

