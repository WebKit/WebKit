/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if USE(LIBWEBRTC)
#include "LibWebRTCSocketFactory.h"
#include "WebRTCMonitor.h"
#include "WebRTCResolver.h"
#include "WebRTCSocket.h"
#endif

#include "WebMDNSRegister.h"

namespace WebKit {

class LibWebRTCNetwork {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LibWebRTCNetwork() = default;

#if USE(LIBWEBRTC)
    WebRTCMonitor& monitor() { return m_webNetworkMonitor; }
    LibWebRTCSocketFactory& socketFactory() { return m_socketFactory; }

    void disableNonLocalhostConnections() { socketFactory().disableNonLocalhostConnections(); }

    WebRTCSocket socket(uint64_t identifier) { return WebRTCSocket(socketFactory(), identifier); }
    WebRTCResolver resolver(uint64_t identifier) { return WebRTCResolver(socketFactory(), identifier); }
#endif

#if ENABLE(WEB_RTC)
    WebMDNSRegister& mdnsRegister() { return m_mdnsRegister; }
#endif

private:
#if USE(LIBWEBRTC)
    LibWebRTCSocketFactory m_socketFactory;
    WebRTCMonitor m_webNetworkMonitor;
#endif
#if ENABLE(WEB_RTC)
    WebMDNSRegister m_mdnsRegister;
#endif
};

} // namespace WebKit
