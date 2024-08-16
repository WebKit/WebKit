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
#include "LoginStatus.h"

#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringCommon.h>

namespace WebCore {

using CodeUnitMatchFunction = bool (*)(UChar);

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(LoginStatus);

ExceptionOr<UniqueRef<LoginStatus>> LoginStatus::create(const RegistrableDomain& domain, const String& username, CredentialTokenType tokenType, AuthenticationType authType)
{
    return create(domain, username, tokenType, authType, authType == AuthenticationType::Unmanaged ? TimeToLiveShort : TimeToLiveLong);
}

ExceptionOr<UniqueRef<LoginStatus>> LoginStatus::create(const RegistrableDomain& domain, const String& username, CredentialTokenType tokenType, AuthenticationType authType, Seconds timeToLive)
{
    if (domain.isEmpty())
        return Exception { ExceptionCode::SecurityError, "LoginStatus status can only be set for origins with a registrable domain."_s };

    if (username.isEmpty())
        return Exception { ExceptionCode::SyntaxError, "LoginStatus requires a non-empty username."_s };

    unsigned length = username.length();
    if (length > UsernameMaxLength)
        return Exception { ExceptionCode::SyntaxError, makeString("LoginStatus usernames cannot be longer than "_s, UsernameMaxLength) };

    auto spaceOrNewline = username.find([](UChar ch) {
        return deprecatedIsSpaceOrNewline(ch);
    });
    if (spaceOrNewline != notFound)
        return Exception { ExceptionCode::InvalidCharacterError, "LoginStatus usernames cannot contain whitespace or newlines."_s };

    return makeUniqueRef<LoginStatus>(*new LoginStatus(domain, username, tokenType, authType, timeToLive));
}

LoginStatus::LoginStatus(const RegistrableDomain& domain, const String& username, CredentialTokenType tokenType, AuthenticationType authType, Seconds timeToLive)
    : m_domain { domain }
    , m_username { username }
    , m_tokenType { tokenType }
    , m_authType { authType }
    , m_loggedInTime { WallTime::now() }
{
    setTimeToLive(timeToLive);
}

void LoginStatus::setTimeToLive(Seconds timeToLive)
{
    m_timeToLive = std::min(timeToLive, m_authType == AuthenticationType::Unmanaged ? TimeToLiveShort : TimeToLiveLong);
}

bool LoginStatus::hasExpired() const
{
    ASSERT(!m_domain.isEmpty() && !m_username.isEmpty());
    return WallTime::now() > m_loggedInTime + m_timeToLive;
}

WallTime LoginStatus::expiry() const
{
    return WallTime::now() + m_timeToLive;
}

}
