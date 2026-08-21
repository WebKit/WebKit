/*
 * Copyright (C) 2026 Igalia S.L.
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
#include "CoreIPCGTlsCertificate.h"

#if USE(GLIB)

#include <gio/gio.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebKit {

CoreIPCGTlsCertificate::CoreIPCGTlsCertificate(const GRefPtr<GTlsCertificate>& certificate)
{
    for (auto* nextCertificate = certificate.get(); nextCertificate; nextCertificate = g_tls_certificate_get_issuer(nextCertificate)) {
        GRefPtr<GByteArray> certificateData;
        g_object_get(nextCertificate, "certificate", &certificateData.outPtr(), nullptr);

        if (!certificateData) {
            m_certificates.clear();
            break;
        }
        m_certificates.insert(0, WTF::move(certificateData));
    }

    if (m_certificates.isEmpty())
        return;

    GUniqueOutPtr<char> privateKeyPKCS11Uri;
    g_object_get(certificate.get(), "private-key", &m_privateKey.outPtr(), "private-key-pkcs11-uri", &privateKeyPKCS11Uri.outPtr(), nullptr);
    m_privateKeyPKCS11Uri = CString(privateKeyPKCS11Uri.get());
}

CoreIPCGTlsCertificate::CoreIPCGTlsCertificate(Vector<GRefPtr<GByteArray>>&& certificates, GRefPtr<GByteArray>&& privateKey, CString&& privateKeyPKCS11Uri)
    : m_certificates(WTF::move(certificates))
    , m_privateKey(WTF::move(privateKey))
    , m_privateKeyPKCS11Uri(WTF::move(privateKeyPKCS11Uri))
{
}

CoreIPCGTlsCertificate::operator GRefPtr<GTlsCertificate>() const
{
    if (m_certificates.isEmpty())
        return nullptr;

    GType certificateType = g_tls_backend_get_certificate_type(g_tls_backend_get_default());
    GRefPtr<GTlsCertificate> certificate;
    GTlsCertificate* issuer = nullptr;
    for (size_t i = 0; i < m_certificates.size(); ++i) {
        bool isLeaf = i == m_certificates.size() - 1;
        certificate = adoptGRef(G_TLS_CERTIFICATE(g_initable_new(
            certificateType, nullptr, nullptr,
            "certificate", m_certificates[i].get(),
            "issuer", issuer,
            "private-key", isLeaf ? m_privateKey.get() : nullptr,
            "private-key-pkcs11-uri", isLeaf ? m_privateKeyPKCS11Uri.data() : nullptr,
            nullptr)));
        issuer = certificate.get();
    }

    return certificate;
}

} // namespace WebKit

#endif // USE(GLIB)
