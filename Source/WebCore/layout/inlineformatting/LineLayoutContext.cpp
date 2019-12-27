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
#include "LineLayoutContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutBox.h"
#include "TextUtil.h"

namespace WebCore {
namespace Layout {

struct LineCandidateContent {
    void append(const InlineItem&, Optional<InlineLayoutUnit> logicalWidth);

    bool hasIntrusiveFloats() const { return !m_floats.isEmpty(); }
    const LineBreaker::RunList& runs() const { return m_runs; }
    const LineLayoutContext::FloatList& floats() const { return m_floats; }

    bool isLineBreak() const { return m_isLineBreak; }
    void setIsLineBreak();

private:
    LineBreaker::RunList m_runs;
    LineLayoutContext::FloatList m_floats;
    bool m_isLineBreak { false };
};

void LineCandidateContent::append(const InlineItem& inlineItem, Optional<InlineLayoutUnit> logicalWidth)
{
    if (inlineItem.isFloat())
        return m_floats.append(makeWeakPtr(inlineItem));
    m_runs.append({ inlineItem, *logicalWidth });
}

void LineCandidateContent::setIsLineBreak()
{
    ASSERT(!hasIntrusiveFloats());
    ASSERT(runs().isEmpty());
    m_isLineBreak = true;
}

static InlineLayoutUnit inlineItemWidth(const FormattingContext& formattingContext, const InlineItem& inlineItem, InlineLayoutUnit contentLogicalLeft)
{
    if (inlineItem.isLineBreak())
        return 0;

    if (is<InlineTextItem>(inlineItem)) {
        auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
        if (auto contentWidth = inlineTextItem.width())
            return *contentWidth;
        auto end = inlineTextItem.isCollapsible() ? inlineTextItem.start() + 1 : inlineTextItem.end();
        return TextUtil::width(inlineTextItem, inlineTextItem.start(), end, contentLogicalLeft);
    }

    auto& layoutBox = inlineItem.layoutBox();
    auto& boxGeometry = formattingContext.geometryForBox(layoutBox);

    if (layoutBox.isFloatingPositioned())
        return boxGeometry.marginBoxWidth();

    if (layoutBox.replaced())
        return boxGeometry.width();

    if (inlineItem.isContainerStart())
        return boxGeometry.marginStart() + boxGeometry.borderLeft() + boxGeometry.paddingLeft().valueOr(0);

    if (inlineItem.isContainerEnd())
        return boxGeometry.marginEnd() + boxGeometry.borderRight() + boxGeometry.paddingRight().valueOr(0);

    // Non-replaced inline box (e.g. inline-block)
    return boxGeometry.width();
}

LineLayoutContext::LineLayoutContext(const InlineFormattingContext& inlineFormattingContext, const Container& formattingContextRoot, const InlineItems& inlineItems)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_formattingContextRoot(formattingContextRoot)
    , m_inlineItems(inlineItems)
{
}

LineLayoutContext::LineContent LineLayoutContext::layoutLine(LineBuilder& line, unsigned leadingInlineItemIndex, Optional<unsigned> partialLeadingContentLength)
{
    auto reset = [&] {
        ASSERT(m_floats.isEmpty());
        m_partialTrailingTextItem = { };
        m_partialLeadingTextItem = { };
    };
    reset();
    auto currentItemIndex = leadingInlineItemIndex;
    unsigned committedInlineItemCount = 0;
    while (currentItemIndex < m_inlineItems.size()) {
        // 1. Collect the set of runs that we can commit to the line as one entity e.g. <span>text_and_span_start_span_end</span>.
        // 2. Apply floats and shrink the available horizontal space e.g. <span>intru_<div style="float: left"></div>sive_float</span>.
        // 3. Check if the content fits the line and commit the content accordingly (full, partial or not commit at all).
        // 4. Return if we are at the end of the line either by not being able to fit more content or because of an explicit line break.
        auto candidateContent = nextContentForLine(currentItemIndex, partialLeadingContentLength, line.lineBox().logicalWidth());
        if (candidateContent.isLineBreak()) {
            line.append(*m_inlineItems[currentItemIndex], 0);
            return close(line, leadingInlineItemIndex, ++committedInlineItemCount, { });
        }
        if (candidateContent.hasIntrusiveFloats()) {
            // Add floats first because they shrink the available horizontal space for the rest of the content.
            auto floatContent = addFloatItems(line, candidateContent.floats());
            committedInlineItemCount += floatContent.count;
            if (floatContent.isEndOfLine == LineBreaker::IsEndOfLine::Yes) {
                // Floats take up all the horizontal space.
                return close(line, leadingInlineItemIndex, committedInlineItemCount, { });
            }
        }
        if (!candidateContent.runs().isEmpty()) {
            // Now check if we can put this content on the current line.
            auto committedContent = placeInlineContentOnCurrentLine(line, candidateContent.runs());
            committedInlineItemCount += committedContent.count;
            if (committedContent.isEndOfLine == LineBreaker::IsEndOfLine::Yes) {
                // We can't place any more items on the current line.
                return close(line, leadingInlineItemIndex, committedInlineItemCount, committedContent.partialContent);
            }
        }
        currentItemIndex = leadingInlineItemIndex + committedInlineItemCount;
        partialLeadingContentLength = { };
    }
    // Looks like we've run out of runs.
    return close(line, leadingInlineItemIndex, committedInlineItemCount, { });
}

LineLayoutContext::LineContent LineLayoutContext::close(LineBuilder& line, unsigned leadingInlineItemIndex, unsigned committedInlineItemCount, Optional<LineContent::PartialContent> partialContent)
{
    ASSERT(committedInlineItemCount || line.hasIntrusiveFloat());
    if (!committedInlineItemCount)
        return LineContent { { }, { }, WTFMove(m_floats), line.close(), line.lineBox() };

    // Adjust hyphenated line count.
    if (partialContent && partialContent->trailingContentNeedsHyphen)
        ++m_successiveHyphenatedLineCount;
    else
        m_successiveHyphenatedLineCount = 0;

    auto trailingInlineItemIndex = leadingInlineItemIndex + committedInlineItemCount - 1;
    auto isLastLineWithInlineContent = [&] {
        if (trailingInlineItemIndex == m_inlineItems.size() - 1)
            return LineBuilder::IsLastLineWithInlineContent::Yes;
        if (partialContent)
            return LineBuilder::IsLastLineWithInlineContent::No;
        // Omit floats to see if this is the last line with inline content.
        for (auto i = m_inlineItems.size(); i--;) {
            if (!m_inlineItems[i]->isFloat())
                return i == trailingInlineItemIndex ? LineBuilder::IsLastLineWithInlineContent::Yes : LineBuilder::IsLastLineWithInlineContent::No;
        }
        // There has to be at least one non-float item.
        ASSERT_NOT_REACHED();
        return LineBuilder::IsLastLineWithInlineContent::No;
    }();
    return LineContent { trailingInlineItemIndex, partialContent, WTFMove(m_floats), line.close(isLastLineWithInlineContent), line.lineBox() };
}

LineCandidateContent LineLayoutContext::nextContentForLine(unsigned inlineItemIndex, Optional<unsigned> partialLeadingContentLength, InlineLayoutUnit currentLogicalRight)
{
    ASSERT(inlineItemIndex < m_inlineItems.size());
    // 1. Simply add any overflow content from the previous line to the candidate content. It's always a text content.
    // 2. Find the next soft wrap position or explicit line break.
    // 3. Collect floats between the inline content.
    auto softWrapOpportunityIndex = LineBreaker::nextWrapOpportunity(m_inlineItems, inlineItemIndex);
    // softWrapOpportunityIndex == m_inlineItems.size() means we don't have any wrap opportunity in this content.
    ASSERT(softWrapOpportunityIndex <= m_inlineItems.size());

    auto candidateContent = LineCandidateContent { };
    if (partialLeadingContentLength) {
        // Handle leading partial content first (split text from the previous line).
        // Construct a partial leading inline item.
        m_partialLeadingTextItem = downcast<InlineTextItem>(*m_inlineItems[inlineItemIndex]).right(*partialLeadingContentLength);
        auto itemWidth = inlineItemWidth(formattingContext(), *m_partialLeadingTextItem, currentLogicalRight);
        candidateContent.append(*m_partialLeadingTextItem, itemWidth);
        currentLogicalRight += itemWidth;
        ++inlineItemIndex;
    }

    // Are we wrapping at a line break?
    auto isSingleItem = inlineItemIndex + 1 == softWrapOpportunityIndex;
    if (isSingleItem && m_inlineItems[inlineItemIndex]->isLineBreak()) {
        candidateContent.setIsLineBreak();
        return candidateContent;
    }

    for (auto index = inlineItemIndex; index < softWrapOpportunityIndex; ++index) {
        auto& inlineItem = *m_inlineItems[index];
        ASSERT(!inlineItem.isLineBreak());
        if (inlineItem.isFloat()) {
            // Floats are not part of the line context.
            // FIXME: Check if their width should be added to currentLogicalRight.
            candidateContent.append(inlineItem, { });
            continue;
        }
        auto inlineItenmWidth = inlineItemWidth(formattingContext(), inlineItem, currentLogicalRight);
        candidateContent.append(inlineItem, inlineItenmWidth);
        currentLogicalRight += inlineItenmWidth;
    }
    return candidateContent;
}

LineLayoutContext::CommittedContent LineLayoutContext::addFloatItems(LineBuilder& line, const FloatList& floats)
{
    size_t committedFloatItemCount = 0;
    for (auto& floatItem : floats) {
        auto logicalWidth = inlineItemWidth(formattingContext(), *floatItem, { });

        auto lineIsConsideredEmpty = line.isVisuallyEmpty() && !line.hasIntrusiveFloat();
        if (LineBreaker().shouldWrapFloatBox(logicalWidth, line.availableWidth() + line.trailingCollapsibleWidth(), lineIsConsideredEmpty))
            return { LineBreaker::IsEndOfLine::Yes, committedFloatItemCount, { } };
        // This float can sit on the current line.
        ++committedFloatItemCount;
        auto& floatBox = floatItem->layoutBox();
        // Shrink available space for current line and move existing inline runs.
        line.setHasIntrusiveFloat();
        if (floatBox.isLeftFloatingPositioned())
            line.moveLogicalLeft(logicalWidth);
        else
            line.moveLogicalRight(logicalWidth);
        m_floats.append(floatItem);
    }
    return { LineBreaker::IsEndOfLine::No, committedFloatItemCount, { } };
}

LineLayoutContext::CommittedContent LineLayoutContext::placeInlineContentOnCurrentLine(LineBuilder& line, const LineBreaker::RunList& runs)
{
    auto shouldDisableHyphenation = [&] {
        auto& style = root().style();
        unsigned limitLines = style.hyphenationLimitLines() == RenderStyle::initialHyphenationLimitLines() ? std::numeric_limits<unsigned>::max() : style.hyphenationLimitLines();
        return m_successiveHyphenatedLineCount >= limitLines;
    };
    // Check if this new content fits.
    auto lineIsConsideredEmpty = line.isVisuallyEmpty() && !line.hasIntrusiveFloat();
    auto lineStatus = LineBreaker::LineStatus { line.availableWidth(), line.trailingCollapsibleWidth(), line.isTrailingRunFullyCollapsible(), lineIsConsideredEmpty };
    auto lineBreaker = LineBreaker { };

    if (shouldDisableHyphenation())
        lineBreaker.setHyphenationDisabled();

    auto breakingContext = lineBreaker.breakingContextForInlineContent(LineBreaker::ContinousContent { runs }, lineStatus);
    if (breakingContext.contentWrappingRule == LineBreaker::BreakingContext::ContentWrappingRule::Keep) {
        // This continuous content can be fully placed on the current line.
        commitContent(line, runs, { });
        return { breakingContext.isEndOfLine, runs.size(), { } };
    }

    if (breakingContext.contentWrappingRule == LineBreaker::BreakingContext::ContentWrappingRule::Push) {
        // This continuous content can't be placed on the current line. Nothing to commit at this time.
        return { breakingContext.isEndOfLine, 0, { } };
    }

    if (breakingContext.contentWrappingRule == LineBreaker::BreakingContext::ContentWrappingRule::Split) {
        // Commit the combination of full and partial content on the current line.
        ASSERT(breakingContext.partialTrailingContent);
        commitContent(line, runs, breakingContext.partialTrailingContent);
        // When splitting multiple runs <span style="word-break: break-all">text</span><span>content</span>, we might end up splitting them at run boundary.
        // It simply means we don't really have a partial run. Partial content yes, but not partial run.
        auto trailingRunIndex = breakingContext.partialTrailingContent->trailingRunIndex;
        auto committedInlineItemCount = trailingRunIndex + 1;
        if (!breakingContext.partialTrailingContent->partialRun)
            return { breakingContext.isEndOfLine, committedInlineItemCount, { } };

        auto partialRun = *breakingContext.partialTrailingContent->partialRun;
        auto& trailingInlineTextItem = downcast<InlineTextItem>(runs[trailingRunIndex].inlineItem);
        auto overflowLength = trailingInlineTextItem.length() - partialRun.length;
        return { breakingContext.isEndOfLine, committedInlineItemCount, LineContent::PartialContent { partialRun.needsHyphen, overflowLength } };
    }
    ASSERT_NOT_REACHED();
    return { LineBreaker::IsEndOfLine::No, 0, { } };
}

void LineLayoutContext::commitContent(LineBuilder& line, const LineBreaker::RunList& runs, Optional<LineBreaker::BreakingContext::PartialTrailingContent> partialTrailingContent)
{
    for (size_t index = 0; index < runs.size(); ++index) {
        auto& run = runs[index];
        if (partialTrailingContent && partialTrailingContent->trailingRunIndex == index) {
            ASSERT(run.inlineItem.isText());
            // Create and commit partial trailing item.
            if (auto partialRun = partialTrailingContent->partialRun) {
                auto& trailingInlineTextItem = downcast<InlineTextItem>(runs[partialTrailingContent->trailingRunIndex].inlineItem);
                // FIXME: LineBuilder should not hold on to the InlineItem.
                ASSERT(!m_partialTrailingTextItem);
                m_partialTrailingTextItem = trailingInlineTextItem.left(partialRun->length);
                line.append(*m_partialTrailingTextItem, partialRun->logicalWidth);
                return;
            }
            // The partial run is the last content to commit.
            line.append(run.inlineItem, run.logicalWidth);
            return;
        }
        line.append(run.inlineItem, run.logicalWidth);
    }
}

}
}

#endif
