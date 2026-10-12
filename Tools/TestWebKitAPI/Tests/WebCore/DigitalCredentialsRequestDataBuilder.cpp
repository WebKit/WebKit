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
    return WTF::switchOn(rawRequests, [](const Vector<UnvalidatedDigitalCredentialRequest>& requests) {
        return requests.map([](auto& request) {
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
    auto* mobileDocumentRequestData = std::get_if<DigitalCredentialsMobileDocumentRequestData>(&requestData);
    ASSERT_TRUE(mobileDocumentRequestData);
    EXPECT_EQ(mobileDocumentRequestData->requests.size(), 2u);
    EXPECT_EQ(rawRequestIdentifiers(rawRequests), (Vector<String> { "valid-1"_s, "valid-2"_s }));
}

TEST(DigitalCredentialsRequestDataBuilder, RawRequestsFollowMobileDocumentPrecedence)
{
    auto document = createDocument();

    auto result = DigitalCredentialsRequestDataBuilder::build(
        { validatedOpenID4VP(DigitalCredentialPresentationProtocol::Openid4vpV1Signed), validatedMobileDocument() },
        document,
        { signedRequest("openid4vp"_s), mobileDocumentRequest("mdoc"_s) });
    ASSERT_FALSE(result.hasException());

    auto [requestData, rawRequests] = result.releaseReturnValue();
    EXPECT_TRUE(std::holds_alternative<DigitalCredentialsMobileDocumentRequestData>(requestData));
    EXPECT_EQ(rawRequestIdentifiers(rawRequests), (Vector<String> { "mdoc"_s }));
}

TEST(DigitalCredentialsRequestDataBuilder, RawRequestsFollowTheFirstOpenID4VPProtocol)
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
    auto* openID4VPRequestData = std::get_if<DigitalCredentialsOpenID4VPRequestData>(&requestData);
    ASSERT_TRUE(openID4VPRequestData);
    ASSERT_EQ(openID4VPRequestData->requests.size(), 2u);
    EXPECT_EQ(openID4VPRequestData->requests[0].protocol, DigitalCredentialPresentationProtocol::Openid4vpV1Unsigned);
    EXPECT_EQ(openID4VPRequestData->requests[1].protocol, DigitalCredentialPresentationProtocol::Openid4vpV1Unsigned);
    EXPECT_EQ(rawRequestIdentifiers(rawRequests), (Vector<String> { "unsigned-1"_s, "unsigned-2"_s }));
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

    auto result = DigitalCredentialsRequestDataBuilder::build(WTF::move(validatedRequests), document, WTF::move(unvalidatedRequests));
    ASSERT_FALSE(result.hasException());

    auto [requestData, rawRequests] = result.releaseReturnValue();
    auto* openID4VPRequestData = std::get_if<DigitalCredentialsOpenID4VPRequestData>(&requestData);
    ASSERT_TRUE(openID4VPRequestData);
    EXPECT_EQ(openID4VPRequestData->requests.size(), maxOpenID4VPRequestCount);
    EXPECT_EQ(rawRequestIdentifiers(rawRequests).size(), maxOpenID4VPRequestCount);
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
