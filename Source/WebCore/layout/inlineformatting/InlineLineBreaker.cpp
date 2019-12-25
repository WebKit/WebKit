/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "InlineLineBreaker.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FontCascade.h"
#include "Hyphenation.h"
#include "InlineItem.h"
#include "InlineTextItem.h"
#include "TextUtil.h"

namespace WebCore {
namespace Layout {

static inline bool isTextContentWrappingAllowed(const RenderStyle& style)
{
    // Do not try to push overflown 'pre' and 'no-wrap' content to next line.
    return style.whiteSpace() != WhiteSpace::Pre && style.whiteSpace() != WhiteSpace::NoWrap;
}

static inline bool isTextContentWrappingAllowed(const LineBreaker::ContinousContent& candidateRuns)
{
    // Use the last inline item with content (where we would be wrapping) to decide if content wrapping is allowed.
    auto runIndex = candidateRuns.lastContentRunIndex().valueOr(candidateRuns.size() - 1);
    return isTextContentWrappingAllowed(candidateRuns.runs()[runIndex].inlineItem.style());
}

static inline bool isContentSplitAllowed(const LineBreaker::Run& run)
{
    ASSERT(run.inlineItem.isText() || run.inlineItem.isContainerStart() || run.inlineItem.isContainerEnd());
    if (!run.inlineItem.isText()) {
        // Can't split horizontal spacing -> e.g. <span style="padding-right: 100px;">textcontent</span>, if the [container end] is the overflown inline item
        // we need to check if there's another inline item beyond the [container end] to split.
        return false;
    }
    return isTextContentWrappingAllowed(run.inlineItem.style());
}

static inline bool shouldKeepEndOfLineWhitespace(const LineBreaker::ContinousContent& candidateRuns)
{
    // Grab the style and check for white-space property to decided whether we should let this whitespace content overflow the current line.
    // Note that the "keep" in the context means we let the whitespace content sit on the current line.
    // It might very well get collapsed when we close the line (normal/nowrap/pre-line).
    // See https://www.w3.org/TR/css-text-3/#white-space-property
    auto whitespace = candidateRuns.runs()[*candidateRuns.firstTextRunIndex()].inlineItem.style().whiteSpace();
    return whitespace == WhiteSpace::Normal || whitespace == WhiteSpace::NoWrap || whitespace == WhiteSpace::PreWrap || whitespace == WhiteSpace::PreLine;
}

LineBreaker::BreakingContext LineBreaker::breakingContextForInlineContent(const ContinousContent& candidateRuns, const LineStatus& lineStatus)
{
    ASSERT(!candidateRuns.isEmpty());
    if (candidateRuns.width() <= lineStatus.availableWidth)
        return { BreakingContext::ContentWrappingRule::Keep, IsEndOfLine::No, { } };
    if (candidateRuns.hasTrailingCollapsibleContent()) {
        ASSERT(candidateRuns.hasTextContentOnly());
        auto IsEndOfLine = isTextContentWrappingAllowed(candidateRuns) ? IsEndOfLine::Yes : IsEndOfLine::No;
        // First check if the content fits without the trailing collapsible part.
        if (candidateRuns.nonCollapsibleWidth() <= lineStatus.availableWidth)
            return { BreakingContext::ContentWrappingRule::Keep, IsEndOfLine, { } };
        // Now check if we can trim the line too.
        if (lineStatus.lineHasFullyCollapsibleTrailingRun && candidateRuns.isTrailingContentFullyCollapsible()) {
            // If this new content is fully collapsible, it shoud surely fit.
            return { BreakingContext::ContentWrappingRule::Keep, IsEndOfLine, { } };
        }
    } else if (lineStatus.collapsibleWidth && candidateRuns.hasNonContentRunsOnly()) {
        // Let's see if the non-content runs fit when the line has trailing collapsible content.
        // "text content <span style="padding: 1px"></span>" <- the <span></span> runs could fit after collapsing the trailing whitespace.
        if (candidateRuns.width() <= lineStatus.availableWidth + lineStatus.collapsibleWidth)
            return { BreakingContext::ContentWrappingRule::Keep, IsEndOfLine::No, { } };
    }
    if (candidateRuns.isVisuallyEmptyWhitespaceContentOnly() && shouldKeepEndOfLineWhitespace(candidateRuns)) {
        // This overflowing content apparently falls into the remove/hang end-of-line-spaces catergory.
        // see https://www.w3.org/TR/css-text-3/#white-space-property matrix
        return { BreakingContext::ContentWrappingRule::Keep, IsEndOfLine::No, { } };
    }

    if (candidateRuns.hasTextContentOnly()) {
        auto& runs = candidateRuns.runs();
        if (auto wrappedTextContent = wrapTextContent(runs, lineStatus)) {
            if (!wrappedTextContent->trailingRunIndex && wrappedTextContent->contentOverflows) {
                // We tried to split the content but the available space can't even accommodate the first character.
                // 1. Push the content over to the next line when we've got content on the line already.
                // 2. Keep the first character on the empty line (or keep the whole run if it has only one character).
                if (!lineStatus.lineIsEmpty)
                    return { BreakingContext::ContentWrappingRule::Push, IsEndOfLine::Yes, { } };
                auto firstTextRunIndex = *candidateRuns.firstTextRunIndex();
                auto& inlineTextItem = downcast<InlineTextItem>(runs[firstTextRunIndex].inlineItem);
                ASSERT(inlineTextItem.length());
                if (inlineTextItem.length() == 1)
                    return { BreakingContext::ContentWrappingRule::Keep, IsEndOfLine::Yes, { } };
                auto firstCharacterWidth = TextUtil::width(inlineTextItem, inlineTextItem.start(), inlineTextItem.start() + 1);
                auto firstCharacterRun = PartialRun { 1, firstCharacterWidth, false };
                return { BreakingContext::ContentWrappingRule::Split, IsEndOfLine::Yes, BreakingContext::PartialTrailingContent { firstTextRunIndex, firstCharacterRun } };
            }
            auto splitContent = BreakingContext::PartialTrailingContent { wrappedTextContent->trailingRunIndex, wrappedTextContent->partialTrailingRun };
            return { BreakingContext::ContentWrappingRule::Split, IsEndOfLine::Yes, splitContent };
        }
    }
    // If we are not allowed to break this content, we still need to decide whether keep it or push it to the next line.
    auto isWrappingAllowed = isTextContentWrappingAllowed(candidateRuns);
    auto contentOverflows = lineStatus.lineIsEmpty || !isWrappingAllowed;
    if (!contentOverflows)
        return { BreakingContext::ContentWrappingRule::Push, IsEndOfLine::Yes, { } };
    return { BreakingContext::ContentWrappingRule::Keep, isWrappingAllowed ? IsEndOfLine::Yes : IsEndOfLine::No, { } };
}

bool LineBreaker::shouldWrapFloatBox(InlineLayoutUnit floatLogicalWidth, InlineLayoutUnit availableWidth, bool lineIsEmpty)
{
    return !lineIsEmpty && floatLogicalWidth > availableWidth;
}

Optional<LineBreaker::WrappedTextContent> LineBreaker::wrapTextContent(const RunList& runs, const LineStatus& lineStatus) const
{
    // Check where the overflow occurs and use the corresponding style to figure out the breaking behaviour.
    // <span style="word-break: normal">first</span><span style="word-break: break-all">second</span><span style="word-break: normal">third</span>
    InlineLayoutUnit accumulatedRunWidth = 0;
    unsigned index = 0;
    while (index < runs.size()) {
        auto& run = runs[index];
        ASSERT(run.inlineItem.isText() || run.inlineItem.isContainerStart() || run.inlineItem.isContainerEnd());
        if (accumulatedRunWidth + run.logicalWidth > lineStatus.availableWidth && isContentSplitAllowed(run)) {
            // At this point the available width can very well be negative e.g. when some part of the continuous text content can not be broken into parts ->
            // <span style="word-break: keep-all">textcontentwithnobreak</span><span>textcontentwithyesbreak</span>
            // When the first span computes longer than the available space, by the time we get to the second span, the adjusted available space becomes negative.
            auto adjustedAvailableWidth = std::max<InlineLayoutUnit>(0, lineStatus.availableWidth - accumulatedRunWidth);
             if (auto partialRun = tryBreakingTextRun(run, adjustedAvailableWidth, lineStatus.lineIsEmpty)) {
                 if (partialRun->length)
                     return WrappedTextContent { index, false, partialRun };
                 // When the content is wrapped at the run boundary, the trailing run is the previous run.
                 if (index)
                     return WrappedTextContent { index - 1, false, { } };
                 // Sometimes we can't accommodate even the very first character.
                 return WrappedTextContent { 0, true, { } };
             }
            // If this run is not breakable, we need to check if any previous run is breakable
            break;
        }
        accumulatedRunWidth += run.logicalWidth;
        ++index;
    }
    // We did not manage to break the run that actually overflows the line.
    // Let's try to find the first breakable run and wrap it at the content boundary (as it surely fits).
    while (index--) {
        auto& run = runs[index];
        if (isContentSplitAllowed(run)) {
            ASSERT(run.inlineItem.isText());
            if (auto partialRun = tryBreakingTextRun(run, maxInlineLayoutUnit(), lineStatus.lineIsEmpty)) {
                 // We know this run fits, so if wrapping is allowed on the run, it should return a non-empty left-side.
                 ASSERT(partialRun->length);
                 return WrappedTextContent { index, false, partialRun };
            }
        }
    }
    // Give up, there's no breakable run in here.
    return { };
}

LineBreaker::WordBreakRule LineBreaker::wordBreakBehavior(const RenderStyle& style, bool lineIsEmpty) const
{
    // Disregard any prohibition against line breaks mandated by the word-break property.
    // The different wrapping opportunities must not be prioritized. Hyphenation is not applied.
    if (style.lineBreak() == LineBreak::Anywhere)
        return WordBreakRule::AtArbitraryPosition;
    // Breaking is allowed within “words”.
    if (style.wordBreak() == WordBreak::BreakAll)
        return WordBreakRule::AtArbitraryPosition;
    // Breaking is forbidden within “words”.
    if (style.wordBreak() == WordBreak::KeepAll)
        return WordBreakRule::NoBreak;
    // For compatibility with legacy content, the word-break property also supports a deprecated break-word keyword.
    // When specified, this has the same effect as word-break: normal and overflow-wrap: anywhere, regardless of the actual value of the overflow-wrap property.
    if (style.wordBreak() == WordBreak::BreakWord && lineIsEmpty)
        return WordBreakRule::AtArbitraryPosition;
    // OverflowWrap::Break: An otherwise unbreakable sequence of characters may be broken at an arbitrary point if there are no otherwise-acceptable break points in the line.
    if (style.overflowWrap() == OverflowWrap::Break && lineIsEmpty)
        return WordBreakRule::AtArbitraryPosition;

    if (!n_hyphenationIsDisabled && style.hyphens() == Hyphens::Auto && canHyphenate(style.locale()))
        return WordBreakRule::OnlyHyphenationAllowed;

    return WordBreakRule::NoBreak;
}

Optional<LineBreaker::PartialRun> LineBreaker::tryBreakingTextRun(const Run& overflowRun, InlineLayoutUnit availableWidth, bool lineIsEmpty) const
{
    ASSERT(overflowRun.inlineItem.isText());
    auto& inlineTextItem = downcast<InlineTextItem>(overflowRun.inlineItem);
    auto& style = inlineTextItem.style();
    auto findLastBreakablePosition = availableWidth == maxInlineLayoutUnit();

    auto breakRule = wordBreakBehavior(style, lineIsEmpty);
    if (breakRule == WordBreakRule::AtArbitraryPosition) {
        if (findLastBreakablePosition) {
            // When the run can be split at arbitrary position,
            // let's just return the entire run when it is intended to fit on the line.
            return PartialRun { inlineTextItem.length(), overflowRun.logicalWidth, false };
        }
        // FIXME: Pass in the content logical left to be able to measure tabs.
        auto splitData = TextUtil::split(inlineTextItem.layoutBox(), inlineTextItem.start(), inlineTextItem.length(), overflowRun.logicalWidth, availableWidth, { });
        return PartialRun { splitData.length, splitData.logicalWidth, false };
    }

    if (breakRule == WordBreakRule::OnlyHyphenationAllowed) {
        // Find the hyphen position as follows:
        // 1. Split the text by taking the hyphen width into account
        // 2. Find the last hyphen position before the split position
        auto runLength = inlineTextItem.length();
        unsigned limitBefore = style.hyphenationLimitBefore() == RenderStyle::initialHyphenationLimitBefore() ? 0 : style.hyphenationLimitBefore();
        unsigned limitAfter = style.hyphenationLimitAfter() == RenderStyle::initialHyphenationLimitAfter() ? 0 : style.hyphenationLimitAfter();
        // Check if this run can accommodate the before/after limits at all before start measuring text.
        if (limitBefore >= runLength || limitAfter >= runLength || limitBefore + limitAfter > runLength)
            return { };

        unsigned leftSideLength = runLength;
        // FIXME: We might want to cache the hyphen width.
        auto& fontCascade = style.fontCascade();
        auto hyphenWidth = InlineLayoutUnit { fontCascade.width(TextRun { StringView { style.hyphenString() } }) };
        if (!findLastBreakablePosition) {
            auto availableWidthExcludingHyphen = availableWidth - hyphenWidth;
            if (availableWidthExcludingHyphen <= 0 || !enoughWidthForHyphenation(availableWidthExcludingHyphen, fontCascade.pixelSize()))
                return { };
            leftSideLength = TextUtil::split(inlineTextItem.layoutBox(), inlineTextItem.start(), runLength, overflowRun.logicalWidth, availableWidthExcludingHyphen, { }).length;
        }
        if (leftSideLength < limitBefore)
            return { };
        auto textContent = inlineTextItem.layoutBox().textContext()->content;
        // Adjust before index to accommodate the limit-after value (it's the last potential hyphen location in this run).
        auto hyphenBefore = std::min(leftSideLength, runLength - limitAfter) + 1;
        unsigned hyphenLocation = lastHyphenLocation(StringView(textContent).substring(inlineTextItem.start(), inlineTextItem.length()), hyphenBefore, style.locale());
        if (!hyphenLocation || hyphenLocation < limitBefore)
            return { };
        // hyphenLocation is relative to the start of this InlineItemText.
        auto trailingPartialRunWidthWithHyphen = TextUtil::width(inlineTextItem, inlineTextItem.start(), inlineTextItem.start() + hyphenLocation) + hyphenWidth; 
        return PartialRun { hyphenLocation, trailingPartialRunWidthWithHyphen, true };
    }

    ASSERT(breakRule == WordBreakRule::NoBreak);
    return { };
}

static bool endsWithSoftWrapOpportunity(const InlineTextItem& currentTextItem, const InlineTextItem& nextInlineTextItem)
{
    ASSERT(!nextInlineTextItem.isWhitespace());
    // We are at the position after a whitespace.
    if (currentTextItem.isWhitespace())
        return true;
    // When both these non-whitespace runs belong to the same layout box, it's guaranteed that
    // they are split at a soft breaking opportunity. See InlineTextItem::moveToNextBreakablePosition.
    if (&currentTextItem.layoutBox() == &nextInlineTextItem.layoutBox())
        return true;
    // Now we need to collect at least 3 adjacent characters to be able to make a descision whether the previous text item ends with breaking opportunity.
    // [ex-][ample] <- second to last[x] last[-] current[a]
    // We need at least 1 character in the current inline text item and 2 more from previous inline items.
    auto previousContent = currentTextItem.layoutBox().textContext()->content;
    auto lineBreakIterator = LazyLineBreakIterator { nextInlineTextItem.layoutBox().textContext()->content };
    auto previousContentLength = previousContent.length();
    // FIXME: We should look into the entire uncommitted content for more text context.
    UChar lastCharacter = previousContentLength ? previousContent[previousContentLength - 1] : 0;
    UChar secondToLastCharacter = previousContentLength > 1 ? previousContent[previousContentLength - 2] : 0;
    lineBreakIterator.setPriorContext(lastCharacter, secondToLastCharacter);
    // Now check if we can break right at the inline item boundary.
    // With the [ex-ample], findNextBreakablePosition should return the startPosition (0).
    // FIXME: Check if there's a more correct way of finding breaking opportunities.
    return !TextUtil::findNextBreakablePosition(lineBreakIterator, 0, nextInlineTextItem.style());
}

static bool isAtSoftWrapOpportunity(const InlineItem& current, const InlineItem& next)
{
    // "is at" simple means that there's a soft wrap opportunity right after the [current].
    // [text][ ][text][container start]... (<div>text content<span>..</div>)
    // soft wrap indexes: 0 and 1 definitely, 2 depends on the content after the [container start].

    // https://drafts.csswg.org/css-text-3/#line-break-details
    // Figure out if the new incoming content puts the uncommitted content on a soft wrap opportunity.
    // e.g. [container start][prior_continuous_content][container end] (<span>prior_continuous_content</span>)
    // An incoming <img> box would enable us to commit the "<span>prior_continuous_content</span>" content
    // but an incoming text content would not necessarily.
    ASSERT(current.isText() || current.isBox());
    ASSERT(next.isText() || next.isBox());
    if (current.isBox() || next.isBox()) {
        // [text][container start][container end][inline box] (text<span></span><img>) : there's a soft wrap opportunity between the [text] and [img].
        // The line breaking behavior of a replaced element or other atomic inline is equivalent to an ideographic character.
        return true;
    }
    if (current.style().lineBreak() == LineBreak::Anywhere || next.style().lineBreak() == LineBreak::Anywhere) {
        // There is a soft wrap opportunity around every typographic character unit, including around any punctuation character
        // or preserved white spaces, or in the middle of words.
        return true;
    }
    auto& currentInlineTextItem = downcast<InlineTextItem>(current);
    auto& nextInlineTextItem = downcast<InlineTextItem>(next);
    if (currentInlineTextItem.isWhitespace()) {
        // [ ][text] : after [whitespace] position is a soft wrap opportunity.
        return true;
    }
    if (nextInlineTextItem.isWhitespace()) {
        // [text][ ] (<span>text</span> )
        // white-space: break-spaces: line breaking opportunity exists after every preserved white space character, but not before.
        return nextInlineTextItem.style().whiteSpace() != WhiteSpace::BreakSpaces;
    }
    // Both current and next items are non-whitespace text.
    // [text][text] : is a continuous content.
    // [text-][text] : after [hyphen] position is a soft wrap opportunity.
    return endsWithSoftWrapOpportunity(currentInlineTextItem, nextInlineTextItem);
}

size_t LineBreaker::nextWrapOpportunity(const InlineItems& inlineContent, unsigned startIndex)
{
    // 1. Find the start candidate by skipping leading non-content items e.g <span><span>start : skip "<span><span>"
    // 2. Find the end candidate by skipping non-content items inbetween e.g. <span><span>start</span>end: skip "</span>"
    // 3. Check if there's a soft wrap opportunity between the 2 candidate inline items and repeat.
    // 4. Any force line break inbetween is considered as a wrap opportunity.

    // [ex-][container start][container end][float][ample] (ex-<span></span><div style="float:left"></div>ample) : wrap index is at [ex-].
    // [ex][container start][amp-][container start][le] (ex<span>amp-<span>ample) : wrap index is at [amp-].
    // [ex-][container start][line break][ample] (ex-<span><br>ample) : wrap index is after [br].
    auto end = inlineContent.size();

    struct WrapContent {
        WrapContent(size_t index, bool isAtLineBreak)
            : m_index(index)
            , m_isAtLineBreak(isAtLineBreak)
        {
        }
        size_t operator*() const { return m_index; }
        bool isAtLineBreak() const { return m_isAtLineBreak; }

    private:
        size_t m_index { 0 };
        bool m_isAtLineBreak { false };
    };
    auto nextInlineItemWithContent = [&] (auto index) {
        // Break at the first text/box/line break inline item.
        for (; index < end; ++index) {
            auto& inlineItem = *inlineContent[index];
            if (inlineItem.isText() || inlineItem.isBox() || inlineItem.isLineBreak())
                return WrapContent { index, inlineItem.isLineBreak() };
        }
        return WrapContent { end, false };
    };

    // Start at the first inline item with content.
    // [container start][ex-] : start at [ex-]
    auto startContent = nextInlineItemWithContent(startIndex);
    if (startContent.isAtLineBreak()) {
        // Content starts with a line break. The wrap position is after the line break.
        return *startContent + 1;
    }

    while (*startContent != end) {
        // 1. Find the next inline item with content.
        // 2. Check if there's a soft wrap opportunity between the start and the next inline item.
        auto nextContent = nextInlineItemWithContent(*startContent + 1);
        if (*nextContent == end || nextContent.isAtLineBreak())
            return *nextContent;
        if (isAtSoftWrapOpportunity(*inlineContent[*startContent], *inlineContent[*nextContent])) {
            // There's a soft wrap opportunity between the start and the nextContent.
            // Now forward-find from the start position to see where we can actually wrap.
            // [ex-][ample] vs. [ex-][container start][container end][ample]
            // where [ex-] is startContent and [ample] is the nextContent.
            auto candidateIndex = *startContent + 1;
            for (; candidateIndex < *nextContent; ++candidateIndex) {
                if (inlineContent[candidateIndex]->isContainerStart()) {
                    // inline content and [container start] and [container end] form unbreakable content.
                    // ex-<span></span>ample  : wrap opportunity is after "ex-".
                    // ex-</span></span>ample : wrap opportunity is after "ex-</span></span>".
                    // ex-</span><span>ample</span> : wrap opportunity is after "ex-</span>".
                    // ex-<span><span>ample</span></span> : wrap opportunity is after "ex-".
                    return candidateIndex;
                }
            }
            return candidateIndex;
        }
        startContent = nextContent;
    }
    return end;
}

LineBreaker::ContinousContent::ContinousContent(const RunList& runs)
    : m_runs(runs)
{
    // Figure out the trailing collapsible state.
    for (auto& run : WTF::makeReversedRange(m_runs)) {
        auto& inlineItem = run.inlineItem;
        if (inlineItem.isBox()) {
            // We did reach a non-collapsible content. We have all the trailing whitespace now.
            break;
        }
        if (inlineItem.isText()) {
            auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
            auto isFullyCollapsible = [&] {
                return inlineTextItem.isWhitespace() && !TextUtil::shouldPreserveTrailingWhitespace(inlineTextItem.style());
            };
            if (isFullyCollapsible()) {
                m_trailingCollapsibleContent.width += run.logicalWidth;
                m_trailingCollapsibleContent.isFullyCollapsible = true;
                // Let's see if we've got more trailing whitespace content.
                continue;
            }
            if (auto collapsibleWidth = inlineTextItem.style().letterSpacing()) {
                m_trailingCollapsibleContent.width += collapsibleWidth;
                m_trailingCollapsibleContent.isFullyCollapsible = false;
            }
            // End of whitspace content.
            break;
        }
    }
    // The trailing whitespace loop above is mostly about inspecting the last entry, so while it
    // looks like we are looping through the m_runs twice, it's really just one full loop in addition to checking the last run.
    for (auto& run : m_runs) {
        // Line break is not considered an inline content.
        ASSERT(!run.inlineItem.isLineBreak());
        m_width += run.logicalWidth;
    }
}

bool LineBreaker::ContinousContent::hasTextContentOnly() const
{
    // <span>text</span> is considered a text run even with the [container start][container end] inline items.
    // Due to commit boundary rules, we just need to check the first non-typeless inline item (can't have both [img] and [text])
    for (auto& run : m_runs) {
        auto& inlineItem = run.inlineItem;
        if (inlineItem.isContainerStart() || inlineItem.isContainerEnd())
            continue;
        return inlineItem.isText();
    }
    return false;
}

bool LineBreaker::ContinousContent::isVisuallyEmptyWhitespaceContentOnly() const
{
    // [<span></span> ] [<span> </span>] [ <span style="padding: 0px;"></span>] are all considered visually empty whitespace content.
    // [<span style="border: 1px solid red"></span> ] while this is whitespace content only, it is not considered visually empty.
    // Due to commit boundary rules, we just need to check the first non-typeless inline item (can't have both [img] and [text])
    for (auto& run : m_runs) {
        auto& inlineItem = run.inlineItem;
        // FIXME: check for padding border etc.
        if (inlineItem.isContainerStart() || inlineItem.isContainerEnd())
            continue;
        return inlineItem.isText() && downcast<InlineTextItem>(inlineItem).isWhitespace();
    }
    return false;
}

Optional<unsigned> LineBreaker::ContinousContent::firstTextRunIndex() const
{
    for (size_t index = 0; index < m_runs.size(); ++index) {
        if (m_runs[index].inlineItem.isText())
            return index;
    }
    return { };
}

Optional<unsigned> LineBreaker::ContinousContent::lastContentRunIndex() const
{
    for (size_t index = m_runs.size(); index--;) {
        if (m_runs[index].inlineItem.isText() || m_runs[index].inlineItem.isBox())
            return index;
    }
    return { };
}

bool LineBreaker::ContinousContent::hasNonContentRunsOnly() const
{
    // <span></span> <- non content runs.
    for (auto& run : m_runs) {
        auto& inlineItem = run.inlineItem;
        if (inlineItem.isContainerStart() || inlineItem.isContainerEnd())
            continue;
        return false;
    }
    return true;
}

void LineBreaker::ContinousContent::TrailingCollapsibleContent::reset()
{
    isFullyCollapsible = false;
    width = 0_lu;
}


}
}
#endif
