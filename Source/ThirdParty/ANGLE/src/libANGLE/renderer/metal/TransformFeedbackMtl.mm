//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TransformFeedbackMtl.mm:
//    Defines the class interface for TransformFeedbackMtl, implementing TransformFeedbackImpl.
//

#include "libANGLE/renderer/metal/TransformFeedbackMtl.h"
#include "libANGLE/renderer/metal/BufferMtl.h"
#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"
#include "libANGLE/renderer/metal/ProgramMtl.h"
#include "libANGLE/renderer/metal/QueryMtl.h"
#include "libANGLE/renderer/metal/mtl_common.h"

#include "libANGLE/Context.h"
#include "libANGLE/Query.h"

#include "common/debug.h"

namespace rx
{

TransformFeedbackMtl::TransformFeedbackMtl(const gl::TransformFeedbackState &state)
    : TransformFeedbackImpl(state),
      mBufferHandles{},
      mBufferOffsets{},
      mBufferSizes{},
      mAlignedBufferOffsets{}
{}

TransformFeedbackMtl::~TransformFeedbackMtl() {}

angle::Result TransformFeedbackMtl::begin(const gl::Context *context,
                                          gl::PrimitiveMode primitiveMode)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);
    // Not currently supported with msl compiler
    if (!contextMtl->getDisplay()->getFeatures().emulateTransformFeedback.enabled)
    {
        return angle::Result::Stop;
    }

    const gl::ProgramExecutable *executable = contextMtl->getState().getProgramExecutable();
    ASSERT(executable);
    size_t xfbBufferCount = executable->getTransformFeedbackBufferCount();

    for (size_t bufferIndex = 0; bufferIndex < xfbBufferCount; ++bufferIndex)
    {
        const gl::OffsetBindingPointer<gl::Buffer> &binding = mState.getIndexedBuffer(bufferIndex);
        ASSERT(binding.get());

        BufferMtl *bufferMtl = mtl::GetImpl(binding.get());

        assert(bufferMtl);
        mBufferOffsets[bufferIndex] = binding.getOffset();
        mBufferSizes[bufferIndex]   = gl::GetBoundBufferAvailableSize(binding);
        mBufferHandles[bufferIndex] = bufferMtl;

        ASSERT(contextMtl->getDisplay()->getFeatures().emulateTransformFeedback.enabled);
        const MtlDeviceSize offsetAlignment = mtl::kUniformBufferSettingOffsetMinAlignment;

        // Make sure there's no possible under/overflow with binding size.
        static_assert(sizeof(MtlDeviceSize) >= sizeof(binding.getSize()),
                      "MtlDeviceSize too small");

        // Set the offset as close as possible to the requested offset while remaining aligned.
        mAlignedBufferOffsets[bufferIndex] =
            (mBufferOffsets[bufferIndex] / offsetAlignment) * offsetAlignment;
    }

    return contextMtl->onBeginTransformFeedback(xfbBufferCount, mBufferHandles);
}

angle::Result TransformFeedbackMtl::end(const gl::Context *context)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);
    // Not currently supported with msl compiler
    if (!contextMtl->getDisplay()->getFeatures().emulateTransformFeedback.enabled)
    {
        return angle::Result::Stop;
    }

    // If there's an active transform feedback query, accumulate the primitives drawn.
    const gl::State &glState = context->getState();
    gl::Query *transformFeedbackQuery =
        glState.getActiveQuery(gl::QueryType::TransformFeedbackPrimitivesWritten);

    if (transformFeedbackQuery)
    {
        mtl::GetImpl(transformFeedbackQuery)->onTransformFeedbackEnd(mState.getPrimitivesDrawn());
    }
    contextMtl->onEndTransformFeedback();
    return angle::Result::Continue;
}

angle::Result TransformFeedbackMtl::pause(const gl::Context *context)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);
    return contextMtl->onPauseTransformFeedback();
}

angle::Result TransformFeedbackMtl::resume(const gl::Context *context)
{
    ContextMtl *contextMtl                  = mtl::GetImpl(context);
    const gl::ProgramExecutable *executable = contextMtl->getState().getProgramExecutable();
    ASSERT(executable);
    size_t xfbBufferCount = executable->getTransformFeedbackBufferCount();
    return contextMtl->onBeginTransformFeedback(xfbBufferCount, mBufferHandles);
}

angle::Result TransformFeedbackMtl::bindIndexedBuffer(
    const gl::Context *context,
    size_t index,
    const gl::OffsetBindingPointer<gl::Buffer> &binding)
{
    UNIMPLEMENTED();

    return angle::Result::Continue;
}

void TransformFeedbackMtl::getBufferOffsets(ContextMtl *contextMtl,
                                            GLint drawCallFirstVertex,
                                            int32_t *offsetsOut,
                                            size_t offsetsSize) const
{
    if (!contextMtl->getDisplay()->getFeatures().emulateTransformFeedback.enabled)
        return;

    GLsizeiptr verticesDrawn = mState.getVerticesDrawn();
    const std::vector<GLsizei> &bufferStrides =
        mState.getBoundProgram()->getTransformFeedbackStrides();
    const gl::ProgramExecutable *executable = contextMtl->getState().getProgramExecutable();
    ASSERT(executable);
    size_t xfbBufferCount = executable->getTransformFeedbackBufferCount();

    ASSERT(xfbBufferCount > 0);

    // The caller should make sure the offsets array has enough space.  The maximum possible
    // number of outputs is gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS.
    ASSERT(offsetsSize >= xfbBufferCount);

    for (size_t bufferIndex = 0; bufferIndex < xfbBufferCount; ++bufferIndex)
    {
        int64_t offsetFromDescriptor =
            static_cast<int64_t>(mBufferOffsets[bufferIndex] - mAlignedBufferOffsets[bufferIndex]);
        int64_t drawCallVertexOffset = static_cast<int64_t>(verticesDrawn) - drawCallFirstVertex;

        int64_t writeOffset =
            (offsetFromDescriptor + drawCallVertexOffset * bufferStrides[bufferIndex]) /
            static_cast<int64_t>(sizeof(uint32_t));

        offsetsOut[bufferIndex] = static_cast<int32_t>(writeOffset);

        // Assert on overflow.  For now, support transform feedback up to 2GB.
        ASSERT(offsetsOut[bufferIndex] == writeOffset);
    }
}

}  // namespace rx
