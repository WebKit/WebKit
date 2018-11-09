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
#include <WebCore/CBORReader.h>
#include <WebCore/CBORValue.h>
#include <WebCore/CBORWriter.h>
#include <WebCore/DeviceResponseConverter.h>
#include <WebCore/FidoConstants.h>

namespace TestWebKitAPI {
using namespace fido;

constexpr uint8_t kTestAuthenticatorGetInfoResponseWithNoVersion[] = {
    // Success status byte
    0x00,
    // Map of 6 elements
    0xA6,
    // Key(01) - versions
    0x01,
    // Array(0)
    0x80,
    // Key(02) - extensions
    0x02,
    // Array(2)
    0x82,
    // "uvm"
    0x63, 0x75, 0x76, 0x6D,
    // "hmac-secret"
    0x6B, 0x68, 0x6D, 0x61, 0x63, 0x2D, 0x73, 0x65, 0x63, 0x72, 0x65, 0x74,
    // Key(03) - AAGUID
    0x03,
    // Bytes(16)
    0x50, 0xF8, 0xA0, 0x11, 0xF3, 0x8C, 0x0A, 0x4D, 0x15, 0x80, 0x06, 0x17,
    0x11, 0x1F, 0x9E, 0xDC, 0x7D,
    // Key(04) - options
    0x04,
    // Map(05)
    0xA5,
    // Key - "rk"
    0x62, 0x72, 0x6B,
    // true
    0xF5,
    // Key - "up"
    0x62, 0x75, 0x70,
    // true
    0xF5,
    // Key - "uv"
    0x62, 0x75, 0x76,
    // true
    0xF5,
    // Key - "plat"
    0x64, 0x70, 0x6C, 0x61, 0x74,
    // true
    0xF5,
    // Key - "clientPin"
    0x69, 0x63, 0x6C, 0x69, 0x65, 0x6E, 0x74, 0x50, 0x69, 0x6E,
    // false
    0xF4,
    // Key(05) - Max message size
    0x05,
    // 1200
    0x19, 0x04, 0xB0,
    // Key(06) - Pin protocols
    0x06,
    // Array[1]
    0x81, 0x01,
};

constexpr uint8_t kTestAuthenticatorGetInfoResponseWithDuplicateVersion[] = {
    // Success status byte
    0x00,
    // Map of 6 elements
    0xA6,
    // Key(01) - versions
    0x01,
    // Array(02)
    0x82,
    // "U2F_V2"
    0x66, 0x55, 0x32, 0x46, 0x5F, 0x56, 0x32,
    // "U2F_V2"
    0x66, 0x55, 0x32, 0x46, 0x5F, 0x56, 0x32,
    // Key(02) - extensions
    0x02,
    // Array(2)
    0x82,
    // "uvm"
    0x63, 0x75, 0x76, 0x6D,
    // "hmac-secret"
    0x6B, 0x68, 0x6D, 0x61, 0x63, 0x2D, 0x73, 0x65, 0x63, 0x72, 0x65, 0x74,
    // Key(03) - AAGUID
    0x03,
    // Bytes(16)
    0x50, 0xF8, 0xA0, 0x11, 0xF3, 0x8C, 0x0A, 0x4D, 0x15, 0x80, 0x06, 0x17,
    0x11, 0x1F, 0x9E, 0xDC, 0x7D,
    // Key(04) - options
    0x04,
    // Map(05)
    0xA5,
    // Key - "rk"
    0x62, 0x72, 0x6B,
    // true
    0xF5,
    // Key - "up"
    0x62, 0x75, 0x70,
    // true
    0xF5,
    // Key - "uv"
    0x62, 0x75, 0x76,
    // true
    0xF5,
    // Key - "plat"
    0x64, 0x70, 0x6C, 0x61, 0x74,
    // true
    0xF5,
    // Key - "clientPin"
    0x69, 0x63, 0x6C, 0x69, 0x65, 0x6E, 0x74, 0x50, 0x69, 0x6E,
    // false
    0xF4,
    // Key(05) - Max message size
    0x05,
    // 1200
    0x19, 0x04, 0xB0,
    // Key(06) - Pin protocols
    0x06,
    // Array[1]
    0x81, 0x01,
};

constexpr uint8_t kTestAuthenticatorGetInfoResponseWithIncorrectAaguid[] = {
    // Success status byte
    0x00,
    // Map of 6 elements
    0xA6,
    // Key(01) - versions
    0x01,
    // Array(01)
    0x81,
    // "U2F_V2"
    0x66, 0x55, 0x32, 0x46, 0x5F, 0x56, 0x32,
    // Key(02) - extensions
    0x02,
    // Array(2)
    0x82,
    // "uvm"
    0x63, 0x75, 0x76, 0x6D,
    // "hmac-secret"
    0x6B, 0x68, 0x6D, 0x61, 0x63, 0x2D, 0x73, 0x65, 0x63, 0x72, 0x65, 0x74,
    // Key(03) - AAGUID
    0x03,
    // Bytes(17) - FIDO2 device AAGUID must be 16 bytes long in order to be
    // correct.
    0x51, 0xF8, 0xA0, 0x11, 0xF3, 0x8C, 0x0A, 0x4D, 0x15, 0x80, 0x06, 0x17,
    0x11, 0x1F, 0x9E, 0xDC, 0x7D, 0x00,
    // Key(04) - options
    0x04,
    // Map(05)
    0xA5,
    // Key - "rk"
    0x62, 0x72, 0x6B,
    // true
    0xF5,
    // Key - "up"
    0x62, 0x75, 0x70,
    // true
    0xF5,
    // Key - "uv"
    0x62, 0x75, 0x76,
    // true
    0xF5,
    // Key - "plat"
    0x64, 0x70, 0x6C, 0x61, 0x74,
    // true
    0xF5,
    // Key - "clientPin"
    0x69, 0x63, 0x6C, 0x69, 0x65, 0x6E, 0x74, 0x50, 0x69, 0x6E,
    // false
    0xF4,
    // Key(05) - Max message size
    0x05,
    // 1200
    0x19, 0x04, 0xB0,
    // Key(06) - Pin protocols
    0x06,
    // Array[1]
    0x81, 0x01,
};

constexpr uint8_t kTestDeviceAaguid[] = {
    0xF8, 0xA0, 0x11, 0xF3, 0x8C, 0x0A, 0x4D, 0x15, 0x80, 0x06, 0x17, 0x11, 0x1F, 0x9E, 0xDC, 0x7D
};

// Get a subset of the response for testing error handling.
Vector<uint8_t> getTestCorruptedSignResponse(size_t length)
{
    ASSERT(length < sizeof(TestData::kTestU2fSignResponse));
    Vector<uint8_t> testCorruptedSignResponse;
    testCorruptedSignResponse.reserveInitialCapacity(length);
    testCorruptedSignResponse.append(TestData::kTestU2fSignResponse, length);
    return testCorruptedSignResponse;
}

// Return a key handle used for GetAssertion request.
Vector<uint8_t> getTestCredentialRawIdBytes()
{
    Vector<uint8_t> testCredentialRawIdBytes;
    testCredentialRawIdBytes.reserveInitialCapacity(sizeof(TestData::kU2fSignKeyHandle));
    testCredentialRawIdBytes.append(TestData::kU2fSignKeyHandle, sizeof(TestData::kU2fSignKeyHandle));
    return testCredentialRawIdBytes;
}

Vector<uint8_t> convertToVector(const uint8_t byteArray[], const size_t length)
{
    Vector<uint8_t> result;
    result.reserveInitialCapacity(length);
    result.append(byteArray, length);
    return result;
}

// Leveraging example 4 of section 6.1 of the spec
// https://fidoalliance.org/specs/fido-v2.0-ps-20170927/fido-client-to-authenticator-protocol-v2.0-ps-20170927.html#commands
TEST(CTAPResponseTest, TestReadMakeCredentialResponse)
{
    auto makeCredentialResponse = readCTAPMakeCredentialResponse(convertToVector(TestData::kTestMakeCredentialResponse, sizeof(TestData::kTestMakeCredentialResponse)));
    ASSERT_TRUE(makeCredentialResponse);
    auto cborAttestationObject = cbor::CBORReader::read(convertToVector(reinterpret_cast<uint8_t*>(makeCredentialResponse->attestationObject->data()), makeCredentialResponse->attestationObject->byteLength()));
    ASSERT_TRUE(cborAttestationObject);
    ASSERT_TRUE(cborAttestationObject->isMap());

    const auto& attestationObjectMap = cborAttestationObject->getMap();
    auto it = attestationObjectMap.find(cbor::CBORValue(kFormatKey));
    ASSERT_TRUE(it != attestationObjectMap.end());
    ASSERT_TRUE(it->second.isString());
    EXPECT_STREQ(it->second.getString().utf8().data(), "packed");

    it = attestationObjectMap.find(cbor::CBORValue(kAuthDataKey));
    ASSERT_TRUE(it != attestationObjectMap.end());
    ASSERT_TRUE(it->second.isByteString());
    EXPECT_EQ(it->second.getByteString(), convertToVector(TestData::kCtap2MakeCredentialAuthData, sizeof(TestData::kCtap2MakeCredentialAuthData)));

    it = attestationObjectMap.find(cbor::CBORValue(kAttestationStatementKey));
    ASSERT_TRUE(it != attestationObjectMap.end());
    ASSERT_TRUE(it->second.isMap());

    const auto& attestationStatementMap = it->second.getMap();
    auto attStmtIt = attestationStatementMap.find(cbor::CBORValue("alg"));

    ASSERT_TRUE(attStmtIt != attestationStatementMap.end());
    ASSERT_TRUE(attStmtIt->second.isInteger());
    EXPECT_EQ(attStmtIt->second.getInteger(), -7);

    attStmtIt = attestationStatementMap.find(cbor::CBORValue("sig"));
    ASSERT_TRUE(attStmtIt != attestationStatementMap.end());
    ASSERT_TRUE(attStmtIt->second.isByteString());
    EXPECT_EQ(attStmtIt->second.getByteString(), convertToVector(TestData::kCtap2MakeCredentialSignature, sizeof(TestData::kCtap2MakeCredentialSignature)));

    attStmtIt = attestationStatementMap.find(cbor::CBORValue("x5c"));
    ASSERT_TRUE(attStmtIt != attestationStatementMap.end());
    const auto& certificate = attStmtIt->second;
    ASSERT_TRUE(certificate.isArray());
    ASSERT_EQ(certificate.getArray().size(), 1u);
    ASSERT_TRUE(certificate.getArray()[0].isByteString());
    EXPECT_EQ(certificate.getArray()[0].getByteString(), convertToVector(TestData::kCtap2MakeCredentialCertificate, sizeof(TestData::kCtap2MakeCredentialCertificate)));
    EXPECT_EQ(makeCredentialResponse->rawId->byteLength(), sizeof(TestData::kCtap2MakeCredentialCredentialId));
    EXPECT_EQ(memcmp(makeCredentialResponse->rawId->data(), TestData::kCtap2MakeCredentialCredentialId, sizeof(TestData::kCtap2MakeCredentialCredentialId)), 0);
}

// Leveraging example 5 of section 6.1 of the CTAP spec.
// https://fidoalliance.org/specs/fido-v2.0-ps-20170927/fido-client-to-authenticator-protocol-v2.0-ps-20170927.html
TEST(CTAPResponseTest, TestReadGetAssertionResponse)
{
    auto getAssertionResponse = readCTAPGetAssertionResponse(convertToVector(TestData::kDeviceGetAssertionResponse, sizeof(TestData::kDeviceGetAssertionResponse)));
    ASSERT_TRUE(getAssertionResponse);

    EXPECT_EQ(getAssertionResponse->authenticatorData->byteLength(), sizeof(TestData::kCtap2GetAssertionAuthData));
    EXPECT_EQ(memcmp(getAssertionResponse->authenticatorData->data(), TestData::kCtap2GetAssertionAuthData, sizeof(TestData::kCtap2GetAssertionAuthData)), 0);
    EXPECT_EQ(getAssertionResponse->signature->byteLength(), sizeof(TestData::kCtap2GetAssertionSignature));
    EXPECT_EQ(memcmp(getAssertionResponse->signature->data(), TestData::kCtap2GetAssertionSignature, sizeof(TestData::kCtap2GetAssertionSignature)), 0);
}

TEST(CTAPResponseTest, TestReadGetInfoResponse)
{
    auto getInfoResponse = readCTAPGetInfoResponse(convertToVector(TestData::kTestGetInfoResponsePlatformDevice, sizeof(TestData::kTestGetInfoResponsePlatformDevice)));
    ASSERT_TRUE(getInfoResponse);
    ASSERT_TRUE(getInfoResponse->maxMsgSize());
    EXPECT_EQ(*getInfoResponse->maxMsgSize(), 1200u);
    EXPECT_NE(getInfoResponse->versions().find(ProtocolVersion::kCtap), getInfoResponse->versions().end());
    EXPECT_NE(getInfoResponse->versions().find(ProtocolVersion::kU2f), getInfoResponse->versions().end());
    EXPECT_TRUE(getInfoResponse->options().isPlatformDevice());
    EXPECT_TRUE(getInfoResponse->options().supportsResidentKey());
    EXPECT_TRUE(getInfoResponse->options().userPresenceRequired());
    EXPECT_EQ(AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedAndConfigured, getInfoResponse->options().userVerificationAvailability());
    EXPECT_EQ(AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedButPinNotSet, getInfoResponse->options().clientPinAvailability());
}

TEST(CTAPResponseTest, TestReadGetInfoResponseWithIncorrectFormat)
{
    EXPECT_FALSE(readCTAPGetInfoResponse(convertToVector(kTestAuthenticatorGetInfoResponseWithNoVersion, sizeof(kTestAuthenticatorGetInfoResponseWithNoVersion))));
    EXPECT_FALSE(readCTAPGetInfoResponse(convertToVector(kTestAuthenticatorGetInfoResponseWithDuplicateVersion, sizeof(kTestAuthenticatorGetInfoResponseWithDuplicateVersion))));
    EXPECT_FALSE(readCTAPGetInfoResponse(convertToVector(kTestAuthenticatorGetInfoResponseWithIncorrectAaguid, sizeof(kTestAuthenticatorGetInfoResponseWithIncorrectAaguid))));
}

TEST(CTAPResponseTest, TestSerializeGetInfoResponse)
{
    AuthenticatorGetInfoResponse response({ ProtocolVersion::kCtap, ProtocolVersion::kU2f }, convertToVector(kTestDeviceAaguid, sizeof(kTestDeviceAaguid)));
    response.setExtensions({ "uvm", "hmac-secret" });
    AuthenticatorSupportedOptions options;
    options.setSupportsResidentKey(true);
    options.setIsPlatformDevice(true);
    options.setClientPinAvailability(AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedButPinNotSet);
    options.setUserVerificationAvailability(AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedAndConfigured);
    response.setOptions(WTFMove(options));
    response.setMaxMsgSize(1200);
    response.setPinProtocols({ 1 });

    auto responseAsCBOR = encodeAsCBOR(response);
    EXPECT_EQ(responseAsCBOR.size(), sizeof(TestData::kTestGetInfoResponsePlatformDevice) - 1);
    EXPECT_EQ(memcmp(responseAsCBOR.data(), TestData::kTestGetInfoResponsePlatformDevice + 1, responseAsCBOR.size()), 0);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_AUTHN)
