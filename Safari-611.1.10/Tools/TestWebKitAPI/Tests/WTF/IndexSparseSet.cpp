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

#include "Test.h"
#include <wtf/IndexSparseSet.h>
#include <wtf/KeyValuePair.h>

namespace TestWebKitAPI {

TEST(WTF_IndexSparseSet, AddRemove)
{
    IndexSparseSet testSet(100);

    // Starts empty
    EXPECT_FALSE(testSet.contains(0));
    EXPECT_FALSE(testSet.contains(42));
    EXPECT_EQ(testSet.size(), 0u);
    EXPECT_TRUE(testSet.isEmpty());

    // After adding a value, that value and nothing else is in
    EXPECT_TRUE(testSet.add(42));
    EXPECT_FALSE(testSet.contains(13));
    EXPECT_TRUE(testSet.contains(42));
    EXPECT_TRUE(testSet.get(13) == nullptr);
    EXPECT_TRUE(testSet.get(42) != nullptr);
    EXPECT_EQ(*testSet.get(42), 42u);
    EXPECT_EQ(testSet.size(), 1u);
    testSet.validate();

    // Adding an already existing value is a no-op that returns false
    EXPECT_FALSE(testSet.add(42));
    EXPECT_TRUE(testSet.contains(42));
    EXPECT_EQ(testSet.size(), 1u);
    testSet.validate();

    // Remove fails on non-existing values
    EXPECT_FALSE(testSet.remove(90));
    EXPECT_EQ(testSet.size(), 1u);
    testSet.validate();

    // But succeeds on an existing one
    EXPECT_TRUE(testSet.remove(42));
    EXPECT_FALSE(testSet.contains(42));
    EXPECT_EQ(testSet.get(42), nullptr);
    EXPECT_FALSE(testSet.remove(42));
    EXPECT_EQ(testSet.size(), 0u);
    testSet.validate();

    // And it is possible to add a value back in
    EXPECT_TRUE(testSet.add(42));
    EXPECT_TRUE(testSet.contains(42));
    EXPECT_EQ(testSet.size(), 1u);
    testSet.validate();
}

TEST(WTF_IndexSparseSet, NonTrivialType)
{
    IndexSparseSet<KeyValuePair<unsigned, double>> testSet(100);

    // Starts empty
    EXPECT_FALSE(testSet.contains(0));
    EXPECT_FALSE(testSet.contains(42));
    EXPECT_EQ(testSet.size(), 0u);
    EXPECT_TRUE(testSet.isEmpty());

    // After adding a value, that value and nothing else is in
    EXPECT_TRUE(testSet.add(42, 3.14));
    EXPECT_FALSE(testSet.contains(13));
    EXPECT_TRUE(testSet.contains(42));
    EXPECT_TRUE(testSet.get(13) == nullptr);
    EXPECT_TRUE(testSet.get(42) != nullptr);
    EXPECT_EQ(testSet.get(42)->value, 3.14);
    EXPECT_EQ(testSet.size(), 1u);
    testSet.validate();

    // Adding an already existing value is a no-op that returns false
    EXPECT_FALSE(testSet.add(42, 5.19));
    EXPECT_TRUE(testSet.contains(42));
    EXPECT_EQ(testSet.get(42)->value, 3.14);
    EXPECT_EQ(testSet.size(), 1u);
    testSet.validate();

    // Set correctly updates the value and returns false in that case
    EXPECT_FALSE(testSet.set(42, 17.18));
    EXPECT_TRUE(testSet.contains(42));
    EXPECT_EQ(testSet.get(42)->value, 17.18);
    EXPECT_EQ(testSet.size(), 1u);
    testSet.validate();

    // Set on a new key acts like add
    EXPECT_TRUE(testSet.set(13, 13.14));
    EXPECT_TRUE(testSet.contains(13));
    EXPECT_EQ(testSet.get(13)->value, 13.14);
    EXPECT_EQ(testSet.size(), 2u);
    testSet.validate();

    // Remove fails on non-existing values
    EXPECT_FALSE(testSet.remove(90));
    EXPECT_EQ(testSet.size(), 2u);
    testSet.validate();

    // But succeeds on an existing one
    EXPECT_TRUE(testSet.remove(42));
    EXPECT_FALSE(testSet.contains(42));
    EXPECT_EQ(testSet.get(42), nullptr);
    EXPECT_FALSE(testSet.remove(42));
    EXPECT_EQ(testSet.size(), 1u);
    testSet.validate();

    // And it is possible to add a value back in
    EXPECT_TRUE(testSet.add(42, 1.1));
    EXPECT_TRUE(testSet.contains(42));
    EXPECT_EQ(testSet.get(42)->value, 1.1);
    EXPECT_EQ(testSet.size(), 2u);
    testSet.validate();
}

TEST(WTF_IndexSparseSet, SortClear)
{
    IndexSparseSet testSet(100);
    unsigned values[] = {1, 4, 32, 13, 2, 25, 54, 2, 3};

    for (unsigned value : values)
        testSet.add(value);

    testSet.sort();
    testSet.validate();

    // The same values are in the set after it is sorted
    for (unsigned value : values)
        EXPECT_TRUE(testSet.contains(value));
    EXPECT_EQ(testSet.size(), 8u);

    // And iterating the set returns them in sorted order
    unsigned sortedValues[20];
    unsigned index = 0;
    for (unsigned value : testSet)
        sortedValues[index++] = value;
    EXPECT_EQ(index, 8u);
    EXPECT_EQ(sortedValues[0], 1u);
    EXPECT_EQ(sortedValues[1], 2u);
    EXPECT_EQ(sortedValues[2], 3u);
    EXPECT_EQ(sortedValues[3], 4u);
    EXPECT_EQ(sortedValues[4], 13u);
    EXPECT_EQ(sortedValues[5], 25u);
    EXPECT_EQ(sortedValues[6], 32u);
    EXPECT_EQ(sortedValues[7], 54u);

    testSet.clear();
    testSet.validate();

    // After clearing the set it is truly empty
    EXPECT_TRUE(testSet.isEmpty());
    for (unsigned value = 0; value < 100; ++value)
        EXPECT_FALSE(testSet.contains(value));
    unsigned numberOfIterations = 0;
    for (unsigned value : testSet) {
        (void) value;
        ++numberOfIterations;
    }
    EXPECT_EQ(numberOfIterations, 0u);

}

} // namespace TestWebKitAPI
