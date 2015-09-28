/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "IDBError.h"

#if ENABLE(INDEXED_DATABASE)

#include <wtf/NeverDestroyed.h>

namespace WebCore {

static const std::pair<String, String>& idbExceptionEntry(IDBExceptionCode code)
{
    NeverDestroyed<Vector<std::pair<String, String>>> entries;
    if (entries.get().isEmpty()) {
        entries.get().append(std::make_pair<String, String>(
            ASCIILiteral("UnknownError"),
            ASCIILiteral("Operation failed for reasons unrelated to the database itself and not covered by any other errors.")));
    }

    return entries.get()[(int)code - 1];
}

IDBError::IDBError(IDBExceptionCode code)
    : IDBError(code, emptyString())
{
}

IDBError::IDBError(IDBExceptionCode code, const String& message)
    : m_code(code)
    , m_message(message)
{
}

IDBError& IDBError::operator=(const IDBError& other)
{
    m_code = other.m_code;
    m_message = other.m_message;
    return *this;
}

const String& IDBError::name() const
{
    return idbExceptionEntry(m_code).first;
}

const String& IDBError::message() const
{
    if (!m_message.isEmpty())
        return m_message;

    return idbExceptionEntry(m_code).second;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
