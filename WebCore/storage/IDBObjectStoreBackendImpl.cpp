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

IDBObjectStoreBackendImpl::IDBObjectStoreBackendImpl(IDBDatabaseBackendImpl* database, int64_t id, const String& name, const String& keyPath, bool autoIncrement)
    : m_database(database)
    , m_id(id)
    , m_name(name)
    , m_keyPath(keyPath)
    , m_autoIncrement(autoIncrement)
{
    loadIndexes();
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

void IDBObjectStoreBackendImpl::get(PassRefPtr<IDBKey> prpKey, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transaction)
{
    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBKey> key = prpKey;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    if (!transaction->scheduleTask(createCallbackTask(&IDBObjectStoreBackendImpl::getInternal, objectStore, key, callbacks)))
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::NOT_ALLOWED_ERR, "get must be called in the context of a transaction."));
}

void IDBObjectStoreBackendImpl::getInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks> callbacks)
{
    SQLiteStatement query(objectStore->sqliteDatabase(), "SELECT keyString, keyDate, keyNumber, value FROM ObjectStoreData " + whereClause(key.get()));
    bool ok = query.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?

    bindWhereClause(query, objectStore->id(), key.get());
    if (query.step() != SQLResultRow) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::NOT_FOUND_ERR, "Key does not exist in the object store."));
        return;
    }

    ASSERT((key->type() == IDBKey::StringType) != query.isColumnNull(0));
    // FIXME: Implement date.
    ASSERT((key->type() == IDBKey::NumberType) != query.isColumnNull(2));

    callbacks->onSuccess(SerializedScriptValue::createFromWire(query.getColumnText(3)));
    ASSERT(query.step() != SQLResultRow);
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

static void putObjectStoreData(SQLiteDatabase& db, IDBKey* key, SerializedScriptValue* value, int64_t objectStoreId, int64_t* dataRowId)
{
    String sql = *dataRowId != -1 ? "UPDATE ObjectStoreData SET keyString = ?, keyDate = ?, keyNumber = ?, value = ? WHERE id = ?"
                                  : "INSERT INTO ObjectStoreData (keyString, keyDate, keyNumber, value, objectStoreId) VALUES (?, ?, ?, ?, ?)";
    SQLiteStatement query(db, sql);
    bool ok = query.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?
    key->bindWithNulls(query, 1);
    query.bindText(4, value->toWireString());
    if (*dataRowId != -1)
        query.bindInt(5, *dataRowId);
    else
        query.bindInt64(5, objectStoreId);

    ok = query.step() == SQLResultDone;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?

    if (*dataRowId == -1)
        *dataRowId = db.lastInsertRowID();
}

static void putIndexData(SQLiteDatabase& db, IDBKey* key, int64_t indexId, int64_t objectStoreDataId)
{
    SQLiteStatement deleteQuery(db, "DELETE FROM IndexData WHERE objectStoreDataId = ?");
    bool ok = deleteQuery.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling.
    deleteQuery.bindInt64(1, objectStoreDataId);
    ok = deleteQuery.step() == SQLResultDone;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling.

    SQLiteStatement putQuery(db, "INSERT INTO IndexData (keyString, keyDate, keyNumber, indexId, objectStoreDataId) VALUES (?, ?, ?, ?, ?)");
    ok = putQuery.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?
    key->bindWithNulls(putQuery, 1);
    putQuery.bindInt64(4, indexId);
    putQuery.bindInt64(5, objectStoreDataId);

    ok = putQuery.step() == SQLResultDone;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?
}

void IDBObjectStoreBackendImpl::put(PassRefPtr<SerializedScriptValue> prpValue, PassRefPtr<IDBKey> prpKey, bool addOnly, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transaction)
{
    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<SerializedScriptValue> value = prpValue;
    RefPtr<IDBKey> key = prpKey;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    if (!transaction->scheduleTask(createCallbackTask(&IDBObjectStoreBackendImpl::putInternal, objectStore, value, key, addOnly, callbacks)))
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::NOT_ALLOWED_ERR, "put must be called in the context of a transaction."));
}

void IDBObjectStoreBackendImpl::putInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<SerializedScriptValue> prpValue, PassRefPtr<IDBKey> prpKey, bool addOnly, PassRefPtr<IDBCallbacks> callbacks)
{
    RefPtr<SerializedScriptValue> value = prpValue;
    RefPtr<IDBKey> key = prpKey;

    if (!objectStore->m_keyPath.isNull()) {
        if (key) {
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "A key was supplied for an objectStore that has a keyPath."));
            return;
        }
        key = fetchKeyFromKeyPath(value.get(), objectStore->m_keyPath);
        if (!key) {
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "The key could not be fetched from the keyPath."));
            return;
        }
    } else if (!key) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::DATA_ERR, "No key supplied."));
        return;
    }

    Vector<RefPtr<IDBKey> > indexKeys;
    for (IndexMap::iterator it = objectStore->m_indexes.begin(); it != objectStore->m_indexes.end(); ++it) {
        RefPtr<IDBKey> key = fetchKeyFromKeyPath(value.get(), it->second->keyPath());
        if (!key) {
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "The key could not be fetched from an index's keyPath."));
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
    if (addOnly && isExistingValue) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::CONSTRAINT_ERR, "Key already exists in the object store."));
        return;
    }

    // Before this point, don't do any mutation.  After this point, don't error out.

    int64_t dataRowId = isExistingValue ? getQuery.getColumnInt(0) : -1;
    putObjectStoreData(objectStore->sqliteDatabase(), key.get(), value.get(), objectStore->id(), &dataRowId);

    int i = 0;
    for (IndexMap::iterator it = objectStore->m_indexes.begin(); it != objectStore->m_indexes.end(); ++it, ++i)
        putIndexData(objectStore->sqliteDatabase(), indexKeys[i].get(), it->second->id(), dataRowId);

    callbacks->onSuccess(key.get());
}

void IDBObjectStoreBackendImpl::remove(PassRefPtr<IDBKey> prpKey, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transaction)
{
    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBKey> key = prpKey;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    if (!transaction->scheduleTask(createCallbackTask(&IDBObjectStoreBackendImpl::removeInternal, objectStore, key, callbacks)))
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::NOT_ALLOWED_ERR, "remove must be called in the context of a transaction."));
}

void IDBObjectStoreBackendImpl::removeInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks> callbacks)
{
    SQLiteStatement query(objectStore->sqliteDatabase(), "DELETE FROM ObjectStoreData " + whereClause(key.get()));
    bool ok = query.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?

    bindWhereClause(query, objectStore->id(), key.get());
    if (query.step() != SQLResultDone) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::NOT_FOUND_ERR, "Key does not exist in the object store."));
        return;
    }

    callbacks->onSuccess();
}

void IDBObjectStoreBackendImpl::createIndex(const String& name, const String& keyPath, bool unique, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transaction)
{
    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    if (!transaction->scheduleTask(createCallbackTask(&IDBObjectStoreBackendImpl::createIndexInternal, objectStore, name, keyPath, unique, callbacks)))
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::NOT_ALLOWED_ERR, "createIndex must be called in the context of a transaction."));
}

void IDBObjectStoreBackendImpl::createIndexInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, const String& name, const String& keyPath, bool unique, PassRefPtr<IDBCallbacks> callbacks)
{
    if (objectStore->m_indexes.contains(name)) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::CONSTRAINT_ERR, "Index name already exists."));
        return;
    }

    SQLiteStatement insert(objectStore->sqliteDatabase(), "INSERT INTO Indexes (objectStoreId, name, keyPath, isUnique) VALUES (?, ?, ?, ?)");
    bool ok = insert.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling.
    insert.bindInt64(1, objectStore->m_id);
    insert.bindText(2, name);
    insert.bindText(3, keyPath);
    insert.bindInt(4, static_cast<int>(unique));
    ok = insert.step() == SQLResultDone;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling.
    int64_t id = objectStore->sqliteDatabase().lastInsertRowID();

    RefPtr<IDBIndexBackendImpl> index = IDBIndexBackendImpl::create(objectStore.get(), id, name, keyPath, unique);
    ASSERT(index->name() == name);
    objectStore->m_indexes.set(name, index);
    callbacks->onSuccess(index.get());
}

PassRefPtr<IDBIndexBackendInterface> IDBObjectStoreBackendImpl::index(const String& name)
{
    return m_indexes.get(name);
}

static void doDelete(SQLiteDatabase& db, const char* sql, int64_t id)
{
    SQLiteStatement deleteQuery(db, sql);
    bool ok = deleteQuery.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling.
    deleteQuery.bindInt64(1, id);
    ok = deleteQuery.step() == SQLResultDone;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling.
}

void IDBObjectStoreBackendImpl::removeIndex(const String& name, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transaction)
{
    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    if (!transaction->scheduleTask(createCallbackTask(&IDBObjectStoreBackendImpl::removeIndexInternal, objectStore, name, callbacks)))
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::NOT_ALLOWED_ERR, "createIndex must be called in the context of a transaction."));
}

void IDBObjectStoreBackendImpl::removeIndexInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, const String& name, PassRefPtr<IDBCallbacks> callbacks)
{
    RefPtr<IDBIndexBackendImpl> index = objectStore->m_indexes.get(name);
    if (!index) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::NOT_FOUND_ERR, "Index name does not exist."));
        return;
    }

    doDelete(objectStore->sqliteDatabase(), "DELETE FROM Indexes WHERE id = ?", index->id());
    doDelete(objectStore->sqliteDatabase(), "DELETE FROM IndexData WHERE indexId = ?", index->id());

    objectStore->m_indexes.remove(name);
    callbacks->onSuccess();
}

void IDBObjectStoreBackendImpl::openCursor(PassRefPtr<IDBKeyRange> prpRange, unsigned short direction, PassRefPtr<IDBCallbacks> prpCallbacks, IDBTransactionBackendInterface* transactionPtr)
{
    RefPtr<IDBObjectStoreBackendImpl> objectStore = this;
    RefPtr<IDBKeyRange> range = prpRange;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<IDBTransactionBackendInterface> transaction = transactionPtr;
    if (!transaction->scheduleTask(createCallbackTask(&IDBObjectStoreBackendImpl::openCursorInternal, objectStore, range, direction, callbacks, transaction)))
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::NOT_ALLOWED_ERR, "openCursor must be called in the context of a transaction."));
}

void IDBObjectStoreBackendImpl::openCursorInternal(ScriptExecutionContext*, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBKeyRange> range, unsigned short tmpDirection, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendInterface> transaction)
{
    // Several files depend on this order of selects.
    String sql = "SELECT id, keyString, keyDate, keyNumber, value FROM ObjectStoreData WHERE ";
    if (range->flags() & IDBKeyRange::LEFT_BOUND || range->flags() == IDBKeyRange::SINGLE)
        sql += range->left()->leftCursorWhereFragment(range->leftWhereClauseComparisonOperator());
    if (range->flags() & IDBKeyRange::RIGHT_BOUND || range->flags() == IDBKeyRange::SINGLE)
        sql += range->right()->rightCursorWhereFragment(range->rightWhereClauseComparisonOperator());
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
    if (range->flags() & IDBKeyRange::LEFT_BOUND || range->flags() == IDBKeyRange::SINGLE)
        currentColumn += range->left()->bind(*query, currentColumn);
    if (range->flags() & IDBKeyRange::RIGHT_BOUND || range->flags() == IDBKeyRange::SINGLE)
        currentColumn += range->right()->bind(*query, currentColumn);
    query->bindInt64(currentColumn, objectStore->id());

    if (query->step() != SQLResultRow) {
        callbacks->onSuccess();
        return;
    }

    RefPtr<IDBCursorBackendInterface> cursor = IDBCursorBackendImpl::create(objectStore, range, direction, query.release(), transaction.get());
    callbacks->onSuccess(cursor.release());
}

void IDBObjectStoreBackendImpl::loadIndexes()
{
    SQLiteStatement indexQuery(sqliteDatabase(), "SELECT id, name, keyPath, isUnique FROM Indexes WHERE objectStoreId = ?");
    bool ok = indexQuery.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?

    indexQuery.bindInt64(1, m_id);

    while (indexQuery.step() == SQLResultRow) {
        int64_t id = indexQuery.getColumnInt64(0);
        String name = indexQuery.getColumnText(1);
        String keyPath = indexQuery.getColumnText(2);
        bool unique = !!indexQuery.getColumnInt(3);

        m_indexes.set(name, IDBIndexBackendImpl::create(this, id, name, keyPath, unique));
    }
}

SQLiteDatabase& IDBObjectStoreBackendImpl::sqliteDatabase() const 
{
    return m_database->sqliteDatabase();
}

} // namespace WebCore

#endif
