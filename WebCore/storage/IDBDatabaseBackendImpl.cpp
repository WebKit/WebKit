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
#include "IDBObjectStoreBackendImpl.h"
#include "IDBTransactionBackendInterface.h"
#include "IDBTransactionCoordinator.h"
#include "SQLiteDatabase.h"
#include "SQLiteStatement.h"
#include "SQLiteTransaction.h"

namespace WebCore {

static bool extractMetaData(SQLiteDatabase* sqliteDatabase, const String& expectedName, String& foundDescription, String& foundVersion)
{
    SQLiteStatement metaDataQuery(*sqliteDatabase, "SELECT name, description, version FROM MetaData");
    if (metaDataQuery.prepare() != SQLResultOk || metaDataQuery.step() != SQLResultRow)
        return false;

    if (metaDataQuery.getColumnText(0) != expectedName) {
        LOG_ERROR("Name in MetaData (%s) doesn't match expected (%s) for IndexedDB", metaDataQuery.getColumnText(0).utf8().data(), expectedName.utf8().data());
        ASSERT_NOT_REACHED();
    }
    foundDescription = metaDataQuery.getColumnText(1);
    foundVersion = metaDataQuery.getColumnText(2);

    if (metaDataQuery.step() == SQLResultRow) {
        LOG_ERROR("More than one row found in MetaData table");
        ASSERT_NOT_REACHED();
    }

    return true;
}

static bool setMetaData(SQLiteDatabase* sqliteDatabase, const String& name, const String& description, const String& version)
{
    ASSERT(!name.isNull() && !description.isNull() && !version.isNull());

    sqliteDatabase->executeCommand("DELETE FROM MetaData");

    SQLiteStatement insert(*sqliteDatabase, "INSERT INTO MetaData (name, description, version) VALUES (?, ?, ?)");
    if (insert.prepare() != SQLResultOk) {
        LOG_ERROR("Failed to prepare MetaData insert statement for IndexedDB");
        return false;
    }

    insert.bindText(1, name);
    insert.bindText(2, description);
    insert.bindText(3, version);

    if (insert.step() != SQLResultDone) {
        LOG_ERROR("Failed to insert row into MetaData for IndexedDB");
        return false;
    }

    // FIXME: Should we assert there's only one row?

    return true;
}

IDBDatabaseBackendImpl::IDBDatabaseBackendImpl(const String& name, const String& description, PassOwnPtr<SQLiteDatabase> sqliteDatabase, IDBTransactionCoordinator* coordinator)
    : m_sqliteDatabase(sqliteDatabase)
    , m_name(name)
    , m_version("")
    , m_transactionCoordinator(coordinator)
{
    ASSERT(!m_name.isNull());

    // FIXME: The spec is in flux about how to handle description. Sync it up once a final decision is made.
    String foundDescription = "";
    bool result = extractMetaData(m_sqliteDatabase.get(), m_name, foundDescription, m_version);
    m_description = description.isNull() ? foundDescription : description;

    if (!result || m_description != foundDescription)
        setMetaData(m_sqliteDatabase.get(), m_name, m_description, m_version);

    loadObjectStores();
}

IDBDatabaseBackendImpl::~IDBDatabaseBackendImpl()
{
}

void IDBDatabaseBackendImpl::setDescription(const String& description)
{
    if (description == m_description)
        return;

    m_description = description;
    setMetaData(m_sqliteDatabase.get(), m_name, m_description, m_version);
}

PassRefPtr<DOMStringList> IDBDatabaseBackendImpl::objectStores() const
{
    RefPtr<DOMStringList> objectStoreNames = DOMStringList::create();
    for (ObjectStoreMap::const_iterator it = m_objectStores.begin(); it != m_objectStores.end(); ++it)
        objectStoreNames->append(it->first);
    return objectStoreNames.release();
}

PassRefPtr<IDBObjectStoreBackendInterface>  IDBDatabaseBackendImpl::createObjectStore(const String& name, const String& keyPath, bool autoIncrement, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    if (m_objectStores.contains(name)) {
        // FIXME: Throw CONSTRAINT_ERR in this case.
        return 0;
    }

    RefPtr<IDBObjectStoreBackendImpl> objectStore = IDBObjectStoreBackendImpl::create(this, name, keyPath, autoIncrement);
    ASSERT(objectStore->name() == name);

    RefPtr<IDBDatabaseBackendImpl> database = this;
    RefPtr<IDBTransactionBackendInterface> transaction = transactionPtr;
    if (!transaction->scheduleTask(createCallbackTask(&IDBDatabaseBackendImpl::createObjectStoreInternal, database, objectStore, transaction),
                                   createCallbackTask(&IDBDatabaseBackendImpl::removeObjectStoreFromMap, database, objectStore))) {
        return 0;
    }

    m_objectStores.set(name, objectStore);
    return objectStore.release();
}

void IDBDatabaseBackendImpl::createObjectStoreInternal(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl> database, PassRefPtr<IDBObjectStoreBackendImpl> objectStore,  PassRefPtr<IDBTransactionBackendInterface> transaction)
{
    SQLiteStatement insert(database->sqliteDatabase(), "INSERT INTO ObjectStores (name, keyPath, doAutoIncrement) VALUES (?, ?, ?)");
    if (insert.prepare() != SQLResultOk) {
        transaction->abort();
        return;
    }
    insert.bindText(1, objectStore->name());
    insert.bindText(2, objectStore->keyPath());
    insert.bindInt(3, static_cast<int>(objectStore->autoIncrement()));
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

void IDBDatabaseBackendImpl::removeObjectStore(const String& name, IDBTransactionBackendInterface* transactionPtr, ExceptionCode& ec)
{
    RefPtr<IDBObjectStoreBackendImpl> objectStore = m_objectStores.get(name);
    if (!objectStore) {
        ec = IDBDatabaseException::NOT_FOUND_ERR;
        return;
    }
    RefPtr<IDBDatabaseBackendImpl> database = this;
    RefPtr<IDBTransactionBackendInterface> transaction = transactionPtr;
    if (!transaction->scheduleTask(createCallbackTask(&IDBDatabaseBackendImpl::removeObjectStoreInternal, database, objectStore, transaction),
                                   createCallbackTask(&IDBDatabaseBackendImpl::addObjectStoreToMap, database, objectStore))) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
        return;
    }
    m_objectStores.remove(name);
}

void IDBDatabaseBackendImpl::removeObjectStoreInternal(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl> database, PassRefPtr<IDBObjectStoreBackendImpl> objectStore, PassRefPtr<IDBTransactionBackendInterface> transaction)
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
    RefPtr<DOMStringList> objectStores = DOMStringList::create();
    RefPtr<IDBTransactionBackendInterface> transaction = m_transactionCoordinator->createTransaction(objectStores.get(), IDBTransaction::VERSION_CHANGE, 0, this);
    if (!transaction->scheduleTask(createCallbackTask(&IDBDatabaseBackendImpl::setVersionInternal, database, version, callbacks, transaction),
                                   createCallbackTask(&IDBDatabaseBackendImpl::resetVersion, database, m_version))) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
    }
}

void IDBDatabaseBackendImpl::setVersionInternal(ScriptExecutionContext*, PassRefPtr<IDBDatabaseBackendImpl> database, const String& version, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<IDBTransactionBackendInterface> transaction)
{
    database->m_version = version;
    if (!setMetaData(database->m_sqliteDatabase.get(), database->m_name, database->m_description, database->m_version)) {
        // FIXME: The Indexed Database specification does not have an error code dedicated to I/O errors.
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Error writing data to stable storage."));
        transaction->abort();
        return;
    }
    callbacks->onSuccess(transaction);
}

PassRefPtr<IDBTransactionBackendInterface> IDBDatabaseBackendImpl::transaction(DOMStringList* objectStores, unsigned short mode, unsigned long timeout, ExceptionCode&)
{
    // FIXME: Return not allowed err if close has been called.
    return m_transactionCoordinator->createTransaction(objectStores, mode, timeout, this);
}

void IDBDatabaseBackendImpl::close()
{
    // FIXME: Implement.
}

void IDBDatabaseBackendImpl::loadObjectStores()
{
    SQLiteStatement objectStoresQuery(sqliteDatabase(), "SELECT id, name, keyPath, doAutoIncrement FROM ObjectStores");
    bool ok = objectStoresQuery.prepare() == SQLResultOk;
    ASSERT_UNUSED(ok, ok); // FIXME: Better error handling?

    while (objectStoresQuery.step() == SQLResultRow) {
        int64_t id = objectStoresQuery.getColumnInt64(0);
        String name = objectStoresQuery.getColumnText(1);
        String keyPath = objectStoresQuery.getColumnText(2);
        bool autoIncrement = !!objectStoresQuery.getColumnInt(3);

        m_objectStores.set(name, IDBObjectStoreBackendImpl::create(this, id, name, keyPath, autoIncrement));
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
