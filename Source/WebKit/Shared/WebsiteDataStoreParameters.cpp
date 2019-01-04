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
    encoder << pendingCookies;

#if ENABLE(INDEXED_DATABASE)
    encoder << indexedDatabaseDirectory << indexedDatabaseDirectoryExtensionHandle;
#if PLATFORM(IOS_FAMILY)
    encoder << indexedDatabaseTempBlobDirectoryExtensionHandle;
#endif
#endif

#if ENABLE(SERVICE_WORKER)
    encoder << serviceWorkerRegistrationDirectory << serviceWorkerRegistrationDirectoryExtensionHandle;
#endif
}

Optional<WebsiteDataStoreParameters> WebsiteDataStoreParameters::decode(IPC::Decoder& decoder)
{
    WebsiteDataStoreParameters parameters;

    Optional<NetworkSessionCreationParameters> networkSessionParameters;
    decoder >> networkSessionParameters;
    if (!networkSessionParameters)
        return WTF::nullopt;
    parameters.networkSessionParameters = WTFMove(*networkSessionParameters);

    Optional<Vector<uint8_t>> uiProcessCookieStorageIdentifier;
    decoder >> uiProcessCookieStorageIdentifier;
    if (!uiProcessCookieStorageIdentifier)
        return WTF::nullopt;
    parameters.uiProcessCookieStorageIdentifier = WTFMove(*uiProcessCookieStorageIdentifier);

    Optional<SandboxExtension::Handle> cookieStoragePathExtensionHandle;
    decoder >> cookieStoragePathExtensionHandle;
    if (!cookieStoragePathExtensionHandle)
        return WTF::nullopt;
    parameters.cookieStoragePathExtensionHandle = WTFMove(*cookieStoragePathExtensionHandle);

    Optional<Vector<WebCore::Cookie>> pendingCookies;
    decoder >> pendingCookies;
    if (!pendingCookies)
        return WTF::nullopt;
    parameters.pendingCookies = WTFMove(*pendingCookies);

#if ENABLE(INDEXED_DATABASE)
    Optional<String> indexedDatabaseDirectory;
    decoder >> indexedDatabaseDirectory;
    if (!indexedDatabaseDirectory)
        return WTF::nullopt;
    parameters.indexedDatabaseDirectory = WTFMove(*indexedDatabaseDirectory);
    
    Optional<SandboxExtension::Handle> indexedDatabaseDirectoryExtensionHandle;
    decoder >> indexedDatabaseDirectoryExtensionHandle;
    if (!indexedDatabaseDirectoryExtensionHandle)
        return WTF::nullopt;
    parameters.indexedDatabaseDirectoryExtensionHandle = WTFMove(*indexedDatabaseDirectoryExtensionHandle);

#if PLATFORM(IOS_FAMILY)
    Optional<SandboxExtension::Handle> indexedDatabaseTempBlobDirectoryExtensionHandle;
    decoder >> indexedDatabaseTempBlobDirectoryExtensionHandle;
    if (!indexedDatabaseTempBlobDirectoryExtensionHandle)
        return WTF::nullopt;
    parameters.indexedDatabaseTempBlobDirectoryExtensionHandle = WTFMove(*indexedDatabaseTempBlobDirectoryExtensionHandle);
#endif
#endif

#if ENABLE(SERVICE_WORKER)
    Optional<String> serviceWorkerRegistrationDirectory;
    decoder >> serviceWorkerRegistrationDirectory;
    if (!serviceWorkerRegistrationDirectory)
        return WTF::nullopt;
    parameters.serviceWorkerRegistrationDirectory = WTFMove(*serviceWorkerRegistrationDirectory);
    
    Optional<SandboxExtension::Handle> serviceWorkerRegistrationDirectoryExtensionHandle;
    decoder >> serviceWorkerRegistrationDirectoryExtensionHandle;
    if (!serviceWorkerRegistrationDirectoryExtensionHandle)
        return WTF::nullopt;
    parameters.serviceWorkerRegistrationDirectoryExtensionHandle = WTFMove(*serviceWorkerRegistrationDirectoryExtensionHandle);
#endif
    
    return WTFMove(parameters);
}

WebsiteDataStoreParameters WebsiteDataStoreParameters::privateSessionParameters(PAL::SessionID sessionID)
{
    ASSERT(sessionID.isEphemeral());
    return { { }, { }, { }, NetworkSessionCreationParameters::privateSessionParameters(sessionID)
#if ENABLE(INDEXED_DATABASE)
        , { }, { }
#if PLATFORM(IOS_FAMILY)
        , { }
#endif
#endif
#if ENABLE(SERVICE_WORKER)
        , { }, { }
#endif
    };
}

} // namespace WebKit
