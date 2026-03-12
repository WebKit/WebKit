//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// span_unittests.cpp: Unit tests for the angle::Span class.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "common/span.h"

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace angle
{
namespace
{

constexpr size_t kSpanDataSize                  = 16;
constexpr unsigned int kSpanData[kSpanDataSize]                  = {0, 1, 2,  3,  4,  5,  6,  7,
                                                                    8, 9, 10, 11, 12, 13, 14, 15};
constexpr std::array<const unsigned int, kSpanDataSize> kSpanArr = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}};

class FakeRange
{
  public:
    size_t size() const { return kSpanDataSize; }
    const unsigned int *data() { return kSpanData; }
};

// Test that comparing spans work
TEST(SpanTest, Comparison)
{
    // Duplicate data to make sure comparison is being done on values (and not addresses).
    static constexpr unsigned int kSpanDataDup[kSpanDataSize] = {0, 1, 2,  3,  4,  5,  6,  7,
                                                                 8, 9, 10, 11, 12, 13, 14, 15};

    // Don't use ASSERT_EQ at first because the == is more hidden
    ASSERT_TRUE(Span<const unsigned int>() == Span(kSpanData, 0));
    ASSERT_TRUE(Span(kSpanData + 3, 4) != Span(kSpanDataDup + 5, 4));

    // Check ASSERT_EQ and ASSERT_NE work correctly
    ASSERT_EQ(Span(kSpanData, kSpanDataSize), Span(kSpanDataDup, kSpanDataSize));
    ASSERT_NE(Span(kSpanData, kSpanDataSize - 1), Span(kSpanDataDup + 1, kSpanDataSize - 1));
    ASSERT_NE(Span(kSpanData, kSpanDataSize), Span(kSpanDataDup, kSpanDataSize - 1));
    ASSERT_NE(Span(kSpanData, kSpanDataSize - 1), Span(kSpanDataDup, kSpanDataSize));
    ASSERT_NE(Span(kSpanData, 0), Span(kSpanDataDup, 1));
    ASSERT_NE(Span(kSpanData, 1), Span(kSpanDataDup, 0));
}

// Test indexing
TEST(SpanTest, Indexing)
{
    constexpr Span sp(kSpanData, kSpanDataSize);

    for (size_t i = 0; i < kSpanDataSize; ++i)
    {
        ASSERT_EQ(sp[i], i);
    }

    unsigned int storage[kSpanDataSize] = {};
    angle::Span<unsigned int> writableSpan(storage, kSpanDataSize);

    for (size_t i = 0; i < kSpanDataSize; ++i)
    {
        writableSpan[i] = i;
    }
    for (size_t i = 0; i < kSpanDataSize; ++i)
    {
        ASSERT_EQ(writableSpan[i], i);
    }
    for (size_t i = 0; i < kSpanDataSize; ++i)
    {
        ASSERT_EQ(storage[i], i);
    }
}

// Test for the various constructors
TEST(SpanTest, Constructors)
{
    // Default constructor
    {
        Span<const unsigned int> sp;
        static_assert(std::is_same_v<decltype(sp), Span<const unsigned int, dynamic_extent>>);
        ASSERT_TRUE(sp.size() == 0);
        ASSERT_TRUE(sp.empty());
    }

    // Constexpr construct from pointer and size
    {
        constexpr Span sp(kSpanData, kSpanDataSize);
        static_assert(std::is_same_v<decltype(sp), const Span<const unsigned int, dynamic_extent>>);
        ASSERT_EQ(sp.data(), kSpanData);
        ASSERT_EQ(sp.size(), kSpanDataSize);
        ASSERT_FALSE(sp.empty());
    }

    // Constexpr construct from fixed C-style array
    {
        constexpr Span sp(kSpanData);
        static_assert(std::is_same_v<decltype(sp), const Span<const unsigned int, kSpanDataSize>>);
        ASSERT_EQ(sp.data(), kSpanData);
        ASSERT_EQ(sp.size(), kSpanDataSize);
        ASSERT_FALSE(sp.empty());
    }

    // Constexpr construct from constexpr std::array<const>
    {
        constexpr Span sp(kSpanArr);
        static_assert(std::is_same_v<decltype(sp), const Span<const unsigned int, kSpanDataSize>>);
        ASSERT_EQ(sp.data(), kSpanArr.data());
        ASSERT_EQ(sp.size(), kSpanArr.size());
        ASSERT_FALSE(sp.empty());
    }

    // Construct from const std::array<non-const>.
    {
        const std::array<int, 2> kArr = {1, 2};
        Span sp(kArr);
        static_assert(std::is_same_v<decltype(sp), Span<const int, 2u>>);
        ASSERT_EQ(sp.data(), kArr.data());
        ASSERT_EQ(sp.size(), kArr.size());
        ASSERT_FALSE(sp.empty());
    }

    // Construct from std::array<const>.
    {
        std::array<const int, 2> arr = {1, 2};
        Span sp(arr);
        static_assert(std::is_same_v<decltype(sp), Span<const int, 2u>>);
        ASSERT_EQ(sp.data(), arr.data());
        ASSERT_EQ(sp.size(), arr.size());
        ASSERT_FALSE(sp.empty());
    }

    // Construct from std::array<non-const>.
    {
        std::array<int, 2> arr = {1, 2};
        Span sp(arr);
        static_assert(std::is_same_v<decltype(sp), Span<int, 2u>>);
        ASSERT_EQ(sp.data(), arr.data());
        ASSERT_EQ(sp.size(), arr.size());
        ASSERT_FALSE(sp.empty());
    }

    // Construct from std::vector
    {
        std::vector<unsigned int> vec({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15});
        Span sp(vec);
        static_assert(std::is_same_v<decltype(sp), Span<unsigned int, dynamic_extent>>);
        ASSERT_EQ(sp.data(), vec.data());
        ASSERT_EQ(sp.size(), vec.size());
        ASSERT_FALSE(sp.empty());
    }

    // Construct from const std::vector
    {
        const std::vector<unsigned int> vec({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15});
        Span sp(vec);
        static_assert(std::is_same_v<decltype(sp), Span<const unsigned int, dynamic_extent>>);
        ASSERT_EQ(sp.data(), vec.data());
        ASSERT_EQ(sp.size(), vec.size());
        ASSERT_FALSE(sp.empty());
    }

    // Construct from std::string
    {
        std::string str = "hooray";
        Span sp(str);
        static_assert(std::is_same_v<decltype(sp), Span<char, dynamic_extent>>);
        ASSERT_EQ(sp.data(), str.data());
        ASSERT_EQ(sp.size(), str.size());
        ASSERT_FALSE(sp.empty());
    }

    // Construct from const std::string
    {
        const std::string str = "hooray";
        Span sp(str);
        static_assert(std::is_same_v<decltype(sp), Span<const char, dynamic_extent>>);
        ASSERT_EQ(sp.data(), str.data());
        ASSERT_EQ(sp.size(), str.size());
        ASSERT_FALSE(sp.empty());
    }

    // Construct from std::string_view
    {
        std::string_view view = "hooray";
        Span sp(view);
        static_assert(std::is_same_v<decltype(sp), Span<const char, dynamic_extent>>);
        ASSERT_EQ(sp.data(), view.data());
        ASSERT_EQ(sp.size(), view.size());
        ASSERT_FALSE(sp.empty());
    }

    // Construction from any class that provides data() and size().
    {
        FakeRange range;
        Span sp(range);
        ASSERT_EQ(sp.data(), kSpanData);
        ASSERT_EQ(sp.size(), kSpanDataSize);
    }

    // Copy constructor and copy assignment
    {
        Span sp(kSpanData);
        Span sp2(sp);
        Span<const unsigned int> sp3;

        ASSERT_EQ(sp, sp2);
        ASSERT_EQ(sp2.data(), kSpanData);
        ASSERT_EQ(sp2.size(), kSpanDataSize);
        ASSERT_FALSE(sp2.empty());

        sp3 = sp;

        ASSERT_EQ(sp3, sp);
        ASSERT_EQ(sp3.data(), kSpanData);
        ASSERT_EQ(sp3.size(), kSpanDataSize);
        ASSERT_FALSE(sp3.empty());
    }
}

// Test accessing the data directly
TEST(SpanTest, DataAccess)
{
    constexpr Span sp(kSpanData, kSpanDataSize);
    const unsigned int *data = sp.data();

    for (size_t i = 0; i < kSpanDataSize; ++i)
    {
        ASSERT_EQ(data[i], i);
    }
}

// Test front and back
TEST(SpanTest, FrontAndBack)
{
    constexpr Span sp(kSpanData, kSpanDataSize);
    ASSERT_TRUE(sp.front() == 0);
    ASSERT_EQ(sp.back(), kSpanDataSize - 1);
}

// Test begin and end
TEST(SpanTest, BeginAndEnd)
{
    constexpr Span sp(kSpanData, kSpanDataSize);

    size_t currentIndex = 0;
    for (unsigned int value : sp)
    {
        ASSERT_EQ(value, currentIndex);
        ++currentIndex;
    }
}

// Test reverse begin and end
TEST(SpanTest, RbeginAndRend)
{
    constexpr Span sp(kSpanData, kSpanDataSize);

    size_t currentIndex = 0;
    for (auto iter = sp.rbegin(); iter != sp.rend(); ++iter)
    {
        ASSERT_EQ(*iter, kSpanDataSize - 1 - currentIndex);
        ++currentIndex;
    }
}

// Test first and last
TEST(SpanTest, FirstAndLast)
{
    constexpr Span sp(kSpanData, kSpanDataSize);
    constexpr size_t kSplitSize = kSpanDataSize / 4;
    {
        constexpr Span first = sp.first(kSplitSize);
        constexpr Span last  = sp.last(kSplitSize);

        static_assert(
            std::is_same_v<decltype(first), const Span<const unsigned int, dynamic_extent>>);
        ASSERT_EQ(first, Span(kSpanData, kSplitSize));
        ASSERT_EQ(first.data(), kSpanData);
        ASSERT_EQ(first.size(), kSplitSize);

        static_assert(
            std::is_same_v<decltype(last), const Span<const unsigned int, dynamic_extent>>);
        ASSERT_EQ(last, Span(kSpanData + kSpanDataSize - kSplitSize, kSplitSize));
        ASSERT_EQ(last.data(), kSpanData + kSpanDataSize - kSplitSize);
        ASSERT_EQ(last.size(), kSplitSize);
    }

    {
        constexpr Span first = sp.first<kSplitSize>();
        constexpr Span last  = sp.last<kSplitSize>();

        static_assert(std::is_same_v<decltype(first), const Span<const unsigned int, kSplitSize>>);
        ASSERT_EQ(first, Span(kSpanData, kSplitSize));
        ASSERT_EQ(first.data(), kSpanData);
        ASSERT_EQ(first.size(), kSplitSize);

        static_assert(std::is_same_v<decltype(last), const Span<const unsigned int, kSplitSize>>);
        ASSERT_EQ(last, Span(kSpanData + kSpanDataSize - kSplitSize, kSplitSize));
        ASSERT_EQ(last.data(), kSpanData + kSpanDataSize - kSplitSize);
        ASSERT_EQ(last.size(), kSplitSize);
    }
}

// Test subspan
TEST(SpanTest, Subspan)
{
    constexpr Span sp(kSpanData, kSpanDataSize);
    constexpr size_t kSplitOffset = kSpanDataSize / 4;
    constexpr size_t kSplitSize   = kSpanDataSize / 2;

    // Test one-arg subspan
    {
        constexpr Span subspan = sp.subspan(kSplitOffset);
        static_assert(
            std::is_same_v<decltype(subspan), const Span<const unsigned int, dynamic_extent>>);
        ASSERT_EQ(subspan, Span(kSpanData + kSplitOffset, kSpanDataSize - kSplitOffset));
        ASSERT_EQ(subspan.data(), kSpanData + kSplitOffset);
        ASSERT_EQ(subspan.size(), kSpanDataSize - kSplitOffset);
    }

    // Test two-arg subspan
    {
        constexpr Span subspan = sp.subspan(kSplitOffset, kSplitSize);
        static_assert(
            std::is_same_v<decltype(subspan), const Span<const unsigned int, dynamic_extent>>);
        ASSERT_EQ(subspan, Span(kSpanData + kSplitOffset, kSplitSize));
        ASSERT_EQ(subspan.data(), kSpanData + kSplitOffset);
        ASSERT_EQ(subspan.size(), kSplitSize);
    }

    // Templated one-arg subspan just sugar until C++20 fixed spans.
    {
        constexpr Span subspan = sp.subspan<kSplitOffset>();
        static_assert(
            std::is_same_v<decltype(subspan), const Span<const unsigned int, dynamic_extent>>);
        ASSERT_EQ(subspan, Span(kSpanData + kSplitOffset, kSpanDataSize - kSplitOffset));
        ASSERT_EQ(subspan.data(), kSpanData + kSplitOffset);
        ASSERT_EQ(subspan.size(), kSpanDataSize - kSplitOffset);
    }

    // Templated two-arg subspan just sugar until C++20 fixed spans.
    {
        constexpr Span subspan = sp.subspan<kSplitOffset, kSplitSize>();
        static_assert(
            std::is_same_v<decltype(subspan), const Span<const unsigned int, dynamic_extent>>);
        ASSERT_EQ(subspan, Span(kSpanData + kSplitOffset, kSplitSize));
        ASSERT_EQ(subspan.data(), kSpanData + kSplitOffset);
        ASSERT_EQ(subspan.size(), kSplitSize);
    }
}

// Test conversions from non-const to const spans.
TEST(SpanTest, ConstConversions)
{
    const unsigned int kStorage[kSpanDataSize] = {0, 1, 2,  3,  4,  5,  6,  7,
                                                  8, 9, 10, 11, 12, 13, 14, 15};
    unsigned int storage[kSpanDataSize] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    angle::Span readable_span(kStorage, kSpanDataSize);
    angle::Span writable_span(storage, kSpanDataSize);

    static_assert(
        std::is_same_v<decltype(readable_span), Span<const unsigned int, dynamic_extent>>);

    static_assert(std::is_same_v<decltype(writable_span), Span<unsigned int, dynamic_extent>>);

    // Direct comparisons allowed.
    EXPECT_TRUE(readable_span == writable_span);
    EXPECT_FALSE(readable_span != writable_span);

    writable_span[0] = 1234;
    EXPECT_FALSE(readable_span == writable_span);

    // Assignment allowed from non-const to const.
    readable_span = writable_span;
    EXPECT_TRUE(readable_span == writable_span);
}

// Test conversions from fixed to dynamic spans.
TEST(SpanTest, FixedConversions)
{
    unsigned int storage[kSpanDataSize] = {};
    std::vector<unsigned int> vec(kSpanDataSize, 42);
    Span<unsigned int> dynamic_span(vec);
    Span<unsigned int, kSpanDataSize> static_span(storage);

    // Direct comparisons allowed.
    EXPECT_NE(static_span, dynamic_span);

    // Assignment allowed from fixed to dynamic.
    dynamic_span = static_span;
    EXPECT_EQ(static_span, dynamic_span);

    // Other way around prohibited without fixed conversion.
    dynamic_span[0] = 1234;
    static_span     = dynamic_span.first<kSpanDataSize>();
    EXPECT_EQ(static_span, dynamic_span);
}

// Test non-member functions.
TEST(SpanTest, Helpers)
{
    // Test as_bytes.
    {
        constexpr int kArray[] = {2, 3, 5, 7, 11, 13};
        auto bytes_span        = as_bytes(Span(kArray));
        EXPECT_EQ(reinterpret_cast<const uint8_t *>(kArray), bytes_span.data());
        EXPECT_EQ(sizeof(kArray), bytes_span.size());
        EXPECT_EQ(bytes_span.size(), bytes_span.size_bytes());
    }

    // Test as_writble_bytes.
    {
        std::vector<int> vec = {1, 1, 2, 3, 5, 8};
        Span<int> mutable_span(vec);
        auto writable_bytes_span = as_writable_bytes(mutable_span);
        static_assert(std::is_same_v<decltype(writable_bytes_span), Span<uint8_t>>);
        EXPECT_EQ(reinterpret_cast<uint8_t *>(vec.data()), writable_bytes_span.data());
        EXPECT_EQ(sizeof(int) * vec.size(), writable_bytes_span.size());
        EXPECT_EQ(writable_bytes_span.size(), writable_bytes_span.size_bytes());
    }

    // Test as_chars.
    {
        constexpr int kArray[] = {2, 3, 5, 7, 11, 13};
        auto chars_span        = as_chars(Span(kArray));
        EXPECT_EQ(reinterpret_cast<const char *>(kArray), chars_span.data());
        EXPECT_EQ(sizeof(kArray), chars_span.size());
        EXPECT_EQ(chars_span.size(), chars_span.size_bytes());
    }

    // Test as writable chars.
    {
        std::vector<int> vec = {1, 1, 2, 3, 5, 8};
        Span<int> mutable_span(vec);
        auto writable_chars_span = as_writable_chars(mutable_span);
        static_assert(std::is_same_v<decltype(writable_chars_span), Span<char>>);
        EXPECT_EQ(reinterpret_cast<char *>(vec.data()), writable_chars_span.data());
        EXPECT_EQ(sizeof(int) * vec.size(), writable_chars_span.size());
        EXPECT_EQ(writable_chars_span.size(), writable_chars_span.size_bytes());
    }

    // Test span_from_ref.
    {
        int x  = 123;
        auto s = span_from_ref(x);
        EXPECT_EQ(&x, s.data());
        EXPECT_EQ(1u, s.size());
        EXPECT_EQ(sizeof(int), s.size_bytes());
        EXPECT_EQ(123, s[0]);
    }

    // Test byte_span_from_ref.
    {
        int x  = 123;
        auto b = byte_span_from_ref(x);
        EXPECT_EQ(reinterpret_cast<uint8_t *>(&x), b.data());
        EXPECT_EQ(sizeof(int), b.size());
    }

    //  Test as_byte_span.
    {
        const std::vector<int> kVec({2, 3, 5, 7, 11, 13});
        auto byte_span = as_byte_span(kVec);
        static_assert(std::is_same_v<decltype(byte_span), Span<const uint8_t>>);
        EXPECT_EQ(byte_span.data(), reinterpret_cast<const uint8_t *>(kVec.data()));
        EXPECT_EQ(byte_span.size(), kVec.size() * sizeof(int));
    }

    // Test as_writable_byte_span
    {
        int kMutArray[] = {2, 3, 5, 7};
        auto byte_span  = as_writable_byte_span(kMutArray);
        EXPECT_EQ(byte_span.data(), reinterpret_cast<uint8_t *>(kMutArray));
        EXPECT_EQ(byte_span.size(), sizeof(kMutArray));
    }
}

}  // anonymous namespace
}  // namespace angle
