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

#include "config.h"
#include "WebSWServerToContextConnection.h"

#if ENABLE(SERVICE_WORKER)

#include "FormDataReference.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkSession.h"
#include "ServiceWorkerFetchTask.h"
#include "ServiceWorkerFetchTaskMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebSWContextManagerConnectionMessages.h"
#include "WebSWServerConnection.h"
#include <WebCore/NotificationData.h>
#include <WebCore/SWServer.h>
#include <WebCore/ServiceWorkerContextData.h>

namespace WebKit {
using namespace WebCore;

WebSWServerToContextConnection::WebSWServerToContextConnection(NetworkConnectionToWebProcess& connection, WebPageProxyIdentifier webPageProxyID, RegistrableDomain&& registrableDomain, std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, SWServer& server)
    : SWServerToContextConnection(server, WTFMove(registrableDomain), serviceWorkerPageIdentifier)
    , m_connection(connection)
    , m_webPageProxyID(webPageProxyID)
{
    server.addContextConnection(*this);
}

WebSWServerToContextConnection::~WebSWServerToContextConnection()
{
    auto fetches = WTFMove(m_ongoingFetches);
    for (auto& fetch : fetches.values())
        fetch->contextClosed();

    auto downloads = WTFMove(m_ongoingDownloads);
    for (auto& weakPtr : downloads.values()) {
        if (auto download = weakPtr.get())
            download->contextClosed();
    }

    if (auto* server = this->server(); server && server->contextConnectionForRegistrableDomain(registrableDomain()) == this)
        server->removeContextConnection(*this);
}

IPC::Connection& WebSWServerToContextConnection::ipcConnection() const
{
    return m_connection.connection();
}

IPC::Connection* WebSWServerToContextConnection::messageSenderConnection() const
{
    return &ipcConnection();
}

uint64_t WebSWServerToContextConnection::messageSenderDestinationID() const
{
    return 0;
}

void WebSWServerToContextConnection::postMessageToServiceWorkerClient(const ScriptExecutionContextIdentifier& destinationIdentifier, const MessageWithMessagePorts& message, ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin)
{
    auto* server = this->server();
    if (auto* connection = server ? server->connection(destinationIdentifier.processIdentifier()) : nullptr)
        connection->postMessageToServiceWorkerClient(destinationIdentifier, message, sourceIdentifier, sourceOrigin);
}

void WebSWServerToContextConnection::skipWaiting(uint64_t requestIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    if (auto* worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier))
        worker->skipWaiting();

    send(Messages::WebSWContextManagerConnection::SkipWaitingCompleted { requestIdentifier });
}

void WebSWServerToContextConnection::close()
{
    send(Messages::WebSWContextManagerConnection::Close { });
}

void WebSWServerToContextConnection::installServiceWorkerContext(const ServiceWorkerContextData& contextData, const ServiceWorkerData& workerData, const String& userAgent, WorkerThreadMode workerThreadMode)
{
    send(Messages::WebSWContextManagerConnection::InstallServiceWorker { contextData, workerData, userAgent, workerThreadMode });
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

void WebSWServerToContextConnection::firePushEvent(ServiceWorkerIdentifier serviceWorkerIdentifier, const std::optional<Vector<uint8_t>>& data, CompletionHandler<void(bool)>&& callback)
{
    if (!m_processingFunctionalEventCount++)
        m_connection.networkProcess().parentProcessConnection()->send(Messages::NetworkProcessProxy::StartServiceWorkerBackgroundProcessing { webProcessIdentifier() }, 0);

    std::optional<IPC::DataReference> ipcData;
    if (data)
        ipcData = IPC::DataReference { data->data(), data->size() };
    sendWithAsyncReply(Messages::WebSWContextManagerConnection::FirePushEvent(serviceWorkerIdentifier, ipcData), [weakThis = WeakPtr { *this }, callback = WTFMove(callback)](bool wasProcessed) mutable {
        if (weakThis && !--weakThis->m_processingFunctionalEventCount)
            weakThis->m_connection.networkProcess().parentProcessConnection()->send(Messages::NetworkProcessProxy::EndServiceWorkerBackgroundProcessing { weakThis->webProcessIdentifier() }, 0);

        callback(wasProcessed);
    });
}

void WebSWServerToContextConnection::fireNotificationEvent(ServiceWorkerIdentifier serviceWorkerIdentifier, const NotificationData& data, NotificationEventType eventType, CompletionHandler<void(bool)>&& callback)
{
    if (!m_processingFunctionalEventCount++)
        m_connection.networkProcess().parentProcessConnection()->send(Messages::NetworkProcessProxy::StartServiceWorkerBackgroundProcessing { webProcessIdentifier() }, 0);

    sendWithAsyncReply(Messages::WebSWContextManagerConnection::FireNotificationEvent { serviceWorkerIdentifier, data, eventType }, [weakThis = WeakPtr { *this }, eventType, callback = WTFMove(callback)](bool wasProcessed) mutable {
        if (!weakThis)
            return callback(wasProcessed);

        if (!--weakThis->m_processingFunctionalEventCount)
            weakThis->m_connection.networkProcess().parentProcessConnection()->send(Messages::NetworkProcessProxy::EndServiceWorkerBackgroundProcessing { weakThis->webProcessIdentifier() }, 0);

        auto* session = weakThis->m_connection.networkSession();
        if (auto* resourceLoadStatistics = session ? session->resourceLoadStatistics() : nullptr; resourceLoadStatistics && wasProcessed && eventType == NotificationEventType::Click) {
            return resourceLoadStatistics->setMostRecentWebPushInteractionTime(RegistrableDomain(weakThis->registrableDomain()), [callback = WTFMove(callback), wasProcessed] () mutable {
                callback(wasProcessed);
            });
        }

        callback(wasProcessed);
    });
}

void WebSWServerToContextConnection::terminateWorker(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    send(Messages::WebSWContextManagerConnection::TerminateWorker(serviceWorkerIdentifier));
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
    m_connection.networkProcess().parentProcessConnection()->send(Messages::NetworkProcessProxy::TerminateUnresponsiveServiceWorkerProcesses { webProcessIdentifier() }, 0);
}

void WebSWServerToContextConnection::openWindow(WebCore::ServiceWorkerIdentifier identifier, const URL& url, OpenWindowCallback&& callback)
{
    auto* server = this->server();
    if (!server) {
        callback(makeUnexpected(ExceptionData { TypeError, "No SWServer"_s }));
        return;
    }

    auto* worker = server->workerByID(identifier);
    if (!worker) {
        callback(makeUnexpected(ExceptionData { TypeError, "No remaining service worker"_s }));
        return;
    }

    auto innerCallback = [callback = WTFMove(callback), server = WeakPtr { *server }, origin = worker->origin()](std::optional<WebCore::PageIdentifier>&& pageIdentifier) mutable {
        if (!pageIdentifier) {
            // FIXME: validate whether we should reject or resolve with null, https://github.com/w3c/ServiceWorker/issues/1639
            callback({ });
            return;
        }

        if (!server) {
            callback(makeUnexpected(ExceptionData { TypeError, "No SWServer"_s }));
            return;
        }

        callback(server->topLevelServiceWorkerClientFromPageIdentifier(origin, *pageIdentifier));
    };

    m_connection.networkProcess().parentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::OpenWindowFromServiceWorker { m_connection.sessionID(), url.string(), worker->origin().clientOrigin }, WTFMove(innerCallback));
}

void WebSWServerToContextConnection::matchAllCompleted(uint64_t requestIdentifier, const Vector<ServiceWorkerClientData>& clientsData)
{
    send(Messages::WebSWContextManagerConnection::MatchAllCompleted { requestIdentifier, clientsData });
}

void WebSWServerToContextConnection::connectionIsNoLongerNeeded()
{
    m_connection.serviceWorkerServerToContextConnectionNoLongerNeeded();
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
    auto iterator = m_ongoingFetches.find(makeObjectIdentifier<FetchIdentifierType>(decoder.destinationID()));
    if (iterator == m_ongoingFetches.end())
        return;

    iterator->value->didReceiveMessage(connection, decoder);
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

WebCore::ProcessIdentifier WebSWServerToContextConnection::webProcessIdentifier() const
{
    return m_connection.webProcessIdentifier();
}

void WebSWServerToContextConnection::focus(ScriptExecutionContextIdentifier clientIdentifier, CompletionHandler<void(std::optional<WebCore::ServiceWorkerClientData>&&)>&& callback)
{
    auto* server = this->server();
    auto* connection = server ? server->connection(clientIdentifier.processIdentifier()) : nullptr;
    if (!connection) {
        callback({ });
        return;
    }
    connection->focusServiceWorkerClient(clientIdentifier, WTFMove(callback));
}

void WebSWServerToContextConnection::navigate(ScriptExecutionContextIdentifier clientIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, const URL& url, CompletionHandler<void(Expected<std::optional<ServiceWorkerClientData>, ExceptionData>&&)>&& callback)
{
    auto* worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier);
    if (!worker) {
        callback(makeUnexpected(ExceptionData { TypeError, "no service worker"_s }));
        return;
    }

    if (!worker->isClientActiveServiceWorker(clientIdentifier)) {
        callback(makeUnexpected(ExceptionData { TypeError, "service worker is not the client active service worker"_s }));
        return;
    }

    auto data = worker->findClientByIdentifier(clientIdentifier);
    if (!data || !data->pageIdentifier || !data->frameIdentifier) {
        callback(makeUnexpected(ExceptionData { TypeError, "cannot navigate service worker client"_s }));
        return;
    }

    auto frameIdentifier = *data->frameIdentifier;
    m_connection.networkProcess().parentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::NavigateServiceWorkerClient { frameIdentifier, clientIdentifier, url }, [weakThis = WeakPtr { *this }, url, clientOrigin = worker->origin(), callback = WTFMove(callback)](auto pageIdentifier, auto frameIdentifier) mutable {
        if (!weakThis || !weakThis->server()) {
            callback(makeUnexpected(ExceptionData { TypeError, "service worker is gone"_s }));
            return;
        }

        if (!pageIdentifier || !frameIdentifier) {
            callback(makeUnexpected(ExceptionData { TypeError, "navigate failed"_s }));
            return;
        }

        std::optional<ServiceWorkerClientData> clientData;
        weakThis->server()->forEachClientForOrigin(clientOrigin, [pageIdentifier, frameIdentifier, url, &clientData](auto& data) {
            if (!clientData && data.pageIdentifier && *data.pageIdentifier == *pageIdentifier && data.frameIdentifier && *data.frameIdentifier == *frameIdentifier && equalIgnoringFragmentIdentifier(data.url, url))
                clientData = data;
        });
        callback(WTFMove(clientData));
    }, 0);
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
