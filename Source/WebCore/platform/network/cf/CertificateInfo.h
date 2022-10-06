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
#if PLATFORM(COCOA)
    explicit CertificateInfo(RetainPtr<SecTrustRef>&& trust)
        : m_trust(WTFMove(trust))
    {
    }
    SecTrustRef trust() const { return m_trust.get(); }
#elif PLATFORM(WIN)
    CertificateInfo(RetainPtr<CFArrayRef>&& certificateChain)
        : m_certificateChain(WTFMove(certificateChain))
    {
    }
    CFArrayRef certificateChain() const { return m_certificateChain.get(); }
#endif
    CertificateInfo isolatedCopy() const { return *this; }

    WEBCORE_EXPORT bool containsNonRootSHA1SignedCertificate() const;

    std::optional<CertificateSummary> summary() const;

    bool isEmpty() const
    {
#if PLATFORM(COCOA)
        return !m_trust;
#elif PLATFORM(WIN)
        return !m_certificateChain;
#endif
    }

    bool operator==(const CertificateInfo& other) const
    {
#if PLATFORM(COCOA)
        return trust() == other.trust();
#elif PLATFORM(WIN)
        return certificateChain() == other.certificateChain();
#endif
    }
    bool operator!=(const CertificateInfo& other) const { return !(*this == other); }

#if PLATFORM(COCOA)
    WEBCORE_EXPORT static RetainPtr<CFArrayRef> certificateChainFromSecTrust(SecTrustRef);
    WEBCORE_EXPORT static RetainPtr<SecTrustRef> secTrustFromCertificateChain(CFArrayRef);
#endif

#ifndef NDEBUG
#if PLATFORM(COCOA)
    void dump() const;
#endif
#endif

private:
#if PLATFORM(COCOA)
    RetainPtr<SecTrustRef> m_trust;
#elif PLATFORM(WIN)
    RetainPtr<CFArrayRef> m_certificateChain;
#endif
};

#if PLATFORM(COCOA)
WEBCORE_EXPORT bool certificatesMatch(SecTrustRef, SecTrustRef);
#endif

} // namespace WebCore
