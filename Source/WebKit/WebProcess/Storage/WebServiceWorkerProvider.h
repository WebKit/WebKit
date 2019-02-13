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

#pragma once

#if ENABLE(SERVICE_WORKER)

#include "ServiceWorkerClientFetch.h"
#include <WebCore/ServiceWorkerProvider.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {
class CachedResource;
}

namespace WebKit {

class WebServiceWorkerProvider final : public WebCore::ServiceWorkerProvider {
public:
    static WebServiceWorkerProvider& singleton();

    void handleFetch(WebCore::ResourceLoader&, PAL::SessionID, bool shouldClearReferrerOnHTTPSToHTTPRedirect, ServiceWorkerClientFetch::Callback&&);
    bool cancelFetch(WebCore::FetchIdentifier);
    void fetchFinished(WebCore::FetchIdentifier);

    void didReceiveServiceWorkerClientFetchMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveServiceWorkerClientRegistrationMatch(IPC::Connection&, IPC::Decoder&);

private:
    friend NeverDestroyed<WebServiceWorkerProvider>;
    WebServiceWorkerProvider();

    WebCore::SWClientConnection* existingServiceWorkerConnectionForSession(PAL::SessionID) final;
    WebCore::SWClientConnection& serviceWorkerConnectionForSession(PAL::SessionID) final;

    HashMap<WebCore::FetchIdentifier, Ref<ServiceWorkerClientFetch>> m_ongoingFetchTasks;
}; // class WebServiceWorkerProvider

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
