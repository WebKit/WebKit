/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2021 Apple Inc.  All rights reserved.
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
#include "SharedBufferChunkReader.h"

namespace WebCore {

#if ENABLE(MHTML)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

SharedBufferChunkReader::SharedBufferChunkReader(FragmentedSharedBuffer* buffer, const Vector<char>& separator)
    : m_iteratorCurrent(buffer->begin())
    , m_iteratorEnd(buffer->end())
    , m_segment(m_iteratorCurrent != m_iteratorEnd ? m_iteratorCurrent->segment->span().data() : nullptr)
    , m_separator(separator)
{
}

SharedBufferChunkReader::SharedBufferChunkReader(FragmentedSharedBuffer* buffer, const char* separator)
    : m_iteratorCurrent(buffer->begin())
    , m_iteratorEnd(buffer->end())
    , m_segment(m_iteratorCurrent != m_iteratorEnd ? m_iteratorCurrent->segment->span().data() : nullptr)
{
    setSeparator(separator);
}

void SharedBufferChunkReader::setSeparator(const Vector<char>& separator)
{
    m_separator = separator;
}

void SharedBufferChunkReader::setSeparator(const char* separator)
{
    m_separator.clear();
    m_separator.append(span(separator));
}

bool SharedBufferChunkReader::nextChunk(Vector<uint8_t>& chunk, bool includeSeparator)
{
    if (m_iteratorCurrent == m_iteratorEnd)
        return false;

    chunk.clear();
    while (true) {
        while (m_segmentIndex < m_iteratorCurrent->segment->size()) {
            // FIXME: The existing code to check for separators doesn't work correctly with arbitrary separator strings.
            auto currentCharacter = m_segment[m_segmentIndex++];
            if (currentCharacter != m_separator[m_separatorIndex]) {
                if (m_separatorIndex > 0) {
                    ASSERT_WITH_SECURITY_IMPLICATION(m_separatorIndex <= m_separator.size());
                    chunk.append(m_separator.span().first(m_separatorIndex));
                    m_separatorIndex = 0;
                }
                chunk.append(currentCharacter);
                continue;
            }
            m_separatorIndex++;
            if (m_separatorIndex == m_separator.size()) {
                if (includeSeparator)
                    chunk.appendVector(m_separator);
                m_separatorIndex = 0;
                return true;
            }
        }

        // Read the next segment.
        m_segmentIndex = 0;
        if (++m_iteratorCurrent == m_iteratorEnd) {
            m_segment = nullptr;
            if (m_separatorIndex > 0)
                chunk.append(byteCast<uint8_t>(m_separator.subspan(0, m_separatorIndex)));
            return !chunk.isEmpty();
        }
        m_segment = m_iteratorCurrent->segment->span().data();
    }

    ASSERT_NOT_REACHED();
    return false;
}

String SharedBufferChunkReader::nextChunkAsUTF8StringWithLatin1Fallback(bool includeSeparator)
{
    Vector<uint8_t> data;
    if (!nextChunk(data, includeSeparator))
        return String();

    return data.size() ? String::fromUTF8WithLatin1Fallback(data.span()) : emptyString();
}

size_t SharedBufferChunkReader::peek(Vector<uint8_t>& data, size_t requestedSize)
{
    data.clear();
    if (m_iteratorCurrent == m_iteratorEnd)
        return 0;

    size_t availableInSegment = std::min(m_iteratorCurrent->segment->size() - m_segmentIndex, requestedSize);
    data.append(std::span { m_segment + m_segmentIndex, availableInSegment });

    size_t readBytesCount = availableInSegment;
    requestedSize -= readBytesCount;

    auto currentSegment = m_iteratorCurrent;

    while (requestedSize && ++currentSegment != m_iteratorEnd) {
        size_t lengthInSegment = std::min(currentSegment->segment->size(), requestedSize);
        data.append(currentSegment->segment->span().first(lengthInSegment));
        readBytesCount += lengthInSegment;
        requestedSize -= lengthInSegment;
    }
    return readBytesCount;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif

}

