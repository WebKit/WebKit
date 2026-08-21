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

#pragma once

#if USE(GLIB)

#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/CString.h>

typedef struct _GByteArray GByteArray;
typedef struct _GTlsCertificate GTlsCertificate;

namespace WebKit {

class CoreIPCGTlsCertificate {
public:
    explicit CoreIPCGTlsCertificate(const GRefPtr<GTlsCertificate>&);
    CoreIPCGTlsCertificate(Vector<GRefPtr<GByteArray>>&&, GRefPtr<GByteArray>&&, CString&&);

    const Vector<GRefPtr<GByteArray>>& certificates() const { return m_certificates; }
    const GRefPtr<GByteArray>& privateKey() const { return m_privateKey; }
    CString privateKeyPKCS11Uri() const { return m_privateKeyPKCS11Uri; }

    operator GRefPtr<GTlsCertificate>() const;

private:
    Vector<GRefPtr<GByteArray>> m_certificates;
    GRefPtr<GByteArray> m_privateKey;
    CString m_privateKeyPKCS11Uri;
};

} // namespace WebKit

#endif // USE(GLIB)
