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
#include "RuntimeEnabledFeatures.h"
#include "TextUtil.h"

namespace WebCore {
namespace Layout {

static inline bool isWrappingAllowed(const RenderStyle& style)
{
    // Do not try to push overflown 'pre' and 'no-wrap' content to next line.
    return style.whiteSpace() != WhiteSpace::Pre && style.whiteSpace() != WhiteSpace::NoWrap;
}

static inline bool shouldKeepBeginningOfLineWhitespace(const RenderStyle& style)
{
    auto whitespace = style.whiteSpace();
    return whitespace == WhiteSpace::Pre || whitespace == WhiteSpace::PreWrap || whitespace == WhiteSpace::BreakSpaces;
}

static inline Optional<size_t> lastWrapOpportunityIndex(const LineBreaker::RunList& runList)
{
    // <span style="white-space: pre">no_wrap</span><span>yes wrap</span><span style="white-space: pre">no_wrap</span>.
    // [container start][no_wrap][container end][container start][yes] <- continuous content
    // [ ] <- continuous content
    // [wrap][container end][container start][no_wrap][container end] <- continuous content
    // Return #0 as the index where the second continuous content can wrap at.
    ASSERT(!runList.isEmpty());
    auto lastItemIndex = runList.size() - 1;
    return isWrappingAllowed(runList[lastItemIndex].inlineItem.style()) ? makeOptional(lastItemIndex) : WTF::nullopt;
}

struct ContinuousContent {
    ContinuousContent(const LineBreaker::RunList&, InlineLayoutUnit contentLogicalWidth);

    const LineBreaker::RunList& runs() const { return m_runs; }
    bool isEmpty() const { return m_runs.isEmpty(); }
    bool hasTextContentOnly() const;
    bool isVisuallyEmptyWhitespaceContentOnly() const;
    bool hasNonContentRunsOnly() const;
    size_t size() const { return m_runs.size(); }
    InlineLayoutUnit width() const { return m_width; }
    InlineLayoutUnit nonCollapsibleWidth() const { return m_width - m_trailingCollapsibleContent.width; }

    bool hasTrailingCollapsibleContent() const { return !!m_trailingCollapsibleContent.width; }
    bool isTrailingContentFullyCollapsible() const { return m_trailingCollapsibleContent.isFullyCollapsible; }

    Optional<size_t> firstTextRunIndex() const;
    Optional<size_t> lastContentRunIndex() const;

private:
    const LineBreaker::RunList& m_runs;
    struct TrailingCollapsibleContent {
        void reset();

        bool isFullyCollapsible { false };
        InlineLayoutUnit width { 0 };
    };
    TrailingCollapsibleContent m_trailingCollapsibleContent;
    InlineLayoutUnit m_width { 0 };
};

struct WrappedTextContent {
    unsigned trailingRunIndex { 0 };
    bool contentOverflows { false };
    Optional<LineBreaker::PartialRun> partialTrailingRun;
};

bool LineBreaker::isContentWrappingAllowed(const ContinuousContent& candidateRuns) const
{
    // Use the last inline item with content (where we would be wrapping) to decide if content wrapping is allowed.
    auto runIndex = candidateRuns.lastContentRunIndex().valueOr(candidateRuns.size() - 1);
    return isWrappingAllowed(candidateRuns.runs()[runIndex].inlineItem.style());
}

bool LineBreaker::shouldKeepEndOfLineWhitespace(const ContinuousContent& candidateRuns) const
{
    // Grab the style and check for white-space property to decided whether we should let this whitespace content overflow the current line.
    // Note that the "keep" in the context means we let the whitespace content sit on the current line.
    // It might very well get collapsed when we close the line (normal/nowrap/pre-line).
    // See https://www.w3.org/TR/css-text-3/#white-space-property
    auto whitespace = candidateRuns.runs()[*candidateRuns.firstTextRunIndex()].inlineItem.style().whiteSpace();
    return whitespace == WhiteSpace::Normal || whitespace == WhiteSpace::NoWrap || whitespace == WhiteSpace::PreWrap || whitespace == WhiteSpace::PreLine;
}

LineBreaker::Result LineBreaker::shouldWrapInlineContent(const RunList& candidateRuns, InlineLayoutUnit candidateContentLogicalWidth, const LineStatus& lineStatus)
{
    auto inlineContentWrapping = [&] {
        if (candidateContentLogicalWidth <= lineStatus.availableWidth)
            return Result { Result::Action::Keep };
#if USE_FLOAT_AS_INLINE_LAYOUT_UNIT
        // Preferred width computation sums up floats while line breaker substracts them. This can lead to epsilon-scale differences.
        if (WTF::areEssentiallyEqual(candidateContentLogicalWidth, lineStatus.availableWidth))
            return Result { Result::Action::Keep };
#endif
        return tryWrappingInlineContent(candidateRuns, candidateContentLogicalWidth, lineStatus);
    };

    auto result = inlineContentWrapping();
    if (result.action == Result::Action::Keep) {
        // If this is not the end of the line, hold on to the last eligible line wrap opportunity so that we could revert back
        // to this position if no other line breaking opportunity exists in this content.
        if (auto lastLineWrapOpportunityIndex = lastWrapOpportunityIndex(candidateRuns)) {
            auto isEligibleLineWrapOpportunity = [&] (auto& candidateItem) {
                // Just check for leading collapsible whitespace for now.
                if (!lineStatus.lineIsEmpty || !candidateItem.isText() || !downcast<InlineTextItem>(candidateItem).isWhitespace())
                    return true;
                return shouldKeepBeginningOfLineWhitespace(candidateItem.style());
            };
            auto& lastWrapOpportunityCandidateItem = candidateRuns[*lastLineWrapOpportunityIndex].inlineItem;
            if (isEligibleLineWrapOpportunity(lastWrapOpportunityCandidateItem)) {
                result.lastWrapOpportunityItem = &lastWrapOpportunityCandidateItem;
                m_hasWrapOpportunityAtPreviousPosition = true;
            }
        }
    }
    return result;
}

LineBreaker::Result LineBreaker::tryWrappingInlineContent(const RunList& candidateRuns, InlineLayoutUnit candidateContentLogicalWidth, const LineStatus& lineStatus) const
{
    auto candidateContent = ContinuousContent { candidateRuns, candidateContentLogicalWidth };
    ASSERT(!candidateContent.isEmpty());

    ASSERT(candidateContent.width() > lineStatus.availableWidth);
    if (candidateContent.hasTrailingCollapsibleContent()) {
        ASSERT(candidateContent.hasTextContentOnly());
        auto IsEndOfLine = isContentWrappingAllowed(candidateContent) ? IsEndOfLine::Yes : IsEndOfLine::No;
        // First check if the content fits without the trailing collapsible part.
        if (candidateContent.nonCollapsibleWidth() <= lineStatus.availableWidth)
            return { Result::Action::Keep, IsEndOfLine };
        // Now check if we can trim the line too.
        if (lineStatus.lineHasFullyCollapsibleTrailingRun && candidateContent.isTrailingContentFullyCollapsible()) {
            // If this new content is fully collapsible, it should surely fit.
            return { Result::Action::Keep, IsEndOfLine };
        }
    } else if (lineStatus.collapsibleWidth && candidateContent.hasNonContentRunsOnly()) {
        // Let's see if the non-content runs fit when the line has trailing collapsible content.
        // "text content <span style="padding: 1px"></span>" <- the <span></span> runs could fit after collapsing the trailing whitespace.
        if (candidateContent.width() <= lineStatus.availableWidth + lineStatus.collapsibleWidth)
            return { Result::Action::Keep };
    }
    if (candidateContent.isVisuallyEmptyWhitespaceContentOnly() && shouldKeepEndOfLineWhitespace(candidateContent)) {
        // This overflowing content apparently falls into the remove/hang end-of-line-spaces category.
        // see https://www.w3.org/TR/css-text-3/#white-space-property matrix
        return { Result::Action::Keep };
    }

    if (candidateContent.hasTextContentOnly()) {
        auto& runs = candidateContent.runs();
        if (auto wrappedTextContent = wrapTextContent(runs, lineStatus)) {
            if (!wrappedTextContent->trailingRunIndex && wrappedTextContent->contentOverflows) {
                // We tried to split the content but the available space can't even accommodate the first character.
                // 1. Push the content over to the next line when we've got content on the line already.
                // 2. Keep the first character on the empty line (or keep the whole run if it has only one character).
                if (!lineStatus.lineIsEmpty)
                    return { Result::Action::Push, IsEndOfLine::Yes, { } };
                auto firstTextRunIndex = *candidateContent.firstTextRunIndex();
                auto& inlineTextItem = downcast<InlineTextItem>(runs[firstTextRunIndex].inlineItem);
                ASSERT(inlineTextItem.length());
                if (inlineTextItem.length() == 1)
                    return Result { Result::Action::Keep, IsEndOfLine::Yes };
                auto firstCharacterWidth = TextUtil::width(inlineTextItem, inlineTextItem.start(), inlineTextItem.start() + 1);
                auto firstCharacterRun = PartialRun { 1, firstCharacterWidth, false };
                return { Result::Action::Split, IsEndOfLine::Yes, Result::PartialTrailingContent { firstTextRunIndex, firstCharacterRun } };
            }
            auto splitContent = Result::PartialTrailingContent { wrappedTextContent->trailingRunIndex, wrappedTextContent->partialTrailingRun };
            return { Result::Action::Split, IsEndOfLine::Yes, splitContent };
        }
    }
    // If we are not allowed to break this overflowing content, we still need to decide whether keep it or push it to the next line.
    if (lineStatus.lineIsEmpty) {
        ASSERT(!m_hasWrapOpportunityAtPreviousPosition);
        return { Result::Action::Keep, IsEndOfLine::No };
    }
    // Now either wrap here or at an earlier position, or not wrap at all.
    if (isContentWrappingAllowed(candidateContent))
        return { Result::Action::Push, IsEndOfLine::Yes };
    if (m_hasWrapOpportunityAtPreviousPosition)
        return { Result::Action::RevertToLastWrapOpportunity, IsEndOfLine::Yes };
    return { Result::Action::Keep, IsEndOfLine::No };
}

Optional<WrappedTextContent> LineBreaker::wrapTextContent(const RunList& runs, const LineStatus& lineStatus) const
{
    auto isContentSplitAllowed = [] (auto& run) {
        ASSERT(run.inlineItem.isText() || run.inlineItem.isContainerStart() || run.inlineItem.isContainerEnd());
        if (!run.inlineItem.isText()) {
            // Can't split horizontal spacing -> e.g. <span style="padding-right: 100px;">textcontent</span>, if the [container end] is the overflown inline item
            // we need to check if there's another inline item beyond the [container end] to split.
            return false;
        }
        return isWrappingAllowed(run.inlineItem.style());
    };

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
            if (auto partialRun = tryBreakingTextRun(run, adjustedAvailableWidth)) {
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
            if (auto partialRun = tryBreakingTextRun(run, maxInlineLayoutUnit())) {
                 // We know this run fits, so if wrapping is allowed on the run, it should return a non-empty left-side.
                 ASSERT(partialRun->length);
                 return WrappedTextContent { index, false, partialRun };
            }
        }
    }
    // Give up, there's no breakable run in here.
    return { };
}

LineBreaker::WordBreakRule LineBreaker::wordBreakBehavior(const RenderStyle& style) const
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
    if (style.wordBreak() == WordBreak::BreakWord && !m_hasWrapOpportunityAtPreviousPosition)
        return WordBreakRule::AtArbitraryPosition;
    // OverflowWrap::Break: An otherwise unbreakable sequence of characters may be broken at an arbitrary point if there are no otherwise-acceptable break points in the line.
    if (style.overflowWrap() == OverflowWrap::Break && !m_hasWrapOpportunityAtPreviousPosition)
        return WordBreakRule::AtArbitraryPosition;

    if (!n_hyphenationIsDisabled && style.hyphens() == Hyphens::Auto && canHyphenate(style.computedLocale()))
        return WordBreakRule::OnlyHyphenationAllowed;

    return WordBreakRule::NoBreak;
}

Optional<LineBreaker::PartialRun> LineBreaker::tryBreakingTextRun(const Run& overflowRun, InlineLayoutUnit availableWidth) const
{
    ASSERT(overflowRun.inlineItem.isText());
    auto& inlineTextItem = downcast<InlineTextItem>(overflowRun.inlineItem);
    auto& style = inlineTextItem.style();
    auto findLastBreakablePosition = availableWidth == maxInlineLayoutUnit();

    auto breakRule = wordBreakBehavior(style);
    if (breakRule == WordBreakRule::AtArbitraryPosition) {
        if (findLastBreakablePosition) {
            // When the run can be split at arbitrary position,
            // let's just return the entire run when it is intended to fit on the line.
            return PartialRun { inlineTextItem.length(), overflowRun.logicalWidth, false };
        }
        // FIXME: Pass in the content logical left to be able to measure tabs.
        auto splitData = TextUtil::split(inlineTextItem.inlineTextBox(), inlineTextItem.start(), inlineTextItem.length(), overflowRun.logicalWidth, availableWidth, { });
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
            leftSideLength = TextUtil::split(inlineTextItem.inlineTextBox(), inlineTextItem.start(), runLength, overflowRun.logicalWidth, availableWidthExcludingHyphen, { }).length;
        }
        if (leftSideLength < limitBefore)
            return { };
        // Adjust before index to accommodate the limit-after value (it's the last potential hyphen location in this run).
        auto hyphenBefore = std::min(leftSideLength, runLength - limitAfter) + 1;
        unsigned hyphenLocation = lastHyphenLocation(StringView(inlineTextItem.inlineTextBox().content()).substring(inlineTextItem.start(), inlineTextItem.length()), hyphenBefore, style.computedLocale());
        if (!hyphenLocation || hyphenLocation < limitBefore)
            return { };
        // hyphenLocation is relative to the start of this InlineItemText.
        auto trailingPartialRunWidthWithHyphen = TextUtil::width(inlineTextItem, inlineTextItem.start(), inlineTextItem.start() + hyphenLocation) + hyphenWidth; 
        return PartialRun { hyphenLocation, trailingPartialRunWidthWithHyphen, true };
    }

    ASSERT(breakRule == WordBreakRule::NoBreak);
    return { };
}

ContinuousContent::ContinuousContent(const LineBreaker::RunList& runs, InlineLayoutUnit contentLogicalWidth)
    : m_runs(runs)
    , m_width(contentLogicalWidth)
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
            if (!RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled()) {
                // A run with trailing letter spacing is partially collapsible.
                if (auto collapsibleWidth = inlineTextItem.style().letterSpacing()) {
                    m_trailingCollapsibleContent.width += collapsibleWidth;
                    m_trailingCollapsibleContent.isFullyCollapsible = false;
                }
            }
            // End of whitespace content.
            break;
        }
    }
}

bool ContinuousContent::hasTextContentOnly() const
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

bool ContinuousContent::isVisuallyEmptyWhitespaceContentOnly() const
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

Optional<size_t> ContinuousContent::firstTextRunIndex() const
{
    for (size_t index = 0; index < m_runs.size(); ++index) {
        if (m_runs[index].inlineItem.isText())
            return index;
    }
    return { };
}

Optional<size_t> ContinuousContent::lastContentRunIndex() const
{
    for (size_t index = m_runs.size(); index--;) {
        if (m_runs[index].inlineItem.isText() || m_runs[index].inlineItem.isBox())
            return index;
    }
    return { };
}

bool ContinuousContent::hasNonContentRunsOnly() const
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

void ContinuousContent::TrailingCollapsibleContent::reset()
{
    isFullyCollapsible = false;
    width = 0_lu;
}

}
}
#endif
