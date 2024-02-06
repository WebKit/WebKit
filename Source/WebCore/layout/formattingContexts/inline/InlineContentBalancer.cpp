/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "InlineContentBalancer.h"

#include "InlineLineBuilder.h"
#include "RenderStyleInlines.h"
#include <limits>
#include <wtf/MathExtras.h>

namespace WebCore {
namespace Layout {

// Ideally, the act of balancing inline content will use the same number of lines as if the inline content
// was laid out via `text-wrap: wrap`. However, adhering to this ideal is expensive (quadratic in the number
// of break opportunities), and not caring about this ideal will allow us to use a more efficient algorithm.
// Typically, if inline content spans many lines, the likelihood of someone caring about the vertical space
// used decreases. So, we ignore this ideal number of lines requirement beyond this threshold.
static const size_t maximumLinesToBalanceWithLineRequirement { 12 };

static Vector<size_t> computeBreakOpportunities(const InlineItemList& inlineItemList, InlineItemRange range)
{
    Vector<size_t> breakOpportunities;
    size_t currentIndex = range.startIndex();
    while (currentIndex < range.endIndex()) {
        currentIndex = InlineFormattingUtils::nextWrapOpportunity(currentIndex, range, inlineItemList);
        breakOpportunities.append(currentIndex);
    }
    return breakOpportunities;
}


static float computeCost(InlineLayoutUnit candidateLineWidth, InlineLayoutUnit idealLineWidth)
{
    auto difference = idealLineWidth - candidateLineWidth;
    return difference * difference;
};

static bool containsTrailingSoftHyphen(const InlineItem& inlineItem)
{
    if (inlineItem.style().hyphens() == Hyphens::None)
        return false;
    if (!inlineItem.isText())
        return false;
    return downcast<InlineTextItem>(inlineItem).hasTrailingSoftHyphen();
}

static bool containsPreservedTab(const InlineItem& inlineItem)
{
    if (!inlineItem.isText())
        return false;
    const auto& textItem = downcast<InlineTextItem>(inlineItem);
    if (!textItem.isWhitespace())
        return false;
    const auto& textBox = textItem.inlineTextBox();
    if (!TextUtil::shouldPreserveSpacesAndTabs(textBox))
        return false;
    auto start = textItem.start();
    auto length = textItem.length();
    const auto& textContent = textBox.content();
    for (size_t index = start; index < start + length; index++) {
        if (textContent[index] == tabCharacter)
            return true;
    }
    return false;
}

static bool cannotBalanceInlineItem(const InlineItem& inlineItem)
{
    if (!inlineItem.layoutBox().isInlineLevelBox())
        return true;
    if (containsTrailingSoftHyphen(inlineItem))
        return true;
    if (containsPreservedTab(inlineItem))
        return true;
    if (inlineItem.style().boxDecorationBreak() == BoxDecorationBreak::Clone)
        return true;
    return false;
}

InlineContentBalancer::InlineContentBalancer(InlineFormattingContext& inlineFormattingContext, const InlineItemList& inlineItemList, const HorizontalConstraints& horizontalConstraints)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_inlineItemList(inlineItemList)
    , m_horizontalConstraints(horizontalConstraints)
{
    initialize();
}

void InlineContentBalancer::initialize()
{
    auto lineClamp = m_inlineFormattingContext.layoutState().parentBlockLayoutState().lineClamp();
    auto numberOfVisibleLinesAllowed = lineClamp ? std::make_optional(lineClamp->allowedLineCount()) : std::nullopt;

    if (!m_inlineFormattingContext.layoutState().placedFloats().isEmpty()) {
        m_cannotBalanceContent = true;
        return;
    }

    // if we have a single line content, we don't have anything to be balanced.
    if (numberOfVisibleLinesAllowed == 1) {
        m_hasSingleLineVisibleContent = true;
        return;
    }

    m_numberOfInlineItems = m_inlineItemList.size();
    m_maximumLineWidth = m_horizontalConstraints.logicalWidth;

    // Compute inline item widths beforehand to speed up later computations
    m_inlineItemWidths.reserveCapacity(m_numberOfInlineItems);
    for (size_t i = 0; i < m_numberOfInlineItems; i++) {
        const auto& item = m_inlineItemList[i];
        if (cannotBalanceInlineItem(item)) {
            m_cannotBalanceContent = true;
            return;
        }
        m_inlineItemWidths.append(m_inlineFormattingContext.formattingUtils().inlineItemWidth(item, 0, false));
        m_firstLineStyleInlineItemWidths.append(m_inlineFormattingContext.formattingUtils().inlineItemWidth(item, 0, true));
    }

    // Perform a line layout with `text-wrap: wrap` to compute useful metrics such as:
    //  - the number of lines used
    //  - the original widths of each line
    //  - forced break locations
    auto layoutRange = InlineItemRange { 0, m_inlineItemList.size() };
    auto lineBuilder = LineBuilder { m_inlineFormattingContext, m_horizontalConstraints, m_inlineItemList };
    auto previousLineEnd = std::optional<InlineItemPosition> { };
    auto previousLine = std::optional<PreviousLine> { };
    auto lineIndex = 0lu;
    while (!layoutRange.isEmpty()) {
        auto lineInitialRect = InlineRect { 0.f, m_horizontalConstraints.logicalLeft, m_horizontalConstraints.logicalWidth, 0.f };
        auto lineLayoutResult = lineBuilder.layoutInlineContent({ layoutRange, lineInitialRect }, previousLine);

        // Record relevant geometry measurements from one line layout
        m_originalLineInlineItemRanges.append(lineLayoutResult.inlineItemRange);
        m_originalLineEndsWithForcedBreak.append(!lineLayoutResult.inlineContent.isEmpty() && lineLayoutResult.inlineContent.last().isLineBreak());
        bool useFirstLineStyle = !lineIndex;
        bool isFirstLineInChunk = !lineIndex || m_originalLineEndsWithForcedBreak[lineIndex - 1];
        SlidingWidth lineSlidingWidth { *this, m_inlineItemList, lineLayoutResult.inlineItemRange.startIndex(), lineLayoutResult.inlineItemRange.endIndex(), useFirstLineStyle, isFirstLineInChunk };
        auto previousLineEndsWithLineBreak = lineIndex ? std::optional<bool> { m_originalLineEndsWithForcedBreak[lineIndex - 1] } : std::nullopt;
        auto textIndent = m_inlineFormattingContext.formattingUtils().computedTextIndent(InlineFormattingUtils::IsIntrinsicWidthMode::No, previousLineEndsWithLineBreak, m_maximumLineWidth);
        m_originalLineWidths.append(textIndent + lineSlidingWidth.width());

        // If next line count would match (or exceed) the number of visible lines due to line-clamp, we can bail out early.
        if (numberOfVisibleLinesAllowed && (lineIndex + 1 >= numberOfVisibleLinesAllowed))
            break;

        layoutRange.start = InlineFormattingUtils::leadingInlineItemPositionForNextLine(lineLayoutResult.inlineItemRange.end, previousLineEnd, !lineLayoutResult.floatContent.hasIntrusiveFloat.isEmpty(), layoutRange.end);
        previousLineEnd = layoutRange.start;
        previousLine = PreviousLine { lineIndex, lineLayoutResult.contentGeometry.trailingOverflowingContentWidth, !lineLayoutResult.inlineContent.isEmpty() && lineLayoutResult.inlineContent.last().isLineBreak(), !lineLayoutResult.inlineContent.isEmpty(), lineLayoutResult.directionality.inlineBaseDirection, WTFMove(lineLayoutResult.floatContent.suspendedFloats) };
        lineIndex++;
    }

    m_numberOfLinesInOriginalLayout = lineIndex;
}

std::optional<Vector<LayoutUnit>> InlineContentBalancer::computeBalanceConstraints()
{
    if (m_cannotBalanceContent || m_hasSingleLineVisibleContent)
        return std::nullopt;

    // If forced line breaks exist, then we can balance each forced-break-delimited
    // chunk of text separately. This helps simplify first line/indentation logic.
    Vector<size_t> chunkSizes; // Number of lines per chunk of text
    size_t currentChunkSize = 0;
    for (size_t i = 0; i < m_originalLineInlineItemRanges.size(); i++) {
        currentChunkSize++;
        if (m_originalLineEndsWithForcedBreak[i]) {
            chunkSizes.append(currentChunkSize);
            currentChunkSize = 0;
        }
    }
    if (currentChunkSize > 0)
        chunkSizes.append(currentChunkSize);

    // Balance each chunk
    size_t startLine = 0;
    Vector<LayoutUnit> balancedLineWidths;
    for (auto chunkSize : chunkSizes) {
        bool isFirstChunk = !startLine;
        auto rangeToBalance = InlineItemRange { m_originalLineInlineItemRanges[startLine].startIndex(), m_originalLineInlineItemRanges[startLine + chunkSize - 1].endIndex() };
        InlineLayoutUnit totalWidth = 0;
        for (size_t i = 0; i < chunkSize; i++)
            totalWidth += m_originalLineWidths[startLine + i];
        InlineLayoutUnit idealLineWidth = totalWidth / chunkSize;

        std::optional<Vector<LayoutUnit>> balancedLineWidthsForChunk;
        if (m_numberOfLinesInOriginalLayout <= maximumLinesToBalanceWithLineRequirement)
            balancedLineWidthsForChunk = balanceRangeWithLineRequirement(rangeToBalance, idealLineWidth, chunkSize, isFirstChunk);
        else
            balancedLineWidthsForChunk = balanceRangeWithNoLineRequirement(rangeToBalance, idealLineWidth, isFirstChunk);

        if (!balancedLineWidthsForChunk) {
            for (size_t i = 0; i < chunkSize; i++)
                balancedLineWidths.append(m_maximumLineWidth);
        } else {
            for (size_t i = 0; i < balancedLineWidthsForChunk->size(); i++)
                balancedLineWidths.append(balancedLineWidthsForChunk.value()[i]);
        }

        startLine += chunkSize;
    }

    return balancedLineWidths;
}

std::optional<Vector<LayoutUnit>> InlineContentBalancer::balanceRangeWithLineRequirement(InlineItemRange range, InlineLayoutUnit idealLineWidth, size_t numberOfLines, bool isFirstChunk)
{
    // breakOpportunities holds the indices i such that a line break can occur before m_inlineItemList[i].
    auto breakOpportunities = computeBreakOpportunities(m_inlineItemList, range);

    // We need a dummy break opportunity at the beginning for algorithmic base case purposes
    breakOpportunities.insert(0, range.startIndex());
    auto numberOfBreakOpportunities = breakOpportunities.size();

    // Indentation offsets
    auto previousLineEndsWithLineBreak = isFirstChunk ? std::nullopt : std::optional<bool> { true };
    auto firstLineTextIndent = m_inlineFormattingContext.formattingUtils().computedTextIndent(InlineFormattingUtils::IsIntrinsicWidthMode::No, previousLineEndsWithLineBreak, m_maximumLineWidth);
    auto textIndent = m_inlineFormattingContext.formattingUtils().computedTextIndent(InlineFormattingUtils::IsIntrinsicWidthMode::No, false, m_maximumLineWidth);

    struct Entry {
        float accumulatedCost { std::numeric_limits<float>::infinity() };
        size_t previousBreakIndex { 0 };
    };

    // state[i][j] holds the optimal set of line breaks where the jth line break (1-indexed) is
    // right before m_inlineItemList[breakOpportunities[i]]. "Optimal" in this context means the
    // lowest possible accumulated cost.
    Vector<Vector<Entry>> state(numberOfBreakOpportunities, Vector<Entry>(numberOfLines + 1));
    state[0][0].accumulatedCost = 0.f;

    // Special case the first line because of ::first-line styling, indentation, etc.
    SlidingWidth firstLineSlidingWidth { *this, m_inlineItemList, range.startIndex(), range.startIndex(), isFirstChunk, true };
    for (size_t breakIndex = 1; breakIndex < numberOfBreakOpportunities; breakIndex++) {
        auto end = breakOpportunities[breakIndex];
        firstLineSlidingWidth.advanceEndTo(end);

        auto firstLineCandidateWidth = firstLineSlidingWidth.width() + firstLineTextIndent;
        if (firstLineCandidateWidth > m_maximumLineWidth)
            break;

        auto cost = computeCost(firstLineCandidateWidth, idealLineWidth);
        state[breakIndex][1].accumulatedCost = cost;
    }

    // breakOpportunities[firstStartIndex] is the first possible starting position for a candidate line that is NOT the first line
    size_t firstStartIndex = 1;
    SlidingWidth slidingWidth { *this, m_inlineItemList, breakOpportunities[firstStartIndex], breakOpportunities[firstStartIndex], false, false };
    for (size_t breakIndex = 1; breakIndex < numberOfBreakOpportunities; breakIndex++) {
        size_t end = breakOpportunities[breakIndex];
        slidingWidth.advanceEndTo(end);

        // We prune our search space by limiting the possible starting positions for our candidate line.
        while (textIndent + slidingWidth.width() > m_maximumLineWidth) {
            firstStartIndex++;
            if (firstStartIndex > breakIndex)
                break;
            slidingWidth.advanceStartTo(breakOpportunities[firstStartIndex]);
        }

        // Evaluate all possible lines that break before m_inlineItemList[end]
        auto innerSlidingWidth = slidingWidth;
        for (size_t startIndex = firstStartIndex; startIndex < breakIndex; startIndex++) {
            size_t start = breakOpportunities[startIndex];
            ASSERT(start != range.startIndex());
            innerSlidingWidth.advanceStartTo(start);
            auto candidateLineWidth = textIndent + innerSlidingWidth.width();
            auto candidateLineCost = computeCost(candidateLineWidth, idealLineWidth);
            ASSERT(candidateLineWidth <= m_maximumLineWidth);

            // Compute the cost of this line based on the line index
            for (size_t lineIndex = 1; lineIndex <= numberOfLines; lineIndex++) {
                auto accumulatedCost = candidateLineCost + state[startIndex][lineIndex - 1].accumulatedCost;
                auto currentAccumulatedCost = state[breakIndex][lineIndex].accumulatedCost;
                if (accumulatedCost < currentAccumulatedCost || WTF::areEssentiallyEqual(accumulatedCost, currentAccumulatedCost)) {
                    state[breakIndex][lineIndex].accumulatedCost = accumulatedCost;
                    state[breakIndex][lineIndex].previousBreakIndex = startIndex;
                }
            }
        }
    }

    // Check if we found no solution
    if (std::isinf(state[numberOfBreakOpportunities - 1][numberOfLines].accumulatedCost))
        return std::nullopt;

    // breaks[i] equals the index into m_inlineItemList before which the ith line will break
    Vector<size_t> breaks(numberOfLines);
    size_t breakIndex = numberOfBreakOpportunities - 1;
    for (size_t line = numberOfLines; line > 0; line--) {
        breaks[line - 1] = breakOpportunities[breakIndex];
        breakIndex = state[breakIndex][line].previousBreakIndex;
    }

    // Compute final line widths
    Vector<LayoutUnit> lineWidths(numberOfLines);
    for (size_t i = 0; i < numberOfLines; i++) {
        auto start = !i ? range.startIndex() : breaks[i - 1];
        auto end = breaks[i];
        auto indentWidth = !i ? firstLineTextIndent : textIndent;
        SlidingWidth slidingWidth { *this, m_inlineItemList, start, end, !i && isFirstChunk, !i };
        lineWidths[i] = LayoutUnit::fromFloatCeil(indentWidth + slidingWidth.width() + LayoutUnit::epsilon());
    }

    return lineWidths;
}

std::optional<Vector<LayoutUnit>> InlineContentBalancer::balanceRangeWithNoLineRequirement(InlineItemRange range, InlineLayoutUnit idealLineWidth, bool isFirstChunk)
{
    // breakOpportunities holds the indices i such that a line break can occur before m_inlineItemList[i].
    auto breakOpportunities = computeBreakOpportunities(m_inlineItemList, range);

    // We need a dummy break opportunity at the beginning for algorithmic base case purposes
    breakOpportunities.insert(0, range.startIndex());
    auto numberOfBreakOpportunities = breakOpportunities.size();

    // Indentation offsets
    auto previousLineEndsWithLineBreak = isFirstChunk ? std::nullopt : std::optional<bool> { true };
    auto firstLineTextIndent = m_inlineFormattingContext.formattingUtils().computedTextIndent(InlineFormattingUtils::IsIntrinsicWidthMode::No, previousLineEndsWithLineBreak, m_maximumLineWidth);
    auto textIndent = m_inlineFormattingContext.formattingUtils().computedTextIndent(InlineFormattingUtils::IsIntrinsicWidthMode::No, false, m_maximumLineWidth);

    struct Entry {
        float accumulatedCost { std::numeric_limits<float>::infinity() };
        size_t previousBreakIndex { 0 };
    };

    // state[i] holds the optimal set of line breaks where the last line break is right
    // before m_inlineItemList[breakOpportunities[i]]. "Optimal" in this context means the
    // lowest possible accumulated cost.
    Vector<Entry> state(numberOfBreakOpportunities);
    state[0].accumulatedCost = 0.f;

    // Special case the first line because of ::first-line styling, indentation, etc.
    SlidingWidth firstLineSlidingWidth { *this, m_inlineItemList, range.startIndex(), range.startIndex(), isFirstChunk, true };
    for (size_t breakIndex = 1; breakIndex < numberOfBreakOpportunities; breakIndex++) {
        auto end = breakOpportunities[breakIndex];
        firstLineSlidingWidth.advanceEndTo(end);

        auto firstLineCandidateWidth = firstLineSlidingWidth.width() + firstLineTextIndent;
        if (firstLineCandidateWidth > m_maximumLineWidth)
            break;

        auto cost = computeCost(firstLineCandidateWidth, idealLineWidth);
        state[breakIndex].accumulatedCost = cost;
    }

    // breakOpportunities[firstStartIndex] is the first possible starting position for a candidate line that is NOT the first line
    size_t firstStartIndex = 1;
    SlidingWidth slidingWidth { *this, m_inlineItemList, breakOpportunities[firstStartIndex], breakOpportunities[firstStartIndex], false, false };
    for (size_t breakIndex = 1; breakIndex < numberOfBreakOpportunities; breakIndex++) {
        size_t end = breakOpportunities[breakIndex];
        slidingWidth.advanceEndTo(end);

        // We prune our search space by limiting the possible starting positions for our candidate line.
        while (textIndent + slidingWidth.width() > m_maximumLineWidth) {
            firstStartIndex++;
            if (firstStartIndex > breakIndex)
                break;
            slidingWidth.advanceStartTo(breakOpportunities[firstStartIndex]);
        }

        // Evaluate all possible lines that break before m_inlineItemList[end]
        auto innerSlidingWidth = slidingWidth;
        for (size_t startIndex = firstStartIndex; startIndex < breakIndex; startIndex++) {
            size_t start = breakOpportunities[startIndex];
            ASSERT(start != range.startIndex());
            innerSlidingWidth.advanceStartTo(start);
            auto candidateLineWidth = textIndent + innerSlidingWidth.width();
            auto candidateLineCost = computeCost(candidateLineWidth, idealLineWidth);
            ASSERT(candidateLineWidth <= m_maximumLineWidth);

            auto accumulatedCost = candidateLineCost + state[startIndex].accumulatedCost;
            if (accumulatedCost < state[breakIndex].accumulatedCost) {
                state[breakIndex].accumulatedCost = accumulatedCost;
                state[breakIndex].previousBreakIndex = startIndex;
            }
        }
    }

    // Check if we found no solution
    if (std::isinf(state[numberOfBreakOpportunities - 1].accumulatedCost))
        return std::nullopt;

    // breaks[i] equals the index into m_inlineItemList before which the ith line will break
    Vector<size_t> breaks;
    size_t breakIndex = numberOfBreakOpportunities - 1;
    do {
        breaks.append(breakOpportunities[breakIndex]);
        breakIndex = state[breakIndex].previousBreakIndex;
    } while (breakIndex);
    breaks.reverse();

    // Compute final line widths
    Vector<LayoutUnit> lineWidths(breaks.size());
    for (size_t i = 0; i < breaks.size(); i++) {
        auto start = !i ? range.startIndex() : breaks[i - 1];
        auto end = breaks[i];
        auto indentWidth = !i ? firstLineTextIndent : textIndent;
        SlidingWidth slidingWidth { *this, m_inlineItemList, start, end, !i && isFirstChunk, !i };
        lineWidths[i] = LayoutUnit::fromFloatCeil(indentWidth + slidingWidth.width() + LayoutUnit::epsilon());
    }

    return lineWidths;
}

InlineLayoutUnit InlineContentBalancer::inlineItemWidth(size_t inlineItemIndex, bool useFirstLineStyle) const
{
    return useFirstLineStyle ? m_firstLineStyleInlineItemWidths[inlineItemIndex] : m_inlineItemWidths[inlineItemIndex];
}

bool InlineContentBalancer::shouldTrimLeading(size_t inlineItemIndex, bool useFirstLineStyle, bool isFirstLineInChunk) const
{
    auto& inlineItem = m_inlineItemList[inlineItemIndex];
    auto& style = useFirstLineStyle ? inlineItem.firstLineStyle() : inlineItem.style();

    // Handle line break first so we can focus on other types of white space
    if (inlineItem.isLineBreak())
        return true;

    if (inlineItem.isText()) {
        auto& textItem = downcast<InlineTextItem>(inlineItem);
        if (textItem.isWhitespace()) {
            bool isFirstLineLeadingPreservedWhiteSpace = style.whiteSpaceCollapse() == WhiteSpaceCollapse::Preserve && isFirstLineInChunk;
            return !isFirstLineLeadingPreservedWhiteSpace && style.whiteSpaceCollapse() != WhiteSpaceCollapse::BreakSpaces;
        }
        return false;
    }

    if (inlineItemWidth(inlineItemIndex, useFirstLineStyle) <= 0)
        return true;

    return false;
}

bool InlineContentBalancer::shouldTrimTrailing(size_t inlineItemIndex, bool useFirstLineStyle) const
{
    auto& inlineItem = m_inlineItemList[inlineItemIndex];
    auto& style = useFirstLineStyle ? inlineItem.firstLineStyle() : inlineItem.style();

    // Handle line break first so we can focus on other types of white space
    if (inlineItem.isLineBreak())
        return true;

    if (inlineItem.isText()) {
        auto& textItem = downcast<InlineTextItem>(inlineItem);
        if (textItem.isWhitespace())
            return style.whiteSpaceCollapse() != WhiteSpaceCollapse::BreakSpaces;
        return false;
    }

    if (inlineItemWidth(inlineItemIndex, useFirstLineStyle) <= 0)
        return true;

    return false;
}

InlineContentBalancer::SlidingWidth::SlidingWidth(const InlineContentBalancer& inlineContentBalancer, const InlineItemList& inlineItemList, size_t start, size_t end, bool useFirstLineStyle, bool isFirstLineInChunk)
    : m_inlineContentBalancer(inlineContentBalancer)
    , m_inlineItemList(inlineItemList)
    , m_start(start)
    , m_end(start)
    , m_useFirstLineStyle(useFirstLineStyle)
    , m_isFirstLineInChunk(isFirstLineInChunk)
{
    ASSERT(start <= end);
    while (m_end < end)
        advanceEnd();
}

InlineLayoutUnit InlineContentBalancer::SlidingWidth::width()
{
    return m_totalWidth - m_leadingTrimmableWidth - m_trailingTrimmableWidth;
}

void InlineContentBalancer::SlidingWidth::advanceStart()
{
    ASSERT(m_start < m_end);
    auto startItemIndex = m_start;
    auto startItemWidth = m_inlineContentBalancer.inlineItemWidth(startItemIndex, m_useFirstLineStyle);
    m_totalWidth -= startItemWidth;
    m_start++;

    if (m_inlineContentBalancer.shouldTrimLeading(startItemIndex, m_useFirstLineStyle, m_isFirstLineInChunk)) {
        m_leadingTrimmableWidth -= startItemWidth;
        return;
    }

    m_firstLeadingNonTrimmedItem = std::nullopt;
    m_leadingTrimmableWidth = 0;
    for (auto current = m_start; current < m_end; current++) {
        if (!m_inlineContentBalancer.shouldTrimLeading(current, m_useFirstLineStyle, m_isFirstLineInChunk)) {
            m_firstLeadingNonTrimmedItem = current;
            break;
        }
        m_leadingTrimmableWidth += m_inlineContentBalancer.inlineItemWidth(current, m_useFirstLineStyle);
    }

    // Update trailing logic if necessary:
    //   1: Check if the removed start item was the first trailing item
    //   2: Check if the first non trimmed leading item surpassed the first trailing item
    // In both cases, we should have m_leadingTrimmableWidth + m_trailingTrimmableWidth = m_totalWidth
    if (m_leadingTrimmableWidth + m_trailingTrimmableWidth > m_totalWidth)
        m_trailingTrimmableWidth = m_totalWidth - m_leadingTrimmableWidth;
}

void InlineContentBalancer::SlidingWidth::advanceStartTo(size_t newStart)
{
    ASSERT(m_start <= newStart);
    while (m_start < newStart)
        advanceStart();
}

void InlineContentBalancer::SlidingWidth::advanceEnd()
{
    ASSERT(m_end < m_inlineItemList.size());
    auto endItemIndex = m_end;
    auto endItemWidth = m_inlineContentBalancer.inlineItemWidth(endItemIndex, m_useFirstLineStyle);
    m_totalWidth += endItemWidth;
    m_end++;

    if (!m_firstLeadingNonTrimmedItem.has_value()) {
        if (m_inlineContentBalancer.shouldTrimLeading(endItemIndex, m_useFirstLineStyle, m_isFirstLineInChunk)) {
            m_leadingTrimmableWidth += endItemWidth;
            return;
        }
        m_firstLeadingNonTrimmedItem = endItemIndex;
        return;
    }

    if (m_inlineContentBalancer.shouldTrimTrailing(m_end - 1, m_useFirstLineStyle)) {
        m_trailingTrimmableWidth += endItemWidth;
        return;
    }

    m_trailingTrimmableWidth = 0;
}

void InlineContentBalancer::SlidingWidth::advanceEndTo(size_t newEnd)
{
    ASSERT(m_end <= newEnd);
    while (m_end < newEnd)
        advanceEnd();
}

}
}
