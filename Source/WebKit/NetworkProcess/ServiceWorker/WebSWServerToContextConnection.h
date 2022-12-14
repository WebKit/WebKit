/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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

#if ENABLE(SERVICE_WORKER)

#include "MessageReceiver.h"
#include "MessageSender.h"
#include "ServiceWorkerDownloadTask.h"
#include "ServiceWorkerFetchTask.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/SWServerToContextConnection.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/URLHash.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
struct FetchOptions;
struct MessageWithMessagePorts;
class ResourceRequest;
}

namespace IPC {
class FormDataReference;
}

namespace PAL {
class SessionID;
}

namespace WebKit {

class NetworkConnectionToWebProcess;
class WebSWServerConnection;

class WebSWServerToContextConnection final: public WebCore::SWServerToContextConnection, public IPC::MessageSender, public IPC::MessageReceiver {
public:
    WebSWServerToContextConnection(NetworkConnectionToWebProcess&, WebPageProxyIdentifier, WebCore::RegistrableDomain&&, std::optional<WebCore::ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, WebCore::SWServer&);
    ~WebSWServerToContextConnection();

    IPC::Connection& ipcConnection() const;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    void startFetch(ServiceWorkerFetchTask&);
    void cancelFetch(WebCore::SWServerConnectionIdentifier, WebCore::FetchIdentifier, WebCore::ServiceWorkerIdentifier);

    void didReceiveFetchTaskMessage(IPC::Connection&, IPC::Decoder&);

    void setThrottleState(bool isThrottleable);
    bool isThrottleable() const { return m_isThrottleable; }

    void registerFetch(ServiceWorkerFetchTask&);
    void unregisterFetch(ServiceWorkerFetchTask&);
    void registerDownload(ServiceWorkerDownloadTask&);
    void unregisterDownload(ServiceWorkerDownloadTask&);

    WebCore::ProcessIdentifier webProcessIdentifier() const final;
    NetworkProcess& networkProcess() { return m_connection.networkProcess(); }

private:
    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;

    void postMessageToServiceWorkerClient(const WebCore::ScriptExecutionContextIdentifier& destinationIdentifier, const WebCore::MessageWithMessagePorts&, WebCore::ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin);
    void skipWaiting(uint64_t requestIdentifier, WebCore::ServiceWorkerIdentifier);

    // Messages to the SW host WebProcess
    void installServiceWorkerContext(const WebCore::ServiceWorkerContextData&, const WebCore::ServiceWorkerData&, const String& userAgent, WebCore::WorkerThreadMode) final;
    void updateAppInitiatedValue(WebCore::ServiceWorkerIdentifier, WebCore::LastNavigationWasAppInitiated) final;
    void fireInstallEvent(WebCore::ServiceWorkerIdentifier) final;
    void fireActivateEvent(WebCore::ServiceWorkerIdentifier) final;
    void terminateWorker(WebCore::ServiceWorkerIdentifier) final;
    void didSaveScriptsToDisk(WebCore::ServiceWorkerIdentifier, const WebCore::ScriptBuffer&, const MemoryCompactRobinHoodHashMap<URL, WebCore::ScriptBuffer>& importedScripts) final;
    void matchAllCompleted(uint64_t requestIdentifier, const Vector<WebCore::ServiceWorkerClientData>&) final;
    void firePushEvent(WebCore::ServiceWorkerIdentifier, const std::optional<Vector<uint8_t>>&, CompletionHandler<void(bool)>&&) final;
    void fireNotificationEvent(WebCore::ServiceWorkerIdentifier, const WebCore::NotificationData&, WebCore::NotificationEventType, CompletionHandler<void(bool)>&&) final;
    void close() final;
    void focus(WebCore::ScriptExecutionContextIdentifier, CompletionHandler<void(std::optional<WebCore::ServiceWorkerClientData>&&)>&&);
    void navigate(WebCore::ScriptExecutionContextIdentifier, WebCore::ServiceWorkerIdentifier, const URL&, CompletionHandler<void(Expected<std::optional<WebCore::ServiceWorkerClientData>, WebCore::ExceptionData>&&)>&&);

    void connectionIsNoLongerNeeded() final;
    void terminateDueToUnresponsiveness() final;
    void openWindow(WebCore::ServiceWorkerIdentifier, const URL&, OpenWindowCallback&&) final;

    void connectionClosed();

    NetworkConnectionToWebProcess& m_connection;
    HashMap<WebCore::FetchIdentifier, WeakPtr<ServiceWorkerFetchTask>> m_ongoingFetches;
    HashMap<WebCore::FetchIdentifier, ThreadSafeWeakPtr<ServiceWorkerDownloadTask>> m_ongoingDownloads;
    bool m_isThrottleable { true };
    WebPageProxyIdentifier m_webPageProxyID;
    size_t m_processingFunctionalEventCount { 0 };
}; // class WebSWServerToContextConnection

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)

