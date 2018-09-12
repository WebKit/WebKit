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
#include <WebCore/FetchIdentifier.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/ServiceWorkerIdentifier.h>
#include <WebCore/ServiceWorkerTypes.h>
#include <pal/SessionID.h>
#include <wtf/CrossThreadTask.h>
#include <wtf/Function.h>

namespace IPC {
class FormDataReference;
}

namespace WebCore {
class ResourceError;
class SWServer;
class ServiceWorkerRegistrationKey;
struct MessageWithMessagePorts;
struct ServiceWorkerClientIdentifier;
}

namespace WebKit {

class StorageToWebProcessConnection;
class WebSWServerConnection;
class WebSWServerToContextConnection;
enum class WebsiteDataType;
struct StorageProcessCreationParameters;

#if ENABLE(SERVICE_WORKER)
class WebSWOriginStore;
#endif

class StorageProcess : public ChildProcess
{
    WTF_MAKE_NONCOPYABLE(StorageProcess);
    friend NeverDestroyed<StorageProcess>;
public:
    static StorageProcess& singleton();
    static constexpr ProcessType processType = ProcessType::Storage;

    ~StorageProcess();

    void postStorageTask(CrossThreadTask&&);

#if PLATFORM(IOS)
    bool parentProcessHasServiceWorkerEntitlement() const;
#else
    bool parentProcessHasServiceWorkerEntitlement() const { return true; }
#endif

#if ENABLE(SERVICE_WORKER)
    WebSWServerToContextConnection* serverToContextConnectionForOrigin(const WebCore::SecurityOriginData&);
    void createServerToContextConnection(const WebCore::SecurityOriginData&, std::optional<PAL::SessionID>);

    WebCore::SWServer& swServerForSession(PAL::SessionID);
    void registerSWServerConnection(WebSWServerConnection&);
    void unregisterSWServerConnection(WebSWServerConnection&);

    void swContextConnectionMayNoLongerBeNeeded(WebSWServerToContextConnection&);
#endif

    void didReceiveStorageProcessMessage(IPC::Connection&, IPC::Decoder&);

#if ENABLE(SERVICE_WORKER)
    WebSWServerToContextConnection* connectionToContextProcessFromIPCConnection(IPC::Connection&);
    void connectionToContextProcessWasClosed(Ref<WebSWServerToContextConnection>&&);
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

    // Message Handlers
    void initializeWebsiteDataStore(const StorageProcessCreationParameters&);
    void createStorageToWebProcessConnection(bool isServiceWorkerProcess, WebCore::SecurityOriginData&&);

    void destroySession(PAL::SessionID);
    void fetchWebsiteData(PAL::SessionID, OptionSet<WebsiteDataType> websiteDataTypes, uint64_t callbackID);
    void deleteWebsiteData(PAL::SessionID, OptionSet<WebsiteDataType> websiteDataTypes, WallTime modifiedSince, uint64_t callbackID);
    void deleteWebsiteDataForOrigins(PAL::SessionID, OptionSet<WebsiteDataType> websiteDataTypes, const Vector<WebCore::SecurityOriginData>& origins, uint64_t callbackID);

#if ENABLE(SERVICE_WORKER)
    void didReceiveFetchResponse(WebCore::SWServerConnectionIdentifier, WebCore::FetchIdentifier, const WebCore::ResourceResponse&);
    void didReceiveFetchData(WebCore::SWServerConnectionIdentifier, WebCore::FetchIdentifier, const IPC::DataReference&, int64_t encodedDataLength);
    void didReceiveFetchFormData(WebCore::SWServerConnectionIdentifier, WebCore::FetchIdentifier, const IPC::FormDataReference&);
    void didFinishFetch(WebCore::SWServerConnectionIdentifier, WebCore::FetchIdentifier);
    void didFailFetch(WebCore::SWServerConnectionIdentifier, WebCore::FetchIdentifier, const WebCore::ResourceError&);
    void didNotHandleFetch(WebCore::SWServerConnectionIdentifier, WebCore::FetchIdentifier);

    void postMessageToServiceWorkerClient(const WebCore::ServiceWorkerClientIdentifier& destinationIdentifier, WebCore::MessageWithMessagePorts&&, WebCore::ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin);
    void postMessageToServiceWorker(WebCore::ServiceWorkerIdentifier destination, WebCore::MessageWithMessagePorts&&, const WebCore::ServiceWorkerOrClientIdentifier& source, WebCore::SWServerConnectionIdentifier);

    void disableServiceWorkerProcessTerminationDelay();

    WebSWOriginStore& swOriginStoreForSession(PAL::SessionID);
    WebSWOriginStore* existingSWOriginStoreForSession(PAL::SessionID) const;
    bool needsServerToContextConnectionForOrigin(const WebCore::SecurityOriginData&) const;
#endif

    // For execution on work queue thread only
    void performNextStorageTask();
    void ensurePathExists(const String&);

    Vector<Ref<StorageToWebProcessConnection>> m_storageToWebProcessConnections;

    Ref<WorkQueue> m_queue;

    Deque<CrossThreadTask> m_storageTasks;
    Lock m_storageTaskMutex;
    
#if ENABLE(SERVICE_WORKER)
    void didCreateWorkerContextProcessConnection(const IPC::Attachment&);

    HashMap<WebCore::SecurityOriginData, RefPtr<WebSWServerToContextConnection>> m_serverToContextConnections;
    bool m_waitingForServerToContextProcessConnection { false };
    bool m_shouldDisableServiceWorkerProcessTerminationDelay { false };
    HashMap<PAL::SessionID, String> m_swDatabasePaths;
    HashMap<PAL::SessionID, std::unique_ptr<WebCore::SWServer>> m_swServers;
    HashMap<WebCore::SWServerConnectionIdentifier, WebSWServerConnection*> m_swServerConnections;
#endif
};

} // namespace WebKit
