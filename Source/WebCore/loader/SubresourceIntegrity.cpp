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
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "ResourceCryptographicDigest.h"
#include "SharedBuffer.h"
#include "SubresourceLoader.h"
#include "ViolationReportType.h"
#include <wtf/text/Base64.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/StringParsingBuffer.h>

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
    IntegrityMetadataParser(std::optional<Vector<EncodedResourceCryptographicDigest>>& digests)
        : m_digests(digests)
    {
    }

    bool operator()(StringParsingBuffer<CharacterType>& buffer)
    {
        // Initialize hashes to be something other std::nullopt, to indicate
        // that at least one token was seen, and thus setting the empty flag
        // from section 3.3.3 Parse metadata, to false.
        if (!m_digests)
            m_digests = Vector<EncodedResourceCryptographicDigest> { };

        auto digest = parseEncodedCryptographicDigest(buffer);
        if (!digest)
            return false;

        // The spec allows for options following the digest, but so far, no
        // specific options have been specified. Thus, we just parse and ignore
        // them. Their syntax is a '?' follow by any number of VCHARs.
        if (skipExactly(buffer, '?'))
            skipWhile<isVCHAR>(buffer);

        // After the base64 value and options, the current character pointed to by position
        // should either be the end or a space.
        if (!buffer.atEnd() && !isASCIIWhitespace(*buffer))
            return false;

        m_digests->append(WTFMove(*digest));
        return true;
    }

private:
    std::optional<Vector<EncodedResourceCryptographicDigest>>& m_digests;
};

}

template <typename CharacterType, typename Functor>
static inline void splitOnSpaces(StringParsingBuffer<CharacterType> buffer, Functor&& functor)
{
    skipWhile<isASCIIWhitespace>(buffer);

    while (buffer.hasCharactersRemaining()) {
        if (!functor(buffer))
            skipWhile<isNotASCIIWhitespace>(buffer);
        skipWhile<isASCIIWhitespace>(buffer);
    }
}

std::optional<Vector<EncodedResourceCryptographicDigest>> parseIntegrityMetadata(const String& integrityMetadata)
{
    if (integrityMetadata.isEmpty())
        return std::nullopt;

    std::optional<Vector<EncodedResourceCryptographicDigest>> result;
    
    readCharactersForParsing(integrityMetadata, [&result]<typename CharacterType> (StringParsingBuffer<CharacterType> buffer) {
        splitOnSpaces(buffer, IntegrityMetadataParser<CharacterType> { result });
    });

    return result;
}

static bool isResponseEligible(const CachedResource& resource)
{
    // FIXME: The spec says this should check XXX.
    return resource.isCORSSameOrigin();
}

static std::optional<EncodedResourceCryptographicDigest::Algorithm> prioritizedHashFunction(EncodedResourceCryptographicDigest::Algorithm a, EncodedResourceCryptographicDigest::Algorithm b)
{
    if (a == b)
        return std::nullopt;
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

static Ref<FormData> createReportFormData(const String& type, const URL& url, const String& userAgent, NOESCAPE const Function<void(JSON::Object&)>& populateBody)
{
    auto body = JSON::Object::create();
    populateBody(body);

    // https://www.w3.org/TR/reporting-1/#queue-report, step 2.3.1.
    auto reportObject = JSON::Object::create();
    reportObject->setObject("body"_s, WTFMove(body));
    reportObject->setString("type"_s, type);
    reportObject->setString("user_agent"_s, userAgent);
    // The spec allows user agents to delay report sending, in order to reduce impact on the user and potential overhead. See https://www.w3.org/TR/reporting-1/#delivery
    // Currently we're not taking advantage of that, so setting the `age` to 0 to indicate immediate delivery.
    reportObject->setInteger("age"_s, 0);
    reportObject->setInteger("attempts"_s, 0);
    if (url.isValid())
        reportObject->setString("url"_s, url.strippedForUseAsReferrer().string);

    auto reportList = JSON::Array::create();
    reportList->pushObject(reportObject);

    return FormData::create(reportList->toJSONString().utf8());
}

static String addHashPrefix(ResourceCryptographicDigest::Algorithm algorithm, StringView hash)
{
    switch (algorithm) {
    case ResourceCryptographicDigest::Algorithm::SHA256:
        return makeString("sha256-"_s, hash);
    case ResourceCryptographicDigest::Algorithm::SHA384:
        return makeString("sha384-"_s, hash);
    case ResourceCryptographicDigest::Algorithm::SHA512:
        return makeString("sha512-"_s, hash);
    }
    ASSERT_NOT_REACHED();
    return String();
}

static std::optional<ResourceCryptographicDigest::Algorithm> findStrongestAlgorithm(HashAlgorithmSet algorithmSet)
{
    for (int i = ResourceCryptographicDigest::algorithmCount - 1; i >= 0; --i) {
        uint8_t algorithm = (1 << i);
        if (algorithmSet & algorithm)
            return static_cast<ResourceCryptographicDigest::Algorithm>(algorithm);
    }
    return std::nullopt;
}

void reportHashesIfNeeded(const CachedResource& resource)
{
    RefPtr loader = resource.loader();
    if (!loader)
        return;
    RefPtr frame = loader->frame();
    if (!frame)
        return;
    RefPtr document = frame->document();
    if (!document)
        return;

    CheckedRef csp = *document->contentSecurityPolicy();
    URL documentURL = document->url();

    auto& hashesToReport = csp->hashesToReport();
    if (hashesToReport.isEmpty())
        return;

    bool canExposeHashes = isResponseEligible(resource);
    for (auto& [algorithmSet, fixedEndpoints] : hashesToReport) {
        auto hashAlgorithm = findStrongestAlgorithm(algorithmSet);
        if (!hashAlgorithm)
            return;

        String hash = ""_s;
        if (canExposeHashes)
            hash = addHashPrefix(hashAlgorithm.value(), base64EncodeToString(resource.cryptographicDigest(hashAlgorithm.value()).value));
        Ref report = createReportFormData("csp-hash"_s, documentURL, document->httpUserAgent(), [&](auto& body) {
            body.setString("documentURL"_s, documentURL.strippedForUseAsReferrer().string);
            body.setString("subresourceURL"_s, resource.url().strippedForUseAsReferrer().string);
            body.setString("hash"_s, hash);
            body.setString("type"_s, "subresource"_s);
            body.setString("destination"_s, "script"_s);
        });
        document->sendReportToEndpoints(documentURL, { }, fixedEndpoints, WTFMove(report), ViolationReportType::CSPHashReport);
    }
}

bool matchIntegrityMetadataSlow(const CachedResource& resource, const String& integrityMetadataList)
{
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
    
    // 6. For each item in metadata:
    for (auto& item : metadata) {
        // 1. Let algorithm be the alg component of item.
        auto algorithm = item.algorithm;
        
        // 2. Let expectedValue be the val component of item.
        auto expectedValue = decodeEncodedResourceCryptographicDigest(item);

        // 3. Let actualValue be the result of applying algorithm to response.
        auto actualValue = resource.cryptographicDigest(algorithm);

        // 4. If actualValue is a case-sensitive match for expectedValue, return true.
        if (expectedValue && actualValue.value == expectedValue->value)
            return true;
    }
    
    return false;
}

String integrityMismatchDescription(const CachedResource& resource, const String& integrityMetadata)
{
    auto resourceURL = resource.url().stringCenterEllipsizedToLength();
    if (RefPtr resourceBuffer = resource.resourceBuffer()) {
        return makeString(resourceURL, ". Failed integrity metadata check. Content length: "_s, resourceBuffer->size(), ", Expected content length: "_s,
            resource.response().expectedContentLength(), ", Expected metadata: "_s, integrityMetadata);
    }
    return makeString(resourceURL, ". Failed integrity metadata check. Content length: (no content), Expected content length: "_s,
        resource.response().expectedContentLength(), ", Expected metadata: "_s, integrityMetadata);
}

}
