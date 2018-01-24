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
#include "MarkerSubrange.h"

#include <wtf/HashSet.h>
#include <algorithm>

namespace WebCore {

Vector<MarkerSubrange> subdivide(const Vector<MarkerSubrange>& subranges, OverlapStrategy overlapStrategy)
{
    if (subranges.isEmpty())
        return { };

    struct Offset {
        enum Kind { Begin, End };
        Kind kind;
        unsigned value; // Copy of subrange.startOffset/endOffset to avoid the need to branch based on kind.
        const MarkerSubrange* subrange;
    };

    // 1. Build table of all offsets.
    Vector<Offset> offsets;
    ASSERT(subranges.size() < std::numeric_limits<unsigned>::max() / 2);
    unsigned numberOfSubranges = subranges.size();
    unsigned numberOfOffsets = 2 * numberOfSubranges;
    offsets.reserveInitialCapacity(numberOfOffsets);
    for (auto& subrange : subranges) {
        offsets.uncheckedAppend({ Offset::Begin, subrange.startOffset, &subrange });
        offsets.uncheckedAppend({ Offset::End, subrange.endOffset, &subrange });
    }

    // 2. Sort offsets such that begin offsets are in paint order and end offsets are in reverse paint order.
    std::sort(offsets.begin(), offsets.end(), [] (const Offset& a, const Offset& b) {
        return a.value < b.value || (a.value == b.value && a.kind == b.kind && a.kind == Offset::Begin && a.subrange->type < b.subrange->type)
        || (a.value == b.value && a.kind == b.kind && a.kind == Offset::End && a.subrange->type > b.subrange->type);
    });

    // 3. Compute intersection.
    Vector<MarkerSubrange> result;
    result.reserveInitialCapacity(numberOfSubranges);
    HashSet<const MarkerSubrange*> processedSubranges;
    unsigned offsetSoFar = offsets[0].value;
    for (unsigned i = 1; i < numberOfOffsets; ++i) {
        if (offsets[i].value > offsets[i - 1].value) {
            if (overlapStrategy == OverlapStrategy::Frontmost) {
                std::optional<unsigned> frontmost;
                for (unsigned j = 0; j < i; ++j) {
                    if (!processedSubranges.contains(offsets[j].subrange) && (!frontmost || offsets[j].subrange->type > offsets[*frontmost].subrange->type))
                        frontmost = j;
                }
                if (frontmost)
                    result.append({ offsetSoFar, offsets[i].value, offsets[*frontmost].subrange->type, offsets[*frontmost].subrange->marker });
            } else {
                // The appended subranges may not be in paint order. We will fix this up at the end of this function.
                for (unsigned j = 0; j < i; ++j) {
                    if (!processedSubranges.contains(offsets[j].subrange))
                        result.append({ offsetSoFar, offsets[i].value, offsets[j].subrange->type, offsets[j].subrange->marker });
                }
            }
            offsetSoFar = offsets[i].value;
        }
        if (offsets[i].kind == Offset::End)
            processedSubranges.add(offsets[i].subrange);
    }
    // Fix up; sort the subranges so that they are in paint order.
    if (overlapStrategy == OverlapStrategy::None)
        std::sort(result.begin(), result.end(), [] (const MarkerSubrange& a, const MarkerSubrange& b) { return a.startOffset < b.startOffset || (a.startOffset == b.startOffset && a.type < b.type); });
    return result;
}

}


