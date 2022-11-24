//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_buffer_manager.h:
//    BufferManager manages buffers across all contexts for a single
//    device.
//
#ifndef LIBANGLE_RENDERER_METAL_MTL_BUFFER_MANAGER_H_
#define LIBANGLE_RENDERER_METAL_MTL_BUFFER_MANAGER_H_

#include "common/FixedVector.h"
#include "libANGLE/renderer/metal/mtl_resources.h"

#include <vector>

namespace rx
{
class ContextMtl;

namespace mtl
{

// GL buffers are backed by Metal buffers. Which metal
// buffer is backing a particular GL buffer is fluid.
// The case being optimized is a loop of something like
//
//    for 1..4
//      glBufferSubData
//      glDrawXXX
//
// You can't update a buffer in the middle of a render pass
// in metal so instead we'd end up using multiple buffers.
//
// Simple case, the call to `glBufferSubData` updates the
// entire buffer. In this case we'd end up with each call
// to `glBufferSubData` getting a new buffer from this
// BufferManager and copying the new data to it. We'd
// end up submitting this renderpass
//
//    draw with buf1
//    draw with buf2
//    draw with buf3
//    draw with buf4
//
// The GL buffer now references buf4. And buf1, buf2, buf3 and
// buf0 (the buffer that was previously referenced by the GL buffer)
// are all added to the inuse-list
//

// This macro enables showing the running totals of the various
// buckets of unused buffers.
// #define ANGLE_MTL_TRACK_BUFFER_MEM

class BufferManager
{
  public:
    BufferManager();

    static constexpr size_t kMaxStagingBufferSize = 1024 * 1024;
    static constexpr size_t kMaxSizePowerOf2      = 64;

    angle::Result queueBlitCopyDataToBuffer(ContextMtl *contextMtl,
                                            const void *srcPtr,
                                            size_t sizeToCopy,
                                            size_t offset,
                                            mtl::BufferRef &dstMetalBuffer);

    angle::Result getBuffer(ContextMtl *contextMtl,
                            size_t size,
                            bool useSharedMem,
                            mtl::BufferRef &bufferRef);
    void returnBuffer(ContextMtl *contextMtl, mtl::BufferRef &bufferRef);

  private:
    typedef std::vector<mtl::BufferRef> BufferList;

    void freeUnusedBuffers(ContextMtl *contextMtl);
    void addBufferRefToFreeLists(mtl::BufferRef &bufferRef);

    BufferList mInUseBuffers;

    angle::FixedVector<BufferList, kMaxSizePowerOf2> mFreeBuffers[2];
#ifdef ANGLE_MTL_TRACK_BUFFER_MEM
    angle::FixedVector<size_t, kMaxSizePowerOf2> mAllocations;
    size_t mTotalMem = 0;
#endif
};

}  // namespace mtl
}  // namespace rx

#endif /* LIBANGLE_RENDERER_METAL_MTL_BUFFER_MANAGER_H_ */
