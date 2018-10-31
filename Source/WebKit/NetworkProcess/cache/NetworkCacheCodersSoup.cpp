/*
 * Copyright (C) 2011, 2014-2015 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Igalia S.L.
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

namespace WTF {
namespace Persistence {


static Vector<GRefPtr<GByteArray>> certificatesDataListFromCertificateInfo(const WebCore::CertificateInfo &certificateInfo)
{
    auto* certificate = certificateInfo.certificate();
    if (!certificate)
        return { };

    Vector<GRefPtr<GByteArray>> certificatesDataList;
    for (; certificate; certificate = g_tls_certificate_get_issuer(certificate)) {
        GByteArray* certificateData = nullptr;
        g_object_get(G_OBJECT(certificate), "certificate", &certificateData, nullptr);

        if (!certificateData) {
            certificatesDataList.clear();
            break;
        }
        certificatesDataList.append(adoptGRef(certificateData));
    }

    // Reverse so that the list starts from the rootmost certificate.
    certificatesDataList.reverse();

    return certificatesDataList;
}

static GRefPtr<GTlsCertificate> certificateFromCertificatesDataList(const Vector<GRefPtr<GByteArray>> &certificatesDataList)
{
    GType certificateType = g_tls_backend_get_certificate_type(g_tls_backend_get_default());
    GRefPtr<GTlsCertificate> certificate;
    for (auto& certificateData : certificatesDataList) {
        certificate = adoptGRef(G_TLS_CERTIFICATE(g_initable_new(
            certificateType, nullptr, nullptr, "certificate", certificateData.get(), "issuer", certificate.get(), nullptr)));
    }

    return certificate;
}

void Coder<WebCore::CertificateInfo>::encode(Encoder& encoder, const WebCore::CertificateInfo& certificateInfo)
{
    auto certificatesDataList = certificatesDataListFromCertificateInfo(certificateInfo);

    encoder << certificatesDataList;

    if (certificatesDataList.isEmpty())
        return;

    encoder << static_cast<uint32_t>(certificateInfo.tlsErrors());
}

bool Coder<WebCore::CertificateInfo>::decode(Decoder& decoder, WebCore::CertificateInfo& certificateInfo)
{
    Vector<GRefPtr<GByteArray>> certificatesDataList;
    if (!decoder.decode(certificatesDataList))
        return false;

    if (certificatesDataList.isEmpty())
        return true;
    certificateInfo.setCertificate(certificateFromCertificatesDataList(certificatesDataList).get());

    uint32_t tlsErrors;
    if (!decoder.decode(tlsErrors))
        return false;
    certificateInfo.setTLSErrors(static_cast<GTlsCertificateFlags>(tlsErrors));

    return true;
}

void Coder<GRefPtr<GByteArray>>::encode(Encoder &encoder, const GRefPtr<GByteArray>& byteArray)
{
    encoder << static_cast<uint32_t>(byteArray->len);
    encoder.encodeFixedLengthData(byteArray->data, byteArray->len);
}

bool Coder<GRefPtr<GByteArray>>::decode(Decoder &decoder, GRefPtr<GByteArray>& byteArray)
{
    uint32_t size;
    if (!decoder.decode(size))
        return false;

    byteArray = adoptGRef(g_byte_array_sized_new(size));
    return decoder.decodeFixedLengthData(byteArray->data, byteArray->len);
}

}
}
