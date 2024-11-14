/*
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/text/LineEnding.h>

#include <wtf/Vector.h>

namespace WTF {

Vector<uint8_t> normalizeLineEndingsToLF(Vector<uint8_t>&& vector)
{
    size_t inputIndex = 0;
    size_t outputIndex = 0;
    while (inputIndex < vector.size()) {
        auto character = vector[inputIndex++];
        if (character == '\r') {
            // Turn CRLF and CR into LF.
            if (inputIndex < vector.size() && vector[inputIndex] == '\n')
                ++inputIndex;
            vector[outputIndex++] = '\n';
        } else {
            // Leave other characters alone.
            vector[outputIndex++] = character;
        }
    }
    vector.shrink(outputIndex);
    return vector;
}

Vector<uint8_t> normalizeLineEndingsToCRLF(Vector<uint8_t>&& source)
{
    size_t sourceIndex = 0;
    size_t resultLength = 0;
    while (sourceIndex < source.size()) {
        auto character = source[sourceIndex++];
        if (character == '\r') {
            // Turn CR or CRLF into CRLF;
            if (sourceIndex < source.size() && source[sourceIndex] == '\n')
                ++sourceIndex;
            resultLength += 2;
        } else if (character == '\n') {
            // Turn LF into CRLF.
            resultLength += 2;
        } else {
            // Leave other characters alone.
            resultLength += 1;
        }
    }

    if (resultLength == source.size())
        return source;

    Vector<uint8_t> result(resultLength);
    sourceIndex = 0;
    size_t resultIndex = 0;
    while (sourceIndex < source.size()) {
        auto character = source[sourceIndex++];
        if (character == '\r') {
            // Turn CR or CRLF into CRLF;
            if (sourceIndex < source.size() && source[sourceIndex] == '\n')
                ++sourceIndex;
            result[resultIndex++] = '\r';
            result[resultIndex++] = '\n';
        } else if (character == '\n') {
            // Turn LF into CRLF.
            result[resultIndex++] = '\r';
            result[resultIndex++] = '\n';
        } else {
            // Leave other characters alone.
            result[resultIndex++] = character;
        }
    }
    ASSERT(resultIndex == resultLength);
    return result;
}

Vector<uint8_t> normalizeLineEndingsToNative(Vector<uint8_t>&& from)
{
#if OS(WINDOWS)
    return normalizeLineEndingsToCRLF(WTFMove(from));
#else
    return normalizeLineEndingsToLF(WTFMove(from));
#endif
}

} // namespace WTF
