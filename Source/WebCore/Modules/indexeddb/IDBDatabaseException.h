/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IDBDatabaseException_h
#define IDBDatabaseException_h

#if ENABLE(INDEXED_DATABASE)

#include "ExceptionBase.h"

namespace WebCore {

class IDBDatabaseException : public ExceptionBase {
public:
    static PassRefPtr<IDBDatabaseException> create(const ExceptionCodeDescription& description)
    {
        return adoptRef(new IDBDatabaseException(description));
    }

    static const int IDBDatabaseExceptionOffset = 1200;
    static const int IDBDatabaseExceptionMax = 1299;

    enum IDBDatabaseExceptionCode {
        NO_ERR = IDBDatabaseExceptionOffset,
        UNKNOWN_ERR,
        LEGACY_NON_TRANSIENT_ERR, // FIXME: Placeholder, not in spec.
        LEGACY_NOT_FOUND_ERR, // FIXME: Placeholder.
        CONSTRAINT_ERR,
        DATA_ERR,
        LEGACY_NOT_ALLOWED_ERR, // FIXME: Placeholder, not in spec.
        TRANSACTION_INACTIVE_ERR,
        LEGACY_ABORT_ERR, // FIXME: Placeholder.
        READ_ONLY_ERR,
        LEGACY_TIMEOUT_ERR, // FIXME: Placeholder.
        LEGACY_QUOTA_ERR, // FIXME: Placeholder.
        VER_ERR,

        IDB_NOT_FOUND_ERR,
        IDB_INVALID_STATE_ERR,
        IDB_INVALID_ACCESS_ERR,
        IDB_ABORT_ERR,
        IDB_TIMEOUT_ERR,
        IDB_QUOTA_EXCEEDED_ERR,
        IDB_SYNTAX_ERR,
        IDB_DATA_CLONE_ERR,
        IDB_TYPE_ERR, // FIXME: Placeholder until bindings have been updated to throw a JS TypeError. See https://bugs.webkit.org/show_bug.cgi?id=85513
        IDB_NOT_SUPPORTED_ERR,
    };

    static int ErrorCodeToExceptionCode(int errorCode)
    {
        if (!errorCode)
            return 0;
        return errorCode + IDBDatabaseExceptionOffset;
    }

    static bool initializeDescription(ExceptionCode, ExceptionCodeDescription*);
    static String getErrorName(ExceptionCode);
    static ExceptionCode getLegacyErrorCode(ExceptionCode);

private:
    IDBDatabaseException(const ExceptionCodeDescription& description)
        : ExceptionBase(description)
    {
    }
};

} // namespace WebCore

#endif

#endif // IDBDatabaseException_h
