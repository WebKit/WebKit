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

#if ENABLE(SERVICE_WORKER)

#include "SWServer.h"
#include "SWServerWorker.h"
#include <wtf/CompletionHandler.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static SWServerToContextConnectionIdentifier generateServerToContextConnectionIdentifier()
{
    return SWServerToContextConnectionIdentifier::generate();
}

SWServerToContextConnection::SWServerToContextConnection(SWServer& server, RegistrableDomain&& registrableDomain, std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier)
    : m_server(server)
    , m_identifier(generateServerToContextConnectionIdentifier())
    , m_registrableDomain(WTFMove(registrableDomain))
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

void SWServerToContextConnection::scriptContextFailedToStart(const std::optional<ServiceWorkerJobDataIdentifier>& jobDataIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, const String& message)
{
    if (auto* worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier))
        worker->scriptContextFailedToStart(jobDataIdentifier, message);
}

void SWServerToContextConnection::scriptContextStarted(const std::optional<ServiceWorkerJobDataIdentifier>& jobDataIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, bool doesHandleFetch)
{
    if (auto* worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier))
        worker->scriptContextStarted(jobDataIdentifier, doesHandleFetch);
}
    
void SWServerToContextConnection::didFinishInstall(const std::optional<ServiceWorkerJobDataIdentifier>& jobDataIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, bool wasSuccessful)
{
    if (auto* worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier))
        worker->didFinishInstall(jobDataIdentifier, wasSuccessful);
}

void SWServerToContextConnection::didFinishActivation(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    if (auto* worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier))
        worker->didFinishActivation();
}

void SWServerToContextConnection::setServiceWorkerHasPendingEvents(ServiceWorkerIdentifier serviceWorkerIdentifier, bool hasPendingEvents)
{
    if (auto* worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier))
        worker->setHasPendingEvents(hasPendingEvents);
}

void SWServerToContextConnection::workerTerminated(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    if (auto* worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier))
        worker->contextTerminated();
}

void SWServerToContextConnection::matchAll(uint64_t requestIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, const ServiceWorkerClientQueryOptions& options)
{
    if (auto* worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier)) {
        worker->matchAll(options, [&] (auto&& data) {
            worker->contextConnection()->matchAllCompleted(requestIdentifier, data);
        });
    }
}

void SWServerToContextConnection::findClientByVisibleIdentifier(ServiceWorkerIdentifier serviceWorkerIdentifier, const String& clientIdentifier, CompletionHandler<void(std::optional<WebCore::ServiceWorkerClientData>&&)>&& callback)
{
    if (auto* worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier))
        worker->findClientByVisibleIdentifier(clientIdentifier, WTFMove(callback));
    else
        callback({ });
}

void SWServerToContextConnection::claim(ServiceWorkerIdentifier serviceWorkerIdentifier, CompletionHandler<void(std::optional<ExceptionData>&&)>&& callback)
{
    auto* worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier);
    auto* server = worker ? worker->server() : nullptr;
    callback(server ? server->claim(*worker) : std::nullopt);
}

void SWServerToContextConnection::setScriptResource(ServiceWorkerIdentifier serviceWorkerIdentifier, URL&& scriptURL, ServiceWorkerContextData::ImportedScript&& script)
{
    if (auto* worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier))
        worker->setScriptResource(WTFMove(scriptURL), WTFMove(script));
}

void SWServerToContextConnection::didFailHeartBeatCheck(ServiceWorkerIdentifier identifier)
{
    if (auto* worker = SWServerWorker::existingWorkerForIdentifier(identifier))
        worker->didFailHeartBeatCheck();
}

void SWServerToContextConnection::setAsInspected(ServiceWorkerIdentifier identifier, bool isInspected)
{
    if (auto* worker = SWServerWorker::existingWorkerForIdentifier(identifier))
        worker->setAsInspected(isInspected);
}

void SWServerToContextConnection::terminateWhenPossible()
{
    m_shouldTerminateWhenPossible = true;

    bool hasServiceWorkerWithPendingEvents = false;
    server()->forEachServiceWorker([&](auto& worker) {
        if (worker.isRunning() && worker.registrableDomain() == m_registrableDomain && worker.hasPendingEvents()) {
            hasServiceWorkerWithPendingEvents = true;
            return false;
        }
        return true;
    });

    // FIXME: If there is a service worker with pending events and we don't close the connection right away, we'd ideally keep
    // track of this and close the connection once it becomes idle.
    if (!hasServiceWorkerWithPendingEvents)
        close();
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
