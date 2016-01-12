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

#include "IDBDatabaseException.h"
#include "NotImplemented.h"

namespace WebCore {
namespace IDBServer {

SQLiteIDBBackingStore::SQLiteIDBBackingStore(const IDBDatabaseIdentifier& identifier)
    : m_identifier(identifier)
{
}

SQLiteIDBBackingStore::~SQLiteIDBBackingStore()
{
}

const IDBDatabaseInfo& SQLiteIDBBackingStore::getOrEstablishDatabaseInfo()
{
    if (!m_databaseInfo)
        m_databaseInfo = std::make_unique<IDBDatabaseInfo>(m_identifier.databaseName(), 0);

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
    notImplemented();
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
