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
#include "ServiceWorkerClients.h"

#include "JSDOMPromiseDeferred.h"
#include "JSServiceWorkerWindowClient.h"
#include "Logging.h"
#include "SWContextManager.h"
#include "ServiceWorker.h"
#include "ServiceWorkerGlobalScope.h"
#include "ServiceWorkerThread.h"
#include "WebCoreOpaqueRoot.h"
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

static inline void didFinishGetRequest(ServiceWorkerGlobalScope& scope, DeferredPromise& promise, std::optional<ServiceWorkerClientData>&& clientData)
{
    if (!clientData) {
        promise.resolve();
        return;
    }

    promise.resolve<IDLInterface<ServiceWorkerClient>>(ServiceWorkerClient::create(scope, WTFMove(*clientData)));
}

void ServiceWorkerClients::get(ScriptExecutionContext& context, const String& id, Ref<DeferredPromise>&& promise)
{
    auto serviceWorkerIdentifier = downcast<ServiceWorkerGlobalScope>(context).thread().identifier();

    callOnMainThread([promiseIdentifier = addPendingPromise(WTFMove(promise)), serviceWorkerIdentifier, id = id.isolatedCopy()] () {
        auto connection = SWContextManager::singleton().connection();
        connection->findClientByVisibleIdentifier(serviceWorkerIdentifier, id, [promiseIdentifier, serviceWorkerIdentifier] (auto&& clientData) {
            SWContextManager::singleton().postTaskToServiceWorker(serviceWorkerIdentifier, [promiseIdentifier, data = crossThreadCopy(std::forward<decltype(clientData)>(clientData))] (auto& context) mutable {
                if (auto promise = context.clients().takePendingPromise(promiseIdentifier))
                    didFinishGetRequest(context, *promise, WTFMove(data));
            });
        });
    });
}


static inline void matchAllCompleted(ServiceWorkerGlobalScope& scope, DeferredPromise& promise, Vector<ServiceWorkerClientData>&& clientsData)
{
    auto clients = WTF::map(WTFMove(clientsData), [&] (ServiceWorkerClientData&& clientData) {
        return ServiceWorkerClient::create(scope, WTFMove(clientData));
    });
    std::sort(clients.begin(), clients.end(), [&] (auto& a, auto& b) {
        return a->data().focusOrder > b->data().focusOrder;
    });
    promise.resolve<IDLSequence<IDLInterface<ServiceWorkerClient>>>(WTFMove(clients));
}

void ServiceWorkerClients::matchAll(ScriptExecutionContext& context, const ClientQueryOptions& options, Ref<DeferredPromise>&& promise)
{
    auto serviceWorkerIdentifier = downcast<ServiceWorkerGlobalScope>(context).thread().identifier();

    callOnMainThread([promiseIdentifier = addPendingPromise(WTFMove(promise)), serviceWorkerIdentifier, options] () mutable {
        auto connection = SWContextManager::singleton().connection();
        connection->matchAll(serviceWorkerIdentifier, options, [promiseIdentifier, serviceWorkerIdentifier] (Vector<ServiceWorkerClientData>&& clientsData) mutable {
            SWContextManager::singleton().postTaskToServiceWorker(serviceWorkerIdentifier, [promiseIdentifier, clientsData = crossThreadCopy(WTFMove(clientsData))] (auto& scope) mutable {
                if (auto promise = scope.clients().takePendingPromise(promiseIdentifier))
                    matchAllCompleted(scope, *promise, WTFMove(clientsData));
            });
        });
    });
}

void ServiceWorkerClients::openWindow(ScriptExecutionContext& context, const String& urlString, Ref<DeferredPromise>&& promise)
{
    LOG(ServiceWorker, "WebProcess %i service worker calling openWindow to URL %s", getpid(), urlString.utf8().data());

    if (context.settingsValues().serviceWorkersUserGestureEnabled && !downcast<ServiceWorkerGlobalScope>(context).isProcessingUserGesture()) {
        promise->reject(Exception { ExceptionCode::InvalidAccessError, "ServiceWorkerClients.openWindow() requires a user gesture"_s });
        return;
    }

    auto url = context.completeURL(urlString);
    if (!url.isValid()) {
        promise->reject(Exception { ExceptionCode::TypeError, makeString("URL string ", urlString, " cannot successfully be parsed"_s) });
        return;
    }

    if (url.protocolIsAbout()) {
        promise->reject(Exception { ExceptionCode::TypeError, makeString("ServiceWorkerClients.openWindow() cannot be called with URL "_s, url.string()) });
        return;
    }

    auto serviceWorkerIdentifier = downcast<ServiceWorkerGlobalScope>(context).thread().identifier();
    callOnMainThread([promiseIdentifier = addPendingPromise(WTFMove(promise)), serviceWorkerIdentifier, url = url.isolatedCopy()] () mutable {
        auto connection = SWContextManager::singleton().connection();
        connection->openWindow(serviceWorkerIdentifier, url, [promiseIdentifier, serviceWorkerIdentifier] (auto&& result) mutable {
            SWContextManager::singleton().postTaskToServiceWorker(serviceWorkerIdentifier, [promiseIdentifier, result = crossThreadCopy(std::forward<decltype(result)>(result))] (ServiceWorkerGlobalScope& scope) mutable {
                LOG(ServiceWorker, "WebProcess %i finished ServiceWorkerClients::openWindow call result is %d.", getpid(), !result.hasException());

                auto promise = scope.clients().takePendingPromise(promiseIdentifier);
                if (!promise)
                    return;

                if (result.hasException()) {
                    promise->reject(result.releaseException());
                    return;
                }

                auto clientData = result.releaseReturnValue();
                if (!clientData) {
                    promise->resolveWithJSValue(JSC::jsNull());
                    return;
                }

#if ASSERT_ENABLED
                auto originData = SecurityOriginData::fromURL(clientData->url);
                ClientOrigin clientOrigin { originData, originData };
#endif
                ASSERT(scope.clientOrigin() == clientOrigin);
                promise->template resolve<IDLInterface<ServiceWorkerClient>>(ServiceWorkerClient::create(scope, WTFMove(*clientData)));
            });
        });
    });
}

void ServiceWorkerClients::claim(ScriptExecutionContext& context, Ref<DeferredPromise>&& promise)
{
    auto serviceWorkerIdentifier = downcast<ServiceWorkerGlobalScope>(context).thread().identifier();

    callOnMainThread([promiseIdentifier = addPendingPromise(WTFMove(promise)), serviceWorkerIdentifier] () mutable {
        auto connection = SWContextManager::singleton().connection();
        connection->claim(serviceWorkerIdentifier, [promiseIdentifier, serviceWorkerIdentifier](ExceptionOr<void>&& result) mutable {
            SWContextManager::singleton().postTaskToServiceWorker(serviceWorkerIdentifier, [promiseIdentifier, result = crossThreadCopy(WTFMove(result))](auto& scope) mutable {
                if (auto promise = scope.clients().takePendingPromise(promiseIdentifier)) {
                    DOMPromiseDeferred<void> pendingPromise { promise.releaseNonNull() };
                    pendingPromise.settle(WTFMove(result));
                }
            });
        });
    });
}

ServiceWorkerClients::PromiseIdentifier ServiceWorkerClients::addPendingPromise(Ref<DeferredPromise>&& promise)
{
    auto identifier = PromiseIdentifier::generate();
    m_pendingPromises.add(identifier, WTFMove(promise));
    return identifier;
}

RefPtr<DeferredPromise> ServiceWorkerClients::takePendingPromise(PromiseIdentifier identifier)
{
    return m_pendingPromises.take(identifier);
}

WebCoreOpaqueRoot root(ServiceWorkerClients* clients)
{
    return WebCoreOpaqueRoot { clients };
}

} // namespace WebCore
