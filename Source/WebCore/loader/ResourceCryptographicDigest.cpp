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

#include "config.h"
#include "ResourceCryptographicDigest.h"

#include "ParsingUtilities.h"
#include <pal/crypto/CryptoDigest.h>
#include <wtf/Optional.h>
#include <wtf/text/Base64.h>

namespace WebCore {

template<typename CharacterType>
static bool parseHashAlgorithmAdvancingPosition(const CharacterType*& position, const CharacterType* end, ResourceCryptographicDigest::Algorithm& algorithm)
{
    // FIXME: This would be much cleaner with a lookup table of pairs of label / algorithm enum values, but I can't
    // figure out how to keep the labels compiletime strings for skipExactlyIgnoringASCIICase.

    if (skipExactlyIgnoringASCIICase(position, end, "sha256")) {
        algorithm = ResourceCryptographicDigest::Algorithm::SHA256;
        return true;
    }
    if (skipExactlyIgnoringASCIICase(position, end, "sha384")) {
        algorithm = ResourceCryptographicDigest::Algorithm::SHA384;
        return true;
    }
    if (skipExactlyIgnoringASCIICase(position, end, "sha512")) {
        algorithm = ResourceCryptographicDigest::Algorithm::SHA512;
        return true;
    }

    return false;
}

template<typename CharacterType>
static Optional<ResourceCryptographicDigest> parseCryptographicDigestImpl(const CharacterType*& position, const CharacterType* end)
{
    if (position == end)
        return WTF::nullopt;

    ResourceCryptographicDigest::Algorithm algorithm;
    if (!parseHashAlgorithmAdvancingPosition(position, end, algorithm))
        return WTF::nullopt;

    if (!skipExactly<CharacterType>(position, end, '-'))
        return WTF::nullopt;

    const CharacterType* beginHashValue = position;
    skipWhile<CharacterType, isBase64OrBase64URLCharacter>(position, end);
    skipExactly<CharacterType>(position, end, '=');
    skipExactly<CharacterType>(position, end, '=');

    if (position == beginHashValue)
        return WTF::nullopt;

    Vector<uint8_t> digest;
    StringView hashValue(beginHashValue, position - beginHashValue);
    if (!base64Decode(hashValue, digest, Base64ValidatePadding)) {
        if (!base64URLDecode(hashValue, digest))
            return WTF::nullopt;
    }

    return ResourceCryptographicDigest { algorithm, WTFMove(digest) };
}

Optional<ResourceCryptographicDigest> parseCryptographicDigest(const UChar*& begin, const UChar* end)
{
    return parseCryptographicDigestImpl(begin, end);
}

Optional<ResourceCryptographicDigest> parseCryptographicDigest(const LChar*& begin, const LChar* end)
{
    return parseCryptographicDigestImpl(begin, end);
}

template<typename CharacterType>
static Optional<EncodedResourceCryptographicDigest> parseEncodedCryptographicDigestImpl(const CharacterType*& position, const CharacterType* end)
{
    if (position == end)
        return WTF::nullopt;

    EncodedResourceCryptographicDigest::Algorithm algorithm;
    if (!parseHashAlgorithmAdvancingPosition(position, end, algorithm))
        return WTF::nullopt;

    if (!skipExactly<CharacterType>(position, end, '-'))
        return WTF::nullopt;

    const CharacterType* beginHashValue = position;
    skipWhile<CharacterType, isBase64OrBase64URLCharacter>(position, end);
    skipExactly<CharacterType>(position, end, '=');
    skipExactly<CharacterType>(position, end, '=');

    if (position == beginHashValue)
        return WTF::nullopt;

    return EncodedResourceCryptographicDigest { algorithm, String(beginHashValue, position - beginHashValue) };
}

Optional<EncodedResourceCryptographicDigest> parseEncodedCryptographicDigest(const UChar*& begin, const UChar* end)
{
    return parseEncodedCryptographicDigestImpl(begin, end);
}

Optional<EncodedResourceCryptographicDigest> parseEncodedCryptographicDigest(const LChar*& begin, const LChar* end)
{
    return parseEncodedCryptographicDigestImpl(begin, end);
}

Optional<ResourceCryptographicDigest> decodeEncodedResourceCryptographicDigest(const EncodedResourceCryptographicDigest& encodedDigest)
{
    Vector<uint8_t> digest;
    if (!base64Decode(encodedDigest.digest, digest, Base64ValidatePadding)) {
        if (!base64URLDecode(encodedDigest.digest, digest))
            return WTF::nullopt;
    }

    return ResourceCryptographicDigest { encodedDigest.algorithm, WTFMove(digest) };
}

static PAL::CryptoDigest::Algorithm toCryptoDigestAlgorithm(ResourceCryptographicDigest::Algorithm algorithm)
{
    switch (algorithm) {
    case ResourceCryptographicDigest::Algorithm::SHA256:
        return PAL::CryptoDigest::Algorithm::SHA_256;
    case ResourceCryptographicDigest::Algorithm::SHA384:
        return PAL::CryptoDigest::Algorithm::SHA_384;
    case ResourceCryptographicDigest::Algorithm::SHA512:
        return PAL::CryptoDigest::Algorithm::SHA_512;
    }
    ASSERT_NOT_REACHED();
    return PAL::CryptoDigest::Algorithm::SHA_512;
}

ResourceCryptographicDigest cryptographicDigestForBytes(ResourceCryptographicDigest::Algorithm algorithm, const void* bytes, size_t length)
{
    auto cryptoDigest = PAL::CryptoDigest::create(toCryptoDigestAlgorithm(algorithm));
    cryptoDigest->addBytes(bytes, length);
    return { algorithm, cryptoDigest->computeHash() };
}

}
