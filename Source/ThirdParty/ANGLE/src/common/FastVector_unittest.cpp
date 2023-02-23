//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FixedVector_unittest:
//   Tests of the FastVector class
//

#include <gtest/gtest.h>

#include "common/FastVector.h"

namespace angle
{
// Make sure the various constructors compile and do basic checks
TEST(FastVector, Constructors)
{
    FastVector<int, 5> defaultContructor;
    EXPECT_EQ(0u, defaultContructor.size());

    // Try varying initial vector sizes to test purely stack-allocated and
    // heap-allocated vectors, and ensure they copy correctly.
    size_t vectorSizes[] = {5, 3, 16, 32};

    for (size_t i = 0; i < sizeof(vectorSizes) / sizeof(vectorSizes[0]); i++)
    {
        FastVector<int, 5> count(vectorSizes[i]);
        EXPECT_EQ(vectorSizes[i], count.size());

        FastVector<int, 5> countAndValue(vectorSizes[i], 2);
        EXPECT_EQ(vectorSizes[i], countAndValue.size());
        EXPECT_EQ(2, countAndValue[1]);

        FastVector<int, 5> copy(countAndValue);
        EXPECT_EQ(copy, countAndValue);

        FastVector<int, 5> copyRValue(std::move(count));
        EXPECT_EQ(vectorSizes[i], copyRValue.size());

        FastVector<int, 5> copyIter(countAndValue.begin(), countAndValue.end());
        EXPECT_EQ(copyIter, countAndValue);

        FastVector<int, 5> copyIterEmpty(countAndValue.begin(), countAndValue.begin());
        EXPECT_TRUE(copyIterEmpty.empty());

        FastVector<int, 5> assignCopy(copyRValue);
        EXPECT_EQ(vectorSizes[i], assignCopy.size());

        FastVector<int, 5> assignRValue(std::move(assignCopy));
        EXPECT_EQ(vectorSizes[i], assignRValue.size());
    }

    FastVector<int, 5> initializerList{1, 2, 3, 4, 5};
    EXPECT_EQ(5u, initializerList.size());
    EXPECT_EQ(3, initializerList[2]);

    // Larger than stack-allocated vector size
    FastVector<int, 5> initializerListHeap{1, 2, 3, 4, 5, 6, 7, 8};
    EXPECT_EQ(8u, initializerListHeap.size());
    EXPECT_EQ(3, initializerListHeap[2]);

    FastVector<int, 5> assignmentInitializerList = {1, 2, 3, 4, 5};
    EXPECT_EQ(5u, assignmentInitializerList.size());
    EXPECT_EQ(3, assignmentInitializerList[2]);

    // Larger than stack-allocated vector size
    FastVector<int, 5> assignmentInitializerListLarge = {1, 2, 3, 4, 5, 6, 7, 8};
    EXPECT_EQ(8u, assignmentInitializerListLarge.size());
    EXPECT_EQ(3, assignmentInitializerListLarge[2]);
}

// Test indexing operations (at, operator[])
TEST(FastVector, Indexing)
{
    FastVector<int, 5> vec = {0, 1, 2, 3, 4};
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(i, vec.at(i));
        EXPECT_EQ(vec[i], vec.at(i));
    }
}

// Test the push_back functions
TEST(FastVector, PushBack)
{
    FastVector<int, 5> vec;
    vec.push_back(1);
    EXPECT_EQ(1, vec[0]);
    vec.push_back(1);
    vec.push_back(1);
    vec.push_back(1);
    vec.push_back(1);
    EXPECT_EQ(5u, vec.size());
}

// Tests growing the fast vector beyond the fixed storage.
TEST(FastVector, Growth)
{
    constexpr size_t kSize = 4;
    FastVector<size_t, kSize> vec;

    for (size_t i = 0; i < kSize * 2; ++i)
    {
        vec.push_back(i);
    }

    EXPECT_EQ(kSize * 2, vec.size());

    for (size_t i = kSize * 2; i > 0; --i)
    {
        ASSERT_EQ(vec.back(), i - 1);
        vec.pop_back();
    }

    EXPECT_EQ(0u, vec.size());
}

// Test the pop_back function
TEST(FastVector, PopBack)
{
    FastVector<int, 5> vec;
    vec.push_back(1);
    EXPECT_EQ(1, (int)vec.size());
    vec.pop_back();
    EXPECT_EQ(0, (int)vec.size());
}

// Test the back function
TEST(FastVector, Back)
{
    FastVector<int, 5> vec;
    vec.push_back(1);
    vec.push_back(2);
    EXPECT_EQ(2, vec.back());
}

// Test the back function
TEST(FastVector, Front)
{
    FastVector<int, 5> vec;
    vec.push_back(1);
    vec.push_back(2);
    EXPECT_EQ(1, vec.front());
}

// Test the sizing operations
TEST(FastVector, Size)
{
    FastVector<int, 5> vec;
    EXPECT_TRUE(vec.empty());
    EXPECT_EQ(0u, vec.size());

    vec.push_back(1);
    EXPECT_FALSE(vec.empty());
    EXPECT_EQ(1u, vec.size());
}

// Test clearing the vector
TEST(FastVector, Clear)
{
    FastVector<int, 5> vec = {0, 1, 2, 3, 4};
    vec.clear();
    EXPECT_TRUE(vec.empty());
}

// Test clearing the vector larger than the fixed size.
TEST(FastVector, ClearWithLargerThanFixedSize)
{
    FastVector<int, 3> vec = {0, 1, 2, 3, 4};
    vec.clear();
    EXPECT_TRUE(vec.empty());
}

// Test resizing the vector
TEST(FastVector, Resize)
{
    FastVector<int, 5> vec;
    vec.resize(5u, 1);
    EXPECT_EQ(5u, vec.size());
    for (int i : vec)
    {
        EXPECT_EQ(1, i);
    }

    vec.resize(2u);
    EXPECT_EQ(2u, vec.size());
    for (int i : vec)
    {
        EXPECT_EQ(1, i);
    }

    // Resize to larger than minimum
    vec.resize(10u, 2);
    EXPECT_EQ(10u, vec.size());

    for (size_t index = 0; index < 2u; ++index)
    {
        EXPECT_EQ(1, vec[index]);
    }
    for (size_t index = 2u; index < 10u; ++index)
    {
        EXPECT_EQ(2, vec[index]);
    }

    // Resize back to smaller
    vec.resize(2u, 2);
    EXPECT_EQ(2u, vec.size());
}

// Test iterating over the vector
TEST(FastVector, Iteration)
{
    FastVector<int, 5> vec = {0, 1, 2, 3};

    int vistedCount = 0;
    for (int value : vec)
    {
        EXPECT_EQ(vistedCount, value);
        vistedCount++;
    }
    EXPECT_EQ(4, vistedCount);
}

// Tests that equality comparisons work even if reserved size differs.
TEST(FastVector, EqualityWithDifferentReservedSizes)
{
    FastVector<int, 3> vec1 = {1, 2, 3, 4, 5};
    FastVector<int, 5> vec2 = {1, 2, 3, 4, 5};
    EXPECT_EQ(vec1, vec2);
    vec2.push_back(6);
    EXPECT_NE(vec1, vec2);
}

// Tests vector operations with a non copyable type.
TEST(FastVector, NonCopyable)
{
    struct s : angle::NonCopyable
    {
        s() : x(0) {}
        s(int xin) : x(xin) {}
        s(s &&other) : x(other.x) {}
        s &operator=(s &&other)
        {
            x = other.x;
            return *this;
        }
        int x;
    };

    FastVector<s, 3> vec;
    vec.push_back(3);
    EXPECT_EQ(3, vec[0].x);

    FastVector<s, 3> copy = std::move(vec);
    EXPECT_EQ(1u, copy.size());
    EXPECT_EQ(3, copy[0].x);
}

// Tests reusing capacity for the new items
TEST(FastVector, ReuseCapacity)
{
    constexpr int kMagicValue = 123454321;
    struct s
    {
        int value;

        // Initialize in the constructor because of MSVC compiler bug:
        // error C2065: 'kMagicValue': undeclared identifier
        s() : value(kMagicValue) {}
    };

    FastVector<s, 3> vec;
    vec.resize(3);
    EXPECT_EQ(kMagicValue, vec[0].value);
    EXPECT_EQ(kMagicValue, vec[1].value);
    EXPECT_EQ(kMagicValue, vec[2].value);

    vec[2].value = 0;
    vec.resize(4);
    EXPECT_EQ(0, vec[2].value);
    EXPECT_EQ(kMagicValue, vec[3].value);

    vec[3].value = 0;
    EXPECT_EQ(0, vec[3].value);

    vec.pop_back();
    vec.resize(2);
    vec.resize(4);
    EXPECT_EQ(kMagicValue, vec[2].value);
    EXPECT_EQ(kMagicValue, vec[3].value);
}

// Tests destroying old items after resize to a lesser size
TEST(FastVector, DestroyOldItems)
{
    int counter = 0;

    struct s : angle::NonCopyable
    {
        int *counter = nullptr;

        ~s() { reset(); }
        s &operator=(const s &other)
        {
            reset();
            init(other.counter);
            return *this;
        }
        s &operator=(s &&other)
        {
            std::swap(counter, other.counter);
            return *this;
        }
        void init(int *c)
        {
            counter = c;
            if (counter != nullptr)
            {
                ++(*counter);
            }
        }
        void reset()
        {
            if (counter != nullptr)
            {
                --(*counter);
                counter = nullptr;
            }
        }
    };

    FastVector<s, 3> vec;

    vec.resize(3);
    vec[0].init(&counter);
    vec[1].init(&counter);
    vec[2].init(&counter);
    EXPECT_EQ(3, counter);

    vec.resize(6);
    vec[3].init(&counter);
    vec[4].init(&counter);
    vec[5].init(&counter);
    EXPECT_EQ(6, counter);

    vec.resize(3);
    EXPECT_EQ(3, counter);

    vec.pop_back();
    EXPECT_EQ(2, counter);

    vec.clear();
    EXPECT_EQ(0, counter);

    vec.resize(3);
    vec[0].init(&counter);
    vec[1].init(&counter);
    vec[2].init(&counter);
    EXPECT_EQ(3, counter);

    FastVector<s, 3> vec2;
    vec = vec2;
    EXPECT_EQ(0, counter);

    vec.resize(3);
    vec[0].init(&counter);
    vec[1].init(&counter);
    vec[2].init(&counter);
    EXPECT_EQ(3, counter);

    vec = {s()};
    EXPECT_EQ(0, counter);

    vec[0].init(&counter);
    EXPECT_EQ(1, counter);
}

// Basic functionality for FlatUnorderedMap
TEST(FlatUnorderedMap, BasicUsage)
{
    FlatUnorderedMap<int, bool, 3> testMap;
    EXPECT_TRUE(testMap.empty());
    EXPECT_EQ(testMap.size(), 0u);

    testMap.insert(5, true);
    EXPECT_TRUE(testMap.contains(5));
    EXPECT_EQ(testMap.size(), 1u);

    bool value = false;
    EXPECT_TRUE(testMap.get(5, &value));
    EXPECT_TRUE(value);
    EXPECT_FALSE(testMap.get(6, &value));

    EXPECT_FALSE(testMap.empty());
    testMap.clear();
    EXPECT_TRUE(testMap.empty());
    EXPECT_EQ(testMap.size(), 0u);

    for (int i = 0; i < 10; ++i)
    {
        testMap.insert(i, false);
    }

    EXPECT_FALSE(testMap.empty());
    EXPECT_EQ(testMap.size(), 10u);

    for (int i = 0; i < 10; ++i)
    {
        EXPECT_TRUE(testMap.contains(i));
        EXPECT_TRUE(testMap.get(i, &value));
        EXPECT_FALSE(value);
    }
}

// Basic functionality for FlatUnorderedSet
TEST(FlatUnorderedSet, BasicUsage)
{
    FlatUnorderedSet<int, 3> testMap;
    EXPECT_TRUE(testMap.empty());

    testMap.insert(5);
    EXPECT_TRUE(testMap.contains(5));
    EXPECT_FALSE(testMap.contains(6));
    EXPECT_FALSE(testMap.empty());

    testMap.clear();
    EXPECT_TRUE(testMap.empty());

    for (int i = 0; i < 10; ++i)
    {
        testMap.insert(i);
    }

    for (int i = 0; i < 10; ++i)
    {
        EXPECT_TRUE(testMap.contains(i));
    }
}

// Comparison of FlatUnorderedSet
TEST(FlatUnorderedSet, Comparison)
{
    FlatUnorderedSet<int, 3> testSet0;
    FlatUnorderedSet<int, 3> testSet1;
    EXPECT_TRUE(testSet0.empty());
    EXPECT_TRUE(testSet1.empty());

    testSet0.insert(5);
    EXPECT_FALSE(testSet0 == testSet1);

    testSet0.insert(10);
    EXPECT_FALSE(testSet0 == testSet1);

    testSet1.insert(5);
    EXPECT_FALSE(testSet0 == testSet1);

    testSet1.insert(15);
    EXPECT_FALSE(testSet0 == testSet1);

    testSet1.clear();
    testSet1.insert(5);
    testSet1.insert(10);
    EXPECT_TRUE(testSet0 == testSet1);
}

// Basic functionality for FastIntegerSet
TEST(FastIntegerSet, BasicUsage)
{
    FastIntegerSet testMap;
    EXPECT_TRUE(testMap.empty());

    testMap.insert(5);
    EXPECT_TRUE(testMap.contains(5));
    EXPECT_FALSE(testMap.contains(6));
    EXPECT_FALSE(testMap.empty());

    testMap.clear();
    EXPECT_TRUE(testMap.empty());

    for (int i = 0; i < 10; ++i)
    {
        testMap.insert(i);
    }

    for (int i = 0; i < 10; ++i)
    {
        EXPECT_TRUE(testMap.contains(i));
    }
}

// Basic functionality for FastIntegerMap
TEST(FastIntegerMap, BasicUsage)
{
    using KeyValuePair             = std::pair<int, std::string>;
    std::set<KeyValuePair> entries = {KeyValuePair(17, "testing"), KeyValuePair(63, "fast"),
                                      KeyValuePair(97, "integer"), KeyValuePair(256, "map")};

    FastIntegerMap<std::string> testMap;
    EXPECT_TRUE(testMap.empty());

    std::string str;
    testMap.insert(entries.begin()->first, entries.begin()->second);
    EXPECT_TRUE(testMap.contains(entries.begin()->first));
    EXPECT_FALSE(testMap.contains(entries.rbegin()->first));
    EXPECT_FALSE(testMap.empty());
    EXPECT_EQ(testMap.size(), 1u);
    EXPECT_TRUE(testMap.get(entries.begin()->first, &str));
    EXPECT_EQ(entries.begin()->second, str);
    EXPECT_FALSE(testMap.get(1, &str));

    testMap.clear();
    EXPECT_TRUE(testMap.empty());
    EXPECT_EQ(testMap.size(), 0u);

    for (KeyValuePair entry : entries)
    {
        testMap.insert(entry.first, entry.second);
    }
    EXPECT_EQ(testMap.size(), 4u);

    for (KeyValuePair entry : entries)
    {
        EXPECT_TRUE(testMap.get(entry.first, &str));
        EXPECT_EQ(entry.second, str);
    }

    testMap.clear();
    EXPECT_TRUE(testMap.empty());
    EXPECT_EQ(testMap.size(), 0u);
}

// Basic usage tests of fast map.
TEST(FastMap, Basic)
{
    FastMap<int, 5> testMap;
    EXPECT_TRUE(testMap.empty());

    testMap[5] = 5;
    EXPECT_FALSE(testMap.empty());

    testMap.clear();
    EXPECT_TRUE(testMap.empty());

    for (int i = 0; i < 10; ++i)
    {
        testMap[i] = i;
    }

    for (int i = 0; i < 10; ++i)
    {
        EXPECT_TRUE(testMap[i] == i);
    }
}
}  // namespace angle
