/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
 * Copyright (C) 2012, 2018 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DNS.h"

#include "DNSResolveQueue.h"
#include <wtf/MainThread.h>

#if OS(UNIX)
#include <arpa/inet.h>
#endif

namespace WebCore {

void prefetchDNS(const String& hostname)
{
    ASSERT(isMainThread());
    if (hostname.isEmpty())
        return;

    DNSResolveQueue::singleton().add(hostname);
}

void resolveDNS(const String& hostname, uint64_t identifier, DNSCompletionHandler&& completionHandler)
{
    ASSERT(isMainThread());
    if (hostname.isEmpty())
        return;

    WebCore::DNSResolveQueue::singleton().resolve(hostname, identifier, WTFMove(completionHandler));
}

void stopResolveDNS(uint64_t identifier)
{
    WebCore::DNSResolveQueue::singleton().stopResolve(identifier);
}

std::optional<IPAddress> IPAddress::fromString(const String& string)
{
#if OS(UNIX)
    struct in6_addr addressV6;
    if (inet_pton(AF_INET6, string.utf8().data(), &addressV6))
        return IPAddress { addressV6 };

    struct in_addr addressV4;
    if (inet_pton(AF_INET, string.utf8().data(), &addressV4))
        return IPAddress { addressV4 };
#else
    // FIXME: Add support for this method on Windows.
    UNUSED_PARAM(string);
#endif
    return std::nullopt;
}

IPAddress IPAddress::isolatedCopy() const
{
    if (isIPv4())
        return IPAddress { ipv4Address() };

    if (isIPv6())
        return IPAddress { ipv6Address() };

    RELEASE_ASSERT_NOT_REACHED();
    return IPAddress { WTF::HashTableEmptyValue };
}

unsigned IPAddress::matchingNetMaskLength(const IPAddress& other) const
{
    const unsigned char* addressData = nullptr;
    const unsigned char* otherAddressData = nullptr;
    size_t addressLengthInBytes = 0;
    if (isIPv4() && other.isIPv4()) {
        addressLengthInBytes = sizeof(struct in_addr);
        addressData = reinterpret_cast<const unsigned char*>(&ipv4Address());
        otherAddressData = reinterpret_cast<const unsigned char*>(&other.ipv4Address());
    } else if (isIPv6() && other.isIPv6()) {
        addressLengthInBytes = sizeof(struct in6_addr);
        addressData = reinterpret_cast<const unsigned char*>(&ipv6Address());
        otherAddressData = reinterpret_cast<const unsigned char*>(&other.ipv6Address());
    } else
        return 0;

    std::optional<size_t> firstNonMatchingIndex;
    unsigned matchingBits = 0;
    for (size_t i = 0; i < addressLengthInBytes; ++i) {
        if (addressData[i] != otherAddressData[i]) {
            firstNonMatchingIndex = i;
            break;
        }
        matchingBits += 8;
    }

    if (firstNonMatchingIndex) {
        // Count the number of matching leading bits in the first byte that is different between the two addresses.
        // For instance, 0xF2 and 0xF3 would have 7 matching bits.
        matchingBits += clz<unsigned char>(addressData[*firstNonMatchingIndex] ^ otherAddressData[*firstNonMatchingIndex]);
    }

    return matchingBits;
}

}
