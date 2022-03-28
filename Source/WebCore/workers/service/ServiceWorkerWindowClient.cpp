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
        promise->reject(Exception { InvalidAccessError, "WindowClient focus requires a user gesture"_s });
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
                    promise->reject(Exception { TypeError, "WindowClient focus failed"_s });
                    return;
                }

                promise->template resolve<IDLInterface<ServiceWorkerWindowClient>>(ServiceWorkerWindowClient::create(serviceWorkerContext, WTFMove(*result)));
            });
        });
    });
}

void ServiceWorkerWindowClient::navigate(ScriptExecutionContext&, const String& url, Ref<DeferredPromise>&& promise)
{
    UNUSED_PARAM(url);
    promise->reject(Exception { NotSupportedError, "windowClient.navigate() is not yet supported"_s });
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
