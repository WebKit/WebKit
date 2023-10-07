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

RTCNetwork::RTCNetwork(const rtc::Network& network)
    : name(network.name().data(), network.name().length())
    , description(network.description().data(), network.description().length())
    , prefix { network.prefix() }
    , prefixLength(network.prefix_length())
    , type(network.type())
    , id(network.id())
    , preference(network.preference())
    , active(network.active())
    , ignored(network.ignored())
    , scopeID(network.scope_id())
    , ips(WTF::map(network.GetIPs(), [] (const rtc::InterfaceAddress& address) {
        return RTCNetwork::InterfaceAddress(address);
    })) { }

RTCNetwork::RTCNetwork(Vector<char>&& name, Vector<char>&& description, IPAddress prefix, int prefixLength, int type, uint16_t id, int preference, bool active, bool ignored, int scopeID, Vector<char>&& key, size_t length, Vector<InterfaceAddress>&& ips)
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
    , key(WTFMove(key))
    , length(length)
    , ips(WTFMove(ips)) { }

rtc::Network RTCNetwork::value() const
{
    rtc::Network network(name.data(), description.data(), prefix.rtcAddress(), prefixLength, rtc::AdapterType(type));
    network.set_id(id);
    network.set_preference(preference);
    network.set_active(active);
    network.set_ignored(ignored);
    network.set_scope_id(scopeID);
    auto rtcInterfaceAddresses = WTF::map(ips, [] (const RTCNetwork::InterfaceAddress& address) {
        return address.rtcAddress();
    });
    network.SetIPs(std::vector<rtc::InterfaceAddress>(rtcInterfaceAddresses.begin(), rtcInterfaceAddresses.end()), true);
    return network;
}

rtc::SocketAddress RTCNetwork::isolatedCopy(const rtc::SocketAddress& value)
{
    rtc::SocketAddress copy;
    copy.SetPort(value.port());
    copy.SetScopeID(value.scope_id());
    copy.SetIP(std::string(value.hostname().data(), value.hostname().size()));
    if (!value.IsUnresolvedIP())
        copy.SetResolvedIP(value.ipaddr());
    return rtc::SocketAddress(copy);
}

namespace RTC::Network {

IPAddress::IPAddress(const rtc::IPAddress& input)
{
    switch (input.family()) {
    case AF_INET6: {
        in6_addr addr = input.ipv6_address();
        std::array<uint32_t, 4> array;
        static_assert(sizeof(array) == sizeof(addr));
        memcpy(array.data(), &addr, sizeof(array));
        value = array;
        return;
    }
    case AF_INET:
        value = input.ipv4_address().s_addr;
        return;
    case AF_UNSPEC:
        value = UnspecifiedFamily { };
        return;
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

SocketAddress::SocketAddress(const rtc::SocketAddress& address)
    : hostname(address.hostname().data(), address.hostname().length())
    , ip(address.IsUnresolvedIP() ? std::nullopt : std::optional(address.ipaddr()))
    , port(address.port())
    , scopeID(address.scope_id()) { }

SocketAddress::SocketAddress(Vector<char>&& hostname, std::optional<IPAddress> ip, uint16_t port, int scopeID)
    : hostname(WTFMove(hostname))
    , ip(ip)
    , port(port)
    , scopeID(scopeID) { }

rtc::SocketAddress SocketAddress::rtcAddress() const
{
    rtc::SocketAddress result;
    result.SetPort(port);
    result.SetScopeID(scopeID);
    result.SetIP(std::string(hostname.data(), hostname.size()));
    if (ip)
        result.SetResolvedIP(ip->rtcAddress());
    return result;
}

InterfaceAddress::InterfaceAddress(const rtc::InterfaceAddress& address)
    : address(address)
    , ipv6Flags(address.ipv6_flags()) { }

rtc::InterfaceAddress InterfaceAddress::rtcAddress() const
{
    return rtc::InterfaceAddress(address.rtcAddress(), ipv6Flags);
}

}

}

#endif // USE(LIBWEBRTC)
