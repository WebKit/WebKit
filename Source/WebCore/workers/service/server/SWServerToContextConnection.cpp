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

SWServerToContextConnection::SWServerToContextConnection(RegistrableDomain&& registrableDomain)
    : m_identifier(generateServerToContextConnectionIdentifier())
    , m_registrableDomain(WTFMove(registrableDomain))
{
}

SWServerToContextConnection::~SWServerToContextConnection()
{
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

void SWServerToContextConnection::findClientByIdentifier(uint64_t requestIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, ServiceWorkerClientIdentifier clientId)
{
    if (auto* worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier))
        worker->contextConnection()->findClientByIdentifierCompleted(requestIdentifier, worker->findClientByIdentifier(clientId), false);
}

void SWServerToContextConnection::matchAll(uint64_t requestIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, const ServiceWorkerClientQueryOptions& options)
{
    if (auto* worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier)) {
        worker->matchAll(options, [&] (auto&& data) {
            worker->contextConnection()->matchAllCompleted(requestIdentifier, data);
        });
    }
}

void SWServerToContextConnection::claim(ServiceWorkerIdentifier serviceWorkerIdentifier, CompletionHandler<void(std::optional<ExceptionData>&&)>&& callback)
{
    auto* worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier);
    auto* server = worker ? worker->server() : nullptr;
    callback(server ? server->claim(*worker) : std::nullopt);
}

void SWServerToContextConnection::skipWaiting(ServiceWorkerIdentifier serviceWorkerIdentifier, CompletionHandler<void()>&& completionHandler)
{
    if (auto* worker = SWServerWorker::existingWorkerForIdentifier(serviceWorkerIdentifier))
        worker->skipWaiting();

    completionHandler();
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

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
