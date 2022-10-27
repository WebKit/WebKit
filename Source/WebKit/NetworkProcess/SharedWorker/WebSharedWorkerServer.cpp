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
#include "WebSharedWorkerServer.h"

#include "Logging.h"
#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkSession.h"
#include "RemoteWorkerType.h"
#include "WebSharedWorker.h"
#include "WebSharedWorkerServerConnection.h"
#include "WebSharedWorkerServerToContextConnection.h"
#include <WebCore/RegistrableDomain.h>
#include <WebCore/WorkerFetchResult.h>
#include <WebCore/WorkerOptions.h>

namespace WebKit {

WebSharedWorkerServer::WebSharedWorkerServer(NetworkSession& session)
    : m_session(session)
{
}

WebSharedWorkerServer::~WebSharedWorkerServer() = default;

PAL::SessionID WebSharedWorkerServer::sessionID()
{
    return m_session.sessionID();
}

void WebSharedWorkerServer::requestSharedWorker(WebCore::SharedWorkerKey&& sharedWorkerKey, WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier, WebCore::TransferredMessagePort&& port, WebCore::WorkerOptions&& workerOptions)
{
    auto& sharedWorker = m_sharedWorkers.ensure(sharedWorkerKey, [&] {
        return makeUnique<WebSharedWorker>(*this, sharedWorkerKey, workerOptions);
    }).iterator->value;
    RELEASE_LOG(SharedWorker, "WebSharedWorkerServer::requestSharedWorker: sharedWorkerObjectIdentifier=%" PRIVATE_LOG_STRING ", sharedWorkerIdentifier=%" PRIu64, sharedWorkerObjectIdentifier.toString().utf8().data(), sharedWorker->identifier().toUInt64());

    if (sharedWorker->workerOptions().type != workerOptions.type || sharedWorker->workerOptions().credentials != workerOptions.credentials) {
        RELEASE_LOG_ERROR(SharedWorker, "WebSharedWorkerServer::requestSharedWorker: A worker already exists with this name but has different type / credentials");
        if (auto* serverConnection = m_connections.get(sharedWorkerObjectIdentifier.processIdentifier()))
            serverConnection->notifyWorkerObjectOfLoadCompletion(sharedWorkerObjectIdentifier, { WebCore::ResourceError::Type::AccessControl });
        return;
    }

    sharedWorker->addSharedWorkerObject(sharedWorkerObjectIdentifier, port);

    if (sharedWorker->sharedWorkerObjectsCount() > 1) {
        RELEASE_LOG(SharedWorker, "WebSharedWorkerServer::requestSharedWorker: A shared worker with this URL already exists (now shared by %u shared worker objects)", sharedWorker->sharedWorkerObjectsCount());
        if (sharedWorker->didFinishFetching()) {
            if (auto* serverConnection = m_connections.get(sharedWorkerObjectIdentifier.processIdentifier()))
                serverConnection->notifyWorkerObjectOfLoadCompletion(sharedWorkerObjectIdentifier, { });
        }
        if (sharedWorker->isRunning()) {
            auto* contextConnection = sharedWorker->contextConnection();
            ASSERT(contextConnection);
            if (contextConnection)
                contextConnection->postConnectEvent(*sharedWorker, port);
        }
        return;
    }
    ASSERT(!sharedWorker->isRunning());

    auto* serverConnection = m_connections.get(sharedWorkerObjectIdentifier.processIdentifier());
    ASSERT(serverConnection);

    RELEASE_LOG(SharedWorker, "WebSharedWorkerServer::requestSharedWorker: Fetching shared worker script in client");
    serverConnection->fetchScriptInClient(*sharedWorker, sharedWorkerObjectIdentifier, [weakThis = WeakPtr { *this }, sharedWorker = WeakPtr { *sharedWorker }](auto&& fetchResult, auto&& initializationData) {
        if (weakThis && sharedWorker)
            weakThis->didFinishFetchingSharedWorkerScript(*sharedWorker, WTFMove(fetchResult), WTFMove(initializationData));
    });
}

void WebSharedWorkerServer::didFinishFetchingSharedWorkerScript(WebSharedWorker& sharedWorker, WebCore::WorkerFetchResult&& fetchResult, WebCore::WorkerInitializationData&& initializationData)
{
    RELEASE_LOG(SharedWorker, "WebSharedWorkerServer::didFinishFetchingSharedWorkerScript sharedWorkerIdentifier=%" PRIu64 ", sharedWorker=%p, success=%d", sharedWorker.identifier().toUInt64(), &sharedWorker, fetchResult.error.isNull());

    sharedWorker.forEachSharedWorkerObject([&](auto sharedWorkerObjectIdentifier, auto&) {
        if (auto* serverConnection = m_connections.get(sharedWorkerObjectIdentifier.processIdentifier()))
            serverConnection->notifyWorkerObjectOfLoadCompletion(sharedWorkerObjectIdentifier, fetchResult.error);
    });

    if (!fetchResult.error.isNull()) {
        m_sharedWorkers.remove(sharedWorker.key());
        return;
    }

    sharedWorker.setInitializationData(WTFMove(initializationData));
    sharedWorker.setFetchResult(WTFMove(fetchResult));

    if (auto* connection = m_contextConnections.get(sharedWorker.registrableDomain()))
        sharedWorker.launch(*connection);
    else
        createContextConnection(sharedWorker.registrableDomain(), WebCore::RegistrableDomain::uncheckedCreateFromHost(sharedWorker.origin().topOrigin.host), sharedWorker.firstSharedWorkerObjectProcess());
}

bool WebSharedWorkerServer::needsContextConnectionForRegistrableDomain(const WebCore::RegistrableDomain& registrableDomain) const
{
    for (auto& sharedWorker : m_sharedWorkers.values()) {
        if (registrableDomain.matches(sharedWorker->url()))
            return true;
    }
    return false;
}

void WebSharedWorkerServer::createContextConnection(const WebCore::RegistrableDomain& registrableDomain, const WebCore::RegistrableDomain& firstPartyForCookies, std::optional<WebCore::ProcessIdentifier> requestingProcessIdentifier)
{
    ASSERT(!m_contextConnections.contains(registrableDomain));
    if (m_pendingContextConnectionDomains.contains(registrableDomain))
        return;

    RELEASE_LOG(SharedWorker, "WebSharedWorkerServer::createContextConnection will create a connection");

    m_pendingContextConnectionDomains.add(registrableDomain);
    m_session.networkProcess().parentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::EstablishRemoteWorkerContextConnectionToNetworkProcess { RemoteWorkerType::SharedWorker, registrableDomain, requestingProcessIdentifier, std::nullopt, m_session.sessionID() }, [this, weakThis = WeakPtr { *this }, registrableDomain, firstPartyForCookies] (auto remoteProcessIdentifier) {
        if (!weakThis)
            return;

        RELEASE_LOG(SharedWorker, "WebSharedWorkerServer::createContextConnection should now have created a connection");

        // FIXME: Add a check that the process this firstPartyForCookies came from was allowed to use it as a firstPartyForCookies.
        m_session.networkProcess().addAllowedFirstPartyForCookies(remoteProcessIdentifier, WebCore::RegistrableDomain(firstPartyForCookies));

        ASSERT(m_pendingContextConnectionDomains.contains(registrableDomain));
        m_pendingContextConnectionDomains.remove(registrableDomain);
        if (m_contextConnections.contains(registrableDomain))
            return;

        if (needsContextConnectionForRegistrableDomain(registrableDomain))
            createContextConnection(registrableDomain, firstPartyForCookies, { });
    }, 0);
}

void WebSharedWorkerServer::addContextConnection(WebSharedWorkerServerToContextConnection& contextConnection)
{
    RELEASE_LOG(SharedWorker, "WebSharedWorkerServer::addContextConnection(%p) webProcessIdentifier=%" PRIu64, &contextConnection, contextConnection.webProcessIdentifier().toUInt64());

    ASSERT(!m_contextConnections.contains(contextConnection.registrableDomain()));

    m_contextConnections.add(contextConnection.registrableDomain(), &contextConnection);

    contextConnectionCreated(contextConnection);
}

void WebSharedWorkerServer::removeContextConnection(WebSharedWorkerServerToContextConnection& contextConnection)
{
    RELEASE_LOG(SharedWorker, "WebSharedWorkerServer::removeContextConnection(%p) webProcessIdentifier=%" PRIu64, &contextConnection, contextConnection.webProcessIdentifier().toUInt64());

    auto registrableDomain = contextConnection.registrableDomain();

    ASSERT(m_contextConnections.get(registrableDomain) == &contextConnection);

    m_contextConnections.remove(registrableDomain);

    if (auto& sharedWorkerObjects = contextConnection.sharedWorkerObjects(); !sharedWorkerObjects.isEmpty()) {
        auto requestingProcessIdentifier = sharedWorkerObjects.begin()->key;
        createContextConnection(registrableDomain, registrableDomain, requestingProcessIdentifier);
    }
}

void WebSharedWorkerServer::contextConnectionCreated(WebSharedWorkerServerToContextConnection& contextConnection)
{
    RELEASE_LOG(SharedWorker, "WebSharedWorkerServer::contextConnectionCreated(%p) webProcessIdentifier=%" PRIu64, &contextConnection, contextConnection.webProcessIdentifier().toUInt64());
    auto& registrableDomain = contextConnection.registrableDomain();
    for (auto& sharedWorker : m_sharedWorkers.values()) {
        if (!registrableDomain.matches(sharedWorker->url()))
            continue;

        sharedWorker->didCreateContextConnection(contextConnection);
    }
}

void WebSharedWorkerServer::sharedWorkerObjectIsGoingAway(const WebCore::SharedWorkerKey& sharedWorkerKey, WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier)
{
    auto* sharedWorker = m_sharedWorkers.get(sharedWorkerKey);
    RELEASE_LOG(SharedWorker, "WebSharedWorkerServer::sharedWorkerObjectIsGoingAway: sharedWorkerObjectIdentifier=%" PUBLIC_LOG_STRING ", sharedWorker=%p", sharedWorkerObjectIdentifier.toString().utf8().data(), sharedWorker);
    if (!sharedWorker)
        return;

    sharedWorker->removeSharedWorkerObject(sharedWorkerObjectIdentifier);
    if (sharedWorker->sharedWorkerObjectsCount())
        return;

    shutDownSharedWorker(sharedWorkerKey);
}

void WebSharedWorkerServer::suspendForBackForwardCache(const WebCore::SharedWorkerKey& sharedWorkerKey, WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier)
{
    auto* sharedWorker = m_sharedWorkers.get(sharedWorkerKey);
    RELEASE_LOG(SharedWorker, "WebSharedWorkerServer::suspendForBackForwardCache: sharedWorkerObjectIdentifier=%" PUBLIC_LOG_STRING ", sharedWorker=%p", sharedWorkerObjectIdentifier.toString().utf8().data(), sharedWorker);
    if (!sharedWorker)
        return;

    sharedWorker->suspend(sharedWorkerObjectIdentifier);
}

void WebSharedWorkerServer::resumeForBackForwardCache(const WebCore::SharedWorkerKey& sharedWorkerKey, WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier)
{
    auto* sharedWorker = m_sharedWorkers.get(sharedWorkerKey);
    RELEASE_LOG(SharedWorker, "WebSharedWorkerServer::resumeForBackForwardCache: sharedWorkerObjectIdentifier=%" PUBLIC_LOG_STRING ", sharedWorker=%p", sharedWorkerObjectIdentifier.toString().utf8().data(), sharedWorker);
    if (!sharedWorker)
        return;

    sharedWorker->resume(sharedWorkerObjectIdentifier);
}

void WebSharedWorkerServer::shutDownSharedWorker(const WebCore::SharedWorkerKey& key)
{
    auto sharedWorker = m_sharedWorkers.take(key);
    RELEASE_LOG(SharedWorker, "WebSharedWorkerServer::shutDownSharedWorker: sharedWorkerIdentifier=%" PRIu64 ", sharedWorker=%p", sharedWorker ? sharedWorker->identifier().toUInt64() : 0, sharedWorker.get());
    if (!sharedWorker)
        return;

    if (auto* contextConnection = sharedWorker->contextConnection())
        contextConnection->terminateSharedWorker(*sharedWorker);
}

void WebSharedWorkerServer::addConnection(std::unique_ptr<WebSharedWorkerServerConnection>&& connection)
{
    auto processIdentifier = connection->webProcessIdentifier();
    RELEASE_LOG(SharedWorker, "WebSharedWorkerServer::addConnection(%p): processIdentifier=%" PRIu64, connection.get(), processIdentifier.toUInt64());
    ASSERT(!m_connections.contains(processIdentifier));
    m_connections.add(processIdentifier, WTFMove(connection));
}

void WebSharedWorkerServer::removeConnection(WebCore::ProcessIdentifier processIdentifier)
{
    auto connection = m_connections.take(processIdentifier);
    RELEASE_LOG(SharedWorker, "WebSharedWorkerServer::removeConnection(%p): processIdentifier=%" PRIu64, connection.get(), processIdentifier.toUInt64());
    ASSERT(connection);

    Vector<std::pair<WebCore::SharedWorkerKey, WebCore::SharedWorkerObjectIdentifier>> sharedWorkerObjectsGoingAway;
    for (auto& sharedWorker : m_sharedWorkers.values()) {
        sharedWorker->forEachSharedWorkerObject([&](auto sharedWorkerObjectIdentifier, auto&) {
            if (sharedWorkerObjectIdentifier.processIdentifier() == processIdentifier)
                sharedWorkerObjectsGoingAway.append(std::make_pair(sharedWorker->key(), sharedWorkerObjectIdentifier));
        });
    }
    for (auto& [sharedWorkerKey, sharedWorkerObjectIdentifier] : sharedWorkerObjectsGoingAway)
        sharedWorkerObjectIsGoingAway(sharedWorkerKey, sharedWorkerObjectIdentifier);
}

WebSharedWorkerServerToContextConnection* WebSharedWorkerServer::contextConnectionForRegistrableDomain(const WebCore::RegistrableDomain& domain) const
{
    return m_contextConnections.get(domain);
}

void WebSharedWorkerServer::postExceptionToWorkerObject(WebCore::SharedWorkerIdentifier sharedWorkerIdentifier, const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL)
{
    auto* sharedWorker = WebSharedWorker::fromIdentifier(sharedWorkerIdentifier);
    RELEASE_LOG_ERROR(SharedWorker, "WebSharedWorkerServer::postExceptionToWorkerObject: sharedWorkerIdentifier=%" PRIu64 ", sharedWorker=%p", sharedWorkerIdentifier.toUInt64(), sharedWorker);
    if (!sharedWorker)
        return;

    sharedWorker->forEachSharedWorkerObject([&](auto sharedWorkerObjectIdentifier, auto&) {
        if (auto* serverConnection = m_connections.get(sharedWorkerObjectIdentifier.processIdentifier()))
            serverConnection->postExceptionToWorkerObject(sharedWorkerObjectIdentifier, errorMessage, lineNumber, columnNumber, sourceURL);
    });
}

void WebSharedWorkerServer::terminateContextConnectionWhenPossible(const WebCore::RegistrableDomain& registrableDomain, WebCore::ProcessIdentifier processIdentifier)
{
    auto* contextConnection = contextConnectionForRegistrableDomain(registrableDomain);
    RELEASE_LOG(SharedWorker, "WebSharedWorkerServer::terminateContextConnectionWhenPossible: processIdentifier=%" PRIu64 ", contextConnection=%p", processIdentifier.toUInt64(), contextConnection);
    if (!contextConnection || contextConnection->webProcessIdentifier() != processIdentifier)
        return;

    contextConnection->terminateWhenPossible();
}

} // namespace WebKit
