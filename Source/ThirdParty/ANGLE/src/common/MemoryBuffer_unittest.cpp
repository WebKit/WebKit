//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for ANGLE's MemoryBuffer class.
//

#include "common/MemoryBuffer.h"

#include <array>

#include "common/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace angle;

namespace
{

// Test usage of MemoryBuffer with multiple resizes
TEST(MemoryBuffer, MultipleResizes)
{
    MemoryBuffer buffer;

    ASSERT_TRUE(buffer.resize(100));
    ASSERT_EQ(buffer.size(), 100u);
    buffer.assertTotalAllocatedBytes(100u);
    buffer.assertTotalCopiedBytes(0u);

    ASSERT_TRUE(buffer.resize(300));
    ASSERT_EQ(buffer.size(), 300u);
    buffer.assertTotalAllocatedBytes(400u);
    buffer.assertTotalCopiedBytes(100u);

    ASSERT_TRUE(buffer.resize(100));
    ASSERT_EQ(buffer.size(), 100u);
    buffer.assertTotalAllocatedBytes(400u);
    buffer.assertTotalCopiedBytes(100u);

    ASSERT_TRUE(buffer.resize(400));
    ASSERT_EQ(buffer.size(), 400u);
    buffer.assertTotalAllocatedBytes(800u);
    buffer.assertTotalCopiedBytes(200u);
}

// Test usage of MemoryBuffer with reserve and then multiple resizes
TEST(MemoryBuffer, ReserveThenResize)
{
    MemoryBuffer buffer;

    ASSERT_TRUE(buffer.reserve(300));
    ASSERT_EQ(buffer.size(), 0u);

    ASSERT_TRUE(buffer.resize(100));
    ASSERT_EQ(buffer.size(), 100u);
    buffer.assertTotalAllocatedBytes(300u);
    buffer.assertTotalCopiedBytes(0u);

    ASSERT_TRUE(buffer.resize(300));
    ASSERT_EQ(buffer.size(), 300u);
    buffer.assertTotalAllocatedBytes(300u);
    buffer.assertTotalCopiedBytes(0u);

    ASSERT_TRUE(buffer.resize(100));
    ASSERT_EQ(buffer.size(), 100u);
    buffer.assertTotalAllocatedBytes(300u);
    buffer.assertTotalCopiedBytes(0u);

    ASSERT_TRUE(buffer.resize(400));
    ASSERT_EQ(buffer.size(), 400u);
    buffer.assertTotalAllocatedBytes(700u);
    buffer.assertTotalCopiedBytes(100u);
}

// Test that clear() of a memory buffer retains the buffer.
TEST(MemoryBuffer, Clear)
{
    MemoryBuffer buffer;
    ASSERT_TRUE(buffer.resize(100));
    ASSERT_EQ(buffer.size(), 100u);
    ASSERT_NE(buffer.data(), nullptr);
    buffer.assertTotalAllocatedBytes(100u);
    buffer.assertTotalCopiedBytes(0u);

    uint8_t *oldPtr = buffer.data();

    buffer.clear();
    EXPECT_EQ(buffer.size(), 0u);
    EXPECT_EQ(buffer.data(), oldPtr);
    buffer.assertTotalAllocatedBytes(100u);
    buffer.assertTotalCopiedBytes(0u);

    ASSERT_TRUE(buffer.resize(100));
    ASSERT_EQ(buffer.size(), 100u);
    EXPECT_EQ(buffer.data(), oldPtr);
    buffer.assertTotalAllocatedBytes(100u);
    buffer.assertTotalCopiedBytes(0u);
}

// Test that destroy() of a memory buffer does not retain the buffer.
// Test destroying MemoryBuffer
TEST(MemoryBuffer, Destroy)
{
    MemoryBuffer buffer;
    ASSERT_TRUE(buffer.resize(100));
    ASSERT_EQ(buffer.size(), 100u);
    ASSERT_NE(buffer.data(), nullptr);
    buffer.assertTotalAllocatedBytes(100u);
    buffer.assertTotalCopiedBytes(0u);

    buffer.destroy();
    EXPECT_EQ(buffer.size(), 0u);
    buffer.assertDataBufferFreed();
    buffer.assertTotalAllocatedBytes(0u);
    buffer.assertTotalCopiedBytes(0u);

    ASSERT_TRUE(buffer.resize(100));
    ASSERT_EQ(buffer.size(), 100u);
    ASSERT_NE(buffer.data(), nullptr);
    buffer.assertTotalAllocatedBytes(100u);
    buffer.assertTotalCopiedBytes(0u);
}

// Test usage of MemoryBuffer with clearAndReserve() and then multiple resizes.
TEST(MemoryBuffer, ClearAndReserve)
{
    MemoryBuffer buffer;
    ASSERT_TRUE(buffer.resize(200));
    ASSERT_EQ(buffer.size(), 200u);
    ASSERT_NE(buffer.data(), nullptr);
    buffer.assertTotalAllocatedBytes(200u);
    buffer.assertTotalCopiedBytes(0u);

    uint8_t *oldPtr = buffer.data();

    ASSERT_TRUE(buffer.clearAndReserve(100));
    ASSERT_EQ(buffer.size(), 0u);
    EXPECT_EQ(buffer.data(), oldPtr);
    buffer.assertTotalAllocatedBytes(200u);
    buffer.assertTotalCopiedBytes(0u);

    ASSERT_TRUE(buffer.resize(200));
    ASSERT_EQ(buffer.size(), 200u);
    EXPECT_EQ(buffer.data(), oldPtr);
    buffer.assertTotalAllocatedBytes(200u);
    buffer.assertTotalCopiedBytes(0u);

    ASSERT_TRUE(buffer.resize(300));
    ASSERT_EQ(buffer.size(), 300u);
    EXPECT_NE(buffer.data(), oldPtr);
    buffer.assertTotalAllocatedBytes(500u);
    buffer.assertTotalCopiedBytes(200u);
}

// Test that the span() method returns entire buffer.
TEST(MemoryBuffer, Span)
{
    MemoryBuffer buf;
    {
        Span<uint8_t> s = buf.span();
        EXPECT_EQ(s.size(), 0u);
        EXPECT_EQ(s.data(), nullptr);
    }
    ASSERT_TRUE(buf.resize(2u));
    {
        Span<uint8_t> s = buf.span();
        EXPECT_EQ(s.size(), 2u);
        EXPECT_EQ(s.data(), buf.data());
    }
}

// Test that the subspan() method returns correct portion of buffer.
TEST(MemoryBuffer, Subspan)
{
    MemoryBuffer buf;
    {
        Span<uint8_t> s = buf.subspan(0);
        EXPECT_EQ(s.size(), 0u);
        EXPECT_EQ(s.data(), nullptr);
    }
    {
        Span<uint8_t> s = buf.subspan(0, 0);
        EXPECT_EQ(s.size(), 0u);
        EXPECT_EQ(s.data(), nullptr);
    }
    ASSERT_TRUE(buf.resize(4u));
    for (size_t i = 0; i < buf.size(); ++i)
    {
        buf[i] = i;
    }
    {
        Span<uint8_t> s = buf.subspan(0, 0);
        EXPECT_EQ(s.size(), 0u);
    }
    {
        Span<uint8_t> s = buf.subspan(2, 0);
        EXPECT_THAT(s.size(), 0u);
    }
    {
        Span<uint8_t> s = buf.subspan(0, 1);
        EXPECT_THAT(s, testing::ElementsAre(0u));
    }
    {
        Span<uint8_t> s = buf.subspan(1, 2);
        EXPECT_THAT(s, testing::ElementsAre(1u, 2u));
    }
    {
        Span<uint8_t> s = buf.subspan(3);
        EXPECT_THAT(s, testing::ElementsAre(3u));
    }
    {
        Span<uint8_t> s = buf.subspan(4);
        EXPECT_THAT(s.size(), 0u);
    }
}

// Test that the first() method returns correct portion of buffer.
TEST(MemoryBuffer, First)
{
    MemoryBuffer buf;
    {
        Span<uint8_t> s = buf.first(0);
        EXPECT_EQ(s.size(), 0u);
        EXPECT_EQ(s.data(), nullptr);
    }
    ASSERT_TRUE(buf.resize(4u));
    for (size_t i = 0; i < buf.size(); ++i)
    {
        buf[i] = i;
    }
    {
        Span<uint8_t> s = buf.first(0);
        EXPECT_EQ(s.size(), 0u);
    }
    {
        Span<uint8_t> s = buf.first(2u);
        EXPECT_THAT(s, testing::ElementsAre(0u, 1u));
    }
}

// Test that the last() method returns correct portion of buffer.
TEST(MemoryBuffer, Last)
{
    MemoryBuffer buf;
    {
        Span<uint8_t> s = buf.last(0);
        EXPECT_EQ(s.size(), 0u);
        EXPECT_EQ(s.data(), nullptr);
    }
    ASSERT_TRUE(buf.resize(4u));
    for (size_t i = 0; i < buf.size(); ++i)
    {
        buf[i] = i;
    }
    {
        Span<uint8_t> s = buf.last(0);
        EXPECT_EQ(s.size(), 0u);
    }
    {
        Span<uint8_t> s = buf.last(2u);
        EXPECT_THAT(s, testing::ElementsAre(2u, 3u));
    }
}

// Test that filling a memory buffer writes the expected value.
TEST(MemoryBuffer, Fill)
{
    MemoryBuffer buf;

    // Test fill is a no-op on an empty buffer.
    buf.fill(0x41);
    EXPECT_TRUE(buf.empty());

    ASSERT_TRUE(buf.resize(2));
    buf.fill(0x41);
    EXPECT_EQ(0x41u, buf[0]);
    EXPECT_EQ(0x41u, buf[1]);
}

// Demonstrate current behavior of ScratchBuffer lifetime mechanism
TEST(ScratchBuffer, Lifetime)
{
    ScratchBuffer scratch(2);  // Live for two ticks.
    MemoryBuffer *out;

    ASSERT_TRUE(scratch.get(100u, &out));
    ASSERT_NE(out, nullptr);
    ASSERT_NE(out->data(), nullptr);

    uint8_t *oldPtr = out->data();

    scratch.tick();
    EXPECT_EQ(out->data(), oldPtr);

    scratch.tick();
    out->assertDataBufferFreed();

    scratch.tick();
    out->assertDataBufferFreed();
}

// Test that an initial lifetime of zero means it never expires.
TEST(ScratchBuffer, EternalLifetime)
{
    ScratchBuffer scratch(0);
    MemoryBuffer *out;

    ASSERT_TRUE(scratch.get(100u, &out));
    ASSERT_NE(out, nullptr);
    ASSERT_NE(out->data(), nullptr);

    uint8_t *oldPtr = out->data();

    scratch.tick();
    EXPECT_EQ(out->data(), oldPtr);

    scratch.tick();
    EXPECT_EQ(out->data(), oldPtr);

    scratch.tick();
    EXPECT_EQ(out->data(), oldPtr);

    scratch.tick();
    EXPECT_EQ(out->data(), oldPtr);
}

}  // namespace
