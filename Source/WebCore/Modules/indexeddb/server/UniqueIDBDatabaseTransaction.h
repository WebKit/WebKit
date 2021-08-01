/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#pragma once

#include "IDBError.h"
#include "IDBTransactionInfo.h"
#include <wtf/Deque.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class IDBCursorInfo;
class IDBDatabaseInfo;
class IDBIndexInfo;
class IDBKeyData;
class IDBObjectStoreInfo;
class IDBRequestData;
class IDBValue;

struct IDBGetAllRecordsData;
struct IDBGetRecordData;
struct IDBIterateCursorData;
struct IDBKeyRangeData;

namespace IDBServer {

class IDBServer;
class UniqueIDBDatabaseConnection;

class UniqueIDBDatabaseTransaction : public RefCounted<UniqueIDBDatabaseTransaction> {
public:
    static Ref<UniqueIDBDatabaseTransaction> create(UniqueIDBDatabaseConnection&, const IDBTransactionInfo&);

    ~UniqueIDBDatabaseTransaction();

    UniqueIDBDatabaseConnection& databaseConnection();
    const IDBTransactionInfo& info() const { return m_transactionInfo; }
    bool isVersionChange() const;
    bool isReadOnly() const;

    IDBDatabaseInfo* originalDatabaseInfo() const;

    void abort();
    void abortWithoutCallback();
    void commit(uint64_t pendingRequestCount);

    void createObjectStore(const IDBRequestData&, const IDBObjectStoreInfo&);
    void deleteObjectStore(const IDBRequestData&, const String& objectStoreName);
    void renameObjectStore(const IDBRequestData&, uint64_t objectStoreIdentifier, const String& newName);
    void clearObjectStore(const IDBRequestData&, uint64_t objectStoreIdentifier);
    void createIndex(const IDBRequestData&, const IDBIndexInfo&);
    void deleteIndex(const IDBRequestData&, uint64_t objectStoreIdentifier, const String& indexName);
    void renameIndex(const IDBRequestData&, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName);
    void putOrAdd(const IDBRequestData&, const IDBKeyData&, const IDBValue&, IndexedDB::ObjectStoreOverwriteMode);
    void getRecord(const IDBRequestData&, const IDBGetRecordData&);
    void getAllRecords(const IDBRequestData&, const IDBGetAllRecordsData&);
    void getCount(const IDBRequestData&, const IDBKeyRangeData&);
    void deleteRecord(const IDBRequestData&, const IDBKeyRangeData&);
    void openCursor(const IDBRequestData&, const IDBCursorInfo&);
    void iterateCursor(const IDBRequestData&, const IDBIterateCursorData&);

    void didActivateInBackingStore(const IDBError&);

    const Vector<uint64_t>& objectStoreIdentifiers();

    void setMainThreadAbortResult(const IDBError& error) { m_mainThreadAbortResult = { error }; }
    const std::optional<IDBError>& mainThreadAbortResult() const { return m_mainThreadAbortResult; }

private:
    UniqueIDBDatabaseTransaction(UniqueIDBDatabaseConnection&, const IDBTransactionInfo&);

    WeakPtr<UniqueIDBDatabaseConnection> m_databaseConnection;
    IDBTransactionInfo m_transactionInfo;

    std::unique_ptr<IDBDatabaseInfo> m_originalDatabaseInfo;

    Vector<uint64_t> m_objectStoreIdentifiers;

    std::optional<IDBError> m_mainThreadAbortResult;
    Deque<IDBError> m_requestResults;
};

} // namespace IDBServer
} // namespace WebCore
