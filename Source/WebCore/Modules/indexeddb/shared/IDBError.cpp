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

static const String& idbErrorName(IDBExceptionCode code)
{
    switch (code) {
    case IDBExceptionCode::Unknown: {
        static NeverDestroyed<String> entry = ASCIILiteral("UnknownError");
        return entry;
    }
    case IDBExceptionCode::ConstraintError: {
        static NeverDestroyed<String> entry = ASCIILiteral("ConstraintError");
        return entry;
    }
    case IDBExceptionCode::VersionError: {
        static NeverDestroyed<String> entry = ASCIILiteral("VersionError");
        return entry;
    }
    case IDBExceptionCode::InvalidStateError: {
        static NeverDestroyed<String> entry = ASCIILiteral("InvalidStateError");
        return entry;
    }
    case IDBExceptionCode::DataError: {
        static NeverDestroyed<String> entry = ASCIILiteral("DataError");
        return entry;
    }
    case IDBExceptionCode::TransactionInactiveError: {
        static NeverDestroyed<String> entry = ASCIILiteral("TransactionInactiveError");
        return entry;
    }
    case IDBExceptionCode::ReadOnlyError: {
        static NeverDestroyed<String> entry = ASCIILiteral("ReadOnlyError");
        return entry;
    }
    case IDBExceptionCode::DataCloneError: {
        static NeverDestroyed<String> entry = ASCIILiteral("DataCloneError");
        return entry;
    }
    case IDBExceptionCode::None:
        RELEASE_ASSERT_NOT_REACHED();
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static const String& idbErrorDescription(IDBExceptionCode code)
{
    switch (code) {
    case IDBExceptionCode::Unknown: {
        static NeverDestroyed<String> entry = ASCIILiteral("Operation failed for reasons unrelated to the database itself and not covered by any other errors.");
        return entry.get();
    }
    case IDBExceptionCode::ConstraintError: {
        static NeverDestroyed<String> entry = ASCIILiteral("Mutation operation in the transaction failed because a constraint was not satisfied.");
        return entry.get();
    }
    case IDBExceptionCode::VersionError: {
        static NeverDestroyed<String> entry = ASCIILiteral("An attempt was made to open a database using a lower version than the existing version.");
        return entry.get();
    }
    case IDBExceptionCode::InvalidStateError: {
        static NeverDestroyed<String> entry = ASCIILiteral("Operation was called on an object on which it is not allowed or at a time when it is not allowed.");
        return entry;
    }
    case IDBExceptionCode::DataError: {
        static NeverDestroyed<String> entry = ASCIILiteral("Data provided to an operation does not meet requirements.");
        return entry;
    }
    case IDBExceptionCode::TransactionInactiveError: {
        static NeverDestroyed<String> entry = ASCIILiteral("Request was placed against a transaction which is currently not active, or which is finished.");
        return entry;
    }
    case IDBExceptionCode::ReadOnlyError: {
        static NeverDestroyed<String> entry = ASCIILiteral("A mutating operation was attempted in a \"readonly\" transaction.");
        return entry;
    }
    case IDBExceptionCode::DataCloneError: {
        static NeverDestroyed<String> entry = ASCIILiteral("Data being stored could not be cloned by the structured cloning algorithm.");
        return entry;
    }
    case IDBExceptionCode::None:
        RELEASE_ASSERT_NOT_REACHED();
    }

    RELEASE_ASSERT_NOT_REACHED();
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

IDBError IDBError::isolatedCopy() const
{
    return { m_code, m_message.isolatedCopy() };
}

IDBError& IDBError::operator=(const IDBError& other)
{
    m_code = other.m_code;
    m_message = other.m_message;
    return *this;
}

const String& IDBError::name() const
{
    return idbErrorName(m_code);
}

const String& IDBError::message() const
{
    if (!m_message.isEmpty())
        return m_message;

    return idbErrorDescription(m_code);
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
