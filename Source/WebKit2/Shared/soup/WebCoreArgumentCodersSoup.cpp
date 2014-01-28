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
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/text/CString.h>

using namespace WebCore;

namespace IPC {

void ArgumentCoder<ResourceRequest>::encodePlatformData(ArgumentEncoder& encoder, const ResourceRequest& resourceRequest)
{
    encoder << static_cast<uint32_t>(resourceRequest.soupMessageFlags());
    encoder << resourceRequest.initiatingPageID();
}

bool ArgumentCoder<ResourceRequest>::decodePlatformData(ArgumentDecoder& decoder, ResourceRequest& resourceRequest)
{
    uint32_t soupMessageFlags;
    if (!decoder.decode(soupMessageFlags))
        return false;
    resourceRequest.setSoupMessageFlags(static_cast<SoupMessageFlags>(soupMessageFlags));

    uint64_t initiatingPageID;
    if (!decoder.decode(initiatingPageID))
        return false;
    resourceRequest.setInitiatingPageID(initiatingPageID);

    return true;
}


void ArgumentCoder<ResourceResponse>::encodePlatformData(ArgumentEncoder& encoder, const ResourceResponse& resourceResponse)
{
    encoder << static_cast<uint32_t>(resourceResponse.soupMessageFlags());
}

bool ArgumentCoder<ResourceResponse>::decodePlatformData(ArgumentDecoder& decoder, ResourceResponse& resourceResponse)
{
    uint32_t soupMessageFlags;
    if (!decoder.decode(soupMessageFlags))
        return false;
    resourceResponse.setSoupMessageFlags(static_cast<SoupMessageFlags>(soupMessageFlags));
    return true;
}

void ArgumentCoder<CertificateInfo>::encode(ArgumentEncoder& encoder, const CertificateInfo& certificateInfo)
{
    if (!certificateInfo.certificate()) {
        encoder << false;
        return;
    }

    GByteArray* certificateData = 0;
    g_object_get(G_OBJECT(certificateInfo.certificate()), "certificate", &certificateData, NULL);
    if (!certificateData) {
        encoder << false;
        return;
    }

    encoder << true;
    GRefPtr<GByteArray> certificate = adoptGRef(certificateData);
    encoder.encodeVariableLengthByteArray(IPC::DataReference(certificate->data, certificate->len));
    encoder << static_cast<uint32_t>(certificateInfo.tlsErrors());
}

bool ArgumentCoder<CertificateInfo>::decode(ArgumentDecoder& decoder, CertificateInfo& certificateInfo)
{
    bool hasCertificate;
    if (!decoder.decode(hasCertificate))
        return false;

    if (!hasCertificate)
        return true;

    IPC::DataReference certificateDataReference;
    if (!decoder.decodeVariableLengthByteArray(certificateDataReference))
        return false;

    GByteArray* certificateData = g_byte_array_sized_new(certificateDataReference.size());
    certificateData = g_byte_array_append(certificateData, certificateDataReference.data(), certificateDataReference.size());
    GRefPtr<GByteArray> certificateBytes = adoptGRef(certificateData);

    GTlsBackend* backend = g_tls_backend_get_default();
    GRefPtr<GTlsCertificate> certificate = adoptGRef(G_TLS_CERTIFICATE(g_initable_new(
        g_tls_backend_get_certificate_type(backend), 0, 0, "certificate", certificateBytes.get(), nullptr)));
    certificateInfo.setCertificate(certificate.get());

    uint32_t tlsErrors;
    if (!decoder.decode(tlsErrors))
        return false;
    certificateInfo.setTLSErrors(static_cast<GTlsCertificateFlags>(tlsErrors));

    return true;
}

void ArgumentCoder<ResourceError>::encodePlatformData(ArgumentEncoder& encoder, const ResourceError& resourceError)
{
    encoder << CertificateInfo(resourceError);
}

bool ArgumentCoder<ResourceError>::decodePlatformData(ArgumentDecoder& decoder, ResourceError& resourceError)
{
    CertificateInfo certificateInfo;
    if (!decoder.decode(certificateInfo))
        return false;

    resourceError.setCertificate(certificateInfo.certificate());
    resourceError.setTLSErrors(certificateInfo.tlsErrors());
    return true;
}

}

