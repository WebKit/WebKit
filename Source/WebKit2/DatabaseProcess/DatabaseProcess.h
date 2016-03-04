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

#ifndef DatabaseProcess_h
#define DatabaseProcess_h

#if ENABLE(DATABASE_PROCESS)

#include "ChildProcess.h"
#include <WebCore/IDBServer.h>
#include <WebCore/UniqueIDBDatabase.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {
class CrossThreadTask;
class SessionID;
struct SecurityOriginData;
}

namespace WebKit {

class DatabaseToWebProcessConnection;
enum class WebsiteDataType;
struct DatabaseProcessCreationParameters;

class DatabaseProcess : public ChildProcess {
    WTF_MAKE_NONCOPYABLE(DatabaseProcess);
    friend class NeverDestroyed<DatabaseProcess>;
public:
    static DatabaseProcess& singleton();
    ~DatabaseProcess();

#if ENABLE(INDEXED_DATABASE)
    const String& indexedDatabaseDirectory() const { return m_indexedDatabaseDirectory; }

    void ensureIndexedDatabaseRelativePathExists(const String&);
    String absoluteIndexedDatabasePathFromDatabaseRelativePath(const String&);

    WebCore::IDBServer::IDBServer& idbServer();
#endif

    WorkQueue& queue() { return m_queue.get(); }

    void postDatabaseTask(std::unique_ptr<WebCore::CrossThreadTask>);

private:
    DatabaseProcess();

    // ChildProcess
    void initializeProcess(const ChildProcessInitializationParameters&) override;
    void initializeProcessName(const ChildProcessInitializationParameters&) override;
    void initializeSandbox(const ChildProcessInitializationParameters&, SandboxInitializationParameters&) override;
    void initializeConnection(IPC::Connection*) override;
    bool shouldTerminate() override;

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;
    void didClose(IPC::Connection&) override;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName) override;
    IPC::ProcessType localProcessType() override { return IPC::ProcessType::Database; }
    IPC::ProcessType remoteProcessType() override { return IPC::ProcessType::UI; }
    void didReceiveDatabaseProcessMessage(IPC::Connection&, IPC::MessageDecoder&);

    // Message Handlers
    void initializeDatabaseProcess(const DatabaseProcessCreationParameters&);
    void createDatabaseToWebProcessConnection();

    void fetchWebsiteData(WebCore::SessionID, OptionSet<WebsiteDataType> websiteDataTypes, uint64_t callbackID);
    void deleteWebsiteData(WebCore::SessionID, OptionSet<WebsiteDataType> websiteDataTypes, std::chrono::system_clock::time_point modifiedSince, uint64_t callbackID);
    void deleteWebsiteDataForOrigins(WebCore::SessionID, OptionSet<WebsiteDataType> websiteDataTypes, const Vector<WebCore::SecurityOriginData>& origins, uint64_t callbackID);

#if ENABLE(INDEXED_DATABASE)
    Vector<RefPtr<WebCore::SecurityOrigin>> indexedDatabaseOrigins();
    void deleteIndexedDatabaseEntriesForOrigins(const Vector<RefPtr<WebCore::SecurityOrigin>>&);
    void deleteIndexedDatabaseEntriesModifiedSince(std::chrono::system_clock::time_point modifiedSince);
#endif

    // For execution on work queue thread only
    void performNextDatabaseTask();
    void ensurePathExists(const String&);

    Vector<RefPtr<DatabaseToWebProcessConnection>> m_databaseToWebProcessConnections;

    Ref<WorkQueue> m_queue;

#if ENABLE(INDEXED_DATABASE)
    String m_indexedDatabaseDirectory;

    RefPtr<WebCore::IDBServer::IDBServer> m_idbServer;
#endif

    Deque<std::unique_ptr<WebCore::CrossThreadTask>> m_databaseTasks;
    Lock m_databaseTaskMutex;
};

} // namespace WebKit

#endif // ENABLE(DATABASE_PROCESS)

#endif // DatabaseProcess_h
