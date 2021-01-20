//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TransformFeedbackMtl.h:
//    Defines the class interface for TransformFeedbackMtl, implementing TransformFeedbackImpl.
//

#ifndef LIBANGLE_RENDERER_METAL_TRANSFORMFEEDBACKMTL_H_
#define LIBANGLE_RENDERER_METAL_TRANSFORMFEEDBACKMTL_H_

#include <Metal/Metal.h>
#include "libANGLE/renderer/TransformFeedbackImpl.h"
#include "libANGLE/renderer/metal/BufferMtl.h"
#include "libANGLE/renderer/metal/mtl_resources.h"

typedef uint64_t MtlDeviceSize;

namespace gl
{
class ProgramState;
}  // namespace gl

namespace rx
{

class TransformFeedbackMtl : public TransformFeedbackImpl
{
  public:
    TransformFeedbackMtl(const gl::TransformFeedbackState &state);
    ~TransformFeedbackMtl() override;

    angle::Result begin(const gl::Context *context, gl::PrimitiveMode primitiveMode) override;
    angle::Result end(const gl::Context *context) override;
    angle::Result pause(const gl::Context *context) override;
    angle::Result resume(const gl::Context *context) override;

    angle::Result bindIndexedBuffer(const gl::Context *context,
                                    size_t index,
                                    const gl::OffsetBindingPointer<gl::Buffer> &binding) override;

    void getBufferOffsets(ContextMtl *contextMtl,
                          GLint drawCallFirstVertex,
                          int32_t *offsetsOut,
                          size_t offsetsSize) const;

    const gl::TransformFeedbackBuffersArray<BufferMtl *> &getBufferHandles() const
    {
        return mBufferHandles;
    }

  private:
    gl::TransformFeedbackBuffersArray<BufferMtl *> mBufferHandles;
    gl::TransformFeedbackBuffersArray<MtlDeviceSize> mBufferOffsets;
    gl::TransformFeedbackBuffersArray<MtlDeviceSize> mBufferSizes;

    // Aligned offset for emulation. Could be smaller than offset.
    gl::TransformFeedbackBuffersArray<MtlDeviceSize> mAlignedBufferOffsets;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_METAL_TRANSFORMFEEDBACKMTL_H_
