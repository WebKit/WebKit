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

typedef struct _GTlsClientConnection GTlsClientConnection;
typedef struct _GTlsPassword GTlsPassword;
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

    AuthenticationChallenge(const ProtectionSpace& protectionSpace, const Credential& proposedCredential, unsigned previousFailureCount, const ResourceResponse& response, const ResourceError& error, uint32_t tlsPasswordFlags)
        : AuthenticationChallengeBase(protectionSpace, proposedCredential, previousFailureCount, response, error)
        , m_tlsPasswordFlags(tlsPasswordFlags)
    {
    }

    AuthenticationChallenge(SoupMessage*, SoupAuth*, bool retrying);
    AuthenticationChallenge(SoupMessage*, GTlsClientConnection*);
    AuthenticationChallenge(SoupMessage*, GTlsPassword*);
    AuthenticationClient* authenticationClient() const { RELEASE_ASSERT_NOT_REACHED(); }
#if USE(SOUP2)
    SoupMessage* soupMessage() const { return m_soupMessage.get(); }
#endif
    SoupAuth* soupAuth() const { return m_soupAuth.get(); }
    GTlsPassword* tlsPassword() const { return m_tlsPassword.get(); }
    void setProposedCredential(const Credential& credential) { m_proposedCredential = credential; }

    uint32_t tlsPasswordFlags() const { return m_tlsPasswordFlags; }
    void setTLSPasswordFlags(uint32_t tlsPasswordFlags) { m_tlsPasswordFlags = tlsPasswordFlags; }

    static ProtectionSpace protectionSpaceForClientCertificate(const URL&);
    static ProtectionSpace protectionSpaceForClientCertificatePassword(const URL&, GTlsPassword*);

private:
    friend class AuthenticationChallengeBase;
    static bool platformCompare(const AuthenticationChallenge&, const AuthenticationChallenge&);

#if USE(SOUP2)
    GRefPtr<SoupMessage> m_soupMessage;
#endif
    GRefPtr<SoupAuth> m_soupAuth;
    GRefPtr<GTlsPassword> m_tlsPassword;
    uint32_t m_tlsPasswordFlags { 0 };
};

} // namespace WebCore

