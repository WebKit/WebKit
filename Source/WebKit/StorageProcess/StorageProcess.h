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

#include "ChildProcess.h"
#include "SandboxExtension.h"
#include <WebCore/IDBBackingStore.h>
#include <WebCore/IDBServer.h>
#include <WebCore/SessionID.h>
#include <WebCore/UniqueIDBDatabase.h>
#include <wtf/CrossThreadTask.h>
#include <wtf/Function.h>

namespace WebCore {
struct SecurityOriginData;
}

namespace WebKit {

class StorageToWebProcessConnection;
enum class WebsiteDataType;
struct StorageProcessCreationParameters;

class StorageProcess : public ChildProcess
#if ENABLE(INDEXED_DATABASE)
    , public WebCore::IDBServer::IDBBackingStoreTemporaryFileHandler
#endif
{
    WTF_MAKE_NONCOPYABLE(StorageProcess);
    friend class NeverDestroyed<StorageProcess>;
public:
    static StorageProcess& singleton();
    ~StorageProcess();

#if ENABLE(INDEXED_DATABASE)
    WebCore::IDBServer::IDBServer& idbServer(WebCore::SessionID);
#endif

    WorkQueue& queue() { return m_queue.get(); }

    void postStorageTask(CrossThreadTask&&);

#if ENABLE(INDEXED_DATABASE)
    // WebCore::IDBServer::IDBBackingStoreFileHandler
    void prepareForAccessToTemporaryFile(const String& path) final;
    void accessToTemporaryFileComplete(const String& path) final;
#endif

#if ENABLE(SANDBOX_EXTENSIONS)
    void getSandboxExtensionsForBlobFiles(const Vector<String>& filenames, WTF::Function<void (SandboxExtension::HandleArray&&)>&& completionHandler);
#endif

private:
    StorageProcess();

    // ChildProcess
    void initializeProcess(const ChildProcessInitializationParameters&) override;
    void initializeProcessName(const ChildProcessInitializationParameters&) override;
    void initializeSandbox(const ChildProcessInitializationParameters&, SandboxInitializationParameters&) override;
    void initializeConnection(IPC::Connection*) override;
    bool shouldTerminate() override;

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didClose(IPC::Connection&) override;
    void didReceiveStorageProcessMessage(IPC::Connection&, IPC::Decoder&);

    // Message Handlers
    void initializeWebsiteDataStore(const StorageProcessCreationParameters&);
    void createStorageToWebProcessConnection();

    void fetchWebsiteData(WebCore::SessionID, OptionSet<WebsiteDataType> websiteDataTypes, uint64_t callbackID);
    void deleteWebsiteData(WebCore::SessionID, OptionSet<WebsiteDataType> websiteDataTypes, std::chrono::system_clock::time_point modifiedSince, uint64_t callbackID);
    void deleteWebsiteDataForOrigins(WebCore::SessionID, OptionSet<WebsiteDataType> websiteDataTypes, const Vector<WebCore::SecurityOriginData>& origins, uint64_t callbackID);
#if ENABLE(SANDBOX_EXTENSIONS)
    void grantSandboxExtensionsForBlobs(const Vector<String>& paths, const SandboxExtension::HandleArray&);
    void didGetSandboxExtensionsForBlobFiles(uint64_t requestID, SandboxExtension::HandleArray&&);
#endif

#if ENABLE(INDEXED_DATABASE)
    Vector<WebCore::SecurityOriginData> indexedDatabaseOrigins(const String& path);
#endif

    // For execution on work queue thread only
    void performNextStorageTask();
    void ensurePathExists(const String&);

    Vector<RefPtr<StorageToWebProcessConnection>> m_databaseToWebProcessConnections;

    Ref<WorkQueue> m_queue;

#if ENABLE(INDEXED_DATABASE)
    HashMap<WebCore::SessionID, String> m_idbDatabasePaths;
    HashMap<WebCore::SessionID, RefPtr<WebCore::IDBServer::IDBServer>> m_idbServers;
#endif
    HashMap<String, RefPtr<SandboxExtension>> m_blobTemporaryFileSandboxExtensions;
    HashMap<uint64_t, WTF::Function<void (SandboxExtension::HandleArray&&)>> m_sandboxExtensionForBlobsCompletionHandlers;

    Deque<CrossThreadTask> m_storageTasks;
    Lock m_storageTaskMutex;
};

} // namespace WebKit
