/*
 * Copyright (C) 2011-2022 Apple Inc. All rights reserved.
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

#include "APIObject.h"
#include "Connection.h"
#include "MessageReceiver.h"
#include "WebContextSupplement.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/GeolocationPositionData.h>
#include <WebCore/RegistrableDomain.h>
#include <wtf/HashMap.h>
#include <wtf/WeakHashSet.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#include <WebCore/CoreLocationGeolocationProvider.h>
#endif

namespace API {
class GeolocationProvider;
}

namespace WebKit {

class WebGeolocationPosition;
class WebProcessPool;

class WebGeolocationManagerProxy : public API::ObjectImpl<API::Object::Type::GeolocationManager>, public WebContextSupplement, private IPC::MessageReceiver
#if PLATFORM(IOS_FAMILY)
    , public WebCore::CoreLocationGeolocationProvider::Client
#endif
{
public:
    static ASCIILiteral supplementName();

    static Ref<WebGeolocationManagerProxy> create(WebProcessPool*);
    ~WebGeolocationManagerProxy();

    void ref() const final { API::ObjectImpl<API::Object::Type::GeolocationManager>::ref(); }
    void deref() const final { API::ObjectImpl<API::Object::Type::GeolocationManager>::deref(); }

    void setProvider(std::unique_ptr<API::GeolocationProvider>&&);

    void providerDidChangePosition(WebGeolocationPosition*);
    void providerDidFailToDeterminePosition(const String& errorMessage = String());
#if PLATFORM(IOS_FAMILY)
    void resetPermissions();
#endif

    using API::Object::ref;
    using API::Object::deref;

    void webProcessIsGoingAway(WebProcessProxy&);
    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess(IPC::Connection&) const;

private:
    explicit WebGeolocationManagerProxy(WebProcessPool*);

    // WebContextSupplement
    void processPoolDestroyed() override;
    void refWebContextSupplement() override;
    void derefWebContextSupplement() override;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // IPC messages.
    void startUpdating(IPC::Connection&, const WebCore::RegistrableDomain&, WebPageProxyIdentifier, const String& authorizationToken, bool enableHighAccuracy);
    void stopUpdating(IPC::Connection&, const WebCore::RegistrableDomain&);
    void setEnableHighAccuracy(IPC::Connection&, const WebCore::RegistrableDomain&, bool);

    void startUpdatingWithProxy(WebProcessProxy&, const WebCore::RegistrableDomain&, WebPageProxyIdentifier, const String& authorizationToken, bool enableHighAccuracy);
    void stopUpdatingWithProxy(WebProcessProxy&, const WebCore::RegistrableDomain&);
    void setEnableHighAccuracyWithProxy(WebProcessProxy&, const WebCore::RegistrableDomain&, bool);

#if PLATFORM(IOS_FAMILY)
    // CoreLocationGeolocationProvider::Client
    void positionChanged(const String& websiteIdentifier, WebCore::GeolocationPositionData&&) final;
    void errorOccurred(const String& websiteIdentifier, const String& errorMessage) final;
    void resetGeolocation(const String& websiteIdentifier) final;
#endif

    struct PerDomainData {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        WeakHashSet<WebProcessProxy> watchers;
        WeakHashSet<WebProcessProxy> watchersNeedingHighAccuracy;
        std::optional<WebCore::GeolocationPositionData> lastPosition;

        // FIXME: Use for all Cocoa ports.
#if PLATFORM(IOS_FAMILY)
        std::unique_ptr<WebCore::CoreLocationGeolocationProvider> provider;
#endif
    };

    bool isUpdating(const PerDomainData&) const;
    bool isHighAccuracyEnabled(const PerDomainData&) const;
    void providerStartUpdating(PerDomainData&, const WebCore::RegistrableDomain&);
    void providerStopUpdating(PerDomainData&);
    void providerSetEnabledHighAccuracy(PerDomainData&, bool enabled);

    HashMap<WebCore::RegistrableDomain, std::unique_ptr<PerDomainData>> m_perDomainData;
    std::unique_ptr<API::GeolocationProvider> m_clientProvider;
};

} // namespace WebKit
