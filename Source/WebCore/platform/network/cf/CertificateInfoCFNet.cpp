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

#include "CertificateSummary.h"
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>

namespace WebCore {

bool certificatesMatch(SecTrustRef trust1, SecTrustRef trust2)
{
    if (!trust1 || !trust2)
        return false;

    CFIndex count1 = SecTrustGetCertificateCount(trust1);
    CFIndex count2 = SecTrustGetCertificateCount(trust2);
    if (count1 != count2)
        return false;

    for (CFIndex i = 0; i < count1; i++) {
        auto cert1 = SecTrustGetCertificateAtIndex(trust1, i);
        auto cert2 = SecTrustGetCertificateAtIndex(trust2, i);
        RELEASE_ASSERT(cert1);
        RELEASE_ASSERT(cert2);
        if (!CFEqual(cert1, cert2))
            return false;
    }

    return true;
}

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

Optional<CertificateSummary> CertificateInfo::summary() const
{
    auto chain = certificateChain();
    if (!chain)
        return WTF::nullopt;

    CertificateSummary summaryInfo;

#if PLATFORM(COCOA) && !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST)
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

namespace WTF {
namespace Persistence {

static void encodeCFData(Encoder& encoder, CFDataRef data)
{
    uint64_t length = CFDataGetLength(data);
    const uint8_t* bytePtr = CFDataGetBytePtr(data);

    encoder << length;
    encoder.encodeFixedLengthData(bytePtr, static_cast<size_t>(length));
}

static Optional<RetainPtr<CFDataRef>> decodeCFData(Decoder& decoder)
{
    Optional<uint64_t> size;
    decoder >> size;
    if (!size)
        return WTF::nullopt;

    if (UNLIKELY(!isInBounds<size_t>(*size)))
        return WTF::nullopt;
    
    Vector<uint8_t> vector(static_cast<size_t>(*size));
    if (!decoder.decodeFixedLengthData(vector.data(), vector.size()))
        return WTF::nullopt;

    return adoptCF(CFDataCreate(nullptr, vector.data(), vector.size()));
}

#if HAVE(SEC_TRUST_SERIALIZATION)
static void encodeSecTrustRef(Encoder& encoder, SecTrustRef trust)
{
    auto data = adoptCF(SecTrustSerialize(trust, nullptr));
    if (!data) {
        encoder << false;
        return;
    }

    encoder << true;
    encodeCFData(encoder, data.get());
}

static Optional<RetainPtr<SecTrustRef>> decodeSecTrustRef(Decoder& decoder)
{
    Optional<bool> hasTrust;
    decoder >> hasTrust;
    if (!hasTrust)
        return WTF::nullopt;

    if (!*hasTrust)
        return { nullptr };

    auto trustData = decodeCFData(decoder);
    if (!trustData)
        return WTF::nullopt;

    auto trust = adoptCF(SecTrustDeserialize(trustData->get(), nullptr));
    if (!trust)
        return WTF::nullopt;

    return trust;
}
#endif

#if PLATFORM(COCOA)
static void encodeCertificateChain(Encoder& encoder, CFArrayRef certificateChain)
{
    CFIndex size = CFArrayGetCount(certificateChain);
    Vector<CFTypeRef, 32> values(size);

    CFArrayGetValues(certificateChain, CFRangeMake(0, size), values.data());

    encoder << static_cast<uint64_t>(size);

    for (CFIndex i = 0; i < size; ++i) {
        ASSERT(values[i]);
        auto data = adoptCF(SecCertificateCopyData(checked_cf_cast<SecCertificateRef>(values[i])));
        encodeCFData(encoder, data.get());
    }
}

static Optional<RetainPtr<CFArrayRef>> decodeCertificateChain(Decoder& decoder)
{
    Optional<uint64_t> size;
    decoder >> size;
    if (!size)
        return WTF::nullopt;

    auto array = adoptCF(CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks));

    for (size_t i = 0; i < *size; ++i) {
        auto data = decodeCFData(decoder);
        if (!data)
            return WTF::nullopt;

        auto certificate = adoptCF(SecCertificateCreateWithData(0, data->get()));
        CFArrayAppendValue(array.get(), certificate.get());
    }

    return { WTFMove(array) };
}
#endif

void Coder<WebCore::CertificateInfo>::encode(Encoder& encoder, const WebCore::CertificateInfo& certificateInfo)
{
    encoder << certificateInfo.type();

    switch (certificateInfo.type()) {
#if HAVE(SEC_TRUST_SERIALIZATION)
    case WebCore::CertificateInfo::Type::Trust:
        encodeSecTrustRef(encoder, certificateInfo.trust());
        break;
#endif
#if PLATFORM(COCOA)
    case WebCore::CertificateInfo::Type::CertificateChain: {
        encodeCertificateChain(encoder, certificateInfo.certificateChain());
        break;
    }
#endif
    case WebCore::CertificateInfo::Type::None:
        // Do nothing.
        break;
    }
}

Optional<WebCore::CertificateInfo> Coder<WebCore::CertificateInfo>::decode(Decoder& decoder)
{
    Optional<WebCore::CertificateInfo::Type> certificateInfoType;
    decoder >> certificateInfoType;
    if (!certificateInfoType)
        return WTF::nullopt;

    switch (*certificateInfoType) {
#if HAVE(SEC_TRUST_SERIALIZATION)
    case WebCore::CertificateInfo::Type::Trust: {
        auto trust = decodeSecTrustRef(decoder);
        if (!trust)
            return WTF::nullopt;

        return WebCore::CertificateInfo(WTFMove(*trust));
    }
#endif
#if PLATFORM(COCOA)
    case WebCore::CertificateInfo::Type::CertificateChain: {
        auto certificateChain = decodeCertificateChain(decoder);
        if (!certificateChain)
            return WTF::nullopt;

        return WebCore::CertificateInfo(WTFMove(*certificateChain));
    }
#endif
    case WebCore::CertificateInfo::Type::None:
        // Do nothing.
        return WebCore::CertificateInfo();
    }

    return WTF::nullopt;
}

} // namespace Persistence
} // namespace WTF
