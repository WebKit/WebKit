/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/Credential.h>
#include <WebCore/CredentialStorage.h>
#include <WebCore/ProtectionSpace.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/URL.h>

namespace TestWebKitAPI {

static WebCore::Credential testCredential()
{
    return WebCore::Credential { "user"_s, "password"_s, WebCore::CredentialPersistence::None };
}

TEST(CredentialStorage, RemoveCredentialsWithHTTPSOriginDefaultPort)
{
    WebCore::CredentialStorage storage;
    WebCore::ProtectionSpace protectionSpace { "example.com"_s, 443, WebCore::ProtectionSpace::ServerType::HTTPS, "realm"_s, WebCore::ProtectionSpace::AuthenticationScheme::Default };
    storage.set(emptyString(), testCredential(), protectionSpace, URL { "https://example.com/index.html"_str });

    EXPECT_FALSE(storage.get(emptyString(), protectionSpace).isEmpty());

    // An origin built from a URL has its default port stripped to std::nullopt, mirroring what the
    // NetworkProcess passes in when clearing website data by origin.
    WebCore::SecurityOriginData origin { "https"_s, "example.com"_s, std::nullopt };
    storage.removeCredentialsWithOrigin(origin);

    EXPECT_TRUE(storage.get(emptyString(), protectionSpace).isEmpty());
}

TEST(CredentialStorage, RemoveCredentialsWithHTTPOriginDefaultPort)
{
    WebCore::CredentialStorage storage;
    WebCore::ProtectionSpace protectionSpace { "example.com"_s, 80, WebCore::ProtectionSpace::ServerType::HTTP, "realm"_s, WebCore::ProtectionSpace::AuthenticationScheme::Default };
    storage.set(emptyString(), testCredential(), protectionSpace, URL { "http://example.com/index.html"_str });

    EXPECT_FALSE(storage.get(emptyString(), protectionSpace).isEmpty());

    WebCore::SecurityOriginData origin { "http"_s, "example.com"_s, std::nullopt };
    storage.removeCredentialsWithOrigin(origin);

    EXPECT_TRUE(storage.get(emptyString(), protectionSpace).isEmpty());
}

TEST(CredentialStorage, RemoveCredentialsWithExplicitPort)
{
    WebCore::CredentialStorage storage;
    WebCore::ProtectionSpace protectionSpace { "example.com"_s, 8443, WebCore::ProtectionSpace::ServerType::HTTPS, "realm"_s, WebCore::ProtectionSpace::AuthenticationScheme::Default };
    storage.set(emptyString(), testCredential(), protectionSpace, URL { "https://example.com:8443/index.html"_str });

    EXPECT_FALSE(storage.get(emptyString(), protectionSpace).isEmpty());

    WebCore::SecurityOriginData origin { "https"_s, "example.com"_s, 8443 };
    storage.removeCredentialsWithOrigin(origin);

    EXPECT_TRUE(storage.get(emptyString(), protectionSpace).isEmpty());
}

TEST(CredentialStorage, RemoveCredentialsWithOriginDoesNotAffectOtherScheme)
{
    WebCore::CredentialStorage storage;
    WebCore::ProtectionSpace httpsSpace { "example.com"_s, 443, WebCore::ProtectionSpace::ServerType::HTTPS, "realm"_s, WebCore::ProtectionSpace::AuthenticationScheme::Default };
    storage.set(emptyString(), testCredential(), httpsSpace, URL { "https://example.com/index.html"_str });

    // Removing the http origin must not purge the https credential.
    WebCore::SecurityOriginData httpOrigin { "http"_s, "example.com"_s, std::nullopt };
    storage.removeCredentialsWithOrigin(httpOrigin);

    EXPECT_FALSE(storage.get(emptyString(), httpsSpace).isEmpty());
}

} // namespace TestWebKitAPI
