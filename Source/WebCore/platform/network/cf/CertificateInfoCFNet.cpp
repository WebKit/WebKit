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

#if PLATFORM(COCOA)
bool certificatesMatch(SecTrustRef trust1, SecTrustRef trust2)
{
    if (!trust1 || !trust2)
        return false;

#if HAVE(SEC_TRUST_COPY_CERTIFICATE_CHAIN)
    auto chain1 = adoptCF(SecTrustCopyCertificateChain(trust1));
    auto chain2 = adoptCF(SecTrustCopyCertificateChain(trust2));
#endif

    CFIndex count1 = SecTrustGetCertificateCount(trust1);
    CFIndex count2 = SecTrustGetCertificateCount(trust2);
    if (count1 != count2)
        return false;

    for (CFIndex i = 0; i < count1; i++) {
#if HAVE(SEC_TRUST_COPY_CERTIFICATE_CHAIN)
        auto cert1 = CFArrayGetValueAtIndex(chain1.get(), i);
        auto cert2 = CFArrayGetValueAtIndex(chain2.get(), i);
#else
        auto cert1 = SecTrustGetCertificateAtIndex(trust1, i);
        auto cert2 = SecTrustGetCertificateAtIndex(trust2, i);
#endif
        RELEASE_ASSERT(cert1);
        RELEASE_ASSERT(cert2);
        if (!CFEqual(cert1, cert2))
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
#if HAVE(SEC_TRUST_COPY_CERTIFICATE_CHAIN)
    return adoptCF(SecTrustCopyCertificateChain(trust));
#else
    auto count = SecTrustGetCertificateCount(trust);
    auto certificateChain = adoptCF(CFArrayCreateMutable(0, count, &kCFTypeArrayCallBacks));
    for (CFIndex i = 0; i < count; i++)
        CFArrayAppendValue(certificateChain.get(), SecTrustGetCertificateAtIndex(trust, i));
    return certificateChain;
#endif
}
#endif

bool CertificateInfo::containsNonRootSHA1SignedCertificate() const
{
#if PLATFORM(COCOA)
    if (m_trust) {
#if HAVE(SEC_TRUST_COPY_CERTIFICATE_CHAIN)
        auto chain = adoptCF(SecTrustCopyCertificateChain(trust()));
#endif
        // Allow only the root certificate (the last in the chain) to be SHA1.
        for (CFIndex i = 0, size = SecTrustGetCertificateCount(trust()) - 1; i < size; ++i) {
#if HAVE(SEC_TRUST_COPY_CERTIFICATE_CHAIN)
            auto certificate = checked_cf_cast<SecCertificateRef>(CFArrayGetValueAtIndex(chain.get(), i));
#else
            auto certificate = SecTrustGetCertificateAtIndex(trust(), i);
#endif
            if (SecCertificateGetSignatureHashAlgorithm(certificate) == kSecSignatureHashAlgorithmSHA1)
                return true;
        }

        return false;
    }
#endif
    return false;
}

std::optional<CertificateSummary> CertificateInfo::summary() const
{
    CertificateSummary summaryInfo;
#if PLATFORM(COCOA)
    auto chain = certificateChainFromSecTrust(m_trust.get());
    if (!chain || !CFArrayGetCount(chain.get()))
        return std::nullopt;

#if !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST)
    auto leafCertificate = checked_cf_cast<SecCertificateRef>(CFArrayGetValueAtIndex(chain.get(), 0));
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
#endif // PLATFORM(MAC)
#endif // PLATFORM(COCOA)
    return summaryInfo;
}

// FIXME: Remove this when WebKit::NetworkCache::Storage::version is incremented.
enum class LegacyCertificateInfoType {
    None,
    CertificateChain,
#if PLATFORM(COCOA)
    Trust,
#endif
};

} // namespace WebCore

namespace WTF {
template<> struct EnumTraits<WebCore::LegacyCertificateInfoType> {
    using values = EnumValues<
        WebCore::LegacyCertificateInfoType,
        WebCore::LegacyCertificateInfoType::None,
        WebCore::LegacyCertificateInfoType::CertificateChain
#if PLATFORM(COCOA)
        , WebCore::LegacyCertificateInfoType::Trust
#endif
    >;
};
}

namespace WTF::Persistence {

#if PLATFORM(COCOA)
static void encodeCFData(Encoder& encoder, CFDataRef data)
{
    uint64_t length = CFDataGetLength(data);
    const uint8_t* bytePtr = CFDataGetBytePtr(data);

    encoder << length;
    encoder.encodeFixedLengthData({ bytePtr, static_cast<size_t>(length) });
}

static std::optional<RetainPtr<CFDataRef>> decodeCFData(Decoder& decoder)
{
    std::optional<uint64_t> size;
    decoder >> size;

    if (UNLIKELY(!isInBounds<size_t>(*size)))
        return std::nullopt;

    auto pointer = decoder.bufferPointerForDirectRead(static_cast<size_t>(*size));
    if (!pointer)
        return std::nullopt;

    return adoptCF(CFDataCreate(nullptr, pointer, *size));
}

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

static std::optional<RetainPtr<SecTrustRef>> decodeSecTrustRef(Decoder& decoder)
{
    std::optional<bool> hasTrust;
    decoder >> hasTrust;
    if (!hasTrust)
        return std::nullopt;

    if (!*hasTrust)
        return { nullptr };

    auto trustData = decodeCFData(decoder);
    if (!trustData)
        return std::nullopt;

    auto trust = adoptCF(SecTrustDeserialize(trustData->get(), nullptr));
    if (!trust)
        return std::nullopt;

    return trust;
}
#endif

#if PLATFORM(COCOA)
static std::optional<RetainPtr<CFArrayRef>> decodeCertificateChain(Decoder& decoder)
{
    std::optional<uint64_t> size;
    decoder >> size;
    if (!size)
        return std::nullopt;

    auto array = adoptCF(CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks));

    for (size_t i = 0; i < *size; ++i) {
        auto data = decodeCFData(decoder);
        if (!data)
            return std::nullopt;

        auto certificate = adoptCF(SecCertificateCreateWithData(0, data->get()));
        CFArrayAppendValue(array.get(), certificate.get());
    }

    return { WTFMove(array) };
}
#endif

void Coder<WebCore::CertificateInfo>::encode(Encoder& encoder, const WebCore::CertificateInfo& certificateInfo)
{
#if PLATFORM(COCOA)
    encoder << WebCore::LegacyCertificateInfoType::Trust;
    encodeSecTrustRef(encoder, certificateInfo.trust());
#endif
}

std::optional<WebCore::CertificateInfo> Coder<WebCore::CertificateInfo>::decode(Decoder& decoder)
{
    std::optional<WebCore::LegacyCertificateInfoType> certificateInfoType;
    decoder >> certificateInfoType;
    if (!certificateInfoType)
        return std::nullopt;

    switch (*certificateInfoType) {
#if PLATFORM(COCOA)
    case WebCore::LegacyCertificateInfoType::Trust: {
        auto trust = decodeSecTrustRef(decoder);
        if (!trust)
            return std::nullopt;

        return WebCore::CertificateInfo(WTFMove(*trust));
    }
    case WebCore::LegacyCertificateInfoType::CertificateChain: {
        auto certificateChain = decodeCertificateChain(decoder);
        if (!certificateChain)
            return std::nullopt;
        return WebCore::CertificateInfo(WebCore::CertificateInfo::secTrustFromCertificateChain(certificateChain->get()));
    }
#endif
    case WebCore::LegacyCertificateInfoType::None:
        // Do nothing.
        return WebCore::CertificateInfo();
    }
}

} // namespace WTF::Persistence
