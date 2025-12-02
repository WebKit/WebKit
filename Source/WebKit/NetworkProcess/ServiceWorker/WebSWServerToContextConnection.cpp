/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebSWServerToContextConnection.h"

#include "FormDataReference.h"
#include "Logging.h"
#include "MessageSenderInlines.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkSession.h"
#include "ServiceWorkerFetchTask.h"
#include "ServiceWorkerFetchTaskMessages.h"
#include "WebSWContextManagerConnectionMessages.h"
#include "WebSWServerConnection.h"
#include <WebCore/NotificationData.h>
#include <WebCore/NotificationPayload.h>
#include <WebCore/SWServer.h>
#include <WebCore/SWServerWorker.h>
#include <WebCore/ServiceWorkerContextData.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, *ipcConnection())

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebSWServerToContextConnection);

Ref<WebSWServerToContextConnection> WebSWServerToContextConnection::create(NetworkConnectionToWebProcess& connection, WebPageProxyIdentifier webPageProxyID, Site&& registrableDomain, std::optional<WebCore::ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, WebCore::SWServer& server)
{
    Ref contextConnection = adoptRef(*new WebSWServerToContextConnection(connection, webPageProxyID, WTFMove(registrableDomain), serviceWorkerPageIdentifier, server));
    server.addContextConnection(contextConnection.get());
    return contextConnection;
}

WebSWServerToContextConnection::WebSWServerToContextConnection(NetworkConnectionToWebProcess& connection, WebPageProxyIdentifier webPageProxyID, Site&& site, std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, SWServer& server)
    : SWServerToContextConnection(server, WTFMove(site), serviceWorkerPageIdentifier)
    , m_webProcessIdentifier(connection.webProcessIdentifier())
    , m_connection(connection)
    , m_webPageProxyID(webPageProxyID)
{
}

WebSWServerToContextConnection::~WebSWServerToContextConnection()
{
    ASSERT(m_ongoingFetches.isEmpty());
    ASSERT(m_ongoingDownloads.isEmpty());
}

template<typename T>
void WebSWServerToContextConnection::sendToParentProcess(T&& message)
{
    protectedNetworkProcess()->protectedParentProcessConnection()->send(WTFMove(message), 0);
}

template<typename T, typename C>
void WebSWServerToContextConnection::sendWithAsyncReplyToParentProcess(T&& message, C&& callback)
{
    protectedNetworkProcess()->protectedParentProcessConnection()->sendWithAsyncReply(WTFMove(message), WTFMove(callback), 0);
}

void WebSWServerToContextConnection::stop()
{
    auto fetches = WTFMove(m_ongoingFetches);
    for (auto& weakPtr : fetches.values()) {
        if (RefPtr fetch = weakPtr.get())
            fetch->contextClosed();
    }

    auto downloads = WTFMove(m_ongoingDownloads);
    for (auto& weakPtr : downloads.values()) {
        if (RefPtr download = weakPtr.get())
            download->contextClosed();
    }

    if (RefPtr server = this->server(); server && server->contextConnectionForRegistrableDomain(registrableDomain()) == this)
        server->removeContextConnection(*this);
}

void WebSWServerToContextConnection::terminateIdleServiceWorkers()
{
    if (RefPtr server = this->server(); server && server->contextConnectionForRegistrableDomain(registrableDomain()) == this)
        server->terminateIdleServiceWorkers(*this);
}

RefPtr<NetworkConnectionToWebProcess> WebSWServerToContextConnection::protectedConnection() const
{
    return m_connection.get();
}

NetworkProcess* WebSWServerToContextConnection::networkProcess()
{
    return m_connection ? &m_connection->networkProcess() : nullptr;
}

RefPtr<NetworkProcess> WebSWServerToContextConnection::protectedNetworkProcess()
{
    return networkProcess();
}

RefPtr<IPC::Connection> WebSWServerToContextConnection::protectedIPCConnection() const
{
    return ipcConnection();
}

IPC::Connection* WebSWServerToContextConnection::ipcConnection() const
{
    return m_connection ? &m_connection->connection() : nullptr;
}

IPC::Connection* WebSWServerToContextConnection::messageSenderConnection() const
{
    return ipcConnection();
}

uint64_t WebSWServerToContextConnection::messageSenderDestinationID() const
{
    return 0;
}

void WebSWServerToContextConnection::postMessageToServiceWorkerClient(const ScriptExecutionContextIdentifier& destinationIdentifier, const MessageWithMessagePorts& message, ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin)
{
    RefPtr server = this->server();
    if (RefPtr connection = server ? server->connection(destinationIdentifier.processIdentifier()) : nullptr)
        connection->postMessageToServiceWorkerClient(destinationIdentifier, message, sourceIdentifier, sourceOrigin);
}

void WebSWServerToContextConnection::skipWaiting(ServiceWorkerIdentifier serviceWorkerIdentifier, CompletionHandler<void()>&& callback)
{
    if (RefPtr worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier)) {
        MESSAGE_CHECK(worker->isTerminating() || worker->registration());
        worker->skipWaiting();
    }

    callback();
}

void WebSWServerToContextConnection::installServiceWorkerContext(const ServiceWorkerContextData& contextData, const ServiceWorkerData& workerData, const String& userAgent, WorkerThreadMode workerThreadMode, OptionSet<AdvancedPrivacyProtections> advancedPrivacyProtections)
{
    send(Messages::WebSWContextManagerConnection::InstallServiceWorker { contextData, workerData, userAgent, workerThreadMode, m_isInspectable, advancedPrivacyProtections });
}

void WebSWServerToContextConnection::updateAppInitiatedValue(ServiceWorkerIdentifier serviceWorkerIdentifier, WebCore::LastNavigationWasAppInitiated lastNavigationWasAppInitiated)
{
    send(Messages::WebSWContextManagerConnection::UpdateAppInitiatedValue(serviceWorkerIdentifier, lastNavigationWasAppInitiated));
}

void WebSWServerToContextConnection::fireInstallEvent(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    send(Messages::WebSWContextManagerConnection::FireInstallEvent(serviceWorkerIdentifier));
}

void WebSWServerToContextConnection::fireActivateEvent(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    send(Messages::WebSWContextManagerConnection::FireActivateEvent(serviceWorkerIdentifier));
}

void WebSWServerToContextConnection::firePushEvent(ServiceWorkerIdentifier serviceWorkerIdentifier, const std::optional<Vector<uint8_t>>& data, std::optional<NotificationPayload>&& proposedPayload, CompletionHandler<void(bool, std::optional<NotificationPayload>&&)>&& callback)
{
    if (!m_processingFunctionalEventCount++)
        sendToParentProcess(Messages::NetworkProcessProxy::StartServiceWorkerBackgroundProcessing { webProcessIdentifier() });

    std::optional<std::span<const uint8_t>> ipcData;
    if (data)
        ipcData = data->span();
    sendWithAsyncReply(Messages::WebSWContextManagerConnection::FirePushEvent(serviceWorkerIdentifier, ipcData, WTFMove(proposedPayload)), [weakThis = WeakPtr { *this }, callback = WTFMove(callback)](bool wasProcessed, std::optional<NotificationPayload>&& resultPayload) mutable {
        if (RefPtr protectedThis = weakThis.get(); protectedThis && !--protectedThis->m_processingFunctionalEventCount)
            protectedThis->sendToParentProcess(Messages::NetworkProcessProxy::EndServiceWorkerBackgroundProcessing { protectedThis->webProcessIdentifier() });

        callback(wasProcessed, WTFMove(resultPayload));
    });
}

void WebSWServerToContextConnection::fireNotificationEvent(ServiceWorkerIdentifier serviceWorkerIdentifier, const NotificationData& data, NotificationEventType eventType, CompletionHandler<void(bool)>&& callback)
{
    if (!m_processingFunctionalEventCount++)
        sendToParentProcess(Messages::NetworkProcessProxy::StartServiceWorkerBackgroundProcessing { webProcessIdentifier() });

    sendWithAsyncReply(Messages::WebSWContextManagerConnection::FireNotificationEvent { serviceWorkerIdentifier, data, eventType }, [weakThis = WeakPtr { *this }, eventType, callback = WTFMove(callback)](bool wasProcessed) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !protectedThis->m_connection)
            return callback(wasProcessed);

        if (!--protectedThis->m_processingFunctionalEventCount)
            protectedThis->sendToParentProcess(Messages::NetworkProcessProxy::EndServiceWorkerBackgroundProcessing { protectedThis->webProcessIdentifier() });

        CheckedPtr session = protectedThis->protectedConnection()->networkSession();
        if (RefPtr resourceLoadStatistics = session ? session->resourceLoadStatistics() : nullptr; resourceLoadStatistics && wasProcessed && eventType == NotificationEventType::Click) {
            return resourceLoadStatistics->setMostRecentWebPushInteractionTime(RegistrableDomain(protectedThis->registrableDomain()), [callback = WTFMove(callback), wasProcessed] () mutable {
                callback(wasProcessed);
            });
        }

        callback(wasProcessed);
    });
}

void WebSWServerToContextConnection::fireBackgroundFetchEvent(ServiceWorkerIdentifier serviceWorkerIdentifier, const BackgroundFetchInformation& info, CompletionHandler<void(bool)>&& callback)
{
    if (!m_processingFunctionalEventCount++)
        sendToParentProcess(Messages::NetworkProcessProxy::StartServiceWorkerBackgroundProcessing { webProcessIdentifier() });

    sendWithAsyncReply(Messages::WebSWContextManagerConnection::FireBackgroundFetchEvent { serviceWorkerIdentifier, info }, [weakThis = WeakPtr { *this }, callback = WTFMove(callback)](bool wasProcessed) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return callback(wasProcessed);

        if (!--protectedThis->m_processingFunctionalEventCount)
            protectedThis->sendToParentProcess(Messages::NetworkProcessProxy::EndServiceWorkerBackgroundProcessing { protectedThis->webProcessIdentifier() });

        callback(wasProcessed);
    });
}

void WebSWServerToContextConnection::fireBackgroundFetchClickEvent(ServiceWorkerIdentifier serviceWorkerIdentifier, const BackgroundFetchInformation& info, CompletionHandler<void(bool)>&& callback)
{
    if (!m_processingFunctionalEventCount++)
        sendToParentProcess(Messages::NetworkProcessProxy::StartServiceWorkerBackgroundProcessing { webProcessIdentifier() });

    sendWithAsyncReply(Messages::WebSWContextManagerConnection::FireBackgroundFetchClickEvent { serviceWorkerIdentifier, info }, [weakThis = WeakPtr { *this }, callback = WTFMove(callback)](bool wasProcessed) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return callback(wasProcessed);

        if (!--protectedThis->m_processingFunctionalEventCount)
            protectedThis->sendToParentProcess(Messages::NetworkProcessProxy::EndServiceWorkerBackgroundProcessing { protectedThis->webProcessIdentifier() });

        callback(wasProcessed);
    });
}

void WebSWServerToContextConnection::terminateWorker(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    if (!m_processingFunctionalEventCount++)
        protectedConnection()->networkProcess().protectedParentProcessConnection()->send(Messages::NetworkProcessProxy::StartServiceWorkerBackgroundProcessing { webProcessIdentifier() }, 0);

    send(Messages::WebSWContextManagerConnection::TerminateWorker(serviceWorkerIdentifier));
}

void WebSWServerToContextConnection::setAsInspected(ServiceWorkerIdentifier serviceWorkerIdentifier, bool isInspected)
{
    SWServerToContextConnection::setAsInspected(serviceWorkerIdentifier, isInspected);

#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    if (isInspected) {
        CheckedPtr session = protectedConnection()->networkSession();
        RefPtr worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier);

        if (session && worker) {
            auto scopeURL = worker->registrationKey().scope();
            session->notificationManager().setServiceWorkerIsBeingInspected(scopeURL, true);

            worker->whenTerminated([connection = WeakPtr { m_connection }, scopeURL]() {
                if (RefPtr protectedConnection = connection.get()) {
                    if (CheckedPtr session = protectedConnection->networkSession())
                        session->notificationManager().setServiceWorkerIsBeingInspected(scopeURL, false);
                }
            });
        }
    }
#endif
}

void WebSWServerToContextConnection::workerTerminated(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    SWServerToContextConnection::workerTerminated(serviceWorkerIdentifier);

    if (--m_processingFunctionalEventCount)
        protectedConnection()->networkProcess().protectedParentProcessConnection()->send(Messages::NetworkProcessProxy::EndServiceWorkerBackgroundProcessing { webProcessIdentifier() }, 0);
}

void WebSWServerToContextConnection::didFinishActivation(WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    RefPtr worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier);
    if (!worker)
        return;

    auto state = worker->state();
    if (state == ServiceWorkerState::Redundant)
        return;

    MESSAGE_CHECK(state == ServiceWorkerState::Activating);
    SWServerToContextConnection::didFinishActivation(serviceWorkerIdentifier);
}

void WebSWServerToContextConnection::didFinishInstall(const std::optional<WebCore::ServiceWorkerJobDataIdentifier>& jobDataIdentifier, WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier, bool wasSuccessful)
{
    RefPtr worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier);
    if (!worker)
        return;

    auto state = worker->state();
    if (state == ServiceWorkerState::Redundant)
        return;

    MESSAGE_CHECK(state == ServiceWorkerState::Installing);
    SWServerToContextConnection::didFinishInstall(jobDataIdentifier, serviceWorkerIdentifier, wasSuccessful);
}

void WebSWServerToContextConnection::didSaveScriptsToDisk(ServiceWorkerIdentifier serviceWorkerIdentifier, const ScriptBuffer& script, const MemoryCompactRobinHoodHashMap<URL, ScriptBuffer>& importedScripts)
{
#if ENABLE(SHAREABLE_RESOURCE) && PLATFORM(COCOA)
    // Send file-mapped ScriptBuffers over to the ServiceWorker process so that it can replace its heap-allocated copies and save on dirty memory.
    auto scriptToSend = script.containsSingleFileMappedSegment() ? script : ScriptBuffer();
    HashMap<URL, ScriptBuffer> importedScriptsToSend;
    for (auto& pair : importedScripts) {
        if (pair.value.containsSingleFileMappedSegment())
            importedScriptsToSend.add(pair.key, pair.value);
    }
    if (scriptToSend || !importedScriptsToSend.isEmpty())
        send(Messages::WebSWContextManagerConnection::DidSaveScriptsToDisk { serviceWorkerIdentifier, scriptToSend, importedScriptsToSend });
#else
    UNUSED_PARAM(script);
    UNUSED_PARAM(importedScripts);
#endif
}

void WebSWServerToContextConnection::terminateDueToUnresponsiveness()
{
    protectedConnection()->terminateSWContextConnectionDueToUnresponsiveness();
}

void WebSWServerToContextConnection::openWindow(WebCore::ServiceWorkerIdentifier identifier, const URL& url, OpenWindowCallback&& callback)
{
    RefPtr server = this->server();
    if (!server) {
        callback(makeUnexpected(ExceptionData { ExceptionCode::TypeError, "No SWServer"_s }));
        return;
    }

    RefPtr worker = server->workerByID(identifier);
    if (!worker) {
        callback(makeUnexpected(ExceptionData { ExceptionCode::TypeError, "No remaining service worker"_s }));
        return;
    }

    auto innerCallback = [callback = WTFMove(callback), weakServer = WeakPtr { *server }, origin = worker->origin()](std::optional<WebCore::PageIdentifier>&& pageIdentifier) mutable {
        if (!pageIdentifier) {
            // FIXME: validate whether we should reject or resolve with null, https://github.com/w3c/ServiceWorker/issues/1639
            callback({ });
            return;
        }

        RefPtr server = weakServer.get();
        if (!server) {
            callback(makeUnexpected(ExceptionData { ExceptionCode::TypeError, "No SWServer"_s }));
            return;
        }

        callback(server->topLevelServiceWorkerClientFromPageIdentifier(origin, *pageIdentifier));
    };

    sendWithAsyncReplyToParentProcess(Messages::NetworkProcessProxy::OpenWindowFromServiceWorker { m_connection->sessionID(), url.string(), worker->origin().clientOrigin }, WTFMove(innerCallback));
}

void WebSWServerToContextConnection::reportConsoleMessage(WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier, MessageSource source, MessageLevel level, const String& message, uint64_t requestIdentifier)
{
    RefPtr server = this->server();
    RefPtr worker = server ? server->workerByID(serviceWorkerIdentifier) : nullptr;
    if (!worker)
        return;
    sendToParentProcess(Messages::NetworkProcessProxy::ReportConsoleMessage { m_connection->sessionID(), worker->scriptURL(), worker->origin().clientOrigin, source, level, message, requestIdentifier });
}

void WebSWServerToContextConnection::connectionIsNoLongerNeeded()
{
    if (RefPtr connection = m_connection.get())
        connection->serviceWorkerServerToContextConnectionNoLongerNeeded();
}

void WebSWServerToContextConnection::setThrottleState(bool isThrottleable)
{
    m_isThrottleable = isThrottleable;
    send(Messages::WebSWContextManagerConnection::SetThrottleState { isThrottleable });
}

void WebSWServerToContextConnection::startFetch(ServiceWorkerFetchTask& task)
{
    task.start(*this);
}

void WebSWServerToContextConnection::didReceiveFetchTaskMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    MESSAGE_CHECK(ObjectIdentifier<FetchIdentifierType>::isValidIdentifier(decoder.destinationID()));

    RefPtr fetch = m_ongoingFetches.get(ObjectIdentifier<FetchIdentifierType>(decoder.destinationID()));
    if (!fetch)
        return;

    fetch->didReceiveMessage(connection, decoder);
}

void WebSWServerToContextConnection::registerFetch(ServiceWorkerFetchTask& task)
{
    ASSERT(!m_ongoingFetches.contains(task.fetchIdentifier()));
    m_ongoingFetches.add(task.fetchIdentifier(), task);
}

void WebSWServerToContextConnection::unregisterFetch(ServiceWorkerFetchTask& task)
{
    ASSERT(m_ongoingFetches.contains(task.fetchIdentifier()));
    m_ongoingFetches.remove(task.fetchIdentifier());
}

void WebSWServerToContextConnection::registerDownload(ServiceWorkerDownloadTask& task)
{
    ASSERT(!m_ongoingDownloads.contains(task.fetchIdentifier()));
    m_ongoingDownloads.add(task.fetchIdentifier(), task);
}

void WebSWServerToContextConnection::unregisterDownload(ServiceWorkerDownloadTask& task)
{
    m_ongoingDownloads.remove(task.fetchIdentifier());
}

void WebSWServerToContextConnection::focus(ScriptExecutionContextIdentifier clientIdentifier, CompletionHandler<void(std::optional<WebCore::ServiceWorkerClientData>&&)>&& callback)
{
    RefPtr server = this->server();
    RefPtr connection = server ? server->connection(clientIdentifier.processIdentifier()) : nullptr;
    if (!connection) {
        callback({ });
        return;
    }
    connection->focusServiceWorkerClient(clientIdentifier, WTFMove(callback));
}

void WebSWServerToContextConnection::navigate(ScriptExecutionContextIdentifier clientIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, const URL& url, CompletionHandler<void(Expected<std::optional<ServiceWorkerClientData>, ExceptionData>&&)>&& callback)
{
    RefPtr worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier);
    if (!worker) {
        callback(makeUnexpected(ExceptionData { ExceptionCode::TypeError, "no service worker"_s }));
        return;
    }

    if (!worker->isClientActiveServiceWorker(clientIdentifier)) {
        callback(makeUnexpected(ExceptionData { ExceptionCode::TypeError, "service worker is not the client active service worker"_s }));
        return;
    }

    auto data = worker->findClientByIdentifier(clientIdentifier);
    if (!data || !data->pageIdentifier || !data->frameIdentifier) {
        callback(makeUnexpected(ExceptionData { ExceptionCode::TypeError, "cannot navigate service worker client"_s }));
        return;
    }

    auto frameIdentifier = *data->frameIdentifier;
    sendWithAsyncReplyToParentProcess(Messages::NetworkProcessProxy::NavigateServiceWorkerClient { frameIdentifier, clientIdentifier, url }, [weakThis = WeakPtr { *this }, url, clientOrigin = worker->origin(), callback = WTFMove(callback)](auto pageIdentifier, auto frameIdentifier) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !protectedThis->server()) {
            callback(makeUnexpected(ExceptionData { ExceptionCode::TypeError, "service worker is gone"_s }));
            return;
        }

        if (!pageIdentifier || !frameIdentifier) {
            callback(makeUnexpected(ExceptionData { ExceptionCode::TypeError, "navigate failed"_s }));
            return;
        }

        std::optional<ServiceWorkerClientData> clientData;
        protectedThis->server()->forEachClientForOrigin(clientOrigin, [pageIdentifier, frameIdentifier, url, &clientData](auto& data) {
            if (!clientData && data.pageIdentifier && *data.pageIdentifier == *pageIdentifier && data.frameIdentifier && *data.frameIdentifier == *frameIdentifier && equalIgnoringFragmentIdentifier(data.url, url))
                clientData = data;
        });
        callback(WTFMove(clientData));
    });
}

void WebSWServerToContextConnection::setInspectable(ServiceWorkerIsInspectable inspectable)
{
    if (m_isInspectable == inspectable)
        return;

    m_isInspectable = inspectable;
    send(Messages::WebSWContextManagerConnection::SetInspectable { inspectable });
}

#if ENABLE(CONTENT_EXTENSIONS)
void WebSWServerToContextConnection::reportNetworkUsageToWorkerClient(const WebCore::ScriptExecutionContextIdentifier identifier, uint64_t bytesTransferredOverNetworkDelta)
{
    if (RefPtr server = this->server()) {
        if (RefPtr connection = dynamicDowncast<WebSWServerConnection>(server->connection(identifier.processIdentifier())))
            connection->reportNetworkUsageToWorkerClient(identifier, bytesTransferredOverNetworkDelta);
    }
}
#endif

std::optional<SharedPreferencesForWebProcess> WebSWServerToContextConnection::sharedPreferencesForWebProcess() const
{
    if (RefPtr connection = m_connection.get())
        return connection->sharedPreferencesForWebProcess();

    return std::nullopt;
}


#undef MESSAGE_CHECK
} // namespace WebKit
