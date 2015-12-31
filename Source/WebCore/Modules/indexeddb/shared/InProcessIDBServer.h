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

#ifndef InProcessIDBServer_h
#define InProcessIDBServer_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBConnectionToClient.h"
#include "IDBConnectionToServer.h"
#include "IDBOpenDBRequestImpl.h"
#include "IDBServer.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

namespace IDBClient {
class IDBConnectionToServer;
}

namespace IDBServer {
class IDBServer;
}

class InProcessIDBServer final : public IDBClient::IDBConnectionToServerDelegate, public IDBServer::IDBConnectionToClientDelegate, public RefCounted<InProcessIDBServer> {
public:
    WEBCORE_EXPORT static Ref<InProcessIDBServer> create();

    WEBCORE_EXPORT IDBClient::IDBConnectionToServer& connectionToServer() const;
    IDBServer::IDBConnectionToClient& connectionToClient() const;

    // IDBConnectionToServer
    virtual void deleteDatabase(IDBRequestData&) override final;
    virtual void openDatabase(IDBRequestData&) override final;
    virtual void abortTransaction(IDBResourceIdentifier&) override final;
    virtual void commitTransaction(IDBResourceIdentifier&) override final;
    virtual void didFinishHandlingVersionChangeTransaction(IDBResourceIdentifier&) override final;
    virtual void createObjectStore(const IDBRequestData&, const IDBObjectStoreInfo&) override final;
    virtual void deleteObjectStore(const IDBRequestData&, const String& objectStoreName) override final;
    virtual void clearObjectStore(const IDBRequestData&, uint64_t objectStoreIdentifier) override final;
    virtual void createIndex(const IDBRequestData&, const IDBIndexInfo&) override final;
    virtual void deleteIndex(const IDBRequestData&, uint64_t objectStoreIdentifier, const String& indexName) override final;
    virtual void putOrAdd(const IDBRequestData&, IDBKey*, SerializedScriptValue&, const IndexedDB::ObjectStoreOverwriteMode) override final;
    virtual void getRecord(const IDBRequestData&, const IDBKeyRangeData&) override final;
    virtual void getCount(const IDBRequestData&, const IDBKeyRangeData&) override final;
    virtual void deleteRecord(const IDBRequestData&, const IDBKeyRangeData&) override final;
    virtual void openCursor(const IDBRequestData&, const IDBCursorInfo&) override final;
    virtual void iterateCursor(const IDBRequestData&, const IDBKeyData&, unsigned long count) override final;
    virtual void establishTransaction(uint64_t databaseConnectionIdentifier, const IDBTransactionInfo&) override final;
    virtual void databaseConnectionClosed(uint64_t databaseConnectionIdentifier) override final;
    virtual void didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier) override final;

    // IDBConnectionToClient
    virtual uint64_t identifier() const override;
    virtual void didDeleteDatabase(const IDBResultData&) override final;
    virtual void didOpenDatabase(const IDBResultData&) override final;
    virtual void didAbortTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError&) override final;
    virtual void didCommitTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError&) override final;
    virtual void didCreateObjectStore(const IDBResultData&) override final;
    virtual void didDeleteObjectStore(const IDBResultData&) override final;
    virtual void didClearObjectStore(const IDBResultData&) override final;
    virtual void didCreateIndex(const IDBResultData&) override final;
    virtual void didDeleteIndex(const IDBResultData&) override final;
    virtual void didPutOrAdd(const IDBResultData&) override final;
    virtual void didGetRecord(const IDBResultData&) override final;
    virtual void didGetCount(const IDBResultData&) override final;
    virtual void didDeleteRecord(const IDBResultData&) override final;
    virtual void didOpenCursor(const IDBResultData&) override final;
    virtual void didIterateCursor(const IDBResultData&) override final;
    virtual void fireVersionChangeEvent(IDBServer::UniqueIDBDatabaseConnection&, const IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion) override final;
    virtual void didStartTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError&) override final;
    virtual void notifyOpenDBRequestBlocked(const IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion) override final;

    virtual void ref() override { RefCounted<InProcessIDBServer>::ref(); }
    virtual void deref() override { RefCounted<InProcessIDBServer>::deref(); }

private:
    InProcessIDBServer();

    Ref<IDBServer::IDBServer> m_server;
    RefPtr<IDBClient::IDBConnectionToServer> m_connectionToServer;
    RefPtr<IDBServer::IDBConnectionToClient> m_connectionToClient;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // InProcessIDBServer_h
