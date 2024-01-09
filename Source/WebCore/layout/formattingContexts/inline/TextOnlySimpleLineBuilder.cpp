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
#include "TextOnlySimpleLineBuilder.h"

#include "InlineContentCache.h"
#include "InlineFormattingContext.h"
#include "RenderStyleInlines.h"

namespace WebCore {
namespace Layout {

struct TextOnlyLineBreakResult {
    InlineContentBreaker::IsEndOfLine isEndOfLine { InlineContentBreaker::IsEndOfLine::Yes };
    size_t committedCount { 0 };
    size_t overflowingContentLength { 0 };
    std::optional<InlineLayoutUnit> overflowLogicalWidth { };
    bool isRevert { false };
};

struct CandidateTextContent {
    void append(InlineLayoutUnit contentWidth)
    {
        logicalWidth += contentWidth;
        ++endIndex;
    }

    size_t startIndex { 0 };
    size_t endIndex { 0 };
    InlineLayoutUnit logicalWidth { 0.f };
};

static inline InlineLayoutUnit measuredInlineTextItem(const InlineTextItem& inlineTextItem, const RenderStyle& style, InlineLayoutUnit contentLogicalLeft)
{
    ASSERT(!inlineTextItem.width());
    if (!inlineTextItem.isWhitespace() || InlineTextItem::shouldPreserveSpacesAndTabs(inlineTextItem))
        return TextUtil::width(inlineTextItem, style.fontCascade(), contentLogicalLeft);
    return TextUtil::width(inlineTextItem, style.fontCascade(), inlineTextItem.start(), inlineTextItem.start() + 1, contentLogicalLeft);
}

static inline InlineItemPosition placedInlineItemEnd(size_t layoutRangeStartIndex, size_t placedInlineItemCount, size_t overflowingContentLength, const InlineItemList& inlineItemList)
{
    if (!overflowingContentLength)
        return { layoutRangeStartIndex + placedInlineItemCount };

    auto trailingInlineItemIndex = layoutRangeStartIndex + placedInlineItemCount - 1;
    auto overflowingInlineTextItemLength = downcast<InlineTextItem>(inlineItemList[trailingInlineItemIndex]).length();
    return { trailingInlineItemIndex, overflowingInlineTextItemLength - overflowingContentLength };
}

static inline bool isLastLineWithInlineContent(InlineItemPosition placedContentEnd, size_t layoutRangeEndIndex)
{
    return placedContentEnd.index == layoutRangeEndIndex && !placedContentEnd.offset;
}

static inline bool consumeTrailingLineBreakIfApplicable(const TextOnlyLineBreakResult& result, size_t trailingInlineItemIndex, size_t layoutRangeEnd, Line& line, const InlineItemList& inlineItemList)
{
    // Trailing forced line break should be consumed after fully placed content.
    auto shouldConsumeTrailingLineBreak = [&] {
        if (result.overflowingContentLength || result.isRevert)
            return false;
        return trailingInlineItemIndex < layoutRangeEnd && inlineItemList[trailingInlineItemIndex].isLineBreak();
    };
    if (!shouldConsumeTrailingLineBreak())
        return false;
    auto& trailingLineBreak = inlineItemList[trailingInlineItemIndex];
    ASSERT(trailingLineBreak.isLineBreak());
    line.append(trailingLineBreak, trailingLineBreak.style(), { });
    return true;
}

TextOnlySimpleLineBuilder::TextOnlySimpleLineBuilder(InlineFormattingContext& inlineFormattingContext, HorizontalConstraints rootHorizontalConstraints, const InlineItemList& inlineItemList)
    : AbstractLineBuilder(inlineFormattingContext, rootHorizontalConstraints, inlineItemList)
    , m_isWrappingAllowed(TextUtil::isWrappingAllowed(root().style()))
{
}

LineLayoutResult TextOnlySimpleLineBuilder::layoutInlineContent(const LineInput& lineInput, const std::optional<PreviousLine>& previousLine)
{
    initialize(lineInput.needsLayoutRange, lineInput.initialLogicalRect, previousLine);
    auto placedContentEnd = isWrappingAllowed() ? placeInlineTextContent(lineInput.needsLayoutRange) : placeNonWrappingInlineTextContent(lineInput.needsLayoutRange);
    auto result = m_line.close();

    auto isLastInlineContent = isLastLineWithInlineContent(placedContentEnd, lineInput.needsLayoutRange.endIndex());
    auto& rootStyle = isFirstFormattedLine() ? root().firstLineStyle() : root().style();
    auto contentLogicalLeft = InlineFormattingUtils::horizontalAlignmentOffset(rootStyle, result.contentLogicalRight, m_lineLogicalRect.width(), result.hangingTrailingContentWidth, result.runs, isLastInlineContent);

    return { { lineInput.needsLayoutRange.start, placedContentEnd }
        , WTFMove(result.runs)
        , { }
        , { contentLogicalLeft, result.contentLogicalWidth, contentLogicalLeft + result.contentLogicalRight, m_overflowContentLogicalWidth }
        , { m_lineLogicalRect.topLeft(), m_lineLogicalRect.width(), m_lineLogicalRect.left()  }
        , { !result.isHangingTrailingContentWhitespace, result.hangingTrailingContentWidth }
        , { }
        , { isFirstFormattedLine() ? LineLayoutResult::IsFirstLast::FirstFormattedLine::WithinIFC : LineLayoutResult::IsFirstLast::FirstFormattedLine::No, isLastInlineContent }
        , { }
        , { }
        , { }
        , m_trimmedTrailingWhitespaceWidth
    };
}

void TextOnlySimpleLineBuilder::initialize(const InlineItemRange& layoutRange, const InlineRect& initialLogicalRect, const std::optional<PreviousLine>& previousLine)
{
    reset();

    ASSERT(!layoutRange.isEmpty() || (previousLine && !previousLine->suspendedFloats.isEmpty()));
    auto partialLeadingTextItem = [&]() -> std::optional<InlineTextItem> {
        if (!previousLine || !layoutRange.start.offset)
            return { };

        auto& overflowingInlineTextItem = downcast<InlineTextItem>(m_inlineItemList[layoutRange.start.index]);
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
    m_trimmedTrailingWhitespaceWidth = { };
    m_overflowContentLogicalWidth = { };
}

InlineItemPosition TextOnlySimpleLineBuilder::placeInlineTextContent(const InlineItemRange& layoutRange)
{
    auto& rootStyle = !isFirstFormattedLine() ? root().style() : root().firstLineStyle();
    auto hasWrapOpportunityBeforeWhitespace = rootStyle.whiteSpaceCollapse() != WhiteSpaceCollapse::BreakSpaces && rootStyle.lineBreak() != LineBreak::AfterWhiteSpace;
    size_t placedInlineItemCount = 0;

    auto result = TextOnlyLineBreakResult { };
    auto candidateContent = CandidateTextContent { layoutRange.startIndex(), layoutRange.startIndex(), { } };

    auto nextItemIndex = layoutRange.startIndex();
    auto isAtSoftWrapOpportunityOrContentEnd = [&](auto& inlineTextItem) {
        if (inlineTextItem.isWhitespace())
            return true;
        if (nextItemIndex >= layoutRange.endIndex() || m_inlineItemList[nextItemIndex].isLineBreak())
            return true;
        auto& nextInlineTextItem = downcast<InlineTextItem>(m_inlineItemList[nextItemIndex]);
        if (nextInlineTextItem.isWhitespace())
            return hasWrapOpportunityBeforeWhitespace;
        return &inlineTextItem.inlineTextBox() == &nextInlineTextItem.inlineTextBox() || TextUtil::mayBreakInBetween(inlineTextItem, nextInlineTextItem);
    };

    auto processCandidateContent = [&] {
        result = commitCandidateContent(candidateContent, layoutRange);
        placedInlineItemCount = !result.isRevert ? placedInlineItemCount + result.committedCount : result.committedCount;
        candidateContent = { candidateContent.endIndex, candidateContent.endIndex, { } };
        return result.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes;
    };

    // Handle leading partial content first (overflowing text from the previous line).
    auto isEndOfLine = false;
    if (m_partialLeadingTextItem) {
        candidateContent.append({ });
        ++nextItemIndex;
        if (isAtSoftWrapOpportunityOrContentEnd(*m_partialLeadingTextItem))
            isEndOfLine = processCandidateContent();
    }

    while (!isEndOfLine && nextItemIndex < layoutRange.endIndex()) {
        auto& inlineItem = m_inlineItemList[nextItemIndex++];
        ASSERT(inlineItem.isText() || inlineItem.isLineBreak());

        if (inlineItem.isText()) {
            auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
            auto contentWidth = [&] {
                if (auto logicalWidth = inlineTextItem.width())
                    return *logicalWidth;
                return measuredInlineTextItem(inlineTextItem, rootStyle, m_line.contentLogicalRight() + candidateContent.logicalWidth);
            };
            candidateContent.append(contentWidth());
            if (isAtSoftWrapOpportunityOrContentEnd(inlineTextItem))
                isEndOfLine = processCandidateContent();
            continue;
        }
        if (inlineItem.isLineBreak()) {
            isEndOfLine = true;
            result = { };
            continue;
        }
    }
    if (consumeTrailingLineBreakIfApplicable(result, layoutRange.startIndex() + placedInlineItemCount, layoutRange.endIndex(), m_line, m_inlineItemList))
        ++placedInlineItemCount;
    ASSERT(placedInlineItemCount);
    auto placedContentEnd = placedInlineItemEnd(layoutRange.startIndex(), placedInlineItemCount, result.overflowingContentLength, m_inlineItemList);
    handleLineEnding(placedContentEnd, layoutRange.endIndex());
    m_overflowContentLogicalWidth = result.overflowLogicalWidth;
    return placedContentEnd;
}

InlineItemPosition TextOnlySimpleLineBuilder::placeNonWrappingInlineTextContent(const InlineItemRange& layoutRange)
{
    ASSERT(!TextUtil::isWrappingAllowed(root().style()));
    ASSERT(!m_partialLeadingTextItem);

    auto candidateContent = CandidateTextContent { layoutRange.startIndex(), layoutRange.startIndex(), { } };
    auto& rootStyle = !isFirstFormattedLine() ? root().style() : root().firstLineStyle();
    auto isEndOfLine = false;
    auto trailingLineBreakIndex = std::optional<size_t> { };
    auto nextItemIndex = layoutRange.startIndex();

    while (!isEndOfLine) {
        auto& inlineItem = m_inlineItemList[nextItemIndex];
        if (inlineItem.isText()) {
            auto contentWidth = [&] {
                if (auto logicalWidth = downcast<InlineTextItem>(inlineItem).width())
                    return *logicalWidth;
                return measuredInlineTextItem(downcast<InlineTextItem>(inlineItem), rootStyle, candidateContent.logicalWidth);
            };
            candidateContent.append(contentWidth());
        } else if (inlineItem.isLineBreak())
            trailingLineBreakIndex = nextItemIndex;
        else {
            ASSERT_NOT_REACHED();
            return layoutRange.end;
        }
        ++nextItemIndex;
        isEndOfLine = nextItemIndex >= layoutRange.endIndex() || trailingLineBreakIndex.has_value();
    }

    if (trailingLineBreakIndex && candidateContent.startIndex == candidateContent.endIndex) {
        m_line.append(m_inlineItemList[*trailingLineBreakIndex], m_inlineItemList[*trailingLineBreakIndex].style(), { });
        return { *trailingLineBreakIndex + 1, { } };
    }

    auto result = commitCandidateContent(candidateContent, layoutRange);
    nextItemIndex = layoutRange.startIndex() + result.committedCount;
    if (consumeTrailingLineBreakIfApplicable(result, nextItemIndex, layoutRange.endIndex(), m_line, m_inlineItemList))
        ++nextItemIndex;

    auto placedInlineItemCount = nextItemIndex - layoutRange.startIndex();
    auto placedContentEnd = placedInlineItemEnd(layoutRange.startIndex(), placedInlineItemCount, result.overflowingContentLength, m_inlineItemList);
    handleLineEnding(placedContentEnd, layoutRange.endIndex());
    return placedContentEnd;
}

TextOnlyLineBreakResult TextOnlySimpleLineBuilder::commitCandidateContent(const CandidateTextContent& candidateContent, const InlineItemRange& layoutRange)
{
    auto& rootStyle = !isFirstFormattedLine() ? root().style() : root().firstLineStyle();
    auto hasLeadingPartiaContent = m_partialLeadingTextItem && candidateContent.startIndex == layoutRange.startIndex();
    auto contentWidth = [&] (auto& inlineTextItem, InlineLayoutUnit contentOffset) {
        if (auto logicalWidth = inlineTextItem.width())
            return *logicalWidth;
        return measuredInlineTextItem(inlineTextItem, rootStyle, m_line.contentLogicalRight() + contentOffset);
    };

    if (candidateContent.logicalWidth <= availableWidth() && !hasLeadingPartiaContent) {
        for (auto index = candidateContent.startIndex; index < candidateContent.endIndex; ++index) {
            auto& inlineTextItem = downcast<InlineTextItem>(m_inlineItemList[index]);
            m_line.appendTextFast(inlineTextItem, rootStyle, contentWidth(inlineTextItem, { }));
        }

        if (m_line.hasContentOrListMarker())
            m_wrapOpportunityList.append(&m_inlineItemList[candidateContent.endIndex - 1]);
        return { InlineContentBreaker::IsEndOfLine::No, candidateContent.endIndex - candidateContent.startIndex };
    }

    auto candidateContentForLineBreaking = InlineContentBreaker::ContinuousContent { };
    auto startIndex = candidateContent.startIndex;

    if (hasLeadingPartiaContent) {
        candidateContentForLineBreaking.appendTextContent(*m_partialLeadingTextItem, rootStyle, contentWidth(*m_partialLeadingTextItem, { }));
        ++startIndex;
    }
    for (auto index = startIndex; index < candidateContent.endIndex; ++index) {
        auto& inlineTextItem = downcast<InlineTextItem>(m_inlineItemList[index]);
        candidateContentForLineBreaking.appendTextContent(inlineTextItem, rootStyle, contentWidth(inlineTextItem, candidateContentForLineBreaking.logicalWidth()));
    }
    return handleOverflowingTextContent(candidateContentForLineBreaking, layoutRange);
}

TextOnlyLineBreakResult TextOnlySimpleLineBuilder::handleOverflowingTextContent(const InlineContentBreaker::ContinuousContent& candidateContent, const InlineItemRange& layoutRange)
{
    ASSERT(!candidateContent.runs().isEmpty());

    auto availableWidth = this->availableWidth();
    auto lineBreakingResult = InlineContentBreaker::Result { InlineContentBreaker::Result::Action::Keep, InlineContentBreaker::IsEndOfLine::No, { }, { } };
    if (candidateContent.logicalWidth() > availableWidth) {
        auto lineStatus = InlineContentBreaker::LineStatus { m_line.contentLogicalRight(), availableWidth, m_line.trimmableTrailingWidth(), m_line.trailingSoftHyphenWidth(), m_line.isTrailingRunFullyTrimmable(), m_line.hasContentOrListMarker(), !m_wrapOpportunityList.isEmpty() };
        lineBreakingResult = inlineContentBreaker().processInlineContent(candidateContent, lineStatus);
    }

    if (lineBreakingResult.action == InlineContentBreaker::Result::Action::Keep) {
        auto& committedRuns = candidateContent.runs();
        for (auto& run : committedRuns)
            m_line.appendTextFast(downcast<InlineTextItem>(run.inlineItem), run.style, run.logicalWidth);
        if (m_line.hasContentOrListMarker())
            m_wrapOpportunityList.append(&committedRuns.last().inlineItem);
        return { lineBreakingResult.isEndOfLine, committedRuns.size() };
    }

    ASSERT(lineBreakingResult.isEndOfLine == InlineContentBreaker::IsEndOfLine::Yes);

    if (lineBreakingResult.action == InlineContentBreaker::Result::Action::Wrap)
        return { InlineContentBreaker::IsEndOfLine::Yes, { }, { }, eligibleOverflowWidthAsLeading(candidateContent.runs(), lineBreakingResult, isFirstFormattedLine()) };

    if (lineBreakingResult.action == InlineContentBreaker::Result::Action::WrapWithHyphen) {
        ASSERT(m_line.trailingSoftHyphenWidth());
        m_line.addTrailingHyphen(*m_line.trailingSoftHyphenWidth());
        return { InlineContentBreaker::IsEndOfLine::Yes };
    }

    if (lineBreakingResult.action == InlineContentBreaker::Result::Action::Break) {
        auto processPartialContent = [&]() -> TextOnlyLineBreakResult {
            if (!lineBreakingResult.partialTrailingContent) {
                ASSERT_NOT_REACHED();
                return { InlineContentBreaker::IsEndOfLine::Yes };
            }
            auto trailingRunIndex = lineBreakingResult.partialTrailingContent->trailingRunIndex;
            auto& runs = candidateContent.runs();
            for (size_t index = 0; index < trailingRunIndex; ++index) {
                auto& run = runs[index];
                m_line.appendTextFast(downcast<InlineTextItem>(run.inlineItem), run.style, run.logicalWidth);
            }

            auto committedInlineItemCount = trailingRunIndex + 1;
            auto& trailingRun = runs[trailingRunIndex];
            if (!lineBreakingResult.partialTrailingContent->partialRun) {
                m_line.appendTextFast(downcast<InlineTextItem>(trailingRun.inlineItem), trailingRun.style, trailingRun.logicalWidth);
                if (auto hyphenWidth = lineBreakingResult.partialTrailingContent->hyphenWidth)
                    m_line.addTrailingHyphen(*hyphenWidth);
                return { InlineContentBreaker::IsEndOfLine::Yes, committedInlineItemCount };
            }

            auto partialRun = *lineBreakingResult.partialTrailingContent->partialRun;
            auto& trailingInlineTextItem = downcast<InlineTextItem>(runs[trailingRunIndex].inlineItem);
            m_line.appendTextFast(trailingInlineTextItem.left(partialRun.length), trailingRun.style, partialRun.logicalWidth);
            if (auto hyphenWidth = partialRun.hyphenWidth)
                m_line.addTrailingHyphen(*hyphenWidth);
            auto overflowingContentLength = trailingInlineTextItem.length() - partialRun.length;
            return { InlineContentBreaker::IsEndOfLine::Yes, committedInlineItemCount, overflowingContentLength, eligibleOverflowWidthAsLeading(candidateContent.runs(), lineBreakingResult, isFirstFormattedLine()) };
        };
        return processPartialContent();
    }

    // Revert to a previous position cases.
    if (m_wrapOpportunityList.isEmpty()) {
        ASSERT_NOT_REACHED();
        return { InlineContentBreaker::IsEndOfLine::Yes };
    }

    if (lineBreakingResult.action == InlineContentBreaker::Result::Action::RevertToLastWrapOpportunity)
        return { InlineContentBreaker::IsEndOfLine::Yes, revertToTrailingItem(layoutRange, downcast<InlineTextItem>(*m_wrapOpportunityList.last())), { }, { }, true };

    if (lineBreakingResult.action == InlineContentBreaker::Result::Action::RevertToLastNonOverflowingWrapOpportunity)
        return { InlineContentBreaker::IsEndOfLine::Yes, revertToLastNonOverflowingItem(layoutRange), { }, { }, true };

    ASSERT_NOT_REACHED();
    return { InlineContentBreaker::IsEndOfLine::Yes };
}

void TextOnlySimpleLineBuilder::handleLineEnding(InlineItemPosition placedContentEnd, size_t layoutRangeEndIndex)
{
    auto horizontalAvailableSpace = m_lineLogicalRect.width();
    auto isLastInlineContent = isLastLineWithInlineContent(placedContentEnd, layoutRangeEndIndex);
    auto shouldPreserveTrailingWhitespace = [&] {
        return root().style().lineBreak() == LineBreak::AfterWhiteSpace && intrinsicWidthMode() != IntrinsicWidthMode::Minimum && (!isLastInlineContent || horizontalAvailableSpace < m_line.contentLogicalWidth());
    };
    m_trimmedTrailingWhitespaceWidth = m_line.handleTrailingTrimmableContent(shouldPreserveTrailingWhitespace() ? Line::TrailingContentAction::Preserve : Line::TrailingContentAction::Remove);
    if (formattingContext().quirks().trailingNonBreakingSpaceNeedsAdjustment(isInIntrinsicWidthMode(), horizontalAvailableSpace < m_line.contentLogicalWidth()))
        m_line.handleOverflowingNonBreakingSpace(shouldPreserveTrailingWhitespace() ? Line::TrailingContentAction::Preserve : Line::TrailingContentAction::Remove, m_line.contentLogicalWidth() - horizontalAvailableSpace);

    m_line.handleTrailingHangingContent(intrinsicWidthMode(), horizontalAvailableSpace, isLastInlineContent);
}

size_t TextOnlySimpleLineBuilder::revertToTrailingItem(const InlineItemRange& layoutRange, const InlineTextItem& trailingInlineItem)
{
    auto isFirstFormattedLine = this->isFirstFormattedLine();
    auto& rootStyle = !isFirstFormattedLine ? root().style() : root().firstLineStyle();
    m_line.initialize({ }, isFirstFormattedLine);
    size_t numberOfInlineItemsOnLine = 0;

    auto appendTextInlineItem = [&] (auto& inlineTextItem) {
        auto logicalWidth = inlineTextItem.width() ? *inlineTextItem.width() : measuredInlineTextItem(inlineTextItem, rootStyle, m_line.contentLogicalRight());
        m_line.appendTextFast(inlineTextItem, rootStyle, logicalWidth);
        ++numberOfInlineItemsOnLine;
        return &inlineTextItem == &trailingInlineItem;
    };

    if (m_partialLeadingTextItem && appendTextInlineItem(*m_partialLeadingTextItem))
        return numberOfInlineItemsOnLine;
    for (size_t index = layoutRange.startIndex() + numberOfInlineItemsOnLine; index < layoutRange.endIndex(); ++index) {
        if (appendTextInlineItem(downcast<InlineTextItem>(m_inlineItemList[index])))
            return numberOfInlineItemsOnLine;
    }
    ASSERT_NOT_REACHED();
    return { };
}

size_t TextOnlySimpleLineBuilder::revertToLastNonOverflowingItem(const InlineItemRange& layoutRange)
{
    // Revert all the way back to a wrap opportunity when either a soft hyphen fits or no hyphen is required.
    for (auto i = m_wrapOpportunityList.size(); i--;) {
        auto committedCount = revertToTrailingItem(layoutRange, downcast<InlineTextItem>(*m_wrapOpportunityList[i]));
        auto trailingSoftHyphenWidth = m_line.trailingSoftHyphenWidth();

        auto hasRevertedEnough = [&] {
            if (!i || !trailingSoftHyphenWidth)
                return true;
            return *trailingSoftHyphenWidth <= availableWidth();
        };
        if (hasRevertedEnough()) {
            if (trailingSoftHyphenWidth)
                m_line.addTrailingHyphen(*trailingSoftHyphenWidth);
            return committedCount;
        }
    }
    ASSERT_NOT_REACHED();
    return 0;
}

InlineLayoutUnit TextOnlySimpleLineBuilder::availableWidth() const
{
    auto contentLogicalRight = m_line.contentLogicalRight();
    return (m_lineLogicalRect.width() + LayoutUnit::epsilon()) - (!std::isnan(contentLogicalRight) ? contentLogicalRight : 0.f);
}

bool TextOnlySimpleLineBuilder::isEligibleForSimplifiedTextOnlyInlineLayoutByContent(const InlineContentCache::InlineItems& inlineItems, const PlacedFloats& placedFloats)
{
    if (inlineItems.isEmpty())
        return false;
    if (!inlineItems.hasTextAndLineBreakOnlyContent() || inlineItems.hasInlineBoxes() || inlineItems.requiresVisualReordering())
        return false;
    if (!placedFloats.isEmpty())
        return false;

    return true;
}

bool TextOnlySimpleLineBuilder::isEligibleForSimplifiedInlineLayoutByStyle(const ElementBox& rootBox)
{
    auto& rootStyle = rootBox.style();
    if (rootStyle.fontCascade().wordSpacing())
        return false;
    if (!rootStyle.isLeftToRightDirection())
        return false;
    if (rootStyle.wordBreak() == WordBreak::AutoPhrase)
        return false;
    if (rootStyle.textIndent() != RenderStyle::initialTextIndent())
        return false;
    if (rootStyle.textAlignLast() == TextAlignLast::Justify || rootStyle.textAlign() == TextAlignMode::Justify || rootBox.isRubyAnnotationBox())
        return false;
    if (rootStyle.boxDecorationBreak() == BoxDecorationBreak::Clone)
        return false;
    if (!rootStyle.hangingPunctuation().isEmpty())
        return false;
    if (rootStyle.hyphenationLimitLines() != RenderStyle::initialHyphenationLimitLines())
        return false;
    if (rootStyle.textWrapMode() == TextWrapMode::Wrap && rootStyle.textWrapStyle() == TextWrapStyle::Balance)
        return false;
    if (rootStyle.lineAlign() != LineAlign::None || rootStyle.lineSnap() != LineSnap::None)
        return false;

    return true;
}

}
}

