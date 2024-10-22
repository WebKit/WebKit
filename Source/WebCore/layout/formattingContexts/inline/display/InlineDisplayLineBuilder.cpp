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
    switch (writingMode.blockDirection()) {
    case FlowDirection::TopToBottom:
    case FlowDirection::BottomToTop:
        return lineLogicalRect;
    case FlowDirection::LeftToRight:
    case FlowDirection::RightToLeft:
        // See InlineFormattingUtils for more info.
        return { lineLogicalRect.left(), lineLogicalRect.top(), lineLogicalRect.height(), lineLogicalRect.width() };
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return lineLogicalRect;
}

InlineDisplayLineBuilder::InlineDisplayLineBuilder(InlineFormattingContext& inlineFormattingContext, const ConstraintsForInlineContent& constraints)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_constraints(constraints)
{
}

InlineDisplayLineBuilder::EnclosingLineGeometry InlineDisplayLineBuilder::collectEnclosingLineGeometry(const LineLayoutResult& lineLayoutResult, const LineBox& lineBox, const InlineRect& lineBoxRect) const
{
    auto& rootInlineBox = lineBox.rootInlineBox();
    auto initialEnclosingTopAndBottom = [&]() -> std::tuple<std::optional<InlineLayoutUnit>, std::optional<InlineLayoutUnit>>  {
        if (!lineBox.hasContent() || !rootInlineBox.hasContent())
            return { };
        return {
            lineBoxRect.top() + rootInlineBox.logicalTop() - rootInlineBox.textEmphasisAbove().value_or(0.f),
            lineBoxRect.top() + rootInlineBox.logicalBottom() + rootInlineBox.textEmphasisBelow().value_or(0.f)
        };
    };
    auto [enclosingTop, enclosingBottom] = initialEnclosingTopAndBottom();
    auto contentOverflowRect = [&]() -> InlineRect {
        auto rect = lineBoxRect;
        auto rootInlineBoxWidth = lineBox.logicalRectForRootInlineBox().width();
        auto isLeftToRightDirection = root().writingMode().isBidiLTR();
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
        if (!inlineLevelBox.isAtomicInlineBox() && !inlineLevelBox.isInlineBox() && !inlineLevelBox.isLineBreakBox())
            continue;

        auto& layoutBox = inlineLevelBox.layoutBox();
        auto borderBox = InlineRect { };

        if (inlineLevelBox.isAtomicInlineBox()) {
            borderBox = lineBox.logicalBorderBoxForAtomicInlineBox(layoutBox, formattingContext().geometryForBox(layoutBox));
            borderBox.moveBy(lineBoxRect.topLeft());
        } else if (inlineLevelBox.isInlineBox()) {
            auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
            // In standards mode, inline boxes always start with an imaginary strut.
            auto isContentful = formattingContext().layoutState().inStandardsMode() || inlineLevelBox.hasContent() || boxGeometry.horizontalBorderAndPadding();
            if (!isContentful)
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

        auto adjustedBorderBoxTop = borderBox.top() - inlineLevelBox.textEmphasisAbove().value_or(0.f);
        auto adjustedBorderBoxBottom = borderBox.bottom() + inlineLevelBox.textEmphasisBelow().value_or(0.f);
        enclosingTop = std::min(enclosingTop.value_or(adjustedBorderBoxTop), adjustedBorderBoxTop);
        enclosingBottom = std::max(enclosingBottom.value_or(adjustedBorderBoxBottom), adjustedBorderBoxBottom);
    }
    return { { enclosingTop.value_or(lineBoxRect.top()), enclosingBottom.value_or(lineBoxRect.top()) }, contentOverflowRect };
}

InlineDisplay::Line InlineDisplayLineBuilder::build(const LineLayoutResult& lineLayoutResult, const LineBox& lineBox, bool lineIsFullyTruncatedInBlockDirection) const
{
    auto& constraints = this->constraints();
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

    auto writingMode = root().writingMode();
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
        , rootInlineBox.layoutBox().writingMode().isHorizontal()
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
        if (displayBox.layoutBox().parent().style().writingMode().bidiDirection() != contentDirection)
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

static float truncateOverflowingDisplayBoxes(InlineDisplay::Boxes& boxes, size_t startIndex, size_t endIndex, float lineBoxVisualLeft, float lineBoxVisualRight, float ellipsisWidth, const RenderStyle& rootStyle, LineEndingTruncationPolicy lineEndingTruncationPolicy)
{
    ASSERT(endIndex && startIndex <= endIndex);
    // We gotta truncate some runs.
    auto isHorizontal = rootStyle.writingMode().isHorizontal();
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
    if (rootStyle.writingMode().isBidiLTR()) {
        auto visualRightForContentEnd = std::max(0.f, lineBoxVisualRight - ellipsisWidth);
#if USE_FLOAT_AS_INLINE_LAYOUT_UNIT
        if (visualRightForContentEnd)
            visualRightForContentEnd += LayoutUnit::epsilon();
#endif
        auto truncateRight = std::optional<float> { };
        for (auto index = startIndex; index <= endIndex; ++index) {
            auto& displayBox = boxes[index];
            if (displayBox.isInlineBox())
                continue;
            if (right(displayBox) > visualRightForContentEnd) {
                auto visibleWidth = truncate(displayBox, width(displayBox), std::max(0.f, visualRightForContentEnd - left(displayBox)), !isFirstContentRun);
                auto truncatePosition = left(displayBox) + visibleWidth;
                truncateRight = truncateRight.value_or(truncatePosition);
            }
            isFirstContentRun = false;
        }
        ASSERT_UNUSED(lineEndingTruncationPolicy, lineEndingTruncationPolicy != LineEndingTruncationPolicy::WhenContentOverflowsInInlineDirection || truncateRight.has_value() || right(boxes.last()) == visualRightForContentEnd || boxes.last().isInlineBox());
        return truncateRight.value_or(right(boxes.last()));
    }

    auto truncateLeft = std::optional<float> { };
    auto visualLeftForContentEnd = std::max(0.f, lineBoxVisualLeft + ellipsisWidth);
#if USE_FLOAT_AS_INLINE_LAYOUT_UNIT
    if (visualLeftForContentEnd)
        visualLeftForContentEnd -= LayoutUnit::epsilon();
#endif
    for (size_t index = endIndex + 1; index-- > startIndex;) {
        auto& displayBox = boxes[index];
        if (displayBox.isInlineBox())
            continue;
        if (left(displayBox) < visualLeftForContentEnd) {
            auto visibleWidth = truncate(displayBox, width(displayBox), std::max(0.f, right(displayBox) - visualLeftForContentEnd), !isFirstContentRun);
            auto truncatePosition = right(displayBox) - visibleWidth;
            truncateLeft = truncateLeft.value_or(truncatePosition);
        }
        isFirstContentRun = false;
    }
    ASSERT_UNUSED(lineEndingTruncationPolicy, lineEndingTruncationPolicy != LineEndingTruncationPolicy::WhenContentOverflowsInInlineDirection || truncateLeft.has_value() || left(boxes.first()) == visualLeftForContentEnd);
    return truncateLeft.value_or(left(boxes.first())) - ellipsisWidth;
}

static std::optional<FloatRect> trailingEllipsisVisualRectAfterTruncation(LineEndingTruncationPolicy lineEndingTruncationPolicy, String ellipsisText, const InlineDisplay::Line& displayLine, InlineDisplay::Boxes& displayBoxes, bool isLastLineWithInlineContent)
{
    ASSERT(lineEndingTruncationPolicy != LineEndingTruncationPolicy::NoTruncation);
    if (displayBoxes.isEmpty())
        return { };

    auto needsEllipsis = [&] {
        if (lineEndingTruncationPolicy == LineEndingTruncationPolicy::WhenContentOverflowsInInlineDirection)
            return displayLine.contentLogicalWidth() && displayLine.contentLogicalWidth() > displayLine.lineBoxLogicalRect().width();
        ASSERT(lineEndingTruncationPolicy == LineEndingTruncationPolicy::WhenContentOverflowsInBlockDirection);
        return !isLastLineWithInlineContent;
    };
    if (!needsEllipsis())
        return { };

    ASSERT(displayBoxes[0].isRootInlineBox());
    auto& rootInlineBox = displayBoxes[0];
    auto& rootStyle = rootInlineBox.style();
    auto ellipsisWidth = std::max(0.f, rootStyle.fontCascade().width(TextRun { ellipsisText }));

    auto contentNeedsTruncation = [&] {
        switch (lineEndingTruncationPolicy) {
        case LineEndingTruncationPolicy::WhenContentOverflowsInInlineDirection:
            ASSERT(displayLine.contentLogicalWidth() > displayLine.lineBoxLogicalRect().width());
            return true;
        case LineEndingTruncationPolicy::WhenContentOverflowsInBlockDirection:
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
            ellipsisStart = rootStyle.writingMode().isBidiLTR() ? displayBoxes.last().right() : displayBoxes[1].left() - ellipsisWidth;
        else {
            // All we have is the root inline box.
            ellipsisStart = displayBoxes.first().left();
        }
    } else {
        auto lineBoxVisualLeft = rootStyle.writingMode().isHorizontal() ? displayLine.left() : displayLine.top();
        auto lineBoxVisualRight = std::max(rootStyle.writingMode().isHorizontal() ? displayLine.right() : displayLine.bottom(), lineBoxVisualLeft);
        ellipsisStart = truncateOverflowingDisplayBoxes(displayBoxes, 0, displayBoxes.size() - 1, lineBoxVisualLeft, lineBoxVisualRight, ellipsisWidth, rootStyle, lineEndingTruncationPolicy);
    }

    if (rootStyle.writingMode().isHorizontal())
        return FloatRect { ellipsisStart, rootInlineBox.top(), ellipsisWidth, rootInlineBox.height() };
    return FloatRect { rootInlineBox.left(), ellipsisStart, rootInlineBox.width(), ellipsisWidth };
}

static inline bool isEligibleForLinkBoxLineClamp(auto& displayBoxes)
{
    if (displayBoxes.size() < 3) {
        // We need at least 3 display boxes to generate content with link ([root inline box][inline box][content])
        return false;
    }
    auto writingMode = displayBoxes[0].layoutBox().writingMode();
    if (writingMode.isBidiRTL() || writingMode.isVertical())
        return false;
    auto& linkCandidateBox = displayBoxes[displayBoxes.size() - 2];
    if (!linkCandidateBox.isNonRootInlineBox() || !linkCandidateBox.isFirstForLayoutBox()) {
        // Link spanning multiple lines looks odd with line-clamp.
        return false;
    }
    return linkCandidateBox.layoutBox().style().isLink();
}

static constexpr int legacyMatchingLinkBoxOffset = 3;
static inline void makeRoomForLinkBoxOnClampedLineIfNeeded(auto& clampedLine, auto& displayBoxes, auto insertionPosition, auto linkContentWidth)
{
    ASSERT(insertionPosition);
    auto ellipsisBoxRect = clampedLine.ellipsis()->visualRect;
    if (ellipsisBoxRect.maxX() + linkContentWidth <= clampedLine.right())
        return;
    auto& rootStyle = displayBoxes[0].layoutBox().style();
    auto startIndex = [&]() -> size_t {
        ASSERT(insertionPosition);
        for (size_t index = insertionPosition - 1; index--;) {
            if (displayBoxes[index].isRootInlineBox())
                return index + 1;
            return index;
        }
        ASSERT_NOT_REACHED();
        return 0;
    };
    auto endIndex = insertionPosition - 1;
    auto ellispsisPosition = clampedLine.right() - linkContentWidth - legacyMatchingLinkBoxOffset;
    auto ellipsisStart = truncateOverflowingDisplayBoxes(displayBoxes, startIndex(), endIndex, clampedLine.left(), ellispsisPosition, ellipsisBoxRect.width(), rootStyle, LineEndingTruncationPolicy::WhenContentOverflowsInBlockDirection);
    ellipsisBoxRect.setX(ellipsisStart);
    clampedLine.setEllipsis({ InlineDisplay::Line::Ellipsis::Type::Block, ellipsisBoxRect, TextUtil::ellipsisTextInInlineDirection(clampedLine.isHorizontal()) });
}

static inline void moveDisplayBoxToClampedLine(auto& displayLines, auto clampedLineIndex, auto& displayBox, auto horizontalOffset)
{
    auto& clampedLine = displayLines[clampedLineIndex];
    displayBox.setLeft(clampedLine.ellipsis()->visualRect.maxX() + horizontalOffset + legacyMatchingLinkBoxOffset);
    // Assume baseline alignment here.
    displayBox.moveVertically((clampedLine.top() + clampedLine.baseline()) - (displayLines.last().top() + displayLines.last().baseline()));
    displayBox.moveToLine(clampedLineIndex);
}

void InlineDisplayLineBuilder::addLegacyLineClampTrailingLinkBoxIfApplicable(const InlineFormattingContext& inlineFormattingContext, const InlineLayoutState& inlineLayoutState, InlineDisplay::Content& displayContent)
{
    // This is link-box type of line clamping (-webkit-line-clamp) where we check if the inline content ends in a link
    // and move such link content next to the clamped line's trailing ellispsis. It is meant to produce the following rendering
    //
    // first line
    // second line is clamped... more info
    //
    // where "more info" is a link and it comes from the end of the inline content (normally invisible due to block direction clamping)
    // This is to match legacy line clamping behavior (and not a block-ellispsis: <string> implementation) which was introduced
    // at https://commits.webkit.org/6086@main to support special article rendering with clamped lines.
    // It supports horizontal, left-to-right content only where the link inline box has (non-split) text content and when
    // the link content ('more info') fits the clamped line.
    auto clampedLineIndex = inlineLayoutState.legacyClampedLineIndex();
    if (!clampedLineIndex)
        return;
    auto& displayLines = displayContent.lines;
    if (*clampedLineIndex >= displayLines.size() || !displayLines[*clampedLineIndex].hasEllipsis()) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (!isEligibleForLinkBoxLineClamp(displayContent.boxes))
        return;

    auto& displayBoxes = displayContent.boxes;
    auto insertionPosition = [&]() -> std::optional<size_t> {
        for (size_t boxIndex = 0; boxIndex < displayBoxes.size(); ++boxIndex) {
            if (displayBoxes[boxIndex].lineIndex() > *clampedLineIndex)
                return boxIndex;
        }
        return { };
    }();

    if (!insertionPosition || !*insertionPosition || *insertionPosition >= displayBoxes.size() - 1) {
        // Unexpected cases where the insertion point is at the leading/trailing box. They both indicate incorrect line-clamp
        // position and would produce incorrect rendering.
        ASSERT_NOT_REACHED_WITH_SECURITY_IMPLICATION();
        return;
    }
    auto& clampedLine = displayLines[*clampedLineIndex];
    auto linkContentWidth = displayBoxes[displayBoxes.size() - 2].visualRectIgnoringBlockDirection().width();
    if (linkContentWidth >= clampedLine.lineBoxWidth()) {
        // Not enough space for "more info" content.
        return;
    }
    auto linkContentDisplayBox = displayBoxes.takeLast();
    auto linkInlineBoxDisplayBox = displayBoxes.takeLast();

    auto handleTrailingLineBreakIfApplicable = [&] {
        // "more info" gets inserted after the trailing run on the clamped line unless the trailing content is forced line break.
        auto& trailingDisplayBox = displayBoxes[*insertionPosition - 1];
        if (!trailingDisplayBox.isLineBreak())
            return;
        // Move trailing line break after the link box.
        trailingDisplayBox.moveHorizontally(linkContentWidth);
        --*insertionPosition;
    };
    handleTrailingLineBreakIfApplicable();

    makeRoomForLinkBoxOnClampedLineIfNeeded(clampedLine, displayBoxes, *insertionPosition, linkContentWidth);

    // link box content
    moveDisplayBoxToClampedLine(displayLines, *clampedLineIndex, linkContentDisplayBox, inlineFormattingContext.geometryForBox(linkInlineBoxDisplayBox.layoutBox()).marginBorderAndPaddingStart());
    displayBoxes.insert(*insertionPosition, linkContentDisplayBox);
    // Link inline box
    moveDisplayBoxToClampedLine(displayLines, *clampedLineIndex, linkInlineBoxDisplayBox, LayoutUnit { });
    displayBoxes.insert(*insertionPosition, linkInlineBoxDisplayBox);

    clampedLine.setHasContentAfterEllipsisBox();
}

void InlineDisplayLineBuilder::applyEllipsisIfNeeded(LineEndingTruncationPolicy truncationPolicy, InlineDisplay::Line& displayLine, InlineDisplay::Boxes& displayBoxes, bool isLastLineWithInlineContent, bool isLegacyLineClamp)
{
    if (truncationPolicy == LineEndingTruncationPolicy::NoTruncation || !displayBoxes.size())
        return;

    auto ellipsisText = [&] {
        if (truncationPolicy == LineEndingTruncationPolicy::WhenContentOverflowsInInlineDirection || isLegacyLineClamp) {
            // Legacy line clamp always uses ...
            return TextUtil::ellipsisTextInInlineDirection(displayLine.isHorizontal());
        }
        auto& blockEllipsis = displayBoxes[0].layoutBox().style().blockEllipsis();
        if (blockEllipsis.type == BlockEllipsis::Type::None)
            return nullAtom();
        if (blockEllipsis.type == BlockEllipsis::Type::Auto)
            return TextUtil::ellipsisTextInInlineDirection(displayLine.isHorizontal());
        return blockEllipsis.string;
    }();

    if (ellipsisText.isNull())
        return;

    if (auto ellipsisRect = trailingEllipsisVisualRectAfterTruncation(truncationPolicy, ellipsisText, displayLine, displayBoxes, isLastLineWithInlineContent))
        displayLine.setEllipsis({ truncationPolicy == LineEndingTruncationPolicy::WhenContentOverflowsInInlineDirection ? InlineDisplay::Line::Ellipsis::Type::Inline : InlineDisplay::Line::Ellipsis::Type::Block, *ellipsisRect, ellipsisText });
}

}
}

