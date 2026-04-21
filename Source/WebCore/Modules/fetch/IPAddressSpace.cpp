/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "IPAddressSpace.h"

#include "DNS.h"
#include <arpa/inet.h>
#include <array>
#include <cstdio>
#include <netinet/in.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/StringView.h>

namespace WebCore {

IPAddressSpace determineIPAddressSpace(const URL& url)
{
    // Defined in https://wicg.github.io/local-network-access/#ip-address-space-section
    String host = url.host().toString();
    host = makeStringByReplacingAll(host, '[', ""_s);
    host = makeStringByReplacingAll(host, ']', ""_s);

    if (!URL::hostIsIPAddress(host))
        return IPAddressSpace::Public;

    // Handle IPv6 addresses (check for colon to distinguish from IPv4)
    if (host.contains(':')) {
        // ::1/128 - IPv6 loopback
        if (host == "::1")
            return IPAddressSpace::Local;

        // fc00::/7 - Unique Local Address - private
        if (host.startsWith("fc"_s) || host.startsWith("fd"_s))
            return IPAddressSpace::Private;

        // fe80::/10 - Link-Local Unicast - private
        if (host.startsWith("fe8"_s) || host.startsWith("fe9"_s) || host.startsWith("fea"_s) || host.startsWith("feb"_s))
            return IPAddressSpace::Private;
        // ::ffff: - IPv4 Mapped IPv6 Addresses - format for parsing by IPv4 Algorithm.
        if (host.startsWith("::ffff:"_s)) {
            host = host.substring(7);
            if (!host.contains('.')) {
                // Parse hex representation like "c0a8:101" -> "192.168.1.1"
                StringView hostView { host };
                auto colonPosition = hostView.find(':');
                if (colonPosition == notFound || hostView.find(':', colonPosition + 1) != notFound)
                    return IPAddressSpace::Public;

                auto value1 = parseInteger<uint16_t>(hostView.left(colonPosition), 16);
                auto value2 = parseInteger<uint16_t>(hostView.substring(colonPosition + 1), 16);

                if (!value1.has_value() || !value2.has_value())
                    return IPAddressSpace::Public;

                // Convert 16-bit hex values to dotted decimal IPv4 format
                uint16_t val1 = *value1;
                uint16_t val2 = *value2;

                host = makeString(
                    static_cast<unsigned>(val1 >> 8), '.',
                    static_cast<unsigned>(val1 & 0xFF), '.',
                    static_cast<unsigned>(val2 >> 8), '.',
                    static_cast<unsigned>(val2 & 0xFF)
                );
            }
        }
    }
    if (host.contains('.')) {
        std::array<uint8_t, 4> parts;
        size_t i = 0;
        for (auto octet : StringView(host).split('.')) {
            if (i >= 4)
                return IPAddressSpace::Public;
            auto value = parseInteger<uint8_t>(octet);
            if (!value)
                return IPAddressSpace::Public;
            parts[i++] = *value;
        }
        if (i != 4)
            return IPAddressSpace::Public;

        // Check IPv4 address blocks according to spec table:

        // 127.0.0.0/8 - IPv4 Loopback - local
        if (parts[0] == 127)
            return IPAddressSpace::Local;

        // 10.0.0.0/8 - Private Use - private
        if (parts[0] == 10)
            return IPAddressSpace::Private;

        // 100.64.0.0/10 - Carrier-Grade NAT - private
        if (parts[0] == 100 && (parts[1] & 0xC0) == 64)
            return IPAddressSpace::Private;

        // 172.16.0.0/12 - Private Use - private
        if (parts[0] == 172 && (parts[1] & 0xF0) == 16)
            return IPAddressSpace::Private;

        // 192.168.0.0/16 - Private Use - private
        if (parts[0] == 192 && parts[1] == 168)
            return IPAddressSpace::Private;

        // 198.18.0.0/15 - Benchmarking - private
        if (parts[0] == 198 && (parts[1] & 0xFE) == 18)
            return IPAddressSpace::Private;

        // 169.254.0.0/16 - Link Local - private
        if (parts[0] == 169 && parts[1] == 254)
            return IPAddressSpace::Private;

        return IPAddressSpace::Public;
    }
    return IPAddressSpace::Public;
}

static IPAddressSpace determineIPv4AddressSpace(const struct in_addr& addr)
{
    uint32_t ip = ntohl(addr.s_addr);
    uint8_t first = ip >> 24;
    uint8_t second = (ip >> 16) & 0xFF;

    // 127.0.0.0/8 - loopback
    if (first == 127)
        return IPAddressSpace::Local;

    // 10.0.0.0/8
    if (first == 10)
        return IPAddressSpace::Private;

    // 100.64.0.0/10
    if (first == 100 && (second & 0xC0) == 64)
        return IPAddressSpace::Private;

    // 172.16.0.0/12
    if (first == 172 && (second & 0xF0) == 16)
        return IPAddressSpace::Private;

    // 192.168.0.0/16
    if (first == 192 && second == 168)
        return IPAddressSpace::Private;

    // 198.18.0.0/15
    if (first == 198 && (second & 0xFE) == 18)
        return IPAddressSpace::Private;

    // 169.254.0.0/16
    if (first == 169 && second == 254)
        return IPAddressSpace::Private;

    return IPAddressSpace::Public;
}

static IPAddressSpace determineIPv6AddressSpace(const struct in6_addr& addr)
{
    // ::1/128 - loopback
    if (IN6_IS_ADDR_LOOPBACK(&addr))
        return IPAddressSpace::Local;

    // ::ffff:0:0/96 - IPv4-mapped
    if (IN6_IS_ADDR_V4MAPPED(&addr)) {
        struct in_addr v4addr;
        uint32_t mapped = (static_cast<uint32_t>(addr.s6_addr[12]) << 24)
            | (static_cast<uint32_t>(addr.s6_addr[13]) << 16)
            | (static_cast<uint32_t>(addr.s6_addr[14]) << 8)
            | static_cast<uint32_t>(addr.s6_addr[15]);
        v4addr.s_addr = htonl(mapped);
        return determineIPv4AddressSpace(v4addr);
    }

    auto byte0 = addr.s6_addr[0];
    auto byte1 = addr.s6_addr[1];

    // fc00::/7 - Unique Local Address
    if ((byte0 & 0xFE) == 0xFC)
        return IPAddressSpace::Private;

    // fe80::/10 - Link-Local
    if (byte0 == 0xFE && (byte1 & 0xC0) == 0x80)
        return IPAddressSpace::Private;

    return IPAddressSpace::Public;
}

IPAddressSpace determineIPAddressSpace(const IPAddress& address)
{
    if (address.isIPv4())
        return determineIPv4AddressSpace(address.ipv4Address());
    if (address.isIPv6())
        return determineIPv6AddressSpace(address.ipv6Address());
    return IPAddressSpace::Public;
}

bool isLocalIPAddressSpace(IPAddressSpace space)
{
    return space == IPAddressSpace::Private || space == IPAddressSpace::Local;
}

bool isLocalIPAddressSpace(const URL& url)
{
    auto space = determineIPAddressSpace(url);
    return space == IPAddressSpace::Private || space == IPAddressSpace::Local;
}

bool isPrivateNetworkRequest(IPAddressSpace source, IPAddressSpace target)
{
    if (source == IPAddressSpace::Public && target != IPAddressSpace::Public)
        return true;
    if (source == IPAddressSpace::Private && target == IPAddressSpace::Local)
        return true;
    return false;
}

} // namespace WebCore
