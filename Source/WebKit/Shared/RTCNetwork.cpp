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

#include "config.h"

#if USE(LIBWEBRTC)
#include "RTCNetwork.h"

#include <wtf/CrossThreadCopier.h>
#include <wtf/StdLibExtras.h>
#include <wtf/posix/SocketPOSIX.h>

namespace WebKit {

/*
static_assert
 enum class EcnMarking : int {
     kNotEct = 0, // Not ECN-Capable Transport
     kEct1 = 1, // ECN-Capable Transport
     kEct0 = 2, // Not used by L4s (or webrtc.)
     kCe = 3, // Congestion experienced
 };

 */

RTCNetwork::RTCNetwork(String&& name, String&& description, IPAddress prefix, int prefixLength, int type, uint16_t id, int preference, bool active, bool ignored, int scopeID, Vector<InterfaceAddress>&& ips)
    : name(WTFMove(name))
    , description(WTFMove(description))
    , prefix(prefix)
    , prefixLength(prefixLength)
    , type(type)
    , id(id)
    , preference(preference)
    , active(active)
    , ignored(ignored)
    , scopeID(scopeID)
    , ips(WTFMove(ips)) { }

webrtc::Network RTCNetwork::value() const
{
    auto nameUTF8 = name.utf8();
    auto descriptionUTF8 = description.utf8();
    webrtc::Network network({ nameUTF8.span().data(), nameUTF8.span().size() }, { descriptionUTF8.span().data(), descriptionUTF8.span().size() }, prefix.rtcAddress(), prefixLength, webrtc::AdapterType(type));
    network.set_id(id);
    network.set_preference(preference);
    network.set_active(active);
    network.set_ignored(ignored);
    network.set_scope_id(scopeID);

    std::vector<webrtc::InterfaceAddress> vector;
    vector.reserve(ips.size());
    for (auto& ip : ips)
        vector.push_back(ip.rtcAddress());
    network.SetIPs(WTFMove(vector), true);

    return network;
}

RTCNetwork RTCNetwork::isolatedCopy() const
{
    return RTCNetwork {
        crossThreadCopy(name),
        crossThreadCopy(description),
        prefix,
        prefixLength,
        type,
        id,
        preference,
        active,
        ignored,
        scopeID,
        crossThreadCopy(ips)
    };
}

namespace WebRTCNetwork {

webrtc::SocketAddress SocketAddress::rtcAddress() const
{
    webrtc::SocketAddress result;
    result.SetPort(port);
    result.SetScopeID(scopeID);
    auto hostnameUTF8 = hostname.utf8();
    result.SetIP({ hostnameUTF8.span().data(), hostnameUTF8.span().size() });
    if (ipAddress)
        result.SetResolvedIP(ipAddress->rtcAddress());
    return result;
}

SocketAddress::SocketAddress(const webrtc::SocketAddress& value)
    : port(value.port())
    , scopeID(value.scope_id())
    , hostname(value.hostname())
    , ipAddress(value.IsUnresolvedIP() ? std::nullopt : std::optional(IPAddress(value.ipaddr())))
{
}

static std::array<uint32_t, 4> fromIPv6Address(const struct in6_addr& address)
{
    std::array<uint32_t, 4> array;
    static_assert(sizeof(array) == sizeof(address));
    memcpySpan(asMutableByteSpan(array), asByteSpan(address));
    return array;
}

IPAddress::IPAddress(const webrtc::IPAddress& input)
{
    switch (input.family()) {
    case AF_INET6:
        value = fromIPv6Address(input.ipv6_address());
        break;
    case AF_INET:
        value = input.ipv4_address().s_addr;
        break;
    case AF_UNSPEC:
        value = UnspecifiedFamily { };
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

IPAddress::IPAddress(const struct sockaddr& address)
{
    switch (address.sa_family) {
    case AF_INET6:
        value = fromIPv6Address(asIPV6SocketAddress(address).sin6_addr);
        break;
    case AF_INET:
        value = asIPV4SocketAddress(address).sin_addr.s_addr;
        break;
    case AF_UNSPEC:
        value = UnspecifiedFamily { };
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

webrtc::IPAddress IPAddress::rtcAddress() const
{
    return WTF::switchOn(value, [](UnspecifiedFamily) {
        return webrtc::IPAddress();
    }, [] (uint32_t ipv4) {
        in_addr addressv4;
        addressv4.s_addr = ipv4;
        return webrtc::IPAddress(addressv4);
    }, [] (std::array<uint32_t, 4> ipv6) {
        in6_addr result;
        static_assert(sizeof(ipv6) == sizeof(result));
        memcpySpan(asMutableByteSpan(result), asByteSpan(ipv6));
        return webrtc::IPAddress(result);
    });
}

webrtc::InterfaceAddress InterfaceAddress::rtcAddress() const
{
    return webrtc::InterfaceAddress(address.rtcAddress(), ipv6Flags);
}

}

}

#endif // USE(LIBWEBRTC)
