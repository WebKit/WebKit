/*
 * Copyright (C) 2026 Anthropic PBC.
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
#include <wtf/LEBEncoder.h>

#include <wtf/LEBDecoder.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

static void testUInt32LEBEncode(uint32_t value, std::initializer_list<uint8_t> expected)
{
    Vector<uint8_t> bytes;
    WTF::LEBEncoder::encodeUInt32(bytes, value);
    EXPECT_EQ(Vector<uint8_t>(expected), bytes) << value;

    size_t offset = 0;
    EXPECT_EQ(value, WTF::LEBDecoder::decodeUInt32OrCrash(bytes, offset));
    EXPECT_EQ(bytes.size(), offset);
}

static void testInt32LEBEncode(int32_t value, std::initializer_list<uint8_t> expected)
{
    Vector<uint8_t> bytes;
    WTF::LEBEncoder::encodeInt32(bytes, value);
    EXPECT_EQ(Vector<uint8_t>(expected), bytes) << value;

    size_t offset = 0;
    EXPECT_EQ(value, WTF::LEBDecoder::decodeInt32OrCrash(bytes, offset));
    EXPECT_EQ(bytes.size(), offset);
}

TEST(WTF, LEBEncoderUInt32)
{
    testUInt32LEBEncode(0, { 0x00 });
    testUInt32LEBEncode(0x7f, { 0x7f });
    testUInt32LEBEncode(0x80, { 0x80, 0x01 });
    testUInt32LEBEncode(0x380, { 0x80, 0x07 });
    testUInt32LEBEncode(0x82f3, { 0xf3, 0x85, 0x02 });
    testUInt32LEBEncode(0xe9fc2f3, { 0xf3, 0x85, 0xff, 0x74 });
    testUInt32LEBEncode(0xffffffff, { 0xff, 0xff, 0xff, 0xff, 0x0f });
}

TEST(WTF, LEBEncoderInt32)
{
    testInt32LEBEncode(0, { 0x00 });
    testInt32LEBEncode(1, { 0x01 });
    testInt32LEBEncode(-1, { 0x7f });
    testInt32LEBEncode(63, { 0x3f });
    testInt32LEBEncode(64, { 0xc0, 0x00 });
    testInt32LEBEncode(-64, { 0x40 });
    testInt32LEBEncode(-65, { 0xbf, 0x7f });
    testInt32LEBEncode(-128, { 0x80, 0x7f });
    testInt32LEBEncode(8191, { 0xff, 0x3f });
    testInt32LEBEncode(-8192, { 0x80, 0x40 });
    testInt32LEBEncode(std::numeric_limits<int32_t>::max(), { 0xff, 0xff, 0xff, 0xff, 0x07 });
    testInt32LEBEncode(std::numeric_limits<int32_t>::min(), { 0x80, 0x80, 0x80, 0x80, 0x78 });
}

TEST(WTF, LEBEncoderRoundTrip64)
{
    for (uint64_t value : std::initializer_list<uint64_t> { 0, 0x7f, 0x80, 0xffffffff, 0x100000000, std::numeric_limits<uint64_t>::max() }) {
        Vector<uint8_t> bytes;
        WTF::LEBEncoder::encodeUInt64(bytes, value);
        size_t offset = 0;
        EXPECT_EQ(value, WTF::LEBDecoder::decodeUInt64OrCrash(bytes, offset));
        EXPECT_EQ(bytes.size(), offset);
    }
    for (int64_t value : std::initializer_list<int64_t> { 0, -1, 63, -64, 64, -65, 0x7fffffff, -0x80000000, std::numeric_limits<int64_t>::max(), std::numeric_limits<int64_t>::min() }) {
        Vector<uint8_t> bytes;
        WTF::LEBEncoder::encodeInt64(bytes, value);
        size_t offset = 0;
        EXPECT_EQ(value, WTF::LEBDecoder::decodeInt64OrCrash(bytes, offset));
        EXPECT_EQ(bytes.size(), offset);
    }
}

} // namespace TestWebKitAPI
