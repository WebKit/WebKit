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

#include "DataReference.h"
#include "WebCoreArgumentCoders.h"

namespace WebKit {

RTCNetwork::RTCNetwork(const rtc::Network& network)
    : name(network.name())
    , description(network.description())
    , prefix { network.prefix() }
    , prefixLength(network.prefix_length())
    , type(network.type())
    , id(network.id())
    , preference(network.preference())
    , active(network.active())
    , ignored(network.ignored())
    , scopeID(network.scope_id())
    , ips(network.GetIPs())
{
}

rtc::Network RTCNetwork::value() const
{
    rtc::Network network(name.data(), description.data(), prefix.value, prefixLength, rtc::AdapterType(type));
    network.set_id(id);
    network.set_preference(preference);
    network.set_active(active);
    network.set_ignored(ignored);
    network.set_scope_id(scopeID);
    network.SetIPs(ips, true);
    return network;
}

bool RTCNetwork::IPAddress::decode(IPC::Decoder& decoder, IPAddress& result)
{
    int family;
    if (!decoder.decode(family))
        return false;

    ASSERT(family == AF_INET || family == AF_INET6 || family == AF_UNSPEC);

    if (family == AF_UNSPEC)
        return true;

    IPC::DataReference data;
    if (!decoder.decode(data))
        return false;

    if (family == AF_INET) {
        if (data.size() != sizeof(in_addr))
            return false;
        result.value = rtc::IPAddress(*reinterpret_cast<const in_addr*>(data.data()));
        return true;
    }

    if (data.size() != sizeof(in6_addr))
        return false;
    result.value = rtc::IPAddress(*reinterpret_cast<const in6_addr*>(data.data()));
    return true;
}

void RTCNetwork::IPAddress::encode(IPC::Encoder& encoder) const
{
    auto family = value.family();
    ASSERT(family == AF_INET || family == AF_INET6 || family == AF_UNSPEC);
    encoder << family;

    if (family == AF_UNSPEC)
        return;

    if (family == AF_INET) {
        auto address = value.ipv4_address();
        encoder << IPC::DataReference(reinterpret_cast<const uint8_t*>(&address), sizeof(address));
        return;
    }

    auto address = value.ipv6_address();
    encoder << IPC::DataReference(reinterpret_cast<const uint8_t*>(&address), sizeof(address));
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

bool RTCNetwork::SocketAddress::decode(IPC::Decoder& decoder, SocketAddress& result)
{
    uint16_t port;
    if (!decoder.decode(port))
        return false;
    int scopeId;
    if (!decoder.decode(scopeId))
        return false;
    result.value.SetPort(port);
    result.value.SetScopeID(scopeId);

    IPC::DataReference hostname;
    if (!decoder.decode(hostname))
        return false;
    result.value.SetIP(std::string(reinterpret_cast<const char*>(hostname.data()), hostname.size()));

    bool isUnresolved;
    if (!decoder.decode(isUnresolved))
        return false;
    if (isUnresolved)
        return true;

    RTCNetwork::IPAddress ipAddress;
    if (!decoder.decode(ipAddress))
        return false;
    result.value.SetResolvedIP(ipAddress.value);
    return true;
}

void RTCNetwork::SocketAddress::encode(IPC::Encoder& encoder) const
{
    encoder << value.port();
    encoder << value.scope_id();

    auto hostname = value.hostname();
    encoder << IPC::DataReference(reinterpret_cast<const uint8_t*>(hostname.data()), hostname.length());

    if (value.IsUnresolvedIP()) {
        encoder << true;
        return;
    }
    encoder << false;
    encoder << RTCNetwork::IPAddress(value.ipaddr());
}

bool RTCNetwork::decode(IPC::Decoder& decoder, RTCNetwork& result)
{
    IPC::DataReference name, description;
    if (!decoder.decode(name))
        return false;
    result.name = std::string(reinterpret_cast<const char*>(name.data()), name.size());
    if (!decoder.decode(description))
        return false;
    result.description = std::string(reinterpret_cast<const char*>(description.data()), description.size());
    if (!decoder.decode(result.prefix))
        return false;
    if (!decoder.decode(result.prefixLength))
        return false;
    if (!decoder.decode(result.type))
        return false;
    if (!decoder.decode(result.id))
        return false;
    if (!decoder.decode(result.preference))
        return false;
    if (!decoder.decode(result.active))
        return false;
    if (!decoder.decode(result.ignored))
        return false;
    if (!decoder.decode(result.scopeID))
        return false;

    uint64_t length;
    if (!decoder.decode(length))
        return false;
    result.ips.reserve(length);
    for (size_t index = 0; index < length; ++index) {
        IPAddress address;
        if (!decoder.decode(address))
            return false;
        int flags;
        if (!decoder.decode(flags))
            return false;
        result.ips.push_back({ address.value, flags });
    }
    return true;
}

void RTCNetwork::encode(IPC::Encoder& encoder) const
{
    encoder << IPC::DataReference(reinterpret_cast<const uint8_t*>(name.data()), name.length());
    encoder << IPC::DataReference(reinterpret_cast<const uint8_t*>(description.data()), description.length());
    encoder << prefix;
    encoder << prefixLength;
    encoder << type;

    encoder << id;
    encoder << preference;
    encoder << active;
    encoder << ignored;
    encoder << scopeID;

    encoder << (uint64_t)ips.size();
    for (auto& ip : ips) {
        encoder << IPAddress { ip };
        encoder << ip.ipv6_flags();
    }
}

}

#endif // USE(LIBWEBRTC)
