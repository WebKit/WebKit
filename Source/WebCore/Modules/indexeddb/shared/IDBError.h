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

#ifndef IDBError_h
#define IDBError_h

#if ENABLE(INDEXED_DATABASE)

#include <wtf/text/WTFString.h>

namespace WebCore {

static const int IDBExceptionCodeOffset = 1200;

enum class IDBExceptionCode {
    None = IDBExceptionCodeOffset,
    Unknown,
    ConstraintError,
    DataError,
    TransactionInactiveError,
    ReadOnlyError,
    VersionError,

    // Indexed DB existing exception codes with IDB-specific error messages:
    InvalidStateError,
    DataCloneError,
    AbortError,
};

class IDBError {
public:
    IDBError() { }
    IDBError(IDBExceptionCode);
    IDBError(IDBExceptionCode, const String& message);

    IDBError& operator=(const IDBError&);

    const String& name() const;
    const String& message() const;

    bool isNull() const { return m_code == IDBExceptionCode::None; }

    IDBError isolatedCopy() const;

private:
    IDBExceptionCode m_code { IDBExceptionCode::None };
    String m_message;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBError_h
