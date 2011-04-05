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
#include "WebIDBDatabaseImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMStringList.h"
#include "IDBCallbacksProxy.h"
#include "IDBDatabaseBackendInterface.h"
#include "IDBDatabaseCallbacksProxy.h"
#include "IDBTransactionBackendInterface.h"
#include "WebIDBCallbacks.h"
#include "WebIDBDatabaseCallbacks.h"
#include "WebIDBObjectStoreImpl.h"
#include "WebIDBTransactionImpl.h"

using namespace WebCore;

namespace WebKit {

WebIDBDatabaseImpl::WebIDBDatabaseImpl(PassRefPtr<IDBDatabaseBackendInterface> databaseBackend)
    : m_databaseBackend(databaseBackend)
{
}

WebIDBDatabaseImpl::~WebIDBDatabaseImpl()
{
}

WebString WebIDBDatabaseImpl::name() const
{
    return m_databaseBackend->name();
}

WebString WebIDBDatabaseImpl::version() const
{
    return m_databaseBackend->version();
}

WebDOMStringList WebIDBDatabaseImpl::objectStoreNames() const
{
    return m_databaseBackend->objectStoreNames();
}

WebIDBObjectStore* WebIDBDatabaseImpl::createObjectStore(const WebString& name, const WebString& keyPath, bool autoIncrement, const WebIDBTransaction& transaction, WebExceptionCode& ec)
{
    RefPtr<IDBObjectStoreBackendInterface> objectStore = m_databaseBackend->createObjectStore(name, keyPath, autoIncrement, transaction.getIDBTransactionBackendInterface(), ec);
    if (!objectStore) {
        ASSERT(ec);
        return 0;
    }
    return new WebIDBObjectStoreImpl(objectStore);
}

void WebIDBDatabaseImpl::deleteObjectStore(const WebString& name, const WebIDBTransaction& transaction, WebExceptionCode& ec)
{
    m_databaseBackend->deleteObjectStore(name, transaction.getIDBTransactionBackendInterface(), ec);
}

void WebIDBDatabaseImpl::setVersion(const WebString& version, WebIDBCallbacks* callbacks, WebExceptionCode& ec)
{
    m_databaseBackend->setVersion(version, IDBCallbacksProxy::create(callbacks), m_databaseCallbacks, ec);
}

WebIDBTransaction* WebIDBDatabaseImpl::transaction(const WebDOMStringList& names, unsigned short mode, WebExceptionCode& ec)
{
    RefPtr<DOMStringList> nameList = PassRefPtr<DOMStringList>(names);
    RefPtr<IDBTransactionBackendInterface> transaction = m_databaseBackend->transaction(nameList.get(), mode, ec);
    if (!transaction) {
        ASSERT(ec);
        return 0;
    }
    return new WebIDBTransactionImpl(transaction);
}

void WebIDBDatabaseImpl::close()
{
    // Use the callbacks that ::open gave us so that the backend in
    // multi-process chromium knows which database connection is closing.
    ASSERT(m_databaseCallbacks);
    m_databaseBackend->close(m_databaseCallbacks);
}

void WebIDBDatabaseImpl::open(WebIDBDatabaseCallbacks* callbacks)
{
    ASSERT(!m_databaseCallbacks);
    m_databaseCallbacks = IDBDatabaseCallbacksProxy::create(callbacks);
    m_databaseBackend->open(m_databaseCallbacks);
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)
