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
#include "WebSWClientConnection.h"

#if ENABLE(SERVICE_WORKER)

#include "DataReference.h"
#include "FormDataReference.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkProcessMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include "WebSWClientConnectionMessages.h"
#include "WebSWContextManagerConnection.h"
#include "WebSWOriginTable.h"
#include "WebSWServerConnectionMessages.h"
#include <WebCore/Document.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/FocusController.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SerializedScriptValue.h>
#include <WebCore/ServiceWorkerClientData.h>
#include <WebCore/ServiceWorkerJobData.h>
#include <WebCore/ServiceWorkerRegistrationData.h>
#include <WebCore/ServiceWorkerRegistrationKey.h>
#include <WebCore/WorkerFetchResult.h>
#include <WebCore/WorkerScriptLoader.h>

namespace WebKit {

using namespace PAL;
using namespace WebCore;

WebSWClientConnection::WebSWClientConnection()
    : m_identifier(Process::identifier())
    , m_swOriginTable(makeUniqueRef<WebSWOriginTable>())
{
}

WebSWClientConnection::~WebSWClientConnection()
{
    ASSERT(isMainRunLoop());

    clear();
}

void WebSWClientConnection::establishConnection()
{
    ASSERT(isMainRunLoop());

    messageSenderConnection()->addWorkQueueMessageReceiver(Messages::WebSWClientConnection::messageReceiverName(), WebSWContextManagerConnection::sharedQueue(), *this);
}

void WebSWClientConnection::closeConnection()
{
    ASSERT(isMainRunLoop());

    if (m_connection)
        m_connection->removeWorkQueueMessageReceiver(Messages::WebSWClientConnection::messageReceiverName());
}

IPC::Connection* WebSWClientConnection::messageSenderConnection() const
{
    if (!m_connection)
        m_connection = &WebProcess::singleton().ensureNetworkProcessConnection().connection();
    return m_connection.get();
}

void WebSWClientConnection::scheduleJobInServer(const ServiceWorkerJobData& jobData)
{
    ASSERT(isMainRunLoop());

    runOrDelayTaskForImport([this, jobData] {
        send(Messages::WebSWServerConnection::ScheduleJobInServer { jobData });
    });
}

void WebSWClientConnection::finishFetchingScriptInServer(const ServiceWorkerJobDataIdentifier& jobDataIdentifier, ServiceWorkerRegistrationKey&& registrationKey, WorkerFetchResult&& result)
{
    ASSERT(isMainRunLoop());

    send(Messages::WebSWServerConnection::FinishFetchingScriptInServer { jobDataIdentifier, registrationKey, result });
}

void WebSWClientConnection::addServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier identifier)
{
    ASSERT(isMainRunLoop());

    // FIXME: We should send the message to network process only if this is a new registration, once we correctly handle recovery upon network process crash.
    WebProcess::singleton().addServiceWorkerRegistration(identifier);
    send(Messages::WebSWServerConnection::AddServiceWorkerRegistrationInServer { identifier });
}

void WebSWClientConnection::removeServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier identifier)
{
    ASSERT(isMainRunLoop());

    if (WebProcess::singleton().removeServiceWorkerRegistration(identifier))
        send(Messages::WebSWServerConnection::RemoveServiceWorkerRegistrationInServer { identifier });
}

void WebSWClientConnection::scheduleUnregisterJobInServer(ServiceWorkerRegistrationIdentifier registrationIdentifier, WebCore::ServiceWorkerOrClientIdentifier documentIdentifier, CompletionHandler<void(ExceptionOr<bool>&&)>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    sendWithAsyncReply(Messages::WebSWServerConnection::ScheduleUnregisterJobInServer { ServiceWorkerJobIdentifier::generateThreadSafe(), registrationIdentifier, documentIdentifier }, [completionHandler = WTFMove(completionHandler)](auto&& result) mutable {
        if (!result.has_value())
            return completionHandler(result.error().toException());
        completionHandler(result.value());
    });
}

void WebSWClientConnection::postMessageToServiceWorker(ServiceWorkerIdentifier destinationIdentifier, MessageWithMessagePorts&& message, const ServiceWorkerOrClientIdentifier& sourceIdentifier)
{
    send(Messages::WebSWServerConnection::PostMessageToServiceWorker { destinationIdentifier, WTFMove(message), sourceIdentifier });
}

void WebSWClientConnection::registerServiceWorkerClient(const ClientOrigin& clientOrigin, WebCore::ServiceWorkerClientData&& data, const std::optional<WebCore::ServiceWorkerRegistrationIdentifier>& controllingServiceWorkerRegistrationIdentifier, String&& userAgent)
{
    send(Messages::WebSWServerConnection::RegisterServiceWorkerClient { clientOrigin, data, controllingServiceWorkerRegistrationIdentifier, userAgent });
}

void WebSWClientConnection::unregisterServiceWorkerClient(ScriptExecutionContextIdentifier contextIdentifier)
{
    send(Messages::WebSWServerConnection::UnregisterServiceWorkerClient { contextIdentifier });
}

void WebSWClientConnection::didResolveRegistrationPromise(const ServiceWorkerRegistrationKey& key)
{
    send(Messages::WebSWServerConnection::DidResolveRegistrationPromise { key });
}

bool WebSWClientConnection::mayHaveServiceWorkerRegisteredForOrigin(const SecurityOriginData& origin) const
{
    ASSERT(isMainRunLoop());

    if (!m_swOriginTable->isImported())
        return true;

    return m_swOriginTable->contains(origin);
}

void WebSWClientConnection::setSWOriginTableSharedMemory(const SharedMemory::IPCHandle& ipcHandle)
{
    assertIsCurrent(WebSWContextManagerConnection::sharedQueue());

    m_swOriginTable->setSharedMemory(ipcHandle.handle);
}

void WebSWClientConnection::setSWOriginTableIsImported()
{
    assertIsCurrent(WebSWContextManagerConnection::sharedQueue());

    callOnMainRunLoop([protectedThis = Ref { *this }, this] {
        m_swOriginTable->setIsImported();
        while (!m_tasksPendingOriginImport.isEmpty())
            m_tasksPendingOriginImport.takeFirst()();
    });
}

void WebSWClientConnection::matchRegistration(SecurityOriginData&& topOrigin, const URL& clientURL, RegistrationCallback&& callback)
{
    ASSERT(isMainRunLoop());

    if (!mayHaveServiceWorkerRegisteredForOrigin(topOrigin)) {
        callback(std::nullopt);
        return;
    }

    runOrDelayTaskForImport([this, callback = WTFMove(callback), topOrigin = WTFMove(topOrigin), clientURL]() mutable {
        sendWithAsyncReply(Messages::WebSWServerConnection::MatchRegistration { topOrigin, clientURL }, WTFMove(callback));
    });
}

void WebSWClientConnection::runOrDelayTaskForImport(Function<void()>&& task)
{
    ASSERT(isMainRunLoop());

    if (m_swOriginTable->isImported()) {
        task();
        return;
    }
    m_tasksPendingOriginImport.append(WTFMove(task));
}

void WebSWClientConnection::whenRegistrationReady(const SecurityOriginData& topOrigin, const URL& clientURL, WhenRegistrationReadyCallback&& callback)
{
    ASSERT(isMainRunLoop());

    sendWithAsyncReply(Messages::WebSWServerConnection::WhenRegistrationReady { topOrigin, clientURL }, [callback = WTFMove(callback)](auto result) mutable {
        if (!result)
            return;
        //  Let's go through the shared queue to resolve the ready promise once the registration state is updated.
        WebSWContextManagerConnection::sharedQueue().dispatch([callback = WTFMove(callback), result = crossThreadCopy(WTFMove(result))]() mutable {
            callOnMainRunLoop([callback = WTFMove(callback), result = crossThreadCopy(WTFMove(result))]() mutable {
                callback(*WTFMove(result));
            });
        });
    });
}

void WebSWClientConnection::setServiceWorkerClientIsControlled(ScriptExecutionContextIdentifier identifier, ServiceWorkerRegistrationData&& data, CompletionHandler<void(bool)>&& completionHandler)
{
    assertIsCurrent(WebSWContextManagerConnection::sharedQueue());

    auto callback = [completionHandler = WTFMove(completionHandler)](bool result) mutable {
        WebSWContextManagerConnection::sharedQueue().dispatch([completionHandler = WTFMove(completionHandler), result]() mutable {
            completionHandler(result);
        });
    };
    callOnMainRunLoop([callback = WTFMove(callback), identifier, data = WTFMove(data).isolatedCopy()]() mutable {
        if (auto* loader = DocumentLoader::fromScriptExecutionContextIdentifier(identifier)) {
            callback(loader->setControllingServiceWorkerRegistration(WTFMove(data)));
            return;
        }

        if (auto* loader = WorkerScriptLoader::fromScriptExecutionContextIdentifier(identifier)) {
            callback(data.activeWorker ? loader->setControllingServiceWorker(WTFMove(*data.activeWorker)) : false);
            return;
        }

        callback(false);
    });
}

void WebSWClientConnection::getRegistrations(SecurityOriginData&& topOrigin, const URL& clientURL, GetRegistrationsCallback&& callback)
{
    ASSERT(isMainRunLoop());

    if (!mayHaveServiceWorkerRegisteredForOrigin(topOrigin)) {
        callback({ });
        return;
    }

    runOrDelayTaskForImport([this, callback = WTFMove(callback), topOrigin = WTFMove(topOrigin), clientURL]() mutable {
        sendWithAsyncReply(Messages::WebSWServerConnection::GetRegistrations { topOrigin, clientURL }, WTFMove(callback));
    });
}

void WebSWClientConnection::connectionToServerLost()
{
    ASSERT(isMainRunLoop());

    closeConnection();
    setIsClosed();
    clear();
}

void WebSWClientConnection::clear()
{
    ASSERT(isMainRunLoop());

    clearPendingJobs();
}

void WebSWClientConnection::terminateWorkerForTesting(ServiceWorkerIdentifier identifier, CompletionHandler<void()>&& callback)
{
    ASSERT(isMainRunLoop());

    sendWithAsyncReply(Messages::WebSWServerConnection::TerminateWorkerFromClient { identifier }, WTFMove(callback));
}

void WebSWClientConnection::whenServiceWorkerIsTerminatedForTesting(ServiceWorkerIdentifier identifier, CompletionHandler<void()>&& callback)
{
    ASSERT(isMainRunLoop());

    sendWithAsyncReply(Messages::WebSWServerConnection::WhenServiceWorkerIsTerminatedForTesting { identifier }, WTFMove(callback));
}

void WebSWClientConnection::updateThrottleState()
{
    ASSERT(isMainRunLoop());

    m_isThrottleable = WebProcess::singleton().areAllPagesThrottleable();
    send(Messages::WebSWServerConnection::SetThrottleState { m_isThrottleable });
}

void WebSWClientConnection::storeRegistrationsOnDiskForTesting(CompletionHandler<void()>&& callback)
{
    ASSERT(isMainRunLoop());

    sendWithAsyncReply(Messages::WebSWServerConnection::StoreRegistrationsOnDisk { }, WTFMove(callback));
}

void WebSWClientConnection::subscribeToPushService(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, const Vector<uint8_t>& applicationServerKey, SubscribeToPushServiceCallback&& callback)
{
    ASSERT(isMainRunLoop());

    sendWithAsyncReply(Messages::WebSWServerConnection::SubscribeToPushService { registrationIdentifier, applicationServerKey }, [callback = WTFMove(callback)](auto&& result) mutable {
        if (!result.has_value())
            return callback(result.error().toException());
        callback(WTFMove(*result));
    });
}

void WebSWClientConnection::unsubscribeFromPushService(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, WebCore::PushSubscriptionIdentifier subscriptionIdentifier, UnsubscribeFromPushServiceCallback&& callback)
{
    ASSERT(isMainRunLoop());

    sendWithAsyncReply(Messages::WebSWServerConnection::UnsubscribeFromPushService { registrationIdentifier, subscriptionIdentifier }, [callback = WTFMove(callback)](auto&& result) mutable {
        if (!result.has_value())
            return callback(result.error().toException());
        callback(*result);
    });
}

void WebSWClientConnection::getPushSubscription(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, GetPushSubscriptionCallback&& callback)
{
    ASSERT(isMainRunLoop());

    sendWithAsyncReply(Messages::WebSWServerConnection::GetPushSubscription { registrationIdentifier }, [callback = WTFMove(callback)](auto&& result) mutable {
        if (!result.has_value())
            return callback(result.error().toException());
        callback(WTFMove(*result));
    });
}

void WebSWClientConnection::getPushPermissionState(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, GetPushPermissionStateCallback&& callback)
{
    ASSERT(isMainRunLoop());

    sendWithAsyncReply(Messages::WebSWServerConnection::GetPushPermissionState { registrationIdentifier }, [callback = WTFMove(callback)](auto&& result) mutable {
        if (!result.has_value())
            return callback(result.error().toException());
        callback(static_cast<PushPermissionState>(*result));
    });
}

void WebSWClientConnection::getNotifications(const URL& registrationURL, const String& tag, GetNotificationsCallback&& callback)
{
    ASSERT(isMainRunLoop());

    WebProcess::singleton().parentProcessConnection()->sendWithAsyncReply(Messages::WebProcessProxy::GetNotifications { registrationURL, tag }, WTFMove(callback));
}

void WebSWClientConnection::enableNavigationPreload(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, ExceptionOrVoidCallback&& callback)
{
    ASSERT(isMainRunLoop());

    sendWithAsyncReply(Messages::WebSWServerConnection::EnableNavigationPreload { registrationIdentifier }, [callback = WTFMove(callback)](auto&& error) mutable {
        if (error)
            return callback(error->toException());
        callback({ });
    });
}

void WebSWClientConnection::disableNavigationPreload(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, ExceptionOrVoidCallback&& callback)
{
    ASSERT(isMainRunLoop());

    sendWithAsyncReply(Messages::WebSWServerConnection::DisableNavigationPreload { registrationIdentifier }, [callback = WTFMove(callback)](auto&& error) mutable {
        if (error)
            return callback(error->toException());
        callback({ });
    });
}

void WebSWClientConnection::setNavigationPreloadHeaderValue(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, String&& headerValue, ExceptionOrVoidCallback&& callback)
{
    ASSERT(isMainRunLoop());

    sendWithAsyncReply(Messages::WebSWServerConnection::SetNavigationPreloadHeaderValue { registrationIdentifier, headerValue }, [callback = WTFMove(callback)](auto&& error) mutable {
        if (error)
            return callback(error->toException());
        callback({ });
    });
}

void WebSWClientConnection::getNavigationPreloadState(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, ExceptionOrNavigationPreloadStateCallback&& callback)
{
    ASSERT(isMainRunLoop());

    sendWithAsyncReply(Messages::WebSWServerConnection::GetNavigationPreloadState { registrationIdentifier }, [callback = WTFMove(callback)](auto&& result) mutable {
        if (!result.has_value())
            return callback(result.error().toException());
        callback(WTFMove(*result));
    });
}

void WebSWClientConnection::focusServiceWorkerClient(ScriptExecutionContextIdentifier clientIdentifier, CompletionHandler<void(std::optional<ServiceWorkerClientData>&&)>&& completionHandler)
{
    assertIsCurrent(WebSWContextManagerConnection::sharedQueue());

    auto callback = [completionHandler = WTFMove(completionHandler)](std::optional<ServiceWorkerClientData>&& result) mutable {
        WebSWContextManagerConnection::sharedQueue().dispatch([completionHandler = WTFMove(completionHandler), result = crossThreadCopy(WTFMove(result))]() mutable {
            completionHandler(WTFMove(result));
        });
    };
    callOnMainRunLoop([callback = WTFMove(callback), clientIdentifier]() mutable {
        auto* client = Document::allDocumentsMap().get(clientIdentifier);
        auto* page = client ? client->page() : nullptr;
        if (!page) {
            callback({ });
            return;
        }

        WebPage::fromCorePage(*page).sendWithAsyncReply(Messages::WebPageProxy::FocusFromServiceWorker { }, [clientIdentifier, callback = WTFMove(callback)]() mutable {
            auto* client = Document::allDocumentsMap().get(clientIdentifier);
            auto* frame = client ? client->frame() : nullptr;
            auto* page = frame ? frame->page() : nullptr;
            if (!page) {
                callback({ });
                return;
            }
            page->focusController().setFocusedFrame(frame);
            callback(ServiceWorkerClientData::from(*client));
        });
    });
}

void WebSWClientConnection::refreshImportedScripts(WebCore::ServiceWorkerJobIdentifier identifier, WebCore::FetchOptions::Cache cache, Vector<URL>&& urls, WebCore::ServiceWorkerJob::RefreshImportedScriptsCallback&& callback)
{
    SWClientConnection::refreshImportedScripts(identifier, cache, WTFMove(urls), WTFMove(callback), WebSWContextManagerConnection::sharedQueue());
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
