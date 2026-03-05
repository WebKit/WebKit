/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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
#include <wtf/OptionCountedSet.h>

namespace TestWebKitAPI {

enum class OptionCountedSetTestFlags : uint32_t {
    A = 1 << 0,
    B = 1 << 1,
    C = 1 << 2,
};

TEST(WTF_OptionCountedSet, EmptySet)
{
    OptionCountedSet<OptionCountedSetTestFlags> set;
    EXPECT_TRUE(set.isEmpty());
    EXPECT_FALSE(set.contains(OptionCountedSetTestFlags::A));
    EXPECT_FALSE(set.contains(OptionCountedSetTestFlags::B));
    EXPECT_FALSE(set.contains(OptionCountedSetTestFlags::C));
    set.add({ OptionCountedSetTestFlags::C, OptionCountedSetTestFlags::B });
    EXPECT_FALSE(set.isEmpty());
    EXPECT_FALSE(set.contains(OptionCountedSetTestFlags::A));
    EXPECT_TRUE(set.contains(OptionCountedSetTestFlags::B));
    EXPECT_TRUE(set.contains(OptionCountedSetTestFlags::C));
}

TEST(WTF_OptionCountedSet, AddAndRemove)
{
    OptionCountedSet<OptionCountedSetTestFlags> set;
    set.add(OptionCountedSetTestFlags::A);
    EXPECT_TRUE(set.contains(OptionCountedSetTestFlags::A));
    EXPECT_FALSE(set.contains(OptionCountedSetTestFlags::B));
    EXPECT_FALSE(set.contains(OptionCountedSetTestFlags::C));

    // Adding the same flag increments the counter.
    set.add(OptionCountedSetTestFlags::A);
    EXPECT_TRUE(set.contains(OptionCountedSetTestFlags::A));
    EXPECT_FALSE(set.contains(OptionCountedSetTestFlags::B));
    EXPECT_FALSE(set.contains(OptionCountedSetTestFlags::C));

    // Removing the flag added twice decrements the counter.
    set.remove(OptionCountedSetTestFlags::A);
    EXPECT_TRUE(set.contains(OptionCountedSetTestFlags::A));
    EXPECT_FALSE(set.contains(OptionCountedSetTestFlags::B));
    EXPECT_FALSE(set.contains(OptionCountedSetTestFlags::C));

    // Removing again makes the flag not present anymore.
    set.remove(OptionCountedSetTestFlags::A);
    EXPECT_FALSE(set.contains(OptionCountedSetTestFlags::A));
    EXPECT_FALSE(set.contains(OptionCountedSetTestFlags::B));
    EXPECT_FALSE(set.contains(OptionCountedSetTestFlags::C));

    // Remove again does nothing.
    set.remove(OptionCountedSetTestFlags::A);
    EXPECT_FALSE(set.contains(OptionCountedSetTestFlags::A));
    EXPECT_FALSE(set.contains(OptionCountedSetTestFlags::B));
    EXPECT_FALSE(set.contains(OptionCountedSetTestFlags::C));

    // Add multiple flags.
    set.add({ OptionCountedSetTestFlags::B, OptionCountedSetTestFlags::C });
    EXPECT_FALSE(set.contains(OptionCountedSetTestFlags::A));
    EXPECT_TRUE(set.contains(OptionCountedSetTestFlags::B));
    EXPECT_TRUE(set.contains(OptionCountedSetTestFlags::C));
    set.add({ OptionCountedSetTestFlags::A, OptionCountedSetTestFlags::C });
    EXPECT_TRUE(set.contains(OptionCountedSetTestFlags::A));
    EXPECT_TRUE(set.contains(OptionCountedSetTestFlags::B));
    EXPECT_TRUE(set.contains(OptionCountedSetTestFlags::C));

    // Remove multiple flags.
    set.remove({ OptionCountedSetTestFlags::B, OptionCountedSetTestFlags::C });
    EXPECT_TRUE(set.contains(OptionCountedSetTestFlags::A));
    EXPECT_FALSE(set.contains(OptionCountedSetTestFlags::B));
    EXPECT_TRUE(set.contains(OptionCountedSetTestFlags::C));
    set.remove({ OptionCountedSetTestFlags::A, OptionCountedSetTestFlags::C });
    EXPECT_FALSE(set.contains(OptionCountedSetTestFlags::A));
    EXPECT_FALSE(set.contains(OptionCountedSetTestFlags::B));
    EXPECT_FALSE(set.contains(OptionCountedSetTestFlags::C));
}

} // namespace TestWebKitAPI
