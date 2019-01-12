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

#pragma once

#if ENABLE(INDEXED_DATABASE)

#include "MessageSender.h"
#include "NetworkConnectionToWebProcess.h"
#include <WebCore/IDBConnectionToClient.h>
#include <pal/SessionID.h>

namespace WebCore {
class IDBCursorInfo;
class IDBIndexInfo;
class IDBKeyData;
class IDBObjectStoreInfo;
class IDBRequestData;
class IDBTransactionInfo;
class IDBValue;
class SerializedScriptValue;
struct IDBGetAllRecordsData;
struct IDBGetRecordData;
struct IDBIterateCursorData;
struct IDBKeyRangeData;
struct SecurityOriginData;
}

namespace WebKit {

class NetworkProcess;

class WebIDBConnectionToClient final : public WebCore::IDBServer::IDBConnectionToClientDelegate, public IPC::MessageSender, public RefCounted<WebIDBConnectionToClient> {
public:
    static Ref<WebIDBConnectionToClient> create(NetworkProcess&, IPC::Connection&, uint64_t serverConnectionIdentifier, PAL::SessionID);

    virtual ~WebIDBConnectionToClient();

    WebCore::IDBServer::IDBConnectionToClient& connectionToClient();
    uint64_t identifier() const final { return m_identifier; }
    uint64_t messageSenderDestinationID() final { return m_identifier; }

    // IDBConnectionToClientDelegate
    void didDeleteDatabase(const WebCore::IDBResultData&) final;
    void didOpenDatabase(const WebCore::IDBResultData&) final;
    void didAbortTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError&) final;
    void didCommitTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError&) final;
    void didCreateObjectStore(const WebCore::IDBResultData&) final;
    void didDeleteObjectStore(const WebCore::IDBResultData&) final;
    void didRenameObjectStore(const WebCore::IDBResultData&) final;
    void didClearObjectStore(const WebCore::IDBResultData&) final;
    void didCreateIndex(const WebCore::IDBResultData&) final;
    void didDeleteIndex(const WebCore::IDBResultData&) final;
    void didRenameIndex(const WebCore::IDBResultData&) final;
    void didPutOrAdd(const WebCore::IDBResultData&) final;
    void didGetRecord(const WebCore::IDBResultData&) final;
    void didGetAllRecords(const WebCore::IDBResultData&) final;
    void didGetCount(const WebCore::IDBResultData&) final;
    void didDeleteRecord(const WebCore::IDBResultData&) final;
    void didOpenCursor(const WebCore::IDBResultData&) final;
    void didIterateCursor(const WebCore::IDBResultData&) final;

    void fireVersionChangeEvent(WebCore::IDBServer::UniqueIDBDatabaseConnection&, const WebCore::IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion) final;
    void didStartTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError&) final;
    void didCloseFromServer(WebCore::IDBServer::UniqueIDBDatabaseConnection&, const WebCore::IDBError&) final;
    void notifyOpenDBRequestBlocked(const WebCore::IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion) final;

    void didGetAllDatabaseNames(uint64_t callbackID, const Vector<String>& databaseNames) final;

    void ref() override { RefCounted<WebIDBConnectionToClient>::ref(); }
    void deref() override { RefCounted<WebIDBConnectionToClient>::deref(); }

    // Messages received from WebProcess
    void deleteDatabase(const WebCore::IDBRequestData&);
    void openDatabase(const WebCore::IDBRequestData&);
    void abortTransaction(const WebCore::IDBResourceIdentifier&);
    void commitTransaction(const WebCore::IDBResourceIdentifier&);
    void didFinishHandlingVersionChangeTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier&);
    void createObjectStore(const WebCore::IDBRequestData&, const WebCore::IDBObjectStoreInfo&);
    void deleteObjectStore(const WebCore::IDBRequestData&, const String& objectStoreName);
    void renameObjectStore(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier, const String& newName);
    void clearObjectStore(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier);
    void createIndex(const WebCore::IDBRequestData&, const WebCore::IDBIndexInfo&);
    void deleteIndex(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier, const String& indexName);
    void renameIndex(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName);
    void putOrAdd(const WebCore::IDBRequestData&, const WebCore::IDBKeyData&, const WebCore::IDBValue&, unsigned overwriteMode);
    void getRecord(const WebCore::IDBRequestData&, const WebCore::IDBGetRecordData&);
    void getAllRecords(const WebCore::IDBRequestData&, const WebCore::IDBGetAllRecordsData&);
    void getCount(const WebCore::IDBRequestData&, const WebCore::IDBKeyRangeData&);
    void deleteRecord(const WebCore::IDBRequestData&, const WebCore::IDBKeyRangeData&);
    void openCursor(const WebCore::IDBRequestData&, const WebCore::IDBCursorInfo&);
    void iterateCursor(const WebCore::IDBRequestData&, const WebCore::IDBIterateCursorData&);

    void establishTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBTransactionInfo&);
    void databaseConnectionPendingClose(uint64_t databaseConnectionIdentifier);
    void databaseConnectionClosed(uint64_t databaseConnectionIdentifier);
    void abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& transactionIdentifier);
    void didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& requestIdentifier);
    void openDBRequestCancelled(const WebCore::IDBRequestData&);
    void confirmDidCloseFromServer(uint64_t databaseConnectionIdentifier);

    void getAllDatabaseNames(uint64_t serverConnectionIdentifier, const WebCore::SecurityOriginData& topOrigin, const WebCore::SecurityOriginData& openingOrigin, uint64_t callbackID);

    void disconnectedFromWebProcess();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

private:
    WebIDBConnectionToClient(NetworkProcess&, IPC::Connection&, uint64_t serverConnectionIdentifier, PAL::SessionID);

    IPC::Connection* messageSenderConnection() final;

    template<class MessageType> void handleGetResult(const WebCore::IDBResultData&);

    Ref<IPC::Connection> m_connection;
    Ref<NetworkProcess> m_networkProcess;

    uint64_t m_identifier;
    PAL::SessionID m_sessionID;
    RefPtr<WebCore::IDBServer::IDBConnectionToClient> m_connectionToClient;
};

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)
