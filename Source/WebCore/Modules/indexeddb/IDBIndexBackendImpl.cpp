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
#include "IDBIndexBackendImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBBackingStore.h"
#include "IDBCallbacks.h"
#include "IDBCursorBackendImpl.h"
#include "IDBDatabaseBackendImpl.h"
#include "IDBDatabaseException.h"
#include "IDBKey.h"
#include "IDBKeyRange.h"
#include "IDBMetadata.h"
#include "IDBObjectStoreBackendImpl.h"
#include "IDBTracing.h"
#include "IDBTransactionBackendImpl.h"

namespace WebCore {

class IDBIndexBackendImpl::OpenIndexCursorOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBIndexBackendImpl> index, PassRefPtr<IDBKeyRange> keyRange, unsigned short direction, IDBCursorBackendInterface::CursorType cursorType, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptPtr(new OpenIndexCursorOperation(index, keyRange, direction, cursorType, callbacks));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    OpenIndexCursorOperation(PassRefPtr<IDBIndexBackendImpl> index, PassRefPtr<IDBKeyRange> keyRange, unsigned short direction, IDBCursorBackendInterface::CursorType cursorType, PassRefPtr<IDBCallbacks> callbacks)
        : m_index(index)
        , m_keyRange(keyRange)
        , m_direction(direction)
        , m_cursorType(cursorType)
        , m_callbacks(callbacks)
    {
    }

    RefPtr<IDBIndexBackendImpl> m_index;
    RefPtr<IDBKeyRange> m_keyRange;
    unsigned short m_direction;
    IDBCursorBackendInterface::CursorType m_cursorType;
    RefPtr<IDBCallbacks> m_callbacks;
};

class IDBIndexBackendImpl::IndexCountOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBIndexBackendImpl> index, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptPtr(new IndexCountOperation(index, keyRange, callbacks));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    IndexCountOperation(PassRefPtr<IDBIndexBackendImpl> index, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
        : m_index(index)
        , m_keyRange(keyRange)
        , m_callbacks(callbacks)
    {
    }

    RefPtr<IDBIndexBackendImpl> m_index;
    RefPtr<IDBKeyRange> m_keyRange;
    RefPtr<IDBCallbacks> m_callbacks;
};

class IDBIndexBackendImpl::IndexReferencedValueRetrievalOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBIndexBackendImpl> index, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptPtr(new IndexReferencedValueRetrievalOperation(index, keyRange, callbacks));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    IndexReferencedValueRetrievalOperation(PassRefPtr<IDBIndexBackendImpl> index, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
        : m_index(index)
        , m_keyRange(keyRange)
        , m_callbacks(callbacks)
    {
    }

    RefPtr<IDBIndexBackendImpl> m_index;
    RefPtr<IDBKeyRange> m_keyRange;
    RefPtr<IDBCallbacks> m_callbacks;
};

class IDBIndexBackendImpl::IndexValueRetrievalOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBIndexBackendImpl> index, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptPtr(new IndexValueRetrievalOperation(index, keyRange, callbacks));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    IndexValueRetrievalOperation(PassRefPtr<IDBIndexBackendImpl> index, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
        : m_index(index)
        , m_keyRange(keyRange)
        , m_callbacks(callbacks)
    {
    }

    RefPtr<IDBIndexBackendImpl> m_index;
    RefPtr<IDBKeyRange> m_keyRange;
    RefPtr<IDBCallbacks> m_callbacks;
};


IDBIndexBackendImpl::IDBIndexBackendImpl(const IDBDatabaseBackendImpl* database, IDBObjectStoreBackendImpl* objectStoreBackend, const IDBIndexMetadata& metadata)
    : m_database(database)
    , m_objectStoreBackend(objectStoreBackend)
    , m_metadata(metadata)
{
}

IDBIndexBackendImpl::~IDBIndexBackendImpl()
{
}

void IDBIndexBackendImpl::OpenIndexCursorOperation::perform(IDBTransactionBackendImpl* transaction)
{
    IDB_TRACE("OpenIndexCursorOperation");
    IDBCursor::Direction direction = static_cast<IDBCursor::Direction>(m_direction);

    RefPtr<IDBBackingStore::Cursor> backingStoreCursor;

    switch (m_cursorType) {
    case IDBCursorBackendInterface::KeyOnly:
        backingStoreCursor = m_index->backingStore()->openIndexKeyCursor(transaction->backingStoreTransaction(), m_index->databaseId(), m_index->m_objectStoreBackend->id(), m_index->id(), m_keyRange.get(), direction);
        break;
    case IDBCursorBackendInterface::KeyAndValue:
        backingStoreCursor = m_index->backingStore()->openIndexCursor(transaction->backingStoreTransaction(), m_index->databaseId(), m_index->m_objectStoreBackend->id(), m_index->id(), m_keyRange.get(), direction);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    if (!backingStoreCursor) {
        m_callbacks->onSuccess(static_cast<SerializedScriptValue*>(0));
        return;
    }

    RefPtr<IDBCursorBackendImpl> cursor = IDBCursorBackendImpl::create(backingStoreCursor.get(), m_cursorType, transaction, m_index->m_objectStoreBackend);
    m_callbacks->onSuccess(cursor, cursor->key(), cursor->primaryKey(), cursor->value());
}

void IDBIndexBackendImpl::openCursor(PassRefPtr<IDBKeyRange> keyRange, unsigned short direction, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode&)
{
    IDB_TRACE("IDBIndexBackendImpl::openCursor");
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    transaction->scheduleTask(OpenIndexCursorOperation::create(this, keyRange, direction, IDBCursorBackendInterface::KeyAndValue, callbacks));
}

void IDBIndexBackendImpl::openKeyCursor(PassRefPtr<IDBKeyRange> keyRange, unsigned short direction, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode&)
{
    IDB_TRACE("IDBIndexBackendImpl::openKeyCursor");
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    transaction->scheduleTask(OpenIndexCursorOperation::create(this, keyRange, direction, IDBCursorBackendInterface::KeyOnly, callbacks));
}

void IDBIndexBackendImpl::IndexCountOperation::perform(IDBTransactionBackendImpl* transaction)
{
    IDB_TRACE("IndexCountOperation");
    uint32_t count = 0;

    RefPtr<IDBBackingStore::Cursor> backingStoreCursor = m_index->backingStore()->openIndexKeyCursor(transaction->backingStoreTransaction(), m_index->databaseId(), m_index->m_objectStoreBackend->id(), m_index->id(), m_keyRange.get(), IDBCursor::NEXT);
    if (!backingStoreCursor) {
        m_callbacks->onSuccess(count);
        return;
    }

    do {
        ++count;
    } while (backingStoreCursor->continueFunction(0));

    m_callbacks->onSuccess(count);
}

void IDBIndexBackendImpl::count(PassRefPtr<IDBKeyRange> range, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode&)
{
    IDB_TRACE("IDBIndexBackendImpl::count");
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    transaction->scheduleTask(IndexCountOperation::create(this, range, callbacks));
}

void IDBIndexBackendImpl::IndexReferencedValueRetrievalOperation::perform(IDBTransactionBackendImpl* transaction)
{
    IDB_TRACE("IndexReferencedValueRetrievalOperation");

    RefPtr<IDBKey> key;

    if (m_keyRange->isOnlyKey())
        key = m_keyRange->lower();
    else {
        RefPtr<IDBBackingStore::Cursor> backingStoreCursor = m_index->backingStore()->openIndexCursor(transaction->backingStoreTransaction(), m_index->databaseId(), m_index->m_objectStoreBackend->id(), m_index->id(), m_keyRange.get(), IDBCursor::NEXT);

        if (!backingStoreCursor) {
            m_callbacks->onSuccess();
            return;
        }
        key = backingStoreCursor->key();
    }

    RefPtr<IDBKey> primaryKey;
    bool ok = m_index->backingStore()->getPrimaryKeyViaIndex(transaction->backingStoreTransaction(), m_index->databaseId(), m_index->m_objectStoreBackend->id(), m_index->id(), *key, primaryKey);
    if (!ok) {
        m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error in getPrimaryKeyViaIndex."));
        return;
    }

    Vector<uint8_t> value;
    ok = m_index->backingStore()->getRecord(transaction->backingStoreTransaction(), m_index->databaseId(), m_index->m_objectStoreBackend->id(), *primaryKey, value);
    if (!ok) {
        m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error in getRecord."));
        return;
    }

    if (value.isEmpty()) {
        m_callbacks->onSuccess();
        return;
    }
    if (m_index->m_objectStoreBackend->autoIncrement() && !m_index->m_objectStoreBackend->keyPath().isNull()) {
        m_callbacks->onSuccess(SerializedScriptValue::createFromWireBytes(value), primaryKey, m_index->m_objectStoreBackend->keyPath());
        return;
    }
    m_callbacks->onSuccess(SerializedScriptValue::createFromWireBytes(value));
}


void IDBIndexBackendImpl::IndexValueRetrievalOperation::perform(IDBTransactionBackendImpl* transaction)
{
    IDB_TRACE("IndexValueRetrievalOperation");

    RefPtr<IDBBackingStore::Cursor> backingStoreCursor = m_index->backingStore()->openIndexKeyCursor(transaction->backingStoreTransaction(), m_index->databaseId(), m_index->m_objectStoreBackend->id(), m_index->id(), m_keyRange.get(), IDBCursor::NEXT);

    if (!backingStoreCursor) {
        m_callbacks->onSuccess(static_cast<IDBKey*>(0));
        return;
    }

    RefPtr<IDBKey> keyResult;
    bool ok = m_index->backingStore()->getPrimaryKeyViaIndex(transaction->backingStoreTransaction(), m_index->databaseId(), m_index->m_objectStoreBackend->id(), m_index->id(), *backingStoreCursor->key(), keyResult);
    if (!ok) {
        m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error in getPrimaryKeyViaIndex."));
        return;
    }
    if (!keyResult) {
        m_callbacks->onSuccess(static_cast<IDBKey*>(0));
        return;
    }
    m_callbacks->onSuccess(keyResult.get());
}

void IDBIndexBackendImpl::get(PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode&)
{
    IDB_TRACE("IDBIndexBackendImpl::get");
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    transaction->scheduleTask(IndexReferencedValueRetrievalOperation::create(this, keyRange, callbacks));
}

void IDBIndexBackendImpl::getKey(PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode&)
{
    IDB_TRACE("IDBIndexBackendImpl::getKey");
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    transaction->scheduleTask(IndexValueRetrievalOperation::create(this, keyRange, callbacks));
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
