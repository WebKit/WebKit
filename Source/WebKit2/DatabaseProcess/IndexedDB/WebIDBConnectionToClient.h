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

namespace WebKit {

class WebIDBConnectionToClient final : public WebCore::IDBServer::IDBConnectionToClientDelegate, public IPC::MessageSender, public RefCounted<WebIDBConnectionToClient> {
public:
    static Ref<WebIDBConnectionToClient> create(DatabaseToWebProcessConnection&, uint64_t serverConnectionIdentifier);

    virtual ~WebIDBConnectionToClient();

    WebCore::IDBServer::IDBConnectionToClient& connectionToClient();
    virtual uint64_t identifier() const override final { return m_identifier; }
    virtual uint64_t messageSenderDestinationID() override final { return m_identifier; }

    virtual void didDeleteDatabase(const WebCore::IDBResultData&) override final;
    virtual void didOpenDatabase(const WebCore::IDBResultData&) override final;
    virtual void didAbortTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError&) override final;
    virtual void didCommitTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError&) override final;
    virtual void didCreateObjectStore(const WebCore::IDBResultData&) override final;
    virtual void didDeleteObjectStore(const WebCore::IDBResultData&) override final;
    virtual void didClearObjectStore(const WebCore::IDBResultData&) override final;
    virtual void didCreateIndex(const WebCore::IDBResultData&) override final;
    virtual void didDeleteIndex(const WebCore::IDBResultData&) override final;
    virtual void didPutOrAdd(const WebCore::IDBResultData&) override final;
    virtual void didGetRecord(const WebCore::IDBResultData&) override final;
    virtual void didGetCount(const WebCore::IDBResultData&) override final;
    virtual void didDeleteRecord(const WebCore::IDBResultData&) override final;
    virtual void didOpenCursor(const WebCore::IDBResultData&) override final;
    virtual void didIterateCursor(const WebCore::IDBResultData&) override final;

    virtual void fireVersionChangeEvent(WebCore::IDBServer::UniqueIDBDatabaseConnection&, const WebCore::IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion) override final;
    virtual void didStartTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError&) override final;
    virtual void notifyOpenDBRequestBlocked(const WebCore::IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion) override final;

    virtual void ref() override { RefCounted<WebIDBConnectionToClient>::ref(); }
    virtual void deref() override { RefCounted<WebIDBConnectionToClient>::deref(); }

    void disconnectedFromWebProcess();

private:
    WebIDBConnectionToClient(DatabaseToWebProcessConnection&, uint64_t serverConnectionIdentifier);

    virtual IPC::Connection* messageSenderConnection() override final;
    Ref<DatabaseToWebProcessConnection> m_connection;

    uint64_t m_identifier;
    RefPtr<WebCore::IDBServer::IDBConnectionToClient> m_connectionToClient;
};

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)
#endif // WebIDBConnectionToClient_h
