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
#include "IDBSQLiteDatabase.h"
#include "IDBTransactionCoordinator.h"
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

void IDBFactoryBackendImpl::removeIDBDatabaseBackend(const String& uniqueIdentifier)
{
    ASSERT(m_databaseBackendMap.contains(uniqueIdentifier));
    m_databaseBackendMap.remove(uniqueIdentifier);
}

void IDBFactoryBackendImpl::removeSQLiteDatabase(const String& uniqueIdentifier)
{
    ASSERT(m_sqliteDatabaseMap.contains(uniqueIdentifier));
    m_sqliteDatabaseMap.remove(uniqueIdentifier);
}

static PassRefPtr<IDBSQLiteDatabase> openSQLiteDatabase(SecurityOrigin* securityOrigin, const String& pathBase, int64_t maximumSize, const String& fileIdentifier, IDBFactoryBackendImpl* factory)
{
    String path = ":memory:";
    if (!pathBase.isEmpty()) {
        if (!makeAllDirectories(pathBase)) {
            // FIXME: Is there any other thing we could possibly do to recover at this point? If so, do it rather than just erroring out.
            LOG_ERROR("Unabled to create LocalStorage database path %s", pathBase.utf8().data());
            return 0;
        }

        path = pathByAppendingComponent(pathBase, IDBFactoryBackendImpl::databaseFileName(securityOrigin));
    }

    RefPtr<IDBSQLiteDatabase> sqliteDatabase = IDBSQLiteDatabase::create(fileIdentifier, factory);
    if (!sqliteDatabase->db().open(path)) {
        // FIXME: Is there any other thing we could possibly do to recover at this point? If so, do it rather than just erroring out.
        LOG_ERROR("Failed to open database file %s for IndexedDB", path.utf8().data());
        return 0;
    }

    // FIXME: Error checking?
    sqliteDatabase->db().setMaximumSize(maximumSize);
    sqliteDatabase->db().turnOnIncrementalAutoVacuum();

    return sqliteDatabase.release();
}

static bool createTables(SQLiteDatabase& sqliteDatabase)
{
    static const char* commands[] = {
        "CREATE TABLE IF NOT EXISTS Databases (id INTEGER PRIMARY KEY, name TEXT NOT NULL, description TEXT NOT NULL, version TEXT NOT NULL)",
        "CREATE UNIQUE INDEX IF NOT EXISTS Databases_name ON Databases(name)",

        "CREATE TABLE IF NOT EXISTS ObjectStores (id INTEGER PRIMARY KEY, name TEXT NOT NULL, keyPath TEXT, doAutoIncrement INTEGER NOT NULL, databaseId INTEGER NOT NULL REFERENCES Databases(id))",
        "CREATE UNIQUE INDEX IF NOT EXISTS ObjectStores_composit ON ObjectStores(databaseId, name)",

        "CREATE TABLE IF NOT EXISTS Indexes (id INTEGER PRIMARY KEY, objectStoreId INTEGER NOT NULL REFERENCES ObjectStore(id), name TEXT NOT NULL, keyPath TEXT, isUnique INTEGER NOT NULL)",
        "CREATE UNIQUE INDEX IF NOT EXISTS Indexes_composit ON Indexes(objectStoreId, name)",

        "CREATE TABLE IF NOT EXISTS ObjectStoreData (id INTEGER PRIMARY KEY, objectStoreId INTEGER NOT NULL REFERENCES ObjectStore(id), keyString TEXT, keyDate INTEGER, keyNumber INTEGER, value TEXT NOT NULL)",
        "CREATE UNIQUE INDEX IF NOT EXISTS ObjectStoreData_composit ON ObjectStoreData(keyString, keyDate, keyNumber, objectStoreId)",

        "CREATE TABLE IF NOT EXISTS IndexData (id INTEGER PRIMARY KEY, indexId INTEGER NOT NULL REFERENCES Indexes(id), keyString TEXT, keyDate INTEGER, keyNumber INTEGER, objectStoreDataId INTEGER NOT NULL REFERENCES ObjectStoreData(id))",
        "CREATE INDEX IF NOT EXISTS IndexData_composit ON IndexData(keyString, keyDate, keyNumber, indexId)",
        "CREATE INDEX IF NOT EXISTS IndexData_objectStoreDataId ON IndexData(objectStoreDataId)",
        "CREATE INDEX IF NOT EXISTS IndexData_indexId ON IndexData(indexId)"
        };

    for (size_t i = 0; i < arraysize(commands); ++i) {
        if (!sqliteDatabase.executeCommand(commands[i])) {
            // FIXME: We should try to recover from this situation. Maybe nuke the database and start over?
            LOG_ERROR("Failed to run the following command for IndexedDB: %s", commands[i]);
            return false;
        }
    }
    return true;
}

void IDBFactoryBackendImpl::open(const String& name, PassRefPtr<IDBCallbacks> callbacks, PassRefPtr<SecurityOrigin> securityOrigin, Frame*, const String& dataDir, int64_t maximumSize)
{
    String fileIdentifier = securityOrigin->databaseIdentifier();
    String uniqueIdentifier = fileIdentifier + "@" + name;
    IDBDatabaseBackendMap::iterator it = m_databaseBackendMap.find(uniqueIdentifier);
    if (it != m_databaseBackendMap.end()) {
        callbacks->onSuccess(it->second);
        return;
    }

    // FIXME: Everything from now on should be done on another thread.

    RefPtr<IDBSQLiteDatabase> sqliteDatabase;
    SQLiteDatabaseMap::iterator it2 = m_sqliteDatabaseMap.find(fileIdentifier);
    if (it2 != m_sqliteDatabaseMap.end())
        sqliteDatabase = it2->second;
    else {
        sqliteDatabase = openSQLiteDatabase(securityOrigin.get(), dataDir, maximumSize, fileIdentifier, this);

        if (!sqliteDatabase || !createTables(sqliteDatabase->db())) {
            callbacks->onError(IDBDatabaseError::create(IDBDatabaseException::UNKNOWN_ERR, "Internal error."));
            return;
        }
        m_sqliteDatabaseMap.set(fileIdentifier, sqliteDatabase.get());
    }

    RefPtr<IDBDatabaseBackendImpl> databaseBackend = IDBDatabaseBackendImpl::create(name, sqliteDatabase.get(), m_transactionCoordinator.get(), this, uniqueIdentifier);
    callbacks->onSuccess(databaseBackend.get());
    m_databaseBackendMap.set(uniqueIdentifier, databaseBackend.get());
}

String IDBFactoryBackendImpl::databaseFileName(SecurityOrigin* securityOrigin)
{
    String databaseIdentifier = securityOrigin->databaseIdentifier();
    return databaseIdentifier + ".indexeddb";
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

