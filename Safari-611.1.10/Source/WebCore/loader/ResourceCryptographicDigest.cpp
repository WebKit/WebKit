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
#include <wtf/text/StringParsingBuffer.h>

namespace WebCore {

template<typename CharacterType> static Optional<ResourceCryptographicDigest::Algorithm> parseHashAlgorithmAdvancingPosition(StringParsingBuffer<CharacterType>& buffer)
{
    // FIXME: This would be much cleaner with a lookup table of pairs of label / algorithm enum values, but I can't
    // figure out how to keep the labels compiletime strings for skipExactlyIgnoringASCIICase.

    if (skipExactlyIgnoringASCIICase(buffer, "sha256"))
        return ResourceCryptographicDigest::Algorithm::SHA256;
    if (skipExactlyIgnoringASCIICase(buffer, "sha384"))
        return ResourceCryptographicDigest::Algorithm::SHA384;
    if (skipExactlyIgnoringASCIICase(buffer, "sha512"))
        return ResourceCryptographicDigest::Algorithm::SHA512;

    return WTF::nullopt;
}

template<typename CharacterType> static Optional<ResourceCryptographicDigest> parseCryptographicDigestImpl(StringParsingBuffer<CharacterType>& buffer)
{
    if (buffer.atEnd())
        return WTF::nullopt;

    auto algorithm = parseHashAlgorithmAdvancingPosition(buffer);
    if (!algorithm)
        return WTF::nullopt;

    if (!skipExactly(buffer, '-'))
        return WTF::nullopt;

    auto beginHashValue = buffer.position();
    skipWhile<isBase64OrBase64URLCharacter>(buffer);
    skipExactly(buffer, '=');
    skipExactly(buffer, '=');

    if (buffer.position() == beginHashValue)
        return WTF::nullopt;

    Vector<uint8_t> digest;
    StringView hashValue(beginHashValue, buffer.position() - beginHashValue);
    if (!base64Decode(hashValue, digest, Base64ValidatePadding)) {
        if (!base64URLDecode(hashValue, digest))
            return WTF::nullopt;
    }

    return ResourceCryptographicDigest { *algorithm, WTFMove(digest) };
}

Optional<ResourceCryptographicDigest> parseCryptographicDigest(StringParsingBuffer<UChar>& buffer)
{
    return parseCryptographicDigestImpl(buffer);
}

Optional<ResourceCryptographicDigest> parseCryptographicDigest(StringParsingBuffer<LChar>& buffer)
{
    return parseCryptographicDigestImpl(buffer);
}

template<typename CharacterType> static Optional<EncodedResourceCryptographicDigest> parseEncodedCryptographicDigestImpl(StringParsingBuffer<CharacterType>& buffer)
{
    if (buffer.atEnd())
        return WTF::nullopt;

    auto algorithm = parseHashAlgorithmAdvancingPosition(buffer);
    if (!algorithm)
        return WTF::nullopt;

    if (!skipExactly(buffer, '-'))
        return WTF::nullopt;

    auto beginHashValue = buffer.position();
    skipWhile<isBase64OrBase64URLCharacter>(buffer);
    skipExactly(buffer, '=');
    skipExactly(buffer, '=');

    if (buffer.position() == beginHashValue)
        return WTF::nullopt;

    return EncodedResourceCryptographicDigest { *algorithm, String(beginHashValue, buffer.position() - beginHashValue) };
}

Optional<EncodedResourceCryptographicDigest> parseEncodedCryptographicDigest(StringParsingBuffer<UChar>& buffer)
{
    return parseEncodedCryptographicDigestImpl(buffer);
}

Optional<EncodedResourceCryptographicDigest> parseEncodedCryptographicDigest(StringParsingBuffer<LChar>& buffer)
{
    return parseEncodedCryptographicDigestImpl(buffer);
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
