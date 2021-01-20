/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_RTC)

#include "SecurityOrigin.h"
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class RTCCertificate : public RefCounted<RTCCertificate> {
public:
    struct DtlsFingerprint {
        String algorithm;
        String value;
    };

    static Ref<RTCCertificate> create(Ref<SecurityOrigin>&&, double expires, Vector<DtlsFingerprint>&&, String&& pemCertificate, String&& pemPrivateKey);

    double expires() const { return m_expires; }
    const Vector<DtlsFingerprint>& getFingerprints() const { return m_fingerprints; }

    const String& pemCertificate() const { return m_pemCertificate; }
    const String& pemPrivateKey() const { return m_pemPrivateKey; }
    const SecurityOrigin& origin() const { return m_origin.get(); }

private:
    RTCCertificate(Ref<SecurityOrigin>&&, double expires, Vector<DtlsFingerprint>&&, String&& pemCertificate, String&& pemPrivateKey);

    Ref<SecurityOrigin> m_origin;
    double m_expires { 0 };
    Vector<DtlsFingerprint> m_fingerprints;
    String m_pemCertificate;
    String m_pemPrivateKey;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
