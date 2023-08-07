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
#include <limits>

namespace WebCore {
namespace Layout {

InlineContentBalancer::InlineContentBalancer(const InlineFormattingContext& inlineFormattingContext, const InlineItems& inlineItems, const HorizontalConstraints& horizontalConstraints)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_inlineItems(inlineItems)
    , m_horizontalConstraints(horizontalConstraints)
{
    initialize();
}

InlineContentBalancer::~InlineContentBalancer()
{
    delete m_lineBuilder;
}

void InlineContentBalancer::initialize()
{
    m_numItems = m_inlineItems.size();
    m_maxLineWidth = m_horizontalConstraints.logicalWidth;

    // Always consider all inline items, as modification of one line can impact line layout
    // across earlier/later lines due to rebalancing.
    InlineItemRange layoutRange = InlineItemRange { 0, m_inlineItems.size() };

    // Perform a line layout with `text-wrap: wrap` to compute the number of lines needed for balancing
    auto floatingState = FloatingState { m_inlineFormattingContext.root() };
    auto parentBlockLayoutState = BlockLayoutState { floatingState, { } };
    auto inlineLayoutState = InlineLayoutState { parentBlockLayoutState, { } };
    m_lineBuilder = new LineBuilder { m_inlineFormattingContext, inlineLayoutState, floatingState, m_horizontalConstraints, m_inlineItems };
    auto previousLineEnd = std::optional<InlineItemPosition> { };
    auto previousLine = std::optional<PreviousLine> { };
    auto lineIndex = 0lu;
    while (!layoutRange.isEmpty()) {
        auto lineInitialRect = InlineRect { 0.f, m_horizontalConstraints.logicalLeft, m_horizontalConstraints.logicalWidth, 0.f };
        auto lineLayoutResult = m_lineBuilder->layoutInlineContent({ layoutRange, lineInitialRect }, previousLine);
        m_lineRanges.append(lineLayoutResult.inlineItemRange);
        m_lineEndsWithForcedBreak.append(!lineLayoutResult.inlineContent.isEmpty() && lineLayoutResult.inlineContent.last().isLineBreak());

        // Abort if float is encountered
        if (!floatingState.isEmpty()) {
            m_cannotBalanceContent = true;
            return;
        }

        layoutRange.start = InlineFormattingGeometry::leadingInlineItemPositionForNextLine(lineLayoutResult.inlineItemRange.end, previousLineEnd, layoutRange.end);
        previousLineEnd = layoutRange.start;
        previousLine = PreviousLine { lineIndex, lineLayoutResult.contentGeometry.trailingOverflowingContentWidth, !lineLayoutResult.inlineContent.isEmpty() && lineLayoutResult.inlineContent.last().isLineBreak(), lineLayoutResult.directionality.inlineBaseDirection, WTFMove(lineLayoutResult.floatContent.suspendedFloats) };

        lineIndex++;
    }

    // Record the total number of lines used when laying out inline content as if `text-wrap: wrap` is applied.
    m_numLines = lineIndex;
}

std::optional<Vector<LayoutUnit>> InlineContentBalancer::computeBalanceConstraints()
{
    if (m_cannotBalanceContent)
        return std::nullopt;

    // If forced line breaks exist, then we can balance each forced-break-delimited
    // chunk of text separately. This helps simplify the Knuth-Plass implementation
    // and any indentation logic.
    Vector<size_t> chunkSizes;
    size_t currChunkSize = 0;
    for (size_t i = 0; i < m_lineRanges.size(); i++) {
        currChunkSize++;
        if (m_lineEndsWithForcedBreak[i]) {
            chunkSizes.append(currChunkSize);
            currChunkSize = 0;
        }
    }
    if (currChunkSize > 0)
        chunkSizes.append(currChunkSize);

    // Balance each chunk
    size_t startLine = 0;
    Vector<LayoutUnit> lineWidths;
    for (auto chunkSize : chunkSizes) {
        std::optional<bool> previousChunkEndsWithLineBreak { std::nullopt };

        if (startLine > 0)
            previousChunkEndsWithLineBreak = m_lineEndsWithForcedBreak[startLine - 1];

        auto rangeToBalance = InlineItemRange { m_lineRanges[startLine].startIndex(), m_lineRanges[startLine + chunkSize - 1].endIndex() };
        std::optional<Vector<LayoutUnit>> lineRanges { std::nullopt };
        if (m_numLines > maxLinesToBalanceWithLineLimit)
            lineRanges = balanceRangeWithKnuthPlassNoLimit(rangeToBalance, previousChunkEndsWithLineBreak);
        else
            lineRanges = balanceRangeWithKnuthPlass(rangeToBalance, chunkSize, previousChunkEndsWithLineBreak);

        if (!lineRanges) {
            for (size_t i = 0; i < chunkSize; i++)
                lineWidths.append(m_maxLineWidth);
        } else {
            for (size_t i = 0; i < lineRanges->size(); i++)
                lineWidths.append(lineRanges.value()[i]);
        }

        startLine += chunkSize;
    }

    return lineWidths;
}

std::optional<Vector<LayoutUnit>> InlineContentBalancer::balanceRangeWithKnuthPlass(InlineItemRange range, unsigned long numLines, std::optional<bool> previousChunkEndsWithLineBreak)
{
    initializeWidthPrefixSums(range.startIndex(), range.endIndex());

    // breakOpportunities holds the indices i such that a line break can occur before m_inlineItems[i].
    auto breakOpportunities = m_lineBuilder->listBreakOpportunities(range);

    // We need a dummy break opportunity at the beginning for algorithmic base case purposes
    breakOpportunities.insert(0, range.startIndex());
    auto numBreakOpportunities = breakOpportunities.size();

    // Compute ideal line width via averaging the total content width across the number of lines
    // FIXME: Be smarter about first-line styling and spaces
    InlineLayoutUnit idealLineWidth = computeWidthBetween(range.startIndex(), range.endIndex(), false) / numLines;

    // Indentation
    auto firstLineTextIndent = m_inlineFormattingContext.formattingGeometry().computedTextIndent(InlineFormattingGeometry::IsIntrinsicWidthMode::No, previousChunkEndsWithLineBreak, m_maxLineWidth);
    auto textIndent = m_inlineFormattingContext.formattingGeometry().computedTextIndent(InlineFormattingGeometry::IsIntrinsicWidthMode::No, false, m_maxLineWidth);

    struct Entry {
        float accumulatedCost;
        size_t previousBreakIdx;
    };

    // state[i][j] holds an optimal solution considering all items up to the ith break opportunity (exclusive) with j lines
    Vector<Vector<Entry>> state(numBreakOpportunities, Vector<Entry>(numLines + 1, { std::numeric_limits<float>::infinity(), 0 }));
    state[0][0].accumulatedCost = 0.f;


    // Special case the first line because of ::first-line styling, indentation, etc.
    bool applyFirstLineStyling = !range.startIndex();
    for (size_t endIdx = 1; endIdx < numBreakOpportunities; endIdx++) {
        size_t start = breakOpportunities[0];
        size_t end = breakOpportunities[endIdx];

        auto candidateLineWidth = computeWidthBetween(start, end, applyFirstLineStyling) + firstLineTextIndent;
        if (candidateLineWidth > m_maxLineWidth)
            break;

        auto candidateLineCost = computeCost(candidateLineWidth, idealLineWidth);
        state[endIdx][1] = { candidateLineCost, 0 };
    }

    // breakOpportunities[firstStartIdx] is the first possible starting position for a candidate line that is NOT the first line
    size_t firstStartIdx = 1;
    for (size_t breakIdx = 1; breakIdx < numBreakOpportunities; breakIdx++) {
        size_t end = breakOpportunities[breakIdx];

        while (computeWidthBetween(breakOpportunities[firstStartIdx], end, false) + textIndent > m_maxLineWidth)
            firstStartIdx++;

        for (size_t startIdx = firstStartIdx; startIdx < breakIdx; startIdx++) {
            size_t start = breakOpportunities[startIdx];
            auto candidateLineWidth = computeWidthBetween(start, end, false) + textIndent;
            auto candidateLineCost = computeCost(candidateLineWidth, idealLineWidth);

            for (size_t lines = 2; lines <= numLines; lines++) {
                auto accumulatedCost = candidateLineCost + state[startIdx][lines - 1].accumulatedCost;
                if (accumulatedCost < state[breakIdx][lines].accumulatedCost) {
                    state[breakIdx][lines].accumulatedCost = accumulatedCost;
                    state[breakIdx][lines].previousBreakIdx = startIdx;
                }
            }
        }
    }

    // Check if we found no solution
    if (std::isinf(state[numBreakOpportunities - 1][numLines].accumulatedCost))
        return std::nullopt;

    // breaks[i] equals the index into m_inlineItems before which the ith line will break
    Vector<size_t> breaks(numLines);
    size_t breakIdx = numBreakOpportunities - 1;
    for (size_t line = numLines; line > 0; line--) {
        breaks[line - 1] = breakOpportunities[breakIdx];
        breakIdx = state[breakIdx][line].previousBreakIdx;
    }

    // Compute final line widths (adding indentation width)
    Vector<LayoutUnit> lineWidths(numLines);
    for (size_t i = 0; i < numLines; i++) {
        auto start = i > 0 ? breaks[i - 1] : range.startIndex();
        auto end = breaks[i];
        auto indentWidth = i > 0 ? textIndent : firstLineTextIndent;
        auto lineWidth = computeWidthBetween(start, end, !i && applyFirstLineStyling) + indentWidth;
        lineWidths[i] = LayoutUnit::fromFloatCeil(lineWidth + LayoutUnit::epsilon());
    }

    return lineWidths;
}

std::optional<Vector<LayoutUnit>> InlineContentBalancer::balanceRangeWithKnuthPlassNoLimit(InlineItemRange range, std::optional<bool> previousChunkEndsWithLineBreak)
{
    initializeWidthPrefixSums(range.startIndex(), range.endIndex());

    // breakOpportunities holds the indices i such that a line break can occur before m_inlineItems[i].
    auto breakOpportunities = m_lineBuilder->listBreakOpportunities(range);

    // We need a dummy break opportunity at the beginning for algorithmic base case purposes
    breakOpportunities.insert(0, range.startIndex());
    auto numBreakOpportunities = breakOpportunities.size();

    // Indentation
    auto firstLineTextIndent = m_inlineFormattingContext.formattingGeometry().computedTextIndent(InlineFormattingGeometry::IsIntrinsicWidthMode::No, previousChunkEndsWithLineBreak, m_maxLineWidth);
    auto textIndent = m_inlineFormattingContext.formattingGeometry().computedTextIndent(InlineFormattingGeometry::IsIntrinsicWidthMode::No, false, m_maxLineWidth);

    struct Entry {
        float accumulatedCost;
        size_t previousBreakIdx;
    };

    // We should only accomodate ::first-line styling when the InlineItemRange starts at 0.
    // It's possible that we are not balancing the first chunk of text, in which case we
    // ignore ::first-line styling.
    bool applyFirstLineStyling = !range.startIndex();

    // After breakIdx is greater than lastFirstLineBreakIdx, it is no longer possible
    // for a line that ends at breakOpportunities[breakIdx] to be the first line.
    size_t lastFirstLineBreakIdx = 1;
    while (lastFirstLineBreakIdx < numBreakOpportunities) {
        auto firstLineWidth = computeWidthBetween(range.startIndex(), breakOpportunities[lastFirstLineBreakIdx], applyFirstLineStyling) + firstLineTextIndent;
        if (firstLineWidth > m_maxLineWidth)
            break;
        lastFirstLineBreakIdx++;
    }
    lastFirstLineBreakIdx--;

    // state[i] holds the optimal set of line breaks where the last line break is right before m_inlineItems[breakOpportunities[i]]
    Vector<Entry> state(numBreakOpportunities, { std::numeric_limits<float>::infinity(), 0 });
    state[0].accumulatedCost = 0.f;

    // Iterate through all possible line break positions.
    // At each break position, record a prior break position such that of the SUM of
    //     (1) the cost of the line delimited by the current and the prior break position
    //     (2) the cost of the optimal solution that breaks at the prior break position
    // is minimized. This results in an optimal solution for the current break position.

    // breakOpportunities[firstStartIdx] is the first possible starting position for a candidate line that is NOT the first line
    size_t firstStartIdx = 1;
    for (size_t breakIdx = 1; breakIdx < numBreakOpportunities; breakIdx++) {
        size_t end = breakOpportunities[breakIdx];

        // Our candidate line can be the first line, handle this special case
        if (breakIdx <= lastFirstLineBreakIdx) {
            auto firstLineCandidateWidth = computeWidthBetween(range.startIndex(), end, applyFirstLineStyling) + firstLineTextIndent;
            auto cost = computeCost(firstLineCandidateWidth, m_maxLineWidth);
            state[breakIdx].accumulatedCost = cost;
            state[breakIdx].previousBreakIdx = 0;
        }

        // We prune our search space by limiting the possible starting positions for our candidate line.
        while (computeWidthBetween(breakOpportunities[firstStartIdx], end, false) + textIndent > m_maxLineWidth)
            firstStartIdx++;

        for (size_t startIdx = firstStartIdx; startIdx < breakIdx; startIdx++) {
            size_t start = breakOpportunities[startIdx];
            ASSERT(start != range.startIndex());

            auto candidateWidth = computeWidthBetween(start, end, false) + textIndent;
            ASSERT(candidateWidth <= m_maxLineWidth);

            auto accumulatedCost = computeCost(candidateWidth, m_maxLineWidth) + state[startIdx].accumulatedCost;
            if (accumulatedCost < state[breakIdx].accumulatedCost) {
                state[breakIdx].accumulatedCost = accumulatedCost;
                state[breakIdx].previousBreakIdx = startIdx;
            }
        }
    }

    // Check if we found no solution
    if (std::isinf(state[numBreakOpportunities - 1].accumulatedCost))
        return std::nullopt;

    // breaks[i] equals the index into m_inlineItems before which the ith line will break
    Vector<size_t> breaks;
    size_t breakIdx = numBreakOpportunities - 1;
    do {
        breaks.append(breakOpportunities[breakIdx]);
        breakIdx = state[breakIdx].previousBreakIdx;
    } while (breakIdx);
    breaks.reverse();

    // Compute final line widths (adding indentation width)
    Vector<LayoutUnit> lineWidths(breaks.size());
    for (size_t i = 0; i < breaks.size(); i++) {
        auto start = i > 0 ? breaks[i - 1] : range.startIndex();
        auto end = breaks[i];
        auto indentWidth = i > 0 ? textIndent : firstLineTextIndent;
        auto lineWidth = computeWidthBetween(start, end, !i && applyFirstLineStyling) + indentWidth;
        lineWidths[i] = LayoutUnit::fromFloatCeil(lineWidth + LayoutUnit::epsilon());
    }

    return lineWidths;
}

InlineLayoutUnit InlineContentBalancer::inlineItemWidth(const InlineItem& inlineItem, bool applyFirstLineStyling)
{
    ASSERT(inlineItem.layoutBox().isInlineLevelBox());
    if (is<InlineTextItem>(inlineItem)) {
        auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
        if (auto contentWidth = inlineTextItem.width())
            return *contentWidth;
        auto& fontCascade = applyFirstLineStyling ? inlineTextItem.firstLineStyle().fontCascade() : inlineTextItem.style().fontCascade();
        if (!inlineTextItem.isWhitespace() || InlineTextItem::shouldPreserveSpacesAndTabs(inlineTextItem))
            return TextUtil::width(inlineTextItem, fontCascade, 0);
        return TextUtil::width(inlineTextItem, fontCascade, inlineTextItem.start(), inlineTextItem.start() + 1, 0);
    }

    if (inlineItem.isLineBreak() || inlineItem.isWordBreakOpportunity())
        return { };

    auto& layoutBox = inlineItem.layoutBox();
    auto& boxGeometry = m_inlineFormattingContext.geometryForBox(layoutBox);

    if (layoutBox.isReplacedBox())
        return boxGeometry.marginBoxWidth();

    if (inlineItem.isInlineBoxStart()) {
        auto logicalWidth = boxGeometry.marginStart() + boxGeometry.borderStart() + boxGeometry.paddingStart().value_or(0);
#if ENABLE(CSS_BOX_DECORATION_BREAK)
        auto& style = applyFirstLineStyling ? inlineItem.firstLineStyle() : inlineItem.style();
        if (style.boxDecorationBreak() == BoxDecorationBreak::Clone)
            logicalWidth += boxGeometry.borderEnd() + boxGeometry.paddingEnd().value_or(0_lu);
#endif
        return logicalWidth;
    }

    if (inlineItem.isInlineBoxEnd())
        return boxGeometry.marginEnd() + boxGeometry.borderEnd() + boxGeometry.paddingEnd().value_or(0);

    // FIXME: The overhang should be computed to not overlap the neighboring runs or overflow the line.
    if (auto* rubyAdjustments = layoutBox.rubyAdjustments()) {
        auto& overhang = applyFirstLineStyling ? rubyAdjustments->firstLineOverhang : rubyAdjustments->overhang;
        return boxGeometry.marginBoxWidth() - (overhang.start + overhang.end);
    }

    // Non-replaced inline box (e.g. inline-block)
    return boxGeometry.marginBoxWidth();
}

bool InlineContentBalancer::shouldTrimLeading(const InlineItem& inlineItem, bool applyFirstLineStyling)
{
    auto& style = applyFirstLineStyling ? inlineItem.firstLineStyle() : inlineItem.style();

    // Handle line break first so we can focus on other types of white space
    if (inlineItem.isLineBreak())
        return true;

    if (inlineItem.isText()) {
        auto& textItem = downcast<InlineTextItem>(inlineItem);
        if (textItem.isWhitespace()) {
            if (style.whiteSpaceCollapse() == WhiteSpaceCollapse::Preserve || style.whiteSpaceCollapse() == WhiteSpaceCollapse::BreakSpaces)
                return false;
            return true;
        }
        return false;
    }

    if (inlineItemWidth(inlineItem, applyFirstLineStyling) <= 0)
        return true;

    return false;
}

bool InlineContentBalancer::shouldTrimTrailing(const InlineItem& inlineItem, bool applyFirstLineStyling)
{
    auto& style = applyFirstLineStyling ? inlineItem.firstLineStyle() : inlineItem.style();

    // Handle line break first so we can focus on other types of white space
    if (inlineItem.isLineBreak())
        return true;

    if (inlineItem.isText()) {
        auto& textItem = downcast<InlineTextItem>(inlineItem);
        if (textItem.isWhitespace()) {
            if (style.whiteSpaceCollapse() == WhiteSpaceCollapse::BreakSpaces)
                return false;
            return true;
        }
        return false;
    }

    if (inlineItemWidth(inlineItem, applyFirstLineStyling) <= 0)
        return true;

    return false;
}

// Initialize prefix sums for inline items from start (inclusive) to end (exclusive)
void InlineContentBalancer::initializeWidthPrefixSums(size_t start, size_t end)
{
    m_prefixSumOffset = start;
    size_t numItems = end - start;

    m_widthPrefixSum.clear();
    m_firstLineWidthPrefixSum.clear();
    m_widthPrefixSum.reserveCapacity(numItems + 1);
    m_firstLineWidthPrefixSum.reserveCapacity(numItems + 1);

    // Compute width prefix sums to efficiently query the width of an arbitrary
    // sequence of inline items.
    double accumulatedWidth = 0;
    m_widthPrefixSum.append(accumulatedWidth);
    for (size_t i = start ; i < end; i++) {
        accumulatedWidth += inlineItemWidth(m_inlineItems[i], false);
        accumulatedWidth = fmod(accumulatedWidth, prefixSumMod);
        m_widthPrefixSum.append(accumulatedWidth);
    }

    accumulatedWidth = 0;
    m_firstLineWidthPrefixSum.append(accumulatedWidth);
    for (size_t i = start ; i < end; i++) {
        accumulatedWidth += inlineItemWidth(m_inlineItems[i], true);
        accumulatedWidth = fmod(accumulatedWidth, prefixSumMod);
        m_firstLineWidthPrefixSum.append(accumulatedWidth);
    }
}

// Helper function to compute line width of inlineItems from start (inclusive)
// to end (exclusive). Trims any starting/ending whitespace.
InlineLayoutUnit InlineContentBalancer::computeWidthBetween(size_t start, size_t end, bool applyFirstLineStyling)
{
    ASSERT(start <= end);
    if (start == end)
        return 0.f;

    while (start < end) {
        if (shouldTrimLeading(m_inlineItems[start], applyFirstLineStyling))
            start++;
        else
            break;
    }

    while (start < end) {
        if (shouldTrimTrailing(m_inlineItems[end - 1], applyFirstLineStyling))
            end--;
        else
            break;
    }

    // Note that width prefix sums only represent a subrange of the entire inline items.
    // We need to adjust the start/end indices.
    start -= m_prefixSumOffset;
    end -= m_prefixSumOffset;

    auto sum = applyFirstLineStyling ? m_firstLineWidthPrefixSum[end] - m_firstLineWidthPrefixSum[start] : m_widthPrefixSum[end] - m_widthPrefixSum[start];
    return fmod(sum + prefixSumMod, prefixSumMod);
};

// Cost function for a single line. The sum of this cost function will be minimized across all lines.
float InlineContentBalancer::computeCost(InlineLayoutUnit candidateLineWidth, InlineLayoutUnit idealLineWidth)
{
    auto cost = idealLineWidth - candidateLineWidth;
    return cost * cost;
};

}
}
