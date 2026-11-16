//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BufferPoolTestMetal.mm:
//   White box tests for Metal BufferPool allocation logic, specifically for testing
//   size_t overflow scenario.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

#include "common/apple_platform_utils.h"
#include "common/mathutil.h"
#include "common/unsafe_buffers.h"
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

    // http://anglebug.com/500280351
    ANGLE_SKIP_TEST_IF(IsIOS());

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
    angle::Span<uint8_t> mappedData1;
    mtl::BufferSlice slice1;

    ASSERT_EQ(bufferPool.allocateAndMap(contextMtl, kLargeSize, &mappedData1, &slice1),
              angle::Result::Continue);
    EXPECT_EQ(slice1.offset(), 0u);
    EXPECT_FALSE(mappedData1.empty());
    EXPECT_NE(slice1.buffer(), nullptr);

    // Fill first allocation with a known pattern (0xAA)
    // We only fill the first 4KB to avoid spending too much time on this test
    constexpr size_t kPatternSize = 4096;
    ANGLE_UNSAFE_TODO(memset(mappedData1.data(), 0xAA, kPatternSize));

    // Commit the first allocation to ensure it's written to the buffer
    ASSERT_EQ(bufferPool.commit(contextMtl), angle::Result::Continue);

    // Perform second allocation
    angle::Span<uint8_t> mappedData2;
    mtl::BufferSlice slice2;

    ASSERT_EQ(bufferPool.allocateAndMap(contextMtl, kSmallSize, &mappedData2, &slice2),
              angle::Result::Continue);

    // With the fix (size_t), a new buffer should be allocated since the calculated offset
    // exceeds the buffer size. Otherwise (no fix), the offset would truncate and
    // potentially reuse the same buffer incorrectly, causing memory corruption.
    // The offset should be 0 in the new buffer (not a truncated large value)
    EXPECT_EQ(slice2.offset(), 0u);
    EXPECT_FALSE(mappedData2.empty());
    EXPECT_NE(slice2.buffer(), nullptr);

    // Buffers should be different
    EXPECT_NE(slice1.buffer().get(), slice2.buffer().get());

    // Fill second allocation with a different pattern (0xBB)
    ANGLE_UNSAFE_TODO(memset(mappedData2.data(), 0xBB, kSmallSize));

    // Commit the second allocation
    ASSERT_EQ(bufferPool.commit(contextMtl), angle::Result::Continue);

    // Now verify that the first buffer's data wasn't corrupted
    // With the uint32_t bug, ptr2 would have overwritten ptr1's data at offset 0
    // because the offset wrapped around to 0 or a small value.
    // Map the first buffer again to verify its contents
    angle::Span<const uint8_t> verifyPtr1 = slice1.buffer()->mapReadOnly(contextMtl);
    ASSERT_FALSE(verifyPtr1.empty());

    // Check that the first pattern (0xAA) is still intact
    // If the bug exists, this would have been overwritten with 0xBB
    for (size_t i = 0; i < kSmallSize && i < kPatternSize; ++i)
    {
        EXPECT_EQ(verifyPtr1[i], 0xAA)
            << "First buffer corrupted at byte " << i
            << " - uint32_t truncation bug likely caused second allocation to overlap!";
    }

    slice1.buffer()->unmap(contextMtl);
    bufferPool.destroy(contextMtl);
}

// Test that:
// 1. glFinish() does not trim BufferPool buffers even after GPU completion.
// 2. glTrimMemoryANGLE() marks idle buffers Volatile while skipping buffers (including active
//    mBuffer) still in-use by the GPU without stalling on uncommitted GPU work.
// 3. Once GPU work finishes, a subsequent glTrimMemoryANGLE() sweeps the newly-idle buffers
//    (including active mBuffer) in registered pools to Volatile.
TEST_P(BufferPoolTest, PurgeableBufferPoolTrimMemory)
{
    ANGLE_SKIP_TEST_IF(!IsMetalRendererAvailable());
    ASSERT_TRUE(EnsureGLExtensionEnabled("GL_ANGLE_trim_memory"));

    glTrimMemoryANGLE(0);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    ContextMtl *contextMtl = getContextMtl();
    ASSERT_NE(contextMtl, nullptr);

    const bool purgeableEnabled =
        contextMtl->getDisplay()->getFeatures().purgeableBufferPool.enabled;
    const MTLPurgeableState expectedPurgedState =
        purgeableEnabled ? MTLPurgeableStateVolatile : MTLPurgeableStateNonVolatile;

    constexpr size_t kBufferSize = 64 * 1024;
    constexpr size_t kAlignment  = 256;

    mtl::BufferPool bufferPool;
    bufferPool.initialize(contextMtl, kBufferSize, kAlignment, 10, mtl::TrackBufferInContext::Yes);

    // 1. Allocate slice1, slice2, and slice3 so slice1 and slice2 are in mInFlightBuffers and
    // slice3 is the active mBuffer.
    mtl::BufferSlice slice1;
    ASSERT_EQ(bufferPool.allocate(contextMtl, kBufferSize, &slice1), angle::Result::Continue);
    contextMtl->markResourceWrittenByCommandBuffer(slice1.buffer());

    mtl::BufferSlice slice2;
    ASSERT_EQ(bufferPool.allocate(contextMtl, kBufferSize, &slice2), angle::Result::Continue);
    mtl::BufferSlice slice3;
    ASSERT_EQ(bufferPool.allocate(contextMtl, kBufferSize / 2, &slice3), angle::Result::Continue);

    // 2. Call glFinish() to wait for slice1's command buffer to finish. Because trimming only
    // runs on glTrimMemoryANGLE(), slice1 must remain NonVolatile.
    glFinish();
    EXPECT_FALSE(slice1.buffer()->isBeingUsedByGPU(contextMtl));
    EXPECT_EQ([slice1.buffer()->get() setPurgeableState:MTLPurgeableStateKeepCurrent],
              MTLPurgeableStateNonVolatile);

    // 3. Mark slice2 and active mBuffer (slice3) as written by a new uncommitted command buffer
    // and call glTrimMemoryANGLE(): idle slice1 becomes Volatile (when enabled), while in-use
    // slice2 and active in-use slice3 must remain NonVolatile without stalling the GPU.
    contextMtl->markResourceWrittenByCommandBuffer(slice2.buffer());
    contextMtl->markResourceWrittenByCommandBuffer(slice3.buffer());
    glTrimMemoryANGLE(GL_MEMORY_TRIM_LOW_ANGLE);
    EXPECT_GL_NO_ERROR();
    EXPECT_TRUE(slice2.buffer()->isBeingUsedByGPU(contextMtl));
    EXPECT_TRUE(slice3.buffer()->isBeingUsedByGPU(contextMtl));
    EXPECT_EQ([slice1.buffer()->get() setPurgeableState:MTLPurgeableStateKeepCurrent],
              expectedPurgedState);
    EXPECT_EQ([slice2.buffer()->get() setPurgeableState:MTLPurgeableStateKeepCurrent],
              MTLPurgeableStateNonVolatile);
    EXPECT_EQ([slice3.buffer()->get() setPurgeableState:MTLPurgeableStateKeepCurrent],
              MTLPurgeableStateNonVolatile);

    // 4. Finish GPU work via glFinish() (slice2 and slice3 remain NonVolatile after glFinish()),
    // then call glTrimMemoryANGLE() so both slice2 and slice3 (mBuffer) now become Volatile.
    glFinish();
    EXPECT_FALSE(slice2.buffer()->isBeingUsedByGPU(contextMtl));
    EXPECT_FALSE(slice3.buffer()->isBeingUsedByGPU(contextMtl));
    EXPECT_EQ([slice2.buffer()->get() setPurgeableState:MTLPurgeableStateKeepCurrent],
              MTLPurgeableStateNonVolatile);
    EXPECT_EQ([slice3.buffer()->get() setPurgeableState:MTLPurgeableStateKeepCurrent],
              MTLPurgeableStateNonVolatile);

    glTrimMemoryANGLE(GL_MEMORY_TRIM_HIGH_ANGLE);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ([slice2.buffer()->get() setPurgeableState:MTLPurgeableStateKeepCurrent],
              expectedPurgedState);
    EXPECT_EQ([slice3.buffer()->get() setPurgeableState:MTLPurgeableStateKeepCurrent],
              expectedPurgedState);

    bufferPool.destroy(contextMtl);
}

// Test that after glTrimMemoryANGLE() moves idle buffers to mVolatileBufferFreeList as Volatile:
// 1. Volatile buffers are reclaimed in LIFO order (most-recently used at back() first).
// 2. NonVolatile buffers (including those released while in-use by the GPU that subsequently
//    finish) are reused before Volatile buffers in mVolatileBufferFreeList.
// 3. When Volatile buffers are popped from mVolatileBufferFreeList, OS-evicted
//    (MTLPurgeableStateEmpty) buffers are discarded and remaining Volatile buffers are restored to
//    NonVolatile.
TEST_P(BufferPoolTest, PurgeableBufferPoolReclaimAndEviction)
{
    ANGLE_SKIP_TEST_IF(!IsMetalRendererAvailable());
    ASSERT_TRUE(EnsureGLExtensionEnabled("GL_ANGLE_trim_memory"));

    ContextMtl *contextMtl = getContextMtl();
    ASSERT_NE(contextMtl, nullptr);

    if (!contextMtl->getDisplay()->getFeatures().purgeableBufferPool.enabled)
    {
        GTEST_SKIP() << "Test skipped because purgeableBufferPool feature is disabled.";
    }

    mtl::BufferPool bufferPool;
    constexpr size_t kBufferSize = 64 * 1024;
    constexpr size_t kAlignment  = 256;
    constexpr size_t kMaxBuffers = 3;

    bufferPool.initialize(contextMtl, kBufferSize, kAlignment, kMaxBuffers,
                          mtl::TrackBufferInContext::Yes);

    // 1. Allocate slice1, slice2, and slice3 (reaching kMaxBuffers = 3).
    mtl::BufferSlice slice1;
    ASSERT_EQ(bufferPool.allocate(contextMtl, kBufferSize, &slice1), angle::Result::Continue);
    mtl::BufferSlice slice2;
    ASSERT_EQ(bufferPool.allocate(contextMtl, kBufferSize, &slice2), angle::Result::Continue);
    mtl::BufferSlice slice3;
    ASSERT_EQ(bufferPool.allocate(contextMtl, kBufferSize, &slice3), angle::Result::Continue);

    // 2. Call glTrimMemoryANGLE(): slice1, slice2, and slice3 move to mVolatileBufferFreeList as
    // Volatile in oldest-to-newest order ([slice1, slice2, slice3] with slice3 at back()).
    glTrimMemoryANGLE(GL_MEMORY_TRIM_HIGH_ANGLE);
    EXPECT_EQ([slice1.buffer()->get() setPurgeableState:MTLPurgeableStateKeepCurrent],
              MTLPurgeableStateVolatile);
    EXPECT_EQ([slice2.buffer()->get() setPurgeableState:MTLPurgeableStateKeepCurrent],
              MTLPurgeableStateVolatile);
    EXPECT_EQ([slice3.buffer()->get() setPurgeableState:MTLPurgeableStateKeepCurrent],
              MTLPurgeableStateVolatile);

    // 3. Allocate slice4 (which reclaims slice3 from the back of mVolatileBufferFreeList as
    // NonVolatile), mark slice3 as in-use by the GPU, and allocate slice5 (which moves slice3 to
    // mInFlightBuffers and reclaims slice2 from the back as NonVolatile). Calling
    // releaseInFlightBuffers() while slice3 is still in-use by the GPU places slice3 in
    // mBufferFreeList while slice1 remains in mVolatileBufferFreeList.
    mtl::BufferSlice slice4;
    ASSERT_EQ(bufferPool.allocate(contextMtl, kBufferSize, &slice4), angle::Result::Continue);
    EXPECT_EQ(slice4.buffer().get(), slice3.buffer().get());
    EXPECT_EQ([slice3.buffer()->get() setPurgeableState:MTLPurgeableStateKeepCurrent],
              MTLPurgeableStateNonVolatile);
    contextMtl->markResourceWrittenByCommandBuffer(slice3.buffer());

    mtl::BufferSlice slice5;
    ASSERT_EQ(bufferPool.allocate(contextMtl, kBufferSize, &slice5), angle::Result::Continue);
    EXPECT_EQ(slice5.buffer().get(), slice2.buffer().get());
    EXPECT_EQ([slice2.buffer()->get() setPurgeableState:MTLPurgeableStateKeepCurrent],
              MTLPurgeableStateNonVolatile);
    bufferPool.releaseInFlightBuffers(contextMtl);

    // 4. Flush and wait for slice3 to finish on the GPU. Next allocate() must prefer the ready
    // NonVolatile slice3 in mBufferFreeList over the Volatile slice1 in mVolatileBufferFreeList.
    glFlush();
    contextMtl->cmdQueue().ensureResourceReadyForCPU(slice3.buffer());
    mtl::BufferSlice slice6;
    ASSERT_EQ(bufferPool.allocate(contextMtl, kBufferSize, &slice6), angle::Result::Continue);
    EXPECT_EQ(slice6.buffer().get(), slice3.buffer().get());
    EXPECT_EQ([slice1.buffer()->get() setPurgeableState:MTLPurgeableStateKeepCurrent],
              MTLPurgeableStateVolatile);

    // 5. Trim again via glTrimMemoryANGLE() so mVolatileBufferFreeList is [slice1, slice2, slice3]
    // with slice3 at back(). Simulate OS eviction (Empty) on slice3 (at back()), leaving slice2 and
    // slice1 Volatile.
    glTrimMemoryANGLE(GL_MEMORY_TRIM_HIGH_ANGLE);
    [slice3.buffer()->get() setPurgeableState:MTLPurgeableStateEmpty];

    // 6. Next allocate() must discard the evicted slice3 (decrementing mBuffersAllocated from 3 to
    // 2), reclaim slice2 from back(), and restore slice2 to NonVolatile. Also allocate slice8
    // (reclaiming slice1).
    mtl::BufferSlice slice7;
    ASSERT_EQ(bufferPool.allocate(contextMtl, kBufferSize, &slice7), angle::Result::Continue);
    EXPECT_EQ(slice7.buffer().get(), slice2.buffer().get());
    EXPECT_EQ([slice7.buffer()->get() setPurgeableState:MTLPurgeableStateKeepCurrent],
              MTLPurgeableStateNonVolatile);
    contextMtl->markResourceWrittenByCommandBuffer(slice7.buffer());

    mtl::BufferSlice slice8;
    ASSERT_EQ(bufferPool.allocate(contextMtl, kBufferSize, &slice8), angle::Result::Continue);
    EXPECT_EQ(slice8.buffer().get(), slice1.buffer().get());
    contextMtl->markResourceWrittenByCommandBuffer(slice8.buffer());

    // 7. Because discarding slice3 decremented mBuffersAllocated below kMaxBuffers (2 < 3),
    // allocating slice9 while both slice7 (slice2) and slice8 (slice1) are in-use by the GPU must
    // allocate a new buffer without stalling on the GPU.
    mtl::BufferSlice slice9;
    ASSERT_EQ(bufferPool.allocate(contextMtl, kBufferSize, &slice9), angle::Result::Continue);
    EXPECT_NE(slice9.buffer().get(), slice1.buffer().get());
    EXPECT_NE(slice9.buffer().get(), slice2.buffer().get());
    EXPECT_TRUE(slice7.buffer()->isBeingUsedByGPU(contextMtl));

    bufferPool.destroy(contextMtl);
}

// Test that a failed BufferPool allocation (exceeding MTLDevice.maxBufferLength) does not leave
// mBuffer or mSize in a corrupted state for subsequent allocations or destruction.
TEST_P(BufferPoolTest, FailedAllocationDoesNotCorruptPoolState)
{
    ANGLE_SKIP_TEST_IF(!IsMetalRendererAvailable());

    ContextMtl *contextMtl = getContextMtl();
    ASSERT_NE(contextMtl, nullptr);

    constexpr size_t kBufferSize = 64 * 1024;
    constexpr size_t kAlignment  = 256;
    const size_t kOversized =
        static_cast<size_t>([contextMtl->getMetalDevice() maxBufferLength]) + kAlignment;

    // 1. Failing on the very first allocation and immediately destroying the pool must not assert.
    {
        mtl::BufferPool bufferPool;
        bufferPool.initialize(contextMtl, 0, kAlignment, 0, mtl::TrackBufferInContext::Yes);

        mtl::BufferSlice slice;
        EXPECT_EQ(bufferPool.allocate(contextMtl, kOversized, &slice), angle::Result::Stop);
        EXPECT_GL_ERROR(GL_OUT_OF_MEMORY);

        bufferPool.destroy(contextMtl);
    }

    // 2. Failing after an initial allocation must revert mSize so subsequent smaller allocations
    // still succeed, and destruction remains consistent.
    {
        mtl::BufferPool bufferPool;
        bufferPool.initialize(contextMtl, kBufferSize, kAlignment, 0,
                              mtl::TrackBufferInContext::Yes);

        mtl::BufferSlice slice1;
        ASSERT_EQ(bufferPool.allocate(contextMtl, kBufferSize, &slice1), angle::Result::Continue);

        mtl::BufferSlice failedSlice;
        EXPECT_EQ(bufferPool.allocate(contextMtl, kOversized, &failedSlice), angle::Result::Stop);
        EXPECT_GL_ERROR(GL_OUT_OF_MEMORY);

        mtl::BufferSlice slice2;
        ASSERT_EQ(bufferPool.allocate(contextMtl, kBufferSize, &slice2), angle::Result::Continue);
        EXPECT_NE(slice2.buffer(), nullptr);
        EXPECT_EQ(slice2.buffer()->size(), kBufferSize);

        bufferPool.destroy(contextMtl);
    }
}

}  // namespace

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(BufferPoolTest);
ANGLE_INSTANTIATE_TEST(BufferPoolTest,
                       ES2_METAL().enable(Feature::PurgeableBufferPool),
                       ES2_METAL().disable(Feature::PurgeableBufferPool));
