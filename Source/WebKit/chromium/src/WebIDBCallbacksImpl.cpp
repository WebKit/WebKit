/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#if ENABLE(INDEXED_DATABASE)

#include "IDBCallbacks.h"
#include "IDBCursorBackendProxy.h"
#include "IDBDatabaseBackendProxy.h"
#include "IDBDatabaseError.h"
#include "IDBKey.h"
#include "IDBTransactionBackendProxy.h"
#include "WebDOMStringList.h"
#include "WebIDBCallbacks.h"
#include "WebIDBDatabase.h"
#include "WebIDBDatabaseError.h"
#include "WebIDBKey.h"
#include "WebIDBTransaction.h"
#include "platform/WebSerializedScriptValue.h"

using namespace WebCore;

namespace WebKit {

WebIDBCallbacksImpl::WebIDBCallbacksImpl(PassRefPtr<IDBCallbacks> callbacks)
    : m_callbacks(callbacks)
    , m_didCreateDatabaseProxy(false)
{
}

WebIDBCallbacksImpl::~WebIDBCallbacksImpl()
{
}

void WebIDBCallbacksImpl::onError(const WebIDBDatabaseError& error)
{
    m_callbacks->onError(error);
}

void WebIDBCallbacksImpl::onSuccess(const WebDOMStringList& domStringList)
{
    m_callbacks->onSuccess(domStringList);
}

void WebIDBCallbacksImpl::onSuccess(WebIDBCursor* cursor, const WebIDBKey& key, const WebIDBKey& primaryKey, const WebSerializedScriptValue& value)
{
    m_callbacks->onSuccess(IDBCursorBackendProxy::create(adoptPtr(cursor)), key, primaryKey, value);
}

void WebIDBCallbacksImpl::onSuccess(WebIDBDatabase* webKitInstance)
{
    if (m_didCreateDatabaseProxy) {
        m_callbacks->onSuccess(IDBDatabaseBackendProxy::create(adoptPtr(static_cast<WebIDBDatabase*>(0))));
        return;
    }
    m_callbacks->onSuccess(IDBDatabaseBackendProxy::create(adoptPtr(webKitInstance)));
}

void WebIDBCallbacksImpl::onSuccess(const WebIDBKey& key)
{
    m_callbacks->onSuccess(key);
}

void WebIDBCallbacksImpl::onSuccess(WebIDBTransaction* webKitInstance)
{
    m_callbacks->onSuccess(IDBTransactionBackendProxy::create(adoptPtr(webKitInstance)));
}

void WebIDBCallbacksImpl::onSuccess(const WebSerializedScriptValue& serializedScriptValue)
{
    m_callbacks->onSuccess(serializedScriptValue);
}

void WebIDBCallbacksImpl::onSuccess(const WebSerializedScriptValue& serializedScriptValue, const WebIDBKey& key, const WebIDBKeyPath& keyPath)
{
    m_callbacks->onSuccess(serializedScriptValue, key, keyPath);
}

void WebIDBCallbacksImpl::onSuccess(const WebIDBKey& key, const WebIDBKey& primaryKey, const WebSerializedScriptValue& value)
{
    m_callbacks->onSuccess(key, primaryKey, value);
}

void WebIDBCallbacksImpl::onBlocked()
{
    m_callbacks->onBlocked();
}

void WebIDBCallbacksImpl::onBlocked(long long oldVersion)
{
    m_callbacks->onBlocked(oldVersion);
}

void WebIDBCallbacksImpl::onUpgradeNeeded(long long oldVersion, WebIDBTransaction* transaction, WebIDBDatabase* database)
{
    m_didCreateDatabaseProxy = true;
    m_callbacks->onUpgradeNeeded(oldVersion, IDBTransactionBackendProxy::create(adoptPtr(transaction)), IDBDatabaseBackendProxy::create(adoptPtr(database)));
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)
