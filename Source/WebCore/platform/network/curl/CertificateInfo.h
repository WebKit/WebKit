/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#pragma once

#include "CertificateSummary.h"
#include "NotImplemented.h"
#include <wtf/Vector.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>

namespace WebCore {

class CertificateInfo {
public:
    using Certificate = Vector<uint8_t>;
    using CertificateChain = Vector<Certificate>;

    CertificateInfo() = default;
    WEBCORE_EXPORT CertificateInfo(int verificationError, CertificateChain&&);

    WEBCORE_EXPORT CertificateInfo isolatedCopy() const;

    int verificationError() const { return m_verificationError; }
    WEBCORE_EXPORT String verificationErrorDescription() const;
    const Vector<Certificate>& certificateChain() const { return m_certificateChain; }

    bool containsNonRootSHA1SignedCertificate() const { notImplemented(); return false; }

    Optional<CertificateSummary> summaryInfo() const;

    bool isEmpty() const { return m_certificateChain.isEmpty(); }

    static Certificate makeCertificate(const uint8_t*, size_t);

private:
    int m_verificationError { 0 };
    CertificateChain m_certificateChain;
};

inline bool operator==(const CertificateInfo& a, const CertificateInfo& b)
{
    return a.verificationError() == b.verificationError() && a.certificateChain() == b.certificateChain();
}

} // namespace WebCore

namespace WTF {
namespace Persistence {

template<> struct Coder<WebCore::CertificateInfo> {
    static void encode(Encoder& encoder, const WebCore::CertificateInfo& certificateInfo)
    {
        auto& certificateChain = certificateInfo.certificateChain();

        encoder << certificateInfo.verificationError();
        encoder << certificateChain.size();
        for (auto& certificate : certificateChain)
            encoder << certificate;
    }

    static Optional<WebCore::CertificateInfo> decode(Decoder& decoder)
    {
        Optional<int> verificationError;
        decoder >> verificationError;
        if (!verificationError)
            return WTF::nullopt;

        Optional<size_t> numOfCerts;
        decoder >> numOfCerts;
        if (!numOfCerts)
            return WTF::nullopt;

        WebCore::CertificateInfo::CertificateChain certificateChain;
        for (size_t i = 0; i < numOfCerts.value(); i++) {
            Optional<WebCore::CertificateInfo::Certificate> certificate;
            decoder >> certificate;
            if (!certificate)
                return WTF::nullopt;

            certificateChain.append(WTFMove(certificate.value()));
        }

        return WebCore::CertificateInfo(verificationError.value(), WTFMove(certificateChain));
    }
};

} // namespace WTF::Persistence
} // namespace WTF
