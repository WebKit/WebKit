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
#include "Logging.h"
#include "SQLiteIDBBackingStore.h"
#include "SQLiteIDBCursor.h"
#include "SQLiteTransaction.h"
#include <wtf/FileSystem.h>

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

    m_sqliteTransaction = makeUnique<SQLiteTransaction>(database, m_info.mode() == IDBTransactionMode::Readonly);
    m_sqliteTransaction->begin();

    if (m_sqliteTransaction->inProgress())
        return IDBError { };

    return IDBError { UnknownError, "Could not start SQLite transaction in database backend"_s };
}

IDBError SQLiteIDBTransaction::commit()
{
    LOG(IndexedDB, "SQLiteIDBTransaction::commit");
    if (!m_sqliteTransaction || !m_sqliteTransaction->inProgress())
        return IDBError { UnknownError, "No SQLite transaction in progress to commit"_s };

    m_sqliteTransaction->commit();

    if (m_sqliteTransaction->inProgress())
        return IDBError { UnknownError, "Unable to commit SQLite transaction in database backend"_s };

    deleteBlobFilesIfNecessary();
    moveBlobFilesIfNecessary();

    reset();
    return IDBError { };
}

void SQLiteIDBTransaction::moveBlobFilesIfNecessary()
{
    String databaseDirectory = m_backingStore.databaseDirectory();
    for (auto& entry : m_blobTemporaryAndStoredFilenames) {
        if (!FileSystem::hardLinkOrCopyFile(entry.first, FileSystem::pathByAppendingComponent(databaseDirectory, entry.second)))
            LOG_ERROR("Failed to link/copy temporary blob file '%s' to location '%s'", entry.first.utf8().data(), FileSystem::pathByAppendingComponent(databaseDirectory, entry.second).utf8().data());

        FileSystem::deleteFile(entry.first);
    }

    m_blobTemporaryAndStoredFilenames.clear();
}

void SQLiteIDBTransaction::deleteBlobFilesIfNecessary()
{
    if (m_blobRemovedFilenames.isEmpty())
        return;

    String databaseDirectory = m_backingStore.databaseDirectory();
    for (auto& entry : m_blobRemovedFilenames) {
        String fullPath = FileSystem::pathByAppendingComponent(databaseDirectory, entry);

        FileSystem::deleteFile(fullPath);
    }

    m_blobRemovedFilenames.clear();
}

IDBError SQLiteIDBTransaction::abort()
{
    for (auto& entry : m_blobTemporaryAndStoredFilenames)
        FileSystem::deleteFile(entry.first);

    m_blobTemporaryAndStoredFilenames.clear();

    if (!m_sqliteTransaction || !m_sqliteTransaction->inProgress())
        return IDBError { UnknownError, "No SQLite transaction in progress to abort"_s };

    m_sqliteTransaction->rollback();

    if (m_sqliteTransaction->inProgress())
        return IDBError { UnknownError, "Unable to abort SQLite transaction in database backend"_s };

    reset();
    return IDBError { };
}

void SQLiteIDBTransaction::reset()
{
    m_sqliteTransaction = nullptr;
    clearCursors();
    ASSERT(m_blobTemporaryAndStoredFilenames.isEmpty());
}

std::unique_ptr<SQLiteIDBCursor> SQLiteIDBTransaction::maybeOpenBackingStoreCursor(uint64_t objectStoreID, uint64_t indexID, const IDBKeyRangeData& range)
{
    ASSERT(m_sqliteTransaction);
    ASSERT(m_sqliteTransaction->inProgress());

    auto cursor = SQLiteIDBCursor::maybeCreateBackingStoreCursor(*this, objectStoreID, indexID, range);

    if (cursor)
        m_backingStoreCursors.add(cursor.get());

    return cursor;
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
    auto backingStoreTake = m_backingStoreCursors.take(&cursor);
    if (backingStoreTake) {
        ASSERT(!m_cursors.contains(cursor.identifier()));
        return;
    }

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

    for (auto* cursor : m_backingStoreCursors) {
        if (cursor->objectStoreID() == objectStoreID)
            cursor->objectStoreRecordsChanged();
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

void SQLiteIDBTransaction::addBlobFile(const String& temporaryPath, const String& storedFilename)
{
    m_blobTemporaryAndStoredFilenames.append({ temporaryPath, storedFilename });
}

void SQLiteIDBTransaction::addRemovedBlobFile(const String& removedFilename)
{
    ASSERT(!m_blobRemovedFilenames.contains(removedFilename));
    m_blobRemovedFilenames.add(removedFilename);
}


} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
