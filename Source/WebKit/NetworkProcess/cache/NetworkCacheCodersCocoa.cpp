/*
 * Copyright (C) 2011, 2014-2015 Apple Inc. All rights reserved.
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
#include "NetworkCacheCoders.h"

#if PLATFORM(COCOA)
#include <Security/SecCertificate.h>
#include <Security/SecTrust.h>
#include <wtf/cf/TypeCastsCF.h>
#include <wtf/spi/cocoa/SecuritySPI.h>
#endif

WTF_DECLARE_CF_TYPE_TRAIT(SecCertificate);

namespace WTF {
namespace Persistence {

static void encodeCFData(Encoder& encoder, CFDataRef data)
{
    uint64_t length = CFDataGetLength(data);
    const uint8_t* bytePtr = CFDataGetBytePtr(data);

    encoder << length;
    encoder.encodeFixedLengthData(bytePtr, length);
}

static bool decodeCFData(Decoder& decoder, RetainPtr<CFDataRef>& data)
{
    uint64_t size = 0;
    if (!decoder.decode(size))
        return false;

    Vector<uint8_t> vector(size);
    if (!decoder.decodeFixedLengthData(vector.data(), vector.size()))
        return false;

    data = adoptCF(CFDataCreate(nullptr, vector.data(), vector.size()));
    return true;
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

static bool decodeSecTrustRef(Decoder& decoder, RetainPtr<SecTrustRef>& result)
{
    bool hasTrust;
    if (!decoder.decode(hasTrust))
        return false;

    if (!hasTrust)
        return true;

    RetainPtr<CFDataRef> trustData;
    if (!decodeCFData(decoder, trustData))
        return false;

    auto trust = adoptCF(SecTrustDeserialize(trustData.get(), nullptr));
    if (!trust)
        return false;

    result = WTFMove(trust);
    return true;
}
#endif

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

static bool decodeCertificateChain(Decoder& decoder, RetainPtr<CFArrayRef>& certificateChain)
{
    uint64_t size;
    if (!decoder.decode(size))
        return false;

    auto array = adoptCF(CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks));

    for (size_t i = 0; i < size; ++i) {
        RetainPtr<CFDataRef> data;
        if (!decodeCFData(decoder, data))
            return false;

        auto certificate = adoptCF(SecCertificateCreateWithData(0, data.get()));
        CFArrayAppendValue(array.get(), certificate.get());
    }

    certificateChain = WTFMove(array);
    return true;
}

void Coder<WebCore::CertificateInfo>::encode(Encoder& encoder, const WebCore::CertificateInfo& certificateInfo)
{
    encoder.encodeEnum(certificateInfo.type());

    switch (certificateInfo.type()) {
#if HAVE(SEC_TRUST_SERIALIZATION)
    case WebCore::CertificateInfo::Type::Trust:
        encodeSecTrustRef(encoder, certificateInfo.trust());
        break;
#endif
    case WebCore::CertificateInfo::Type::CertificateChain: {
        encodeCertificateChain(encoder, certificateInfo.certificateChain());
        break;
    }
    case WebCore::CertificateInfo::Type::None:
        // Do nothing.
        break;
    }
}

bool Coder<WebCore::CertificateInfo>::decode(Decoder& decoder, WebCore::CertificateInfo& certificateInfo)
{
    WebCore::CertificateInfo::Type certificateInfoType;
    if (!decoder.decodeEnum(certificateInfoType))
        return false;

    switch (certificateInfoType) {
#if HAVE(SEC_TRUST_SERIALIZATION)
    case WebCore::CertificateInfo::Type::Trust: {
        RetainPtr<SecTrustRef> trust;
        if (!decodeSecTrustRef(decoder, trust))
            return false;

        certificateInfo = WebCore::CertificateInfo(WTFMove(trust));
        return true;
    }
#endif
    case WebCore::CertificateInfo::Type::CertificateChain: {
        RetainPtr<CFArrayRef> certificateChain;
        if (!decodeCertificateChain(decoder, certificateChain))
            return false;

        certificateInfo = WebCore::CertificateInfo(WTFMove(certificateChain));
        return true;
    }    
    case WebCore::CertificateInfo::Type::None:
        // Do nothing.
        break;
    }

    return true;
}

}
}
