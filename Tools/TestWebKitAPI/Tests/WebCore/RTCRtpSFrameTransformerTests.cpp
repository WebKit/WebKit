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
    transformer->setAuthenticationSize(10);

    auto keyId = Vector<uint8_t>::from(198, 31, 251, 197, 48, 139, 91, 51);
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
        92,
        228,
        139,
        195,
        98,
        16,
        4,
        193,
        221,
        248,
        232,
        178,
        135,
        22,
        141,
        150,
        165,
        242,
        73,
        72,
        10,
        197,
        15,
        87,
        181,
        34,
        120,
        71,
        42,
        174,
        131,
        72
    ));
}

TEST(RTCRtpSFrameTransformer, EncryptionKey)
{
    auto transformer = createVideoTransformer();
    checkVectorsAreEqual(transformer->encryptionKey(), Vector<uint8_t>::from(
        207,
        251,
        52,
        21,
        101,
        50,
        232,
        48,
        211,
        115,
        11,
        155,
        101,
        116,
        153,
        71
    ));
}

TEST(RTCRtpSFrameTransformer, SaltKey)
{
    auto transformer = createVideoTransformer();
    checkVectorsAreEqual(transformer->saltKey(), Vector<uint8_t>::from(
        229,
        19,
        42,
        93,
        164,
        103,
        120,
        53,
        25,
        26,
        237,
        204,
        79,
        138,
        138,
        231
    ));
}

TEST(RTCRtpSFrameTransformer, EncryptDecrypt)
{
    auto encryptor = createVideoTransformer();
    auto decryptor = createVideoTransformer(false);
    auto frame = Vector<uint8_t>::from(135, 89, 51, 166, 248, 129, 157, 111, 190, 134, 220);

    auto encryptedResult = encryptor->transform(frame.data(), frame.size());
    EXPECT_FALSE(encryptedResult.hasException());

    auto encrypted = encryptedResult.releaseReturnValue();
    auto decryptedResult = decryptor->transform(encrypted.data(), encrypted.size());
    EXPECT_FALSE(decryptedResult.hasException());

    checkVectorsAreEqual(decryptedResult.returnValue(), frame);
}

TEST(RTCRtpSFrameTransformer, EncryptDecryptKeyID0)
{
    auto encryptor = createVideoTransformer();
    encryptor->setEncryptionKey(getRawKey(), 0);

    auto decryptor = createVideoTransformer(false);
    decryptor->setEncryptionKey(getRawKey(), 0);

    auto frame = Vector<uint8_t>::from(135, 89, 51, 166, 248, 129, 157, 111, 190, 134, 220);

    auto encryptedResult = encryptor->transform(frame.data(), frame.size());
    EXPECT_FALSE(encryptedResult.hasException());

    auto encrypted = encryptedResult.releaseReturnValue();
    auto decryptedResult = decryptor->transform(encrypted.data(), encrypted.size());
    EXPECT_FALSE(decryptedResult.hasException());

    checkVectorsAreEqual(decryptedResult.returnValue(), frame);
}

TEST(RTCRtpSFrameTransformer, TransformCounter0)
{
    auto transformer = createVideoTransformer();

    uint8_t frame1[] = { 135, 89, 51, 166, 248, 129, 157, 111, 190, 134, 220 };
    auto result = transformer->transform(frame1, sizeof(frame1));
    EXPECT_FALSE(result.hasException());

    checkVectorsAreEqual(result.releaseReturnValue(), Vector<uint8_t>::from(
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
        254,
        171,
        168,
        127,
        158,
        97,
        70,
        4,
        233,
        156,
        134,
        220,
        44,
        50,
        90,
        3,
        68,
        200,
        127,
        223,
        6
    ));
}

TEST(RTCRtpSFrameTransformer, TransformCounter256)
{
    auto transformer = createVideoTransformer();

    uint8_t frame1[] = { 8, 164, 189, 18, 61, 117, 132, 43, 117, 169, 42 };
    WebCore::ExceptionOr<Vector<uint8_t>> result = Vector<uint8_t>();
    result = transformer->transform(frame1, sizeof(frame1));
    for (size_t cptr = 0; cptr < 256; ++cptr) {
        result = transformer->transform(frame1, sizeof(frame1));
        EXPECT_FALSE(result.hasException());
    }
    checkVectorsAreEqual(result.releaseReturnValue(), Vector<uint8_t>::from(
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
        124,
        218,
        61,
        23,
        78,
        15,
        190,
        35,
        255,
        253,
        120,
        64,
        28,
        183,
        172,
        154,
        144,
        16,
        229,
        87,
        52
    ));
}

TEST(RTCRtpSFrameTransformer, TransformCounter65536)
{
    auto transformer = createVideoTransformer();
    transformer->setCounter(65536);

    uint8_t frame1[] = { 0, 33, 244, 24, 236, 156, 127, 8, 48, 88, 220 };
    auto result = transformer->transform(frame1, sizeof(frame1));
    EXPECT_FALSE(result.hasException());

    checkVectorsAreEqual(result.releaseReturnValue(), Vector<uint8_t>::from(
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
        98,
        212,
        137,
        244,
        209,
        16,
        89,
        234,
        21,
        171,
        15,
        107,
        50,
        145,
        73,
        107,
        165,
        45,
        190,
        25,
        82
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
