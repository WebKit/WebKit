/*
 * Copyright (C) 2010-2021 Apple Inc. All rights reserved.
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
#include "CertificateInfo.h"

#include "CertificateSummary.h"
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>

namespace WebCore {

bool certificatesMatch(SecTrustRef trust1, SecTrustRef trust2)
{
    if (!trust1 || !trust2)
        return false;

    RetainPtr chain1 = adoptCF(SecTrustCopyCertificateChain(trust1));
    RetainPtr chain2 = adoptCF(SecTrustCopyCertificateChain(trust2));
    CFIndex count1 = chain1 ? CFArrayGetCount(chain1.get()) : 0;
    CFIndex count2 = chain2 ? CFArrayGetCount(chain2.get()) : 0;

    if (count1 != count2)
        return false;

    for (CFIndex i = 0; i < count1; ++i) {
        RetainPtr cert1 = CFArrayGetValueAtIndex(chain1.get(), i);
        RetainPtr cert2 = CFArrayGetValueAtIndex(chain2.get(), i);
        RELEASE_ASSERT(cert1);
        RELEASE_ASSERT(cert2);
        if (!CFEqual(cert1.get(), cert2.get()))
            return false;
    }

    return true;
}

RetainPtr<SecTrustRef> CertificateInfo::secTrustFromCertificateChain(CFArrayRef certificateChain)
{
    SecTrustRef trustRef = nullptr;
    if (SecTrustCreateWithCertificates(certificateChain, nullptr, &trustRef) != noErr)
        return nullptr;
    return adoptCF(trustRef);
}

RetainPtr<CFArrayRef> CertificateInfo::certificateChainFromSecTrust(SecTrustRef trust)
{
    return adoptCF(SecTrustCopyCertificateChain(trust));
}

bool CertificateInfo::containsNonRootSHA1SignedCertificate() const
{
    if (m_trust) {
        auto chain = adoptCF(SecTrustCopyCertificateChain(trust().get()));
        // Allow only the root certificate (the last in the chain) to be SHA1.
        for (CFIndex i = 0, size = SecTrustGetCertificateCount(trust().get()) - 1; i < size; ++i) {
            RetainPtr certificate = checked_cf_cast<SecCertificateRef>(CFArrayGetValueAtIndex(chain.get(), i));
            if (SecCertificateGetSignatureHashAlgorithm(certificate.get()) == kSecSignatureHashAlgorithmSHA1)
                return true;
        }

        return false;
    }

    return false;
}

std::optional<CertificateSummary> CertificateInfo::summary() const
{
    CertificateSummary summaryInfo;
    auto chain = certificateChainFromSecTrust(m_trust.get());
    if (!chain || !CFArrayGetCount(chain.get()))
        return std::nullopt;

#if !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST)
    RetainPtr leafCertificate = checked_cf_cast<SecCertificateRef>(CFArrayGetValueAtIndex(chain.get(), 0));
    RetainPtr subjectCF = adoptCF(SecCertificateCopySubjectSummary(leafCertificate.get()));
    summaryInfo.subject = subjectCF.get();
#endif

#if PLATFORM(MAC)
    if (auto certificateDictionary = adoptCF(SecCertificateCopyValues(leafCertificate.get(), nullptr, nullptr))) {
        if (RetainPtr validNotBefore = checked_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(certificateDictionary.get(), kSecOIDX509V1ValidityNotBefore))) {
            if (RetainPtr number = checked_cf_cast<CFNumberRef>(CFDictionaryGetValue(validNotBefore.get(), CFSTR("value")))) {
                double numberValue;
                if (CFNumberGetValue(number.get(), kCFNumberDoubleType, &numberValue))
                    summaryInfo.validFrom = Seconds(kCFAbsoluteTimeIntervalSince1970 + numberValue);
            }
        }

        if (RetainPtr validNotAfter = checked_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(certificateDictionary.get(), kSecOIDX509V1ValidityNotAfter))) {
            if (RetainPtr number = checked_cf_cast<CFNumberRef>(CFDictionaryGetValue(validNotAfter.get(), CFSTR("value")))) {
                double numberValue;
                if (CFNumberGetValue(number.get(), kCFNumberDoubleType, &numberValue))
                    summaryInfo.validUntil = Seconds(kCFAbsoluteTimeIntervalSince1970 + numberValue);
            }
        }

        if (RetainPtr dnsNames = checked_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(certificateDictionary.get(), CFSTR("DNSNAMES")))) {
            if (RetainPtr dnsNamesArray = checked_cf_cast<CFArrayRef>(CFDictionaryGetValue(dnsNames.get(), CFSTR("value")))) {
                for (CFIndex i = 0, count = CFArrayGetCount(dnsNamesArray.get()); i < count; ++i) {
                    if (RetainPtr dnsName = checked_cf_cast<CFStringRef>(CFArrayGetValueAtIndex(dnsNamesArray.get(), i)))
                        summaryInfo.dnsNames.append(dnsName.get());
                }
            }
        }

        if (RetainPtr ipAddresses = checked_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(certificateDictionary.get(), CFSTR("IPADDRESSES")))) {
            if (RetainPtr ipAddressesArray = checked_cf_cast<CFArrayRef>(CFDictionaryGetValue(ipAddresses.get(), CFSTR("value")))) {
                for (CFIndex i = 0, count = CFArrayGetCount(ipAddressesArray.get()); i < count; ++i) {
                    if (RetainPtr ipAddress = checked_cf_cast<CFStringRef>(CFArrayGetValueAtIndex(ipAddressesArray.get(), i)))
                        summaryInfo.ipAddresses.append(ipAddress.get());
                }
            }
        }
    }
#endif // PLATFORM(MAC)
    return summaryInfo;
}

} // namespace WTF::Persistence
