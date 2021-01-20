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

#pragma once

#if ENABLE(INDEXED_DATABASE)

#include "IDBError.h"
#include "IDBResourceIdentifier.h"
#include "IDBTransactionInfo.h"
#include "IndexedDB.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class IDBCursorInfo;
class SQLiteDatabase;
class SQLiteTransaction;
struct IDBKeyRangeData;

namespace IDBServer {

class SQLiteIDBBackingStore;
class SQLiteIDBCursor;

class SQLiteIDBTransaction {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(SQLiteIDBTransaction);
public:
    SQLiteIDBTransaction(SQLiteIDBBackingStore&, const IDBTransactionInfo&);
    ~SQLiteIDBTransaction();

    const IDBResourceIdentifier& transactionIdentifier() const { return m_info.identifier(); }

    IDBError begin(SQLiteDatabase&);
    IDBError commit();
    IDBError abort();

    std::unique_ptr<SQLiteIDBCursor> maybeOpenBackingStoreCursor(uint64_t objectStoreID, uint64_t indexID, const IDBKeyRangeData&);
    SQLiteIDBCursor* maybeOpenCursor(const IDBCursorInfo&);

    void closeCursor(SQLiteIDBCursor&);
    void notifyCursorsOfChanges(int64_t objectStoreID);

    IDBTransactionMode mode() const { return m_info.mode(); }
    bool inProgress() const;

    SQLiteTransaction* sqliteTransaction() const { return m_sqliteTransaction.get(); }
    SQLiteIDBBackingStore& backingStore() { return m_backingStore; }

    void addBlobFile(const String& temporaryPath, const String& storedFilename);
    void addRemovedBlobFile(const String& removedFilename);

private:
    void clearCursors();
    void reset();

    void moveBlobFilesIfNecessary();
    void deleteBlobFilesIfNecessary();

    IDBTransactionInfo m_info;

    SQLiteIDBBackingStore& m_backingStore;
    std::unique_ptr<SQLiteTransaction> m_sqliteTransaction;
    HashMap<IDBResourceIdentifier, std::unique_ptr<SQLiteIDBCursor>> m_cursors;
    HashSet<SQLiteIDBCursor*> m_backingStoreCursors;
    Vector<std::pair<String, String>> m_blobTemporaryAndStoredFilenames;
    HashSet<String> m_blobRemovedFilenames;
};

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
