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

#if ENABLE(AX_THREAD_TEXT_APIS)

#include <wtf/text/MakeString.h>

namespace WebCore {

String AXTextRuns::debugDescription() const
{
    return makeString('[', interleave(runs, [&](auto& run) { return run.debugDescription(containingBlock); }, ", "_s), ']');
}

size_t AXTextRuns::indexForOffset(unsigned textOffset) const
{
    size_t cumulativeLength = 0;
    for (size_t i = 0; i < runs.size(); i++) {
        cumulativeLength += runLength(i);
        if (cumulativeLength >= textOffset)
            return i;
    }
    return notFound;
}

AXTextRunLineID AXTextRuns::lineIDForOffset(unsigned textOffset) const
{
    size_t runIndex = indexForOffset(textOffset);
    return runIndex == notFound ? AXTextRunLineID() : lineID(runIndex);
}

unsigned AXTextRuns::runLengthSumTo(size_t index) const
{
    unsigned length = 0;
    for (size_t i = 0; i <= index && i < runs.size(); i++)
        length += runLength(i);
    return length;
}

String AXTextRuns::substring(unsigned start, unsigned length) const
{
    if (!length)
        return emptyString();

    StringBuilder result;
    size_t charactersSeen = 0;
    auto remaining = [&] () {
        return result.length() >= length ? 0 : length - result.length();
    };
    for (unsigned i = 0; i < runs.size() && result.length() < length; i++) {
        size_t runLength = this->runLength(i);
        if (charactersSeen >= start) {
            // The start points entirely within bounds of this run.
            result.append(runs[i].text.left(remaining()));
        } else if (charactersSeen + runLength > start) {
            // start points somewhere in the middle of the current run, collect part of the text.
            unsigned startInRun = start - charactersSeen;
            RELEASE_ASSERT(startInRun < runLength);
            result.append(runs[i].text.substring(startInRun, remaining()));
        }
        // If charactersSeen + runLength == start, the start points to the end of the run, and there is no text to gather.

        charactersSeen += runLength;
    }
    return result.toString();
}

unsigned AXTextRuns::domOffset(unsigned renderedTextOffset) const
{
    unsigned cumulativeDomOffset = 0;
    unsigned previousEndDomOffset = 0;
    for (size_t i = 0; i < size(); i++) {
        const auto& domOffsets = at(i).domOffsets();
        for (const auto& domOffsetPair : domOffsets) {
            RELEASE_ASSERT(domOffsetPair[0] >= previousEndDomOffset);
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
    RELEASE_ASSERT_NOT_REACHED();
    return renderedTextOffset;
}

FloatRect AXTextRuns::localRect(unsigned start, unsigned end, float lineHeight) const
{
    unsigned smallerOffset = start;
    unsigned largerOffset = end;
    if (smallerOffset > largerOffset)
        std::swap(smallerOffset, largerOffset);

    unsigned runIndexOfSmallerOffset = indexForOffset(smallerOffset);
    unsigned runIndexOfLargerOffset = indexForOffset(largerOffset);

    // FIXME: Probably want a special case for hard linebreaks (<br>s). Investigate how the main-thread does this.
    // FIXME: We'll need to flip the result rect based on writing mode.

    unsigned x = 0;
    unsigned maxWidth = 0;
    float measuredHeight = 0.0f;
    float heightBeforeRuns = 0.0f;
    for (unsigned i = 0; i <= runIndexOfLargerOffset; i++) {
        if (i < runIndexOfSmallerOffset) {
            // Each text run represents a line, so count up the height of lines prior to our range start.
            heightBeforeRuns += lineHeight;
        } else {
            const auto& run = at(i);
            unsigned measuredWidth = 0;
            if (i == runIndexOfSmallerOffset) {
                unsigned offsetOfFirstCharacterInRun = !i ? 0 : runLengthSumTo(i - 1);
                RELEASE_ASSERT(smallerOffset >= offsetOfFirstCharacterInRun);
                // Measure the characters in this run (accomplished by smallerOffset - offsetOfFirstCharacterInRun)
                // prior to the offset.
                unsigned widthPriorToStart = (smallerOffset - offsetOfFirstCharacterInRun) * estimatedCharacterWidth;
                unsigned widthAfterEnd = runIndexOfSmallerOffset == runIndexOfLargerOffset
                    // aa|aaa|aa
                    // length 7, smallerOffset = 2, largerOffset = 5 — measure the last two "a" characters.
                    ? (runLengthSumTo(i) - largerOffset) * estimatedCharacterWidth
                    // The offsets pointed into different runs, so the width of this run extends to the end.
                    : 0;
                unsigned fullRunWidth = run.text.length() * estimatedCharacterWidth;

                RELEASE_ASSERT(fullRunWidth >= (widthPriorToStart + widthAfterEnd));
                measuredWidth = fullRunWidth - widthPriorToStart - widthAfterEnd;
                if (!measuredWidth) {
                    bool isCollapsedRange = (runIndexOfSmallerOffset == runIndexOfLargerOffset && smallerOffset == largerOffset);

                    if (isCollapsedRange) {
                        // If this is a collapsed range (start.offset == end.offset), we want to return the width of a cursor.
                        // Use 2px for this, matching CaretRectComputation::caretWidth. This overall behavior for collapsed
                        // ranges matches that of CaretRectComputation::computeLocalCaretRect, which is downstream of
                        // the main-thread-text-implementation equivalent of this function, AXObjectCache::boundsForRange.
                        measuredWidth = 2;
                    } else {
                        // There was no measured width in this run, so we should count this as a line before the actual rect starts.
                        heightBeforeRuns += lineHeight;
                    }
                }

                if (measuredWidth)
                    x = widthPriorToStart;
            } else if (i == runIndexOfLargerOffset) {
                // We're measuring the end of the range, so measure from the first character in the run up to largerOffset.
                unsigned offsetOfFirstCharacterInRun = !i ? 0 : runLengthSumTo(i - 1);
                RELEASE_ASSERT(largerOffset >= offsetOfFirstCharacterInRun);
                measuredWidth = (largerOffset - offsetOfFirstCharacterInRun) * estimatedCharacterWidth;

                if (measuredWidth) {
                    // Because our rect now includes the beginning of a run, set |x| to be 0, indicating the rect is not
                    // offset from its container.
                    x = 0;
                }
            } else {
                // We're in some run between runIndexOfSmallerOffset and runIndexOfLargerOffset, so measure the whole run.
                // For example, this could be the "bbb" runs:
                // a|aa
                // bbb
                // cc|c
                measuredWidth = run.text.length() * estimatedCharacterWidth;
                if (measuredWidth) {
                    // Since we are measuring from the beginning of a run, x should be 0.
                    x = 0;
                }
            }

            if (measuredWidth) {
                // This run is within the range specified by |start| and |end|, so if we measured a width for it,
                // also add to the height. It's important to only do this if we actually measured a width, as an
                // offset pointing past the last character in a run will not add any width and thus should not
                // contribute any height.
                measuredHeight += lineHeight;
            }
            maxWidth = std::max(maxWidth, measuredWidth);
        }
    }
    return { static_cast<float>(x), heightBeforeRuns, static_cast<float>(maxWidth), measuredHeight };
}

} // namespace WebCore
#endif // ENABLE(AX_THREAD_TEXT_APIS)
