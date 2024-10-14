/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

#if USE(LIBWEBRTC) && PLATFORM(COCOA)

#include "NetworkRTCProvider.h"
#include <Network/Network.h>
#include <wtf/TZoneMalloc.h>

namespace rtc {
class SocketAddress;
}

namespace WebKit {

class NetworkRTCTCPSocketCocoa final : public NetworkRTCProvider::Socket {
    WTF_MAKE_TZONE_ALLOCATED(NetworkRTCTCPSocketCocoa);
public:
    static std::unique_ptr<NetworkRTCProvider::Socket> createClientTCPSocket(WebCore::LibWebRTCSocketIdentifier, NetworkRTCProvider&, const rtc::SocketAddress&, int options, const String& attributedBundleIdentifier, bool isFirstParty, bool isRelayDisabled, const WebCore::RegistrableDomain&, Ref<IPC::Connection>&&);

    NetworkRTCTCPSocketCocoa(WebCore::LibWebRTCSocketIdentifier, NetworkRTCProvider&, const rtc::SocketAddress&, int options, const String& attributedBundleIdentifier, bool isFirstParty, bool isRelayDisabled, const WebCore::RegistrableDomain&, Ref<IPC::Connection>&&);
    ~NetworkRTCTCPSocketCocoa();

    static void getInterfaceName(NetworkRTCProvider&, const URL&, const String& attributedBundleIdentifier, bool isFirstParty, bool isRelayDisabled, const WebCore::RegistrableDomain&, CompletionHandler<void(String&&)>&&);

private:
    // NetworkRTCProvider::Socket.
    WebCore::LibWebRTCSocketIdentifier identifier() const final { return m_identifier; }
    Type type() const final { return Type::ClientTCP; }
    void close() final;
    void setOption(int option, int value) final;
    void sendTo(std::span<const uint8_t>, const rtc::SocketAddress&, const rtc::PacketOptions&) final;

    Vector<uint8_t> createMessageBuffer(std::span<const uint8_t>);

    WebCore::LibWebRTCSocketIdentifier m_identifier;
    CheckedRef<NetworkRTCProvider> m_rtcProvider;
    Ref<IPC::Connection> m_connection;
    RetainPtr<nw_connection_t> m_nwConnection;
    bool m_isSTUN { false };
#if ASSERT_ENABLED
    bool m_isClosed { false };
#endif
};

} // namespace WebKit

#endif // USE(LIBWEBRTC) && PLATFORM(COCOA)
