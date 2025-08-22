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

#include <array>
#include <cstdio>
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
        // ::1/128 - IPv6 Local - loopback
        if (host == "::1")
            return IPAddressSpace::Local;

        // fc00::/7 - Unique Loopback - local
        if (host.startsWith("fc"_s) || host.startsWith("fd"_s))
            return IPAddressSpace::Local;

        // fe80::/10 - Link-Loopback Unicast - local
        if (host.startsWith("fe8"_s) || host.startsWith("fe9"_s) || host.startsWith("fea"_s) || host.startsWith("feb"_s))
            return IPAddressSpace::Local;
        // ::ffff: - IPv4 Mapped IPv6 Addresses - format for parsing by IPv4 Algorithm.
        if (host.startsWith("::ffff:"_s)) {
            host = host.substring(7);
            if (!host.contains('.')) {
                // Parse hex representation like "c0a8:101" -> "192.168.1.1"
                Vector<String> halves = host.split(':');
                if (halves.size() != 2)
                    return IPAddressSpace::Public;

                auto value1 = parseInteger<uint16_t>(halves[0], 16);
                auto value2 = parseInteger<uint16_t>(halves[1], 16);

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
        Vector<String> octets = host.split('.');
        if (octets.size() != 4)
            return IPAddressSpace::Public;

        std::array<uint8_t, 4> parts;
        for (size_t i = 0; i < 4; i++) {
            auto value = parseInteger<uint8_t>(octets[i]);
            if (!value)
                return IPAddressSpace::Public;
            parts[i] = *value;
        }

        // Check IPv4 address blocks according to spec table:

        // 127.0.0.0/8 - IPv4 Loopback - loopback
        if (parts[0] == 127)
            return IPAddressSpace::Local;

        // 10.0.0.0/8 - Local Use - local
        if (parts[0] == 10)
            return IPAddressSpace::Local;

        // 100.64.0.0/10 - Carrier-Grade NAT - local
        if (parts[0] == 100 && (parts[1] & 0xC0) == 64)
            return IPAddressSpace::Local;

        // 172.16.0.0/12 - Local Use - local
        if (parts[0] == 172 && (parts[1] & 0xF0) == 16)
            return IPAddressSpace::Local;

        // 192.168.0.0/16 - Local Use - local
        if (parts[0] == 192 && parts[1] == 168)
            return IPAddressSpace::Local;

        // 198.18.0.0/15 - Benchmarking - local
        if (parts[0] == 198 && (parts[1] & 0xFE) == 18)
            return IPAddressSpace::Local;

        // 169.254.0.0/16 - Link Local - local
        if (parts[0] == 169 && parts[1] == 254)
            return IPAddressSpace::Local;

        return IPAddressSpace::Public;
    }
    return IPAddressSpace::Public;
}

bool isLocalIPAddressSpace(const URL& url)
{
    return determineIPAddressSpace(url) == IPAddressSpace::Local;
}

} // namespace WebCore
