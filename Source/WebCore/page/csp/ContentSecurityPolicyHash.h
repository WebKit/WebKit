/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#ifndef ContentSecurityPolicyHash_h
#define ContentSecurityPolicyHash_h

#include <wtf/HashTraits.h>
#include <wtf/Hasher.h>
#include <wtf/Vector.h>

namespace WebCore {

// Keep this synchronized with the constant maximumContentSecurityPolicyDigestLength below.
enum class ContentSecurityPolicyHashAlgorithm {
    SHA_256 = 1 << 0,
    SHA_384 = 1 << 1,
    SHA_512 = 1 << 2,
};

const size_t maximumContentSecurityPolicyDigestLength = 64; // bytes to hold SHA-512 digest

typedef Vector<uint8_t> ContentSecurityPolicyDigest;
typedef std::pair<ContentSecurityPolicyHashAlgorithm, ContentSecurityPolicyDigest> ContentSecurityPolicyHash;

}

namespace WTF {

template<> struct DefaultHash<WebCore::ContentSecurityPolicyHashAlgorithm> { typedef IntHash<WebCore::ContentSecurityPolicyHashAlgorithm> Hash; };
template<> struct HashTraits<WebCore::ContentSecurityPolicyHashAlgorithm> : StrongEnumHashTraits<WebCore::ContentSecurityPolicyHashAlgorithm> { };
template<> struct DefaultHash<WebCore::ContentSecurityPolicyDigest> {
    struct Hash {
        static unsigned hash(const WebCore::ContentSecurityPolicyDigest& digest)
        {
            return StringHasher::computeHashAndMaskTop8Bits(digest.data(), digest.size());
        }
        static bool equal(const WebCore::ContentSecurityPolicyDigest& a, const WebCore::ContentSecurityPolicyDigest& b)
        {
            return a == b;
        }
        static const bool safeToCompareToEmptyOrDeleted = true;
    };
};

}

#endif // ContentSecurityPolicyHash_h
