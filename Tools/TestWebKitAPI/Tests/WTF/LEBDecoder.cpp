/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <wtf/LEBDecoder.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

static void testUnsignedLEBDecode(std::initializer_list<uint8_t> data, size_t startOffset, bool expectedStatus, uint32_t expectedResult, size_t expectedOffset)
{
    Vector<uint8_t> vector(data);
    uint32_t result;
    bool status = WTF::LEBDecoder::decodeUInt32(vector.data(), vector.size(), startOffset, result);
    EXPECT_EQ(status, expectedStatus);
    if (expectedStatus) {
        EXPECT_EQ(result, expectedResult);
        EXPECT_EQ(startOffset, expectedOffset);
    }
}

TEST(WTF, LEBDecoderUInt32)
{
    // Simple tests that use all the bits in the array
    testUnsignedLEBDecode({ 0x07 }, 0, true, 0x7lu, 1lu);
    testUnsignedLEBDecode({ 0x77 }, 0, true, 0x77lu, 1lu);
    testUnsignedLEBDecode({ 0x80, 0x07 }, 0, true, 0x380lu, 2lu);
    testUnsignedLEBDecode({ 0x89, 0x12 }, 0, true, 0x909lu, 2lu);
    testUnsignedLEBDecode({ 0xf3, 0x85, 0x02 }, 0, true, 0x82f3lu, 3lu);
    testUnsignedLEBDecode({ 0xf3, 0x85, 0xff, 0x74 }, 0, true, 0xe9fc2f3lu, 4lu);
    testUnsignedLEBDecode({ 0xf3, 0x85, 0xff, 0xf4, 0x7f }, 0, true, 0xfe9fc2f3lu, 5lu);
    // Test with extra trailing numbers
    testUnsignedLEBDecode({ 0x07, 0x80 }, 0, true, 0x7lu, 1lu);
    testUnsignedLEBDecode({ 0x07, 0x75 }, 0, true, 0x7lu, 1lu);
    testUnsignedLEBDecode({ 0xf3, 0x85, 0xff, 0x74, 0x43 }, 0, true, 0xe9fc2f3lu, 4lu);
    testUnsignedLEBDecode({ 0xf3, 0x85, 0xff, 0x74, 0x80 }, 0, true, 0xe9fc2f3lu, 4lu);
    // Test with preceeding numbers
    testUnsignedLEBDecode({ 0xf3, 0x07 }, 1, true, 0x7lu, 2lu);
    testUnsignedLEBDecode({ 0x03, 0x07 }, 1, true, 0x7lu, 2lu);
    testUnsignedLEBDecode({ 0xf2, 0x53, 0x43, 0x67, 0x79, 0x77 }, 5, true, 0x77lu, 6lu);
    testUnsignedLEBDecode({ 0xf2, 0x53, 0x43, 0xf7, 0x84, 0x77 }, 5, true, 0x77lu, 6ul);
    testUnsignedLEBDecode({ 0xf2, 0x53, 0x43, 0xf3, 0x85, 0x02 }, 3, true, 0x82f3lu, 6lu);
    // Test in the middle
    testUnsignedLEBDecode({ 0xf3, 0x07, 0x89 }, 1, true, 0x7lu, 2lu);
    testUnsignedLEBDecode({ 0x03, 0x07, 0x23 }, 1, true, 0x7lu, 2lu);
    testUnsignedLEBDecode({ 0xf2, 0x53, 0x43, 0x67, 0x79, 0x77, 0x43 }, 5, true, 0x77lu, 6lu);
    testUnsignedLEBDecode({ 0xf2, 0x53, 0x43, 0xf7, 0x84, 0x77, 0xf9 }, 5, true, 0x77lu, 6lu);
    testUnsignedLEBDecode({ 0xf2, 0x53, 0x43, 0xf3, 0x85, 0x02, 0xa4 }, 3, true, 0x82f3lu, 6lu);
    // Test decode too long
    testUnsignedLEBDecode({ 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}, 0, false, 0x0lu, 0lu);
    testUnsignedLEBDecode({ 0x80, 0x80, 0xab, 0x8a, 0x9a, 0xa3, 0xff}, 1, false, 0x0lu, 0lu);
    testUnsignedLEBDecode({ 0x80, 0x80, 0xab, 0x8a, 0x9a, 0xa3, 0xff}, 0, false, 0x0lu, 0lu);
    // Test decode off end of array
    testUnsignedLEBDecode({ 0x80, 0x80, 0xab, 0x8a, 0x9a, 0xa3, 0xff}, 2, false, 0x0lu, 0lu);
}

static void testInt32LEBDecode(std::initializer_list<uint8_t> data, size_t startOffset, bool expectedStatus, int32_t expectedResult, size_t expectedOffset)
{
    Vector<uint8_t> vector(data);
    int32_t result;
    bool status = WTF::LEBDecoder::decodeInt32(vector.data(), vector.size(), startOffset, result);
    EXPECT_EQ(status, expectedStatus);
    if (expectedStatus) {
        EXPECT_EQ(result, expectedResult);
        EXPECT_EQ(startOffset, expectedOffset);
    }
}

TEST(WTF, LEBDecoderInt32)
{
    // Simple tests that use all the bits in the array
    testInt32LEBDecode({ 0x07 }, 0, true, 0x7, 1lu);
    testInt32LEBDecode({ 0x77 }, 0, true, -0x9, 1lu);
    testInt32LEBDecode({ 0x80, 0x07 }, 0, true, 0x380, 2lu);
    testInt32LEBDecode({ 0x89, 0x12 }, 0, true, 0x909, 2lu);
    testInt32LEBDecode({ 0xf3, 0x85, 0x02 }, 0, true, 0x82f3, 3lu);
    testInt32LEBDecode({ 0xf3, 0x85, 0xff, 0x74 }, 0, true, 0xfe9fc2f3, 4lu);
    testInt32LEBDecode({ 0xf3, 0x85, 0xff, 0xf4, 0x7f }, 0, true, 0xfe9fc2f3, 5lu);
    // Test with extra trailing numbers
    testInt32LEBDecode({ 0x07, 0x80 }, 0, true, 0x7, 1lu);
    testInt32LEBDecode({ 0x07, 0x75 }, 0, true, 0x7, 1lu);
    testInt32LEBDecode({ 0xf3, 0x85, 0xff, 0x74, 0x43 }, 0, true, 0xfe9fc2f3, 4lu);
    testInt32LEBDecode({ 0xf3, 0x85, 0xff, 0x74, 0x80 }, 0, true, 0xfe9fc2f3, 4lu);
    // Test with preceeding numbers
    testInt32LEBDecode({ 0xf3, 0x07 }, 1, true, 0x7, 2lu);
    testInt32LEBDecode({ 0x03, 0x07 }, 1, true, 0x7, 2lu);
    testInt32LEBDecode({ 0xf2, 0x53, 0x43, 0x67, 0x79, 0x77 }, 5, true, -0x9, 6lu);
    testInt32LEBDecode({ 0xf2, 0x53, 0x43, 0xf7, 0x84, 0x77 }, 5, true, -0x9, 6lu);
    testInt32LEBDecode({ 0xf2, 0x53, 0x43, 0xf3, 0x85, 0x02 }, 3, true, 0x82f3, 6lu);
    // Test in the middle
    testInt32LEBDecode({ 0xf3, 0x07, 0x89 }, 1, true, 0x7, 2lu);
    testInt32LEBDecode({ 0x03, 0x07, 0x23 }, 1, true, 0x7, 2lu);
    testInt32LEBDecode({ 0xf2, 0x53, 0x43, 0x67, 0x79, 0x77, 0x43 }, 5, true, -0x9, 6lu);
    testInt32LEBDecode({ 0xf2, 0x53, 0x43, 0xf7, 0x84, 0x77, 0xf9 }, 5, true, -0x9, 6lu);
    testInt32LEBDecode({ 0xf2, 0x53, 0x43, 0xf3, 0x85, 0x02, 0xa4 }, 3, true, 0x82f3, 6lu);
    // Test decode too long
    testInt32LEBDecode({ 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}, 0, false, 0x0, 0lu);
    testInt32LEBDecode({ 0x80, 0x80, 0xab, 0x8a, 0x9a, 0xa3, 0xff}, 1, false, 0x0, 0lu);
    testInt32LEBDecode({ 0x80, 0x80, 0xab, 0x8a, 0x9a, 0xa3, 0xff}, 0, false, 0x0, 0lu);
    // Test decode off end of array
    testInt32LEBDecode({ 0x80, 0x80, 0xab, 0x8a, 0x9a, 0xa3, 0xff}, 2, false, 0x0, 0lu);
}

} // namespace TestWebKitAPI
