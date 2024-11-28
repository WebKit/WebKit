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
#include "SWServerToContextConnection.h"

#include "SWServer.h"
#include "SWServerWorker.h"
#include <wtf/CompletionHandler.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SWServerToContextConnection);

SWServerToContextConnection::SWServerToContextConnection(SWServer& server, Site&& site, std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier)
    : m_server(server)
    , m_site(WTFMove(site))
    , m_serviceWorkerPageIdentifier(serviceWorkerPageIdentifier)
{
}

SWServerToContextConnection::~SWServerToContextConnection()
{
}

SWServer* SWServerToContextConnection::server() const
{
    return m_server.get();
}

RefPtr<SWServer> SWServerToContextConnection::protectedServer() const
{
    return m_server.get();
}

void SWServerToContextConnection::scriptContextFailedToStart(const std::optional<ServiceWorkerJobDataIdentifier>& jobDataIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, const String& message)
{
    if (RefPtr worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier))
        worker->scriptContextFailedToStart(jobDataIdentifier, message);
}

void SWServerToContextConnection::scriptContextStarted(const std::optional<ServiceWorkerJobDataIdentifier>& jobDataIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, bool doesHandleFetch)
{
    if (RefPtr worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier))
        worker->scriptContextStarted(jobDataIdentifier, doesHandleFetch);
}
    
void SWServerToContextConnection::didFinishInstall(const std::optional<ServiceWorkerJobDataIdentifier>& jobDataIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, bool wasSuccessful)
{
    if (RefPtr worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier))
        worker->didFinishInstall(jobDataIdentifier, wasSuccessful);
}

void SWServerToContextConnection::didFinishActivation(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    if (RefPtr worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier))
        worker->didFinishActivation();
}

void SWServerToContextConnection::setServiceWorkerHasPendingEvents(ServiceWorkerIdentifier serviceWorkerIdentifier, bool hasPendingEvents)
{
    if (RefPtr worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier))
        worker->setHasPendingEvents(hasPendingEvents);
}

void SWServerToContextConnection::workerTerminated(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    if (RefPtr worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier))
        worker->contextTerminated();
}

void SWServerToContextConnection::matchAll(ServiceWorkerIdentifier serviceWorkerIdentifier, const ServiceWorkerClientQueryOptions& options, CompletionHandler<void(Vector<WebCore::ServiceWorkerClientData>&&)>&& callback)
{
    RefPtr worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier);
    if (!worker) {
        callback({ });
        return;
    }

    worker->matchAll(options, WTFMove(callback));
}

void SWServerToContextConnection::findClientByVisibleIdentifier(ServiceWorkerIdentifier serviceWorkerIdentifier, const String& clientIdentifier, CompletionHandler<void(std::optional<WebCore::ServiceWorkerClientData>&&)>&& callback)
{
    if (RefPtr worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier))
        worker->findClientByVisibleIdentifier(clientIdentifier, WTFMove(callback));
    else
        callback({ });
}

void SWServerToContextConnection::claim(ServiceWorkerIdentifier serviceWorkerIdentifier, CompletionHandler<void(std::optional<ExceptionData>&&)>&& callback)
{
    RefPtr worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier);
    RefPtr server = worker ? worker->server() : nullptr;
    callback(server ? server->claim(*worker) : std::nullopt);
}

void SWServerToContextConnection::setScriptResource(ServiceWorkerIdentifier serviceWorkerIdentifier, URL&& scriptURL, ServiceWorkerContextData::ImportedScript&& script)
{
    if (RefPtr worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier))
        worker->setScriptResource(WTFMove(scriptURL), WTFMove(script));
}

void SWServerToContextConnection::didFailHeartBeatCheck(ServiceWorkerIdentifier identifier)
{
    if (RefPtr worker = SWServerWorker::existingWorkerForIdentifier(identifier))
        worker->didFailHeartBeatCheck();
}

void SWServerToContextConnection::setAsInspected(ServiceWorkerIdentifier identifier, bool isInspected)
{
    if (RefPtr worker = SWServerWorker::existingWorkerForIdentifier(identifier))
        worker->setAsInspected(isInspected);
}

bool SWServerToContextConnection::terminateWhenPossible()
{
    m_shouldTerminateWhenPossible = true;

    bool hasServiceWorkerWithPendingEvents = false;
    protectedServer()->forEachServiceWorker([&](auto& worker) {
        if (worker.isRunning() && worker.topRegistrableDomain() == m_site.domain() && worker.hasPendingEvents()) {
            hasServiceWorkerWithPendingEvents = true;
            return false;
        }
        return true;
    });

    // FIXME: If there is a service worker with pending events and we don't close the connection right away, we'd ideally keep
    // track of this and close the connection once it becomes idle.
    return !hasServiceWorkerWithPendingEvents;
}

} // namespace WebCore
