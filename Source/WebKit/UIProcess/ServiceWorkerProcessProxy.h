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

#include "WebProcessProxy.h"
#include <WebCore/RegistrableDomain.h>

namespace WebKit {
class AuthenticationChallengeProxy;
struct WebPreferencesStore;

class ServiceWorkerProcessProxy final : public WebProcessProxy {
public:
    static Ref<ServiceWorkerProcessProxy> create(WebProcessPool&, const WebCore::RegistrableDomain&, WebsiteDataStore&);
    ~ServiceWorkerProcessProxy();

    static bool hasRegisteredServiceWorkers(const String& serviceWorkerDirectory);

    void start(const WebPreferencesStore&, Optional<PAL::SessionID> initialSessionID);
    void setUserAgent(const String&);
    void updatePreferencesStore(const WebPreferencesStore&);

    const WebCore::RegistrableDomain& registrableDomain() { return m_registrableDomain; }
    WebCore::PageIdentifier pageID() const { return m_serviceWorkerPageID; }

private:
    // AuxiliaryProcessProxy
    void getLaunchOptions(ProcessLauncher::LaunchOptions&) final;

    bool isServiceWorkerProcess() const final { return true; }

    ServiceWorkerProcessProxy(WebProcessPool&, const WebCore::RegistrableDomain&, WebsiteDataStore&);

    WebCore::RegistrableDomain m_registrableDomain;
    WebCore::PageIdentifier m_serviceWorkerPageID;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::ServiceWorkerProcessProxy)
    static bool isType(const WebKit::WebProcessProxy& webProcessProxy) { return webProcessProxy.isServiceWorkerProcess(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(SERVICE_WORKER)
