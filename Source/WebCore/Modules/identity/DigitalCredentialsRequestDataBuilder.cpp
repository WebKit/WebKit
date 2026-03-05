/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "DigitalCredentialsRequestDataBuilder.h"

#include <WebCore/DigitalCredentialsRequestData.h>
#include <WebCore/DocumentSecurityOrigin.h>
#include <WebCore/ISO18013DocumentRequest.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if ENABLE(ISO18013_DOCUMENT_REQUEST_INFO)
#include <WebCore/DigitalCredentialsMobileDocumentRequestDataWithRequestInfo.h>
#include <WebCore/ISO18013DocumentRequestInfo.h>
#endif // ENABLE(ISO18013_DOCUMENT_REQUEST_INFO)

namespace WebCore {

ExceptionOr<std::pair<DigitalCredentialsRequestData, DigitalCredentialsRawRequests>> DigitalCredentialsRequestDataBuilder::build(Vector<ValidatedMobileDocumentRequest> validatedCredentialRequests, const Document& document, Vector<UnvalidatedDigitalCredentialRequest>&& unvalidatedRequests)
{
#if ENABLE(ISO18013_DOCUMENT_REQUEST_INFO)
    auto allRequests = flatMap(validatedCredentialRequests, [](auto& validatedRequest) {
        return flatMap(validatedRequest.presentmentRequests, [](auto& presentmentRequest) {
            return flatMap(presentmentRequest.documentRequestSets, [](auto& documentSet) {
                return documentSet.requests;
            });
        });
    });

    auto results = compactMap(allRequests, [&document](auto& request) -> std::optional<ExceptionOr<std::pair<DigitalCredentialsRequestData, DigitalCredentialsRawRequests>>> {
        if (!request.requestInfo.has_value() || request.documentType != ISO18013RequestInfoDocType)
            return std::nullopt;

        auto result = buildAndValidateRequestDataWithRequestInfo(request, document);
        if (result.hasException())
            return result.releaseException();

        auto [requestDataWithRequestInfo, rawRequestStrings] = result.releaseReturnValue();
        return std::make_pair(
            DigitalCredentialsRequestData { WTF::move(requestDataWithRequestInfo) },
            DigitalCredentialsRawRequests { WTF::move(rawRequestStrings) });
    });

    std::optional<ExceptionOr<std::pair<DigitalCredentialsRequestData, DigitalCredentialsRawRequests>>> firstException = std::nullopt;
    for (auto result : results) {
        // return the first matching result without exception
        if (!result.hasException())
            return result.releaseReturnValue();

        if (result.hasException() && !firstException)
            firstException = result.releaseException();
    }
    if (firstException)
        return firstException.value();
#endif // ENABLE(ISO18013_DOCUMENT_REQUEST_INFO)

    // otherwise send all requests
    return std::make_pair(
        DigitalCredentialsRequestData {
            DigitalCredentialsMobileDocumentRequestData {
                { protect(document.topOrigin())->data(), protect(document.securityOrigin())->data() },
                WTF::move(validatedCredentialRequests) } },
        DigitalCredentialsRawRequests { WTF::move(unvalidatedRequests) });
}

#if ENABLE(ISO18013_DOCUMENT_REQUEST_INFO)

ExceptionOr<std::pair<DigitalCredentialsMobileDocumentRequestDataWithRequestInfo, RawDigitalCredentialsWithRequestInfo>> DigitalCredentialsRequestDataBuilder::buildAndValidateRequestDataWithRequestInfo(const ISO18013DocumentRequest& documentRequest, const Document& document)
{
    auto responseType = parseRequestedDataElements(documentRequest);
    if (responseType.hasException())
        return responseType.releaseException();

    auto matchingHintAndRawRequests = parseMatchingHintAndRawRequests(documentRequest);
    if (matchingHintAndRawRequests.hasException())
        return matchingHintAndRawRequests.releaseException();

    auto [ matchingHint, rawRequests ] = matchingHintAndRawRequests.releaseReturnValue();

    auto requestData = DigitalCredentialsMobileDocumentRequestDataWithRequestInfo {
        { protect(document.topOrigin())->data(), protect(document.securityOrigin())->data() },
        responseType.releaseReturnValue(),
        matchingHint
    };

    return std::make_pair(WTF::move(requestData), WTF::move(rawRequests));
}

ExceptionOr<std::pair<String, RawDigitalCredentialsWithRequestInfo>> DigitalCredentialsRequestDataBuilder::parseMatchingHintAndRawRequests(const ISO18013DocumentRequest& documentRequest)
{
    if (documentRequest.requestInfo->extension.isEmpty())
        return Exception { ExceptionCode::TypeError, "Missing data in request info"_s };

    auto extension = documentRequest.requestInfo->extension.begin()->value;

    return switchOn(extension.data,
        [](const HashMap<String, Box<ISO18013Any>>& extensionMap) -> ExceptionOr<std::pair<String, RawDigitalCredentialsWithRequestInfo>> {
            auto rawRequests = parseRawRequests(extensionMap);
            if (rawRequests.hasException())
                return rawRequests.releaseException();

            auto matchingHint = parseMatchingHint(extensionMap);
            if (matchingHint.hasException())
                return matchingHint.releaseException();

            return std::make_pair(matchingHint.releaseReturnValue(), rawRequests.releaseReturnValue());
        },
        [](const auto& value) -> ExceptionOr<std::pair<String, RawDigitalCredentialsWithRequestInfo>> {
            UNUSED_PARAM(value);
            return Exception { ExceptionCode::TypeError, "Extension is wrong type"_s };
        }
    );
}

ExceptionOr<RawDigitalCredentialsWithRequestInfo> DigitalCredentialsRequestDataBuilder::parseRawRequests(const HashMap<String, Box<ISO18013Any>>& extension)
{
    auto rawRequests = extension.getOptional(rawRequestKey);
    if (!rawRequests)
        return Exception { ExceptionCode::TypeError, "Missing raw request key"_s };

    return WTF::switchOn((*rawRequests)->data,
        [](const Vector<Box<ISO18013Any>>& rawRequestsVec) -> ExceptionOr<RawDigitalCredentialsWithRequestInfo> {
            // Try to convert Vector<Box<ISO18013Any>> to Vector<String>
            Vector<String> rawRequestsVector;
            rawRequestsVector.reserveInitialCapacity(rawRequestsVec.size());

            for (auto rawRequest : rawRequestsVec) {
                auto stringResult = WTF::switchOn(rawRequest->data,
                    [](const String& str) -> std::optional<String> {
                        return str;
                    },
                    [](const auto& value) -> std::optional<String> {
                        UNUSED_PARAM(value);
                        return std::nullopt;
                    }
                );

                if (!stringResult)
                    return Exception { ExceptionCode::TypeError, "Raw request element is not a String"_s };

                rawRequestsVector.append(stringResult.value());
            }

            return rawRequestsVector;
        },
        [](const auto& value) -> ExceptionOr<RawDigitalCredentialsWithRequestInfo> {
            UNUSED_PARAM(value);
            return Exception { ExceptionCode::TypeError, "Raw requests are wrong type"_s };
        }
    );
}

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/DigitalCredentialsRequestDataBuilderAdditions.cpp>)
#include <WebKitAdditions/DigitalCredentialsRequestDataBuilderAdditions.cpp>
#else
static inline bool isValidMatchingHint(StringView matchingHint)
{
    return true;
}
#endif

ExceptionOr<String> DigitalCredentialsRequestDataBuilder::parseMatchingHint(const HashMap<String, Box<ISO18013Any>>& extension)
{
    auto matchingHint = extension.getOptional(matchingHintKey);
    if (!matchingHint)
        return Exception { ExceptionCode::TypeError, "Missing matching hint key"_s };

    return WTF::switchOn((*matchingHint)->data, [](String matchingHint) -> ExceptionOr<String> {
        if (!isValidMatchingHint(matchingHint))
            return Exception { ExceptionCode::TypeError, "Invalid matching hint value"_s };
        return matchingHint;
    }, [](const auto& value) -> ExceptionOr<String> {
        UNUSED_PARAM(value);
        return Exception { ExceptionCode::TypeError, "Matching hint the is wrong type"_s };
        }
    );
}

ExceptionOr<ResponseType> DigitalCredentialsRequestDataBuilder::parseRequestedDataElements(const ISO18013DocumentRequest& documentRequest)
{
    auto requestInfoNamespaceVector = [&documentRequest]() -> ExceptionOr<const ISO18013ElementNamespaceVector&> {
        for (auto& nameSpace : documentRequest.namespaces) {
            if (nameSpace.first == requestInfoNamespace)
                return nameSpace.second;
        }
        return Exception { ExceptionCode::TypeError, makeString("Unable to find request info namespace: \""_s, requestInfoNamespace, "\""_s) };
    }();

    if (requestInfoNamespaceVector.hasException())
        return requestInfoNamespaceVector.releaseException();

    auto containsDataElementIdentifier = [&requestInfoNamespaceVector](const ASCIILiteral& dataElementIdentifier) {
        return requestInfoNamespaceVector.returnValue().containsIf([&dataElementIdentifier](const auto& item) {
            auto [requestedDataElementIdentifier, isRetaining] = item;
            UNUSED_PARAM(isRetaining);
            return requestedDataElementIdentifier == dataElementIdentifier;
        });
    };

    bool requestingAttestation = containsDataElementIdentifier(attestationElementIdentifier);
    bool requestingDisclosure = containsDataElementIdentifier(disclosureElementIdentifier);

    if (requestingAttestation && requestingDisclosure)
        return ResponseType::Disclosure;

    if (requestingAttestation)
        return ResponseType::Attestation;

    return Exception { ExceptionCode::TypeError, "Missing supported data element identifiers"_s };
}

#endif // ENABLE(ISO18013_DOCUMENT_REQUEST_INFO)

} // namespace WebCore
