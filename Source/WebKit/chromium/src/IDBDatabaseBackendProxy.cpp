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
#include "IDBKeyRange.h"
#include "IDBMetadata.h"
#include "IDBTransactionBackendProxy.h"
#include "WebDOMStringList.h"
#include "WebFrameImpl.h"
#include "WebIDBCallbacksImpl.h"
#include "WebIDBCursor.h"
#include "WebIDBDatabase.h"
#include "WebIDBDatabaseCallbacksImpl.h"
#include "WebIDBDatabaseError.h"
#include "WebIDBKeyRange.h"
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

IDBDatabaseMetadata IDBDatabaseBackendProxy::metadata() const
{
    return m_webIDBDatabase->metadata();
}

void IDBDatabaseBackendProxy::createObjectStore(int64_t transactionId, int64_t objectStoreId, const String& name, const IDBKeyPath& keyPath, bool autoIncrement)
{
    if (m_webIDBDatabase)
        m_webIDBDatabase->createObjectStore(transactionId, objectStoreId, name, keyPath, autoIncrement);
}

void IDBDatabaseBackendProxy::deleteObjectStore(int64_t transactionId, int64_t objectStoreId)
{
    if (m_webIDBDatabase)
        m_webIDBDatabase->deleteObjectStore(transactionId, objectStoreId);
}

PassRefPtr<IDBTransactionBackendInterface> IDBDatabaseBackendProxy::createTransaction(int64_t id, const Vector<int64_t>& objectStoreIds, IDBTransaction::Mode mode)
{
    OwnPtr<WebIDBTransaction> transaction = adoptPtr(m_webIDBDatabase->createTransaction(id, objectStoreIds, mode));
    if (!transaction)
        return 0;

    return IDBTransactionBackendProxy::create(transaction.release());
}

void IDBDatabaseBackendProxy::createTransaction(int64_t id, PassRefPtr<IDBDatabaseCallbacks> callbacks, const Vector<int64_t>& objectStoreIds, unsigned short mode)
{
    m_webIDBDatabase->createTransaction(id, new WebIDBDatabaseCallbacksImpl(callbacks), objectStoreIds, mode);
}

void IDBDatabaseBackendProxy::commit(int64_t transactionId)
{
    m_webIDBDatabase->commit(transactionId);
}

void IDBDatabaseBackendProxy::abort(int64_t transactionId)
{
    m_webIDBDatabase->abort(transactionId);
}

void IDBDatabaseBackendProxy::openCursor(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, unsigned short direction, bool keyOnly, TaskType taskType, PassRefPtr<IDBCallbacks> callbacks)
{
    m_webIDBDatabase->openCursor(transactionId, objectStoreId, indexId, keyRange, static_cast<WebIDBCursor::Direction>(direction), keyOnly, static_cast<WebIDBDatabase::TaskType>(taskType), new WebIDBCallbacksImpl(callbacks));
}

void IDBDatabaseBackendProxy::count(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
{
    if (m_webIDBDatabase)
        m_webIDBDatabase->count(transactionId, objectStoreId, indexId, keyRange, new WebIDBCallbacksImpl(callbacks));
}

void IDBDatabaseBackendProxy::get(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange> keyRange, bool keyOnly, PassRefPtr<IDBCallbacks> callbacks)
{
    if (m_webIDBDatabase)
        m_webIDBDatabase->get(transactionId, objectStoreId, indexId, keyRange, keyOnly, new WebIDBCallbacksImpl(callbacks));
}

void IDBDatabaseBackendProxy::put(int64_t transactionId, int64_t objectStoreId, Vector<uint8_t>* buffer, PassRefPtr<IDBKey> key, PutMode putMode, PassRefPtr<IDBCallbacks> callbacks, const Vector<int64_t>& indexIds, const Vector<IndexKeys>& indexKeys)
{
    if (m_webIDBDatabase) {
        WebVector<unsigned char> webBuffer(buffer->size());
        webBuffer.assign(buffer->data(), buffer->size());
        m_webIDBDatabase->put(transactionId, objectStoreId, &webBuffer, key, static_cast<WebIDBDatabase::PutMode>(putMode), new WebIDBCallbacksImpl(callbacks), indexIds, indexKeys);
    }
}

void IDBDatabaseBackendProxy::setIndexKeys(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBKey> primaryKey, const Vector<int64_t>& indexIds, const Vector<IndexKeys>& indexKeys)
{
    if (m_webIDBDatabase)
        m_webIDBDatabase->setIndexKeys(transactionId, objectStoreId, primaryKey, indexIds, indexKeys);
}

void IDBDatabaseBackendProxy::setIndexesReady(int64_t transactionId, int64_t objectStoreId, const Vector<int64_t>& indexIds)
{
    if (m_webIDBDatabase)
        m_webIDBDatabase->setIndexesReady(transactionId, objectStoreId, indexIds);
}

void IDBDatabaseBackendProxy::deleteRange(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
{
    if (m_webIDBDatabase)
        m_webIDBDatabase->deleteRange(transactionId, objectStoreId, keyRange, new WebIDBCallbacksImpl(callbacks));
}

void IDBDatabaseBackendProxy::clear(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBCallbacks> callbacks)
{
    if (m_webIDBDatabase)
        m_webIDBDatabase->clear(transactionId, objectStoreId, new WebIDBCallbacksImpl(callbacks));
}

void IDBDatabaseBackendProxy::createIndex(int64_t transactionId, int64_t objectStoreId, int64_t indexId, const String& name, const WebCore::IDBKeyPath& keyPath, bool unique, bool multiEntry)
{
    if (m_webIDBDatabase)
        m_webIDBDatabase->createIndex(transactionId, objectStoreId, indexId, name, keyPath, unique, multiEntry);
}

void IDBDatabaseBackendProxy::deleteIndex(int64_t transactionId, int64_t objectStoreId, int64_t indexId)
{
    if (m_webIDBDatabase)
        m_webIDBDatabase->deleteIndex(transactionId, objectStoreId, indexId);
}

void IDBDatabaseBackendProxy::close(PassRefPtr<IDBDatabaseCallbacks>)
{
    m_webIDBDatabase->close();
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)
