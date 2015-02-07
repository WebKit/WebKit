/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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
#include "NetworkCacheKey.h"

#if ENABLE(NETWORK_CACHE)

#include "NetworkCacheCoders.h"
#include <wtf/ASCIICType.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {

NetworkCacheKey::NetworkCacheKey(const NetworkCacheKey& o)
    : m_method(o.m_method.isolatedCopy())
    , m_partition(o.m_partition.isolatedCopy())
    , m_identifier(o.m_identifier.isolatedCopy())
    , m_hash(o.m_hash)
{
}

NetworkCacheKey::NetworkCacheKey(NetworkCacheKey&& o)
    : m_method(WTF::move(o.m_method))
    , m_partition(WTF::move(o.m_partition))
    , m_identifier(WTF::move(o.m_identifier))
    , m_hash(o.m_hash)
{
}

NetworkCacheKey::NetworkCacheKey(const String& method, const String& partition, const String& identifier)
    : m_method(method.isolatedCopy())
    , m_partition(partition.isolatedCopy())
    , m_identifier(identifier.isolatedCopy())
    , m_hash(computeHash())
{
}

static void hashString(MD5& md5, const String& string)
{
    const uint8_t zero = 0;
    if (string.containsOnlyASCII()) {
        md5.addBytes(string.characters8(), string.length());
        md5.addBytes(&zero, 1);
        return;
    }
    auto cString = string.utf8();
    md5.addBytes(reinterpret_cast<const uint8_t*>(cString.data()), cString.length());
    md5.addBytes(&zero, 1);
}

NetworkCacheKey::HashType NetworkCacheKey::computeHash() const
{
    // We don't really need a cryptographic hash. The key is always verified against the entry header.
    // MD5 just happens to be suitably sized, fast and available.
    MD5 md5;
    hashString(md5, m_method);
    hashString(md5, m_partition);
    hashString(md5, m_identifier);
    MD5::Digest hash;
    md5.checksum(hash);
    return hash;
}

String NetworkCacheKey::hashAsString() const
{
    StringBuilder builder;
    builder.reserveCapacity(hashStringLength());
    for (auto byte : m_hash) {
        builder.append(upperNibbleToASCIIHexDigit(byte));
        builder.append(lowerNibbleToASCIIHexDigit(byte));
    }
    return builder.toString();
}

template <typename CharType> bool hexDigitsToHash(CharType* characters, NetworkCacheKey::HashType& hash)
{
    for (unsigned i = 0; i < sizeof(hash); ++i) {
        auto high = characters[2 * i];
        auto low = characters[2 * i + 1];
        if (!isASCIIHexDigit(high) || !isASCIIHexDigit(low))
            return false;
        hash[i] = toASCIIHexValue(high, low);
    }
    return true;
}

bool NetworkCacheKey::stringToHash(const String& string, HashType& hash)
{
    if (string.length() != hashStringLength())
        return false;
    if (string.is8Bit())
        return hexDigitsToHash(string.characters8(), hash);
    return hexDigitsToHash(string.characters16(), hash);
}

bool NetworkCacheKey::operator==(const NetworkCacheKey& other) const
{
    return m_hash == other.m_hash && m_method == other.m_method && m_partition == other.m_partition && m_identifier == other.m_identifier;
}

void NetworkCacheKey::encode(NetworkCacheEncoder& encoder) const
{
    encoder << m_method;
    encoder << m_partition;
    encoder << m_identifier;
    encoder << m_hash;
}

bool NetworkCacheKey::decode(NetworkCacheDecoder& decoder, NetworkCacheKey& key)
{
    return decoder.decode(key.m_method) && decoder.decode(key.m_partition) && decoder.decode(key.m_identifier) && decoder.decode(key.m_hash);
}

}

#endif
