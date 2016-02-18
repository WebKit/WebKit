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

#ifndef WebIDBConnectionToServer_h
#define WebIDBConnectionToServer_h

#if ENABLE(INDEXED_DATABASE)

#include "MessageSender.h"
#include <WebCore/IDBConnectionToServer.h>

namespace WebKit {

class WebIDBConnectionToServer final : public WebCore::IDBClient::IDBConnectionToServerDelegate, public IPC::MessageSender, public RefCounted<WebIDBConnectionToServer> {
public:
    static Ref<WebIDBConnectionToServer> create();

    virtual ~WebIDBConnectionToServer();

    WebCore::IDBClient::IDBConnectionToServer& coreConnectionToServer();
    virtual uint64_t identifier() const override final { return m_identifier; }
    virtual uint64_t messageSenderDestinationID() override final { return m_identifier; }

    // IDBConnectionToServerDelegate
    virtual void deleteDatabase(WebCore::IDBRequestData&) override final;
    virtual void openDatabase(WebCore::IDBRequestData&) override final;
    virtual void abortTransaction(WebCore::IDBResourceIdentifier&) override final;
    virtual void commitTransaction(WebCore::IDBResourceIdentifier&) override final;
    virtual void didFinishHandlingVersionChangeTransaction(WebCore::IDBResourceIdentifier&) override final;
    virtual void createObjectStore(const WebCore::IDBRequestData&, const WebCore::IDBObjectStoreInfo&) override final;
    virtual void deleteObjectStore(const WebCore::IDBRequestData&, const String& objectStoreName) override final;
    virtual void clearObjectStore(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier) override final;
    virtual void createIndex(const WebCore::IDBRequestData&, const WebCore::IDBIndexInfo&) override final;
    virtual void deleteIndex(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier, const String& indexName) override final;
    virtual void putOrAdd(const WebCore::IDBRequestData&, WebCore::IDBKey*, WebCore::SerializedScriptValue&, const WebCore::IndexedDB::ObjectStoreOverwriteMode) override final;
    virtual void getRecord(const WebCore::IDBRequestData&, const WebCore::IDBKeyRangeData&) override final;
    virtual void getCount(const WebCore::IDBRequestData&, const WebCore::IDBKeyRangeData&) override final;
    virtual void deleteRecord(const WebCore::IDBRequestData&, const WebCore::IDBKeyRangeData&) override final;
    virtual void openCursor(const WebCore::IDBRequestData&, const WebCore::IDBCursorInfo&) override final;
    virtual void iterateCursor(const WebCore::IDBRequestData&, const WebCore::IDBKeyData&, unsigned long count) override final;
    virtual void establishTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBTransactionInfo&) override final;
    virtual void databaseConnectionClosed(uint64_t databaseConnectionIdentifier) override final;
    virtual void abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& transactionIdentifier) override final;
    virtual void didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& requestIdentifier) override final;

    virtual void ref() override { RefCounted<WebIDBConnectionToServer>::ref(); }
    virtual void deref() override { RefCounted<WebIDBConnectionToServer>::deref(); }

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
    void didGetRecord(const WebCore::IDBResultData&);
    void didGetCount(const WebCore::IDBResultData&);
    void didDeleteRecord(const WebCore::IDBResultData&);
    void didOpenCursor(const WebCore::IDBResultData&);
    void didIterateCursor(const WebCore::IDBResultData&);
    void fireVersionChangeEvent(uint64_t uniqueDatabaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion);
    void didStartTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError&);
    void notifyOpenDBRequestBlocked(const WebCore::IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion);

    void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&);

private:
    WebIDBConnectionToServer();

    virtual IPC::Connection* messageSenderConnection() override final;

    uint64_t m_identifier;
    bool m_isOpenInServer { false };
    RefPtr<WebCore::IDBClient::IDBConnectionToServer> m_connectionToServer;
};

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)
#endif // WebIDBConnectionToServer_h
