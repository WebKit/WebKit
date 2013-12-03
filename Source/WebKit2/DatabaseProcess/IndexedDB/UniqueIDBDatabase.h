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
class DatabaseProcessIDBConnection;

struct SecurityOriginData;

class UniqueIDBDatabase : public RefCounted<UniqueIDBDatabase> {
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

private:
    UniqueIDBDatabase(const UniqueIDBDatabaseIdentifier&);

    UniqueIDBDatabaseIdentifier m_identifier;

    HashSet<RefPtr<DatabaseProcessIDBConnection>> m_connections;
    HashMap<uint64_t, RefPtr<AsyncRequest>> m_databaseRequests;

    void enqueueDatabaseQueueRequest(PassRefPtr<AsyncRequest>);

    // To be called from the database workqueue thread only
    void processDatabaseRequestQueue();
    bool getOrEstablishIDBDatabaseMetadataInternal(const WebCore::IDBDatabaseMetadata&);

    Mutex m_databaseQueueRequestsMutex;
    Deque<RefPtr<AsyncRequest>> m_databaseQueueRequests;
    bool m_processingDatabaseQueueRequests;
};

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
#endif // UniqueIDBDatabase_h
