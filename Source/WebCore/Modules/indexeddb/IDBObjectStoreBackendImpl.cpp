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
    , m_autoIncrementNumber(-1)
{
    loadIndexes();
}

IDBObjectStoreBackendImpl::IDBObjectStoreBackendImpl(const IDBDatabaseBackendImpl* database, const String& name, const IDBKeyPath& keyPath, bool autoIncrement)
    : m_database(database)
    , m_id(InvalidId)
    , m_name(name)
    , m_keyPath(keyPath)
    , m_autoIncrement(autoIncrement)
    , m_autoIncrementNumber(-1)
{
}

IDBObjectStoreMetadata IDBObjectStoreBackendImpl::metadata() const
{
    IDBObjectStoreMetadata metadata(m_name, m_keyPath, m_autoIncrement);
    for (IndexMap::const_iterator it = m_indexes.begin(); it != m_indexes.end(); ++it)
        metadata.indexes.set(it->first, it->second->metadata());
    return metadata;
}

void IDBObjectStoreBackendImpl::get(PassRefPtr<IDBKeyRange> prpKeyRange, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transaction, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::get");
    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBKeyRange> keyRange = prpKeyRange;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    if (!transaction->scheduleTask(createCallbackTask(&IDBObjectStoreBackendImpl::getInternal, objectStore, keyRange, callbacks)))
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

static PassRefPtr<IDBKey> fetchKeyFromKeyPath(SerializedScriptValue* value, const IDBKeyPath& keyPath)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::fetchKeyFromKeyPath");
    ASSERT(!keyPath.isNull());

    Vector<RefPtr<SerializedScriptValue> > values;
    values.append(value);
    Vector<RefPtr<IDBKey> > keys;
    IDBKeyPathBackendImpl::createIDBKeysFromSerializedValuesAndKeyPath(values, keyPath, keys);
    if (keys.isEmpty())
        return 0;
    ASSERT(keys.size() == 1);
    return keys[0].release();
}

static PassRefPtr<SerializedScriptValue> injectKeyIntoKeyPath(PassRefPtr<IDBKey> key, PassRefPtr<SerializedScriptValue> value, const IDBKeyPath& keyPath)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::injectKeyIntoKeyPath");
    return IDBKeyPathBackendImpl::injectIDBKeyIntoSerializedValue(key, value, keyPath);
}

void IDBObjectStoreBackendImpl::put(PassRefPtr<SerializedScriptValue> prpValue, PassRefPtr<IDBKey> prpKey, PutMode putMode, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::put");

    ASSERT(transactionPtr->mode() != IDBTransaction::READ_ONLY);

    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<SerializedScriptValue> value = prpValue;
    RefPtr<IDBKey> key = prpKey;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendInterface> transaction = transactionPtr;

    if (!transaction->scheduleTask(
            createCallbackTask(&IDBObjectStoreBackendImpl::putInternal, objectStore, value, key, putMode, callbacks, transaction),
            // FIXME: One of these per put() is overkill, since it's simply a cache invalidation.
            createCallbackTask(&IDBObjectStoreBackendImpl::revertAutoIncrementKeyCache, objectStore)))
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
}

void IDBObjectStoreBackendImpl::revertAutoIncrementKeyCache(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore)
{
    objectStore->resetAutoIncrementKeyCache();
}

namespace {
class IndexWriter {
public:
    explicit IndexWriter(PassRefPtr<IDBIndexBackendImpl> index)
        : m_index(index)
    {
    }

    bool loadIndexKeysForValue(SerializedScriptValue* objectValue, const IDBKey* primaryKey = 0, String* errorMessage = 0)
    {
        m_indexKeys.clear();

        RefPtr<IDBKey> indexKey = fetchKeyFromKeyPath(objectValue, m_index->keyPath());

        if (!indexKey)
            return true;

        if (!m_index->multiEntry() || indexKey->type() != IDBKey::ArrayType) {
            if (!indexKey->isValid())
                return true;

            if (!m_index->addingKeyAllowed(indexKey.get(), primaryKey)) {
                if (errorMessage)
                    *errorMessage = String::format("Unable to add key to index '%s': the key does not satisfy its uniqueness requirements.",
                                                   m_index->name().utf8().data());
                return false;
            }
            m_indexKeys.append(indexKey);
        } else {
            ASSERT(m_index->multiEntry());
            ASSERT(indexKey->type() == IDBKey::ArrayType);
            indexKey = IDBKey::createMultiEntryArray(indexKey->array());

            for (size_t i = 0; i < indexKey->array().size(); ++i) {
                if (!m_index->addingKeyAllowed(indexKey->array()[i].get(), primaryKey)) {
                    if (errorMessage)
                        *errorMessage = String::format("Unable to add key to index '%s': at least one key does not satisfy the uniqueness requirements.",
                                                       m_index->name().utf8().data());
                    return false;
                }
                m_indexKeys.append(indexKey->array()[i]);
            }
        }
        return true;
    }

    bool writeIndexKeys(const IDBBackingStore::ObjectStoreRecordIdentifier* recordIdentifier, IDBBackingStore& backingStore, int64_t databaseId, int64_t objectStoreId, String* errorMessage = 0)
    {
        for (size_t i = 0; i < m_indexKeys.size(); ++i) {
            if (!backingStore.deleteIndexDataForRecord(databaseId, objectStoreId, m_index->id(), recordIdentifier))
                return false;
            if (!backingStore.putIndexDataForRecord(databaseId, objectStoreId, m_index->id(), *m_indexKeys[i].get(), recordIdentifier))
                return false;
        }
        return true;
    }

private:
    RefPtr<IDBIndexBackendImpl> m_index;
    Vector<RefPtr<IDBKey> > m_indexKeys;
};
}

void IDBObjectStoreBackendImpl::putInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<SerializedScriptValue> prpValue, PassRefPtr<IDBKey> prpKey, PutMode putMode, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendInterface> transaction)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::putInternal");
    ASSERT(transaction->mode() != IDBTransaction::READ_ONLY);
    RefPtr<SerializedScriptValue> value = prpValue;
    RefPtr<IDBKey> key = prpKey;

    if (putMode != CursorUpdate) {
        const bool autoIncrement = objectStore->autoIncrement();
        const bool hasKeyPath = !objectStore->m_keyPath.isNull();
        if (hasKeyPath) {
            ASSERT(!key);
            RefPtr<IDBKey> keyPathKey = fetchKeyFromKeyPath(value.get(), objectStore->m_keyPath);
            if (keyPathKey)
                key = keyPathKey;
        }
        if (autoIncrement) {
            if (!key) {
                RefPtr<IDBKey> autoIncKey = objectStore->genAutoIncrementKey();
                if (!autoIncKey->isValid()) {
                    callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::DATA_ERR, "Maximum key generator value reached."));
                    return;
                }
                if (hasKeyPath) {
                    RefPtr<SerializedScriptValue> valueAfterInjection = injectKeyIntoKeyPath(autoIncKey, value, objectStore->m_keyPath);
                    ASSERT(valueAfterInjection);
                    if (!valueAfterInjection) {
                        objectStore->resetAutoIncrementKeyCache();
                        // Checks in put() ensure this should only happen if I/O error occurs.
                        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Internal error inserting generated key into the object."));
                        return;
                    }
                    value = valueAfterInjection;
                }
                key = autoIncKey;
            } else {
                // FIXME: Logic to update generator state should go here. Currently it does a scan.
                objectStore->resetAutoIncrementKeyCache();
            }
        }
    }

    ASSERT(key && key->isValid());

    RefPtr<IDBBackingStore::ObjectStoreRecordIdentifier> recordIdentifier = objectStore->backingStore()->createInvalidRecordIdentifier();
    if (putMode == AddOnly && objectStore->backingStore()->keyExistsInObjectStore(objectStore->databaseId(), objectStore->id(), *key, recordIdentifier.get())) {
        objectStore->resetAutoIncrementKeyCache();
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::CONSTRAINT_ERR, "Key already exists in the object store."));
        return;
    }

    Vector<OwnPtr<IndexWriter> > indexWriters;
    for (IndexMap::iterator it = objectStore->m_indexes.begin(); it != objectStore->m_indexes.end(); ++it) {

        const RefPtr<IDBIndexBackendImpl>& index = it->second;
        if (!index->hasValidId())
            continue; // The index object has been created, but does not exist in the database yet.

        OwnPtr<IndexWriter> indexWriter(adoptPtr(new IndexWriter(index)));
        String errorMessage;
        if (!indexWriter->loadIndexKeysForValue(value.get(), key.get(), &errorMessage)) {
            objectStore->resetAutoIncrementKeyCache();
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
        if (!indexWriters[i]->writeIndexKeys(recordIdentifier.get(),
                                             *objectStore->backingStore(), objectStore->databaseId(),
                                             objectStore->m_id)) {
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Error writing data to stable storage."));
            transaction->abort();
            return;
        }
    }

    callbacks->onSuccess(key.get());
}

void IDBObjectStoreBackendImpl::deleteFunction(PassRefPtr<IDBKeyRange> prpKeyRange, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transaction, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::delete");

    ASSERT(transaction->mode() != IDBTransaction::READ_ONLY);

    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBKeyRange> keyRange = prpKeyRange;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;

    if (!transaction->scheduleTask(createCallbackTask(&IDBObjectStoreBackendImpl::deleteInternal, objectStore, keyRange, callbacks)))
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

    ASSERT(transaction->mode() != IDBTransaction::READ_ONLY);

    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;

    if (!transaction->scheduleTask(createCallbackTask(&IDBObjectStoreBackendImpl::clearInternal, objectStore, callbacks)))
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
    PopulateIndexCallback(IDBBackingStore& backingStore, int64_t databaseId, int64_t objectStoreId, PassRefPtr<IDBIndexBackendImpl> index)
        : m_backingStore(backingStore)
        , m_databaseId(databaseId)
        , m_objectStoreId(objectStoreId)
        , m_writer(index)
    {
    }

    virtual bool callback(const IDBBackingStore::ObjectStoreRecordIdentifier* recordIdentifier, const String& value)
    {
        RefPtr<SerializedScriptValue> objectValue = SerializedScriptValue::createFromWire(value);
        if (!m_writer.loadIndexKeysForValue(objectValue.get()))
            return false;
        return m_writer.writeIndexKeys(recordIdentifier, m_backingStore, m_databaseId, m_objectStoreId);
    }

private:
    IDBBackingStore& m_backingStore;
    int64_t m_databaseId;
    int64_t m_objectStoreId;
    IndexWriter m_writer;
};
}

bool IDBObjectStoreBackendImpl::populateIndex(IDBBackingStore& backingStore, int64_t databaseId, int64_t objectStoreId, PassRefPtr<IDBIndexBackendImpl> index)
{
    PopulateIndexCallback callback(backingStore, databaseId, objectStoreId, index);
    if (!backingStore.forEachObjectStoreRecord(databaseId, objectStoreId, callback))
        return false;
    return true;
}

PassRefPtr<IDBIndexBackendInterface> IDBObjectStoreBackendImpl::createIndex(const String& name, const IDBKeyPath& keyPath, bool unique, bool multiEntry, IDBTransactionBackendInterface* transaction, ExceptionCode& ec)
{
    ASSERT(transaction->mode() == IDBTransaction::VERSION_CHANGE);
    ASSERT(!m_indexes.contains(name));

    RefPtr<IDBIndexBackendImpl> index = IDBIndexBackendImpl::create(m_database, this, name, keyPath, unique, multiEntry);
    ASSERT(index->name() == name);

    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBTransactionBackendInterface> transactionPtr = transaction;
    if (!transaction->scheduleTask(
              createCallbackTask(&IDBObjectStoreBackendImpl::createIndexInternal,
                                 objectStore, index, transactionPtr),
              createCallbackTask(&IDBObjectStoreBackendImpl::removeIndexFromMap,
                                 objectStore, index))) {
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
        return 0;
    }

    m_indexes.set(name, index);
    return index.release();
}

void IDBObjectStoreBackendImpl::createIndexInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index, PassRefPtr<IDBTransactionBackendInterface> transaction)
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

void IDBObjectStoreBackendImpl::deleteIndex(const String& name, IDBTransactionBackendInterface* transaction, ExceptionCode& ec)
{
    ASSERT(transaction->mode() == IDBTransaction::VERSION_CHANGE);
    ASSERT(m_indexes.contains(name));

    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBIndexBackendImpl> index = m_indexes.get(name);
    RefPtr<IDBTransactionBackendInterface> transactionPtr = transaction;
    if (!transaction->scheduleTask(
              createCallbackTask(&IDBObjectStoreBackendImpl::deleteIndexInternal,
                                 objectStore, index, transactionPtr),
              createCallbackTask(&IDBObjectStoreBackendImpl::addIndexToMap,
                                 objectStore, index))) {
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
        return;
    }
    m_indexes.remove(name);
}

void IDBObjectStoreBackendImpl::deleteIndexInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index, PassRefPtr<IDBTransactionBackendInterface> transaction)
{
    objectStore->backingStore()->deleteIndex(objectStore->databaseId(), objectStore->id(), index->id());
    transaction->didCompleteTaskEvents();
}

void IDBObjectStoreBackendImpl::openCursor(PassRefPtr<IDBKeyRange> prpRange, unsigned short direction, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transaction, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::openCursor");
    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBKeyRange> range = prpRange;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendInterface> transactionPtr = transaction;
    if (!transaction->scheduleTask(
            createCallbackTask(&IDBObjectStoreBackendImpl::openCursorInternal,
                               objectStore, range, direction, callbacks, transactionPtr))) {
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
    }
}

void IDBObjectStoreBackendImpl::openCursorInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKeyRange> range, unsigned short tmpDirection, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendInterface> transaction)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::openCursorInternal");
    IDBCursor::Direction direction = static_cast<IDBCursor::Direction>(tmpDirection);

    RefPtr<IDBBackingStore::Cursor> backingStoreCursor = objectStore->backingStore()->openObjectStoreCursor(objectStore->databaseId(), objectStore->id(), range.get(), direction);
    if (!backingStoreCursor) {
        callbacks->onSuccess(SerializedScriptValue::nullValue());
        return;
    }

    RefPtr<IDBCursorBackendInterface> cursor = IDBCursorBackendImpl::create(backingStoreCursor.release(), direction, IDBCursorBackendInterface::ObjectStoreCursor, transaction.get(), objectStore.get());
    callbacks->onSuccess(cursor.release());
}

void IDBObjectStoreBackendImpl::count(PassRefPtr<IDBKeyRange> range, PassRefPtr<IDBCallbacks> callbacks, IDBTransactionBackendInterface* transaction, ExceptionCode& ec)
{
    IDB_TRACE("IDBObjectStoreBackendImpl::count");
    if (!transaction->scheduleTask(createCallbackTask(&IDBObjectStoreBackendImpl::countInternal, this, range, callbacks, transaction)))
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
}

void IDBObjectStoreBackendImpl::countInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKeyRange> range, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendInterface>)
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

PassRefPtr<IDBKey> IDBObjectStoreBackendImpl::genAutoIncrementKey()
{
    const int64_t kMaxGeneratorValue = 9007199254740992LL; // Maximum integer storable as ECMAScript number.
    if (m_autoIncrementNumber > kMaxGeneratorValue)
        return IDBKey::createInvalid();
    if (m_autoIncrementNumber > 0)
        return IDBKey::createNumber(m_autoIncrementNumber++);

    m_autoIncrementNumber = backingStore()->nextAutoIncrementNumber(databaseId(), id());
    if (m_autoIncrementNumber > kMaxGeneratorValue)
        return IDBKey::createInvalid();
    return IDBKey::createNumber(m_autoIncrementNumber++);
}


} // namespace WebCore

#endif
