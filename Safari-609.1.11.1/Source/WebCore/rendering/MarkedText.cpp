/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "MarkedText.h"

#include <algorithm>
#include <wtf/HashSet.h>

namespace WebCore {

Vector<MarkedText> subdivide(const Vector<MarkedText>& markedTexts, OverlapStrategy overlapStrategy)
{
    if (markedTexts.isEmpty())
        return { };

    struct Offset {
        enum Kind { Begin, End };
        Kind kind;
        unsigned value; // Copy of markedText.startOffset/endOffset to avoid the need to branch based on kind.
        const MarkedText* markedText;
    };

    // 1. Build table of all offsets.
    Vector<Offset> offsets;
    ASSERT(markedTexts.size() < std::numeric_limits<unsigned>::max() / 2);
    unsigned numberOfMarkedTexts = markedTexts.size();
    unsigned numberOfOffsets = 2 * numberOfMarkedTexts;
    offsets.reserveInitialCapacity(numberOfOffsets);
    for (auto& markedText : markedTexts) {
        offsets.uncheckedAppend({ Offset::Begin, markedText.startOffset, &markedText });
        offsets.uncheckedAppend({ Offset::End, markedText.endOffset, &markedText });
    }

    // 2. Sort offsets such that begin offsets are in paint order and end offsets are in reverse paint order.
    std::sort(offsets.begin(), offsets.end(), [] (const Offset& a, const Offset& b) {
        return a.value < b.value || (a.value == b.value && a.kind == b.kind && a.kind == Offset::Begin && a.markedText->type < b.markedText->type)
        || (a.value == b.value && a.kind == b.kind && a.kind == Offset::End && a.markedText->type > b.markedText->type);
    });

    // 3. Compute intersection.
    Vector<MarkedText> result;
    result.reserveInitialCapacity(numberOfMarkedTexts);
    HashSet<const MarkedText*> processedMarkedTexts;
    unsigned offsetSoFar = offsets[0].value;
    for (unsigned i = 1; i < numberOfOffsets; ++i) {
        if (offsets[i].value > offsets[i - 1].value) {
            if (overlapStrategy == OverlapStrategy::Frontmost) {
                Optional<unsigned> frontmost;
                for (unsigned j = 0; j < i; ++j) {
                    if (!processedMarkedTexts.contains(offsets[j].markedText) && (!frontmost || offsets[j].markedText->type > offsets[*frontmost].markedText->type))
                        frontmost = j;
                }
                if (frontmost)
                    result.append({ offsetSoFar, offsets[i].value, offsets[*frontmost].markedText->type, offsets[*frontmost].markedText->marker });
            } else {
                // The appended marked texts may not be in paint order. We will fix this up at the end of this function.
                for (unsigned j = 0; j < i; ++j) {
                    if (!processedMarkedTexts.contains(offsets[j].markedText))
                        result.append({ offsetSoFar, offsets[i].value, offsets[j].markedText->type, offsets[j].markedText->marker });
                }
            }
            offsetSoFar = offsets[i].value;
        }
        if (offsets[i].kind == Offset::End)
            processedMarkedTexts.add(offsets[i].markedText);
    }
    // Fix up; sort the marked texts so that they are in paint order.
    if (overlapStrategy == OverlapStrategy::None)
        std::sort(result.begin(), result.end(), [] (const MarkedText& a, const MarkedText& b) { return a.startOffset < b.startOffset || (a.startOffset == b.startOffset && a.type < b.type); });
    return result;
}

}


