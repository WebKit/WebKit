/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef UniqueIDBDatabase_h
#define UniqueIDBDatabase_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBBackingStore.h"
#include "IDBDatabaseIdentifier.h"
#include "IDBDatabaseInfo.h"
#include "IDBServerOperation.h"
#include "UniqueIDBDatabaseConnection.h"
#include "UniqueIDBDatabaseTransaction.h"
#include <wtf/Deque.h>
#include <wtf/HashSet.h>
#include <wtf/Ref.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class IDBRequestData;

namespace IDBServer {

class IDBConnectionToClient;
class IDBServer;

class UniqueIDBDatabase : public ThreadSafeRefCounted<UniqueIDBDatabase> {
public:
    static Ref<UniqueIDBDatabase> create(IDBServer& server, const IDBDatabaseIdentifier& identifier)
    {
        return adoptRef(*new UniqueIDBDatabase(server, identifier));
    }

    void openDatabaseConnection(IDBConnectionToClient&, const IDBRequestData&);

    const IDBDatabaseInfo& info() const;

private:
    UniqueIDBDatabase(IDBServer&, const IDBDatabaseIdentifier&);
    
    void handleOpenDatabaseOperations();
    void addOpenDatabaseConnection(Ref<UniqueIDBDatabaseConnection>&&);
    bool hasAnyOpenConnections() const;

    void startVersionChangeTransaction();
    void notifyConnectionsOfVersionChange();
    
    // Database thread operations
    void openBackingStore(const IDBDatabaseIdentifier&);

    // Main thread callbacks
    void didOpenBackingStore(const IDBDatabaseInfo&);

    IDBServer& m_server;
    IDBDatabaseIdentifier m_identifier;
    
    Deque<Ref<IDBServerOperation>> m_pendingOpenDatabaseOperations;

    HashSet<RefPtr<UniqueIDBDatabaseConnection>> m_openDatabaseConnections;

    RefPtr<IDBServerOperation> m_versionChangeOperation;
    RefPtr<UniqueIDBDatabaseConnection> m_versionChangeDatabaseConnection;

    RefPtr<UniqueIDBDatabaseTransaction> m_versionChangeTransaction;
    HashMap<IDBResourceIdentifier, RefPtr<UniqueIDBDatabaseTransaction>> m_inProgressTransactions;

    std::unique_ptr<IDBBackingStore> m_backingStore;
    std::unique_ptr<IDBDatabaseInfo> m_databaseInfo;
};

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // UniqueIDBDatabase_h
