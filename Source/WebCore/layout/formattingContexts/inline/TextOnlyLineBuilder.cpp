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
#include "TextOnlyLineBuilder.h"

namespace WebCore {
namespace Layout {

struct TextOnlyLineBreakResult {
    InlineContentBreaker::IsEndOfLine isEndOfLine { InlineContentBreaker::IsEndOfLine::Yes };
    size_t committedCount { 0 };
    size_t overflowingContentLength { 0 };
    bool isRevert { false };
};

static inline InlineLayoutUnit inlineTextItemWidth(const InlineTextItem& inlineTextItem, const RenderStyle& style, InlineLayoutUnit contentLogicalLeft)
{
    if (auto contentWidth = inlineTextItem.width())
        return *contentWidth;
    if (!inlineTextItem.isWhitespace() || InlineTextItem::shouldPreserveSpacesAndTabs(inlineTextItem))
        return TextUtil::width(inlineTextItem, style.fontCascade(), contentLogicalLeft);
    return TextUtil::width(inlineTextItem, style.fontCascade(), inlineTextItem.start(), inlineTextItem.start() + 1, contentLogicalLeft);
}

static inline InlineItemPosition placedInlineItemEnd(size_t layoutRangeStartIndex, size_t placedInlineItemCount, size_t overflowingContentLength, const InlineItems& inlineItems)
{
    if (!overflowingContentLength)
        return { layoutRangeStartIndex + placedInlineItemCount };

    auto trailingInlineItemIndex = layoutRangeStartIndex + placedInlineItemCount - 1;
    auto overflowingInlineTextItemLength = downcast<InlineTextItem>(inlineItems[trailingInlineItemIndex]).length();
    return { trailingInlineItemIndex, overflowingInlineTextItemLength - overflowingContentLength };
}

static inline bool isLastLineWithInlineContent(InlineItemPosition placedContentEnd, size_t layoutRangeEndIndex)
{
    return placedContentEnd.index == layoutRangeEndIndex && !placedContentEnd.offset;
}

static inline void appendInlineTextItem(const InlineTextItem& inlineTextItem, bool isFirstFormattedLine, const Line& line, InlineContentBreaker::ContinuousContent& candidateContent)
{
    auto& style = isFirstFormattedLine ? inlineTextItem.firstLineStyle() : inlineTextItem.style();
    auto textContentlWidth = [&] {
        if (auto textWidth = inlineTextItem.width())
            return *textWidth;
        return inlineTextItemWidth(inlineTextItem, style, line.contentLogicalRight() + candidateContent.logicalWidth());
    };
    candidateContent.appendTextContent(inlineTextItem, style, textContentlWidth());
}

static inline bool consumeTrailingLineBreakIfApplicable(const TextOnlyLineBreakResult& result, size_t trailingInlineItemIndex, size_t layoutRangeEnd, Line& line, const InlineItems& inlineItems)
{
    // Trailing forced line break should be consumed after fully placed content.
    auto shouldConsumeTrailingLineBreak = [&] {
        if (result.overflowingContentLength || result.isRevert)
            return false;
        return trailingInlineItemIndex < layoutRangeEnd && inlineItems[trailingInlineItemIndex].isLineBreak();
    };
    if (!shouldConsumeTrailingLineBreak())
        return false;
    auto& trailingLineBreak = inlineItems[trailingInlineItemIndex];
    ASSERT(trailingLineBreak.isLineBreak());
    line.append(trailingLineBreak, trailingLineBreak.style(), { });
    return true;
}

static InlineLayoutUnit horizontalAlignmentOffset(const RenderStyle& rootStyle, bool isLastLine, const Line::RunList& runs, InlineLayoutUnit contentLogicalRight, InlineLayoutUnit lineLogicalRight, InlineLayoutUnit hangingTrailingWidth)
{
    // Depending on the line’s alignment/justification, the hanging glyph can be placed outside the line box.
    if (hangingTrailingWidth) {
        // If white-space is set to pre-wrap, the UA must (unconditionally) hang this sequence, unless the sequence is followed
        // by a forced line break, in which case it must conditionally hang the sequence is instead.
        // Note that end of last line in a paragraph is considered a forced break.
        auto isConditionalHanging = runs.last().isLineBreak() || isLastLine;
        // In some cases, a glyph at the end of a line can conditionally hang: it hangs only if it does not otherwise fit in the line prior to justification.
        if (isConditionalHanging) {
            // FIXME: Conditional hanging needs partial overflow trimming at glyph boundary, one by one until they fit.
            contentLogicalRight = std::min(contentLogicalRight, lineLogicalRight);
        } else
            contentLogicalRight -= hangingTrailingWidth;
    }
    auto isLastLineOrAfterLineBreak = isLastLine || (!runs.isEmpty() && runs.last().isLineBreak()) ? InlineFormattingGeometry::IsLastLineOrAfterLineBreak::Yes : InlineFormattingGeometry::IsLastLineOrAfterLineBreak::No;
    return InlineFormattingGeometry::horizontalAlignmentOffset(rootStyle, lineLogicalRight - contentLogicalRight, isLastLineOrAfterLineBreak, TextDirection::LTR);
}

TextOnlyLineBuilder::TextOnlyLineBuilder(const InlineFormattingContext& inlineFormattingContext, HorizontalConstraints rootHorizontalConstraints, const InlineItems& inlineItems)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_rootHorizontalConstraints(rootHorizontalConstraints)
    , m_line(inlineFormattingContext)
    , m_inlineItems(inlineItems)
{
}

LineLayoutResult TextOnlyLineBuilder::layoutInlineContent(const LineInput& lineInput, const std::optional<PreviousLine>& previousLine)
{
    initialize(lineInput.needsLayoutRange, lineInput.initialLogicalRect, previousLine);
    auto placedContentEnd = TextUtil::isWrappingAllowed(root().style()) ? placeInlineTextContent(lineInput.needsLayoutRange) : placeNonWrappingInlineTextContent(lineInput.needsLayoutRange);
    auto result = m_line.close();

    auto isLastLine = isLastLineWithInlineContent(placedContentEnd, lineInput.needsLayoutRange.endIndex());
    auto& rootStyle = isFirstFormattedLine() ? root().firstLineStyle() : root().style();
    auto contentLogicalLeft = horizontalAlignmentOffset(rootStyle, isLastLine, result.runs, result.contentLogicalRight, m_lineLogicalRect.width(), result.hangingTrailingContentWidth);

    return { { lineInput.needsLayoutRange.start, placedContentEnd }
        , WTFMove(result.runs)
        , { }
        , { contentLogicalLeft, result.contentLogicalWidth, contentLogicalLeft + result.contentLogicalRight, { } }
        , { m_lineLogicalRect.topLeft(), m_lineLogicalRect.width(), m_lineLogicalRect.left()  }
        , { !result.isHangingTrailingContentWhitespace, result.hangingTrailingContentWidth }
        , { }
        , { isFirstFormattedLine() ? LineLayoutResult::IsFirstLast::FirstFormattedLine::WithinIFC : LineLayoutResult::IsFirstLast::FirstFormattedLine::No, isLastLine }
    };
}

void TextOnlyLineBuilder::initialize(const InlineItemRange& layoutRange, const InlineRect& initialLogicalRect, const std::optional<PreviousLine>& previousLine)
{
    ASSERT(!layoutRange.isEmpty() || (previousLine && !previousLine->suspendedFloats.isEmpty()));
    auto partialLeadingTextItem = [&]() -> std::optional<InlineTextItem> {
        if (!previousLine || !layoutRange.start.offset)
            return { };

        auto& overflowingInlineTextItem = downcast<InlineTextItem>(m_inlineItems[layoutRange.start.index]);
        ASSERT(layoutRange.start.offset < overflowingInlineTextItem.length());
        auto overflowingLength = overflowingInlineTextItem.length() - layoutRange.start.offset;
        if (overflowingLength) {
            // Turn previous line's overflow content into the next line's leading content.
            // "sp[<-line break->]lit_content" -> break position: 2 -> leading partial content length: 11.
            return overflowingInlineTextItem.right(overflowingLength, previousLine->trailingOverflowingContentWidth);
        }
        return { };
    };
    m_partialLeadingTextItem = partialLeadingTextItem();
    m_line.initialize({ }, isFirstFormattedLine());
    m_previousLine = previousLine;
    m_lineLogicalRect = initialLogicalRect;
    m_wrapOpportunityList = { };
}

InlineItemPosition TextOnlyLineBuilder::placeInlineTextContent(const InlineItemRange& layoutRange)
{
    auto candidateContent = InlineContentBreaker::ContinuousContent { };
    auto isFirstFormattedLine = this->isFirstFormattedLine();
    auto isEndOfLine = false;

    auto nextItemIndex = layoutRange.startIndex();
    auto result = TextOnlyLineBreakResult { };
    auto placeSingleInlineTextItem = [&] (auto& inlineTextItem) {
        candidateContent.reset();
        appendInlineTextItem(inlineTextItem, isFirstFormattedLine, m_line, candidateContent);
        result = handleInlineTextContent(candidateContent, layoutRange);
        nextItemIndex = !result.isRevert ? nextItemIndex + result.committedCount : result.committedCount;
        isEndOfLine = result.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes || nextItemIndex >= layoutRange.endIndex();
    };

    // Handle leading partial content first (overflowing text from the previous line).
    if (m_partialLeadingTextItem)
        placeSingleInlineTextItem(*m_partialLeadingTextItem);

    while (!isEndOfLine) {
        auto& inlineItem = m_inlineItems[nextItemIndex];
        ASSERT(inlineItem.isText() || inlineItem.isLineBreak());

        if (inlineItem.isText()) {
            placeSingleInlineTextItem(downcast<InlineTextItem>(inlineItem));
            continue;
        }
        if (inlineItem.isLineBreak()) {
            isEndOfLine = true;
            continue;
        }
        // Skip unsupported content.
        ++nextItemIndex;
    }
    if (consumeTrailingLineBreakIfApplicable(result, nextItemIndex, layoutRange.endIndex(), m_line, m_inlineItems))
        ++nextItemIndex;

    auto placedInlineItemCount = nextItemIndex - layoutRange.startIndex();
    auto placedContentEnd = placedInlineItemEnd(layoutRange.startIndex(), placedInlineItemCount, result.overflowingContentLength, m_inlineItems);
    handleLineEnding(placedContentEnd, layoutRange.endIndex());
    return placedContentEnd;
}

InlineItemPosition TextOnlyLineBuilder::placeNonWrappingInlineTextContent(const InlineItemRange& layoutRange)
{
    ASSERT(!TextUtil::isWrappingAllowed(root().style()));

    auto candidateContent = InlineContentBreaker::ContinuousContent { };
    auto isFirstFormattedLine = this->isFirstFormattedLine();
    auto isEndOfLine = false;
    auto trailingLineBreakIndex = std::optional<size_t> { };
    auto nextItemIndex = layoutRange.startIndex();

    if (m_partialLeadingTextItem) {
        appendInlineTextItem(*m_partialLeadingTextItem, isFirstFormattedLine, m_line, candidateContent);
        ++nextItemIndex;
        isEndOfLine = nextItemIndex >= layoutRange.endIndex();
    }
    while (!isEndOfLine) {
        auto& inlineItem = m_inlineItems[nextItemIndex];
        ASSERT(inlineItem.isText() || inlineItem.isLineBreak());
        if (inlineItem.isText())
            appendInlineTextItem(downcast<InlineTextItem>(inlineItem), isFirstFormattedLine, m_line, candidateContent);
        else if (inlineItem.isLineBreak())
            trailingLineBreakIndex = nextItemIndex;
        ++nextItemIndex;
        isEndOfLine = nextItemIndex >= layoutRange.endIndex() || trailingLineBreakIndex.has_value();
    }

    if (trailingLineBreakIndex && candidateContent.runs().isEmpty()) {
        m_line.append(m_inlineItems[*trailingLineBreakIndex], m_inlineItems[*trailingLineBreakIndex].style(), { });
        return { *trailingLineBreakIndex + 1, { } };
    }

    auto result = handleInlineTextContent(candidateContent, layoutRange);
    nextItemIndex = layoutRange.startIndex() + result.committedCount;
    if (consumeTrailingLineBreakIfApplicable(result, nextItemIndex, layoutRange.endIndex(), m_line, m_inlineItems))
        ++nextItemIndex;

    auto placedInlineItemCount = nextItemIndex - layoutRange.startIndex();
    auto placedContentEnd = placedInlineItemEnd(layoutRange.startIndex(), placedInlineItemCount, result.overflowingContentLength, m_inlineItems);
    handleLineEnding(placedContentEnd, layoutRange.endIndex());
    return placedContentEnd;
}

TextOnlyLineBreakResult TextOnlyLineBuilder::handleInlineTextContent(const InlineContentBreaker::ContinuousContent& candidateContent, const InlineItemRange& layoutRange)
{
    auto availableWidth = (m_lineLogicalRect.width() + LayoutUnit::epsilon()) - m_line.contentLogicalRight();
    auto lineStatus = InlineContentBreaker::LineStatus { m_line.contentLogicalRight(), availableWidth, m_line.trimmableTrailingWidth(), m_line.trailingSoftHyphenWidth(), m_line.isTrailingRunFullyTrimmable(), m_line.hasContentOrListMarker(), !m_wrapOpportunityList.isEmpty() };
    m_inlineContentBreaker.setIsInIntrinsicWidthMode(isInIntrinsicWidthMode());
    auto lineBreakingResult = m_inlineContentBreaker.processInlineContent(candidateContent, lineStatus);

    ASSERT(!candidateContent.runs().isEmpty());

    if (lineBreakingResult.action == InlineContentBreaker::Result::Action::Keep) {
        auto& committedRuns = candidateContent.runs();
        for (auto& run : committedRuns)
            m_line.appendTextFast(downcast<InlineTextItem>(run.inlineItem), run.style, run.logicalWidth);

        auto& trailingInlineItem = committedRuns.last().inlineItem;
        if (m_line.hasContentOrListMarker() && downcast<InlineTextItem>(trailingInlineItem).isWhitespace())
            m_wrapOpportunityList.append(&downcast<InlineTextItem>(trailingInlineItem));
        return { lineBreakingResult.isEndOfLine, committedRuns.size() };
    }

    ASSERT(lineBreakingResult.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes);

    if (lineBreakingResult.action == InlineContentBreaker::Result::Action::Wrap)
        return { InlineContentBreaker::IsEndOfLine::Yes };

    if (lineBreakingResult.action == InlineContentBreaker::Result::Action::WrapWithHyphen) {
        ASSERT(m_line.trailingSoftHyphenWidth());
        m_line.addTrailingHyphen(*m_line.trailingSoftHyphenWidth());
        return { InlineContentBreaker::IsEndOfLine::Yes };
    }

    if (lineBreakingResult.action == InlineContentBreaker::Result::Action::Break) {
        auto processPartialContent = [&]() -> TextOnlyLineBreakResult {
            if (!lineBreakingResult.partialTrailingContent || lineBreakingResult.partialTrailingContent->trailingRunIndex || !lineBreakingResult.partialTrailingContent->partialRun) {
                ASSERT_NOT_REACHED();
                return { InlineContentBreaker::IsEndOfLine::Yes };
            }
            auto& trailingInlineTextItem = downcast<InlineTextItem>(candidateContent.runs()[0].inlineItem);
            auto partialRun = *lineBreakingResult.partialTrailingContent->partialRun;
            m_line.appendTextFast(trailingInlineTextItem.left(partialRun.length), trailingInlineTextItem.style(), partialRun.logicalWidth);
            if (auto hyphenWidth = partialRun.hyphenWidth)
                m_line.addTrailingHyphen(*hyphenWidth);
            auto overflowingContentLength = trailingInlineTextItem.length() - partialRun.length;
            return { InlineContentBreaker::IsEndOfLine::Yes, candidateContent.runs().size(), overflowingContentLength };
        };
        return processPartialContent();
    }

    if (lineBreakingResult.action == InlineContentBreaker::Result::Action::RevertToLastWrapOpportunity) {
        ASSERT(!m_wrapOpportunityList.isEmpty());
        return { InlineContentBreaker::IsEndOfLine::Yes, rebuildLine(layoutRange, *m_wrapOpportunityList.last()), { }, true };
    }

    ASSERT_NOT_REACHED();
    return { InlineContentBreaker::IsEndOfLine::Yes };
}

void TextOnlyLineBuilder::handleLineEnding(InlineItemPosition placedContentEnd, size_t layoutRangeEndIndex)
{
    auto horizontalAvailableSpace = m_lineLogicalRect.width();

    m_line.handleTrailingTrimmableContent(Line::TrailingContentAction::Remove);
    if (formattingContext().formattingQuirks().trailingNonBreakingSpaceNeedsAdjustment(isInIntrinsicWidthMode(), horizontalAvailableSpace < m_line.contentLogicalWidth()))
        m_line.handleOverflowingNonBreakingSpace(Line::TrailingContentAction::Remove, m_line.contentLogicalWidth() - horizontalAvailableSpace);

    auto isLastLine = isLastLineWithInlineContent(placedContentEnd, layoutRangeEndIndex);
    m_line.handleTrailingHangingContent(intrinsicWidthMode(), horizontalAvailableSpace, isLastLine);
}

size_t TextOnlyLineBuilder::rebuildLine(const InlineItemRange& layoutRange, const InlineTextItem& trailingInlineItem)
{
    auto isFirstFormattedLine = this->isFirstFormattedLine();
    m_line.initialize({ }, isFirstFormattedLine);
    size_t numberOfInlineItemsOnLine = 0;

    auto appendTextInlineItem = [&] (auto& inlineTextItem) {
        auto& style = isFirstFormattedLine ? inlineTextItem.firstLineStyle() : inlineTextItem.style();
        m_line.appendTextFast(inlineTextItem, style, inlineTextItemWidth(inlineTextItem, style, m_line.contentLogicalRight()));
        ++numberOfInlineItemsOnLine;
        return &inlineTextItem == &trailingInlineItem;
    };

    if (m_partialLeadingTextItem && appendTextInlineItem(*m_partialLeadingTextItem))
        return numberOfInlineItemsOnLine;
    for (size_t index = layoutRange.startIndex() + numberOfInlineItemsOnLine; index < layoutRange.endIndex(); ++index) {
        if (appendTextInlineItem(downcast<InlineTextItem>(m_inlineItems[index])))
            return numberOfInlineItemsOnLine;
    }
    ASSERT_NOT_REACHED();
    return { };
}

const ElementBox& TextOnlyLineBuilder::root() const
{
    return formattingContext().root();
}

bool TextOnlyLineBuilder::isEligibleForSimplifiedTextOnlyInlineLayout(const ElementBox& rootBox, const InlineFormattingState& inlineFormattingState, const FloatingState* floatingState)
{
    if (floatingState && !floatingState->isEmpty())
        return false;
    if (!inlineFormattingState.isNonBidiTextAndForcedLineBreakOnlyContent())
        return false;

    auto& rootStyle = rootBox.style();
    if (rootStyle.fontCascade().wordSpacing())
        return false;
    if (!rootStyle.isLeftToRightDirection())
        return false;
    if (rootStyle.wordBreak() == WordBreak::BreakWord || rootStyle.wordBreak() == WordBreak::Auto || rootStyle.overflowWrap() == OverflowWrap::BreakWord || rootStyle.overflowWrap() == OverflowWrap::Anywhere)
        return false;
    if (rootStyle.textIndent() != RenderStyle::initialTextIndent())
        return false;
    if (rootStyle.textAlignLast() == TextAlignLast::Justify || rootStyle.textAlign() == TextAlignMode::Justify)
        return false;
    if (rootStyle.lineBreak() == LineBreak::AfterWhiteSpace)
        return false;
    if (rootStyle.boxDecorationBreak() == BoxDecorationBreak::Clone)
        return false;
    if (!rootStyle.hangingPunctuation().isEmpty())
        return false;
    if (rootStyle.hyphenationLimitLines() != RenderStyle::initialHyphenationLimitLines())
        return false;
    if (rootStyle.whiteSpace() == WhiteSpace::BreakSpaces)
        return false;

    return true;
}

}
}

