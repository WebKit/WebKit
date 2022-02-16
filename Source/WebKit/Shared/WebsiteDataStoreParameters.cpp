/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "WebsiteDataStoreParameters.h"

#include "WebCoreArgumentCoders.h"
#include "WebsiteDataStore.h"

namespace WebKit {

WebsiteDataStoreParameters::~WebsiteDataStoreParameters()
{
}

void WebsiteDataStoreParameters::encode(IPC::Encoder& encoder) const
{
    encoder << networkSessionParameters;
    encoder << uiProcessCookieStorageIdentifier;
    encoder << cookieStoragePathExtensionHandle;
    encoder << indexedDatabaseDirectory << indexedDatabaseDirectoryExtensionHandle;

#if ENABLE(SERVICE_WORKER)
    encoder << serviceWorkerRegistrationDirectory << serviceWorkerRegistrationDirectoryExtensionHandle << serviceWorkerProcessTerminationDelayEnabled;
#endif

    encoder << localStorageDirectory << localStorageDirectoryExtensionHandle;
    encoder << cacheStorageDirectory << cacheStorageDirectoryExtensionHandle;
    encoder << generalStorageDirectory << generalStorageDirectoryHandle;

    encoder << perOriginStorageQuota;
    encoder << perThirdPartyOriginStorageQuota;

    encoder << shouldUseCustomStoragePaths;
}

std::optional<WebsiteDataStoreParameters> WebsiteDataStoreParameters::decode(IPC::Decoder& decoder)
{
    WebsiteDataStoreParameters parameters;

    std::optional<NetworkSessionCreationParameters> networkSessionParameters;
    decoder >> networkSessionParameters;
    if (!networkSessionParameters)
        return std::nullopt;
    parameters.networkSessionParameters = WTFMove(*networkSessionParameters);

    std::optional<Vector<uint8_t>> uiProcessCookieStorageIdentifier;
    decoder >> uiProcessCookieStorageIdentifier;
    if (!uiProcessCookieStorageIdentifier)
        return std::nullopt;
    parameters.uiProcessCookieStorageIdentifier = WTFMove(*uiProcessCookieStorageIdentifier);

    std::optional<SandboxExtension::Handle> cookieStoragePathExtensionHandle;
    decoder >> cookieStoragePathExtensionHandle;
    if (!cookieStoragePathExtensionHandle)
        return std::nullopt;
    parameters.cookieStoragePathExtensionHandle = WTFMove(*cookieStoragePathExtensionHandle);

    std::optional<String> indexedDatabaseDirectory;
    decoder >> indexedDatabaseDirectory;
    if (!indexedDatabaseDirectory)
        return std::nullopt;
    parameters.indexedDatabaseDirectory = WTFMove(*indexedDatabaseDirectory);
    
    std::optional<SandboxExtension::Handle> indexedDatabaseDirectoryExtensionHandle;
    decoder >> indexedDatabaseDirectoryExtensionHandle;
    if (!indexedDatabaseDirectoryExtensionHandle)
        return std::nullopt;
    parameters.indexedDatabaseDirectoryExtensionHandle = WTFMove(*indexedDatabaseDirectoryExtensionHandle);

#if ENABLE(SERVICE_WORKER)
    std::optional<String> serviceWorkerRegistrationDirectory;
    decoder >> serviceWorkerRegistrationDirectory;
    if (!serviceWorkerRegistrationDirectory)
        return std::nullopt;
    parameters.serviceWorkerRegistrationDirectory = WTFMove(*serviceWorkerRegistrationDirectory);
    
    std::optional<SandboxExtension::Handle> serviceWorkerRegistrationDirectoryExtensionHandle;
    decoder >> serviceWorkerRegistrationDirectoryExtensionHandle;
    if (!serviceWorkerRegistrationDirectoryExtensionHandle)
        return std::nullopt;
    parameters.serviceWorkerRegistrationDirectoryExtensionHandle = WTFMove(*serviceWorkerRegistrationDirectoryExtensionHandle);
    
    std::optional<bool> serviceWorkerProcessTerminationDelayEnabled;
    decoder >> serviceWorkerProcessTerminationDelayEnabled;
    if (!serviceWorkerProcessTerminationDelayEnabled)
        return std::nullopt;
    parameters.serviceWorkerProcessTerminationDelayEnabled = WTFMove(*serviceWorkerProcessTerminationDelayEnabled);
#endif

    std::optional<String> localStorageDirectory;
    decoder >> localStorageDirectory;
    if (!localStorageDirectory)
        return std::nullopt;
    parameters.localStorageDirectory = WTFMove(*localStorageDirectory);

    std::optional<SandboxExtension::Handle> localStorageDirectoryExtensionHandle;
    decoder >> localStorageDirectoryExtensionHandle;
    if (!localStorageDirectoryExtensionHandle)
        return std::nullopt;
    parameters.localStorageDirectoryExtensionHandle = WTFMove(*localStorageDirectoryExtensionHandle);

    std::optional<String> cacheStorageDirectory;
    decoder >> cacheStorageDirectory;
    if (!cacheStorageDirectory)
        return std::nullopt;
    parameters.cacheStorageDirectory = WTFMove(*cacheStorageDirectory);

    std::optional<SandboxExtension::Handle> cacheStorageDirectoryExtensionHandle;
    decoder >> cacheStorageDirectoryExtensionHandle;
    if (!cacheStorageDirectoryExtensionHandle)
        return std::nullopt;
    parameters.cacheStorageDirectoryExtensionHandle = WTFMove(*cacheStorageDirectoryExtensionHandle);

    std::optional<String> generalStorageDirectory;
    decoder >> generalStorageDirectory;
    if (!generalStorageDirectory)
        return std::nullopt;
    parameters.generalStorageDirectory = WTFMove(*generalStorageDirectory);

    std::optional<SandboxExtension::Handle> generalStorageDirectoryHandle;
    decoder >> generalStorageDirectoryHandle;
    if (!generalStorageDirectoryHandle)
        return std::nullopt;
    parameters.generalStorageDirectoryHandle = WTFMove(*generalStorageDirectoryHandle);

    std::optional<uint64_t> perOriginStorageQuota;
    decoder >> perOriginStorageQuota;
    if (!perOriginStorageQuota)
        return std::nullopt;
    parameters.perOriginStorageQuota = *perOriginStorageQuota;

    std::optional<uint64_t> perThirdPartyOriginStorageQuota;
    decoder >> perThirdPartyOriginStorageQuota;
    if (!perThirdPartyOriginStorageQuota)
        return std::nullopt;
    parameters.perThirdPartyOriginStorageQuota = *perThirdPartyOriginStorageQuota;

    std::optional<bool> shouldUseCustomStoragePaths;
    decoder >> shouldUseCustomStoragePaths;
    if (!shouldUseCustomStoragePaths)
        return std::nullopt;
    parameters.shouldUseCustomStoragePaths = *shouldUseCustomStoragePaths;
    
    return parameters;
}

} // namespace WebKit
