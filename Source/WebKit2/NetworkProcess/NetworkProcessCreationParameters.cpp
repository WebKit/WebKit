/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)
#include "ArgumentCodersCF.h"
#endif

namespace WebKit {

NetworkProcessCreationParameters::NetworkProcessCreationParameters()
{
}

void NetworkProcessCreationParameters::encode(IPC::Encoder& encoder) const
{
    encoder << privateBrowsingEnabled;
    encoder.encodeEnum(cacheModel);
    encoder << diskCacheSizeOverride;
    encoder << canHandleHTTPSServerTrustEvaluation;
    encoder << diskCacheDirectory;
    encoder << diskCacheDirectoryExtensionHandle;
#if ENABLE(NETWORK_CACHE)
    encoder << shouldEnableNetworkCache;
    encoder << shouldEnableNetworkCacheEfficacyLogging;
#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
    encoder << shouldEnableNetworkCacheSpeculativeRevalidation;
#endif
#endif
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
    encoder << uiProcessCookieStorageIdentifier;
#endif
#if PLATFORM(IOS)
    encoder << cookieStorageDirectoryExtensionHandle;
    encoder << containerCachesDirectoryExtensionHandle;
    encoder << parentBundleDirectoryExtensionHandle;
#endif
    encoder << shouldSuppressMemoryPressureHandler;
    encoder << shouldUseTestingNetworkSession;
    encoder << urlSchemesRegisteredForCustomProtocols;
#if PLATFORM(COCOA)
    encoder << parentProcessName;
    encoder << uiProcessBundleIdentifier;
    encoder << nsURLCacheMemoryCapacity;
    encoder << nsURLCacheDiskCapacity;
    encoder << httpProxy;
    encoder << httpsProxy;
#if TARGET_OS_IPHONE || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100)
    IPC::encode(encoder, networkATSContext.get());
#endif
    encoder << cookieStoragePartitioningEnabled;
#endif
#if USE(SOUP)
    encoder << cookiePersistentStoragePath;
    encoder << cookiePersistentStorageType;
    encoder.encodeEnum(cookieAcceptPolicy);
    encoder << ignoreTLSErrors;
    encoder << languages;
#endif
#if OS(LINUX)
    encoder << memoryPressureMonitorHandle;
#endif
}

bool NetworkProcessCreationParameters::decode(IPC::Decoder& decoder, NetworkProcessCreationParameters& result)
{
    if (!decoder.decode(result.privateBrowsingEnabled))
        return false;
    if (!decoder.decodeEnum(result.cacheModel))
        return false;
    if (!decoder.decode(result.diskCacheSizeOverride))
        return false;
    if (!decoder.decode(result.canHandleHTTPSServerTrustEvaluation))
        return false;
    if (!decoder.decode(result.diskCacheDirectory))
        return false;
    if (!decoder.decode(result.diskCacheDirectoryExtensionHandle))
        return false;
#if ENABLE(NETWORK_CACHE)
    if (!decoder.decode(result.shouldEnableNetworkCache))
        return false;
    if (!decoder.decode(result.shouldEnableNetworkCacheEfficacyLogging))
        return false;
#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
    if (!decoder.decode(result.shouldEnableNetworkCacheSpeculativeRevalidation))
        return false;
#endif
#endif
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
    if (!decoder.decode(result.uiProcessCookieStorageIdentifier))
        return false;
#endif
#if PLATFORM(IOS)
    if (!decoder.decode(result.cookieStorageDirectoryExtensionHandle))
        return false;
    if (!decoder.decode(result.containerCachesDirectoryExtensionHandle))
        return false;
    if (!decoder.decode(result.parentBundleDirectoryExtensionHandle))
        return false;
#endif
    if (!decoder.decode(result.shouldSuppressMemoryPressureHandler))
        return false;
    if (!decoder.decode(result.shouldUseTestingNetworkSession))
        return false;
    if (!decoder.decode(result.urlSchemesRegisteredForCustomProtocols))
        return false;
#if PLATFORM(COCOA)
    if (!decoder.decode(result.parentProcessName))
        return false;
    if (!decoder.decode(result.uiProcessBundleIdentifier))
        return false;
    if (!decoder.decode(result.nsURLCacheMemoryCapacity))
        return false;
    if (!decoder.decode(result.nsURLCacheDiskCapacity))
        return false;
    if (!decoder.decode(result.httpProxy))
        return false;
    if (!decoder.decode(result.httpsProxy))
        return false;
#if TARGET_OS_IPHONE || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100)
    if (!IPC::decode(decoder, result.networkATSContext))
        return false;
#endif
    if (!decoder.decode(result.cookieStoragePartitioningEnabled))
        return false;
#endif

#if USE(SOUP)
    if (!decoder.decode(result.cookiePersistentStoragePath))
        return false;
    if (!decoder.decode(result.cookiePersistentStorageType))
        return false;
    if (!decoder.decodeEnum(result.cookieAcceptPolicy))
        return false;
    if (!decoder.decode(result.ignoreTLSErrors))
        return false;
    if (!decoder.decode(result.languages))
        return false;
#endif

#if OS(LINUX)
    if (!decoder.decode(result.memoryPressureMonitorHandle))
        return false;
#endif

    return true;
}

} // namespace WebKit
