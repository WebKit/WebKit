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
#include "ServiceWorkerWindowClient.h"

#include "JSDOMPromiseDeferred.h"
#include "JSServiceWorkerWindowClient.h"
#include "SWContextManager.h"
#include "ServiceWorkerClients.h"
#include "ServiceWorkerGlobalScope.h"
#include "ServiceWorkerThread.h"

namespace WebCore {

ServiceWorkerWindowClient::ServiceWorkerWindowClient(ServiceWorkerGlobalScope& context, ServiceWorkerClientData&& data)
    : ServiceWorkerClient(context, WTFMove(data))
{
}

void ServiceWorkerWindowClient::focus(ScriptExecutionContext& context, Ref<DeferredPromise>&& promise)
{
    auto& serviceWorkerContext = downcast<ServiceWorkerGlobalScope>(context);

    if (context.settingsValues().serviceWorkersUserGestureEnabled && !serviceWorkerContext.isProcessingUserGesture()) {
        promise->reject(Exception { ExceptionCode::InvalidAccessError, "WindowClient focus requires a user gesture"_s });
        return;
    }

    auto promiseIdentifier = serviceWorkerContext.clients().addPendingPromise(WTFMove(promise));
    callOnMainThread([clientIdentifier = identifier(), promiseIdentifier, serviceWorkerIdentifier = serviceWorkerContext.thread().identifier()]() mutable {
        SWContextManager::singleton().connection()->focus(clientIdentifier, [promiseIdentifier, serviceWorkerIdentifier](auto result) mutable {
            SWContextManager::singleton().postTaskToServiceWorker(serviceWorkerIdentifier, [promiseIdentifier, result = crossThreadCopy(WTFMove(result))](auto& serviceWorkerContext) mutable {
                auto promise = serviceWorkerContext.clients().takePendingPromise(promiseIdentifier);
                if (!promise)
                    return;

                // FIXME: Check isFocused state and reject if not focused.
                if (!result) {
                    promise->reject(Exception { ExceptionCode::TypeError, "WindowClient focus failed"_s });
                    return;
                }

                promise->template resolve<IDLInterface<ServiceWorkerWindowClient>>(ServiceWorkerWindowClient::create(serviceWorkerContext, WTFMove(*result)));
            });
        });
    });
}

void ServiceWorkerWindowClient::navigate(ScriptExecutionContext& context, const String& urlString, Ref<DeferredPromise>&& promise)
{
    auto url = context.completeURL(urlString);

    if (!url.isValid()) {
        promise->reject(Exception { ExceptionCode::TypeError, makeString("URL string ", urlString, " cannot successfully be parsed") });
        return;
    }

    if (url.protocolIsAbout()) {
        promise->reject(Exception { ExceptionCode::TypeError, makeString("ServiceWorkerClients.navigate() cannot be called with URL ", url.string()) });
        return;
    }

    // We implement step 4 (checking of client's active service worker) in network process as we cannot do it synchronously.
    auto& serviceWorkerContext = downcast<ServiceWorkerGlobalScope>(context);
    auto promiseIdentifier = serviceWorkerContext.clients().addPendingPromise(WTFMove(promise));
    callOnMainThread([clientIdentifier = identifier(), promiseIdentifier, serviceWorkerIdentifier = serviceWorkerContext.thread().identifier(), url = WTFMove(url).isolatedCopy()]() mutable {
        SWContextManager::singleton().connection()->navigate(clientIdentifier, serviceWorkerIdentifier, url, [promiseIdentifier, serviceWorkerIdentifier](auto result) mutable {
            SWContextManager::singleton().postTaskToServiceWorker(serviceWorkerIdentifier, [promiseIdentifier, result = crossThreadCopy(WTFMove(result))](auto& serviceWorkerContext) mutable {
                auto promise = serviceWorkerContext.clients().takePendingPromise(promiseIdentifier);
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
                ASSERT(serviceWorkerContext.clientOrigin() == clientOrigin);
                promise->template resolve<IDLInterface<ServiceWorkerWindowClient>>(ServiceWorkerWindowClient::create(serviceWorkerContext, WTFMove(*clientData)));
            });
        });
    });
}

} // namespace WebCore
