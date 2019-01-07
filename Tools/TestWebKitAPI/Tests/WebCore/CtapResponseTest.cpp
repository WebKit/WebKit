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
#include <WebCore/U2fResponseConverter.h>
#include <WebCore/WebAuthenticationUtils.h>

namespace TestWebKitAPI {
using namespace WebCore;
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

// The attested credential data, excluding the public key bytes. Append
// with kTestECPublicKeyCOSE to get the complete attestation data.
constexpr uint8_t kTestAttestedCredentialDataPrefix[] = {
    // 16-byte aaguid
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    // 2-byte length
    0x00, 0x40,
    // 64-byte key handle
    0x3E, 0xBD, 0x89, 0xBF, 0x77, 0xEC, 0x50, 0x97, 0x55, 0xEE, 0x9C, 0x26,
    0x35, 0xEF, 0xAA, 0xAC, 0x7B, 0x2B, 0x9C, 0x5C, 0xEF, 0x17, 0x36, 0xC3,
    0x71, 0x7D, 0xA4, 0x85, 0x34, 0xC8, 0xC6, 0xB6, 0x54, 0xD7, 0xFF, 0x94,
    0x5F, 0x50, 0xB5, 0xCC, 0x4E, 0x78, 0x05, 0x5B, 0xDD, 0x39, 0x6B, 0x64,
    0xF7, 0x8D, 0xA2, 0xC5, 0xF9, 0x62, 0x00, 0xCC, 0xD4, 0x15, 0xCD, 0x08,
    0xFE, 0x42, 0x00, 0x38,
};

// The authenticator data, excluding the attested credential data bytes. Append
// with attested credential data to get the complete authenticator data.
constexpr uint8_t kTestAuthenticatorDataPrefix[] = {
    // sha256 hash of rp id.
    0x11, 0x94, 0x22, 0x8D, 0xA8, 0xFD, 0xBD, 0xEE, 0xFD, 0x26, 0x1B, 0xD7,
    0xB6, 0x59, 0x5C, 0xFD, 0x70, 0xA5, 0x0D, 0x70, 0xC6, 0x40, 0x7B, 0xCF,
    0x01, 0x3D, 0xE9, 0x6D, 0x4E, 0xFB, 0x17, 0xDE,
    // flags (TUP and AT bits set)
    0x41,
    // counter
    0x00, 0x00, 0x00, 0x00
};

// Components of the CBOR needed to form an authenticator object.
// Combined diagnostic notation:
// {"fmt": "fido-u2f", "attStmt": {"sig": h'30...}, "authData": h'D4C9D9...'}
constexpr uint8_t kFormatFidoU2fCBOR[] = {
    // map(3)
    0xA3,
    // text(3)
    0x63,
    // "fmt"
    0x66, 0x6D, 0x74,
    // text(8)
    0x68,
    // "fido-u2f"
    0x66, 0x69, 0x64, 0x6F, 0x2D, 0x75, 0x32, 0x66
};

constexpr uint8_t kAttStmtCBOR[] = {
    // text(7)
    0x67,
    // "attStmt"
    0x61, 0x74, 0x74, 0x53, 0x74, 0x6D, 0x74
};

constexpr uint8_t kAuthDataCBOR[] = {
    // text(8)
    0x68,
    // "authData"
    0x61, 0x75, 0x74, 0x68, 0x44, 0x61, 0x74, 0x61,
    // bytes(196). i.e., the authenticator_data byte array corresponding to
    // kTestAuthenticatorDataPrefix|, |kTestAttestedCredentialDataPrefix|,
    // and test_data::kTestECPublicKeyCOSE.
    0x58, 0xC4
};

constexpr uint8_t kTestDeviceAaguid[] = {
    0xF8, 0xA0, 0x11, 0xF3, 0x8C, 0x0A, 0x4D, 0x15, 0x80, 0x06, 0x17, 0x11, 0x1F, 0x9E, 0xDC, 0x7D
};

Vector<uint8_t> getTestAttestedCredentialDataBytes()
{
    // Combine kTestAttestedCredentialDataPrefix and kTestECPublicKeyCOSE.
    auto testAttestedData = convertBytesToVector(kTestAttestedCredentialDataPrefix, sizeof(kTestAttestedCredentialDataPrefix));
    testAttestedData.append(TestData::kTestECPublicKeyCOSE, sizeof(TestData::kTestECPublicKeyCOSE));
    return testAttestedData;
}

Vector<uint8_t> getTestAuthenticatorDataBytes()
{
    // Build the test authenticator data.
    auto testAuthenticatorData = convertBytesToVector(kTestAuthenticatorDataPrefix, sizeof(kTestAuthenticatorDataPrefix));
    auto testAttestedData = getTestAttestedCredentialDataBytes();
    testAuthenticatorData.appendVector(testAttestedData);
    return testAuthenticatorData;
}

Vector<uint8_t> getTestAttestationObjectBytes()
{
    auto testAuthenticatorObject = convertBytesToVector(kFormatFidoU2fCBOR, sizeof(kFormatFidoU2fCBOR));
    testAuthenticatorObject.append(kAttStmtCBOR, sizeof(kAttStmtCBOR));
    testAuthenticatorObject.append(TestData::kU2fAttestationStatementCBOR, sizeof(TestData::kU2fAttestationStatementCBOR));
    testAuthenticatorObject.append(kAuthDataCBOR, sizeof(kAuthDataCBOR));
    auto testAuthenticatorData = getTestAuthenticatorDataBytes();
    testAuthenticatorObject.appendVector(testAuthenticatorData);
    return testAuthenticatorObject;
}

Vector<uint8_t> getTestSignResponse()
{
    return convertBytesToVector(TestData::kTestU2fSignResponse, sizeof(TestData::kTestU2fSignResponse));
}

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

// Return a malformed U2fRegisterResponse.
Vector<uint8_t> getTestU2fRegisterResponse(size_t prefixSize, const uint8_t appendix[], size_t appendixSize)
{
    Vector<uint8_t> result;
    result.reserveInitialCapacity(prefixSize + appendixSize);
    result.append(TestData::kTestU2fRegisterResponse, prefixSize);
    result.append(appendix, appendixSize);
    return result;
}

// Leveraging example 4 of section 6.1 of the spec
// https://fidoalliance.org/specs/fido-v2.0-ps-20170927/fido-client-to-authenticator-protocol-v2.0-ps-20170927.html#commands
TEST(CTAPResponseTest, TestReadMakeCredentialResponse)
{
    auto makeCredentialResponse = readCTAPMakeCredentialResponse(convertBytesToVector(TestData::kTestMakeCredentialResponse, sizeof(TestData::kTestMakeCredentialResponse)));
    ASSERT_TRUE(makeCredentialResponse);
    auto cborAttestationObject = cbor::CBORReader::read(convertBytesToVector(reinterpret_cast<uint8_t*>(makeCredentialResponse->attestationObject->data()), makeCredentialResponse->attestationObject->byteLength()));
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
    EXPECT_EQ(it->second.getByteString(), convertBytesToVector(TestData::kCtap2MakeCredentialAuthData, sizeof(TestData::kCtap2MakeCredentialAuthData)));

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
    EXPECT_EQ(attStmtIt->second.getByteString(), convertBytesToVector(TestData::kCtap2MakeCredentialSignature, sizeof(TestData::kCtap2MakeCredentialSignature)));

    attStmtIt = attestationStatementMap.find(cbor::CBORValue("x5c"));
    ASSERT_TRUE(attStmtIt != attestationStatementMap.end());
    const auto& certificate = attStmtIt->second;
    ASSERT_TRUE(certificate.isArray());
    ASSERT_EQ(certificate.getArray().size(), 1u);
    ASSERT_TRUE(certificate.getArray()[0].isByteString());
    EXPECT_EQ(certificate.getArray()[0].getByteString(), convertBytesToVector(TestData::kCtap2MakeCredentialCertificate, sizeof(TestData::kCtap2MakeCredentialCertificate)));
    EXPECT_EQ(makeCredentialResponse->rawId->byteLength(), sizeof(TestData::kCtap2MakeCredentialCredentialId));
    EXPECT_EQ(memcmp(makeCredentialResponse->rawId->data(), TestData::kCtap2MakeCredentialCredentialId, sizeof(TestData::kCtap2MakeCredentialCredentialId)), 0);
}

// Leveraging example 5 of section 6.1 of the CTAP spec.
// https://fidoalliance.org/specs/fido-v2.0-ps-20170927/fido-client-to-authenticator-protocol-v2.0-ps-20170927.html
TEST(CTAPResponseTest, TestReadGetAssertionResponse)
{
    auto getAssertionResponse = readCTAPGetAssertionResponse(convertBytesToVector(TestData::kDeviceGetAssertionResponse, sizeof(TestData::kDeviceGetAssertionResponse)));
    ASSERT_TRUE(getAssertionResponse);

    EXPECT_EQ(getAssertionResponse->authenticatorData->byteLength(), sizeof(TestData::kCtap2GetAssertionAuthData));
    EXPECT_EQ(memcmp(getAssertionResponse->authenticatorData->data(), TestData::kCtap2GetAssertionAuthData, sizeof(TestData::kCtap2GetAssertionAuthData)), 0);
    EXPECT_EQ(getAssertionResponse->signature->byteLength(), sizeof(TestData::kCtap2GetAssertionSignature));
    EXPECT_EQ(memcmp(getAssertionResponse->signature->data(), TestData::kCtap2GetAssertionSignature, sizeof(TestData::kCtap2GetAssertionSignature)), 0);
}

// Test that U2F register response is properly parsed.
TEST(CTAPResponseTest, TestParseRegisterResponseData)
{
    auto response = readU2fRegisterResponse(TestData::kRelyingPartyId, convertBytesToVector(TestData::kTestU2fRegisterResponse, sizeof(TestData::kTestU2fRegisterResponse)));
    ASSERT_TRUE(response);
    EXPECT_EQ(response->rawId->byteLength(), sizeof(TestData::kU2fSignKeyHandle));
    EXPECT_EQ(memcmp(response->rawId->data(), TestData::kU2fSignKeyHandle, sizeof(TestData::kU2fSignKeyHandle)), 0);
    EXPECT_TRUE(response->isAuthenticatorAttestationResponse);
    auto expectedAttestationObject = getTestAttestationObjectBytes();
    EXPECT_EQ(response->attestationObject->byteLength(), expectedAttestationObject.size());
    EXPECT_EQ(memcmp(response->attestationObject->data(), expectedAttestationObject.data(), expectedAttestationObject.size()), 0);
}

// Test malformed user public key.
TEST(CTAPResponseTest, TestParseIncorrectRegisterResponseData1)
{
    const uint8_t testData1[] = { 0x05 };
    auto response = readU2fRegisterResponse(TestData::kRelyingPartyId, convertBytesToVector(testData1, sizeof(testData1)));
    EXPECT_FALSE(response);

    const uint8_t testData2[] = { 0x05, 0x00 };
    response = readU2fRegisterResponse(TestData::kRelyingPartyId, convertBytesToVector(testData2, sizeof(testData2)));
    EXPECT_FALSE(response);

    const uint8_t testData3[] = { 0x05, 0x04, 0x00 };
    response = readU2fRegisterResponse(TestData::kRelyingPartyId, convertBytesToVector(testData3, sizeof(testData3)));
    EXPECT_FALSE(response);
}

// Test malformed key handle.
TEST(CTAPResponseTest, TestParseIncorrectRegisterResponseData2)
{
    auto response = readU2fRegisterResponse(TestData::kRelyingPartyId, getTestU2fRegisterResponse(kU2fKeyHandleLengthOffset, nullptr, 0));
    EXPECT_FALSE(response);

    const uint8_t testData[] = { 0x40 };
    response = readU2fRegisterResponse(TestData::kRelyingPartyId, getTestU2fRegisterResponse(kU2fKeyHandleLengthOffset, testData, sizeof(testData)));
    EXPECT_FALSE(response);
}

// Test malformed X.509.
TEST(CTAPResponseTest, TestParseIncorrectRegisterResponseData3)
{
    const auto prefix = kU2fKeyHandleOffset + 64;
    auto response = readU2fRegisterResponse(TestData::kRelyingPartyId, getTestU2fRegisterResponse(prefix, nullptr, 0));
    EXPECT_FALSE(response);

    const uint8_t testData1[] = { 0x40 };
    response = readU2fRegisterResponse(TestData::kRelyingPartyId, getTestU2fRegisterResponse(prefix, testData1, sizeof(testData1)));
    EXPECT_FALSE(response);

    const uint8_t testData2[] = { 0x30 };
    response = readU2fRegisterResponse(TestData::kRelyingPartyId, getTestU2fRegisterResponse(prefix, testData2, sizeof(testData2)));
    EXPECT_FALSE(response);

    const uint8_t testData3[] = { 0x30, 0x82 };
    response = readU2fRegisterResponse(TestData::kRelyingPartyId, getTestU2fRegisterResponse(prefix, testData3, sizeof(testData3)));
    EXPECT_FALSE(response);

    const uint8_t testData4[] = { 0x30, 0xC1 };
    response = readU2fRegisterResponse(TestData::kRelyingPartyId, getTestU2fRegisterResponse(prefix, testData4, sizeof(testData4)));
    EXPECT_FALSE(response);

    const uint8_t testData5[] = { 0x30, 0x82, 0x02, 0x4A };
    response = readU2fRegisterResponse(TestData::kRelyingPartyId, getTestU2fRegisterResponse(prefix, testData5, sizeof(testData5)));
    EXPECT_FALSE(response);
}

// Test malformed signature.
TEST(CTAPResponseTest, TestParseIncorrectRegisterResponseData4)
{
    const auto prefix = sizeof(TestData::kTestU2fRegisterResponse);
    auto response = readU2fRegisterResponse(TestData::kRelyingPartyId, getTestU2fRegisterResponse(prefix - 71, nullptr, 0));
    EXPECT_FALSE(response);

    const uint8_t testData[] = { 0x40, 0x40, 0x40 };
    response = readU2fRegisterResponse(TestData::kRelyingPartyId, getTestU2fRegisterResponse(prefix, testData, sizeof(testData)));
    EXPECT_FALSE(response);
}

// Test malformed X.509 but pass.
TEST(CTAPResponseTest, TestParseIncorrectRegisterResponseData5)
{
    const auto prefix = kU2fKeyHandleOffset + 64;
    const auto signatureSize = 71;
    const auto suffix = sizeof(TestData::kTestU2fRegisterResponse) - signatureSize;

    Vector<uint8_t> testData1;
    testData1.append(TestData::kTestU2fRegisterResponse, prefix);
    testData1.append(0x30);
    testData1.append(0x01);
    testData1.append(0x00);
    testData1.append(TestData::kTestU2fRegisterResponse + suffix, signatureSize);
    auto response = readU2fRegisterResponse(TestData::kRelyingPartyId, testData1);
    EXPECT_TRUE(response);

    Vector<uint8_t> testData2;
    testData2.append(TestData::kTestU2fRegisterResponse, prefix);
    testData2.append(0x30);
    testData2.append(0x81);
    testData2.append(0x01);
    testData2.append(0x00);
    testData2.append(TestData::kTestU2fRegisterResponse + suffix, signatureSize);
    response = readU2fRegisterResponse(TestData::kRelyingPartyId, testData2);
    EXPECT_TRUE(response);
}

// Tests that U2F authenticator data is properly serialized.
TEST(CTAPResponseTest, TestParseSignResponseData)
{
    auto response = readFromU2fSignResponse(TestData::kRelyingPartyId, getTestCredentialRawIdBytes(), getTestSignResponse());
    ASSERT_TRUE(response);
    EXPECT_EQ(response->rawId->byteLength(), sizeof(TestData::kU2fSignKeyHandle));
    EXPECT_EQ(memcmp(response->rawId->data(), TestData::kU2fSignKeyHandle, sizeof(TestData::kU2fSignKeyHandle)), 0);
    EXPECT_FALSE(response->isAuthenticatorAttestationResponse);
    EXPECT_EQ(response->authenticatorData->byteLength(), sizeof(TestData::kTestSignAuthenticatorData));
    EXPECT_EQ(memcmp(response->authenticatorData->data(), TestData::kTestSignAuthenticatorData, sizeof(TestData::kTestSignAuthenticatorData)), 0);
    EXPECT_EQ(response->signature->byteLength(), sizeof(TestData::kU2fSignature));
    EXPECT_EQ(memcmp(response->signature->data(), TestData::kU2fSignature, sizeof(TestData::kU2fSignature)), 0);
}

TEST(CTAPResponseTest, TestParseU2fSignWithNullKeyHandle)
{
    auto response = readFromU2fSignResponse(TestData::kRelyingPartyId, Vector<uint8_t>(), getTestSignResponse());
    EXPECT_FALSE(response);
}

TEST(CTAPResponseTest, TestParseU2fSignWithNullResponse)
{
    auto response = readFromU2fSignResponse(TestData::kRelyingPartyId, getTestCredentialRawIdBytes(), Vector<uint8_t>());
    EXPECT_FALSE(response);
}

TEST(CTAPResponseTest, TestParseU2fSignWithCorruptedCounter)
{
    // A sign response of less than 5 bytes.
    auto response = readFromU2fSignResponse(TestData::kRelyingPartyId, getTestCredentialRawIdBytes(), getTestCorruptedSignResponse(3));
    EXPECT_FALSE(response);
}

TEST(CTAPResponseTest, TestParseU2fSignWithCorruptedSignature)
{
    // A sign response no more than 5 bytes.
    auto response = readFromU2fSignResponse(TestData::kRelyingPartyId, getTestCredentialRawIdBytes(), getTestCorruptedSignResponse(5));
    EXPECT_FALSE(response);
}

TEST(CTAPResponseTest, TestReadGetInfoResponse)
{
    auto getInfoResponse = readCTAPGetInfoResponse(convertBytesToVector(TestData::kTestGetInfoResponsePlatformDevice, sizeof(TestData::kTestGetInfoResponsePlatformDevice)));
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
    EXPECT_FALSE(readCTAPGetInfoResponse(convertBytesToVector(kTestAuthenticatorGetInfoResponseWithNoVersion, sizeof(kTestAuthenticatorGetInfoResponseWithNoVersion))));
    EXPECT_FALSE(readCTAPGetInfoResponse(convertBytesToVector(kTestAuthenticatorGetInfoResponseWithDuplicateVersion, sizeof(kTestAuthenticatorGetInfoResponseWithDuplicateVersion))));
    EXPECT_FALSE(readCTAPGetInfoResponse(convertBytesToVector(kTestAuthenticatorGetInfoResponseWithIncorrectAaguid, sizeof(kTestAuthenticatorGetInfoResponseWithIncorrectAaguid))));
}

TEST(CTAPResponseTest, TestSerializeGetInfoResponse)
{
    AuthenticatorGetInfoResponse response({ ProtocolVersion::kCtap, ProtocolVersion::kU2f }, convertBytesToVector(kTestDeviceAaguid, sizeof(kTestDeviceAaguid)));
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
