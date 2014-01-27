/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef IDBServerConnection_h
#define IDBServerConnection_h

#include "IDBCursorBackendOperations.h"
#include "IDBDatabaseMetadata.h"
#include "IDBGetResult.h"
#include "IDBTransactionBackendOperations.h"
#include "IndexedDB.h"
#include <functional>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBDatabaseError;
class IDBKey;
class IDBTransactionBackend;

struct IDBOpenCursorResult;
struct IDBIndexMetadata;
struct IDBObjectStoreMetadata;

// This interface provides a single asynchronous layer between the web-facing frontend
// and the I/O performing backend of IndexedDatabase.
// If an operation's completion needs to be confirmed that must be done through use of a callback function.
class IDBServerConnection : public RefCounted<IDBServerConnection> {
public:
    virtual ~IDBServerConnection() { }

    virtual bool isClosed() = 0;

    typedef std::function<void (bool success)> BoolCallbackFunction;

    // Factory-level operations
    virtual void deleteDatabase(const String& name, BoolCallbackFunction successCallback) = 0;

    // Database-level operations
    typedef std::function<void (const IDBDatabaseMetadata&, bool success)> GetIDBDatabaseMetadataFunction;
    virtual void getOrEstablishIDBDatabaseMetadata(GetIDBDatabaseMetadataFunction) = 0;
    virtual void close() = 0;

    // Transaction-level operations
    virtual void openTransaction(int64_t transactionID, const HashSet<int64_t>& objectStoreIds, IndexedDB::TransactionMode, BoolCallbackFunction successCallback) = 0;
    virtual void beginTransaction(int64_t transactionID, std::function<void()> completionCallback) = 0;
    virtual void commitTransaction(int64_t transactionID, BoolCallbackFunction successCallback) = 0;
    virtual void resetTransaction(int64_t transactionID, std::function<void()> completionCallback) = 0;
    virtual void rollbackTransaction(int64_t transactionID, std::function<void()> completionCallback) = 0;

    virtual void setIndexKeys(int64_t transactionID, int64_t databaseID, int64_t objectStoreID, const IDBObjectStoreMetadata&, IDBKey& primaryKey, const Vector<int64_t>& indexIDs, const Vector<Vector<RefPtr<IDBKey>>>& indexKeys, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) = 0;

    virtual void createObjectStore(IDBTransactionBackend&, const CreateObjectStoreOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) = 0;
    virtual void createIndex(IDBTransactionBackend&, const CreateIndexOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) = 0;
    virtual void deleteIndex(IDBTransactionBackend&, const DeleteIndexOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) = 0;
    virtual void get(IDBTransactionBackend&, const GetOperation&, std::function<void(const IDBGetResult&, PassRefPtr<IDBDatabaseError>)> completionCallback) = 0;
    virtual void put(IDBTransactionBackend&, const PutOperation&, std::function<void(PassRefPtr<IDBKey>, PassRefPtr<IDBDatabaseError>)> completionCallback) = 0;
    virtual void openCursor(IDBTransactionBackend&, const OpenCursorOperation&, std::function<void(int64_t, PassRefPtr<IDBDatabaseError>)> completionCallback) = 0;
    virtual void count(IDBTransactionBackend&, const CountOperation&, std::function<void(int64_t, PassRefPtr<IDBDatabaseError>)> completionCallback) = 0;
    virtual void deleteRange(IDBTransactionBackend&, const DeleteRangeOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) = 0;
    virtual void clearObjectStore(IDBTransactionBackend&, const ClearObjectStoreOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) = 0;
    virtual void deleteObjectStore(IDBTransactionBackend&, const DeleteObjectStoreOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) = 0;
    virtual void changeDatabaseVersion(IDBTransactionBackend&, const IDBDatabaseBackend::VersionChangeOperation&, std::function<void(PassRefPtr<IDBDatabaseError>)> completionCallback) = 0;

    // Cursor-level operations
    virtual void cursorAdvance(IDBCursorBackend&, const CursorAdvanceOperation&, std::function<void()> completionCallback) = 0;
    virtual void cursorIterate(IDBCursorBackend&, const CursorIterationOperation&, std::function<void()> completionCallback) = 0;
    virtual void cursorPrefetchIteration(IDBCursorBackend&, const CursorPrefetchIterationOperation&, std::function<void()> completionCallback) = 0;
    virtual void cursorPrefetchReset(IDBCursorBackend&, int usedPrefetches) = 0;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBServerConnection_h
