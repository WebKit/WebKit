/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "FidoTestData.h"
#include "PlatformUtilities.h"
#include <WebCore/CBORReader.h>
#include <WebCore/CBORValue.h>
#include <WebCore/CryptoAlgorithmAES_CBC.h>
#include <WebCore/CryptoAlgorithmAesCbcCfbParams.h>
#include <WebCore/CryptoAlgorithmECDH.h>
#include <WebCore/CryptoKeyAES.h>
#include <WebCore/CryptoKeyEC.h>
#include <WebCore/CryptoKeyHMAC.h>
#include <WebCore/FidoConstants.h>
#include <WebCore/Pin.h>
#include <WebCore/WebAuthenticationConstants.h>
#include <WebCore/WebAuthenticationUtils.h>
#include <wtf/text/Base64.h>

namespace TestWebKitAPI {
using namespace WebCore;
using namespace cbor;
using namespace fido;
using namespace fido::pin;

TEST(CtapPinTest, TestValidateAndConvertToUTF8)
{
    // Failure cases
    auto result = validateAndConvertToUTF8("123");
    EXPECT_FALSE(result);
    result = validateAndConvertToUTF8("");
    EXPECT_FALSE(result);
    result = validateAndConvertToUTF8("1234567812345678123456781234567812345678123456781234567812345678");
    EXPECT_FALSE(result);

    // Success cases
    result = validateAndConvertToUTF8("1234");
    EXPECT_TRUE(result);
    EXPECT_EQ(result->length(), 4u);
    EXPECT_STREQ(result->data(), "1234");

    result = validateAndConvertToUTF8("123456781234567812345678123456781234567812345678123456781234567");
    EXPECT_TRUE(result);
    EXPECT_EQ(result->length(), 63u);
    EXPECT_STREQ(result->data(), "123456781234567812345678123456781234567812345678123456781234567");
}

TEST(CtapPinTest, TestRetriesRequest)
{
    auto result = encodeAsCBOR(RetriesRequest { });
    EXPECT_EQ(result.size(), sizeof(TestData::kCtapClientPinRetries));
    EXPECT_EQ(memcmp(result.data(), TestData::kCtapClientPinRetries, result.size()), 0);
}

TEST(CtapPinTest, TestRetriesResponse)
{
    // Failure cases
    auto result = RetriesResponse::parse({ });
    EXPECT_FALSE(result);

    const uint8_t testData1[] = { 0x05 }; // wrong response code
    result = RetriesResponse::parse(convertBytesToVector(testData1, sizeof(testData1)));
    EXPECT_FALSE(result);

    const uint8_t testData2[] = { 0x00, 0x00 }; // wrong CBOR map
    result = RetriesResponse::parse(convertBytesToVector(testData2, sizeof(testData2)));
    EXPECT_FALSE(result);

    result = RetriesResponse::parse(convertBytesToVector(TestData::kCtapClientPinTokenResponse, sizeof(TestData::kCtapClientPinTokenResponse))); // wrong response
    EXPECT_FALSE(result);

    // Success cases
    result = RetriesResponse::parse(convertBytesToVector(TestData::kCtapClientPinRetriesResponse, sizeof(TestData::kCtapClientPinRetriesResponse)));
    EXPECT_TRUE(result);
    EXPECT_EQ(result->retries, 8);
}

TEST(CtapPinTest, TestKeyAgreementRequest)
{
    auto result = encodeAsCBOR(KeyAgreementRequest { });
    EXPECT_EQ(result.size(), sizeof(TestData::kCtapClientPinKeyAgreement));
    EXPECT_EQ(memcmp(result.data(), TestData::kCtapClientPinKeyAgreement, result.size()), 0);
}

TEST(CtapPinTest, TestKeyAgreementResponse)
{
    // Failure cases
    auto result = KeyAgreementResponse::parse({ });
    EXPECT_FALSE(result);

    const uint8_t testData1[] = { 0x05 }; // wrong response code
    result = KeyAgreementResponse::parse(convertBytesToVector(testData1, sizeof(testData1)));
    EXPECT_FALSE(result);

    const uint8_t testData2[] = { 0x00, 0x00 }; // wrong CBOR map
    result = KeyAgreementResponse::parse(convertBytesToVector(testData2, sizeof(testData2)));
    EXPECT_FALSE(result);

    result = KeyAgreementResponse::parse(convertBytesToVector(TestData::kCtapClientPinTokenResponse, sizeof(TestData::kCtapClientPinTokenResponse))); // wrong response
    EXPECT_FALSE(result);

    // Test COSE
    auto coseKey = encodeCOSEPublicKey(Vector<uint8_t>(65));
    coseKey[CBORValue(COSE::kty)] = CBORValue(0); // wrong kty
    auto coseResult = KeyAgreementResponse::parseFromCOSE(coseKey);
    EXPECT_FALSE(coseResult);

    coseKey = encodeCOSEPublicKey(Vector<uint8_t>(65));
    coseKey[CBORValue(COSE::alg)] = CBORValue(0); // wrong alg
    coseResult = KeyAgreementResponse::parseFromCOSE(coseKey);
    EXPECT_FALSE(coseResult);

    coseKey = encodeCOSEPublicKey(Vector<uint8_t>(65));
    coseKey[CBORValue(COSE::crv)] = CBORValue(0); // wrong crv
    coseResult = KeyAgreementResponse::parseFromCOSE(coseKey);
    EXPECT_FALSE(coseResult);

    coseKey = encodeCOSEPublicKey(Vector<uint8_t>(65));
    coseKey[CBORValue(COSE::x)] = CBORValue(0); // wrong x
    coseResult = KeyAgreementResponse::parseFromCOSE(coseKey);
    EXPECT_FALSE(coseResult);

    coseKey = encodeCOSEPublicKey(Vector<uint8_t>(65));
    coseKey[CBORValue(COSE::y)] = CBORValue(0); // wrong y
    coseResult = KeyAgreementResponse::parseFromCOSE(coseKey);
    EXPECT_FALSE(coseResult);

    // Success cases
    result = KeyAgreementResponse::parse(convertBytesToVector(TestData::kCtapClientPinKeyAgreementResponse, sizeof(TestData::kCtapClientPinKeyAgreementResponse)));
    EXPECT_TRUE(result);
    auto exportedRawKey = result->peerKey->exportRaw();
    EXPECT_FALSE(exportedRawKey.hasException());
    Vector<uint8_t> expectedRawKey;
    expectedRawKey.reserveCapacity(65);
    expectedRawKey.append(0x04);
    expectedRawKey.append(TestData::kCtapClientPinKeyAgreementResponse + 14, 32); // X
    expectedRawKey.append(TestData::kCtapClientPinKeyAgreementResponse + 49, 32); // Y
    EXPECT_TRUE(exportedRawKey.returnValue() == expectedRawKey);
}

TEST(CtapPinTest, TestTokenRequest)
{
    // Generate an EC key pair as the peer key.
    auto keyPairResult = CryptoKeyEC::generatePair(CryptoAlgorithmIdentifier::ECDH, "P-256", true, CryptoKeyUsageDeriveBits);
    ASSERT_FALSE(keyPairResult.hasException());
    auto keyPair = keyPairResult.releaseReturnValue();

    CString pin = "1234";

    auto token = TokenRequest::tryCreate(pin, downcast<CryptoKeyEC>(*keyPair.publicKey));
    EXPECT_TRUE(token);
    auto result = encodeAsCBOR(*token);
    EXPECT_EQ(result.size(), 120u);
    EXPECT_EQ(result[0], static_cast<uint8_t>(CtapRequestCommand::kAuthenticatorClientPin));

    // Decode the CBOR binary to check if each field is encoded correctly.
    Vector<uint8_t> buffer;
    buffer.append(result.data() + 1, result.size() - 1);
    auto decodedResponse = cbor::CBORReader::read(buffer);
    EXPECT_TRUE(decodedResponse);
    EXPECT_TRUE(decodedResponse->isMap());
    const auto& responseMap = decodedResponse->getMap();

    const auto& it1 = responseMap.find(CBORValue(static_cast<uint8_t>(RequestKey::kProtocol)));
    EXPECT_NE(it1, responseMap.end());
    EXPECT_EQ(it1->second.getInteger(), kProtocolVersion);

    const auto& it2 = responseMap.find(CBORValue(static_cast<uint8_t>(RequestKey::kSubcommand)));
    EXPECT_NE(it2, responseMap.end());
    EXPECT_EQ(it2->second.getInteger(), static_cast<uint8_t>(Subcommand::kGetPinToken));

    // COSE
    auto it = responseMap.find(CBORValue(static_cast<int>(RequestKey::kKeyAgreement)));
    EXPECT_TRUE(decodedResponse);
    EXPECT_TRUE(decodedResponse->isMap());
    const auto& coseKey = it->second.getMap();

    const auto& it3 = coseKey.find(CBORValue(COSE::kty));
    EXPECT_NE(it3, coseKey.end());
    EXPECT_EQ(it3->second.getInteger(), COSE::EC2);

    const auto& it4 = coseKey.find(CBORValue(COSE::alg));
    EXPECT_NE(it4, coseKey.end());
    EXPECT_EQ(it4->second.getInteger(), COSE::ECDH256);

    const auto& it5 = coseKey.find(CBORValue(COSE::crv));
    EXPECT_NE(it5, coseKey.end());
    EXPECT_EQ(it5->second.getInteger(), COSE::P_256);

    // Check the cose key.
    const auto& xIt = coseKey.find(CBORValue(COSE::x));
    EXPECT_NE(xIt, coseKey.end());
    const auto& yIt = coseKey.find(CBORValue(COSE::y));
    EXPECT_NE(yIt, coseKey.end());
    auto cosePublicKey = CryptoKeyEC::importRaw(CryptoAlgorithmIdentifier::ECDH, "P-256", encodeRawPublicKey(xIt->second.getByteString(), yIt->second.getByteString()), true, CryptoKeyUsageDeriveBits);
    EXPECT_TRUE(cosePublicKey);

    // Check the encrypted Pin.
    auto sharedKeyResult = CryptoAlgorithmECDH::platformDeriveBits(downcast<CryptoKeyEC>(*keyPair.privateKey), *cosePublicKey);
    EXPECT_TRUE(sharedKeyResult);
    auto aesKey = CryptoKeyAES::importRaw(CryptoAlgorithmIdentifier::AES_CBC, WTFMove(*sharedKeyResult), true, CryptoKeyUsageDecrypt);
    EXPECT_TRUE(aesKey);

    const auto& it6 = responseMap.find(CBORValue(static_cast<uint8_t>(RequestKey::kPinHashEnc)));
    EXPECT_NE(it6, responseMap.end());
    auto pinHashResult = CryptoAlgorithmAES_CBC::platformDecrypt({ }, *aesKey, it6->second.getByteString());
    EXPECT_FALSE(pinHashResult.hasException());
    auto pinHash = pinHashResult.releaseReturnValue();
    const uint8_t expectedPinHash[] = { 0x03, 0xac, 0x67, 0x42, 0x16, 0xf3, 0xe1, 0x5c, 0x76, 0x1e, 0xe1, 0xa5, 0xe2, 0x55, 0xf0, 0x67 };
    EXPECT_EQ(pinHash.size(), 16u);
    EXPECT_EQ(memcmp(pinHash.data(), expectedPinHash, pinHash.size()), 0);
}

TEST(CtapPinTest, TestTokenResponse)
{
    const uint8_t sharedKeyData[] = {
        0x03, 0xac, 0x67, 0x42, 0x16, 0xf3, 0xe1, 0x5c,
        0x76, 0x1e, 0xe1, 0xa5, 0xe2, 0x55, 0xf0, 0x67,
        0x95, 0x36, 0x23, 0xc8, 0xb3, 0x88, 0xb4, 0x45,
        0x9e, 0x13, 0xf9, 0x78, 0xd7, 0xc8, 0x46, 0xf4, };
    auto sharedKey = CryptoKeyAES::importRaw(CryptoAlgorithmIdentifier::AES_CBC, convertBytesToVector(sharedKeyData, sizeof(sharedKeyData)), true, CryptoKeyUsageEncrypt | CryptoKeyUsageDecrypt);
    ASSERT_TRUE(sharedKey);

    // Failure cases
    auto result = TokenResponse::parse(*sharedKey, { });
    EXPECT_FALSE(result);

    const uint8_t testData1[] = { 0x05 }; // wrong response code
    result = TokenResponse::parse(*sharedKey, convertBytesToVector(testData1, sizeof(testData1)));
    EXPECT_FALSE(result);

    const uint8_t testData2[] = { 0x00, 0x00 }; // wrong CBOR map
    result = TokenResponse::parse(*sharedKey, convertBytesToVector(testData2, sizeof(testData2)));
    EXPECT_FALSE(result);

    result = TokenResponse::parse(*sharedKey, convertBytesToVector(TestData::kCtapClientPinKeyAgreementResponse, sizeof(TestData::kCtapClientPinKeyAgreementResponse))); // wrong response
    EXPECT_FALSE(result);

    // Success cases
    result = TokenResponse::parse(*sharedKey, convertBytesToVector(TestData::kCtapClientPinTokenResponse, sizeof(TestData::kCtapClientPinTokenResponse)));
    EXPECT_TRUE(result);
    const uint8_t expectedToken[] = { 0x03, 0xac, 0x67, 0x42, 0x16, 0xf3, 0xe1, 0x5c, 0x76, 0x1e, 0xe1, 0xa5, 0xe2, 0x55, 0xf0, 0x67 };
    EXPECT_EQ(result->token().size(), 16u);
    EXPECT_EQ(memcmp(result->token().data(), expectedToken, result->token().size()), 0);
}

TEST(CtapPinTest, TestPinAuth)
{
    // 1. Generate the token.
    const uint8_t sharedKeyData[] = {
        0x03, 0xac, 0x67, 0x42, 0x16, 0xf3, 0xe1, 0x5c,
        0x76, 0x1e, 0xe1, 0xa5, 0xe2, 0x55, 0xf0, 0x67,
        0x95, 0x36, 0x23, 0xc8, 0xb3, 0x88, 0xb4, 0x45,
        0x9e, 0x13, 0xf9, 0x78, 0xd7, 0xc8, 0x46, 0xf4, };
    auto sharedKey = CryptoKeyAES::importRaw(CryptoAlgorithmIdentifier::AES_CBC, convertBytesToVector(sharedKeyData, sizeof(sharedKeyData)), true, CryptoKeyUsageEncrypt | CryptoKeyUsageDecrypt);
    ASSERT_TRUE(sharedKey);
    auto result = TokenResponse::parse(*sharedKey, convertBytesToVector(TestData::kCtapClientPinTokenResponse, sizeof(TestData::kCtapClientPinTokenResponse)));
    ASSERT_TRUE(result);

    // 2. Generate the pinAuth.
    auto pinAuth = result->pinAuth(convertBytesToVector(sharedKeyData, sizeof(sharedKeyData))); // sharedKeyData pretends to be clientDataHash
    const uint8_t expectedPinAuth[] = { 0xb3, 0xc2, 0x65, 0x1c, 0xfd, 0xc8, 0x42, 0xb4, 0x60, 0x16, 0xed, 0x20, 0x64, 0x53, 0xaf, 0x84 };
    EXPECT_EQ(pinAuth.size(), 16u);
    EXPECT_EQ(memcmp(pinAuth.data(), expectedPinAuth, pinAuth.size()), 0);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_AUTHN)
