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
#include "IDBIndexBackendImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBCallbacks.h"
#include "IDBCursorBackendImpl.h"
#include "IDBDatabaseBackendImpl.h"
#include "IDBDatabaseException.h"
#include "IDBKey.h"
#include "IDBKeyRange.h"
#include "IDBObjectStoreBackendImpl.h"
#include "SQLiteDatabase.h"
#include "SQLiteStatement.h"

namespace WebCore {

IDBIndexBackendImpl::IDBIndexBackendImpl(IDBObjectStoreBackendImpl* objectStore, int64_t id, const String& name, const String& keyPath, bool unique)
    : m_objectStore(objectStore)
    , m_id(id)
    , m_name(name)
    , m_keyPath(keyPath)
    , m_unique(unique)
{
}

IDBIndexBackendImpl::~IDBIndexBackendImpl()
{
}

String IDBIndexBackendImpl::storeName()
{
    return m_objectStore->name();
}

static void openCursorInternal(SQLiteDatabase& database, IDBIndexBackendImpl* index, IDBKeyRange* range, unsigned short untypedDirection, bool objectCursor, IDBCallbacks* callbacks)
{
    // Several files depend on this order of selects.
    String sql = String("SELECT IndexData.id, IndexData.keyString, IndexData.keyDate, IndexData.keyNumber, ")
                 + (objectCursor ? "ObjectStoreData.value " : "ObjectStoreData.keyString, ObjectStoreData.keyDate, ObjectStoreData.keyNumber ")
                 + "FROM IndexData INNER JOIN ObjectStoreData ON IndexData.objectStoreDataId = ObjectStoreData.id WHERE ";

    bool leftBound = range && (range->flags() & IDBKeyRange::LEFT_BOUND || range->flags() == IDBKeyRange::SINGLE);
    bool rightBound = range && (range->flags() & IDBKeyRange::RIGHT_BOUND || range->flags() == IDBKeyRange::SINGLE);
    
    if (leftBound)
        sql += range->left()->leftCursorWhereFragment(range->leftWhereClauseComparisonOperator(), "IndexData.");
    if (rightBound)
        sql += range->right()->rightCursorWhereFragment(range->rightWhereClauseComparisonOperator(), "IndexData.");
    sql += "IndexData.indexId = ? ORDER BY ";

    IDBCursor::Direction direction = static_cast<IDBCursor::Direction>(untypedDirection);
    if (direction == IDBCursor::NEXT || direction == IDBCursor::NEXT_NO_DUPLICATE)
        sql += "IndexData.keyString, IndexData.keyDate, IndexData.keyNumber, IndexData.id";
    else
        sql += "IndexData.keyString DESC, IndexData.keyDate DESC, IndexData.keyNumber DESC, IndexData.id DESC";

    OwnPtr<SQLiteStatement> query = adoptPtr(new SQLiteStatement(database, sql));
    bool ok = query->prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?

    int indexColumn = 1;
    if (leftBound)
        indexColumn += range->left()->bind(*query, indexColumn);
    if (rightBound)
        indexColumn += range->right()->bind(*query, indexColumn);
    query->bindInt64(indexColumn, index->id());

    if (query->step() != SQLResultRow) {
        callbacks->onSuccess();
        return;
    }

    RefPtr<IDBCursorBackendInterface> cursor = IDBCursorBackendImpl::create(index, range, direction, query.release(), objectCursor);
    callbacks->onSuccess(cursor.release());
}

void IDBIndexBackendImpl::openObjectCursor(PassRefPtr<IDBKeyRange> keyRange, unsigned short direction, PassRefPtr<IDBCallbacks> callbacks)
{
    openCursorInternal(sqliteDatabase(), this, keyRange.get(), direction, true, callbacks.get());
}

void IDBIndexBackendImpl::openCursor(PassRefPtr<IDBKeyRange> keyRange, unsigned short direction, PassRefPtr<IDBCallbacks> callbacks)
{
    openCursorInternal(sqliteDatabase(), this, keyRange.get(), direction, false, callbacks.get());
}

static void getInternal(SQLiteDatabase& database, int64 id, IDBKey* key, bool getObject, IDBCallbacks* callbacks)
{
    String sql = String("SELECT ")
                 + (getObject ? "ObjectStoreData.value " : "ObjectStoreData.keyString, ObjectStoreData.keyDate, ObjectStoreData.keyNumber ")
                 + "FROM IndexData INNER JOIN ObjectStoreData ON IndexData.objectStoreDataId = ObjectStoreData.id "
                 + "WHERE IndexData.indexId = ?  AND  " + key->whereSyntax("IndexData.")
                 + "ORDER BY IndexData.id LIMIT 1"; // Order by insertion order when all else fails.
    SQLiteStatement query(database, sql);
    bool ok = query.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?

    query.bindInt64(1, id);
    key->bind(query, 2);
    if (query.step() != SQLResultRow) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::NOT_FOUND_ERR, "Key does not exist in the index."));
        return;
    }

    if (getObject)
        callbacks->onSuccess(SerializedScriptValue::createFromWire(query.getColumnText(0)));
    else
        callbacks->onSuccess(IDBKey::fromQuery(query, 0));
    ASSERT(query.step() != SQLResultRow);
}

void IDBIndexBackendImpl::getObject(PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks> callbacks)
{
    getInternal(sqliteDatabase(), m_id, key.get(), true, callbacks.get());
}

void IDBIndexBackendImpl::get(PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks> callbacks)
{
    getInternal(sqliteDatabase(), m_id, key.get(), false, callbacks.get());
}

static String whereClause(IDBKey* key)
{
    return "WHERE indexId = ?  AND  " + key->whereSyntax();
}

static void bindWhereClause(SQLiteStatement& query, int64_t id, IDBKey* key)
{
    query.bindInt64(1, id);
    key->bind(query, 2);
}

bool IDBIndexBackendImpl::addingKeyAllowed(IDBKey* key)
{
    if (!m_unique)
        return true;

    SQLiteStatement query(sqliteDatabase(), "SELECT id FROM IndexData " + whereClause(key));
    bool ok = query.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?
    bindWhereClause(query, m_id, key);
    bool existingValue = query.step() == SQLResultRow;

    return !existingValue;
}

SQLiteDatabase& IDBIndexBackendImpl::sqliteDatabase() const
{
    return m_objectStore->database()->sqliteDatabase();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
