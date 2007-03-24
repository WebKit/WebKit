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

#include "Credential.h"
#include "ProtectionSpace.h"
#include "ResourceResponse.h"
#include "ResourceError.h"

#include <wtf/RefPtr.h>


#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
#ifndef __OBJC__
typedef struct objc_object *id;
class NSURLAuthenticationChallenge;
#else
@class NSURLAuthenticationChallenge;
#endif
#endif

#if USE(CFNETWORK)
typedef struct _CFURLAuthChallenge* CFURLAuthChallengeRef;
#endif


namespace WebCore {

class ResourceHandle;

class AuthenticationChallenge {

public:
    AuthenticationChallenge();
    AuthenticationChallenge(const ProtectionSpace& protectionSpace, const Credential& proposedCredential, unsigned previousFailureCount, const ResourceResponse& response, const ResourceError& error);
#if PLATFORM(MAC)
    AuthenticationChallenge(NSURLAuthenticationChallenge *);
#elif USE(CFNETWORK)
    AuthenticationChallenge(CFURLAuthChallengeRef, ResourceHandle* sourceHandle);
#endif

    unsigned previousFailureCount() const;
    const Credential& proposedCredential() const;
    const ProtectionSpace& protectionSpace() const;
    const ResourceResponse& failureResponse() const;
    const ResourceError& error() const;
    
    bool isNull() const;
    void nullify();
    
#if PLATFORM(MAC)
    id sender() const { return m_sender.get(); }
    NSURLAuthenticationChallenge *nsURLAuthenticationChallenge() const { return m_macChallenge.get(); }
#elif USE(CFNETWORK)
    ResourceHandle* sourceHandle() const { return m_sourceHandle.get(); }
    CFURLAuthChallengeRef cfURLAuthChallengeRef() const { return m_cfChallenge.get(); }
#endif
private:
    bool m_isNull;
    ProtectionSpace m_protectionSpace;
    Credential m_proposedCredential;
    unsigned m_previousFailureCount;
    ResourceResponse m_failureResponse;
    ResourceError m_error;

#if PLATFORM(MAC)
    RetainPtr<id> m_sender;
    RetainPtr<NSURLAuthenticationChallenge *> m_macChallenge;
#elif USE(CFNETWORK)
    RefPtr<ResourceHandle> m_sourceHandle;
    RetainPtr<CFURLAuthChallengeRef> m_cfChallenge;
#endif

};

bool operator==(const AuthenticationChallenge&, const AuthenticationChallenge&);
bool operator!=(const AuthenticationChallenge&, const AuthenticationChallenge&);
}
#endif

