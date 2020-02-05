//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BufferMtl.h:
//    Defines the class interface for BufferMtl, implementing BufferImpl.
//

#ifndef LIBANGLE_RENDERER_METAL_BUFFERMTL_H_
#define LIBANGLE_RENDERER_METAL_BUFFERMTL_H_

#import <Metal/Metal.h>

#include <utility>

#include "libANGLE/Buffer.h"
#include "libANGLE/Observer.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/BufferImpl.h"
#include "libANGLE/renderer/Format.h"
#include "libANGLE/renderer/metal/mtl_buffer_pool.h"
#include "libANGLE/renderer/metal/mtl_resources.h"

namespace rx
{

// Conversion buffers hold translated index and vertex data.
struct ConversionBufferMtl
{
    ConversionBufferMtl(const gl::Context *context, size_t initialSize, size_t alignment);
    ~ConversionBufferMtl();

    // One state value determines if we need to re-stream vertex data.
    bool dirty;

    // The conversion is stored in a dynamic buffer.
    mtl::BufferPool data;

    // These properties are to be filled by user of this buffer conversion
    mtl::BufferRef convertedBuffer;
    size_t convertedOffset;
};

struct IndexConversionBufferMtl : public ConversionBufferMtl
{
    IndexConversionBufferMtl(const gl::Context *context,
                             gl::DrawElementsType type,
                             size_t offsetIn);

    const gl::DrawElementsType type;
    const size_t offset;
};

class BufferHolderMtl
{
  public:
    virtual ~BufferHolderMtl() = default;

    // Due to the complication of synchronizing accesses between CPU and GPU,
    // a mtl::Buffer might be under used by GPU but CPU wants to modify its content through
    // map() method, this could lead to GPU stalling. The more efficient method is maintain
    // a queue of mtl::Buffer and only let CPU modifies a free mtl::Buffer.
    // So, in order to let GPU use the most recent modified content, one must call this method
    // right before the draw call to retrieved the most up-to-date mtl::Buffer.
    mtl::BufferRef getCurrentBuffer(const gl::Context *context)
    {
        return mIsWeak ? mBufferWeakRef.lock() : mBuffer;
    }

  protected:
    mtl::BufferRef mBuffer;
    mtl::BufferWeakRef mBufferWeakRef;
    bool mIsWeak = false;
};

class BufferMtl : public BufferImpl, public BufferHolderMtl
{
  public:
    BufferMtl(const gl::BufferState &state);
    ~BufferMtl() override;
    void destroy(const gl::Context *context) override;

    angle::Result setData(const gl::Context *context,
                          gl::BufferBinding target,
                          const void *data,
                          size_t size,
                          gl::BufferUsage usage) override;
    angle::Result setSubData(const gl::Context *context,
                             gl::BufferBinding target,
                             const void *data,
                             size_t size,
                             size_t offset) override;
    angle::Result copySubData(const gl::Context *context,
                              BufferImpl *source,
                              GLintptr sourceOffset,
                              GLintptr destOffset,
                              GLsizeiptr size) override;
    angle::Result map(const gl::Context *context, GLenum access, void **mapPtr) override;
    angle::Result mapRange(const gl::Context *context,
                           size_t offset,
                           size_t length,
                           GLbitfield access,
                           void **mapPtr) override;
    angle::Result unmap(const gl::Context *context, GLboolean *result) override;

    angle::Result getIndexRange(const gl::Context *context,
                                gl::DrawElementsType type,
                                size_t offset,
                                size_t count,
                                bool primitiveRestartEnabled,
                                gl::IndexRange *outRange) override;

    angle::Result getFirstLastIndices(const gl::Context *context,
                                      gl::DrawElementsType type,
                                      size_t offset,
                                      size_t count,
                                      std::pair<uint32_t, uint32_t> *outIndices) const;

    const uint8_t *getClientShadowCopyData(const gl::Context *context);

    ConversionBufferMtl *getVertexConversionBuffer(const gl::Context *context,
                                                   angle::FormatID formatID,
                                                   GLuint stride,
                                                   size_t offset);

    IndexConversionBufferMtl *getIndexConversionBuffer(const gl::Context *context,
                                                       gl::DrawElementsType type,
                                                       size_t offset);

    size_t size() const { return static_cast<size_t>(mState.getSize()); }

  private:
    angle::Result setSubDataImpl(const gl::Context *context,
                                 const void *data,
                                 size_t size,
                                 size_t offset);

    angle::Result commitShadowCopy(const gl::Context *context);
    angle::Result commitShadowCopy(const gl::Context *context, size_t size);

    void markConversionBuffersDirty();

    void clearConversionBuffers();

    // Client side shadow buffer
    angle::MemoryBuffer mShadowCopy;

    // GPU side buffers pool
    mtl::BufferPool mBufferPool;

    struct VertexConversionBuffer : public ConversionBufferMtl
    {
        VertexConversionBuffer(const gl::Context *context,
                               angle::FormatID formatIDIn,
                               GLuint strideIn,
                               size_t offsetIn);

        // The conversion is identified by the triple of {format, stride, offset}.
        angle::FormatID formatID;
        GLuint stride;
        size_t offset;
    };

    // A cache of converted vertex data.
    std::vector<VertexConversionBuffer> mVertexConversionBuffers;

    std::vector<IndexConversionBufferMtl> mIndexConversionBuffers;
};

class SimpleWeakBufferHolderMtl : public BufferHolderMtl
{
  public:
    SimpleWeakBufferHolderMtl();

    void set(const mtl::BufferRef &buffer) { mBufferWeakRef = buffer; }
};

}  // namespace rx

#endif /* LIBANGLE_RENDERER_METAL_BUFFERMTL_H_ */
