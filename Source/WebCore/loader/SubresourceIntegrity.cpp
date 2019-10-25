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
#include "SubresourceIntegrity.h"

#include "CachedResource.h"
#include "HTMLParserIdioms.h"
#include "ParsingUtilities.h"
#include "ResourceCryptographicDigest.h"
#include "SharedBuffer.h"

namespace WebCore {

namespace {

template<typename CharacterType>
static bool isVCHAR(CharacterType c)
{
    return c >= 0x21 && c <= 0x7e;
}

template<typename CharacterType>
struct IntegrityMetadataParser {
public:
    IntegrityMetadataParser(Optional<Vector<EncodedResourceCryptographicDigest>>& digests)
        : m_digests(digests)
    {
    }

    bool operator()(const CharacterType*& position, const CharacterType* end)
    {
        // Initialize hashes to be something other WTF::nullopt, to indicate
        // that at least one token was seen, and thus setting the empty flag
        // from section 3.3.3 Parse metadata, to false.
        if (!m_digests)
            m_digests = Vector<EncodedResourceCryptographicDigest> { };

        auto digest = parseEncodedCryptographicDigest(position, end);
        if (!digest)
            return false;

        // The spec allows for options following the digest, but so far, no
        // specific options have been specified. Thus, we just parse and ignore
        // them. Their syntax is a '?' follow by any number of VCHARs.
        if (skipExactly<CharacterType>(position, end, '?'))
            skipWhile<CharacterType, isVCHAR>(position, end);

        // After the base64 value and options, the current character pointed to by position
        // should either be the end or a space.
        if (position != end && !isHTMLSpace(*position))
            return false;

        m_digests->append(WTFMove(*digest));
        return true;
    }

private:
    Optional<Vector<EncodedResourceCryptographicDigest>>& m_digests;
};

}

template <typename CharacterType, typename Functor>
static inline void splitOnSpaces(const CharacterType* begin, const CharacterType* end, Functor&& functor)
{
    const CharacterType* position = begin;

    skipWhile<CharacterType, isHTMLSpace>(position, end);

    while (position < end) {
        if (!functor(position, end))
            skipWhile<CharacterType, isNotHTMLSpace>(position, end);

        skipWhile<CharacterType, isHTMLSpace>(position, end);
    }
}

static Optional<Vector<EncodedResourceCryptographicDigest>> parseIntegrityMetadata(const String& integrityMetadata)
{
    if (integrityMetadata.isEmpty())
        return WTF::nullopt;

    Optional<Vector<EncodedResourceCryptographicDigest>> result;
    
    const StringImpl& stringImpl = *integrityMetadata.impl();
    if (stringImpl.is8Bit())
        splitOnSpaces(stringImpl.characters8(), stringImpl.characters8() + stringImpl.length(), IntegrityMetadataParser<LChar> { result });
    else
        splitOnSpaces(stringImpl.characters16(), stringImpl.characters16() + stringImpl.length(), IntegrityMetadataParser<UChar> { result });

    return result;
}

static bool isResponseEligible(const CachedResource& resource)
{
    // FIXME: The spec says this should check XXX.
    return resource.isCORSSameOrigin();
}

static Optional<EncodedResourceCryptographicDigest::Algorithm> prioritizedHashFunction(EncodedResourceCryptographicDigest::Algorithm a, EncodedResourceCryptographicDigest::Algorithm b)
{
    if (a == b)
        return WTF::nullopt;
    return (a > b) ? a : b;
}

static Vector<EncodedResourceCryptographicDigest> strongestMetadataFromSet(Vector<EncodedResourceCryptographicDigest>&& set)
{
    // 1. Let result be the empty set and strongest be the empty string.
    Vector<EncodedResourceCryptographicDigest> result;
    auto strongest = EncodedResourceCryptographicDigest::Algorithm::SHA256;

    // 2. For each item in set:
    for (auto& item : set) {
        // 1. If result is the empty set, add item to result and set strongest to item, skip to the next item.
        if (result.isEmpty()) {
            strongest = item.algorithm;
            result.append(WTFMove(item));
            continue;
        }
        
        // 2. Let currentAlgorithm be the alg component of strongest.
        auto currentAlgorithm = strongest;

        // 3. Let newAlgorithm be the alg component of item.
        auto newAlgorithm = item.algorithm;
        
        // 4. If the result of getPrioritizedHashFunction(currentAlgorithm, newAlgorithm) is
        //    the empty string, add item to result. If the result is newAlgorithm, set strongest
        //    to item, set result to the empty set, and add item to result.
        auto priority = prioritizedHashFunction(currentAlgorithm, newAlgorithm);
        if (!priority)
            result.append(WTFMove(item));
        else if (priority.value() == newAlgorithm) {
            strongest = item.algorithm;

            result.clear();
            result.append(WTFMove(item));
        }
    }

    return result;
}

bool matchIntegrityMetadata(const CachedResource& resource, const String& integrityMetadataList)
{
    // FIXME: Consider caching digests on the CachedResource rather than always recomputing it.

    // 1. Let parsedMetadata be the result of parsing metadataList.
    auto parsedMetadata = parseIntegrityMetadata(integrityMetadataList);
    
    // 2. If parsedMetadata is no metadata, return true.
    if (!parsedMetadata)
        return true;

    // 3. If response is not eligible for integrity validation, return false.
    if (!isResponseEligible(resource))
        return false;

    // 4. If parsedMetadata is the empty set, return true.
    if (parsedMetadata->isEmpty())
        return true;

    // 5. Let metadata be the result of getting the strongest metadata from parsedMetadata.
    auto metadata = strongestMetadataFromSet(WTFMove(*parsedMetadata));

    const auto* sharedBuffer = resource.resourceBuffer();
    
    // 6. For each item in metadata:
    for (auto& item : metadata) {
        // 1. Let algorithm be the alg component of item.
        auto algorithm = item.algorithm;
        
        // 2. Let expectedValue be the val component of item.
        auto expectedValue = decodeEncodedResourceCryptographicDigest(item);

        // 3. Let actualValue be the result of applying algorithm to response.
        auto actualValue = cryptographicDigestForBytes(algorithm, sharedBuffer ? sharedBuffer->data() : nullptr, sharedBuffer ? sharedBuffer->size() : 0);

        // 4. If actualValue is a case-sensitive match for expectedValue, return true.
        if (expectedValue && actualValue.value == expectedValue->value)
            return true;
    }
    
    return false;
}

String integrityMismatchDescription(const CachedResource& resource, const String& integrityMetadata)
{
    StringBuilder builder;

    builder.append(resource.url().stringCenterEllipsizedToLength());
    builder.append(". Failed integrity metadata check. ");
    builder.append("Content length: ");
    if (auto* resourceBuffer = resource.resourceBuffer())
        builder.appendNumber(resourceBuffer->size());
    else
        builder.append("(no content)");
    builder.append(", Expected content length: ");
    builder.appendNumber(resource.response().expectedContentLength());
    builder.append(", Expected metadata: ");
    builder.append(integrityMetadata);

    return builder.toString();
}

}
