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

#include "CrossThreadTask.h"
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

class IDBIndexBackendImpl::OpenIndexCursorOperation {
public:
    static PassOwnPtr<ScriptExecutionContext::Task> create(PassRefPtr<IDBIndexBackendImpl> index, PassRefPtr<IDBKeyRange> keyRange, unsigned short direction, IDBCursorBackendInterface::CursorType cursorType, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendImpl> transaction)
    {
        return createCallbackTask(&OpenIndexCursorOperation::perform, index, keyRange, direction, cursorType, callbacks, transaction);
    }
private:
    static void perform(ScriptExecutionContext*, PassRefPtr<IDBIndexBackendImpl>, PassRefPtr<IDBKeyRange>, unsigned short untypedDirection, IDBCursorBackendInterface::CursorType, PassRefPtr<IDBCallbacks>, PassRefPtr<IDBTransactionBackendImpl>);
};

class IDBIndexBackendImpl::IndexCountOperation {
public:
    static PassOwnPtr<ScriptExecutionContext::Task> create(PassRefPtr<IDBIndexBackendImpl> index, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendImpl> transaction)
    {
        return createCallbackTask(&IndexCountOperation::perform, index, keyRange, callbacks, transaction);
    }
private:
    static void perform(ScriptExecutionContext*, PassRefPtr<IDBIndexBackendImpl>, PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>, PassRefPtr<IDBTransactionBackendImpl>);
};

class IDBIndexBackendImpl::IndexReferencedValueRetrievalOperation {
public:
    static PassOwnPtr<ScriptExecutionContext::Task> create(PassRefPtr<IDBIndexBackendImpl> index, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendImpl> transaction)
    {
        return createCallbackTask(&IndexReferencedValueRetrievalOperation::perform, index, keyRange, callbacks, transaction);
    }
private:
    static void perform(ScriptExecutionContext*, PassRefPtr<IDBIndexBackendImpl>, PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>, PassRefPtr<IDBTransactionBackendImpl>);
};

class IDBIndexBackendImpl::IndexValueRetrievalOperation {
public:
    static PassOwnPtr<ScriptExecutionContext::Task> create(PassRefPtr<IDBIndexBackendImpl> index, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendImpl> transaction)
    {
        return createCallbackTask(&IndexValueRetrievalOperation::perform, index, keyRange, callbacks, transaction);
    }
private:
    static void perform(ScriptExecutionContext*, PassRefPtr<IDBIndexBackendImpl>, PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>, PassRefPtr<IDBTransactionBackendImpl>);
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

void IDBIndexBackendImpl::OpenIndexCursorOperation::perform(ScriptExecutionContext*, PassRefPtr<IDBIndexBackendImpl> index, PassRefPtr<IDBKeyRange> range, unsigned short untypedDirection, IDBCursorBackendInterface::CursorType cursorType, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendImpl> transaction)
{
    IDB_TRACE("OpenIndexCursorOperation");
    IDBCursor::Direction direction = static_cast<IDBCursor::Direction>(untypedDirection);

    RefPtr<IDBBackingStore::Cursor> backingStoreCursor;

    switch (cursorType) {
    case IDBCursorBackendInterface::IndexKeyCursor:
        backingStoreCursor = index->backingStore()->openIndexKeyCursor(transaction->backingStoreTransaction(), index->databaseId(), index->m_objectStoreBackend->id(), index->id(), range.get(), direction);
        break;
    case IDBCursorBackendInterface::IndexCursor:
        backingStoreCursor = index->backingStore()->openIndexCursor(transaction->backingStoreTransaction(), index->databaseId(), index->m_objectStoreBackend->id(), index->id(), range.get(), direction);
        break;
    case IDBCursorBackendInterface::ObjectStoreCursor:
    case IDBCursorBackendInterface::InvalidCursorType:
        ASSERT_NOT_REACHED();
        break;
    }

    if (!backingStoreCursor) {
        callbacks->onSuccess(static_cast<SerializedScriptValue*>(0));
        return;
    }

    RefPtr<IDBCursorBackendImpl> cursor = IDBCursorBackendImpl::create(backingStoreCursor.get(), cursorType, transaction.get(), index->m_objectStoreBackend);
    callbacks->onSuccess(cursor, cursor->key(), cursor->primaryKey(), cursor->value());
}

void IDBIndexBackendImpl::openCursor(PassRefPtr<IDBKeyRange> keyRange, unsigned short direction, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode&)
{
    IDB_TRACE("IDBIndexBackendImpl::openCursor");
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    if (!transaction->scheduleTask(OpenIndexCursorOperation::create(this, keyRange, direction, IDBCursorBackendInterface::IndexCursor, callbacks, transaction)))
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::AbortError));
}

void IDBIndexBackendImpl::openKeyCursor(PassRefPtr<IDBKeyRange> keyRange, unsigned short direction, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode&)
{
    IDB_TRACE("IDBIndexBackendImpl::openKeyCursor");
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    if (!transaction->scheduleTask(OpenIndexCursorOperation::create(this, keyRange, direction, IDBCursorBackendInterface::IndexKeyCursor, callbacks, transaction)))
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::AbortError));
}

void IDBIndexBackendImpl::IndexCountOperation::perform(ScriptExecutionContext*, PassRefPtr<IDBIndexBackendImpl> index, PassRefPtr<IDBKeyRange> range, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendImpl> transaction)
{
    IDB_TRACE("IndexCountOperation");
    uint32_t count = 0;

    RefPtr<IDBBackingStore::Cursor> backingStoreCursor = index->backingStore()->openIndexKeyCursor(transaction->backingStoreTransaction(), index->databaseId(), index->m_objectStoreBackend->id(), index->id(), range.get(), IDBCursor::NEXT);
    if (!backingStoreCursor) {
        callbacks->onSuccess(count);
        return;
    }

    do {
        ++count;
    } while (backingStoreCursor->continueFunction(0));

    callbacks->onSuccess(count);
}

void IDBIndexBackendImpl::count(PassRefPtr<IDBKeyRange> range, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode&)
{
    IDB_TRACE("IDBIndexBackendImpl::count");
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    if (!transaction->scheduleTask(IndexCountOperation::create(this, range, callbacks, transaction)))
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::AbortError));
}

void IDBIndexBackendImpl::IndexReferencedValueRetrievalOperation::perform(ScriptExecutionContext*, PassRefPtr<IDBIndexBackendImpl> index, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendImpl> transaction)
{
    IDB_TRACE("IndexReferencedValueRetrievalOperation");

    RefPtr<IDBKey> key;

    if (keyRange->isOnlyKey())
        key = keyRange->lower();
    else {
        RefPtr<IDBBackingStore::Cursor> backingStoreCursor = index->backingStore()->openIndexCursor(transaction->backingStoreTransaction(), index->databaseId(), index->m_objectStoreBackend->id(), index->id(), keyRange.get(), IDBCursor::NEXT);

        if (!backingStoreCursor) {
            callbacks->onSuccess();
            return;
        }
        key = backingStoreCursor->key();
    }

    RefPtr<IDBKey> primaryKey;
    bool ok = index->backingStore()->getPrimaryKeyViaIndex(transaction->backingStoreTransaction(), index->databaseId(), index->m_objectStoreBackend->id(), index->id(), *key, primaryKey);
    if (!ok) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error in getPrimaryKeyViaIndex."));
        return;
    }

    String value;
    ok = index->backingStore()->getRecord(transaction->backingStoreTransaction(), index->databaseId(), index->m_objectStoreBackend->id(), *primaryKey, value);
    if (!ok) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error in getRecord."));
        return;
    }

    if (value.isNull()) {
        callbacks->onSuccess();
        return;
    }
    if (index->m_objectStoreBackend->autoIncrement() && !index->m_objectStoreBackend->keyPath().isNull()) {
        callbacks->onSuccess(SerializedScriptValue::createFromWire(value),
                             primaryKey, index->m_objectStoreBackend->keyPath());
        return;
    }
    callbacks->onSuccess(SerializedScriptValue::createFromWire(value));
}


void IDBIndexBackendImpl::IndexValueRetrievalOperation::perform(ScriptExecutionContext*, PassRefPtr<IDBIndexBackendImpl> index, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendImpl> transaction)
{
    IDB_TRACE("IndexValueRetrievalOperation");

    RefPtr<IDBBackingStore::Cursor> backingStoreCursor =
            index->backingStore()->openIndexKeyCursor(transaction->backingStoreTransaction(), index->databaseId(), index->m_objectStoreBackend->id(), index->id(), keyRange.get(), IDBCursor::NEXT);

    if (!backingStoreCursor) {
        callbacks->onSuccess(static_cast<IDBKey*>(0));
        return;
    }

    RefPtr<IDBKey> keyResult;
    bool ok = index->backingStore()->getPrimaryKeyViaIndex(transaction->backingStoreTransaction(), index->databaseId(), index->m_objectStoreBackend->id(), index->id(), *backingStoreCursor->key(), keyResult);
    if (!ok) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error in getPrimaryKeyViaIndex."));
        return;
    }
    if (!keyResult) {
        callbacks->onSuccess(static_cast<IDBKey*>(0));
        return;
    }
    callbacks->onSuccess(keyResult.get());
}

void IDBIndexBackendImpl::get(PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode&)
{
    IDB_TRACE("IDBIndexBackendImpl::get");
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    if (!transaction->scheduleTask(IndexReferencedValueRetrievalOperation::create(this, keyRange, callbacks, transaction)))
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::AbortError));
}

void IDBIndexBackendImpl::getKey(PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode&)
{
    IDB_TRACE("IDBIndexBackendImpl::getKey");
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    if (!transaction->scheduleTask(IndexValueRetrievalOperation::create(this, keyRange, callbacks, transaction)))
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::AbortError));
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
