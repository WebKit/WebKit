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
#include "InlineContentBreaker.h"

#include "FontCascade.h"
#include "Hyphenation.h"
#include "InlineItem.h"
#include "InlineTextItem.h"
#include "LayoutElementBox.h"
#include "TextUtil.h"

namespace WebCore {
namespace Layout {


#if ASSERT_ENABLED
static inline bool hasTrailingTextContent(const InlineContentBreaker::ContinuousContent& continuousContent)
{
    for (auto& run : makeReversedRange(continuousContent.runs())) {
        auto& inlineItem = run.inlineItem;
        if (inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd())
            continue;
        return inlineItem.isText();
    }
    return false;
}
#endif

static inline bool hasLeadingTextContent(const InlineContentBreaker::ContinuousContent& continuousContent)
{
    for (auto& run : continuousContent.runs()) {
        auto& inlineItem = run.inlineItem;
        if (inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd())
            continue;
        return inlineItem.isText();
    }
    return false;
}

static inline bool hasTextRun(const InlineContentBreaker::ContinuousContent& continuousContent)
{
    // <span>text</span> is considered a text run even with the [inline box start][inline box end] inline items.
    // Based on standards commit boundary rules it would be enough to check the first inline item.
    for (auto& run : continuousContent.runs()) {
        if (run.inlineItem.isText())
            return true;
    }
    return false;
}

static inline std::optional<size_t> nextTextRunIndex(const InlineContentBreaker::ContinuousContent::RunList& runs, size_t startIndex)
{
    for (auto index = startIndex + 1; index < runs.size(); ++index) {
        if (runs[index].inlineItem.isText())
            return index;
    }
    return { };
}

static inline bool isWhitespaceOnlyContent(const InlineContentBreaker::ContinuousContent& continuousContent)
{
    // [<span></span> ] [<span> </span>] [ <span style="padding: 0px;"></span>] are all considered visually empty whitespace content.
    // [<span style="border: 1px solid red"></span> ] while this is whitespace content only, it is not considered visually empty.
    ASSERT(!continuousContent.runs().isEmpty());
    auto hasWhitespace = false;
    for (auto& run : continuousContent.runs()) {
        auto& inlineItem = run.inlineItem;
        if (inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd())
            continue;
        auto isWhitespace = inlineItem.isText() && downcast<InlineTextItem>(inlineItem).isWhitespace();
        if (!isWhitespace)
            return false;
        hasWhitespace = true;
    }
    return hasWhitespace;
}

static inline bool isNonContentRunsOnly(const InlineContentBreaker::ContinuousContent& continuousContent)
{
    // <span></span> <- non content runs.
    for (auto& run : continuousContent.runs()) {
        auto& inlineItem = run.inlineItem;
        if (inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd())
            continue;
        return false;
    }
    return true;
}

static inline std::optional<size_t> firstTextRunIndex(const InlineContentBreaker::ContinuousContent& continuousContent)
{
    auto& runs = continuousContent.runs();
    for (size_t index = 0; index < runs.size(); ++index) {
        if (runs[index].inlineItem.isText())
            return index;
    }
    return { };
}

InlineContentBreaker::InlineContentBreaker(std::optional<IntrinsicWidthMode> intrinsicWidthMode)
    : m_intrinsicWidthMode(intrinsicWidthMode)
{
}

InlineContentBreaker::Result InlineContentBreaker::processInlineContent(const ContinuousContent& candidateContent, const LineStatus& lineStatus)
{
    ASSERT(!std::isnan(lineStatus.availableWidth));
    auto processCandidateContent = [&] {
        if (candidateContent.logicalWidth() <= lineStatus.availableWidth)
            return Result { Result::Action::Keep };
        return processOverflowingContent(candidateContent, lineStatus);
    };

    auto result = processCandidateContent();
    if (result.action == Result::Action::Wrap && lineStatus.trailingSoftHyphenWidth && hasLeadingTextContent(candidateContent)) {
        // A trailing soft hyphen with a wrapped text content turns into a visible hyphen.
        // Let's check if there's enough space for the hyphen character.
        auto hyphenOverflows = *lineStatus.trailingSoftHyphenWidth > lineStatus.availableWidth;
        auto action = hyphenOverflows ? Result::Action::RevertToLastNonOverflowingWrapOpportunity : Result::Action::WrapWithHyphen;
        result = { action, IsEndOfLine::Yes };
    }
    return result;
}

InlineContentBreaker::Result InlineContentBreaker::processOverflowingContent(const ContinuousContent& overflowContent, const LineStatus& lineStatus) const
{
    auto continuousContent = ContinuousContent { overflowContent };
    ASSERT(!continuousContent.runs().isEmpty());

    ASSERT(continuousContent.logicalWidth() > lineStatus.availableWidth);
    auto checkForTrailingContentFit = [&]() -> std::optional<InlineContentBreaker::Result> {
        if (continuousContent.hasTrimmableContent()) {
            // Check if the content fits if we trimmed it.
            if (continuousContent.isFullyTrimmable() || isWhitespaceOnlyContent(continuousContent)) {
                // If this new content is fully trimmable (including when it is enclosed by an inline box with overflowing decoration)
                // it should not be wrapped to the next line (as it either fits/or gets fully trimmed).
                return InlineContentBreaker::Result { Result::Action::Keep };
            }
            auto spaceRequired = continuousContent.logicalWidth() - continuousContent.trailingTrimmableWidth().value_or(0.f);
            if (lineStatus.hasFullyTrimmableTrailingContent)
                spaceRequired -= continuousContent.leadingTrimmableWidth().value_or(0.f);
            if (spaceRequired <= lineStatus.availableWidth)
                return InlineContentBreaker::Result { Result::Action::Keep };
        }

        if (continuousContent.hasHangingContent()) {
            if (continuousContent.isHangingContent())
                return InlineContentBreaker::Result { Result::Action::Keep };
            auto hangingTrailingContentWidth = *continuousContent.hangingContentWidth();
            auto spaceRequired = continuousContent.logicalWidth() - hangingTrailingContentWidth;
            if (spaceRequired <= lineStatus.availableWidth)
                return InlineContentBreaker::Result { Result::Action::Keep };
        }

        auto canIgnoreNonContentTrailingRuns = lineStatus.trimmableOrHangingWidth && isNonContentRunsOnly(continuousContent);
        if (canIgnoreNonContentTrailingRuns) {
            // Let's see if the non-content runs fit when the line has trailing trimmable/hanging content.
            // "text content <span style="padding: 1px"></span>" <- the <span></span> runs could fit after trimming the trailing whitespace.
            if (continuousContent.logicalWidth() <= lineStatus.availableWidth + lineStatus.trimmableOrHangingWidth)
                return InlineContentBreaker::Result { Result::Action::Keep };
        }

        return { };
    };
    if (auto result = checkForTrailingContentFit())
        return *result;

    size_t overflowingRunIndex = 0;
    if (hasTextRun(continuousContent)) {
        auto tryBreakingContentWithText = [&]() -> std::optional<Result> {
            // 1. This text content is not breakable.
            // 2. This breakable text content does not fit at all. Not even the first glyph. This is a very special case.
            // 3. We can break the content but it still overflows.
            // 4. Managed to break the content before the overflow point.
            auto overflowingContent = processOverflowingContentWithText(continuousContent, lineStatus);
            overflowingRunIndex = overflowingContent.runIndex;
            if (!overflowingContent.breakingPosition)
                return { };
            auto trailingContent = overflowingContent.breakingPosition->trailingContent;
            if (!trailingContent) {
                // We tried to break the content but the available space can't even accommodate the first glyph.
                // 1. Wrap the content over to the next line when we've got content on the line already.
                // 2. Keep the first glyph on the empty line (or keep the whole run if it has only one glyph/completely empty)
                // including closing inline boxes e.g. <span><span>X</span></span> where "X" is the overflowing glyph).
                if (lineStatus.hasContent)
                    return Result { Result::Action::Wrap, IsEndOfLine::Yes };

                auto leadingTextRunIndex = *firstTextRunIndex(continuousContent);
                auto& leadingTextRun = continuousContent.runs()[leadingTextRunIndex];
                auto& inlineTextItem = downcast<InlineTextItem>(leadingTextRun.inlineItem);
                auto firstCharacterLength = TextUtil::firstUserPerceivedCharacterLength(inlineTextItem);
                ASSERT(firstCharacterLength > 0);

                if (inlineTextItem.length() <= firstCharacterLength) {
                    auto trailingRunIndex = [&]() -> std::optional<size_t> {
                        // Keep the overflowing text content and the closing inline box runs together.
                        // e.g. X</span><span>Y</span> where "X" overflows, the trailing run index is 1.
                        auto& runs = continuousContent.runs();
                        if (leadingTextRunIndex == runs.size() - 1)
                            return { };
                        if (!runs[leadingTextRunIndex + 1].inlineItem.isInlineBoxEnd())
                            return leadingTextRunIndex;
                        for (auto runIndex = leadingTextRunIndex + 1; runIndex < runs.size(); ++runIndex) {
                            if (!runs[runIndex].inlineItem.isInlineBoxEnd())
                                return runIndex - 1;
                        }
                        return { };
                    };
                    if (auto runToBreakAfter = trailingRunIndex())
                        return Result { Result::Action::Break, IsEndOfLine::Yes, Result::PartialTrailingContent { *runToBreakAfter, { } } };
                    return Result { Result::Action::Keep, IsEndOfLine::Yes };
                }

                auto firstCharacterWidth = TextUtil::width(inlineTextItem, leadingTextRun.style.fontCascade(), inlineTextItem.start(), inlineTextItem.start() + firstCharacterLength, lineStatus.contentLogicalRight);
                return Result { Result::Action::Break, IsEndOfLine::Yes, Result::PartialTrailingContent { leadingTextRunIndex, PartialRun { firstCharacterLength, firstCharacterWidth } } };
            }
            if (trailingContent->overflows && lineStatus.hasContent) {
                // We managed to break a run with overflow but the line already has content. Let's wrap it to the next line.
                return Result { Result::Action::Wrap, IsEndOfLine::Yes };
            }
            // Either we managed to break with no overflow or the line is empty.
            auto trailingPartialContent = Result::PartialTrailingContent { overflowingContent.breakingPosition->runIndex, trailingContent->partialRun };
            return Result { Result::Action::Break, IsEndOfLine::Yes, trailingPartialContent };
        };
        if (auto result = tryBreakingContentWithText())
            return *result;
    } else if (continuousContent.runs().size() > 1) {
        // FIXME: Add support for various content.
        auto& runs = continuousContent.runs();
        for (size_t i = 0; i < runs.size(); ++i) {
            if (runs[i].inlineItem.isBox()) {
                overflowingRunIndex = i;
                break;
            }
        }
    }

    // If we are not allowed to break this overflowing content, we still need to decide whether keep it or wrap it to the next line.
    if (!lineStatus.hasContent)
        return { Result::Action::Keep, IsEndOfLine::No };
    // Now either wrap this content over to the next line or revert back to an earlier wrapping opportunity, or not wrap at all.
    auto shouldWrapUnbreakableContentToNextLine = [&] {
        // The individual runs in this continuous content don't break, let's check if we are allowed to wrap this content to next line (e.g. pre would prevent us from wrapping).
        // Parent style drives the wrapping behavior here.
        // e.g. <div style="white-space: nowrap">some text<div style="display: inline-block; white-space: pre-wrap"></div></div>.
        // While the inline-block has pre-wrap which allows wrapping, the content lives in a nowrap context.
        auto& parentLayoutBox = continuousContent.runs()[overflowingRunIndex].inlineItem.layoutBox().parent();
        return TextUtil::isWrappingAllowed(parentLayoutBox.style());
    };
    if (shouldWrapUnbreakableContentToNextLine())
        return { Result::Action::Wrap, IsEndOfLine::Yes };
    if (lineStatus.hasWrapOpportunityAtPreviousPosition)
        return { Result::Action::RevertToLastWrapOpportunity, IsEndOfLine::Yes };
    return { Result::Action::Keep, IsEndOfLine::No };
}

static std::optional<size_t> findTrailingRunIndex(const InlineContentBreaker::ContinuousContent::RunList& runs, size_t breakableRunIndex)
{
    // When the breaking position is at the beginning of the run, the trailing run is the previous one.
    if (!breakableRunIndex)
        return { };
    // Try not break content at inline box boundary
    // e.g. <span>fits</span><span>overflows</span>
    // when the text "overflows" completely overflows, let's break the content right before the '<span>'.
    for (auto trailingCandidateIndex = breakableRunIndex; trailingCandidateIndex--;) {
        auto isAtInlineBox = runs[trailingCandidateIndex].inlineItem.isInlineBoxStart();
        if (!isAtInlineBox)
            return trailingCandidateIndex;
    }
    return { };
}

static bool isWrappableRun(const InlineContentBreaker::ContinuousContent::Run& run)
{
    ASSERT(run.inlineItem.isText() || run.inlineItem.isInlineBoxStart() || run.inlineItem.isInlineBoxEnd() || run.inlineItem.layoutBox().isImage() || run.inlineItem.layoutBox().isListMarkerBox());
    if (!run.inlineItem.isText()) {
        // Can't break horizontal spacing -> e.g. <span style="padding-right: 100px;">textcontent</span>, if the [inline box end] is the overflown inline item
        // we need to check if there's another inline item beyond the [inline box end] to split.
        return false;
    }
    // Check if this text run needs to stay on the current line.
    return TextUtil::isWrappingAllowed(run.style);
}

static inline bool canBreakBefore(UChar32 character, LineBreak lineBreak)
{
    // FIXME: This should include all the cases from https://unicode.org/reports/tr14
    // Use a breaking matrix similar to lineBreakTable in BreakLines.cpp
    // Also see kBreakAllLineBreakClassTable in third_party/blink/renderer/platform/text/text_break_iterator.cc
    if (lineBreak != LineBreak::Loose) {
        // The following breaks are allowed for loose line breaking if the preceding character belongs to the Unicode
        // line breaking class ID, and are otherwise forbidden:
        // ‐ U+2010, – U+2013
        // https://drafts.csswg.org/css-text/#line-break-property
        if (character == hyphen || character == enDash)
            return false;
    }
    if (character == noBreakSpace)
        return false;
    auto isPunctuation = U_GET_GC_MASK(character) & (U_GC_PS_MASK | U_GC_PE_MASK | U_GC_PI_MASK | U_GC_PF_MASK | U_GC_PO_MASK);
    return character == reverseSolidus || !isPunctuation;
}

static inline std::optional<size_t> lastValidBreakingPosition(const InlineContentBreaker::ContinuousContent::RunList& runs, size_t textRunIndex)
{
    auto& textRun = runs[textRunIndex];
    auto& inlineTextItem = downcast<InlineTextItem>(textRun.inlineItem);
    ASSERT(inlineTextItem.length());
    auto lineBreak = textRun.style.lineBreak();

    auto adjactentTextRunIndex = nextTextRunIndex(runs, textRunIndex);
    if (!adjactentTextRunIndex)
        return inlineTextItem.end();

    auto& nextInlineTextItem = downcast<InlineTextItem>(runs[*adjactentTextRunIndex].inlineItem);
    auto canBreakAtRunBoundary = nextInlineTextItem.isWhitespace() ? nextInlineTextItem.style().whiteSpace() != WhiteSpace::BreakSpaces :
        canBreakBefore(nextInlineTextItem.inlineTextBox().content()[nextInlineTextItem.start()], lineBreak);
    if (canBreakAtRunBoundary)
        return inlineTextItem.end();

    // Find out if the candidate position for arbitrary breaking is valid. We can't always break between any characters.
    auto text = inlineTextItem.inlineTextBox().content();
    auto left = inlineTextItem.start();
    for (auto index = inlineTextItem.end() - 1; index > left; --index) {
        U16_SET_CP_START(text, left, index);
        // We should never find surrogates/segments across inline items.
        ASSERT(index >= inlineTextItem.start());
        if (canBreakBefore(text[index], lineBreak))
            return index == inlineTextItem.start() ? std::nullopt : std::make_optional(index);
    }
    return { };
}

static std::optional<TextUtil::WordBreakLeft> midWordBreak(const InlineContentBreaker::ContinuousContent::Run& textRun, InlineLayoutUnit runLogicalLeft, InlineLayoutUnit availableWidth)
{
    ASSERT(textRun.style.wordBreak() == WordBreak::BreakAll);
    auto& inlineTextItem = downcast<InlineTextItem>(textRun.inlineItem);

    auto wordBreak = TextUtil::breakWord(inlineTextItem, textRun.style.fontCascade(), textRun.logicalWidth, availableWidth, runLogicalLeft);
    if (!wordBreak.length || wordBreak.length == inlineTextItem.length())
        return { };

    // Find out if the candidate position for arbitrary breaking is valid. We can't always break between any characters.
    auto lineBreak = textRun.style.lineBreak();
    auto text = inlineTextItem.inlineTextBox().content();
    if (canBreakBefore(text[inlineTextItem.start() + wordBreak.length], lineBreak))
        return wordBreak;

    const auto left = inlineTextItem.start();
    auto right = left + wordBreak.length;
    for (; right > left; --right) {
        U16_SET_CP_START(text, left, right);
        if (canBreakBefore(text[right], lineBreak))
            break;
    }
    if (left == right)
        return { };
    return TextUtil::WordBreakLeft { right - left, TextUtil::width(inlineTextItem, textRun.style.fontCascade(), left, right, runLogicalLeft) };
}

struct CandidateTextRunForBreaking {
    size_t index { 0 };
    bool isOverflowingRun { true };
    InlineLayoutUnit logicalLeft { 0 };
};
std::optional<InlineContentBreaker::PartialRun> InlineContentBreaker::tryBreakingTextRun(const ContinuousContent::RunList& runs, const CandidateTextRunForBreaking& candidateTextRun, InlineLayoutUnit availableWidth, const LineStatus& lineStatus) const
{
    auto& candidateRun = runs[candidateTextRun.index];
    ASSERT(candidateRun.inlineItem.isText());
    auto& inlineTextItem = downcast<InlineTextItem>(candidateRun.inlineItem);
    auto& style = candidateRun.style;
    auto lineHasRoomForContent = availableWidth > 0;

    auto breakRules = wordBreakBehavior(style, lineStatus.hasWrapOpportunityAtPreviousPosition);
    if (breakRules.isEmpty())
        return { };

    auto& fontCascade = style.fontCascade();
    if (breakRules.contains(WordBreakRule::AtArbitraryPositionWithinWords)) {
        auto tryBreakingAtArbitraryPositionWithinWords = [&]() -> std::optional<PartialRun> {
            // Breaking is allowed within “words”: specifically, in addition to soft wrap opportunities allowed for normal, any typographic letter units
            // It does not affect rules governing the soft wrap opportunities created by white space. Hyphenation is not applied.
            ASSERT(!breakRules.contains(WordBreakRule::AtHyphenationOpportunities));
            if (inlineTextItem.isWhitespace()) {
                // AtArbitraryPositionWithinWords does not affect the breaking opportunities around whitespace.
                return { };
            }

            if (!inlineTextItem.length()) {
                // Empty/single character text runs may be breakable based on style, but in practice we can't really split them any further.
                return { };
            }

            if (candidateTextRun.isOverflowingRun) {
                if (lineHasRoomForContent) {
                    // Try to break the overflowing run mid-word.
                    if (auto wordBreak = midWordBreak(candidateRun, candidateTextRun.logicalLeft, availableWidth))
                        return PartialRun { wordBreak->length, wordBreak->logicalWidth };
                }
                if (canBreakBefore(inlineTextItem.inlineTextBox().content()[inlineTextItem.start()], style.lineBreak()))
                    return PartialRun { };
                else {
                    // Since this is an overflowing content and we are allowed to break at arbitrary position, we really ought to find a breaking position.
                    // Unless of course it's really an unbreakable content with nothing but e.g. punctuation characters.
                    // FIXME: This should be merged with the "let's keep the first character on the line" logic (see in InlineContentBreaker::processOverflowingContent)
                    auto firstBreakablePosition = [&] () -> std::optional<TextUtil::WordBreakLeft> {
                        if (lineStatus.hasContent)
                            return { };
                        auto text = inlineTextItem.inlineTextBox().content();
                        const auto left = inlineTextItem.start();
                        auto right = left;
                        U16_SET_CP_START(text, left, right);
                        while (right < inlineTextItem.end()) {
                            U16_FWD_1(text, right, inlineTextItem.length());
                            if (canBreakBefore(text[right], style.lineBreak())) {
                                if (right == inlineTextItem.end())
                                    return { };
                                return TextUtil::WordBreakLeft { right - left, TextUtil::width(inlineTextItem, style.fontCascade(), left, right, candidateTextRun.logicalLeft) };
                            }
                        }
                        return { };
                    };
                    if (auto wordBreak = firstBreakablePosition())
                        return PartialRun { wordBreak->length, wordBreak->logicalWidth };
                }
                return { };
            }

            // This is a non-overflowing content.
            ASSERT(lineHasRoomForContent);
            if (auto breakingPosition = lastValidBreakingPosition(runs, candidateTextRun.index)) {
                ASSERT(*breakingPosition <= inlineTextItem.end());
                auto trailingLength = *breakingPosition - inlineTextItem.start();
                auto startPosition = inlineTextItem.start();
                auto endPosition = startPosition + trailingLength;
                return PartialRun { trailingLength, TextUtil::width(inlineTextItem, fontCascade, startPosition, endPosition, candidateTextRun.logicalLeft) };
            }
            return { };
        };
        return tryBreakingAtArbitraryPositionWithinWords();
    }

    if (breakRules.contains(WordBreakRule::AtHyphenationOpportunities)) {
        auto tryBreakingAtHyphenationOpportunity = [&]() -> std::optional<PartialRun> {
            // Find the hyphen position as follows:
            // 1. Split the text by taking the hyphen width into account
            // 2. Find the last hyphen position before the split position
            if (candidateTextRun.isOverflowingRun && !lineHasRoomForContent) {
                // We won't be able to find hyphen location when there's no available space.
                return { };
            }
            auto runLength = inlineTextItem.length();
            unsigned limitBefore = style.hyphenationLimitBefore() == RenderStyle::initialHyphenationLimitBefore() ? 0 : style.hyphenationLimitBefore();
            unsigned limitAfter = style.hyphenationLimitAfter() == RenderStyle::initialHyphenationLimitAfter() ? 0 : style.hyphenationLimitAfter();
            // Check if this run can accommodate the before/after limits at all before start measuring text.
            if (limitBefore >= runLength || limitAfter >= runLength || limitBefore + limitAfter > runLength)
                return { };

            unsigned leftSideLength = runLength;
            auto hyphenWidth = InlineLayoutUnit { fontCascade.width(TextRun { StringView { style.hyphenString() } }) };
            if (candidateTextRun.isOverflowingRun) {
                auto availableWidthExcludingHyphen = availableWidth - hyphenWidth;
                if (availableWidthExcludingHyphen <= 0 || !enoughWidthForHyphenation(availableWidthExcludingHyphen, fontCascade.pixelSize()))
                    return { };
                leftSideLength = TextUtil::breakWord(inlineTextItem, fontCascade, candidateRun.logicalWidth, availableWidthExcludingHyphen, candidateTextRun.logicalLeft).length;
            }
            if (leftSideLength < limitBefore)
                return { };
            // Adjust before index to accommodate the limit-after value (it's the last potential hyphen location in this run).
            auto hyphenBefore = std::min(leftSideLength, runLength - limitAfter) + 1;
            unsigned hyphenLocation = lastHyphenLocation(StringView(inlineTextItem.inlineTextBox().content()).substring(inlineTextItem.start(), inlineTextItem.length()), hyphenBefore, style.computedLocale());
            if (!hyphenLocation || hyphenLocation < limitBefore)
                return { };
            // hyphenLocation is relative to the start of this InlineItemText.
            ASSERT(inlineTextItem.start() + hyphenLocation < inlineTextItem.end());
            auto trailingPartialRunWidthWithHyphen = TextUtil::width(inlineTextItem, fontCascade, inlineTextItem.start(), inlineTextItem.start() + hyphenLocation, candidateTextRun.logicalLeft);
            return PartialRun { hyphenLocation, trailingPartialRunWidthWithHyphen, hyphenWidth };
        };
        if (auto partialRun = tryBreakingAtHyphenationOpportunity())
            return partialRun;
    }

    if (breakRules.contains(WordBreakRule::AtArbitraryPosition)) {
        auto tryBreakingAtArbitraryPosition = [&]() -> std::optional<PartialRun> {
            if (!inlineTextItem.length()) {
                // Empty text runs may be breakable based on style, but in practice we can't really split them any further.
                return { };
            }
            if (!candidateTextRun.isOverflowingRun) {
                // When the run can be split at arbitrary position let's just return the entire run when it is intended to fit on the line.
                // However the breaking properties only set rules for text content, so let's check if this run is adjacent to another text run.
                ASSERT(inlineTextItem.length());
                // FIXME: We may need to check if the "next" text run is visually adjacent to this non-overflowing run too (e.g. A<span style="border: 100px solid green;"></span>B)
                if (nextTextRunIndex(runs, candidateTextRun.index)) {
                    // We are in-between text runs. It's okay to return the entire run triggering split at the very right edge.
                    auto trailingPartialRunWidth = TextUtil::width(inlineTextItem, fontCascade, candidateTextRun.logicalLeft);
                    return PartialRun { inlineTextItem.length(), trailingPartialRunWidth };
                }
                if (inlineTextItem.length() > 1) {
                    auto startPosition = inlineTextItem.start();
                    auto endPosition = inlineTextItem.end() - 1;
                    return PartialRun { inlineTextItem.length() - 1, TextUtil::width(inlineTextItem, fontCascade, startPosition, endPosition, candidateTextRun.logicalLeft) };
                }
                return { };
            }
            if (!lineHasRoomForContent) {
                // Fast path for cases when there's no room at all. The content is breakable but we don't have space for it.
                return PartialRun { };
            }
            auto wordBreak = TextUtil::breakWord(inlineTextItem, fontCascade, candidateRun.logicalWidth, availableWidth, candidateTextRun.logicalLeft);
            return PartialRun { wordBreak.length, wordBreak.logicalWidth };
        };
        // With arbitrary breaking there's always a valid breaking position (even if it is before the first position).
        return tryBreakingAtArbitraryPosition();
    }

    return { };
}

std::optional<InlineContentBreaker::OverflowingTextContent::BreakingPosition> InlineContentBreaker::tryBreakingOverflowingRun(const LineStatus& lineStatus, const ContinuousContent::RunList& runs, size_t overflowingRunIndex, InlineLayoutUnit nonOverflowingContentWidth) const
{
    auto overflowingRun = runs[overflowingRunIndex];
    if (!isWrappableRun(overflowingRun))
        return { };

    auto availableWidth = std::max(0.f, lineStatus.availableWidth - nonOverflowingContentWidth);
    auto partialOverflowingRun = tryBreakingTextRun(runs, { overflowingRunIndex, true, lineStatus.contentLogicalRight + nonOverflowingContentWidth }, availableWidth, lineStatus);
    if (!partialOverflowingRun)
        return { };
    if (partialOverflowingRun->length)
        return OverflowingTextContent::BreakingPosition { overflowingRunIndex, OverflowingTextContent::BreakingPosition::TrailingContent { false, partialOverflowingRun } };
    // When the breaking position is at the beginning of the run, the trailing run is the previous one.
    if (auto trailingRunIndex = findTrailingRunIndex(runs, overflowingRunIndex))
        return OverflowingTextContent::BreakingPosition { *trailingRunIndex, OverflowingTextContent::BreakingPosition::TrailingContent { } };
    // Sometimes we can't accommodate even the very first character.
    // Note that this is different from when there's no breakable run in this set.
    return OverflowingTextContent::BreakingPosition { };
}

std::optional<InlineContentBreaker::OverflowingTextContent::BreakingPosition> InlineContentBreaker::tryBreakingPreviousNonOverflowingRuns(const LineStatus& lineStatus, const ContinuousContent::RunList& runs, size_t overflowingRunIndex, InlineLayoutUnit nonOverflowingContentWidth) const
{
    auto previousContentWidth = nonOverflowingContentWidth;
    for (auto index = overflowingRunIndex; index--;) {
        auto& run = runs[index];
        previousContentWidth -= run.logicalWidth;
        if (!isWrappableRun(run))
            continue;
        ASSERT(run.inlineItem.isText());
        auto availableWidth = std::max(0.f, lineStatus.availableWidth - previousContentWidth);
        if (auto partialRun = tryBreakingTextRun(runs, { index, false, lineStatus.contentLogicalRight + previousContentWidth }, availableWidth, lineStatus)) {
            // We know this run fits, so if breaking is allowed on the run, it should return a non-empty left-side
            // since it's either at hyphen position or the entire run is returned.
            ASSERT(partialRun->length);
            auto runIsFullyAccommodated = partialRun->length == downcast<InlineTextItem>(run.inlineItem).length();
            if (runIsFullyAccommodated) {
                auto trailingRunIndex = [&] {
                    // Try not break content at inline box boundary.
                    // e.g. <span style="word-wrap: break-word">fits_and_we_break_at_the_right_edge</span><span>overflows</span>
                    // we should forward the breaking index to the closing inline box.
                    // FIXME: We may wanna skip over the visually empty inline boxes only e.g. <span style="word-wrap: break-word">fits_and_we_break_at_the_right_edge</span><span></span><span>overflows</span>
                    auto trailingInlineBoxEndIndex = std::optional<size_t> { };
                    for (auto candidateIndex = index + 1; candidateIndex <= overflowingRunIndex; ++candidateIndex) {
                        auto& trailingInlineItem = runs[candidateIndex].inlineItem;
                        if (trailingInlineItem.isInlineBoxEnd())
                            trailingInlineBoxEndIndex = candidateIndex;
                        if (!trailingInlineItem.isInlineBoxStart() && !trailingInlineItem.isInlineBoxEnd())
                            break;
                    }
                    ASSERT(!trailingInlineBoxEndIndex || *trailingInlineBoxEndIndex <= overflowingRunIndex);
                    return trailingInlineBoxEndIndex.value_or(index);
                };
                return OverflowingTextContent::BreakingPosition { trailingRunIndex(), OverflowingTextContent::BreakingPosition::TrailingContent { false, std::nullopt } };
            }
            return OverflowingTextContent::BreakingPosition { index, OverflowingTextContent::BreakingPosition::TrailingContent { false, partialRun } };
        }
    }
    return { };
}

std::optional<InlineContentBreaker::OverflowingTextContent::BreakingPosition> InlineContentBreaker::tryBreakingNextOverflowingRuns(const LineStatus& lineStatus, const ContinuousContent::RunList& runs, size_t overflowingRunIndex, InlineLayoutUnit nonOverflowingContentWidth) const
{
    auto nextContentWidth = nonOverflowingContentWidth + runs[overflowingRunIndex].logicalWidth;
    for (auto index = overflowingRunIndex + 1; index < runs.size(); ++index) {
        auto& run = runs[index];
        if (!isWrappableRun(run)) {
            nextContentWidth += run.logicalWidth;
            continue;
        }
        ASSERT(run.inlineItem.isText());
        // At this point the available space is zero. Let's try the break these overflowing set of runs at the earliest possible.
        if (auto partialRun = tryBreakingTextRun(runs, { index, true, lineStatus.contentLogicalRight + nextContentWidth }, 0, lineStatus)) {
            // <span>unbreakable_and_overflows<span style="word-break: break-all">breakable</span>
            // The partial run length could very well be 0 meaning the trailing run is actually the overflowing run (see above in the example).
            if (partialRun->length) {
                // We managed to break this text run mid content. It has to be either an arbitrary mid-word or a hyphen break.
                return OverflowingTextContent::BreakingPosition { index, OverflowingTextContent::BreakingPosition::TrailingContent { true, partialRun } };
            }
            if (auto trailingRunIndex = findTrailingRunIndex(runs, index)) {
                // At worst we are back to the overflowing run, like in the example above.
                ASSERT(*trailingRunIndex >= overflowingRunIndex);
                return OverflowingTextContent::BreakingPosition { *trailingRunIndex, OverflowingTextContent::BreakingPosition::TrailingContent { true } };
            }
            // This happens when the overflowing run is also the first run in this set, no trailing run.
            return OverflowingTextContent::BreakingPosition { overflowingRunIndex, { } };
        }
        nextContentWidth += run.logicalWidth;
    }
    return { };
}

InlineContentBreaker::OverflowingTextContent InlineContentBreaker::processOverflowingContentWithText(const ContinuousContent& continuousContent, const LineStatus& lineStatus) const
{
    auto& runs = continuousContent.runs();
    ASSERT(!runs.isEmpty());

    // Check where the overflow occurs and use the corresponding style to figure out the breaking behavior.
    // <span style="word-break: normal">first</span><span style="word-break: break-all">second</span><span style="word-break: normal">third</span>

    // First find the overflowing run.
    auto nonOverflowingContentWidth = InlineLayoutUnit { };
    auto overflowingRunIndex = runs.size(); 
    for (size_t index = 0; index < runs.size(); ++index) {
        auto runLogicalWidth = runs[index].logicalWidth;
        if (nonOverflowingContentWidth + runLogicalWidth > lineStatus.availableWidth) {
            overflowingRunIndex = index;
            break;
        }
        nonOverflowingContentWidth += runLogicalWidth;
    }
    // We have to have an overflowing run.
    RELEASE_ASSERT(overflowingRunIndex < runs.size());

    // Check first if we can actually break the overflowing run.
    if (auto breakingPosition = tryBreakingOverflowingRun(lineStatus, runs, overflowingRunIndex, nonOverflowingContentWidth))
        return { overflowingRunIndex, breakingPosition };

    auto& overflowingInlineItem = runs[overflowingRunIndex].inlineItem;
    // In some cases we just can't break before certain overflowing runs due to content specific CSS rules, e.g. line-break: after-white-space.
    // This is in addition to having soft wrap opportunties only after the whitespace. This is about not breaking at all
    // before the whitespace content e.g.
    // <div style="line-break: after-white-space; word-wrap: break-word">before<span style="white-space: pre">   </span>after</div>
    // "before" content is not breakable sine it is _before_ the overflowing whitespace content.
    auto isBreakingAllowedBeforeOverflowingRun = !is<InlineTextItem>(overflowingInlineItem)
        || !downcast<InlineTextItem>(overflowingInlineItem).isWhitespace()
        || overflowingInlineItem.style().lineBreak() != LineBreak::AfterWhiteSpace;
    if (isBreakingAllowedBeforeOverflowingRun) {
        // We did not manage to break the run that overflows the line.
        // Let's try to find a previous breaking position starting from the overflowing run. It surely fits.
        if (auto breakingPosition = tryBreakingPreviousNonOverflowingRuns(lineStatus, runs, overflowingRunIndex, nonOverflowingContentWidth))
            return { overflowingRunIndex, breakingPosition };
    }

    // At this point we know that there's no breakable run all the way to the overflowing run.
    // Now we need to check if any run after the overflowing content can break.
    // e.g. <span>this_content_overflows_but_not_breakable<span><span style="word-break: break-all">but_this_is_breakable</span>
    if (auto breakingPosition = tryBreakingNextOverflowingRuns(lineStatus, runs, overflowingRunIndex, nonOverflowingContentWidth))
        return { overflowingRunIndex, breakingPosition };

    // Give up, there's no breakable run in here.
    return { overflowingRunIndex };
}

OptionSet<InlineContentBreaker::WordBreakRule> InlineContentBreaker::wordBreakBehavior(const RenderStyle& style, bool hasWrapOpportunityAtPreviousPosition) const
{
    // Disregard any prohibition against line breaks mandated by the word-break property.
    // The different wrapping opportunities must not be prioritized.
    // Note hyphenation is not applied.
    if (style.lineBreak() == LineBreak::Anywhere)
        return { WordBreakRule::AtArbitraryPosition };

    // Breaking is allowed within “words”.
    if (style.wordBreak() == WordBreak::BreakAll)
        return { WordBreakRule::AtArbitraryPositionWithinWords };

    auto includeHyphenationIfAllowed = [&](std::optional<InlineContentBreaker::WordBreakRule> wordBreakRule) -> OptionSet<InlineContentBreaker::WordBreakRule> {
        auto hyphenationIsAllowed = !n_hyphenationIsDisabled && style.hyphens() == Hyphens::Auto && canHyphenate(style.computedLocale());
        if (hyphenationIsAllowed) {
            if (wordBreakRule)
                return { *wordBreakRule, WordBreakRule::AtHyphenationOpportunities };
            return { WordBreakRule::AtHyphenationOpportunities };
        }
        if (wordBreakRule)
            return *wordBreakRule;
        return { };
    };

    // For compatibility with legacy content, the word-break property also supports a deprecated break-word keyword.
    // When specified, this has the same effect as word-break: normal and overflow-wrap: anywhere, regardless of the actual value of the overflow-wrap property.
    if (style.wordBreak() == WordBreak::BreakWord && !hasWrapOpportunityAtPreviousPosition)
        return includeHyphenationIfAllowed(WordBreakRule::AtArbitraryPosition);
    // OverflowWrap::BreakWord/Anywhere An otherwise unbreakable sequence of characters may be broken at an arbitrary point if there are no otherwise-acceptable break points in the line.
    // Note that this applies to content where CSS properties (e.g. WordBreak::KeepAll) make it unbreakable. 
    // Soft wrap opportunities introduced by overflow-wrap/word-wrap: break-word are not considered when calculating min-content intrinsic sizes.
    auto overflowWrapBreakWordIsApplicable = !isInIntrinsicWidthMode();
    if (((overflowWrapBreakWordIsApplicable && style.overflowWrap() == OverflowWrap::BreakWord) || style.overflowWrap() == OverflowWrap::Anywhere) && !hasWrapOpportunityAtPreviousPosition)
        return includeHyphenationIfAllowed(WordBreakRule::AtArbitraryPosition);
    // Breaking is forbidden within “words”.
    if (style.wordBreak() == WordBreak::KeepAll)
        return { };
    return includeHyphenationIfAllowed({ });
}

void InlineContentBreaker::ContinuousContent::appendToRunList(const InlineItem& inlineItem, const RenderStyle& style, InlineLayoutUnit logicalWidth)
{
    m_runs.append({ inlineItem, style, logicalWidth });
    m_logicalWidth = clampTo<InlineLayoutUnit>(m_logicalWidth + logicalWidth);
}

void InlineContentBreaker::ContinuousContent::resetTrailingTrimmableContent()
{
    if (!m_leadingTrimmableWidth)
        m_leadingTrimmableWidth = m_trailingTrimmableWidth;
    m_trailingTrimmableWidth = { };
}

void InlineContentBreaker::ContinuousContent::append(const InlineItem& inlineItem, const RenderStyle& style, InlineLayoutUnit logicalWidth)
{
    ASSERT(inlineItem.isBox() || inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd());
    appendToRunList(inlineItem, style, logicalWidth);
    if (inlineItem.isBox()) {
        // Inline boxes (whitespace-> <span></span>) do not prevent the trailing content from getting trimmed/hung
        // but atomic inline level boxes do.
        resetTrailingTrimmableContent();
    }
}

void InlineContentBreaker::ContinuousContent::append(const InlineTextItem& inlineTextItem, const RenderStyle& style, InlineLayoutUnit logicalWidth, std::optional<InlineLayoutUnit> trimmableWidth)
{
    if (!trimmableWidth) {
        appendToRunList(inlineTextItem, style, logicalWidth);
        resetTrailingTrimmableContent();
        return;
    }

    ASSERT(*trimmableWidth <= logicalWidth);
    auto isLeadingTrimmable = trimmableWidth && (!this->logicalWidth() || isFullyTrimmable());
    appendToRunList(inlineTextItem, style, logicalWidth);
    if (isLeadingTrimmable) {
        ASSERT(!m_trailingTrimmableWidth);
        m_leadingTrimmableWidth = m_leadingTrimmableWidth.value_or(0.f) + *trimmableWidth;
        return;
    }
    m_trailingTrimmableWidth = *trimmableWidth == logicalWidth ? m_trailingTrimmableWidth.value_or(0.f) + logicalWidth : *trimmableWidth;
}

void InlineContentBreaker::ContinuousContent::append(const InlineTextItem& inlineTextItem, const RenderStyle& style, InlineLayoutUnit hangingWidth)
{
    appendToRunList(inlineTextItem, style, hangingWidth);
    m_trailingHangingContentWidth = hangingWidth;
    resetTrailingTrimmableContent();
}

void InlineContentBreaker::ContinuousContent::reset()
{
    m_logicalWidth = { };
    m_leadingTrimmableWidth = { };
    m_trailingTrimmableWidth = { };
    m_trailingHangingContentWidth = { };
    m_runs.clear();
}
}
}
