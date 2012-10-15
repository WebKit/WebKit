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

#include "CrossThreadTask.h"
#include "DOMStringList.h"
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
#include "ScriptExecutionContext.h"
#include <wtf/MathExtras.h>

namespace WebCore {

IDBObjectStoreBackendImpl::~IDBObjectStoreBackendImpl()
{
}

IDBObjectStoreBackendImpl::IDBObjectStoreBackendImpl(const IDBDatabaseBackendImpl* database, int64_t id, const String& name, const IDBKeyPath& keyPath, bool autoIncrement, int64_t maxIndexId)
    : m_database(database)
    , m_id(id)
    , m_name(name)
    , m_keyPath(keyPath)
    , m_autoIncrement(autoIncrement)
    , m_maxIndexId(maxIndexId)
{
    loadIndexes();
}

IDBObjectStoreMetadata IDBObjectStoreBackendImpl::metadata() const
{
    IDBObjectStoreMetadata metadata(m_name, m_id, m_keyPath, m_autoIncrement, m_maxIndexId);
    for (IndexMap::const_iterator it = m_indexes.begin(); it != m_indexes.end(); ++it)
        metadata.indexes.set(it->key, it->value->metadata());
    return metadata;
}

void IDBObjectStoreBackendImpl::get(PassRefPtr<IDBKeyRange> prpKeyRange, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode&)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::get");
    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBKeyRange> keyRange = prpKeyRange;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    if (!transaction->scheduleTask(
            createCallbackTask(&IDBObjectStoreBackendImpl::getInternal, objectStore, keyRange, callbacks, transaction)))
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::IDB_ABORT_ERR));
}

void IDBObjectStoreBackendImpl::getInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendImpl> transaction)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::getInternal");
    RefPtr<IDBKey> key;
    if (keyRange->isOnlyKey())
        key = keyRange->lower();
    else {
        RefPtr<IDBBackingStore::Cursor> backingStoreCursor = objectStore->backingStore()->openObjectStoreCursor(transaction->backingStoreTransaction(), objectStore->databaseId(), objectStore->id(), keyRange.get(), IDBCursor::NEXT);
        if (!backingStoreCursor) {
            callbacks->onSuccess(SerializedScriptValue::undefinedValue());
            return;
        }
        key = backingStoreCursor->key();
        backingStoreCursor->close();
    }

    String wireData = objectStore->backingStore()->getObjectStoreRecord(transaction->backingStoreTransaction(), objectStore->databaseId(), objectStore->id(), *key);
    if (wireData.isNull()) {
        callbacks->onSuccess(SerializedScriptValue::undefinedValue());
        return;
    }

    if (objectStore->autoIncrement() && !objectStore->keyPath().isNull()) {
        callbacks->onSuccess(SerializedScriptValue::createFromWire(wireData),
                             key, objectStore->keyPath());
        return;
    }
    callbacks->onSuccess(SerializedScriptValue::createFromWire(wireData));
}

void IDBObjectStoreBackendImpl::putWithIndexKeys(PassRefPtr<SerializedScriptValue> prpValue, PassRefPtr<IDBKey> prpKey, PutMode putMode, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, const Vector<String>& indexNames, const Vector<IndexKeys>& indexKeys, ExceptionCode&)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::putWithIndexKeys");

    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<SerializedScriptValue> value = prpValue;
    RefPtr<IDBKey> key = prpKey;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    ASSERT(transaction->mode() != IDBTransaction::READ_ONLY);

    OwnPtr<Vector<String> > newIndexNames = adoptPtr(new Vector<String>(indexNames));
    OwnPtr<Vector<IndexKeys> > newIndexKeys = adoptPtr(new Vector<IndexKeys>(indexKeys));

    ASSERT(autoIncrement() || key);

    if (!transaction->scheduleTask(
            createCallbackTask(&IDBObjectStoreBackendImpl::putInternal, objectStore, value, key, putMode, callbacks, transaction, newIndexNames.release(), newIndexKeys.release())))
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::IDB_ABORT_ERR));
}

namespace {
class IndexWriter {
public:
    explicit IndexWriter(const IDBIndexMetadata& indexMetadata)
        : m_indexMetadata(indexMetadata)
    { }

    IndexWriter(const IDBIndexMetadata& indexMetadata,
                const IDBObjectStoreBackendInterface::IndexKeys& indexKeys)
        : m_indexMetadata(indexMetadata)
        , m_indexKeys(indexKeys)
    { }

    bool verifyIndexKeys(IDBBackingStore& backingStore, IDBBackingStore::Transaction* transaction,
                         int64_t databaseId, int64_t objectStoreId, int64_t indexId,
                         const IDBKey* primaryKey = 0, String* errorMessage = 0)
    {
        for (size_t i = 0; i < m_indexKeys.size(); ++i) {
            if (!addingKeyAllowed(backingStore, transaction, databaseId, objectStoreId, indexId,
                                  (m_indexKeys)[i].get(), primaryKey)) {
                if (errorMessage)
                    *errorMessage = String::format("Unable to add key to index '%s': at least one key does not satisfy the uniqueness requirements.",
                                                   m_indexMetadata.name.utf8().data());
                return false;
            }
        }
        return true;
    }

    bool writeIndexKeys(const IDBBackingStore::ObjectStoreRecordIdentifier* recordIdentifier, IDBBackingStore& backingStore, IDBBackingStore::Transaction* transaction, int64_t databaseId, int64_t objectStoreId, int64_t indexId) const
    {
        for (size_t i = 0; i < m_indexKeys.size(); ++i) {
            if (!backingStore.deleteIndexDataForRecord(transaction, databaseId, objectStoreId, indexId, recordIdentifier))
                return false;
            if (!backingStore.putIndexDataForRecord(transaction, databaseId, objectStoreId, indexId, *(m_indexKeys)[i].get(), recordIdentifier))
                return false;
        }
        return true;
    }

    const String& indexName() const { return m_indexMetadata.name; }

private:

    bool addingKeyAllowed(IDBBackingStore& backingStore, IDBBackingStore::Transaction* transaction,
                          int64_t databaseId, int64_t objectStoreId, int64_t indexId,
                          const IDBKey* indexKey, const IDBKey* primaryKey) const
    {
        if (!m_indexMetadata.unique)
            return true;

        RefPtr<IDBKey> foundPrimaryKey;
        bool found = backingStore.keyExistsInIndex(transaction, databaseId, objectStoreId, indexId, *indexKey, foundPrimaryKey);
        if (!found)
            return true;
        if (primaryKey && foundPrimaryKey->isEqual(primaryKey))
            return true;
        return false;
    }

    const IDBIndexMetadata m_indexMetadata;
    IDBObjectStoreBackendInterface::IndexKeys m_indexKeys;
};
}

static bool makeIndexWriters(PassRefPtr<IDBTransactionBackendImpl> transaction, IDBObjectStoreBackendImpl* objectStore, PassRefPtr<IDBKey> primaryKey, bool keyWasGenerated, const Vector<String> indexNames, const Vector<IDBObjectStoreBackendInterface::IndexKeys>& indexKeys, Vector<OwnPtr<IndexWriter> >* indexWriters, String* errorMessage)
{
    ASSERT(indexNames.size() == indexKeys.size());

    HashMap<String, IDBObjectStoreBackendInterface::IndexKeys> indexKeyMap;
    for (size_t i = 0; i < indexNames.size(); i++)
        indexKeyMap.add(indexNames[i], indexKeys[i]);

    for (IDBObjectStoreBackendImpl::IndexMap::iterator it = objectStore->iterIndexesBegin(); it != objectStore->iterIndexesEnd(); ++it) {

        const RefPtr<IDBIndexBackendImpl>& index = it->value;
        if (!index->hasValidId())
            continue; // The index object has been created, but does not exist in the database yet.

        IDBObjectStoreBackendInterface::IndexKeys keys = indexKeyMap.get(it->key);
        // If the objectStore is using autoIncrement, then any indexes with an identical keyPath need to also use the primary (generated) key as a key.
        if (keyWasGenerated) {
            const IDBKeyPath& indexKeyPath = index->keyPath();
            if (indexKeyPath == objectStore->keyPath())
                keys.append(primaryKey);
        }

        OwnPtr<IndexWriter> indexWriter(adoptPtr(new IndexWriter(index->metadata(), keys)));
        if (!indexWriter->verifyIndexKeys(*objectStore->backingStore(),
                                          transaction->backingStoreTransaction(),
                                          objectStore->databaseId(),
                                          objectStore->id(),
                                          index->id(), primaryKey.get(), errorMessage)) {
            return false;
        }

        indexWriters->append(indexWriter.release());
    }

    return true;
}

void IDBObjectStoreBackendImpl::setIndexKeys(PassRefPtr<IDBKey> prpPrimaryKey, const Vector<String>& indexNames, const Vector<IndexKeys>& indexKeys, IDBTransactionBackendInterface* transactionPtr)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::setIndexKeys");
    RefPtr<IDBKey> primaryKey = prpPrimaryKey;
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);

    // FIXME: This method could be asynchronous, but we need to evaluate if it's worth the extra complexity.
    RefPtr<IDBBackingStore::ObjectStoreRecordIdentifier> recordIdentifier = backingStore()->createInvalidRecordIdentifier();
    if (!backingStore()->keyExistsInObjectStore(transaction->backingStoreTransaction(), databaseId(), id(), *primaryKey, recordIdentifier.get())) {
        transaction->abort();
        return;
    }

    Vector<OwnPtr<IndexWriter> > indexWriters;
    String errorMessage;
    if (!makeIndexWriters(transaction, this, primaryKey, false, indexNames, indexKeys, &indexWriters, &errorMessage)) {
        // FIXME: Need to deal with errorMessage here. makeIndexWriters only fails on uniqueness constraint errors.
        transaction->abort(IDBDatabaseError::create(IDBDatabaseException::CONSTRAINT_ERR, "Duplicate index keys exist in the object store."));
        return;
    }

    for (size_t i = 0; i < indexWriters.size(); ++i) {
        IndexWriter* indexWriter = indexWriters[i].get();
        if (!indexWriter->writeIndexKeys(recordIdentifier.get(),
                                         *backingStore(),
                                         transaction->backingStoreTransaction(),
                                         databaseId(),
                                         m_id,
                                         m_indexes.get(indexWriter->indexName())->id())) {
            transaction->abort();
            return;
        }
    }
}

void IDBObjectStoreBackendImpl::setIndexesReady(const Vector<String>& indexNames, IDBTransactionBackendInterface* transactionInterface)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::setIndexesReady");
    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;

    OwnPtr<Vector<String> > names = adoptPtr(new Vector<String>(indexNames));
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionInterface);

    if (!transaction->scheduleTask(
            IDBTransactionBackendInterface::PreemptiveTask,
            createCallbackTask(&IDBObjectStoreBackendImpl::setIndexesReadyInternal, objectStore, names.release(), transaction)))
        ASSERT_NOT_REACHED();
}

void IDBObjectStoreBackendImpl::setIndexesReadyInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassOwnPtr<Vector<String> > popIndexNames, PassRefPtr<IDBTransactionBackendImpl> transaction)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::setIndexesReadyInternal");
    OwnPtr<Vector<String> > indexNames = popIndexNames;
    for (size_t i = 0; i < indexNames->size(); ++i)
        transaction->didCompletePreemptiveEvent();
    transaction->didCompleteTaskEvents();
}

void IDBObjectStoreBackendImpl::putInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<SerializedScriptValue> prpValue, PassRefPtr<IDBKey> prpKey, PutMode putMode, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendImpl> prpTransaction, PassOwnPtr<Vector<String> > popIndexNames, PassOwnPtr<Vector<IndexKeys> > popIndexKeys)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::putInternal");
    RefPtr<IDBTransactionBackendImpl> transaction = prpTransaction;
    ASSERT(transaction->mode() != IDBTransaction::READ_ONLY);
    RefPtr<SerializedScriptValue> value = prpValue;
    RefPtr<IDBKey> key = prpKey;
    OwnPtr<Vector<String> > indexNames = popIndexNames;
    OwnPtr<Vector<IndexKeys> > indexKeys = popIndexKeys;
    ASSERT(indexNames && indexKeys && indexNames->size() == indexKeys->size());
    const bool autoIncrement = objectStore->autoIncrement();
    bool keyWasGenerated = false;

    if (putMode != CursorUpdate && autoIncrement && !key) {
        RefPtr<IDBKey> autoIncKey = objectStore->generateKey(transaction);
        keyWasGenerated = true;
        if (!autoIncKey->isValid()) {
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::CONSTRAINT_ERR, "Maximum key generator value reached."));
            return;
        }
        key = autoIncKey;
    }

    ASSERT(key && key->isValid());

    RefPtr<IDBBackingStore::ObjectStoreRecordIdentifier> recordIdentifier = objectStore->backingStore()->createInvalidRecordIdentifier();
    if (putMode == AddOnly && objectStore->backingStore()->keyExistsInObjectStore(transaction->backingStoreTransaction(), objectStore->databaseId(), objectStore->id(), *key, recordIdentifier.get())) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::CONSTRAINT_ERR, "Key already exists in the object store."));
        return;
    }

    Vector<OwnPtr<IndexWriter> > indexWriters;
    String errorMessage;
    if (!makeIndexWriters(transaction, objectStore.get(), key, keyWasGenerated, *indexNames, *indexKeys, &indexWriters, &errorMessage)) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::CONSTRAINT_ERR, errorMessage));
        return;
    }

    // Before this point, don't do any mutation.  After this point, rollback the transaction in case of error.

    if (!objectStore->backingStore()->putObjectStoreRecord(transaction->backingStoreTransaction(), objectStore->databaseId(), objectStore->id(), *key, value->toWireString(), recordIdentifier.get())) {
        RefPtr<IDBDatabaseError> error = IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Error writing data to stable storage.");
        callbacks->onError(error);
        transaction->abort(error);
        return;
    }

    for (size_t i = 0; i < indexWriters.size(); ++i) {
        IndexWriter* indexWriter = indexWriters[i].get();
        if (!indexWriter->writeIndexKeys(recordIdentifier.get(),
                                         *objectStore->backingStore(),
                                         transaction->backingStoreTransaction(),
                                         objectStore->databaseId(),
                                         objectStore->m_id,
                                         objectStore->m_indexes.get(indexWriter->indexName())->id())) {

            RefPtr<IDBDatabaseError> error = IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Error writing data to stable storage.");
            callbacks->onError(error);
            transaction->abort(error);
            return;
        }
    }

    if (autoIncrement && putMode != CursorUpdate && key->type() == IDBKey::NumberType)
        objectStore->updateKeyGenerator(transaction, key.get(), !keyWasGenerated);

    callbacks->onSuccess(key.release());
}

void IDBObjectStoreBackendImpl::deleteFunction(PassRefPtr<IDBKeyRange> prpKeyRange, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode&)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::delete");

    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    ASSERT(transaction->mode() != IDBTransaction::READ_ONLY);

    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBKeyRange> keyRange = prpKeyRange;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;

    if (!transaction->scheduleTask(
            createCallbackTask(&IDBObjectStoreBackendImpl::deleteInternal, objectStore, keyRange, callbacks, transaction)))
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::IDB_ABORT_ERR));
}

void IDBObjectStoreBackendImpl::deleteInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendImpl> transaction)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::deleteInternal");
    RefPtr<IDBBackingStore::ObjectStoreRecordIdentifier> recordIdentifier;

    RefPtr<IDBBackingStore::Cursor> backingStoreCursor = objectStore->backingStore()->openObjectStoreCursor(transaction->backingStoreTransaction(), objectStore->databaseId(), objectStore->id(), keyRange.get(), IDBCursor::NEXT);
    if (backingStoreCursor) {

        do {
            recordIdentifier = backingStoreCursor->objectStoreRecordIdentifier();

            for (IDBObjectStoreBackendImpl::IndexMap::iterator it = objectStore->m_indexes.begin(); it != objectStore->m_indexes.end(); ++it) {
                if (!it->value->hasValidId())
                    continue; // The index object has been created, but does not exist in the database yet.

                bool success = objectStore->backingStore()->deleteIndexDataForRecord(transaction->backingStoreTransaction(), objectStore->databaseId(), objectStore->id(), it->value->id(), recordIdentifier.get());
                ASSERT_UNUSED(success, success);
            }

            objectStore->backingStore()->deleteObjectStoreRecord(transaction->backingStoreTransaction(), objectStore->databaseId(), objectStore->id(), recordIdentifier.get());

        } while (backingStoreCursor->continueFunction(0));

        backingStoreCursor->close();
    }

    callbacks->onSuccess(SerializedScriptValue::undefinedValue());
}

void IDBObjectStoreBackendImpl::clear(PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode&)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::clear");

    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    ASSERT(transaction->mode() != IDBTransaction::READ_ONLY);

    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;

    if (!transaction->scheduleTask(
            createCallbackTask(&IDBObjectStoreBackendImpl::clearInternal, objectStore, callbacks, transaction)))
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::IDB_ABORT_ERR));
}

void IDBObjectStoreBackendImpl::clearInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendImpl> transaction)
{
    objectStore->backingStore()->clearObjectStore(transaction->backingStoreTransaction(), objectStore->databaseId(), objectStore->id());
    callbacks->onSuccess(SerializedScriptValue::undefinedValue());
}

PassRefPtr<IDBIndexBackendInterface> IDBObjectStoreBackendImpl::createIndex(int64_t id, const String& name, const IDBKeyPath& keyPath, bool unique, bool multiEntry, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    ASSERT_WITH_MESSAGE(!m_indexes.contains(name), "Indexes already contain %s", name.utf8().data());

    RefPtr<IDBIndexBackendImpl> index = IDBIndexBackendImpl::create(m_database, this, id, name, keyPath, unique, multiEntry);
    ASSERT(index->name() == name);

    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    ASSERT(transaction->mode() == IDBTransaction::VERSION_CHANGE);
    ASSERT(id > m_maxIndexId);
    m_maxIndexId = id;

    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    if (!transaction->scheduleTask(
              createCallbackTask(&IDBObjectStoreBackendImpl::createIndexInternal, objectStore, index, transaction),
              createCallbackTask(&IDBObjectStoreBackendImpl::removeIndexFromMap, objectStore, index))) {
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
        return 0;
    }

    m_indexes.set(name, index);
    return index.release();
}

void IDBObjectStoreBackendImpl::createIndexInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index, PassRefPtr<IDBTransactionBackendImpl> transaction)
{
    if (!objectStore->backingStore()->createIndex(transaction->backingStoreTransaction(), objectStore->databaseId(), objectStore->id(), index->id(), index->name(), index->keyPath(), index->unique(), index->multiEntry())) {
        transaction->abort();
        return;
    }

    transaction->didCompleteTaskEvents();
}

PassRefPtr<IDBIndexBackendInterface> IDBObjectStoreBackendImpl::index(const String& name, ExceptionCode& ec)
{
    RefPtr<IDBIndexBackendInterface> index = m_indexes.get(name);
    if (!index) {
        ec = IDBDatabaseException::IDB_NOT_FOUND_ERR;
        return 0;
    }
    return index.release();
}

void IDBObjectStoreBackendImpl::deleteIndex(const String& name, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    ASSERT(m_indexes.contains(name));

    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBIndexBackendImpl> index = m_indexes.get(name);
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    ASSERT(transaction->mode() == IDBTransaction::VERSION_CHANGE);

    if (!transaction->scheduleTask(
              createCallbackTask(&IDBObjectStoreBackendImpl::deleteIndexInternal, objectStore, index, transaction),
              createCallbackTask(&IDBObjectStoreBackendImpl::addIndexToMap, objectStore, index))) {
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
        return;
    }
    m_indexes.remove(name);
}

void IDBObjectStoreBackendImpl::deleteIndexInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index, PassRefPtr<IDBTransactionBackendImpl> transaction)
{
    objectStore->backingStore()->deleteIndex(transaction->backingStoreTransaction(), objectStore->databaseId(), objectStore->id(), index->id());
    transaction->didCompleteTaskEvents();
}

void IDBObjectStoreBackendImpl::openCursor(PassRefPtr<IDBKeyRange> prpRange, IDBCursor::Direction direction, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface::TaskType taskType, IDBTransactionBackendInterface* transactionPtr, ExceptionCode&)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::openCursor");
    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBKeyRange> range = prpRange;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    if (!transaction->scheduleTask(
            createCallbackTask(&IDBObjectStoreBackendImpl::openCursorInternal, objectStore, range, direction, callbacks, taskType, transaction))) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::IDB_ABORT_ERR));
    }
}

void IDBObjectStoreBackendImpl::openCursorInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKeyRange> range, IDBCursor::Direction direction, PassRefPtr<IDBCallbacks> callbacks, IDBTransactionBackendInterface::TaskType taskType, PassRefPtr<IDBTransactionBackendImpl> transaction)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::openCursorInternal");

    RefPtr<IDBBackingStore::Cursor> backingStoreCursor = objectStore->backingStore()->openObjectStoreCursor(transaction->backingStoreTransaction(), objectStore->databaseId(), objectStore->id(), range.get(), direction);
    // The frontend has begun indexing, so this pauses the transaction
    // until the indexing is complete. This can't happen any earlier
    // because we don't want to switch to early mode in case multiple
    // indexes are being created in a row, with put()'s in between.
    if (taskType == IDBTransactionBackendInterface::PreemptiveTask)
        transaction->addPreemptiveEvent();
    if (!backingStoreCursor) {
        callbacks->onSuccess(SerializedScriptValue::nullValue());
        return;
    }

    RefPtr<IDBCursorBackendImpl> cursor = IDBCursorBackendImpl::create(backingStoreCursor.release(), IDBCursorBackendInterface::ObjectStoreCursor, taskType, transaction.get(), objectStore.get());
    callbacks->onSuccess(cursor, cursor->key(), cursor->primaryKey(), cursor->value());
}

void IDBObjectStoreBackendImpl::count(PassRefPtr<IDBKeyRange> range, PassRefPtr<IDBCallbacks> callbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode&)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::count");
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    if (!transaction->scheduleTask(
            createCallbackTask(&IDBObjectStoreBackendImpl::countInternal, this, range, callbacks, transaction)))
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::IDB_ABORT_ERR));
}

void IDBObjectStoreBackendImpl::countInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKeyRange> range, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendImpl> transaction)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::countInternal");
    uint32_t count = 0;
    RefPtr<IDBBackingStore::Cursor> backingStoreCursor = objectStore->backingStore()->openObjectStoreKeyCursor(transaction->backingStoreTransaction(), objectStore->databaseId(), objectStore->id(), range.get(), IDBCursor::NEXT);
    if (!backingStoreCursor) {
        callbacks->onSuccess(SerializedScriptValue::numberValue(count));
        return;
    }

    do {
        ++count;
    } while (backingStoreCursor->continueFunction(0));

    backingStoreCursor->close();
    callbacks->onSuccess(SerializedScriptValue::numberValue(count));
}

void IDBObjectStoreBackendImpl::loadIndexes()
{
    Vector<int64_t> ids;
    Vector<String> names;
    Vector<IDBKeyPath> keyPaths;
    Vector<bool> uniqueFlags;
    Vector<bool> multiEntryFlags;
    backingStore()->getIndexes(databaseId(), m_id, ids, names, keyPaths, uniqueFlags, multiEntryFlags);

    ASSERT(names.size() == ids.size());
    ASSERT(keyPaths.size() == ids.size());
    ASSERT(uniqueFlags.size() == ids.size());
    ASSERT(multiEntryFlags.size() == ids.size());

    for (size_t i = 0; i < ids.size(); ++i)
        m_indexes.set(names[i], IDBIndexBackendImpl::create(m_database, this, ids[i], names[i], keyPaths[i], uniqueFlags[i], multiEntryFlags[i]));
}

void IDBObjectStoreBackendImpl::removeIndexFromMap(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index)
{
    ASSERT(objectStore->m_indexes.contains(index->name()));
    objectStore->m_indexes.remove(index->name());
}

void IDBObjectStoreBackendImpl::addIndexToMap(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index)
{
    RefPtr<IDBIndexBackendImpl> indexPtr = index;
    ASSERT(!objectStore->m_indexes.contains(indexPtr->name()));
    objectStore->m_indexes.set(indexPtr->name(), indexPtr);
}

PassRefPtr<IDBKey> IDBObjectStoreBackendImpl::generateKey(PassRefPtr<IDBTransactionBackendImpl> transaction)
{
    const int64_t maxGeneratorValue = 9007199254740992LL; // Maximum integer storable as ECMAScript number.
    int64_t currentNumber = backingStore()->getKeyGeneratorCurrentNumber(transaction->backingStoreTransaction(), databaseId(), id());
    if (currentNumber < 0 || currentNumber > maxGeneratorValue)
        return IDBKey::createInvalid();

    return IDBKey::createNumber(currentNumber);
}

void IDBObjectStoreBackendImpl::updateKeyGenerator(PassRefPtr<IDBTransactionBackendImpl> transaction, const IDBKey* key, bool checkCurrent)
{
    ASSERT(key && key->type() == IDBKey::NumberType);
    backingStore()->maybeUpdateKeyGeneratorCurrentNumber(transaction->backingStoreTransaction(), databaseId(), id(), static_cast<int64_t>(floor(key->number())) + 1, checkCurrent);
}


} // namespace WebCore

#endif
