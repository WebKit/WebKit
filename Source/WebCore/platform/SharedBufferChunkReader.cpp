/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#if ENABLE(MHTML)

// FIXME: This class is overkill. Remove this class and just iterate the segments of a SharedBuffer
// using the cool new SharedBuffer::begin() and SharedBuffer::end() instead of using this class.

#include "SharedBuffer.h"

namespace WebCore {

SharedBufferChunkReader::SharedBufferChunkReader(SharedBuffer* buffer, const Vector<char>& separator)
    : m_buffer(buffer)
    , m_separator(separator)
{
}

SharedBufferChunkReader::SharedBufferChunkReader(SharedBuffer* buffer, const char* separator)
    : m_buffer(buffer)
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
    m_separator.append(separator, strlen(separator));
}

bool SharedBufferChunkReader::nextChunk(Vector<uint8_t>& chunk, bool includeSeparator)
{
    if (m_reachedEndOfFile)
        return false;

    chunk.clear();
    while (true) {
        while (m_segmentIndex < m_segmentLength) {
            auto currentCharacter = m_segment[m_segmentIndex++];
            if (currentCharacter != m_separator[m_separatorIndex]) {
                if (m_separatorIndex > 0) {
                    ASSERT_WITH_SECURITY_IMPLICATION(m_separatorIndex <= m_separator.size());
                    chunk.append(m_separator.data(), m_separatorIndex);
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
        m_bufferPosition += m_segmentLength;
        // Let's pretend all the data is in one block.
        // FIXME: This class should be removed in favor of just iterating the segments of the SharedBuffer.
        m_segment = m_buffer->data() + m_bufferPosition;
        m_segmentLength = m_buffer->size() - m_bufferPosition;
        if (!m_segmentLength) {
            m_reachedEndOfFile = true;
            if (m_separatorIndex > 0)
                chunk.append(reinterpret_cast<const uint8_t*>(m_separator.data()), m_separatorIndex);
            return !chunk.isEmpty();
        }
    }
    ASSERT_NOT_REACHED();
    return false;
}

String SharedBufferChunkReader::nextChunkAsUTF8StringWithLatin1Fallback(bool includeSeparator)
{
    Vector<uint8_t> data;
    if (!nextChunk(data, includeSeparator))
        return String();

    return data.size() ? String::fromUTF8WithLatin1Fallback(data.data(), data.size()) : emptyString();
}

size_t SharedBufferChunkReader::peek(Vector<uint8_t>& data, size_t requestedSize)
{
    data.clear();
    if (requestedSize <= m_segmentLength - m_segmentIndex) {
        data.append(m_segment + m_segmentIndex, requestedSize);
        return requestedSize;
    }

    size_t readBytesCount = m_segmentLength - m_segmentIndex;
    data.append(m_segment + m_segmentIndex, readBytesCount);

    size_t bufferPosition = m_bufferPosition + m_segmentLength;
    const uint8_t* segment = nullptr;

    // Let's pretend all the data is in one block.
    // FIXME: This class should be removed in favor of just iterating the segments of the SharedBuffer.
    if (bufferPosition != m_buffer->size()) {
        segment = m_buffer->data() + bufferPosition;
        size_t segmentLength = m_buffer->size() - bufferPosition;
        if (segmentLength > requestedSize)
            segmentLength = requestedSize;
        data.append(segment, segmentLength);
        readBytesCount += segmentLength;
        bufferPosition += segmentLength;
    }
    return readBytesCount;
}

}

#endif
