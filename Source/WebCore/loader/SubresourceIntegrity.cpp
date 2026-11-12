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
#include "FetchOptionsDestination.h"
#include "HTTPStatusCodes.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "ResourceCryptographicDigest.h"
#include "ResourceError.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include "SubresourceLoader.h"
#include "ViolationReportType.h"
#include <wtf/ScopedLambda.h>
#include <wtf/text/Base64.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

// https://w3c.github.io/webappsec-subresource-integrity/#valid-sri-hash-algorithm-token
static std::optional<EncodedResourceCryptographicDigest::Algorithm> validSRIHashAlgorithmToken(StringView string)
{
    // A string is a valid SRI hash algorithm token if its ASCII lowercase is contained in the valid SRI hash algorithm token set.
    if (equalLettersIgnoringASCIICase(string, "sha256"_s))
        return EncodedResourceCryptographicDigest::Algorithm::SHA256;
    if (equalLettersIgnoringASCIICase(string, "sha384"_s))
        return EncodedResourceCryptographicDigest::Algorithm::SHA384;
    if (equalLettersIgnoringASCIICase(string, "sha512"_s))
        return EncodedResourceCryptographicDigest::Algorithm::SHA512;
    return std::nullopt;
}

// Base64 or base64url characters, followed by at most two U+003D (=).
static bool isBase64OrBase64URLValue(StringView value)
{
    auto end = value.length();
    for (unsigned i = 0; i < 2 && end && value[end - 1] == '='; ++i)
        --end;
    if (!end)
        return false;
    for (unsigned i = 0; i < end; ++i) {
        if (!isBase64OrBase64URLCharacter(value[i]))
            return false;
    }
    return true;
}

// https://w3c.github.io/webappsec-subresource-integrity/#parse-metadata-section
Vector<EncodedResourceCryptographicDigest> parseIntegrityMetadata(const String& metadata)
{
    // 1. Let result be the empty set.
    Vector<EncodedResourceCryptographicDigest> result;

    // 2. For each item returned by splitting metadata on spaces:
    // FIXME: The spec links "splitting" to strictly splitting on U+0020 SPACE; this splits on ASCII whitespace,
    // as earlier drafts did, so that values spanning several lines keep working.
    auto metadataWithSingleSpaces = metadata.simplifyWhiteSpace(isASCIIWhitespace);
    for (auto item : StringView { metadataWithSingleSpaces }.split(' ')) {
        // 1. Let expression-and-options be the result of splitting item on U+003F (?).
        // 2. Let algorithm-expression be expression-and-options[0].
        auto algorithmExpression = item.left(item.find('?'));

        // 3. Let base64-value be the empty string.
        StringView base64Value = emptyString();

        // 4. Let algorithm-and-value be the result of splitting algorithm-expression on U+002D (-).
        // 5. Let algorithm be algorithm-and-value[0].
        auto firstHyphen = algorithmExpression.find('-');
        auto algorithm = validSRIHashAlgorithmToken(algorithmExpression.left(firstHyphen));

        // 6. If algorithm is not a valid SRI hash algorithm token, then continue.
        if (!algorithm)
            continue;

        // 7. If algorithm-and-value[1] exists, set base64-value to algorithm-and-value[1].
        // FIXME: This keeps everything after the first U+002D (-), so that base64url values, which can contain one, keep working.
        if (firstHyphen != notFound)
            base64Value = algorithmExpression.substring(firstHyphen + 1);

        // FIXME: The spec keeps a value that is empty or not base64; this skips the token instead, as other engines do and WPT expects.
        if (!isBase64OrBase64URLValue(base64Value))
            continue;

        // 8. Let metadata be the ordered map «["alg" → algorithm, "val" → base64-value]».
        // 9. Append metadata to result.
        result.append({ *algorithm, base64Value.toString() });
    }

    // 3. Return result.
    return result;
}

// https://fetch.spec.whatwg.org/#main-fetch filters the response "depending on request's response tainting".
// WebKit does not filter subresource responses, so this derives the filtered response's type from the tainting.
NODELETE static ResourceResponse::Type filteredResponseType(ResourceResponse::Tainting tainting)
{
    switch (tainting) {
    case ResourceResponse::Tainting::Basic:
        return ResourceResponse::Type::Basic;
    case ResourceResponse::Tainting::Cors:
        return ResourceResponse::Type::Cors;
    case ResourceResponse::Tainting::Opaque:
        return ResourceResponse::Type::Opaque;
    case ResourceResponse::Tainting::Opaqueredirect:
        return ResourceResponse::Type::Opaqueredirect;
    }
    ASSERT_NOT_REACHED();
    return ResourceResponse::Type::Opaque;
}

// https://html.spec.whatwg.org/multipage/urls-and-fetching.html#cors-same-origin
NODELETE static bool isCORSSameOrigin(ResourceResponse::Type type)
{
    // A response whose type is "basic", "cors", or "default" is CORS-same-origin.
    switch (type) {
    case ResourceResponse::Type::Basic:
    case ResourceResponse::Type::Cors:
    case ResourceResponse::Type::Default:
        return true;
    case ResourceResponse::Type::Error:
    case ResourceResponse::Type::Opaque:
    case ResourceResponse::Type::Opaqueredirect:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

// https://w3c.github.io/webappsec-subresource-integrity/#get-the-strongest-metadata
static Vector<EncodedResourceCryptographicDigest> strongestMetadataFromSet(Vector<EncodedResourceCryptographicDigest>&& set)
{
    // 1. Let result be the empty set and strongest be null.
    Vector<EncodedResourceCryptographicDigest> result;
    std::optional<EncodedResourceCryptographicDigest::Algorithm> strongest;

    // 2. For each item in set:
    for (auto& item : set) {
        // 1. Assert: item["alg"] is a valid SRI hash algorithm token.
        // NOTE: Algorithm can only hold valid tokens at this point.
        ASSERT(item.algorithm == EncodedResourceCryptographicDigest::Algorithm::SHA256
            || item.algorithm == EncodedResourceCryptographicDigest::Algorithm::SHA384
            || item.algorithm == EncodedResourceCryptographicDigest::Algorithm::SHA512);

        // 2. If result is the empty set, then:
        if (result.isEmpty()) {
            // 1. Append item to result.
            // 2. Set strongest to item.
            strongest = item.algorithm;
            result.append(WTF::move(item));

            // 3. Continue.
            continue;
        }

        // 3. Let currentAlgorithm be strongest["alg"], and currentAlgorithmIndex be the index of currentAlgorithm in the valid SRI hash algorithm token set.
        // 4. Let newAlgorithm be the item["alg"], and newAlgorithmIndex be the index of newAlgorithm in the valid SRI hash algorithm token set.
        // (Algorithm's values are ordered the same way as the valid SRI hash algorithm token set.)
        auto currentAlgorithm = *strongest;
        auto newAlgorithm = item.algorithm;

        // 5. If newAlgorithmIndex is less than currentAlgorithmIndex, continue.
        if (newAlgorithm < currentAlgorithm)
            continue;

        // 6. Otherwise, if newAlgorithmIndex is greater than currentAlgorithmIndex:
        if (newAlgorithm > currentAlgorithm) {
            // 1. Set strongest to item.
            // 2. Set result to « item ».
            strongest = item.algorithm;
            result.clear();
            result.append(WTF::move(item));
            continue;
        }

        // 7. Otherwise, newAlgorithmIndex and currentAlgorithmIndex are the same value. Append item to result.
        result.append(WTF::move(item));
    }

    // 3. Return result.
    return result;
}

static Ref<FormData> createReportFormData(const String& type, const URL& url, const String& userAgent, NOESCAPE const Function<void(JSON::Object&)>& populateBody)
{
    auto body = JSON::Object::create();
    populateBody(body);

    // https://www.w3.org/TR/reporting-1/#queue-report, step 2.3.1.
    auto reportObject = JSON::Object::create();
    reportObject->setObject("body"_s, WTF::move(body));
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

    return FormData::create(reportList->toJSONString().utf8().span());
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

NODELETE static std::optional<ResourceCryptographicDigest::Algorithm> findStrongestAlgorithm(HashAlgorithmSet algorithmSet)
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
    if (!resource.isHashReportingNeeded())
        return;

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

    bool canExposeHashes = !resource.loadFailedOrCanceled() && isCORSSameOrigin(filteredResponseType(resource.responseTainting()));
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
        document->sendReportToEndpoints(documentURL, { }, fixedEndpoints, WTF::move(report), ViolationReportType::CSPHashReport);
    }
}

bool doBytesMatchMetadataList(ResourceResponse::Tainting tainting, NOESCAPE const ScopedLambda<ResourceCryptographicDigest(ResourceCryptographicDigest::Algorithm)>& digestForAlgorithm, const String& metadataList)
{
    // 1. Let parsedMetadata be the result of parsing metadataList.
    auto parsedMetadata = parseIntegrityMetadata(metadataList);

    // 2. If parsedMetadata is empty, return true.
    if (parsedMetadata.isEmpty())
        return true;

    // FIXME: The current draft has no such step; earlier drafts had "If response is not eligible for integrity
    // validation, return false", and the current one only notes that Subresource Integrity requires CORS.
    if (!isCORSSameOrigin(filteredResponseType(tainting)))
        return false;

    // 3. Let metadata be the result of getting the strongest metadata from parsedMetadata.
    auto metadata = strongestMetadataFromSet(WTF::move(parsedMetadata));

    // 4. For each item in metadata:
    for (auto& item : metadata) {
        // 1. Let algorithm be item["alg"].
        auto algorithm = item.algorithm;

        // 2. Let expectedValue be item["val"].
        auto& expectedValue = item.digest;

        // 3. Let actualValue be the result of applying algorithm to bytes.
        // https://w3c.github.io/webappsec-subresource-integrity/#apply-algorithm-to-response
        //   1. Let result be the result of applying algorithm to bytes.
        //   2. Return the result of base64 encoding result.
        auto digest = digestForAlgorithm(algorithm);
        auto actualValue = base64EncodeToString(digest.value);

        // 4. If actualValue is a case-sensitive match for expectedValue, return true.
        if (actualValue == expectedValue)
            return true;

        // FIXME: The spec only matches padded base64; this also accepts unpadded base64 and base64url, as other engines do and WPT expects.
        if (auto expectedDigest = base64Decode(expectedValue); expectedDigest && *expectedDigest == digest.value)
            return true;
        if (auto expectedDigest = base64URLDecode(expectedValue); expectedDigest && *expectedDigest == digest.value)
            return true;
    }

    // 5. Return false.
    return false;
}

bool doBytesMatchMetadataList(ResourceResponse::Tainting tainting, const FragmentedSharedBuffer* bytes, const String& metadataList)
{
    return doBytesMatchMetadataList(tainting, [bytes = RefPtr { bytes }](auto algorithm) {
        return cryptographicDigestForSharedBuffer(algorithm, bytes.get());
    }, metadataList);
}

static constexpr int integrityMismatchErrorCode = 1;

bool isIntegrityMismatchError(const ResourceError& error)
{
    return error.isAccessControl() && error.domain() == errorDomainWebKitInternal && error.errorCode() == integrityMismatchErrorCode;
}

static ASCIILiteral cannotLoadPrefix(FetchOptionsDestination destination)
{
    switch (destination) {
    case FetchOptionsDestination::Script:
    case FetchOptionsDestination::Worker:
    case FetchOptionsDestination::Sharedworker:
    case FetchOptionsDestination::Serviceworker:
    case FetchOptionsDestination::Audioworklet:
    case FetchOptionsDestination::Paintworklet:
        return "Cannot load script "_s;
    case FetchOptionsDestination::Style:
        return "Cannot load stylesheet "_s;
    case FetchOptionsDestination::Image:
        return "Cannot load image "_s;
    case FetchOptionsDestination::Font:
        return "Cannot load font "_s;
    default:
        return "Cannot load "_s;
    }
}

std::optional<ResourceError> checkResponseBodyIntegrity(const ResourceResponse& response, ResourceResponse::Tainting tainting, FetchOptionsDestination destination, const FragmentedSharedBuffer* body, const String& integrityMetadata)
{
    ASSERT(!integrityMetadata.isEmpty());

    // 23. If request's integrity metadata is not the empty string, then:
    // (The caller fully reads response's body (step 23.4) before calling this, and hands the body over itself when this returns no error.)
    //   1. Let processBodyError be this step: run fetch response handover given fetchParams and a network error.
    auto networkError = [&] {
        return ResourceError { errorDomainWebKitInternal, integrityMismatchErrorCode, response.url(), makeString(cannotLoadPrefix(destination), integrityMismatchDescription(response.url(), response, body, integrityMetadata)), ResourceError::Type::AccessControl };
    };

    //   2. If response's body is null, then run processBodyError and abort these steps.
    // FIXME: WebKit does not distinguish a null body from an empty one, so this only checks for a null body status.
    if (isHttpNullBodyStatus(response.httpStatusCode()))
        return networkError();

    //   3. Let processBody given bytes be these steps:
    //     1. If bytes do not match request's integrity metadata, then run processBodyError and abort these steps.
    if (!doBytesMatchMetadataList(tainting, body, integrityMetadata))
        return networkError();

    return std::nullopt;
}

String integrityMismatchDescription(const URL& url, const ResourceResponse& response, const FragmentedSharedBuffer* body, const String& integrityMetadata)
{
    auto resourceURL = url.stringCenterEllipsizedToLength();
    if (body)
        return makeString(resourceURL, ". Failed integrity metadata check. Content length: "_s, body->size(), ", Expected content length: "_s, response.expectedContentLength(), ", Expected metadata: "_s, integrityMetadata);
    return makeString(resourceURL, ". Failed integrity metadata check. Content length: (no content), Expected content length: "_s, response.expectedContentLength(), ", Expected metadata: "_s, integrityMetadata);
}

bool matchIntegrityMetadataWithoutReportingHashes(const CachedResource& resource, const String& integrityMetadata)
{
    if (resource.loadFailedOrCanceled())
        return parseIntegrityMetadata(integrityMetadata).isEmpty();

    return doBytesMatchMetadataList(resource.responseTainting(), [resource = Ref { resource }](auto algorithm) {
        return resource->cryptographicDigest(algorithm);
    }, integrityMetadata);
}

bool matchIntegrityMetadata(const CachedResource& resource, const String& integrityMetadata)
{
    if (integrityMetadata.isEmpty() && !resource.isHashReportingNeeded()) [[likely]]
        return true;
    bool result = matchIntegrityMetadataWithoutReportingHashes(resource, integrityMetadata);
    if (result)
        reportHashesIfNeeded(resource);
    return result;
}

String integrityMismatchDescription(const CachedResource& resource, const String& integrityMetadata)
{
    return integrityMismatchDescription(resource.url(), resource.response(), protect(resource.resourceBuffer()).get(), integrityMetadata);
}

} // namespace WebCore
