/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <WebCore/LibWebRTCMacros.h>
#include <optional>
#include <wtf/Forward.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

IGNORE_CLANG_WARNINGS_BEGIN("nullability-completeness")
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <webrtc/api/transport/ecn_marking.h>
#include <webrtc/rtc_base/ip_address.h>
#include <webrtc/rtc_base/socket_address.h>
#include <webrtc/rtc_base/network.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
IGNORE_CLANG_WARNINGS_END

namespace WebKit {

namespace WebRTCNetwork {

// This enums corresponds to webrtc::EcnMarking.
enum class EcnMarking : int {
    kNotEct = static_cast<int>(webrtc::EcnMarking::kNotEct), // Not ECN-Capable Transport
    kEct1 = static_cast<int>(webrtc::EcnMarking::kEct1), // ECN-Capable Transport
    kEct0 = static_cast<int>(webrtc::EcnMarking::kEct0), // Not used by L4s (or webrtc.)
    kCe = static_cast<int>(webrtc::EcnMarking::kCe), // Congestion experienced
};

struct IPAddress {
    struct UnspecifiedFamily { };

    IPAddress() = default;
    explicit IPAddress(const webrtc::IPAddress&);
    explicit IPAddress(const struct sockaddr&);
    explicit IPAddress(Variant<UnspecifiedFamily, uint32_t, std::array<uint32_t, 4>> value)
        : value(value)
    {
    }

    IPAddress isolatedCopy() const { return *this; }
    webrtc::IPAddress rtcAddress() const;

    bool isUnspecified() const { return std::holds_alternative<UnspecifiedFamily>(value); }

    Variant<UnspecifiedFamily, uint32_t, std::array<uint32_t, 4>> value;
};

struct InterfaceAddress {
    explicit InterfaceAddress(IPAddress address, int ipv6Flags)
        : address(address), ipv6Flags(ipv6Flags)
    {
    }

    webrtc::InterfaceAddress rtcAddress() const;
    InterfaceAddress isolatedCopy() const { return *this; }

    IPAddress address;
    int ipv6Flags;
};

struct SocketAddress {
    explicit SocketAddress(const webrtc::SocketAddress&);
    explicit SocketAddress(uint16_t port, int scopeID, String&& hostname, std::optional<IPAddress> ipAddress)
        : port(port)
        , scopeID(scopeID)
        , hostname(WTFMove(hostname))
        , ipAddress(ipAddress) { }

    webrtc::SocketAddress rtcAddress() const;

    uint16_t port;
    int scopeID;
    String hostname;
    std::optional<IPAddress> ipAddress;
};

} // namespace WebRTCNetwork

struct RTCNetwork {
    using SocketAddress = WebRTCNetwork::SocketAddress;
    using IPAddress = WebRTCNetwork::IPAddress;
    using InterfaceAddress = WebRTCNetwork::InterfaceAddress;

    RTCNetwork() = default;
    explicit RTCNetwork(String&& name, String&& description, IPAddress prefix, int prefixLength, int type, uint16_t id, int preference, bool active, bool ignored, int scopeID, Vector<InterfaceAddress>&& ips);
    RTCNetwork isolatedCopy() const;

    webrtc::Network value() const;

    String name;
    String description;
    IPAddress prefix;
    int prefixLength { 0 };
    int type { 0 };
    uint16_t id { 0 };
    int preference { 0 };
    bool active { 0 };
    bool ignored { false };
    int scopeID { 0 };
    Vector<InterfaceAddress> ips;
};

}

#endif // USE(LIBWEBRTC)
