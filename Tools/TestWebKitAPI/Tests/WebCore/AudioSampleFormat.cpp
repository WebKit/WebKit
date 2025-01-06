/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "Test.h"

#include <WebCore/AudioSampleFormat.h>
#include <cstdint>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

using namespace WebCore;
using namespace WTF;

#define AUDIO_SAMPLE_WITH_MORE_TESTS 1

TEST(AudioSampleFormat, U8)
{
    auto testVectorUint8 = Vector<uint8_t>::from(0, INT8_MAX / 2, 128, INT8_MAX / 2 + 128, UINT8_MAX);
    Vector<float> testVectorFloatFromUint8(testVectorUint8.size(), [&](auto index) {
        return convertAudioSample<float>(testVectorUint8[index]);
    });
    Vector<uint8_t> testVectorUint8FromFloat(testVectorFloatFromUint8.size(), [&](auto index) {
        return convertAudioSample<uint8_t>(testVectorFloatFromUint8[index]);
    });
    for (size_t i = 0; i < testVectorUint8.size(); i++)
        EXPECT_EQ(testVectorUint8[i], testVectorUint8FromFloat[i]);

    // Test that the min and max of the [-1, 1] range can be reached and that BIAS value gets converted to 0.
    EXPECT_EQ(-1.0f, convertAudioSample<float>(static_cast<uint8_t>(0)));
    EXPECT_EQ(0.0f, convertAudioSample<float>(static_cast<uint8_t>(128)));
    EXPECT_EQ(1.0f, convertAudioSample<float>(static_cast<uint8_t>(255)));

    // Test that conversion to float and back is lossless.
    for (uint8_t i = 0; i < 255; i++)
        EXPECT_EQ(i, convertAudioSample<uint8_t>(convertAudioSample<float>(i)));
}

TEST(AudioSampleFormat, S16)
{
    auto testVectorInt16 = Vector<int16_t>::from(INT16_MIN, 0, INT16_MIN / 2, INT16_MAX / 2, INT16_MAX);
    Vector<float> testVectorFloatFromInt16(testVectorInt16.size(), [&](auto index) {
        return convertAudioSample<float>(testVectorInt16[index]);
    });
    Vector<int16_t> testVectorInt16FromFloat(testVectorFloatFromInt16.size(), [&](auto index) {
        return convertAudioSample<int16_t>(testVectorFloatFromInt16[index]);
    });
    for (size_t i = 0; i < testVectorInt16.size(); i++)
        EXPECT_EQ(testVectorInt16[i], testVectorInt16FromFloat[i]);

    // Test that the min and max of the [-1, 1] range can be reached and that silence stays silent.
    EXPECT_EQ(-1.0f, convertAudioSample<float>(static_cast<int16_t>(INT16_MIN)));
    EXPECT_EQ(0.0f, convertAudioSample<float>(static_cast<int16_t>(0)));
    EXPECT_EQ(1.0f, convertAudioSample<float>(static_cast<int16_t>(INT16_MAX)));
}

TEST(AudioSampleFormat, S16toU8)
{
    EXPECT_EQ(0, convertAudioSample<uint8_t>(static_cast<int16_t>(INT16_MIN)));
    EXPECT_EQ(128, convertAudioSample<uint8_t>(static_cast<int16_t>(0)));
    EXPECT_EQ(255, convertAudioSample<uint8_t>(static_cast<int16_t>(INT16_MAX)));
}

TEST(AudioSampleFormat, S16toS32)
{
    // Conversion from S16 to S32 and back is lossless.
    auto testVectorInt16 = Vector<int16_t>::from(INT16_MIN, 0, INT16_MIN / 2, INT16_MAX / 2, INT16_MAX);
    Vector<int32_t> testVectorInt32FromInt16(testVectorInt16.size(), [&](auto index) {
        return convertAudioSample<int32_t>(testVectorInt16[index]);
    });
    Vector<int16_t> testVectorInt16FromInt32(testVectorInt32FromInt16.size(), [&](auto index) {
        return convertAudioSample<int16_t>(testVectorInt32FromInt16[index]);
    });
    for (size_t i = 0; i < testVectorInt16.size(); i++)
        EXPECT_EQ(testVectorInt16[i], testVectorInt16FromInt32[i]);

    EXPECT_GE(INT32_MAX / 2, convertAudioSample<int32_t>(static_cast<int16_t>(INT16_MAX / 2)));
    EXPECT_LE(INT32_MIN / 2, convertAudioSample<int32_t>(static_cast<int16_t>(INT16_MIN / 2)));

#if AUDIO_SAMPLE_WITH_MORE_TESTS
    // May be too slow to enable all the time.
    for (int32_t i = std::numeric_limits<int16_t>::lowest(); i <= std::numeric_limits<int16_t>::lowest(); i++)
        EXPECT_EQ(i, convertAudioSample<int16_t>(convertAudioSample<int32_t, int16_t>(i)));
#endif
}

TEST(AudioSampleFormat, S32)
{
    auto testVectorInt32 = Vector<int32_t>::from(INT32_MIN, 0, INT32_MIN / 2, INT32_MAX / 2, INT32_MAX);
    Vector<float> testVectorFloatFromInt32(testVectorInt32.size(), [&](auto index) {
        return convertAudioSample<float>(testVectorInt32[index]);
    });
    Vector<int32_t> testVectorInt32FromFloat(testVectorFloatFromInt32.size(), [&](auto index) {
        return convertAudioSample<int32_t>(testVectorFloatFromInt32[index]);
    });
    // S32 provides greater accuracy than a 32 bits float (it only has a 24 bits mantissa). So the conversion back and forth may not be lossless.
    // At the most it will be off by one.
    for (size_t i = 0; i < testVectorInt32.size(); i++)
        EXPECT_LE(std::abs(testVectorInt32[i] - testVectorInt32FromFloat[i]), 1);

    // Test that the min and max of the [-1, 1] range can be reached.
    EXPECT_EQ(-1.0f, convertAudioSample<float>(static_cast<int32_t>(INT32_MIN)));
    EXPECT_EQ(0.0f, convertAudioSample<float>(static_cast<int32_t>(0)));
    EXPECT_EQ(1.0f, convertAudioSample<float>(static_cast<int32_t>(INT32_MAX)));
}

TEST(AudioSampleFormat, S32toS16)
{
    EXPECT_EQ(INT16_MIN, convertAudioSample<int16_t>(static_cast<int32_t>(INT32_MIN)));
    EXPECT_EQ(INT16_MAX, convertAudioSample<int16_t>(static_cast<int32_t>(INT32_MAX)));
    EXPECT_GE(INT16_MAX / 2, convertAudioSample<int16_t>(static_cast<int32_t>(INT32_MAX / 2)));
    EXPECT_LE(INT16_MIN / 2, convertAudioSample<int16_t>(static_cast<int32_t>(INT32_MIN / 2)));
}

TEST(AudioSampleFormat, F32)
{
    auto testVectorFloat = Vector<float>::from(-1.0, -.75, -0.5, -0.3, -0.1, 0.0, 0.1, 0.3, 0.5, 0.75, 1.0);
    Vector<int32_t> testVectorInt32FromFloat(testVectorFloat.size(), [&](auto index) {
        return convertAudioSample<int32_t>(testVectorFloat[index]);
    });
    Vector<float> testVectorFloatFromInt32(testVectorInt32FromFloat.size(), [&](auto index) {
        return convertAudioSample<float>(testVectorInt32FromFloat[index]);
    });
    for (size_t i = 0; i < testVectorFloat.size(); i++)
        EXPECT_EQ(testVectorFloat[i], testVectorFloatFromInt32[i]);

    // Test that clipping is properly done.
    EXPECT_EQ(1.0f, convertAudioSample<float>(1.1f));
    EXPECT_EQ(-1.0f, convertAudioSample<float>(-1.1f));
    EXPECT_EQ(1.0f, convertAudioSample<float>(convertAudioSample<int32_t>(1.1f)));
    EXPECT_EQ(-1.0f, convertAudioSample<float>(convertAudioSample<int32_t>(-1.1f)));
}

} // namespace TestWebKitAPI
