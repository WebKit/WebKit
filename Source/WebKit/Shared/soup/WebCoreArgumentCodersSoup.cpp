/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Portions Copyright (c) 2013 Company 100 Inc. All rights reserved.
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
#include "DataReference.h"
#include "WebCoreArgumentCoders.h"

#include <WebCore/CertificateInfo.h>
#include <WebCore/DictionaryPopupInfo.h>
#include <WebCore/FontAttributes.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SoupNetworkProxySettings.h>
#include <wtf/text/CString.h>

namespace IPC {
using namespace WebCore;

void ArgumentCoder<ResourceRequest>::encodePlatformData(Encoder& encoder, const ResourceRequest& resourceRequest)
{
    resourceRequest.encodeWithPlatformData(encoder);
}

bool ArgumentCoder<ResourceRequest>::decodePlatformData(Decoder& decoder, ResourceRequest& resourceRequest)
{
    return resourceRequest.decodeWithPlatformData(decoder);
}

void ArgumentCoder<CertificateInfo>::encode(Encoder& encoder, const CertificateInfo& certificateInfo)
{
    auto* certificate = certificateInfo.certificate();
    if (!certificate) {
        encoder << 0;
        return;
    }

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

    encoder << static_cast<uint32_t>(certificatesDataList.size());

    if (certificatesDataList.isEmpty())
        return;

    // Encode starting from the root certificate.
    for (size_t i = certificatesDataList.size(); i > 0; --i) {
        GByteArray* certificate = certificatesDataList[i - 1].get();
        encoder.encodeVariableLengthByteArray(IPC::DataReference(certificate->data, certificate->len));
    }

    encoder << static_cast<uint32_t>(certificateInfo.tlsErrors());
}

bool ArgumentCoder<CertificateInfo>::decode(Decoder& decoder, CertificateInfo& certificateInfo)
{
    uint32_t chainLength;
    if (!decoder.decode(chainLength))
        return false;

    if (!chainLength)
        return true;

    GType certificateType = g_tls_backend_get_certificate_type(g_tls_backend_get_default());
    GRefPtr<GTlsCertificate> certificate;
    for (uint32_t i = 0; i < chainLength; i++) {
        IPC::DataReference certificateDataReference;
        if (!decoder.decodeVariableLengthByteArray(certificateDataReference))
            return false;

        GByteArray* certificateData = g_byte_array_sized_new(certificateDataReference.size());
        GRefPtr<GByteArray> certificateBytes = adoptGRef(g_byte_array_append(certificateData, certificateDataReference.data(), certificateDataReference.size()));

        certificate = adoptGRef(G_TLS_CERTIFICATE(g_initable_new(
            certificateType, nullptr, nullptr, "certificate", certificateBytes.get(), "issuer", certificate.get(), nullptr)));
    }

    uint32_t tlsErrors;
    if (!decoder.decode(tlsErrors))
        return false;

    certificateInfo.setCertificate(certificate.get());
    certificateInfo.setTLSErrors(static_cast<GTlsCertificateFlags>(tlsErrors));

    return true;
}

void ArgumentCoder<ResourceError>::encodePlatformData(Encoder& encoder, const ResourceError& resourceError)
{
    encoder << resourceError.domain();
    encoder << resourceError.errorCode();
    encoder << resourceError.failingURL().string();
    encoder << resourceError.localizedDescription();

    encoder << CertificateInfo(resourceError);
}

bool ArgumentCoder<ResourceError>::decodePlatformData(Decoder& decoder, ResourceError& resourceError)
{
    String domain;
    if (!decoder.decode(domain))
        return false;

    int errorCode;
    if (!decoder.decode(errorCode))
        return false;

    String failingURL;
    if (!decoder.decode(failingURL))
        return false;

    String localizedDescription;
    if (!decoder.decode(localizedDescription))
        return false;

    resourceError = ResourceError(domain, errorCode, URL(URL(), failingURL), localizedDescription);

    CertificateInfo certificateInfo;
    if (!decoder.decode(certificateInfo))
        return false;

    resourceError.setCertificate(certificateInfo.certificate());
    resourceError.setTLSErrors(certificateInfo.tlsErrors());
    return true;
}

void ArgumentCoder<SoupNetworkProxySettings>::encode(Encoder& encoder, const SoupNetworkProxySettings& settings)
{
    ASSERT(!settings.isEmpty());
    encoder.encodeEnum(settings.mode);
    if (settings.mode != SoupNetworkProxySettings::Mode::Custom)
        return;

    encoder << settings.defaultProxyURL;
    uint32_t ignoreHostsCount = settings.ignoreHosts ? g_strv_length(settings.ignoreHosts.get()) : 0;
    encoder << ignoreHostsCount;
    if (ignoreHostsCount) {
        for (uint32_t i = 0; settings.ignoreHosts.get()[i]; ++i)
            encoder << CString(settings.ignoreHosts.get()[i]);
    }
    encoder << settings.proxyMap;
}

bool ArgumentCoder<SoupNetworkProxySettings>::decode(Decoder& decoder, SoupNetworkProxySettings& settings)
{
    if (!decoder.decodeEnum(settings.mode))
        return false;

    if (settings.mode != SoupNetworkProxySettings::Mode::Custom)
        return true;

    if (!decoder.decode(settings.defaultProxyURL))
        return false;

    uint32_t ignoreHostsCount;
    if (!decoder.decode(ignoreHostsCount))
        return false;

    if (ignoreHostsCount) {
        settings.ignoreHosts.reset(g_new0(char*, ignoreHostsCount + 1));
        for (uint32_t i = 0; i < ignoreHostsCount; ++i) {
            CString host;
            if (!decoder.decode(host))
                return false;

            settings.ignoreHosts.get()[i] = g_strdup(host.data());
        }
    }

    if (!decoder.decode(settings.proxyMap))
        return false;

    return !settings.isEmpty();
}

void ArgumentCoder<ProtectionSpace>::encodePlatformData(Encoder&, const ProtectionSpace&)
{
    ASSERT_NOT_REACHED();
}

bool ArgumentCoder<ProtectionSpace>::decodePlatformData(Decoder&, ProtectionSpace&)
{
    ASSERT_NOT_REACHED();
    return false;
}

void ArgumentCoder<Credential>::encodePlatformData(Encoder&, const Credential&)
{
    ASSERT_NOT_REACHED();
}

bool ArgumentCoder<Credential>::decodePlatformData(Decoder&, Credential&)
{
    ASSERT_NOT_REACHED();
    return false;
}

void ArgumentCoder<FontAttributes>::encodePlatformData(Encoder&, const FontAttributes&)
{
    ASSERT_NOT_REACHED();
}

Optional<FontAttributes> ArgumentCoder<FontAttributes>::decodePlatformData(Decoder&, FontAttributes&)
{
    ASSERT_NOT_REACHED();
    return WTF::nullopt;
}

void ArgumentCoder<DictionaryPopupInfo>::encodePlatformData(Encoder&, const DictionaryPopupInfo&)
{
    ASSERT_NOT_REACHED();
}

bool ArgumentCoder<DictionaryPopupInfo>::decodePlatformData(Decoder&, DictionaryPopupInfo&)
{
    ASSERT_NOT_REACHED();
    return false;
}

}

