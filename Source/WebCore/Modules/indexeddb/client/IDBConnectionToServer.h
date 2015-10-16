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

#ifndef IDBConnectionToServer_h
#define IDBConnectionToServer_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBConnectionToServerDelegate.h"
#include "IDBResourceIdentifier.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class IDBError;
class IDBResultData;

namespace IDBClient {

class IDBDatabase;
class IDBOpenDBRequest;
class IDBTransaction;

class IDBConnectionToServer : public RefCounted<IDBConnectionToServer> {
public:
    static Ref<IDBConnectionToServer> create(IDBConnectionToServerDelegate&);

    uint64_t identifier() const;

    void deleteDatabase(IDBOpenDBRequest&);
    void didDeleteDatabase(const IDBResultData&);

    void openDatabase(IDBOpenDBRequest&);
    void didOpenDatabase(const IDBResultData&);

    void commitTransaction(IDBTransaction&);
    void didCommitTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError&);

    void fireVersionChangeEvent(uint64_t databaseConnectionIdentifier, uint64_t requestedVersion);

    void databaseConnectionClosed(IDBDatabase&);
    void registerDatabaseConnection(IDBDatabase&);
    void unregisterDatabaseConnection(IDBDatabase&);

private:
    IDBConnectionToServer(IDBConnectionToServerDelegate&);
    
    Ref<IDBConnectionToServerDelegate> m_delegate;

    HashMap<IDBResourceIdentifier, RefPtr<IDBClient::IDBOpenDBRequest>> m_openDBRequestMap;
    HashSet<RefPtr<IDBDatabase>> m_databaseConnections;
    HashMap<IDBResourceIdentifier, RefPtr<IDBTransaction>> m_committingTransactions;
};

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBConnectionToServer_h
