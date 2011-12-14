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
#include "IDBIndexBackendProxy.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBCallbacks.h"
#include "IDBKeyRange.h"
#include "IDBTransactionBackendProxy.h"
#include "WebIDBCallbacksImpl.h"
#include "WebIDBDatabaseError.h"
#include "WebIDBIndex.h"
#include "WebIDBKey.h"
#include "WebIDBKeyRange.h"

using namespace WebCore;

namespace WebKit {

PassRefPtr<IDBIndexBackendInterface> IDBIndexBackendProxy::create(PassOwnPtr<WebIDBIndex> index)
{
    return adoptRef(new IDBIndexBackendProxy(index));
}

IDBIndexBackendProxy::IDBIndexBackendProxy(PassOwnPtr<WebIDBIndex> index)
    : m_webIDBIndex(index)
{
}

IDBIndexBackendProxy::~IDBIndexBackendProxy()
{
}

String IDBIndexBackendProxy::name()
{
    return m_webIDBIndex->name();
}

String IDBIndexBackendProxy::storeName()
{
    return m_webIDBIndex->storeName();
}

String IDBIndexBackendProxy::keyPath()
{
    return m_webIDBIndex->keyPath();
}

bool IDBIndexBackendProxy::unique()
{
    return m_webIDBIndex->unique();
}

void IDBIndexBackendProxy::openCursor(PassRefPtr<IDBKeyRange> keyRange, unsigned short direction, PassRefPtr<IDBCallbacks> callbacks, IDBTransactionBackendInterface* transaction, ExceptionCode& ec)
{
    // The transaction pointer is guaranteed to be a pointer to a proxy object as, in the renderer,
    // all implementations of IDB interfaces are proxy objects.
    IDBTransactionBackendProxy* transactionProxy = static_cast<IDBTransactionBackendProxy*>(transaction);
    m_webIDBIndex->openObjectCursor(keyRange, direction, new WebIDBCallbacksImpl(callbacks), *transactionProxy->getWebIDBTransaction(), ec);
}

void IDBIndexBackendProxy::openKeyCursor(PassRefPtr<IDBKeyRange> keyRange, unsigned short direction, PassRefPtr<IDBCallbacks> callbacks, IDBTransactionBackendInterface* transaction, ExceptionCode& ec)
{
    // The transaction pointer is guaranteed to be a pointer to a proxy object as, in the renderer,
    // all implementations of IDB interfaces are proxy objects.
    IDBTransactionBackendProxy* transactionProxy = static_cast<IDBTransactionBackendProxy*>(transaction);
    m_webIDBIndex->openKeyCursor(keyRange, direction, new WebIDBCallbacksImpl(callbacks), *transactionProxy->getWebIDBTransaction(), ec);
}

void IDBIndexBackendProxy::get(PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks> callbacks, IDBTransactionBackendInterface* transaction, ExceptionCode& ec)
{
    // The transaction pointer is guaranteed to be a pointer to a proxy object as, in the renderer,
    // all implementations of IDB interfaces are proxy objects.
    IDBTransactionBackendProxy* transactionProxy = static_cast<IDBTransactionBackendProxy*>(transaction);
    m_webIDBIndex->getObject(key, new WebIDBCallbacksImpl(callbacks), *transactionProxy->getWebIDBTransaction(), ec);
}

void IDBIndexBackendProxy::getKey(PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks> callbacks, IDBTransactionBackendInterface* transaction, ExceptionCode& ec)
{
    // The transaction pointer is guaranteed to be a pointer to a proxy object as, in the renderer,
    // all implementations of IDB interfaces are proxy objects.
    IDBTransactionBackendProxy* transactionProxy = static_cast<IDBTransactionBackendProxy*>(transaction);
    m_webIDBIndex->getKey(key, new WebIDBCallbacksImpl(callbacks), *transactionProxy->getWebIDBTransaction(), ec);
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)
