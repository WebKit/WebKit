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

#if ENABLE(SERVICE_WORKER)
#include "ServiceWorkerClients.h"

#include "JSDOMPromiseDeferred.h"
#include "JSServiceWorkerWindowClient.h"
#include "SWContextManager.h"
#include "ServiceWorker.h"
#include "ServiceWorkerGlobalScope.h"
#include "ServiceWorkerThread.h"

namespace WebCore {

static inline void didFinishGetRequest(ServiceWorkerGlobalScope& scope, DeferredPromise& promise, ExceptionOr<Optional<ServiceWorkerClientData>>&& clientData)
{
    if (clientData.hasException()) {
        promise.reject(clientData.releaseException());
        return;
    }
    auto data = clientData.releaseReturnValue();
    if (!data) {
        promise.resolve();
        return;
    }

    promise.resolve<IDLInterface<ServiceWorkerClient>>(ServiceWorkerClient::getOrCreate(scope, WTFMove(data.value())));
}

void ServiceWorkerClients::get(ScriptExecutionContext& context, const String& id, Ref<DeferredPromise>&& promise)
{
    auto identifier = ServiceWorkerClientIdentifier::fromString(id);
    if (!identifier) {
        promise->resolve();
        return;
    }
    auto clientIdentifier = identifier.value();

    auto serviceWorkerIdentifier = downcast<ServiceWorkerGlobalScope>(context).thread().identifier();

    auto promisePointer = promise.ptr();
    m_pendingPromises.add(promisePointer, WTFMove(promise));

    callOnMainThread([promisePointer, serviceWorkerIdentifier, clientIdentifier] () {
        auto connection = SWContextManager::singleton().connection();
        connection->findClientByIdentifier(serviceWorkerIdentifier, clientIdentifier, [promisePointer, serviceWorkerIdentifier] (auto&& clientData) {
            SWContextManager::singleton().postTaskToServiceWorker(serviceWorkerIdentifier, [promisePointer, data = crossThreadCopy(clientData)] (auto& context) mutable {
                if (auto promise = context.clients().m_pendingPromises.take(promisePointer))
                    didFinishGetRequest(context, *promise, WTFMove(data));
            });
        });
    });
}


static inline void matchAllCompleted(ServiceWorkerGlobalScope& scope, DeferredPromise& promise, Vector<ServiceWorkerClientData>&& clientsData)
{
    auto clients = WTF::map(clientsData, [&] (auto&& clientData) {
        return ServiceWorkerClient::getOrCreate(scope, WTFMove(clientData));
    });
    promise.resolve<IDLSequence<IDLInterface<ServiceWorkerClient>>>(WTFMove(clients));
}

void ServiceWorkerClients::matchAll(ScriptExecutionContext& context, const ClientQueryOptions& options, Ref<DeferredPromise>&& promise)
{
    auto promisePointer = promise.ptr();
    m_pendingPromises.add(promisePointer, WTFMove(promise));

    auto serviceWorkerIdentifier = downcast<ServiceWorkerGlobalScope>(context).thread().identifier();

    callOnMainThread([promisePointer, serviceWorkerIdentifier, options] () mutable {
        auto connection = SWContextManager::singleton().connection();
        connection->matchAll(serviceWorkerIdentifier, options, [promisePointer, serviceWorkerIdentifier] (auto&& clientsData) mutable {
            SWContextManager::singleton().postTaskToServiceWorker(serviceWorkerIdentifier, [promisePointer, clientsData = crossThreadCopy(clientsData)] (auto& scope) mutable {
                if (auto promise = scope.clients().m_pendingPromises.take(promisePointer))
                    matchAllCompleted(scope, *promise, WTFMove(clientsData));
            });
        });
    });
}

void ServiceWorkerClients::openWindow(ScriptExecutionContext&, const String& url, Ref<DeferredPromise>&& promise)
{
    UNUSED_PARAM(url);
    promise->reject(Exception { NotSupportedError, "clients.openWindow() is not yet supported"_s });
}

void ServiceWorkerClients::claim(ScriptExecutionContext& context, Ref<DeferredPromise>&& promise)
{
    auto& serviceWorkerGlobalScope = downcast<ServiceWorkerGlobalScope>(context);

    auto serviceWorkerIdentifier = serviceWorkerGlobalScope.thread().identifier();

    auto promisePointer = promise.ptr();
    m_pendingPromises.add(promisePointer, WTFMove(promise));

    callOnMainThread([promisePointer, serviceWorkerIdentifier] () mutable {
        auto connection = SWContextManager::singleton().connection();
        connection->claim(serviceWorkerIdentifier, [promisePointer, serviceWorkerIdentifier](auto&& result) mutable {
            SWContextManager::singleton().postTaskToServiceWorker(serviceWorkerIdentifier, [promisePointer, result = isolatedCopy(WTFMove(result))](auto& scope) mutable {
                if (auto promise = scope.clients().m_pendingPromises.take(promisePointer)) {
                    DOMPromiseDeferred<void> pendingPromise { WTFMove(promise.value()) };
                    pendingPromise.settle(WTFMove(result));
                }
            });
        });
    });
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
