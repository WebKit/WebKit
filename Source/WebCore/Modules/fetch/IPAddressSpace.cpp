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
#include "SecurityOrigin.h"
#include "Site.h"
#include <array>
#include <optional>
#include <span>
#include <wtf/ASCIICType.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/StringView.h>

namespace WebCore {

struct AddressRange {
    std::array<uint8_t, 16> address;
    uint8_t prefixLength;
    IPAddressSpace space;
};

static bool matchesPrefix(std::span<const uint8_t> bytes, const AddressRange& range)
{
    ASSERT(range.prefixLength <= bytes.size() * 8);
    size_t wholeBytes = range.prefixLength / 8;
    unsigned trailingBits = range.prefixLength % 8;
    for (size_t byteIndex = 0; byteIndex < wholeBytes; ++byteIndex) {
        if (bytes[byteIndex] != range.address[byteIndex])
            return false;
    }
    if (!trailingBits)
        return true;
    uint8_t mask = 0xFF << (8 - trailingBits);
    return (bytes[wholeBytes] & mask) == (range.address[wholeBytes] & mask);
}

static IPAddressSpace classifyAddress(std::span<const uint8_t> bytes, std::span<const AddressRange> ranges)
{
    for (auto& range : ranges) {
        if (matchesPrefix(bytes, range))
            return range.space;
    }
    return IPAddressSpace::Public;
}

static IPAddressSpace classifyIPv4Address(std::span<const uint8_t, 4> bytes)
{
    static constexpr AddressRange ranges[] = {
        { { 0, 0, 0, 0 }, 32, IPAddressSpace::Loopback },
        { { 127 }, 8, IPAddressSpace::Loopback },
        { { 198, 18 }, 15, IPAddressSpace::Loopback },
        { { 0 }, 8, IPAddressSpace::Local },
        { { 10 }, 8, IPAddressSpace::Local },
        { { 100, 64 }, 10, IPAddressSpace::Local },
        { { 172, 16 }, 12, IPAddressSpace::Local },
        { { 192, 168 }, 16, IPAddressSpace::Local },
        { { 169, 254 }, 16, IPAddressSpace::Local },
        { { 192, 0, 0 }, 24, IPAddressSpace::Local },
        { { 192, 0, 2 }, 24, IPAddressSpace::Local },
        { { 192, 88, 99 }, 24, IPAddressSpace::Local },
        { { 198, 51, 100 }, 24, IPAddressSpace::Local },
        { { 203, 0, 113 }, 24, IPAddressSpace::Local },
        { { 224 }, 4, IPAddressSpace::Local },
        { { 240 }, 4, IPAddressSpace::Local },
    };
    return classifyAddress(bytes, ranges);
}

// RFC 6052 embeds an IPv4 address in the bits immediately following the prefix, skipping the octet at
// bits 64-71 that the IPv6 addressing architecture reserves. Only the two prefixes with fixed values
// can be recognised here; an operator's network-specific prefix is not knowable.
static std::optional<std::array<uint8_t, 4>> nat64EmbeddedIPv4Address(std::span<const uint8_t, 16> bytes)
{
    // 64:ff9b::/96, the Well-Known Prefix (RFC 6052).
    static constexpr std::array<uint8_t, 12> wellKnownPrefix { 0x00, 0x64, 0xFF, 0x9B };
    if (equalSpans(bytes.first<12>(), std::span { wellKnownPrefix }))
        return std::array<uint8_t, 4> { bytes[12], bytes[13], bytes[14], bytes[15] };

    // 64:ff9b:1::/48, reserved for local use (RFC 8215), where the address straddles the reserved octet.
    static constexpr std::array<uint8_t, 6> localUsePrefix { 0x00, 0x64, 0xFF, 0x9B, 0x00, 0x01 };
    if (equalSpans(bytes.first<6>(), std::span { localUsePrefix }))
        return std::array<uint8_t, 4> { bytes[6], bytes[7], bytes[9], bytes[10] };

    return std::nullopt;
}

static IPAddressSpace classifyIPv6Address(std::span<const uint8_t, 16> bytes)
{
    static constexpr AddressRange ipv4MappedRange { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF }, 96, IPAddressSpace::Public };
    if (matchesPrefix(bytes, ipv4MappedRange))
        return classifyIPv4Address(bytes.subspan<12, 4>());

    if (auto embedded = nat64EmbeddedIPv4Address(bytes))
        return classifyIPv4Address(std::span<const uint8_t, 4> { *embedded });

    static constexpr AddressRange ranges[] = {
        { { }, 128, IPAddressSpace::Loopback },
        { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }, 128, IPAddressSpace::Loopback },
        { { 0xFC }, 7, IPAddressSpace::Local },
        { { 0xFE, 0x80 }, 10, IPAddressSpace::Local },
        { { 0xFE, 0xC0 }, 10, IPAddressSpace::Local },
        { { 0x20, 0x01, 0x0D, 0xB8 }, 32, IPAddressSpace::Local },
        { { 0x3F, 0xFF }, 20, IPAddressSpace::Local },
        { { 0xFF }, 8, IPAddressSpace::Local },
        { { 0x01 }, 64, IPAddressSpace::Local },
    };
    return classifyAddress(bytes, ranges);
}

static std::optional<std::array<uint8_t, 4>> parseIPv4Address(StringView host)
{
    std::array<uint8_t, 4> bytes { };
    size_t index = 0;
    for (auto octet : host.split('.')) {
        if (index >= 4)
            return std::nullopt;
        if (octet.isEmpty() || !isASCIIDigit(octet[0]))
            return std::nullopt;
        auto value = parseInteger<uint8_t>(octet);
        if (!value)
            return std::nullopt;
        bytes[index++] = *value;
    }
    if (index != 4)
        return std::nullopt;
    return bytes;
}

static std::optional<std::array<uint8_t, 16>> parseIPv6Address(StringView host)
{
    if (auto percent = host.find('%'); percent != notFound)
        host = host.left(percent);

    if (host.isEmpty())
        return std::nullopt;

    StringView head = host;
    StringView tail;
    bool isCompressed = false;
    if (auto doubleColon = host.find("::"_s); doubleColon != notFound) {
        isCompressed = true;
        head = host.left(doubleColon);
        tail = host.substring(doubleColon + 2);
        if (tail.contains("::"_s))
            return std::nullopt;
    }

    auto parseGroups = [](StringView text, std::span<uint8_t, 16> out, size_t offset) -> std::optional<size_t> {
        if (text.isEmpty())
            return 0;
        size_t written = 0;
        for (auto group : text.split(':')) {
            if (group.isEmpty())
                return std::nullopt;
            if (group.contains('.')) {
                auto ipv4 = parseIPv4Address(group);
                if (!ipv4 || offset + written + 4 > 16)
                    return std::nullopt;
                for (size_t i = 0; i < 4; ++i)
                    out[offset + written + i] = (*ipv4)[i];
                written += 4;
                continue;
            }
            if (group.length() > 4)
                return std::nullopt;
            auto value = parseInteger<uint16_t>(group, 16);
            if (!value || offset + written + 2 > 16)
                return std::nullopt;
            out[offset + written] = static_cast<uint8_t>(*value >> 8);
            out[offset + written + 1] = static_cast<uint8_t>(*value & 0xFF);
            written += 2;
        }
        return written;
    };

    std::array<uint8_t, 16> bytes { };
    auto headBytes = parseGroups(head, bytes, 0);
    if (!headBytes)
        return std::nullopt;

    if (!isCompressed)
        return *headBytes == 16 ? std::make_optional(bytes) : std::nullopt;

    std::array<uint8_t, 16> tailBuffer { };
    auto tailBytes = parseGroups(tail, tailBuffer, 0);
    if (!tailBytes || *headBytes + *tailBytes > 16)
        return std::nullopt;

    if (*headBytes + *tailBytes == 16)
        return std::nullopt;

    for (size_t i = 0; i < *tailBytes; ++i)
        bytes[16 - *tailBytes + i] = tailBuffer[i];

    return bytes;
}

static IPAddressSpace classifyHost(StringView host)
{
    if (host.contains(':')) {
        if (auto bytes = parseIPv6Address(host))
            return classifyIPv6Address(std::span<const uint8_t, 16> { *bytes });
        return IPAddressSpace::Public;
    }

    if (auto bytes = parseIPv4Address(host))
        return classifyIPv4Address(std::span<const uint8_t, 4> { *bytes });

    return IPAddressSpace::Public;
}

static IPAddressSpace determineIPAddressSpaceFromHost(StringView host)
{
    if (host.startsWith('[') && host.endsWith(']'))
        host = host.substring(1, host.length() - 2);

    if (host.endsWith('.'))
        host = host.left(host.length() - 1);

    // classifyHost() only parses address literals, so these reserved names need matching separately.
    if (SecurityOrigin::isLocalhostAddress(host))
        return IPAddressSpace::Loopback;

    if (host.endsWithIgnoringASCIICase(".local"_s))
        return IPAddressSpace::Local;

    return classifyHost(host);
}

IPAddressSpace determineIPAddressSpace(const URL& url)
{
    return determineIPAddressSpaceFromHost(url.host());
}

IPAddressSpace determineIPAddressSpace(const Site& site)
{
    return determineIPAddressSpaceFromHost(site.domain().string());
}

IPAddressSpace classifyIPAddressSpace(const IPAddress& address)
{
    if (address.isIPv4())
        return classifyIPv4Address(asByteSpan(address.ipv4Address()).first<4>());

    if (address.isIPv6())
        return classifyIPv6Address(asByteSpan(address.ipv6Address()).first<16>());

    return IPAddressSpace::Unknown;
}

} // namespace WebCore
