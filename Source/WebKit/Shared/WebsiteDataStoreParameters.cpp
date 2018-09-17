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
#endif
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

    std::optional<Vector<WebCore::Cookie>> pendingCookies;
    decoder >> pendingCookies;
    if (!pendingCookies)
        return std::nullopt;
    parameters.pendingCookies = WTFMove(*pendingCookies);

#if ENABLE(INDEXED_DATABASE)
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
#endif

    return WTFMove(parameters);
}

WebsiteDataStoreParameters WebsiteDataStoreParameters::privateSessionParameters(PAL::SessionID sessionID)
{
    ASSERT(sessionID.isEphemeral());
    return { { }, { }, { }, { sessionID, { }, AllowsCellularAccess::Yes
#if PLATFORM(COCOA)
        , nullptr , { } , { }
#endif
        }
#if ENABLE(INDEXED_DATABASE)
        , { }, { }
#endif
    };
}

} // namespace WebKit
