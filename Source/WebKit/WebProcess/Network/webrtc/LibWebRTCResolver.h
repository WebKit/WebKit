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

#include "LibWebRTCResolverIdentifier.h"
#include <WebCore/LibWebRTCMacros.h>
#include <wtf/Vector.h>

ALLOW_COMMA_BEGIN

#include <webrtc/api/packet_socket_factory.h>
#include <webrtc/rtc_base/async_resolver_interface.h>

ALLOW_COMMA_END

namespace IPC {
class Connection;
}

namespace WebKit {
class LibWebRTCSocketFactory;

class LibWebRTCResolver final : public rtc::AsyncResolverInterface {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LibWebRTCResolver() : m_identifier(LibWebRTCResolverIdentifier::generate()) { }

    bool isResolving() const { return m_isResolving; }
    LibWebRTCResolverIdentifier identifier() const { return m_identifier; }

private:
    friend class WebRTCResolver;

    // AsyncResolverInterface API.
    void Start(const rtc::SocketAddress&) final;
    bool GetResolvedAddress(int, rtc::SocketAddress*) const final;
    int GetError() const final { return m_error; }
    void Destroy(bool) final;

    void doDestroy();
    void setError(int);
    void setResolvedAddress(const Vector<rtc::IPAddress>&);

    static void sendOnMainThread(Function<void(IPC::Connection&)>&&);

    LibWebRTCResolverIdentifier m_identifier;
    Vector<rtc::IPAddress> m_addresses;
    rtc::SocketAddress m_addressToResolve;
    int m_error { 0 };
    uint16_t m_port { 0 };
    bool m_isResolving { false };
    bool m_isProvidingResults { false };
    bool m_shouldDestroy { false };
};

} // namespace WebKit

#endif // USE(LIBWEBRTC)
