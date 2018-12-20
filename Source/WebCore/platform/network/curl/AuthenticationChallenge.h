/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "AuthenticationChallengeBase.h"
#include "AuthenticationClient.h"

namespace WebCore {

class CurlResponse;

class WEBCORE_EXPORT AuthenticationChallenge final : public AuthenticationChallengeBase {
public:
    AuthenticationChallenge()
    {
    }

    AuthenticationChallenge(const ProtectionSpace& protectionSpace, const Credential& proposedCredential, unsigned previousFailureCount, const ResourceResponse& response, const ResourceError& error)
        : AuthenticationChallengeBase(protectionSpace, proposedCredential, previousFailureCount, response, error)
    {
    }

    AuthenticationChallenge(const CurlResponse&, unsigned, const ResourceResponse&, AuthenticationClient* = nullptr);
    AuthenticationClient* authenticationClient() const { return m_authenticationClient.get(); }

private:
    ProtectionSpaceServerType protectionSpaceServerTypeFromURI(const URL&, bool isForProxy);
    ProtectionSpace protectionSpaceFromHandle(const CurlResponse&, const ResourceResponse&);
    Optional<uint16_t> determineProxyPort(const URL&);
    ProtectionSpaceAuthenticationScheme authenticationSchemeFromCurlAuth(long);
    String parseRealm(const ResourceResponse&);
    void removeLeadingAndTrailingQuotes(String&);

    RefPtr<AuthenticationClient> m_authenticationClient;
};

} // namespace WebCore
