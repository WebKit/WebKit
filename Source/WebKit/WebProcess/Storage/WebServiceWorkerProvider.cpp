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
#include <WebCore/SchemeRegistry.h>
#include <WebCore/ServiceWorkerJob.h>
#include <pal/SessionID.h>
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

WebCore::SWClientConnection& WebServiceWorkerProvider::serviceWorkerConnectionForSession(SessionID sessionID)
{
    ASSERT(sessionID.isValid());
    return WebProcess::singleton().ensureNetworkProcessConnection().serviceWorkerConnectionForSession(sessionID);
}

WebCore::SWClientConnection* WebServiceWorkerProvider::existingServiceWorkerConnectionForSession(SessionID sessionID)
{
    ASSERT(sessionID.isValid());
    auto* networkProcessConnection = WebProcess::singleton().existingNetworkProcessConnection();
    if (!networkProcessConnection)
        return nullptr;
    return networkProcessConnection->existingServiceWorkerConnectionForSession(sessionID);
}

static inline bool shouldHandleFetch(const ResourceLoaderOptions& options)
{
    if (options.serviceWorkersMode == ServiceWorkersMode::None)
        return false;

    if (isPotentialNavigationOrSubresourceRequest(options.destination))
        return false;

    return !!options.serviceWorkerRegistrationIdentifier;
}

void WebServiceWorkerProvider::handleFetch(ResourceLoader& loader, CachedResource* resource, PAL::SessionID sessionID, bool shouldClearReferrerOnHTTPSToHTTPRedirect, ServiceWorkerClientFetch::Callback&& callback)
{
    if (!SchemeRegistry::canServiceWorkersHandleURLScheme(loader.request().url().protocol().toStringWithoutCopying()) || !shouldHandleFetch(loader.options())) {
        callback(ServiceWorkerClientFetch::Result::Unhandled);
        return;
    }

    auto& connection = WebProcess::singleton().ensureNetworkProcessConnection().serviceWorkerConnectionForSession(sessionID);
    auto fetchIdentifier = makeObjectIdentifier<FetchIdentifierType>(loader.identifier());
    m_ongoingFetchTasks.add(fetchIdentifier, ServiceWorkerClientFetch::create(*this, loader, fetchIdentifier, connection, shouldClearReferrerOnHTTPSToHTTPRedirect, WTFMove(callback)));
}

bool WebServiceWorkerProvider::cancelFetch(FetchIdentifier fetchIdentifier)
{
    auto fetch = m_ongoingFetchTasks.take(fetchIdentifier);
    if (fetch)
        (*fetch)->cancel();
    return !!fetch;
}

void WebServiceWorkerProvider::fetchFinished(FetchIdentifier fetchIdentifier)
{
    m_ongoingFetchTasks.take(fetchIdentifier);
}

void WebServiceWorkerProvider::didReceiveServiceWorkerClientFetchMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (auto fetch = m_ongoingFetchTasks.get(makeObjectIdentifier<FetchIdentifierType>(decoder.destinationID())))
        fetch->didReceiveMessage(connection, decoder);
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
