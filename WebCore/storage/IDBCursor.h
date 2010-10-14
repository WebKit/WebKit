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

#ifndef IDBCursor_h
#define IDBCursor_h

#if ENABLE(INDEXED_DATABASE)

#include "ExceptionCode.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class IDBAny;
class IDBCallbacks;
class IDBCursorBackendInterface;
class IDBKey;
class IDBRequest;
class ScriptExecutionContext;
class SerializedScriptValue;
class IDBTransactionBackendInterface;

class IDBCursor : public RefCounted<IDBCursor> {
public:
    enum Direction {
        NEXT = 0,
        NEXT_NO_DUPLICATE = 1,
        PREV = 2,
        PREV_NO_DUPLICATE = 3,
    };
    static PassRefPtr<IDBCursor> create(PassRefPtr<IDBCursorBackendInterface> backend, IDBRequest* request, IDBTransactionBackendInterface* transaction)
    {
        return adoptRef(new IDBCursor(backend, request, transaction));
    }
    ~IDBCursor();

    // FIXME: Try to modify the code generator so this is unneeded.
    void continueFunction(ExceptionCode& ec) { continueFunction(0, ec); }

    // Implement the IDL
    unsigned short direction() const;
    PassRefPtr<IDBKey> key() const;
    PassRefPtr<IDBAny> value() const;
    PassRefPtr<IDBRequest> update(ScriptExecutionContext*, PassRefPtr<SerializedScriptValue>, ExceptionCode&);
    void continueFunction(PassRefPtr<IDBKey>, ExceptionCode&);
    PassRefPtr<IDBRequest> remove(ScriptExecutionContext*, ExceptionCode&);

private:
    explicit IDBCursor(PassRefPtr<IDBCursorBackendInterface>, IDBRequest*, IDBTransactionBackendInterface*);

    RefPtr<IDBCursorBackendInterface> m_backend;
    RefPtr<IDBRequest> m_request;
    RefPtr<IDBTransactionBackendInterface> m_transaction;
};

} // namespace WebCore

#endif

#endif // IDBCursor_h
