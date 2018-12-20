/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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

#include "DOMException.h"

namespace WebCore {

IDBError::IDBError(Optional<ExceptionCode> code, const String& message)
    : m_code(code)
    , m_message(message)
{
}

IDBError IDBError::isolatedCopy() const
{
    return IDBError { m_code, m_message.isolatedCopy() };
}

IDBError& IDBError::operator=(const IDBError& other)
{
    m_code = other.m_code;
    m_message = other.m_message;
    return *this;
}

String IDBError::name() const
{
    if (!m_code)
        return { };
    return DOMException::name(m_code.value());
}

String IDBError::message() const
{
    if (!m_code)
        return { };
    return DOMException::message(m_code.value());
}

RefPtr<DOMException> IDBError::toDOMException() const
{
    if (!m_code)
        return nullptr;

    return DOMException::create(*m_code, m_message);
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
