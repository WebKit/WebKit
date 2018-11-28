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

#include "config.h"
#include "SQLiteIDBCursor.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBCursorInfo.h"
#include "IDBGetResult.h"
#include "IDBSerialization.h"
#include "Logging.h"
#include "SQLiteIDBBackingStore.h"
#include "SQLiteIDBTransaction.h"
#include "SQLiteStatement.h"
#include "SQLiteTransaction.h"
#include <sqlite3.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace IDBServer {

static const size_t prefetchLimit = 8;

std::unique_ptr<SQLiteIDBCursor> SQLiteIDBCursor::maybeCreate(SQLiteIDBTransaction& transaction, const IDBCursorInfo& info)
{
    auto cursor = std::make_unique<SQLiteIDBCursor>(transaction, info);

    if (!cursor->establishStatement())
        return nullptr;

    if (!cursor->advance(1))
        return nullptr;

    return cursor;
}

std::unique_ptr<SQLiteIDBCursor> SQLiteIDBCursor::maybeCreateBackingStoreCursor(SQLiteIDBTransaction& transaction, const uint64_t objectStoreID, const uint64_t indexID, const IDBKeyRangeData& range)
{
    auto cursor = std::make_unique<SQLiteIDBCursor>(transaction, objectStoreID, indexID, range);

    if (!cursor->establishStatement())
        return nullptr;

    if (!cursor->advance(1))
        return nullptr;

    return cursor;
}

SQLiteIDBCursor::SQLiteIDBCursor(SQLiteIDBTransaction& transaction, const IDBCursorInfo& info)
    : m_transaction(&transaction)
    , m_cursorIdentifier(info.identifier())
    , m_objectStoreID(info.objectStoreIdentifier())
    , m_indexID(info.cursorSource() == IndexedDB::CursorSource::Index ? info.sourceIdentifier() : IDBIndexInfo::InvalidId)
    , m_cursorDirection(info.cursorDirection())
    , m_cursorType(info.cursorType())
    , m_keyRange(info.range())
{
    ASSERT(m_objectStoreID);
}

SQLiteIDBCursor::SQLiteIDBCursor(SQLiteIDBTransaction& transaction, const uint64_t objectStoreID, const uint64_t indexID, const IDBKeyRangeData& range)
    : m_transaction(&transaction)
    , m_cursorIdentifier(transaction.transactionIdentifier())
    , m_objectStoreID(objectStoreID)
    , m_indexID(indexID ? indexID : IDBIndexInfo::InvalidId)
    , m_cursorDirection(IndexedDB::CursorDirection::Next)
    , m_cursorType(IndexedDB::CursorType::KeyAndValue)
    , m_keyRange(range)
    , m_backingStoreCursor(true)
{
    ASSERT(m_objectStoreID);
}

SQLiteIDBCursor::~SQLiteIDBCursor()
{
    if (m_backingStoreCursor)
        m_transaction->closeCursor(*this);
}

void SQLiteIDBCursor::currentData(IDBGetResult& result)
{
    ASSERT(!m_fetchedRecords.isEmpty());

    auto& currentRecord = m_fetchedRecords.first();
    if (currentRecord.completed) {
        ASSERT(!currentRecord.errored);
        result = { };
        return;
    }

    result = { currentRecord.record.key, currentRecord.record.primaryKey, currentRecord.record.value ? *currentRecord.record.value : IDBValue() };
}

static String buildIndexStatement(const IDBKeyRangeData& keyRange, IndexedDB::CursorDirection cursorDirection)
{
    StringBuilder builder;

    builder.appendLiteral("SELECT rowid, key, value FROM IndexRecords WHERE indexID = ? AND objectStoreID = ? AND key ");
    if (!keyRange.lowerKey.isNull() && !keyRange.lowerOpen)
        builder.appendLiteral(">=");
    else
        builder.append('>');

    builder.appendLiteral(" CAST(? AS TEXT) AND key ");
    if (!keyRange.upperKey.isNull() && !keyRange.upperOpen)
        builder.appendLiteral("<=");
    else
        builder.append('<');

    builder.appendLiteral(" CAST(? AS TEXT) ORDER BY key");
    if (cursorDirection == IndexedDB::CursorDirection::Prev || cursorDirection == IndexedDB::CursorDirection::Prevunique)
        builder.appendLiteral(" DESC");

    builder.appendLiteral(", value");
    if (cursorDirection == IndexedDB::CursorDirection::Prev)
        builder.appendLiteral(" DESC");

    builder.append(';');

    return builder.toString();
}

static String buildObjectStoreStatement(const IDBKeyRangeData& keyRange, IndexedDB::CursorDirection cursorDirection)
{
    StringBuilder builder;

    builder.appendLiteral("SELECT rowid, key, value FROM Records WHERE objectStoreID = ? AND key ");

    if (!keyRange.lowerKey.isNull() && !keyRange.lowerOpen)
        builder.appendLiteral(">=");
    else
        builder.append('>');

    builder.appendLiteral(" CAST(? AS TEXT) AND key ");

    if (!keyRange.upperKey.isNull() && !keyRange.upperOpen)
        builder.appendLiteral("<=");
    else
        builder.append('<');

    builder.appendLiteral(" CAST(? AS TEXT) ORDER BY key");

    if (cursorDirection == IndexedDB::CursorDirection::Prev || cursorDirection == IndexedDB::CursorDirection::Prevunique)
        builder.appendLiteral(" DESC");

    builder.append(';');

    return builder.toString();
}

bool SQLiteIDBCursor::establishStatement()
{
    ASSERT(!m_statement);
    String sql;

    if (m_indexID != IDBIndexInfo::InvalidId) {
        sql = buildIndexStatement(m_keyRange, m_cursorDirection);
        m_boundID = m_indexID;
    } else {
        sql = buildObjectStoreStatement(m_keyRange, m_cursorDirection);
        m_boundID = m_objectStoreID;
    }

    m_currentLowerKey = m_keyRange.lowerKey.isNull() ? IDBKeyData::minimum() : m_keyRange.lowerKey;
    m_currentUpperKey = m_keyRange.upperKey.isNull() ? IDBKeyData::maximum() : m_keyRange.upperKey;

    return createSQLiteStatement(sql);
}

bool SQLiteIDBCursor::createSQLiteStatement(const String& sql)
{
    LOG(IndexedDB, "Creating cursor with SQL query: \"%s\"", sql.utf8().data());

    ASSERT(!m_currentLowerKey.isNull());
    ASSERT(!m_currentUpperKey.isNull());
    ASSERT(m_transaction->sqliteTransaction());

    m_statement = std::make_unique<SQLiteStatement>(m_transaction->sqliteTransaction()->database(), sql);

    if (m_statement->prepare() != SQLITE_OK) {
        LOG_ERROR("Could not create cursor statement (prepare/id) - '%s'", m_transaction->sqliteTransaction()->database().lastErrorMsg());
        return false;
    }

    return bindArguments();
}

void SQLiteIDBCursor::objectStoreRecordsChanged()
{
    if (m_statementNeedsReset)
        return;

    ASSERT(!m_fetchedRecords.isEmpty());

    m_currentKeyForUniqueness = m_fetchedRecords.first().record.key;

    if (m_cursorDirection != IndexedDB::CursorDirection::Nextunique && m_cursorDirection != IndexedDB::CursorDirection::Prevunique) {
        if (!m_fetchedRecords.last().isTerminalRecord())
            fetch(ShouldFetchForSameKey::Yes);

        while (m_fetchedRecords.last().record.key != m_fetchedRecords.first().record.key)
            m_fetchedRecords.removeLast();
    } else
        m_fetchedRecords.clear();

    // If ObjectStore or Index contents changed, we need to reset the statement and bind new parameters to it.
    // This is to pick up any changes that might exist.
    // We also need to throw away any fetched records as they may no longer be valid.

    m_statementNeedsReset = true;

    if (m_cursorDirection == IndexedDB::CursorDirection::Next || m_cursorDirection == IndexedDB::CursorDirection::Nextunique) {
        m_currentLowerKey = m_currentKeyForUniqueness;
        if (!m_keyRange.lowerOpen) {
            m_keyRange.lowerOpen = true;
            m_keyRange.lowerKey = m_currentLowerKey;
            m_statement = nullptr;
        }
    } else {
        m_currentUpperKey = m_currentKeyForUniqueness;
        if (!m_keyRange.upperOpen) {
            m_keyRange.upperOpen = true;
            m_keyRange.upperKey = m_currentUpperKey;
            m_statement = nullptr;
        }
    }
}

void SQLiteIDBCursor::resetAndRebindStatement()
{
    ASSERT(!m_currentLowerKey.isNull());
    ASSERT(!m_currentUpperKey.isNull());
    ASSERT(m_transaction->sqliteTransaction());
    ASSERT(m_statementNeedsReset);

    m_statementNeedsReset = false;

    if (!m_statement && !establishStatement()) {
        LOG_ERROR("Unable to establish new statement for cursor iteration");
        return;
    }

    if (m_statement->reset() != SQLITE_OK) {
        LOG_ERROR("Could not reset cursor statement to respond to object store changes");
        return;
    }

    bindArguments();
}

bool SQLiteIDBCursor::bindArguments()
{
    LOG(IndexedDB, "Cursor is binding lower key '%s' and upper key '%s'", m_currentLowerKey.loggingString().utf8().data(), m_currentUpperKey.loggingString().utf8().data());

    int currentBindArgument = 1;

    if (m_statement->bindInt64(currentBindArgument++, m_boundID) != SQLITE_OK) {
        LOG_ERROR("Could not bind id argument (bound ID)");
        return false;
    }

    if (m_indexID != IDBIndexInfo::InvalidId && m_statement->bindInt64(currentBindArgument++, m_objectStoreID) != SQLITE_OK) {
        LOG_ERROR("Could not bind object store id argument for an index cursor");
        return false;
    }

    RefPtr<SharedBuffer> buffer = serializeIDBKeyData(m_currentLowerKey);
    if (m_statement->bindBlob(currentBindArgument++, buffer->data(), buffer->size()) != SQLITE_OK) {
        LOG_ERROR("Could not create cursor statement (lower key)");
        return false;
    }

    buffer = serializeIDBKeyData(m_currentUpperKey);
    if (m_statement->bindBlob(currentBindArgument++, buffer->data(), buffer->size()) != SQLITE_OK) {
        LOG_ERROR("Could not create cursor statement (upper key)");
        return false;
    }

    return true;
}

bool SQLiteIDBCursor::prefetch()
{
    LOG(IndexedDB, "SQLiteIDBCursor::prefetch() - Cursor already has %zu fetched records", m_fetchedRecords.size());

    if (m_fetchedRecords.isEmpty() || m_fetchedRecords.size() >= prefetchLimit || m_fetchedRecords.last().isTerminalRecord())
        return false;

    m_currentKeyForUniqueness = m_fetchedRecords.last().record.key;
    fetch();

    return m_fetchedRecords.size() < prefetchLimit;
}

bool SQLiteIDBCursor::advance(uint64_t count)
{
    LOG(IndexedDB, "SQLiteIDBCursor::advance() - Count %" PRIu64 ", %zu fetched records", count, m_fetchedRecords.size());
    ASSERT(count);

    if (!m_fetchedRecords.isEmpty() && m_fetchedRecords.first().isTerminalRecord()) {
        LOG_ERROR("Attempt to advance a completed cursor");
        return false;
    }

    if (!m_fetchedRecords.isEmpty())
        m_currentKeyForUniqueness = m_fetchedRecords.last().record.key;

    // Drop already-fetched records up to `count` to see if we've already fetched the record we're looking for.
    bool hadCurrentRecord = !m_fetchedRecords.isEmpty();
    for (; count && !m_fetchedRecords.isEmpty(); --count) {
        if (m_fetchedRecords.first().isTerminalRecord())
            break;

        m_fetchedRecords.removeFirst();
    }

    // If we still have any records left, the first record is our new current record.
    if (!m_fetchedRecords.isEmpty())
        return true;

    ASSERT(m_fetchedRecords.isEmpty());

    // If we started out with a current record, we burnt a count on removing it.
    // Replace that count now.
    if (hadCurrentRecord)
        ++count;

    for (; count; --count) {
        if (!m_fetchedRecords.isEmpty()) {
            ASSERT(m_fetchedRecords.size() == 1);
            m_currentKeyForUniqueness = m_fetchedRecords.first().record.key;
            m_fetchedRecords.removeFirst();
        }

        if (!fetch())
            return false;

        ASSERT(!m_fetchedRecords.isEmpty());
        ASSERT(!m_fetchedRecords.first().errored);
        if (m_fetchedRecords.first().completed)
            break;
    }

    return true;
}

bool SQLiteIDBCursor::fetch(ShouldFetchForSameKey shouldFetchForSameKey)
{
    ASSERT(m_fetchedRecords.isEmpty() || !m_fetchedRecords.last().isTerminalRecord());

    m_fetchedRecords.append({ });

    bool isUnique = m_cursorDirection == IndexedDB::CursorDirection::Nextunique || m_cursorDirection == IndexedDB::CursorDirection::Prevunique || shouldFetchForSameKey == ShouldFetchForSameKey::Yes;
    if (!isUnique)
        return fetchNextRecord(m_fetchedRecords.last());

    while (fetchNextRecord(m_fetchedRecords.last())) {
        if (m_currentKeyForUniqueness.compare(m_fetchedRecords.last().record.key))
            return true;

        if (m_fetchedRecords.last().completed)
            return false;

        if (shouldFetchForSameKey == ShouldFetchForSameKey::Yes)
            m_fetchedRecords.append({ });
    }

    return false;
}

bool SQLiteIDBCursor::fetchNextRecord(SQLiteCursorRecord& record)
{
    if (m_statementNeedsReset)
        resetAndRebindStatement();

    FetchResult result;
    do {
        result = internalFetchNextRecord(record);
    } while (result == FetchResult::ShouldFetchAgain);

    return result == FetchResult::Success;
}

void SQLiteIDBCursor::markAsErrored(SQLiteCursorRecord& record)
{
    record.record = { };
    record.completed = true;
    record.errored = true;
    record.rowID = 0;
}

SQLiteIDBCursor::FetchResult SQLiteIDBCursor::internalFetchNextRecord(SQLiteCursorRecord& record)
{
    ASSERT(m_transaction->sqliteTransaction());
    ASSERT(m_statement);
    ASSERT(!m_fetchedRecords.isEmpty());
    ASSERT(!m_fetchedRecords.last().isTerminalRecord());

    record.record.value = nullptr;

    int result = m_statement->step();
    if (result == SQLITE_DONE) {
        // When a cursor reaches its end, that is indicated by having undefined keys/values
        record = { };
        record.completed = true;

        return FetchResult::Success;
    }

    if (result != SQLITE_ROW) {
        LOG_ERROR("Error advancing cursor - (%i) %s", result, m_transaction->sqliteTransaction()->database().lastErrorMsg());
        markAsErrored(record);
        return FetchResult::Failure;
    }

    record.rowID = m_statement->getColumnInt64(0);
    ASSERT(record.rowID);

    Vector<uint8_t> keyData;
    m_statement->getColumnBlobAsVector(1, keyData);

    if (!deserializeIDBKeyData(keyData.data(), keyData.size(), record.record.key)) {
        LOG_ERROR("Unable to deserialize key data from database while advancing cursor");
        markAsErrored(record);
        return FetchResult::Failure;
    }

    m_statement->getColumnBlobAsVector(2, keyData);

    // The primaryKey of an ObjectStore cursor is the same as its key.
    if (m_indexID == IDBIndexInfo::InvalidId) {
        record.record.primaryKey = record.record.key;

        Vector<String> blobURLs, blobFilePaths;
        PAL::SessionID sessionID;
        auto error = m_transaction->backingStore().getBlobRecordsForObjectStoreRecord(record.rowID, blobURLs, sessionID, blobFilePaths);
        if (!error.isNull()) {
            LOG_ERROR("Unable to fetch blob records from database while advancing cursor");
            markAsErrored(record);
            return FetchResult::Failure;
        }

        if (m_cursorType == IndexedDB::CursorType::KeyAndValue)
            record.record.value = std::make_unique<IDBValue>(ThreadSafeDataBuffer::create(WTFMove(keyData)), blobURLs, sessionID, blobFilePaths);
    } else {
        if (!deserializeIDBKeyData(keyData.data(), keyData.size(), record.record.primaryKey)) {
            LOG_ERROR("Unable to deserialize value data from database while advancing index cursor");
            markAsErrored(record);
            return FetchResult::Failure;
        }

        SQLiteStatement objectStoreStatement(m_statement->database(), "SELECT value FROM Records WHERE key = CAST(? AS TEXT) and objectStoreID = ?;");

        if (objectStoreStatement.prepare() != SQLITE_OK
            || objectStoreStatement.bindBlob(1, keyData.data(), keyData.size()) != SQLITE_OK
            || objectStoreStatement.bindInt64(2, m_objectStoreID) != SQLITE_OK) {
            LOG_ERROR("Could not create index cursor statement into object store records (%i) '%s'", m_statement->database().lastError(), m_statement->database().lastErrorMsg());
            markAsErrored(record);
            return FetchResult::Failure;
        }

        int result = objectStoreStatement.step();

        if (result == SQLITE_ROW) {
            objectStoreStatement.getColumnBlobAsVector(0, keyData);
            record.record.value = std::make_unique<IDBValue>(ThreadSafeDataBuffer::create(WTFMove(keyData)));
        } else if (result == SQLITE_DONE) {
            // This indicates that the record we're trying to retrieve has been removed from the object store.
            // Skip over it.
            return FetchResult::ShouldFetchAgain;
        } else {
            LOG_ERROR("Could not step index cursor statement into object store records (%i) '%s'", m_statement->database().lastError(), m_statement->database().lastErrorMsg());
            markAsErrored(record);
            return FetchResult::Failure;

        }
    }

    return FetchResult::Success;
}

bool SQLiteIDBCursor::iterate(const IDBKeyData& targetKey, const IDBKeyData& targetPrimaryKey)
{
    ASSERT(m_transaction->sqliteTransaction());
    ASSERT(m_statement);

    bool result = advance(1);
    ASSERT(!m_fetchedRecords.isEmpty());

    // Iterating with no key is equivalent to advancing 1 step.
    if (targetKey.isNull() || !result)
        return result;

    while (!m_fetchedRecords.first().isTerminalRecord()) {
        if (!result)
            return false;

        // Search for the next key >= the target if the cursor is a Next cursor, or the next key <= if the cursor is a Previous cursor.
        if (m_cursorDirection == IndexedDB::CursorDirection::Next || m_cursorDirection == IndexedDB::CursorDirection::Nextunique) {
            if (m_fetchedRecords.first().record.key.compare(targetKey) >= 0)
                break;
        } else if (m_fetchedRecords.first().record.key.compare(targetKey) <= 0)
            break;

        result = advance(1);
    }

    if (targetPrimaryKey.isValid()) {
        while (!m_fetchedRecords.first().isTerminalRecord() && !m_fetchedRecords.first().record.key.compare(targetKey)) {
            if (!result)
                return false;

            // Search for the next primary key >= the primary target if the cursor is a Next cursor, or the next key <= if the cursor is a Previous cursor.
            if (m_cursorDirection == IndexedDB::CursorDirection::Next || m_cursorDirection == IndexedDB::CursorDirection::Nextunique) {
                if (m_fetchedRecords.first().record.primaryKey.compare(targetPrimaryKey) >= 0)
                    break;
            } else if (m_fetchedRecords.first().record.primaryKey.compare(targetPrimaryKey) <= 0)
                break;

            result = advance(1);
        }
    }

    return result;
}

const IDBKeyData& SQLiteIDBCursor::currentKey() const
{
    ASSERT(!m_fetchedRecords.isEmpty());
    return m_fetchedRecords.first().record.key;
}

const IDBKeyData& SQLiteIDBCursor::currentPrimaryKey() const
{
    ASSERT(!m_fetchedRecords.isEmpty());
    return m_fetchedRecords.first().record.primaryKey;
}

IDBValue* SQLiteIDBCursor::currentValue() const
{
    ASSERT(!m_fetchedRecords.isEmpty());
    return m_fetchedRecords.first().record.value.get();
}

bool SQLiteIDBCursor::didComplete() const
{
    ASSERT(!m_fetchedRecords.isEmpty());
    return m_fetchedRecords.first().completed;
}

bool SQLiteIDBCursor::didError() const
{
    ASSERT(!m_fetchedRecords.isEmpty());
    return m_fetchedRecords.first().errored;
}

int64_t SQLiteIDBCursor::currentRecordRowID() const
{
    ASSERT(!m_fetchedRecords.isEmpty());
    return m_fetchedRecords.first().rowID;
}


} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
