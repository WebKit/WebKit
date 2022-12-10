/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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

#if USE(CURL) || USE(SOUP)
#include "WebCoreArgumentCoders.h"
#endif

namespace WebKit {

void NetworkSessionCreationParameters::encode(IPC::Encoder& encoder) const
{
    encoder << sessionID;
    encoder << dataStoreIdentifier;
    encoder << boundInterfaceIdentifier;
    encoder << allowsCellularAccess;
#if PLATFORM(COCOA)
    encoder << proxyConfiguration;
    encoder << sourceApplicationBundleIdentifier;
    encoder << sourceApplicationSecondaryIdentifier;
    encoder << shouldLogCookieInformation;
    encoder << httpProxy;
    encoder << httpsProxy;
#endif
#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    encoder << alternativeServiceDirectory;
    encoder << alternativeServiceDirectoryExtensionHandle;
#endif
    encoder << hstsStorageDirectory;
    encoder << hstsStorageDirectoryExtensionHandle;
#if USE(SOUP)
    encoder << cookiePersistentStoragePath;
    encoder << cookiePersistentStorageType;
    encoder << persistentCredentialStorageEnabled;
    encoder << ignoreTLSErrors;
    encoder << proxySettings;
    encoder << cookieAcceptPolicy;
#endif
#if USE(CURL)
    encoder << cookiePersistentStorageFile;
    encoder << proxySettings;
#endif
    encoder << networkCacheDirectory << networkCacheDirectoryExtensionHandle;

    encoder << deviceManagementRestrictionsEnabled;
    encoder << allLoadsBlockedByDeviceManagementRestrictionsForTesting;
    encoder << webPushDaemonConnectionConfiguration;
    encoder << dataConnectionServiceType;
    encoder << fastServerTrustEvaluationEnabled;
    encoder << networkCacheSpeculativeValidationEnabled;
    encoder << shouldUseTestingNetworkSession;
    encoder << staleWhileRevalidateEnabled;
    encoder << testSpeedMultiplier;
    encoder << suppressesConnectionTerminationOnSystemChange;
    encoder << allowsServerPreconnect;
    encoder << requiresSecureHTTPSProxyConnection;
    encoder << shouldRunServiceWorkersOnMainThreadForTesting;
    encoder << overrideServiceWorkerRegistrationCountTestingValue;
    encoder << preventsSystemHTTPProxyAuthentication;
    encoder << appHasRequestedCrossWebsiteTrackingPermission;
    encoder << useNetworkLoader;
    encoder << allowsHSTSWithUntrustedRootCertificate;
    encoder << pcmMachServiceName;
    encoder << webPushMachServiceName;
    encoder << webPushPartitionString;
    encoder << enablePrivateClickMeasurementDebugMode;
#if !HAVE(NSURLSESSION_WEBSOCKET)
    encoder << shouldAcceptInsecureCertificatesForWebSockets;
#endif

    encoder << shouldUseCustomStoragePaths;
    encoder << perOriginStorageQuota << perThirdPartyOriginStorageQuota;
    encoder << localStorageDirectory << localStorageDirectoryExtensionHandle;
    encoder << indexedDBDirectory << indexedDBDirectoryExtensionHandle;
    encoder << cacheStorageDirectory << cacheStorageDirectoryExtensionHandle;
    encoder << generalStorageDirectory << generalStorageDirectoryHandle;
#if ENABLE(SERVICE_WORKER)
    encoder << serviceWorkerRegistrationDirectory << serviceWorkerRegistrationDirectoryExtensionHandle << serviceWorkerProcessTerminationDelayEnabled;
#endif
    encoder << resourceLoadStatisticsParameters;
}

std::optional<NetworkSessionCreationParameters> NetworkSessionCreationParameters::decode(IPC::Decoder& decoder)
{
    std::optional<PAL::SessionID> sessionID;
    decoder >> sessionID;
    if (!sessionID)
        return std::nullopt;
    
    std::optional<std::optional<UUID>> dataStoreIdentifier;
    decoder >> dataStoreIdentifier;
    if (!dataStoreIdentifier)
        return std::nullopt;

    std::optional<String> boundInterfaceIdentifier;
    decoder >> boundInterfaceIdentifier;
    if (!boundInterfaceIdentifier)
        return std::nullopt;
    
    std::optional<AllowsCellularAccess> allowsCellularAccess;
    decoder >> allowsCellularAccess;
    if (!allowsCellularAccess)
        return std::nullopt;
    
#if PLATFORM(COCOA)
    RetainPtr<CFDictionaryRef> proxyConfiguration;
    if (!decoder.decode(proxyConfiguration))
        return std::nullopt;
    
    std::optional<String> sourceApplicationBundleIdentifier;
    decoder >> sourceApplicationBundleIdentifier;
    if (!sourceApplicationBundleIdentifier)
        return std::nullopt;
    
    std::optional<String> sourceApplicationSecondaryIdentifier;
    decoder >> sourceApplicationSecondaryIdentifier;
    if (!sourceApplicationSecondaryIdentifier)
        return std::nullopt;
    
    std::optional<bool> shouldLogCookieInformation;
    decoder >> shouldLogCookieInformation;
    if (!shouldLogCookieInformation)
        return std::nullopt;

    std::optional<URL> httpProxy;
    decoder >> httpProxy;
    if (!httpProxy)
        return std::nullopt;

    std::optional<URL> httpsProxy;
    decoder >> httpsProxy;
    if (!httpsProxy)
        return std::nullopt;
#endif

#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    std::optional<String> alternativeServiceDirectory;
    decoder >> alternativeServiceDirectory;
    if (!alternativeServiceDirectory)
        return std::nullopt;

    std::optional<SandboxExtension::Handle> alternativeServiceDirectoryExtensionHandle;
    decoder >> alternativeServiceDirectoryExtensionHandle;
    if (!alternativeServiceDirectoryExtensionHandle)
        return std::nullopt;
#endif

    std::optional<String> hstsStorageDirectory;
    decoder >> hstsStorageDirectory;
    if (!hstsStorageDirectory)
        return std::nullopt;

    std::optional<SandboxExtension::Handle> hstsStorageDirectoryExtensionHandle;
    decoder >> hstsStorageDirectoryExtensionHandle;
    if (!hstsStorageDirectoryExtensionHandle)
        return std::nullopt;
    
#if USE(SOUP)
    std::optional<String> cookiePersistentStoragePath;
    decoder >> cookiePersistentStoragePath;
    if (!cookiePersistentStoragePath)
        return std::nullopt;

    std::optional<SoupCookiePersistentStorageType> cookiePersistentStorageType;
    decoder >> cookiePersistentStorageType;
    if (!cookiePersistentStorageType)
        return std::nullopt;

    std::optional<bool> persistentCredentialStorageEnabled;
    decoder >> persistentCredentialStorageEnabled;
    if (!persistentCredentialStorageEnabled)
        return std::nullopt;

    std::optional<bool> ignoreTLSErrors;
    decoder >> ignoreTLSErrors;
    if (!ignoreTLSErrors)
        return std::nullopt;

    std::optional<WebCore::SoupNetworkProxySettings> proxySettings;
    decoder >> proxySettings;
    if (!proxySettings)
        return std::nullopt;

    std::optional<WebCore::HTTPCookieAcceptPolicy> cookieAcceptPolicy;
    decoder >> cookieAcceptPolicy;
    if (!cookieAcceptPolicy)
        return std::nullopt;
#endif

#if USE(CURL)
    std::optional<String> cookiePersistentStorageFile;
    decoder >> cookiePersistentStorageFile;
    if (!cookiePersistentStorageFile)
        return std::nullopt;

    std::optional<WebCore::CurlProxySettings> proxySettings;
    decoder >> proxySettings;
    if (!proxySettings)
        return std::nullopt;
#endif

    std::optional<String> networkCacheDirectory;
    decoder >> networkCacheDirectory;
    if (!networkCacheDirectory)
        return std::nullopt;
    
    std::optional<SandboxExtension::Handle> networkCacheDirectoryExtensionHandle;
    decoder >> networkCacheDirectoryExtensionHandle;
    if (!networkCacheDirectoryExtensionHandle)
        return std::nullopt;

    std::optional<bool> deviceManagementRestrictionsEnabled;
    decoder >> deviceManagementRestrictionsEnabled;
    if (!deviceManagementRestrictionsEnabled)
        return std::nullopt;

    std::optional<bool> allLoadsBlockedByDeviceManagementRestrictionsForTesting;
    decoder >> allLoadsBlockedByDeviceManagementRestrictionsForTesting;
    if (!allLoadsBlockedByDeviceManagementRestrictionsForTesting)
        return std::nullopt;

    std::optional<WebPushD::WebPushDaemonConnectionConfiguration> webPushDaemonConnectionConfiguration;
    decoder >> webPushDaemonConnectionConfiguration;
    if (!webPushDaemonConnectionConfiguration)
        return std::nullopt;

    std::optional<String> dataConnectionServiceType;
    decoder >> dataConnectionServiceType;
    if (!dataConnectionServiceType)
        return std::nullopt;
    
    std::optional<bool> fastServerTrustEvaluationEnabled;
    decoder >> fastServerTrustEvaluationEnabled;
    if (!fastServerTrustEvaluationEnabled)
        return std::nullopt;
    
    std::optional<bool> networkCacheSpeculativeValidationEnabled;
    decoder >> networkCacheSpeculativeValidationEnabled;
    if (!networkCacheSpeculativeValidationEnabled)
        return std::nullopt;
    
    std::optional<bool> shouldUseTestingNetworkSession;
    decoder >> shouldUseTestingNetworkSession;
    if (!shouldUseTestingNetworkSession)
        return std::nullopt;

    std::optional<bool> staleWhileRevalidateEnabled;
    decoder >> staleWhileRevalidateEnabled;
    if (!staleWhileRevalidateEnabled)
        return std::nullopt;

    std::optional<unsigned> testSpeedMultiplier;
    decoder >> testSpeedMultiplier;
    if (!testSpeedMultiplier)
        return std::nullopt;
    
    std::optional<bool> suppressesConnectionTerminationOnSystemChange;
    decoder >> suppressesConnectionTerminationOnSystemChange;
    if (!suppressesConnectionTerminationOnSystemChange)
        return std::nullopt;

    std::optional<bool> allowsServerPreconnect;
    decoder >> allowsServerPreconnect;
    if (!allowsServerPreconnect)
        return std::nullopt;

    std::optional<bool> requiresSecureHTTPSProxyConnection;
    decoder >> requiresSecureHTTPSProxyConnection;
    if (!requiresSecureHTTPSProxyConnection)
        return std::nullopt;

    std::optional<bool> shouldRunServiceWorkersOnMainThreadForTesting;
    decoder >> shouldRunServiceWorkersOnMainThreadForTesting;
    if (!shouldRunServiceWorkersOnMainThreadForTesting)
        return std::nullopt;
    
    std::optional<std::optional<unsigned>> overrideServiceWorkerRegistrationCountTestingValue;
    decoder >> overrideServiceWorkerRegistrationCountTestingValue;
    if (!overrideServiceWorkerRegistrationCountTestingValue)
        return std::nullopt;

    std::optional<bool> preventsSystemHTTPProxyAuthentication;
    decoder >> preventsSystemHTTPProxyAuthentication;
    if (!preventsSystemHTTPProxyAuthentication)
        return std::nullopt;
    
    std::optional<bool> appHasRequestedCrossWebsiteTrackingPermission;
    decoder >> appHasRequestedCrossWebsiteTrackingPermission;
    if (!appHasRequestedCrossWebsiteTrackingPermission)
        return std::nullopt;

    std::optional<bool> useNetworkLoader;
    decoder >> useNetworkLoader;
    if (!useNetworkLoader)
        return std::nullopt;

    std::optional<bool> allowsHSTSWithUntrustedRootCertificate;
    decoder >> allowsHSTSWithUntrustedRootCertificate;
    if (!allowsHSTSWithUntrustedRootCertificate)
        return std::nullopt;

    std::optional<String> pcmMachServiceName;
    decoder >> pcmMachServiceName;
    if (!pcmMachServiceName)
        return std::nullopt;

    std::optional<String> webPushMachServiceName;
    decoder >> webPushMachServiceName;
    if (!webPushMachServiceName)
        return std::nullopt;
    
    std::optional<String> webPushPartitionString;
    decoder >> webPushPartitionString;
    if (!webPushPartitionString)
        return std::nullopt;

    std::optional<bool> enablePrivateClickMeasurementDebugMode;
    decoder >> enablePrivateClickMeasurementDebugMode;
    if (!enablePrivateClickMeasurementDebugMode)
        return std::nullopt;

#if !HAVE(NSURLSESSION_WEBSOCKET)
    std::optional<bool> shouldAcceptInsecureCertificatesForWebSockets;
    decoder >> shouldAcceptInsecureCertificatesForWebSockets;
    if (!shouldAcceptInsecureCertificatesForWebSockets)
        return std::nullopt;
#endif

    std::optional<bool> shouldUseCustomStoragePaths;
    decoder >> shouldUseCustomStoragePaths;
    if (!shouldUseCustomStoragePaths)
        return std::nullopt;

    std::optional<uint64_t> perOriginStorageQuota;
    decoder >> perOriginStorageQuota;
    if (!perOriginStorageQuota)
        return std::nullopt;

    std::optional<uint64_t> perThirdPartyOriginStorageQuota;
    decoder >> perThirdPartyOriginStorageQuota;
    if (!perThirdPartyOriginStorageQuota)
        return std::nullopt;

    std::optional<String> localStorageDirectory;
    decoder >> localStorageDirectory;
    if (!localStorageDirectory)
        return std::nullopt;

    std::optional<SandboxExtension::Handle> localStorageDirectoryExtensionHandle;
    decoder >> localStorageDirectoryExtensionHandle;
    if (!localStorageDirectoryExtensionHandle)
        return std::nullopt;

    std::optional<String> indexedDBDirectory;
    decoder >> indexedDBDirectory;
    if (!indexedDBDirectory)
        return std::nullopt;
    
    std::optional<SandboxExtension::Handle> indexedDBDirectoryExtensionHandle;
    decoder >> indexedDBDirectoryExtensionHandle;
    if (!indexedDBDirectoryExtensionHandle)
        return std::nullopt;

    std::optional<String> cacheStorageDirectory;
    decoder >> cacheStorageDirectory;
    if (!cacheStorageDirectory)
        return std::nullopt;

    std::optional<SandboxExtension::Handle> cacheStorageDirectoryExtensionHandle;
    decoder >> cacheStorageDirectoryExtensionHandle;
    if (!cacheStorageDirectoryExtensionHandle)
        return std::nullopt;

    std::optional<String> generalStorageDirectory;
    decoder >> generalStorageDirectory;
    if (!generalStorageDirectory)
        return std::nullopt;

    std::optional<SandboxExtension::Handle> generalStorageDirectoryHandle;
    decoder >> generalStorageDirectoryHandle;
    if (!generalStorageDirectoryHandle)
        return std::nullopt;

#if ENABLE(SERVICE_WORKER)
    std::optional<String> serviceWorkerRegistrationDirectory;
    decoder >> serviceWorkerRegistrationDirectory;
    if (!serviceWorkerRegistrationDirectory)
        return std::nullopt;
    
    std::optional<SandboxExtension::Handle> serviceWorkerRegistrationDirectoryExtensionHandle;
    decoder >> serviceWorkerRegistrationDirectoryExtensionHandle;
    if (!serviceWorkerRegistrationDirectoryExtensionHandle)
        return std::nullopt;
    
    std::optional<bool> serviceWorkerProcessTerminationDelayEnabled;
    decoder >> serviceWorkerProcessTerminationDelayEnabled;
    if (!serviceWorkerProcessTerminationDelayEnabled)
        return std::nullopt;
#endif

    std::optional<ResourceLoadStatisticsParameters> resourceLoadStatisticsParameters;
    decoder >> resourceLoadStatisticsParameters;
    if (!resourceLoadStatisticsParameters)
        return std::nullopt;

    return {{
        *sessionID
        , WTFMove(*dataStoreIdentifier)
        , WTFMove(*boundInterfaceIdentifier)
        , WTFMove(*allowsCellularAccess)
#if PLATFORM(COCOA)
        , WTFMove(proxyConfiguration)
        , WTFMove(*sourceApplicationBundleIdentifier)
        , WTFMove(*sourceApplicationSecondaryIdentifier)
        , WTFMove(*shouldLogCookieInformation)
        , WTFMove(*httpProxy)
        , WTFMove(*httpsProxy)
#endif
#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
        , WTFMove(*alternativeServiceDirectory)
        , WTFMove(*alternativeServiceDirectoryExtensionHandle)
#endif
        , WTFMove(*hstsStorageDirectory)
        , WTFMove(*hstsStorageDirectoryExtensionHandle)
#if USE(SOUP)
        , WTFMove(*cookiePersistentStoragePath)
        , WTFMove(*cookiePersistentStorageType)
        , WTFMove(*persistentCredentialStorageEnabled)
        , WTFMove(*ignoreTLSErrors)
        , WTFMove(*proxySettings)
        , WTFMove(*cookieAcceptPolicy)
#endif
#if USE(CURL)
        , WTFMove(*cookiePersistentStorageFile)
        , WTFMove(*proxySettings)
#endif
        , WTFMove(*deviceManagementRestrictionsEnabled)
        , WTFMove(*allLoadsBlockedByDeviceManagementRestrictionsForTesting)
        , WTFMove(*webPushDaemonConnectionConfiguration)
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
        , *shouldRunServiceWorkersOnMainThreadForTesting
        , WTFMove(*overrideServiceWorkerRegistrationCountTestingValue)
        , WTFMove(*preventsSystemHTTPProxyAuthentication)
        , WTFMove(*appHasRequestedCrossWebsiteTrackingPermission)
        , WTFMove(*useNetworkLoader)
        , WTFMove(*allowsHSTSWithUntrustedRootCertificate)
        , WTFMove(*pcmMachServiceName)
        , WTFMove(*webPushMachServiceName)
        , WTFMove(*webPushPartitionString)
        , WTFMove(*enablePrivateClickMeasurementDebugMode)
#if !HAVE(NSURLSESSION_WEBSOCKET)
        , WTFMove(*shouldAcceptInsecureCertificatesForWebSockets)
#endif
        , *shouldUseCustomStoragePaths
        , WTFMove(*perOriginStorageQuota)
        , WTFMove(*perThirdPartyOriginStorageQuota)
        , WTFMove(*localStorageDirectory)
        , WTFMove(*localStorageDirectoryExtensionHandle)
        , WTFMove(*indexedDBDirectory)
        , WTFMove(*indexedDBDirectoryExtensionHandle)
        , WTFMove(*cacheStorageDirectory)
        , WTFMove(*cacheStorageDirectoryExtensionHandle)
        , WTFMove(*generalStorageDirectory)
        , WTFMove(*generalStorageDirectoryHandle)
#if ENABLE(SERVICE_WORKER)
        , WTFMove(*serviceWorkerRegistrationDirectory)
        , WTFMove(*serviceWorkerRegistrationDirectoryExtensionHandle)
        , *serviceWorkerProcessTerminationDelayEnabled
#endif
        , WTFMove(*resourceLoadStatisticsParameters)
    }};
}

} // namespace WebKit
