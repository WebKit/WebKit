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

#ifndef CertificateInfo_h
#define CertificateInfo_h

#if PLATFORM(MAC) || USE(CFNETWORK)
#include <wtf/RetainPtr.h>
#elif USE(SOUP)
#include <gio/gio.h>
#include <wtf/gobject/GRefPtr.h>
#endif

namespace WebCore {

class CertificateInfo {
public:
    CertificateInfo();

#if PLATFORM(MAC)
    explicit CertificateInfo(CFArrayRef certificateChain);
#elif USE(SOUP)
    explicit CertificateInfo(GTlsCertificate*, GTlsCertificateFlags);
#endif

    ~CertificateInfo();

#if PLATFORM(MAC) || USE(CFNETWORK)
    void setCertificateChain(CFArrayRef);
    CFArrayRef certificateChain() const;
#ifndef NDEBUG
    void dump() const;
#endif

#elif USE(SOUP)
    GTlsCertificate* certificate() const { return m_certificate.get(); }
    void setCertificate(GTlsCertificate* certificate) { m_certificate = certificate; }

    GTlsCertificateFlags tlsErrors() const { return m_tlsErrors; }
    void setTLSErrors(GTlsCertificateFlags tlsErrors) { m_tlsErrors = tlsErrors; }
#endif

private:
#if PLATFORM(MAC) || USE(CFNETWORK)
    // Certificate chain is normally part of NS/CFURLResponse, but there is no way to re-add it to a deserialized response after IPC.
    RetainPtr<CFArrayRef> m_certificateChain;
#elif USE(SOUP)
    GRefPtr<GTlsCertificate> m_certificate;
    GTlsCertificateFlags m_tlsErrors;
#endif
};

} // namespace WebCore

#endif // CertificateInfo_h
