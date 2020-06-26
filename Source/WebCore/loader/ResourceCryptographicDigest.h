/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#include <type_traits>
#include <wtf/HashTraits.h>
#include <wtf/Hasher.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CachedResource;

struct ResourceCryptographicDigest {
    enum class Algorithm {
        SHA256 = 1 << 0,
        SHA384 = 1 << 1,
        SHA512 = 1 << 2,
    };

    // Number of bytes to hold SHA-512 digest
    static constexpr size_t maximumDigestLength = 64;

    Algorithm algorithm;
    Vector<uint8_t> value;

    bool operator==(const ResourceCryptographicDigest& other) const
    {
        return algorithm == other.algorithm && value == other.value;
    }

    bool operator!=(const ResourceCryptographicDigest& other) const
    {
        return !(*this == other);
    }
};

struct EncodedResourceCryptographicDigest {
    using Algorithm = ResourceCryptographicDigest::Algorithm;
    
    Algorithm algorithm;
    String digest;
};

Optional<ResourceCryptographicDigest> parseCryptographicDigest(StringParsingBuffer<UChar>&);
Optional<ResourceCryptographicDigest> parseCryptographicDigest(StringParsingBuffer<LChar>&);

Optional<EncodedResourceCryptographicDigest> parseEncodedCryptographicDigest(StringParsingBuffer<UChar>&);
Optional<EncodedResourceCryptographicDigest> parseEncodedCryptographicDigest(StringParsingBuffer<LChar>&);

Optional<ResourceCryptographicDigest> decodeEncodedResourceCryptographicDigest(const EncodedResourceCryptographicDigest&);

ResourceCryptographicDigest cryptographicDigestForBytes(ResourceCryptographicDigest::Algorithm, const void* bytes, size_t length);

}

namespace WTF {

template<> struct DefaultHash<WebCore::ResourceCryptographicDigest> {
    struct Hash {
        static unsigned hash(const WebCore::ResourceCryptographicDigest& digest)
        {
            return pairIntHash(intHash(static_cast<unsigned>(digest.algorithm)), StringHasher::computeHash(digest.value.data(), digest.value.size()));
        }
        static bool equal(const WebCore::ResourceCryptographicDigest& a, const WebCore::ResourceCryptographicDigest& b)
        {
            return a == b;
        }
        static const bool safeToCompareToEmptyOrDeleted = true;
    };
};

template<> struct HashTraits<WebCore::ResourceCryptographicDigest> : GenericHashTraits<WebCore::ResourceCryptographicDigest> {
    using Algorithm = WebCore::ResourceCryptographicDigest::Algorithm;
    using AlgorithmUnderlyingType = typename std::underlying_type<Algorithm>::type;
    static constexpr auto emptyAlgorithmValue = static_cast<Algorithm>(std::numeric_limits<AlgorithmUnderlyingType>::max());
    static constexpr auto deletedAlgorithmValue = static_cast<Algorithm>(std::numeric_limits<AlgorithmUnderlyingType>::max() - 1);

    static const bool emptyValueIsZero = false;

    static WebCore::ResourceCryptographicDigest emptyValue()
    {
        return { emptyAlgorithmValue, { } };
    }

    static void constructDeletedValue(WebCore::ResourceCryptographicDigest& slot)
    {
        slot.algorithm = deletedAlgorithmValue;
    }

    static bool isDeletedValue(const WebCore::ResourceCryptographicDigest& slot)
    {
        return slot.algorithm == deletedAlgorithmValue;
    }
};

}
