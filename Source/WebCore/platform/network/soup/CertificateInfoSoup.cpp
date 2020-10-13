/*
 * Copyright (C) 2012 Igalia S.L.
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

#if USE(SOUP)

#include "CertificateInfo.h"

#include <ResourceError.h>
#include <ResourceResponse.h>
#include <libsoup/soup.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

CertificateInfo::CertificateInfo()
    : m_tlsErrors(static_cast<GTlsCertificateFlags>(0))
{
}

CertificateInfo::CertificateInfo(const ResourceResponse& response)
    : m_certificate(response.soupMessageCertificate())
    , m_tlsErrors(response.soupMessageTLSErrors())
{
}

CertificateInfo::CertificateInfo(const ResourceError& resourceError)
    : m_certificate(resourceError.certificate())
    , m_tlsErrors(static_cast<GTlsCertificateFlags>(resourceError.tlsErrors()))
{
}

CertificateInfo::CertificateInfo(GTlsCertificate* certificate, GTlsCertificateFlags tlsErrors)
    : m_certificate(certificate)
    , m_tlsErrors(tlsErrors)
{
}

CertificateInfo::~CertificateInfo() = default;

static GRefPtr<GTlsCertificate> createCertificate(GByteArray* bytes, GTlsCertificate* issuer)
{
    gpointer cert = g_initable_new(g_tls_backend_get_certificate_type(g_tls_backend_get_default()),
        nullptr, nullptr,
        "certificate", bytes,
        "issuer", issuer,
        nullptr);
    RELEASE_ASSERT(cert);
    return adoptGRef(G_TLS_CERTIFICATE(cert));
}

CertificateInfo CertificateInfo::isolatedCopy() const
{
    // We can only copy the public portions, so this can only be used for server certificates, not
    // for client certificates. Sadly, other ports don't have this restriction, and there is no way
    // to assert that we are not messing up here because we can't know how callers are using the
    // certificate. So be careful?
    //
    // We should add g_tls_certificate_copy() to GLib so that we can copy the private portion too.

    Vector<GRefPtr<GByteArray>> certificateBytes;
    GTlsCertificate* cert = m_certificate.get();
    if (!cert)
        return CertificateInfo();

    do {
        GRefPtr<GByteArray> der;
        g_object_get(cert, "certificate", &der.outPtr(), nullptr);

        GRefPtr<GByteArray> copy = adoptGRef(g_byte_array_new());
        g_byte_array_append(copy.get(), der->data, der->len);
        certificateBytes.append(WTFMove(copy));
    } while ((cert = g_tls_certificate_get_issuer(cert)));

    auto finalCertificateIndex = certificateBytes.size() - 1;
    GRefPtr<GTlsCertificate> copy = createCertificate(certificateBytes[finalCertificateIndex].get(), nullptr);
    for (ssize_t i = finalCertificateIndex - 1; i >= 0; i--)
        copy = createCertificate(certificateBytes[i].get(), copy.get());
    return CertificateInfo(copy.get(), m_tlsErrors);
}

} // namespace WebCore

#endif
