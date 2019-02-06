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
#include "InlineFormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FloatingContext.h"
#include "FloatingState.h"
#include "InlineFormattingState.h"
#include "InlineLineBreaker.h"
#include "InlineRunProvider.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutState.h"
#include "TextUtil.h"

namespace WebCore {
namespace Layout {

class Line {
public:
    void init(const LayoutPoint& topLeft, LayoutUnit availableWidth, LayoutUnit minimalHeight);
    void close();

    void appendContent(const InlineRunProvider::Run&, const LayoutSize&);

    void adjustLogicalLeft(LayoutUnit delta);
    void adjustLogicalRight(LayoutUnit delta);

    bool hasContent() const { return !m_inlineRuns.isEmpty(); }
    bool isClosed() const { return m_closed; }
    bool isFirstLine() const { return m_isFirstLine; }
    Vector<InlineRun>& runs() { return m_inlineRuns; }

    LayoutUnit contentLogicalRight() const;
    LayoutUnit contentLogicalLeft() const { return m_logicalRect.left(); }
    LayoutUnit availableWidth() const { return m_availableWidth; }
    Optional<InlineRunProvider::Run::Type> lastRunType() const { return m_lastRunType; }

    LayoutUnit logicalTop() const { return m_logicalRect.top(); }
    LayoutUnit logicalBottom() const { return m_logicalRect.bottom(); }
    LayoutUnit logicalHeight() const { return logicalBottom() - logicalTop(); }

private:
    struct TrailingTrimmableContent {
        LayoutUnit width;
        unsigned length;
    };
    Optional<TrailingTrimmableContent> m_trailingTrimmableContent;
    Optional<InlineRunProvider::Run::Type> m_lastRunType;
    bool m_lastRunCanExpand { false };

    Display::Box::Rect m_logicalRect;
    LayoutUnit m_availableWidth;

    Vector<InlineRun> m_inlineRuns;
    bool m_isFirstLine { true };
    bool m_closed { true };
};

void Line::init(const LayoutPoint& topLeft, LayoutUnit availableWidth, LayoutUnit minimalHeight)
{
    m_logicalRect.setTopLeft(topLeft);
    m_logicalRect.setWidth(availableWidth);
    m_logicalRect.setHeight(minimalHeight);
    m_availableWidth = availableWidth;

    m_inlineRuns.clear();
    m_lastRunType = { };
    m_lastRunCanExpand = false;
    m_trailingTrimmableContent = { };
    m_closed = false;
}

void Line::adjustLogicalLeft(LayoutUnit delta)
{
    ASSERT(delta > 0);

    m_availableWidth -= delta;
    m_logicalRect.shiftLeftTo(m_logicalRect.left() + delta);

    for (auto& inlineRun : m_inlineRuns)
        inlineRun.moveHorizontally(delta);
}

void Line::adjustLogicalRight(LayoutUnit delta)
{
    ASSERT(delta > 0);

    m_availableWidth -= delta;
    m_logicalRect.shiftRightTo(m_logicalRect.right() - delta);
}

static bool isTrimmableContent(const InlineRunProvider::Run& inlineRun)
{
    return inlineRun.isWhitespace() && inlineRun.style().collapseWhiteSpace();
}

LayoutUnit Line::contentLogicalRight() const
{
    if (m_inlineRuns.isEmpty())
        return m_logicalRect.left();

    return m_inlineRuns.last().logicalRight();
}

void Line::appendContent(const InlineRunProvider::Run& run, const LayoutSize& runSize)
{
    ASSERT(!isClosed());

    // Append this text run to the end of the last text run, if the last run is continuous.
    Optional<InlineRun::TextContext> textRun;
    if (run.isText()) {
        auto textContext = run.textContext();
        auto runLength = textContext->isCollapsed() ? 1 : textContext->length();
        textRun = InlineRun::TextContext { textContext->start(), runLength };
    }

    auto requiresNewInlineRun = !hasContent() || !run.isText() || !m_lastRunCanExpand;
    if (requiresNewInlineRun) {
        // FIXME: This needs proper baseline handling
        auto inlineRun = InlineRun { { logicalTop(), contentLogicalRight(), runSize.width(), runSize.height() }, run.inlineItem() };
        if (textRun)
            inlineRun.setTextContext({ textRun->start(), textRun->length() });
        m_inlineRuns.append(inlineRun);
        m_logicalRect.setHeight(std::max(runSize.height(), m_logicalRect.height()));
    } else {
        // Non-text runs always require new inline run.
        ASSERT(textRun);
        auto& inlineRun = m_inlineRuns.last();
        ASSERT(runSize.height() == inlineRun.logicalHeight());
        inlineRun.setLogicalWidth(inlineRun.logicalWidth() + runSize.width());
        inlineRun.textContext()->setLength(inlineRun.textContext()->length() + textRun->length());
    }

    m_availableWidth -= runSize.width();
    m_lastRunType = run.type();
    m_lastRunCanExpand = run.isText() && !run.textContext()->isCollapsed();
    m_trailingTrimmableContent = { };
    if (isTrimmableContent(run))
        m_trailingTrimmableContent = TrailingTrimmableContent { runSize.width(), textRun->length() };
}

void Line::close()
{
    auto trimTrailingContent = [&] {
        if (!m_trailingTrimmableContent)
            return;
        auto& lastInlineRun = m_inlineRuns.last();
        lastInlineRun.setLogicalWidth(lastInlineRun.logicalWidth() - m_trailingTrimmableContent->width);
        lastInlineRun.textContext()->setLength(lastInlineRun.textContext()->length() - m_trailingTrimmableContent->length);

        if (!lastInlineRun.textContext()->length())
            m_inlineRuns.removeLast();
        m_availableWidth += m_trailingTrimmableContent->width;
        m_trailingTrimmableContent = { };
    };

    if (!hasContent())
        return;

    trimTrailingContent();
    m_isFirstLine = false;
    m_closed = true;
}

InlineFormattingContext::LineLayout::LineLayout(const InlineFormattingContext& inlineFormattingContext)
    : m_formattingContext(inlineFormattingContext)
    , m_formattingState(m_formattingContext.formattingState())
    , m_floatingState(m_formattingState.floatingState())
    , m_formattingRoot(downcast<Container>(m_formattingContext.root()))
{
}

static bool isTrimmableContent(const InlineLineBreaker::Run& run)
{
    return run.content.isWhitespace() && run.content.style().collapseWhiteSpace();
}

void InlineFormattingContext::LineLayout::layout(const InlineRunProvider& inlineRunProvider) const
{
    auto& layoutState = m_formattingContext.layoutState();
    auto floatingContext = FloatingContext { m_floatingState };

    Line line;
    initializeNewLine(line);

    InlineLineBreaker lineBreaker(layoutState, m_formattingState.inlineContent(), inlineRunProvider.runs());
    while (auto run = lineBreaker.nextRun(line.contentLogicalRight(), line.availableWidth(), !line.hasContent())) {
        auto isFirstRun = run->position == InlineLineBreaker::Run::Position::LineBegin;
        auto isLastRun = run->position == InlineLineBreaker::Run::Position::LineEnd;
        auto generatesInlineRun = true;

        // Position float and adjust the runs on line.
        if (run->content.isFloat()) {
            auto& floatBox = run->content.inlineItem().layoutBox();
            computeFloatPosition(floatingContext, line, floatBox);
            m_floatingState.append(floatBox);

            auto floatBoxWidth = layoutState.displayBoxForLayoutBox(floatBox).marginBox().width();
            // Shrink availble space for current line and move existing inline runs.
            floatBox.isLeftFloatingPositioned() ? line.adjustLogicalLeft(floatBoxWidth) : line.adjustLogicalRight(floatBoxWidth);

            generatesInlineRun = false;
        }

        // 1. Initialize new line if needed.
        // 2. Append inline run unless it is skipped.
        // 3. Close current line if needed.
        if (isFirstRun) {
            // When the first run does not generate an actual inline run, the next run comes in first-run as well.
            // No need to spend time on closing/initializing.
            // Skip leading whitespace.
            if (!generatesInlineRun || isTrimmableContent(*run))
                continue;

            if (line.hasContent()) {
                // Previous run ended up being at the line end. Adjust the line accordingly.
                if (!line.isClosed())
                    closeLine(line, IsLastLine::No);
                initializeNewLine(line);
            }
        }

        if (generatesInlineRun) {
            auto width = run->width;
            auto height = run->content.isText() ? LayoutUnit(m_formattingRoot.style().computedLineHeight()) : layoutState.displayBoxForLayoutBox(run->content.inlineItem().layoutBox()).height(); 
            appendContentToLine(line, run->content, { width, height });
        }

        if (isLastRun)
            closeLine(line, IsLastLine::No);
    }

    closeLine(line, IsLastLine::Yes);
}

void InlineFormattingContext::LineLayout::initializeNewLine(Line& line) const
{
    auto& formattingRootDisplayBox = m_formattingContext.layoutState().displayBoxForLayoutBox(m_formattingRoot);

    auto lineLogicalLeft = formattingRootDisplayBox.contentBoxLeft();
    auto lineLogicalTop = line.isFirstLine() ? formattingRootDisplayBox.contentBoxTop() : line.logicalBottom();
    auto availableWidth = formattingRootDisplayBox.contentBoxWidth();

    // Check for intruding floats and adjust logical left/available width for this line accordingly.
    if (!m_floatingState.isEmpty()) {
        auto floatConstraints = m_floatingState.constraints({ lineLogicalTop }, m_formattingRoot);
        // Check if these constraints actually put limitation on the line.
        if (floatConstraints.left && *floatConstraints.left <= formattingRootDisplayBox.contentBoxLeft())
            floatConstraints.left = { };

        if (floatConstraints.right && *floatConstraints.right >= formattingRootDisplayBox.contentBoxRight())
            floatConstraints.right = { };

        if (floatConstraints.left && floatConstraints.right) {
            ASSERT(*floatConstraints.left < *floatConstraints.right);
            availableWidth = *floatConstraints.right - *floatConstraints.left;
            lineLogicalLeft = *floatConstraints.left;
        } else if (floatConstraints.left) {
            ASSERT(*floatConstraints.left > lineLogicalLeft);
            availableWidth -= (*floatConstraints.left - lineLogicalLeft);
            lineLogicalLeft = *floatConstraints.left;
        } else if (floatConstraints.right) {
            ASSERT(*floatConstraints.right > lineLogicalLeft);
            availableWidth = *floatConstraints.right - lineLogicalLeft;
        }
    }

    line.init({ lineLogicalLeft, lineLogicalTop }, availableWidth, m_formattingRoot.style().computedLineHeight());
}

void InlineFormattingContext::LineLayout::splitInlineRunIfNeeded(const InlineRun& inlineRun, InlineRuns& splitRuns) const
{
    ASSERT(inlineRun.textContext());
    ASSERT(inlineRun.overlapsMultipleInlineItems());
    // In certain cases, a run can overlap multiple inline elements like this:
    // <span>normal text content</span><span style="position: relative; left: 10px;">but this one needs a dedicated run</span><span>end of text</span>
    // The content above generates one long run <normal text contentbut this one needs dedicated runend of text>
    // However, since the middle run is positioned, it needs to be moved independently from the rest of the content, hence it needs a dedicated inline run.

    // 1. Start with the first inline item (element) and travers the list until
    // 2. either find an inline item that needs a dedicated run or we reach the end of the run
    // 3. Create dedicate inline runs.
    auto& inlineContent = m_formattingState.inlineContent();
    auto contentStart = inlineRun.logicalLeft();
    auto startPosition = inlineRun.textContext()->start();
    auto remaningLength = inlineRun.textContext()->length();

    struct Uncommitted {
        const InlineItem* firstInlineItem { nullptr };
        const InlineItem* lastInlineItem { nullptr };
        unsigned length { 0 };
    };
    Optional<Uncommitted> uncommitted;

    auto commit = [&] {
        if (!uncommitted)
            return;

        contentStart += uncommitted->firstInlineItem->nonBreakableStart();

        auto runWidth = this->runWidth(inlineContent, *uncommitted->firstInlineItem, startPosition, uncommitted->length, contentStart);
        auto run = InlineRun { { inlineRun.logicalTop(), contentStart, runWidth, inlineRun.logicalHeight() }, *uncommitted->firstInlineItem };
        run.setTextContext({ startPosition, uncommitted->length });
        splitRuns.append(run);

        contentStart += runWidth + uncommitted->lastInlineItem->nonBreakableEnd();

        startPosition = 0;
        uncommitted = { };
    };

    for (auto iterator = inlineContent.find(const_cast<InlineItem*>(&inlineRun.inlineItem())); iterator != inlineContent.end() && remaningLength > 0; ++iterator) {
        auto& inlineItem = **iterator;

        // Skip all non-inflow boxes (floats, out-of-flow positioned elements). They don't participate in the inline run context.
        if (!inlineItem.layoutBox().isInFlow())
            continue;

        auto currentLength = [&] {
            return std::min(remaningLength, inlineItem.textContent().length() - startPosition);
        };

        // 1. Break before/after -> requires dedicated run -> commit what we've got so far and also commit the current inline element as a separate inline run.
        // 2. Break at the beginning of the inline element -> commit what we've got so far. Current element becomes the first uncommitted.
        // 3. Break at the end of the inline element -> commit what we've got so far including the current element.
        // 4. Inline element does not require run breaking -> add current inline element to uncommitted. Jump to the next element.
        auto detachingRules = inlineItem.detachingRules();

        // #1
        if (detachingRules.containsAll({ InlineItem::DetachingRule::BreakAtStart, InlineItem::DetachingRule::BreakAtEnd })) {
            commit();
            auto contentLength = currentLength();
            uncommitted = Uncommitted { &inlineItem, &inlineItem, contentLength };
            remaningLength -= contentLength;
            commit();
            continue;
        }

        // #2
        if (detachingRules.contains(InlineItem::DetachingRule::BreakAtStart))
            commit();

        // Add current inline item to uncommitted.
        // #3 and #4
        auto contentLength = currentLength();
        if (!uncommitted)
            uncommitted = Uncommitted { &inlineItem, &inlineItem, 0 };
        uncommitted->length += contentLength;
        uncommitted->lastInlineItem = &inlineItem;
        remaningLength -= contentLength;

        // #3
        if (detachingRules.contains(InlineItem::DetachingRule::BreakAtEnd))
            commit();
    }
    // Either all inline elements needed dedicated runs or neither of them.
    if (!remaningLength || remaningLength == inlineRun.textContext()->length())
        return;

    commit();
}

void InlineFormattingContext::LineLayout::createFinalRuns(Line& line) const
{
    for (auto& inlineRun : line.runs()) {
        if (inlineRun.overlapsMultipleInlineItems()) {
            InlineRuns splitRuns;
            splitInlineRunIfNeeded(inlineRun, splitRuns);
            for (auto& splitRun : splitRuns)
                m_formattingState.appendInlineRun(splitRun);

            if (!splitRuns.isEmpty())
                continue;
        }

        auto finalRun = [&] {
            auto& inlineItem = inlineRun.inlineItem();
            if (inlineItem.detachingRules().isEmpty())
                return inlineRun;

            InlineRun adjustedRun = inlineRun;
            auto width = inlineRun.logicalWidth() - inlineItem.nonBreakableStart() - inlineItem.nonBreakableEnd();
            adjustedRun.setLogicalLeft(inlineRun.logicalLeft() + inlineItem.nonBreakableStart());
            adjustedRun.setLogicalWidth(width);
            return adjustedRun;
        };

        m_formattingState.appendInlineRun(finalRun());
    }
}

void InlineFormattingContext::LineLayout::postProcessInlineRuns(Line& line, IsLastLine isLastLine) const
{
    alignRuns(m_formattingRoot.style().textAlign(), line, isLastLine);
    auto firstRunIndex = m_formattingState.inlineRuns().size();
    createFinalRuns(line);

    placeInFlowPositionedChildren(firstRunIndex);
}

void InlineFormattingContext::LineLayout::closeLine(Line& line, IsLastLine isLastLine) const
{
    line.close();
    if (!line.hasContent())
        return;

    postProcessInlineRuns(line, isLastLine);
}

void InlineFormattingContext::LineLayout::appendContentToLine(Line& line, const InlineRunProvider::Run& run, const LayoutSize& runSize) const
{
    auto lastRunType = line.lastRunType();
    line.appendContent(run, runSize);

    if (m_formattingRoot.style().textAlign() == TextAlignMode::Justify)
        computeExpansionOpportunities(line, run, lastRunType.valueOr(InlineRunProvider::Run::Type::NonWhitespace));
}

void InlineFormattingContext::LineLayout::computeFloatPosition(const FloatingContext& floatingContext, Line& line, const Box& floatBox) const
{
    auto& layoutState = m_formattingContext.layoutState();
    ASSERT(layoutState.hasDisplayBox(floatBox));
    auto& displayBox = layoutState.displayBoxForLayoutBox(floatBox);

    // Set static position first.
    displayBox.setTopLeft({ line.contentLogicalRight(), line.logicalTop() });
    // Float it.
    displayBox.setTopLeft(floatingContext.positionForFloat(floatBox));
}

void InlineFormattingContext::LineLayout::placeInFlowPositionedChildren(unsigned fistRunIndex) const
{
    auto& layoutState = m_formattingContext.layoutState();
    auto& inlineRuns = m_formattingState.inlineRuns();
    for (auto runIndex = fistRunIndex; runIndex < inlineRuns.size(); ++runIndex) {
        auto& inlineRun = inlineRuns[runIndex];

        auto positionOffset = [&](auto& layoutBox) {
            // FIXME: Need to figure out whether in-flow offset should stick. This might very well be temporary.
            Optional<LayoutSize> offset;
            for (auto* box = &layoutBox; box != &m_formattingRoot; box = box->parent()) {
                if (!box->isInFlowPositioned())
                    continue;
                offset = offset.valueOr(LayoutSize()) + Geometry::inFlowPositionedPositionOffset(layoutState, *box);
            }
            return offset;
        };

        if (auto offset = positionOffset(inlineRun.inlineItem().layoutBox())) {
            inlineRun.moveVertically(offset->height());
            inlineRun.moveHorizontally(offset->width());
        }
    }
}

static LayoutUnit adjustedLineLogicalLeft(TextAlignMode align, LayoutUnit lineLogicalLeft, LayoutUnit remainingWidth)
{
    switch (align) {
    case TextAlignMode::Left:
    case TextAlignMode::WebKitLeft:
    case TextAlignMode::Start:
        return lineLogicalLeft;
    case TextAlignMode::Right:
    case TextAlignMode::WebKitRight:
    case TextAlignMode::End:
        return lineLogicalLeft + std::max(remainingWidth, 0_lu);
    case TextAlignMode::Center:
    case TextAlignMode::WebKitCenter:
        return lineLogicalLeft + std::max(remainingWidth / 2, 0_lu);
    case TextAlignMode::Justify:
        ASSERT_NOT_REACHED();
        break;
    }
    ASSERT_NOT_REACHED();
    return lineLogicalLeft;
}

void InlineFormattingContext::LineLayout::justifyRuns(Line& line)
{
    auto& inlineRuns = line.runs();
    auto& lastInlineRun = inlineRuns.last();

    // Adjust (forbid) trailing expansion for the last text run on line.
    auto expansionBehavior = lastInlineRun.expansionOpportunity().behavior;
    // Remove allow and add forbid.
    expansionBehavior ^= AllowTrailingExpansion;
    expansionBehavior |= ForbidTrailingExpansion;
    lastInlineRun.expansionOpportunity().behavior = expansionBehavior;

    // Collect expansion opportunities and justify the runs.
    auto widthToDistribute = line.availableWidth();
    if (widthToDistribute <= 0)
        return;

    auto expansionOpportunities = 0;
    for (auto& inlineRun : inlineRuns)
        expansionOpportunities += inlineRun.expansionOpportunity().count;

    if (!expansionOpportunities)
        return;

    float expansion = widthToDistribute.toFloat() / expansionOpportunities;
    LayoutUnit accumulatedExpansion;
    for (auto& inlineRun : inlineRuns) {
        auto expansionForRun = inlineRun.expansionOpportunity().count * expansion;

        inlineRun.expansionOpportunity().expansion = expansionForRun;
        inlineRun.setLogicalLeft(inlineRun.logicalLeft() + accumulatedExpansion);
        inlineRun.setLogicalWidth(inlineRun.logicalWidth() + expansionForRun);
        accumulatedExpansion += expansionForRun;
    }
}

void InlineFormattingContext::LineLayout::computeExpansionOpportunities(Line& line, const InlineRunProvider::Run& run, InlineRunProvider::Run::Type lastRunType) const
{
    auto isExpansionOpportunity = [](auto currentRunIsWhitespace, auto lastRunIsWhitespace) {
        return currentRunIsWhitespace || (!currentRunIsWhitespace && !lastRunIsWhitespace);
    };

    auto expansionBehavior = [](auto isAtExpansionOpportunity) {
        ExpansionBehavior expansionBehavior = AllowTrailingExpansion;
        expansionBehavior |= isAtExpansionOpportunity ? ForbidLeadingExpansion : AllowLeadingExpansion;
        return expansionBehavior;
    };

    auto isAtExpansionOpportunity = isExpansionOpportunity(run.isWhitespace(), lastRunType == InlineRunProvider::Run::Type::Whitespace);

    auto& currentInlineRun = line.runs().last();
    auto& expansionOpportunity = currentInlineRun.expansionOpportunity();
    if (isAtExpansionOpportunity)
        ++expansionOpportunity.count;

    expansionOpportunity.behavior = expansionBehavior(isAtExpansionOpportunity);
}

void InlineFormattingContext::LineLayout::alignRuns(TextAlignMode textAlign, Line& line,  IsLastLine isLastLine) const
{
    auto adjutedTextAlignment = textAlign != TextAlignMode::Justify ? textAlign : isLastLine == IsLastLine::No ? TextAlignMode::Justify : TextAlignMode::Left;
    if (adjutedTextAlignment == TextAlignMode::Justify) {
        justifyRuns(line);
        return;
    }

    auto lineLogicalLeft = line.contentLogicalLeft();
    auto adjustedLogicalLeft = adjustedLineLogicalLeft(adjutedTextAlignment, lineLogicalLeft, line.availableWidth());
    if (adjustedLogicalLeft == lineLogicalLeft)
        return;

    auto delta = adjustedLogicalLeft - lineLogicalLeft;
    for (auto& inlineRun : line.runs())
        inlineRun.setLogicalLeft(inlineRun.logicalLeft() + delta);
}

LayoutUnit InlineFormattingContext::LineLayout::runWidth(const InlineContent& inlineContent, const InlineItem& inlineItem, ItemPosition from, unsigned length, LayoutUnit contentLogicalLeft) const 
{
    LayoutUnit width;
    auto startPosition = from;
    auto iterator = inlineContent.find(const_cast<InlineItem*>(&inlineItem));
#if !ASSERT_DISABLED
    auto inlineItemEnd = inlineContent.end();
#endif
    while (length) {
        ASSERT(iterator != inlineItemEnd);
        auto& currentInlineItem = **iterator;
        auto endPosition = std::min<ItemPosition>(startPosition + length, currentInlineItem.textContent().length());
        auto textWidth = TextUtil::width(currentInlineItem, startPosition, endPosition, contentLogicalLeft);

        contentLogicalLeft += textWidth;
        width += textWidth;
        length -= (endPosition - startPosition);

        startPosition = 0;
        ++iterator;
    }
    return width;
}

}
}

#endif
