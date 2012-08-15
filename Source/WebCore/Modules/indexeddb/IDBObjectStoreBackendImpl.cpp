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
#include "IDBKeyPathBackendImpl.h"
#include "IDBKeyRange.h"
#include "IDBTracing.h"
#include "IDBTransactionBackendImpl.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

IDBObjectStoreBackendImpl::~IDBObjectStoreBackendImpl()
{
}

IDBObjectStoreBackendImpl::IDBObjectStoreBackendImpl(const IDBDatabaseBackendImpl* database, int64_t id, const String& name, const IDBKeyPath& keyPath, bool autoIncrement)
    : m_database(database)
    , m_id(id)
    , m_name(name)
    , m_keyPath(keyPath)
    , m_autoIncrement(autoIncrement)
{
    loadIndexes();
}

IDBObjectStoreBackendImpl::IDBObjectStoreBackendImpl(const IDBDatabaseBackendImpl* database, const String& name, const IDBKeyPath& keyPath, bool autoIncrement)
    : m_database(database)
    , m_id(InvalidId)
    , m_name(name)
    , m_keyPath(keyPath)
    , m_autoIncrement(autoIncrement)
{
}

IDBObjectStoreMetadata IDBObjectStoreBackendImpl::metadata() const
{
    IDBObjectStoreMetadata metadata(m_name, m_keyPath, m_autoIncrement);
    for (IndexMap::const_iterator it = m_indexes.begin(); it != m_indexes.end(); ++it)
        metadata.indexes.set(it->first, it->second->metadata());
    return metadata;
}

void IDBObjectStoreBackendImpl::get(PassRefPtr<IDBKeyRange> prpKeyRange, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::get");
    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBKeyRange> keyRange = prpKeyRange;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    if (!transaction->scheduleTask(
            createCallbackTask(&IDBObjectStoreBackendImpl::getInternal, objectStore, keyRange, callbacks)))
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
}

void IDBObjectStoreBackendImpl::getInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::getInternal");
    RefPtr<IDBKey> key;
    if (keyRange->isOnlyKey())
        key = keyRange->lower();
    else {
        RefPtr<IDBBackingStore::Cursor> backingStoreCursor = objectStore->backingStore()->openObjectStoreCursor(objectStore->databaseId(), objectStore->id(), keyRange.get(), IDBCursor::NEXT);
        if (!backingStoreCursor) {
            callbacks->onSuccess(SerializedScriptValue::undefinedValue());
            return;
        }
        key = backingStoreCursor->key();
        backingStoreCursor->close();
    }

    String wireData = objectStore->backingStore()->getObjectStoreRecord(objectStore->databaseId(), objectStore->id(), *key);
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

void IDBObjectStoreBackendImpl::putWithIndexKeys(PassRefPtr<SerializedScriptValue> prpValue, PassRefPtr<IDBKey> prpKey, PutMode putMode, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, const Vector<String>& indexNames, const Vector<IndexKeys>& indexKeys, ExceptionCode& ec)
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
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
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

    // FIXME: remove this once createIndex() generates these in the renderer.
    void generateIndexKeysForValue(SerializedScriptValue* objectValue)
    {
        m_indexKeys.clear();

        RefPtr<IDBKey> indexKey = fetchKeyFromKeyPath(objectValue);

        if (!indexKey)
            return;

        if (!m_indexMetadata.multiEntry || indexKey->type() != IDBKey::ArrayType) {
            if (!indexKey->isValid())
                return;

            m_indexKeys.append(indexKey);
        } else {
            ASSERT(m_indexMetadata.multiEntry);
            ASSERT(indexKey->type() == IDBKey::ArrayType);
            indexKey = IDBKey::createMultiEntryArray(indexKey->array());

            for (size_t i = 0; i < indexKey->array().size(); ++i)
                m_indexKeys.append(indexKey->array()[i]);
        }
    }

    bool verifyIndexKeys(IDBBackingStore& backingStore,
                         int64_t databaseId, int64_t objectStoreId, int64_t indexId,
                         const IDBKey* primaryKey = 0, String* errorMessage = 0)
    {
        for (size_t i = 0; i < m_indexKeys.size(); ++i) {
            if (!addingKeyAllowed(backingStore, databaseId, objectStoreId, indexId,
                                  (m_indexKeys)[i].get(), primaryKey)) {
                if (errorMessage)
                    *errorMessage = String::format("Unable to add key to index '%s': at least one key does not satisfy the uniqueness requirements.",
                                                   m_indexMetadata.name.utf8().data());
                return false;
            }
        }
        return true;
    }

    bool writeIndexKeys(const IDBBackingStore::ObjectStoreRecordIdentifier* recordIdentifier, IDBBackingStore& backingStore, int64_t databaseId, int64_t objectStoreId, int64_t indexId) const
    {
        for (size_t i = 0; i < m_indexKeys.size(); ++i) {
            if (!backingStore.deleteIndexDataForRecord(databaseId, objectStoreId, indexId, recordIdentifier))
                return false;
            if (!backingStore.putIndexDataForRecord(databaseId, objectStoreId, indexId, *(m_indexKeys)[i].get(), recordIdentifier))
                return false;
        }
        return true;
    }

    const String& indexName() const { return m_indexMetadata.name; }

private:

    bool addingKeyAllowed(IDBBackingStore& backingStore,
                          int64_t databaseId, int64_t objectStoreId, int64_t indexId,
                          const IDBKey* indexKey, const IDBKey* primaryKey) const
    {
        if (!m_indexMetadata.unique)
            return true;

        RefPtr<IDBKey> foundPrimaryKey;
        bool found = backingStore.keyExistsInIndex(databaseId, objectStoreId, indexId, *indexKey, foundPrimaryKey);
        if (!found)
            return true;
        if (primaryKey && foundPrimaryKey->isEqual(primaryKey))
            return true;
        return false;
    }

    PassRefPtr<IDBKey> fetchKeyFromKeyPath(SerializedScriptValue* value)
    {
        IDB_TRACE("IndexWriter::fetchKeyFromKeyPath");

        Vector<RefPtr<SerializedScriptValue> > values;
        values.append(value);
        Vector<RefPtr<IDBKey> > keys;
        IDBKeyPathBackendImpl::createIDBKeysFromSerializedValuesAndKeyPath(values, m_indexMetadata.keyPath, keys);
        if (keys.isEmpty())
            return 0;
        ASSERT(keys.size() == 1);
        return keys[0].release();
    }

    const IDBIndexMetadata m_indexMetadata;
    IDBObjectStoreBackendInterface::IndexKeys m_indexKeys;
};
}

void IDBObjectStoreBackendImpl::putInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<SerializedScriptValue> prpValue, PassRefPtr<IDBKey> prpKey, PutMode putMode, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendImpl> transaction, PassOwnPtr<Vector<String> > popIndexNames, PassOwnPtr<Vector<IndexKeys> > popIndexKeys)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::putInternal");
    ASSERT(transaction->mode() != IDBTransaction::READ_ONLY);
    RefPtr<SerializedScriptValue> value = prpValue;
    RefPtr<IDBKey> key = prpKey;
    OwnPtr<Vector<String> > indexNames = popIndexNames;
    OwnPtr<Vector<IndexKeys> > indexKeys = popIndexKeys;
    ASSERT(indexNames && indexKeys && indexNames->size() == indexKeys->size());
    const bool autoIncrement = objectStore->autoIncrement();
    bool keyWasGenerated = false;

    if (putMode != CursorUpdate && autoIncrement && !key) {
        RefPtr<IDBKey> autoIncKey = objectStore->generateKey();
        keyWasGenerated = true;
        if (!autoIncKey->isValid()) {
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::DATA_ERR, "Maximum key generator value reached."));
            return;
        }
        key = autoIncKey;
    }

    ASSERT(key && key->isValid());

    RefPtr<IDBBackingStore::ObjectStoreRecordIdentifier> recordIdentifier = objectStore->backingStore()->createInvalidRecordIdentifier();
    if (putMode == AddOnly && objectStore->backingStore()->keyExistsInObjectStore(objectStore->databaseId(), objectStore->id(), *key, recordIdentifier.get())) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::CONSTRAINT_ERR, "Key already exists in the object store."));
        return;
    }

    Vector<OwnPtr<IndexWriter> > indexWriters;
    HashMap<String, IndexKeys> indexKeyMap;
    for (size_t i = 0; i < indexNames->size(); ++i) {
        IndexKeys keys = indexKeys->at(i);

        // If the objectStore is using autoIncrement, then any indexes with an identical keyPath need to also use the primary (generated) key as a key.
        if (keyWasGenerated) {
            const IDBKeyPath& indexKeyPath = objectStore->m_indexes.get(indexNames->at(i))->keyPath();
            if (indexKeyPath == objectStore->keyPath())
                keys.append(key);
        }

        indexKeyMap.add(indexNames->at(i), keys);
    }

    for (IndexMap::iterator it = objectStore->m_indexes.begin(); it != objectStore->m_indexes.end(); ++it) {

        const RefPtr<IDBIndexBackendImpl>& index = it->second;
        if (!index->hasValidId())
            continue; // The index object has been created, but does not exist in the database yet.

        OwnPtr<IndexWriter> indexWriter;
        indexWriter = adoptPtr(new IndexWriter(index->metadata(), indexKeyMap.get(it->first)));

        String errorMessage;
        if (!indexWriter->verifyIndexKeys(*objectStore->backingStore(),
                                          objectStore->databaseId(),
                                          objectStore->id(),
                                          index->id(), key.get(), &errorMessage)) {
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::DATA_ERR, errorMessage));
            return;
        }

        indexWriters.append(indexWriter.release());
    }

    // Before this point, don't do any mutation.  After this point, rollback the transaction in case of error.

    if (!objectStore->backingStore()->putObjectStoreRecord(objectStore->databaseId(), objectStore->id(), *key, value->toWireString(), recordIdentifier.get())) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Error writing data to stable storage."));
        transaction->abort();
        return;
    }

    for (size_t i = 0; i < indexWriters.size(); ++i) {
        IndexWriter* indexWriter = indexWriters[i].get();
        if (!indexWriter->writeIndexKeys(recordIdentifier.get(),
                                         *objectStore->backingStore(),
                                         objectStore->databaseId(),
                                         objectStore->m_id,
                                         objectStore->m_indexes.get(indexWriter->indexName())->id())) {

            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Error writing data to stable storage."));
            transaction->abort();
            return;
        }
    }

    if (autoIncrement && putMode != CursorUpdate && key->type() == IDBKey::NumberType)
        objectStore->updateKeyGenerator(key.get(), !keyWasGenerated);

    callbacks->onSuccess(key.release());
}

void IDBObjectStoreBackendImpl::deleteFunction(PassRefPtr<IDBKeyRange> prpKeyRange, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transaction, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::delete");

    ASSERT(IDBTransactionBackendImpl::from(transaction)->mode() != IDBTransaction::READ_ONLY);

    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBKeyRange> keyRange = prpKeyRange;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;

    if (!IDBTransactionBackendImpl::from(transaction)->scheduleTask(
            createCallbackTask(&IDBObjectStoreBackendImpl::deleteInternal, objectStore, keyRange, callbacks)))
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
}

void IDBObjectStoreBackendImpl::deleteInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKeyRange> keyRange, PassRefPtr<IDBCallbacks> callbacks)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::deleteInternal");
    RefPtr<IDBBackingStore::ObjectStoreRecordIdentifier> recordIdentifier;

    RefPtr<IDBBackingStore::Cursor> backingStoreCursor = objectStore->backingStore()->openObjectStoreCursor(objectStore->databaseId(), objectStore->id(), keyRange.get(), IDBCursor::NEXT);
    if (backingStoreCursor) {

        do {
            recordIdentifier = backingStoreCursor->objectStoreRecordIdentifier();

            for (IndexMap::iterator it = objectStore->m_indexes.begin(); it != objectStore->m_indexes.end(); ++it) {
                if (!it->second->hasValidId())
                    continue; // The index object has been created, but does not exist in the database yet.

                bool success = objectStore->backingStore()->deleteIndexDataForRecord(objectStore->databaseId(), objectStore->id(), it->second->id(), recordIdentifier.get());
                ASSERT_UNUSED(success, success);
            }

            objectStore->backingStore()->deleteObjectStoreRecord(objectStore->databaseId(), objectStore->id(), recordIdentifier.get());

        } while (backingStoreCursor->continueFunction(0));

        backingStoreCursor->close();
    }

    callbacks->onSuccess(SerializedScriptValue::undefinedValue());
}

void IDBObjectStoreBackendImpl::clear(PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transaction, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::clear");

    ASSERT(IDBTransactionBackendImpl::from(transaction)->mode() != IDBTransaction::READ_ONLY);

    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;

    if (!IDBTransactionBackendImpl::from(transaction)->scheduleTask(
            createCallbackTask(&IDBObjectStoreBackendImpl::clearInternal, objectStore, callbacks)))
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
}

void IDBObjectStoreBackendImpl::clearInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBCallbacks> callbacks)
{
    objectStore->backingStore()->clearObjectStore(objectStore->databaseId(), objectStore->id());
    callbacks->onSuccess(SerializedScriptValue::undefinedValue());
}

namespace {
class PopulateIndexCallback : public IDBBackingStore::ObjectStoreRecordCallback {
public:
    PopulateIndexCallback(IDBBackingStore& backingStore, const IDBIndexMetadata& indexMetadata, int64_t databaseId, int64_t objectStoreId, int64_t indexId)
        : m_backingStore(backingStore)
        , m_databaseId(databaseId)
        , m_objectStoreId(objectStoreId)
        , m_indexId(indexId)
        , m_writer(indexMetadata)
    {
    }

    virtual bool callback(const IDBBackingStore::ObjectStoreRecordIdentifier* recordIdentifier, const String& value)
    {
        RefPtr<SerializedScriptValue> objectValue = SerializedScriptValue::createFromWire(value);
        m_writer.generateIndexKeysForValue(objectValue.get());
        if (!m_writer.verifyIndexKeys(m_backingStore, m_databaseId, m_objectStoreId, m_indexId))
            return false;
        return m_writer.writeIndexKeys(recordIdentifier, m_backingStore, m_databaseId, m_objectStoreId, m_indexId);
    }

private:
    IDBBackingStore& m_backingStore;
    int64_t m_databaseId;
    int64_t m_objectStoreId;
    int64_t m_indexId;
    IndexWriter m_writer;
};
}

bool IDBObjectStoreBackendImpl::populateIndex(IDBBackingStore& backingStore, int64_t databaseId, int64_t objectStoreId, PassRefPtr<IDBIndexBackendImpl> index)
{
    PopulateIndexCallback callback(backingStore, index->metadata(), databaseId, objectStoreId, index->id());
    if (!backingStore.forEachObjectStoreRecord(databaseId, objectStoreId, callback))
        return false;
    return true;
}

PassRefPtr<IDBIndexBackendInterface> IDBObjectStoreBackendImpl::createIndex(const String& name, const IDBKeyPath& keyPath, bool unique, bool multiEntry, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    ASSERT(!m_indexes.contains(name));

    RefPtr<IDBIndexBackendImpl> index = IDBIndexBackendImpl::create(m_database, this, name, keyPath, unique, multiEntry);
    ASSERT(index->name() == name);

    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    ASSERT(transaction->mode() == IDBTransaction::VERSION_CHANGE);

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
    int64_t id;
    if (!objectStore->backingStore()->createIndex(objectStore->databaseId(), objectStore->id(), index->name(), index->keyPath(), index->unique(), index->multiEntry(), id)) {
        transaction->abort();
        return;
    }

    index->setId(id);

    if (!populateIndex(*objectStore->backingStore(), objectStore->databaseId(), objectStore->m_id, index)) {
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
    objectStore->backingStore()->deleteIndex(objectStore->databaseId(), objectStore->id(), index->id());
    transaction->didCompleteTaskEvents();
}

void IDBObjectStoreBackendImpl::openCursor(PassRefPtr<IDBKeyRange> prpRange, unsigned short direction, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::openCursor");
    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBKeyRange> range = prpRange;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendImpl> transaction = IDBTransactionBackendImpl::from(transactionPtr);
    if (!transaction->scheduleTask(
            createCallbackTask(&IDBObjectStoreBackendImpl::openCursorInternal, objectStore, range, direction, callbacks, transaction))) {
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
    }
}

void IDBObjectStoreBackendImpl::openCursorInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKeyRange> range, unsigned short tmpDirection, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendImpl> transaction)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::openCursorInternal");
    IDBCursor::Direction direction = static_cast<IDBCursor::Direction>(tmpDirection);

    RefPtr<IDBBackingStore::Cursor> backingStoreCursor = objectStore->backingStore()->openObjectStoreCursor(objectStore->databaseId(), objectStore->id(), range.get(), direction);
    if (!backingStoreCursor) {
        callbacks->onSuccess(SerializedScriptValue::nullValue());
        return;
    }

    RefPtr<IDBCursorBackendImpl> cursor = IDBCursorBackendImpl::create(backingStoreCursor.release(), IDBCursorBackendInterface::ObjectStoreCursor, transaction.get(), objectStore.get());
    callbacks->onSuccess(cursor, cursor->key(), cursor->primaryKey(), cursor->value());
}

void IDBObjectStoreBackendImpl::count(PassRefPtr<IDBKeyRange> range, PassRefPtr<IDBCallbacks> callbacks, IDBTransactionBackendInterface* transaction, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::count");
    if (!IDBTransactionBackendImpl::from(transaction)->scheduleTask(
            createCallbackTask(&IDBObjectStoreBackendImpl::countInternal, this, range, callbacks)))
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
}

void IDBObjectStoreBackendImpl::countInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKeyRange> range, PassRefPtr<IDBCallbacks> callbacks)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::countInternal");
    uint32_t count = 0;
    RefPtr<IDBBackingStore::Cursor> backingStoreCursor = objectStore->backingStore()->openObjectStoreCursor(objectStore->databaseId(), objectStore->id(), range.get(), IDBCursor::NEXT);
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

PassRefPtr<IDBKey> IDBObjectStoreBackendImpl::generateKey()
{
    const int64_t maxGeneratorValue = 9007199254740992LL; // Maximum integer storable as ECMAScript number.
    int64_t currentNumber = backingStore()->getKeyGeneratorCurrentNumber(databaseId(), id());
    if (currentNumber < 0 || currentNumber > maxGeneratorValue)
        return IDBKey::createInvalid();

    return IDBKey::createNumber(currentNumber);
}

void IDBObjectStoreBackendImpl::updateKeyGenerator(const IDBKey* key, bool checkCurrent)
{
    ASSERT(key && key->type() == IDBKey::NumberType);
    backingStore()->maybeUpdateKeyGeneratorCurrentNumber(databaseId(), id(), static_cast<int64_t>(floor(key->number())) + 1, checkCurrent);
}


} // namespace WebCore

#endif
