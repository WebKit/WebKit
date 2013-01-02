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
#include "IDBKeyRange.h"
#include "IDBMetadata.h"
#include "IDBObjectStoreBackendInterface.h"
#include "IDBTransactionBackendInterface.h"
#include "WebIDBCallbacks.h"
#include "WebIDBDatabaseCallbacks.h"
#include "WebIDBKeyRange.h"
#include "WebIDBMetadata.h"
#include "WebIDBObjectStoreImpl.h"
#include "WebIDBTransactionImpl.h"

using namespace WebCore;

namespace WebKit {

WebIDBDatabaseImpl::WebIDBDatabaseImpl(PassRefPtr<IDBDatabaseBackendInterface> databaseBackend, WTF::PassRefPtr<IDBDatabaseCallbacksProxy> databaseCallbacks)
    : m_databaseBackend(databaseBackend)
    , m_databaseCallbacks(databaseCallbacks)
{
}

WebIDBDatabaseImpl::~WebIDBDatabaseImpl()
{
}

WebIDBMetadata WebIDBDatabaseImpl::metadata() const
{
    return m_databaseBackend->metadata();
}

WebIDBObjectStore* WebIDBDatabaseImpl::createObjectStore(long long id, const WebString& name, const WebIDBKeyPath& keyPath, bool autoIncrement, const WebIDBTransaction& transaction, WebExceptionCode& ec)
{
    RefPtr<IDBObjectStoreBackendInterface> objectStore = m_databaseBackend->createObjectStore(id, name, keyPath, autoIncrement, transaction.getIDBTransactionBackendInterface(), ec);
    if (!objectStore) {
        ASSERT(ec);
        return 0;
    }
    return new WebIDBObjectStoreImpl(objectStore);
}

void WebIDBDatabaseImpl::deleteObjectStore(long long objectStoreId, const WebIDBTransaction& transaction, WebExceptionCode& ec)
{
    m_databaseBackend->deleteObjectStore(objectStoreId, transaction.getIDBTransactionBackendInterface(), ec);
}

// FIXME: Remove this method in https://bugs.webkit.org/show_bug.cgi?id=103923.
WebIDBTransaction* WebIDBDatabaseImpl::createTransaction(long long id, const WebVector<long long>& objectStoreIds, unsigned short mode)
{
    Vector<int64_t> objectStoreIdList(objectStoreIds.size());
    for (size_t i = 0; i < objectStoreIds.size(); ++i)
        objectStoreIdList[i] = objectStoreIds[i];
    RefPtr<IDBTransactionBackendInterface> transaction = m_databaseBackend->createTransaction(id, objectStoreIdList, static_cast<IDBTransaction::Mode>(mode));
    if (!transaction)
        return 0;
    return new WebIDBTransactionImpl(transaction);
}

void WebIDBDatabaseImpl::createTransaction(long long id, WebIDBDatabaseCallbacks* callbacks, const WebVector<long long>& objectStoreIds, unsigned short mode)
{
    Vector<int64_t> objectStoreIdList(objectStoreIds.size());
    for (size_t i = 0; i < objectStoreIds.size(); ++i)
        objectStoreIdList[i] = objectStoreIds[i];
    RefPtr<IDBDatabaseCallbacksProxy> databaseCallbacksProxy = IDBDatabaseCallbacksProxy::create(adoptPtr(callbacks));
    m_databaseBackend->createTransaction(id, databaseCallbacksProxy.get(), objectStoreIdList, mode);
}

void WebIDBDatabaseImpl::close()
{
    // Use the callbacks passed in to the constructor so that the backend in
    // multi-process chromium knows which database connection is closing.
    if (!m_databaseCallbacks)
        return;
    m_databaseBackend->close(m_databaseCallbacks.release());
}

void WebIDBDatabaseImpl::forceClose()
{
    if (!m_databaseCallbacks)
        return;
    RefPtr<IDBDatabaseCallbacksProxy> callbacks = m_databaseCallbacks.release();
    m_databaseBackend->close(callbacks);
    callbacks->onForcedClose();
}

void WebIDBDatabaseImpl::abort(long long transactionId)
{
    if (m_databaseBackend)
        m_databaseBackend->abort(transactionId);
}

void WebIDBDatabaseImpl::commit(long long transactionId)
{
    if (m_databaseBackend)
        m_databaseBackend->commit(transactionId);
}


void WebIDBDatabaseImpl::openCursor(long long transactionId, long long objectStoreId, long long indexId, const WebIDBKeyRange& keyRange, unsigned short direction, bool keyOnly, TaskType taskType, WebIDBCallbacks* callbacks)
{
    if (m_databaseBackend)
        m_databaseBackend->openCursor(transactionId, objectStoreId, indexId, keyRange, static_cast<IDBCursor::Direction>(direction), keyOnly, static_cast<IDBDatabaseBackendInterface::TaskType>(taskType), IDBCallbacksProxy::create(adoptPtr(callbacks)));
}

void WebIDBDatabaseImpl::count(long long transactionId, long long objectStoreId, long long indexId, const WebIDBKeyRange& keyRange, WebIDBCallbacks* callbacks)
{
    if (m_databaseBackend)
        m_databaseBackend->count(transactionId, objectStoreId, indexId, keyRange, IDBCallbacksProxy::create(adoptPtr(callbacks)));
}

void WebIDBDatabaseImpl::get(long long transactionId, long long objectStoreId, long long indexId, const WebIDBKeyRange& keyRange, bool keyOnly, WebIDBCallbacks* callbacks)
{
    if (m_databaseBackend)
        m_databaseBackend->get(transactionId, objectStoreId, indexId, keyRange, keyOnly, IDBCallbacksProxy::create(adoptPtr(callbacks)));
}

void WebIDBDatabaseImpl::put(long long transactionId, long long objectStoreId, WebVector<unsigned char>* value, const WebIDBKey& key, PutMode putMode, WebIDBCallbacks* callbacks, const WebVector<long long>& webIndexIds, const WebVector<WebIndexKeys>& webIndexKeys)
{
    if (!m_databaseBackend)
        return;

    ASSERT(webIndexIds.size() == webIndexKeys.size());
    Vector<int64_t> indexIds(webIndexIds.size());
    Vector<IDBObjectStoreBackendInterface::IndexKeys> indexKeys(webIndexKeys.size());

    for (size_t i = 0; i < webIndexIds.size(); ++i) {
        indexIds[i] = webIndexIds[i];
        Vector<RefPtr<IDBKey> > indexKeyList(webIndexKeys[i].size());
        for (size_t j = 0; j < webIndexKeys[i].size(); ++j)
            indexKeyList[j] = webIndexKeys[i][j];
        indexKeys[i] = indexKeyList;
    }

    Vector<uint8_t> valueBuffer(value->size());
    valueBuffer.append(value->data(), value->size());
    m_databaseBackend->put(transactionId, objectStoreId, &valueBuffer, key, static_cast<IDBDatabaseBackendInterface::PutMode>(putMode), IDBCallbacksProxy::create(adoptPtr(callbacks)), indexIds, indexKeys);
}

void WebIDBDatabaseImpl::setIndexKeys(long long transactionId, long long objectStoreId, const WebIDBKey& primaryKey, const WebVector<long long>& webIndexIds, const WebVector<WebIndexKeys>& webIndexKeys)
{
    if (!m_databaseBackend)
        return;

    ASSERT(webIndexIds.size() == webIndexKeys.size());
    Vector<int64_t> indexIds(webIndexIds.size());
    Vector<IDBObjectStoreBackendInterface::IndexKeys> indexKeys(webIndexKeys.size());

    for (size_t i = 0; i < webIndexIds.size(); ++i) {
        indexIds[i] = webIndexIds[i];
        Vector<RefPtr<IDBKey> > indexKeyList(webIndexKeys[i].size());
        for (size_t j = 0; j < webIndexKeys[i].size(); ++j)
            indexKeyList[j] = webIndexKeys[i][j];
        indexKeys[i] = indexKeyList;
    }
    m_databaseBackend->setIndexKeys(transactionId, objectStoreId, primaryKey, indexIds, indexKeys);
}

void WebIDBDatabaseImpl::setIndexesReady(long long transactionId, long long objectStoreId, const WebVector<long long>& webIndexIds)
{
    if (!m_databaseBackend)
        return;

    Vector<int64_t> indexIds(webIndexIds.size());
    for (size_t i = 0; i < webIndexIds.size(); ++i)
        indexIds[i] = webIndexIds[i];
    m_databaseBackend->setIndexesReady(transactionId, objectStoreId, indexIds);
}

void WebIDBDatabaseImpl::deleteRange(long long transactionId, long long objectStoreId, const WebIDBKeyRange& keyRange, WebIDBCallbacks* callbacks)
{
    if (m_databaseBackend)
        m_databaseBackend->deleteRange(transactionId, objectStoreId, keyRange, IDBCallbacksProxy::create(adoptPtr(callbacks)));
}

void WebIDBDatabaseImpl::clear(long long transactionId, long long objectStoreId, WebIDBCallbacks* callbacks)
{
    if (m_databaseBackend)
        m_databaseBackend->clear(transactionId, objectStoreId, IDBCallbacksProxy::create(adoptPtr(callbacks)));
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)
