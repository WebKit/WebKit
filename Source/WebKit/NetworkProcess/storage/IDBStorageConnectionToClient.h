/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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

#include "Connection.h"
#include <WebCore/IDBConnectionToClient.h>
#include <WebCore/IDBConnectionToClientDelegate.h>

namespace WebKit {

class IDBStorageConnectionToClient final : public WebCore::IDBServer::IDBConnectionToClientDelegate {
    WTF_MAKE_FAST_ALLOCATED;
public:
    IDBStorageConnectionToClient(IPC::Connection::UniqueID, WebCore::IDBConnectionIdentifier);
    ~IDBStorageConnectionToClient();

    WebCore::IDBConnectionIdentifier identifier() const final { return m_identifier; }
    IPC::Connection::UniqueID ipcConnection() const { return m_connection; }
    WebCore::IDBServer::IDBConnectionToClient& connectionToClient();

private:
    template<class MessageType> void didGetResult(const WebCore::IDBResultData&);

    // IDBConnectionToClientDelegate
    void didDeleteDatabase(const WebCore::IDBResultData&) final;
    void didOpenDatabase(const WebCore::IDBResultData&) final;
    void didAbortTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError&) final;
    void didCommitTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError&) final;
    void didStartTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBError&) final;
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
    void didGetAllDatabaseNamesAndVersions(const WebCore::IDBResourceIdentifier&, Vector<WebCore::IDBDatabaseNameAndVersion>&&) final;
    void notifyOpenDBRequestBlocked(const WebCore::IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion) final;
    void fireVersionChangeEvent(WebCore::IDBServer::UniqueIDBDatabaseConnection&, const WebCore::IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion) final;
    void didCloseFromServer(WebCore::IDBServer::UniqueIDBDatabaseConnection&, const WebCore::IDBError&) final;

    IPC::Connection::UniqueID m_connection;
    WebCore::IDBConnectionIdentifier m_identifier;
    Ref<WebCore::IDBServer::IDBConnectionToClient> m_connectionToClient;
};

} // namespace WebKit
