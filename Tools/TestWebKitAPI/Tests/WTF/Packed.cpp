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

#include <wtf/Packed.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

struct PackedPair {
    PackedPtr<uint8_t> key { nullptr };
    PackedPtr<uint8_t> value { nullptr };
};

TEST(WTF_Packed, StructSize)
{
    EXPECT_EQ(alignof(PackedPair), 1U);
#if CPU(X86_64)
    EXPECT_EQ(sizeof(PackedPair), 12U);
#endif
    {
        Packed<double> value;
        value = 4.2;
        EXPECT_EQ(value.get(), 4.2);
    }
    {
        uint64_t originalValue = 0xff00ff00dd00dd00UL;
        Packed<uint64_t> value;
        value = originalValue;
        EXPECT_EQ(value.get(), originalValue);
        EXPECT_EQ(alignof(Packed<uint64_t>), 1U);
        EXPECT_EQ(sizeof(Packed<uint64_t>), sizeof(uint64_t));
    }
}

TEST(WTF_Packed, AssignAndGet)
{
    {
        PackedPtr<uint8_t> key { nullptr };
        static_assert(OS_CONSTANT(EFFECTIVE_ADDRESS_WIDTH) != 64, "");
        uint8_t* max = bitwise_cast<uint8_t*>(static_cast<uintptr_t>(((1ULL) << OS_CONSTANT(EFFECTIVE_ADDRESS_WIDTH)) - 1));
        key = max;
        EXPECT_EQ(key.get(), max);
    }
}

TEST(WTF_Packed, PackedAlignedPtr)
{
    {
        PackedAlignedPtr<uint8_t, 256> key { nullptr };
        EXPECT_LE(sizeof(key), 5U);
    }
    {
        PackedAlignedPtr<uint8_t, 16> key { nullptr };
#if OS(DARWIN) && CPU(ARM64)
        EXPECT_EQ(sizeof(key), 4U);
#else
        EXPECT_LE(sizeof(key), 6U);
#endif
    }
}

struct PackingTarget {
    unsigned m_value { 0 };
};
TEST(WTF_Packed, HashMap)
{
    Vector<PackingTarget> vector;
    HashMap<PackedPtr<PackingTarget>, unsigned> map;
    vector.reserveCapacity(10000);
    for (unsigned i = 0; i < 10000; ++i)
        vector.uncheckedAppend(PackingTarget { i });

    for (auto& target : vector)
        map.add(&target, target.m_value);

    for (auto& target : vector) {
        EXPECT_TRUE(map.contains(&target));
        EXPECT_EQ(map.get(&target), target.m_value);
    }
}


} // namespace TestWebKitAPI
