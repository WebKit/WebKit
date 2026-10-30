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

#if ENABLE(WEB_AUTHN)

#include <WebCore/DigitalCredentialsRequestDataBuilder.h>
#include <WebCore/DocumentInlines.h>
#include <WebCore/ProcessWarming.h>
#include <WebCore/Settings.h>
#include <WebCore/ValidatedOpenID4VPRequest.h>
#include <wtf/text/StringBuilder.h>

namespace TestWebKitAPI {

using namespace WebCore;

static Ref<Document> createDocument()
{
    ProcessWarming::initializeNames();
    auto settings = Settings::create(nullptr);
    return Document::create(settings.get(), URL { "https://example.com/"_s });
}

static UnvalidatedDigitalCredentialRequest mobileDocumentRequest(ASCIILiteral deviceRequest)
{
    return MobileDocumentRequest { deviceRequest, "encryption-info"_s };
}

static UnvalidatedDigitalCredentialRequest unsignedRequest(ASCIILiteral requestJSON)
{
    return OpenID4VPUnsignedRequest { requestJSON };
}

static UnvalidatedDigitalCredentialRequest signedRequest(ASCIILiteral request)
{
    return OpenID4VPSignedRequest { request };
}

static std::optional<ValidatedDigitalCredentialRequest> validatedMobileDocument()
{
    return ValidatedMobileDocumentRequest { };
}

static std::optional<ValidatedDigitalCredentialRequest> validatedOpenID4VP(DigitalCredentialPresentationProtocol protocol)
{
    return ValidatedOpenID4VPRequest { protocol, { } };
}

static Vector<String> rawRequestIdentifiers(const DigitalCredentialsRawRequests& rawRequests)
{
    return rawRequests.map([](auto& request) {
        return WTF::switchOn(request,
            [](const MobileDocumentRequest& request) {
                return request.deviceRequest;
            },
            [](const OpenID4VPSignedRequest& request) {
                return request.request;
            },
            [](const OpenID4VPMultisignedRequest& request) {
                return request.payload;
            },
            [](const OpenID4VPUnsignedRequest& request) {
                return request.requestJSON;
            });
    });
}

TEST(DigitalCredentialsRequestDataBuilder, RawRequestsOmitRequestsThatDidNotValidate)
{
    auto document = createDocument();

    auto result = DigitalCredentialsRequestDataBuilder::build(
        { std::nullopt, validatedMobileDocument(), std::nullopt, validatedMobileDocument() },
        document,
        { mobileDocumentRequest("invalid-1"_s), mobileDocumentRequest("valid-1"_s), mobileDocumentRequest("invalid-2"_s), mobileDocumentRequest("valid-2"_s) });
    ASSERT_FALSE(result.hasException());

    auto [requestData, rawRequests] = result.releaseReturnValue();
    EXPECT_EQ(requestData.requests.size(), 2u);
    EXPECT_EQ(rawRequestIdentifiers(rawRequests), (Vector<String> { "valid-1"_s, "valid-2"_s }));
}

TEST(DigitalCredentialsRequestDataBuilder, PresentsMobileDocumentAndOpenID4VPRequestsInPageOrder)
{
    auto document = createDocument();

    auto result = DigitalCredentialsRequestDataBuilder::build(
        { validatedOpenID4VP(DigitalCredentialPresentationProtocol::Openid4vpV1Signed), validatedMobileDocument() },
        document,
        { signedRequest("openid4vp"_s), mobileDocumentRequest("mdoc"_s) });
    ASSERT_FALSE(result.hasException());

    auto [requestData, rawRequests] = result.releaseReturnValue();
    ASSERT_EQ(requestData.requests.size(), 2u);
    EXPECT_TRUE(std::holds_alternative<ValidatedOpenID4VPRequest>(requestData.requests[0]));
    EXPECT_TRUE(std::holds_alternative<ValidatedMobileDocumentRequest>(requestData.requests[1]));
    EXPECT_EQ(rawRequestIdentifiers(rawRequests), (Vector<String> { "openid4vp"_s, "mdoc"_s }));
}

TEST(DigitalCredentialsRequestDataBuilder, PresentsOpenID4VPRequestsOfDifferentProtocols)
{
    auto document = createDocument();

    Vector<std::optional<ValidatedDigitalCredentialRequest>> validatedRequests {
        validatedOpenID4VP(DigitalCredentialPresentationProtocol::Openid4vpV1Unsigned),
        validatedOpenID4VP(DigitalCredentialPresentationProtocol::Openid4vpV1Signed),
        std::nullopt,
        validatedOpenID4VP(DigitalCredentialPresentationProtocol::Openid4vpV1Unsigned),
    };
    auto result = DigitalCredentialsRequestDataBuilder::build(
        WTF::move(validatedRequests),
        document,
        { unsignedRequest("unsigned-1"_s), signedRequest("signed"_s), unsignedRequest("invalid"_s), unsignedRequest("unsigned-2"_s) });
    ASSERT_FALSE(result.hasException());

    auto [requestData, rawRequests] = result.releaseReturnValue();
    auto protocols = requestData.requests.map([](auto& request) {
        return std::get<ValidatedOpenID4VPRequest>(request).protocol;
    });
    EXPECT_EQ(protocols, (Vector<DigitalCredentialPresentationProtocol> { DigitalCredentialPresentationProtocol::Openid4vpV1Unsigned, DigitalCredentialPresentationProtocol::Openid4vpV1Signed, DigitalCredentialPresentationProtocol::Openid4vpV1Unsigned }));
    EXPECT_EQ(rawRequestIdentifiers(rawRequests), (Vector<String> { "unsigned-1"_s, "signed"_s, "unsigned-2"_s }));
}

TEST(DigitalCredentialsRequestDataBuilder, RejectsValidationThatDoesNotCoverEveryRequest)
{
    auto document = createDocument();

    auto result = DigitalCredentialsRequestDataBuilder::build(
        { validatedMobileDocument() },
        document,
        { mobileDocumentRequest("first"_s), mobileDocumentRequest("second"_s) });
    EXPECT_TRUE(result.hasException());
}

TEST(DigitalCredentialsRequestDataBuilder, PresentsAtMostTheMaximumNumberOfOpenID4VPRequests)
{
    auto document = createDocument();

    Vector<std::optional<ValidatedDigitalCredentialRequest>> validatedRequests;
    Vector<UnvalidatedDigitalCredentialRequest> unvalidatedRequests;
    for (size_t index = 0; index <= maxOpenID4VPRequestCount; ++index) {
        validatedRequests.append(validatedOpenID4VP(DigitalCredentialPresentationProtocol::Openid4vpV1Unsigned));
        unvalidatedRequests.append(unsignedRequest("request"_s));
    }
    validatedRequests.append(validatedMobileDocument());
    unvalidatedRequests.append(mobileDocumentRequest("mdoc"_s));

    auto result = DigitalCredentialsRequestDataBuilder::build(WTF::move(validatedRequests), document, WTF::move(unvalidatedRequests));
    ASSERT_FALSE(result.hasException());

    auto [requestData, rawRequests] = result.releaseReturnValue();
    ASSERT_EQ(requestData.requests.size(), maxOpenID4VPRequestCount + 1);
    EXPECT_TRUE(std::holds_alternative<ValidatedMobileDocumentRequest>(requestData.requests.last()));
    auto rawRequestIDs = rawRequestIdentifiers(rawRequests);
    ASSERT_EQ(rawRequestIDs.size(), maxOpenID4VPRequestCount + 1);
    EXPECT_EQ(rawRequestIDs.last(), "mdoc"_s);
}

TEST(ValidatedOpenID4VPRequest, LimitsBoundEveryElement)
{
    ValidatedOpenID4VPRequestObject request;
    request.credentials.append(OpenID4VPCredentialQuery { .identifier = "credential"_s, .format = OpenID4VPCredentialFormat::MsoMdoc });
    EXPECT_TRUE(isWithinOpenID4VPRequestLimits(request));

    request.credentials[0].claimSets.resize(maxOpenID4VPRequestElementCount);
    EXPECT_FALSE(isWithinOpenID4VPRequestLimits(request));
}

TEST(ValidatedOpenID4VPRequest, LimitsBoundStringLength)
{
    ValidatedOpenID4VPRequestObject request;
    request.verifierIdentities.append(OpenID4VPVerifierIdentity { .identifier = "verifier"_s });
    EXPECT_TRUE(isWithinOpenID4VPRequestLimits(request));

    StringBuilder longIdentifier;
    for (size_t i = 0; i <= maxOpenID4VPRequestDataLength; ++i)
        longIdentifier.append('a');
    request.verifierIdentities[0].identifier = longIdentifier.toString();
    EXPECT_FALSE(isWithinOpenID4VPRequestLimits(request));
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_AUTHN)
