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

#pragma once

#include "FormattingConstraints.h"
#include "InlineFormattingContext.h"
#include "InlineFormattingGeometry.h"
#include "InlineItem.h"
#include "InlineLineBuilder.h"
#include "InlineTextItem.h"

namespace WebCore {
namespace Layout {

class InlineContentBalancer {
public:
    InlineContentBalancer(const InlineFormattingContext&, const InlineItems&, const HorizontalConstraints&);
    ~InlineContentBalancer();
    std::optional<Vector<LayoutUnit>> computeBalanceConstraints();

private:
    void initialize();

    // FIXME: Consider tabs? contentLogicalLeft parameter
    InlineLayoutUnit inlineItemWidth(const InlineItem&, bool applyFirstLineStyling);
    bool shouldTrimLeading(const InlineItem&, bool applyFirstLineStyling);
    bool shouldTrimTrailing(const InlineItem&, bool applyFirstLineStyling);
    void initializeWidthPrefixSums(size_t start, size_t end);
    InlineLayoutUnit computeWidthBetween(size_t start, size_t end, bool applyFirstLineStyling);
    float computeCost(InlineLayoutUnit candidateLineWidth, InlineLayoutUnit idealLineWidth);

    std::optional<Vector<LayoutUnit>> balanceRangeWithKnuthPlass(InlineItemRange, unsigned long numLines, std::optional<bool> previousChunkEndsWithLineBreak);
    std::optional<Vector<LayoutUnit>> balanceRangeWithKnuthPlassNoLimit(InlineItemRange, std::optional<bool> previousChunkEndsWithLineBreak);

    const InlineFormattingContext& m_inlineFormattingContext;
    const InlineItems& m_inlineItems;
    const HorizontalConstraints& m_horizontalConstraints;

    // Used to lay out lines to compute the initial line count. Also used to list wrapping opportunities
    // in a specific range.
    LineBuilder* m_lineBuilder;

    // Holds the inline item ranges that correspond to each line built by the initial line layout.
    // (The initial line layout is performed as if the default text-wrap: wrap behavior is in effect).
    Vector<InlineItemRange> m_lineRanges;
    Vector<bool> m_lineEndsWithForcedBreak;

    double m_maxLineWidth;
    size_t m_numLines;
    size_t m_numItems;

    // m_widthPrefixSum[i] holds the total accumulated width excluding the ith inlineItem
    // Note, this is the ith inlineItem starting from the inline item denoted by m_prefixSumOffset
    Vector<double> m_widthPrefixSum;

    // m_firstLineWidthPrefixSum[i] holds the total accumulated width excluding the ith inlineItem.
    // Each inlineItem uses the ::first-line styling instead of the regular styling.
    // Note, this is the ith inlineItem starting from the inline item denoted by m_prefixSumOffset
    Vector<double> m_firstLineWidthPrefixSum;

    // The first inline item included in the width prefix sums
    size_t m_prefixSumOffset;

    // This flag is set if this balancer finds conditions that make it unable to balance the m_inlineItems
    // e.g. there exist floats
    bool m_cannotBalanceContent = false;

    // The balancing algorithm (which can have potentially quadratic time complexity) will only operate on chunks
    // of text no longer than `maxLinesPerChunk` lines long (when laid out as if text-wrap: wrap).
    static const size_t maxLinesPerChunk = 10;

    // Ideally, the act of balancing inline content will use the same number of lines as if the inline content
    // was laid out via `text-wrap: wrap`. However, adhering to this ideal is expensive (quadratic in the number
    // of break opportunities), and not caring about this ideal will allow us to use a more efficient algorithm.
    // Since inline content with more lines is less likely to depend on the number of lines used, we ignore this
    // ideal number of lines requirement beyond this threshold.
    static const size_t maxLinesToBalanceWithLineLimit = 12;

    // Used as the modulus for prefix sum calculations. Need to preserve floating point precision while
    // accumulating inline item widths, so prefix sum must never go above this number.
    // FIXME: We assume that content width will never exceed 100000px? Could change this
    // to m_maxLineWidth.
    static constexpr float prefixSumMod = 100000.f;
};

}
}
