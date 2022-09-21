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
#include "WebServiceWorkerProvider.h"

#if ENABLE(SERVICE_WORKER)

#include "NetworkProcessConnection.h"
#include "WebProcess.h"
#include "WebSWClientConnection.h"
#include "WebSWServerConnection.h"
#include <WebCore/CachedResource.h>
#include <WebCore/Exception.h>
#include <WebCore/ExceptionCode.h>
#include <WebCore/LegacySchemeRegistry.h>
#include <WebCore/ServiceWorkerJob.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
using namespace PAL;
using namespace WebCore;

WebServiceWorkerProvider& WebServiceWorkerProvider::singleton()
{
    static NeverDestroyed<WebServiceWorkerProvider> provider;
    return provider;
}

WebServiceWorkerProvider::WebServiceWorkerProvider()
{
}

WebCore::SWClientConnection& WebServiceWorkerProvider::serviceWorkerConnection()
{
    return WebProcess::singleton().ensureNetworkProcessConnection().serviceWorkerConnection();
}

WebCore::SWClientConnection* WebServiceWorkerProvider::existingServiceWorkerConnection()
{
    auto* networkProcessConnection = WebProcess::singleton().existingNetworkProcessConnection();
    if (!networkProcessConnection)
        return nullptr;

    return &networkProcessConnection->serviceWorkerConnection();
}

void WebServiceWorkerProvider::updateThrottleState(bool isThrottleable)
{
    auto* networkProcessConnection = WebProcess::singleton().existingNetworkProcessConnection();
    if (!networkProcessConnection)
        return;
    auto& connection = networkProcessConnection->serviceWorkerConnection();
    if (isThrottleable != connection.isThrottleable())
        connection.updateThrottleState();
}

void WebServiceWorkerProvider::terminateWorkerForTesting(WebCore::ServiceWorkerIdentifier identifier, CompletionHandler<void()>&& callback)
{
    WebProcess::singleton().ensureNetworkProcessConnection().serviceWorkerConnection().terminateWorkerForTesting(identifier, WTFMove(callback));
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
