/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "FlexLineBreaker.h"

#include <algorithm>
#include <limits>

namespace WebCore {

Vector<size_t> greedyLineBreaks(std::span<const LayoutUnit> itemMainAxisSizes, LayoutUnit mainAxisAvailableSpace, LayoutUnit gapBetweenItems)
{
    Vector<size_t> lineBreaks;
    size_t nextIndex = 0;
    while (nextIndex < itemMainAxisSizes.size()) {
        auto lineStartIndex = nextIndex;
        LayoutUnit lineMainSize;
        for (; nextIndex < itemMainAxisSizes.size(); ++nextIndex) {
            if (nextIndex > lineStartIndex && lineMainSize + itemMainAxisSizes[nextIndex] > mainAxisAvailableSpace)
                break;
            lineMainSize += itemMainAxisSizes[nextIndex] + gapBetweenItems;
        }
        lineBreaks.append(nextIndex);
    }
    return lineBreaks;
}

// Knuth-Plass, minimizing the sum of the squares of each line's free space. Costs O(n * L) for L
// items on the fullest line, so quadratic only when one line can hold most of the items.
// TODO: A better solution for such cases is LARSCH, which is O(n) but costs a few hundred lines.
Vector<size_t> balancedLineBreaks(std::span<const LayoutUnit> itemMainAxisSizes, LayoutUnit mainAxisAvailableSpace, LayoutUnit gapBetweenItems)
{
    ASSERT(mainAxisAvailableSpace >= 0);
    ASSERT(gapBetweenItems >= 0);
    auto itemCount = itemMainAxisSizes.size();
    auto capacity = static_cast<uint64_t>(mainAxisAvailableSpace.rawValue());

    if (!itemCount)
        return { };

    // Precomputing turns an O(n) addition to an O(1) subtraction inside the main loop.
    auto gap = static_cast<uint64_t>(gapBetweenItems.rawValue());
    Vector<uint64_t> prefixSums(FillWith { }, itemCount + 1, 0);
    for (size_t index = 0; index < itemCount; ++index) {
        auto size = static_cast<uint64_t>(std::max(0, itemMainAxisSizes[index].rawValue()));
        prefixSums[index + 1] = prefixSums[index] + size + gap;
    }

    auto lineLength = [&](size_t start, size_t end) -> uint64_t {
        ASSERT(start < end);
        return prefixSums[end] - prefixSums[start] - gap;
    };

    // NOTE: lineScore is bounded by 2^62.
    auto lineScore = [&](size_t start, size_t end) -> uint64_t {
        auto length = lineLength(start, end);
        auto freeSpace = length < capacity ? capacity - length : 0;
        return freeSpace * freeSpace;
    };

    if (lineLength(0, itemCount) <= capacity)
        return Vector<size_t>::from(itemCount);

    // lastFittingEnd[start] is the largest end whose line still fits, or start + 1 when the item at
    // start overflows on its own. Placing a single item is always permitted.
    Vector<size_t> lastFittingEnd(FillWith { }, itemCount, 0);
    for (size_t start = 0, end = 1; start < itemCount; ++start) {
        while (end < itemCount && lineLength(start, end + 1) <= capacity)
            ++end;
        lastFittingEnd[start] = std::max(end, start + 1);
    }

    auto minScores = Vector<uint64_t>(FillWith { }, itemCount + 1, std::numeric_limits<uint64_t>::max());
    minScores[itemCount] = 0;
    Vector<size_t> bestEndForStart(FillWith { }, itemCount, 0);

    auto totalScore = [&](size_t start, size_t end) -> uint64_t {
        auto score = lineScore(start, end);
        auto total = score + minScores[end];
        return total < score ? std::numeric_limits<uint64_t>::max() : total;
    };

    for (size_t start = itemCount; start--;) {
        for (auto end = start + 1; end <= lastFittingEnd[start]; ++end) {
            auto total = totalScore(start, end);
            // Equal minimum error gives the most items to the earliest line, per the tie-break in
            // https://drafts.csswg.org/css-flexbox-2/#algo-balance
            if (total <= minScores[start]) {
                minScores[start] = total;
                bestEndForStart[start] = end;
            }
        }
    }

    Vector<size_t> lineBreaks;
    for (size_t start = 0; start < itemCount;) {
        start = bestEndForStart[start];
        lineBreaks.append(start);
    }
    return lineBreaks;
}

} // namespace WebCore
