/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/IDBKeyData.h>
#include <WebCore/IDBSerialization.h>
#include <array>

using namespace WebCore;

namespace TestWebKitAPI {

TEST(IDBSerialization, KeyDeserializationStringOverflow)
{
    // Crafted data: version byte (0x00), key type String (0x60),
    // length 0x80000000 (little-endian), then 4 bytes of char data.
    // The uint32_t length * 2 overflows to 0, bypassing the bounds check
    // without the fix.
    constexpr auto corruptData = std::to_array<uint8_t>({
        0x00, // version
        0x60, // SIDBKeyType::String
        0x00, 0x00, 0x00, 0x80, // length = 0x80000000 (little-endian)
        0x41, 0x00, 0x41, 0x00 // 2 UTF-16 chars 'A', 'A'
    });

    IDBKeyData result;
    bool success = deserializeIDBKeyData(std::span { corruptData }, result);
    EXPECT_FALSE(success);
}

TEST(IDBSerialization, KeyDeserializationStringValid)
{
    // A valid string key: version byte, String type, length 2, then 2 UTF-16 chars.
    constexpr auto validData = std::to_array<uint8_t>({
        0x00, // version
        0x60, // SIDBKeyType::String
        0x02, 0x00, 0x00, 0x00, // length = 2 (little-endian)
        0x41, 0x00, 0x42, 0x00 // 'A', 'B'
    });

    IDBKeyData result;
    bool success = deserializeIDBKeyData(std::span { validData }, result);
    EXPECT_TRUE(success);
}

TEST(IDBSerialization, KeyDeserializationBinaryArraySlicing)
{
    // An array key with two binary sub-keys: [binary(0xAA, 0xBB), binary(0xCC)].
    // Without the fix, the first binary sub-key's value would include the
    // trailing serialized bytes of the second sub-key.
    constexpr auto arrayData = std::to_array<uint8_t>({
        0x00, // version
        0xA0, // SIDBKeyType::Array
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // array length = 2 (uint64_t LE)
        0x80, // SIDBKeyType::Binary (first element)
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // binary size = 2 (uint64_t LE)
        0xAA, 0xBB, // binary data
        0x80, // SIDBKeyType::Binary (second element)
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // binary size = 1 (uint64_t LE)
        0xCC // binary data
    });

    IDBKeyData result;
    bool success = deserializeIDBKeyData(std::span { arrayData }, result);
    EXPECT_TRUE(success);

    auto& array = result.array();
    ASSERT_EQ(array.size(), 2u);

    auto* firstData = array[0].binary().data();
    ASSERT_TRUE(firstData);
    EXPECT_EQ(firstData->size(), 2u);
    EXPECT_EQ((*firstData)[0], 0xAA);
    EXPECT_EQ((*firstData)[1], 0xBB);

    auto* secondData = array[1].binary().data();
    ASSERT_TRUE(secondData);
    EXPECT_EQ(secondData->size(), 1u);
    EXPECT_EQ((*secondData)[0], 0xCC);
}

} // namespace TestWebKitAPI
