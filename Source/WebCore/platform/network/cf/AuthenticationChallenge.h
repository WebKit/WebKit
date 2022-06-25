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
#include <wtf/RefPtr.h>

#if USE(CFURLCONNECTION)
typedef struct _CFURLAuthChallenge* CFURLAuthChallengeRef;
#else
OBJC_CLASS NSURLAuthenticationChallenge;
#endif

namespace WebCore {

class AuthenticationChallenge : public AuthenticationChallengeBase {
public:
    AuthenticationChallenge() {}
    WEBCORE_EXPORT AuthenticationChallenge(const ProtectionSpace&, const Credential& proposedCredential, unsigned previousFailureCount, const ResourceResponse&, const ResourceError&);

#if USE(CFURLCONNECTION)
    AuthenticationChallenge(CFURLAuthChallengeRef, AuthenticationClient*);

    CFURLAuthChallengeRef cfURLAuthChallengeRef() const { return m_cfChallenge.get(); }
#else
    WEBCORE_EXPORT AuthenticationChallenge(NSURLAuthenticationChallenge *);

    id sender() const { return m_sender.get(); }
    NSURLAuthenticationChallenge *nsURLAuthenticationChallenge() const { return m_nsChallenge.get(); }
#endif

    WEBCORE_EXPORT void setAuthenticationClient(AuthenticationClient*); // Changes sender to one that invokes client methods.
    WEBCORE_EXPORT AuthenticationClient* authenticationClient() const;

private:
    friend class AuthenticationChallengeBase;
    static bool platformCompare(const AuthenticationChallenge& a, const AuthenticationChallenge& b);

    // Platform challenge may be null. If it's non-null, it's always up to date with other fields.
#if USE(CFURLCONNECTION)
    RefPtr<AuthenticationClient> m_authenticationClient;
    RetainPtr<CFURLAuthChallengeRef> m_cfChallenge;
#else
    RetainPtr<id> m_sender;
    RetainPtr<NSURLAuthenticationChallenge> m_nsChallenge;
#endif
};

}
