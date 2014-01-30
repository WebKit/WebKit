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

#ifndef SQLiteIDBTransaction_h
#define SQLiteIDBTransaction_h

#if ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

#include "IDBIdentifier.h"
#include <WebCore/IDBDatabaseBackend.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class SQLiteDatabase;
class SQLiteTransaction;

namespace IndexedDB {
enum class CursorDirection;
enum class CursorType;
enum class TransactionMode;
}

struct IDBKeyRangeData;

}

namespace WebKit {

class SQLiteIDBCursor;
class UniqueIDBDatabaseBackingStoreSQLite;

class SQLiteIDBTransaction {
    WTF_MAKE_NONCOPYABLE(SQLiteIDBTransaction);
public:
    static std::unique_ptr<SQLiteIDBTransaction> create(UniqueIDBDatabaseBackingStoreSQLite& backingStore, const IDBIdentifier& transactionIdentifier, WebCore::IndexedDB::TransactionMode mode)
    {
        return std::unique_ptr<SQLiteIDBTransaction>(new SQLiteIDBTransaction(backingStore, transactionIdentifier, mode));
    }

    ~SQLiteIDBTransaction();

    const IDBIdentifier& transactionIdentifier() const { return m_identifier; }

    bool begin(WebCore::SQLiteDatabase&);
    bool commit();
    bool reset();
    bool rollback();

    SQLiteIDBCursor* openCursor(int64_t objectStoreID, int64_t indexID, WebCore::IndexedDB::CursorDirection, WebCore::IndexedDB::CursorType, WebCore::IDBDatabaseBackend::TaskType, const WebCore::IDBKeyRangeData&);

    WebCore::IndexedDB::TransactionMode mode() const { return m_mode; }
    bool inProgress() const;

    WebCore::SQLiteTransaction* sqliteTransaction() const { return m_sqliteTransaction.get(); }

private:
    SQLiteIDBTransaction(UniqueIDBDatabaseBackingStoreSQLite&, const IDBIdentifier& transactionIdentifier, WebCore::IndexedDB::TransactionMode);

    void clearCursors();

    IDBIdentifier m_identifier;
    WebCore::IndexedDB::TransactionMode m_mode;

    UniqueIDBDatabaseBackingStoreSQLite& m_backingStore;
    std::unique_ptr<WebCore::SQLiteTransaction> m_sqliteTransaction;
    HashMap<IDBIdentifier, std::unique_ptr<SQLiteIDBCursor>> m_cursors;
};

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
#endif // SQLiteIDBTransaction_h
