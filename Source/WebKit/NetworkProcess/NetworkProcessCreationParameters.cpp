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

#include "config.h"
#include "NetworkProcessCreationParameters.h"

#include "ArgumentCoders.h"
#include "WebCoreArgumentCoders.h"

#if PLATFORM(COCOA)
#include "ArgumentCodersCF.h"
#endif

namespace WebKit {

NetworkProcessCreationParameters::NetworkProcessCreationParameters() = default;

void NetworkProcessCreationParameters::encode(IPC::Encoder& encoder) const
{
    encoder << cacheModel;
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    encoder << uiProcessCookieStorageIdentifier;
#endif
#if PLATFORM(IOS_FAMILY)
    encoder << cookieStorageDirectoryExtensionHandle;
    encoder << containerCachesDirectoryExtensionHandle;
    encoder << parentBundleDirectoryExtensionHandle;
#endif
    encoder << shouldSuppressMemoryPressureHandler;
    encoder << urlSchemesRegisteredForCustomProtocols;
#if PLATFORM(COCOA)
    encoder << uiProcessBundleIdentifier;
    encoder << uiProcessSDKVersion;
    IPC::encode(encoder, networkATSContext.get());
#endif
    encoder << defaultDataStoreParameters;
#if USE(SOUP)
    encoder << cookieAcceptPolicy;
    encoder << languages;
#endif

    encoder << urlSchemesRegisteredAsSecure;
    encoder << urlSchemesRegisteredAsBypassingContentSecurityPolicy;
    encoder << urlSchemesRegisteredAsLocal;
    encoder << urlSchemesRegisteredAsNoAccess;

#if ENABLE(SERVICE_WORKER)
    encoder << serviceWorkerRegistrationDirectory << serviceWorkerRegistrationDirectoryExtensionHandle << shouldDisableServiceWorkerProcessTerminationDelay;
#endif
    encoder << shouldEnableITPDatabase;
    encoder << enableAdClickAttributionDebugMode;
}

bool NetworkProcessCreationParameters::decode(IPC::Decoder& decoder, NetworkProcessCreationParameters& result)
{
    if (!decoder.decode(result.cacheModel))
        return false;

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    if (!decoder.decode(result.uiProcessCookieStorageIdentifier))
        return false;
#endif
#if PLATFORM(IOS_FAMILY)
    Optional<SandboxExtension::Handle> cookieStorageDirectoryExtensionHandle;
    decoder >> cookieStorageDirectoryExtensionHandle;
    if (!cookieStorageDirectoryExtensionHandle)
        return false;
    result.cookieStorageDirectoryExtensionHandle = WTFMove(*cookieStorageDirectoryExtensionHandle);

    Optional<SandboxExtension::Handle> containerCachesDirectoryExtensionHandle;
    decoder >> containerCachesDirectoryExtensionHandle;
    if (!containerCachesDirectoryExtensionHandle)
        return false;
    result.containerCachesDirectoryExtensionHandle = WTFMove(*containerCachesDirectoryExtensionHandle);

    Optional<SandboxExtension::Handle> parentBundleDirectoryExtensionHandle;
    decoder >> parentBundleDirectoryExtensionHandle;
    if (!parentBundleDirectoryExtensionHandle)
        return false;
    result.parentBundleDirectoryExtensionHandle = WTFMove(*parentBundleDirectoryExtensionHandle);
#endif
    if (!decoder.decode(result.shouldSuppressMemoryPressureHandler))
        return false;
    if (!decoder.decode(result.urlSchemesRegisteredForCustomProtocols))
        return false;
#if PLATFORM(COCOA)
    if (!decoder.decode(result.uiProcessBundleIdentifier))
        return false;
    if (!decoder.decode(result.uiProcessSDKVersion))
        return false;
    if (!IPC::decode(decoder, result.networkATSContext))
        return false;
#endif

    Optional<WebsiteDataStoreParameters> defaultDataStoreParameters;
    decoder >> defaultDataStoreParameters;
    if (!defaultDataStoreParameters)
        return false;
    result.defaultDataStoreParameters = WTFMove(*defaultDataStoreParameters);

#if USE(SOUP)
    if (!decoder.decode(result.cookieAcceptPolicy))
        return false;
    if (!decoder.decode(result.languages))
        return false;
#endif

    if (!decoder.decode(result.urlSchemesRegisteredAsSecure))
        return false;
    if (!decoder.decode(result.urlSchemesRegisteredAsBypassingContentSecurityPolicy))
        return false;
    if (!decoder.decode(result.urlSchemesRegisteredAsLocal))
        return false;
    if (!decoder.decode(result.urlSchemesRegisteredAsNoAccess))
        return false;

#if ENABLE(SERVICE_WORKER)
    if (!decoder.decode(result.serviceWorkerRegistrationDirectory))
        return false;
    
    Optional<SandboxExtension::Handle> serviceWorkerRegistrationDirectoryExtensionHandle;
    decoder >> serviceWorkerRegistrationDirectoryExtensionHandle;
    if (!serviceWorkerRegistrationDirectoryExtensionHandle)
        return false;
    result.serviceWorkerRegistrationDirectoryExtensionHandle = WTFMove(*serviceWorkerRegistrationDirectoryExtensionHandle);

    if (!decoder.decode(result.shouldDisableServiceWorkerProcessTerminationDelay))
        return false;
#endif

    if (!decoder.decode(result.shouldEnableITPDatabase))
        return false;

    if (!decoder.decode(result.enableAdClickAttributionDebugMode))
        return false;

    return true;
}

} // namespace WebKit
