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

#include <wtf/MD5.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class NetworkCacheEncoder;
class NetworkCacheDecoder;

class NetworkCacheKey {
public:
    typedef MD5::Digest HashType;

    NetworkCacheKey() { }
    NetworkCacheKey(const NetworkCacheKey&);
    NetworkCacheKey(NetworkCacheKey&&);
    NetworkCacheKey(const String& method, const String& partition, const String& identifier);

    NetworkCacheKey& operator=(const NetworkCacheKey&);

    bool isNull() const { return m_identifier.isNull(); }

    const String& method() const { return m_method; }
    const String& partition() const { return m_partition; }
    const String& identifier() const { return m_identifier; }

    HashType hash() const { return m_hash; }
    unsigned shortHash() const  { return toShortHash(m_hash); }

    static unsigned toShortHash(const HashType& hash) { return *reinterpret_cast<const unsigned*>(hash.data()); }
    static bool stringToHash(const String&, HashType&);

    static size_t hashStringLength() { return 2 * sizeof(m_hash); }
    String hashAsString() const;

    void encode(NetworkCacheEncoder&) const;
    static bool decode(NetworkCacheDecoder&, NetworkCacheKey&);

    bool operator==(const NetworkCacheKey&) const;
    bool operator!=(const NetworkCacheKey& other) const { return !(*this == other); }

private:
    HashType computeHash() const;

    String m_method;
    String m_partition;
    String m_identifier;
    HashType m_hash;
};

}

#endif
#endif
