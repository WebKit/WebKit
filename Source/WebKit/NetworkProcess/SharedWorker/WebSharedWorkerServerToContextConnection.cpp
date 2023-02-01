/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WebSharedWorkerServerToContextConnection.h"

#include "LoadedWebArchive.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "RemoteWorkerType.h"
#include "WebCoreArgumentCoders.h"
#include "WebSWServerConnection.h"
#include "WebSharedWorker.h"
#include "WebSharedWorkerContextManagerConnectionMessages.h"
#include "WebSharedWorkerServer.h"
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <wtf/MemoryPressureHandler.h>

namespace WebKit {

#define CONTEXT_CONNECTION_RELEASE_LOG(fmt, ...) RELEASE_LOG(SharedWorker, "%p - [webProcessIdentifier=%" PRIu64 "] WebSharedWorkerServerToContextConnection::" fmt, this, webProcessIdentifier().toUInt64(), ##__VA_ARGS__)

// We terminate the context connection after 5 seconds if it is no longer used by any SharedWorker objects,
// as a performance optimization.
constexpr Seconds idleTerminationDelay { 5_s };

WebSharedWorkerServerToContextConnection::WebSharedWorkerServerToContextConnection(NetworkConnectionToWebProcess& connection, const WebCore::RegistrableDomain& registrableDomain, WebSharedWorkerServer& server)
    : m_connection(connection)
    , m_server(server)
    , m_registrableDomain(registrableDomain)
    , m_idleTerminationTimer(*this, &WebSharedWorkerServerToContextConnection::idleTerminationTimerFired)
{
    CONTEXT_CONNECTION_RELEASE_LOG("WebSharedWorkerServerToContextConnection:");
    server.addContextConnection(*this);
}

WebSharedWorkerServerToContextConnection::~WebSharedWorkerServerToContextConnection()
{
    CONTEXT_CONNECTION_RELEASE_LOG("~WebSharedWorkerServerToContextConnection:");
    if (m_server && m_server->contextConnectionForRegistrableDomain(registrableDomain()) == this)
        m_server->removeContextConnection(*this);
}

WebCore::ProcessIdentifier WebSharedWorkerServerToContextConnection::webProcessIdentifier() const
{
    return m_connection.webProcessIdentifier();
}

IPC::Connection& WebSharedWorkerServerToContextConnection::ipcConnection() const
{
    return m_connection.connection();
}

IPC::Connection* WebSharedWorkerServerToContextConnection::messageSenderConnection() const
{
    return &ipcConnection();
}

uint64_t WebSharedWorkerServerToContextConnection::messageSenderDestinationID() const
{
    return 0;
}

void WebSharedWorkerServerToContextConnection::connectionIsNoLongerNeeded()
{
    CONTEXT_CONNECTION_RELEASE_LOG("connectionIsNoLongerNeeded:");
    m_connection.sharedWorkerServerToContextConnectionIsNoLongerNeeded();
}

void WebSharedWorkerServerToContextConnection::postExceptionToWorkerObject(WebCore::SharedWorkerIdentifier sharedWorkerIdentifier, const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL)
{
    CONTEXT_CONNECTION_RELEASE_LOG("postExceptionToWorkerObject: sharedWorkerIdentifier=%" PRIu64, sharedWorkerIdentifier.toUInt64());
    if (m_server)
        m_server->postExceptionToWorkerObject(sharedWorkerIdentifier, errorMessage, lineNumber, columnNumber, sourceURL);
}

void WebSharedWorkerServerToContextConnection::sharedWorkerTerminated(WebCore::SharedWorkerIdentifier sharedWorkerIdentifier)
{
    CONTEXT_CONNECTION_RELEASE_LOG("sharedWorkerTerminated: sharedWorkerIdentifier=%" PRIu64, sharedWorkerIdentifier.toUInt64());
    if (m_server)
        m_server->sharedWorkerTerminated(sharedWorkerIdentifier);
}

void WebSharedWorkerServerToContextConnection::launchSharedWorker(WebSharedWorker& sharedWorker)
{
    m_connection.networkProcess().addAllowedFirstPartyForCookies(m_connection.webProcessIdentifier(), WebCore::RegistrableDomain::uncheckedCreateFromHost(sharedWorker.origin().topOrigin.host), LoadedWebArchive::No, [] { });

    CONTEXT_CONNECTION_RELEASE_LOG("launchSharedWorker: sharedWorkerIdentifier=%" PRIu64, sharedWorker.identifier().toUInt64());
    sharedWorker.markAsRunning();
    auto initializationData = sharedWorker.initializationData();
    if (auto contextIdentifier = initializationData.clientIdentifier) {
        initializationData.clientIdentifier = WebCore::ScriptExecutionContextIdentifier { contextIdentifier->object(), webProcessIdentifier() };
#if ENABLE(SERVICE_WORKER)
        if (auto* serviceWorkerOldConnection = m_connection.networkProcess().webProcessConnection(contextIdentifier->processIdentifier())) {
            if (auto* swOldConnection = serviceWorkerOldConnection->swConnection()) {
                if (auto clientData = swOldConnection->gatherClientData(*contextIdentifier)) {
                    swOldConnection->unregisterServiceWorkerClient(*contextIdentifier);
                    if (auto* swNewConnection = m_connection.swConnection()) {
                        clientData->serviceWorkerClientData.identifier = *initializationData.clientIdentifier;
                        swNewConnection->registerServiceWorkerClient(WTFMove(clientData->clientOrigin), WTFMove(clientData->serviceWorkerClientData), clientData->controllingServiceWorkerRegistrationIdentifier, WTFMove(clientData->userAgent));
                    }
                }
            }
        }
#endif
    }
    send(Messages::WebSharedWorkerContextManagerConnection::LaunchSharedWorker { sharedWorker.origin(), sharedWorker.identifier(), sharedWorker.workerOptions(), sharedWorker.fetchResult(), initializationData });
    sharedWorker.forEachSharedWorkerObject([&](auto, auto& port) {
        postConnectEvent(sharedWorker, port, [](bool) { });
    });
}

void WebSharedWorkerServerToContextConnection::suspendSharedWorker(WebCore::SharedWorkerIdentifier identifier)
{
    send(Messages::WebSharedWorkerContextManagerConnection::SuspendSharedWorker { identifier });
}

void WebSharedWorkerServerToContextConnection::resumeSharedWorker(WebCore::SharedWorkerIdentifier identifier)
{
    send(Messages::WebSharedWorkerContextManagerConnection::ResumeSharedWorker { identifier });
}

void WebSharedWorkerServerToContextConnection::postConnectEvent(const WebSharedWorker& sharedWorker, const WebCore::TransferredMessagePort& port, CompletionHandler<void(bool)>&& completionHandler)
{
    CONTEXT_CONNECTION_RELEASE_LOG("postConnectEvent: sharedWorkerIdentifier=%" PRIu64, sharedWorker.identifier().toUInt64());
    sendWithAsyncReply(Messages::WebSharedWorkerContextManagerConnection::PostConnectEvent { sharedWorker.identifier(), port, sharedWorker.origin().clientOrigin.toString() }, WTFMove(completionHandler));
}

void WebSharedWorkerServerToContextConnection::terminateSharedWorker(const WebSharedWorker& sharedWorker)
{
    CONTEXT_CONNECTION_RELEASE_LOG("terminateSharedWorker: sharedWorkerIdentifier=%" PRIu64, sharedWorker.identifier().toUInt64());
    send(Messages::WebSharedWorkerContextManagerConnection::TerminateSharedWorker { sharedWorker.identifier() });
}

void WebSharedWorkerServerToContextConnection::addSharedWorkerObject(WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier)
{
    CONTEXT_CONNECTION_RELEASE_LOG("addSharedWorkerObject: sharedWorkerObjectIdentifier=%" PUBLIC_LOG_STRING, sharedWorkerObjectIdentifier.toString().utf8().data());
    auto& sharedWorkerObjects = m_sharedWorkerObjects.ensure(sharedWorkerObjectIdentifier.processIdentifier(), [] { return HashSet<WebCore::SharedWorkerObjectIdentifier> { }; }).iterator->value;
    ASSERT(!sharedWorkerObjects.contains(sharedWorkerObjectIdentifier));
    sharedWorkerObjects.add(sharedWorkerObjectIdentifier);

    if (webProcessIdentifier() != sharedWorkerObjectIdentifier.processIdentifier() && sharedWorkerObjects.size() == 1)
        m_connection.networkProcess().send(Messages::NetworkProcessProxy::RegisterRemoteWorkerClientProcess { RemoteWorkerType::SharedWorker, sharedWorkerObjectIdentifier.processIdentifier(), webProcessIdentifier() }, 0);

    m_idleTerminationTimer.stop();
}

void WebSharedWorkerServerToContextConnection::removeSharedWorkerObject(WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier)
{
    CONTEXT_CONNECTION_RELEASE_LOG("removeSharedWorkerObject: sharedWorkerObjectIdentifier=%" PUBLIC_LOG_STRING, sharedWorkerObjectIdentifier.toString().utf8().data());
    auto it = m_sharedWorkerObjects.find(sharedWorkerObjectIdentifier.processIdentifier());
    ASSERT(it != m_sharedWorkerObjects.end());
    if (it == m_sharedWorkerObjects.end())
        return;
    it->value.remove(sharedWorkerObjectIdentifier);
    if (!it->value.isEmpty())
        return;

    m_sharedWorkerObjects.remove(it);
    if (webProcessIdentifier() != sharedWorkerObjectIdentifier.processIdentifier())
        m_connection.networkProcess().send(Messages::NetworkProcessProxy::UnregisterRemoteWorkerClientProcess { RemoteWorkerType::SharedWorker, sharedWorkerObjectIdentifier.processIdentifier(), webProcessIdentifier() }, 0);

    if (m_sharedWorkerObjects.isEmpty()) {
        CONTEXT_CONNECTION_RELEASE_LOG("removeSharedWorkerObject: connection is now idle, starting a timer to terminate it");
        ASSERT(!m_idleTerminationTimer.isActive());
        m_idleTerminationTimer.startOneShot(MemoryPressureHandler::singleton().isUnderMemoryPressure() || m_shouldTerminateWhenPossible ? 0_s : idleTerminationDelay);
    }
}

void WebSharedWorkerServerToContextConnection::idleTerminationTimerFired()
{
    RELEASE_ASSERT(m_sharedWorkerObjects.isEmpty());
    connectionIsNoLongerNeeded();
}

#undef CONTEXT_CONNECTION_RELEASE_LOG

} // namespace WebKit
