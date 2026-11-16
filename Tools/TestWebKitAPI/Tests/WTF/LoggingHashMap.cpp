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
#include <wtf/LoggingHashMap.h>

namespace TestWebKitAPI {

namespace {

constexpr char intMapTypeArguments[] = "int, int";
using IntLoggingHashMap = LoggingHashMap<intMapTypeArguments, int, int>;

}

TEST(WTF_LoggingHashMap, SetForwardsToUnderlyingMap)
{
    IntLoggingHashMap map;
    int key = 1;
    map.set(key, 10);
    map.set(2, 20);
    EXPECT_EQ(2u, map.size());

    auto result = map.set(key, 11);
    EXPECT_FALSE(result.isNewEntry);
    EXPECT_EQ(2u, map.size());

    unsigned sum = 0;
    for (auto& entry : map)
        sum += entry.value;
    EXPECT_EQ(31u, sum);
}

TEST(WTF_LoggingHashMap, AddForwardsToUnderlyingMap)
{
    IntLoggingHashMap map;
    int key = 1;
    EXPECT_TRUE(map.add(key, 10).isNewEntry);
    EXPECT_TRUE(map.add(2, 20).isNewEntry);
    EXPECT_FALSE(map.add(key, 99).isNewEntry);
    EXPECT_EQ(2u, map.size());

    unsigned sum = 0;
    for (auto& entry : map)
        sum += entry.value;
    EXPECT_EQ(30u, sum);
}

TEST(WTF_LoggingHashMap, FastAddForwardsToUnderlyingMap)
{
    IntLoggingHashMap map;
    int key = 1;
    EXPECT_TRUE(map.fastAdd(key, 10).isNewEntry);
    EXPECT_TRUE(map.fastAdd(2, 20).isNewEntry);
    EXPECT_FALSE(map.fastAdd(key, 99).isNewEntry);
    EXPECT_EQ(2u, map.size());
}

TEST(WTF_LoggingHashMap, AssignmentReturnsThis)
{
    IntLoggingHashMap source;
    source.add(1, 10);

    IntLoggingHashMap copy;
    IntLoggingHashMap& copyResult = (copy = source);
    EXPECT_EQ(&copy, &copyResult);
    EXPECT_EQ(1u, copy.size());

    IntLoggingHashMap moved;
    IntLoggingHashMap& moveResult = (moved = WTF::move(copy));
    EXPECT_EQ(&moved, &moveResult);
    EXPECT_EQ(1u, moved.size());
}

} // namespace TestWebKitAPI
