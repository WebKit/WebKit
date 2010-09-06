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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
#include "IDBFactoryBackendImpl.h"

#include "DOMStringList.h"
#include "FileSystem.h"
#include "IDBDatabaseBackendImpl.h"
#include "IDBDatabaseException.h"
#include "IDBTransactionCoordinator.h"
#include "SQLiteDatabase.h"
#include "SecurityOrigin.h"
#include <wtf/Threading.h>
#include <wtf/UnusedParam.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

IDBFactoryBackendImpl::IDBFactoryBackendImpl() 
    : m_transactionCoordinator(IDBTransactionCoordinator::create())
{
}

IDBFactoryBackendImpl::~IDBFactoryBackendImpl()
{
}

static PassOwnPtr<SQLiteDatabase> openSQLiteDatabase(SecurityOrigin* securityOrigin, String name)
{
    String pathBase = "/tmp/temporary-indexed-db-files"; // FIXME: Write a PageGroupSettings class and have this value come from that.
    if (!makeAllDirectories(pathBase)) {
        // FIXME: Is there any other thing we could possibly do to recover at this point? If so, do it rather than just erroring out.
        LOG_ERROR("Unabled to create LocalStorage database path %s", pathBase.utf8().data());
        return 0;
    }

    String databaseIdentifier = securityOrigin->databaseIdentifier();
    String santizedName = encodeForFileName(name);
    String path = pathByAppendingComponent(pathBase, databaseIdentifier + "_" + santizedName + ".indexeddb");
    
    OwnPtr<SQLiteDatabase> sqliteDatabase = adoptPtr(new SQLiteDatabase());
    if (!sqliteDatabase->open(path)) {
        // FIXME: Is there any other thing we could possibly do to recover at this point? If so, do it rather than just erroring out.
        LOG_ERROR("Failed to open database file %s for IndexedDB", path.utf8().data());
        return 0;
    }

    return sqliteDatabase.release();
}

static bool createTables(SQLiteDatabase* sqliteDatabase)
{
    // FIXME: Remove all the drop table commands once the on disk structure stabilizes.
    static const char* commands[] = {
        "DROP TABLE IF EXISTS MetaData",
        "CREATE TABLE IF NOT EXISTS MetaData (id INTEGER PRIMARY KEY, name TEXT NOT NULL, description TEXT NOT NULL, version TEXT NOT NULL)",

        "DROP TABLE IF EXISTS ObjectStores",
        "CREATE TABLE IF NOT EXISTS ObjectStores (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, keyPath TEXT, doAutoIncrement INTEGER NOT NULL)",
        "DROP INDEX IF EXISTS ObjectStores_name",
        "CREATE UNIQUE INDEX IF NOT EXISTS ObjectStores_name ON ObjectStores(name)",

        "DROP TABLE IF EXISTS Indexes",
        "CREATE TABLE IF NOT EXISTS Indexes (id INTEGER PRIMARY KEY, objectStoreId INTEGER NOT NULL REFERENCES ObjectStore(id), name TEXT NOT NULL UNIQUE, keyPath TEXT, isUnique INTEGER NOT NULL)",
        "DROP INDEX IF EXISTS Indexes_composit",
        "CREATE UNIQUE INDEX IF NOT EXISTS Indexes_composit ON Indexes(objectStoreId, name)",

        "DROP TABLE IF EXISTS ObjectStoreData",
        "CREATE TABLE IF NOT EXISTS ObjectStoreData (id INTEGER PRIMARY KEY, objectStoreId INTEGER NOT NULL REFERENCES ObjectStore(id), keyString TEXT UNIQUE, keyDate INTEGER UNIQUE, keyNumber INTEGER UNIQUE, value TEXT NOT NULL)",
        "DROP INDEX IF EXISTS ObjectStoreData_composit",
        "CREATE UNIQUE INDEX IF NOT EXISTS ObjectStoreData_composit ON ObjectStoreData(keyString, keyDate, keyNumber, objectStoreId)",

        "DROP TABLE IF EXISTS IndexData",
        "CREATE TABLE IF NOT EXISTS IndexData (id INTEGER PRIMARY KEY, indexId INTEGER NOT NULL REFERENCES Indexs(id), keyString TEXT, keyDate INTEGER, keyNumber INTEGER, objectStoreDataId INTEGER NOT NULL UNIQUE REFERENCES ObjectStoreData(id))",
        "DROP INDEX IF EXISTS IndexData_composit",
        "CREATE UNIQUE INDEX IF NOT EXISTS IndexData_composit ON IndexData(keyString, keyDate, keyNumber, indexId)",
        "DROP INDEX IF EXISTS IndexData_objectStoreDataId",
        "CREATE UNIQUE INDEX IF NOT EXISTS IndexData_objectStoreDataId ON IndexData(objectStoreDataId)",
        "DROP INDEX IF EXISTS IndexData_indexId",
        "CREATE UNIQUE INDEX IF NOT EXISTS IndexData_indexId ON IndexData(indexId)"
        };

    for (size_t i = 0; i < arraysize(commands); ++i) {
        if (!sqliteDatabase->executeCommand(commands[i])) {
            // FIXME: We should try to recover from this situation. Maybe nuke the database and start over?
            LOG_ERROR("Failed to run the following command for IndexedDB: %s", commands[i]);
            return false;
        }
    }
    return true;
}

void IDBFactoryBackendImpl::open(const String& name, const String& description, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<SecurityOrigin> securityOrigin, Frame*)
{
    IDBDatabaseBackendMap::iterator it = m_databaseBackendMap.find(name);
    if (it != m_databaseBackendMap.end()) {
        if (!description.isNull())
            it->second->setDescription(description); // The description may have changed.
        callbacks->onSuccess(it->second.get());
        return;
    }

    // FIXME: Everything from now on should be done on another thread.

    OwnPtr<SQLiteDatabase> sqliteDatabase = openSQLiteDatabase(securityOrigin.get(), name);
    if (!sqliteDatabase || !createTables(sqliteDatabase.get())) {
        callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Internal error."));
        return;
    }

    RefPtr<IDBDatabaseBackendImpl> databaseBackend = IDBDatabaseBackendImpl::create(name, description, sqliteDatabase.release(), m_transactionCoordinator.get());
    callbacks->onSuccess(databaseBackend.get());
    m_databaseBackendMap.set(name, databaseBackend.release());
}

void IDBFactoryBackendImpl::abortPendingTransactions(const Vector<int>& pendingIDs)
{
    for (size_t i = 0; i < pendingIDs.size(); ++i)
        m_transactionCoordinator->abort(pendingIDs.at(i));
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

