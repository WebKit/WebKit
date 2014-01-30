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
#include "config.h"
#include "SQLiteIDBCursor.h"

#if ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

#include "IDBSerialization.h"
#include "Logging.h"
#include "SQLiteIDBTransaction.h"
#include <WebCore/SQLiteStatement.h>
#include <WebCore/SQLiteTransaction.h>
#include <sqlite3.h>

using namespace WebCore;

namespace WebKit {

std::unique_ptr<SQLiteIDBCursor> SQLiteIDBCursor::maybeCreate(SQLiteIDBTransaction* transaction, const IDBIdentifier& cursorIdentifier, int64_t objectStoreID, int64_t indexID, IndexedDB::CursorDirection cursorDirection, IndexedDB::CursorType cursorType, IDBDatabaseBackend::TaskType taskType, const IDBKeyRangeData& keyRange)
{
    auto cursor = std::unique_ptr<SQLiteIDBCursor>(new SQLiteIDBCursor(transaction, cursorIdentifier, objectStoreID, indexID, cursorDirection, cursorType, taskType, keyRange));

    if (!cursor->establishStatement())
        return nullptr;

    return cursor;
}

SQLiteIDBCursor::SQLiteIDBCursor(SQLiteIDBTransaction* transaction, const IDBIdentifier& cursorIdentifier, int64_t objectStoreID, int64_t indexID, IndexedDB::CursorDirection cursorDirection, IndexedDB::CursorType, IDBDatabaseBackend::TaskType, const IDBKeyRangeData& keyRange)
    : m_transaction(transaction)
    , m_cursorIdentifier(cursorIdentifier)
    , m_objectStoreID(objectStoreID)
    , m_indexID(indexID)
    , m_cursorDirection(cursorDirection)
    , m_keyRange(keyRange)
    , m_completed(false)
{
    ASSERT(m_objectStoreID);
}

bool SQLiteIDBCursor::establishStatement()
{
    if (m_indexID != IDBIndexMetadata::InvalidId)
        return createIndexCursorStatement();

    return createObjectStoreCursorStatement();
}

bool SQLiteIDBCursor::createIndexCursorStatement()
{
    LOG_ERROR("Index cursor not yet supported (index id is %lli)", m_indexID);
    return false;
}

bool SQLiteIDBCursor::createObjectStoreCursorStatement()
{
    ASSERT(m_transaction->sqliteTransaction());
    SQLiteDatabase& database = m_transaction->sqliteTransaction()->database();

    String lowerSQL;
    if (!m_keyRange.lowerKey.isNull)
        lowerSQL = m_keyRange.lowerOpen ? ASCIILiteral("AND key >= CAST(? AS TEXT)") : ASCIILiteral("AND key > CAST(? AS TEXT)");

    String upperSQL;
    if (!m_keyRange.upperKey.isNull)
        lowerSQL = m_keyRange.upperOpen ? ASCIILiteral("AND key <= CAST(? AS TEXT)") : ASCIILiteral("AND key < CAST(? AS TEXT)");

    String orderSQL;
    if (m_cursorDirection == IndexedDB::CursorDirection::Next || m_cursorDirection == IndexedDB::CursorDirection::NextNoDuplicate)
        orderSQL = ASCIILiteral(" ORDER BY key;");
    else
        orderSQL = ASCIILiteral(" ORDER BY key DESC;");

    String sql = ASCIILiteral("SELECT key, value FROM Records WHERE objectStoreID = ?") + lowerSQL + upperSQL + orderSQL;

    LOG(IDB, "Creating cursor with SQL query: \"%s\"", sql.utf8().data());

    m_statement = std::make_unique<SQLiteStatement>(database, sql);

    if (m_statement->prepare() != SQLResultOk
        || m_statement->bindInt64(1, m_objectStoreID) != SQLResultOk) {
        LOG_ERROR("Could not create cursor statement");
        return false;
    }

    int nextBindArgument = 2;

    if (!lowerSQL.isEmpty()) {
        RefPtr<SharedBuffer> buffer = serializeIDBKeyData(m_keyRange.lowerKey);
        if (m_statement->bindBlob(nextBindArgument++, buffer->data(), buffer->size()) != SQLResultOk) {
            LOG_ERROR("Could not create cursor statement");
            return false;
        }
    }
    if (!upperSQL.isEmpty()) {
        RefPtr<SharedBuffer> buffer = serializeIDBKeyData(m_keyRange.upperKey);
        if (m_statement->bindBlob(nextBindArgument, buffer->data(), buffer->size()) != SQLResultOk) {
            LOG_ERROR("Could not create cursor statement");
            return false;
        }
    }

    return true;
}

bool SQLiteIDBCursor::advance(uint64_t count)
{
    ASSERT(m_transaction->sqliteTransaction());
    ASSERT(m_statement);

    if (m_completed) {
        LOG_ERROR("Attempt to advance a completed cursor");
        return false;
    }

    int result = m_statement->step();
    if (result == SQLResultDone) {
        m_completed = true;
        return true;
    }

    if (result != SQLResultRow) {
        LOG_ERROR("Error advancing cursor - (%i) %s", result, m_transaction->sqliteTransaction()->database().lastErrorMsg());
        m_completed = true;
        return false;
    }

    Vector<char> keyData;
    m_statement->getColumnBlobAsVector(0, keyData);

    IDBKeyData key;
    if (!deserializeIDBKeyData(reinterpret_cast<const uint8_t*>(keyData.data()), keyData.size(), key)) {
        LOG_ERROR("Unable to deserialize key data from database while advancing cursor");
        m_completed = true;
        return false;
    }

    m_statement->getColumnBlobAsVector(1, keyData);

    m_currentKey = key;
    m_currentPrimaryKey = key;
    m_currentValue = keyData;

    return true;
}

bool SQLiteIDBCursor::iterate(const WebCore::IDBKeyData& targetKey)
{
    ASSERT(m_transaction->sqliteTransaction());
    ASSERT(m_statement);

    bool result = advance(1);

    // Iterating with no key is equivalent to advancing 1 step.
    if (targetKey.isNull || !result)
        return result;

    while (!m_completed && m_currentKey.compare(targetKey)) {
        result = advance(1);
        if (!result)
            return false;
    }

    return result;
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
