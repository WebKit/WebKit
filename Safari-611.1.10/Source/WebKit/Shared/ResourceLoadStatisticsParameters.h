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
    bool isItpStateExplicitlySet { false };
    bool enableLogTestingEvent { false };
    bool shouldIncludeLocalhost { true };
    bool enableDebugMode { false };
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebCore::ThirdPartyCookieBlockingMode thirdPartyCookieBlockingMode { WebCore::ThirdPartyCookieBlockingMode::All };
    WebCore::SameSiteStrictEnforcementEnabled sameSiteStrictEnforcementEnabled { WebCore::SameSiteStrictEnforcementEnabled::No };
#endif
    WebCore::FirstPartyWebsiteDataRemovalMode firstPartyWebsiteDataRemovalMode { WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookies };
    WebCore::RegistrableDomain standaloneApplicationDomain;
    HashSet<WebCore::RegistrableDomain> appBoundDomains;
    WebCore::RegistrableDomain manualPrevalentResource;
    
    void encode(IPC::Encoder& encoder) const
    {
        encoder << directory;
        encoder << directoryExtensionHandle;
        encoder << enabled;
        encoder << isItpStateExplicitlySet;
        encoder << enableLogTestingEvent;
        encoder << shouldIncludeLocalhost;
        encoder << enableDebugMode;
#if ENABLE(RESOURCE_LOAD_STATISTICS)
        encoder << thirdPartyCookieBlockingMode;
        encoder << sameSiteStrictEnforcementEnabled;
#endif
        encoder << firstPartyWebsiteDataRemovalMode;
        encoder << standaloneApplicationDomain;
        encoder << appBoundDomains;
        encoder << manualPrevalentResource;
    }

    static Optional<ResourceLoadStatisticsParameters> decode(IPC::Decoder& decoder)
    {
        Optional<String> directory;
        decoder >> directory;
        if (!directory)
            return WTF::nullopt;
        
        Optional<SandboxExtension::Handle> directoryExtensionHandle;
        decoder >> directoryExtensionHandle;
        if (!directoryExtensionHandle)
            return WTF::nullopt;
        
        Optional<bool> enabled;
        decoder >> enabled;
        if (!enabled)
            return WTF::nullopt;

        Optional<bool> isItpStateExplicitlySet;
        decoder >> isItpStateExplicitlySet;
        if (!isItpStateExplicitlySet)
            return WTF::nullopt;

        Optional<bool> enableLogTestingEvent;
        decoder >> enableLogTestingEvent;
        if (!enableLogTestingEvent)
            return WTF::nullopt;

        Optional<bool> shouldIncludeLocalhost;
        decoder >> shouldIncludeLocalhost;
        if (!shouldIncludeLocalhost)
            return WTF::nullopt;

        Optional<bool> enableDebugMode;
        decoder >> enableDebugMode;
        if (!enableDebugMode)
            return WTF::nullopt;

#if ENABLE(RESOURCE_LOAD_STATISTICS)
        Optional<WebCore::ThirdPartyCookieBlockingMode> thirdPartyCookieBlockingMode;
        decoder >> thirdPartyCookieBlockingMode;
        if (!thirdPartyCookieBlockingMode)
            return WTF::nullopt;

        Optional<WebCore::SameSiteStrictEnforcementEnabled> sameSiteStrictEnforcementEnabled;
        decoder >> sameSiteStrictEnforcementEnabled;
        if (!sameSiteStrictEnforcementEnabled)
            return WTF::nullopt;
#endif

        Optional<WebCore::FirstPartyWebsiteDataRemovalMode> firstPartyWebsiteDataRemovalMode;
        decoder >> firstPartyWebsiteDataRemovalMode;
        if (!firstPartyWebsiteDataRemovalMode)
            return WTF::nullopt;

        Optional<WebCore::RegistrableDomain> standaloneApplicationDomain;
        decoder >> standaloneApplicationDomain;
        if (!standaloneApplicationDomain)
            return WTF::nullopt;

        Optional<HashSet<WebCore::RegistrableDomain>> appBoundDomains;
        decoder >> appBoundDomains;
        if (!appBoundDomains)
            return WTF::nullopt;

        Optional<WebCore::RegistrableDomain> manualPrevalentResource;
        decoder >> manualPrevalentResource;
        if (!manualPrevalentResource)
            return WTF::nullopt;

        return {{
            WTFMove(*directory),
            WTFMove(*directoryExtensionHandle),
            WTFMove(*enabled),
            WTFMove(*isItpStateExplicitlySet),
            WTFMove(*enableLogTestingEvent),
            WTFMove(*shouldIncludeLocalhost),
            WTFMove(*enableDebugMode),
#if ENABLE(RESOURCE_LOAD_STATISTICS)
            WTFMove(*thirdPartyCookieBlockingMode),
            WTFMove(*sameSiteStrictEnforcementEnabled),
#endif
            WTFMove(*firstPartyWebsiteDataRemovalMode),
            WTFMove(*standaloneApplicationDomain),
            WTFMove(*appBoundDomains),
            WTFMove(*manualPrevalentResource),
        }};
    }
};

} // namespace WebKit
