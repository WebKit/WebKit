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
#include "TextFlags.h"
#include "TextUtil.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

Line::Line(const InlineFormattingContext& inlineFormattingContext)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_trimmableTrailingContent(m_runs)
{
}

Line::~Line()
{
}

void Line::initialize(const Vector<InlineItem>& lineSpanningInlineBoxes)
{
    m_contentIsTruncated = false;
    m_inlineBoxListWithClonedDecorationEnd.clear();
    m_clonedEndDecorationWidthForInlineBoxRuns = { };
    m_nonSpanningInlineLevelBoxCount = 0;
    m_hasNonDefaultBidiLevelRun = false;
    m_contentLogicalWidth = { };
    m_runs.clear();
    resetTrailingContent();
    auto appendLineSpanningInlineBoxes = [&] {
        for (auto& inlineBoxStartItem : lineSpanningInlineBoxes) {
#if ENABLE(CSS_BOX_DECORATION_BREAK)
            if (inlineBoxStartItem.style().boxDecorationBreak() != BoxDecorationBreak::Clone)
                m_runs.append({ inlineBoxStartItem, lastRunLogicalRight(), { } });
            else {
                // https://drafts.csswg.org/css-break/#break-decoration
                // clone: Each box fragment is independently wrapped with the border, padding, and margin.
                auto& inlineBoxGeometry = formattingContext().geometryForBox(inlineBoxStartItem.layoutBox());
                auto marginBorderAndPaddingStart = inlineBoxGeometry.marginStart() + inlineBoxGeometry.borderStart() + inlineBoxGeometry.paddingStart().value_or(0_lu);
                auto runLogicalLeft = lastRunLogicalRight();
                m_runs.append({ inlineBoxStartItem, runLogicalLeft, marginBorderAndPaddingStart });
                // Do not let negative margin make the content shorter than it already is.
                m_contentLogicalWidth = std::max(m_contentLogicalWidth, runLogicalLeft + marginBorderAndPaddingStart);
                m_contentLogicalWidth += addBorderAndPaddingEndForInlineBoxDecorationClone(inlineBoxStartItem);
            }
#else
            m_runs.append({ inlineBoxStartItem, lastRunLogicalRight(), { } });
#endif
        }
    };
    appendLineSpanningInlineBoxes();
}

void Line::resetTrailingContent()
{
    m_trimmableTrailingContent.reset();
    m_hangingTrailingContent.reset();
    m_trailingSoftHyphenWidth = { };
}

void Line::applyRunExpansion(InlineLayoutUnit horizontalAvailableSpace)
{
    ASSERT(formattingContext().root().style().textAlign() == TextAlignMode::Justify || formattingContext().root().style().textAlignLast() == TextAlignLast::Justify);
    // Text is justified according to the method specified by the text-justify property,
    // in order to exactly fill the line box. Unless otherwise specified by text-align-last,
    // the last line before a forced break or the end of the block is start-aligned.
    if (m_runs.isEmpty() || m_runs.last().isLineBreak())
        return;
    // A hanging glyph is still enclosed inside its parent inline box and still participates in text justification:
    // its character advance is just not measured when determining how much content fits on the line, how much the line’s contents
    // need to be expanded or compressed for justification, or how to position the content within the line box for text alignment.
    auto spaceToDistribute = horizontalAvailableSpace - contentLogicalWidth() + m_hangingTrailingContent.width();
    if (spaceToDistribute <= 0)
        return;
    // Collect and distribute the expansion opportunities.
    size_t lineExpansionOpportunities = 0;
    Vector<size_t> runsExpansionOpportunities(m_runs.size());
    Vector<ExpansionBehavior> runsExpansionBehaviors(m_runs.size());
    auto lastRunIndexWithContent = std::optional<size_t> { };

    // Line start behaves as if we had an expansion here (i.e. fist runs should not start with allowing left expansion).
    auto runIsAfterExpansion = true;
    auto hangingTrailingContentLength = m_hangingTrailingContent.length();
    for (size_t runIndex = 0; runIndex < m_runs.size(); ++runIndex) {
        auto& run = m_runs[runIndex];
        auto expansionBehavior = ExpansionBehavior::defaultBehavior();
        size_t expansionOpportunitiesInRun = 0;

        // FIXME: Check why we don't apply expansion when whitespace is preserved.
        if (run.isText() && (!TextUtil::shouldPreserveSpacesAndTabs(run.layoutBox()) || hangingTrailingContentLength)) {
            if (run.hasTextCombine())
                expansionBehavior = ExpansionBehavior::forbidAll();
            else {
                expansionBehavior.left = runIsAfterExpansion ? ExpansionBehavior::Behavior::Forbid : ExpansionBehavior::Behavior::Allow;
                expansionBehavior.right = ExpansionBehavior::Behavior::Allow;
                auto& textContent = *run.textContent();
                // Trailing hanging whitespace sequence is ignored when computing the expansion opportunities.
                auto hangingTrailingContentInCurrentRun = std::min(textContent.length, hangingTrailingContentLength);
                auto length = textContent.length - hangingTrailingContentInCurrentRun;
                hangingTrailingContentLength -= hangingTrailingContentInCurrentRun;
                std::tie(expansionOpportunitiesInRun, runIsAfterExpansion) = FontCascade::expansionOpportunityCount(StringView(downcast<InlineTextBox>(run.layoutBox()).content()).substring(textContent.start, length), run.inlineDirection(), expansionBehavior);
            }
        } else if (run.isBox())
            runIsAfterExpansion = false;

        runsExpansionBehaviors[runIndex] = expansionBehavior;
        runsExpansionOpportunities[runIndex] = expansionOpportunitiesInRun;
        lineExpansionOpportunities += expansionOpportunitiesInRun;

        if (run.isText() || run.isBox())
            lastRunIndexWithContent = runIndex;
    }
    // Forbid right expansion in the last run to prevent trailing expansion at the end of the line.
    if (lastRunIndexWithContent && runsExpansionOpportunities[*lastRunIndexWithContent]) {
        runsExpansionBehaviors[*lastRunIndexWithContent].right = ExpansionBehavior::Behavior::Forbid;
        if (runIsAfterExpansion) {
            // When the last run has an after expansion (e.g. CJK ideograph) we need to remove this trailing expansion opportunity.
            // Note that this is not about trailing collapsible whitespace as at this point we trimmed them all.
            ASSERT(lineExpansionOpportunities && runsExpansionOpportunities[*lastRunIndexWithContent]);
            --lineExpansionOpportunities;
            --runsExpansionOpportunities[*lastRunIndexWithContent];
        }
    }
    // Anything to distribute?
    if (!lineExpansionOpportunities)
        return;
    // Distribute the extra space.
    auto expansionToDistribute = spaceToDistribute / lineExpansionOpportunities;
    auto accumulatedExpansion = InlineLayoutUnit { };
    for (size_t runIndex = 0; runIndex < m_runs.size(); ++runIndex) {
        auto& run = m_runs[runIndex];
        // Expand and move runs by the accumulated expansion.
        run.moveHorizontally(accumulatedExpansion);
        auto computedExpansion = expansionToDistribute * runsExpansionOpportunities[runIndex];
        run.setExpansion({ runsExpansionBehaviors[runIndex], computedExpansion });
        run.shrinkHorizontally(-computedExpansion);
        accumulatedExpansion += computedExpansion;
    }
    // Content grows as runs expand.
    m_contentLogicalWidth += accumulatedExpansion;
}

void Line::truncate(InlineLayoutUnit logicalRight)
{
    ASSERT(!m_contentIsTruncated);
    ASSERT(m_contentLogicalWidth > logicalRight);
    auto isFirstRun = true;
    for (auto& run : m_runs) {
        if (run.isInlineBox())
            continue;
        if (run.logicalRight() > logicalRight) {
            // The first character or atomic inline-level element on a line must be clipped rather than ellipsed.
            if (isFirstRun)
                m_contentIsTruncated = run.truncate(logicalRight - run.logicalLeft(), Run::CanFullyTruncate::No);
            else {
                run.truncate(logicalRight - run.logicalLeft());
                m_contentIsTruncated = true;
            }
        }
        isFirstRun = false;
    }
}

void Line::handleTrailingTrimmableContent(TrailingContentAction trailingTrimmableContentAction)
{
    if (m_trimmableTrailingContent.isEmpty() || m_runs.isEmpty())
        return;

    if (trailingTrimmableContentAction == TrailingContentAction::Preserve)
        return m_trimmableTrailingContent.reset();

    m_contentLogicalWidth -= m_trimmableTrailingContent.remove();
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
                // Only trim/remove trailign non-breaking space.
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
        m_runs.remove(index);
    }
    m_contentLogicalWidth -= removedOrCollapsedContentWidth;
}

void Line::removeHangingGlyphs()
{
    ASSERT(m_trimmableTrailingContent.isEmpty());
    m_contentLogicalWidth -= m_hangingTrailingContent.width();
    m_hangingTrailingContent.reset();
}

void Line::resetBidiLevelForTrailingWhitespace(UBiDiLevel rootBidiLevel)
{
    ASSERT(m_trimmableTrailingContent.isEmpty());
    if (!m_hasNonDefaultBidiLevelRun)
        return;
    // UAX#9 L1: trailing whitespace should use paragraph direction.
    // see https://unicode.org/reports/tr9/#L1
    for (auto index = m_runs.size(); index--;) {
        auto& run = m_runs[index];
        if (run.isBox() || run.isLineBreak() || (run.isText() && !run.hasTrailingWhitespace()))
            break;

        if (!run.hasTrailingWhitespace()) {
            // Skip non-content type of runs e.g. <span>
            continue;
        }

        auto adjustBidiLevelIfNeeded = [&] {
            // No need to adjust the bidi level unless the directionality is different.
            // e.g. rtl root dir with trailing whitespace attached to an rtl run.
            auto sameInlineDirection = run.bidiLevel() % 2 == rootBidiLevel % 2;
            if (sameInlineDirection)
                return;
            if (run.isWhitespaceOnly()) {
                run.setBidiLevel(rootBidiLevel);
                return;
            }
            auto detachedTrailingRun = *run.detachTrailingWhitespace();
            detachedTrailingRun.setBidiLevel(rootBidiLevel);
            if (index == m_runs.size() - 1) {
                m_runs.append(detachedTrailingRun);
                return;
            }
            m_runs.insert(index + 1, detachedTrailingRun);
        };
        adjustBidiLevelIfNeeded();
        if (!run.isWhitespaceOnly()) {
            // There can't be any trailing whitespace in front of this non-whitespace/whitespace content.
            break;
        }
    }
}

void Line::append(const InlineItem& inlineItem, const RenderStyle& style, InlineLayoutUnit logicalWidth)
{
    if (inlineItem.isText())
        appendTextContent(downcast<InlineTextItem>(inlineItem), style, logicalWidth);
    else if (inlineItem.isLineBreak())
        appendLineBreak(inlineItem, style);
    else if (inlineItem.isWordBreakOpportunity())
        appendWordBreakOpportunity(inlineItem, style);
    else if (inlineItem.isInlineBoxStart())
        appendInlineBoxStart(inlineItem, style, logicalWidth);
    else if (inlineItem.isInlineBoxEnd())
        appendInlineBoxEnd(inlineItem, style, logicalWidth);
    else if (inlineItem.layoutBox().isReplacedBox())
        appendReplacedInlineLevelBox(inlineItem, style, logicalWidth);
    else if (inlineItem.isBox())
        appendNonReplacedInlineLevelBox(inlineItem, style, logicalWidth);
    else
        ASSERT_NOT_REACHED();
    m_hasNonDefaultBidiLevelRun = m_hasNonDefaultBidiLevelRun || inlineItem.bidiLevel() != UBIDI_DEFAULT_LTR;
}

void Line::appendInlineBoxStart(const InlineItem& inlineItem, const RenderStyle& style, InlineLayoutUnit logicalWidth)
{
    // This is really just a placeholder to mark the start of the inline box <span>.
    ++m_nonSpanningInlineLevelBoxCount;
    auto logicalLeft = lastRunLogicalRight();
    // Incoming logical width includes the cloned decoration end to be able to do line breaking.
    auto borderAndPaddingEndForDecorationClone = addBorderAndPaddingEndForInlineBoxDecorationClone(inlineItem);
    // Do not let negative margin make the content shorter than it already is.
    m_contentLogicalWidth = std::max(m_contentLogicalWidth, logicalLeft + logicalWidth);

    auto marginStart = formattingContext().geometryForBox(inlineItem.layoutBox()).marginStart();
    if (marginStart >= 0) {
        m_runs.append({ inlineItem, style, logicalLeft, logicalWidth - borderAndPaddingEndForDecorationClone });
        return;
    }
    // Negative margin-start pulls the content to the logical left direction.
    m_runs.append({ inlineItem, style, logicalLeft + marginStart, logicalWidth - marginStart - borderAndPaddingEndForDecorationClone });
}

void Line::appendInlineBoxEnd(const InlineItem& inlineItem, const RenderStyle& style, InlineLayoutUnit logicalWidth)
{
    // This is really just a placeholder to mark the end of the inline box </span>.
    auto removeTrailingLetterSpacing = [&] {
        if (!m_trimmableTrailingContent.isTrailingRunPartiallyTrimmable())
            return;
        m_contentLogicalWidth -= m_trimmableTrailingContent.removePartiallyTrimmableContent();
    };
    // Prevent trailing letter-spacing from spilling out of the inline box.
    // https://drafts.csswg.org/css-text-3/#letter-spacing-property See example 21.
    removeTrailingLetterSpacing();
    m_contentLogicalWidth -= removeBorderAndPaddingEndForInlineBoxDecorationClone(inlineItem);
    auto logicalLeft = lastRunLogicalRight();
    m_runs.append({ inlineItem, style, logicalLeft, logicalWidth });
    // Do not let negative margin make the content shorter than it already is.
    m_contentLogicalWidth = std::max(m_contentLogicalWidth, logicalLeft + logicalWidth);
}

void Line::appendTextContent(const InlineTextItem& inlineTextItem, const RenderStyle& style, InlineLayoutUnit logicalWidth)
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
        for (auto& run : makeReversedRange(m_runs)) {
            if (run.isBox())
                return false;
            // https://drafts.csswg.org/css-text-3/#white-space-phase-1
            // Any collapsible space immediately following another collapsible space—even one outside the boundary of the inline containing that space,
            // provided both spaces are within the same inline formatting context—is collapsed to have zero advance width.
            if (run.isText())
                return run.hasCollapsibleTrailingWhitespace();
            ASSERT(run.isListMarker() || run.isLineSpanningInlineBoxStart() || run.isInlineBoxStart() || run.isInlineBoxEnd() || run.isWordBreakOpportunity());
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
        return false;
    }();
    auto oldContentLogicalWidth = contentLogicalWidth();
    if (needsNewRun) {
        // Note, negative word spacing may cause glyph overlap.
        auto runLogicalLeft = lastRunLogicalRight() + (inlineTextItem.isWordSeparator() ? style.fontCascade().wordSpacing() : 0.0f);
        m_runs.append({ inlineTextItem, style, runLogicalLeft, logicalWidth });
        // Note that the _content_ logical right may be larger than the _run_ logical right.
        auto contentLogicalRight = runLogicalLeft + logicalWidth + m_clonedEndDecorationWidthForInlineBoxRuns;
        m_contentLogicalWidth = std::max(oldContentLogicalWidth, contentLogicalRight);
    } else if (style.letterSpacing() >= 0) {
        auto& lastRun = m_runs.last();
        lastRun.expand(inlineTextItem, logicalWidth);
        // Ensure that property values that act like negative margin are not making the line wider.
        m_contentLogicalWidth = std::max(oldContentLogicalWidth, lastRun.logicalRight());
    } else {
        auto& lastRun = m_runs.last();
        ASSERT(lastRun.isText());
        // Negative letter spacing should only shorten the content to the boundary of the previous run.
        // FIXME: We may need to traverse all the way to the previous non-text run (or even across inline boxes).
        auto contentWidthWithoutLastTextRun = [&] {
            if (style.fontCascade().wordSpacing() >= 0)
                return m_contentLogicalWidth - std::max(0.f, lastRun.logicalWidth());
            // FIXME: Let's see if we need to optimize for this is the rare case of both letter and word spacing being negative.
            auto rightMostPosition = InlineLayoutUnit { };
            for (auto& run : makeReversedRange(m_runs))
                rightMostPosition = std::max(rightMostPosition, run.logicalRight());
            return std::max(0.f, rightMostPosition);
        }();
        auto lastRunLogicalRight = lastRun.logicalRight();
        lastRun.expand(inlineTextItem, logicalWidth);
        m_contentLogicalWidth = std::max(contentWidthWithoutLastTextRun, lastRunLogicalRight + logicalWidth);
    }

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
        // FIXME: Move it to InlineTextItem after removing the integration codepath check.
        auto isPartiallyTrimmable = !inlineTextItem.isWhitespace() && style.letterSpacing() > 0 && !formattingContext().layoutState().shouldIgnoreTrailingLetterSpacing();
        if (isPartiallyTrimmable) {
            m_trimmableTrailingContent.addPartiallyTrimmableContent(lastRunIndex, style.letterSpacing());
            return true;
        }
        m_trimmableTrailingContent.reset();
        return false;
    };
    auto isTrimmable = updateTrimmableStatus();

    auto updateHangingStatus = [&] {
        if (isTrimmable || !inlineTextItem.isWhitespace() || !m_runs[lastRunIndex].shouldTrailingWhitespaceHang()) {
            m_hangingTrailingContent.reset();
            return;
        }
        m_hangingTrailingContent.add(inlineTextItem, logicalWidth);
    };
    updateHangingStatus();

    if (inlineTextItem.hasTrailingSoftHyphen())
        m_trailingSoftHyphenWidth = style.fontCascade().width(TextRun { StringView { style.hyphenString() } });

}

void Line::appendNonReplacedInlineLevelBox(const InlineItem& inlineItem, const RenderStyle& style, InlineLayoutUnit marginBoxLogicalWidth)
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

void Line::appendReplacedInlineLevelBox(const InlineItem& inlineItem, const RenderStyle& style, InlineLayoutUnit marginBoxLogicalWidth)
{
    ASSERT(inlineItem.layoutBox().isReplacedBox());
    // FIXME: Surely replaced boxes behave differently.
    appendNonReplacedInlineLevelBox(inlineItem, style, marginBoxLogicalWidth);
}

void Line::appendLineBreak(const InlineItem& inlineItem, const RenderStyle& style)
{
    m_trailingSoftHyphenWidth = { };
    if (inlineItem.isHardLineBreak()) {
        ++m_nonSpanningInlineLevelBoxCount;
        return m_runs.append({ inlineItem, style, lastRunLogicalRight() });
    }
    // Soft line breaks (preserved new line characters) require inline text boxes for compatibility reasons.
    ASSERT(inlineItem.isSoftLineBreak());
    m_runs.append({ downcast<InlineSoftLineBreakItem>(inlineItem), inlineItem.style(), lastRunLogicalRight() });
}

void Line::appendWordBreakOpportunity(const InlineItem& inlineItem, const RenderStyle& style)
{
    m_runs.append({ inlineItem, style, lastRunLogicalRight() });
}

InlineLayoutUnit Line::addBorderAndPaddingEndForInlineBoxDecorationClone(const InlineItem& inlineBoxStartItem)
{
    ASSERT(inlineBoxStartItem.isInlineBoxStart());
#if ENABLE(CSS_BOX_DECORATION_BREAK)
    if (inlineBoxStartItem.style().boxDecorationBreak() != BoxDecorationBreak::Clone)
        return { };
    // https://drafts.csswg.org/css-break/#break-decoration
    auto& inlineBoxGeometry = formattingContext().geometryForBox(inlineBoxStartItem.layoutBox());
    auto borderAndPaddingEnd = inlineBoxGeometry.borderEnd() + inlineBoxGeometry.paddingEnd().value_or(0_lu);
    m_inlineBoxListWithClonedDecorationEnd.add(&inlineBoxStartItem.layoutBox(), borderAndPaddingEnd);
    m_clonedEndDecorationWidthForInlineBoxRuns += borderAndPaddingEnd;
    return borderAndPaddingEnd;
#endif
    return { };
}

InlineLayoutUnit Line::removeBorderAndPaddingEndForInlineBoxDecorationClone(const InlineItem& inlineBoxEndItem)
{
    ASSERT(inlineBoxEndItem.isInlineBoxEnd());
#if ENABLE(CSS_BOX_DECORATION_BREAK)
    auto borderAndPaddingEnd = m_inlineBoxListWithClonedDecorationEnd.take(&inlineBoxEndItem.layoutBox());
    if (std::isinf(borderAndPaddingEnd))
        return { };
    // This inline box end now contributes to the line content width in the regular way, so let's remove
    // it from the side structure where we keep track of the "not-yet placed but space taking" decorations.
    m_clonedEndDecorationWidthForInlineBoxRuns -= borderAndPaddingEnd;
    return borderAndPaddingEnd;
#endif
    return { };
}

void Line::addTrailingHyphen(InlineLayoutUnit hyphenLogicalWidth)
{
    for (auto& run : makeReversedRange(m_runs)) {
        if (!run.isText())
            continue;
        run.setNeedsHyphen(hyphenLogicalWidth);
        m_contentLogicalWidth += hyphenLogicalWidth;
        return;
    }
    ASSERT_NOT_REACHED();
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
    // Any subsequent trimmable whitespace should collapse to zero advanced width and ignored at ::appendTextContent().
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
        ASSERT(run.isWordBreakOpportunity() || run.isLineSpanningInlineBoxStart() || run.isInlineBoxStart() || run.isInlineBoxEnd() || run.isLineBreak());
        run.moveHorizontally(-trimmedWidth);
    }
    if (!trimmableRun.textContent()->length) {
        // This trimmable run is fully collapsed now (e.g. <div><img>    <span></span></div>).
        // We don't need to keep it around anymore.
        m_runs.remove(*m_firstTrimmableRunIndex);
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

void Line::HangingTrailingContent::add(const InlineTextItem& trailingWhitespace, InlineLayoutUnit logicalWidth)
{
    // When a glyph at the start or end edge of a line hangs, it is not considered when measuring the line’s contents for fit, alignment, or justification.
    // Depending on the line’s alignment/justification, this can result in the mark being placed outside the line box.
    // https://drafts.csswg.org/css-text-3/#hanging
    ASSERT(trailingWhitespace.isWhitespace());
    m_width += logicalWidth;
    m_length += trailingWhitespace.length();
}

inline static Line::Run::Type toLineRunType(const InlineItem& inlineItem)
{
    switch (inlineItem.type()) {
    case InlineItem::Type::HardLineBreak:
        return Line::Run::Type::HardLineBreak;
    case InlineItem::Type::WordBreakOpportunity:
        return Line::Run::Type::WordBreakOpportunity;
    case InlineItem::Type::Box:
        return inlineItem.layoutBox().isListMarkerBox() ? Line::Run::Type::ListMarker : Line::Run::Type::GenericInlineLevelBox;
    case InlineItem::Type::InlineBoxStart:
        return Line::Run::Type::InlineBoxStart;
    case InlineItem::Type::InlineBoxEnd:
        return Line::Run::Type::InlineBoxEnd;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
    return { };
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

Line::Run::Run(const InlineItem& inlineItem, const RenderStyle& style, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth)
    : m_type(toLineRunType(inlineItem))
    , m_layoutBox(&inlineItem.layoutBox())
    , m_style(style)
    , m_logicalLeft(logicalLeft)
    , m_logicalWidth(logicalWidth)
    , m_bidiLevel(inlineItem.bidiLevel())
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

Line::Run::Run(const InlineItem& lineSpanningInlineBoxItem, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth)
    : m_type(Type::LineSpanningInlineBoxStart)
    , m_layoutBox(&lineSpanningInlineBoxItem.layoutBox())
    , m_style(lineSpanningInlineBoxItem.style())
    , m_logicalLeft(logicalLeft)
    , m_logicalWidth(logicalWidth)
    , m_bidiLevel(lineSpanningInlineBoxItem.bidiLevel())
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

Line::Run::Run(const InlineTextItem& inlineTextItem, const RenderStyle& style, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth)
    : m_type(inlineTextItem.isWordSeparator() ? Type::WordSeparator : inlineTextItem.isQuirkNonBreakingSpace() ? Type::NonBreakingSpace : Type::Text)
    , m_layoutBox(&inlineTextItem.layoutBox())
    , m_style(style)
    , m_logicalLeft(logicalLeft)
    , m_logicalWidth(logicalWidth)
    , m_bidiLevel(inlineTextItem.bidiLevel())
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
    trailingWhitespaceRun.m_textContent = { m_textContent->start + leadingNonWhitespaceContentLength, m_trailingWhitespace->length, { }, false };

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

bool Line::Run::truncate(InlineLayoutUnit visibleWidth, CanFullyTruncate canFullyTruncate)
{
    ASSERT(!visibleWidth || visibleWidth < m_logicalWidth);

    if (isText()) {
        m_isTruncated = true;
        auto& inlineTextBox = downcast<InlineTextBox>(*m_layoutBox);
        auto leftSide = TextUtil::WordBreakLeft { };
        if (visibleWidth > 0.f)
            leftSide = TextUtil::breakWord(inlineTextBox, m_textContent->start, m_textContent->length, m_logicalWidth, visibleWidth, m_logicalLeft, m_style.fontCascade());

        if (leftSide.length)
            m_textContent->partiallyVisibleContent = { leftSide.length, leftSide.logicalWidth };
        else if (canFullyTruncate == CanFullyTruncate::No) {
            auto firstCharacterLength = TextUtil::firstUserPerceivedCharacterLength(inlineTextBox, m_textContent->start, m_textContent->length);
            auto firstCharacterWidth = TextUtil::width(inlineTextBox, inlineTextBox.style().fontCascade(), m_textContent->start, m_textContent->start + firstCharacterLength, { }, TextUtil::UseTrailingWhitespaceMeasuringOptimization::No);
            m_textContent->partiallyVisibleContent = { firstCharacterLength, firstCharacterWidth };
        }
    } else
        m_isTruncated = canFullyTruncate == CanFullyTruncate::Yes;

    return m_isTruncated;
}

}
}

