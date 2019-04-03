/*
 * Copyright (C) 2014, 2016 Apple Inc. All rights reserved.
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

#if ENABLE(INDEXED_DATABASE)

#include "IDBCursorRecord.h"
#include "IDBIndexInfo.h"
#include "IDBKeyData.h"
#include "IDBKeyRangeData.h"
#include "IDBResourceIdentifier.h"
#include "IDBValue.h"
#include "SQLiteStatement.h"
#include <wtf/Deque.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class IDBCursorInfo;
class IDBGetResult;

namespace IDBServer {

enum class ShouldFetchForSameKey : bool { No, Yes };

class SQLiteIDBTransaction;

class SQLiteIDBCursor {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(SQLiteIDBCursor);
public:
    static std::unique_ptr<SQLiteIDBCursor> maybeCreate(SQLiteIDBTransaction&, const IDBCursorInfo&);
    static std::unique_ptr<SQLiteIDBCursor> maybeCreateBackingStoreCursor(SQLiteIDBTransaction&, const uint64_t objectStoreIdentifier, const uint64_t indexIdentifier, const IDBKeyRangeData&);

    SQLiteIDBCursor(SQLiteIDBTransaction&, const IDBCursorInfo&);
    SQLiteIDBCursor(SQLiteIDBTransaction&, uint64_t objectStoreID, uint64_t indexID, const IDBKeyRangeData&);

    ~SQLiteIDBCursor();

    const IDBResourceIdentifier& identifier() const { return m_cursorIdentifier; }
    SQLiteIDBTransaction* transaction() const { return m_transaction; }

    int64_t objectStoreID() const { return m_objectStoreID; }
    int64_t currentRecordRowID() const;

    const IDBKeyData& currentKey() const;
    const IDBKeyData& currentPrimaryKey() const;
    IDBValue* currentValue() const;

    bool advance(uint64_t count);
    bool iterate(const IDBKeyData& targetKey, const IDBKeyData& targetPrimaryKey);
    bool prefetch();

    bool didComplete() const;
    bool didError() const;

    void objectStoreRecordsChanged();

    void currentData(IDBGetResult&, const Optional<IDBKeyPath>&);

private:
    bool establishStatement();
    bool createSQLiteStatement(const String& sql);
    bool bindArguments();

    void resetAndRebindStatement();

    enum class FetchResult {
        Success,
        Failure,
        ShouldFetchAgain
    };

    bool fetch(ShouldFetchForSameKey = ShouldFetchForSameKey::No);

    struct SQLiteCursorRecord {
        IDBCursorRecord record;
        bool completed { false };
        bool errored { false };
        int64_t rowID { 0 };
        bool isTerminalRecord() const { return completed || errored; }
    };
    bool fetchNextRecord(SQLiteCursorRecord&);
    FetchResult internalFetchNextRecord(SQLiteCursorRecord&);

    void markAsErrored(SQLiteCursorRecord&);

    SQLiteIDBTransaction* m_transaction;
    IDBResourceIdentifier m_cursorIdentifier;
    int64_t m_objectStoreID;
    int64_t m_indexID { IDBIndexInfo::InvalidId };
    IndexedDB::CursorDirection m_cursorDirection { IndexedDB::CursorDirection::Next };
    IndexedDB::CursorType m_cursorType;
    IDBKeyRangeData m_keyRange;

    IDBKeyData m_currentLowerKey;
    IDBKeyData m_currentUpperKey;

    Deque<SQLiteCursorRecord> m_fetchedRecords;
    IDBKeyData m_currentKeyForUniqueness;

    std::unique_ptr<SQLiteStatement> m_statement;
    bool m_statementNeedsReset { true };
    int64_t m_boundID { 0 };

    bool m_backingStoreCursor { false };
};

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
