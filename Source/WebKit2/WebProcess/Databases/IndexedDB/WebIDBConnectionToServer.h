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
#include "SandboxExtension.h"
#include <WebCore/IDBConnectionToServer.h>

namespace WebKit {

class WebIDBResult;

class WebIDBConnectionToServer final : public WebCore::IDBClient::IDBConnectionToServerDelegate, public IPC::MessageSender, public RefCounted<WebIDBConnectionToServer> {
public:
    static Ref<WebIDBConnectionToServer> create();

    virtual ~WebIDBConnectionToServer();

    WebCore::IDBClient::IDBConnectionToServer& coreConnectionToServer();
    uint64_t identifier() const final { return m_identifier; }
    uint64_t messageSenderDestinationID() final { return m_identifier; }

    // IDBConnectionToServerDelegate
    void deleteDatabase(const WebCore::IDBRequestData&) final;
    void openDatabase(const WebCore::IDBRequestData&) final;
    void abortTransaction(const WebCore::IDBResourceIdentifier&) final;
    void commitTransaction(const WebCore::IDBResourceIdentifier&) final;
    void didFinishHandlingVersionChangeTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier&) final;
    void createObjectStore(const WebCore::IDBRequestData&, const WebCore::IDBObjectStoreInfo&) final;
    void deleteObjectStore(const WebCore::IDBRequestData&, const String& objectStoreName) final;
    void clearObjectStore(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier) final;
    void createIndex(const WebCore::IDBRequestData&, const WebCore::IDBIndexInfo&) final;
    void deleteIndex(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier, const String& indexName) final;
    void putOrAdd(const WebCore::IDBRequestData&, const WebCore::IDBKeyData&, const WebCore::IDBValue&, const WebCore::IndexedDB::ObjectStoreOverwriteMode) final;
    void getRecord(const WebCore::IDBRequestData&, const WebCore::IDBGetRecordData&) final;
    void getCount(const WebCore::IDBRequestData&, const WebCore::IDBKeyRangeData&) final;
    void deleteRecord(const WebCore::IDBRequestData&, const WebCore::IDBKeyRangeData&) final;
    void openCursor(const WebCore::IDBRequestData&, const WebCore::IDBCursorInfo&) final;
    void iterateCursor(const WebCore::IDBRequestData&, const WebCore::IDBKeyData&, unsigned long count) final;
    void establishTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBTransactionInfo&) final;
    void databaseConnectionClosed(uint64_t databaseConnectionIdentifier) final;
    void abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& transactionIdentifier) final;
    void didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& requestIdentifier) final;
    void openDBRequestCancelled(const WebCore::IDBRequestData&) final;
    void confirmDidCloseFromServer(uint64_t databaseConnectionIdentifier) final;

    void getAllDatabaseNames(const WebCore::SecurityOriginData& topOrigin, const WebCore::SecurityOriginData& openingOrigin, uint64_t callbackID) final;

    void ref() override { RefCounted<WebIDBConnectionToServer>::ref(); }
    void deref() override { RefCounted<WebIDBConnectionToServer>::deref(); }

    // Messages received from DatabaseProcess
    void didDeleteDatabase(const WebCore::IDBResultData&);
    void didOpenDatabase(const WebCore::IDBResultData&);
    void didAbortTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError&);
    void didCommitTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError&);
    void didCreateObjectStore(const WebCore::IDBResultData&);
    void didDeleteObjectStore(const WebCore::IDBResultData&);
    void didClearObjectStore(const WebCore::IDBResultData&);
    void didCreateIndex(const WebCore::IDBResultData&);
    void didDeleteIndex(const WebCore::IDBResultData&);
    void didPutOrAdd(const WebCore::IDBResultData&);
    void didGetRecord(const WebIDBResult&);
    void didGetCount(const WebCore::IDBResultData&);
    void didDeleteRecord(const WebCore::IDBResultData&);
    void didOpenCursor(const WebIDBResult&);
    void didIterateCursor(const WebIDBResult&);
    void fireVersionChangeEvent(uint64_t uniqueDatabaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion);
    void didStartTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError&);
    void didCloseFromServer(uint64_t databaseConnectionIdentifier, const WebCore::IDBError&);
    void notifyOpenDBRequestBlocked(const WebCore::IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion);
    void didGetAllDatabaseNames(uint64_t callbackID, const Vector<String>& databaseNames);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    void connectionToServerLost();

private:
    WebIDBConnectionToServer();

    IPC::Connection* messageSenderConnection() final;

    uint64_t m_identifier;
    bool m_isOpenInServer { false };
    RefPtr<WebCore::IDBClient::IDBConnectionToServer> m_connectionToServer;
};

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)
