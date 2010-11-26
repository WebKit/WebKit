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
#include "IDBDatabaseBackendImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "CrossThreadTask.h"
#include "DOMStringList.h"
#include "IDBDatabaseException.h"
#include "IDBFactoryBackendImpl.h"
#include "IDBObjectStoreBackendImpl.h"
#include "IDBSQLiteDatabase.h"
#include "IDBTransactionBackendImpl.h"
#include "IDBTransactionCoordinator.h"
#include "SQLiteStatement.h"
#include "SQLiteTransaction.h"

namespace WebCore {

static bool extractMetaData(SQLiteDatabase& sqliteDatabase, const String& name, String& foundVersion, int64& foundId)
{
    SQLiteStatement databaseQuery(sqliteDatabase, "SELECT id, version FROM Databases WHERE name = ?");
    if (databaseQuery.prepare() != SQLResultOk) {
        ASSERT_NOT_REACHED();
        return false;
    }
    databaseQuery.bindText(1, name);
    if (databaseQuery.step() != SQLResultRow)
        return false;

    foundId = databaseQuery.getColumnInt64(0);
    foundVersion = databaseQuery.getColumnText(1);

    if (databaseQuery.step() == SQLResultRow)
        ASSERT_NOT_REACHED();
    return true;
}

static bool setMetaData(SQLiteDatabase& sqliteDatabase, const String& name, const String& version, int64_t& rowId)
{
    ASSERT(!name.isNull());
    ASSERT(!version.isNull());

    String sql = rowId != IDBDatabaseBackendImpl::InvalidId ? "UPDATE Databases SET name = ?, version = ? WHERE id = ?"
                                                            : "INSERT INTO Databases (name, description, version) VALUES (?, '', ?)";
    SQLiteStatement query(sqliteDatabase, sql);
    if (query.prepare() != SQLResultOk) {
        ASSERT_NOT_REACHED();
        return false;
    }

    query.bindText(1, name);
    query.bindText(2, version);
    if (rowId != IDBDatabaseBackendImpl::InvalidId)
        query.bindInt64(3, rowId);

    if (query.step() != SQLResultDone)
        return false;

    if (rowId == IDBDatabaseBackendImpl::InvalidId)
        rowId = sqliteDatabase.lastInsertRowID();

    return true;
}

IDBDatabaseBackendImpl::IDBDatabaseBackendImpl(const String& name, IDBSQLiteDatabase* sqliteDatabase, IDBTransactionCoordinator* coordinator, IDBFactoryBackendImpl* factory, const String& uniqueIdentifier)
    : m_sqliteDatabase(sqliteDatabase)
    , m_id(InvalidId)
    , m_name(name)
    , m_version("")
    , m_identifier(uniqueIdentifier)
    , m_factory(factory)
    , m_transactionCoordinator(coordinator)
{
    ASSERT(!m_name.isNull());

    bool success = extractMetaData(m_sqliteDatabase->db(), m_name, m_version, m_id);
    ASSERT_UNUSED(success, success == (m_id != InvalidId));
    if (!setMetaData(m_sqliteDatabase->db(), m_name, m_version, m_id))
        ASSERT_NOT_REACHED(); // FIXME: Need better error handling.
    loadObjectStores();
}

IDBDatabaseBackendImpl::~IDBDatabaseBackendImpl()
{
    m_factory->removeIDBDatabaseBackend(m_identifier);
}

SQLiteDatabase& IDBDatabaseBackendImpl::sqliteDatabase() const
{
    return m_sqliteDatabase->db();
}

PassRefPtr<DOMStringList> IDBDatabaseBackendImpl::objectStoreNames() const
{
    RefPtr<DOMStringList> objectStoreNames = DOMStringList::create();
    for (ObjectStoreMap::const_iterator it = m_objectStores.begin(); it != m_objectStores.end(); ++it)
        objectStoreNames->append(it->first);
    return objectStoreNames.release();
}

PassRefPtr<IDBObjectStoreBackendInterface>  IDBDatabaseBackendImpl::createObjectStore(const String& name, const String& keyPath, bool autoIncrement, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    ASSERT(transactionPtr->mode() == IDBTransaction::VERSION_CHANGE);

    if (m_objectStores.contains(name)) {
        ec = IDBDatabaseException::CONSTRAINT_ERR;
        return 0;
    }

    RefPtr<IDBObjectStoreBackendImpl> objectStore = IDBObjectStoreBackendImpl::create(m_sqliteDatabase.get(), name, keyPath, autoIncrement);
    ASSERT(objectStore->name() == name);

    RefPtr<IDBDatabaseBackendImpl> database = this;
    RefPtr<IDBTransactionBackendInterface> transaction = transactionPtr;
    if (!transaction->scheduleTask(createCallbackTask(&IDBDatabaseBackendImpl::createObjectStoreInternal, database, objectStore, transaction),
                                   createCallbackTask(&IDBDatabaseBackendImpl::removeObjectStoreFromMap, database, objectStore))) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
        return 0;
    }

    m_objectStores.set(name, objectStore);
    return objectStore.release();
}

void IDBDatabaseBackendImpl::createObjectStoreInternal(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl> database, PassRefPtr<IDBObjectStoreBackendImpl> objectStore,  PassRefPtr<IDBTransactionBackendInterface> transaction)
{
    SQLiteStatement insert(database->sqliteDatabase(), "INSERT INTO ObjectStores (name, keyPath, doAutoIncrement, databaseId) VALUES (?, ?, ?, ?)");
    if (insert.prepare() != SQLResultOk) {
        transaction->abort();
        return;
    }
    insert.bindText(1, objectStore->name());
    insert.bindText(2, objectStore->keyPath());
    insert.bindInt(3, static_cast<int>(objectStore->autoIncrement()));
    insert.bindInt64(4, database->id());
    if (insert.step() != SQLResultDone) {
        transaction->abort();
        return;
    }
    int64_t id = database->sqliteDatabase().lastInsertRowID();
    objectStore->setId(id);
    transaction->didCompleteTaskEvents();
}

PassRefPtr<IDBObjectStoreBackendInterface> IDBDatabaseBackendImpl::objectStore(const String& name)
{
    return m_objectStores.get(name);
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

void IDBDatabaseBackendImpl::deleteObjectStore(const String& name, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    RefPtr<IDBObjectStoreBackendImpl> objectStore = m_objectStores.get(name);
    if (!objectStore) {
        ec = IDBDatabaseException::NOT_FOUND_ERR;
        return;
    }
    RefPtr<IDBDatabaseBackendImpl> database = this;
    RefPtr<IDBTransactionBackendInterface> transaction = transactionPtr;
    if (!transaction->scheduleTask(createCallbackTask(&IDBDatabaseBackendImpl::deleteObjectStoreInternal, database, objectStore, transaction),
                                   createCallbackTask(&IDBDatabaseBackendImpl::addObjectStoreToMap, database, objectStore))) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
        return;
    }
    m_objectStores.remove(name);
}

void IDBDatabaseBackendImpl::deleteObjectStoreInternal(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl> database, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBTransactionBackendInterface> transaction)
{
    doDelete(database->sqliteDatabase(), "DELETE FROM ObjectStores WHERE id = ?", objectStore->id());
    doDelete(database->sqliteDatabase(), "DELETE FROM ObjectStoreData WHERE objectStoreId = ?", objectStore->id());
    doDelete(database->sqliteDatabase(), "DELETE FROM IndexData WHERE indexId IN (SELECT id FROM Indexes WHERE objectStoreId = ?)", objectStore->id());
    doDelete(database->sqliteDatabase(), "DELETE FROM Indexes WHERE objectStoreId = ?", objectStore->id());

    transaction->didCompleteTaskEvents();
}

void IDBDatabaseBackendImpl::setVersion(const String& version, PassRefPtr<IDBCallbacks> prpCallbacks, ExceptionCode& ec)
{
    RefPtr<IDBDatabaseBackendImpl> database = this;
    RefPtr<IDBCallbacks> callbacks = prpCallbacks;
    RefPtr<DOMStringList> objectStoreNames = DOMStringList::create();
    RefPtr<IDBTransactionBackendInterface> transaction = IDBTransactionBackendImpl::create(objectStoreNames.get(), IDBTransaction::VERSION_CHANGE, 0, this);
    if (!transaction->scheduleTask(createCallbackTask(&IDBDatabaseBackendImpl::setVersionInternal, database, version, callbacks, transaction),
                                   createCallbackTask(&IDBDatabaseBackendImpl::resetVersion, database, m_version))) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
    }
}

void IDBDatabaseBackendImpl::setVersionInternal(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl> database, const String& version, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendInterface> transaction)
{
    int64_t databaseId = database->id();
    database->m_version = version;
    if (!setMetaData(database->m_sqliteDatabase->db(), database->m_name, database->m_version, databaseId)) {
        // FIXME: The Indexed Database specification does not have an error code dedicated to I/O errors.
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Error writing data to stable storage."));
        transaction->abort();
        return;
    }
    callbacks->onSuccess(transaction);
}

PassRefPtr<IDBTransactionBackendInterface> IDBDatabaseBackendImpl::transaction(DOMStringList* objectStoreNames, unsigned short mode, unsigned long timeout, ExceptionCode& ec)
{
    for (size_t i = 0; i < objectStoreNames->length(); ++i) {
        if (!m_objectStores.contains(objectStoreNames->item(i))) {
            ec = IDBDatabaseException::NOT_FOUND_ERR;
            return 0;
        }
    }

    // FIXME: Return not allowed err if close has been called.
    return IDBTransactionBackendImpl::create(objectStoreNames, mode, timeout, this);
}

void IDBDatabaseBackendImpl::close()
{
    // FIXME: Implement.
}

void IDBDatabaseBackendImpl::loadObjectStores()
{
    SQLiteStatement objectStoresQuery(sqliteDatabase(), "SELECT id, name, keyPath, doAutoIncrement FROM ObjectStores WHERE databaseId = ?");
    bool ok = objectStoresQuery.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?

    objectStoresQuery.bindInt64(1, m_id);

    while (objectStoresQuery.step() == SQLResultRow) {
        int64_t id = objectStoresQuery.getColumnInt64(0);
        String name = objectStoresQuery.getColumnText(1);
        String keyPath = objectStoresQuery.getColumnText(2);
        bool autoIncrement = !!objectStoresQuery.getColumnInt(3);

        m_objectStores.set(name, IDBObjectStoreBackendImpl::create(m_sqliteDatabase.get(), id, name, keyPath, autoIncrement));
    }
}

void IDBDatabaseBackendImpl::removeObjectStoreFromMap(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl> database, PassRefPtr<IDBObjectStoreBackendImpl> objectStore)
{
    ASSERT(database->m_objectStores.contains(objectStore->name()));
    database->m_objectStores.remove(objectStore->name());
}

void IDBDatabaseBackendImpl::addObjectStoreToMap(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl> database, PassRefPtr<IDBObjectStoreBackendImpl> objectStore)
{
    RefPtr<IDBObjectStoreBackendImpl> objectStorePtr = objectStore;
    ASSERT(!database->m_objectStores.contains(objectStorePtr->name()));
    database->m_objectStores.set(objectStorePtr->name(), objectStorePtr);
}

void IDBDatabaseBackendImpl::resetVersion(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl> database, const String& version)
{
    database->m_version = version;
}


} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
