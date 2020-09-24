/*
 * Copyright (C) 2018-2020 Apple Inc. All rights reserved.
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
#include "NetworkSessionCreationParameters.h"

#include "ArgumentCoders.h"

#if PLATFORM(COCOA)
#include "ArgumentCodersCF.h"
#endif

#if USE(CURL)
#include "WebCoreArgumentCoders.h"
#endif

namespace WebKit {

void NetworkSessionCreationParameters::encode(IPC::Encoder& encoder) const
{
    encoder << sessionID;
    encoder << boundInterfaceIdentifier;
    encoder << allowsCellularAccess;
#if PLATFORM(COCOA)
    IPC::encode(encoder, proxyConfiguration.get());
    encoder << sourceApplicationBundleIdentifier;
    encoder << sourceApplicationSecondaryIdentifier;
    encoder << shouldLogCookieInformation;
    encoder << loadThrottleLatency;
    encoder << httpProxy;
    encoder << httpsProxy;
#endif
#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    encoder << alternativeServiceDirectory;
    encoder << alternativeServiceDirectoryExtensionHandle;
    encoder << http3Enabled;
#endif
    encoder << hstsStorageDirectory;
    encoder << hstsStorageDirectoryExtensionHandle;
#if USE(SOUP)
    encoder << cookiePersistentStoragePath;
    encoder << cookiePersistentStorageType;
    encoder << persistentCredentialStorageEnabled;
    encoder << ignoreTLSErrors;
    encoder << proxySettings;
#endif
#if USE(CURL)
    encoder << cookiePersistentStorageFile;
    encoder << proxySettings;
#endif
    encoder << networkCacheDirectory << networkCacheDirectoryExtensionHandle;

    encoder << deviceManagementRestrictionsEnabled;
    encoder << allLoadsBlockedByDeviceManagementRestrictionsForTesting;
    encoder << dataConnectionServiceType;
    encoder << fastServerTrustEvaluationEnabled;
    encoder << networkCacheSpeculativeValidationEnabled;
    encoder << shouldUseTestingNetworkSession;
    encoder << staleWhileRevalidateEnabled;
    encoder << testSpeedMultiplier;
    encoder << suppressesConnectionTerminationOnSystemChange;
    encoder << allowsServerPreconnect;
    encoder << requiresSecureHTTPSProxyConnection;
    encoder << preventsSystemHTTPProxyAuthentication;
    encoder << appHasRequestedCrossWebsiteTrackingPermission;
    encoder << resourceLoadStatisticsParameters;
}

Optional<NetworkSessionCreationParameters> NetworkSessionCreationParameters::decode(IPC::Decoder& decoder)
{
    Optional<PAL::SessionID> sessionID;
    decoder >> sessionID;
    if (!sessionID)
        return WTF::nullopt;
    
    Optional<String> boundInterfaceIdentifier;
    decoder >> boundInterfaceIdentifier;
    if (!boundInterfaceIdentifier)
        return WTF::nullopt;
    
    Optional<AllowsCellularAccess> allowsCellularAccess;
    decoder >> allowsCellularAccess;
    if (!allowsCellularAccess)
        return WTF::nullopt;
    
#if PLATFORM(COCOA)
    RetainPtr<CFDictionaryRef> proxyConfiguration;
    if (!IPC::decode(decoder, proxyConfiguration))
        return WTF::nullopt;
    
    Optional<String> sourceApplicationBundleIdentifier;
    decoder >> sourceApplicationBundleIdentifier;
    if (!sourceApplicationBundleIdentifier)
        return WTF::nullopt;
    
    Optional<String> sourceApplicationSecondaryIdentifier;
    decoder >> sourceApplicationSecondaryIdentifier;
    if (!sourceApplicationSecondaryIdentifier)
        return WTF::nullopt;

    Optional<bool> shouldLogCookieInformation;
    decoder >> shouldLogCookieInformation;
    if (!shouldLogCookieInformation)
        return WTF::nullopt;
    
    Optional<Seconds> loadThrottleLatency;
    decoder >> loadThrottleLatency;
    if (!loadThrottleLatency)
        return WTF::nullopt;
    
    Optional<URL> httpProxy;
    decoder >> httpProxy;
    if (!httpProxy)
        return WTF::nullopt;

    Optional<URL> httpsProxy;
    decoder >> httpsProxy;
    if (!httpsProxy)
        return WTF::nullopt;
#endif

#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    Optional<String> alternativeServiceDirectory;
    decoder >> alternativeServiceDirectory;
    if (!alternativeServiceDirectory)
        return WTF::nullopt;

    Optional<SandboxExtension::Handle> alternativeServiceDirectoryExtensionHandle;
    decoder >> alternativeServiceDirectoryExtensionHandle;
    if (!alternativeServiceDirectoryExtensionHandle)
        return WTF::nullopt;
    
    Optional<bool> http3Enabled;
    decoder >> http3Enabled;
    if (!http3Enabled)
        return WTF::nullopt;
#endif

    Optional<String> hstsStorageDirectory;
    decoder >> hstsStorageDirectory;
    if (!hstsStorageDirectory)
        return WTF::nullopt;

    Optional<SandboxExtension::Handle> hstsStorageDirectoryExtensionHandle;
    decoder >> hstsStorageDirectoryExtensionHandle;
    if (!hstsStorageDirectoryExtensionHandle)
        return WTF::nullopt;
    
#if USE(SOUP)
    Optional<String> cookiePersistentStoragePath;
    decoder >> cookiePersistentStoragePath;
    if (!cookiePersistentStoragePath)
        return WTF::nullopt;

    Optional<SoupCookiePersistentStorageType> cookiePersistentStorageType;
    decoder >> cookiePersistentStorageType;
    if (!cookiePersistentStorageType)
        return WTF::nullopt;

    Optional<bool> persistentCredentialStorageEnabled;
    decoder >> persistentCredentialStorageEnabled;
    if (!persistentCredentialStorageEnabled)
        return WTF::nullopt;

    Optional<bool> ignoreTLSErrors;
    decoder >> ignoreTLSErrors;
    if (!ignoreTLSErrors)
        return WTF::nullopt;

    Optional<WebCore::SoupNetworkProxySettings> proxySettings;
    decoder >> proxySettings;
    if (!proxySettings)
        return WTF::nullopt;
#endif

#if USE(CURL)
    Optional<String> cookiePersistentStorageFile;
    decoder >> cookiePersistentStorageFile;
    if (!cookiePersistentStorageFile)
        return WTF::nullopt;

    Optional<WebCore::CurlProxySettings> proxySettings;
    decoder >> proxySettings;
    if (!proxySettings)
        return WTF::nullopt;
#endif

    Optional<String> networkCacheDirectory;
    decoder >> networkCacheDirectory;
    if (!networkCacheDirectory)
        return WTF::nullopt;
    
    Optional<SandboxExtension::Handle> networkCacheDirectoryExtensionHandle;
    decoder >> networkCacheDirectoryExtensionHandle;
    if (!networkCacheDirectoryExtensionHandle)
        return WTF::nullopt;

    Optional<bool> deviceManagementRestrictionsEnabled;
    decoder >> deviceManagementRestrictionsEnabled;
    if (!deviceManagementRestrictionsEnabled)
        return WTF::nullopt;

    Optional<bool> allLoadsBlockedByDeviceManagementRestrictionsForTesting;
    decoder >> allLoadsBlockedByDeviceManagementRestrictionsForTesting;
    if (!allLoadsBlockedByDeviceManagementRestrictionsForTesting)
        return WTF::nullopt;

    Optional<String> dataConnectionServiceType;
    decoder >> dataConnectionServiceType;
    if (!dataConnectionServiceType)
        return WTF::nullopt;
    
    Optional<bool> fastServerTrustEvaluationEnabled;
    decoder >> fastServerTrustEvaluationEnabled;
    if (!fastServerTrustEvaluationEnabled)
        return WTF::nullopt;
    
    Optional<bool> networkCacheSpeculativeValidationEnabled;
    decoder >> networkCacheSpeculativeValidationEnabled;
    if (!networkCacheSpeculativeValidationEnabled)
        return WTF::nullopt;
    
    Optional<bool> shouldUseTestingNetworkSession;
    decoder >> shouldUseTestingNetworkSession;
    if (!shouldUseTestingNetworkSession)
        return WTF::nullopt;

    Optional<bool> staleWhileRevalidateEnabled;
    decoder >> staleWhileRevalidateEnabled;
    if (!staleWhileRevalidateEnabled)
        return WTF::nullopt;

    Optional<unsigned> testSpeedMultiplier;
    decoder >> testSpeedMultiplier;
    if (!testSpeedMultiplier)
        return WTF::nullopt;
    
    Optional<bool> suppressesConnectionTerminationOnSystemChange;
    decoder >> suppressesConnectionTerminationOnSystemChange;
    if (!suppressesConnectionTerminationOnSystemChange)
        return WTF::nullopt;

    Optional<bool> allowsServerPreconnect;
    decoder >> allowsServerPreconnect;
    if (!allowsServerPreconnect)
        return WTF::nullopt;

    Optional<bool> requiresSecureHTTPSProxyConnection;
    decoder >> requiresSecureHTTPSProxyConnection;
    if (!requiresSecureHTTPSProxyConnection)
        return WTF::nullopt;
    
    Optional<bool> preventsSystemHTTPProxyAuthentication;
    decoder >> preventsSystemHTTPProxyAuthentication;
    if (!preventsSystemHTTPProxyAuthentication)
        return WTF::nullopt;
    
    Optional<bool> appHasRequestedCrossWebsiteTrackingPermission;
    decoder >> appHasRequestedCrossWebsiteTrackingPermission;
    if (!appHasRequestedCrossWebsiteTrackingPermission)
        return WTF::nullopt;

    Optional<ResourceLoadStatisticsParameters> resourceLoadStatisticsParameters;
    decoder >> resourceLoadStatisticsParameters;
    if (!resourceLoadStatisticsParameters)
        return WTF::nullopt;
    
    return {{
        *sessionID
        , WTFMove(*boundInterfaceIdentifier)
        , WTFMove(*allowsCellularAccess)
#if PLATFORM(COCOA)
        , WTFMove(proxyConfiguration)
        , WTFMove(*sourceApplicationBundleIdentifier)
        , WTFMove(*sourceApplicationSecondaryIdentifier)
        , WTFMove(*shouldLogCookieInformation)
        , WTFMove(*loadThrottleLatency)
        , WTFMove(*httpProxy)
        , WTFMove(*httpsProxy)
#endif
#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
        , WTFMove(*alternativeServiceDirectory)
        , WTFMove(*alternativeServiceDirectoryExtensionHandle)
        , WTFMove(*http3Enabled)
#endif
        , WTFMove(*hstsStorageDirectory)
        , WTFMove(*hstsStorageDirectoryExtensionHandle)
#if USE(SOUP)
        , WTFMove(*cookiePersistentStoragePath)
        , WTFMove(*cookiePersistentStorageType)
        , WTFMove(*persistentCredentialStorageEnabled)
        , WTFMove(*ignoreTLSErrors)
        , WTFMove(*proxySettings)
#endif
#if USE(CURL)
        , WTFMove(*cookiePersistentStorageFile)
        , WTFMove(*proxySettings)
#endif
        , WTFMove(*deviceManagementRestrictionsEnabled)
        , WTFMove(*allLoadsBlockedByDeviceManagementRestrictionsForTesting)
        , WTFMove(*networkCacheDirectory)
        , WTFMove(*networkCacheDirectoryExtensionHandle)
        , WTFMove(*dataConnectionServiceType)
        , WTFMove(*fastServerTrustEvaluationEnabled)
        , WTFMove(*networkCacheSpeculativeValidationEnabled)
        , WTFMove(*shouldUseTestingNetworkSession)
        , WTFMove(*staleWhileRevalidateEnabled)
        , WTFMove(*testSpeedMultiplier)
        , WTFMove(*suppressesConnectionTerminationOnSystemChange)
        , WTFMove(*allowsServerPreconnect)
        , WTFMove(*requiresSecureHTTPSProxyConnection)
        , WTFMove(*preventsSystemHTTPProxyAuthentication)
        , WTFMove(*appHasRequestedCrossWebsiteTrackingPermission)
        , WTFMove(*resourceLoadStatisticsParameters)
    }};
}

} // namespace WebKit
