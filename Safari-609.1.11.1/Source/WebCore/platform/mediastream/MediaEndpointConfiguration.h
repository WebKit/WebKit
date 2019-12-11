/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_RTC)

#include "RTCBundlePolicy.h"
#include "RTCIceTransportPolicy.h"
#include "RTCPMuxPolicy.h"
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct MediaEndpointConfiguration {
    // FIXME: We might be able to remove these constructors once all compilers can handle without it (see https://bugs.webkit.org/show_bug.cgi?id=163255#c15)
    struct IceServerInfo {
        Vector<URL> urls;
        String credential;
        String username;

        IceServerInfo(Vector<URL>&&, const String&, const String&);
    };
    struct CertificatePEM {
        String certificate;
        String privateKey;
    };

    MediaEndpointConfiguration(Vector<IceServerInfo>&&, RTCIceTransportPolicy, RTCBundlePolicy, RTCPMuxPolicy, unsigned short, Vector<CertificatePEM>&&);

    Vector<IceServerInfo> iceServers;
    RTCIceTransportPolicy iceTransportPolicy;
    RTCBundlePolicy bundlePolicy;
    RTCPMuxPolicy rtcpMuxPolicy;
    unsigned short iceCandidatePoolSize;
    Vector<CertificatePEM> certificates;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
