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

#ifndef UniqueIDBDatabase_h
#define UniqueIDBDatabase_h

#if ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

#include "IDBTransactionIdentifier.h"
#include "UniqueIDBDatabaseIdentifier.h"
#include <functional>
#include <wtf/Deque.h>
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
struct IDBDatabaseMetadata;
}

namespace WebKit {

class AsyncRequest;
class AsyncTask;
class DatabaseProcessIDBConnection;
class UniqueIDBDatabaseBackingStore;

struct SecurityOriginData;

class UniqueIDBDatabase : public ThreadSafeRefCounted<UniqueIDBDatabase> {
public:
    static PassRefPtr<UniqueIDBDatabase> create(const UniqueIDBDatabaseIdentifier& identifier)
    {
        return adoptRef(new UniqueIDBDatabase(identifier));
    }

    ~UniqueIDBDatabase();

    const UniqueIDBDatabaseIdentifier& identifier() const { return m_identifier; }

    void registerConnection(DatabaseProcessIDBConnection&);
    void unregisterConnection(DatabaseProcessIDBConnection&);

    void getOrEstablishIDBDatabaseMetadata(std::function<void(bool, const WebCore::IDBDatabaseMetadata&)> completionCallback);
    void openTransaction(const IDBTransactionIdentifier&, std::function<void(bool)> successCallback);

private:
    UniqueIDBDatabase(const UniqueIDBDatabaseIdentifier&);

    UniqueIDBDatabaseIdentifier m_identifier;

    bool m_inMemory;
    String m_databaseRelativeDirectory;

    HashSet<RefPtr<DatabaseProcessIDBConnection>> m_connections;
    HashMap<uint64_t, RefPtr<AsyncRequest>> m_databaseRequests;

    String absoluteDatabaseDirectory() const;

    void postDatabaseTask(std::unique_ptr<AsyncTask>);
    void shutdown();

    // Method that attempts to make legal filenames from all legal database names
    String filenameForDatabaseName() const;

    // Returns a string that is appropriate for use as a unique filename
    String databaseFilenameIdentifier(const SecurityOriginData&) const;

    // Returns true if this origin can use the same databases as the given origin.
    bool canShareDatabases(const SecurityOriginData&, const SecurityOriginData&) const;
    
    // To be called from the database workqueue thread only
    void performNextDatabaseTask();
    void postMainThreadTask(std::unique_ptr<AsyncTask>);
    void openBackingStoreAndReadMetadata(const UniqueIDBDatabaseIdentifier&, const String& databaseDirectory);
    void openBackingStoreTransaction(const IDBTransactionIdentifier&);

    // Callbacks from the database workqueue thread, to be performed on the main thread only
    void performNextMainThreadTask();
    void didOpenBackingStoreAndReadMetadata(const WebCore::IDBDatabaseMetadata&, bool success);
    void didOpenBackingStoreTransaction(const IDBTransactionIdentifier&, bool success);

    bool m_acceptingNewRequests;

    Deque<RefPtr<AsyncRequest>> m_pendingMetadataRequests;
    HashMap<IDBTransactionIdentifier, RefPtr<AsyncRequest>> m_pendingOpenTransactionRequests;

    std::unique_ptr<WebCore::IDBDatabaseMetadata> m_metadata;
    bool m_didGetMetadataFromBackingStore;

    RefPtr<UniqueIDBDatabaseBackingStore> m_backingStore;

    Deque<std::unique_ptr<AsyncTask>> m_databaseTasks;
    Mutex m_databaseTaskMutex;

    Deque<std::unique_ptr<AsyncTask>> m_mainThreadTasks;
    Mutex m_mainThreadTaskMutex;
};

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
#endif // UniqueIDBDatabase_h
