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

#include "config.h"
#include "CertificateInfo.h"

namespace WebCore {

#if PLATFORM(COCOA)
RetainPtr<CFArrayRef> CertificateInfo::certificateChainFromSecTrust(SecTrustRef trust)
{
    auto count = SecTrustGetCertificateCount(trust);
    auto certificateChain = CFArrayCreateMutable(0, count, &kCFTypeArrayCallBacks);
    for (CFIndex i = 0; i < count; i++)
        CFArrayAppendValue(certificateChain, SecTrustGetCertificateAtIndex(trust, i));
    return adoptCF((CFArrayRef)certificateChain);
}
#endif

CertificateInfo::Type CertificateInfo::type() const
{
#if HAVE(SEC_TRUST_SERIALIZATION)
    if (m_trust)
        return Type::Trust;
#endif
    if (m_certificateChain)
        return Type::CertificateChain;
    return Type::None;
}

CFArrayRef CertificateInfo::certificateChain() const
{
#if HAVE(SEC_TRUST_SERIALIZATION)
    if (m_certificateChain)
        return m_certificateChain.get();

    if (m_trust) 
        m_certificateChain = CertificateInfo::certificateChainFromSecTrust(m_trust.get());
#endif

    return m_certificateChain.get();
}

bool CertificateInfo::containsNonRootSHA1SignedCertificate() const
{
#if HAVE(SEC_TRUST_SERIALIZATION)
    if (m_trust) {
        // Allow only the root certificate (the last in the chain) to be SHA1.
        for (CFIndex i = 0, size = SecTrustGetCertificateCount(trust()) - 1; i < size; ++i) {
            auto certificate = SecTrustGetCertificateAtIndex(trust(), i);
            if (SecCertificateGetSignatureHashAlgorithm(certificate) == kSecSignatureHashAlgorithmSHA1)
                return true;
        }

        return false;
    }
#endif

#if PLATFORM(COCOA)
    if (m_certificateChain) {
        // Allow only the root certificate (the last in the chain) to be SHA1.
        for (CFIndex i = 0, size = CFArrayGetCount(m_certificateChain.get()) - 1; i < size; ++i) {
            auto certificate = checked_cf_cast<SecCertificateRef>(CFArrayGetValueAtIndex(m_certificateChain.get(), i));
            if (SecCertificateGetSignatureHashAlgorithm(certificate) == kSecSignatureHashAlgorithmSHA1)
                return true;
        }
        return false;
    }
#endif

    return false;
}

Optional<CertificateInfo::SummaryInfo> CertificateInfo::summaryInfo() const
{
    auto chain = certificateChain();
    if (!chain)
        return WTF::nullopt;

    SummaryInfo summaryInfo;

#if PLATFORM(COCOA) && !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(IOSMAC)
    auto leafCertificate = checked_cf_cast<SecCertificateRef>(CFArrayGetValueAtIndex(chain, 0));

    auto subjectCF = adoptCF(SecCertificateCopySubjectSummary(leafCertificate));
    summaryInfo.subject = subjectCF.get();
#endif

#if PLATFORM(MAC)
    if (auto certificateDictionary = adoptCF(SecCertificateCopyValues(leafCertificate, nullptr, nullptr))) {
        // CFAbsoluteTime is relative to 01/01/1970 00:00:00 GMT.
        const Seconds absoluteReferenceDate(978307200);

        if (auto validNotBefore = checked_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(certificateDictionary.get(), kSecOIDX509V1ValidityNotBefore))) {
            if (auto number = checked_cf_cast<CFNumberRef>(CFDictionaryGetValue(validNotBefore, CFSTR("value")))) {
                double numberValue;
                if (CFNumberGetValue(number, kCFNumberDoubleType, &numberValue))
                    summaryInfo.validFrom = absoluteReferenceDate + Seconds(numberValue);
            }
        }

        if (auto validNotAfter = checked_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(certificateDictionary.get(), kSecOIDX509V1ValidityNotAfter))) {
            if (auto number = checked_cf_cast<CFNumberRef>(CFDictionaryGetValue(validNotAfter, CFSTR("value")))) {
                double numberValue;
                if (CFNumberGetValue(number, kCFNumberDoubleType, &numberValue))
                    summaryInfo.validUntil = absoluteReferenceDate + Seconds(numberValue);
            }
        }

        if (auto dnsNames = checked_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(certificateDictionary.get(), CFSTR("DNSNAMES")))) {
            if (auto dnsNamesArray = checked_cf_cast<CFArrayRef>(CFDictionaryGetValue(dnsNames, CFSTR("value")))) {
                for (CFIndex i = 0, count = CFArrayGetCount(dnsNamesArray); i < count; ++i) {
                    if (auto dnsName = checked_cf_cast<CFStringRef>(CFArrayGetValueAtIndex(dnsNamesArray, i)))
                        summaryInfo.dnsNames.append(dnsName);
                }
            }
        }

        if (auto ipAddresses = checked_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(certificateDictionary.get(), CFSTR("IPADDRESSES")))) {
            if (auto ipAddressesArray = checked_cf_cast<CFArrayRef>(CFDictionaryGetValue(ipAddresses, CFSTR("value")))) {
                for (CFIndex i = 0, count = CFArrayGetCount(ipAddressesArray); i < count; ++i) {
                    if (auto ipAddress = checked_cf_cast<CFStringRef>(CFArrayGetValueAtIndex(ipAddressesArray, i)))
                        summaryInfo.ipAddresses.append(ipAddress);
                }
            }
        }
    }
#endif

    return summaryInfo;
}

} // namespace WebCore
