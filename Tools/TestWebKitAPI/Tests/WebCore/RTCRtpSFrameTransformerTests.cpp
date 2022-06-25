/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_RTC)

#include <WebCore/RTCRtpSFrameTransformer.h>
#include <WebCore/SFrameUtils.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

Vector<uint8_t> getRawKey()
{
    return Vector<uint8_t>::from(
        249,
        195,
        202,
        159,
        56,
        191,
        103,
        247,
        245,
        213,
        20,
        68,
        67,
        20,
        151,
        185,
        242,
        78,
        176,
        158,
        91,
        205,
        138,
        55,
        176,
        147,
        198,
        243,
        146,
        204,
        243,
        220
    );
}

static Ref<WebCore::RTCRtpSFrameTransformer> createVideoTransformer(bool isEncrypting = true)
{
    auto transformer = WebCore::RTCRtpSFrameTransformer::create();
    transformer->setIsEncrypting(isEncrypting);
    transformer->setMediaType(WebCore::RTCRtpTransformBackend::MediaType::Video);

    auto keyId = Vector<uint8_t>::from(198, 31, 251, 197, 48, 139, 91, 51);
    uint64_t keyIdValue = 0;
    for (auto value : keyId)
        keyIdValue = value + (keyIdValue << 8);

    auto keyResult = transformer->setEncryptionKey(getRawKey(), keyIdValue);
    EXPECT_FALSE(keyResult.hasException());

    return transformer;
}

static Ref<WebCore::RTCRtpSFrameTransformer> createAudioTransformer(bool isEncrypting, WebCore::RTCRtpSFrameTransformer::CompatibilityMode mode)
{
    auto transformer = WebCore::RTCRtpSFrameTransformer::create(mode);
    transformer->setIsEncrypting(isEncrypting);
    transformer->setMediaType(WebCore::RTCRtpTransformBackend::MediaType::Audio);

    auto keyId = Vector<uint8_t>::from(31, 251, 197, 48, 139, 91, 51);
    uint64_t keyIdValue = 0;
    for (auto value : keyId)
        keyIdValue = value + (keyIdValue << 8);

    auto keyResult = transformer->setEncryptionKey(getRawKey(), keyIdValue);
    EXPECT_FALSE(keyResult.hasException());

    return transformer;
}

static void checkVectorsAreEqual(const Vector<uint8_t>& computed, const Vector<uint8_t>& expected)
{
    EXPECT_EQ(computed.size(), expected.size());
    for (size_t cptr = 0; cptr < std::min(computed.size(), expected.size()); ++cptr)
        EXPECT_EQ(computed[cptr], expected[cptr]);
}

TEST(RTCRtpSFrameTransformer, AuthenticationKey)
{
    auto transformer = createVideoTransformer();
    checkVectorsAreEqual(transformer->authenticationKey(), Vector<uint8_t>::from(
        4,
        98,
        231,
        89,
        19,
        177,
        253,
        241,
        246,
        85,
        193,
        64,
        19,
        72,
        211,
        48,
        57,
        235,
        63,
        57,
        165,
        252,
        130,
        58,
        142,
        14,
        142,
        223,
        134,
        92,
        167,
        253
    ));
}

TEST(RTCRtpSFrameTransformer, EncryptionKey)
{
    auto transformer = createVideoTransformer();
    checkVectorsAreEqual(transformer->encryptionKey(), Vector<uint8_t>::from(
        180,
        189,
        66,
        149,
        6,
        101,
        148,
        179,
        158,
        210,
        132,
        83,
        75,
        11,
        43,
        50
    ));
}

TEST(RTCRtpSFrameTransformer, SaltKey)
{
    auto transformer = createVideoTransformer();
    checkVectorsAreEqual(transformer->saltKey(), Vector<uint8_t>::from(
        184,
        43,
        180,
        28,
        93,
        139,
        8,
        109,
        65,
        142,
        81,
        22
    ));
}

TEST(RTCRtpSFrameTransformer, EncryptDecrypt)
{
    auto encryptor = createVideoTransformer();
    auto decryptor = createVideoTransformer(false);
    auto frame = Vector<uint8_t>::from(135, 89, 51, 166, 248, 129, 157, 111, 190, 134, 220);

    auto encryptedResult = encryptor->transform({ frame.data(), frame.size() });
    EXPECT_TRUE(encryptedResult.has_value());

    auto encrypted = WTFMove(encryptedResult.value());
    auto decryptedResult = decryptor->transform({ encrypted.data(), encrypted.size() });
    EXPECT_TRUE(decryptedResult.has_value());

    checkVectorsAreEqual(decryptedResult.value(), frame);
}

TEST(RTCRtpSFrameTransformer, EncryptDecryptKeyID0)
{
    auto encryptor = createVideoTransformer();
    encryptor->setEncryptionKey(getRawKey(), 0);

    auto decryptor = createVideoTransformer(false);
    decryptor->setEncryptionKey(getRawKey(), 0);

    auto frame = Vector<uint8_t>::from(135, 89, 51, 166, 248, 129, 157, 111, 190, 134, 220);

    auto encryptedResult = encryptor->transform({ frame.data(), frame.size() });
    EXPECT_TRUE(encryptedResult.has_value());

    auto encrypted = WTFMove(encryptedResult.value());
    auto decryptedResult = decryptor->transform({ encrypted.data(), encrypted.size() });
    EXPECT_TRUE(decryptedResult.has_value());

    checkVectorsAreEqual(decryptedResult.value(), frame);
}

TEST(RTCRtpSFrameTransformer, EncryptDecryptAudio)
{
    // We ignore compatiblity mode for audio.
    uint64_t keyId = 1255995222;
    auto encryptor = createAudioTransformer(true, WebCore::RTCRtpSFrameTransformer::CompatibilityMode::H264);
    encryptor->setEncryptionKey(getRawKey(), keyId);

    auto decryptor = createAudioTransformer(false, WebCore::RTCRtpSFrameTransformer::CompatibilityMode::None);
    decryptor->setEncryptionKey(getRawKey(), keyId);

    auto frame = Vector<uint8_t>::from(135, 89, 51, 166, 248, 129, 157, 111, 190, 134, 220, 56);

    auto encryptedResult = encryptor->transform({ frame.data(), frame.size() });
    EXPECT_TRUE(encryptedResult.has_value());

    auto encrypted = WTFMove(encryptedResult.value());
    auto decryptedResult = decryptor->transform({ encrypted.data(), encrypted.size() });
    EXPECT_TRUE(decryptedResult.has_value());

    checkVectorsAreEqual(decryptedResult.value(), frame);
}

TEST(RTCRtpSFrameTransformer, TransformCounter0)
{
    auto transformer = createVideoTransformer();

    uint8_t frame1[] = { 135, 89, 51, 166, 248, 129, 157, 111, 190, 134, 220 };
    auto result = transformer->transform({ frame1, sizeof(frame1) });
    EXPECT_TRUE(result.has_value());

    checkVectorsAreEqual(result.value(), Vector<uint8_t>::from(
        15,
        198,
        31,
        251,
        197,
        48,
        139,
        91,
        51,
        0,
        176,
        194,
        94,
        42,
        248,
        190,
        103,
        74,
        123,
        208,
        120,
        249,
        75,
        63,
        244,
        81,
        244,
        71,
        64,
        46,
        28
    ));
}

TEST(RTCRtpSFrameTransformer, TransformCounter256)
{
    auto transformer = createVideoTransformer();

    uint8_t frame1[] = { 8, 164, 189, 18, 61, 117, 132, 43, 117, 169, 42 };
    auto result = transformer->transform({ frame1, sizeof(frame1) });
    EXPECT_TRUE(result.has_value());
    for (size_t cptr = 0; cptr < 256; ++cptr) {
        result = transformer->transform({ frame1, sizeof(frame1) });
        EXPECT_TRUE(result.has_value());
    }

    checkVectorsAreEqual(result.value(), Vector<uint8_t>::from(
        31,
        198,
        31,
        251,
        197,
        48,
        139,
        91,
        51,
        1,
        0,
        37,
        39,
        205,
        106,
        44,
        155,
        25,
        174,
        2,
        206,
        198,
        89,
        82,
        57,
        224,
        232,
        26,
        29,
        98,
        176,
        57
    ));
}

TEST(RTCRtpSFrameTransformer, TransformCounter65536)
{
    auto transformer = createVideoTransformer();
    transformer->setCounter(65536);

    uint8_t frame1[] = { 0, 33, 244, 24, 236, 156, 127, 8, 48, 88, 220 };
    auto result = transformer->transform({ frame1, sizeof(frame1) });
    EXPECT_TRUE(result.has_value());

    checkVectorsAreEqual(result.value(), Vector<uint8_t>::from(
        47,
        198,
        31,
        251,
        197,
        48,
        139,
        91,
        51,
        1,
        0,
        0,
        117,
        5,
        112,
        126,
        205,
        197,
        116,
        213,
        249,
        178,
        47,
        154,
        151,
        123,
        40,
        58,
        121,
        201,
        159,
        90,
        170
    ));
}


TEST(RTCRtpSFrameTransformer, RBSPEscaping)
{
    uint8_t frame0[] = { 0, 33, 00, 24, 236, 156, 127, 8, 0, 0, 4 };
    EXPECT_FALSE(WebCore::needsRbspUnescaping(frame0, sizeof(frame0)));

    uint8_t frame0b[] = { 0, 33, 00, 24, 236, 156, 127, 8, 0, 0, 3 };
    EXPECT_FALSE(WebCore::needsRbspUnescaping(frame0b, sizeof(frame0b)));

    uint8_t frame1[] = { 0, 33, 00, 24, 236, 156, 127, 8, 0, 0, 3, 1 };
    uint8_t frame1Unescaped[] = { 0, 33, 00, 24, 236, 156, 127, 8, 0, 0, 1 };
    EXPECT_TRUE(WebCore::needsRbspUnescaping(frame1, sizeof(frame1)));

    auto result = WebCore::fromRbsp(frame1, sizeof(frame1));

    EXPECT_EQ(result.size(), sizeof(frame1Unescaped));
    for (size_t i = 0; i < sizeof(frame1Unescaped); ++i)
        EXPECT_EQ(result[i], frame1Unescaped[i]);

    Vector<uint8_t> frame2 { 0, 0, 0, 65, 0, 0, 1, 66, 0, 0, 2, 67, 0, 0, 3, 68, 0, 0, 4, 0, 0, 1 };
    Vector<uint8_t> escaped { 0, 0, 0, 65, 0, 0, 1, 66, 0, 0, 2, 67, 0, 0, 3, 68, 0, 0, 4, 0, 0, 1 };

    WebCore::toRbsp(escaped, 0);
    EXPECT_TRUE(WebCore::needsRbspUnescaping(escaped.data(), escaped.size()));
    auto unescaped = WebCore::fromRbsp(escaped.data(), escaped.size());

    EXPECT_EQ(unescaped.size(), frame2.size());
    for (size_t i = 0; i < frame2.size(); ++i)
        EXPECT_EQ(unescaped[i], frame2[i]);
}

}

#endif // ENABLE(WEB_RTC)
