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

#include "PlatformCertificateInfo.h"

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include <WebCore/ResourceResponse.h>

#if PLATFORM(CG)
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#endif

using namespace WebCore;

namespace WebKit {

PlatformCertificateInfo::PlatformCertificateInfo()
    : m_certificateContext(0)
{
}

PlatformCertificateInfo::PlatformCertificateInfo(const ResourceResponse& response)
    : m_certificateContext(0)
{
    CFURLResponseRef cfResponse = response.cfURLResponse();
    if (!cfResponse)
        return;

#if PLATFORM(CG)
    CFDictionaryRef certificateInfo = wkGetSSLCertificateInfo(cfResponse);
    if (!certificateInfo)
        return;

    void* data = wkGetSSLPeerCertificateData(certificateInfo);
    if (!data)
        return;

    m_certificateContext = ::CertDuplicateCertificateContext(static_cast<PCCERT_CONTEXT>(data));
#else
    // FIXME: WinCairo implementation
#endif
}

PlatformCertificateInfo::~PlatformCertificateInfo()
{
    if (m_certificateContext)
        ::CertFreeCertificateContext(m_certificateContext);
}

PlatformCertificateInfo::PlatformCertificateInfo(const PlatformCertificateInfo& other)
    : m_certificateContext(::CertDuplicateCertificateContext(other.m_certificateContext))
{
}

PlatformCertificateInfo& PlatformCertificateInfo::operator=(const PlatformCertificateInfo& other)
{
    ::CertDuplicateCertificateContext(other.m_certificateContext);
    if (m_certificateContext)
        ::CertFreeCertificateContext(m_certificateContext);
    m_certificateContext = other.m_certificateContext;
    return *this;
}

void PlatformCertificateInfo::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    // FIXME: We should encode the no certificate context case in the
    // number of the bytes.
    if (!m_certificateContext) {
        encoder->encodeBool(true);
        return;
    }

    encoder->encodeBool(false);
    encoder->encodeBytes(static_cast<uint8_t*>(m_certificateContext->pbCertEncoded), m_certificateContext->cbCertEncoded);
}

bool PlatformCertificateInfo::decode(CoreIPC::ArgumentDecoder* decoder, PlatformCertificateInfo& c)
{
    bool noCertificate;
    if (!decoder->decode(noCertificate))
        return false;

    if (noCertificate)
        return true;

    Vector<uint8_t> bytes;
    if (!decoder->decodeBytes(bytes))
        return false;

    PCCERT_CONTEXT certificateContext = ::CertCreateCertificateContext(PKCS_7_ASN_ENCODING | X509_ASN_ENCODING, bytes.data(), bytes.size());
    if (!certificateContext)
        return false;

    c.m_certificateContext = certificateContext;
    return true;
}

} // namespace WebKit
