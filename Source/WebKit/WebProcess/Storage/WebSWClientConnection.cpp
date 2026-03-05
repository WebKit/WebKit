/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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

#include "FormDataReference.h"
#include "Logging.h"
#include "MessageSenderInlines.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkProcessMessages.h"
#include "SharedBufferReference.h"
#include "WebMessagePortChannelProvider.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include "WebSWOriginTable.h"
#include "WebSWServerConnectionMessages.h"
#include <WebCore/BackgroundFetchInformation.h>
#include <WebCore/BackgroundFetchRecordInformation.h>
#include <WebCore/BackgroundFetchRequest.h>
#include <WebCore/CookieChangeSubscription.h>
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DocumentPage.h>
#include <WebCore/EventLoop.h>
#include <WebCore/ExceptionOr.h>
#include <WebCore/FocusController.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/FrameInlines.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SerializedScriptValue.h>
#include <WebCore/ServiceWorkerClientData.h>
#include <WebCore/ServiceWorkerJobData.h>
#include <WebCore/ServiceWorkerRegistrationData.h>
#include <WebCore/ServiceWorkerRegistrationKey.h>
#include <WebCore/ServiceWorkerRoute.h>
#include <WebCore/WorkerFetchResult.h>
#include <WebCore/WorkerScriptLoader.h>
#include <wtf/Vector.h>

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
    clear();
}

IPC::Connection* WebSWClientConnection::messageSenderConnection() const
{
    return &WebProcess::singleton().ensureNetworkProcessConnection().connection();
}

void WebSWClientConnection::scheduleJobInServer(const ServiceWorkerJobData& jobData)
{
    runOrDelayTaskForImport([weakThis = WeakPtr { *this }, jobData] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->send(Messages::WebSWServerConnection::ScheduleJobInServer { jobData });
    });
}

void WebSWClientConnection::finishFetchingScriptInServer(const ServiceWorkerJobDataIdentifier& jobDataIdentifier, ServiceWorkerRegistrationKey&& registrationKey, WorkerFetchResult&& result)
{
    send(Messages::WebSWServerConnection::FinishFetchingScriptInServer { jobDataIdentifier, registrationKey, result });
}

void WebSWClientConnection::addServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier identifier)
{
    // FIXME: We should send the message to network process only if this is a new registration, once we correctly handle recovery upon network process crash.
    WebProcess::singleton().addServiceWorkerRegistration(identifier);
    send(Messages::WebSWServerConnection::AddServiceWorkerRegistrationInServer { identifier });
}

void WebSWClientConnection::removeServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier identifier)
{
    if (WebProcess::singleton().removeServiceWorkerRegistration(identifier)) {
        RunLoop::mainSingleton().dispatch([identifier, connection = Ref { *this }]() {
            connection->send(Messages::WebSWServerConnection::RemoveServiceWorkerRegistrationInServer { identifier });
        });
    }
}

void WebSWClientConnection::registerServiceWorkerInServer(ServiceWorkerIdentifier identifier)
{
    if (WebProcess::singleton().registerServiceWorker(identifier))
        send(Messages::WebSWServerConnection::RegisterServiceWorkerInServer { identifier });
}

void WebSWClientConnection::unregisterServiceWorkerInServer(ServiceWorkerIdentifier identifier)
{
    if (WebProcess::singleton().unregisterServiceWorker(identifier)) {
        RunLoop::mainSingleton().dispatch([identifier, connection = Ref { *this }]() {
            connection->send(Messages::WebSWServerConnection::UnregisterServiceWorkerInServer { identifier });
        });
    }
}

void WebSWClientConnection::scheduleUnregisterJobInServer(ServiceWorkerRegistrationIdentifier registrationIdentifier, WebCore::ServiceWorkerOrClientIdentifier documentIdentifier, CompletionHandler<void(ExceptionOr<bool>&&)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::ScheduleUnregisterJobInServer { ServiceWorkerJobIdentifier::generate(), registrationIdentifier, documentIdentifier }, [completionHandler = WTF::move(completionHandler)](auto&& result) mutable {
        if (!result.has_value())
            return completionHandler(result.error().toException());
        completionHandler(result.value());
    });
}

void WebSWClientConnection::postMessageToServiceWorker(ServiceWorkerIdentifier destinationIdentifier, MessageWithMessagePorts&& message, const ServiceWorkerOrClientIdentifier& sourceIdentifier)
{
    for (auto& port : message.transferredPorts)
        WebMessagePortChannelProvider::singleton().messagePortSentToRemote(port.first);

    send(Messages::WebSWServerConnection::PostMessageToServiceWorker { destinationIdentifier, WTF::move(message), sourceIdentifier });
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
    if (!m_swOriginTable->isImported())
        return true;

    return m_swOriginTable->contains(origin);
}

void WebSWClientConnection::setSWOriginTableSharedMemory(SharedMemory::Handle&& handle)
{
    m_swOriginTable->setSharedMemory(WTF::move(handle));
}

void WebSWClientConnection::setSWOriginTableIsImported()
{
    m_swOriginTable->setIsImported();
    while (!m_tasksPendingOriginImport.isEmpty())
        m_tasksPendingOriginImport.takeFirst()();
}

void WebSWClientConnection::matchRegistration(SecurityOriginData&& topOrigin, const URL& clientURL, RegistrationCallback&& callback)
{
    ASSERT(isMainRunLoop());

    if (!mayHaveServiceWorkerRegisteredForOrigin(topOrigin)) {
        callback(std::nullopt);
        return;
    }

    CompletionHandlerWithFinalizer<void(std::optional<ServiceWorkerRegistrationData>)> completionHandler(WTF::move(callback), [] (auto& callback) {
        callback(std::nullopt);
    });
    runOrDelayTaskForImport([weakThis = WeakPtr { *this }, completionHandler = WTF::move(completionHandler), topOrigin = WTF::move(topOrigin), clientURL]() mutable {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->sendWithAsyncReply(Messages::WebSWServerConnection::MatchRegistration { topOrigin, clientURL }, WTF::move(completionHandler));
    });
}

void WebSWClientConnection::runOrDelayTaskForImport(Function<void()>&& task)
{
    if (m_swOriginTable->isImported()) {
        task();
        return;
    }
    m_tasksPendingOriginImport.append(WTF::move(task));
}

void WebSWClientConnection::whenRegistrationReady(const SecurityOriginData& topOrigin, const URL& clientURL, WhenRegistrationReadyCallback&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::WhenRegistrationReady { topOrigin, clientURL }, [callback = WTF::move(callback)](auto result) mutable {
        if (result)
            callback(*WTF::move(result));
    });
}

void WebSWClientConnection::setServiceWorkerClientIsControlled(ScriptExecutionContextIdentifier identifier, ServiceWorkerRegistrationData&& data, CompletionHandler<void(bool)>&& completionHandler)
{
    if (RefPtr loader = DocumentLoader::fromScriptExecutionContextIdentifier(identifier)) {
        completionHandler(loader->setControllingServiceWorkerRegistration(WTF::move(data)));
        return;
    }

    if (auto manager = WorkerScriptLoader::serviceWorkerDataManagerFromIdentifier(identifier)) {
        if (data.activeWorker) {
            manager->setData(WTF::move(*data.activeWorker));
            completionHandler(true);
            return;
        }
    }

    completionHandler(false);
}

void WebSWClientConnection::getRegistrations(SecurityOriginData&& topOrigin, const URL& clientURL, GetRegistrationsCallback&& callback)
{
    ASSERT(isMainRunLoop());

    if (!mayHaveServiceWorkerRegisteredForOrigin(topOrigin)) {
        callback({ });
        return;
    }

    runOrDelayTaskForImport([weakThis = WeakPtr { *this }, callback = WTF::move(callback), topOrigin = WTF::move(topOrigin), clientURL]() mutable {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->sendWithAsyncReply(Messages::WebSWServerConnection::GetRegistrations { topOrigin, clientURL }, WTF::move(callback));
    });
}

void WebSWClientConnection::connectionToServerLost()
{
    setIsClosed();
    clear();
}

void WebSWClientConnection::clear()
{
    clearPendingJobs();
}

void WebSWClientConnection::terminateWorkerForTesting(ServiceWorkerIdentifier identifier, CompletionHandler<void()>&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::TerminateWorkerFromClient { identifier }, WTF::move(callback));
}

void WebSWClientConnection::whenServiceWorkerIsTerminatedForTesting(ServiceWorkerIdentifier identifier, CompletionHandler<void()>&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::WhenServiceWorkerIsTerminatedForTesting { identifier }, WTF::move(callback));
}

void WebSWClientConnection::updateThrottleState()
{
    m_isThrottleable = WebProcess::singleton().areAllPagesThrottleable();
    send(Messages::WebSWServerConnection::SetThrottleState { m_isThrottleable });
}

void WebSWClientConnection::storeRegistrationsOnDiskForTesting(CompletionHandler<void()>&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::StoreRegistrationsOnDisk { }, WTF::move(callback));
}

template<typename C, typename U> void callExceptionOrResultCallback(C&& callback, U&& valueOrException)
{
    if (!valueOrException.has_value()) {
        callback(valueOrException.error().toException());
        return;
    }
    callback(WTF::move(*valueOrException));
}

void WebSWClientConnection::subscribeToPushService(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, const Vector<uint8_t>& applicationServerKey, SubscribeToPushServiceCallback&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::SubscribeToPushService { registrationIdentifier, applicationServerKey }, [callback = WTF::move(callback)](auto&& result) mutable {
        callExceptionOrResultCallback(WTF::move(callback), std::forward<decltype(result)>(result));
    });
}

void WebSWClientConnection::unsubscribeFromPushService(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, WebCore::PushSubscriptionIdentifier subscriptionIdentifier, UnsubscribeFromPushServiceCallback&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::UnsubscribeFromPushService { registrationIdentifier, subscriptionIdentifier }, [callback = WTF::move(callback)](auto&& result) mutable {
        callExceptionOrResultCallback(WTF::move(callback), WTF::move(result));
    });
}

void WebSWClientConnection::getPushSubscription(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, GetPushSubscriptionCallback&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::GetPushSubscription { registrationIdentifier }, [callback = WTF::move(callback)](auto&& result) mutable {
        callExceptionOrResultCallback(WTF::move(callback), WTF::move(result));
    });
}

void WebSWClientConnection::getPushPermissionState(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, GetPushPermissionStateCallback&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::GetPushPermissionState { registrationIdentifier }, [callback = WTF::move(callback)](auto&& result) mutable {
        if (!result.has_value())
            return callback(result.error().toException());
        callback(static_cast<PushPermissionState>(*result));
    });
}

#if ENABLE(NOTIFICATION_EVENT)
void WebSWClientConnection::getNotifications(const URL& registrationURL, const String& tag, GetNotificationsCallback&& callback)
{
#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    if (DeprecatedGlobalSettings::builtInNotificationsEnabled()) {
        sendWithAsyncReply(Messages::WebSWServerConnection::GetNotifications { registrationURL, tag }, [callback = WTF::move(callback)](auto&& result) mutable {
            if (!result.has_value())
                return callback(result.error().toException());

            callback(static_cast<Vector<NotificationData>>(*result));
        });

        return;
    }
#endif

    protect(WebProcess::singleton().parentProcessConnection())->sendWithAsyncReply(Messages::WebProcessProxy::GetNotifications { registrationURL, tag }, WTF::move(callback));
}
#endif

void WebSWClientConnection::enableNavigationPreload(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, ExceptionOrVoidCallback&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::EnableNavigationPreload { registrationIdentifier }, [callback = WTF::move(callback)](auto&& error) mutable {
        if (error)
            return callback(error->toException());
        callback({ });
    });
}

void WebSWClientConnection::disableNavigationPreload(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, ExceptionOrVoidCallback&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::DisableNavigationPreload { registrationIdentifier }, [callback = WTF::move(callback)](auto&& error) mutable {
        if (error)
            return callback(error->toException());
        callback({ });
    });
}

void WebSWClientConnection::setNavigationPreloadHeaderValue(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, String&& headerValue, ExceptionOrVoidCallback&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::SetNavigationPreloadHeaderValue { registrationIdentifier, headerValue }, [callback = WTF::move(callback)](auto&& error) mutable {
        if (error)
            return callback(error->toException());
        callback({ });
    });
}

void WebSWClientConnection::getNavigationPreloadState(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, ExceptionOrNavigationPreloadStateCallback&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::GetNavigationPreloadState { registrationIdentifier }, [callback = WTF::move(callback)](auto&& result) mutable {
        callExceptionOrResultCallback(WTF::move(callback), WTF::move(result));
    });
}

void WebSWClientConnection::startBackgroundFetch(ServiceWorkerRegistrationIdentifier registrationIdentifier, const String& backgroundFetchIdentifier, Vector<BackgroundFetchRequest>&& requests, BackgroundFetchOptions&& options, ExceptionOrBackgroundFetchInformationCallback&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::StartBackgroundFetch { registrationIdentifier, backgroundFetchIdentifier, requests, options }, [callback = WTF::move(callback)](auto&& result) mutable {
        callExceptionOrResultCallback(WTF::move(callback), WTF::move(result));
    });
}

void WebSWClientConnection::backgroundFetchInformation(ServiceWorkerRegistrationIdentifier registrationIdentifier, const String& backgroundFetchIdentifier, ExceptionOrBackgroundFetchInformationCallback&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::BackgroundFetchInformation { registrationIdentifier, backgroundFetchIdentifier }, [callback = WTF::move(callback)](auto&& result) mutable {
        callExceptionOrResultCallback(WTF::move(callback), WTF::move(result));
    });
}

void WebSWClientConnection::backgroundFetchIdentifiers(ServiceWorkerRegistrationIdentifier registrationIdentifier, BackgroundFetchIdentifiersCallback&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::BackgroundFetchIdentifiers { registrationIdentifier }, WTF::move(callback));
}

void WebSWClientConnection::abortBackgroundFetch(ServiceWorkerRegistrationIdentifier registrationIdentifier, const String& backgroundFetchIdentifier, AbortBackgroundFetchCallback&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::AbortBackgroundFetch { registrationIdentifier, backgroundFetchIdentifier }, WTF::move(callback));
}

void WebSWClientConnection::matchBackgroundFetch(ServiceWorkerRegistrationIdentifier registrationIdentifier, const String& backgroundFetchIdentifier, RetrieveRecordsOptions&& recordOptions, MatchBackgroundFetchCallback&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::MatchBackgroundFetch { registrationIdentifier, backgroundFetchIdentifier, recordOptions }, WTF::move(callback));
}

void WebSWClientConnection::retrieveRecordResponse(BackgroundFetchRecordIdentifier recordIdentifier, RetrieveRecordResponseCallback&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::RetrieveRecordResponse { recordIdentifier }, [callback = WTF::move(callback)](auto&& result) mutable {
        callExceptionOrResultCallback(WTF::move(callback), WTF::move(result));
    });
}

void WebSWClientConnection::retrieveRecordResponseBody(BackgroundFetchRecordIdentifier recordIdentifier, RetrieveRecordResponseBodyCallback&& callback)
{
    auto identifier = RetrieveRecordResponseBodyCallbackIdentifier::generate();
    m_retrieveRecordResponseBodyCallbacks.add(identifier, WTF::move(callback));
    send(Messages::WebSWServerConnection::RetrieveRecordResponseBody { recordIdentifier, identifier });
}

void WebSWClientConnection::notifyRecordResponseBodyChunk(RetrieveRecordResponseBodyCallbackIdentifier identifier, IPC::SharedBufferReference&& data)
{
    auto iterator = m_retrieveRecordResponseBodyCallbacks.find(identifier);
    if (iterator == m_retrieveRecordResponseBodyCallbacks.end())
        return;
    auto buffer = data.unsafeBuffer();
    bool isDone = !buffer;
    iterator->value(WTF::move(buffer));
    if (isDone)
        m_retrieveRecordResponseBodyCallbacks.remove(iterator);
}

void WebSWClientConnection::notifyRecordResponseBodyEnd(RetrieveRecordResponseBodyCallbackIdentifier identifier, WebCore::ResourceError&& error)
{
    if (auto callback = m_retrieveRecordResponseBodyCallbacks.take(identifier))
        callback(makeUnexpected(WTF::move(error)));
}

static RefPtr<Page> pageFromScriptExecutionContextIdentifier(ScriptExecutionContextIdentifier clientIdentifier)
{
    RefPtr document = Document::allDocumentsMap().get(clientIdentifier);
    if (!document) {
        RefPtr loader = DocumentLoader::fromScriptExecutionContextIdentifier(clientIdentifier);
        RefPtr frame = loader ? loader->frame() : nullptr;
        return frame ? frame->page() : nullptr;
    }
    return document->page();
}

void WebSWClientConnection::focusServiceWorkerClient(ScriptExecutionContextIdentifier clientIdentifier, CompletionHandler<void(std::optional<ServiceWorkerClientData>&&)>&& callback)
{
    RefPtr page = pageFromScriptExecutionContextIdentifier(clientIdentifier);
    if (!page) {
        callback({ });
        return;
    }

    WebPage::fromCorePage(*page)->sendWithAsyncReply(Messages::WebPageProxy::FocusFromServiceWorker { }, [clientIdentifier, callback = WTF::move(callback)] () mutable {
        auto doFocusSteps = [callback = WTF::move(callback)] (auto* document) mutable {
            if (!document) {
                callback({ });
                return;
            }

            document->eventLoop().queueTask(TaskSource::Networking, [document = RefPtr { document }, callback = WTF::move(callback)] () mutable {
                RefPtr frame = document ? document->frame() : nullptr;
                RefPtr page = frame ? frame->page() : nullptr;

                if (!page) {
                    callback({ });
                    return;
                }

                page->focusController().setFocusedFrame(frame.get());
                // FIXME: This is a safer cpp false positive.
                SUPPRESS_UNCOUNTED_ARG callback(ServiceWorkerClientData::from(*document));
            });
        };

        RefPtr document = Document::allDocumentsMap().get(clientIdentifier);
        if (!document) {
            RefPtr loader = DocumentLoader::fromScriptExecutionContextIdentifier(clientIdentifier);
            if (!loader) {
                callback({ });
                return;
            };
            loader->whenDocumentIsCreated(WTF::move(doFocusSteps));
            return;
        }

        doFocusSteps(document.get());
    });
}

void WebSWClientConnection::addCookieChangeSubscriptions(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, Vector<WebCore::CookieChangeSubscription>&& subscriptions, ExceptionOrVoidCallback&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::AddCookieChangeSubscriptions { registrationIdentifier, subscriptions }, [callback = WTF::move(callback)](auto&& error) mutable {
        if (error)
            return callback(error->toException());
        callback({ });
    });
}

void WebSWClientConnection::removeCookieChangeSubscriptions(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, Vector<WebCore::CookieChangeSubscription>&& subscriptions, ExceptionOrVoidCallback&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::RemoveCookieChangeSubscriptions { registrationIdentifier, subscriptions }, [callback = WTF::move(callback)](auto&& error) mutable {
        if (error)
            return callback(error->toException());
        callback({ });
    });
}

void WebSWClientConnection::cookieChangeSubscriptions(WebCore::ServiceWorkerRegistrationIdentifier registrationIdentifier, ExceptionOrCookieChangeSubscriptionsCallback&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::CookieChangeSubscriptions { registrationIdentifier }, [callback = WTF::move(callback)](auto&& result) mutable {
        callExceptionOrResultCallback(WTF::move(callback), WTF::move(result));
    });
}

Ref<WebSWClientConnection::AddRoutePromise> WebSWClientConnection::addRoutes(ServiceWorkerRegistrationIdentifier identifier, Vector<ServiceWorkerRoute>&& routes)
{
    struct PromiseConverter {
        static auto convertError(IPC::Error)
        {
            return makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::TypeError, "Internal error"_s });
        }
    };
    return WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithPromisedReply<PromiseConverter>(Messages::WebSWServerConnection::AddRoutes { identifier, routes });
}

} // namespace WebKit
