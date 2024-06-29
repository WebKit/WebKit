/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include <WebCore/LoginStatus.h>
#include <WebCore/RegistrableDomain.h>
#include <wtf/Seconds.h>
#include <wtf/text/StringBuilder.h>

using namespace WebCore;

namespace TestWebKitAPI {

// Positive test cases.

TEST(LoginStatus, DefaultExpiryWebAuthn)
{
    RegistrableDomain loginDomain { URL { "https://login.example.com"_s } };

    auto loggedInResult = LoginStatus::create(loginDomain, "admin"_s, LoginStatus::CredentialTokenType::HTTPStateToken, LoginStatus::AuthenticationType::WebAuthn);
    auto& loggedIn = loggedInResult.returnValue().get();
    auto now = WallTime::now();
    ASSERT_EQ(loggedIn.registrableDomain().string(), "example.com"_s);
    ASSERT_EQ(loggedIn.username(), "admin"_s);
    ASSERT_EQ(loggedIn.credentialTokenType(), LoginStatus::CredentialTokenType::HTTPStateToken);
    ASSERT_EQ(loggedIn.authenticationType(), LoginStatus::AuthenticationType::WebAuthn);
    ASSERT_TRUE(loggedIn.loggedInTime() < now + 60_s);
    ASSERT_TRUE(loggedIn.loggedInTime() > now - 60_s);
    ASSERT_TRUE(loggedIn.expiry() < now + LoginStatus::TimeToLiveLong + 60_s);
    ASSERT_TRUE(loggedIn.expiry() > now + LoginStatus::TimeToLiveLong - 60_s);
    ASSERT_FALSE(loggedIn.hasExpired());
}

TEST(LoginStatus, DefaultExpiryPasswordManager)
{
    RegistrableDomain loginDomain { URL { "https://login.example.com"_s } };

    auto loggedInResult = LoginStatus::create(loginDomain, "admin"_s, LoginStatus::CredentialTokenType::HTTPStateToken, LoginStatus::AuthenticationType::PasswordManager);
    auto& loggedIn = loggedInResult.returnValue().get();
    auto now = WallTime::now();
    ASSERT_EQ(loggedIn.authenticationType(), LoginStatus::AuthenticationType::PasswordManager);
    ASSERT_TRUE(loggedIn.expiry() < now + LoginStatus::TimeToLiveLong + 60_s);
    ASSERT_TRUE(loggedIn.expiry() > now + LoginStatus::TimeToLiveLong - 60_s);
    ASSERT_FALSE(loggedIn.hasExpired());
}

TEST(LoginStatus, DefaultExpiryUnmanaged)
{
    RegistrableDomain loginDomain { URL { "https://login.example.com"_s } };

    auto loggedInResult = LoginStatus::create(loginDomain, "admin"_s, LoginStatus::CredentialTokenType::HTTPStateToken, LoginStatus::AuthenticationType::Unmanaged);
    auto& loggedIn = loggedInResult.returnValue().get();
    auto now = WallTime::now();
    ASSERT_EQ(loggedIn.authenticationType(), LoginStatus::AuthenticationType::Unmanaged);
    ASSERT_TRUE(loggedIn.expiry() < now + LoginStatus::TimeToLiveShort + 60_s);
    ASSERT_TRUE(loggedIn.expiry() > now + LoginStatus::TimeToLiveShort - 60_s);
    ASSERT_FALSE(loggedIn.hasExpired());
}

TEST(LoginStatus, CustomExpiryBelowLong)
{
    RegistrableDomain loginDomain { URL { "https://login.example.com"_s } };

    auto customExpiry = LoginStatus::TimeToLiveLong - 48_h;
    auto loggedInResult = LoginStatus::create(loginDomain, "admin"_s, LoginStatus::CredentialTokenType::HTTPStateToken, LoginStatus::AuthenticationType::WebAuthn, customExpiry);
    auto& loggedIn = loggedInResult.returnValue().get();
    auto now = WallTime::now();
    ASSERT_EQ(loggedIn.authenticationType(), LoginStatus::AuthenticationType::WebAuthn);
    ASSERT_TRUE(loggedIn.expiry() < now + customExpiry + 60_s);
    ASSERT_TRUE(loggedIn.expiry() > now + customExpiry - 60_s);
    ASSERT_FALSE(loggedIn.hasExpired());
}

TEST(LoginStatus, CustomExpiryBelowShort)
{
    RegistrableDomain loginDomain { URL { "https://login.example.com"_s } };

    auto customExpiry = LoginStatus::TimeToLiveShort - 48_h;
    auto loggedInResult = LoginStatus::create(loginDomain, "admin"_s, LoginStatus::CredentialTokenType::HTTPStateToken, LoginStatus::AuthenticationType::PasswordManager, customExpiry);
    auto& loggedIn = loggedInResult.returnValue().get();
    auto now = WallTime::now();
    ASSERT_EQ(loggedIn.authenticationType(), LoginStatus::AuthenticationType::PasswordManager);
    ASSERT_TRUE(loggedIn.expiry() < now + customExpiry + 60_s);
    ASSERT_TRUE(loggedIn.expiry() > now + customExpiry - 60_s);
    ASSERT_FALSE(loggedIn.hasExpired());
}

TEST(LoginStatus, RenewedExpiry)
{
    RegistrableDomain loginDomain { URL { "https://login.example.com"_s } };

    auto customExpiry = LoginStatus::TimeToLiveShort - 48_h;
    auto loggedInResult = LoginStatus::create(loginDomain, "admin"_s, LoginStatus::CredentialTokenType::HTTPStateToken, LoginStatus::AuthenticationType::WebAuthn, customExpiry);
    auto loggedIn = loggedInResult.releaseReturnValue();
    auto now = WallTime::now();
    ASSERT_EQ(loggedIn->authenticationType(), LoginStatus::AuthenticationType::WebAuthn);
    ASSERT_TRUE(loggedIn->expiry() < now + customExpiry + 60_s);
    ASSERT_TRUE(loggedIn->expiry() > now + customExpiry - 60_s);
    ASSERT_FALSE(loggedIn->hasExpired());
    auto newCustomExpiry = 24_h;
    loggedIn->setTimeToLive(newCustomExpiry);
    ASSERT_EQ(loggedIn->authenticationType(), LoginStatus::AuthenticationType::WebAuthn);
    ASSERT_TRUE(loggedIn->expiry() < now + newCustomExpiry + 60_s);
    ASSERT_TRUE(loggedIn->expiry() > now + newCustomExpiry - 60_s);
    ASSERT_FALSE(loggedIn->hasExpired());
}

// Negative test cases.

TEST(LoginStatus, InvalidUsernames)
{
    RegistrableDomain loginDomain { URL { "https://login.example.com"_s } };

    // Whitespace is not allowed.
    auto loggedInResult = LoginStatus::create(loginDomain, "I use spaces"_s, LoginStatus::CredentialTokenType::HTTPStateToken, LoginStatus::AuthenticationType::WebAuthn);
    ASSERT_TRUE(loggedInResult.hasException());

    // Newlines are not allowed.
    loggedInResult = LoginStatus::create(loginDomain, "Enter\nreturn"_s, LoginStatus::CredentialTokenType::HTTPStateToken, LoginStatus::AuthenticationType::WebAuthn);
    ASSERT_TRUE(loggedInResult.hasException());

    // There is a max length to usernames.
    StringBuilder builder;
    for (unsigned i = 0; i <= LoginStatus::UsernameMaxLength; ++i)
        builder.append('a');
    loggedInResult = LoginStatus::create(loginDomain, builder.toString(), LoginStatus::CredentialTokenType::HTTPStateToken, LoginStatus::AuthenticationType::WebAuthn);
    ASSERT_TRUE(loggedInResult.hasException());
}

TEST(LoginStatus, ClampedExpiryLong)
{
    RegistrableDomain loginDomain { URL { "https://login.example.com"_s } };

    // Too long expiries for managed auth types should be clamped to LoginStatus::TimeToLiveLong.
    auto loggedInResult = LoginStatus::create(loginDomain, "admin"_s, LoginStatus::CredentialTokenType::HTTPStateToken, LoginStatus::AuthenticationType::WebAuthn, LoginStatus::TimeToLiveLong + 24_h);
    auto loggedIn = loggedInResult.releaseReturnValue();
    auto now = WallTime::now();
    ASSERT_EQ(loggedIn->authenticationType(), LoginStatus::AuthenticationType::WebAuthn);
    ASSERT_TRUE(loggedIn->expiry() < now + LoginStatus::TimeToLiveLong + 60_s);
    ASSERT_TRUE(loggedIn->expiry() > now + LoginStatus::TimeToLiveLong - 60_s);
    ASSERT_FALSE(loggedIn->hasExpired());

    // Renewed, too long expiries for managed auth types should also be clamped to LoginStatus::TimeToLiveLong.
    auto newCustomExpiry = LoginStatus::TimeToLiveLong + 24_h;
    loggedIn->setTimeToLive(newCustomExpiry);
    ASSERT_EQ(loggedIn->authenticationType(), LoginStatus::AuthenticationType::WebAuthn);
    ASSERT_TRUE(loggedIn->expiry() < now + LoginStatus::TimeToLiveLong + 60_s);
    ASSERT_TRUE(loggedIn->expiry() > now + LoginStatus::TimeToLiveLong - 60_s);
    ASSERT_FALSE(loggedIn->hasExpired());
}

TEST(LoginStatus, ClampedExpiryShort)
{
    RegistrableDomain loginDomain { URL { "https://login.example.com"_s } };

    // Too long expiries for unmanaged auth types should be clamped to LoginStatus::TimeToLiveShort.
    auto loggedInResult = LoginStatus::create(loginDomain, "admin"_s, LoginStatus::CredentialTokenType::HTTPStateToken, LoginStatus::AuthenticationType::Unmanaged, LoginStatus::TimeToLiveShort + 24_h);
    auto loggedIn = loggedInResult.returnValue().get();
    auto now = WallTime::now();
    ASSERT_EQ(loggedIn.authenticationType(), LoginStatus::AuthenticationType::Unmanaged);
    ASSERT_TRUE(loggedIn.expiry() < now + LoginStatus::TimeToLiveShort + 60_s);
    ASSERT_TRUE(loggedIn.expiry() > now + LoginStatus::TimeToLiveShort - 60_s);
    ASSERT_FALSE(loggedIn.hasExpired());

    // Renewed, too long expiries for unmanaged auth types should also be clamped to LoginStatus::TimeToLiveShort.
    auto newCustomExpiry = LoginStatus::TimeToLiveShort + 24_h;
    loggedIn.setTimeToLive(newCustomExpiry);
    ASSERT_EQ(loggedIn.authenticationType(), LoginStatus::AuthenticationType::Unmanaged);
    ASSERT_TRUE(loggedIn.expiry() < now + LoginStatus::TimeToLiveShort + 60_s);
    ASSERT_TRUE(loggedIn.expiry() > now + LoginStatus::TimeToLiveShort - 60_s);
    ASSERT_FALSE(loggedIn.hasExpired());
}

} // namespace TestWebKitAPI
