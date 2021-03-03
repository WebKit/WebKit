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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FontCascade.h"
#include "Hyphenation.h"
#include "InlineItem.h"
#include "InlineTextItem.h"
#include "LayoutContainerBox.h"
#include "TextUtil.h"

namespace WebCore {
namespace Layout {


#if ASSERT_ENABLED
static inline bool hasTrailingTextContent(const InlineContentBreaker::ContinuousContent& continuousContent)
{
    for (auto& run : WTF::makeReversedRange(continuousContent.runs())) {
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
    // <span>text</span> is considered a text run even with the [container start][container end] inline items.
    // Based on standards commit boundary rules it would be enough to check the first inline item, but due to the table quirk, we can have
    // image and text next to each other inside a continuous set of runs (see InlineFormattingContext::Quirks::hasSoftWrapOpportunityAtImage).
    for (auto& run : continuousContent.runs()) {
        if (run.inlineItem.isText())
            return true;
    }
    return false;
}

static inline bool isVisuallyEmptyWhitespaceContent(const InlineContentBreaker::ContinuousContent& continuousContent)
{
    // [<span></span> ] [<span> </span>] [ <span style="padding: 0px;"></span>] are all considered visually empty whitespace content.
    // [<span style="border: 1px solid red"></span> ] while this is whitespace content only, it is not considered visually empty.
    // Due to commit boundary rules, we just need to check the first non-typeless inline item (can't have both [img] and [text])
    for (auto& run : continuousContent.runs()) {
        auto& inlineItem = run.inlineItem;
        // FIXME: check for padding border etc.
        if (inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd())
            continue;
        return inlineItem.isText() && downcast<InlineTextItem>(inlineItem).isWhitespace();
    }
    return false;
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

static inline Optional<size_t> firstTextRunIndex(const InlineContentBreaker::ContinuousContent& continuousContent)
{
    auto& runs = continuousContent.runs();
    for (size_t index = 0; index < runs.size(); ++index) {
        if (runs[index].inlineItem.isText())
            return index;
    }
    return { };
}

bool InlineContentBreaker::isWrappingAllowed(const InlineItem& inlineItem)
{
    auto& styleToUse = inlineItem.isBox() ? inlineItem.layoutBox().parent().style() : inlineItem.layoutBox().style(); 
    // Do not try to wrap overflown 'pre' and 'no-wrap' content to next line.
    return styleToUse.whiteSpace() != WhiteSpace::Pre && styleToUse.whiteSpace() != WhiteSpace::NoWrap;
}

bool InlineContentBreaker::shouldKeepEndOfLineWhitespace(const ContinuousContent& continuousContent) const
{
    // Grab the style and check for white-space property to decide whether we should let this whitespace content overflow the current line.
    // Note that the "keep" in this context means we let the whitespace content sit on the current line.
    // It might very well get collapsed when we close the line (normal/nowrap/pre-line).
    // See https://www.w3.org/TR/css-text-3/#white-space-property
    auto whitespace = continuousContent.runs()[*firstTextRunIndex(continuousContent)].inlineItem.style().whiteSpace();
    return whitespace == WhiteSpace::Normal || whitespace == WhiteSpace::NoWrap || whitespace == WhiteSpace::PreWrap || whitespace == WhiteSpace::PreLine;
}

InlineContentBreaker::Result InlineContentBreaker::processInlineContent(const ContinuousContent& candidateContent, const LineStatus& lineStatus)
{
    auto processCandidateContent = [&] {
        if (candidateContent.logicalWidth() <= lineStatus.availableWidth)
            return Result { Result::Action::Keep };
#if USE_FLOAT_AS_INLINE_LAYOUT_UNIT
        // Preferred width computation sums up floats while line breaker subtracts them. This can lead to epsilon-scale differences.
        if (WTF::areEssentiallyEqual(candidateContent.logicalWidth(), lineStatus.availableWidth))
            return Result { Result::Action::Keep };
#endif
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

struct OverflowingTextContent {
    size_t runIndex { 0 }; // Overflowing run index. There's always an overflowing run.
    struct BreakingPosition {
        size_t runIndex { 0 };
        struct TrailingContent {
            // Trailing content is either the run's left side (when we break the run somewhere in the middle) or the previous run.
            // Sometimes the breaking position is at the very beginning of the first run, so there's no trailing run at all.
            bool overflows { false };
            Optional<InlineContentBreaker::PartialRun> partialRun { };
        };
        Optional<TrailingContent> trailingContent { };
    };
    Optional<BreakingPosition> breakingPosition { }; // Where we actually break this overflowing content.
};

InlineContentBreaker::Result InlineContentBreaker::processOverflowingContent(const ContinuousContent& overflowContent, const LineStatus& lineStatus) const
{
    auto continuousContent = ContinuousContent { overflowContent };
    ASSERT(!continuousContent.runs().isEmpty());

    ASSERT(continuousContent.logicalWidth() > lineStatus.availableWidth);
    if (continuousContent.hasTrailingCollapsibleContent()) {
        ASSERT(hasTrailingTextContent(overflowContent));
        // First check if the content fits without the trailing collapsible part.
        if (continuousContent.nonCollapsibleLogicalWidth() <= lineStatus.availableWidth)
            return { Result::Action::Keep, IsEndOfLine::No };
        // Now check if we can trim the line too.
        if (lineStatus.hasFullyCollapsibleTrailingRun && continuousContent.isFullyCollapsible()) {
            // If this new content is fully collapsible, it should surely fit.
            return { Result::Action::Keep, IsEndOfLine::No };
        }
    } else if (lineStatus.collapsibleWidth && isNonContentRunsOnly(continuousContent)) {
        // Let's see if the non-content runs fit when the line has trailing collapsible content.
        // "text content <span style="padding: 1px"></span>" <- the <span></span> runs could fit after collapsing the trailing whitespace.
        if (continuousContent.logicalWidth() <= lineStatus.availableWidth + lineStatus.collapsibleWidth)
            return { Result::Action::Keep };
    }
    if (isVisuallyEmptyWhitespaceContent(continuousContent) && shouldKeepEndOfLineWhitespace(continuousContent)) {
        // This overflowing content apparently falls into the remove/hang end-of-line-spaces category.
        // see https://www.w3.org/TR/css-text-3/#white-space-property matrix
        return { Result::Action::Keep };
    }

    size_t overflowingRunIndex = 0;
    if (hasTextRun(continuousContent)) {
        auto tryBreakingContentWithText = [&]() -> Optional<Result> {
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
                // 2. Keep the first glyph on the empty line (or keep the whole run if it has only one glyph/completely empty).
                if (lineStatus.hasContent)
                    return Result { Result::Action::Wrap, IsEndOfLine::Yes };
                auto leadingTextRunIndex = *firstTextRunIndex(continuousContent);
                auto& inlineTextItem = downcast<InlineTextItem>(continuousContent.runs()[leadingTextRunIndex].inlineItem);
                if (inlineTextItem.length() <= 1)
                    return Result { Result::Action::Keep, IsEndOfLine::Yes };
                auto firstCharacterWidth = TextUtil::width(inlineTextItem, inlineTextItem.start(), inlineTextItem.start() + 1, lineStatus.contentLogicalRight);
                auto firstCharacterRun = PartialRun { 1, firstCharacterWidth };
                return Result { Result::Action::Break, IsEndOfLine::Yes, Result::PartialTrailingContent { leadingTextRunIndex, firstCharacterRun } };
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
        return isWrappingAllowed(continuousContent.runs()[overflowingRunIndex].inlineItem);
    };
    if (shouldWrapUnbreakableContentToNextLine())
        return { Result::Action::Wrap, IsEndOfLine::Yes };
    if (lineStatus.hasWrapOpportunityAtPreviousPosition)
        return { Result::Action::RevertToLastWrapOpportunity, IsEndOfLine::Yes };
    return { Result::Action::Keep, IsEndOfLine::No };
}

OverflowingTextContent InlineContentBreaker::processOverflowingContentWithText(const ContinuousContent& continuousContent, const LineStatus& lineStatus) const
{
    auto& runs = continuousContent.runs();
    ASSERT(!runs.isEmpty());

    auto isBreakableRun = [] (auto& run) {
        ASSERT(run.inlineItem.isText() || run.inlineItem.isInlineBoxStart() || run.inlineItem.isInlineBoxEnd() || run.inlineItem.layoutBox().isImage());
        if (!run.inlineItem.isText()) {
            // Can't break horizontal spacing -> e.g. <span style="padding-right: 100px;">textcontent</span>, if the [container end] is the overflown inline item
            // we need to check if there's another inline item beyond the [container end] to split.
            return false;
        }
        // Check if this text run needs to stay on the current line.  
        return isWrappingAllowed(run.inlineItem);
    };

    auto findTrailingRunIndex = [&] (auto breakableRunIndex) -> Optional<size_t> {
        // When the breaking position is at the beginning of the run, the trailing run is the previous one.
        if (!breakableRunIndex)
            return { };
        // Try not break content at inline box boundary
        // e.g. <span>fits</span><span>overflows</span>
        // when the text "overflows" completely overflows, let's break the content right before the '<span>'.
        // FIXME: Add support for subsequent empty inline boxes e.g.
        auto trailingCandidateIndex = breakableRunIndex - 1;
        auto isAtInlineBox = runs[trailingCandidateIndex].inlineItem.isInlineBoxStart();
        return !isAtInlineBox ? trailingCandidateIndex : trailingCandidateIndex ? makeOptional(trailingCandidateIndex - 1) : WTF::nullopt;
    };

    // Check where the overflow occurs and use the corresponding style to figure out the breaking behaviour.
    // <span style="word-break: normal">first</span><span style="word-break: break-all">second</span><span style="word-break: normal">third</span>

    // First find the overflowing run. 
    auto accumulatedContentWidth = InlineLayoutUnit { };
    auto overflowingRunIndex = runs.size(); 
    for (size_t index = 0; index < runs.size(); ++index) {
        auto runLogicalWidth = runs[index].logicalWidth;
        if (accumulatedContentWidth + runLogicalWidth  > lineStatus.availableWidth) {
            overflowingRunIndex = index;
            break;
        }
        accumulatedContentWidth += runLogicalWidth;
    }
    // We have to have an overflowing run.
    RELEASE_ASSERT(overflowingRunIndex < runs.size());

    auto tryBreakingOverflowingRun = [&]() -> Optional<OverflowingTextContent::BreakingPosition> {
        auto overflowingRun = runs[overflowingRunIndex];
        if (!isBreakableRun(overflowingRun))
            return { };
        if (auto partialRun = tryBreakingTextRun(overflowingRun, lineStatus.contentLogicalRight + accumulatedContentWidth, std::max(0.0f, lineStatus.availableWidth - accumulatedContentWidth), lineStatus.hasWrapOpportunityAtPreviousPosition)) {
            if (partialRun->length)
                return OverflowingTextContent::BreakingPosition { overflowingRunIndex, OverflowingTextContent::BreakingPosition::TrailingContent { false, partialRun } };
            // When the breaking position is at the beginning of the run, the trailing run is the previous one.
            if (auto trailingRunIndex = findTrailingRunIndex(overflowingRunIndex))
                return OverflowingTextContent::BreakingPosition { *trailingRunIndex, OverflowingTextContent::BreakingPosition::TrailingContent { } };
            // Sometimes we can't accommodate even the very first character. 
            // Note that this is different from when there's no breakable run in this set.
            return OverflowingTextContent::BreakingPosition { };
        }
        return { };
    };
    // Check if we actually break this run.
    if (auto breakingPosition = tryBreakingOverflowingRun())
        return { overflowingRunIndex, breakingPosition };

    auto tryBreakingPreviousNonOverflowingRuns = [&]() -> Optional<OverflowingTextContent::BreakingPosition> {
        auto previousContentWidth = accumulatedContentWidth;
        for (auto index = overflowingRunIndex; index--;) {
            auto& run = runs[index];
            previousContentWidth -= run.logicalWidth;
            if (!isBreakableRun(run))
                continue;
            ASSERT(run.inlineItem.isText());
            if (auto partialRun = tryBreakingTextRun(run, lineStatus.contentLogicalRight + previousContentWidth, { }, lineStatus.hasWrapOpportunityAtPreviousPosition)) {
                // We know this run fits, so if breaking is allowed on the run, it should return a non-empty left-side
                // since it's either at hyphen position or the entire run is returned.
                ASSERT(partialRun->length);
                auto runIsFullyAccommodated = partialRun->length == downcast<InlineTextItem>(run.inlineItem).length();
                return OverflowingTextContent::BreakingPosition { index, OverflowingTextContent::BreakingPosition::TrailingContent { false, runIsFullyAccommodated ? WTF::nullopt : partialRun } };
            }
        }
        return { };
    };
    // We did not manage to break the run that actually overflows the line.
    // Let's try to find a previous breaking position starting from the overflowing run. It surely fits.
    if (auto breakingPosition = tryBreakingPreviousNonOverflowingRuns())
        return { overflowingRunIndex, breakingPosition };

    auto tryBreakingNextOverflowingRuns = [&]() -> Optional<OverflowingTextContent::BreakingPosition> {
        auto nextContentWidth = accumulatedContentWidth + runs[overflowingRunIndex].logicalWidth;
        for (auto index = overflowingRunIndex + 1; index < runs.size(); ++index) {
            auto& run = runs[index];
            if (isBreakableRun(run)) {
                ASSERT(run.inlineItem.isText());
                // We know that this run does not fit the available space. If we can break it at any position, let's just use the start of the run.
                if (wordBreakBehavior(run.inlineItem.style(), lineStatus.hasWrapOpportunityAtPreviousPosition) == WordBreakRule::AtArbitraryPosition) {
                    // We must be on an inline box boundary. Let's go back to the run in front of the inline box start run.
                    // e.g. <span>unbreakable_and_overflow<span style="word-break: break-all">breakable</span>
                    // We are at "breakable", <span> is at index - 1 and the trailing run is at index - 2.
                    ASSERT(runs[index - 1].inlineItem.isInlineBoxStart());
                    auto trailingRunIndex = findTrailingRunIndex(index);
                    if (!trailingRunIndex) {
                        // This continuous content did not fit from the get-go. No trailing run.
                        return OverflowingTextContent::BreakingPosition { };
                    }
                    // At worst we are back to the overflowing run, like in the example above.
                    ASSERT(*trailingRunIndex >= overflowingRunIndex);
                    return OverflowingTextContent::BreakingPosition { *trailingRunIndex, OverflowingTextContent::BreakingPosition::TrailingContent { true } };
                }
                if (auto partialRun = tryBreakingTextRun(run, lineStatus.contentLogicalRight + nextContentWidth, { }, lineStatus.hasWrapOpportunityAtPreviousPosition)) {
                    ASSERT(partialRun->length);
                    // We managed to break this text run mid content. It has to be a hyphen break.
                    return OverflowingTextContent::BreakingPosition { index, OverflowingTextContent::BreakingPosition::TrailingContent { true, partialRun } };
                }
            }
            nextContentWidth += run.logicalWidth;
        }
        return { };
    };
    // At this point we know that there's no breakable run all the way to the overflowing run.
    // Now we need to check if any run after the overflowing content can break.
    // e.g. <span>this_content_overflows_but_not_breakable<span><span style="word-break: break-all">but_this_is_breakable</span>
    if (auto breakingPosition = tryBreakingNextOverflowingRuns())
        return { overflowingRunIndex, breakingPosition };

    // Give up, there's no breakable run in here.
    return { overflowingRunIndex };
}

InlineContentBreaker::WordBreakRule InlineContentBreaker::wordBreakBehavior(const RenderStyle& style, bool hasWrapOpportunityAtPreviousPosition) const
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
    if (style.wordBreak() == WordBreak::BreakWord && !hasWrapOpportunityAtPreviousPosition)
        return WordBreakRule::AtArbitraryPosition;
    // OverflowWrap::Break: An otherwise unbreakable sequence of characters may be broken at an arbitrary point if there are no otherwise-acceptable break points in the line.
    if (style.overflowWrap() == OverflowWrap::Break && !hasWrapOpportunityAtPreviousPosition)
        return WordBreakRule::AtArbitraryPosition;

    if (!n_hyphenationIsDisabled && style.hyphens() == Hyphens::Auto && canHyphenate(style.computedLocale()))
        return WordBreakRule::OnlyHyphenationAllowed;

    return WordBreakRule::NoBreak;
}

Optional<InlineContentBreaker::PartialRun> InlineContentBreaker::tryBreakingTextRun(const ContinuousContent::Run& overflowingRun, InlineLayoutUnit logicalLeft, Optional<InlineLayoutUnit> availableWidth, bool hasWrapOpportunityAtPreviousPosition) const
{
    ASSERT(overflowingRun.inlineItem.isText());
    auto& inlineTextItem = downcast<InlineTextItem>(overflowingRun.inlineItem);
    auto& style = inlineTextItem.style();
    auto availableSpaceIsInfinite = !availableWidth.hasValue();

    auto breakRule = wordBreakBehavior(style, hasWrapOpportunityAtPreviousPosition);
    if (breakRule == WordBreakRule::AtArbitraryPosition) {
        if (!inlineTextItem.length()) {
            // Empty text runs may be breakable based on style, but in practice we can't really split them any further.
            return PartialRun { };
        }
        if (availableSpaceIsInfinite) {
            // When the run can be split at arbitrary position let's just return the entire run when it is intended to fit on the line.
            ASSERT(inlineTextItem.length());
            auto trailingPartialRunWidth = TextUtil::width(inlineTextItem, logicalLeft);
            return PartialRun { inlineTextItem.length(), trailingPartialRunWidth };
        }
        if (!*availableWidth) {
            // Fast path for cases when there's no room at all. The content is breakable but we don't have space for it.
            return PartialRun { };
        }
        auto splitData = TextUtil::split(inlineTextItem, overflowingRun.logicalWidth, *availableWidth, logicalLeft);
        return PartialRun { splitData.length, splitData.logicalWidth };
    }

    if (breakRule == WordBreakRule::OnlyHyphenationAllowed) {
        // Find the hyphen position as follows:
        // 1. Split the text by taking the hyphen width into account
        // 2. Find the last hyphen position before the split position
        if (!availableSpaceIsInfinite && !*availableWidth) {
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
        auto& fontCascade = style.fontCascade();
        auto hyphenWidth = InlineLayoutUnit { fontCascade.width(TextRun { StringView { style.hyphenString() } }) };
        if (!availableSpaceIsInfinite) {
            auto availableWidthExcludingHyphen = *availableWidth - hyphenWidth;
            if (availableWidthExcludingHyphen <= 0 || !enoughWidthForHyphenation(availableWidthExcludingHyphen, fontCascade.pixelSize()))
                return { };
            leftSideLength = TextUtil::split(inlineTextItem, overflowingRun.logicalWidth, availableWidthExcludingHyphen, logicalLeft).length;
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
        auto trailingPartialRunWidthWithHyphen = TextUtil::width(inlineTextItem, inlineTextItem.start(), inlineTextItem.start() + hyphenLocation, logicalLeft); 
        return PartialRun { hyphenLocation, trailingPartialRunWidthWithHyphen, hyphenWidth };
    }

    ASSERT(breakRule == WordBreakRule::NoBreak);
    return { };
}

void InlineContentBreaker::ContinuousContent::append(const InlineItem& inlineItem, InlineLayoutUnit logicalWidth, Optional<InlineLayoutUnit> collapsibleWidth)
{
    m_runs.append({ inlineItem, logicalWidth });
    m_logicalWidth = clampTo<InlineLayoutUnit>(m_logicalWidth + logicalWidth);
    if (!collapsibleWidth) {
        m_collapsibleLogicalWidth = { };
        return;
    }
    if (*collapsibleWidth == logicalWidth) {
        // Fully collapsible run.
        m_collapsibleLogicalWidth += logicalWidth;
        ASSERT(m_collapsibleLogicalWidth <= m_logicalWidth);
        return;
    }
    // Partially collapsible run.
    m_collapsibleLogicalWidth = *collapsibleWidth;
    ASSERT(m_collapsibleLogicalWidth <= m_logicalWidth);
}

void InlineContentBreaker::ContinuousContent::reset()
{
    m_logicalWidth = { };
    m_collapsibleLogicalWidth = { };
    m_runs.clear();
}

}
}
#endif
