/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SQLiteIDBBackingStore.h"

#if ENABLE(INDEXED_DATABASE)

#include "FileSystem.h"
#include "IDBDatabaseException.h"
#include "IDBKeyData.h"
#include "Logging.h"
#include "SQLiteDatabase.h"
#include "SQLiteFileSystem.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {
namespace IDBServer {

SQLiteIDBBackingStore::SQLiteIDBBackingStore(const IDBDatabaseIdentifier& identifier, const String& databaseRootDirectory)
    : m_identifier(identifier)
{
    m_absoluteDatabaseDirectory = identifier.databaseDirectoryRelativeToRoot(databaseRootDirectory);
}

SQLiteIDBBackingStore::~SQLiteIDBBackingStore()
{
    if (m_sqliteDB)
        m_sqliteDB->close();
}

const IDBDatabaseInfo& SQLiteIDBBackingStore::getOrEstablishDatabaseInfo()
{
    LOG(IndexedDB, "SQLiteIDBBackingStore::getOrEstablishDatabaseInfo - database %s", m_identifier.databaseName().utf8().data());

    if (m_databaseInfo)
        return *m_databaseInfo;

    m_databaseInfo = std::make_unique<IDBDatabaseInfo>(m_identifier.databaseName(), 0);

    makeAllDirectories(m_absoluteDatabaseDirectory);

    String dbFilename = pathByAppendingComponent(m_absoluteDatabaseDirectory, "IndexedDB.sqlite3");

    m_sqliteDB = std::make_unique<SQLiteDatabase>();
    if (!m_sqliteDB->open(dbFilename)) {
        LOG_ERROR("Failed to open SQLite database at path '%s'", dbFilename.utf8().data());
        m_sqliteDB = nullptr;
    }

    if (!m_sqliteDB)
        return *m_databaseInfo;

    // FIXME: Support populating new SQLite files and pulling DatabaseInfo from existing SQLite files.
    // Doing so will make a new m_databaseInfo which overrides the default one we created up above.

    return *m_databaseInfo;
}

IDBError SQLiteIDBBackingStore::beginTransaction(const IDBTransactionInfo&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::abortTransaction(const IDBResourceIdentifier&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::commitTransaction(const IDBResourceIdentifier&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::createObjectStore(const IDBResourceIdentifier&, const IDBObjectStoreInfo&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::deleteObjectStore(const IDBResourceIdentifier&, const String&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::clearObjectStore(const IDBResourceIdentifier&, uint64_t)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::createIndex(const IDBResourceIdentifier&, const IDBIndexInfo&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::deleteIndex(const IDBResourceIdentifier&, uint64_t, const String&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::keyExistsInObjectStore(const IDBResourceIdentifier&, uint64_t, const IDBKeyData&, bool&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::deleteRange(const IDBResourceIdentifier&, uint64_t, const IDBKeyRangeData&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::addRecord(const IDBResourceIdentifier&, uint64_t, const IDBKeyData&, const ThreadSafeDataBuffer&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::getRecord(const IDBResourceIdentifier&, uint64_t, const IDBKeyRangeData&, ThreadSafeDataBuffer&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::getIndexRecord(const IDBResourceIdentifier&, uint64_t, uint64_t, IndexedDB::IndexRecordType, const IDBKeyRangeData&, IDBGetResult&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::getCount(const IDBResourceIdentifier&, uint64_t, uint64_t, const IDBKeyRangeData&, uint64_t&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::generateKeyNumber(const IDBResourceIdentifier&, uint64_t, uint64_t&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::revertGeneratedKeyNumber(const IDBResourceIdentifier&, uint64_t, uint64_t)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::maybeUpdateKeyGeneratorNumber(const IDBResourceIdentifier&, uint64_t, double)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::openCursor(const IDBResourceIdentifier&, const IDBCursorInfo&, IDBGetResult&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

IDBError SQLiteIDBBackingStore::iterateCursor(const IDBResourceIdentifier&, const IDBResourceIdentifier&, const IDBKeyData&, uint32_t, IDBGetResult&)
{
    return { IDBDatabaseException::UnknownError, ASCIILiteral("Not implemented") };
}

void SQLiteIDBBackingStore::deleteBackingStore()
{
    String dbFilename = pathByAppendingComponent(m_absoluteDatabaseDirectory, "IndexedDB.sqlite3");

    LOG(IndexedDB, "SQLiteIDBBackingStore::deleteBackingStore deleting file '%s' on disk", dbFilename.utf8().data());

    if (m_sqliteDB) {
        m_sqliteDB->close();
        m_sqliteDB = nullptr;
    }

    SQLiteFileSystem::deleteDatabaseFile(dbFilename);
    SQLiteFileSystem::deleteEmptyDatabaseDirectory(m_absoluteDatabaseDirectory);
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
