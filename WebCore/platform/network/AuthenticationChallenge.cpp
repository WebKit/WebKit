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
#include "config.h"
#include "AuthenticationChallenge.h"

#include "ResourceHandle.h"

namespace WebCore {

AuthenticationChallenge::AuthenticationChallenge()
    : m_isNull(true)
    , m_previousFailureCount(0)
{
}

AuthenticationChallenge::AuthenticationChallenge(const ProtectionSpace& protectionSpace, const Credential& proposedCredential, 
                                                       unsigned previousFailureCount, const ResourceResponse& response, const ResourceError& error)
    : m_isNull(false)
    , m_protectionSpace(protectionSpace)
    , m_proposedCredential(proposedCredential)
    , m_previousFailureCount(previousFailureCount)
    , m_failureResponse(response)
    , m_error(error)
{
}

unsigned AuthenticationChallenge::previousFailureCount() const 
{ 
    return m_previousFailureCount; 
}

const Credential& AuthenticationChallenge::proposedCredential() const 
{ 
    return m_proposedCredential; 
}

const ProtectionSpace& AuthenticationChallenge::protectionSpace() const 
{ 
    return m_protectionSpace; 
}

const ResourceResponse& AuthenticationChallenge::failureResponse() const 
{ 
    return m_failureResponse; 
}

const ResourceError& AuthenticationChallenge::error() const 
{ 
    return m_error; 
}

bool AuthenticationChallenge::isNull() const
{
    return m_isNull;
}

void AuthenticationChallenge::nullify()
{
    m_isNull = true;
}

bool operator==(const AuthenticationChallenge& a, const AuthenticationChallenge& b)
{
    if (a.isNull() != b.isNull())
        return false;
    if (a.isNull())
        return true;
        
#if PLATFORM(MAC)
    if (a.sender() != b.sender())
        return false;
        
    if (a.nsURLAuthenticationChallenge() != b.nsURLAuthenticationChallenge())
        return false;
#elif USE(CFNETWORK)
    if (a.sourceHandle() != b.sourceHandle())
        return false;

    if (a.cfURLAuthChallengeRef() != b.cfURLAuthChallengeRef())
        return false;
#endif

    if (a.protectionSpace() != b.protectionSpace())
        return false;
        
    if (a.proposedCredential() != b.proposedCredential())
        return false;
        
    if (a.previousFailureCount() != b.previousFailureCount())
        return false;
        
    if (a.failureResponse() != b.failureResponse())
        return false;
        
    if (a.error() != b.error())
        return false;
        
    return true;
}

bool operator!=(const AuthenticationChallenge& a, const AuthenticationChallenge& b)
{
    return !(a == b);
}

}


