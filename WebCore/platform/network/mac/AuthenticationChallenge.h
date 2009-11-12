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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
#ifndef AuthenticationChallenge_h
#define AuthenticationChallenge_h

#include "AuthenticationChallengeBase.h"

#include <wtf/RetainPtr.h>
#ifndef __OBJC__
typedef struct objc_object *id;
class NSURLAuthenticationChallenge;
#else
@class NSURLAuthenticationChallenge;
#endif

namespace WebCore {

class AuthenticationClient;

class AuthenticationChallenge : public AuthenticationChallengeBase {
public:
    AuthenticationChallenge() { }
    AuthenticationChallenge(const ProtectionSpace& protectionSpace, const Credential& proposedCredential, unsigned previousFailureCount, const ResourceResponse& response, const ResourceError& error);
    AuthenticationChallenge(NSURLAuthenticationChallenge *);

    id sender() const { return m_sender.get(); }
    NSURLAuthenticationChallenge *nsURLAuthenticationChallenge() const { return m_nsChallenge.get(); }

    void setAuthenticationClient(AuthenticationClient*); // Changes sender to one that invokes client methods.

private:
    friend class AuthenticationChallengeBase;
    static bool platformCompare(const AuthenticationChallenge& a, const AuthenticationChallenge& b);

    RetainPtr<id> m_sender; // Always the same as [m_macChallenge.get() sender], cached here for performance.
    RetainPtr<NSURLAuthenticationChallenge *> m_nsChallenge;
};

}

#endif
