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
#include "AnnexBUtilities.h"

namespace WebCore {

Vector<NaluIndex> findNaluIndices(std::span<const uint8_t> buffer)
{
    // This is sorta like Boyer-Moore, but with only the first optimization step: given a 3-byte
    // sequence we're looking at, if the 3rd byte isn't 1 or 0, skip ahead to the next 3-byte
    // sequence. 0s and 1s are relatively rare, so this will skip the majority of reads/checks.
    constexpr size_t naluShortStartSequenceSize = 3;
    Vector<NaluIndex> indices;
    if (buffer.size() < naluShortStartSequenceSize)
        return indices;

    size_t end = buffer.size() - naluShortStartSequenceSize;
    for (size_t i = 0; i < end;) {
        if (buffer[i + 2] > 1) {
            i += 3;
            continue;
        }
        if (buffer[i + 2] == 1) {
            if (!buffer[i + 1] && !buffer[i]) {
                NaluIndex index { i, i + 3, 0 };
                if (index.startOffset > 0 && !buffer[index.startOffset - 1])
                    --index.startOffset;

                if (!indices.isEmpty())
                    indices.last().payloadSize = index.startOffset - indices.last().payloadStartOffset;

                indices.append(index);
            }
            i += 3;
        } else
            ++i;
    }

    if (!indices.isEmpty())
        indices.last().payloadSize = buffer.size() - indices.last().payloadStartOffset;

    return indices;
}

Vector<uint8_t> parseRbsp(std::span<const uint8_t> data)
{
    Vector<uint8_t> out;
    out.reserveInitialCapacity(data.size());
    for (size_t i = 0; i < data.size();) {
        if (data.size() - i >= 3 && !data[i] && !data[i + 1] && data[i + 2] == 3) {
            out.append(data[i++]);
            out.append(data[i++]);
            // Skip the emulation prevention byte.
            i++;
        } else
            out.append(data[i++]);
    }
    return out;
}

}
