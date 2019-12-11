/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "WebCredential.h"

#if PLATFORM(MAC)

#include "WebCertificateInfo.h"
#include <Security/SecIdentity.h>
#include <WebCore/CertificateInfo.h>
#include <wtf/cf/TypeCastsCF.h>

namespace WebKit {
using namespace WebCore;

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
    return chainCount > 1 ? [(__bridge NSArray *)certificateInfo.certificateChain() subarrayWithRange:NSMakeRange(1, chainCount - 1)] : nil;
}

WebCredential::WebCredential(WebCertificateInfo* certificateInfo)
{
    if (!certificateInfo || certificateInfo->certificateInfo().isEmpty())
        return;

    // The passed-in certificate chain includes the identity certificate at index 0, and additional certificates starting at index 1.
    SecIdentityRef identity;
    OSStatus result = SecIdentityCreateWithCertificate(NULL, leafCertificate(certificateInfo->certificateInfo()), &identity);
    if (result != errSecSuccess) {
        LOG_ERROR("Unable to create SecIdentityRef with certificate - %i", result);
        return;
    }

    m_coreCredential = Credential([NSURLCredential credentialWithIdentity:identity certificates:chain(certificateInfo->certificateInfo()) persistence:NSURLCredentialPersistenceNone]);
}

} // namespace WebKit

#endif // PLATFORM(MAC)
