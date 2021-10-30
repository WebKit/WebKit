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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FontCascade.h"
#include "InlineFormattingContext.h"
#include "InlineSoftLineBreakItem.h"
#include "LayoutBoxGeometry.h"
#include "RuntimeEnabledFeatures.h"
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
    m_inlineBoxListWithClonedDecorationEnd.clear();
    m_clonedEndDecorationWidthForInlineBoxRuns = { };
    m_nonSpanningInlineLevelBoxCount = 0;
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
                auto marginBorderAndPaddingStart = inlineBoxGeometry.marginStart() + inlineBoxGeometry.borderLeft() + inlineBoxGeometry.paddingLeft().value_or(0_lu);
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

void Line::removeTrimmableContent(InlineLayoutUnit horizontalAvailableSpace)
{
    removeTrailingTrimmableContent();
    visuallyCollapseHangingOverflow(horizontalAvailableSpace);
}

void Line::applyRunExpansion(InlineLayoutUnit horizontalAvailableSpace)
{
    ASSERT(formattingContext().root().style().textAlign() == TextAlignMode::Justify);
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
        int expansionBehavior = DefaultExpansion;
        size_t expansionOpportunitiesInRun = 0;

        // FIXME: Check why we don't apply expansion when whitespace is preserved.
        if (run.isText() && (!TextUtil::shouldPreserveSpacesAndTabs(run.layoutBox()) || hangingTrailingContentLength)) {
            if (run.hasTextCombine())
                expansionBehavior = ForbidLeftExpansion | ForbidRightExpansion;
            else {
                expansionBehavior = (runIsAfterExpansion ? ForbidLeftExpansion : AllowLeftExpansion) | AllowRightExpansion;
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
    // Need to fix up the last run's trailing expansion.
    if (lastRunIndexWithContent && runsExpansionOpportunities[*lastRunIndexWithContent]) {
        // Turn off the trailing bits first and add the forbid trailing expansion.
        auto leadingExpansion = runsExpansionBehaviors[*lastRunIndexWithContent] & LeftExpansionMask;
        runsExpansionBehaviors[*lastRunIndexWithContent] = leadingExpansion | ForbidRightExpansion;
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

void Line::removeTrailingTrimmableContent()
{
    if (m_trimmableTrailingContent.isEmpty() || m_runs.isEmpty())
        return;

    // Complex line layout quirk: keep the trailing whitespace around when it is followed by a line break, unless the content overflows the line.
    if (RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled()) {
        auto isTextAlignRight = [&] {
            auto textAlign = formattingContext().root().style().textAlign();
            return textAlign == TextAlignMode::Right
                || textAlign == TextAlignMode::WebKitRight
                || textAlign == TextAlignMode::End;
            }();

        if (m_runs.last().isLineBreak() && !isTextAlignRight) {
            m_trimmableTrailingContent.reset();
            return;
        }
    }

    m_contentLogicalWidth -= m_trimmableTrailingContent.remove();
}

void Line::visuallyCollapseHangingOverflow(InlineLayoutUnit horizontalAvailableSpace)
{
    ASSERT(m_trimmableTrailingContent.isEmpty());
    // If white-space is set to pre-wrap, the UA must
    // ...
    // It may also visually collapse the character advance widths of any that would otherwise overflow.
    auto overflowWidth = contentLogicalWidth() - horizontalAvailableSpace;
    if (overflowWidth <= 0)
        return;
    // Let's just find the trailing pre-wrap whitespace content for now (e.g check if there are multiple trailing runs with
    // different set of white-space values and decide if the in-between pre-wrap content should be collapsed as well.)
    auto trimmedContentWidth = InlineLayoutUnit { };
    for (auto& run : makeReversedRange(m_runs)) {
        if (!run.shouldTrailingWhitespaceHang())
            break;
        auto visuallyCollapsibleInlineItem = run.isLineSpanningInlineBoxStart() || run.isInlineBoxStart() || run.isInlineBoxEnd() || run.hasTrailingWhitespace();
        if (!visuallyCollapsibleInlineItem)
            break;
        ASSERT(!run.hasCollapsibleTrailingWhitespace());
        auto trimmableWidth = InlineLayoutUnit { };
        if (run.isText()) {
            // FIXME: We should always collapse the run at a glyph boundary as the spec indicates: "collapse the character advance widths of any that would otherwise overflow"
            // and the trimmed width should be capped at std::min(run.trailingWhitespaceWidth(), overflowWidth) for text runs. Both FF and Chrome agree.
            trimmableWidth = run.visuallyCollapseTrailingWhitespace(overflowWidth);
        } else {
            trimmableWidth = run.logicalWidth();
            run.shrinkHorizontally(trimmableWidth);
        }
        trimmedContentWidth += trimmableWidth;
        overflowWidth -= trimmableWidth;
        if (overflowWidth <= 0)
            break;
    }
    // FIXME: Add support for incremental reset, where the hanging whitespace partially overflows.
    m_hangingTrailingContent.reset();
    m_contentLogicalWidth -= trimmedContentWidth;
}

void Line::append(const InlineItem& inlineItem, const RenderStyle& style, InlineLayoutUnit logicalWidth)
{
    if (inlineItem.isText())
        appendTextContent(downcast<InlineTextItem>(inlineItem), style, logicalWidth);
    else if (inlineItem.isLineBreak())
        appendLineBreak(inlineItem);
    else if (inlineItem.isWordBreakOpportunity())
        appendWordBreakOpportunity(inlineItem);
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
}

void Line::appendInlineBoxStart(const InlineItem& inlineItem, const RenderStyle& style, InlineLayoutUnit logicalWidth)
{
    // This is really just a placeholder to mark the start of the inline box <span>.
    ++m_nonSpanningInlineLevelBoxCount;
    auto logicalLeft = lastRunLogicalRight();
    // Incoming logical width includes the cloned decoration end to be able to do line breaking.
    auto borderAndPaddingEndForDecorationClone = addBorderAndPaddingEndForInlineBoxDecorationClone(inlineItem);
    m_runs.append({ inlineItem, style, logicalLeft, logicalWidth - borderAndPaddingEndForDecorationClone });
    // Do not let negative margin make the content shorter than it already is.
    m_contentLogicalWidth = std::max(m_contentLogicalWidth, logicalLeft + logicalWidth);
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
            ASSERT(run.isLineSpanningInlineBoxStart() || run.isInlineBoxStart() || run.isInlineBoxEnd() || run.isWordBreakOpportunity());
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
        if (inlineTextItem.isWordSeparator() && style.fontCascade().wordSpacing())
            return true;
        if (inlineTextItem.isZeroWidthSpaceSeparator())
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
    } else {
        m_runs.last().expand(inlineTextItem, logicalWidth);
        // Do not let negative letter spacing make the content shorter than it already is.
        m_contentLogicalWidth += std::max(0.0f, logicalWidth);
    }

    // Handle trailing content, specifically whitespace and letter spacing.
    auto lastRunIndex = m_runs.size() - 1;
    if (inlineTextItem.isWhitespace()) {
        if (InlineTextItem::shouldPreserveSpacesAndTabs(inlineTextItem)) {
            m_trimmableTrailingContent.reset();
            if (m_runs[lastRunIndex].shouldTrailingWhitespaceHang())
                m_hangingTrailingContent.add(inlineTextItem, logicalWidth);
        } else  {
            m_hangingTrailingContent.reset();
            m_trimmableTrailingContent.addFullyTrimmableContent(lastRunIndex, contentLogicalWidth() - oldContentLogicalWidth);
        }
        m_trailingSoftHyphenWidth = { };
    } else {
        resetTrailingContent();
        if (style.letterSpacing() > 0 && !formattingContext().layoutState().shouldIgnoreTrailingLetterSpacing())
            m_trimmableTrailingContent.addPartiallyTrimmableContent(lastRunIndex, style.letterSpacing());
        if (inlineTextItem.hasTrailingSoftHyphen())
            m_trailingSoftHyphenWidth = style.fontCascade().width(TextRun { StringView { style.hyphenString() } });
    }
}

void Line::appendNonReplacedInlineLevelBox(const InlineItem& inlineItem, const RenderStyle& style, InlineLayoutUnit marginBoxLogicalWidth)
{
    resetTrailingContent();
    m_contentLogicalWidth += marginBoxLogicalWidth;
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

void Line::appendLineBreak(const InlineItem& inlineItem)
{
    m_trailingSoftHyphenWidth = { };
    if (inlineItem.isHardLineBreak()) {
        ++m_nonSpanningInlineLevelBoxCount;
        return m_runs.append({ inlineItem, lastRunLogicalRight() });
    }
    // Soft line breaks (preserved new line characters) require inline text boxes for compatibility reasons.
    ASSERT(inlineItem.isSoftLineBreak());
    m_runs.append({ downcast<InlineSoftLineBreakItem>(inlineItem), lastRunLogicalRight() });
}

void Line::appendWordBreakOpportunity(const InlineItem& inlineItem)
{
    m_runs.append({ inlineItem, lastRunLogicalRight() });
}

InlineLayoutUnit Line::addBorderAndPaddingEndForInlineBoxDecorationClone(const InlineItem& inlineBoxStartItem)
{
    ASSERT(inlineBoxStartItem.isInlineBoxStart());
#if ENABLE(CSS_BOX_DECORATION_BREAK)
    if (inlineBoxStartItem.style().boxDecorationBreak() != BoxDecorationBreak::Clone)
        return { };
    // https://drafts.csswg.org/css-break/#break-decoration
    auto& inlineBoxGeometry = formattingContext().geometryForBox(inlineBoxStartItem.layoutBox());
    auto borderAndPaddingEnd = inlineBoxGeometry.borderRight() + inlineBoxGeometry.paddingRight().value_or(0_lu);
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

void Line::TrimmableTrailingContent::addFullyTrimmableContent(size_t runIndex, InlineLayoutUnit trimmableWidth)
{
    // Any subsequent trimmable whitespace should collapse to zero advanced width and ignored at ::appendTextContent().
    ASSERT(!m_hasFullyTrimmableContent);
    m_fullyTrimmableWidth = trimmableWidth;
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

    if (m_hasFullyTrimmableContent)
        trimmableRun.removeTrailingWhitespace();
    if (m_partiallyTrimmableWidth)
        trimmableRun.removeTrailingLetterSpacing();

    auto trimmableWidth = width();
    // When the trimmable run is followed by some non-content runs, we need to adjust their horizontal positions.
    // e.g. <div>text is followed by trimmable content    <span> </span></div>
    // When the [text...] run is trimmed (trailing whitespace is removed), both "<span>" and "</span>" runs
    // need to be moved horizontally to catch up with the [text...] run. Note that the whitespace inside the <span> does
    // not produce a run since in ::appendText() we see it as a fully collapsible run.
    for (auto index = *m_firstTrimmableRunIndex + 1; index < m_runs.size(); ++index) {
        auto& run = m_runs[index];
        ASSERT(run.isWordBreakOpportunity() || run.isLineSpanningInlineBoxStart() || run.isInlineBoxStart() || run.isInlineBoxEnd() || run.isLineBreak());
        run.moveHorizontally(-trimmableWidth);
    }
    if (!trimmableRun.textContent()->length) {
        // This trimmable run is fully collapsed now (e.g. <div><img>    <span></span></div>).
        // We don't need to keep it around anymore.
        m_runs.remove(*m_firstTrimmableRunIndex);
    }
    reset();
    return trimmableWidth;
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

inline static Line::Run::Type toLineRunType(InlineItem::Type inlineItemType)
{
    switch (inlineItemType) {
    case InlineItem::Type::HardLineBreak:
        return Line::Run::Type::HardLineBreak;
    case InlineItem::Type::WordBreakOpportunity:
        return Line::Run::Type::WordBreakOpportunity;
    case InlineItem::Type::Box:
        return Line::Run::Type::AtomicBox;
    case InlineItem::Type::InlineBoxStart:
        return Line::Run::Type::InlineBoxStart;
    case InlineItem::Type::InlineBoxEnd:
        return Line::Run::Type::InlineBoxEnd;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
    return { };
}

Line::Run::Run(const InlineItem& inlineItem, const RenderStyle& style, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth)
    : m_type(toLineRunType(inlineItem.type()))
    , m_layoutBox(&inlineItem.layoutBox())
    , m_logicalLeft(logicalLeft)
    , m_logicalWidth(logicalWidth)
    , m_style({ { }, style.direction(), { }, { } })
    , m_bidiLevel(inlineItem.bidiLevel())
{
}

Line::Run::Run(const InlineItem& zeroWidhtInlineItem, InlineLayoutUnit logicalLeft)
    : m_type(toLineRunType(zeroWidhtInlineItem.type()))
    , m_layoutBox(&zeroWidhtInlineItem.layoutBox())
    , m_logicalLeft(logicalLeft)
    , m_bidiLevel(zeroWidhtInlineItem.bidiLevel())
{
}

Line::Run::Run(const InlineItem& lineSpanningInlineBoxItem, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth)
    : m_type(Type::LineSpanningInlineBoxStart)
    , m_layoutBox(&lineSpanningInlineBoxItem.layoutBox())
    , m_logicalLeft(logicalLeft)
    , m_logicalWidth(logicalWidth)
    , m_bidiLevel(lineSpanningInlineBoxItem.bidiLevel())
{
    ASSERT(lineSpanningInlineBoxItem.isInlineBoxStart());
}

Line::Run::Run(const InlineSoftLineBreakItem& softLineBreakItem, InlineLayoutUnit logicalLeft)
    : m_type(Type::SoftLineBreak)
    , m_layoutBox(&softLineBreakItem.layoutBox())
    , m_logicalLeft(logicalLeft)
    , m_textContent({ softLineBreakItem.position(), 1 })
    , m_bidiLevel(softLineBreakItem.bidiLevel())
{
}

Line::Run::Run(const InlineTextItem& inlineTextItem, const RenderStyle& style, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth)
    : m_type(Type::Text)
    , m_layoutBox(&inlineTextItem.layoutBox())
    , m_logicalLeft(logicalLeft)
    , m_logicalWidth(logicalWidth)
    , m_trailingWhitespaceType(trailingWhitespaceType(inlineTextItem))
    , m_trailingWhitespaceWidth(m_trailingWhitespaceType != TrailingWhitespace::None ? logicalWidth : InlineLayoutUnit { })
    , m_textContent({ inlineTextItem.start(), m_trailingWhitespaceType == TrailingWhitespace::Collapsed ? 1 : inlineTextItem.length() })
    , m_style({ style.whiteSpace() == WhiteSpace::PreWrap, style.direction(), style.letterSpacing(), style.hasTextCombine() })
    , m_bidiLevel(inlineTextItem.bidiLevel())
{
}

void Line::Run::expand(const InlineTextItem& inlineTextItem, InlineLayoutUnit logicalWidth)
{
    ASSERT(!hasCollapsedTrailingWhitespace());
    ASSERT(isText() && inlineTextItem.isText());
    ASSERT(m_layoutBox == &inlineTextItem.layoutBox());
    ASSERT(m_bidiLevel == inlineTextItem.bidiLevel());

    m_logicalWidth += logicalWidth;
    m_trailingWhitespaceType = trailingWhitespaceType(inlineTextItem);

    if (m_trailingWhitespaceType == TrailingWhitespace::None) {
        m_trailingWhitespaceWidth = { };
        m_textContent->length += inlineTextItem.length();
        return;
    }
    m_trailingWhitespaceWidth += logicalWidth;
    m_textContent->length += m_trailingWhitespaceType == TrailingWhitespace::Collapsed ? 1 : inlineTextItem.length();
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

void Line::Run::removeTrailingLetterSpacing()
{
    ASSERT(hasTrailingLetterSpacing());
    shrinkHorizontally(trailingLetterSpacing());
    ASSERT(logicalWidth() > 0 || (!logicalWidth() && letterSpacing() >= static_cast<float>(intMaxForLayoutUnit)));
}

void Line::Run::removeTrailingWhitespace()
{
    // According to https://www.w3.org/TR/css-text-3/#white-space-property matrix
    // Trimmable whitespace is always collapsible so the length of the trailing trimmable whitespace is always 1 (or non-existent).
    ASSERT(m_textContent->length);
    constexpr size_t trailingTrimmableContentLength = 1;
    m_textContent->length -= trailingTrimmableContentLength;
    visuallyCollapseTrailingWhitespace(m_trailingWhitespaceWidth);
}

InlineLayoutUnit Line::Run::visuallyCollapseTrailingWhitespace(InlineLayoutUnit tryCollapsingThisMuchSpace)
{
    ASSERT(hasTrailingWhitespace());
    // This is just a visual adjustment, the text length should remain the same.
    auto trimmedWidth = std::min(tryCollapsingThisMuchSpace, m_trailingWhitespaceWidth);
    shrinkHorizontally(trimmedWidth);
    m_trailingWhitespaceWidth -= trimmedWidth;
    if (!m_trailingWhitespaceWidth) {
        // We trimmed the trailing whitespace completely.
        m_trailingWhitespaceType = TrailingWhitespace::None;
    }
    return trimmedWidth;
}

}
}

#endif
