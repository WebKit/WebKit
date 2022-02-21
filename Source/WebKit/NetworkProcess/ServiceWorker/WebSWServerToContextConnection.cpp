/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "ServiceWorkerDownloadTaskMessages.h"
#include "ServiceWorkerFetchTask.h"
#include "ServiceWorkerFetchTaskMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebSWContextManagerConnectionMessages.h"
#include "WebSWServerConnection.h"
#include <WebCore/SWServer.h>
#include <WebCore/ServiceWorkerContextData.h>

namespace WebKit {
using namespace WebCore;

WebSWServerToContextConnection::WebSWServerToContextConnection(NetworkConnectionToWebProcess& connection, WebPageProxyIdentifier webPageProxyID, RegistrableDomain&& registrableDomain, std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, SWServer& server)
    : SWServerToContextConnection(WTFMove(registrableDomain), serviceWorkerPageIdentifier)
    , m_connection(connection)
    , m_server(server)
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
    for (auto& download : downloads.values())
        download->contextClosed();

    if (m_server && m_server->contextConnectionForRegistrableDomain(registrableDomain()) == this)
        m_server->removeContextConnection(*this);
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
    if (!m_server)
        return;

    if (auto* connection = m_server->connection(destinationIdentifier.processIdentifier()))
        connection->postMessageToServiceWorkerClient(destinationIdentifier, message, sourceIdentifier, sourceOrigin);
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

void WebSWServerToContextConnection::firePushEvent(WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier, const std::optional<Vector<uint8_t>>& data, CompletionHandler<void(bool)>&& callback)
{
    if (!m_processingPushEventsCount++)
        m_connection.networkProcess().parentProcessConnection()->send(Messages::NetworkProcessProxy::StartServiceWorkerBackgroundProcessing { webProcessIdentifier() }, 0);

    std::optional<IPC::DataReference> ipcData;
    if (data)
        ipcData = IPC::DataReference { data->data(), data->size() };
    sendWithAsyncReply(Messages::WebSWContextManagerConnection::FirePushEvent(serviceWorkerIdentifier, ipcData), [weakThis = WeakPtr { *this }, callback = WTFMove(callback)](bool wasProcessed) mutable {
        if (weakThis && !--weakThis->m_processingPushEventsCount)
            weakThis->m_connection.networkProcess().parentProcessConnection()->send(Messages::NetworkProcessProxy::EndServiceWorkerBackgroundProcessing { weakThis->webProcessIdentifier() }, 0);
        callback(wasProcessed);
    });
}

void WebSWServerToContextConnection::terminateWorker(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    send(Messages::WebSWContextManagerConnection::TerminateWorker(serviceWorkerIdentifier));
}

void WebSWServerToContextConnection::didSaveScriptsToDisk(ServiceWorkerIdentifier serviceWorkerIdentifier, const ScriptBuffer& script, const HashMap<URL, ScriptBuffer>& importedScripts)
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
    m_connection.connection().addThreadMessageReceiver(Messages::ServiceWorkerDownloadTask::messageReceiverName(), &task, task.fetchIdentifier().toUInt64());
}

void WebSWServerToContextConnection::unregisterDownload(ServiceWorkerDownloadTask& task)
{
    m_ongoingDownloads.remove(task.fetchIdentifier());
    m_connection.connection().removeThreadMessageReceiver(Messages::ServiceWorkerDownloadTask::messageReceiverName(), task.fetchIdentifier().toUInt64());
}

WebCore::ProcessIdentifier WebSWServerToContextConnection::webProcessIdentifier() const
{
    return m_connection.webProcessIdentifier();
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
