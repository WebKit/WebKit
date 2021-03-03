/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

typedef struct _SoupAuth SoupAuth;
typedef struct _SoupMessage SoupMessage;

namespace WebCore {

class AuthenticationChallenge final : public AuthenticationChallengeBase {
public:
    AuthenticationChallenge()
    {
    }

    AuthenticationChallenge(const ProtectionSpace& protectionSpace, const Credential& proposedCredential, unsigned previousFailureCount, const ResourceResponse& response, const ResourceError& error)
        : AuthenticationChallengeBase(protectionSpace, proposedCredential, previousFailureCount, response, error)
    {
    }

    AuthenticationChallenge(SoupMessage*, SoupAuth*, bool retrying);
    AuthenticationClient* authenticationClient() const { return nullptr; }
#if USE(SOUP2)
    SoupMessage* soupMessage() const { return m_soupMessage.get(); }
#endif
    SoupAuth* soupAuth() const { return m_soupAuth.get(); }
    void setProposedCredential(const Credential& credential) { m_proposedCredential = credential; }

private:
    friend class AuthenticationChallengeBase;
    static bool platformCompare(const AuthenticationChallenge&, const AuthenticationChallenge&);

#if USE(SOUP2)
    GRefPtr<SoupMessage> m_soupMessage;
#endif
    GRefPtr<SoupAuth> m_soupAuth;
};

} // namespace WebCore

