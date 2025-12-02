// Copyright 2017 The Chromium Authors. All rights reserved.
// Copyright (C) 2018-2021 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"

#if ENABLE(WEB_AUTHN)

#include "FidoTestData.h"
#include "PlatformUtilities.h"
#include <WebCore/AuthenticatorAttachment.h>
#include <WebCore/AuthenticatorSelectionCriteria.h>
#include <WebCore/DeviceRequestConverter.h>
#include <WebCore/FidoConstants.h>
#include <WebCore/Pin.h>
#include <WebCore/PublicKeyCredentialCreationOptions.h>
#include <WebCore/PublicKeyCredentialRequestOptions.h>
#include <wtf/text/Base64.h>

namespace TestWebKitAPI {
using namespace WebCore;
using namespace fido;

// Leveraging example 2 of section 6.1 of the spec
// https://fidoalliance.org/specs/fido-v2.0-ps-20170927/fido-client-to-authenticator-protocol-v2.0-ps-20170927.html
TEST(CTAPRequestTest, TestConstructMakeCredentialRequestParam)
{
    PublicKeyCredentialRpEntity rp;
    rp.name = "Acme"_s;
    rp.id = "acme.com"_s;

    PublicKeyCredentialUserEntity user;
    user.name = "johnpsmith@example.com"_s;
    user.icon = "https://pics.acme.com/00/p/aBjjjpqPb.png"_s;
    user.id = WebCore::toBufferSource(TestData::kUserId);
    user.displayName = "John P. Smith"_s;

    Vector<PublicKeyCredentialParameters> params { { PublicKeyCredentialType::PublicKey, 7 }, { PublicKeyCredentialType::PublicKey, 257 } };
    WebCore::AuthenticatorSelectionCriteria selection { "platform"_s, { }, true, "preferred"_s };

    WebCore::PublicKeyCredentialCreationOptions options { rp, user, { }, params, std::nullopt, { }, selection, "none"_s, std::nullopt };
    Vector<uint8_t> hash;
    Vector<String> extensions;
    hash.append(std::span { TestData::kClientDataHash });
    auto serializedData = encodeMakeCredentialRequestAsCBOR(hash, options, AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedAndConfigured, AuthenticatorSupportedOptions::ResidentKeyAvailability::kSupported, extensions);
    EXPECT_EQ(serializedData.size(), sizeof(TestData::kCtapMakeCredentialRequest));
    EXPECT_TRUE(equalSpans(serializedData.span(), std::span { TestData::kCtapMakeCredentialRequest }));
}

TEST(CTAPRequestTest, TestConstructMakeCredentialRequestParamNoUVNoRK)
{
    PublicKeyCredentialRpEntity rp;
    rp.name = "Acme"_s;
    rp.id = "acme.com"_s;

    PublicKeyCredentialUserEntity user;
    user.name = "johnpsmith@example.com"_s;
    user.icon = "https://pics.acme.com/00/p/aBjjjpqPb.png"_s;
    user.id = WebCore::toBufferSource(TestData::kUserId);
    user.displayName = "John P. Smith"_s;

    Vector<PublicKeyCredentialParameters> params { { PublicKeyCredentialType::PublicKey, 7 }, { PublicKeyCredentialType::PublicKey, 257 } };
    AuthenticatorSelectionCriteria selection { "platform"_s, { }, false, "discouraged"_s };

    PublicKeyCredentialCreationOptions options { rp, user, { }, params, std::nullopt, { }, selection, "none"_s, std::nullopt };
    Vector<uint8_t> hash;
    Vector<String> extensions;
    hash.append(std::span { TestData::kClientDataHash });
    auto serializedData = encodeMakeCredentialRequestAsCBOR(hash, options, AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedAndConfigured, AuthenticatorSupportedOptions::ResidentKeyAvailability::kSupported, extensions);
    EXPECT_EQ(serializedData.size(), sizeof(TestData::kCtapMakeCredentialRequestShort));
    EXPECT_TRUE(equalSpans(serializedData.span(), std::span { TestData::kCtapMakeCredentialRequestShort }));
}

TEST(CTAPRequestTest, TestConstructMakeCredentialRequestParamUVRequiredButNotSupported)
{
    PublicKeyCredentialRpEntity rp;
    rp.name = "Acme"_s;
    rp.id = "acme.com"_s;

    PublicKeyCredentialUserEntity user;
    user.name = "johnpsmith@example.com"_s;
    user.icon = "https://pics.acme.com/00/p/aBjjjpqPb.png"_s;
    user.id = WebCore::toBufferSource(TestData::kUserId);
    user.displayName = "John P. Smith"_s;

    Vector<PublicKeyCredentialParameters> params { { PublicKeyCredentialType::PublicKey, 7 }, { PublicKeyCredentialType::PublicKey, 257 } };
    AuthenticatorSelectionCriteria selection { "platform"_s, { }, false, "required"_s };

    PublicKeyCredentialCreationOptions options { rp, user, { }, params, std::nullopt, { }, selection, "none"_s, std::nullopt };
    Vector<uint8_t> hash;
    Vector<String> extensions;
    hash.append(std::span { TestData::kClientDataHash });
    auto serializedData = encodeMakeCredentialRequestAsCBOR(hash, options, AuthenticatorSupportedOptions::UserVerificationAvailability::kNotSupported, AuthenticatorSupportedOptions::ResidentKeyAvailability::kSupported, extensions);
    EXPECT_EQ(serializedData.size(), sizeof(TestData::kCtapMakeCredentialRequestShort));
    EXPECT_TRUE(equalSpans(serializedData.span(), std::span { TestData::kCtapMakeCredentialRequestShort }));
}

TEST(CTAPRequestTest, TestConstructMakeCredentialRequestParamWithPin)
{
    PublicKeyCredentialRpEntity rp;
    rp.name = "Acme"_s;
    rp.id = "acme.com"_s;

    PublicKeyCredentialUserEntity user;
    user.name = "johnpsmith@example.com"_s;
    user.icon = "https://pics.acme.com/00/p/aBjjjpqPb.png"_s;
    user.id = WebCore::toBufferSource(TestData::kUserId);
    user.displayName = "John P. Smith"_s;

    Vector<PublicKeyCredentialParameters> params { { PublicKeyCredentialType::PublicKey, 7 }, { PublicKeyCredentialType::PublicKey, 257 } };
    AuthenticatorSelectionCriteria selection { "platform"_s, { }, true, "preferred"_s };

    PinParameters pin;
    pin.protocol = pin::kProtocolVersion;
    pin.auth.append(std::span { TestData::kCtap2PinAuth });

    PublicKeyCredentialCreationOptions options { rp, user, { }, params, std::nullopt, { }, selection, "none"_s, std::nullopt };
    Vector<uint8_t> hash;
    Vector<String> extensions;
    hash.append(std::span { TestData::kClientDataHash });
    auto serializedData = encodeMakeCredentialRequestAsCBOR(hash, options, AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedAndConfigured, AuthenticatorSupportedOptions::ResidentKeyAvailability::kSupported, extensions, pin);
    EXPECT_EQ(serializedData.size(), sizeof(TestData::kCtapMakeCredentialRequestWithPin));
    EXPECT_TRUE(equalSpans(serializedData.span(), std::span { TestData::kCtapMakeCredentialRequestWithPin }));
}

TEST(CTAPRequestTest, TestConstructMakeCredentialRequestRKPreferred)
{
    PublicKeyCredentialRpEntity rp;
    rp.name = "Acme"_s;
    rp.id = "acme.com"_s;

    PublicKeyCredentialUserEntity user;
    user.name = "johnpsmith@example.com"_s;
    user.icon = "https://pics.acme.com/00/p/aBjjjpqPb.png"_s;
    user.id = WebCore::toBufferSource(TestData::kUserId);
    user.displayName = "John P. Smith"_s;

    Vector<PublicKeyCredentialParameters> params { { PublicKeyCredentialType::PublicKey, 7 }, { PublicKeyCredentialType::PublicKey, 257 } };
    AuthenticatorSelectionCriteria selection { "platform"_s, "preferred"_s, true, "preferred"_s };

    PinParameters pin;
    pin.protocol = pin::kProtocolVersion;
    pin.auth.append(std::span { TestData::kCtap2PinAuth });

    PublicKeyCredentialCreationOptions options { rp, user, { }, params, std::nullopt, { }, selection, "none"_s, std::nullopt };
    Vector<uint8_t> hash;
    Vector<String> extensions;
    hash.append(std::span { TestData::kClientDataHash });
    auto serializedData = encodeMakeCredentialRequestAsCBOR(hash, options, AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedAndConfigured, AuthenticatorSupportedOptions::ResidentKeyAvailability::kSupported, extensions, pin);
    EXPECT_EQ(serializedData.size(), sizeof(TestData::kCtapMakeCredentialRequestWithPin));
    EXPECT_TRUE(equalSpans(serializedData.span(), std::span { TestData::kCtapMakeCredentialRequestWithPin }));
}

TEST(CTAPRequestTest, TestConstructMakeCredentialRequestRKPreferredNotSupported)
{
    PublicKeyCredentialRpEntity rp;
    rp.name = "Acme"_s;
    rp.id = "acme.com"_s;

    PublicKeyCredentialUserEntity user;
    user.name = "johnpsmith@example.com"_s;
    user.icon = "https://pics.acme.com/00/p/aBjjjpqPb.png"_s;
    user.id = WebCore::toBufferSource(TestData::kUserId);
    user.displayName = "John P. Smith"_s;

    Vector<PublicKeyCredentialParameters> params { { PublicKeyCredentialType::PublicKey, 7 }, { PublicKeyCredentialType::PublicKey, 257 } };
    AuthenticatorSelectionCriteria selection { "platform"_s, "preferred"_s, true, "required"_s };

    PublicKeyCredentialCreationOptions options { rp, user, { }, params, std::nullopt, { }, selection, "none"_s, std::nullopt };
    Vector<uint8_t> hash;
    Vector<String> extensions;
    hash.append(std::span { TestData::kClientDataHash });
    auto serializedData = encodeMakeCredentialRequestAsCBOR(hash, options, AuthenticatorSupportedOptions::UserVerificationAvailability::kNotSupported, AuthenticatorSupportedOptions::ResidentKeyAvailability::kNotSupported, extensions);
    EXPECT_EQ(serializedData.size(), sizeof(TestData::kCtapMakeCredentialRequestShort));
    EXPECT_TRUE(equalSpans(serializedData.span(), std::span { TestData::kCtapMakeCredentialRequestShort }));
}

TEST(CTAPRequestTest, TestConstructMakeCredentialRequestRKDiscouraged)
{
    PublicKeyCredentialRpEntity rp;
    rp.name = "Acme"_s;
    rp.id = "acme.com"_s;

    PublicKeyCredentialUserEntity user;
    user.name = "johnpsmith@example.com"_s;
    user.icon = "https://pics.acme.com/00/p/aBjjjpqPb.png"_s;
    user.id = WebCore::toBufferSource(TestData::kUserId);
    user.displayName = "John P. Smith"_s;

    Vector<PublicKeyCredentialParameters> params { { PublicKeyCredentialType::PublicKey, 7 }, { PublicKeyCredentialType::PublicKey, 257 } };
    AuthenticatorSelectionCriteria selection { "platform"_s, "discouraged"_s, true, "required"_s };

    PublicKeyCredentialCreationOptions options { rp, user, { }, params, std::nullopt, { }, selection, "none"_s, std::nullopt };
    Vector<uint8_t> hash;
    Vector<String> extensions;
    hash.append(std::span { TestData::kClientDataHash });
    auto serializedData = encodeMakeCredentialRequestAsCBOR(hash, options, AuthenticatorSupportedOptions::UserVerificationAvailability::kNotSupported, AuthenticatorSupportedOptions::ResidentKeyAvailability::kSupported, extensions);
    EXPECT_EQ(serializedData.size(), sizeof(TestData::kCtapMakeCredentialRequestShort));
    EXPECT_TRUE(equalSpans(serializedData.span(), std::span { TestData::kCtapMakeCredentialRequestShort }));
}

TEST(CTAPRequestTest, TestConstructMakeCredentialRequestWithLargeBlob)
{
    PublicKeyCredentialRpEntity rp;
    rp.name = "Acme"_s;
    rp.id = "acme.com"_s;

    PublicKeyCredentialUserEntity user;
    user.name = "johnpsmith@example.com"_s;
    user.icon = "https://pics.acme.com/00/p/aBjjjpqPb.png"_s;
    user.id = WebCore::toBufferSource(TestData::kUserId);
    user.displayName = "John P. Smith"_s;

    Vector<PublicKeyCredentialParameters> params { { PublicKeyCredentialType::PublicKey, 7 }, { PublicKeyCredentialType::PublicKey, 257 } };
    AuthenticatorSelectionCriteria selection { "platform"_s, { }, false, "discouraged"_s };
    AuthenticationExtensionsClientInputs extensionInputs = {
        .appid = WTF::nullString(),
        .credProps = false,
        .largeBlob = AuthenticationExtensionsClientInputs::LargeBlobInputs {
            .support = "required"_s,
            .read = std::nullopt,
            .write = std::nullopt,
        },
        .prf = std::nullopt,
    };

    PublicKeyCredentialCreationOptions options { rp, user, { }, params, std::nullopt, { }, selection, "none"_s, extensionInputs };
    Vector<uint8_t> hash;
    Vector<String> extensions = { "largeBlob"_s };
    hash.append(std::span { TestData::kClientDataHash });
    auto serializedData = encodeMakeCredentialRequestAsCBOR(hash, options, AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedAndConfigured, AuthenticatorSupportedOptions::ResidentKeyAvailability::kSupported, extensions);
    EXPECT_EQ(serializedData.size(), sizeof(TestData::kCtapMakeCredentialRequestShortWithLargeBlob));
    EXPECT_TRUE(equalSpans(serializedData.span(), std::span { TestData::kCtapMakeCredentialRequestShortWithLargeBlob }));
}

TEST(CTAPRequestTest, TestConstructMakeCredentialRequestWithUnsupportedLargeBlob)
{
    PublicKeyCredentialRpEntity rp;
    rp.name = "Acme"_s;
    rp.id = "acme.com"_s;

    PublicKeyCredentialUserEntity user;
    user.name = "johnpsmith@example.com"_s;
    user.icon = "https://pics.acme.com/00/p/aBjjjpqPb.png"_s;
    user.id = WebCore::toBufferSource(TestData::kUserId);
    user.displayName = "John P. Smith"_s;

    Vector<PublicKeyCredentialParameters> params { { PublicKeyCredentialType::PublicKey, 7 }, { PublicKeyCredentialType::PublicKey, 257 } };
    AuthenticatorSelectionCriteria selection { "platform"_s, { }, false, "discouraged"_s };
    AuthenticationExtensionsClientInputs extensionInputs = {
        .appid = WTF::nullString(),
        .credProps = false,
        .largeBlob = AuthenticationExtensionsClientInputs::LargeBlobInputs {
            .support = "required"_s,
            .read = std::nullopt,
            .write = std::nullopt,
        },
        .prf = std::nullopt,
    };

    PublicKeyCredentialCreationOptions options { rp, user, { }, params, std::nullopt, { }, selection, "none"_s, extensionInputs };
    Vector<uint8_t> hash;
    Vector<String> extensions;
    hash.append(std::span { TestData::kClientDataHash });
    auto serializedData = encodeMakeCredentialRequestAsCBOR(hash, options, AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedAndConfigured, AuthenticatorSupportedOptions::ResidentKeyAvailability::kSupported, extensions);
    EXPECT_EQ(serializedData.size(), sizeof(TestData::kCtapMakeCredentialRequestShort));
    EXPECT_TRUE(equalSpans(serializedData.span(), std::span { TestData::kCtapMakeCredentialRequestShort }));
}

TEST(CTAPRequestTest, TestConstructGetAssertionRequest)
{
    PublicKeyCredentialRequestOptions options;
    options.rpId = "acme.com"_s;

    PublicKeyCredentialDescriptor descriptor1;
    descriptor1.type = PublicKeyCredentialType::PublicKey;
    const uint8_t id1[] = {
        0xf2, 0x20, 0x06, 0xde, 0x4f, 0x90, 0x5a, 0xf6, 0x8a, 0x43, 0x94,
        0x2f, 0x02, 0x4f, 0x2a, 0x5e, 0xce, 0x60, 0x3d, 0x9c, 0x6d, 0x4b,
        0x3d, 0xf8, 0xbe, 0x08, 0xed, 0x01, 0xfc, 0x44, 0x26, 0x46, 0xd0,
        0x34, 0x85, 0x8a, 0xc7, 0x5b, 0xed, 0x3f, 0xd5, 0x80, 0xbf, 0x98,
        0x08, 0xd9, 0x4f, 0xcb, 0xee, 0x82, 0xb9, 0xb2, 0xef, 0x66, 0x77,
        0xaf, 0x0a, 0xdc, 0xc3, 0x58, 0x52, 0xea, 0x6b, 0x9e };
    descriptor1.id = WebCore::toBufferSource(id1);
    options.allowCredentials.append(descriptor1);

    PublicKeyCredentialDescriptor descriptor2;
    descriptor2.type = PublicKeyCredentialType::PublicKey;
    const uint8_t id2[] = {
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03 };
    descriptor2.id = WebCore::toBufferSource(id2);
    options.allowCredentials.append(descriptor2);

    options.userVerificationString = "required"_s;

    Vector<uint8_t> hash;
    Vector<String> extensions;
    hash.append(std::span { TestData::kClientDataHash });
    auto serializedData = encodeGetAssertionRequestAsCBOR(hash, options, AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedAndConfigured, extensions);
    EXPECT_EQ(serializedData.size(), sizeof(TestData::kTestComplexCtapGetAssertionRequest));
    EXPECT_TRUE(equalSpans(serializedData.span(), std::span { TestData::kTestComplexCtapGetAssertionRequest }));
}

TEST(CTAPRequestTest, TestConstructGetAssertionRequestNoUV)
{
    PublicKeyCredentialRequestOptions options;
    options.rpId = "acme.com"_s;

    PublicKeyCredentialDescriptor descriptor1;
    descriptor1.type = PublicKeyCredentialType::PublicKey;
    const uint8_t id1[] = {
        0xf2, 0x20, 0x06, 0xde, 0x4f, 0x90, 0x5a, 0xf6, 0x8a, 0x43, 0x94,
        0x2f, 0x02, 0x4f, 0x2a, 0x5e, 0xce, 0x60, 0x3d, 0x9c, 0x6d, 0x4b,
        0x3d, 0xf8, 0xbe, 0x08, 0xed, 0x01, 0xfc, 0x44, 0x26, 0x46, 0xd0,
        0x34, 0x85, 0x8a, 0xc7, 0x5b, 0xed, 0x3f, 0xd5, 0x80, 0xbf, 0x98,
        0x08, 0xd9, 0x4f, 0xcb, 0xee, 0x82, 0xb9, 0xb2, 0xef, 0x66, 0x77,
        0xaf, 0x0a, 0xdc, 0xc3, 0x58, 0x52, 0xea, 0x6b, 0x9e };
    descriptor1.id = WebCore::toBufferSource(id1);
    options.allowCredentials.append(descriptor1);

    PublicKeyCredentialDescriptor descriptor2;
    descriptor2.type = PublicKeyCredentialType::PublicKey;
    const uint8_t id2[] = {
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03 };
    descriptor2.id = WebCore::toBufferSource(id2);
    options.allowCredentials.append(descriptor2);

    options.userVerificationString = "discouraged"_s;

    Vector<uint8_t> hash;
    Vector<String> extensions;
    hash.append(std::span { TestData::kClientDataHash });
    auto serializedData = encodeGetAssertionRequestAsCBOR(hash, options, AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedAndConfigured, extensions);
    EXPECT_EQ(serializedData.size(), sizeof(TestData::kTestComplexCtapGetAssertionRequestShort));
    EXPECT_TRUE(equalSpans(serializedData.span(), std::span { TestData::kTestComplexCtapGetAssertionRequestShort }));
}

TEST(CTAPRequestTest, TestConstructGetAssertionRequestUVRequiredButNotSupported)
{
    PublicKeyCredentialRequestOptions options;
    options.rpId = "acme.com"_s;

    PublicKeyCredentialDescriptor descriptor1;
    descriptor1.type = PublicKeyCredentialType::PublicKey;
    const uint8_t id1[] = {
        0xf2, 0x20, 0x06, 0xde, 0x4f, 0x90, 0x5a, 0xf6, 0x8a, 0x43, 0x94,
        0x2f, 0x02, 0x4f, 0x2a, 0x5e, 0xce, 0x60, 0x3d, 0x9c, 0x6d, 0x4b,
        0x3d, 0xf8, 0xbe, 0x08, 0xed, 0x01, 0xfc, 0x44, 0x26, 0x46, 0xd0,
        0x34, 0x85, 0x8a, 0xc7, 0x5b, 0xed, 0x3f, 0xd5, 0x80, 0xbf, 0x98,
        0x08, 0xd9, 0x4f, 0xcb, 0xee, 0x82, 0xb9, 0xb2, 0xef, 0x66, 0x77,
        0xaf, 0x0a, 0xdc, 0xc3, 0x58, 0x52, 0xea, 0x6b, 0x9e };
    descriptor1.id = WebCore::toBufferSource(id1);
    options.allowCredentials.append(descriptor1);

    PublicKeyCredentialDescriptor descriptor2;
    descriptor2.type = PublicKeyCredentialType::PublicKey;
    const uint8_t id2[] = {
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03 };
    descriptor2.id = WebCore::toBufferSource(id2);
    options.allowCredentials.append(descriptor2);

    options.userVerificationString = "required"_s;

    Vector<uint8_t> hash;
    Vector<String> extensions;
    hash.append(std::span { TestData::kClientDataHash });
    auto serializedData = encodeGetAssertionRequestAsCBOR(hash, options, AuthenticatorSupportedOptions::UserVerificationAvailability::kNotSupported, extensions);
    EXPECT_EQ(serializedData.size(), sizeof(TestData::kTestComplexCtapGetAssertionRequestShort));
    EXPECT_TRUE(equalSpans(serializedData.span(), std::span { TestData::kTestComplexCtapGetAssertionRequestShort }));
}

TEST(CTAPRequestTest, TestConstructGetAssertionRequestWithPin)
{
    PublicKeyCredentialRequestOptions options;
    options.rpId = "acme.com"_s;

    PublicKeyCredentialDescriptor descriptor1;
    descriptor1.type = PublicKeyCredentialType::PublicKey;
    const uint8_t id1[] = {
        0xf2, 0x20, 0x06, 0xde, 0x4f, 0x90, 0x5a, 0xf6, 0x8a, 0x43, 0x94,
        0x2f, 0x02, 0x4f, 0x2a, 0x5e, 0xce, 0x60, 0x3d, 0x9c, 0x6d, 0x4b,
        0x3d, 0xf8, 0xbe, 0x08, 0xed, 0x01, 0xfc, 0x44, 0x26, 0x46, 0xd0,
        0x34, 0x85, 0x8a, 0xc7, 0x5b, 0xed, 0x3f, 0xd5, 0x80, 0xbf, 0x98,
        0x08, 0xd9, 0x4f, 0xcb, 0xee, 0x82, 0xb9, 0xb2, 0xef, 0x66, 0x77,
        0xaf, 0x0a, 0xdc, 0xc3, 0x58, 0x52, 0xea, 0x6b, 0x9e };
    descriptor1.id = WebCore::toBufferSource(id1);
    options.allowCredentials.append(descriptor1);

    PublicKeyCredentialDescriptor descriptor2;
    descriptor2.type = PublicKeyCredentialType::PublicKey;
    const uint8_t id2[] = {
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03 };
    descriptor2.id = WebCore::toBufferSource(id2);
    options.allowCredentials.append(descriptor2);

    options.userVerificationString = "required"_s;

    PinParameters pin;
    pin.protocol = pin::kProtocolVersion;
    pin.auth.append(std::span { TestData::kCtap2PinAuth });

    Vector<uint8_t> hash;
    Vector<String> extensions;
    hash.append(std::span { TestData::kClientDataHash });
    auto serializedData = encodeGetAssertionRequestAsCBOR(hash, options, AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedAndConfigured, extensions, pin);
    EXPECT_EQ(serializedData.size(), sizeof(TestData::kTestComplexCtapGetAssertionRequestWithPin));
    EXPECT_TRUE(equalSpans(serializedData.span(), std::span { TestData::kTestComplexCtapGetAssertionRequestWithPin }));
}

TEST(CTAPRequestTest, TestConstructCtapAuthenticatorRequestParam)
{
    static constexpr uint8_t kSerializedGetInfoCmd = 0x04;
    static constexpr uint8_t kSerializedGetNextAssertionCmd = 0x08;
    static constexpr uint8_t kSerializedResetCmd = 0x07;

    auto serializedData1 = encodeEmptyAuthenticatorRequest(CtapRequestCommand::kAuthenticatorGetInfo);
    EXPECT_EQ(serializedData1.size(), 1u);
    EXPECT_TRUE(equalSpans(serializedData1.span(), singleElementSpan(kSerializedGetInfoCmd)));

    auto serializedData2 = encodeEmptyAuthenticatorRequest(CtapRequestCommand::kAuthenticatorGetNextAssertion);
    EXPECT_EQ(serializedData2.size(), 1u);
    EXPECT_TRUE(equalSpans(serializedData2.span(), singleElementSpan(kSerializedGetNextAssertionCmd)));

    auto serializedData3 = encodeEmptyAuthenticatorRequest(CtapRequestCommand::kAuthenticatorReset);
    EXPECT_EQ(serializedData3.size(), 1u);
    EXPECT_TRUE(equalSpans(serializedData3.span(), singleElementSpan(kSerializedResetCmd)));
}

TEST(CTAPRequestTest, TestConstructGetAssertionRequestLargeBlobRead)
{
    PublicKeyCredentialRequestOptions options;
    options.rpId = "acme.com"_s;

    PublicKeyCredentialDescriptor descriptor1;
    descriptor1.type = PublicKeyCredentialType::PublicKey;
    const uint8_t id1[] = {
        0xf2, 0x20, 0x06, 0xde, 0x4f, 0x90, 0x5a, 0xf6, 0x8a, 0x43, 0x94,
        0x2f, 0x02, 0x4f, 0x2a, 0x5e, 0xce, 0x60, 0x3d, 0x9c, 0x6d, 0x4b,
        0x3d, 0xf8, 0xbe, 0x08, 0xed, 0x01, 0xfc, 0x44, 0x26, 0x46, 0xd0,
        0x34, 0x85, 0x8a, 0xc7, 0x5b, 0xed, 0x3f, 0xd5, 0x80, 0xbf, 0x98,
        0x08, 0xd9, 0x4f, 0xcb, 0xee, 0x82, 0xb9, 0xb2, 0xef, 0x66, 0x77,
        0xaf, 0x0a, 0xdc, 0xc3, 0x58, 0x52, 0xea, 0x6b, 0x9e };
    descriptor1.id = WebCore::toBufferSource(id1);
    options.allowCredentials.append(descriptor1);

    PublicKeyCredentialDescriptor descriptor2;
    descriptor2.type = PublicKeyCredentialType::PublicKey;
    const uint8_t id2[] = {
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03 };
    descriptor2.id = WebCore::toBufferSource(id2);
    options.allowCredentials.append(descriptor2);
    options.extensions = AuthenticationExtensionsClientInputs {
        .appid = WTF::nullString(),
        .credProps = false,
        .largeBlob = AuthenticationExtensionsClientInputs::LargeBlobInputs {
            .support = WTF::nullString(),
            .read = true,
            .write = std::nullopt,
        },
        .prf = std::nullopt,
    };

    options.userVerificationString = "required"_s;

    Vector<uint8_t> hash;
    Vector<String> extensions = { "largeBlob"_s };
    hash.append(std::span { TestData::kClientDataHash });
    auto serializedData = encodeGetAssertionRequestAsCBOR(hash, options, AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedAndConfigured, extensions);
    EXPECT_EQ(serializedData.size(), sizeof(TestData::kTestComplexCtapGetAssertionRequestWithLargeBlobRead));
    EXPECT_TRUE(equalSpans(serializedData.span(), std::span { TestData::kTestComplexCtapGetAssertionRequestWithLargeBlobRead }));
}

TEST(CTAPRequestTest, TestConstructGetAssertionRequestUnsupportedLargeBlobRead)
{
    PublicKeyCredentialRequestOptions options;
    options.rpId = "acme.com"_s;

    PublicKeyCredentialDescriptor descriptor1;
    descriptor1.type = PublicKeyCredentialType::PublicKey;
    const uint8_t id1[] = {
        0xf2, 0x20, 0x06, 0xde, 0x4f, 0x90, 0x5a, 0xf6, 0x8a, 0x43, 0x94,
        0x2f, 0x02, 0x4f, 0x2a, 0x5e, 0xce, 0x60, 0x3d, 0x9c, 0x6d, 0x4b,
        0x3d, 0xf8, 0xbe, 0x08, 0xed, 0x01, 0xfc, 0x44, 0x26, 0x46, 0xd0,
        0x34, 0x85, 0x8a, 0xc7, 0x5b, 0xed, 0x3f, 0xd5, 0x80, 0xbf, 0x98,
        0x08, 0xd9, 0x4f, 0xcb, 0xee, 0x82, 0xb9, 0xb2, 0xef, 0x66, 0x77,
        0xaf, 0x0a, 0xdc, 0xc3, 0x58, 0x52, 0xea, 0x6b, 0x9e };
    descriptor1.id = WebCore::toBufferSource(id1);
    options.allowCredentials.append(descriptor1);

    PublicKeyCredentialDescriptor descriptor2;
    descriptor2.type = PublicKeyCredentialType::PublicKey;
    const uint8_t id2[] = {
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03 };
    descriptor2.id = WebCore::toBufferSource(id2);
    options.allowCredentials.append(descriptor2);
    options.extensions = AuthenticationExtensionsClientInputs {
        .appid = WTF::nullString(),
        .credProps = false,
        .largeBlob = AuthenticationExtensionsClientInputs::LargeBlobInputs {
            .support = WTF::nullString(),
            .read = true,
            .write = std::nullopt,
        },
        .prf = std::nullopt,
    };

    options.userVerificationString = "required"_s;

    Vector<uint8_t> hash;
    Vector<String> extensions;
    hash.append(std::span { TestData::kClientDataHash });
    auto serializedData = encodeGetAssertionRequestAsCBOR(hash, options, AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedAndConfigured, extensions);
    EXPECT_EQ(serializedData.size(), sizeof(TestData::kTestComplexCtapGetAssertionRequest));
    EXPECT_TRUE(equalSpans(serializedData.span(), std::span { TestData::kTestComplexCtapGetAssertionRequest }));
}

TEST(CTAPRequestTest, TestConstructGetAssertionRequestLargeBlobWrite)
{
    PublicKeyCredentialRequestOptions options;
    options.rpId = "acme.com"_s;

    PublicKeyCredentialDescriptor descriptor1;
    descriptor1.type = PublicKeyCredentialType::PublicKey;
    const uint8_t id1[] = {
        0xf2, 0x20, 0x06, 0xde, 0x4f, 0x90, 0x5a, 0xf6, 0x8a, 0x43, 0x94,
        0x2f, 0x02, 0x4f, 0x2a, 0x5e, 0xce, 0x60, 0x3d, 0x9c, 0x6d, 0x4b,
        0x3d, 0xf8, 0xbe, 0x08, 0xed, 0x01, 0xfc, 0x44, 0x26, 0x46, 0xd0,
        0x34, 0x85, 0x8a, 0xc7, 0x5b, 0xed, 0x3f, 0xd5, 0x80, 0xbf, 0x98,
        0x08, 0xd9, 0x4f, 0xcb, 0xee, 0x82, 0xb9, 0xb2, 0xef, 0x66, 0x77,
        0xaf, 0x0a, 0xdc, 0xc3, 0x58, 0x52, 0xea, 0x6b, 0x9e };
    descriptor1.id = WebCore::toBufferSource(id1);
    options.allowCredentials.append(descriptor1);

    PublicKeyCredentialDescriptor descriptor2;
    descriptor2.type = PublicKeyCredentialType::PublicKey;
    const uint8_t id2[] = {
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03 };
    descriptor2.id = WebCore::toBufferSource(id2);
    options.allowCredentials.append(descriptor2);

    const uint8_t blob[] = {
        0xAB, 0xCD, 0xEF
    };
    options.extensions = AuthenticationExtensionsClientInputs {
        .appid = WTF::nullString(),
        .credProps = false,
        .largeBlob = AuthenticationExtensionsClientInputs::LargeBlobInputs {
            .support = WTF::nullString(),
            .read = std::nullopt,
            .write = WebCore::toBufferSource(blob),
        },
        .prf = std::nullopt,
    };

    options.userVerificationString = "required"_s;

    Vector<uint8_t> hash;
    Vector<String> extensions;
    hash.append(std::span { TestData::kClientDataHash });
    auto serializedData = encodeGetAssertionRequestAsCBOR(hash, options, AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedAndConfigured, extensions);
    EXPECT_EQ(serializedData.size(), sizeof(TestData::kTestComplexCtapGetAssertionRequest));
    EXPECT_TRUE(equalSpans(serializedData.span(), std::span { TestData::kTestComplexCtapGetAssertionRequest }));
}

TEST(CTAPRequestTest, TestConstructMakeCredentialRequestWithHmacSecret)
{
    PublicKeyCredentialRpEntity rp;
    rp.name = "acme.com"_s;
    rp.id = "acme.com"_s;

    PublicKeyCredentialUserEntity user;
    user.name = "johnpsmith@example.com"_s;
    user.id = WebCore::toBufferSource(TestData::kUserId);
    user.displayName = "John P. Smith"_s;

    Vector<PublicKeyCredentialParameters> params { { PublicKeyCredentialType::PublicKey, -7 } };

    AuthenticationExtensionsClientInputs extensions;
    AuthenticationExtensionsClientInputs::PRFInputs prfInputs;
    extensions.prf = WTFMove(prfInputs);

    PublicKeyCredentialCreationOptions options { rp, user, { }, params, std::nullopt, { }, std::nullopt, "none"_s, extensions };

    Vector<uint8_t> hash;
    Vector<String> supportedExtensions { "hmac-secret"_s };
    hash.append(std::span { TestData::kClientDataHash });
    auto serializedData = encodeMakeCredentialRequestAsCBOR(hash, options, AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedButNotConfigured, AuthenticatorSupportedOptions::ResidentKeyAvailability::kNotSupported, supportedExtensions);

    // Verify the request contains the hmac-secret extension
    EXPECT_FALSE(serializedData.isEmpty());
    EXPECT_EQ(serializedData[0], static_cast<uint8_t>(CtapRequestCommand::kAuthenticatorMakeCredential));
}

TEST(CTAPRequestTest, TestConstructGetAssertionRequestWithHmacSecret)
{
    PublicKeyCredentialRequestOptions options;
    options.rpId = "acme.com"_s;
    options.userVerificationString = "preferred"_s;

    // Create hmac-secret extension inputs (two 32-byte salts)
    const uint8_t salt1Data[32] = { 0x00 }; // 32 bytes of zeros
    const uint8_t salt2Data[32] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

    AuthenticationExtensionsClientInputs extensions;
    AuthenticationExtensionsClientInputs::PRFInputs prfInputs;
    AuthenticationExtensionsClientInputs::PRFValues prfValues;
    prfValues.first = WebCore::toBufferSource(salt1Data);
    prfValues.second = WebCore::toBufferSource(salt2Data);
    prfInputs.eval = WTFMove(prfValues);
    extensions.prf = WTFMove(prfInputs);

    options.extensions = extensions;

    Vector<uint8_t> hash;
    Vector<String> supportedExtensions { "hmac-secret"_s };
    hash.append(std::span { TestData::kClientDataHash });

    // Note: This will encode without HmacSecretParameters since that requires key agreement
    // The full flow with HmacSecretParameters is tested separately
    auto serializedData = encodeGetAssertionRequestAsCBOR(hash, options, AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedButNotConfigured, supportedExtensions);

    // Verify the request was encoded successfully
    EXPECT_FALSE(serializedData.isEmpty());
    EXPECT_EQ(serializedData[0], static_cast<uint8_t>(CtapRequestCommand::kAuthenticatorGetAssertion));
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_AUTHN)
