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
#include "AXTextRun.h"

#include "Logging.h"

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)

#include <algorithm>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AXTextRuns);

String AXTextRuns::debugDescription() const
{
    StringBuilder builder;
    builder.append('[');
    for (size_t i = 0; i < runs.size(); i++) {
        AXTextRunLineID lineID = { containingBlock, runs[i].lineIndex };
        builder.append(makeString(
            lineID.debugDescription(),
            ": |"_s, makeStringByReplacingAll(runString(i), '\n', "{newline}"_s),
            "|(len "_s, runs[i].length(), ")"_s
        ));
        if (i != runs.size() - 1)
            builder.append(", "_s);
    }
    builder.append(']');

    return builder.toString();
}

size_t AXTextRuns::indexForOffset(unsigned textOffset, Affinity affinity) const
{
    size_t cumulativeLength = 0;
    for (size_t i = 0; i < runs.size(); i++) {
        cumulativeLength += runLength(i);
        if (cumulativeLength > textOffset) {
            // The offset points into the middle of a run, which is never amibiguous.
            return i;
        }
        if (cumulativeLength == textOffset) {
            // The offset points to the end of a run, which could make this an ambiguous position
            // when considering soft linebreaks.
            if (affinity == Affinity::Downstream && i < lastRunIndex())
                return i + 1;
            return i;
        }
    }
    return notFound;
}

unsigned AXTextRuns::runLengthSumTo(size_t index) const
{
    unsigned length = 0;
    for (size_t i = 0; i <= index && i < runs.size(); i++)
        length += runLength(i);
    return length;
}

unsigned AXTextRuns::domOffset(unsigned renderedTextOffset) const
{
    unsigned cumulativeDomOffset = 0;
    unsigned previousEndDomOffset = 0;
    for (size_t i = 0; i < size(); i++) {
        const auto& domOffsets = at(i).domOffsets();
        for (const auto& domOffsetPair : domOffsets) {
            AX_ASSERT(domOffsetPair[0] >= previousEndDomOffset);
            if (domOffsetPair[0] < previousEndDomOffset)
                return renderedTextOffset;
            // domOffsetPair[0] represents the start DOM offset of this run. Subtracting it
            // from the previous run's end DOM offset, we know how much whitespace was collapsed,
            // and thus know the offset between the DOM text and what was actually rendered.
            // For example, given domOffsets: [2, 10], [13, 18]
            // The first offset to rendered text is 2 (2 - 0), e.g. because of two leading
            // whitespaces that were trimmed: "  foo"
            // The second offset to rendered text is 3 (13 - 10), e.g. because of three
            // collapsed whitespaces in between the first and second runs.
            cumulativeDomOffset += domOffsetPair[0] - previousEndDomOffset;

            // Using the example above, these values would be 0 and 8 for the first run,
            // and 8 and 13 for the second run. Text that would fits this example would be:
            // "  Charlie    Delta", rendered as: "Charlie Delta".
            unsigned startRenderedTextOffset = domOffsetPair[0] - cumulativeDomOffset;
            unsigned endRenderedTextOffset = domOffsetPair[1] - cumulativeDomOffset;
            if (renderedTextOffset >= startRenderedTextOffset && renderedTextOffset <= endRenderedTextOffset) {
                // The rendered text offset is in range of this run. We can get the DOM offset
                // by adding the accumulated difference between the rendered text and DOM text.
                return renderedTextOffset + cumulativeDomOffset;
            }
            previousEndDomOffset = domOffsetPair[1];
        }
    }
    // We were provided with a rendered-text offset that didn't actually fit into our
    // runs. This should never happen.
    AX_ASSERT_NOT_REACHED();
    return renderedTextOffset;
}

FloatRect AXTextRuns::localRect(unsigned start, unsigned end, FontOrientation orientation) const
{
    auto perLineRects = localRectsPerLine(start, end, orientation);
    if (!perLineRects.isEmpty()) {
        auto result = perLineRects[0];
        for (size_t i = 1; i < perLineRects.size(); ++i)
            result.unite(perLineRects[i]);
        return result;
    }

    // localRectsPerLine returns empty for collapsed ranges (start == end). For these,
    // return a caret-width rect at the offset position, matching
    // CaretRectComputation::caretWidth.
    if (start == end) {
        unsigned offset = start;
        size_t runIndex = indexForOffset(offset, Affinity::Downstream);
        if (runIndex == notFound)
            return { };

        const auto& run = at(runIndex);
        float heightBeforeRun = 0;
        for (size_t i = 0; i < runIndex; ++i)
            heightBeforeRun += at(i).lineHeight;

        unsigned offsetOfFirstCharacterInRun = !runIndex ? 0 : runLengthSumTo(runIndex - 1);
        float xPosition = run.distanceFromBoundsInDirection;
        const auto& advances = run.advances();
        unsigned startInRun = offset - offsetOfFirstCharacterInRun;
        for (unsigned i = 0; i < startInRun && i < advances.size(); ++i)
            xPosition += static_cast<float>(advances[i]);

        static constexpr float caretWidth = 2;
        if (orientation == FontOrientation::Horizontal)
            return { xPosition, heightBeforeRun, caretWidth, run.lineHeight };
        return { heightBeforeRun, xPosition, run.lineHeight, caretWidth };
    }

    return { };
}

Vector<FloatRect> AXTextRuns::localRectsPerLine(unsigned start, unsigned end, FontOrientation orientation) const
{
    unsigned smallerOffset = start;
    unsigned largerOffset = end;
    if (smallerOffset > largerOffset)
        std::swap(smallerOffset, largerOffset);

    // Hardcode Affinity::Downstream to avoid unnecessarily accounting for the end of the line above.
    unsigned runIndexOfSmallerOffset = indexForOffset(smallerOffset, Affinity::Downstream);
    unsigned runIndexOfLargerOffset = indexForOffset(largerOffset, Affinity::Downstream);

    auto computeAdvance = [&] (const AXTextRun& run, unsigned offsetOfFirstCharacterInRun, unsigned startIndex, unsigned endIndex) {
        const auto& characterAdvances = run.advances();
        float totalAdvance = 0;
        unsigned startIndexInRun = startIndex - offsetOfFirstCharacterInRun;
        unsigned endIndexInRun = endIndex - offsetOfFirstCharacterInRun;
        AX_ASSERT(startIndexInRun <= endIndexInRun);
        AX_ASSERT(endIndexInRun <= characterAdvances.size());
        endIndexInRun = std::min(endIndexInRun, static_cast<unsigned>(characterAdvances.size()));
        for (size_t i = startIndexInRun; i < endIndexInRun; i++)
            totalAdvance += (float)characterAdvances[i];
        return totalAdvance;
    };

    // Compared to the main-thread implementation, we regularly produce rects that are 1-3px smaller due to the various
    // levels of float rounding that happen to get here. It's better to be a bit wider to ensure AT cursors capture the
    // entire range of text than it is to be too small. Concretely, too-wide is better than too-small for low-vision
    // VoiceOver users who magnify the VoiceOver cursor's contents. Subjectively, the main-thread implementation feels
    // a bit too large, even favoring too-wide sizes, so only bump by 1px. This is especially impactful when navigating
    // character-by-character in small text.
    static constexpr unsigned sizeBump = 1;
    // FIXME: Probably want a special case for hard linebreaks (<br>s). Investigate how the main-thread does this.
    // FIXME: We'll need to flip the result rect based on writing mode.
    Vector<FloatRect> result;
    float heightBeforeRuns = 0;

    for (unsigned i = 0; i <= runIndexOfLargerOffset; i++) {
        const auto& run = at(i);
        if (i < runIndexOfSmallerOffset) {
            heightBeforeRuns += run.lineHeight;
            continue;
        }

        float measuredWidth = 0;
        float xOffset = 0;

        if (i == runIndexOfSmallerOffset) {
            unsigned offsetOfFirstCharacterInRun = !i ? 0 : runLengthSumTo(i - 1);
            AX_ASSERT(smallerOffset >= offsetOfFirstCharacterInRun);
            if (smallerOffset < offsetOfFirstCharacterInRun)
                smallerOffset = offsetOfFirstCharacterInRun;

            float widthPriorToStart = 0;
            if (smallerOffset - offsetOfFirstCharacterInRun > 0)
                widthPriorToStart = computeAdvance(run, offsetOfFirstCharacterInRun, offsetOfFirstCharacterInRun, smallerOffset);

            unsigned endOffsetInLine = runIndexOfSmallerOffset == runIndexOfLargerOffset
                ? largerOffset
                : !i ? run.length() : runLengthSumTo(i - 1) + run.length();

            if (endOffsetInLine - smallerOffset > 0)
                measuredWidth = computeAdvance(run, offsetOfFirstCharacterInRun, smallerOffset, endOffsetInLine);

            if (measuredWidth)
                xOffset = widthPriorToStart + run.distanceFromBoundsInDirection;
        } else if (i == runIndexOfLargerOffset) {
            unsigned offsetOfFirstCharacterInRun = !i ? 0 : runLengthSumTo(i - 1);
            AX_ASSERT(largerOffset >= offsetOfFirstCharacterInRun);
            if (largerOffset < offsetOfFirstCharacterInRun)
                largerOffset = offsetOfFirstCharacterInRun;

            measuredWidth = computeAdvance(run, offsetOfFirstCharacterInRun, offsetOfFirstCharacterInRun, largerOffset);
            xOffset = run.distanceFromBoundsInDirection;
        } else {
            unsigned offsetOfFirstCharacterInRun = !i ? 0 : runLengthSumTo(i - 1);
            measuredWidth = computeAdvance(run, offsetOfFirstCharacterInRun, offsetOfFirstCharacterInRun, offsetOfFirstCharacterInRun + run.length());
            xOffset = run.distanceFromBoundsInDirection;
        }

        if (measuredWidth) {
            if (orientation == FontOrientation::Horizontal)
                result.append({ xOffset, heightBeforeRuns, measuredWidth + sizeBump, run.lineHeight });
            else
                result.append({ heightBeforeRuns, xOffset, run.lineHeight + sizeBump, measuredWidth });
        }
        heightBeforeRuns += run.lineHeight;
    }

    return result;
}

} // namespace WebCore
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
