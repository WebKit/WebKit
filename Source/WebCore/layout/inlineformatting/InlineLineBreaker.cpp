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

static inline bool isContentSplitAllowed(const LineBreaker::Content::Run& run)
{
    ASSERT(run.inlineItem.isText() || run.inlineItem.isContainerStart() || run.inlineItem.isContainerEnd());
    if (!run.inlineItem.isText()) {
        // Can't split horizontal spacing -> e.g. <span style="padding-right: 100px;">textcontent</span>, if the [container end] is the overflown inline item
        // we need to check if there's another inline item beyond the [container end] to split.
        return false;
    }
    return isTextContentWrappingAllowed(run.inlineItem.style());
}

static inline bool isTextSplitAtArbitraryPositionAllowed(const RenderStyle& style, bool lineIsEmpty)
{
    if (style.wordBreak() == WordBreak::BreakAll)
        return true;
    // For compatibility with legacy content, the word-break property also supports a deprecated break-word keyword.
    // When specified, this has the same effect as word-break: normal and overflow-wrap: anywhere, regardless of the actual value of the overflow-wrap property.
    if (style.wordBreak() == WordBreak::BreakWord && lineIsEmpty)
        return true;
    // OverflowWrap::Break -> An otherwise unbreakable sequence of characters may be broken at an arbitrary point if there are no otherwise-acceptable break points in the line.
    if (style.overflowWrap() == OverflowWrap::Break && lineIsEmpty)
        return true;
    return false;
}

static inline bool shouldKeepEndOfLineWhitespace(const LineBreaker::Content& candidateRuns)
{
    // Grab the style and check for white-space property to decided whether we should let this whitespace content overflow the current line.
    // Note that the "keep" in the context means we let the whitespace content sit on the current line.
    // It might very well get trimmed when we close the line (normal/nowrap/pre-line).
    // See https://www.w3.org/TR/css-text-3/#white-space-property
    auto whitespace = candidateRuns.runs()[*candidateRuns.firstTextRunIndex()].inlineItem.style().whiteSpace();
    return whitespace == WhiteSpace::Normal || whitespace == WhiteSpace::NoWrap || whitespace == WhiteSpace::PreWrap || whitespace == WhiteSpace::PreLine;
}

LineBreaker::BreakingContext LineBreaker::breakingContextForInlineContent(const Content& candidateRuns, const LineStatus& lineStatus)
{
    ASSERT(!candidateRuns.isEmpty());
    if (candidateRuns.width() <= lineStatus.availableWidth)
        return { BreakingContext::ContentWrappingRule::Keep, { } };
    if (candidateRuns.hasTrailingTrimmableContent()) {
        // First check if the content fits without the trailing trimmable part.
        if (candidateRuns.nonTrimmableWidth() <= lineStatus.availableWidth)
            return { BreakingContext::ContentWrappingRule::Keep, { } };
        // Now check if we can trim the line too.
        if (lineStatus.lineHasFullyTrimmableTrailingRun && candidateRuns.isTrailingContentFullyTrimmable()) {
            // If this new content is fully trimmable, it shoud surely fit.
            return { BreakingContext::ContentWrappingRule::Keep, { } };
        }
    } else if (lineStatus.trimmableWidth && candidateRuns.hasNonContentRunsOnly()) {
        // Let's see if the non-content runs fit when the line has trailing trimmable content
        // "text content <span style="padding: 1px"></span>" <- the <span></span> runs could fit after trimming the trailing whitespace.
        if (candidateRuns.width() <= lineStatus.availableWidth + lineStatus.trimmableWidth)
            return { BreakingContext::ContentWrappingRule::Keep, { } };
    }
    if (candidateRuns.isVisuallyEmptyWhitespaceContentOnly() && shouldKeepEndOfLineWhitespace(candidateRuns)) {
        // This overflowing content apparently falls into the remove/hang end-of-line-spaces catergory.
        // see https://www.w3.org/TR/css-text-3/#white-space-property matrix
        return { BreakingContext::ContentWrappingRule::Keep, { } };
    }

    if (candidateRuns.hasTextContentOnly()) {
        auto& runs = candidateRuns.runs();
        if (auto wrappedTextContent = wrapTextContent(runs, lineStatus)) {
            if (!wrappedTextContent->trailingRunIndex && wrappedTextContent->contentOverflows) {
                // We tried to split the content but the available space can't even accommodate the first character.
                // 1. Push the content over to the next line when we've got content on the line already.
                // 2. Keep the first character on the empty line (or keep the whole run if it has only one character).
                if (!lineStatus.lineIsEmpty)
                    return { BreakingContext::ContentWrappingRule::Push, { } };
                auto firstTextRunIndex = *candidateRuns.firstTextRunIndex();
                auto& inlineTextItem = downcast<InlineTextItem>(runs[firstTextRunIndex].inlineItem);
                ASSERT(inlineTextItem.length());
                if (inlineTextItem.length() == 1)
                    return { BreakingContext::ContentWrappingRule::Keep, { } };
                auto firstCharacterWidth = TextUtil::width(inlineTextItem, inlineTextItem.start(), inlineTextItem.start() + 1);
                auto firstCharacterRun = PartialRun { 1, firstCharacterWidth, false };
                return { BreakingContext::ContentWrappingRule::Split, BreakingContext::PartialTrailingContent { firstTextRunIndex, firstCharacterRun } };
            }
            auto splitContent = BreakingContext::PartialTrailingContent { wrappedTextContent->trailingRunIndex, wrappedTextContent->partialTrailingRun };
            return { BreakingContext::ContentWrappingRule::Split, splitContent };
        }
        // If we are not allowed to break this content, we still need to decide whether keep it or push it to the next line.
        auto contentShouldOverflow = lineStatus.lineIsEmpty || !isTextContentWrappingAllowed(runs[0].inlineItem.style());
        return { contentShouldOverflow ? BreakingContext::ContentWrappingRule::Keep : BreakingContext::ContentWrappingRule::Push, { } };
    }
    // First non-text inline content always stays on line.
    return { lineStatus.lineIsEmpty ? BreakingContext::ContentWrappingRule::Keep : BreakingContext::ContentWrappingRule::Push, { } };
}

bool LineBreaker::shouldWrapFloatBox(InlineLayoutUnit floatLogicalWidth, InlineLayoutUnit availableWidth, bool lineIsEmpty)
{
    return !lineIsEmpty && floatLogicalWidth > availableWidth;
}

Optional<LineBreaker::WrappedTextContent> LineBreaker::wrapTextContent(const Content::RunList& runs, const LineStatus& lineStatus) const
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

Optional<LineBreaker::PartialRun> LineBreaker::tryBreakingTextRun(const Content::Run& overflowRun, InlineLayoutUnit availableWidth, bool lineIsEmpty) const
{
    ASSERT(overflowRun.inlineItem.isText());
    auto& style = overflowRun.inlineItem.style();
    if (style.wordBreak() == WordBreak::KeepAll)
        return { };

    auto findLastBreakablePosition = availableWidth == maxInlineLayoutUnit();
    auto& inlineTextItem = downcast<InlineTextItem>(overflowRun.inlineItem);
    if (isTextSplitAtArbitraryPositionAllowed(style, lineIsEmpty)) {
        if (findLastBreakablePosition) {
            // When the run can be split at arbitrary positions,
            // let's just return the entire run when it is intended to fit on the line.
            return PartialRun { inlineTextItem.length(), overflowRun.logicalWidth, false };
        }
        // FIXME: Pass in the content logical left to be able to measure tabs.
        auto splitData = TextUtil::split(inlineTextItem.layoutBox(), inlineTextItem.start(), inlineTextItem.length(), overflowRun.logicalWidth, availableWidth, { });
        return PartialRun { splitData.length, splitData.logicalWidth, false };
    }
    // Find the hyphen position as follows:
    // 1. Split the text by taking the hyphen width into account
    // 2. Find the last hyphen position before the split position
    if (n_hyphenationIsDisabled || style.hyphens() != Hyphens::Auto || !canHyphenate(style.locale()))
        return { };

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

static bool endsWithSoftWrapOpportunity(const InlineTextItem& previousTextItem, const InlineTextItem& nextInlineTextItem)
{
    ASSERT(!previousTextItem.isWhitespace());
    ASSERT(!nextInlineTextItem.isWhitespace());
    // When both these non-whitespace runs belong to the same layout box, it's guaranteed that
    // they are split at a soft breaking opportunity. See InlineTextItem::moveToNextBreakablePosition.
    if (&previousTextItem.layoutBox() == &nextInlineTextItem.layoutBox())
        return true;
    // Now we need to collect at least 3 adjacent characters to be able to make a descision whether the previous text item ends with breaking opportunity.
    // [ex-][ample] <- second to last[x] last[-] current[a]
    // We need at least 1 character in the current inline text item and 2 more from previous inline items.
    auto previousContent = previousTextItem.layoutBox().textContext()->content;
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

bool LineBreaker::Content::isAtSoftWrapOpportunity(const InlineItem& inlineItem, const Content& priorContent)
{
    // https://drafts.csswg.org/css-text-3/#line-break-details
    // Figure out if the new incoming content puts the uncommitted content on a soft wrap opportunity.
    // e.g. [container start][prior_continuous_content][container end] (<span>prior_continuous_content</span>)
    // An incoming <img> box would enable us to commit the "<span>prior_continuous_content</span>" content
    // but an incoming text content would not necessarily.
    ASSERT(!inlineItem.isFloat() && !inlineItem.isLineBreak());
    if (priorContent.isEmpty()) {
        // Can't decide it yet.
        return false;
    }
    auto* lastUncomittedContent = &priorContent.runs().last().inlineItem;
    if (inlineItem.isText()) {
        if (downcast<InlineTextItem>(inlineItem).isWhitespace()) {
            // [prior content][ ] (<span>some_content</span> )
            // white-space: break-spaces: line breaking opportunity exists after every preserved white space character, but not before.
            auto isAtSoftWrapOpportunityBeforeWhitespace = inlineItem.style().whiteSpace() != WhiteSpace::BreakSpaces;
            // [ ][ ] : adjacent whitespace content has soft wrap opportunity.
            if (lastUncomittedContent->isText() && downcast<InlineTextItem>(*lastUncomittedContent).isWhitespace())
                isAtSoftWrapOpportunityBeforeWhitespace = true;
            return isAtSoftWrapOpportunityBeforeWhitespace;
        }
        if (lastUncomittedContent->isContainerStart()) {
            // [container start][text] (<span>text) : the [container start] and the [text] content form a continuous content.
            return false;
        }
        if (lastUncomittedContent->isContainerEnd()) {
            // [container end][text] (</span>text)
            // Need to check what's before the </span> to be able to decide whether it's a continuous content.
            // e.g.
            // [text][container end][text] (text</span>text) : there's no soft wrap opportunity here.
            // [inline box][container end][text] (<img></span>text) : after [container end] position is a soft wrap opportunity.
            auto& runs = priorContent.runs();
            auto didFindContent = false;
            for (auto i = priorContent.size(); i--;) {
                auto& previousInlineItem = runs[i].inlineItem;
                if (previousInlineItem.isContainerStart() || previousInlineItem.isContainerEnd())
                    continue;
                if (previousInlineItem.isText() || previousInlineItem.isBox()) {
                    lastUncomittedContent = &previousInlineItem;
                    didFindContent = true;
                    break;
                }
                ASSERT_NOT_REACHED();
            }
            // Did not find any content at all (e.g. [container start][container end][text] (<span></span>text)).
            if (!didFindContent)
                return false;
        }
        if (lastUncomittedContent->isText()) {
            // [text][text] : is a continuous content.
            // [text-][text] : after [hyphen] position is a soft wrap opportunity.
            // [ ][text] : after [whitespace] position is a soft wrap opportunity.
            auto& lastInlineTextItem = downcast<InlineTextItem>(*lastUncomittedContent);
            if (lastInlineTextItem.isWhitespace())
                return true;
            return endsWithSoftWrapOpportunity(lastInlineTextItem, downcast<InlineTextItem>(inlineItem));
        }
        if (lastUncomittedContent->isBox()) {
            // [inline box][text] (<img>text) : after [inline box] position is a soft wrap opportunity.
            return true;
        }
        ASSERT_NOT_REACHED();
    }
    if (inlineItem.isBox()) {
        if (lastUncomittedContent->isContainerStart()) {
            // [container start][inline box] (<spam><img>) : the [container start] and the [inline box] form a continuous content.
            return false;
        }
        if (lastUncomittedContent->isContainerEnd()) {
            // [container end][inline box] (</span><img>) : after [container end] position is a soft wrap opportunity.
            return true;
        }
        if (lastUncomittedContent->isText() || lastUncomittedContent->isBox()) {
            // [inline box][text] (<img>text) and [inline box][inline box] (<img><img>) : after first [inline box] position is a soft wrap opportunity.
            return true;
        }
        ASSERT_NOT_REACHED();
    }

    if (inlineItem.isContainerStart() || inlineItem.isContainerEnd()) {
        if (lastUncomittedContent->isContainerStart() || lastUncomittedContent->isContainerEnd()) {
            // [container start][container end] (<span><span>) or
            // [container end][container start] (</span><span>) : need more content to decide.
            return false;
        }
        if (lastUncomittedContent->isText()) {
            // [ ][container start] ( <span>) : after [whitespace] position is a soft wrap opportunity.
            // [text][container start] (text<span>) : Need more content to decide (e.g. text<span>text vs. text<span><img>).
            return downcast<InlineTextItem>(*lastUncomittedContent).isWhitespace();
        }
        if (lastUncomittedContent->isBox()) {
            // [inline box][container start] (<img><span>) : after [inline box] position is a soft wrap opportunity.
            // [inline box][container end] (<img></span>) : the [inline box] and the [container end] form a continuous content.
            return inlineItem.isContainerStart();
        }
        ASSERT_NOT_REACHED();
    }
    ASSERT_NOT_REACHED();
    return true;
}

void LineBreaker::Content::append(const InlineItem& inlineItem, InlineLayoutUnit logicalWidth)
{
    ASSERT(!inlineItem.isFloat());
    ASSERT(inlineItem.isLineBreak() || !isAtSoftWrapOpportunity(inlineItem, *this));
    m_continousRuns.append({ inlineItem, logicalWidth });
    m_width += logicalWidth;
    // Figure out the trailing trimmable state.
    if (inlineItem.isBox() || inlineItem.isLineBreak())
        m_trailingTrimmableContent.reset();
    else if (inlineItem.isText()) {
        auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
        auto isFullyTrimmable = [&] {
            return inlineTextItem.isWhitespace() && !TextUtil::shouldPreserveTrailingWhitespace(inlineTextItem.style());
        };
        if (isFullyTrimmable()) {
            m_trailingTrimmableContent.width += logicalWidth;
            m_trailingTrimmableContent.isFullyTrimmable = true;
        } else if (auto trimmableWidth = inlineTextItem.style().letterSpacing()) {
            m_trailingTrimmableContent.width = trimmableWidth;
            m_trailingTrimmableContent.isFullyTrimmable = false;
        } else
            m_trailingTrimmableContent.reset();
    }
}

void LineBreaker::Content::reset()
{
    m_continousRuns.clear();
    m_trailingTrimmableContent.reset();
    m_width = 0_lu;
}

void LineBreaker::Content::trim(unsigned newSize)
{
    for (auto i = m_continousRuns.size(); i--;)
        m_width -= m_continousRuns[i].logicalWidth;
    m_continousRuns.shrink(newSize);
}

bool LineBreaker::Content::hasTextContentOnly() const
{
    // <span>text</span> is considered a text run even with the [container start][container end] inline items.
    // Due to commit boundary rules, we just need to check the first non-typeless inline item (can't have both [img] and [text])
    for (auto& run : m_continousRuns) {
        auto& inlineItem = run.inlineItem;
        if (inlineItem.isContainerStart() || inlineItem.isContainerEnd())
            continue;
        return inlineItem.isText();
    }
    return false;
}

bool LineBreaker::Content::isVisuallyEmptyWhitespaceContentOnly() const
{
    // [<span></span> ] [<span> </span>] [ <span style="padding: 0px;"></span>] are all considered visually empty whitespace content.
    // [<span style="border: 1px solid red"></span> ] while this is whitespace content only, it is not considered visually empty.
    // Due to commit boundary rules, we just need to check the first non-typeless inline item (can't have both [img] and [text])
    for (auto& run : m_continousRuns) {
        auto& inlineItem = run.inlineItem;
        // FIXME: check for padding border etc.
        if (inlineItem.isContainerStart() || inlineItem.isContainerEnd())
            continue;
        return inlineItem.isText() && downcast<InlineTextItem>(inlineItem).isWhitespace();
    }
    return false;
}

Optional<unsigned> LineBreaker::Content::firstTextRunIndex() const
{
    for (size_t index = 0; index < m_continousRuns.size(); ++index) {
        if (m_continousRuns[index].inlineItem.isText())
            return index;
    }
    return { };
}

bool LineBreaker::Content::hasNonContentRunsOnly() const
{
    // <span></span> <- non content runs.
    for (auto& run : m_continousRuns) {
        auto& inlineItem = run.inlineItem;
        if (inlineItem.isContainerStart() || inlineItem.isContainerEnd())
            continue;
        return false;
    }
    return true;
}

void LineBreaker::Content::TrailingTrimmableContent::reset()
{
    isFullyTrimmable = false;
    width = 0_lu;
}


}
}
#endif
