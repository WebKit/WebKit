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

#include <WebCore/LoggedInStatus.h>
#include <WebCore/RegistrableDomain.h>
#include <wtf/Seconds.h>
#include <wtf/text/StringBuilder.h>

using namespace WebCore;

namespace TestWebKitAPI {

// Positive test cases.

TEST(LoggedInStatus, DefaultExpiryWebAuthn)
{
    RegistrableDomain loginDomain { URL { { }, "https://login.example.com"_s } };

    auto loggedInResult = LoggedInStatus::create(loginDomain, "admin"_s, LoggedInStatus::CredentialTokenType::HTTPStateToken, LoggedInStatus::AuthenticationType::WebAuthn);
    auto& loggedIn = loggedInResult.returnValue().get();
    auto now = WallTime::now();
    ASSERT_EQ(loggedIn.registrableDomain().string(), "example.com"_s);
    ASSERT_EQ(loggedIn.username(), "admin"_s);
    ASSERT_EQ(loggedIn.credentialTokenType(), LoggedInStatus::CredentialTokenType::HTTPStateToken);
    ASSERT_EQ(loggedIn.authenticationType(), LoggedInStatus::AuthenticationType::WebAuthn);
    ASSERT_TRUE(loggedIn.loggedInTime() < now + 60_s);
    ASSERT_TRUE(loggedIn.loggedInTime() > now - 60_s);
    ASSERT_TRUE(loggedIn.expiry() < now + LoggedInStatus::TimeToLiveLong + 60_s);
    ASSERT_TRUE(loggedIn.expiry() > now + LoggedInStatus::TimeToLiveLong - 60_s);
    ASSERT_FALSE(loggedIn.hasExpired());
}

TEST(LoggedInStatus, DefaultExpiryPasswordManager)
{
    RegistrableDomain loginDomain { URL { { }, "https://login.example.com"_s } };

    auto loggedInResult = LoggedInStatus::create(loginDomain, "admin"_s, LoggedInStatus::CredentialTokenType::HTTPStateToken, LoggedInStatus::AuthenticationType::PasswordManager);
    auto& loggedIn = loggedInResult.returnValue().get();
    auto now = WallTime::now();
    ASSERT_EQ(loggedIn.authenticationType(), LoggedInStatus::AuthenticationType::PasswordManager);
    ASSERT_TRUE(loggedIn.expiry() < now + LoggedInStatus::TimeToLiveLong + 60_s);
    ASSERT_TRUE(loggedIn.expiry() > now + LoggedInStatus::TimeToLiveLong - 60_s);
    ASSERT_FALSE(loggedIn.hasExpired());
}

TEST(LoggedInStatus, DefaultExpiryUnmanaged)
{
    RegistrableDomain loginDomain { URL { { }, "https://login.example.com"_s } };

    auto loggedInResult = LoggedInStatus::create(loginDomain, "admin"_s, LoggedInStatus::CredentialTokenType::HTTPStateToken, LoggedInStatus::AuthenticationType::Unmanaged);
    auto& loggedIn = loggedInResult.returnValue().get();
    auto now = WallTime::now();
    ASSERT_EQ(loggedIn.authenticationType(), LoggedInStatus::AuthenticationType::Unmanaged);
    ASSERT_TRUE(loggedIn.expiry() < now + LoggedInStatus::TimeToLiveShort + 60_s);
    ASSERT_TRUE(loggedIn.expiry() > now + LoggedInStatus::TimeToLiveShort - 60_s);
    ASSERT_FALSE(loggedIn.hasExpired());
}

TEST(LoggedInStatus, CustomExpiryBelowLong)
{
    RegistrableDomain loginDomain { URL { { }, "https://login.example.com"_s } };

    auto customExpiry = LoggedInStatus::TimeToLiveLong - 48_h;
    auto loggedInResult = LoggedInStatus::create(loginDomain, "admin"_s, LoggedInStatus::CredentialTokenType::HTTPStateToken, LoggedInStatus::AuthenticationType::WebAuthn, customExpiry);
    auto& loggedIn = loggedInResult.returnValue().get();
    auto now = WallTime::now();
    ASSERT_EQ(loggedIn.authenticationType(), LoggedInStatus::AuthenticationType::WebAuthn);
    ASSERT_TRUE(loggedIn.expiry() < now + customExpiry + 60_s);
    ASSERT_TRUE(loggedIn.expiry() > now + customExpiry - 60_s);
    ASSERT_FALSE(loggedIn.hasExpired());
}

TEST(LoggedInStatus, CustomExpiryBelowShort)
{
    RegistrableDomain loginDomain { URL { { }, "https://login.example.com"_s } };

    auto customExpiry = LoggedInStatus::TimeToLiveShort - 48_h;
    auto loggedInResult = LoggedInStatus::create(loginDomain, "admin"_s, LoggedInStatus::CredentialTokenType::HTTPStateToken, LoggedInStatus::AuthenticationType::PasswordManager, customExpiry);
    auto& loggedIn = loggedInResult.returnValue().get();
    auto now = WallTime::now();
    ASSERT_EQ(loggedIn.authenticationType(), LoggedInStatus::AuthenticationType::PasswordManager);
    ASSERT_TRUE(loggedIn.expiry() < now + customExpiry + 60_s);
    ASSERT_TRUE(loggedIn.expiry() > now + customExpiry - 60_s);
    ASSERT_FALSE(loggedIn.hasExpired());
}

TEST(LoggedInStatus, RenewedExpiry)
{
    RegistrableDomain loginDomain { URL { { }, "https://login.example.com"_s } };

    auto customExpiry = LoggedInStatus::TimeToLiveShort - 48_h;
    auto loggedInResult = LoggedInStatus::create(loginDomain, "admin"_s, LoggedInStatus::CredentialTokenType::HTTPStateToken, LoggedInStatus::AuthenticationType::WebAuthn, customExpiry);
    auto loggedIn = loggedInResult.releaseReturnValue();
    auto now = WallTime::now();
    ASSERT_EQ(loggedIn->authenticationType(), LoggedInStatus::AuthenticationType::WebAuthn);
    ASSERT_TRUE(loggedIn->expiry() < now + customExpiry + 60_s);
    ASSERT_TRUE(loggedIn->expiry() > now + customExpiry - 60_s);
    ASSERT_FALSE(loggedIn->hasExpired());
    auto newCustomExpiry = 24_h;
    loggedIn->setTimeToLive(newCustomExpiry);
    ASSERT_EQ(loggedIn->authenticationType(), LoggedInStatus::AuthenticationType::WebAuthn);
    ASSERT_TRUE(loggedIn->expiry() < now + newCustomExpiry + 60_s);
    ASSERT_TRUE(loggedIn->expiry() > now + newCustomExpiry - 60_s);
    ASSERT_FALSE(loggedIn->hasExpired());
}

// Negative test cases.

TEST(LoggedInStatus, InvalidUsernames)
{
    RegistrableDomain loginDomain { URL { { }, "https://login.example.com"_s } };

    // Whitespace is not allowed.
    auto loggedInResult = LoggedInStatus::create(loginDomain, "I use spaces"_s, LoggedInStatus::CredentialTokenType::HTTPStateToken, LoggedInStatus::AuthenticationType::WebAuthn);
    ASSERT_TRUE(loggedInResult.hasException());

    // Newlines are not allowed.
    loggedInResult = LoggedInStatus::create(loginDomain, "Enter\nreturn"_s, LoggedInStatus::CredentialTokenType::HTTPStateToken, LoggedInStatus::AuthenticationType::WebAuthn);
    ASSERT_TRUE(loggedInResult.hasException());

    // There is a max length to usernames.
    StringBuilder builder;
    for (unsigned i = 0; i <= LoggedInStatus::UsernameMaxLength; ++i)
        builder.append('a');
    loggedInResult = LoggedInStatus::create(loginDomain, builder.toString(), LoggedInStatus::CredentialTokenType::HTTPStateToken, LoggedInStatus::AuthenticationType::WebAuthn);
    ASSERT_TRUE(loggedInResult.hasException());
}

TEST(LoggedInStatus, ClampedExpiryLong)
{
    RegistrableDomain loginDomain { URL { { }, "https://login.example.com"_s } };

    // Too long expiries for managed auth types should be clamped to LoggedInStatus::TimeToLiveLong.
    auto loggedInResult = LoggedInStatus::create(loginDomain, "admin"_s, LoggedInStatus::CredentialTokenType::HTTPStateToken, LoggedInStatus::AuthenticationType::WebAuthn, LoggedInStatus::TimeToLiveLong + 24_h);
    auto loggedIn = loggedInResult.releaseReturnValue();
    auto now = WallTime::now();
    ASSERT_EQ(loggedIn->authenticationType(), LoggedInStatus::AuthenticationType::WebAuthn);
    ASSERT_TRUE(loggedIn->expiry() < now + LoggedInStatus::TimeToLiveLong + 60_s);
    ASSERT_TRUE(loggedIn->expiry() > now + LoggedInStatus::TimeToLiveLong - 60_s);
    ASSERT_FALSE(loggedIn->hasExpired());

    // Renewed, too long expiries for managed auth types should also be clamped to LoggedInStatus::TimeToLiveLong.
    auto newCustomExpiry = LoggedInStatus::TimeToLiveLong + 24_h;
    loggedIn->setTimeToLive(newCustomExpiry);
    ASSERT_EQ(loggedIn->authenticationType(), LoggedInStatus::AuthenticationType::WebAuthn);
    ASSERT_TRUE(loggedIn->expiry() < now + LoggedInStatus::TimeToLiveLong + 60_s);
    ASSERT_TRUE(loggedIn->expiry() > now + LoggedInStatus::TimeToLiveLong - 60_s);
    ASSERT_FALSE(loggedIn->hasExpired());
}

TEST(LoggedInStatus, ClampedExpiryShort)
{
    RegistrableDomain loginDomain { URL { { }, "https://login.example.com"_s } };

    // Too long expiries for unmanaged auth types should be clamped to LoggedInStatus::TimeToLiveShort.
    auto loggedInResult = LoggedInStatus::create(loginDomain, "admin"_s, LoggedInStatus::CredentialTokenType::HTTPStateToken, LoggedInStatus::AuthenticationType::Unmanaged, LoggedInStatus::TimeToLiveShort + 24_h);
    auto loggedIn = loggedInResult.returnValue().get();
    auto now = WallTime::now();
    ASSERT_EQ(loggedIn.authenticationType(), LoggedInStatus::AuthenticationType::Unmanaged);
    ASSERT_TRUE(loggedIn.expiry() < now + LoggedInStatus::TimeToLiveShort + 60_s);
    ASSERT_TRUE(loggedIn.expiry() > now + LoggedInStatus::TimeToLiveShort - 60_s);
    ASSERT_FALSE(loggedIn.hasExpired());

    // Renewed, too long expiries for unmanaged auth types should also be clamped to LoggedInStatus::TimeToLiveShort.
    auto newCustomExpiry = LoggedInStatus::TimeToLiveShort + 24_h;
    loggedIn.setTimeToLive(newCustomExpiry);
    ASSERT_EQ(loggedIn.authenticationType(), LoggedInStatus::AuthenticationType::Unmanaged);
    ASSERT_TRUE(loggedIn.expiry() < now + LoggedInStatus::TimeToLiveShort + 60_s);
    ASSERT_TRUE(loggedIn.expiry() > now + LoggedInStatus::TimeToLiveShort - 60_s);
    ASSERT_FALSE(loggedIn.hasExpired());
}

} // namespace TestWebKitAPI
