/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "InlineLine.h"

#include "FontCascade.h"
#include "InlineFormattingContext.h"
#include "InlineSoftLineBreakItem.h"
#include "LayoutBoxGeometry.h"
#include "RenderStyleInlines.h"
#include "TextFlags.h"
#include "TextUtil.h"
#include <ranges>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace Layout {

Line::Line(const InlineFormattingContext& inlineFormattingContext)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_trimmableTrailingContent(m_runs)
{
}

void Line::initialize(const Vector<InlineItem, 1>& lineSpanningInlineBoxes, bool isFirstFormattedLine)
{
    m_isFirstFormattedLine = isFirstFormattedLine;
    m_inlineBoxListWithClonedDecorationEnd.clear();
    m_rubyAlignContentRightOffset = { };
    m_nonSpanningInlineLevelBoxCount = 0;
    m_hasNonDefaultBidiLevelRun = false;
    m_hasRubyContent = false;
    m_contentLogicalWidth = { };
    m_inlineBoxLogicalLeftStack.clear();
    m_runs.clear();
    resetTrailingContent();
    auto appendLineSpanningInlineBoxes = [&] {
        for (auto& inlineBoxStartItem : lineSpanningInlineBoxes) {
            if (inlineBoxStartItem.style().boxDecorationBreak() != BoxDecorationBreak::Clone)
                m_runs.append({ inlineBoxStartItem, lastRunLogicalRight(), { } });
            else {
                // https://drafts.csswg.org/css-break/#break-decoration
                // clone: Each box fragment is independently wrapped with the border, padding, and margin.
                auto& inlineBoxGeometry = formattingContext().geometryForBox(inlineBoxStartItem.layoutBox());
                auto marginBorderAndPaddingStart = inlineBoxGeometry.marginBorderAndPaddingStart();
                auto runLogicalLeft = lastRunLogicalRight();
                m_runs.append({ inlineBoxStartItem, runLogicalLeft, marginBorderAndPaddingStart });
                // Do not let negative margin make the content shorter than it already is.
                m_contentLogicalWidth = std::max(m_contentLogicalWidth, runLogicalLeft + marginBorderAndPaddingStart);
                m_inlineBoxListWithClonedDecorationEnd.append(&inlineBoxStartItem.layoutBox());
            }
        }
    };
    appendLineSpanningInlineBoxes();
}

void Line::resetTrailingContent()
{
    m_trimmableTrailingContent.reset();
    m_hangingContent.resetTrailingContent();
    m_trailingSoftHyphenWidth = { };
}

Line::Result Line::close()
{
    auto trailingClonedDecorationWidth = [&] {
        auto decorationWidth = InlineLayoutUnit { };
        for (auto* inlineBox : m_inlineBoxListWithClonedDecorationEnd) {
            auto& boxGeometry = formattingContext().geometryForBox(*inlineBox);
            decorationWidth += boxGeometry.borderEnd() + boxGeometry.paddingEnd();
        }
        return decorationWidth;
    }();

    auto contentLogicalRight = this->contentLogicalRight() + m_rubyAlignContentRightOffset;
    auto isContentful = lineHasVisuallyNonEmptyContent();
    return { WTFMove(m_runs)
        , contentLogicalWidth() + trailingClonedDecorationWidth
        , contentLogicalRight + trailingClonedDecorationWidth
        , isContentful
        , !!m_hangingContent.trailingWhitespaceLength()
        , m_hangingContent.trailingWidth()
        , m_hangingContent.leadingPunctuationWidth()
        , m_hasNonDefaultBidiLevelRun
        , m_nonSpanningInlineLevelBoxCount
    };
}

InlineLayoutUnit Line::handleTrailingTrimmableContent(TrailingContentAction trailingTrimmableContentAction)
{
    if (m_trimmableTrailingContent.isEmpty() || m_runs.isEmpty())
        return { };

    if (trailingTrimmableContentAction == TrailingContentAction::Preserve) {
        m_trimmableTrailingContent.reset();
        return { };
    }

    auto trimmedWidth = m_trimmableTrailingContent.remove();
    m_contentLogicalWidth -= trimmedWidth;
    return trimmedWidth;
}

void Line::handleOverflowingNonBreakingSpace(TrailingContentAction trailingContentAction, InlineLayoutUnit overflowingWidth)
{
    ASSERT(m_trimmableTrailingContent.isEmpty());
    ASSERT(overflowingWidth > 0);

    auto startIndex = [&] () -> size_t {
        auto trimmedContentWidth = InlineLayoutUnit { };
        for (auto index = m_runs.size(); index--;) {
            auto& run = m_runs[index];
            if (run.isLineBreak() || run.isInlineBox() || run.isWordBreakOpportunity() || run.isWordSeparator()) {
                // It's ok to trim/remove non-breaking spaces across inline boxes.
                continue;
            }
            if (!run.isNonBreakingSpace()) {
                // Only trim/remove trailing non-breaking space.
                return index;
            }
            trimmedContentWidth += run.logicalWidth();
            if (trimmedContentWidth >= overflowingWidth) {
                // This is how much non-breakable space needs to be removed or trimmed.
                return index;
            }
        }
        return 0;
    };

    auto index = startIndex();
    auto removedOrCollapsedContentWidth = InlineLayoutUnit { };
    while (index < m_runs.size()) {
        auto& run = m_runs[index];

        if (!run.isNonBreakingSpace()) {
            run.moveHorizontally(-removedOrCollapsedContentWidth);
            ++index;
            continue;
        }
        if (trailingContentAction == TrailingContentAction::Preserve) {
            run.moveHorizontally(-removedOrCollapsedContentWidth);
            removedOrCollapsedContentWidth += run.logicalWidth();
            run.shrinkHorizontally(run.logicalWidth());
            ++index;
            continue;
        }

        removedOrCollapsedContentWidth += run.logicalWidth();
        m_runs.removeAt(index);
    }
    m_contentLogicalWidth -= removedOrCollapsedContentWidth;
}

const Box* Line::removeOverflowingOutOfFlowContent()
{
    auto lastTrailingOutOfFlowItemIndex = std::optional<size_t> { };
    for (size_t index = m_runs.size(); index--;) {
        auto& run = m_runs[index];
        if (run.isOpaque() && run.layoutBox().isOutOfFlowPositioned()) {
            lastTrailingOutOfFlowItemIndex = index;
            continue;
        }
        if (!run.logicalWidth() && (run.isInlineBoxStart() || run.isInlineBoxEnd()))
            continue;
        break;
    }
    if (!lastTrailingOutOfFlowItemIndex)
        return { };
    auto* lastTrailingOpaqueBox = &m_runs[*lastTrailingOutOfFlowItemIndex].layoutBox();
    m_runs.removeAt(*lastTrailingOutOfFlowItemIndex, m_runs.size() - *lastTrailingOutOfFlowItemIndex);
    ASSERT(!m_runs.isEmpty());
    return lastTrailingOpaqueBox;
}

void Line::handleTrailingHangingContent(std::optional<IntrinsicWidthMode> intrinsicWidthMode, InlineLayoutUnit horizontalAvailableSpaceForContent, bool isLastFormattedLine)
{
    // https://drafts.csswg.org/css-text/#hanging
    if (!m_hangingContent.trailingWidth())
        return;

    if (m_hangingContent.isTrailingContentPunctuation() && !isLastFormattedLine)
        m_hangingContent.resetTrailingContent();

    auto hangingTrailingContentIsConditional = [&] {
        auto lineEndsWithForcedLineBreak = isLastFormattedLine || (!runs().isEmpty() && runs().last().isLineBreak());
        return m_hangingContent.isTrailingContentConditional() || (m_hangingContent.isTrailingContentConditionalWhenFollowedByForcedLineBreak() && lineEndsWithForcedLineBreak);
    }();

    if (!intrinsicWidthMode) {
        // Make sure when the conditionally hanging content actually fits we don't hang the content anymore during normal layout.
        if (hangingTrailingContentIsConditional) {
            // In some cases, a glyph at the end of a line can conditionally hang: it hangs only if it does not otherwise fit in the line prior to justification.
            auto doesTrailingHangingContentFit = m_contentLogicalWidth <= horizontalAvailableSpaceForContent;
            if (doesTrailingHangingContentFit) {
                // Reset here means the trailing content is not hanging anymore -i.e. part of the line content.
                m_hangingContent.resetTrailingContent();
            }
        }
        return;
    }

    if (intrinsicWidthMode) {
        // 1. The hanging glyph is not taken into account when computing intrinsic sizes (min-content size and max-content size)
        // 2. Glyphs that conditionally hang are not taken into account when computing min-content sizes, but they are taken into account for max-content sizes.
        if (*intrinsicWidthMode == IntrinsicWidthMode::Minimum || !hangingTrailingContentIsConditional) {
            ASSERT(m_trimmableTrailingContent.isEmpty());
            m_contentLogicalWidth -= m_hangingContent.trailingWidth();
        }
    }
}

void Line::resetBidiLevelForTrailingWhitespace(UBiDiLevel rootBidiLevel)
{
    ASSERT(m_trimmableTrailingContent.isEmpty());
    if (!m_hasNonDefaultBidiLevelRun)
        return;
    // UAX#9 L1: trailing whitespace should use paragraph direction.
    // see https://unicode.org/reports/tr9/#L1
    auto trailingNonWhitespaceOnlyRunIndex = std::optional<size_t> { };
    for (auto index = m_runs.size(); index--;) {
        auto& run = m_runs[index];
        if (run.isAtomicInlineBox() || run.isLineBreak() || (run.isText() && !run.hasTrailingWhitespace()))
            break;

        if (!run.hasTrailingWhitespace()) {
            // Skip non-content type of runs e.g. <span>
            continue;
        }
        // No need to adjust the bidi level unless the directionality is different.
        // e.g. rtl root dir with trailing whitespace attached to an rtl run.
        auto sameInlineDirection = run.bidiLevel() % 2 == rootBidiLevel % 2;
        if (!run.isWhitespaceOnly()) {
            trailingNonWhitespaceOnlyRunIndex = !sameInlineDirection ? std::make_optional(index) : std::nullopt;
            // There can't be any trailing whitespace in front of this non-whitespace/whitespace content.
            break;
        }
        // Whitespace only runs just need simple bidi level reset.
        if (!sameInlineDirection)
            run.setBidiLevel(rootBidiLevel);
    }

    auto detachTrailingWhitespaceIfNeeded = [&] {
        if (!trailingNonWhitespaceOnlyRunIndex)
            return;
        auto runIndex = *trailingNonWhitespaceOnlyRunIndex;
        auto& run = m_runs[runIndex];
        auto detachedTrailingRun = *run.detachTrailingWhitespace();
        detachedTrailingRun.setBidiLevel(rootBidiLevel);
        if (runIndex == m_runs.size() - 1) {
            m_runs.append(detachedTrailingRun);
            return;
        }
        m_runs.insert(runIndex + 1, detachedTrailingRun);
    };
    detachTrailingWhitespaceIfNeeded();
}

void Line::appendInlineBoxStart(const InlineItem& inlineItem, const RenderStyle& style, InlineLayoutUnit logicalWidth, InlineLayoutUnit textSpacingAdjustment)
{
    auto& inlineBoxGeometry = formattingContext().geometryForBox(inlineItem.layoutBox());
    if (inlineBoxGeometry.marginBorderAndPaddingStart())
        m_hangingContent.resetTrailingContent();
    // This is really just a placeholder to mark the start of the inline box <span>.
    ++m_nonSpanningInlineLevelBoxCount;
    auto logicalLeft = lastRunLogicalRight();
    // Do not let negative margin make the content shorter than it already is.
    m_contentLogicalWidth = std::max(m_contentLogicalWidth, logicalLeft + logicalWidth);

    auto marginStart = inlineBoxGeometry.marginStart();
    if (marginStart < 0) {
        // Negative margin-start pulls the content to the logical left direction.
        logicalLeft += marginStart;
        logicalWidth -= marginStart;
    }

    auto mayPullNonInlineBoxContentToLogicalLeft = style.usedLetterSpacing() < 0;
    if (mayPullNonInlineBoxContentToLogicalLeft)
        m_inlineBoxLogicalLeftStack.append(logicalLeft);

    m_hasRubyContent = m_hasRubyContent || inlineItem.layoutBox().isRubyBase();
    m_runs.append({ inlineItem, style, logicalLeft, logicalWidth, textSpacingAdjustment });
    if (style.boxDecorationBreak() == BoxDecorationBreak::Clone)
        m_inlineBoxListWithClonedDecorationEnd.append(&inlineItem.layoutBox());
}

void Line::appendInlineBoxEnd(const InlineItem& inlineItem, const RenderStyle& style, InlineLayoutUnit logicalWidth)
{
    if (formattingContext().geometryForBox(inlineItem.layoutBox()).marginBorderAndPaddingEnd())
        m_hangingContent.resetTrailingContent();
    // This is really just a placeholder to mark the end of the inline box </span>.
    auto removeTrailingLetterSpacing = [&] {
        if (!m_trimmableTrailingContent.isTrailingRunPartiallyTrimmable())
            return;
        m_contentLogicalWidth -= m_trimmableTrailingContent.removePartiallyTrimmableContent();
    };
    // Prevent trailing letter-spacing from spilling out of the inline box.
    // https://drafts.csswg.org/css-text-3/#letter-spacing-property See example 21.
    removeTrailingLetterSpacing();
    auto logicalLeft = lastRunLogicalRight();
    auto mayPullNonInlineBoxContentToLogicalLeft = style.usedLetterSpacing() < 0;
    if (mayPullNonInlineBoxContentToLogicalLeft) {
        // Do not let negative spacing pull content to the left of the inline box logical left.
        // e.g. <span style="border-left: solid red; letter-spacing: -200px;">content</span>This should not be to the left of the red border)
        logicalLeft = std::max(logicalLeft, m_inlineBoxLogicalLeftStack.isEmpty() ? 0.f : m_inlineBoxLogicalLeftStack.takeLast());
    }
    m_runs.append({ inlineItem, style, logicalLeft, logicalWidth });
    // Do not let negative margin make the content shorter than it already is.
    m_contentLogicalWidth = std::max(m_contentLogicalWidth, logicalLeft + logicalWidth);
    if (style.boxDecorationBreak() == BoxDecorationBreak::Clone) {
        ASSERT(&inlineItem.layoutBox() == m_inlineBoxListWithClonedDecorationEnd.last());
        m_inlineBoxListWithClonedDecorationEnd.removeLast();
    }
}

void Line::appendText(const InlineTextItem& inlineTextItem, const RenderStyle& style, InlineLayoutUnit logicalWidth, std::optional<Line::ShapingBoundary> shapingBoundary)
{
    auto willCollapseCompletely = [&] {
        if (inlineTextItem.isEmpty()) {
            // Some generated content initiates empty text items. They are truly collapsible.
            return true;
        }
        if (!inlineTextItem.isWhitespace())
            return false;
        if (InlineTextItem::shouldPreserveSpacesAndTabs(inlineTextItem))
            return false;
        // This content is collapsible. Let's check if the last item is collapsed.
        for (auto& run : m_runs | std::views::reverse) {
            if (run.isAtomicInlineBox())
                return false;
            // https://drafts.csswg.org/css-text-3/#white-space-phase-1
            // Any collapsible space immediately following another collapsible space—even one outside the boundary of the inline containing that space,
            // provided both spaces are within the same inline formatting context—is collapsed to have zero advance width.
            if (run.isText())
                return run.hasCollapsibleTrailingWhitespace();
            ASSERT(run.isListMarker() || run.isLineSpanningInlineBoxStart() || run.isInlineBoxStart() || run.isInlineBoxEnd() || run.isWordBreakOpportunity() || run.isOpaque());
        }
        // Leading whitespace.
        return true;
    };

    if (willCollapseCompletely())
        return;

    auto needsNewRun = [&] {
        if (m_runs.isEmpty())
            return true;
        auto& lastRun = m_runs.last();
        if (&lastRun.layoutBox() != &inlineTextItem.layoutBox())
            return true;
        if (lastRun.bidiLevel() != inlineTextItem.bidiLevel())
            return true;
        if (!lastRun.isText())
            return true;
        if (lastRun.hasCollapsedTrailingWhitespace())
            return true;
        if (style.fontCascade().wordSpacing()
            && (inlineTextItem.isWordSeparator() || (lastRun.isWordSeparator() && lastRun.bidiLevel() != UBIDI_DEFAULT_LTR)))
            return true;
        if (inlineTextItem.isZeroWidthSpaceSeparator())
            return true;
        if (inlineTextItem.isQuirkNonBreakingSpace() || lastRun.isNonBreakingSpace())
            return true;
        if (!inlineTextItem.style().writingMode().isBidiLTR() && TextUtil::shouldPreserveSpacesAndTabs(inlineTextItem.layoutBox()) && inlineTextItem.isWhitespace() != lastRun.isWhitespaceOnly()) {
            // Whitespace content with position dependent width (e.g. tab character) does not work well when included with non-whitespace content.
            // We may end up with mismatching computed widths and misplaced glyphs at paint time -unless we keep dedicated runs with explicit positions.
            return true;
        }
        if (shapingBoundary || lastRun.isShapingBoundary())
            return true;
        return false;
    }();
    auto oldContentLogicalWidth = contentLogicalWidth();
    auto runHasHangablePunctuationStart = isFirstFormattedLine() && TextUtil::hasHangablePunctuationStart(inlineTextItem, style) && !lineHasVisuallyNonEmptyContent();
    auto contentLogicalRight = InlineLayoutUnit { };
    if (needsNewRun) {
        // Note, negative word spacing may cause glyph overlap.
        auto runLogicalLeft = [&] {
            if (runHasHangablePunctuationStart)
                return -TextUtil::hangablePunctuationStartWidth(inlineTextItem, style);
            return lastRunLogicalRight() + (inlineTextItem.isWordSeparator() ? style.fontCascade().wordSpacing() : 0.0f);
        }();
        m_runs.append({ inlineTextItem, style, runLogicalLeft, logicalWidth, { }, shapingBoundary });
        // Note that the _content_ logical right may be larger than the _run_ logical right.
        contentLogicalRight = runLogicalLeft + logicalWidth;
    } else {
        auto& lastRun = m_runs.last();
        ASSERT(lastRun.isText());
        if (style.usedLetterSpacing() >= 0) {
            lastRun.expand(inlineTextItem, logicalWidth);
            contentLogicalRight = lastRun.logicalRight();
        } else {
            auto contentWidthWithoutLastTextRun = [&] {
                // FIXME: We may need to traverse all the way to the previous non-text run (or even across inline boxes).
                if (style.fontCascade().wordSpacing() >= 0)
                    return m_contentLogicalWidth - std::max(0.f, lastRun.logicalWidth());
                // FIXME: Let's see if we need to optimize for this is the rare case of both letter and word spacing being negative.
                auto rightMostPosition = InlineLayoutUnit { };
                for (auto& run : m_runs | std::views::reverse)
                    rightMostPosition = std::max(rightMostPosition, run.logicalRight());
                return std::max(0.f, rightMostPosition);
            }();
            auto lastRunLogicalRight = lastRun.logicalRight();
            lastRun.expand(inlineTextItem, logicalWidth);
            // Negative letter spacing should only shorten the content to the boundary of the previous run.
            contentLogicalRight = std::max(contentWidthWithoutLastTextRun, lastRunLogicalRight + logicalWidth);
        }
    }
    // Ensure that property values that act like negative margin are not making the line wider.
    m_contentLogicalWidth = std::max(oldContentLogicalWidth, contentLogicalRight);

    auto lastRunIndex = m_runs.size() - 1;
    m_trailingSoftHyphenWidth = { };
    // Handle trailing content, specifically whitespace and letter spacing.
    auto updateTrimmableStatus = [&] {
        if (inlineTextItem.isFullyTrimmable()) {
            auto trimmableWidth = logicalWidth;
            auto trimmableContentOffset = (contentLogicalWidth() - oldContentLogicalWidth) - trimmableWidth;
            m_trimmableTrailingContent.addFullyTrimmableContent(lastRunIndex, trimmableContentOffset, trimmableWidth);
            return true;
        }
        m_trimmableTrailingContent.reset();
        return false;
    };
    auto isTrimmable = updateTrimmableStatus();

    auto updateHangingStatus = [&] {
        if (runHasHangablePunctuationStart)
            m_hangingContent.setLeadingPunctuation(TextUtil::hangablePunctuationStartWidth(inlineTextItem, style));

        auto runHasHangableWhitespaceEnd = !isTrimmable && inlineTextItem.isWhitespace() && TextUtil::shouldTrailingWhitespaceHang(m_runs[lastRunIndex].style());
        if (runHasHangableWhitespaceEnd) {
            m_hangingContent.setTrailingWhitespace(inlineTextItem.length(), logicalWidth);
            return;
        }
        if (TextUtil::hasHangablePunctuationEnd(inlineTextItem, style)) {
            m_hangingContent.setTrailingPunctuation(TextUtil::hangablePunctuationEndWidth(inlineTextItem, style));
            return;
        }
        if (TextUtil::hasHangableStopOrCommaEnd(inlineTextItem, style)) {
            auto isConditionalHanging = style.hangingPunctuation().contains(Style::HangingPunctuationValue::AllowEnd);
            m_hangingContent.setTrailingStopOrComma(TextUtil::hangableStopOrCommaEndWidth(inlineTextItem, style), isConditionalHanging);
            return;
        }
        m_hangingContent.resetTrailingContent();
    };
    updateHangingStatus();

    if (inlineTextItem.hasTrailingSoftHyphen())
        m_trailingSoftHyphenWidth = TextUtil::hyphenWidth(style);
}

void Line::appendTextFast(const InlineTextItem& inlineTextItem, const RenderStyle& style, InlineLayoutUnit logicalWidth)
{
    auto lineHasContent = !m_runs.isEmpty();
    auto willCollapseCompletely = [&] {
        if (inlineTextItem.isEmpty())
            return true;
        if (!inlineTextItem.isWhitespace() || InlineTextItem::shouldPreserveSpacesAndTabs(inlineTextItem))
            return false;
        return !lineHasContent || m_runs.last().hasCollapsibleTrailingWhitespace();
    };

    if (willCollapseCompletely())
        return;

    auto needsNewRun = [&] {
        if (!lineHasContent)
            return true;
        auto& lastRun = m_runs.last();
        if (lastRun.hasCollapsedTrailingWhitespace())
            return true;
        if (&lastRun.layoutBox() != &inlineTextItem.layoutBox())
            return true;
        if (inlineTextItem.isZeroWidthSpaceSeparator())
            return true;
        if (inlineTextItem.isQuirkNonBreakingSpace() || lastRun.isNonBreakingSpace())
            return true;
        return false;
    }();
    auto oldContentLogicalWidth = contentLogicalWidth();
    if (needsNewRun) {
        auto runLogicalLeft = lastRunLogicalRight();
        m_runs.append({ inlineTextItem, style, runLogicalLeft, logicalWidth });
        m_contentLogicalWidth = runLogicalLeft + logicalWidth;
    } else {
        auto& lastRun = m_runs.last();
        ASSERT(lastRun.isText());
        if (style.usedLetterSpacing() >= 0) {
            lastRun.expand(inlineTextItem, logicalWidth);
            m_contentLogicalWidth = lastRun.logicalRight();
        } else {
            auto contentWidthWithoutLastTextRun = m_contentLogicalWidth - std::max(0.f, lastRun.logicalWidth());
            auto lastRunLogicalRight = lastRun.logicalRight();
            lastRun.expand(inlineTextItem, logicalWidth);
            // Negative letter spacing should only shorten the content to the boundary of the previous run.
            m_contentLogicalWidth = std::max(contentWidthWithoutLastTextRun, lastRunLogicalRight + logicalWidth);
        }
    }

    m_trailingSoftHyphenWidth = { };
    auto updateTrimmableStatus = [&] {
        if (inlineTextItem.isFullyTrimmable()) {
            auto trimmableWidth = logicalWidth;
            auto trimmableContentOffset = (contentLogicalWidth() - oldContentLogicalWidth) - trimmableWidth;
            m_trimmableTrailingContent.addFullyTrimmableContent(m_runs.size() - 1, trimmableContentOffset, trimmableWidth);
            return true;
        }
        m_trimmableTrailingContent.reset();
        return false;
    };
    auto isTrimmable = updateTrimmableStatus();

    auto updateHangingStatus = [&] {
        auto runHasHangableWhitespaceEnd = !isTrimmable && inlineTextItem.isWhitespace() && TextUtil::shouldTrailingWhitespaceHang(style);
        if (runHasHangableWhitespaceEnd) {
            m_hangingContent.setTrailingWhitespace(inlineTextItem.length(), logicalWidth);
            return;
        }
        m_hangingContent.resetTrailingContent();
    };
    updateHangingStatus();

    if (inlineTextItem.hasTrailingSoftHyphen())
        m_trailingSoftHyphenWidth = TextUtil::hyphenWidth(style);
}

void Line::appendAtomicInlineBox(const InlineItem& inlineItem, const RenderStyle& style, InlineLayoutUnit marginBoxLogicalWidth)
{
    resetTrailingContent();
    // Do not let negative margin make the content shorter than it already is.
    m_contentLogicalWidth = std::max(m_contentLogicalWidth, lastRunLogicalRight() + marginBoxLogicalWidth);
    ++m_nonSpanningInlineLevelBoxCount;
    auto marginStart = formattingContext().geometryForBox(inlineItem.layoutBox()).marginStart();
    if (marginStart >= 0) {
        m_runs.append({ inlineItem, style, lastRunLogicalRight(), marginBoxLogicalWidth });
        return;
    }
    // Negative margin-start pulls the content to the logical left direction.
    // Negative margin also squeezes the margin box, we need to stretch it to make sure the subsequent content won't overlap.
    // e.g. <img style="width: 100px; margin-left: -100px;"> pulls the replaced box to -100px with the margin box width of 0px.
    // Instead we need to position it at -100px and size it to 100px so the subsequent content starts at 0px. 
    m_runs.append({ inlineItem, style, lastRunLogicalRight() + marginStart, marginBoxLogicalWidth - marginStart });
}

void Line::appendBlock(const InlineItem& blockItem, InlineLayoutUnit marginBoxLogicalWidth)
{
#if ASSERT_ENABLED
    // We may have added line spanning inline boxes when initializing this line for the block content.
    for (auto& run : m_runs)
        ASSERT(run.isLineSpanningInlineBoxStart());
#endif
    // Cloned decoration should not show up on block lines. Make the line hug the block level element.
    m_contentLogicalWidth = marginBoxLogicalWidth;
    m_runs.append({ blockItem, blockItem.style(), { }, marginBoxLogicalWidth });
}

void Line::appendLineBreak(const InlineItem& inlineItem, const RenderStyle& style)
{
    m_trailingSoftHyphenWidth = { };
    if (inlineItem.isHardLineBreak()) {
        ++m_nonSpanningInlineLevelBoxCount;
        return m_runs.append({ inlineItem, style, lastRunLogicalRight() });
    }
    // Soft line breaks (preserved new line characters) require inline text boxes for compatibility reasons.
    m_runs.append({ downcast<InlineSoftLineBreakItem>(inlineItem), inlineItem.style(), lastRunLogicalRight() });
}

void Line::appendWordBreakOpportunity(const InlineItem& inlineItem, const RenderStyle& style)
{
    m_runs.append({ inlineItem, style, lastRunLogicalRight() });
}

void Line::appendOpaqueBox(const InlineItem& inlineItem, const RenderStyle& style)
{
    m_runs.append({ inlineItem, style, lastRunLogicalRight() });
}

void Line::addTrailingHyphen(InlineLayoutUnit hyphenLogicalWidth)
{
    for (auto& run : m_runs | std::views::reverse) {
        if (!run.isText())
            continue;
        run.setNeedsHyphen(hyphenLogicalWidth);
        m_contentLogicalWidth += hyphenLogicalWidth;
        return;
    }
    ASSERT_NOT_REACHED();
}

bool Line::lineHasVisuallyNonEmptyContent() const
{
    auto& formattingContext = this->formattingContext();
    for (auto& run : m_runs | std::views::reverse) {
        if (Line::Run::isContentfulOrHasDecoration(run, formattingContext))
            return true;
    }
    return false;
}

bool Line::appendTrailingInlineItemAsTrailingRun(RunList& runs, InlineLayoutUnit trimmedTrailingWhitespaceWidth, InlineItemRange inlineItemRange, const InlineItemList& inlineItems)
{
    auto candidateInlineItemIndex = [&]() -> std::optional<size_t> {
        if (inlineItemRange.endIndex() > inlineItems.size())
            return { };
        auto& lastRunLayoutBox = runs.last().layoutBox();
        for (auto index = inlineItemRange.endIndex(); index--;) {
            if (&inlineItems[index].layoutBox() == &lastRunLayoutBox)
                return ++index;
        }
        return { };
    };

    auto trailingInlineItemIndex = candidateInlineItemIndex();
    if (!trailingInlineItemIndex || *trailingInlineItemIndex >= inlineItems.size()) {
        ASSERT_NOT_REACHED();
        return false;
    }

    auto* trailingInlineTextItem = dynamicDowncast<InlineTextItem>(inlineItems[*trailingInlineItemIndex]);
    if (!trailingInlineTextItem || !trailingInlineTextItem->isFullyTrimmable()) {
        ASSERT_NOT_REACHED();
        return false;
    }

    runs.append({ *trailingInlineTextItem, trailingInlineTextItem->style(), runs.last().logicalRight(), trimmedTrailingWhitespaceWidth });
    return true;
}

bool Line::restoreTrimmedTrailingWhitespace(InlineLayoutUnit trimmedTrailingWhitespaceWidth, RunList& runs, InlineItemRange inlineItemRange, const InlineItemList& inlineItems)
{
    auto restore = [&](auto& trailingRun) {
        ASSERT(trailingRun.isText());
        auto& layoutBox = downcast<InlineTextBox>(trailingRun.layoutBox());
        if (trailingRun.m_textContent->start + trailingRun.m_textContent->length == layoutBox.content().length()) {
            // It looks like we didn't _trim_ this trailing run but removed the lat run instead (happens with dedicated layout box).
            // Let's put that content back as trailing run.
            if (appendTrailingInlineItemAsTrailingRun(runs, trimmedTrailingWhitespaceWidth, inlineItemRange, inlineItems))
                return true;
            ASSERT_NOT_REACHED();
            return false;
        }
        trailingRun.m_logicalWidth += trimmedTrailingWhitespaceWidth;
        // This must be collapsed whitespace.
        trailingRun.m_textContent->length += 1;
        return true;
    };

    auto& lastRun = runs.last();
    if (lastRun.isText())
        return restore(lastRun);

    auto isHardLineBreakWithTrailingTextRun = lastRun.isHardLineBreak() && runs.size() > 1 && runs[runs.size() - 2].isText();
    if (isHardLineBreakWithTrailingTextRun) {
        if (!restore(runs[runs.size() - 2]))
            return false;
        lastRun.moveHorizontally(trimmedTrailingWhitespaceWidth);
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool Line::hasTrailingForcedLineBreak(const RunList& runs)
{
    return !runs.isEmpty() && runs.last().isLineBreak();
}

const InlineFormattingContext& Line::formattingContext() const
{
    return m_inlineFormattingContext;
}

Line::TrimmableTrailingContent::TrimmableTrailingContent(RunList& runs)
    : m_runs(runs)
{
}

void Line::TrimmableTrailingContent::addFullyTrimmableContent(size_t runIndex, InlineLayoutUnit trimmableContentOffset, InlineLayoutUnit trimmableWidth)
{
    // Any subsequent trimmable whitespace should collapse to zero advanced width and ignored at ::appendText()
    ASSERT(!m_hasFullyTrimmableContent);
    m_fullyTrimmableWidth = trimmableContentOffset + trimmableWidth;
    m_trimmableContentOffset = trimmableContentOffset;
    // Note that just because the trimmable width is 0 (font-size: 0px), it does not mean we don't have a trimmable trailing content.
    m_hasFullyTrimmableContent = true;
    m_firstTrimmableRunIndex = m_firstTrimmableRunIndex.value_or(runIndex);
}

void Line::TrimmableTrailingContent::addPartiallyTrimmableContent(size_t runIndex, InlineLayoutUnit trimmableWidth)
{
    // Do not add trimmable letter spacing after a fully trimmable whitespace.
    ASSERT(!m_firstTrimmableRunIndex);
    ASSERT(!m_hasFullyTrimmableContent);
    ASSERT(!m_partiallyTrimmableWidth);
    ASSERT(trimmableWidth);
    m_partiallyTrimmableWidth = trimmableWidth;
    m_firstTrimmableRunIndex = runIndex;
}

InlineLayoutUnit Line::TrimmableTrailingContent::remove()
{
    // Remove trimmable trailing content and move all the subsequent trailing runs.
    // <span> </span><span></span>
    // [trailing whitespace][inline box end][inline box start][inline box end]
    // Trim the whitespace run and move the trailing inline box runs to the logical left.
    ASSERT(!isEmpty());
    auto& trimmableRun = m_runs[*m_firstTrimmableRunIndex];
    ASSERT(trimmableRun.isText());

    auto trimmedWidth = m_trimmableContentOffset;
    if (m_hasFullyTrimmableContent)
        trimmedWidth += trimmableRun.removeTrailingWhitespace();
    if (m_partiallyTrimmableWidth)
        trimmedWidth += trimmableRun.removeTrailingLetterSpacing();

    // When the trimmable run is followed by some non-content runs, we need to adjust their horizontal positions.
    // e.g. <div>text is followed by trimmable content    <span> </span></div>
    // When the [text...] run is trimmed (trailing whitespace is removed), both "<span>" and "</span>" runs
    // need to be moved horizontally to catch up with the [text...] run. Note that the whitespace inside the <span> does
    // not produce a run since in ::appendText() we see it as a fully trimmable run.
    for (auto index = *m_firstTrimmableRunIndex + 1; index < m_runs.size(); ++index) {
        auto& run = m_runs[index];
        ASSERT(run.isWordBreakOpportunity() || run.isLineSpanningInlineBoxStart() || run.isInlineBoxStart() || run.isInlineBoxEnd() || run.isLineBreak() || run.isOpaque());
        run.moveHorizontally(-trimmedWidth);
    }
    if (!trimmableRun.textContent()->length) {
        // This trimmable run is fully collapsed now (e.g. <div><img>    <span></span></div>).
        // We don't need to keep it around anymore.
        m_runs.removeAt(*m_firstTrimmableRunIndex);
    }
    reset();
    return trimmedWidth;
}

InlineLayoutUnit Line::TrimmableTrailingContent::removePartiallyTrimmableContent()
{
    // Partially trimmable content is always gated by a fully trimmable content.
    // We can't just trim spacing in the middle.
    ASSERT(!m_fullyTrimmableWidth);
    return remove();
}

inline static Line::Run::Type toLineRunType(const InlineItem& inlineItem)
{
    switch (inlineItem.type()) {
    case InlineItem::Type::HardLineBreak:
        return Line::Run::Type::HardLineBreak;
    case InlineItem::Type::WordBreakOpportunity:
        return Line::Run::Type::WordBreakOpportunity;
    case InlineItem::Type::AtomicInlineBox:
        if (inlineItem.layoutBox().isListMarkerBox())
            return downcast<ElementBox>(inlineItem.layoutBox()).isListMarkerOutside() ? Line::Run::Type::ListMarkerOutside : Line::Run::Type::ListMarkerInside;
        return Line::Run::Type::AtomicInlineBox;
    case InlineItem::Type::InlineBoxStart:
        return Line::Run::Type::InlineBoxStart;
    case InlineItem::Type::InlineBoxEnd:
        return Line::Run::Type::InlineBoxEnd;
    case InlineItem::Type::Opaque:
        return Line::Run::Type::Opaque;
    case InlineItem::Type::Block:
        return Line::Run::Type::Block;
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

std::optional<Line::Run::TrailingWhitespace::Type> Line::Run::trailingWhitespaceType(const InlineTextItem& inlineTextItem)
{
    if (!inlineTextItem.isWhitespace())
        return { };
    if (InlineTextItem::shouldPreserveSpacesAndTabs(inlineTextItem))
        return { TrailingWhitespace::Type::NotCollapsible };
    if (inlineTextItem.length() == 1)
        return { TrailingWhitespace::Type::Collapsible };
    return { TrailingWhitespace::Type::Collapsed };
}

Line::Run::Run(const InlineItem& inlineItem, const RenderStyle& style, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth, InlineLayoutUnit textSpacingAdjustment)
    : m_type(toLineRunType(inlineItem))
    , m_layoutBox(&inlineItem.layoutBox())
    , m_style(style)
    , m_logicalLeft(logicalLeft)
    , m_logicalWidth(logicalWidth)
    , m_bidiLevel(inlineItem.bidiLevel())
    , m_textSpacingAdjustment(textSpacingAdjustment)
{
}

Line::Run::Run(const InlineItem& zeroWidhtInlineItem, const RenderStyle& style, InlineLayoutUnit logicalLeft)
    : m_type(toLineRunType(zeroWidhtInlineItem))
    , m_layoutBox(&zeroWidhtInlineItem.layoutBox())
    , m_style(style)
    , m_logicalLeft(logicalLeft)
    , m_bidiLevel(zeroWidhtInlineItem.bidiLevel())
{
}

Line::Run::Run(const InlineItem& lineSpanningInlineBoxItem, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth, InlineLayoutUnit textSpacingAdjustment)
    : m_type(Type::LineSpanningInlineBoxStart)
    , m_layoutBox(&lineSpanningInlineBoxItem.layoutBox())
    , m_style(lineSpanningInlineBoxItem.style())
    , m_logicalLeft(logicalLeft)
    , m_logicalWidth(logicalWidth)
    , m_bidiLevel(lineSpanningInlineBoxItem.bidiLevel())
    , m_textSpacingAdjustment(textSpacingAdjustment)
{
    ASSERT(lineSpanningInlineBoxItem.isInlineBoxStart());
}

Line::Run::Run(const InlineSoftLineBreakItem& softLineBreakItem, const RenderStyle& style, InlineLayoutUnit logicalLeft)
    : m_type(Type::SoftLineBreak)
    , m_layoutBox(&softLineBreakItem.layoutBox())
    , m_style(style)
    , m_logicalLeft(logicalLeft)
    , m_bidiLevel(softLineBreakItem.bidiLevel())
    , m_textContent({ softLineBreakItem.position(), 1 })
{
}

Line::Run::Run(const InlineTextItem& inlineTextItem, const RenderStyle& style, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth, InlineLayoutUnit textSpacingAdjustment, std::optional<Line::ShapingBoundary> shapingBoundary)
    : m_type(inlineTextItem.isWordSeparator() ? Type::WordSeparator : inlineTextItem.isQuirkNonBreakingSpace() ? Type::NonBreakingSpace : Type::Text)
    , m_layoutBox(&inlineTextItem.layoutBox())
    , m_style(style)
    , m_logicalLeft(logicalLeft)
    , m_logicalWidth(logicalWidth)
    , m_bidiLevel(inlineTextItem.bidiLevel())
    , m_shapingBoundary(shapingBoundary)
    , m_textSpacingAdjustment(textSpacingAdjustment)
{
    auto length = inlineTextItem.length();
    auto whitespaceType = trailingWhitespaceType(inlineTextItem);
    if (whitespaceType) {
        if (*whitespaceType == TrailingWhitespace::Type::Collapsed)
            length =  1;
        m_trailingWhitespace = { *whitespaceType, logicalWidth, length };
    }
    m_textContent = { inlineTextItem.start(), length };
}

void Line::Run::expand(const InlineTextItem& inlineTextItem, InlineLayoutUnit logicalWidth)
{
    ASSERT(!hasCollapsedTrailingWhitespace());
    ASSERT(isText() && inlineTextItem.isText());
    ASSERT(m_layoutBox == &inlineTextItem.layoutBox());
    ASSERT(m_bidiLevel == inlineTextItem.bidiLevel());
    ASSERT(!m_shapingBoundary || *m_shapingBoundary != ShapingBoundary::End);

    m_logicalWidth += logicalWidth;
    auto whitespaceType = trailingWhitespaceType(inlineTextItem);

    if (!whitespaceType) {
        m_trailingWhitespace = { };
        m_textContent->length += inlineTextItem.length();
        m_lastNonWhitespaceContentStart = inlineTextItem.start();
        return;
    }
    auto whitespaceWidth = !m_trailingWhitespace ? logicalWidth : m_trailingWhitespace->width + logicalWidth;
    auto trailingWhitespaceLength = *whitespaceType == TrailingWhitespace::Type::Collapsed ? 1 : inlineTextItem.length(); 
    m_trailingWhitespace = { *whitespaceType, whitespaceWidth, trailingWhitespaceLength };
    m_textContent->length += trailingWhitespaceLength;
}

std::optional<Line::Run> Line::Run::detachTrailingWhitespace()
{
    if (!m_trailingWhitespace || isWhitespaceOnly())
        return { };

    ASSERT(m_trailingWhitespace->length < m_textContent->length);
    auto trailingWhitespaceRun = *this;

    auto leadingNonWhitespaceContentLength = m_textContent->length - m_trailingWhitespace->length;
    trailingWhitespaceRun.m_textContent = { m_textContent->start + leadingNonWhitespaceContentLength, m_trailingWhitespace->length, false };

    trailingWhitespaceRun.m_logicalWidth = m_trailingWhitespace->width;
    trailingWhitespaceRun.m_logicalLeft = logicalRight() - m_trailingWhitespace->width;

    trailingWhitespaceRun.m_trailingWhitespace = { };
    trailingWhitespaceRun.m_lastNonWhitespaceContentStart = { };

    m_logicalWidth -= trailingWhitespaceRun.logicalWidth();
    m_textContent->length = leadingNonWhitespaceContentLength;
    m_trailingWhitespace = { };

    return trailingWhitespaceRun;
}

bool Line::Run::hasTrailingLetterSpacing() const
{
    return !hasTrailingWhitespace() && letterSpacing() > 0;
}

InlineLayoutUnit Line::Run::trailingLetterSpacing() const
{
    if (!hasTrailingLetterSpacing())
        return { };
    return InlineLayoutUnit { letterSpacing() };
}

InlineLayoutUnit Line::Run::removeTrailingLetterSpacing()
{
    ASSERT(hasTrailingLetterSpacing());
    auto trailingWidth = trailingLetterSpacing();
    shrinkHorizontally(trailingWidth);
    ASSERT(logicalWidth() > 0 || (!logicalWidth() && letterSpacing() >= static_cast<float>(intMaxForLayoutUnit)));
    return trailingWidth;
}

InlineLayoutUnit Line::Run::removeTrailingWhitespace()
{
    ASSERT(m_trailingWhitespace);
    // According to https://www.w3.org/TR/css-text-3/#white-space-property matrix
    // Trimmable whitespace is always collapsible so the length of the trailing trimmable whitespace is always 1 (or non-existent).
    ASSERT(m_textContent && m_textContent->length);
    constexpr size_t trailingTrimmableContentLength = 1;

    auto trimmedWidth = m_trailingWhitespace->width;
    if (m_lastNonWhitespaceContentStart && inlineDirection() == TextDirection::RTL) {
        // While LTR content could also suffer from slightly incorrect content width after trimming trailing whitespace (see TextUtil::width)
        // it hardly produces visually observable result.
        // FIXME: This may still incorrectly leave some content on the line (vs. re-measuring also at ::expand).
        auto& inlineTextBox = downcast<InlineTextBox>(*m_layoutBox);
        auto startPosition = *m_lastNonWhitespaceContentStart;
        auto endPosition = m_textContent->start + m_textContent->length;
        RELEASE_ASSERT(startPosition < endPosition - trailingTrimmableContentLength);
        if (inlineTextBox.content()[endPosition - 1] == space)
            trimmedWidth = TextUtil::trailingWhitespaceWidth(inlineTextBox, m_style.fontCascade(), startPosition, endPosition);
    }
    m_textContent->length -= trailingTrimmableContentLength;
    m_trailingWhitespace = { };
    shrinkHorizontally(trimmedWidth);
    return trimmedWidth;
}

bool Line::Run::isContentfulOrHasDecoration(const Run& run, const InlineFormattingContext& formattingContext)
{
    if (run.isContentful())
        return true;
    if (run.isInlineBox()) {
        if (run.logicalWidth())
            return true;
        if (run.layoutBox().isRubyBase())
            return true;
        // Even negative horizontal margin makes the line "contentful".
        auto& inlineBoxGeometry = formattingContext.geometryForBox(run.layoutBox());
        if (run.isInlineBoxStart())
            return inlineBoxGeometry.marginStart() || inlineBoxGeometry.borderStart() || inlineBoxGeometry.paddingStart();
        if (run.isInlineBoxEnd())
            return inlineBoxGeometry.marginEnd() || inlineBoxGeometry.borderEnd() || inlineBoxGeometry.paddingEnd();
        if (run.isLineSpanningInlineBoxStart()) {
            if (run.style().boxDecorationBreak() != BoxDecorationBreak::Clone)
                return false;
            return inlineBoxGeometry.borderStart() || inlineBoxGeometry.paddingStart();
        }
    }
    return false;
}

bool Line::Run::hasTextCombine() const
{
    return m_style.hasTextCombine();
}

InlineLayoutUnit Line::Run::letterSpacing() const
{
    return m_style.usedLetterSpacing();
}

}
}

