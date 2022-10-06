/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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

#pragma once

#include "CertificateSummary.h"
#include "NotImplemented.h"
#include <libsoup/soup.h>
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>

namespace WebCore {

class ResourceError;
class ResourceResponse;

class CertificateInfo {
public:
    CertificateInfo();
    explicit CertificateInfo(const WebCore::ResourceResponse&);
    explicit CertificateInfo(const WebCore::ResourceError&);
    CertificateInfo(GTlsCertificate*, GTlsCertificateFlags);
    WEBCORE_EXPORT ~CertificateInfo();

    CertificateInfo isolatedCopy() const;

    GTlsCertificate* certificate() const { return m_certificate.get(); }
    void setCertificate(GTlsCertificate* certificate) { m_certificate = certificate; }
    GTlsCertificateFlags tlsErrors() const { return m_tlsErrors; }
    void setTLSErrors(GTlsCertificateFlags tlsErrors) { m_tlsErrors = tlsErrors; }

    bool containsNonRootSHA1SignedCertificate() const { notImplemented(); return false; }

    std::optional<CertificateSummary> summary() const;

    bool isEmpty() const { return !m_certificate; }

    bool operator==(const CertificateInfo& other) const
    {
        if (tlsErrors() != other.tlsErrors())
            return false;

        if (m_certificate.get() == other.m_certificate.get())
            return true;

        return m_certificate && other.m_certificate && g_tls_certificate_is_same(m_certificate.get(), other.m_certificate.get());
    }
    bool operator!=(const CertificateInfo& other) const { return !(*this == other); }

private:
    GRefPtr<GTlsCertificate> m_certificate;
    GTlsCertificateFlags m_tlsErrors;
};

} // namespace WebCore
