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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
#include "IDBCallbacksProxy.h"

#include "IDBDatabaseError.h"
#include "IDBDatabaseProxy.h"
#include "WebIDBCallbacks.h"
#include "WebIDBCursorImpl.h"
#include "WebIDBDatabaseImpl.h"
#include "WebIDBDatabaseError.h"
#include "WebIDBIndexImpl.h"
#include "WebIDBKey.h"
#include "WebIDBObjectStoreImpl.h"
#include "WebIDBTransactionImpl.h"
#include "WebSerializedScriptValue.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

PassRefPtr<IDBCallbacksProxy> IDBCallbacksProxy::create(PassOwnPtr<WebKit::WebIDBCallbacks> callbacks)
{
    return adoptRef(new IDBCallbacksProxy(callbacks));
}

IDBCallbacksProxy::IDBCallbacksProxy(PassOwnPtr<WebKit::WebIDBCallbacks> callbacks)
    : m_callbacks(callbacks)
{
}

IDBCallbacksProxy::~IDBCallbacksProxy()
{
}

void IDBCallbacksProxy::onError(PassRefPtr<IDBDatabaseError> idbDatabaseError)
{
    m_callbacks->onError(WebKit::WebIDBDatabaseError(idbDatabaseError));
    m_callbacks.clear();
}

void IDBCallbacksProxy::onSuccess()
{
    m_callbacks->onSuccess();
    m_callbacks.clear();
}

void IDBCallbacksProxy::onSuccess(PassRefPtr<IDBCursorBackendInterface> idbCursorBackend)
{
    m_callbacks->onSuccess(new WebKit::WebIDBCursorImpl(idbCursorBackend));
    m_callbacks.clear();
}

void IDBCallbacksProxy::onSuccess(PassRefPtr<IDBDatabaseBackendInterface> backend)
{
    m_callbacks->onSuccess(new WebKit::WebIDBDatabaseImpl(backend));
    m_callbacks.clear();
}

void IDBCallbacksProxy::onSuccess(PassRefPtr<IDBIndexBackendInterface> backend)
{
    m_callbacks->onSuccess(new WebKit::WebIDBIndexImpl(backend));
    m_callbacks.clear();
}

void IDBCallbacksProxy::onSuccess(PassRefPtr<IDBKey> idbKey)
{
    m_callbacks->onSuccess(WebKit::WebIDBKey(idbKey));
    m_callbacks.clear();
}

void IDBCallbacksProxy::onSuccess(PassRefPtr<IDBObjectStoreBackendInterface> backend)
{
    m_callbacks->onSuccess(new WebKit::WebIDBObjectStoreImpl(backend));
    m_callbacks.clear();
}

void IDBCallbacksProxy::onSuccess(PassRefPtr<IDBTransactionBackendInterface> backend)
{
    m_callbacks->onSuccess(new WebKit::WebIDBTransactionImpl(backend));
    m_callbacks.clear();
}

void IDBCallbacksProxy::onSuccess(PassRefPtr<SerializedScriptValue> serializedScriptValue)
{
    m_callbacks->onSuccess(WebKit::WebSerializedScriptValue(serializedScriptValue));
    m_callbacks.clear();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
