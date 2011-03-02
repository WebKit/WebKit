/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "IDBTransactionBackendInterface.h"
#include "ScriptExecutionContext.h"
#include "SQLiteDatabase.h"
#include "SQLiteStatement.h"
#include "SQLiteTransaction.h"

namespace WebCore {

IDBObjectStoreBackendImpl::~IDBObjectStoreBackendImpl()
{
}

IDBObjectStoreBackendImpl::IDBObjectStoreBackendImpl(IDBBackingStore* backingStore, int64_t id, const String& name, const String& keyPath, bool autoIncrement)
    : m_backingStore(backingStore)
    , m_id(id)
    , m_name(name)
    , m_keyPath(keyPath)
    , m_autoIncrement(autoIncrement)
    , m_autoIncrementNumber(-1)
{
    loadIndexes();
}

IDBObjectStoreBackendImpl::IDBObjectStoreBackendImpl(IDBBackingStore* backingStore, const String& name, const String& keyPath, bool autoIncrement)
    : m_backingStore(backingStore)
    , m_id(InvalidId)
    , m_name(name)
    , m_keyPath(keyPath)
    , m_autoIncrement(autoIncrement)
    , m_autoIncrementNumber(-1)
{
}

PassRefPtr<DOMStringList> IDBObjectStoreBackendImpl::indexNames() const
{
    RefPtr<DOMStringList> indexNames = DOMStringList::create();
    for (IndexMap::const_iterator it = m_indexes.begin(); it != m_indexes.end(); ++it)
        indexNames->append(it->first);
    return indexNames.release();
}

static String whereClause(IDBKey* key)
{
    return "WHERE objectStoreId = ?  AND  " + key->whereSyntax();
}

static void bindWhereClause(SQLiteStatement& query, int64_t id, IDBKey* key)
{
    query.bindInt64(1, id);
    key->bind(query, 2);
}

void IDBObjectStoreBackendImpl::get(PassRefPtr<IDBKey> prpKey, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transaction, ExceptionCode& ec)
{
    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBKey> key = prpKey;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    if (!transaction->scheduleTask(createCallbackTask(&IDBObjectStoreBackendImpl::getInternal, objectStore, key, callbacks)))
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
}

void IDBObjectStoreBackendImpl::getInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks> callbacks)
{
    String wireData = objectStore->m_backingStore->getObjectStoreRecord(objectStore->id(), *key);
    if (wireData.isNull()) {
        callbacks->onSuccess(SerializedScriptValue::undefinedValue());
        return;
    }

    callbacks->onSuccess(SerializedScriptValue::createFromWire(wireData));
}

static PassRefPtr<IDBKey> fetchKeyFromKeyPath(SerializedScriptValue* value, const String& keyPath)
{
    Vector<RefPtr<SerializedScriptValue> > values;
    values.append(value);
    Vector<RefPtr<IDBKey> > keys;
    IDBKeyPathBackendImpl::createIDBKeysFromSerializedValuesAndKeyPath(values, keyPath, keys);
    if (keys.isEmpty())
        return 0;
    ASSERT(keys.size() == 1);
    return keys[0].release();
}

static PassRefPtr<SerializedScriptValue> injectKeyIntoKeyPath(PassRefPtr<IDBKey> key, PassRefPtr<SerializedScriptValue> value, const String& keyPath)
{
    return IDBKeyPathBackendImpl::injectIDBKeyIntoSerializedValue(key, value, keyPath);
}

void IDBObjectStoreBackendImpl::put(PassRefPtr<SerializedScriptValue> prpValue, PassRefPtr<IDBKey> prpKey, PutMode putMode, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    if (transactionPtr->mode() == IDBTransaction::READ_ONLY) {
        ec = IDBDatabaseException::READ_ONLY_ERR;
        return;
    }

    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<SerializedScriptValue> value = prpValue;
    RefPtr<IDBKey> key = prpKey;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendInterface> transaction = transactionPtr;
    // FIXME: This should throw a SERIAL_ERR on structured clone problems.
    // FIXME: This should throw a DATA_ERR when the wrong key/keyPath data is supplied.
    if (!transaction->scheduleTask(createCallbackTask(&IDBObjectStoreBackendImpl::putInternal, objectStore, value, key, putMode, callbacks, transaction)))
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
}

PassRefPtr<IDBKey> IDBObjectStoreBackendImpl::selectKeyForPut(IDBObjectStoreBackendImpl* objectStore, IDBKey* key, PutMode putMode, IDBCallbacks* callbacks, RefPtr<SerializedScriptValue>& value)
{
    if (putMode == CursorUpdate)
        ASSERT(key);

    const bool autoIncrement = objectStore->autoIncrement();
    const bool hasKeyPath = !objectStore->m_keyPath.isNull();

    if (hasKeyPath && key && putMode != CursorUpdate) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::DATA_ERR, "A key was supplied for an objectStore that has a keyPath."));
        return 0;
    }

    if (autoIncrement && key) {
        objectStore->resetAutoIncrementKeyCache();
        return key;
    }

    if (autoIncrement) {
        ASSERT(!key);
        if (!hasKeyPath)
            return objectStore->genAutoIncrementKey();

        RefPtr<IDBKey> keyPathKey = fetchKeyFromKeyPath(value.get(), objectStore->m_keyPath);
        if (keyPathKey) {
            objectStore->resetAutoIncrementKeyCache();
            return keyPathKey;
        }

        RefPtr<IDBKey> autoIncKey = objectStore->genAutoIncrementKey();
        RefPtr<SerializedScriptValue> valueAfterInjection = injectKeyIntoKeyPath(autoIncKey, value, objectStore->m_keyPath);
        if (!valueAfterInjection) {
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::DATA_ERR, "The generated key could not be inserted into the object using the keyPath."));
            return 0;
        }
        value = valueAfterInjection;
        return autoIncKey.release();
    }

    if (hasKeyPath) {
        RefPtr<IDBKey> keyPathKey = fetchKeyFromKeyPath(value.get(), objectStore->m_keyPath);

        if (!keyPathKey) {
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::DATA_ERR, "The key could not be fetched from the keyPath."));
            return 0;
        }

        if (putMode == CursorUpdate && !keyPathKey->isEqual(key)) {
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::DATA_ERR, "The key fetched from the keyPath does not match the key of the cursor."));
            return 0;
        }

        return keyPathKey.release();
    }

    if (!key) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::DATA_ERR, "No key supplied"));
        return 0;
    }

    return key;
}

void IDBObjectStoreBackendImpl::putInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<SerializedScriptValue> prpValue, PassRefPtr<IDBKey> prpKey, PutMode putMode, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendInterface> transaction)
{
    RefPtr<SerializedScriptValue> value = prpValue;
    RefPtr<IDBKey> key = selectKeyForPut(objectStore.get(), prpKey.get(), putMode, callbacks.get(), value);
    if (!key)
        return;

    if (key->type() == IDBKey::NullType) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::DATA_ERR, "NULL key is not allowed."));
        return;
    }

    Vector<RefPtr<IDBKey> > indexKeys;
    for (IndexMap::iterator it = objectStore->m_indexes.begin(); it != objectStore->m_indexes.end(); ++it) {
        RefPtr<IDBKey> key = fetchKeyFromKeyPath(value.get(), it->second->keyPath());
        if (!key) {
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "The key could not be fetched from an index's keyPath."));
            return;
        }
        if (key->type() == IDBKey::NullType) {
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::DATA_ERR, "One of the derived (from a keyPath) keys for an index is NULL."));
            return;
        }
        if (!it->second->addingKeyAllowed(key.get())) {
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "One of the derived (from a keyPath) keys for an index does not satisfy its uniqueness requirements."));
            return;
        }
        indexKeys.append(key.release());
    }

    SQLiteStatement getQuery(objectStore->sqliteDatabase(), "SELECT id FROM ObjectStoreData " + whereClause(key.get()));
    bool ok = getQuery.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?

    bindWhereClause(getQuery, objectStore->id(), key.get());
    bool isExistingValue = getQuery.step() == SQLResultRow;
    if (putMode == AddOnly && isExistingValue) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::CONSTRAINT_ERR, "Key already exists in the object store."));
        return;
    }

    // Before this point, don't do any mutation.  After this point, rollback the transaction in case of error.

    int64_t dataRowId = isExistingValue ? getQuery.getColumnInt(0) : InvalidId;
    if (!objectStore->m_backingStore->putObjectStoreRecord(objectStore->id(), *key, value->toWireString(), dataRowId, dataRowId == InvalidId)) {
        // FIXME: The Indexed Database specification does not have an error code dedicated to I/O errors.
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Error writing data to stable storage."));
        transaction->abort();
        return;
    }

    if (!objectStore->m_backingStore->deleteIndexDataForRecord(dataRowId)) {
        // FIXME: The Indexed Database specification does not have an error code dedicated to I/O errors.
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Error writing data to stable storage."));
        transaction->abort();
        return;
    }

    int i = 0;
    for (IndexMap::iterator it = objectStore->m_indexes.begin(); it != objectStore->m_indexes.end(); ++it, ++i) {
        if (!it->second->hasValidId())
            continue; // The index object has been created, but does not exist in the database yet.
        if (!objectStore->m_backingStore->putIndexDataForRecord(it->second->id(), *indexKeys[i], dataRowId)) {
            // FIXME: The Indexed Database specification does not have an error code dedicated to I/O errors.
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Error writing data to stable storage."));
            transaction->abort();
            return;
        }
    }

    callbacks->onSuccess(key.get());
}

void IDBObjectStoreBackendImpl::deleteFunction(PassRefPtr<IDBKey> prpKey, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transaction, ExceptionCode& ec)
{
    if (transaction->mode() == IDBTransaction::READ_ONLY) {
        ec = IDBDatabaseException::READ_ONLY_ERR;
        return;
    }

    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBKey> key = prpKey;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;

    if (!transaction->scheduleTask(createCallbackTask(&IDBObjectStoreBackendImpl::deleteInternal, objectStore, key, callbacks)))
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
}

void IDBObjectStoreBackendImpl::deleteInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks> callbacks)
{
    SQLiteStatement idQuery(objectStore->sqliteDatabase(), "SELECT id FROM ObjectStoreData " + whereClause(key.get()));
    bool ok = idQuery.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?
    bindWhereClause(idQuery, objectStore->id(), key.get());
    if (idQuery.step() != SQLResultRow) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::NOT_FOUND_ERR, "Key does not exist in the object store."));
        return;
    }

    int64_t id = idQuery.getColumnInt64(0);

    SQLiteStatement osQuery(objectStore->sqliteDatabase(), "DELETE FROM ObjectStoreData WHERE id = ?");
    ok = osQuery.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?

    osQuery.bindInt64(1, id);

    ok = osQuery.step() == SQLResultDone;
    ASSERT_UNUSED(ok, ok);

    SQLiteStatement indexQuery(objectStore->sqliteDatabase(), "DELETE FROM IndexData WHERE objectStoreDataId = ?");
    ok = indexQuery.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?

    indexQuery.bindInt64(1, id);

    ok = indexQuery.step() == SQLResultDone;
    ASSERT_UNUSED(ok, ok);

    callbacks->onSuccess(SerializedScriptValue::nullValue());
}

void IDBObjectStoreBackendImpl::clear(PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transaction, ExceptionCode& ec)
{
    if (transaction->mode() == IDBTransaction::READ_ONLY) {
        ec = IDBDatabaseException::READ_ONLY_ERR;
        return;
    }

    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;

    if (!transaction->scheduleTask(createCallbackTask(&IDBObjectStoreBackendImpl::clearInternal, objectStore, callbacks)))
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
}

void IDBObjectStoreBackendImpl::clearInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBCallbacks> callbacks)
{
    objectStore->m_backingStore->clearObjectStore(objectStore->id());
    callbacks->onSuccess(SerializedScriptValue::undefinedValue());
}

namespace {
class PopulateIndexCallback : public IDBBackingStore::ObjectStoreRecordCallback {
public:
    PopulateIndexCallback(IDBBackingStore& backingStore, const String& indexKeyPath, int64_t indexId)
        : m_backingStore(backingStore)
        , m_indexKeyPath(indexKeyPath)
        , m_indexId(indexId)
    {
    }

    virtual bool callback(int64_t objectStoreDataId, const String& value)
    {
        RefPtr<SerializedScriptValue> objectValue = SerializedScriptValue::createFromWire(value);
        RefPtr<IDBKey> indexKey = fetchKeyFromKeyPath(objectValue.get(), m_indexKeyPath);

        if (!m_backingStore.putIndexDataForRecord(m_indexId, *indexKey, objectStoreDataId))
            return false;

        return true;
    }

private:
    IDBBackingStore& m_backingStore;
    const String& m_indexKeyPath;
    int64_t m_indexId;
};
}

static bool populateIndex(IDBBackingStore& backingStore, int64_t objectStoreId, int64_t indexId, const String& indexKeyPath)
{
    PopulateIndexCallback callback(backingStore, indexKeyPath, indexId);
    if (!backingStore.forEachObjectStoreRecord(objectStoreId, callback))
        return false;
    return true;
}

PassRefPtr<IDBIndexBackendInterface> IDBObjectStoreBackendImpl::createIndex(const String& name, const String& keyPath, bool unique, IDBTransactionBackendInterface* transaction, ExceptionCode& ec)
{
    if (m_indexes.contains(name)) {
        ec = IDBDatabaseException::CONSTRAINT_ERR;
        return 0;
    }
    if (transaction->mode() != IDBTransaction::VERSION_CHANGE) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
        return 0;
    }

    RefPtr<IDBIndexBackendImpl> index = IDBIndexBackendImpl::create(m_backingStore.get(), name, m_name, keyPath, unique);
    ASSERT(index->name() == name);

    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBTransactionBackendInterface> transactionPtr = transaction;
    if (!transaction->scheduleTask(createCallbackTask(&IDBObjectStoreBackendImpl::createIndexInternal, objectStore, index, transaction),
                                   createCallbackTask(&IDBObjectStoreBackendImpl::removeIndexFromMap, objectStore, index))) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
        return 0;
    }

    m_indexes.set(name, index);
    return index.release();
}

void IDBObjectStoreBackendImpl::createIndexInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index, PassRefPtr<IDBTransactionBackendInterface> transaction)
{
    int64_t id;
    if (!objectStore->m_backingStore->createIndex(objectStore->m_id, index->name(), index->keyPath(), index->unique(), id)) {
        transaction->abort();
        return;
    }

    index->setId(id);

    if (!populateIndex(*objectStore->m_backingStore, objectStore->m_id, id, index->keyPath())) {
        transaction->abort();
        return;
    }

    transaction->didCompleteTaskEvents();
}

PassRefPtr<IDBIndexBackendInterface> IDBObjectStoreBackendImpl::index(const String& name, ExceptionCode& ec)
{
    RefPtr<IDBIndexBackendInterface> index = m_indexes.get(name);
    if (!index) {
        ec = IDBDatabaseException::NOT_FOUND_ERR;
        return 0;
    }
    return index.release();
}

void IDBObjectStoreBackendImpl::deleteIndex(const String& name, IDBTransactionBackendInterface* transaction, ExceptionCode& ec)
{
    if (transaction->mode() != IDBTransaction::VERSION_CHANGE) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
        return;
    }

    RefPtr<IDBIndexBackendImpl> index = m_indexes.get(name);
    if (!index) {
        ec = IDBDatabaseException::NOT_FOUND_ERR;
        return;
    }

    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBTransactionBackendInterface> transactionPtr = transaction;
    if (!transaction->scheduleTask(createCallbackTask(&IDBObjectStoreBackendImpl::deleteIndexInternal, objectStore, index, transactionPtr),
                                   createCallbackTask(&IDBObjectStoreBackendImpl::addIndexToMap, objectStore, index))) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
        return;
    }
    m_indexes.remove(name);
}

void IDBObjectStoreBackendImpl::deleteIndexInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBIndexBackendImpl> index, PassRefPtr<IDBTransactionBackendInterface> transaction)
{
    objectStore->m_backingStore->deleteIndex(index->id());
    transaction->didCompleteTaskEvents();
}

void IDBObjectStoreBackendImpl::openCursor(PassRefPtr<IDBKeyRange> prpRange, unsigned short direction, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBKeyRange> range = prpRange;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendInterface> transaction = transactionPtr;
    if (!transaction->scheduleTask(createCallbackTask(&IDBObjectStoreBackendImpl::openCursorInternal, objectStore, range, direction, callbacks, transaction)))
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
}

void IDBObjectStoreBackendImpl::openCursorInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKeyRange> range, unsigned short tmpDirection, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendInterface> transaction)
{
    bool lowerBound = range && range->lower();
    bool upperBound = range && range->upper();

    // Several files depend on this order of selects.
    String sql = "SELECT id, keyString, keyDate, keyNumber, value, keyString, keyDate, keyNumber FROM ObjectStoreData WHERE ";
    if (lowerBound)
        sql += range->lower()->lowerCursorWhereFragment(range->lowerWhereClauseComparisonOperator());
    if (upperBound)
        sql += range->upper()->upperCursorWhereFragment(range->upperWhereClauseComparisonOperator());
    sql += "objectStoreId = ? ORDER BY ";

    IDBCursor::Direction direction = static_cast<IDBCursor::Direction>(tmpDirection);
    if (direction == IDBCursor::NEXT || direction == IDBCursor::NEXT_NO_DUPLICATE)
        sql += "keyString, keyDate, keyNumber";
    else
        sql += "keyString DESC, keyDate DESC, keyNumber DESC";

    OwnPtr<SQLiteStatement> query = adoptPtr(new SQLiteStatement(objectStore->sqliteDatabase(), sql));
    bool ok = query->prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?

    int currentColumn = 1;
    if (lowerBound)
        currentColumn += range->lower()->bind(*query, currentColumn);
    if (upperBound)
        currentColumn += range->upper()->bind(*query, currentColumn);
    query->bindInt64(currentColumn, objectStore->id());

    if (query->step() != SQLResultRow) {
        callbacks->onSuccess(SerializedScriptValue::nullValue());
        return;
    }

    RefPtr<IDBCursorBackendInterface> cursor = IDBCursorBackendImpl::create(objectStore->m_backingStore.get(), range, direction, query.release(), IDBCursorBackendInterface::ObjectStoreCursor, transaction.get(), objectStore.get());
    callbacks->onSuccess(cursor.release());
}

void IDBObjectStoreBackendImpl::loadIndexes()
{
    Vector<int64_t> ids;
    Vector<String> names;
    Vector<String> keyPaths;
    Vector<bool> uniqueFlags;
    m_backingStore->getIndexes(m_id, ids, names, keyPaths, uniqueFlags);

    ASSERT(names.size() == ids.size());
    ASSERT(keyPaths.size() == ids.size());
    ASSERT(uniqueFlags.size() == ids.size());

    for (size_t i = 0; i < ids.size(); i++)
        m_indexes.set(names[i], IDBIndexBackendImpl::create(m_backingStore.get(), ids[i], names[i], m_name, keyPaths[i], uniqueFlags[i]));
}

SQLiteDatabase& IDBObjectStoreBackendImpl::sqliteDatabase() const 
{
    return m_backingStore->db();
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
    if (m_autoIncrementNumber > 0)
        return IDBKey::createNumber(m_autoIncrementNumber++);

    m_autoIncrementNumber = static_cast<int>(m_backingStore->nextAutoIncrementNumber(id()));
    return IDBKey::createNumber(m_autoIncrementNumber++);
}


} // namespace WebCore

#endif
