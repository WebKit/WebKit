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
#include "IDBDatabaseBackendProxy.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMStringList.h"
#include "IDBCallbacks.h"
#include "IDBDatabaseCallbacks.h"
#include "IDBObjectStoreBackendProxy.h"
#include "IDBTransactionBackendProxy.h"
#include "WebDOMStringList.h"
#include "WebFrameImpl.h"
#include "WebIDBCallbacksImpl.h"
#include "WebIDBDatabase.h"
#include "WebIDBDatabaseCallbacksImpl.h"
#include "WebIDBDatabaseError.h"
#include "WebIDBObjectStore.h"
#include "WebIDBTransaction.h"

using namespace WebCore;

namespace WebKit {

PassRefPtr<IDBDatabaseBackendInterface> IDBDatabaseBackendProxy::create(PassOwnPtr<WebIDBDatabase> database)
{
    return adoptRef(new IDBDatabaseBackendProxy(database));
}

IDBDatabaseBackendProxy::IDBDatabaseBackendProxy(PassOwnPtr<WebIDBDatabase> database)
    : m_webIDBDatabase(database)
{
}

IDBDatabaseBackendProxy::~IDBDatabaseBackendProxy()
{
}

String IDBDatabaseBackendProxy::name() const
{
    return m_webIDBDatabase->name();
}

String IDBDatabaseBackendProxy::version() const
{
    return m_webIDBDatabase->version();
}

PassRefPtr<DOMStringList> IDBDatabaseBackendProxy::objectStoreNames() const
{
    return m_webIDBDatabase->objectStoreNames();
}

PassRefPtr<IDBObjectStoreBackendInterface> IDBDatabaseBackendProxy::createObjectStore(const String& name, const String& keyPath, bool autoIncrement, IDBTransactionBackendInterface* transaction, ExceptionCode& ec)
{
    // The transaction pointer is guaranteed to be a pointer to a proxy object as, in the renderer,
    // all implementations of IDB interfaces are proxy objects.
    IDBTransactionBackendProxy* transactionProxy = static_cast<IDBTransactionBackendProxy*>(transaction);
    OwnPtr<WebIDBObjectStore> objectStore = adoptPtr(m_webIDBDatabase->createObjectStore(name, keyPath, autoIncrement, *transactionProxy->getWebIDBTransaction(), ec));
    if (!objectStore)
        return 0;
    return IDBObjectStoreBackendProxy::create(objectStore.release());
}

void IDBDatabaseBackendProxy::deleteObjectStore(const String& name, IDBTransactionBackendInterface* transaction, ExceptionCode& ec)
{
    // The transaction pointer is guaranteed to be a pointer to a proxy object as, in the renderer,
    // all implementations of IDB interfaces are proxy objects.
    IDBTransactionBackendProxy* transactionProxy = static_cast<IDBTransactionBackendProxy*>(transaction);
    m_webIDBDatabase->deleteObjectStore(name, *transactionProxy->getWebIDBTransaction(), ec);
}

void IDBDatabaseBackendProxy::setVersion(const String& version, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks, ExceptionCode& ec)
{
    m_webIDBDatabase->setVersion(version, new WebIDBCallbacksImpl(callbacks), ec);
}

PassRefPtr<IDBTransactionBackendInterface> IDBDatabaseBackendProxy::transaction(DOMStringList* storeNames, unsigned short mode, ExceptionCode& ec)
{
    WebDOMStringList names(storeNames);
    OwnPtr<WebIDBTransaction> transaction = adoptPtr(m_webIDBDatabase->transaction(names, mode, ec));
    if (!transaction) {
        ASSERT(ec);
        return 0;
    }
    return IDBTransactionBackendProxy::create(transaction.release());
}

void IDBDatabaseBackendProxy::close(PassRefPtr<IDBDatabaseCallbacks>)
{
    m_webIDBDatabase->close();
}

void IDBDatabaseBackendProxy::open(PassRefPtr<IDBDatabaseCallbacks> databaseCallbacks)
{
    m_webIDBDatabase->open(new WebIDBDatabaseCallbacksImpl(databaseCallbacks));
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)
