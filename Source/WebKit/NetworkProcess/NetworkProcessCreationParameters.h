/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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

#include "CacheModel.h"
#include "NetworkSessionCreationParameters.h"
#include "SandboxExtension.h"
#include <WebCore/Cookie.h>
#include <wtf/ProcessID.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if USE(SOUP)
#include "HTTPCookieAcceptPolicy.h"
#include <WebCore/SoupNetworkProxySettings.h>
#endif

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

struct NetworkProcessCreationParameters {
    NetworkProcessCreationParameters();

    void encode(IPC::Encoder&) const;
    static bool decode(IPC::Decoder&, NetworkProcessCreationParameters&);

    bool privateBrowsingEnabled { false };
    CacheModel cacheModel { CacheModelDocumentViewer };
    int64_t diskCacheSizeOverride { -1 };
    bool canHandleHTTPSServerTrustEvaluation { true };

    String diskCacheDirectory;
    SandboxExtension::Handle diskCacheDirectoryExtensionHandle;
    bool shouldEnableNetworkCacheEfficacyLogging { false };
#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
    bool shouldEnableNetworkCacheSpeculativeRevalidation { false };
#endif
#if PLATFORM(MAC)
    Vector<uint8_t> uiProcessCookieStorageIdentifier;
#endif
    Vector<WebCore::Cookie> defaultSessionPendingCookies;
#if PLATFORM(IOS_FAMILY)
    SandboxExtension::Handle cookieStorageDirectoryExtensionHandle;
    SandboxExtension::Handle containerCachesDirectoryExtensionHandle;
    SandboxExtension::Handle parentBundleDirectoryExtensionHandle;
#if ENABLE(INDEXED_DATABASE)
    SandboxExtension::Handle indexedDatabaseTempBlobDirectoryExtensionHandle;
#endif
#endif
    bool shouldSuppressMemoryPressureHandler { false };
    bool shouldUseTestingNetworkSession { false };
    Seconds loadThrottleLatency;

    Vector<String> urlSchemesRegisteredForCustomProtocols;
    ProcessID presentingApplicationPID { 0 };

#if PLATFORM(COCOA)
    String uiProcessBundleIdentifier;
    uint32_t uiProcessSDKVersion { 0 };
    String sourceApplicationBundleIdentifier;
    String sourceApplicationSecondaryIdentifier;
#if PLATFORM(IOS_FAMILY)
    String ctDataConnectionServiceType;
#endif
    String httpProxy;
    String httpsProxy;
    RetainPtr<CFDataRef> networkATSContext;
    bool storageAccessAPIEnabled;
    bool suppressesConnectionTerminationOnSystemChange;
#endif

#if USE(SOUP)
    String cookiePersistentStoragePath;
    uint32_t cookiePersistentStorageType { 0 };
    HTTPCookieAcceptPolicy cookieAcceptPolicy { HTTPCookieAcceptPolicyAlways };
    bool ignoreTLSErrors { false };
    Vector<String> languages;
    WebCore::SoupNetworkProxySettings proxySettings;
#elif USE(CURL)
    String cookiePersistentStorageFile;
#endif

#if ENABLE(RESOURCE_LOAD_STATISTICS) && !RELEASE_LOG_DISABLED
    bool logCookieInformation { false };
#endif

#if ENABLE(NETWORK_CAPTURE)
    String recordReplayMode;
    String recordReplayCacheLocation;
#endif

    Vector<String> urlSchemesRegisteredAsSecure;
    Vector<String> urlSchemesRegisteredAsBypassingContentSecurityPolicy;
    Vector<String> urlSchemesRegisteredAsLocal;
    Vector<String> urlSchemesRegisteredAsNoAccess;
    Vector<String> urlSchemesRegisteredAsDisplayIsolated;
    Vector<String> urlSchemesRegisteredAsCanDisplayOnlyIfCanRequest;
    Vector<String> urlSchemesRegisteredAsCORSEnabled;

#if ENABLE(PROXIMITY_NETWORKING)
    unsigned wirelessContextIdentifier { 0 };
#endif

#if ENABLE(INDEXED_DATABASE)
    String indexedDatabaseDirectory;
    SandboxExtension::Handle indexedDatabaseDirectoryExtensionHandle;
#endif

#if ENABLE(SERVICE_WORKER)
    String serviceWorkerRegistrationDirectory;
    SandboxExtension::Handle serviceWorkerRegistrationDirectoryExtensionHandle;
    Vector<String> urlSchemesServiceWorkersCanHandle;
    bool shouldDisableServiceWorkerProcessTerminationDelay { false };
#endif
};

} // namespace WebKit
