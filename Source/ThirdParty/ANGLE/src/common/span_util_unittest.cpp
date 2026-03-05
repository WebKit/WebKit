// Copyright 2025 The ANGLE Project Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "common/span_util.h"

#include <array>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace angle
{
namespace
{

// Test that SpanMemcpy() can copy two same-size spans.
TEST(SpanMemcpy, FitsEntirely)
{
    std::vector<char> src(4, 'A');
    std::vector<char> dst(4, 'B');
    angle::SpanMemcpy(angle::Span(dst), angle::Span(src));
    EXPECT_EQ(dst[0], 'A');
    EXPECT_EQ(dst[1], 'A');
    EXPECT_EQ(dst[2], 'A');
    EXPECT_EQ(dst[3], 'A');
}

// Test that SpanMemcpy() can copy a smaller span into a larger one.
TEST(SpanMemcpy, FitsWithin)
{
    std::vector<char> src(2, 'A');
    std::vector<char> dst(4, 'B');
    // Also show that a const src argument is acceptable.
    angle::SpanMemcpy(angle::Span(dst).subspan<1u>(), angle::Span<const char>(src));
    EXPECT_EQ(dst[0], 'B');
    EXPECT_EQ(dst[1], 'A');
    EXPECT_EQ(dst[2], 'A');
    EXPECT_EQ(dst[3], 'B');
}

// Test that SpanMemcpy() tolerates empty src arguments.
TEST(SpanMemcpy, EmptyCopyWithin)
{
    std::vector<char> src;
    std::vector<char> dst(4, 'B');
    angle::SpanMemcpy(angle::Span(dst).subspan<1u>(), angle::Span(src));
    EXPECT_EQ(dst[0], 'B');
    EXPECT_EQ(dst[1], 'B');
    EXPECT_EQ(dst[2], 'B');
    EXPECT_EQ(dst[3], 'B');
}

// Test that SpanMemcpy() tolerates copies between two empty spans.
TEST(SpanMemcpy, EmptyCopyToEmpty)
{
    std::vector<char> src;
    std::vector<char> dst;
    angle::SpanMemcpy(angle::Span(dst), angle::Span(src));
}

// Test that SpanMemmove() actually moves characters.
TEST(SpanMemmove, FitsWithin)
{
    std::vector<char> src(2, 'A');
    std::vector<char> dst(4, 'B');
    // Also show that a const src argument is acceptable.
    angle::SpanMemmove(angle::Span(dst).subspan<1u>(), angle::Span<const char>(src));
    EXPECT_EQ(dst[0], 'B');
    EXPECT_EQ(dst[1], 'A');
    EXPECT_EQ(dst[2], 'A');
    EXPECT_EQ(dst[3], 'B');
}

// Test that SpanMemmove() tolerates empty src arguments.
TEST(SpanMemmove, EmptyCopyWithin)
{
    std::vector<char> src;
    std::vector<char> dst(4, 'B');
    angle::SpanMemmove(angle::Span(dst).subspan<1u>(), angle::Span(src));
    EXPECT_EQ(dst[0], 'B');
    EXPECT_EQ(dst[1], 'B');
    EXPECT_EQ(dst[2], 'B');
    EXPECT_EQ(dst[3], 'B');
}

// Test that SpanMemmove() tolerates copies between two empty spans.
TEST(SpanMemmove, EmptyCopyToEmpty)
{
    std::vector<char> src;
    std::vector<char> dst;
    angle::SpanMemmove(angle::Span(dst), angle::Span(src));
}

// Test that SpanMemset() tolerates setting into an empty span.
TEST(SpanMemset, AllowsEmpty)
{
    angle::Span<unsigned int> empty;
    angle::SpanMemset(empty, 0xff);
}

// Test that SpanMemset() writes all the bytes of a larger type.
TEST(SpanMemset, WritesAll)
{
    std::vector<uint32_t> dst(2);
    angle::SpanMemset(angle::Span(dst), 0xff);
    EXPECT_EQ(dst[0], 0xffffffff);
    EXPECT_EQ(dst[1], 0xffffffff);
}

}  // namespace
}  // namespace angle
