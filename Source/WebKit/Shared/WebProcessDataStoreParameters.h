/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

#include "SandboxExtension.h"
#include <WebCore/NetworkStorageSession.h>
#include <pal/SessionID.h>
#include <wtf/HashMap.h>

namespace WebKit {

struct WebProcessDataStoreParameters {
    using TopFrameDomain = WebCore::RegistrableDomain;
    using SubResourceDomain = WebCore::RegistrableDomain;

    PAL::SessionID sessionID;
    String applicationCacheDirectory;
    SandboxExtension::Handle applicationCacheDirectoryExtensionHandle;
    String applicationCacheFlatFileSubdirectoryName;
    String mediaCacheDirectory;
    SandboxExtension::Handle mediaCacheDirectoryExtensionHandle;
    String mediaKeyStorageDirectory;
    SandboxExtension::Handle mediaKeyStorageDirectoryExtensionHandle;
    String javaScriptConfigurationDirectory;
    SandboxExtension::Handle javaScriptConfigurationDirectoryExtensionHandle;
#if ENABLE(TRACKING_PREVENTION)
    WebCore::ThirdPartyCookieBlockingMode thirdPartyCookieBlockingMode { WebCore::ThirdPartyCookieBlockingMode::All };
    HashSet<WebCore::RegistrableDomain> domainsWithUserInteraction;
    HashMap<TopFrameDomain, SubResourceDomain> domainsWithStorageAccessQuirk;
#endif
#if ENABLE(ARKIT_INLINE_PREVIEW)
    String modelElementCacheDirectory;
    SandboxExtension::Handle modelElementCacheDirectoryExtensionHandle;
#endif
#if PLATFORM(IOS_FAMILY)
    std::optional<SandboxExtension::Handle> cookieStorageDirectoryExtensionHandle;
    std::optional<SandboxExtension::Handle> containerCachesDirectoryExtensionHandle;
    std::optional<SandboxExtension::Handle> containerTemporaryDirectoryExtensionHandle;
#endif
    bool resourceLoadStatisticsEnabled { false };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<WebProcessDataStoreParameters> decode(Decoder&);
};

template<class Encoder>
void WebProcessDataStoreParameters::encode(Encoder& encoder) const
{
    encoder << sessionID;
    encoder << applicationCacheDirectory;
    encoder << applicationCacheDirectoryExtensionHandle;
    encoder << applicationCacheFlatFileSubdirectoryName;
    encoder << mediaCacheDirectory;
    encoder << mediaCacheDirectoryExtensionHandle;
    encoder << mediaKeyStorageDirectory;
    encoder << mediaKeyStorageDirectoryExtensionHandle;
    encoder << javaScriptConfigurationDirectory;
    encoder << javaScriptConfigurationDirectoryExtensionHandle;
#if ENABLE(TRACKING_PREVENTION)
    encoder << thirdPartyCookieBlockingMode;
    encoder << domainsWithUserInteraction;
    encoder << domainsWithStorageAccessQuirk;
#endif
#if ENABLE(ARKIT_INLINE_PREVIEW)
    encoder << modelElementCacheDirectory;
    encoder << modelElementCacheDirectoryExtensionHandle;
#endif
#if PLATFORM(IOS_FAMILY)
    encoder << cookieStorageDirectoryExtensionHandle;
    encoder << containerCachesDirectoryExtensionHandle;
    encoder << containerTemporaryDirectoryExtensionHandle;
#endif
    encoder << resourceLoadStatisticsEnabled;
}

template<class Decoder>
std::optional<WebProcessDataStoreParameters> WebProcessDataStoreParameters::decode(Decoder& decoder)
{
    std::optional<PAL::SessionID> sessionID;
    decoder >> sessionID;
    if (!sessionID)
        return std::nullopt;

    String applicationCacheDirectory;
    if (!decoder.decode(applicationCacheDirectory))
        return std::nullopt;

    std::optional<SandboxExtension::Handle> applicationCacheDirectoryExtensionHandle;
    decoder >> applicationCacheDirectoryExtensionHandle;
    if (!applicationCacheDirectoryExtensionHandle)
        return std::nullopt;

    String applicationCacheFlatFileSubdirectoryName;
    if (!decoder.decode(applicationCacheFlatFileSubdirectoryName))
        return std::nullopt;

    String mediaCacheDirectory;
    if (!decoder.decode(mediaCacheDirectory))
        return std::nullopt;

    std::optional<SandboxExtension::Handle> mediaCacheDirectoryExtensionHandle;
    decoder >> mediaCacheDirectoryExtensionHandle;
    if (!mediaCacheDirectoryExtensionHandle)
        return std::nullopt;

    String mediaKeyStorageDirectory;
    if (!decoder.decode(mediaKeyStorageDirectory))
        return std::nullopt;

    std::optional<SandboxExtension::Handle> mediaKeyStorageDirectoryExtensionHandle;
    decoder >> mediaKeyStorageDirectoryExtensionHandle;
    if (!mediaKeyStorageDirectoryExtensionHandle)
        return std::nullopt;

    String javaScriptConfigurationDirectory;
    if (!decoder.decode(javaScriptConfigurationDirectory))
        return std::nullopt;

    std::optional<SandboxExtension::Handle> javaScriptConfigurationDirectoryExtensionHandle;
    decoder >> javaScriptConfigurationDirectoryExtensionHandle;
    if (!javaScriptConfigurationDirectoryExtensionHandle)
        return std::nullopt;
        
#if ENABLE(TRACKING_PREVENTION)
    std::optional<WebCore::ThirdPartyCookieBlockingMode> thirdPartyCookieBlockingMode;
    decoder >> thirdPartyCookieBlockingMode;
    if (!thirdPartyCookieBlockingMode)
        return std::nullopt;

    std::optional<HashSet<WebCore::RegistrableDomain>> domainsWithUserInteraction;
    decoder >> domainsWithUserInteraction;
    if (!domainsWithUserInteraction)
        return std::nullopt;
    
    std::optional<HashMap<TopFrameDomain, SubResourceDomain>> domainsWithStorageAccessQuirk;
    decoder >> domainsWithStorageAccessQuirk;
    if (!domainsWithStorageAccessQuirk)
        return std::nullopt;
#endif
#if ENABLE(ARKIT_INLINE_PREVIEW)
    String modelElementCacheDirectory;
    if (!decoder.decode(modelElementCacheDirectory))
        return std::nullopt;

    std::optional<SandboxExtension::Handle> modelElementCacheDirectoryExtensionHandle;
    decoder >> modelElementCacheDirectoryExtensionHandle;
    if (!modelElementCacheDirectoryExtensionHandle)
        return std::nullopt;
#endif
#if PLATFORM(IOS_FAMILY)
    std::optional<std::optional<SandboxExtension::Handle>> cookieStorageDirectoryExtensionHandle;
    decoder >> cookieStorageDirectoryExtensionHandle;
    if (!cookieStorageDirectoryExtensionHandle)
        return std::nullopt;

    std::optional<std::optional<SandboxExtension::Handle>> containerCachesDirectoryExtensionHandle;
    decoder >> containerCachesDirectoryExtensionHandle;
    if (!containerCachesDirectoryExtensionHandle)
        return std::nullopt;

    std::optional<std::optional<SandboxExtension::Handle>> containerTemporaryDirectoryExtensionHandle;
    decoder >> containerTemporaryDirectoryExtensionHandle;
    if (!containerTemporaryDirectoryExtensionHandle)
        return std::nullopt;
#endif

    bool resourceLoadStatisticsEnabled = false;
    if (!decoder.decode(resourceLoadStatisticsEnabled))
        return std::nullopt;

    return WebProcessDataStoreParameters {
        WTFMove(*sessionID),
        WTFMove(applicationCacheDirectory),
        WTFMove(*applicationCacheDirectoryExtensionHandle),
        WTFMove(applicationCacheFlatFileSubdirectoryName),
        WTFMove(mediaCacheDirectory),
        WTFMove(*mediaCacheDirectoryExtensionHandle),
        WTFMove(mediaKeyStorageDirectory),
        WTFMove(*mediaKeyStorageDirectoryExtensionHandle),
        WTFMove(javaScriptConfigurationDirectory),
        WTFMove(*javaScriptConfigurationDirectoryExtensionHandle),
#if ENABLE(TRACKING_PREVENTION)
        *thirdPartyCookieBlockingMode,
        WTFMove(*domainsWithUserInteraction),
        WTFMove(*domainsWithStorageAccessQuirk),
#endif
#if ENABLE(ARKIT_INLINE_PREVIEW)
        WTFMove(modelElementCacheDirectory),
        WTFMove(*modelElementCacheDirectoryExtensionHandle),
#endif
#if PLATFORM(IOS_FAMILY)
        WTFMove(*cookieStorageDirectoryExtensionHandle),
        WTFMove(*containerCachesDirectoryExtensionHandle),
        WTFMove(*containerTemporaryDirectoryExtensionHandle),
#endif
        resourceLoadStatisticsEnabled
    };
}

} // namespace WebKit
