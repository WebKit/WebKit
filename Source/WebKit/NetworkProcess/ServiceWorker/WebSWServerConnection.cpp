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
#include "WebSWServerConnection.h"

#if ENABLE(SERVICE_WORKER)

#include "DataReference.h"
#include "FormDataReference.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkResourceLoader.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include "WebProcessMessages.h"
#include "WebResourceLoaderMessages.h"
#include "WebSWClientConnectionMessages.h"
#include "WebSWContextManagerConnectionMessages.h"
#include "WebSWServerConnectionMessages.h"
#include "WebSWServerToContextConnection.h"
#include <WebCore/DocumentIdentifier.h>
#include <WebCore/ExceptionData.h>
#include <WebCore/LegacySchemeRegistry.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/SWServerRegistration.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/ServiceWorkerClientData.h>
#include <WebCore/ServiceWorkerClientIdentifier.h>
#include <WebCore/ServiceWorkerContextData.h>
#include <WebCore/ServiceWorkerJobData.h>
#include <WebCore/ServiceWorkerUpdateViaCache.h>
#include <cstdint>
#include <wtf/Algorithms.h>
#include <wtf/MainThread.h>

namespace WebKit {
using namespace PAL;
using namespace WebCore;

#define SWSERVERCONNECTION_RELEASE_LOG(fmt, ...) RELEASE_LOG(ServiceWorker, "%p - WebSWServerConnection::" fmt, this, ##__VA_ARGS__)
#define SWSERVERCONNECTION_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(ServiceWorker, "%p - WebSWServerConnection::" fmt, this, ##__VA_ARGS__)

WebSWServerConnection::WebSWServerConnection(NetworkProcess& networkProcess, SWServer& server, IPC::Connection& connection, ProcessIdentifier processIdentifier)
    : SWServer::Connection(server, processIdentifier)
    , m_contentConnection(connection)
    , m_networkProcess(networkProcess)
{
    networkProcess.registerSWServerConnection(*this);
}

WebSWServerConnection::~WebSWServerConnection()
{
    m_networkProcess->unregisterSWServerConnection(*this);
    for (const auto& keyValue : m_clientOrigins)
        server().unregisterServiceWorkerClient(keyValue.value, keyValue.key);
    for (auto& completionHandler : m_unregisterJobs.values())
        completionHandler(false);
}

void WebSWServerConnection::rejectJobInClient(ServiceWorkerJobIdentifier jobIdentifier, const ExceptionData& exceptionData)
{
    if (auto completionHandler = m_unregisterJobs.take(jobIdentifier))
        return completionHandler(makeUnexpected(exceptionData));
    send(Messages::WebSWClientConnection::JobRejectedInServer(jobIdentifier, exceptionData));
}

void WebSWServerConnection::resolveRegistrationJobInClient(ServiceWorkerJobIdentifier jobIdentifier, const ServiceWorkerRegistrationData& registrationData, ShouldNotifyWhenResolved shouldNotifyWhenResolved)
{
    send(Messages::WebSWClientConnection::RegistrationJobResolvedInServer(jobIdentifier, registrationData, shouldNotifyWhenResolved));
}

void WebSWServerConnection::resolveUnregistrationJobInClient(ServiceWorkerJobIdentifier jobIdentifier, const ServiceWorkerRegistrationKey& registrationKey, bool unregistrationResult)
{
    ASSERT(m_unregisterJobs.contains(jobIdentifier));
    if (auto completionHandler = m_unregisterJobs.take(jobIdentifier))
        completionHandler(unregistrationResult);
}

void WebSWServerConnection::startScriptFetchInClient(ServiceWorkerJobIdentifier jobIdentifier, const ServiceWorkerRegistrationKey& registrationKey, FetchOptions::Cache cachePolicy)
{
    send(Messages::WebSWClientConnection::StartScriptFetchForServer(jobIdentifier, registrationKey, cachePolicy));
}

void WebSWServerConnection::updateRegistrationStateInClient(ServiceWorkerRegistrationIdentifier identifier, ServiceWorkerRegistrationState state, const std::optional<ServiceWorkerData>& serviceWorkerData)
{
    send(Messages::WebSWClientConnection::UpdateRegistrationState(identifier, state, serviceWorkerData));
}

void WebSWServerConnection::fireUpdateFoundEvent(ServiceWorkerRegistrationIdentifier identifier)
{
    send(Messages::WebSWClientConnection::FireUpdateFoundEvent(identifier));
}

void WebSWServerConnection::setRegistrationLastUpdateTime(ServiceWorkerRegistrationIdentifier identifier, WallTime lastUpdateTime)
{
    send(Messages::WebSWClientConnection::SetRegistrationLastUpdateTime(identifier, lastUpdateTime));
}

void WebSWServerConnection::setRegistrationUpdateViaCache(ServiceWorkerRegistrationIdentifier identifier, ServiceWorkerUpdateViaCache updateViaCache)
{
    send(Messages::WebSWClientConnection::SetRegistrationUpdateViaCache(identifier, updateViaCache));
}

void WebSWServerConnection::notifyClientsOfControllerChange(const HashSet<DocumentIdentifier>& contextIdentifiers, const ServiceWorkerData& newController)
{
    send(Messages::WebSWClientConnection::NotifyClientsOfControllerChange(contextIdentifiers, newController));
}

void WebSWServerConnection::updateWorkerStateInClient(ServiceWorkerIdentifier worker, ServiceWorkerState state)
{
    send(Messages::WebSWClientConnection::UpdateWorkerState(worker, state));
}

void WebSWServerConnection::controlClient(ServiceWorkerClientIdentifier clientIdentifier, SWServerRegistration& registration, const ResourceRequest& request)
{
    // As per step 12 of https://w3c.github.io/ServiceWorker/#on-fetch-request-algorithm, the active service worker should be controlling the document.
    // We register a temporary service worker client using the identifier provided by DocumentLoader and notify DocumentLoader about it.
    // If notification is successful, DocumentLoader will unregister the temporary service worker client just after the document is created and registered as a client.
    sendWithAsyncReply(Messages::WebSWClientConnection::SetDocumentIsControlled { clientIdentifier.contextIdentifier, registration.data() }, [weakThis = makeWeakPtr(this), this, clientIdentifier](bool isSuccess) {
        if (!weakThis || isSuccess)
            return;
        unregisterServiceWorkerClient(clientIdentifier);
    });

    ServiceWorkerClientData data { clientIdentifier, ServiceWorkerClientType::Window, ServiceWorkerClientFrameType::None, request.url(), request.isAppInitiated() ? WebCore::LastNavigationWasAppInitiated::Yes : WebCore::LastNavigationWasAppInitiated::No };
    registerServiceWorkerClient(SecurityOriginData { registration.key().topOrigin() }, WTFMove(data), registration.identifier(), request.httpUserAgent());
}

std::unique_ptr<ServiceWorkerFetchTask> WebSWServerConnection::createFetchTask(NetworkResourceLoader& loader, const ResourceRequest& request)
{
    if (loader.parameters().serviceWorkersMode == ServiceWorkersMode::None)
        return nullptr;

    if (!server().canHandleScheme(loader.originalRequest().url().protocol()))
        return nullptr;

    std::optional<ServiceWorkerRegistrationIdentifier> serviceWorkerRegistrationIdentifier;
    if (loader.parameters().options.mode == FetchOptions::Mode::Navigate) {
        auto topOrigin = loader.parameters().isMainFrameNavigation ? SecurityOriginData::fromURL(request.url()) : loader.parameters().topOrigin->data();
        auto* registration = doRegistrationMatching(topOrigin, request.url());
        if (!registration)
            return nullptr;

        serviceWorkerRegistrationIdentifier = registration->identifier();
        controlClient(ServiceWorkerClientIdentifier { loader.connectionToWebProcess().webProcessIdentifier(), *loader.parameters().options.clientIdentifier }, *registration, request);
    } else {
        if (!loader.parameters().serviceWorkerRegistrationIdentifier)
            return nullptr;

        if (isPotentialNavigationOrSubresourceRequest(loader.parameters().options.destination))
            return nullptr;

        serviceWorkerRegistrationIdentifier = *loader.parameters().serviceWorkerRegistrationIdentifier;
    }

    auto* worker = server().activeWorkerFromRegistrationID(*serviceWorkerRegistrationIdentifier);
    if (!worker) {
        SWSERVERCONNECTION_RELEASE_LOG_ERROR("startFetch: DidNotHandle because no active worker for registration %" PRIu64, serviceWorkerRegistrationIdentifier->toUInt64());
        return nullptr;
    }

    auto* registration = server().getRegistration(*serviceWorkerRegistrationIdentifier);
    bool shouldSoftUpdate = registration && registration->shouldSoftUpdate(loader.parameters().options);
    if (worker->shouldSkipFetchEvent()) {
        if (shouldSoftUpdate)
            registration->scheduleSoftUpdate(loader.isAppInitiated() ? WebCore::IsAppInitiated::Yes : WebCore::IsAppInitiated::No);

        return nullptr;
    }

    if (worker->hasTimedOutAnyFetchTasks()) {
        SWSERVERCONNECTION_RELEASE_LOG_ERROR("startFetch: DidNotHandle because worker %" PRIu64 " has some timeouts", worker->identifier().toUInt64());
        return nullptr;
    }

    auto task = makeUnique<ServiceWorkerFetchTask>(*this, loader, ResourceRequest { request }, identifier(), worker->identifier(), *serviceWorkerRegistrationIdentifier, shouldSoftUpdate);
    startFetch(*task, *worker);
    return task;
}

void WebSWServerConnection::startFetch(ServiceWorkerFetchTask& task, SWServerWorker& worker)
{
    auto runServerWorkerAndStartFetch = [weakThis = makeWeakPtr(this), this, task = makeWeakPtr(task)](bool success) mutable {
        if (!task)
            return;

        if (!weakThis) {
            task->cannotHandle();
            return;
        }

        if (!success) {
            SWSERVERCONNECTION_RELEASE_LOG_ERROR("startFetch: fetchIdentifier=%" PRIu64 " DidNotHandle because worker did not become activated", task->fetchIdentifier().toUInt64());
            task->cannotHandle();
            return;
        }

        auto* worker = server().workerByID(task->serviceWorkerIdentifier());
        if (!worker || worker->hasTimedOutAnyFetchTasks()) {
            task->cannotHandle();
            return;
        }

        if (!worker->contextConnection())
            server().createContextConnection(worker->registrableDomain());

        auto identifier = task->serviceWorkerIdentifier();
        server().runServiceWorkerIfNecessary(identifier, [weakThis = WTFMove(weakThis), this, task = WTFMove(task)](auto* contextConnection) mutable {
            if (!task)
                return;
            
            if (!weakThis) {
                task->cannotHandle();
                return;
            }

            if (!contextConnection) {
                SWSERVERCONNECTION_RELEASE_LOG_ERROR("startFetch: fetchIdentifier=%s DidNotHandle because failed to run service worker", task->fetchIdentifier().loggingString().utf8().data());
                task->cannotHandle();
                return;
            }
            SWSERVERCONNECTION_RELEASE_LOG("startFetch: Starting fetch %" PRIu64 " via service worker %" PRIu64, task->fetchIdentifier().toUInt64(), task->serviceWorkerIdentifier().toUInt64());
            static_cast<WebSWServerToContextConnection&>(*contextConnection).startFetch(*task);
        });
    };

    worker.whenActivated(WTFMove(runServerWorkerAndStartFetch));
}

void WebSWServerConnection::postMessageToServiceWorker(ServiceWorkerIdentifier destinationIdentifier, MessageWithMessagePorts&& message, const ServiceWorkerOrClientIdentifier& sourceIdentifier)
{
    auto* destinationWorker = server().workerByID(destinationIdentifier);
    if (!destinationWorker)
        return;

    std::optional<ServiceWorkerOrClientData> sourceData;
    WTF::switchOn(sourceIdentifier, [&](ServiceWorkerIdentifier identifier) {
        if (auto* sourceWorker = server().workerByID(identifier))
            sourceData = ServiceWorkerOrClientData { sourceWorker->data() };
    }, [&](ServiceWorkerClientIdentifier identifier) {
        if (auto clientData = destinationWorker->findClientByIdentifier(identifier))
            sourceData = ServiceWorkerOrClientData { *clientData };
    });

    if (!sourceData)
        return;

    // It's possible this specific worker cannot be re-run (e.g. its registration has been removed)
    server().runServiceWorkerIfNecessary(destinationIdentifier, [destinationIdentifier, message = WTFMove(message), sourceData = WTFMove(*sourceData)](auto* contextConnection) mutable {
        if (contextConnection)
            sendToContextProcess(*contextConnection, Messages::WebSWContextManagerConnection::PostMessageToServiceWorker { destinationIdentifier, WTFMove(message), WTFMove(sourceData) });
    });
}

void WebSWServerConnection::scheduleJobInServer(ServiceWorkerJobData&& jobData)
{
    ASSERT(!jobData.scopeURL.isNull());
    if (jobData.scopeURL.isNull()) {
        rejectJobInClient(jobData.identifier().jobIdentifier, ExceptionData { InvalidStateError, "Scope URL is empty"_s });
        return;
    }

    SWSERVERCONNECTION_RELEASE_LOG("Scheduling ServiceWorker job %s in server", jobData.identifier().loggingString().utf8().data());
    ASSERT(identifier() == jobData.connectionIdentifier());

    server().scheduleJob(WTFMove(jobData));
}

URL WebSWServerConnection::clientURLFromIdentifier(DocumentOrWorkerIdentifier contextIdentifier)
{
    return WTF::switchOn(contextIdentifier, [&](DocumentIdentifier documentIdentifier) -> URL {
        ServiceWorkerClientIdentifier clientIdentifier { identifier(), documentIdentifier };

        auto iterator = m_clientOrigins.find(clientIdentifier);
        if (iterator == m_clientOrigins.end())
            return { };

        auto clientData = server().serviceWorkerClientWithOriginByID(iterator->value, clientIdentifier);
        if (!clientData)
            return { };

        return clientData->url;
    }, [&](ServiceWorkerIdentifier serviceWorkerIdentifier) -> URL {
        auto* worker = server().workerByID(serviceWorkerIdentifier);
        if (!worker)
            return { };
        return worker->data().scriptURL;
    });
}

void WebSWServerConnection::scheduleUnregisterJobInServer(ServiceWorkerJobIdentifier jobIdentifier, ServiceWorkerRegistrationIdentifier registrationIdentifier, DocumentOrWorkerIdentifier contextIdentifier, CompletionHandler<void(UnregisterJobResult&&)>&& completionHandler)
{
    SWSERVERCONNECTION_RELEASE_LOG("Scheduling unregister ServiceWorker job in server");

    auto* registration = server().getRegistration(registrationIdentifier);
    if (!registration)
        return completionHandler(false);

    auto clientURL = clientURLFromIdentifier(contextIdentifier);
    if (!clientURL.isValid())
        return completionHandler(makeUnexpected(ExceptionData { InvalidStateError, "Client is unknown"_s }));

    ASSERT(!m_unregisterJobs.contains(jobIdentifier));
    m_unregisterJobs.add(jobIdentifier, WTFMove(completionHandler));

    server().scheduleUnregisterJob(ServiceWorkerJobDataIdentifier { identifier(), jobIdentifier }, *registration, contextIdentifier, WTFMove(clientURL));
}

void WebSWServerConnection::postMessageToServiceWorkerClient(DocumentIdentifier destinationContextIdentifier, const MessageWithMessagePorts& message, ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin)
{
    auto* sourceServiceWorker = server().workerByID(sourceIdentifier);
    if (!sourceServiceWorker)
        return;

    send(Messages::WebSWClientConnection::PostMessageToServiceWorkerClient { destinationContextIdentifier, message, sourceServiceWorker->data(), sourceOrigin });
}

void WebSWServerConnection::matchRegistration(uint64_t registrationMatchRequestIdentifier, const SecurityOriginData& topOrigin, const URL& clientURL)
{
    if (auto* registration = doRegistrationMatching(topOrigin, clientURL)) {
        send(Messages::WebSWClientConnection::DidMatchRegistration { registrationMatchRequestIdentifier, registration->data() });
        return;
    }
    send(Messages::WebSWClientConnection::DidMatchRegistration { registrationMatchRequestIdentifier, std::nullopt });
}

void WebSWServerConnection::registrationReady(uint64_t registrationReadyRequestIdentifier, ServiceWorkerRegistrationData&& registrationData)
{
    send(Messages::WebSWClientConnection::RegistrationReady { registrationReadyRequestIdentifier, WTFMove(registrationData) });
}

void WebSWServerConnection::getRegistrations(uint64_t registrationMatchRequestIdentifier, const SecurityOriginData& topOrigin, const URL& clientURL)
{
    auto registrations = server().getRegistrations(topOrigin, clientURL);
    send(Messages::WebSWClientConnection::DidGetRegistrations { registrationMatchRequestIdentifier, registrations });
}

void WebSWServerConnection::registerServiceWorkerClient(SecurityOriginData&& topOrigin, ServiceWorkerClientData&& data, const std::optional<ServiceWorkerRegistrationIdentifier>& controllingServiceWorkerRegistrationIdentifier, String&& userAgent)
{
    auto contextOrigin = SecurityOriginData::fromURL(data.url);
    bool isNewOrigin = WTF::allOf(m_clientOrigins.values(), [&contextOrigin](auto& origin) {
        return contextOrigin != origin.clientOrigin;
    });
    auto* contextConnection = isNewOrigin ? server().contextConnectionForRegistrableDomain(RegistrableDomain { contextOrigin }) : nullptr;

    auto clientOrigin = ClientOrigin { WTFMove(topOrigin), WTFMove(contextOrigin) };
    m_clientOrigins.add(data.identifier, clientOrigin);
    server().registerServiceWorkerClient(WTFMove(clientOrigin), WTFMove(data), controllingServiceWorkerRegistrationIdentifier, WTFMove(userAgent));

    if (!m_isThrottleable)
        updateThrottleState();

    if (contextConnection) {
        auto& connection = static_cast<WebSWServerToContextConnection&>(*contextConnection);
        m_networkProcess->parentProcessConnection()->send(Messages::NetworkProcessProxy::RegisterServiceWorkerClientProcess { identifier(), connection.webProcessIdentifier() }, 0);
    }
}

void WebSWServerConnection::unregisterServiceWorkerClient(const ServiceWorkerClientIdentifier& clientIdentifier)
{
    auto iterator = m_clientOrigins.find(clientIdentifier);
    if (iterator == m_clientOrigins.end())
        return;

    auto clientOrigin = iterator->value;

    server().unregisterServiceWorkerClient(clientOrigin, clientIdentifier);
    m_clientOrigins.remove(iterator);

    if (!m_isThrottleable)
        updateThrottleState();

    bool isDeletedOrigin = WTF::allOf(m_clientOrigins.values(), [&clientOrigin](auto& origin) {
        return clientOrigin.clientOrigin != origin.clientOrigin;
    });

    if (isDeletedOrigin) {
        if (auto* contextConnection = server().contextConnectionForRegistrableDomain(RegistrableDomain { clientOrigin.clientOrigin })) {
            auto& connection = static_cast<WebSWServerToContextConnection&>(*contextConnection);
            m_networkProcess->parentProcessConnection()->send(Messages::NetworkProcessProxy::UnregisterServiceWorkerClientProcess { identifier(), connection.webProcessIdentifier() }, 0);
        }
    }
}

bool WebSWServerConnection::hasMatchingClient(const RegistrableDomain& domain) const
{
    return WTF::anyOf(m_clientOrigins.values(), [&domain](auto& origin) {
        return domain.matches(origin.clientOrigin);
    });
}

bool WebSWServerConnection::computeThrottleState(const RegistrableDomain& domain) const
{
    return WTF::allOf(server().connections().values(), [&domain](auto& serverConnection) {
        auto& connection = static_cast<WebSWServerConnection&>(*serverConnection);
        return connection.isThrottleable() || !connection.hasMatchingClient(domain);
    });
}

void WebSWServerConnection::setThrottleState(bool isThrottleable)
{
    m_isThrottleable = isThrottleable;
    updateThrottleState();
}

void WebSWServerConnection::updateThrottleState()
{
    HashSet<SecurityOriginData> origins;
    for (auto& origin : m_clientOrigins.values())
        origins.add(origin.clientOrigin);

    for (auto& origin : origins) {
        if (auto* contextConnection = server().contextConnectionForRegistrableDomain(RegistrableDomain { origin })) {
            auto& connection = static_cast<WebSWServerToContextConnection&>(*contextConnection);

            if (connection.isThrottleable() == m_isThrottleable)
                continue;
            bool newThrottleState = computeThrottleState(connection.registrableDomain());
            if (connection.isThrottleable() == newThrottleState)
                continue;
            connection.setThrottleState(newThrottleState);
        }
    }
}

void WebSWServerConnection::contextConnectionCreated(SWServerToContextConnection& contextConnection)
{
    auto& connection =  static_cast<WebSWServerToContextConnection&>(contextConnection);
    connection.setThrottleState(computeThrottleState(connection.registrableDomain()));

    if (hasMatchingClient(connection.registrableDomain()))
        m_networkProcess->parentProcessConnection()->send(Messages::NetworkProcessProxy::RegisterServiceWorkerClientProcess { identifier(), connection.webProcessIdentifier() }, 0);
}

void WebSWServerConnection::terminateWorkerFromClient(ServiceWorkerIdentifier serviceWorkerIdentifier, CompletionHandler<void()>&& callback)
{
    auto* worker = server().workerByID(serviceWorkerIdentifier);
    if (!worker)
        return callback();
    worker->terminate(WTFMove(callback));
}

void WebSWServerConnection::whenServiceWorkerIsTerminatedForTesting(WebCore::ServiceWorkerIdentifier identifier, CompletionHandler<void()>&& completionHandler)
{
    auto* worker = SWServerWorker::existingWorkerForIdentifier(identifier);
    if (!worker || worker->isNotRunning())
        return completionHandler();
    worker->whenTerminated(WTFMove(completionHandler));
}

PAL::SessionID WebSWServerConnection::sessionID() const
{
    return server().sessionID();
}
    
template<typename U> void WebSWServerConnection::sendToContextProcess(WebCore::SWServerToContextConnection& connection, U&& message)
{
    static_cast<WebSWServerToContextConnection&>(connection).send(WTFMove(message));
}

void WebSWServerConnection::fetchTaskTimedOut(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    auto* worker = server().workerByID(serviceWorkerIdentifier);
    if (!worker)
        return;

    worker->setHasTimedOutAnyFetchTasks();
    worker->terminate();
}

} // namespace WebKit

#undef SWSERVERCONNECTION_RELEASE_LOG
#undef SWSERVERCONNECTION_RELEASE_LOG_ERROR

#endif // ENABLE(SERVICE_WORKER)
