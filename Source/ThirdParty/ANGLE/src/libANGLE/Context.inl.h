//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Context.inl.h: Defines inline functions of gl::Context class
// Has to be included after libANGLE/Context.h when using one
// of the defined functions

#ifndef LIBANGLE_CONTEXT_INL_H_
#define LIBANGLE_CONTEXT_INL_H_

#include "libANGLE/Context.h"
#include "libANGLE/GLES1Renderer.h"
#include "libANGLE/renderer/ContextImpl.h"

#define ANGLE_HANDLE_ERR(X) \
    (void)(X);              \
    return;
#define ANGLE_CONTEXT_TRY(EXPR) ANGLE_TRY_TEMPLATE(EXPR, ANGLE_HANDLE_ERR)

namespace gl
{
constexpr angle::PackedEnumMap<PrimitiveMode, GLsizei> kMinimumPrimitiveCounts = {{
    {PrimitiveMode::Points, 1},
    {PrimitiveMode::Lines, 2},
    {PrimitiveMode::LineLoop, 2},
    {PrimitiveMode::LineStrip, 2},
    {PrimitiveMode::Triangles, 3},
    {PrimitiveMode::TriangleStrip, 3},
    {PrimitiveMode::TriangleFan, 3},
    {PrimitiveMode::LinesAdjacency, 2},
    {PrimitiveMode::LineStripAdjacency, 2},
    {PrimitiveMode::TrianglesAdjacency, 3},
    {PrimitiveMode::TriangleStripAdjacency, 3},
}};

ANGLE_INLINE void MarkTransformFeedbackBufferUsage(const Context *context,
                                                   GLsizei count,
                                                   GLsizei instanceCount)
{
    if (context->getStateCache().isTransformFeedbackActiveUnpaused())
    {
        TransformFeedback *transformFeedback = context->getState().getCurrentTransformFeedback();
        transformFeedback->onVerticesDrawn(context, count, instanceCount);
    }
}

ANGLE_INLINE void MarkShaderStorageUsage(const Context *context)
{
    for (size_t index : context->getStateCache().getActiveShaderStorageBufferIndices())
    {
        Buffer *buffer = context->getState().getIndexedShaderStorageBuffer(index).get();
        if (buffer)
        {
            buffer->onDataChanged();
        }
    }

    for (size_t index : context->getStateCache().getActiveImageUnitIndices())
    {
        const ImageUnit &imageUnit = context->getState().getImageUnit(index);
        const Texture *texture     = imageUnit.texture.get();
        if (texture)
        {
            texture->onStateChange(angle::SubjectMessage::ContentsChanged);
        }
    }
}

// Return true if the draw is a no-op, else return false.
//  If there is no active program for the vertex or fragment shader stages, the results of vertex
//  and fragment shader execution will respectively be undefined. However, this is not
//  an error. ANGLE will treat this as a no-op.
//  A no-op draw occurs if the count of vertices is less than the minimum required to
//  have a valid primitive for this mode (0 for points, 0-1 for lines, 0-2 for tris).
ANGLE_INLINE bool Context::noopDraw(PrimitiveMode mode, GLsizei count) const
{
    if (!mStateCache.getCanDraw())
    {
        return true;
    }

    return count < kMinimumPrimitiveCounts[mode];
}

ANGLE_INLINE angle::Result Context::syncDirtyBits(Command command)
{
    const State::DirtyBits &dirtyBits = mState.getDirtyBits();
    ANGLE_TRY(mImplementation->syncState(this, dirtyBits, mAllDirtyBits, command));
    mState.clearDirtyBits();
    return angle::Result::Continue;
}

ANGLE_INLINE angle::Result Context::syncDirtyBits(const State::DirtyBits &bitMask, Command command)
{
    const State::DirtyBits &dirtyBits = (mState.getDirtyBits() & bitMask);
    ANGLE_TRY(mImplementation->syncState(this, dirtyBits, bitMask, command));
    mState.clearDirtyBits(dirtyBits);
    return angle::Result::Continue;
}

ANGLE_INLINE angle::Result Context::syncDirtyObjects(const State::DirtyObjects &objectMask,
                                                     Command command)
{
    return mState.syncDirtyObjects(this, objectMask, command);
}

ANGLE_INLINE angle::Result Context::prepareForDraw(PrimitiveMode mode)
{
    if (mGLES1Renderer)
    {
        ANGLE_TRY(mGLES1Renderer->prepareForDraw(mode, this, &mState));
    }

    ANGLE_TRY(syncDirtyObjects(mDrawDirtyObjects, Command::Draw));
    ASSERT(!isRobustResourceInitEnabled() ||
           !mState.getDrawFramebuffer()->hasResourceThatNeedsInit());
    return syncDirtyBits(Command::Draw);
}

ANGLE_INLINE void Context::drawArrays(PrimitiveMode mode, GLint first, GLsizei count)
{
    // No-op if count draws no primitives for given mode
    if (noopDraw(mode, count))
    {
        ANGLE_CONTEXT_TRY(mImplementation->handleNoopDrawEvent());
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(mImplementation->drawArrays(this, mode, first, count));
    MarkTransformFeedbackBufferUsage(this, count, 1);
}

ANGLE_INLINE void Context::drawElements(PrimitiveMode mode,
                                        GLsizei count,
                                        DrawElementsType type,
                                        const void *indices)
{
    // No-op if count draws no primitives for given mode
    if (noopDraw(mode, count))
    {
        ANGLE_CONTEXT_TRY(mImplementation->handleNoopDrawEvent());
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDraw(mode));
    ANGLE_CONTEXT_TRY(mImplementation->drawElements(this, mode, count, type, indices));
}

ANGLE_INLINE void StateCache::onBufferBindingChange(Context *context)
{
    updateBasicDrawStatesError();
    updateBasicDrawElementsError();
}

ANGLE_INLINE void Context::bindBuffer(BufferBinding target, BufferID buffer)
{
    Buffer *bufferObject =
        mState.mBufferManager->checkBufferAllocation(mImplementation.get(), buffer);

    // Early return if rebinding the same buffer
    if (bufferObject == mState.getTargetBuffer(target))
    {
        return;
    }

    mState.setBufferBinding(this, target, bufferObject);
    mStateCache.onBufferBindingChange(this);
}

}  // namespace gl

#endif  // LIBANGLE_CONTEXT_INL_H_
