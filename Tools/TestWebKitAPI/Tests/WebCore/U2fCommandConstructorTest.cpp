// Copyright 2018 The Chromium Authors. All rights reserved.
// Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include <WebCore/FidoConstants.h>
#include <WebCore/PublicKeyCredentialCreationOptions.h>
#include <WebCore/PublicKeyCredentialRequestOptions.h>
#include <WebCore/U2fCommandConstructor.h>
#include <WebCore/WebAuthenticationConstants.h>
#include <WebCore/WebAuthenticationUtils.h>

namespace TestWebKitAPI {
using namespace WebCore;
using namespace fido;

PublicKeyCredentialCreationOptions constructMakeCredentialRequest()
{
    PublicKeyCredentialCreationOptions::RpEntity rp;
    rp.id = "acme.com";
    rp.name = "acme.com";

    PublicKeyCredentialCreationOptions::UserEntity user;
    user.idVector = convertBytesToVector(TestData::kUserId, sizeof(TestData::kUserId));
    user.name = "johnpsmith@example.com";
    user.displayName = "John P. Smith";
    user.icon = "https://pics.acme.com/00/p/aBjjjpqPb.png";

    PublicKeyCredentialCreationOptions::Parameters params;
    params.type = PublicKeyCredentialType::PublicKey;
    params.alg = COSE::ES256;

    AuthenticationExtensionsClientInputs extensions;
    extensions.googleLegacyAppidSupport = false;

    PublicKeyCredentialCreationOptions options;
    options.rp = WTFMove(rp);
    options.user = WTFMove(user);
    options.pubKeyCredParams.append(WTFMove(params));
    options.extensions = WTFMove(extensions);

    return options;
}

PublicKeyCredentialCreationOptions constructMakeCredentialRequestWithGoogleLegacyAppidSupport()
{
    auto options = constructMakeCredentialRequest();
    options.extensions->googleLegacyAppidSupport = true;
    return options;
}

PublicKeyCredentialRequestOptions constructGetAssertionRequest()
{
    PublicKeyCredentialRequestOptions options;
    options.rpId = "acme.com";
    return options;
}

TEST(U2fCommandConstructorTest, TestConvertCtapMakeCredentialToU2fRegister)
{
    const auto makeCredentialParam = constructMakeCredentialRequest();

    EXPECT_TRUE(isConvertibleToU2fRegisterCommand(makeCredentialParam));

    const auto u2fRegisterCommand = convertToU2fRegisterCommand(convertBytesToVector(TestData::kClientDataHash, sizeof(TestData::kClientDataHash)), makeCredentialParam);
    ASSERT_TRUE(u2fRegisterCommand);
    EXPECT_EQ(*u2fRegisterCommand, convertBytesToVector(TestData::kU2fRegisterCommandApdu, sizeof(TestData::kU2fRegisterCommandApdu)));
}

TEST(U2fCommandConstructorTest, TestConvertCtapMakeCredentialToU2fRegisterWithGoogleLegacyAppidSupport)
{
    const auto makeCredentialParam = constructMakeCredentialRequestWithGoogleLegacyAppidSupport();

    EXPECT_TRUE(isConvertibleToU2fRegisterCommand(makeCredentialParam));

    const auto u2fRegisterCommand = convertToU2fRegisterCommand(convertBytesToVector(TestData::kClientDataHash, sizeof(TestData::kClientDataHash)), makeCredentialParam);
    ASSERT_TRUE(u2fRegisterCommand);
    EXPECT_EQ(*u2fRegisterCommand, convertBytesToVector(TestData::kU2fRegisterCommandApduWithGoogleLegacyAppidSupport, sizeof(TestData::kU2fRegisterCommandApduWithGoogleLegacyAppidSupport)));
}

TEST(U2fCommandConstructorTest, TestConvertCtapMakeCredentialToU2fCheckOnlySign)
{
    auto makeCredentialParam = constructMakeCredentialRequest();
    PublicKeyCredentialDescriptor credentialDescriptor;
    credentialDescriptor.type = PublicKeyCredentialType::PublicKey;
    credentialDescriptor.idVector = convertBytesToVector(TestData::kU2fSignKeyHandle, sizeof(TestData::kU2fSignKeyHandle));
    Vector<PublicKeyCredentialDescriptor> excludeList;
    excludeList.append(credentialDescriptor);
    makeCredentialParam.excludeCredentials = WTFMove(excludeList);
    EXPECT_TRUE(isConvertibleToU2fRegisterCommand(makeCredentialParam));

    const auto u2fCheckOnlySign = convertToU2fCheckOnlySignCommand(convertBytesToVector(TestData::kClientDataHash, sizeof(TestData::kClientDataHash)), makeCredentialParam, credentialDescriptor);
    ASSERT_TRUE(u2fCheckOnlySign);
    EXPECT_EQ(*u2fCheckOnlySign, convertBytesToVector(TestData::kU2fCheckOnlySignCommandApdu, sizeof(TestData::kU2fCheckOnlySignCommandApdu)));
}

TEST(U2fCommandConstructorTest, TestConvertCtapMakeCredentialToU2fCheckOnlySignWithInvalidCredentialType)
{
    auto makeCredentialParam = constructMakeCredentialRequest();
    PublicKeyCredentialDescriptor credentialDescriptor;
    credentialDescriptor.type = static_cast<PublicKeyCredentialType>(-1);
    credentialDescriptor.idVector = convertBytesToVector(TestData::kU2fSignKeyHandle, sizeof(TestData::kU2fSignKeyHandle));
    Vector<PublicKeyCredentialDescriptor> excludeList;
    excludeList.append(credentialDescriptor);
    makeCredentialParam.excludeCredentials = WTFMove(excludeList);
    EXPECT_TRUE(isConvertibleToU2fRegisterCommand(makeCredentialParam));

    const auto u2fCheckOnlySign = convertToU2fCheckOnlySignCommand(convertBytesToVector(TestData::kClientDataHash, sizeof(TestData::kClientDataHash)), makeCredentialParam, credentialDescriptor);
    EXPECT_FALSE(u2fCheckOnlySign);
}

TEST(U2fCommandConstructorTest, TestU2fRegisterCredentialAlgorithmRequirement)
{
    PublicKeyCredentialCreationOptions::RpEntity rp;
    rp.id = "acme.com";
    rp.name = "acme.com";

    PublicKeyCredentialCreationOptions::UserEntity user;
    user.idVector = convertBytesToVector(TestData::kUserId, sizeof(TestData::kUserId));
    user.name = "johnpsmith@example.com";
    user.displayName = "John P. Smith";
    user.icon = "https://pics.acme.com/00/p/aBjjjpqPb.png";

    PublicKeyCredentialCreationOptions::Parameters params;
    params.type = PublicKeyCredentialType::PublicKey;
    params.alg = -257;

    PublicKeyCredentialCreationOptions makeCredentialParam;
    makeCredentialParam.rp = WTFMove(rp);
    makeCredentialParam.user = WTFMove(user);
    makeCredentialParam.pubKeyCredParams.append(WTFMove(params));

    EXPECT_FALSE(isConvertibleToU2fRegisterCommand(makeCredentialParam));
}

TEST(U2fCommandConstructorTest, TestU2fRegisterUserVerificationRequirement)
{
    auto makeCredentialParam = constructMakeCredentialRequest();
    PublicKeyCredentialCreationOptions::AuthenticatorSelectionCriteria selection;
    selection.userVerification = UserVerificationRequirement::Required;
    makeCredentialParam.authenticatorSelection = WTFMove(selection);

    EXPECT_FALSE(isConvertibleToU2fRegisterCommand(makeCredentialParam));
}

TEST(U2fCommandConstructorTest, TestU2fRegisterResidentKeyRequirement)
{
    auto makeCredentialParam = constructMakeCredentialRequest();
    PublicKeyCredentialCreationOptions::AuthenticatorSelectionCriteria selection;
    selection.requireResidentKey = true;
    makeCredentialParam.authenticatorSelection = WTFMove(selection);

    EXPECT_FALSE(isConvertibleToU2fRegisterCommand(makeCredentialParam));
}

TEST(U2fCommandConstructorTest, TestConvertCtapGetAssertionToU2fSignRequest)
{
    auto getAssertionReq = constructGetAssertionRequest();
    PublicKeyCredentialDescriptor credentialDescriptor;
    credentialDescriptor.type = PublicKeyCredentialType::PublicKey;
    credentialDescriptor.idVector = convertBytesToVector(TestData::kU2fSignKeyHandle, sizeof(TestData::kU2fSignKeyHandle));
    Vector<PublicKeyCredentialDescriptor> allowedList;
    allowedList.append(WTFMove(credentialDescriptor));
    getAssertionReq.allowCredentials = WTFMove(allowedList);
    EXPECT_TRUE(isConvertibleToU2fSignCommand(getAssertionReq));

    const auto u2fSignCommand = convertToU2fSignCommand(convertBytesToVector(TestData::kClientDataHash, sizeof(TestData::kClientDataHash)), getAssertionReq, convertBytesToVector(TestData::kU2fSignKeyHandle, sizeof(TestData::kU2fSignKeyHandle)));
    ASSERT_TRUE(u2fSignCommand);
    EXPECT_EQ(*u2fSignCommand, convertBytesToVector(TestData::kU2fSignCommandApdu, sizeof(TestData::kU2fSignCommandApdu)));
}

TEST(U2fCommandConstructorTest, TestConvertCtapGetAssertionWithAppIDToU2fSignRequest)
{
    auto getAssertionReq = constructGetAssertionRequest();
    PublicKeyCredentialDescriptor credentialDescriptor;
    credentialDescriptor.type = PublicKeyCredentialType::PublicKey;
    credentialDescriptor.idVector = convertBytesToVector(TestData::kU2fSignKeyHandle, sizeof(TestData::kU2fSignKeyHandle));
    Vector<PublicKeyCredentialDescriptor> allowedList;
    allowedList.append(WTFMove(credentialDescriptor));
    getAssertionReq.allowCredentials = WTFMove(allowedList);
    EXPECT_TRUE(isConvertibleToU2fSignCommand(getAssertionReq));

    // AppID
    WebCore::AuthenticationExtensionsClientInputs extensions;
    extensions.appid = "https://www.example.com/appid";
    getAssertionReq.extensions = WTFMove(extensions);

    const auto u2fSignCommand = convertToU2fSignCommand(convertBytesToVector(TestData::kClientDataHash, sizeof(TestData::kClientDataHash)), getAssertionReq, convertBytesToVector(TestData::kU2fSignKeyHandle, sizeof(TestData::kU2fSignKeyHandle)), true);
    ASSERT_TRUE(u2fSignCommand);
    EXPECT_EQ(*u2fSignCommand, convertBytesToVector(TestData::kU2fAppIDSignCommandApdu, sizeof(TestData::kU2fAppIDSignCommandApdu)));
}

TEST(U2fCommandConstructorTest, TestU2fSignAllowListRequirement)
{
    auto getAssertionReq = constructGetAssertionRequest();
    EXPECT_FALSE(isConvertibleToU2fSignCommand(getAssertionReq));
}

TEST(U2fCommandConstructorTest, TestU2fSignUserVerificationRequirement)
{
    auto getAssertionReq = constructGetAssertionRequest();
    PublicKeyCredentialDescriptor credentialDescriptor;
    credentialDescriptor.type = PublicKeyCredentialType::PublicKey;
    credentialDescriptor.idVector = convertBytesToVector(TestData::kU2fSignKeyHandle, sizeof(TestData::kU2fSignKeyHandle));
    Vector<PublicKeyCredentialDescriptor> allowedList;
    allowedList.append(WTFMove(credentialDescriptor));
    getAssertionReq.allowCredentials = WTFMove(allowedList);
    getAssertionReq.userVerification = UserVerificationRequirement::Required;

    EXPECT_FALSE(isConvertibleToU2fSignCommand(getAssertionReq));
}

TEST(U2fCommandConstructorTest, TestCreateSignWithIncorrectKeyHandle)
{
    auto getAssertionReq = constructGetAssertionRequest();
    PublicKeyCredentialDescriptor credentialDescriptor;
    credentialDescriptor.type = PublicKeyCredentialType::PublicKey;
    credentialDescriptor.idVector = convertBytesToVector(TestData::kU2fSignKeyHandle, sizeof(TestData::kU2fSignKeyHandle));
    Vector<PublicKeyCredentialDescriptor> allowedList;
    allowedList.append(WTFMove(credentialDescriptor));
    getAssertionReq.allowCredentials = WTFMove(allowedList);
    ASSERT_TRUE(isConvertibleToU2fSignCommand(getAssertionReq));

    Vector<uint8_t> keyHandle(kMaxKeyHandleLength, 0xff);
    const auto validSignCommand = convertToU2fSignCommand(convertBytesToVector(TestData::kClientDataHash, sizeof(TestData::kClientDataHash)), getAssertionReq, keyHandle);
    EXPECT_TRUE(validSignCommand);

    keyHandle.append(0xff);
    const auto invalidSignCommand = convertToU2fSignCommand(convertBytesToVector(TestData::kClientDataHash, sizeof(TestData::kClientDataHash)), getAssertionReq, keyHandle);
    EXPECT_FALSE(invalidSignCommand);
}

TEST(U2fCommandConstructorTest, TestConstructBogusU2fRegistrationCommand)
{
    EXPECT_EQ(constructBogusU2fRegistrationCommand(), convertBytesToVector(TestData::kU2fFakeRegisterCommand, sizeof(TestData::kU2fFakeRegisterCommand)));
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_AUTHN)
