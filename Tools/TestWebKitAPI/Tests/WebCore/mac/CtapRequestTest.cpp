// Copyright 2017 The Chromium Authors. All rights reserved.
// Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include <WebCore/DeviceRequestConverter.h>
#include <WebCore/FidoConstants.h>
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
    PublicKeyCredentialCreationOptions::RpEntity rp;
    rp.name = "Acme";
    rp.id = "acme.com";

    PublicKeyCredentialCreationOptions::UserEntity user;
    user.name = "johnpsmith@example.com";
    user.icon = "https://pics.acme.com/00/p/aBjjjpqPb.png";
    user.idVector.append(TestData::kUserId, sizeof(TestData::kUserId));
    user.displayName = "John P. Smith";

    Vector<PublicKeyCredentialCreationOptions::Parameters> params { { PublicKeyCredentialType::PublicKey, 7 }, { PublicKeyCredentialType::PublicKey, 257 } };
    PublicKeyCredentialCreationOptions::AuthenticatorSelectionCriteria selection { PublicKeyCredentialCreationOptions::AuthenticatorAttachment::Platform, true, UserVerificationRequirement::Preferred };

    PublicKeyCredentialCreationOptions options { rp, user, { }, params, std::nullopt, { }, selection };
    Vector<uint8_t> hash;
    hash.append(TestData::kClientDataHash, sizeof(TestData::kClientDataHash));
    auto serializedData = encodeMakeCredenitalRequestAsCBOR(hash, options, AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedButNotConfigured);
    EXPECT_EQ(serializedData.size(), sizeof(TestData::kCtapMakeCredentialRequest));
    EXPECT_EQ(memcmp(serializedData.data(), TestData::kCtapMakeCredentialRequest, serializedData.size()), 0);
}

TEST(CTAPRequestTest, TestConstructGetAssertionRequest)
{
    PublicKeyCredentialRequestOptions options;
    options.rpId = "acme.com";

    PublicKeyCredentialDescriptor descriptor1;
    descriptor1.type = PublicKeyCredentialType::PublicKey;
    const uint8_t id1[] = {
        0xf2, 0x20, 0x06, 0xde, 0x4f, 0x90, 0x5a, 0xf6, 0x8a, 0x43, 0x94,
        0x2f, 0x02, 0x4f, 0x2a, 0x5e, 0xce, 0x60, 0x3d, 0x9c, 0x6d, 0x4b,
        0x3d, 0xf8, 0xbe, 0x08, 0xed, 0x01, 0xfc, 0x44, 0x26, 0x46, 0xd0,
        0x34, 0x85, 0x8a, 0xc7, 0x5b, 0xed, 0x3f, 0xd5, 0x80, 0xbf, 0x98,
        0x08, 0xd9, 0x4f, 0xcb, 0xee, 0x82, 0xb9, 0xb2, 0xef, 0x66, 0x77,
        0xaf, 0x0a, 0xdc, 0xc3, 0x58, 0x52, 0xea, 0x6b, 0x9e };
    descriptor1.idVector.append(id1, sizeof(id1));
    options.allowCredentials.append(descriptor1);

    PublicKeyCredentialDescriptor descriptor2;
    descriptor2.type = PublicKeyCredentialType::PublicKey;
    const uint8_t id2[] = {
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03 };
    descriptor2.idVector.append(id2, sizeof(id2));
    options.allowCredentials.append(descriptor2);

    options.userVerification = UserVerificationRequirement::Required;

    Vector<uint8_t> hash;
    hash.append(TestData::kClientDataHash, sizeof(TestData::kClientDataHash));
    auto serializedData = encodeGetAssertionRequestAsCBOR(hash, options, AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedButNotConfigured);
    EXPECT_EQ(serializedData.size(), sizeof(TestData::kTestComplexCtapGetAssertionRequest));
    EXPECT_EQ(memcmp(serializedData.data(), TestData::kTestComplexCtapGetAssertionRequest, serializedData.size()), 0);
}

TEST(CTAPRequestTest, TestConstructCtapAuthenticatorRequestParam)
{
    static constexpr uint8_t kSerializedGetInfoCmd = 0x04;
    static constexpr uint8_t kSerializedGetNextAssertionCmd = 0x08;
    static constexpr uint8_t kSerializedResetCmd = 0x07;

    auto serializedData1 = encodeEmptyAuthenticatorRequest(CtapRequestCommand::kAuthenticatorGetInfo);
    EXPECT_EQ(serializedData1.size(), 1u);
    EXPECT_EQ(memcmp(serializedData1.data(), &kSerializedGetInfoCmd, 1), 0);

    auto serializedData2 = encodeEmptyAuthenticatorRequest(CtapRequestCommand::kAuthenticatorGetNextAssertion);
    EXPECT_EQ(serializedData2.size(), 1u);
    EXPECT_EQ(memcmp(serializedData2.data(), &kSerializedGetNextAssertionCmd, 1), 0);

    auto serializedData3 = encodeEmptyAuthenticatorRequest(CtapRequestCommand::kAuthenticatorReset);
    EXPECT_EQ(serializedData3.size(), 1u);
    EXPECT_EQ(memcmp(serializedData3.data(), &kSerializedResetCmd, 1), 0);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_AUTHN)
