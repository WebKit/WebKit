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

#include "config.h"
#include "PlatformCertificateInfo.h"

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include <WebCore/ResourceResponse.h>

#if USE(CG)
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#endif

using namespace WebCore;

namespace WebKit {

PlatformCertificateInfo::PlatformCertificateInfo()
{
}

PlatformCertificateInfo::PlatformCertificateInfo(const ResourceResponse& response)
{
    CFURLResponseRef cfResponse = response.cfURLResponse();
    if (!cfResponse)
        return;

#if USE(CG)
    CFDictionaryRef certificateInfo = wkGetSSLCertificateInfo(cfResponse);
    if (!certificateInfo)
        return;

    void* data = wkGetSSLCertificateChainContext(certificateInfo);
    if (!data)
        return;

    PCCERT_CHAIN_CONTEXT chainContext = static_cast<PCCERT_CHAIN_CONTEXT>(data);
    if (chainContext->cChain < 1)
        return;

    // The first simple chain starts with the leaf certificate and ends with a trusted root or self-signed certificate.
    PCERT_SIMPLE_CHAIN firstSimpleChain = chainContext->rgpChain[0];
    for (unsigned i = 0; i < firstSimpleChain->cElement; ++i) {
        PCCERT_CONTEXT certificateContext = firstSimpleChain->rgpElement[i]->pCertContext;
        PCCERT_CONTEXT certificateContextCopy = ::CertDuplicateCertificateContext(certificateContext);
        m_certificateChain.append(certificateContextCopy);
    }
#else
    // FIXME: WinCairo implementation
#endif
}

PlatformCertificateInfo::PlatformCertificateInfo(PCCERT_CONTEXT certificateContext)
{
    if (!certificateContext)
        return;
    
    PCCERT_CONTEXT certificateContextCopy = ::CertDuplicateCertificateContext(certificateContext);
    m_certificateChain.append(certificateContextCopy);    
}

PlatformCertificateInfo::~PlatformCertificateInfo()
{
    clearCertificateChain();
}

PlatformCertificateInfo::PlatformCertificateInfo(const PlatformCertificateInfo& other)
{
    for (size_t i = 0; i < other.m_certificateChain.size(); ++i) {
        PCCERT_CONTEXT certificateContextCopy = ::CertDuplicateCertificateContext(other.m_certificateChain[i]);
        m_certificateChain.append(certificateContextCopy);
    }
}

PlatformCertificateInfo& PlatformCertificateInfo::operator=(const PlatformCertificateInfo& other)
{
    clearCertificateChain();
    for (size_t i = 0; i < other.m_certificateChain.size(); ++i) {
        PCCERT_CONTEXT certificateContextCopy = ::CertDuplicateCertificateContext(other.m_certificateChain[i]);
        m_certificateChain.append(certificateContextCopy);
    }
    return *this;
}

void PlatformCertificateInfo::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    // Special case no certificates
    if (m_certificateChain.isEmpty()) {
        encoder->encodeUInt64(std::numeric_limits<uint64_t>::max());
        return;
    }

    uint64_t length = m_certificateChain.size();
    encoder->encodeUInt64(length);

    for (size_t i = 0; i < length; ++i)
        encoder->encodeBytes(static_cast<uint8_t*>(m_certificateChain[i]->pbCertEncoded), m_certificateChain[i]->cbCertEncoded);
}

bool PlatformCertificateInfo::decode(CoreIPC::ArgumentDecoder* decoder, PlatformCertificateInfo& c)
{
    uint64_t length;
    if (!decoder->decode(length))
        return false;

    if (length == std::numeric_limits<uint64_t>::max()) {
        // This is the no certificates case.
        return true;
    }

    for (size_t i = 0; i < length; ++i) {
        Vector<uint8_t> bytes;
        if (!decoder->decodeBytes(bytes)) {
            c.clearCertificateChain();
            return false;
        }

        PCCERT_CONTEXT certificateContext = ::CertCreateCertificateContext(PKCS_7_ASN_ENCODING | X509_ASN_ENCODING, bytes.data(), bytes.size());
        if (!certificateContext) {
            c.clearCertificateChain();
            return false;
        }
        
        c.m_certificateChain.append(certificateContext);
    }

    return true;
}

void PlatformCertificateInfo::clearCertificateChain()
{
    for (size_t i = 0; i < m_certificateChain.size(); ++i)
        ::CertFreeCertificateContext(m_certificateChain[i]);
    m_certificateChain.clear();
}

} // namespace WebKit
