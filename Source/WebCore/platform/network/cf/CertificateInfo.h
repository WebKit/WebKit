/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include <wtf/EnumTraits.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/cf/TypeCastsCF.h>
#include <wtf/persistence/PersistentCoder.h>

#if PLATFORM(COCOA)
#include <Security/SecCertificate.h>
#include <Security/SecTrust.h>
#include <wtf/spi/cocoa/SecuritySPI.h>

WTF_DECLARE_CF_TYPE_TRAIT(SecCertificate);
#endif

namespace WebCore {

struct CertificateSummary;

class CertificateInfo {
public:
     CertificateInfo() = default;
 
    enum class Type {
        None,
        CertificateChain,
#if HAVE(SEC_TRUST_SERIALIZATION)
        Trust,
#endif
    };

#if HAVE(SEC_TRUST_SERIALIZATION)
    explicit CertificateInfo(RetainPtr<SecTrustRef>&& trust)
        : m_trust(WTFMove(trust))
    {
    }
 
    SecTrustRef trust() const { return m_trust.get(); }
#endif

    CertificateInfo(RetainPtr<CFArrayRef>&& certificateChain)
        : m_certificateChain(WTFMove(certificateChain))
    {
    }

    CertificateInfo isolatedCopy() const { return *this; }

    WEBCORE_EXPORT CFArrayRef certificateChain() const;

    WEBCORE_EXPORT Type type() const;
    WEBCORE_EXPORT bool containsNonRootSHA1SignedCertificate() const;

    Optional<CertificateSummary> summary() const;

    bool isEmpty() const { return type() == Type::None; }

#if PLATFORM(COCOA)
    static RetainPtr<CFArrayRef> certificateChainFromSecTrust(SecTrustRef);
#endif

#ifndef NDEBUG
#if PLATFORM(COCOA)
    void dump() const;
#endif
#endif

private:
#if HAVE(SEC_TRUST_SERIALIZATION)
    RetainPtr<SecTrustRef> m_trust;
#endif
    mutable RetainPtr<CFArrayRef> m_certificateChain;
};

#if PLATFORM(COCOA)
WEBCORE_EXPORT bool certificatesMatch(SecTrustRef, SecTrustRef);
#endif

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::CertificateInfo::Type> {
    using values = EnumValues<
        WebCore::CertificateInfo::Type,
        WebCore::CertificateInfo::Type::None,
        WebCore::CertificateInfo::Type::CertificateChain
#if HAVE(SEC_TRUST_SERIALIZATION)
        , WebCore::CertificateInfo::Type::Trust
#endif
    >;
};

namespace Persistence {

template<> struct Coder<WebCore::CertificateInfo> {
    static WEBCORE_EXPORT void encode(Encoder&, const WebCore::CertificateInfo&);
    static WEBCORE_EXPORT Optional<WebCore::CertificateInfo> decode(Decoder&);
};

} // namespace WTF::Persistence

} // namespace WTF
