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

    if (!cursor->advance(1))
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
    , m_errored(false)
{
    ASSERT(m_objectStoreID);
}

static const String& getIndexStatement(bool hasLowerKey, bool isLowerOpen, bool hasUpperKey, bool isUpperOpen, bool descending)
{
    DEFINE_STATIC_LOCAL(Vector<String>, indexStatements, ());

    if (indexStatements.isEmpty()) {
        indexStatements.reserveCapacity(18);

        // No lower key statements (6)
        indexStatements.append(ASCIILiteral("SELECT key, value FROM IndexRecords WHERE indexID = ? ORDER BY key;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM IndexRecords WHERE indexID = ? ORDER BY key DESC;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM IndexRecords WHERE indexID = ? AND key <= CAST(? AS TEXT) ORDER BY key;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM IndexRecords WHERE indexID = ? AND key <= CAST(? AS TEXT) ORDER BY key DESC;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM IndexRecords WHERE indexID = ? AND key < CAST(? AS TEXT) ORDER BY key;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM IndexRecords WHERE indexID = ? AND key < CAST(? AS TEXT) ORDER BY key DESC;"));

        // Closed lower key statements (6)
        indexStatements.append(ASCIILiteral("SELECT key, value FROM IndexRecords WHERE indexID = ? AND key >= CAST(? AS TEXT) ORDER BY key;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM IndexRecords WHERE indexID = ? AND key >= CAST(? AS TEXT) ORDER BY key DESC;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM IndexRecords WHERE indexID = ? AND key >= CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM IndexRecords WHERE indexID = ? AND key >= CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key DESC;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM IndexRecords WHERE indexID = ? AND key >= CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM IndexRecords WHERE indexID = ? AND key >= CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key DESC;"));

        // Open lower key statements (6)
        indexStatements.append(ASCIILiteral("SELECT key, value FROM IndexRecords WHERE indexID = ? AND key > CAST(? AS TEXT) ORDER BY key;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM IndexRecords WHERE indexID = ? AND key > CAST(? AS TEXT) ORDER BY key DESC;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM IndexRecords WHERE indexID = ? AND key > CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM IndexRecords WHERE indexID = ? AND key > CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key DESC;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM IndexRecords WHERE indexID = ? AND key > CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM IndexRecords WHERE indexID = ? AND key > CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key DESC;"));
    }

    size_t i = 0;

    if (hasLowerKey) {
        i += 6;
        if (isLowerOpen)
            i += 6;
    }

    if (hasUpperKey) {
        i += 2;
        if (isUpperOpen)
            i += 2;
    }

    if (descending)
        i += 1;

    return indexStatements[i];
}

static const String& getObjectStoreStatement(bool hasLowerKey, bool isLowerOpen, bool hasUpperKey, bool isUpperOpen, bool descending)
{
    DEFINE_STATIC_LOCAL(Vector<String>, indexStatements, ());

    if (indexStatements.isEmpty()) {
        indexStatements.reserveCapacity(18);

        // No lower key statements (6)
        indexStatements.append(ASCIILiteral("SELECT key, value FROM Records WHERE objectStoreID = ? ORDER BY key;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM Records WHERE objectStoreID = ? ORDER BY key DESC;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM Records WHERE objectStoreID = ? AND key <= CAST(? AS TEXT) ORDER BY key;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM Records WHERE objectStoreID = ? AND key <= CAST(? AS TEXT) ORDER BY key DESC;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM Records WHERE objectStoreID = ? AND key < CAST(? AS TEXT) ORDER BY key;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM Records WHERE objectStoreID = ? AND key < CAST(? AS TEXT) ORDER BY key DESC;"));

        // Closed lower key statements (6)
        indexStatements.append(ASCIILiteral("SELECT key, value FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) ORDER BY key;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) ORDER BY key DESC;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key DESC;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM Records WHERE objectStoreID = ? AND key >= CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key DESC;"));

        // Open lower key statements (6)
        indexStatements.append(ASCIILiteral("SELECT key, value FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) ORDER BY key;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) ORDER BY key DESC;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) AND key <= CAST(? AS TEXT) ORDER BY key DESC;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key;"));
        indexStatements.append(ASCIILiteral("SELECT key, value FROM Records WHERE objectStoreID = ? AND key > CAST(? AS TEXT) AND key < CAST(? AS TEXT) ORDER BY key DESC;"));
    }

    size_t i = 0;

    if (hasLowerKey) {
        i += 6;
        if (isLowerOpen)
            i += 6;
    }

    if (hasUpperKey) {
        i += 2;
        if (isUpperOpen)
            i += 2;
    }

    if (descending)
        i += 1;

    return indexStatements[i];
}

bool SQLiteIDBCursor::establishStatement()
{
    String sql;
    int64_t id;

    if (m_indexID != IDBIndexMetadata::InvalidId) {
        sql = getIndexStatement(!m_keyRange.lowerKey.isNull, m_keyRange.lowerOpen, !m_keyRange.upperKey.isNull, m_keyRange.upperOpen, m_cursorDirection == IndexedDB::CursorDirection::Prev || m_cursorDirection == IndexedDB::CursorDirection::PrevNoDuplicate);
        id = m_indexID;
    } else {
        sql = getObjectStoreStatement(!m_keyRange.lowerKey.isNull, m_keyRange.lowerOpen, !m_keyRange.upperKey.isNull, m_keyRange.upperOpen, m_cursorDirection == IndexedDB::CursorDirection::Prev || m_cursorDirection == IndexedDB::CursorDirection::PrevNoDuplicate);
        id = m_objectStoreID;
    }

    return createSQLiteStatement(sql, id);
}

bool SQLiteIDBCursor::createSQLiteStatement(const String& sql, int64_t idToBind)
{
    ASSERT(m_transaction->sqliteTransaction());
    SQLiteDatabase& database = m_transaction->sqliteTransaction()->database();

    LOG(IDB, "Creating cursor with SQL query: \"%s\"", sql.utf8().data());

    m_statement = std::make_unique<SQLiteStatement>(database, sql);

    if (m_statement->prepare() != SQLResultOk
        || m_statement->bindInt64(1, idToBind) != SQLResultOk) {
        LOG_ERROR("Could not create cursor statement");
        return false;
    }

    int nextBindArgument = 2;

    if (!m_keyRange.lowerKey.isNull) {
        RefPtr<SharedBuffer> buffer = serializeIDBKeyData(m_keyRange.lowerKey);
        if (m_statement->bindBlob(nextBindArgument++, buffer->data(), buffer->size()) != SQLResultOk) {
            LOG_ERROR("Could not create cursor statement");
            return false;
        }
    }
    if (!m_keyRange.upperKey.isNull) {
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
    bool isUnique = m_cursorDirection == IndexedDB::CursorDirection::NextNoDuplicate || m_cursorDirection == IndexedDB::CursorDirection::PrevNoDuplicate;

    for (uint64_t i = 0; i < count; ++i) {
        if (!isUnique) {
            if (!advanceOnce())
                return false;
            continue;
        }

        if (!advanceUnique())
            return false;
    }

    return true;
}

bool SQLiteIDBCursor::advanceUnique()
{
    IDBKeyData currentKey = m_currentKey;

    while (!m_completed) {
        if (!advanceOnce())
            return false;

        // If the new current key is different from the old current key, we're done.
        if (currentKey.compare(m_currentKey))
            return true;
    }

    return false;
}


bool SQLiteIDBCursor::advanceOnce()
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

        // When a cursor reaches its end, that is indicated by having undefined keys/values
        m_currentKey = IDBKeyData();
        m_currentPrimaryKey = IDBKeyData();
        m_currentValueBuffer.clear();

        return true;
    }

    if (result != SQLResultRow) {
        LOG_ERROR("Error advancing cursor - (%i) %s", result, m_transaction->sqliteTransaction()->database().lastErrorMsg());
        m_completed = true;
        m_errored = true;
        return false;
    }

    Vector<uint8_t> keyData;
    m_statement->getColumnBlobAsVector(0, keyData);

    if (!deserializeIDBKeyData(keyData.data(), keyData.size(), m_currentKey)) {
        LOG_ERROR("Unable to deserialize key data from database while advancing cursor");
        m_completed = true;
        m_errored = true;
        return false;
    }

    m_statement->getColumnBlobAsVector(1, keyData);
    m_currentValueBuffer = keyData;

    if (m_indexID != IDBIndexMetadata::InvalidId) {
        if (!deserializeIDBKeyData(keyData.data(), keyData.size(), m_currentPrimaryKey)) {
            LOG_ERROR("Unable to deserialize value data from database while advancing index cursor");
            m_completed = true;
            m_errored = true;
            return false;
        }

        SQLiteStatement objectStoreStatement(*m_statement->database(), "SELECT value FROM Records WHERE key = CAST(? AS TEXT) and objectStoreID = ?;");

        if (objectStoreStatement.prepare() != SQLResultOk
            || objectStoreStatement.bindBlob(1, m_currentValueBuffer.data(), m_currentValueBuffer.size()) != SQLResultOk
            || objectStoreStatement.bindInt64(2, m_objectStoreID) != SQLResultOk
            || objectStoreStatement.step() != SQLResultRow) {
            LOG_ERROR("Could not create index cursor statement into object store records");
            m_completed = true;
            m_errored = true;
            return false;
        }

        objectStoreStatement.getColumnBlobAsVector(0, m_currentValueBuffer);
    }

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
