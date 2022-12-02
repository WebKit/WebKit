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
#include "NetworkNotificationManager.h"
#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkResourceLoader.h"
#include "NetworkSession.h"
#include "RemoteWorkerType.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include "WebProcessMessages.h"
#include "WebResourceLoaderMessages.h"
#include "WebSWClientConnectionMessages.h"
#include "WebSWContextManagerConnectionMessages.h"
#include "WebSWServerConnectionMessages.h"
#include "WebSWServerToContextConnection.h"
#include <WebCore/ExceptionData.h>
#include <WebCore/LegacySchemeRegistry.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/SWServerRegistration.h>
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/ServiceWorkerClientData.h>
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

#define CONNECTION_MESSAGE_CHECK(assertion) CONNECTION_MESSAGE_CHECK_COMPLETION(assertion, (void)0)
#define CONNECTION_MESSAGE_CHECK_COMPLETION(assertion, completion) do { \
    ASSERT(assertion); \
    if (UNLIKELY(!(assertion))) { \
        m_networkProcess->parentProcessConnection()->send(Messages::NetworkProcessProxy::TerminateWebProcess(identifier()), 0); \
        { completion; } \
        return; \
    } \
} while (0)

WebSWServerConnection::WebSWServerConnection(NetworkProcess& networkProcess, SWServer& server, IPC::Connection& connection, ProcessIdentifier processIdentifier)
    : SWServer::Connection(server, processIdentifier)
    , m_contentConnection(connection)
    , m_networkProcess(networkProcess)
{
    if (auto* session = this->session())
        session->registerSWServerConnection(*this);
}

WebSWServerConnection::~WebSWServerConnection()
{
    if (auto* session = this->session())
        session->unregisterSWServerConnection(*this);
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
    if (auto completionHandler = m_unregisterJobs.take(jobIdentifier)) {
#if ENABLE(BUILT_IN_NOTIFICATIONS)
        if (!session()) {
            completionHandler(unregistrationResult);
            return;
        }
        
        auto scopeURL = registrationKey.scope();
        session()->notificationManager().unsubscribeFromPushService(WTFMove(scopeURL), std::nullopt, [completionHandler = WTFMove(completionHandler), unregistrationResult](auto&&) mutable {
            completionHandler(unregistrationResult);
        });
        
#else
        completionHandler(unregistrationResult);
#endif
    }
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

void WebSWServerConnection::notifyClientsOfControllerChange(const HashSet<ScriptExecutionContextIdentifier>& contextIdentifiers, const ServiceWorkerData& newController)
{
    send(Messages::WebSWClientConnection::NotifyClientsOfControllerChange(contextIdentifiers, newController));
}

void WebSWServerConnection::updateWorkerStateInClient(ServiceWorkerIdentifier worker, ServiceWorkerState state)
{
    send(Messages::WebSWClientConnection::UpdateWorkerState(worker, state));
}

void WebSWServerConnection::controlClient(const NetworkResourceLoadParameters& parameters, SWServerRegistration& registration, const ResourceRequest& request)
{
    ServiceWorkerClientType clientType;
    if (parameters.options.destination  == FetchOptions::Destination::Worker)
        clientType = ServiceWorkerClientType::Worker;
    else if (parameters.options.destination  == FetchOptions::Destination::Sharedworker)
        clientType = ServiceWorkerClientType::Sharedworker;
    else
        clientType = ServiceWorkerClientType::Window;

    auto clientIdentifier = *parameters.options.clientIdentifier;
    // As per step 12 of https://w3c.github.io/ServiceWorker/#on-fetch-request-algorithm, the active service worker should be controlling the document.
    // We register the service worker client using the identifier provided by DocumentLoader and notify DocumentLoader about it.
    // If notification is successful, DocumentLoader is responsible to unregister the service worker client as needed.
    sendWithAsyncReply(Messages::WebSWClientConnection::SetServiceWorkerClientIsControlled { clientIdentifier, registration.data() }, [weakThis = WeakPtr { *this }, this, clientIdentifier](bool isSuccess) {
        if (!weakThis || isSuccess)
            return;
        unregisterServiceWorkerClient(clientIdentifier);
    });

    auto ancestorOrigins = map(parameters.frameAncestorOrigins, [](auto& origin) { return origin->toString(); });
    ServiceWorkerClientData data { clientIdentifier, clientType, ServiceWorkerClientFrameType::None, request.url(), URL(), parameters.webPageID, parameters.webFrameID, request.isAppInitiated() ? WebCore::LastNavigationWasAppInitiated::Yes : WebCore::LastNavigationWasAppInitiated::No, false, false, 0, WTFMove(ancestorOrigins) };
    registerServiceWorkerClient(ClientOrigin { registration.key().topOrigin(), SecurityOriginData::fromURL(request.url()) }, WTFMove(data), registration.identifier(), request.httpUserAgent());
}

std::unique_ptr<ServiceWorkerFetchTask> WebSWServerConnection::createFetchTask(NetworkResourceLoader& loader, const ResourceRequest& request)
{
    if (loader.parameters().serviceWorkersMode == ServiceWorkersMode::None) {
        if (loader.parameters().request.requester() == ResourceRequestRequester::Fetch && isNavigationRequest(loader.parameters().options.destination)) {
            if (auto task = ServiceWorkerFetchTask::fromNavigationPreloader(*this, loader, request, session()))
                return task;
        }
        return nullptr;
    }

    if (!server().canHandleScheme(loader.originalRequest().url().protocol()))
        return nullptr;

    std::optional<ServiceWorkerRegistrationIdentifier> serviceWorkerRegistrationIdentifier;
    if (loader.parameters().options.mode == FetchOptions::Mode::Navigate || loader.parameters().options.destination == FetchOptions::Destination::Worker || loader.parameters().options.destination == FetchOptions::Destination::Sharedworker) {
        auto topOrigin = loader.parameters().isMainFrameNavigation ? SecurityOriginData::fromURL(request.url()) : loader.parameters().topOrigin->data();
        auto* registration = doRegistrationMatching(topOrigin, request.url());
        if (!registration)
            return nullptr;

        serviceWorkerRegistrationIdentifier = registration->identifier();
        controlClient(loader.parameters(), *registration, request);
        loader.setResultingClientIdentifier(loader.parameters().options.clientIdentifier->toString());
        loader.setServiceWorkerRegistration(*registration);
    } else {
        if (!loader.parameters().serviceWorkerRegistrationIdentifier)
            return nullptr;

        if (isPotentialNavigationOrSubresourceRequest(loader.parameters().options.destination))
            return nullptr;

        serviceWorkerRegistrationIdentifier = *loader.parameters().serviceWorkerRegistrationIdentifier;
    }

    auto* registration = server().getRegistration(*serviceWorkerRegistrationIdentifier);
    auto* worker = registration ? registration->activeWorker() : nullptr;
    if (!worker) {
        SWSERVERCONNECTION_RELEASE_LOG_ERROR("startFetch: DidNotHandle because no active worker for registration %" PRIu64, serviceWorkerRegistrationIdentifier->toUInt64());
        return nullptr;
    }

    if (worker->shouldSkipFetchEvent()) {
        if (registration->shouldSoftUpdate(loader.parameters().options))
            registration->scheduleSoftUpdate(loader.isAppInitiated() ? WebCore::IsAppInitiated::Yes : WebCore::IsAppInitiated::No);

        return nullptr;
    }

    if (worker->hasTimedOutAnyFetchTasks()) {
        SWSERVERCONNECTION_RELEASE_LOG_ERROR("startFetch: DidNotHandle because worker %" PRIu64 " has some timeouts", worker->identifier().toUInt64());
        return nullptr;
    }

    bool isWorkerReady = worker->isRunning() && worker->state() == ServiceWorkerState::Activated;
    auto task = makeUnique<ServiceWorkerFetchTask>(*this, loader, ResourceRequest { request }, identifier(), worker->identifier(), *registration, session(), isWorkerReady);
    startFetch(*task, *worker);
    return task;
}

void WebSWServerConnection::startFetch(ServiceWorkerFetchTask& task, SWServerWorker& worker)
{
    auto runServerWorkerAndStartFetch = [weakThis = WeakPtr { *this }, this, task = WeakPtr { task }](bool success) mutable {
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
            server().createContextConnection(worker->registrableDomain(), worker->serviceWorkerPageIdentifier());

        auto identifier = task->serviceWorkerIdentifier();
        server().runServiceWorkerIfNecessary(identifier, [weakThis = WTFMove(weakThis), this, task = WTFMove(task)](auto* contextConnection) mutable {
#if RELEASE_LOG_DISABLED
            UNUSED_PARAM(this);
#endif
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
    }, [&](ScriptExecutionContextIdentifier identifier) {
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

URL WebSWServerConnection::clientURLFromIdentifier(ServiceWorkerOrClientIdentifier contextIdentifier)
{
    return WTF::switchOn(contextIdentifier, [&](ScriptExecutionContextIdentifier clientIdentifier) -> URL {
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

void WebSWServerConnection::scheduleUnregisterJobInServer(ServiceWorkerJobIdentifier jobIdentifier, ServiceWorkerRegistrationIdentifier registrationIdentifier, ServiceWorkerOrClientIdentifier contextIdentifier, CompletionHandler<void(UnregisterJobResult&&)>&& completionHandler)
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

void WebSWServerConnection::postMessageToServiceWorkerClient(ScriptExecutionContextIdentifier destinationContextIdentifier, const MessageWithMessagePorts& message, ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin)
{
    auto* sourceServiceWorker = server().workerByID(sourceIdentifier);
    if (!sourceServiceWorker)
        return;

    send(Messages::WebSWClientConnection::PostMessageToServiceWorkerClient { destinationContextIdentifier, message, sourceServiceWorker->data(), sourceOrigin });
}

void WebSWServerConnection::matchRegistration(const SecurityOriginData& topOrigin, const URL& clientURL, CompletionHandler<void(std::optional<ServiceWorkerRegistrationData>&&)>&& callback)
{
    if (auto* registration = doRegistrationMatching(topOrigin, clientURL)) {
        callback(registration->data());
        return;
    }
    callback({ });
}

void WebSWServerConnection::getRegistrations(const SecurityOriginData& topOrigin, const URL& clientURL, CompletionHandler<void(const Vector<ServiceWorkerRegistrationData>&)>&& callback)
{
    callback(server().getRegistrations(topOrigin, clientURL));
}

void WebSWServerConnection::registerServiceWorkerClient(WebCore::ClientOrigin&& clientOrigin, ServiceWorkerClientData&& data, const std::optional<ServiceWorkerRegistrationIdentifier>& controllingServiceWorkerRegistrationIdentifier, String&& userAgent)
{
    CONNECTION_MESSAGE_CHECK(data.identifier.processIdentifier() == identifier());
    CONNECTION_MESSAGE_CHECK(!clientOrigin.topOrigin.isNull());

    auto& contextOrigin = clientOrigin.clientOrigin;
    if (data.url.protocolIsInHTTPFamily()) {
        // We do not register any sandbox document.
        if (contextOrigin != SecurityOriginData::fromURL(data.url))
            return;
    }

    CONNECTION_MESSAGE_CHECK(!contextOrigin.isNull());

    bool isNewOrigin = WTF::allOf(m_clientOrigins.values(), [&contextOrigin](auto& origin) {
        return contextOrigin != origin.clientOrigin;
    });
    auto* contextConnection = isNewOrigin ? server().contextConnectionForRegistrableDomain(RegistrableDomain { contextOrigin }) : nullptr;

    m_clientOrigins.add(data.identifier, clientOrigin);
    server().registerServiceWorkerClient(WTFMove(clientOrigin), WTFMove(data), controllingServiceWorkerRegistrationIdentifier, WTFMove(userAgent));

    if (!m_isThrottleable)
        updateThrottleState();

    if (contextConnection) {
        auto& connection = static_cast<WebSWServerToContextConnection&>(*contextConnection);
        m_networkProcess->parentProcessConnection()->send(Messages::NetworkProcessProxy::RegisterRemoteWorkerClientProcess { RemoteWorkerType::ServiceWorker, identifier(), connection.webProcessIdentifier() }, 0);
    }
}

void WebSWServerConnection::unregisterServiceWorkerClient(const ScriptExecutionContextIdentifier& clientIdentifier)
{
    CONNECTION_MESSAGE_CHECK(clientIdentifier.processIdentifier() == identifier());
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
            m_networkProcess->parentProcessConnection()->send(Messages::NetworkProcessProxy::UnregisterRemoteWorkerClientProcess { RemoteWorkerType::ServiceWorker, identifier(), connection.webProcessIdentifier() }, 0);
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

void WebSWServerConnection::subscribeToPushService(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, Vector<uint8_t>&& applicationServerKey, CompletionHandler<void(Expected<PushSubscriptionData, ExceptionData>&&)>&& completionHandler)
{
#if !ENABLE(BUILT_IN_NOTIFICATIONS)
    UNUSED_PARAM(registrationIdentifier);
    UNUSED_PARAM(applicationServerKey);
    completionHandler(makeUnexpected(ExceptionData { AbortError, "Push service not implemented"_s }));
#else
    auto registration = server().getRegistration(registrationIdentifier);
    if (!registration) {
        completionHandler(makeUnexpected(ExceptionData { InvalidStateError, "Subscribing for push requires an active service worker"_s }));
        return;
    }

    if (!session()) {
        completionHandler(makeUnexpected(ExceptionData { InvalidStateError, "No active network session"_s }));
        return;
    }

    session()->notificationManager().subscribeToPushService(registration->scopeURLWithoutFragment(), WTFMove(applicationServerKey), [weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler), registrableDomain = RegistrableDomain(registration->data().scopeURL)] (Expected<PushSubscriptionData, ExceptionData>&& result) mutable {
        if (auto resourceLoadStatistics = weakThis && weakThis->session() ? weakThis->session()->resourceLoadStatistics() : nullptr; result && resourceLoadStatistics) {
            return resourceLoadStatistics->setMostRecentWebPushInteractionTime(WTFMove(registrableDomain), [result = WTFMove(result), completionHandler = WTFMove(completionHandler)] () mutable {
                completionHandler(WTFMove(result));
            });
        }
        completionHandler(WTFMove(result));
    });
#endif
}

void WebSWServerConnection::unsubscribeFromPushService(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, WebCore::PushSubscriptionIdentifier subscriptionIdentifier, CompletionHandler<void(Expected<bool, ExceptionData>&&)>&& completionHandler)
{
#if !ENABLE(BUILT_IN_NOTIFICATIONS)
    UNUSED_PARAM(registrationIdentifier);
    UNUSED_PARAM(subscriptionIdentifier);

    completionHandler(false);
#else
    auto registration = server().getRegistration(registrationIdentifier);
    if (!registration) {
        completionHandler(makeUnexpected(ExceptionData { InvalidStateError, "Unsubscribing from push requires a service worker"_s }));
        return;
    }

    if (!session()) {
        completionHandler(makeUnexpected(ExceptionData { InvalidStateError, "No active network session"_s }));
        return;
    }

    session()->notificationManager().unsubscribeFromPushService(registration->scopeURLWithoutFragment(), subscriptionIdentifier, WTFMove(completionHandler));
#endif
}

void WebSWServerConnection::getPushSubscription(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, CompletionHandler<void(Expected<std::optional<PushSubscriptionData>, ExceptionData>&&)>&& completionHandler)
{
#if !ENABLE(BUILT_IN_NOTIFICATIONS)
    UNUSED_PARAM(registrationIdentifier);

    completionHandler(std::optional<PushSubscriptionData>(std::nullopt));
#else
    auto registration = server().getRegistration(registrationIdentifier);
    if (!registration) {
        completionHandler(makeUnexpected(ExceptionData { InvalidStateError, "Getting push subscription requires a service worker"_s }));
        return;
    }

    if (!session()) {
        completionHandler(makeUnexpected(ExceptionData { InvalidStateError, "No active network session"_s }));
        return;
    }

    session()->notificationManager().getPushSubscription(registration->scopeURLWithoutFragment(), WTFMove(completionHandler));
#endif
}

void WebSWServerConnection::getPushPermissionState(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, CompletionHandler<void(Expected<uint8_t, ExceptionData>&&)>&& completionHandler)
{
#if !ENABLE(BUILT_IN_NOTIFICATIONS)
    UNUSED_PARAM(registrationIdentifier);

    completionHandler(static_cast<uint8_t>(PushPermissionState::Denied));
#else
    auto registration = server().getRegistration(registrationIdentifier);
    if (!registration) {
        completionHandler(makeUnexpected(ExceptionData { InvalidStateError, "Getting push permission state requires a service worker"_s }));
        return;
    }

    if (!session()) {
        completionHandler(makeUnexpected(ExceptionData { InvalidStateError, "No active network session"_s }));
        return;
    }

    session()->notificationManager().getPushPermissionState(registration->scopeURLWithoutFragment(), WTFMove(completionHandler));
#endif
}

void WebSWServerConnection::contextConnectionCreated(SWServerToContextConnection& contextConnection)
{
    auto& connection =  static_cast<WebSWServerToContextConnection&>(contextConnection);
    connection.setThrottleState(computeThrottleState(connection.registrableDomain()));

    if (hasMatchingClient(connection.registrableDomain()))
        m_networkProcess->parentProcessConnection()->send(Messages::NetworkProcessProxy::RegisterRemoteWorkerClientProcess { RemoteWorkerType::ServiceWorker, identifier(), connection.webProcessIdentifier() }, 0);
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

NetworkSession* WebSWServerConnection::session()
{
    return m_networkProcess->networkSession(sessionID());
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

void WebSWServerConnection::enableNavigationPreload(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, ExceptionOrVoidCallback&& callback)
{
    auto* registration = server().getRegistration(registrationIdentifier);
    if (!registration) {
        callback(ExceptionData { InvalidStateError, "No registration"_s });
        return;
    }
    callback(registration->enableNavigationPreload());
}

void WebSWServerConnection::disableNavigationPreload(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, ExceptionOrVoidCallback&& callback)
{
    auto* registration = server().getRegistration(registrationIdentifier);
    if (!registration) {
        callback(ExceptionData { InvalidStateError, "No registration"_s });
        return;
    }
    callback(registration->disableNavigationPreload());
}

void WebSWServerConnection::setNavigationPreloadHeaderValue(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, String&& headerValue, ExceptionOrVoidCallback&& callback)
{
    auto* registration = server().getRegistration(registrationIdentifier);
    if (!registration) {
        callback(ExceptionData { InvalidStateError, "No registration"_s });
        return;
    }
    callback(registration->setNavigationPreloadHeaderValue(WTFMove(headerValue)));
}

void WebSWServerConnection::getNavigationPreloadState(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, ExceptionOrNavigationPreloadStateCallback&& callback)
{
    auto* registration = server().getRegistration(registrationIdentifier);
    if (!registration) {
        callback(makeUnexpected(ExceptionData { InvalidStateError, { } }));
        return;
    }
    callback(registration->navigationPreloadState());
}

void WebSWServerConnection::focusServiceWorkerClient(WebCore::ScriptExecutionContextIdentifier clientIdentifier, CompletionHandler<void(std::optional<ServiceWorkerClientData>&&)>&& callback)
{
    sendWithAsyncReply(Messages::WebSWClientConnection::FocusServiceWorkerClient { clientIdentifier }, WTFMove(callback));
}

void WebSWServerConnection::transferServiceWorkerLoadToNewWebProcess(NetworkResourceLoader& loader, WebCore::SWServerRegistration& registration)
{
    controlClient(loader.parameters(), registration, loader.originalRequest());
}

std::optional<SWServer::GatheredClientData> WebSWServerConnection::gatherClientData(ScriptExecutionContextIdentifier clientIdentifier)
{
    ASSERT(clientIdentifier.processIdentifier() == identifier());
    auto iterator = m_clientOrigins.find(clientIdentifier);
    if (iterator == m_clientOrigins.end())
        return { };

    return server().gatherClientData(iterator->value, clientIdentifier);
}

} // namespace WebKit

#undef CONNECTION_MESSAGE_CHECK_COMPLETION
#undef CONNECTION_MESSAGE_CHECK
#undef SWSERVERCONNECTION_RELEASE_LOG
#undef SWSERVERCONNECTION_RELEASE_LOG_ERROR

#endif // ENABLE(SERVICE_WORKER)
