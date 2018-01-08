/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "AuthenticationManager.h"

#if HAVE(SEC_IDENTITY)

#include <Security/SecIdentity.h>
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/CertificateInfo.h>
#include <WebCore/NotImplemented.h>
#include <wtf/cf/TypeCastsCF.h>

WTF_DECLARE_CF_TYPE_TRAIT(SecCertificate);

using namespace WebCore;

namespace WebKit {

static SecCertificateRef leafCertificate(const CertificateInfo& certificateInfo)
{
#if HAVE(SEC_TRUST_SERIALIZATION)
    if (certificateInfo.type() == CertificateInfo::Type::Trust)
        return SecTrustGetCertificateAtIndex(certificateInfo.trust(), 0);
#endif
    ASSERT(certificateInfo.type() == CertificateInfo::Type::CertificateChain);
    ASSERT(CFArrayGetCount(certificateInfo.certificateChain()));
    return checked_cf_cast<SecCertificateRef>(CFArrayGetValueAtIndex(certificateInfo.certificateChain(), 0));
}

static NSArray *chain(const CertificateInfo& certificateInfo)
{
#if HAVE(SEC_TRUST_SERIALIZATION)
    if (certificateInfo.type() == CertificateInfo::Type::Trust) {
        CFIndex count = SecTrustGetCertificateCount(certificateInfo.trust());
        if (count < 2)
            return nil;

        NSMutableArray *array = [NSMutableArray array];
        for (CFIndex i = 1; i < count; ++i)
            [array addObject:(id)SecTrustGetCertificateAtIndex(certificateInfo.trust(), i)];
            
        return array;
    }
#endif
    ASSERT(certificateInfo.type() == CertificateInfo::Type::CertificateChain);
    CFIndex chainCount = CFArrayGetCount(certificateInfo.certificateChain());
    return chainCount > 1 ? [(NSArray *)certificateInfo.certificateChain() subarrayWithRange:NSMakeRange(1, chainCount - 1)] : nil;
}

// FIXME: This function creates an identity from a certificate, which should not be needed. We should pass an identity over IPC (as we do on iOS).
bool AuthenticationManager::tryUseCertificateInfoForChallenge(const AuthenticationChallenge& challenge, const CertificateInfo& certificateInfo, ChallengeCompletionHandler& completionHandler)
{
    if (certificateInfo.isEmpty())
        return false;

    // The passed-in certificate chain includes the identity certificate at index 0, and additional certificates starting at index 1.
    SecIdentityRef identity;
    OSStatus result = SecIdentityCreateWithCertificate(NULL, leafCertificate(certificateInfo), &identity);
    if (result != errSecSuccess) {
        LOG_ERROR("Unable to create SecIdentityRef with certificate - %i", result);
        if (completionHandler)
            completionHandler(AuthenticationChallengeDisposition::Cancel, { });
        else
            [challenge.sender() cancelAuthenticationChallenge:challenge.nsURLAuthenticationChallenge()];
        return true;
    }

    NSURLCredential *credential = [NSURLCredential credentialWithIdentity:identity certificates:chain(certificateInfo) persistence:NSURLCredentialPersistenceNone];
    if (completionHandler)
        completionHandler(AuthenticationChallengeDisposition::UseCredential, Credential(credential));
    else
        [challenge.sender() useCredential:credential forAuthenticationChallenge:challenge.nsURLAuthenticationChallenge()];
    return true;
}

} // namespace WebKit

#endif // HAVE(SEC_IDENTITY)
