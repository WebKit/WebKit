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
#include <wtf/text/CString.h>

namespace WebCore {

class ResourceResponse : public ResourceResponseBase {
public:
    ResourceResponse() = default;

    ResourceResponse(const URL& url, const String& mimeType, long long expectedLength, const String& textEncodingName)
        : ResourceResponseBase(url, mimeType, expectedLength, textEncodingName)
    {
    }

    ResourceResponse(ResourceResponseBase&& base)
        : ResourceResponseBase(WTFMove(base))
    {
    }

    ResourceResponse(SoupMessage*, const CString& sniffedContentType = CString());

    void updateSoupMessageHeaders(SoupMessageHeaders*) const;
    void updateFromSoupMessageHeaders(SoupMessageHeaders*);

    GTlsCertificate* soupMessageCertificate() const { return m_certificate.get(); }
    GTlsCertificateFlags soupMessageTLSErrors() const { return m_tlsErrors; }

private:
    friend class ResourceResponseBase;

    String m_sniffedContentType;
    GRefPtr<GTlsCertificate> m_certificate;
    GTlsCertificateFlags m_tlsErrors { static_cast<GTlsCertificateFlags>(0) };

    void doUpdateResourceResponse() { }
    String platformSuggestedFilename() const;
    CertificateInfo platformCertificateInfo(Span<const std::byte>) const;
};

} // namespace WebCore
