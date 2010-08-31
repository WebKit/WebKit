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

#include "DOMStringList.h"
#include "IDBBindingUtilities.h"
#include "IDBCallbacks.h"
#include "IDBCursorBackendImpl.h"
#include "IDBDatabaseBackendImpl.h"
#include "IDBDatabaseException.h"
#include "IDBIndexBackendImpl.h"
#include "IDBKeyPath.h"
#include "IDBKeyPathBackendImpl.h"
#include "IDBKeyRange.h"
#include "SQLiteDatabase.h"
#include "SQLiteStatement.h"

#if ENABLE(INDEXED_DATABASE)

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

static String whereClause(IDBKey::Type type)
{
    switch (type) {
    case IDBKey::StringType:
        return "WHERE objectStoreId = ?  AND  keyString = ?";
    case IDBKey::NumberType:
        return "WHERE objectStoreId = ?  AND  keyNumber = ?";
    // FIXME: Implement date.
    case IDBKey::NullType:
        return "WHERE objectStoreId = ?  AND  keyString IS NULL  AND  keyDate IS NULL  AND  keyNumber IS NULL";
    }

    ASSERT_NOT_REACHED();
    return "";
}

// Returns number of items bound.
static int bindKey(SQLiteStatement& query, int column, IDBKey* key)
{
    switch (key->type()) {
    case IDBKey::StringType:
        query.bindText(column, key->string());
        return 1;
    case IDBKey::NumberType:
        query.bindInt(column, key->number());
        return 1;
    // FIXME: Implement date.
    case IDBKey::NullType:
        return 0;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static void bindWhereClause(SQLiteStatement& query, int64_t id, IDBKey* key)
{
    query.bindInt64(1, id);
    bindKey(query, 2, key);
}

void IDBObjectStoreBackendImpl::get(PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks> callbacks)
{
    SQLiteStatement query(sqliteDatabase(), "SELECT keyString, keyDate, keyNumber, value FROM ObjectStoreData " + whereClause(key->type()));
    bool ok = query.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?

    bindWhereClause(query, m_id, key.get());
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

void IDBObjectStoreBackendImpl::put(PassRefPtr<SerializedScriptValue> prpValue, PassRefPtr<IDBKey> prpKey, bool addOnly, PassRefPtr<IDBCallbacks> callbacks)
{
    RefPtr<SerializedScriptValue> value = prpValue;
    RefPtr<IDBKey> key = prpKey;

    if (!m_keyPath.isNull()) {
        if (key) {
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "A key was supplied for an objectStore that has a keyPath."));
            return;
        }
        Vector<RefPtr<SerializedScriptValue> > values;
        values.append(value);
        Vector<RefPtr<IDBKey> > idbKeys;
        IDBKeyPathBackendImpl::createIDBKeysFromSerializedValuesAndKeyPath(values, m_keyPath, idbKeys);
        if (idbKeys.isEmpty()) {
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "An invalid keyPath was supplied for an objectStore."));
            return;
        }
        key = idbKeys[0];
    }

    if (!key) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::DATA_ERR, "No key supplied."));
        return;
    }

    SQLiteStatement getQuery(sqliteDatabase(), "SELECT id FROM ObjectStoreData " + whereClause(key->type()));
    bool ok = getQuery.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?

    bindWhereClause(getQuery, m_id, key.get());
    bool existingValue = getQuery.step() == SQLResultRow;
    if (addOnly && existingValue) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::CONSTRAINT_ERR, "Key already exists in the object store."));
        return;
    }

    String sql = existingValue ? "UPDATE ObjectStoreData SET keyString = ?, keyDate = ?, keyNumber = ?, value = ? WHERE id = ?"
                               : "INSERT INTO ObjectStoreData (keyString, keyDate, keyNumber, value, objectStoreId) VALUES (?, ?, ?, ?, ?)";
    SQLiteStatement putQuery(sqliteDatabase(), sql);
    ok = putQuery.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?
    switch (key->type()) {
    case IDBKey::StringType:
        putQuery.bindText(1, key->string());
        putQuery.bindNull(2);
        putQuery.bindNull(3);
        break;
    // FIXME: Implement date.
    case IDBKey::NumberType:
        putQuery.bindNull(1);
        putQuery.bindNull(2);
        putQuery.bindInt(3, key->number());
        break;
    case IDBKey::NullType:
        putQuery.bindNull(1);
        putQuery.bindNull(2);
        putQuery.bindNull(3);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    putQuery.bindText(4, value->toWireString());
    if (existingValue)
        putQuery.bindInt(5, getQuery.getColumnInt(0));
    else
        putQuery.bindInt64(5, m_id);

    ok = putQuery.step() == SQLResultDone;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?

    callbacks->onSuccess(key.get());
}

void IDBObjectStoreBackendImpl::remove(PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks> callbacks)
{
    SQLiteStatement query(sqliteDatabase(), "DELETE FROM ObjectStoreData " + whereClause(key->type()));
    bool ok = query.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?

    bindWhereClause(query, m_id, key.get());
    if (query.step() != SQLResultDone) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::NOT_FOUND_ERR, "Key does not exist in the object store."));
        return;
    }

    callbacks->onSuccess();
}

void IDBObjectStoreBackendImpl::createIndex(const String& name, const String& keyPath, bool unique, PassRefPtr<IDBCallbacks> callbacks)
{
    if (m_indexes.contains(name)) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::CONSTRAINT_ERR, "Index name already exists."));
        return;
    }

    SQLiteStatement insert(sqliteDatabase(), "INSERT INTO Indexes (objectStoreId, name, keyPath, isUnique) VALUES (?, ?, ?, ?)");
    bool ok = insert.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling.
    insert.bindInt64(1, m_id);
    insert.bindText(2, name);
    insert.bindText(3, keyPath);
    insert.bindInt(4, static_cast<int>(unique));
    ok = insert.step() == SQLResultDone;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling.
    int64_t id = sqliteDatabase().lastInsertRowID();

    RefPtr<IDBIndexBackendInterface> index = IDBIndexBackendImpl::create(this, id, name, keyPath, unique);
    ASSERT(index->name() == name);
    m_indexes.set(name, index);
    callbacks->onSuccess(index.release());
}

PassRefPtr<IDBIndexBackendInterface> IDBObjectStoreBackendImpl::index(const String& name)
{
    return m_indexes.get(name);
}

void IDBObjectStoreBackendImpl::removeIndex(const String& name, PassRefPtr<IDBCallbacks> callbacks)
{
    if (!m_indexes.contains(name)) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::NOT_FOUND_ERR, "Index name does not exist."));
        return;
    }

    SQLiteStatement deleteQuery(sqliteDatabase(), "DELETE FROM Indexes WHERE name = ? AND objectStoreId = ?");
    bool ok = deleteQuery.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling.
    deleteQuery.bindText(1, name);
    deleteQuery.bindInt64(2, m_id);
    ok = deleteQuery.step() == SQLResultDone;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling.

    // FIXME: Delete index data as well.

    m_indexes.remove(name);
    callbacks->onSuccess();
}

static String leftCursorWhereFragment(IDBKey::Type type, String comparisonOperator)
{
    switch (type) {
    case IDBKey::StringType:
        return "? " + comparisonOperator + " keyString  AND  ";
    // FIXME: Implement date.
    case IDBKey::NumberType:
        return "(? " + comparisonOperator + " keyNumber  OR  NOT keyString IS NULL  OR  NOT keyDate IS NULL)  AND  ";
    case IDBKey::NullType:
        if (comparisonOperator == "<")
            return "NOT(keyString IS NULL  AND  keyDate IS NULL  AND  keyNumber IS NULL)  AND  ";
        return ""; // If it's =, the upper bound half will do the constraining. If it's <=, then that's a no-op.
    }
    ASSERT_NOT_REACHED();
    return "";
}

static String rightCursorWhereFragment(IDBKey::Type type, String comparisonOperator)
{
    switch (type) {
    case IDBKey::StringType:
        return "(keyString " + comparisonOperator + " ?  OR  keyString IS NULL)  AND  ";
    // FIXME: Implement date.
    case IDBKey::NumberType:
        return "(keyNumber " + comparisonOperator + " ? OR keyNumber IS NULL)  AND  keyString IS NULL  AND  keyDate IS NULL  AND  ";
    case IDBKey::NullType:
        if (comparisonOperator == "<")
            return "0 != 0  AND  ";
        return "keyString IS NULL  AND  keyDate IS NULL  AND  keyNumber IS NULL  AND  ";
    }
    ASSERT_NOT_REACHED();
    return "";
}

void IDBObjectStoreBackendImpl::openCursor(PassRefPtr<IDBKeyRange> range, unsigned short tmpDirection, PassRefPtr<IDBCallbacks> callbacks)
{
    String lowerEquality;
    if (range->flags() & IDBKeyRange::LEFT_OPEN)
        lowerEquality = "<";
    else if (range->flags() & IDBKeyRange::LEFT_BOUND)
        lowerEquality = "<=";
    else
        lowerEquality = "=";

    String upperEquality;
    if (range->flags() & IDBKeyRange::RIGHT_OPEN)
        upperEquality = "<";
    else if (range->flags() & IDBKeyRange::RIGHT_BOUND)
        upperEquality = "<=";
    else
        upperEquality = "=";

    // If you change the order of this select, you'll need to change it in IDBCursorBackendImpl.cpp as well.
    String sql = "SELECT id, keyString, keyDate, keyNumber, value FROM ObjectStoreData WHERE ";
    if (range->flags() & IDBKeyRange::LEFT_BOUND || range->flags() == IDBKeyRange::SINGLE)
        sql += leftCursorWhereFragment(range->left()->type(), lowerEquality);
    if (range->flags() & IDBKeyRange::RIGHT_BOUND || range->flags() == IDBKeyRange::SINGLE)
        sql += rightCursorWhereFragment(range->right()->type(), upperEquality);
    sql += "objectStoreId = ? ORDER BY ";

    IDBCursor::Direction direction = static_cast<IDBCursor::Direction>(tmpDirection);
    if (direction == IDBCursor::NEXT || direction == IDBCursor::NEXT_NO_DUPLICATE)
        sql += "keyString, keyDate, keyNumber";
    else
        sql += "keyString DESC, keyDate DESC, keyNumber DESC";

    OwnPtr<SQLiteStatement> query = adoptPtr(new SQLiteStatement(sqliteDatabase(), sql));
    bool ok = query->prepare() == SQLResultOk;
    ASSERT(ok); // FIXME: Better error handling?

    int currentColumn = 1;
    if (range->flags() & IDBKeyRange::LEFT_BOUND || range->flags() == IDBKeyRange::SINGLE)
        currentColumn += bindKey(*query, currentColumn, range->left().get());
    if (range->flags() & IDBKeyRange::RIGHT_BOUND || range->flags() == IDBKeyRange::SINGLE)
        currentColumn += bindKey(*query, currentColumn, range->right().get());
    query->bindInt64(currentColumn, m_id);

    if (query->step() != SQLResultRow) {
        callbacks->onSuccess();
        return;
    }

    RefPtr<IDBCursorBackendInterface> cursor = IDBCursorBackendImpl::create(this, range, direction, query.release());
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
