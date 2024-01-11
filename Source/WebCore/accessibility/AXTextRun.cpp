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
namespace WebCore {

String AXTextRuns::debugDescription() const
{
    StringBuilder result;
    result.append('[');
    for (unsigned i = 0; i < runs.size(); i++) {
        result.append(runs[i].debugDescription(containingBlock));
        if (i != runs.size() - 1)
            result.append(", ");
    }
    result.append(']');
    return result.toString();
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

} // namespace WebCore
#endif // ENABLE(AX_THREAD_TEXT_APIS)
