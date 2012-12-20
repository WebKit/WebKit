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
#include "IDBObjectStoreBackendImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBBackingStore.h"
#include "IDBBindingUtilities.h"
#include "IDBCallbacks.h"
#include "IDBCursorBackendImpl.h"
#include "IDBDatabaseBackendImpl.h"
#include "IDBDatabaseException.h"
#include "IDBIndexBackendImpl.h"
#include "IDBKey.h"
#include "IDBKeyPath.h"
#include "IDBKeyRange.h"
#include "IDBTracing.h"
#include "IDBTransactionBackendImpl.h"
#include <wtf/MathExtras.h>

namespace WebCore {

class IDBObjectStoreBackendImpl::ObjectStoreRetrievalOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptPtr(new ObjectStoreRetrievalOperation(objectStore, keyRange, callbacks));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    ObjectStoreRetrievalOperation(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
        : m_objectStore(objectStore)
        , m_keyRange(keyRange)
        , m_callbacks(callbacks)
    {
    }

    RefPtr<IDBObjectStoreBackendImpl> m_objectStore;
    RefPtr<IDBKeyRange> m_keyRange;
    RefPtr<IDBCallbacks> m_callbacks;
};

class IDBObjectStoreBackendImpl::ObjectStoreStorageOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<SerializedScriptValue> value, PassRefPtr<IDBKey> key, PutMode putMode, PassRefPtr<IDBCallbacks> callbacks, PassOwnPtr<Vector<int64_t> > popIndexIds, PassOwnPtr<Vector<IndexKeys> > popIndexKeys)
    {
        return adoptPtr(new ObjectStoreStorageOperation(objectStore, value, key, putMode, callbacks, popIndexIds, popIndexKeys));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    ObjectStoreStorageOperation(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<SerializedScriptValue> value, PassRefPtr<IDBKey> key, PutMode putMode, PassRefPtr<IDBCallbacks> callbacks, PassOwnPtr<Vector<int64_t> > popIndexIds, PassOwnPtr<Vector<IndexKeys> > popIndexKeys)
        : m_objectStore(objectStore)
        , m_value(value)
        , m_key(key)
        , m_putMode(putMode)
        , m_callbacks(callbacks)
        , m_popIndexIds(popIndexIds)
        , m_popIndexKeys(popIndexKeys)
    {
    }

    RefPtr<IDBObjectStoreBackendImpl> m_objectStore;
    RefPtr<SerializedScriptValue> m_value;
    RefPtr<IDBKey> m_key;
    PutMode m_putMode;
    RefPtr<IDBCallbacks> m_callbacks;
    OwnPtr<Vector<int64_t> > m_popIndexIds;
    OwnPtr<Vector<IndexKeys> > m_popIndexKeys;
};

class IDBObjectStoreBackendImpl::ObjectStoreIndexesReadyOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassOwnPtr<Vector<int64_t> > popIndexIds)
    {
        return adoptPtr(new ObjectStoreIndexesReadyOperation(objectStore, popIndexIds));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    ObjectStoreIndexesReadyOperation(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassOwnPtr<Vector<int64_t> > popIndexIds)
        : m_objectStore(objectStore)
        , m_popIndexIds(popIndexIds)
    {
    }

    RefPtr<IDBObjectStoreBackendImpl> m_objectStore;
    OwnPtr<Vector<int64_t> > m_popIndexIds;
};

class IDBObjectStoreBackendImpl::ObjectStoreDeletionOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptPtr(new ObjectStoreDeletionOperation(objectStore, keyRange, callbacks));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    ObjectStoreDeletionOperation(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
        : m_objectStore(objectStore)
        , m_keyRange(keyRange)
        , m_callbacks(callbacks)
    {
    }

    RefPtr<IDBObjectStoreBackendImpl> m_objectStore;
    RefPtr<IDBKeyRange> m_keyRange;
    RefPtr<IDBCallbacks> m_callbacks;
};

class IDBObjectStoreBackendImpl::ObjectStoreClearOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptPtr(new ObjectStoreClearOperation(objectStore, callbacks));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    ObjectStoreClearOperation(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBCallbacks> callbacks)
        : m_objectStore(objectStore)
        , m_callbacks(callbacks)
    {
    }

    RefPtr<IDBObjectStoreBackendImpl> m_objectStore;
    RefPtr<IDBCallbacks> m_callbacks;
};

class IDBObjectStoreBackendImpl::CreateIndexOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index)
    {
        return adoptPtr(new CreateIndexOperation(objectStore, index));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    CreateIndexOperation(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index)
        : m_objectStore(objectStore)
        , m_index(index)
    {
    }

    RefPtr<IDBObjectStoreBackendImpl> m_objectStore;
    RefPtr<IDBIndexBackendImpl> m_index;
};

class IDBObjectStoreBackendImpl::DeleteIndexOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index)
    {
        return adoptPtr(new DeleteIndexOperation(objectStore, index));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    DeleteIndexOperation(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index)
        : m_objectStore(objectStore)
        , m_index(index)
    {
    }

    RefPtr<IDBObjectStoreBackendImpl> m_objectStore;
    RefPtr<IDBIndexBackendImpl> m_index;
};

class IDBObjectStoreBackendImpl::OpenObjectStoreCursorOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKeyRange> range, IDBCursor::Direction direction, PassRefPtr<IDBCallbacks> callbacks, IDBTransactionBackendInterface::TaskType taskType)
    {
        return adoptPtr(new OpenObjectStoreCursorOperation(objectStore, range, direction, callbacks, taskType));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    OpenObjectStoreCursorOperation(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKeyRange> range, IDBCursor::Direction direction, PassRefPtr<IDBCallbacks> callbacks, IDBTransactionBackendInterface::TaskType taskType)
        : m_objectStore(objectStore)
        , m_range(range)
        , m_direction(direction)
        , m_callbacks(callbacks)
        , m_taskType(taskType)
    {
    }

    RefPtr<IDBObjectStoreBackendImpl> m_objectStore;
    RefPtr<IDBKeyRange> m_range;
    IDBCursor::Direction m_direction;
    RefPtr<IDBCallbacks> m_callbacks;
    IDBTransactionBackendInterface::TaskType m_taskType;
};

class IDBObjectStoreBackendImpl::ObjectStoreCountOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKeyRange> range, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptPtr(new ObjectStoreCountOperation(objectStore, range, callbacks));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    ObjectStoreCountOperation(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKeyRange> range, PassRefPtr<IDBCallbacks> callbacks)
        : m_objectStore(objectStore)
        , m_range(range)
        , m_callbacks(callbacks)
    {
    }

    RefPtr<IDBObjectStoreBackendImpl> m_objectStore;
    RefPtr<IDBKeyRange> m_range;
    RefPtr<IDBCallbacks> m_callbacks;
};

class IDBObjectStoreBackendImpl::CreateIndexAbortOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index)
    {
        return adoptPtr(new CreateIndexAbortOperation(objectStore, index));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    CreateIndexAbortOperation(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index)
        : m_objectStore(objectStore)
        , m_index(index)
    {
    }

    RefPtr<IDBObjectStoreBackendImpl> m_objectStore;
    RefPtr<IDBIndexBackendImpl> m_index;
};

class IDBObjectStoreBackendImpl::DeleteIndexAbortOperation : public IDBTransactionBackendImpl::Operation {
public:
    static PassOwnPtr<IDBTransactionBackendImpl::Operation> create(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index)
    {
        return adoptPtr(new DeleteIndexAbortOperation(objectStore, index));
    }
    virtual void perform(IDBTransactionBackendImpl*);
private:
    DeleteIndexAbortOperation(PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index)
        : m_objectStore(objectStore)
        , m_index(index)
    {
    }

    RefPtr<IDBObjectStoreBackendImpl> m_objectStore;
    RefPtr<IDBIndexBackendImpl> m_index;
};


IDBObjectStoreBackendImpl::~IDBObjectStoreBackendImpl()
{
}

IDBObjectStoreBackendImpl::IDBObjectStoreBackendImpl(const IDBDatabaseBackendImpl* database, const IDBObjectStoreMetadata& metadata)
    : m_database(database)
    , m_metadata(metadata)
{
    loadIndexes();
}

IDBObjectStoreMetadata IDBObjectStoreBackendImpl::metadata() const
{
    IDBObjectStoreMetadata metadata(m_metadata);
    for (IndexMap::const_iterator it = m_indexes.begin(); it != m_indexes.end(); ++it)
        metadata.indexes.set(it->key, it->value->metadata());
    return metadata;
}

void IDBObjectStoreBackendImpl::get(PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode&)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::get");
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    if (!transaction->scheduleTask(ObjectStoreRetrievalOperation::create(this, keyRange, callbacks)))
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::AbortError));
}

void IDBObjectStoreBackendImpl::ObjectStoreRetrievalOperation::perform(IDBTransactionBackendImpl* transaction)
{
    IDB_TRACE("ObjectStoreRetrievalOperation");
    RefPtr<IDBKey> key;
    if (m_keyRange->isOnlyKey())
        key = m_keyRange->lower();
    else {
        RefPtr<IDBBackingStore::Cursor> backingStoreCursor = m_objectStore->backingStore()->openObjectStoreCursor(transaction->backingStoreTransaction(), m_objectStore->databaseId(), m_objectStore->id(), m_keyRange.get(), IDBCursor::NEXT);
        if (!backingStoreCursor) {
            m_callbacks->onSuccess();
            return;
        }
        key = backingStoreCursor->key();
    }

    Vector<uint8_t> wireData;
    bool ok = m_objectStore->backingStore()->getRecord(transaction->backingStoreTransaction(), m_objectStore->databaseId(), m_objectStore->id(), *key, wireData);
    if (!ok) {
        m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error in getRecord."));
        return;
    }
    if (wireData.isEmpty()) {
        m_callbacks->onSuccess();
        return;
    }

    if (m_objectStore->autoIncrement() && !m_objectStore->keyPath().isNull()) {
        m_callbacks->onSuccess(SerializedScriptValue::createFromWireBytes(wireData), key, m_objectStore->keyPath());
        return;
    }
    m_callbacks->onSuccess(SerializedScriptValue::createFromWireBytes(wireData));
}

void IDBObjectStoreBackendImpl::put(PassRefPtr<SerializedScriptValue> value, PassRefPtr<IDBKey> key, PutMode putMode, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, const Vector<int64_t>& indexIds, const Vector<IndexKeys>& indexKeys)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::put");

    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    ASSERT(transaction->mode() != IDBTransaction::READ_ONLY);

    OwnPtr<Vector<int64_t> > newIndexIds = adoptPtr(new Vector<int64_t>(indexIds));
    OwnPtr<Vector<IndexKeys> > newIndexKeys = adoptPtr(new Vector<IndexKeys>(indexKeys));

    ASSERT(autoIncrement() || key.get());

    if (!transaction->scheduleTask(ObjectStoreStorageOperation::create(this, value, key, putMode, callbacks, newIndexIds.release(), newIndexKeys.release())))
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::AbortError));
}

bool IDBObjectStoreBackendImpl::IndexWriter::verifyIndexKeys(IDBBackingStore& backingStore, IDBBackingStore::Transaction* transaction, int64_t databaseId, int64_t objectStoreId, int64_t indexId, bool& canAddKeys, const IDBKey* primaryKey, String* errorMessage) const
{
    canAddKeys = false;
    for (size_t i = 0; i < m_indexKeys.size(); ++i) {
        bool ok = addingKeyAllowed(backingStore, transaction, databaseId, objectStoreId, indexId, (m_indexKeys)[i].get(), primaryKey, canAddKeys);
        if (!ok)
            return false;
        if (!canAddKeys) {
            if (errorMessage)
                *errorMessage = String::format("Unable to add key to index '%s': at least one key does not satisfy the uniqueness requirements.", m_indexMetadata.name.utf8().data());
            return true;
        }
    }
    canAddKeys = true;
    return true;
}

void IDBObjectStoreBackendImpl::IndexWriter::writeIndexKeys(const IDBBackingStore::RecordIdentifier& recordIdentifier, IDBBackingStore& backingStore, IDBBackingStore::Transaction* transaction, int64_t databaseId, int64_t objectStoreId) const
{
    int64_t indexId = m_indexMetadata.id;
    for (size_t i = 0; i < m_indexKeys.size(); ++i) {
        backingStore.putIndexDataForRecord(transaction, databaseId, objectStoreId, indexId, *(m_indexKeys)[i].get(), recordIdentifier);
    }
}

bool IDBObjectStoreBackendImpl::IndexWriter::addingKeyAllowed(IDBBackingStore& backingStore, IDBBackingStore::Transaction* transaction, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey* indexKey, const IDBKey* primaryKey, bool& allowed) const
{
    allowed = false;
    if (!m_indexMetadata.unique) {
        allowed = true;
        return true;
    }

    RefPtr<IDBKey> foundPrimaryKey;
    bool found = false;
    bool ok = backingStore.keyExistsInIndex(transaction, databaseId, objectStoreId, indexId, *indexKey, foundPrimaryKey, found);
    if (!ok)
        return false;
    if (!found || (primaryKey && foundPrimaryKey->isEqual(primaryKey)))
        allowed = true;
    return true;
}

bool IDBObjectStoreBackendImpl::makeIndexWriters(PassRefPtr<IDBTransactionBackendImpl> transaction, IDBBackingStore* backingStore, int64_t databaseId, const IDBObjectStoreMetadata& objectStore, PassRefPtr<IDBKey> primaryKey, bool keyWasGenerated, const Vector<int64_t>& indexIds, const Vector<IDBObjectStoreBackendInterface::IndexKeys>& indexKeys, Vector<OwnPtr<IndexWriter> >* indexWriters, String* errorMessage, bool& completed)
{
    ASSERT(indexIds.size() == indexKeys.size());
    completed = false;

    HashMap<int64_t, IDBObjectStoreBackendInterface::IndexKeys> indexKeyMap;
    for (size_t i = 0; i < indexIds.size(); ++i)
        indexKeyMap.add(indexIds[i], indexKeys[i]);

    for (IDBObjectStoreMetadata::IndexMap::const_iterator it = objectStore.indexes.begin(); it != objectStore.indexes.end(); ++it) {

        const IDBIndexMetadata& index = it->value;

        IDBObjectStoreBackendInterface::IndexKeys keys = indexKeyMap.get(it->key);
        // If the objectStore is using autoIncrement, then any indexes with an identical keyPath need to also use the primary (generated) key as a key.
        if (keyWasGenerated && (index.keyPath == objectStore.keyPath))
            keys.append(primaryKey);

        OwnPtr<IndexWriter> indexWriter(adoptPtr(new IndexWriter(index, keys)));
        bool canAddKeys = false;
        bool backingStoreSuccess = indexWriter->verifyIndexKeys(*backingStore, transaction->backingStoreTransaction(), databaseId, objectStore.id, index.id, canAddKeys, primaryKey.get(), errorMessage);
        if (!backingStoreSuccess)
            return false;
        if (!canAddKeys)
            return true;

        indexWriters->append(indexWriter.release());
    }

    completed = true;
    return true;
}

void IDBObjectStoreBackendImpl::setIndexKeys(PassRefPtr<IDBKey> prpPrimaryKey, const Vector<int64_t>& indexIds, const Vector<IndexKeys>& indexKeys, IDBTransactionBackendInterface* transactionPtr)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::setIndexKeys");
    RefPtr<IDBKey> primaryKey = prpPrimaryKey;
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    if (transaction->isFinished())
        return;

    // FIXME: This method could be asynchronous, but we need to evaluate if it's worth the extra complexity.
    IDBBackingStore::RecordIdentifier recordIdentifier;
    bool found = false;
    bool ok = backingStore()->keyExistsInObjectStore(transaction->backingStoreTransaction(), databaseId(), id(), *primaryKey, &recordIdentifier, found);
    if (!ok) {
        LOG_ERROR("keyExistsInObjectStore reported an error");
        transaction->abort(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error setting index keys."));
        return;
    }
    if (!found) {
        RefPtr<IDBDatabaseError> error = IDBDatabaseError::create(IDBDatabaseException::UnknownError, String::format("Internal error setting index keys for object store '%s'.", name().utf8().data()));
        transaction->abort(error.release());
        return;
    }

    Vector<OwnPtr<IndexWriter> > indexWriters;
    String errorMessage;
    bool obeysConstraints = false;
    RefPtr<IDBBackingStore> store = backingStore();
    bool backingStoreSuccess = makeIndexWriters(transaction, store.get(), databaseId(), metadata(), primaryKey, false, indexIds, indexKeys, &indexWriters, &errorMessage, obeysConstraints);
    if (!backingStoreSuccess) {
        transaction->abort(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error: backing store error updating index keys."));
        return;
    }
    if (!obeysConstraints) {
        transaction->abort(IDBDatabaseError::create(IDBDatabaseException::ConstraintError, errorMessage));
        return;
    }

    for (size_t i = 0; i < indexWriters.size(); ++i) {
        IndexWriter* indexWriter = indexWriters[i].get();
        indexWriter->writeIndexKeys(recordIdentifier, *backingStore(), transaction->backingStoreTransaction(), databaseId(), m_metadata.id);
    }
}

void IDBObjectStoreBackendImpl::setIndexesReady(const Vector<int64_t>& indexIds, IDBTransactionBackendInterface* transactionInterface)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::setIndexesReady");

    OwnPtr<Vector<int64_t> > newIndexIds = adoptPtr(new Vector<int64_t>(indexIds));
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionInterface);
    if (transaction->isFinished())
        return;

    if (!transaction->scheduleTask(IDBTransactionBackendInterface::PreemptiveTask, ObjectStoreIndexesReadyOperation::create(this, newIndexIds.release())))
        ASSERT_NOT_REACHED();
}

void IDBObjectStoreBackendImpl::ObjectStoreIndexesReadyOperation::perform(IDBTransactionBackendImpl* transaction)
{
    IDB_TRACE("ObjectStoreIndexesReadyOperation");
    for (size_t i = 0; i < m_popIndexIds->size(); ++i)
        transaction->didCompletePreemptiveEvent();
}

void IDBObjectStoreBackendImpl::ObjectStoreStorageOperation::perform(IDBTransactionBackendImpl* transaction)
{
    IDB_TRACE("ObjectStoreStorageOperation");
    ASSERT(transaction->mode() != IDBTransaction::READ_ONLY);
    ASSERT(m_popIndexIds && m_popIndexKeys && m_popIndexIds->size() == m_popIndexKeys->size());
    const bool autoIncrement = m_objectStore->autoIncrement();
    bool keyWasGenerated = false;

    if (m_putMode != CursorUpdate && autoIncrement && !m_key) {
        RefPtr<IDBKey> autoIncKey = m_objectStore->generateKey(transaction);
        keyWasGenerated = true;
        if (!autoIncKey->isValid()) {
            m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::ConstraintError, "Maximum key generator value reached."));
            return;
        }
        m_key = autoIncKey;
    }

    ASSERT(m_key && m_key->isValid());

    IDBObjectStoreMetadata objectStoreMetadata = m_objectStore->metadata();
    IDBBackingStore::RecordIdentifier recordIdentifier;
    RefPtr<IDBBackingStore> backingStore = m_objectStore->backingStore();
    if (m_putMode == AddOnly) {
        bool found = false;
        bool ok = backingStore->keyExistsInObjectStore(transaction->backingStoreTransaction(), m_objectStore->databaseId(), m_objectStore->id(), *m_key, &recordIdentifier, found);
        if (!ok) {
            m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error checking key existence."));
            return;
        }
        if (found) {
            m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::ConstraintError, "Key already exists in the object store."));
            return;
        }
    }

    Vector<OwnPtr<IndexWriter> > indexWriters;
    String errorMessage;
    bool obeysConstraints = false;
    bool backingStoreSuccess = makeIndexWriters(transaction, backingStore.get(), m_objectStore->databaseId(), objectStoreMetadata, m_key, keyWasGenerated, *m_popIndexIds, *m_popIndexKeys, &indexWriters, &errorMessage, obeysConstraints);
    if (!backingStoreSuccess) {
        m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error: backing store error updating index keys."));
        return;
    }
    if (!obeysConstraints) {
        m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::ConstraintError, errorMessage));
        return;
    }

    // Before this point, don't do any mutation.  After this point, rollback the transaction in case of error.

    backingStoreSuccess = m_objectStore->backingStore()->putRecord(transaction->backingStoreTransaction(), m_objectStore->databaseId(), m_objectStore->id(), *m_key, m_value->toWireBytes(), &recordIdentifier);
    if (!backingStoreSuccess) {
        m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error: backing store error performing put/add."));
        return;
    }

    for (size_t i = 0; i < indexWriters.size(); ++i) {
        IndexWriter* indexWriter = indexWriters[i].get();
        indexWriter->writeIndexKeys(recordIdentifier, *m_objectStore->backingStore(), transaction->backingStoreTransaction(), m_objectStore->databaseId(), m_objectStore->m_metadata.id);
    }

    if (autoIncrement && m_putMode != CursorUpdate && m_key->type() == IDBKey::NumberType) {
        bool ok = m_objectStore->updateKeyGenerator(transaction, m_key.get(), !keyWasGenerated);
        if (!ok) {
            m_callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UnknownError, "Internal error updating key generator."));
            return;
        }
    }

    m_callbacks->onSuccess(m_key.release());
}

void IDBObjectStoreBackendImpl::deleteFunction(PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode&)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::delete");

    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    ASSERT(transaction->mode() != IDBTransaction::READ_ONLY);
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;

    if (!transaction->scheduleTask(ObjectStoreDeletionOperation::create(this, keyRange, callbacks)))
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::AbortError));
}

void IDBObjectStoreBackendImpl::ObjectStoreDeletionOperation::perform(IDBTransactionBackendImpl* transaction)
{
    IDB_TRACE("ObjectStoreDeletionOperation");

    RefPtr<IDBBackingStore::Cursor> backingStoreCursor = m_objectStore->backingStore()->openObjectStoreCursor(transaction->backingStoreTransaction(), m_objectStore->databaseId(), m_objectStore->id(), m_keyRange.get(), IDBCursor::NEXT);
    if (backingStoreCursor) {

        do {
            m_objectStore->backingStore()->deleteRecord(transaction->backingStoreTransaction(), m_objectStore->databaseId(), m_objectStore->id(), backingStoreCursor->recordIdentifier());

        } while (backingStoreCursor->continueFunction(0));
    }

    m_callbacks->onSuccess();
}

void IDBObjectStoreBackendImpl::clear(PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode&)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::clear");

    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    ASSERT(transaction->mode() != IDBTransaction::READ_ONLY);
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;

    if (!transaction->scheduleTask(ObjectStoreClearOperation::create(this, callbacks)))
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::AbortError));
}

void IDBObjectStoreBackendImpl::ObjectStoreClearOperation::perform(IDBTransactionBackendImpl* transaction)
{
    IDB_TRACE("ObjectStoreClearOperation");
    m_objectStore->backingStore()->clearObjectStore(transaction->backingStoreTransaction(), m_objectStore->databaseId(), m_objectStore->id());
    m_callbacks->onSuccess();
}

PassRefPtr<IDBIndexBackendInterface> IDBObjectStoreBackendImpl::createIndex(int64_t id, const String& name, const IDBKeyPath& keyPath, bool unique, bool multiEntry, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    ASSERT_WITH_MESSAGE(!m_indexes.contains(id), "Indexes already contain '%s'", name.utf8().data());

    RefPtr<IDBIndexBackendImpl> index = IDBIndexBackendImpl::create(m_database, this, IDBIndexMetadata(name, id, keyPath, unique, multiEntry));
    ASSERT(index->name() == name);

    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    ASSERT(transaction->mode() == IDBTransaction::VERSION_CHANGE);
    ASSERT(id > m_metadata.maxIndexId);
    m_metadata.maxIndexId = id;

    if (!transaction->scheduleTask(CreateIndexOperation::create(this, index), CreateIndexAbortOperation::create(this, index))) {
        ec = IDBDatabaseException::TransactionInactiveError;
        return 0;
    }

    m_indexes.set(id, index);
    return index.release();
}

void IDBObjectStoreBackendImpl::CreateIndexOperation::perform(IDBTransactionBackendImpl* transaction)
{
    IDB_TRACE("CreateIndexOperation");
    if (!m_objectStore->backingStore()->createIndex(transaction->backingStoreTransaction(), m_objectStore->databaseId(), m_objectStore->id(), m_index->id(), m_index->name(), m_index->keyPath(), m_index->unique(), m_index->multiEntry())) {
        transaction->abort(IDBDatabaseError::create(IDBDatabaseException::UnknownError, String::format("Internal error when trying to create index '%s'.", m_index->name().utf8().data())));
        return;
    }
}

PassRefPtr<IDBIndexBackendInterface> IDBObjectStoreBackendImpl::index(int64_t indexId)
{
    RefPtr<IDBIndexBackendInterface> index = m_indexes.get(indexId);
    ASSERT(index);
    return index.release();
}

void IDBObjectStoreBackendImpl::deleteIndex(int64_t indexId, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    ASSERT(m_indexes.contains(indexId));

    RefPtr<IDBIndexBackendImpl> index = m_indexes.get(indexId);
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    ASSERT(transaction->mode() == IDBTransaction::VERSION_CHANGE);

    if (!transaction->scheduleTask(DeleteIndexOperation::create(this, index), DeleteIndexAbortOperation::create(this, index))) {
        ec = IDBDatabaseException::TransactionInactiveError;
        return;
    }
    m_indexes.remove(indexId);
}

void IDBObjectStoreBackendImpl::DeleteIndexOperation::perform(IDBTransactionBackendImpl* transaction)
{
    IDB_TRACE("DeleteIndexOperation");
    m_objectStore->backingStore()->deleteIndex(transaction->backingStoreTransaction(), m_objectStore->databaseId(), m_objectStore->id(), m_index->id());
}

void IDBObjectStoreBackendImpl::openCursor(PassRefPtr<IDBKeyRange> range, IDBCursor::Direction direction, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface::TaskType taskType, IDBTransactionBackendInterface* transactionPtr, ExceptionCode&)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::openCursor");
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    if (!transaction->scheduleTask(OpenObjectStoreCursorOperation::create(this, range, direction, callbacks, taskType))) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::AbortError));
    }
}

void IDBObjectStoreBackendImpl::OpenObjectStoreCursorOperation::perform(IDBTransactionBackendImpl* transaction)
{
    IDB_TRACE("OpenObjectStoreCursor");

    RefPtr<IDBBackingStore::Cursor> backingStoreCursor = m_objectStore->backingStore()->openObjectStoreCursor(transaction->backingStoreTransaction(), m_objectStore->databaseId(), m_objectStore->id(), m_range.get(), m_direction);
    // The frontend has begun indexing, so this pauses the transaction
    // until the indexing is complete. This can't happen any earlier
    // because we don't want to switch to early mode in case multiple
    // indexes are being created in a row, with put()'s in between.
    if (m_taskType == IDBTransactionBackendInterface::PreemptiveTask)
        transaction->addPreemptiveEvent();
    if (!backingStoreCursor) {
        m_callbacks->onSuccess(static_cast<SerializedScriptValue*>(0));
        return;
    }

    RefPtr<IDBCursorBackendImpl> cursor = IDBCursorBackendImpl::create(backingStoreCursor.release(), IDBCursorBackendInterface::KeyAndValue, m_taskType, transaction, m_objectStore.get());
    m_callbacks->onSuccess(cursor, cursor->key(), cursor->primaryKey(), cursor->value());
}

void IDBObjectStoreBackendImpl::count(PassRefPtr<IDBKeyRange> range, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode&)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::count");
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    if (!transaction->scheduleTask(ObjectStoreCountOperation::create(this, range, callbacks)))
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::AbortError));
}

void IDBObjectStoreBackendImpl::ObjectStoreCountOperation::perform(IDBTransactionBackendImpl* transaction)
{
    IDB_TRACE("ObjectStoreCountOperation");
    uint32_t count = 0;
    RefPtr<IDBBackingStore::Cursor> backingStoreCursor = m_objectStore->backingStore()->openObjectStoreKeyCursor(transaction->backingStoreTransaction(), m_objectStore->databaseId(), m_objectStore->id(), m_range.get(), IDBCursor::NEXT);
    if (!backingStoreCursor) {
        m_callbacks->onSuccess(count);
        return;
    }

    do {
        ++count;
    } while (backingStoreCursor->continueFunction(0));

    m_callbacks->onSuccess(count);
}

void IDBObjectStoreBackendImpl::loadIndexes()
{
    Vector<IDBIndexMetadata> indexes = backingStore()->getIndexes(databaseId(), m_metadata.id);

    for (size_t i = 0; i < indexes.size(); ++i)
        m_indexes.set(indexes[i].id, IDBIndexBackendImpl::create(m_database, this, indexes[i]));
}

void IDBObjectStoreBackendImpl::CreateIndexAbortOperation::perform(IDBTransactionBackendImpl* transaction)
{
    ASSERT(!transaction);
    ASSERT(m_objectStore->m_indexes.contains(m_index->id()));
    m_objectStore->m_indexes.remove(m_index->id());
}

void IDBObjectStoreBackendImpl::DeleteIndexAbortOperation::perform(IDBTransactionBackendImpl* transaction)
{
    ASSERT(!transaction);
    ASSERT(!m_objectStore->m_indexes.contains(m_index->id()));
    m_objectStore->m_indexes.set(m_index->id(), m_index);
}

PassRefPtr<IDBKey> IDBObjectStoreBackendImpl::generateKey(PassRefPtr<IDBTransactionBackendImpl> transaction)
{
    const int64_t maxGeneratorValue = 9007199254740992LL; // Maximum integer storable as ECMAScript number.
    int64_t currentNumber;
    bool ok = backingStore()->getKeyGeneratorCurrentNumber(transaction->backingStoreTransaction(), databaseId(), id(), currentNumber);
    if (!ok) {
        LOG_ERROR("Failed to getKeyGeneratorCurrentNumber");
        return IDBKey::createInvalid();
    }
    if (currentNumber < 0 || currentNumber > maxGeneratorValue)
        return IDBKey::createInvalid();

    return IDBKey::createNumber(currentNumber);
}

bool IDBObjectStoreBackendImpl::updateKeyGenerator(PassRefPtr<IDBTransactionBackendImpl> transaction, const IDBKey* key, bool checkCurrent)
{
    ASSERT(key && key->type() == IDBKey::NumberType);
    return backingStore()->maybeUpdateKeyGeneratorCurrentNumber(transaction->backingStoreTransaction(), databaseId(), id(), static_cast<int64_t>(floor(key->number())) + 1, checkCurrent);
}

} // namespace WebCore

#endif
