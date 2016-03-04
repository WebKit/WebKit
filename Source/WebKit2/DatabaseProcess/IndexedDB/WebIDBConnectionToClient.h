/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#ifndef WebIDBConnectionToClient_h
#define WebIDBConnectionToClient_h

#if ENABLE(INDEXED_DATABASE)

#include "DatabaseToWebProcessConnection.h"
#include "MessageSender.h"
#include <WebCore/IDBConnectionToClient.h>

namespace WebCore {
class IDBCursorInfo;
class IDBIndexInfo;
class IDBKeyData;
class IDBObjectStoreInfo;
class IDBRequestData;
class IDBTransactionInfo;
class SerializedScriptValue;
struct IDBKeyRangeData;
}

namespace WebKit {

class WebIDBConnectionToClient final : public WebCore::IDBServer::IDBConnectionToClientDelegate, public IPC::MessageSender, public RefCounted<WebIDBConnectionToClient> {
public:
    static Ref<WebIDBConnectionToClient> create(DatabaseToWebProcessConnection&, uint64_t serverConnectionIdentifier);

    virtual ~WebIDBConnectionToClient();

    WebCore::IDBServer::IDBConnectionToClient& connectionToClient();
    uint64_t identifier() const override final { return m_identifier; }
    uint64_t messageSenderDestinationID() override final { return m_identifier; }

    // IDBConnectionToClientDelegate
    void didDeleteDatabase(const WebCore::IDBResultData&) override final;
    void didOpenDatabase(const WebCore::IDBResultData&) override final;
    void didAbortTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError&) override final;
    void didCommitTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError&) override final;
    void didCreateObjectStore(const WebCore::IDBResultData&) override final;
    void didDeleteObjectStore(const WebCore::IDBResultData&) override final;
    void didClearObjectStore(const WebCore::IDBResultData&) override final;
    void didCreateIndex(const WebCore::IDBResultData&) override final;
    void didDeleteIndex(const WebCore::IDBResultData&) override final;
    void didPutOrAdd(const WebCore::IDBResultData&) override final;
    void didGetRecord(const WebCore::IDBResultData&) override final;
    void didGetCount(const WebCore::IDBResultData&) override final;
    void didDeleteRecord(const WebCore::IDBResultData&) override final;
    void didOpenCursor(const WebCore::IDBResultData&) override final;
    void didIterateCursor(const WebCore::IDBResultData&) override final;

    void fireVersionChangeEvent(WebCore::IDBServer::UniqueIDBDatabaseConnection&, const WebCore::IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion) override final;
    void didStartTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError&) override final;
    void notifyOpenDBRequestBlocked(const WebCore::IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion) override final;

    void ref() override { RefCounted<WebIDBConnectionToClient>::ref(); }
    void deref() override { RefCounted<WebIDBConnectionToClient>::deref(); }

    // Messages received from WebProcess
    void deleteDatabase(const WebCore::IDBRequestData&);
    void openDatabase(const WebCore::IDBRequestData&);
    void abortTransaction(const WebCore::IDBResourceIdentifier&);
    void commitTransaction(const WebCore::IDBResourceIdentifier&);
    void didFinishHandlingVersionChangeTransaction(const WebCore::IDBResourceIdentifier&);
    void createObjectStore(const WebCore::IDBRequestData&, const WebCore::IDBObjectStoreInfo&);
    void deleteObjectStore(const WebCore::IDBRequestData&, const String& objectStoreName);
    void clearObjectStore(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier);
    void createIndex(const WebCore::IDBRequestData&, const WebCore::IDBIndexInfo&);
    void deleteIndex(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier, const String& indexName);
    void putOrAdd(const WebCore::IDBRequestData&, const WebCore::IDBKeyData&, const IPC::DataReference& value, unsigned overwriteMode);
    void getRecord(const WebCore::IDBRequestData&, const WebCore::IDBKeyRangeData&);
    void getCount(const WebCore::IDBRequestData&, const WebCore::IDBKeyRangeData&);
    void deleteRecord(const WebCore::IDBRequestData&, const WebCore::IDBKeyRangeData&);
    void openCursor(const WebCore::IDBRequestData&, const WebCore::IDBCursorInfo&);
    void iterateCursor(const WebCore::IDBRequestData&, const WebCore::IDBKeyData&, unsigned long count);

    void establishTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBTransactionInfo&);
    void databaseConnectionClosed(uint64_t databaseConnectionIdentifier);
    void abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& transactionIdentifier);
    void didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& requestIdentifier);

    void disconnectedFromWebProcess();

    void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&);

private:
    WebIDBConnectionToClient(DatabaseToWebProcessConnection&, uint64_t serverConnectionIdentifier);

    IPC::Connection* messageSenderConnection() override final;

    Ref<DatabaseToWebProcessConnection> m_connection;

    uint64_t m_identifier;
    RefPtr<WebCore::IDBServer::IDBConnectionToClient> m_connectionToClient;
};

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)
#endif // WebIDBConnectionToClient_h
