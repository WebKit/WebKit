/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include <limits>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/TZoneMalloc.h>

namespace WTF {

template<> struct DefaultHash<webrtc::SocketAddress> {
    static unsigned hash(const webrtc::SocketAddress& address) { return address.Hash(); }
    static bool equal(const webrtc::SocketAddress& a, const webrtc::SocketAddress& b) { return a == b || (a.IsNil() && b.IsNil() && a.scope_id() == b.scope_id()); }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

template<> struct HashTraits<webrtc::SocketAddress> : GenericHashTraits<webrtc::SocketAddress> {
    static webrtc::SocketAddress emptyValue() { return webrtc::SocketAddress { }; }
    static void constructDeletedValue(webrtc::SocketAddress& address) { address.SetScopeID(std::numeric_limits<int>::min()); }
    static bool isDeletedValue(const webrtc::SocketAddress& address) { return address.scope_id() == std::numeric_limits<int>::min(); }
};

}

namespace WebKit {

class NetworkRTCUDPSocketCocoaConnections;

class NetworkRTCUDPSocketCocoa final : public NetworkRTCProvider::Socket {
    WTF_MAKE_TZONE_ALLOCATED(NetworkRTCUDPSocketCocoa);
public:
    static std::unique_ptr<NetworkRTCProvider::Socket> createUDPSocket(WebCore::LibWebRTCSocketIdentifier, NetworkRTCProvider&, const webrtc::SocketAddress&, uint16_t minPort, uint16_t maxPort, Ref<IPC::Connection>&&, String&& attributedBundleIdentifier, bool isFirstParty, bool isRelayDisabled, const WebCore::RegistrableDomain&);

    NetworkRTCUDPSocketCocoa(WebCore::LibWebRTCSocketIdentifier, NetworkRTCProvider&, const webrtc::SocketAddress&, Ref<IPC::Connection>&&, String&& attributedBundleIdentifier, bool isFirstParty, bool isRelayDisabled, const WebCore::RegistrableDomain&);
    ~NetworkRTCUDPSocketCocoa();

private:
    // NetworkRTCProvider::Socket.
    WebCore::LibWebRTCSocketIdentifier identifier() const final { return m_identifier; }
    Type type() const final { return Type::UDP; }
    void close() final;
    void setOption(int option, int value) final;
    void sendTo(std::span<const uint8_t>, const webrtc::SocketAddress&, const webrtc::AsyncSocketPacketOptions&) final;

    CheckedRef<NetworkRTCProvider> m_rtcProvider;
    WebCore::LibWebRTCSocketIdentifier m_identifier;
    const Ref<NetworkRTCUDPSocketCocoaConnections> m_connections;
};

} // namespace WebKit

#endif // USE(LIBWEBRTC) && PLATFORM(COCOA)
