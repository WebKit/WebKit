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

#include "config.h"
#include "AuthenticationCF.h"

#if USE(CFURLCONNECTION)

#include "AuthenticationChallenge.h"
#include "AuthenticationClient.h"
#include "Credential.h"
#include "ProtectionSpace.h"
#include <CFNetwork/CFURLAuthChallengePriv.h>
#include <CFNetwork/CFURLCredentialPriv.h>
#include <CFNetwork/CFURLProtectionSpacePriv.h>

namespace WebCore {

AuthenticationChallenge::AuthenticationChallenge(const ProtectionSpace& protectionSpace, const Credential& proposedCredential, unsigned previousFailureCount, const ResourceResponse& response, const ResourceError& error)
    : AuthenticationChallengeBase(protectionSpace, proposedCredential, previousFailureCount, response, error)
{
}

AuthenticationChallenge::AuthenticationChallenge(CFURLAuthChallengeRef cfChallenge, AuthenticationClient* authenticationClient)
    : AuthenticationChallengeBase(core(CFURLAuthChallengeGetProtectionSpace(cfChallenge)), core(CFURLAuthChallengeGetProposedCredential(cfChallenge)),
        CFURLAuthChallengeGetPreviousFailureCount(cfChallenge), (CFURLResponseRef)CFURLAuthChallengeGetFailureResponse(cfChallenge), CFURLAuthChallengeGetError(cfChallenge))
    , m_authenticationClient(authenticationClient)
    , m_cfChallenge(cfChallenge)
{
}

void AuthenticationChallenge::setAuthenticationClient(AuthenticationClient* client)
{
    m_authenticationClient = client;
}

AuthenticationClient* AuthenticationChallenge::authenticationClient() const
{
    return m_authenticationClient.get();
}

bool AuthenticationChallenge::platformCompare(const AuthenticationChallenge& a, const AuthenticationChallenge& b)
{
    if (a.authenticationClient() != b.authenticationClient())
        return false;

    if (a.cfURLAuthChallengeRef() != b.cfURLAuthChallengeRef())
        return false;
        
    return true;
}

RetainPtr<CFURLAuthChallengeRef> createCF(const AuthenticationChallenge& coreChallenge)
{
    // FIXME: Why not cache CFURLAuthChallengeRef in m_cfChallenge? Foundation counterpart does that.

    auto credential = createCF(coreChallenge.proposedCredential());
    auto protectionSpace = createCF(coreChallenge.protectionSpace());
    return adoptCF(CFURLAuthChallengeCreate(0, protectionSpace.get(), credential.get(), coreChallenge.previousFailureCount(), coreChallenge.failureResponse().cfURLResponse(), coreChallenge.error()));
}

RetainPtr<CFURLCredentialRef> createCF(const Credential& coreCredential)
{
    CFURLCredentialPersistence persistence = kCFURLCredentialPersistenceNone;
    switch (coreCredential.persistence()) {
    case CredentialPersistenceNone:
        break;
    case CredentialPersistenceForSession:
        persistence = kCFURLCredentialPersistenceForSession;
        break;
    case CredentialPersistencePermanent:
        persistence = kCFURLCredentialPersistencePermanent;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    
    return adoptCF(CFURLCredentialCreate(0, coreCredential.user().createCFString().get(), coreCredential.password().createCFString().get(), 0, persistence));
}

RetainPtr<CFURLProtectionSpaceRef> createCF(const ProtectionSpace& coreSpace)
{
    CFURLProtectionSpaceServerType serverType = kCFURLProtectionSpaceServerHTTP;
    switch (coreSpace.serverType()) {
    case ProtectionSpace::ServerType::HTTP:
        serverType = kCFURLProtectionSpaceServerHTTP;
        break;
    case ProtectionSpace::ServerType::HTTPS:
        serverType = kCFURLProtectionSpaceServerHTTPS;
        break;
    case ProtectionSpace::ServerType::FTP:
        serverType = kCFURLProtectionSpaceServerFTP;
        break;
    case ProtectionSpace::ServerType::FTPS:
        serverType = kCFURLProtectionSpaceServerFTPS;
        break;
    case ProtectionSpace::ServerType::ProxyHTTP:
        serverType = kCFURLProtectionSpaceProxyHTTP;
        break;
    case ProtectionSpace::ServerType::ProxyHTTPS:
        serverType = kCFURLProtectionSpaceProxyHTTPS;
        break;
    case ProtectionSpace::ServerType::ProxyFTP:
        serverType = kCFURLProtectionSpaceProxyFTP;
        break;
    case ProtectionSpace::ServerType::ProxySOCKS:
        serverType = kCFURLProtectionSpaceProxySOCKS;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    CFURLProtectionSpaceAuthenticationScheme scheme = kCFURLProtectionSpaceAuthenticationSchemeDefault;
    switch (coreSpace.authenticationScheme()) {
    case ProtectionSpace::AuthenticationScheme::Default:
        scheme = kCFURLProtectionSpaceAuthenticationSchemeDefault;
        break;
    case ProtectionSpace::AuthenticationScheme::HTTPBasic:
        scheme = kCFURLProtectionSpaceAuthenticationSchemeHTTPBasic;
        break;
    case ProtectionSpace::AuthenticationScheme::HTTPDigest:
        scheme = kCFURLProtectionSpaceAuthenticationSchemeHTTPDigest;
        break;
    case ProtectionSpace::AuthenticationScheme::HTMLForm:
        scheme = kCFURLProtectionSpaceAuthenticationSchemeHTMLForm;
        break;
    case ProtectionSpace::AuthenticationScheme::NTLM:
        scheme = kCFURLProtectionSpaceAuthenticationSchemeNTLM;
        break;
    case ProtectionSpace::AuthenticationScheme::Negotiate:
        scheme = kCFURLProtectionSpaceAuthenticationSchemeNegotiate;
        break;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    case ProtectionSpace::AuthenticationScheme::ServerTrustEvaluationRequested:
        scheme = kCFURLProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested;
        break;
    case ProtectionSpace::AuthenticationScheme::ClientCertificateRequested:
        scheme = kCFURLProtectionSpaceAuthenticationSchemeClientCertificateRequested;
        break;
#endif
    default:
        ASSERT_NOT_REACHED();
    }

    return adoptCF(CFURLProtectionSpaceCreate(0, coreSpace.host().createCFString().get(), coreSpace.port(), serverType, coreSpace.realm().createCFString().get(), scheme));
}

Credential core(CFURLCredentialRef cfCredential)
{
    if (!cfCredential)
        return Credential();

    CredentialPersistence persistence = CredentialPersistenceNone;
    switch (CFURLCredentialGetPersistence(cfCredential)) {
    case kCFURLCredentialPersistenceNone:
        break;
    case kCFURLCredentialPersistenceForSession:
        persistence = CredentialPersistenceForSession;
        break;
    case kCFURLCredentialPersistencePermanent:
        persistence = CredentialPersistencePermanent;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    RetainPtr<CFStringRef> password = adoptCF(CFURLCredentialCopyPassword(cfCredential));
    return Credential(CFURLCredentialGetUsername(cfCredential), password.get(), persistence);
}

ProtectionSpace core(CFURLProtectionSpaceRef cfSpace)
{
    auto serverType = ProtectionSpace::ServerType::HTTP;
    
    switch (CFURLProtectionSpaceGetServerType(cfSpace)) {
    case kCFURLProtectionSpaceServerHTTP:
        break;
    case kCFURLProtectionSpaceServerHTTPS:
        serverType = ProtectionSpace::ServerType::HTTPS;
        break;
    case kCFURLProtectionSpaceServerFTP:
        serverType = ProtectionSpace::ServerType::FTP;
        break;
    case kCFURLProtectionSpaceServerFTPS:
        serverType = ProtectionSpace::ServerType::FTPS;
        break;
    case kCFURLProtectionSpaceProxyHTTP:
        serverType = ProtectionSpace::ServerType::ProxyHTTP;
        break;
    case kCFURLProtectionSpaceProxyHTTPS:
        serverType = ProtectionSpace::ServerType::ProxyHTTPS;
        break;
    case kCFURLProtectionSpaceProxyFTP:
        serverType = ProtectionSpace::ServerType::ProxyFTP;
        break;
    case kCFURLProtectionSpaceProxySOCKS:
        serverType = ProtectionSpace::ServerType::ProxySOCKS;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    auto scheme = ProtectionSpace::AuthenticationScheme::Default;
    
    switch (CFURLProtectionSpaceGetAuthenticationScheme(cfSpace)) {
    case kCFURLProtectionSpaceAuthenticationSchemeDefault:
        scheme = ProtectionSpace::AuthenticationScheme::Default;
        break;
    case kCFURLProtectionSpaceAuthenticationSchemeHTTPBasic:
        scheme = ProtectionSpace::AuthenticationScheme::HTTPBasic;
        break;
    case kCFURLProtectionSpaceAuthenticationSchemeHTTPDigest:
        scheme = ProtectionSpace::AuthenticationScheme::HTTPDigest;
        break;
    case kCFURLProtectionSpaceAuthenticationSchemeHTMLForm:
        scheme = ProtectionSpace::AuthenticationScheme::HTMLForm;
        break;
    case kCFURLProtectionSpaceAuthenticationSchemeNTLM:
        scheme = ProtectionSpace::AuthenticationScheme::NTLM;
        break;
    case kCFURLProtectionSpaceAuthenticationSchemeNegotiate:
        scheme = ProtectionSpace::AuthenticationScheme::Negotiate;
        break;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    case kCFURLProtectionSpaceAuthenticationSchemeClientCertificateRequested:
        scheme = ProtectionSpace::AuthenticationScheme::ClientCertificateRequested;
        break;
    case kCFURLProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested:
        scheme = ProtectionSpace::AuthenticationScheme::ServerTrustEvaluationRequested;
        break;
#endif
    default:
        scheme = ProtectionSpace::AuthenticationScheme::Unknown;
        ASSERT_NOT_REACHED();
    }
        
    return ProtectionSpace(CFURLProtectionSpaceGetHost(cfSpace), 
                           CFURLProtectionSpaceGetPort(cfSpace),
                           serverType,
                           CFURLProtectionSpaceGetRealm(cfSpace),
                           scheme);
}

}

#endif // USE(CFURLCONNECTION)
