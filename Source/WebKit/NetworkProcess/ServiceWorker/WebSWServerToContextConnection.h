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

#include "MessageReceiver.h"
#include "MessageSender.h"
#include "ServiceWorkerDownloadTask.h"
#include "ServiceWorkerFetchTask.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/SWServerToContextConnection.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/URLHash.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
struct FetchOptions;
struct MessageWithMessagePorts;
class ResourceRequest;
class Site;
}

namespace IPC {
class FormDataReference;
}

namespace PAL {
class SessionID;
}

namespace WebKit {

class NetworkConnectionToWebProcess;
class NetworkProcess;
class ServiceWorkerDownloadTask;
class WebSWServerConnection;

class WebSWServerToContextConnection final: public WebCore::SWServerToContextConnection, public IPC::MessageSender, public IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(WebSWServerToContextConnection);
public:
    USING_CAN_MAKE_WEAKPTR(WebCore::SWServerToContextConnection);

    static Ref<WebSWServerToContextConnection> create(NetworkConnectionToWebProcess&, WebPageProxyIdentifier, WebCore::Site&&, std::optional<WebCore::ScriptExecutionContextIdentifier>, WebCore::SWServer&);
    ~WebSWServerToContextConnection();

    void stop();

    RefPtr<IPC::Connection> protectedIPCConnection() const;
    IPC::Connection* ipcConnection() const;

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

    WebCore::ProcessIdentifier webProcessIdentifier() const final { return m_webProcessIdentifier; }
    NetworkProcess* networkProcess();
    RefPtr<NetworkProcess> protectedNetworkProcess();

    void didFinishInstall(const std::optional<WebCore::ServiceWorkerJobDataIdentifier>&, WebCore::ServiceWorkerIdentifier, bool wasSuccessful);
    void didFinishActivation(WebCore::ServiceWorkerIdentifier);

    void terminateIdleServiceWorkers();

private:
    WebSWServerToContextConnection(NetworkConnectionToWebProcess&, WebPageProxyIdentifier, WebCore::Site&&, std::optional<WebCore::ScriptExecutionContextIdentifier>, WebCore::SWServer&);

    RefPtr<NetworkConnectionToWebProcess> protectedConnection() const;

    template<typename T> void sendToParentProcess(T&&);
    template<typename T, typename C> void sendWithAsyncReplyToParentProcess(T&&, C&&);

    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;

    void postMessageToServiceWorkerClient(const WebCore::ScriptExecutionContextIdentifier& destinationIdentifier, const WebCore::MessageWithMessagePorts&, WebCore::ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin);
    void skipWaiting(WebCore::ServiceWorkerIdentifier, CompletionHandler<void()>&&);

    // Messages back from the SW host process
    void workerTerminated(WebCore::ServiceWorkerIdentifier);

    // Messages to the SW host WebProcess
    void installServiceWorkerContext(const WebCore::ServiceWorkerContextData&, const WebCore::ServiceWorkerData&, const String& userAgent, WebCore::WorkerThreadMode, OptionSet<WebCore::AdvancedPrivacyProtections>) final;
    void updateAppInitiatedValue(WebCore::ServiceWorkerIdentifier, WebCore::LastNavigationWasAppInitiated) final;
    void fireInstallEvent(WebCore::ServiceWorkerIdentifier) final;
    void fireActivateEvent(WebCore::ServiceWorkerIdentifier) final;
    void terminateWorker(WebCore::ServiceWorkerIdentifier) final;
    void didSaveScriptsToDisk(WebCore::ServiceWorkerIdentifier, const WebCore::ScriptBuffer&, const MemoryCompactRobinHoodHashMap<URL, WebCore::ScriptBuffer>& importedScripts) final;
    void firePushEvent(WebCore::ServiceWorkerIdentifier, const std::optional<Vector<uint8_t>>&, std::optional<WebCore::NotificationPayload>&&, CompletionHandler<void(bool, std::optional<WebCore::NotificationPayload>&&)>&&) final;
    void fireNotificationEvent(WebCore::ServiceWorkerIdentifier, const WebCore::NotificationData&, WebCore::NotificationEventType, CompletionHandler<void(bool)>&&) final;
    void fireBackgroundFetchEvent(WebCore::ServiceWorkerIdentifier, const WebCore::BackgroundFetchInformation&, CompletionHandler<void(bool)>&&) final;
    void fireBackgroundFetchClickEvent(WebCore::ServiceWorkerIdentifier, const WebCore::BackgroundFetchInformation&, CompletionHandler<void(bool)>&&) final;
    void focus(WebCore::ScriptExecutionContextIdentifier, CompletionHandler<void(std::optional<WebCore::ServiceWorkerClientData>&&)>&&);
    void navigate(WebCore::ScriptExecutionContextIdentifier, WebCore::ServiceWorkerIdentifier, const URL&, CompletionHandler<void(Expected<std::optional<WebCore::ServiceWorkerClientData>, WebCore::ExceptionData>&&)>&&);

    void connectionIsNoLongerNeeded() final;
    void terminateDueToUnresponsiveness() final;
    void openWindow(WebCore::ServiceWorkerIdentifier, const URL&, OpenWindowCallback&&) final;
    void reportConsoleMessage(WebCore::ServiceWorkerIdentifier, MessageSource, MessageLevel, const String& message, unsigned long requestIdentifier);

    void connectionClosed();

    void setInspectable(WebCore::ServiceWorkerIsInspectable) final;
    void serviceWorkerNeedsRunning() final;

    void processAssertionTimerFired();
    void startProcessAssertionTimer();
    bool areServiceWorkersIdle() const;

    WebCore::ProcessIdentifier m_webProcessIdentifier;
    WeakPtr<NetworkConnectionToWebProcess> m_connection;
    HashMap<WebCore::FetchIdentifier, WeakPtr<ServiceWorkerFetchTask>> m_ongoingFetches;
    HashMap<WebCore::FetchIdentifier, ThreadSafeWeakPtr<ServiceWorkerDownloadTask>> m_ongoingDownloads;
    bool m_isThrottleable { true };
    WebPageProxyIdentifier m_webPageProxyID;
    WebCore::ServiceWorkerIsInspectable m_isInspectable { WebCore::ServiceWorkerIsInspectable::Yes };
    bool m_isTakingProcessAssertion { false };
    WebCore::Timer m_processAssertionTimer;
}; // class WebSWServerToContextConnection

} // namespace WebKit
