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
#include "InlineLineBuilder.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "InlineFormattingContext.h"
#include "InlineSoftLineBreakItem.h"
#include "RuntimeEnabledFeatures.h"
#include "TextUtil.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

static inline bool isWhitespacePreserved(const RenderStyle& style)
{
    auto whitespace = style.whiteSpace();
    return whitespace == WhiteSpace::Pre || whitespace == WhiteSpace::PreWrap || whitespace == WhiteSpace::BreakSpaces;
}

struct HangingContent {
public:
    void reset();

    InlineLayoutUnit width() const { return m_width; }
    bool isConditional() const { return m_isConditional; }

    void setIsConditional() { m_isConditional = true; }
    void expand(InlineLayoutUnit width) { m_width += width; }

private:
    bool m_isConditional { false };
    InlineLayoutUnit m_width { 0 };
};

void HangingContent::reset()
{
    m_isConditional = false;
    m_width =  0;
}

struct LineBuilder::ContinuousContent {
public:
    ContinuousContent(const InlineItemRun&, bool textIsAlignJustify);

    bool isEligible(const InlineItemRun&) const;
    void append(const InlineItemRun&);
    LineBuilder::Run close();

    static bool canInlineItemRunBeExpanded(const InlineItemRun& run) { return run.isText() && !run.isCollapsed() && !run.isCollapsedToZeroAdvanceWidth(); }

private:
    const InlineItemRun& m_initialInlineRun;
    const bool m_collectExpansionOpportunities { false };
    unsigned m_expandedLength { 0 };
    InlineLayoutUnit m_expandedWidth { 0 };
    bool m_trailingRunCanBeExpanded { true };
    bool m_hasTrailingExpansionOpportunity { false };
    unsigned m_expansionOpportunityCount { 0 };
};

LineBuilder::ContinuousContent::ContinuousContent(const InlineItemRun& initialInlineRun, bool textIsAlignJustify)
    : m_initialInlineRun(initialInlineRun)
    , m_collectExpansionOpportunities(textIsAlignJustify && !isWhitespacePreserved(m_initialInlineRun.style())) // Do not collect expansion data on preserved whitespace content (we should not mutate the spacing between runs in such cases).
{
    // We should not create a ContinuousContent object when even the inital run can not be expanded.
    ASSERT(canInlineItemRunBeExpanded(initialInlineRun));
}

bool LineBuilder::ContinuousContent::isEligible(const InlineItemRun& inlineItemRun) const
{
    if (!m_trailingRunCanBeExpanded)
        return false;
    // Only non-collapsed text runs with the same layout box can be added as continuous content.
    return inlineItemRun.isText() && !inlineItemRun.isCollapsedToZeroAdvanceWidth() && &m_initialInlineRun.layoutBox() == &inlineItemRun.layoutBox();
}

void LineBuilder::ContinuousContent::append(const InlineItemRun& inlineItemRun)
{
    // Merged content needs to be continuous.
    ASSERT(isEligible(inlineItemRun));
    m_trailingRunCanBeExpanded = canInlineItemRunBeExpanded(inlineItemRun);

    ASSERT(inlineItemRun.isText());
    m_expandedLength += inlineItemRun.textContext()->length();
    m_expandedWidth += inlineItemRun.logicalWidth();

    if (m_collectExpansionOpportunities) {
        m_hasTrailingExpansionOpportunity = inlineItemRun.hasExpansionOpportunity();
        if (m_hasTrailingExpansionOpportunity)
            ++m_expansionOpportunityCount;
    }
}

LineBuilder::Run LineBuilder::ContinuousContent::close()
{
    if (!m_expandedLength)
        return { m_initialInlineRun };
    // Expand the text content and set the expansion opportunities.
    ASSERT(m_initialInlineRun.isText());
    auto textContext = *m_initialInlineRun.textContext();
    auto length = textContext.length() + m_expandedLength;
    textContext.expand(length);

    if (m_collectExpansionOpportunities) {
        // FIXME: This is a very simple expansion merge. We should eventually switch over to FontCascade::expansionOpportunityCount.
        ExpansionBehavior expansionBehavior = m_hasTrailingExpansionOpportunity ? (ForbidLeadingExpansion | AllowTrailingExpansion) : (AllowLeadingExpansion | AllowTrailingExpansion);
        if (m_initialInlineRun.hasExpansionOpportunity())
            ++m_expansionOpportunityCount;
        textContext.setExpansion({ expansionBehavior, { } });
    }
    return { m_initialInlineRun,  Display::InlineRect { 0_lu, m_initialInlineRun.logicalLeft(), m_initialInlineRun.logicalWidth() + m_expandedWidth, 0_lu }, textContext, m_expansionOpportunityCount };
}

LineBuilder::Run::Run(const InlineItemRun& inlineItemRun)
    : m_layoutBox(&inlineItemRun.layoutBox())
    , m_type(inlineItemRun.type())
    , m_logicalRect({ 0_lu, inlineItemRun.logicalLeft(), inlineItemRun.logicalWidth(), 0_lu })
    , m_textContext(inlineItemRun.textContext())
    , m_isCollapsedToVisuallyEmpty(inlineItemRun.isCollapsedToZeroAdvanceWidth())
{
    if (inlineItemRun.hasExpansionOpportunity()) {
        m_expansionOpportunityCount = 1;
        ASSERT(m_textContext);
        m_textContext->setExpansion({ DefaultExpansion, { } });
    }
}

LineBuilder::Run::Run(const InlineItemRun& inlineItemRun, const Display::InlineRect& logicalRect, const Display::Run::TextContext& textContext, unsigned expansionOpportunityCount)
    : m_layoutBox(&inlineItemRun.layoutBox())
    , m_type(inlineItemRun.type())
    , m_logicalRect(logicalRect)
    , m_textContext(textContext)
    , m_expansionOpportunityCount(expansionOpportunityCount)
    , m_isCollapsedToVisuallyEmpty(inlineItemRun.isCollapsedToZeroAdvanceWidth())
{
}

void LineBuilder::Run::adjustExpansionBehavior(ExpansionBehavior expansionBehavior)
{
    ASSERT(isText());
    ASSERT(hasExpansionOpportunity());
    m_textContext->setExpansion({ expansionBehavior, m_textContext->expansion()->horizontalExpansion });
}

inline Optional<ExpansionBehavior> LineBuilder::Run::expansionBehavior() const
{
    ASSERT(isText());
    if (auto expansionContext = m_textContext->expansion())
        return expansionContext->behavior;
    return { };
}

void LineBuilder::Run::setComputedHorizontalExpansion(InlineLayoutUnit logicalExpansion)
{
    ASSERT(isText());
    ASSERT(hasExpansionOpportunity());
    m_logicalRect.expandHorizontally(logicalExpansion);
    m_textContext->setExpansion({ m_textContext->expansion()->behavior, logicalExpansion });
}

LineBuilder::LineBuilder(const InlineFormattingContext& inlineFormattingContext, Optional<TextAlignMode> horizontalAlignment, IntrinsicSizing intrinsicSizing)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_trimmableTrailingContent(m_inlineItemRuns)
    , m_horizontalAlignment(horizontalAlignment)
    , m_isIntrinsicSizing(intrinsicSizing == IntrinsicSizing::Yes)
    , m_shouldIgnoreTrailingLetterSpacing(RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled())
{
}

LineBuilder::~LineBuilder()
{
}

void LineBuilder::initialize(const Constraints& constraints)
{
    ASSERT(m_isIntrinsicSizing || constraints.heightAndBaseline);

    InlineLayoutUnit initialLineHeight = 0;
    InlineLayoutUnit initialBaselineOffset = 0;
    if (constraints.heightAndBaseline) {
        m_initialStrut = constraints.heightAndBaseline->strut;
        initialLineHeight = constraints.heightAndBaseline->height;
        initialBaselineOffset = constraints.heightAndBaseline->baselineOffset;
    } else
        m_initialStrut = { };

    auto lineRect = Display::InlineRect { constraints.logicalTopLeft, 0_lu, initialLineHeight };
    auto baseline = Display::LineBox::Baseline { initialBaselineOffset, initialLineHeight - initialBaselineOffset };
    m_lineBox = Display::LineBox { lineRect, baseline, initialBaselineOffset };
    m_lineLogicalWidth = constraints.availableLogicalWidth;
    m_hasIntrusiveFloat = constraints.lineIsConstrainedByFloat;

    m_inlineItemRuns.clear();
    m_trimmableTrailingContent.reset();
    m_lineIsVisuallyEmptyBeforeTrimmableTrailingContent = { };
}

static inline bool shouldPreserveLeadingContent(const InlineTextItem& inlineTextItem)
{
    if (!inlineTextItem.isWhitespace())
        return true;
    auto whitespace = inlineTextItem.style().whiteSpace();
    return whitespace == WhiteSpace::Pre || whitespace == WhiteSpace::PreWrap || whitespace == WhiteSpace::BreakSpaces;
}

LineBuilder::RunList LineBuilder::close(IsLastLineWithInlineContent isLastLineWithInlineContent)
{
    // 1. Remove trimmable trailing content.
    // 2. Join text runs together when possible [foo][ ][bar] -> [foo bar].
    // 3. Align merged runs both vertically and horizontally.
    removeTrailingTrimmableContent();
    visuallyCollapsePreWrapOverflowContent();
    auto hangingContent = collectHangingContent(isLastLineWithInlineContent);

    auto mergedInlineItemRuns = [&] {
        RunList runList;
        unsigned runIndex = 0;
        while (runIndex < m_inlineItemRuns.size()) {
            // Merge eligible runs.
            auto& inlineItemRun = m_inlineItemRuns[runIndex];
            if (!ContinuousContent::canInlineItemRunBeExpanded(inlineItemRun)) {
                runList.append({ inlineItemRun });
                ++runIndex;
                continue;
            }
            auto mergedRuns = ContinuousContent { inlineItemRun, isTextAlignJustify() };
            for (runIndex = runIndex + 1; runIndex < m_inlineItemRuns.size() && mergedRuns.isEligible(m_inlineItemRuns[runIndex]); ++runIndex)
                mergedRuns.append(m_inlineItemRuns[runIndex]);
            runList.append(mergedRuns.close());
        }
        return runList;
    };

    auto runList = mergedInlineItemRuns();
    if (!m_isIntrinsicSizing) {
        for (auto& run : runList) {
            adjustBaselineAndLineHeight(run);
            run.setLogicalHeight(runContentHeight(run));
        }
        if (isVisuallyEmpty()) {
            m_lineBox.resetBaseline();
            m_lineBox.setLogicalHeight(0_lu);
        }
        // Remove descent when all content is baseline aligned but none of them have descent.
        if (formattingContext().quirks().lineDescentNeedsCollapsing(runList)) {
            m_lineBox.shrinkVertically(m_lineBox.baseline().descent());
            m_lineBox.resetDescent();
        }
        alignContentVertically(runList);
        alignHorizontally(runList, hangingContent, isLastLineWithInlineContent);
    }
    return runList;
}

size_t LineBuilder::revert(const InlineItem& revertTo)
{
    if (m_inlineItemRuns.last() == revertTo) {
        // Since the LineBreaker does not know what has been pushed on the current line
        // in some cases revert() is called with the last item on the line.
        return { };
    }
    // 1. Remove and shrink the trailing content.
    // 2. Rebuild trimmable trailing whitespace content.
    ASSERT(!m_inlineItemRuns.isEmpty());
    auto revertedWidth = InlineLayoutUnit { };
    auto originalSize = m_inlineItemRuns.size();
    int64_t index = static_cast<int64_t>(originalSize - 1);
    while (index >= 0 && m_inlineItemRuns[index] != revertTo)
        revertedWidth += m_inlineItemRuns[index--].logicalWidth();
    m_lineBox.shrinkHorizontally(revertedWidth);
    m_inlineItemRuns.shrink(index + 1);
    // Should never need to clear the line.
    ASSERT(!m_inlineItemRuns.isEmpty());

    // It's easier just to rebuild trailing trimmable content.
    m_trimmableTrailingContent.reset();
    m_lineIsVisuallyEmptyBeforeTrimmableTrailingContent = isVisuallyEmpty();
    // Find the first trimmable run.
    Optional<size_t> firstTrimmableRunIndex;
    for (auto index = m_inlineItemRuns.size(); index--;) {
        auto& inlineItemRun = m_inlineItemRuns[index];
        if (inlineItemRun.isContainerStart() || inlineItemRun.isContainerEnd())
            continue;
        auto hasTrailingTrimmableContent = inlineItemRun.isTrimmableWhitespace() || inlineItemRun.hasTrailingLetterSpacing();
        if (!hasTrailingTrimmableContent)
            break;
        if (inlineItemRun.isTrimmableWhitespace()) {
            firstTrimmableRunIndex = index;
            continue;
        }
        if (inlineItemRun.hasTrailingLetterSpacing()) {
            // While trailing letter spacing is considered trimmable, it is supposed to be last one in the list.
            firstTrimmableRunIndex = index;
            break;
        }
    }
    // Forward-append runs to m_trimmableTrailingContent. 
    if (firstTrimmableRunIndex) {
        for (auto index = *firstTrimmableRunIndex; index < m_inlineItemRuns.size(); ++index) {
            auto& inlineItemRun = m_inlineItemRuns[index];
            if (inlineItemRun.isContainerStart() || inlineItemRun.isContainerEnd())
                continue;
            ASSERT(inlineItemRun.isText());
            m_trimmableTrailingContent.append(index);
        }
    }
    // Consider alternative solutions if the (edge case)revert gets overly complicated.
    return originalSize - m_inlineItemRuns.size();
}

void LineBuilder::alignContentVertically(RunList& runList)
{
    ASSERT(!m_isIntrinsicSizing);
    auto scrollableOverflowRect = m_lineBox.logicalRect();
    for (auto& run : runList) {
        InlineLayoutUnit logicalTop = 0;
        auto& layoutBox = run.layoutBox();
        auto verticalAlign = layoutBox.style().verticalAlign();
        auto ascent = layoutBox.style().fontMetrics().ascent();

        switch (verticalAlign) {
        case VerticalAlign::Baseline:
            if (run.isLineBreak() || run.isText())
                logicalTop = baselineOffset() - ascent;
            else if (run.isContainerStart()) {
                auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
                logicalTop = baselineOffset() - ascent - boxGeometry.borderTop() - boxGeometry.paddingTop().valueOr(0);
            } else if (layoutBox.isInlineBlockBox() && layoutBox.establishesInlineFormattingContext()) {
                auto& formattingState = downcast<InlineFormattingState>(layoutState().establishedFormattingState(downcast<Container>(layoutBox)));
                // Spec makes us generate at least one line -even if it is empty.
                auto inlineBlockBaselineOffset = formattingState.displayInlineContent()->lineBoxes.last().baselineOffset();
                // The inline-block's baseline offset is relative to its content box. Let's convert it relative to the margin box.
                //           _______________ <- margin box
                //          |
                //          |  ____________  <- border box
                //          | |
                //          | |  _________  <- content box
                //          | | |   ^
                //          | | |   |  <- baseline offset
                //          | | |   |
                //     text | | |   v text
                //     -----|-|-|---------- <- baseline
                //
                auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
                auto baselineOffsetFromMarginBox = boxGeometry.marginBefore() + boxGeometry.borderTop() + boxGeometry.paddingTop().valueOr(0) + inlineBlockBaselineOffset;
                logicalTop = baselineOffset() - baselineOffsetFromMarginBox;
            } else
                logicalTop = baselineOffset() - run.logicalRect().height();
            break;
        case VerticalAlign::Top:
            logicalTop = 0_lu;
            break;
        case VerticalAlign::Bottom:
            logicalTop = logicalBottom() - run.logicalRect().height();
            break;
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
        run.adjustLogicalTop(logicalTop);
        // Adjust scrollable overflow if the run overflows the line.
        scrollableOverflowRect.expandVerticallyToContain(run.logicalRect());
        // Convert runs from relative to the line top/left to the formatting root's border box top/left.
        run.moveVertically(this->logicalTop());
        run.moveHorizontally(this->logicalLeft());
    }
    m_lineBox.setScrollableOverflow(scrollableOverflowRect);
}

void LineBuilder::justifyRuns(RunList& runList, InlineLayoutUnit availableWidth) const
{
    ASSERT(!runList.isEmpty());
    ASSERT(availableWidth > 0);
    // Collect the expansion opportunity numbers and find the last run with content.
    auto expansionOpportunityCount = 0;
    Run* lastRunWithContent = nullptr;
    for (auto& run : runList) {
        expansionOpportunityCount += run.expansionOpportunityCount();
        if ((run.isText() && !run.isCollapsedToVisuallyEmpty()) || run.isBox())
            lastRunWithContent = &run;
    }
    // Need to fix up the last run's trailing expansion.
    if (lastRunWithContent && lastRunWithContent->hasExpansionOpportunity()) {
        // Turn off the trailing bits first and add the forbid trailing expansion.
        auto leadingExpansion = *lastRunWithContent->expansionBehavior() & LeadingExpansionMask;
        lastRunWithContent->adjustExpansionBehavior(leadingExpansion | ForbidTrailingExpansion);
    }
    // Nothing to distribute?
    if (!expansionOpportunityCount)
        return;
    // Distribute the extra space.
    auto expansionToDistribute = availableWidth / expansionOpportunityCount;
    InlineLayoutUnit accumulatedExpansion = 0;
    for (auto& run : runList) {
        // Expand and moves runs by the accumulated expansion.
        if (!run.hasExpansionOpportunity()) {
            run.moveHorizontally(accumulatedExpansion);
            continue;
        }
        ASSERT(run.expansionOpportunityCount());
        auto computedExpansion = expansionToDistribute * run.expansionOpportunityCount();
        run.setComputedHorizontalExpansion(computedExpansion);
        run.moveHorizontally(accumulatedExpansion);
        accumulatedExpansion += computedExpansion;
    }
}

void LineBuilder::alignHorizontally(RunList& runList, const HangingContent& hangingContent, IsLastLineWithInlineContent lastLine)
{
    ASSERT(!m_isIntrinsicSizing);
    auto availableWidth = this->availableWidth() + hangingContent.width();
    if (runList.isEmpty() || availableWidth <= 0)
        return;

    if (isTextAlignJustify()) {
        // Do not justify align the last line.
        if (lastLine == IsLastLineWithInlineContent::No)
            justifyRuns(runList, availableWidth);
        return;
    }

    auto adjustmentForAlignment = [&]() -> Optional<InlineLayoutUnit> {
        switch (*m_horizontalAlignment) {
        case TextAlignMode::Left:
        case TextAlignMode::WebKitLeft:
        case TextAlignMode::Start:
            return { };
        case TextAlignMode::Right:
        case TextAlignMode::WebKitRight:
        case TextAlignMode::End:
            return std::max<InlineLayoutUnit>(availableWidth, 0);
        case TextAlignMode::Center:
        case TextAlignMode::WebKitCenter:
            return std::max<InlineLayoutUnit>(availableWidth / 2, 0);
        case TextAlignMode::Justify:
            ASSERT_NOT_REACHED();
            break;
        }
        ASSERT_NOT_REACHED();
        return { };
    };

    auto adjustment = adjustmentForAlignment();
    if (!adjustment)
        return;
    // Horizontal alignment means that we not only adjust the runs but also make sure
    // that the line box is aligned as well
    // e.g. <div style="text-align: center; width: 100px;">centered text</div> : the line box will also be centered
    // as opposed to start at 0px all the way to [centered text] run's right edge.
    m_lineBox.moveHorizontally(*adjustment);
    for (auto& run : runList)
        run.moveHorizontally(*adjustment);
}

void LineBuilder::removeTrailingTrimmableContent()
{
    if (m_trimmableTrailingContent.isEmpty() || m_inlineItemRuns.isEmpty())
        return;

    // Complex line layout quirk: keep the trailing whitespace around when it is followed by a line break, unless the content overflows the line.
    if (RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled()) {
        if (m_inlineItemRuns.last().isLineBreak() && availableWidth() >= 0 && !isTextAlignRight()) {
            m_trimmableTrailingContent.reset();
            return;
        }
    }

    m_lineBox.shrinkHorizontally(m_trimmableTrailingContent.remove());
    // If we removed the first visible run on the line, we need to re-check the visibility status.
    if (!m_lineIsVisuallyEmptyBeforeTrimmableTrailingContent)
        return;
    // Just because the line was visually empty before the removed content, it does not necessarily mean it is still visually empty.
    // <span>  </span><span style="padding-left: 10px"></span>  <- non-empty
    auto lineIsVisuallyEmpty = [&] {
        for (auto& run : m_inlineItemRuns) {
            if (isVisuallyNonEmpty(run))
                return false;
        }
        return true;
    };
    // We could only go from visually non empty -> to visually empty. Trimmed runs should never make the line visible.
    if (lineIsVisuallyEmpty())
        m_lineBox.setIsConsideredEmpty();
    m_lineIsVisuallyEmptyBeforeTrimmableTrailingContent = { };
}

void LineBuilder::visuallyCollapsePreWrapOverflowContent()
{
    ASSERT(m_trimmableTrailingContent.isEmpty());
    // If white-space is set to pre-wrap, the UA must
    // ...
    // It may also visually collapse the character advance widths of any that would otherwise overflow.
    auto overflowWidth = -availableWidth();
    if (overflowWidth <= 0)
        return;
    // Let's just find the trailing pre-wrap whitespace content for now (e.g check if there are multiple trailing runs with
    // different set of white-space values and decide if the in-between pre-wrap content should be collapsed as well.)
    InlineLayoutUnit trimmedContentWidth = 0;
    for (auto& inlineItemRun : WTF::makeReversedRange(m_inlineItemRuns)) {
        if (inlineItemRun.style().whiteSpace() != WhiteSpace::PreWrap) {
            // We are only interested in pre-wrap trailing content.
            break;
        }
        auto preWrapVisuallyCollapsibleInlineItem = inlineItemRun.isContainerStart() || inlineItemRun.isContainerEnd() || inlineItemRun.isWhitespace();
        if (!preWrapVisuallyCollapsibleInlineItem)
            break;
        auto runLogicalWidth = inlineItemRun.logicalWidth();
        // Never partially collapse inline container start/end items.
        auto isPartialCollapsingAllowed = inlineItemRun.isText();
        // FIXME: We should always collapse the run at a glyph boundary as the spec indicates: "collapse the character advance widths of any that would otherwise overflow"
        auto runLogicalWidthAfterCollapsing = isPartialCollapsingAllowed ? std::max<InlineLayoutUnit>(0, runLogicalWidth - overflowWidth) : 0;
        auto trimmed = runLogicalWidth - runLogicalWidthAfterCollapsing;
        trimmedContentWidth += trimmed;
        overflowWidth -= trimmed;
        inlineItemRun.adjustLogicalWidth(runLogicalWidthAfterCollapsing);
        if (overflowWidth <= 0)
            break;
    }
    m_lineBox.shrinkHorizontally(trimmedContentWidth);
}

HangingContent LineBuilder::collectHangingContent(IsLastLineWithInlineContent isLastLineWithInlineContent)
{
    auto hangingContent = HangingContent { };
    // Can't setup hanging content with removable trailing whitespace.
    ASSERT(m_trimmableTrailingContent.isEmpty());
    if (isLastLineWithInlineContent == IsLastLineWithInlineContent::Yes)
        hangingContent.setIsConditional();
    for (auto& inlineItemRun : WTF::makeReversedRange(m_inlineItemRuns)) {
        if (inlineItemRun.isContainerStart() || inlineItemRun.isContainerEnd())
            continue;
        if (inlineItemRun.isLineBreak()) {
            hangingContent.setIsConditional();
            continue;
        }
        if (!inlineItemRun.isText() || !inlineItemRun.isWhitespace() || inlineItemRun.isCollapsible())
            break;
        // Check if we have a preserved or hung whitespace.
        if (inlineItemRun.style().whiteSpace() != WhiteSpace::PreWrap)
            break;
        // This is either a normal or conditionally hanging trailing whitespace.
        hangingContent.expand(inlineItemRun.logicalWidth());
    }
    return hangingContent;
}

void LineBuilder::moveLogicalLeft(InlineLayoutUnit delta)
{
    if (!delta)
        return;
    ASSERT(delta > 0);
    m_lineBox.moveHorizontally(delta);
    m_lineLogicalWidth -= delta;
}

void LineBuilder::moveLogicalRight(InlineLayoutUnit delta)
{
    ASSERT(delta > 0);
    m_lineLogicalWidth -= delta;
}

void LineBuilder::append(const InlineItem& inlineItem, InlineLayoutUnit logicalWidth)
{
    if (inlineItem.isText())
        appendTextContent(downcast<InlineTextItem>(inlineItem), logicalWidth);
    else if (inlineItem.isLineBreak())
        appendLineBreak(inlineItem);
    else if (inlineItem.isContainerStart())
        appendInlineContainerStart(inlineItem, logicalWidth);
    else if (inlineItem.isContainerEnd())
        appendInlineContainerEnd(inlineItem, logicalWidth);
    else if (inlineItem.layoutBox().replaced())
        appendReplacedInlineBox(inlineItem, logicalWidth);
    else if (inlineItem.isBox())
        appendNonReplacedInlineBox(inlineItem, logicalWidth);
    else
        ASSERT_NOT_REACHED();

    // Check if this freshly appended content makes the line visually non-empty.
    ASSERT(!m_inlineItemRuns.isEmpty());
    if (m_lineBox.isConsideredEmpty() && isVisuallyNonEmpty(m_inlineItemRuns.last()))
        m_lineBox.setIsConsideredNonEmpty();
}

void LineBuilder::appendNonBreakableSpace(const InlineItem& inlineItem, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth)
{
    m_inlineItemRuns.append({ inlineItem, logicalLeft, logicalWidth });
    m_lineBox.expandHorizontally(logicalWidth);
}

void LineBuilder::appendInlineContainerStart(const InlineItem& inlineItem, InlineLayoutUnit logicalWidth)
{
    // This is really just a placeholder to mark the start of the inline level container <span>.
    appendNonBreakableSpace(inlineItem, contentLogicalWidth(), logicalWidth);
}

void LineBuilder::appendInlineContainerEnd(const InlineItem& inlineItem, InlineLayoutUnit logicalWidth)
{
    // This is really just a placeholder to mark the end of the inline level container </span>.
    auto removeTrailingLetterSpacing = [&] {
        if (!m_trimmableTrailingContent.isTrailingRunPartiallyTrimmable())
            return;
        m_lineBox.shrinkHorizontally(m_trimmableTrailingContent.removeTrailingRun());
    };
    // Prevent trailing letter-spacing from spilling out of the inline container.
    // https://drafts.csswg.org/css-text-3/#letter-spacing-property See example 21.
    removeTrailingLetterSpacing();
    appendNonBreakableSpace(inlineItem, contentLogicalRight(), logicalWidth);
}

void LineBuilder::appendTextContent(const InlineTextItem& inlineItem, InlineLayoutUnit logicalWidth)
{
    auto isCollapsible = inlineItem.isCollapsible();
    auto willCollapseCompletely = [&] {
        if (!isCollapsible)
            return false;
        // Check if the last item is collapsed as well.
        for (auto& run : WTF::makeReversedRange(m_inlineItemRuns)) {
            if (run.isBox())
                return false;
            // https://drafts.csswg.org/css-text-3/#white-space-phase-1
            // Any collapsible space immediately following another collapsible space—even one outside the boundary of the inline containing that space,
            // provided both spaces are within the same inline formatting context—is collapsed to have zero advance width.
            // : "<span>  </span> " <- the trailing whitespace collapses completely.
            // Not that when the inline container has preserve whitespace style, "<span style="white-space: pre">  </span> " <- this whitespace stays around.
            if (run.isText())
                return run.isCollapsible();
            ASSERT(run.isContainerStart() || run.isContainerEnd());
        }
        // Leading whitespace.
        return !shouldPreserveLeadingContent(inlineItem);
    };

    auto collapsesToZeroAdvanceWidth = willCollapseCompletely();
    logicalWidth = collapsesToZeroAdvanceWidth ? 0 : logicalWidth;
    auto collapsedRun = isCollapsible && inlineItem.length() > 1;
    auto contentLength =  collapsedRun ? 1 : inlineItem.length();
    m_inlineItemRuns.append({ inlineItem, contentLogicalWidth(), logicalWidth, collapsedRun, collapsesToZeroAdvanceWidth, Display::Run::TextContext { inlineItem.start(), contentLength, inlineItem.layoutBox().textContext()->content } });
    m_lineBox.expandHorizontally(logicalWidth);

    if (isCollapsible && !TextUtil::shouldPreserveTrailingWhitespace(inlineItem.style())) {
        // If we ever collapse this content, we need to know if the line visibility state needs to be recomputed.
        if (m_trimmableTrailingContent.isEmpty())
            m_lineIsVisuallyEmptyBeforeTrimmableTrailingContent = isVisuallyEmpty();
        m_trimmableTrailingContent.append(m_inlineItemRuns.size() - 1);
    } else {
        // Existing trailing collapsible content can only be expanded if the current run is fully collapsible.
        m_trimmableTrailingContent.reset();
        if (!m_shouldIgnoreTrailingLetterSpacing && !inlineItem.isWhitespace() && inlineItem.style().letterSpacing() > 0)
            m_trimmableTrailingContent.append(m_inlineItemRuns.size() - 1);
    }
}

void LineBuilder::appendNonReplacedInlineBox(const InlineItem& inlineItem, InlineLayoutUnit logicalWidth)
{
    auto& layoutBox = inlineItem.layoutBox();
    auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
    auto horizontalMargin = boxGeometry.horizontalMargin();
    m_inlineItemRuns.append({ inlineItem, contentLogicalWidth() + horizontalMargin.start, logicalWidth });
    m_lineBox.expandHorizontally(logicalWidth + horizontalMargin.start + horizontalMargin.end);
    m_trimmableTrailingContent.reset();
}

void LineBuilder::appendReplacedInlineBox(const InlineItem& inlineItem, InlineLayoutUnit logicalWidth)
{
    ASSERT(inlineItem.layoutBox().isReplaced());
    // FIXME: Surely replaced boxes behave differently.
    appendNonReplacedInlineBox(inlineItem, logicalWidth);
}

void LineBuilder::appendLineBreak(const InlineItem& inlineItem)
{
    if (inlineItem.isHardLineBreak())
        return m_inlineItemRuns.append({ inlineItem, contentLogicalWidth(), 0_lu });
    // Soft line breaks (preserved new line characters) require inline text boxes for compatibility reasons.
    ASSERT(inlineItem.isSoftLineBreak());
    auto& softLineBreakItem = downcast<InlineSoftLineBreakItem>(inlineItem);
    m_inlineItemRuns.append({ softLineBreakItem, contentLogicalWidth(), 0_lu, false, false, Display::Run::TextContext { softLineBreakItem.position(), 1, softLineBreakItem.layoutBox().textContext()->content } });
}

void LineBuilder::adjustBaselineAndLineHeight(const Run& run)
{
    auto& baseline = m_lineBox.baseline();
    if (run.isText() || run.isLineBreak()) {
        // For text content we set the baseline either through the initial strut (set by the formatting context root) or
        // through the inline container (start) -see above. Normally the text content itself does not stretch the line.
        if (!m_initialStrut)
            return;
        m_lineBox.setAscentIfGreater(m_initialStrut->ascent());
        m_lineBox.setDescentIfGreater(m_initialStrut->descent());
        m_lineBox.setLogicalHeightIfGreater(baseline.height());
        m_initialStrut = { };
        return;
    }

    auto& layoutBox = run.layoutBox();
    auto& style = layoutBox.style();
    if (run.isContainerStart()) {
        // Inline containers stretch the line by their font size.
        // Vertical margins, paddings and borders don't contribute to the line height.
        auto& fontMetrics = style.fontMetrics();
        if (style.verticalAlign() == VerticalAlign::Baseline) {
            auto halfLeading = halfLeadingMetrics(fontMetrics, style.computedLineHeight());
            // Both halfleading ascent and descent could be negative (tall font vs. small line-height value)
            if (halfLeading.descent() > 0)
                m_lineBox.setDescentIfGreater(halfLeading.descent());
            if (halfLeading.ascent() > 0)
                m_lineBox.setAscentIfGreater(halfLeading.ascent());
            m_lineBox.setLogicalHeightIfGreater(baseline.height());
        } else
            m_lineBox.setLogicalHeightIfGreater(fontMetrics.height());
        return;
    }

    if (run.isContainerEnd()) {
        // The line's baseline and height have already been adjusted at ContainerStart.
        return;
    }

    if (run.isBox()) {
        auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
        auto marginBoxHeight = boxGeometry.marginBoxHeight();

        switch (style.verticalAlign()) {
        case VerticalAlign::Baseline: {
            if (layoutBox.isInlineBlockBox() && layoutBox.establishesInlineFormattingContext()) {
                // Inline-blocks with inline content always have baselines.
                auto& formattingState = downcast<InlineFormattingState>(layoutState().establishedFormattingState(downcast<Container>(layoutBox)));
                // Spec makes us generate at least one line -even if it is empty.
                auto& lastLineBox = formattingState.displayInlineContent()->lineBoxes.last();
                auto inlineBlockBaseline = lastLineBox.baseline();
                auto beforeHeight = boxGeometry.marginBefore() + boxGeometry.borderTop() + boxGeometry.paddingTop().valueOr(0);

                m_lineBox.setAscentIfGreater(inlineBlockBaseline.ascent());
                m_lineBox.setDescentIfGreater(inlineBlockBaseline.descent());
                m_lineBox.setBaselineOffsetIfGreater(beforeHeight + lastLineBox.baselineOffset());
                m_lineBox.setLogicalHeightIfGreater(marginBoxHeight);
            } else {
                // Non inline-block boxes sit on the baseline (including their bottom margin).
                m_lineBox.setAscentIfGreater(marginBoxHeight);
                // Ignore negative descent (yes, negative descent is a thing).
                m_lineBox.setLogicalHeightIfGreater(marginBoxHeight + std::max<InlineLayoutUnit>(0, baseline.descent()));
            }
            break;
        }
        case VerticalAlign::Top:
            // Top align content never changes the baseline, it only pushes the bottom of the line further down.
            m_lineBox.setLogicalHeightIfGreater(marginBoxHeight);
            break;
        case VerticalAlign::Bottom: {
            // Bottom aligned, tall content pushes the baseline further down from the line top.
            auto lineLogicalHeight = m_lineBox.logicalHeight();
            if (marginBoxHeight > lineLogicalHeight) {
                m_lineBox.setLogicalHeightIfGreater(marginBoxHeight);
                m_lineBox.setBaselineOffsetIfGreater(m_lineBox.baselineOffset() + (marginBoxHeight - lineLogicalHeight));
            }
            break;
        }
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
        return;
    }
    ASSERT_NOT_REACHED();
}

InlineLayoutUnit LineBuilder::runContentHeight(const Run& run) const
{
    ASSERT(!m_isIntrinsicSizing);
    auto& fontMetrics = run.style().fontMetrics();
    if (run.isText() || run.isLineBreak())
        return fontMetrics.height();

    if (run.isContainerStart() || run.isContainerEnd())
        return fontMetrics.height();

    auto& layoutBox = run.layoutBox();
    auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
    if (layoutBox.replaced() || layoutBox.isFloatingPositioned())
        return boxGeometry.contentBoxHeight();

    // Non-replaced inline box (e.g. inline-block). It looks a bit misleading but their margin box is considered the content height here.
    return boxGeometry.marginBoxHeight();
}

bool LineBuilder::isVisuallyNonEmpty(const InlineItemRun& run) const
{
    if (run.isText())
        return !run.hasEmptyTextContent();

    // Note that this does not check whether the inline container has content. It simply checks if the container itself is considered non-empty.
    if (run.isContainerStart() || run.isContainerEnd()) {
        if (!run.logicalWidth())
            return false;
        // Margin does not make the container visually non-empty. Check if it has border or padding.
        auto& boxGeometry = formattingContext().geometryForBox(run.layoutBox());
        if (run.isContainerStart())
            return boxGeometry.borderLeft() || (boxGeometry.paddingLeft() && boxGeometry.paddingLeft().value());
        return boxGeometry.borderRight() || (boxGeometry.paddingRight() && boxGeometry.paddingRight().value());
    }

    if (run.isLineBreak())
        return true;

    if (run.isBox()) {
        if (!run.layoutBox().establishesFormattingContext())
            return true;
        ASSERT(run.layoutBox().isInlineBlockBox());
        if (!run.logicalWidth())
            return false;
        if (m_isIntrinsicSizing || formattingContext().geometryForBox(run.layoutBox()).height())
            return true;
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

Display::LineBox::Baseline LineBuilder::halfLeadingMetrics(const FontMetrics& fontMetrics, InlineLayoutUnit lineLogicalHeight)
{
    auto ascent = fontMetrics.ascent();
    auto descent = fontMetrics.descent();
    // 10.8.1 Leading and half-leading
    auto halfLeading = (lineLogicalHeight - (ascent + descent)) / 2;
    // Inline tree height is all integer based.
    auto adjustedAscent = std::max<InlineLayoutUnit>(floorf(ascent + halfLeading), 0);
    auto adjustedDescent = std::max<InlineLayoutUnit>(ceilf(descent + halfLeading), 0);
    return { adjustedAscent, adjustedDescent };
}

LayoutState& LineBuilder::layoutState() const
{ 
    return formattingContext().layoutState();
}

const InlineFormattingContext& LineBuilder::formattingContext() const
{
    return m_inlineFormattingContext;
}

LineBuilder::TrimmableTrailingContent::TrimmableTrailingContent(InlineItemRunList& inlineItemRunList)
    : m_inlineitemRunList(inlineItemRunList)
{
}

void LineBuilder::TrimmableTrailingContent::append(size_t runIndex)
{
    auto& trimmableRun = m_inlineitemRunList[runIndex];
    InlineLayoutUnit trimmableWidth = 0;
    auto isFullyTrimmable = trimmableRun.isTrimmableWhitespace();
    if (isFullyTrimmable)
        trimmableWidth = trimmableRun.logicalWidth();
    else {
        ASSERT(trimmableRun.hasTrailingLetterSpacing());
        trimmableWidth = trimmableRun.trailingLetterSpacing();
    }
    m_width += trimmableWidth;
    m_lastRunIsFullyTrimmable = isFullyTrimmable;
    m_firstRunIndex = m_firstRunIndex.valueOr(runIndex);
}

InlineLayoutUnit LineBuilder::TrimmableTrailingContent::remove()
{
    ASSERT(!isEmpty());
#if ASSERT_ENABLED
    auto hasSeenNonWhitespaceTextContent = false;
#endif
    // Remove trimmable trailing content and move all the other trailing runs.
    // <span> </span><span></span> ->
    // [whitespace][container end][container start][container end]
    // Remove the whitespace run and move the trailing inline container runs to the left.
    InlineLayoutUnit accumulatedTrimmedWidth = 0;
    for (auto index = *m_firstRunIndex; index < m_inlineitemRunList.size(); ++index) {
        auto& run = m_inlineitemRunList[index];
        run.moveHorizontally(-accumulatedTrimmedWidth);
        if (!run.isText()) {
            ASSERT(run.isContainerStart() || run.isContainerEnd() || run.isLineBreak());
            continue;
        }
        if (run.isWhitespace()) {
            accumulatedTrimmedWidth += run.logicalWidth();
            run.setCollapsesToZeroAdvanceWidth();
        } else {
            ASSERT(!hasSeenNonWhitespaceTextContent);
#if ASSERT_ENABLED
            hasSeenNonWhitespaceTextContent = true;
#endif
            // Must be a letter spacing trimming.
            ASSERT(run.hasTrailingLetterSpacing());
            accumulatedTrimmedWidth += run.trailingLetterSpacing();
            run.removeTrailingLetterSpacing();
        }
    }
    ASSERT(accumulatedTrimmedWidth == width());
    reset();
    return accumulatedTrimmedWidth;
}

InlineLayoutUnit LineBuilder::TrimmableTrailingContent::removeTrailingRun()
{
    ASSERT(!isEmpty());
    // Find the last trimmable run (it is not necessarily the last run e.g [container start][whitespace][container end])
    for (auto index = m_inlineitemRunList.size(); index-- && *m_firstRunIndex >= index;) {
        auto& run = m_inlineitemRunList[index];
        if (!run.isText()) {
            ASSERT(run.isContainerStart() || run.isContainerEnd());
            continue;
        }
        InlineLayoutUnit trimmedWidth = 0;
        if (run.isWhitespace()) {
            trimmedWidth = run.logicalWidth();
            run.setCollapsesToZeroAdvanceWidth();
        } else {
            ASSERT(run.hasTrailingLetterSpacing());
            trimmedWidth = run.trailingLetterSpacing();
            run.removeTrailingLetterSpacing();
        }
        m_width -= trimmedWidth;
        // We managed to remove the last trimmable run.
        if (index == *m_firstRunIndex) {
            ASSERT(!m_width);
            m_firstRunIndex = { };
        }
        return trimmedWidth;
    }
    ASSERT_NOT_REACHED();
    return 0_lu;
}

LineBuilder::InlineItemRun::InlineItemRun(const InlineItem& inlineItem, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth)
    : m_inlineItem(inlineItem)
    , m_logicalLeft(logicalLeft)
    , m_logicalWidth(logicalWidth)
{
}

LineBuilder::InlineItemRun::InlineItemRun(const InlineItem& inlineItem, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth, bool isCollapsed, bool isCollapsedToZeroAdvanceWidth, Display::Run::TextContext&& textContext)
    : m_inlineItem(inlineItem)
    , m_logicalLeft(logicalLeft)
    , m_logicalWidth(logicalWidth)
    , m_textContext(WTFMove(textContext))
    , m_isCollapsed(isCollapsed)
    , m_collapsedToZeroAdvanceWidth(isCollapsedToZeroAdvanceWidth)
{
}

bool LineBuilder::InlineItemRun::isTrimmableWhitespace() const
{
    // Return true if the "end-of-line spaces" can be removed.
    // See https://www.w3.org/TR/css-text-3/#white-space-property matrix.
    if (!isWhitespace())
        return false;
    return !TextUtil::shouldPreserveTrailingWhitespace(style());
}

bool LineBuilder::InlineItemRun::hasTrailingLetterSpacing() const
{
    // Complex line layout does not keep track of trailing letter spacing.
    if (RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled())
        return false;
    return !isWhitespace() && style().letterSpacing() > 0;
}

InlineLayoutUnit LineBuilder::InlineItemRun::trailingLetterSpacing() const
{
    if (!hasTrailingLetterSpacing())
        return 0_lu;
    return InlineLayoutUnit { style().letterSpacing() };
}

void LineBuilder::InlineItemRun::setCollapsesToZeroAdvanceWidth()
{
    m_collapsedToZeroAdvanceWidth = true;
    m_logicalWidth = 0_lu;
}

void LineBuilder::InlineItemRun::removeTrailingLetterSpacing()
{
    ASSERT(hasTrailingLetterSpacing());
    m_logicalWidth -= trailingLetterSpacing();
    ASSERT(m_logicalWidth > 0 || (!m_logicalWidth && style().letterSpacing() >= intMaxForLayoutUnit));
}

bool LineBuilder::InlineItemRun::hasEmptyTextContent() const
{
    ASSERT(isText());
    return isCollapsedToZeroAdvanceWidth() || downcast<InlineTextItem>(m_inlineItem).isEmptyContent();
}

}
}

#endif
