//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BinaryStream_unittest.cpp: Unit tests of the binary stream classes.

#include "common/BinaryStream.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "common/PackedEnums.h"
#include "common/span.h"
#include "common/span_util.h"
#include "common/unsafe_buffers.h"
#include "gtest/gtest.h"

namespace angle
{

// Test that errors are properly generated for overflows.
TEST(BinaryInputStream, Overflow)
{
    const uint8_t goodValue = 2;
    const uint8_t badValue  = 255;

    const size_t dataSize = 1024;
    const size_t slopSize = 1024;

    std::vector<uint8_t> data(dataSize + slopSize);
    angle::Span<uint8_t> good_data = angle::Span(data).first(dataSize);
    angle::Span<uint8_t> bad_data  = angle::Span(data).subspan(dataSize);
    std::fill(good_data.begin(), good_data.end(), goodValue);
    std::fill(bad_data.begin(), bad_data.end(), badValue);

    auto checkDataIsSafe = [=](uint8_t item) { return item == goodValue; };

    {
        // One large read
        std::vector<uint8_t> outputData(dataSize);
        gl::BinaryInputStream stream(good_data);
        stream.readBytes(outputData);
        ASSERT_FALSE(stream.error());
        ASSERT_TRUE(std::all_of(outputData.begin(), outputData.end(), checkDataIsSafe));
        ASSERT_TRUE(stream.endOfStream());
    }

    {
        // Two half-sized reads
        std::vector<uint8_t> outputData(dataSize);
        gl::BinaryInputStream stream(good_data);
        stream.readBytes(angle::Span(outputData).first(dataSize / 2));
        ASSERT_FALSE(stream.error());
        stream.readBytes(angle::Span(outputData).subspan(dataSize / 2));
        ASSERT_FALSE(stream.error());
        ASSERT_TRUE(std::all_of(outputData.begin(), outputData.end(), checkDataIsSafe));
        ASSERT_TRUE(stream.endOfStream());
    }

    {
        // One large read that is too big
        std::vector<uint8_t> outputData(dataSize + 1);
        gl::BinaryInputStream stream(good_data);
        stream.readBytes(outputData);
        ASSERT_TRUE(stream.error());
    }

    {
        // Two reads, one that overflows the offset
        std::vector<uint8_t> outputData(dataSize - 1);
        gl::BinaryInputStream stream(good_data);
        stream.readBytes(outputData);
        ASSERT_FALSE(stream.error());
        // SAFETY: required for test, span is not legitimate.
        ANGLE_UNSAFE_BUFFERS(stream.readBytes(
            angle::Span(outputData.data(), std::numeric_limits<size_t>::max() - dataSize - 2)));
        ASSERT_TRUE(stream.error());
    }
}

// Test that readInt<> and writeInt<> match.
TEST(BinaryStream, Int)
{
    gl::BinaryOutputStream out;
    out.writeInt<int8_t>(-100);
    out.writeInt<int16_t>(-200);
    out.writeInt<int32_t>(-300);
    out.writeInt<int64_t>(-400);
    out.writeInt<uint8_t>(100);
    out.writeInt<uint16_t>(200);
    out.writeInt<uint32_t>(300);
    out.writeInt<uint64_t>(400);
    out.writeInt<size_t>(500);

    gl::BinaryInputStream in(out);
    EXPECT_EQ(in.readInt<int8_t>(), -100);
    EXPECT_EQ(in.readInt<int16_t>(), -200);
    EXPECT_EQ(in.readInt<int32_t>(), -300);
    EXPECT_EQ(in.readInt<int64_t>(), -400);
    EXPECT_EQ(in.readInt<uint8_t>(), 100u);
    EXPECT_EQ(in.readInt<uint16_t>(), 200u);
    EXPECT_EQ(in.readInt<uint32_t>(), 300u);
    EXPECT_EQ(in.readInt<uint64_t>(), 400u);
    EXPECT_EQ(in.readInt<size_t>(), 500u);

    EXPECT_FALSE(in.error());
    EXPECT_TRUE(in.endOfStream());
}

// Test that readBool and writeBool match.
TEST(BinaryStream, Bool)
{
    gl::BinaryOutputStream out;
    out.writeBool(true);
    out.writeBool(false);

    gl::BinaryInputStream in(out);
    EXPECT_EQ(in.readBool(), true);
    EXPECT_EQ(in.readBool(), false);

    EXPECT_FALSE(in.error());
    EXPECT_TRUE(in.endOfStream());
}

// Test that readVector and writeVector match.
TEST(BinaryStream, Vector)
{
    std::vector<unsigned int> writeData = {1, 2, 3, 4, 5};
    std::vector<unsigned int> readData;

    gl::BinaryOutputStream out;
    out.writeVector(writeData);

    gl::BinaryInputStream in(out);
    in.readVector(&readData);

    ASSERT_EQ(writeData.size(), readData.size());

    for (size_t i = 0; i < writeData.size(); ++i)
    {
        ASSERT_EQ(writeData[i], readData[i]);
    }
}

// Test that readString and writeString match.
TEST(BinaryStream, String)
{
    std::string empty;
    std::string hello("hello");
    std::string nulls("\0\0\0", 3u);
    EXPECT_EQ(3u, nulls.size());

    gl::BinaryOutputStream out;
    out.writeString(empty);
    out.writeString(hello);
    out.writeString(nulls);
    out.writeString(empty);
    out.writeString(empty);
    out.writeString(hello);

    gl::BinaryInputStream in(out);
    EXPECT_EQ(in.readString(), empty);
    EXPECT_EQ(in.readString(), hello);
    EXPECT_EQ(in.readString(), nulls);
    EXPECT_EQ(in.readString(), empty);
    EXPECT_EQ(in.readString(), empty);
    EXPECT_EQ(in.readString(), hello);

    EXPECT_FALSE(in.error());
    EXPECT_TRUE(in.endOfStream());
}

// Test that readStruct and writeStruct match.
TEST(BinaryStream, Struct)
{
    struct Pod
    {
        int count;
        char array[3];
    };

    Pod pod1 = {123, {1, 2, 3}};
    gl::BinaryOutputStream out;
    out.writeStruct(pod1);

    Pod pod2;
    gl::BinaryInputStream in(out);
    in.readStruct(&pod2);
    EXPECT_TRUE(angle::byte_span_from_ref(pod1) == angle::byte_span_from_ref(pod2));

    EXPECT_FALSE(in.error());
    EXPECT_TRUE(in.endOfStream());
}

// Test that readEnum and writeEnum match.
TEST(BinaryStream, Enum)
{
    enum class Color : uint8_t
    {
        kRed,
        kGreen,
        kBlue
    };
    enum class Shorty : int16_t
    {
        kNeg = -1,
        kMax = 32767
    };

    gl::BinaryOutputStream out;
    out.writeEnum(Color::kRed);
    out.writeEnum(Shorty::kNeg);
    out.writeEnum(Color::kGreen);
    out.writeEnum(Shorty::kMax);
    out.writeEnum(Color::kBlue);

    gl::BinaryInputStream in(out);
    EXPECT_EQ(Color::kRed, in.readEnum<Color>());
    EXPECT_EQ(Shorty::kNeg, in.readEnum<Shorty>());
    EXPECT_EQ(Color::kGreen, in.readEnum<Color>());
    EXPECT_EQ(Shorty::kMax, in.readEnum<Shorty>());
    EXPECT_EQ(Color::kBlue, in.readEnum<Color>());

    EXPECT_FALSE(in.error());
    EXPECT_TRUE(in.endOfStream());
}

// Test that readFloat and writeFloat match.
TEST(BinaryStream, Float)
{
    gl::BinaryOutputStream out;
    out.writeFloat(123.456f);
    out.writeFloat(-100.0f);
    out.writeFloat(0.0f);

    gl::BinaryInputStream in(out);
    EXPECT_EQ(123.456f, in.readFloat());
    EXPECT_EQ(-100.0f, in.readFloat());
    EXPECT_EQ(0.0f, in.readFloat());

    EXPECT_FALSE(in.error());
    EXPECT_TRUE(in.endOfStream());
}

// Test that readPackedEnumMap and writePackedEnumMap match.
TEST(BinaryStream, PackedEnumMap)
{
    enum class Color : int16_t
    {
        kRed,
        kGreen,
        kBlue,
        EnumCount = 3
    };

    angle::PackedEnumMap<Color, float> map1;
    map1[Color::kRed]   = 1.0f;
    map1[Color::kGreen] = 2.0f;
    map1[Color::kBlue]  = 3.0f;

    gl::BinaryOutputStream out;
    out.writePackedEnumMap(map1);

    angle::PackedEnumMap<Color, float> map2;
    gl::BinaryInputStream in(out);
    in.readPackedEnumMap(&map2);
    EXPECT_EQ(map2[Color::kRed], 1.0f);
    EXPECT_EQ(map2[Color::kGreen], 2.0f);
    EXPECT_EQ(map2[Color::kBlue], 3.0f);

    EXPECT_FALSE(in.error());
    EXPECT_TRUE(in.endOfStream());
}

// Test that skipping ahead works as expected.
TEST(BinaryStream, Skip)
{
    gl::BinaryOutputStream out;
    out.writeFloat(123.456f);
    out.writeFloat(-100.0f);

    gl::BinaryInputStream in(out);
    in.skip(sizeof(float));
    EXPECT_EQ(-100.0f, in.readFloat());

    EXPECT_FALSE(in.error());
    EXPECT_TRUE(in.endOfStream());
}

}  // namespace angle
