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
#include "IDBDatabaseProxy.h"

#include "DOMStringList.h"
#include "IDBCallbacks.h"
#include "IDBObjectStoreProxy.h"
#include "IDBTransactionBackendProxy.h"
#include "WebDOMStringList.h"
#include "WebFrameImpl.h"
#include "WebIDBCallbacksImpl.h"
#include "WebIDBDatabase.h"
#include "WebIDBDatabaseError.h"
#include "WebIDBObjectStore.h"
#include "WebIDBTransaction.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

PassRefPtr<IDBDatabaseBackendInterface> IDBDatabaseProxy::create(PassOwnPtr<WebKit::WebIDBDatabase> database)
{
    return adoptRef(new IDBDatabaseProxy(database));
}

IDBDatabaseProxy::IDBDatabaseProxy(PassOwnPtr<WebKit::WebIDBDatabase> database)
    : m_webIDBDatabase(database)
{
}

IDBDatabaseProxy::~IDBDatabaseProxy()
{
}

String IDBDatabaseProxy::name() const
{
    return m_webIDBDatabase->name();
}

String IDBDatabaseProxy::description() const
{
    return m_webIDBDatabase->description();
}

String IDBDatabaseProxy::version() const
{
    return m_webIDBDatabase->version();
}

PassRefPtr<DOMStringList> IDBDatabaseProxy::objectStores() const
{
    return m_webIDBDatabase->objectStores();
}

PassRefPtr<IDBObjectStoreBackendInterface> IDBDatabaseProxy::createObjectStore(const String& name, const String& keyPath, bool autoIncrement, IDBTransactionBackendInterface* transaction)
{
    // The transaction pointer is guaranteed to be a pointer to a proxy object as, in the renderer,
    // all implementations of IDB interfaces are proxy objects.
    IDBTransactionBackendProxy* transactionProxy = static_cast<IDBTransactionBackendProxy*>(transaction);
    WebKit::WebIDBObjectStore* objectStore = m_webIDBDatabase->createObjectStore(name, keyPath, autoIncrement, *transactionProxy->getWebIDBTransaction());
    if (!objectStore)
        return 0;
    return IDBObjectStoreProxy::create(objectStore);
}

PassRefPtr<IDBObjectStoreBackendInterface> IDBDatabaseProxy::objectStore(const String& name, unsigned short mode)
{
    WebKit::WebIDBObjectStore* objectStore = m_webIDBDatabase->objectStore(name, mode);
    if (!objectStore)
        return 0;
    return IDBObjectStoreProxy::create(objectStore);
}

void IDBDatabaseProxy::removeObjectStore(const String& name, IDBTransactionBackendInterface* transaction)
{
    // The transaction pointer is guaranteed to be a pointer to a proxy object as, in the renderer,
    // all implementations of IDB interfaces are proxy objects.
    IDBTransactionBackendProxy* transactionProxy = static_cast<IDBTransactionBackendProxy*>(transaction);
    m_webIDBDatabase->removeObjectStore(name, *transactionProxy->getWebIDBTransaction());
}

void IDBDatabaseProxy::setVersion(const String& version, PassRefPtr<IDBCallbacks> callbacks)
{
    m_webIDBDatabase->setVersion(version, new WebIDBCallbacksImpl(callbacks));
}

PassRefPtr<IDBTransactionBackendInterface> IDBDatabaseProxy::transaction(DOMStringList* storeNames, unsigned short mode, unsigned long timeout)
{
    WebKit::WebDOMStringList names(storeNames);
    WebKit::WebIDBTransaction* transaction = m_webIDBDatabase->transaction(names, mode, timeout);
    return IDBTransactionBackendProxy::create(transaction);
}

void IDBDatabaseProxy::close()
{
    m_webIDBDatabase->close();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
