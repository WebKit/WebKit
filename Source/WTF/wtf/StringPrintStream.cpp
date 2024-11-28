/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/StringPrintStream.h>

#include <stdarg.h>
#include <stdio.h>
#include <wtf/MallocSpan.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

StringPrintStream::StringPrintStream()
{
    m_buffer = std::span { m_inlineBuffer };
    m_buffer[0] = 0; // Make sure that we always have a null terminator.
}

StringPrintStream::~StringPrintStream()
{
    if (m_buffer.data() != m_inlineBuffer.data())
        fastFree(m_buffer.data());
}

void StringPrintStream::vprintf(const char* format, va_list argList)
{
    ASSERT_WITH_SECURITY_IMPLICATION(m_length < m_buffer.size());
    ASSERT(!m_buffer[m_length]);

    va_list firstPassArgList;
    va_copy(firstPassArgList, argList);

    int numberOfBytesNotIncludingTerminatorThatWouldHaveBeenWritten =
        vsnprintf(m_buffer.subspan(m_length).data(), m_buffer.size() - m_length, format, firstPassArgList);

    va_end(firstPassArgList);

    int numberOfBytesThatWouldHaveBeenWritten =
        numberOfBytesNotIncludingTerminatorThatWouldHaveBeenWritten + 1;

    if (m_length + numberOfBytesThatWouldHaveBeenWritten <= m_buffer.size()) {
        m_length += numberOfBytesNotIncludingTerminatorThatWouldHaveBeenWritten;
        return; // This means that vsnprintf() succeeded.
    }

    increaseSize(m_length + numberOfBytesThatWouldHaveBeenWritten);

    int numberOfBytesNotIncludingTerminatorThatWereWritten =
        vsnprintf(m_buffer.subspan(m_length).data(), m_buffer.size() - m_length, format, argList);

    int numberOfBytesThatWereWritten = numberOfBytesNotIncludingTerminatorThatWereWritten + 1;

    ASSERT_UNUSED(numberOfBytesThatWereWritten, m_length + numberOfBytesThatWereWritten <= m_buffer.size());

    m_length += numberOfBytesNotIncludingTerminatorThatWereWritten;

    ASSERT_WITH_SECURITY_IMPLICATION(m_length < m_buffer.size());
    ASSERT(!m_buffer[m_length]);
}

CString StringPrintStream::toCString() const
{
    ASSERT(m_length == strlen(m_buffer.data()));
    return CString(m_buffer.first(m_length));
}

void StringPrintStream::reset()
{
    m_length = 0;
    m_buffer[0] = 0;
}

Expected<String, UTF8ConversionError> StringPrintStream::tryToString() const
{
    ASSERT(m_length == strlen(m_buffer.data()));
    if (m_length > String::MaxLength)
        return makeUnexpected(UTF8ConversionError::OutOfMemory);
    return String::fromUTF8(m_buffer.first(m_length));
}

String StringPrintStream::toString() const
{
    ASSERT(m_length == strlen(m_buffer.data()));
    return String::fromUTF8(m_buffer.first(m_length));
}

String StringPrintStream::toStringWithLatin1Fallback() const
{
    ASSERT(m_length == strlen(m_buffer.data()));
    return String::fromUTF8WithLatin1Fallback(m_buffer.first(m_length));
}

void StringPrintStream::increaseSize(size_t newSize)
{
    ASSERT_WITH_SECURITY_IMPLICATION(newSize > m_buffer.size());
    ASSERT(newSize > m_inlineBuffer.size());

    // Use exponential resizing to reduce thrashing.
    newSize = newSize << 1;

    // Use fastMalloc instead of fastRealloc because we know that for the sizes we're using,
    // fastRealloc will just do malloc+free anyway. Also, this simplifies the code since
    // we can't realloc the inline buffer.
    auto newBuffer = MallocSpan<char>::malloc(newSize);
    memcpySpan(newBuffer.mutableSpan(), m_buffer.first(m_length + 1));
    if (m_buffer.data() != m_inlineBuffer.data())
        fastFree(m_buffer.data());
    m_buffer = newBuffer.leakSpan();
}

} // namespace WTF
