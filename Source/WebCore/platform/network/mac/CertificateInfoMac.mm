/*
 * Copyright (C) 2010, 2015 Apple Inc. All rights reserved.
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

#import "config.h"
#import "CertificateInfo.h"

#import "NotImplemented.h"
#import <wtf/spi/cocoa/SecuritySPI.h>

namespace WebCore {

bool CertificateInfo::containsNonRootSHA1SignedCertificate() const
{
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100) || PLATFORM(IOS)
    if (!m_certificateChain)
        return false;

    for (CFIndex i = 0, size = CFArrayGetCount(m_certificateChain.get()) - 1; i < size; ++i) {
        SecCertificateRef certificate = (SecCertificateRef)CFArrayGetValueAtIndex(m_certificateChain.get(), i);
        if (SecCertificateGetSignatureHashAlgorithm(certificate) == kSecSignatureHashAlgorithmSHA1)
            return true;
    }
    return false;
#else
    notImplemented();
    return false;
#endif
}

#ifndef NDEBUG
void CertificateInfo::dump() const
{
    unsigned entries = m_certificateChain ? CFArrayGetCount(m_certificateChain.get()) : 0;

    NSLog(@"CertificateInfo\n");
    NSLog(@"  Entries: %d\n", entries);
    for (unsigned i = 0; i < entries; ++i) {
        RetainPtr<CFStringRef> summary = adoptCF(SecCertificateCopySubjectSummary((SecCertificateRef)CFArrayGetValueAtIndex(m_certificateChain.get(), i)));
        NSLog(@"  %@", (NSString *)summary.get());
    }
}
#endif

} // namespace WebCore
