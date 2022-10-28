/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "ArgumentCoders.h"
#include "SandboxExtension.h"
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/RegistrableDomain.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

struct ResourceLoadStatisticsParameters {

    String directory;
    SandboxExtension::Handle directoryExtensionHandle;
    bool enabled { false };
    bool isTrackingPreventionStateExplicitlySet { false };
    bool enableLogTestingEvent { false };
    bool shouldIncludeLocalhost { true };
    bool enableDebugMode { false };
#if ENABLE(TRACKING_PREVENTION)
    WebCore::ThirdPartyCookieBlockingMode thirdPartyCookieBlockingMode { WebCore::ThirdPartyCookieBlockingMode::All };
    WebCore::SameSiteStrictEnforcementEnabled sameSiteStrictEnforcementEnabled { WebCore::SameSiteStrictEnforcementEnabled::No };
#endif
    WebCore::FirstPartyWebsiteDataRemovalMode firstPartyWebsiteDataRemovalMode { WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookies };
    WebCore::RegistrableDomain standaloneApplicationDomain;
    HashSet<WebCore::RegistrableDomain> appBoundDomains;
    HashSet<WebCore::RegistrableDomain> managedDomains;
    WebCore::RegistrableDomain manualPrevalentResource;
    
    void encode(IPC::Encoder& encoder) const
    {
        encoder << directory;
        encoder << directoryExtensionHandle;
        encoder << enabled;
        encoder << isTrackingPreventionStateExplicitlySet;
        encoder << enableLogTestingEvent;
        encoder << shouldIncludeLocalhost;
        encoder << enableDebugMode;
#if ENABLE(TRACKING_PREVENTION)
        encoder << thirdPartyCookieBlockingMode;
        encoder << sameSiteStrictEnforcementEnabled;
#endif
        encoder << firstPartyWebsiteDataRemovalMode;
        encoder << standaloneApplicationDomain;
        encoder << appBoundDomains;
        encoder << managedDomains;
        encoder << manualPrevalentResource;
    }

    static std::optional<ResourceLoadStatisticsParameters> decode(IPC::Decoder& decoder)
    {
        std::optional<String> directory;
        decoder >> directory;
        if (!directory)
            return std::nullopt;
        
        std::optional<SandboxExtension::Handle> directoryExtensionHandle;
        decoder >> directoryExtensionHandle;
        if (!directoryExtensionHandle)
            return std::nullopt;

        std::optional<bool> enabled;
        decoder >> enabled;
        if (!enabled)
            return std::nullopt;

        std::optional<bool> isTrackingPreventionStateExplicitlySet;
        decoder >> isTrackingPreventionStateExplicitlySet;
        if (!isTrackingPreventionStateExplicitlySet)
            return std::nullopt;

        std::optional<bool> enableLogTestingEvent;
        decoder >> enableLogTestingEvent;
        if (!enableLogTestingEvent)
            return std::nullopt;

        std::optional<bool> shouldIncludeLocalhost;
        decoder >> shouldIncludeLocalhost;
        if (!shouldIncludeLocalhost)
            return std::nullopt;

        std::optional<bool> enableDebugMode;
        decoder >> enableDebugMode;
        if (!enableDebugMode)
            return std::nullopt;

#if ENABLE(TRACKING_PREVENTION)
        std::optional<WebCore::ThirdPartyCookieBlockingMode> thirdPartyCookieBlockingMode;
        decoder >> thirdPartyCookieBlockingMode;
        if (!thirdPartyCookieBlockingMode)
            return std::nullopt;

        std::optional<WebCore::SameSiteStrictEnforcementEnabled> sameSiteStrictEnforcementEnabled;
        decoder >> sameSiteStrictEnforcementEnabled;
        if (!sameSiteStrictEnforcementEnabled)
            return std::nullopt;
#endif

        std::optional<WebCore::FirstPartyWebsiteDataRemovalMode> firstPartyWebsiteDataRemovalMode;
        decoder >> firstPartyWebsiteDataRemovalMode;
        if (!firstPartyWebsiteDataRemovalMode)
            return std::nullopt;

        std::optional<WebCore::RegistrableDomain> standaloneApplicationDomain;
        decoder >> standaloneApplicationDomain;
        if (!standaloneApplicationDomain)
            return std::nullopt;

        std::optional<HashSet<WebCore::RegistrableDomain>> appBoundDomains;
        decoder >> appBoundDomains;
        if (!appBoundDomains)
            return std::nullopt;

        std::optional<HashSet<WebCore::RegistrableDomain>> managedDomains;
        decoder >> managedDomains;
        if (!managedDomains)
            return std::nullopt;

        std::optional<WebCore::RegistrableDomain> manualPrevalentResource;
        decoder >> manualPrevalentResource;
        if (!manualPrevalentResource)
            return std::nullopt;

        return {{
            WTFMove(*directory),
            WTFMove(*directoryExtensionHandle),
            WTFMove(*enabled),
            WTFMove(*isTrackingPreventionStateExplicitlySet),
            WTFMove(*enableLogTestingEvent),
            WTFMove(*shouldIncludeLocalhost),
            WTFMove(*enableDebugMode),
#if ENABLE(TRACKING_PREVENTION)
            WTFMove(*thirdPartyCookieBlockingMode),
            WTFMove(*sameSiteStrictEnforcementEnabled),
#endif
            WTFMove(*firstPartyWebsiteDataRemovalMode),
            WTFMove(*standaloneApplicationDomain),
            WTFMove(*appBoundDomains),
            WTFMove(*managedDomains),
            WTFMove(*manualPrevalentResource),
        }};
    }
};

} // namespace WebKit
