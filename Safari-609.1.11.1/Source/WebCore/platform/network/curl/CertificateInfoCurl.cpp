/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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

#include "OpenSSLHelper.h"
#include <wtf/CrossThreadCopier.h>

#if USE(CURL)

namespace WebCore {

CertificateInfo::CertificateInfo(int verificationError, CertificateChain&& certificateChain)
    : m_verificationError(verificationError)
    , m_certificateChain(WTFMove(certificateChain))
{
}

CertificateInfo CertificateInfo::isolatedCopy() const
{
    return { m_verificationError, crossThreadCopy(m_certificateChain) };
}

CertificateInfo::Certificate CertificateInfo::makeCertificate(const uint8_t* buffer, size_t size)
{
    Certificate certificate;
    certificate.append(buffer, size);
    return certificate;
}

Optional<CertificateInfo::SummaryInfo> CertificateInfo::summaryInfo() const
{
    if (!m_certificateChain.size())
        return WTF::nullopt;

    return OpenSSL::createSummaryInfo(m_certificateChain.at(0));
}

}

#endif
