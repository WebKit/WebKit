/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ResourceResponseBase.h"

#include <libsoup/soup.h>
#include <wtf/glib/GRefPtr.h>

namespace WebCore {

class ResourceResponse : public ResourceResponseBase {
public:
    ResourceResponse()
        : ResourceResponseBase()
        , m_soupFlags(static_cast<SoupMessageFlags>(0))
        , m_tlsErrors(static_cast<GTlsCertificateFlags>(0))
    {
    }

    ResourceResponse(const URL& url, const String& mimeType, long long expectedLength, const String& textEncodingName)
        : ResourceResponseBase(url, mimeType, expectedLength, textEncodingName)
        , m_soupFlags(static_cast<SoupMessageFlags>(0))
        , m_tlsErrors(static_cast<GTlsCertificateFlags>(0))
    {
    }

    ResourceResponse(SoupMessage* soupMessage)
        : ResourceResponseBase()
        , m_soupFlags(static_cast<SoupMessageFlags>(0))
        , m_tlsErrors(static_cast<GTlsCertificateFlags>(0))
    {
        updateFromSoupMessage(soupMessage);
    }

    void updateSoupMessageHeaders(SoupMessageHeaders*) const;
    void updateFromSoupMessage(SoupMessage*);
    void updateFromSoupMessageHeaders(const SoupMessageHeaders*);

    SoupMessageFlags soupMessageFlags() const { return m_soupFlags; }
    void setSoupMessageFlags(SoupMessageFlags soupFlags) { m_soupFlags = soupFlags; }

    const String& sniffedContentType() const { return m_sniffedContentType; }
    void setSniffedContentType(const String& value) { m_sniffedContentType = value; }

    GTlsCertificate* soupMessageCertificate() const { return m_certificate.get(); }
    void setSoupMessageCertificate(GTlsCertificate* certificate) { m_certificate = certificate; }

    GTlsCertificateFlags soupMessageTLSErrors() const { return m_tlsErrors; }
    void setSoupMessageTLSErrors(GTlsCertificateFlags tlsErrors) { m_tlsErrors = tlsErrors; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, ResourceResponse&);

private:
    friend class ResourceResponseBase;

    SoupMessageFlags m_soupFlags;
    String m_sniffedContentType;
    GRefPtr<GTlsCertificate> m_certificate;
    GTlsCertificateFlags m_tlsErrors;

    void doUpdateResourceResponse() { }
    String platformSuggestedFilename() const;
    CertificateInfo platformCertificateInfo() const;
};

template<class Encoder>
void ResourceResponse::encode(Encoder& encoder) const
{
    ResourceResponseBase::encode(encoder);
    encoder.encodeEnum(m_soupFlags);
}

template<class Decoder>
bool ResourceResponse::decode(Decoder& decoder, ResourceResponse& response)
{
    if (!ResourceResponseBase::decode(decoder, response))
        return false;
    if (!decoder.decodeEnum(response.m_soupFlags))
        return false;
    return true;
}

} // namespace WebCore
