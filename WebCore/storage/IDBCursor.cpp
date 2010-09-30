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

#include "config.h"
#include "IDBCursor.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBAny.h"
#include "IDBCallbacks.h"
#include "IDBCursorBackendInterface.h"
#include "IDBKey.h"
#include "IDBRequest.h"
#include "IDBTransactionBackendInterface.h"
#include "ScriptExecutionContext.h"
#include "SerializedScriptValue.h"

namespace WebCore {

IDBCursor::IDBCursor(PassRefPtr<IDBCursorBackendInterface> backend, IDBRequest* request, IDBTransactionBackendInterface* transaction)
    : m_backend(backend)
    , m_request(request)
    , m_transaction(transaction)
{
    ASSERT(m_backend);
    ASSERT(m_request);
    ASSERT(m_transaction);
}

IDBCursor::~IDBCursor()
{
}

unsigned short IDBCursor::direction() const
{
    return m_backend->direction();
}

PassRefPtr<IDBKey> IDBCursor::key() const
{
    return m_backend->key();
}

PassRefPtr<IDBAny> IDBCursor::value() const
{
    return m_backend->value();
}

PassRefPtr<IDBRequest> IDBCursor::update(ScriptExecutionContext* context, PassRefPtr<SerializedScriptValue> value)
{
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    m_backend->update(value, request);
    return request.release();
}

void IDBCursor::continueFunction(PassRefPtr<IDBKey> key)
{
    // FIXME: We're not using the context from when continue was called, which means the callback
    //        will be on the original context openCursor was called on. Is this right?
    if (m_request->resetReadyState(m_transaction.get()))
        m_backend->continueFunction(key, m_request);
    // FIXME: Else throw?
}

PassRefPtr<IDBRequest> IDBCursor::remove(ScriptExecutionContext* context)
{
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    m_backend->remove(request);
    return request.release();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
