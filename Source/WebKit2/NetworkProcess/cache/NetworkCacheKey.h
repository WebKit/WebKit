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

#ifndef NetworkCacheKey_h
#define NetworkCacheKey_h

#if ENABLE(NETWORK_CACHE)

#include <wtf/SHA1.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
namespace NetworkCache {

class Encoder;
class Decoder;

class Key {
public:
    typedef SHA1::Digest HashType;

    Key() { }
    Key(const Key&);
    Key(Key&&) = default;
    Key(const String& partition, const String& type, const String& range, const String& identifier);

    Key& operator=(const Key&);
    Key& operator=(Key&&) = default;

    Key(WTF::HashTableDeletedValueType);
    bool isHashTableDeletedValue() const { return m_identifier.isHashTableDeletedValue(); }

    bool isNull() const { return m_identifier.isNull(); }

    bool hasPartition() const;
    const String& partition() const { return m_partition; }
    const String& identifier() const { return m_identifier; }
    const String& type() const { return m_type; }
    const String& range() const { return m_range; }

    HashType hash() const { return m_hash; }

    static bool stringToHash(const String&, HashType&);

    static size_t hashStringLength() { return 2 * sizeof(m_hash); }
    String hashAsString() const;

    void encode(Encoder&) const;
    static bool decode(Decoder&, Key&);

    bool operator==(const Key&) const;
    bool operator!=(const Key& other) const { return !(*this == other); }

private:
    HashType computeHash() const;

    String m_partition;
    String m_type;
    String m_identifier;
    String m_range;
    HashType m_hash;
};

}
}

namespace WTF {

struct NetworkCacheKeyHash {
    static unsigned hash(const WebKit::NetworkCache::Key& key)
    {
        static_assert(SHA1::hashSize >= sizeof(unsigned), "Hash size must be greater than sizeof(unsigned)");
        return *reinterpret_cast<const unsigned*>(key.hash().data());
    }

    static bool equal(const WebKit::NetworkCache::Key& a, const WebKit::NetworkCache::Key& b)
    {
        return a == b;
    }

    static const bool safeToCompareToEmptyOrDeleted = false;
};

template<typename T> struct DefaultHash;
template<> struct DefaultHash<WebKit::NetworkCache::Key> {
    typedef NetworkCacheKeyHash Hash;
};

template<> struct HashTraits<WebKit::NetworkCache::Key> : SimpleClassHashTraits<WebKit::NetworkCache::Key> {
    static const bool emptyValueIsZero = false;

    static const bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const WebKit::NetworkCache::Key& key) { return key.isNull(); }
};

} // namespace WTF

#endif
#endif
