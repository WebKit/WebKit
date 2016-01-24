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
#ifndef SQLiteIDBCursor_h
#define SQLiteIDBCursor_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBDatabaseBackend.h"
#include "IDBKeyData.h"
#include "IDBKeyRangeData.h"
#include "IDBResourceIdentifier.h"
#include "SQLiteStatement.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class IDBCursorInfo;

namespace IDBServer {

class SQLiteIDBTransaction;

class SQLiteIDBCursor {
    WTF_MAKE_NONCOPYABLE(SQLiteIDBCursor);
public:
    static std::unique_ptr<SQLiteIDBCursor> maybeCreate(SQLiteIDBTransaction&, const IDBCursorInfo&);
    static std::unique_ptr<SQLiteIDBCursor> maybeCreateBackingStoreCursor(SQLiteIDBTransaction&, const uint64_t objectStoreIdentifier);

    SQLiteIDBCursor(SQLiteIDBTransaction&, const IDBCursorInfo&);
    SQLiteIDBCursor(SQLiteIDBTransaction&, uint64_t objectStoreID);

    const IDBResourceIdentifier& identifier() const { return m_cursorIdentifier; }
    SQLiteIDBTransaction* transaction() const { return m_transaction; }

    int64_t objectStoreID() const { return m_objectStoreID; }

    const IDBKeyData& currentKey() const { return m_currentKey; }
    const IDBKeyData& currentPrimaryKey() const { return m_currentPrimaryKey; }
    const Vector<uint8_t>& currentValueBuffer() const { return m_currentValueBuffer; }

    bool advance(uint64_t count);
    bool iterate(const IDBKeyData& targetKey);

    bool didError() const { return m_errored; }

    void objectStoreRecordsChanged();

private:
    bool establishStatement();
    bool createSQLiteStatement(const String& sql);
    bool bindArguments();

    void resetAndRebindStatement();

    enum class AdvanceResult {
        Success,
        Failure,
        ShouldAdvanceAgain
    };

    AdvanceResult internalAdvanceOnce();
    bool advanceOnce();
    bool advanceUnique();

    SQLiteIDBTransaction* m_transaction;
    IDBResourceIdentifier m_cursorIdentifier;
    int64_t m_objectStoreID;
    int64_t m_indexID { IDBIndexMetadata::InvalidId };
    IndexedDB::CursorDirection m_cursorDirection { IndexedDB::CursorDirection::Next };
    IDBKeyRangeData m_keyRange;

    IDBKeyData m_currentLowerKey;
    IDBKeyData m_currentUpperKey;

    int64_t m_currentRecordID { -1 };
    IDBKeyData m_currentKey;
    IDBKeyData m_currentPrimaryKey;
    Vector<uint8_t> m_currentValueBuffer;

    std::unique_ptr<SQLiteStatement> m_statement;
    bool m_statementNeedsReset { false };
    int64_t m_boundID { 0 };

    bool m_completed { false };
    bool m_errored { false };
};


} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // SQLiteIDBCursor_h
