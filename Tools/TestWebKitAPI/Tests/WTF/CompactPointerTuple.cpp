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

#include "Test.h"
#include <wtf/CompactPointerTuple.h>

namespace TestWebKitAPI {

enum class ExampleFlags : uint8_t {
    A = 1 << 0,
    B = 1 << 1,
    C = 1 << 2,
};

TEST(CompactPointerTuple, CompactPointerWithUInt8_Simple)
{
    CompactPointerTuple<int*, uint8_t> pointerWithFlags { nullptr, 7 };
    EXPECT_EQ(nullptr, pointerWithFlags.pointer());
    EXPECT_EQ(7, pointerWithFlags.type());
}

TEST(CompactPointerTuple, CompactPointerWithOptionSet_EmptySet)
{
    CompactPointerTuple<int*, OptionSet<ExampleFlags>> pointerWithFlags;
    EXPECT_TRUE(pointerWithFlags.type().isEmpty());
    EXPECT_FALSE(pointerWithFlags.type().contains(ExampleFlags::A));
    EXPECT_FALSE(pointerWithFlags.type().contains(ExampleFlags::B));
    EXPECT_FALSE(pointerWithFlags.type().contains(ExampleFlags::C));
}

TEST(CompactPointerTuple, CompactPointerWithOptionSet_Simple)
{
    CompactPointerTuple<int*, OptionSet<ExampleFlags>> pointerWithFlags;
    pointerWithFlags.setType({ ExampleFlags::A, ExampleFlags::C });
    EXPECT_FALSE(pointerWithFlags.type().isEmpty());
    EXPECT_TRUE(pointerWithFlags.type().contains(ExampleFlags::A));
    EXPECT_FALSE(pointerWithFlags.type().contains(ExampleFlags::B));
    EXPECT_TRUE(pointerWithFlags.type().contains(ExampleFlags::C));
}

} // namespace TestWebKitAPI
