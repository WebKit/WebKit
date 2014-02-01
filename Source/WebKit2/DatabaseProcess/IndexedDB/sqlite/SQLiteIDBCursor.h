/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

#include "IDBIdentifier.h"
#include <WebCore/IDBDatabaseBackend.h>
#include <WebCore/IDBKeyData.h>
#include <WebCore/IDBKeyRangeData.h>
#include <WebCore/SQLiteStatement.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

namespace IndexedDB {
enum class CursorDirection;
enum class CursorType;
}

}

namespace WebKit {

class SQLiteIDBTransaction;

class SQLiteIDBCursor {
    WTF_MAKE_NONCOPYABLE(SQLiteIDBCursor);

public:
    static std::unique_ptr<SQLiteIDBCursor> maybeCreate(SQLiteIDBTransaction*, const IDBIdentifier& cursorIdentifier, int64_t objectStoreID, int64_t indexID, WebCore::IndexedDB::CursorDirection, WebCore::IndexedDB::CursorType, WebCore::IDBDatabaseBackend::TaskType, const WebCore::IDBKeyRangeData&);

    const IDBIdentifier& identifier() const { return m_cursorIdentifier; }
    SQLiteIDBTransaction* transaction() const { return m_transaction; }

    const WebCore::IDBKeyData& currentKey() const { return m_currentKey; }
    const WebCore::IDBKeyData& currentPrimaryKey() const { return m_currentPrimaryKey; }
    const Vector<char>& currentValueBuffer() const { return m_currentValueBuffer; }
    const WebCore::IDBKeyData& currentValueKey() const { return m_currentValueKey; }

    bool advance(uint64_t count);
    bool iterate(const WebCore::IDBKeyData& targetKey);

    bool didError() const { return m_errored; }

private:
    SQLiteIDBCursor(SQLiteIDBTransaction*, const IDBIdentifier& cursorIdentifier, int64_t objectStoreID, int64_t indexID, WebCore::IndexedDB::CursorDirection, WebCore::IndexedDB::CursorType, WebCore::IDBDatabaseBackend::TaskType, const WebCore::IDBKeyRangeData&);

    bool establishStatement();
    bool createSQLiteStatement(const String& sql, int64_t idToBind);

    bool advanceOnce();
    bool advanceUnique();

    SQLiteIDBTransaction* m_transaction;
    IDBIdentifier m_cursorIdentifier;
    int64_t m_objectStoreID;
    int64_t m_indexID;
    WebCore::IndexedDB::CursorDirection m_cursorDirection;
    WebCore::IDBKeyRangeData m_keyRange;

    WebCore::IDBKeyData m_currentKey;
    WebCore::IDBKeyData m_currentPrimaryKey;
    Vector<char> m_currentValueBuffer;
    WebCore::IDBKeyData m_currentValueKey;

    std::unique_ptr<WebCore::SQLiteStatement> m_statement;

    bool m_completed;
    bool m_errored;
};

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
#endif // SQLiteIDBCursor_h
