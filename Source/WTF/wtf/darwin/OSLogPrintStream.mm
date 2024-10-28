/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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
#include "OSLogPrintStream.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WTF {

#if OS(DARWIN)

OSLogPrintStream::OSLogPrintStream(os_log_t log, os_log_type_t logType)
    : m_log(log)
    , m_logType(logType)
    , m_string("initial string... lol")
{
}

OSLogPrintStream::~OSLogPrintStream()
{ }

std::unique_ptr<OSLogPrintStream> OSLogPrintStream::open(const char* subsystem, const char* category, os_log_type_t logType)
{
    os_log_t log = os_log_create(subsystem, category);
    return makeUnique<OSLogPrintStream>(log, logType);
}

void OSLogPrintStream::vprintf(const char* format, va_list argList)
{
    Locker lock { m_stringLock };
    size_t offset = m_offset;
    size_t freeBytes = m_string.length() - offset;
    va_list backup;
    va_copy(backup, argList);
ALLOW_NONLITERAL_FORMAT_BEGIN
    size_t bytesWritten = vsnprintf(m_string.mutableData() + offset, freeBytes, format, argList);
    if (UNLIKELY(bytesWritten >= freeBytes)) {
        size_t newLength = std::max(bytesWritten + m_string.length(), m_string.length() * 2);
        m_string.grow(newLength);
        freeBytes = newLength - offset;
        bytesWritten = vsnprintf(m_string.mutableData() + offset, freeBytes, format, backup);
        ASSERT(bytesWritten < freeBytes);
    }
ALLOW_NONLITERAL_FORMAT_END

    size_t newOffset = offset + bytesWritten;
    char* buffer = m_string.mutableData();
    bool loggedText = false;
    do {
        if (buffer[offset] == '\n') {
            // Set the new line to a null character so os_log stops copying there.
            buffer[offset] = '\0';
            os_log_with_type(m_log, m_logType, "%{public}s", buffer);
            buffer += offset + 1;
            newOffset -= offset + 1;
            offset = 0;
            loggedText = true;
        } else
            offset++;
    } while (offset < newOffset);

    if (loggedText)
        memmove(m_string.mutableData(), buffer, newOffset);
    m_offset = newOffset;
}

#endif

} // namespace WTF

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
