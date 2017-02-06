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

#pragma once

#if ENABLE(DATABASE_PROCESS)

#include "ChildProcess.h"
#include "SandboxExtension.h"
#include <WebCore/IDBBackingStore.h>
#include <WebCore/IDBServer.h>
#include <WebCore/UniqueIDBDatabase.h>
#include <wtf/CrossThreadTask.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {
class SessionID;
struct SecurityOriginData;
}

namespace WebKit {

class DatabaseToWebProcessConnection;
enum class WebsiteDataType;
struct DatabaseProcessCreationParameters;

class DatabaseProcess : public ChildProcess
#if ENABLE(INDEXED_DATABASE)
    , public WebCore::IDBServer::IDBBackingStoreTemporaryFileHandler
#endif
{
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

    void postDatabaseTask(CrossThreadTask&&);

#if ENABLE(INDEXED_DATABASE)
    // WebCore::IDBServer::IDBBackingStoreFileHandler
    void prepareForAccessToTemporaryFile(const String& path) final;
    void accessToTemporaryFileComplete(const String& path) final;
#endif

#if ENABLE(SANDBOX_EXTENSIONS)
    void getSandboxExtensionsForBlobFiles(const Vector<String>& filenames, std::function<void (SandboxExtension::HandleArray&&)> completionHandler);
#endif

private:
    DatabaseProcess();

    // ChildProcess
    void initializeProcess(const ChildProcessInitializationParameters&) override;
    void initializeProcessName(const ChildProcessInitializationParameters&) override;
    void initializeSandbox(const ChildProcessInitializationParameters&, SandboxInitializationParameters&) override;
    void initializeConnection(IPC::Connection*) override;
    bool shouldTerminate() override;

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didClose(IPC::Connection&) override;
    void didReceiveDatabaseProcessMessage(IPC::Connection&, IPC::Decoder&);

    // Message Handlers
    void initializeDatabaseProcess(const DatabaseProcessCreationParameters&);
    void createDatabaseToWebProcessConnection();

    void fetchWebsiteData(WebCore::SessionID, OptionSet<WebsiteDataType> websiteDataTypes, uint64_t callbackID);
    void deleteWebsiteData(WebCore::SessionID, OptionSet<WebsiteDataType> websiteDataTypes, std::chrono::system_clock::time_point modifiedSince, uint64_t callbackID);
    void deleteWebsiteDataForOrigins(WebCore::SessionID, OptionSet<WebsiteDataType> websiteDataTypes, const Vector<WebCore::SecurityOriginData>& origins, uint64_t callbackID);
#if ENABLE(SANDBOX_EXTENSIONS)
    void grantSandboxExtensionsForBlobs(const Vector<String>& paths, const SandboxExtension::HandleArray&);
    void didGetSandboxExtensionsForBlobFiles(uint64_t requestID, SandboxExtension::HandleArray&&);
#endif

#if ENABLE(INDEXED_DATABASE)
    Vector<WebCore::SecurityOriginData> indexedDatabaseOrigins();
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
    HashMap<String, RefPtr<SandboxExtension>> m_blobTemporaryFileSandboxExtensions;
    HashMap<uint64_t, std::function<void (SandboxExtension::HandleArray&&)>> m_sandboxExtensionForBlobsCompletionHandlers;

    Deque<CrossThreadTask> m_databaseTasks;
    Lock m_databaseTaskMutex;
};

} // namespace WebKit

#endif // ENABLE(DATABASE_PROCESS)
