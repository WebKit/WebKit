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
#include "WebIDBCallbacksImpl.h"

#include "IDBCallbacks.h"
#include "IDBCursorBackendProxy.h"
#include "IDBDatabaseError.h"
#include "IDBDatabaseProxy.h"
#include "IDBIndexBackendProxy.h"
#include "IDBKey.h"
#include "IDBObjectStoreProxy.h"
#include "WebIDBCallbacks.h"
#include "WebIDBDatabase.h"
#include "WebIDBDatabaseError.h"
#include "WebIDBIndex.h"
#include "WebIDBKey.h"
#include "WebIDBObjectStore.h"
#include "WebSerializedScriptValue.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

WebIDBCallbacksImpl::WebIDBCallbacksImpl(PassRefPtr<IDBCallbacks> callbacks)
    : m_callbacks(callbacks)
{
}

WebIDBCallbacksImpl::~WebIDBCallbacksImpl()
{
}

void WebIDBCallbacksImpl::onError(const WebKit::WebIDBDatabaseError& error)
{
    m_callbacks->onError(error);
}

void WebIDBCallbacksImpl::onSuccess()
{
    m_callbacks->onSuccess();
}

void WebIDBCallbacksImpl::onSuccess(WebKit::WebIDBCursor* cursor)
{
    m_callbacks->onSuccess(IDBCursorBackendProxy::create(cursor));
}

void WebIDBCallbacksImpl::onSuccess(WebKit::WebIDBDatabase* webKitInstance)
{
    m_callbacks->onSuccess(IDBDatabaseProxy::create(webKitInstance));
}

void WebIDBCallbacksImpl::onSuccess(const WebKit::WebIDBKey& key)
{
    m_callbacks->onSuccess(key);
}

void WebIDBCallbacksImpl::onSuccess(WebKit::WebIDBIndex* webKitInstance)
{
    m_callbacks->onSuccess(IDBIndexBackendProxy::create(webKitInstance));
}

void WebIDBCallbacksImpl::onSuccess(WebKit::WebIDBObjectStore* webKitInstance)
{
    m_callbacks->onSuccess(IDBObjectStoreProxy::create(webKitInstance));
}

void WebIDBCallbacksImpl::onSuccess(const WebKit::WebSerializedScriptValue& serializedScriptValue)
{
    m_callbacks->onSuccess(serializedScriptValue);
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
