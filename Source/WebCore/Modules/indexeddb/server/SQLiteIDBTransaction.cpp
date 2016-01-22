/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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

#if ENABLE(INDEXED_DATABASE)

#include "IDBCursorInfo.h"
#include "IndexedDB.h"
#include "SQLiteIDBBackingStore.h"
#include "SQLiteIDBCursor.h"
#include "SQLiteTransaction.h"

namespace WebCore {
namespace IDBServer {

SQLiteIDBTransaction::SQLiteIDBTransaction(SQLiteIDBBackingStore& backingStore, const IDBTransactionInfo& info)
    : m_info(info)
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


IDBError SQLiteIDBTransaction::begin(SQLiteDatabase& database)
{
    ASSERT(!m_sqliteTransaction);

    m_sqliteTransaction = std::make_unique<SQLiteTransaction>(database, m_info.mode() == IndexedDB::TransactionMode::ReadOnly);
    m_sqliteTransaction->begin();

    if (m_sqliteTransaction->inProgress())
        return { };

    return { IDBDatabaseException::UnknownError, ASCIILiteral("Could not start SQLite transaction in database backend") };
}

IDBError SQLiteIDBTransaction::commit()
{
    if (!m_sqliteTransaction || !m_sqliteTransaction->inProgress())
        return { IDBDatabaseException::UnknownError, ASCIILiteral("No SQLite transaction in progress to commit") };

    m_sqliteTransaction->commit();

    if (m_sqliteTransaction->inProgress())
        return { IDBDatabaseException::UnknownError, ASCIILiteral("Unable to commit SQLite transaction in database backend") };

    reset();
    return { };
}

IDBError SQLiteIDBTransaction::abort()
{
    if (!m_sqliteTransaction || !m_sqliteTransaction->inProgress())
        return { IDBDatabaseException::UnknownError, ASCIILiteral("No SQLite transaction in progress to abort") };

    m_sqliteTransaction->rollback();

    if (m_sqliteTransaction->inProgress())
        return { IDBDatabaseException::UnknownError, ASCIILiteral("Unable to abort SQLite transaction in database backend") };

    reset();
    return { };
}

void SQLiteIDBTransaction::reset()
{
    m_sqliteTransaction = nullptr;
    clearCursors();
}

SQLiteIDBCursor* SQLiteIDBTransaction::maybeOpenCursor(const IDBCursorInfo& info)
{
    ASSERT(m_sqliteTransaction);
    if (!m_sqliteTransaction->inProgress())
        return nullptr;

    auto addResult = m_cursors.add(info.identifier(), SQLiteIDBCursor::maybeCreate(*this, info));

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

    m_backingStore.unregisterCursor(cursor);
    m_cursors.remove(cursor.identifier());
}

void SQLiteIDBTransaction::notifyCursorsOfChanges(int64_t objectStoreID)
{
    for (auto& i : m_cursors) {
        if (i.value->objectStoreID() == objectStoreID)
            i.value->objectStoreRecordsChanged();
    }
}

void SQLiteIDBTransaction::clearCursors()
{
    for (auto& cursor : m_cursors.values())
        m_backingStore.unregisterCursor(*cursor);

    m_cursors.clear();
}

bool SQLiteIDBTransaction::inProgress() const
{
    return m_sqliteTransaction && m_sqliteTransaction->inProgress();
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
