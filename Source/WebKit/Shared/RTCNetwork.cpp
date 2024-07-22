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
#include "RTCNetwork.h"

#if USE(LIBWEBRTC)

namespace WebKit {

RTCNetwork::RTCNetwork(Vector<char>&& name, Vector<char>&& description, IPAddress prefix, int prefixLength, int type, uint16_t id, int preference, bool active, bool ignored, int scopeID, Vector<InterfaceAddress>&& ips)
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

rtc::Network RTCNetwork::value() const
{
    rtc::Network network({ name.data(), name.size() }, { description.data(), description.size() }, prefix.rtcAddress(), prefixLength, rtc::AdapterType(type));
    network.set_id(id);
    network.set_preference(preference);
    network.set_active(active);
    network.set_ignored(ignored);
    network.set_scope_id(scopeID);

    std::vector<rtc::InterfaceAddress> vector;
    vector.reserve(ips.size());
    for (auto& ip : ips)
        vector.push_back(ip.rtcAddress());
    network.SetIPs(WTFMove(vector), true);

    return network;
}

namespace RTC::Network {

rtc::SocketAddress SocketAddress::rtcAddress() const
{
    rtc::SocketAddress result;
    result.SetPort(port);
    result.SetScopeID(scopeID);
    result.SetIP({ hostname.data(), hostname.size() });
    if (ipAddress)
        result.SetResolvedIP(ipAddress->rtcAddress());
    return result;
}

SocketAddress::SocketAddress(const rtc::SocketAddress& value)
    : port(value.port())
    , scopeID(value.scope_id())
    , hostname(std::span { value.hostname() })
    , ipAddress(value.IsUnresolvedIP() ? std::nullopt : std::optional(IPAddress(value.ipaddr())))
{
}

static std::array<uint32_t, 4> fromIPv6Address(const struct in6_addr& address)
{
    std::array<uint32_t, 4> array;
    static_assert(sizeof(array) == sizeof(address));
    memcpy(array.data(), &address, sizeof(array));
    return array;
}

IPAddress::IPAddress(const rtc::IPAddress& input)
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
        value = fromIPv6Address(reinterpret_cast<const sockaddr_in6*>(&address)->sin6_addr);
        break;
    case AF_INET:
        value = reinterpret_cast<const sockaddr_in*>(&address)->sin_addr.s_addr;
        break;
    case AF_UNSPEC:
        value = UnspecifiedFamily { };
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

rtc::IPAddress IPAddress::rtcAddress() const
{
    return WTF::switchOn(value, [](UnspecifiedFamily) {
        return rtc::IPAddress();
    }, [] (uint32_t ipv4) {
        in_addr addressv4;
        addressv4.s_addr = ipv4;
        return rtc::IPAddress(addressv4);
    }, [] (std::array<uint32_t, 4> ipv6) {
        in6_addr result;
        static_assert(sizeof(ipv6) == sizeof(result));
        memcpy(&result, ipv6.data(), sizeof(ipv6));
        return rtc::IPAddress(result);
    });
}

rtc::InterfaceAddress InterfaceAddress::rtcAddress() const
{
    return rtc::InterfaceAddress(address.rtcAddress(), ipv6Flags);
}

}

}

#endif // USE(LIBWEBRTC)
