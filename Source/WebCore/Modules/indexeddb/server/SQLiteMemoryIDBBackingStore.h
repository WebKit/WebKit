/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <WebCore/SQLiteIDBBackingStore.h>

namespace WebCore {
namespace IDBServer {

// SQLite-backed in-memory IndexedDB backing store.
// Unlike MemoryIDBBackingStore (which uses HashMaps), this uses SQLite's
// in-memory database (":memory:") for better scalability and ACID guarantees.
// Unlike SQLiteIDBBackingStore (which persists to disk), all data is ephemeral
// and lost when the backing store is destroyed.
class SQLiteMemoryIDBBackingStore final : public SQLiteIDBBackingStore {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(SQLiteMemoryIDBBackingStore, WEBCORE_EXPORT);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SQLiteMemoryIDBBackingStore);
public:
    WEBCORE_EXPORT explicit SQLiteMemoryIDBBackingStore(const IDBDatabaseIdentifier&);
    WEBCORE_EXPORT ~SQLiteMemoryIDBBackingStore() final;

    // Override database initialization to use in-memory SQLite database
    IDBError getOrEstablishDatabaseInfo(IDBDatabaseInfo&) final;

    // Override to indicate this is an ephemeral, in-memory backing store
    bool isEphemeral() final { return true; }

    // SQLite only allows one transaction per connection, even for in-memory databases
    bool supportsSimultaneousReadWriteTransactions() final { return false; }

    // No database file path for in-memory databases
    String fullDatabasePath() const final { return nullString(); }

    // No-op for in-memory databases - nothing to delete from disk
    void deleteBackingStore() final { }
};

} // namespace IDBServer
} // namespace WebCore
