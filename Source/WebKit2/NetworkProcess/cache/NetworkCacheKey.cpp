/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

static NetworkCacheKey::HashType hashString(const String& string)
{
    // String::hash() masks away the top bits.
    if (string.is8Bit())
        return StringHasher::computeHash(string.characters8(), string.length());
    return StringHasher::computeHash(string.characters16(), string.length());
}

NetworkCacheKey::HashType NetworkCacheKey::computeHash() const
{
    return WTF::pairIntHash(hashString(m_method), WTF::pairIntHash(hashString(m_identifier), hashString(m_partition)));
}

String NetworkCacheKey::hashAsString() const
{
    return String::format("%08x", m_hash);
}

bool NetworkCacheKey::stringToHash(const String& string, HashType& hash)
{
    if (string.length() != 8)
        return false;
    bool success;
    hash = string.toUIntStrict(&success, 16);
    return success;
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
