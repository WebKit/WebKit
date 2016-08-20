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

#pragma once

#include "Attachment.h"
#include "CacheModel.h"
#include "SandboxExtension.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if USE(SOUP)
#include "HTTPCookieAcceptPolicy.h"
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

    bool privateBrowsingEnabled;
    CacheModel cacheModel;
    int64_t diskCacheSizeOverride { -1 };
    bool canHandleHTTPSServerTrustEvaluation;

    String diskCacheDirectory;
    SandboxExtension::Handle diskCacheDirectoryExtensionHandle;
#if ENABLE(NETWORK_CACHE)
    bool shouldEnableNetworkCache;
    bool shouldEnableNetworkCacheEfficacyLogging;
#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
    bool shouldEnableNetworkCacheSpeculativeRevalidation;
#endif
#endif
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
    Vector<uint8_t> uiProcessCookieStorageIdentifier;
#endif
#if PLATFORM(IOS)
    SandboxExtension::Handle cookieStorageDirectoryExtensionHandle;
    SandboxExtension::Handle containerCachesDirectoryExtensionHandle;
    SandboxExtension::Handle parentBundleDirectoryExtensionHandle;
#endif
    bool shouldSuppressMemoryPressureHandler { false };
    bool shouldUseTestingNetworkSession;

    Vector<String> urlSchemesRegisteredForCustomProtocols;

#if PLATFORM(COCOA)
    String parentProcessName;
    String uiProcessBundleIdentifier;
    uint64_t nsURLCacheMemoryCapacity;
    uint64_t nsURLCacheDiskCapacity;

    String httpProxy;
    String httpsProxy;
#if TARGET_OS_IPHONE || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100)
    RetainPtr<CFDataRef> networkATSContext;
#endif
    bool cookieStoragePartitioningEnabled;
#endif

#if USE(SOUP)
    String cookiePersistentStoragePath;
    uint32_t cookiePersistentStorageType;
    HTTPCookieAcceptPolicy cookieAcceptPolicy;
    bool ignoreTLSErrors;
    Vector<String> languages;
#endif

#if OS(LINUX)
    IPC::Attachment memoryPressureMonitorHandle;
#endif
};

} // namespace WebKit
