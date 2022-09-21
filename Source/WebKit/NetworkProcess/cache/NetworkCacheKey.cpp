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

#include "NetworkCacheCoders.h"
#include <wtf/ASCIICType.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {
namespace NetworkCache {

Key::Key(const Key& o)
    : m_partition(o.m_partition.isolatedCopy())
    , m_type(o.m_type.isolatedCopy())
    , m_identifier(o.m_identifier.isolatedCopy())
    , m_range(o.m_range.isolatedCopy())
    , m_hash(o.m_hash)
    , m_partitionHash(o.m_partitionHash)
{
}

Key::Key(const String& partition, const String& type, const String& range, const String& identifier, const Salt& salt)
    : m_partition(partition)
    , m_type(type)
    , m_identifier(identifier)
    , m_range(range)
    , m_hash(computeHash(salt))
    , m_partitionHash(computePartitionHash(salt))
{
}

Key::Key(WTF::HashTableDeletedValueType)
    : m_identifier(WTF::HashTableDeletedValue)
{
}

Key::Key(const DataKey& dataKey, const Salt& salt)
    : m_partition(dataKey.partition)
    , m_type(dataKey.type)
    , m_identifier(hashAsString(dataKey.identifier))
    , m_hash(computeHash(salt))
    , m_partitionHash(computePartitionHash(salt))
{
}

Key& Key::operator=(const Key& other)
{
    m_partition = other.m_partition.isolatedCopy();
    m_type = other.m_type.isolatedCopy();
    m_identifier = other.m_identifier.isolatedCopy();
    m_range = other.m_range.isolatedCopy();
    m_hash = other.m_hash;
    m_partitionHash = other.m_partitionHash;
    return *this;
}

static void hashString(SHA1& sha1, const String& string)
{
    if (string.isNull())
        return;

    if (string.is8Bit() && string.isAllASCII()) {
        const uint8_t nullByte = 0;
        sha1.addBytes(string.characters8(), string.length());
        sha1.addBytes(&nullByte, 1);
        return;
    }
    auto cString = string.utf8();
    // Include terminating null byte.
    sha1.addBytes(cString.dataAsUInt8Ptr(), cString.length() + 1);
}

Key::HashType Key::computeHash(const Salt& salt) const
{
    // We don't really need a cryptographic hash. The key is always verified against the entry header.
    // SHA1 just happens to be suitably sized, fast and available.
    SHA1 sha1;
    sha1.addBytes(salt.data(), salt.size());

    hashString(sha1, m_partition);
    hashString(sha1, m_type);
    hashString(sha1, m_identifier);
    hashString(sha1, m_range);

    SHA1::Digest hash;
    sha1.computeHash(hash);
    return hash;
}

Key::HashType Key::computePartitionHash(const Salt& salt) const
{
    SHA1 sha1;
    sha1.addBytes(salt.data(), salt.size());

    hashString(sha1, m_partition);

    SHA1::Digest hash;
    sha1.computeHash(hash);
    return hash;
}

String Key::hashAsString(const HashType& hash)
{
    StringBuilder builder;
    builder.reserveCapacity(hashStringLength());
    for (auto byte : hash) {
        builder.append(upperNibbleToASCIIHexDigit(byte));
        builder.append(lowerNibbleToASCIIHexDigit(byte));
    }
    return builder.toString();
}

template <typename CharType> bool hexDigitsToHash(CharType* characters, Key::HashType& hash)
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

bool Key::stringToHash(const String& string, HashType& hash)
{
    if (string.length() != hashStringLength())
        return false;
    if (string.is8Bit())
        return hexDigitsToHash(string.characters8(), hash);
    return hexDigitsToHash(string.characters16(), hash);
}

bool Key::operator==(const Key& other) const
{
    return m_hash == other.m_hash && m_partition == other.m_partition && m_type == other.m_type && m_identifier == other.m_identifier && m_range == other.m_range;
}

}
}
