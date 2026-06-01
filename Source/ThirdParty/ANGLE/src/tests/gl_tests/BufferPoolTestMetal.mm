//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BufferPoolTestMetal.mm:
//   White box tests for Metal BufferPool allocation logic, specifically for testing
//   size_t overflow scenario.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

#include "common/apple_platform_utils.h"
#include "common/mathutil.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"
#include "libANGLE/renderer/metal/mtl_buffer_pool.h"

using namespace rx;
using namespace angle;

namespace
{

class BufferPoolTest : public ANGLETest<>
{
  protected:
    BufferPoolTest()
    {
        setWindowWidth(1);
        setWindowHeight(1);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    ContextMtl *getContextMtl()
    {
        // Get the context through the display, similar to D3D11 white box tests
        egl::Display *display   = static_cast<egl::Display *>(getEGLWindow()->getDisplay());
        gl::ContextID contextID = {
            static_cast<GLuint>(reinterpret_cast<uintptr_t>(getEGLWindow()->getContext()))};
        gl::Context *context = display->getContext(contextID);
        return mtl::GetImpl(context);
    }
};

// Test that BufferPool correctly handles allocations with size_t offsets > UINT32_MAX
TEST_P(BufferPoolTest, AllocationOffsetNoTruncation)
{
    ANGLE_SKIP_TEST_IF(!IsMetalRendererAvailable());

    ContextMtl *contextMtl = getContextMtl();
    ASSERT_NE(contextMtl, nullptr);

    mtl::BufferPool bufferPool;

    // Use a large buffer size that will cause offset calculations to exceed UINT32_MAX
    // when aligned.
    constexpr size_t kLargeSize = 0xFFFFFFFF;  // UINT32_MAX + 512 (4GB bytes)
    constexpr size_t kAlignment = 256;         // Typical Metal buffer alignment
    constexpr size_t kSmallSize = 1024;        // Second allocation size

    // Initialize buffer pool with large initial size
    bufferPool.initialize(contextMtl, kLargeSize, kAlignment, 10);

    // Perform first allocation
    uint8_t *ptr1       = nullptr;
    mtl::BufferRef buf1 = nullptr;
    size_t offset1      = 0;
    bool newBuffer1     = false;

    ASSERT_EQ(bufferPool.allocate(contextMtl, kLargeSize, &ptr1, &buf1, &offset1, &newBuffer1),
              angle::Result::Continue);
    EXPECT_TRUE(newBuffer1);
    EXPECT_EQ(offset1, 0u);
    EXPECT_NE(ptr1, nullptr);
    EXPECT_NE(buf1, nullptr);

    // Fill first allocation with a known pattern (0xAA)
    // We only fill the first 4KB to avoid spending too much time on this test
    constexpr size_t kPatternSize = 4096;
    memset(ptr1, 0xAA, kPatternSize);

    // Commit the first allocation to ensure it's written to the buffer
    ASSERT_EQ(bufferPool.commit(contextMtl), angle::Result::Continue);

    // Perform second allocation
    uint8_t *ptr2       = nullptr;
    mtl::BufferRef buf2 = nullptr;
    size_t offset2      = 0;
    bool newBuffer2     = false;

    ASSERT_EQ(bufferPool.allocate(contextMtl, kSmallSize, &ptr2, &buf2, &offset2, &newBuffer2),
              angle::Result::Continue);

    // With the fix (size_t), a new buffer should be allocated since the calculated offset
    // exceeds the buffer size. Otherwise (no fix), the offset would truncate and
    // potentially reuse the same buffer incorrectly, causing memory corruption.
    EXPECT_TRUE(newBuffer2);

    // The offset should be 0 in the new buffer (not a truncated large value)
    EXPECT_EQ(offset2, 0u);
    EXPECT_NE(ptr2, nullptr);
    EXPECT_NE(buf2, nullptr);

    // Buffers should be different
    EXPECT_NE(buf1.get(), buf2.get());

    // Fill second allocation with a different pattern (0xBB)
    memset(ptr2, 0xBB, kSmallSize);

    // Commit the second allocation
    ASSERT_EQ(bufferPool.commit(contextMtl), angle::Result::Continue);

    // Now verify that the first buffer's data wasn't corrupted
    // With the uint32_t bug, ptr2 would have overwritten ptr1's data at offset 0
    // because the offset wrapped around to 0 or a small value.
    // Map the first buffer again to verify its contents
    angle::Span<const uint8_t> verifyPtr1 = buf1->mapReadOnly(contextMtl);
    ASSERT_FALSE(verifyPtr1.empty());

    // Check that the first pattern (0xAA) is still intact
    // If the bug exists, this would have been overwritten with 0xBB
    for (size_t i = 0; i < kSmallSize && i < kPatternSize; ++i)
    {
        EXPECT_EQ(verifyPtr1[i], 0xAA)
            << "First buffer corrupted at byte " << i
            << " - uint32_t truncation bug likely caused second allocation to overlap!";
    }

    buf1->unmap(contextMtl);
    bufferPool.destroy(contextMtl);
}

}  // namespace

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(BufferPoolTest);
ANGLE_INSTANTIATE_TEST(BufferPoolTest, ES2_METAL(), ES3_METAL());
