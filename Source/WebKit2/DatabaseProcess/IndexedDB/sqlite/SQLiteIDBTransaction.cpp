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
#include "config.h"
#include "SQLiteIDBTransaction.h"

#if ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

#include "SQLiteIDBCursor.h"
#include "UniqueIDBDatabaseBackingStoreSQLite.h"
#include <WebCore/IndexedDB.h>
#include <WebCore/SQLiteTransaction.h>

using namespace WebCore;

static int64_t nextCursorID = 1;

namespace WebKit {

SQLiteIDBTransaction::SQLiteIDBTransaction(UniqueIDBDatabaseBackingStoreSQLite& backingStore, const IDBIdentifier& transactionIdentifier, IndexedDB::TransactionMode mode)
    : m_identifier(transactionIdentifier)
    , m_mode(mode)
    , m_backingStore(backingStore)
{
}

SQLiteIDBTransaction::~SQLiteIDBTransaction()
{
    if (inProgress())
        m_sqliteTransaction->rollback();

    // Explicitly clear cursors, as that also unregisters them from the backing store.
    clearCursors();
}


bool SQLiteIDBTransaction::begin(SQLiteDatabase& database)
{
    ASSERT(!m_sqliteTransaction);
    m_sqliteTransaction = std::make_unique<SQLiteTransaction>(database, m_mode == IndexedDB::TransactionMode::ReadOnly);

    m_sqliteTransaction->begin();

    return m_sqliteTransaction->inProgress();
}

bool SQLiteIDBTransaction::commit()
{
    ASSERT(m_sqliteTransaction);
    if (!m_sqliteTransaction->inProgress())
        return false;

    m_sqliteTransaction->commit();

    return !m_sqliteTransaction->inProgress();
}

bool SQLiteIDBTransaction::reset()
{
    m_sqliteTransaction = nullptr;
    clearCursors();

    return true;
}

bool SQLiteIDBTransaction::rollback()
{
    ASSERT(m_sqliteTransaction);
    if (m_sqliteTransaction->inProgress())
        m_sqliteTransaction->rollback();

    return true;
}

SQLiteIDBCursor* SQLiteIDBTransaction::openCursor(int64_t objectStoreID, int64_t indexID, IndexedDB::CursorDirection cursorDirection, IndexedDB::CursorType cursorType, IDBDatabaseBackend::TaskType taskType, const IDBKeyRangeData& keyRange)
{
    ASSERT(m_sqliteTransaction);
    if (!m_sqliteTransaction->inProgress())
        return nullptr;

    IDBIdentifier cursorIdentifier(m_identifier.connection(), nextCursorID++);

    auto addResult = m_cursors.add(cursorIdentifier, SQLiteIDBCursor::maybeCreate(this, cursorIdentifier, objectStoreID, indexID, cursorDirection, cursorType, taskType, keyRange));

    ASSERT(addResult.isNewEntry);

    // It is possible the cursor failed to create and we just stored a null value.
    if (!addResult.iterator->value) {
        m_cursors.remove(addResult.iterator);
        return nullptr;
    }

    return addResult.iterator->value.get();
}

void SQLiteIDBTransaction::closeCursor(SQLiteIDBCursor& cursor)
{
    ASSERT(m_cursors.contains(cursor.identifier()));

    m_backingStore.unregisterCursor(&cursor);
    m_cursors.remove(cursor.identifier());
}

void SQLiteIDBTransaction::clearCursors()
{
    // Iterate over the keys instead of each key/value pair because std::unique_ptr<> can't be iterated over directly.
    for (auto key : m_cursors.keys())
        m_backingStore.unregisterCursor(m_cursors.get(key));

    m_cursors.clear();
}

bool SQLiteIDBTransaction::inProgress() const
{
    return m_sqliteTransaction && m_sqliteTransaction->inProgress();
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
