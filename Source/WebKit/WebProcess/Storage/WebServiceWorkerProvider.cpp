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

#include "WebProcess.h"
#include "WebSWServerConnection.h"
#include "WebToStorageProcessConnection.h"
#include <WebCore/CachedResource.h>
#include <WebCore/Exception.h>
#include <WebCore/ExceptionCode.h>
#include <WebCore/ServiceWorkerJob.h>
#include <pal/SessionID.h>
#include <wtf/text/WTFString.h>

using namespace PAL;
using namespace WebCore;

namespace WebKit {

WebServiceWorkerProvider& WebServiceWorkerProvider::singleton()
{
    static NeverDestroyed<WebServiceWorkerProvider> provider;
    return provider;
}

WebServiceWorkerProvider::WebServiceWorkerProvider()
{
}

WebCore::SWClientConnection& WebServiceWorkerProvider::serviceWorkerConnectionForSession(SessionID sessionID)
{
    ASSERT(WebProcess::singleton().webToStorageProcessConnection());
    return WebProcess::singleton().webToStorageProcessConnection()->serviceWorkerConnectionForSession(sessionID);
}

static inline bool shouldHandleFetch(const WebSWClientConnection& connection, CachedResource* resource, const ResourceLoaderOptions& options)
{
    if (options.serviceWorkersMode == ServiceWorkersMode::None)
        return false;

    if (isPotentialNavigationOrSubresourceRequest(options.destination))
        return false;

    // FIXME: Implement non-subresource request loads.
    if (isNonSubresourceRequest(options.destination) || !options.serviceWorkerIdentifier)
        return false;

    if (!resource)
        return false;

    return connection.hasServiceWorkerRegisteredForOrigin(*resource->origin());
}

void WebServiceWorkerProvider::handleFetch(ResourceLoader& loader, CachedResource* resource, PAL::SessionID sessionID, ServiceWorkerClientFetch::Callback&& callback)
{
    auto& connection = WebProcess::singleton().webToStorageProcessConnection()->serviceWorkerConnectionForSession(sessionID);

    if (!shouldHandleFetch(connection, resource, loader.options())) {
        // FIXME: Add an option to error resource load for DTL to actually go through preflight.
        callback(ServiceWorkerClientFetch::Result::Unhandled);
        return;
    }

    auto fetch = connection.startFetch(*this, loader, loader.identifier(), WTFMove(callback));
    ASSERT(fetch->isOngoing());

    m_ongoingFetchTasks.add(loader.identifier(), WTFMove(fetch));
}

bool WebServiceWorkerProvider::cancelFetch(uint64_t fetchIdentifier)
{
    auto fetch = m_ongoingFetchTasks.take(fetchIdentifier);
    if (fetch)
        (*fetch)->cancel();
    return !!fetch;
}

void WebServiceWorkerProvider::fetchFinished(uint64_t fetchIdentifier)
{
    m_ongoingFetchTasks.take(fetchIdentifier);
}

void WebServiceWorkerProvider::didReceiveServiceWorkerClientFetchMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (auto fetch = m_ongoingFetchTasks.get(decoder.destinationID()))
        fetch->didReceiveMessage(connection, decoder);
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
