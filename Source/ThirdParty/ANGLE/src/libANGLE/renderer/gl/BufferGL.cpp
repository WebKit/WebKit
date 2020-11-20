//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BufferGL.cpp: Implements the class methods for BufferGL.

#include "libANGLE/renderer/gl/BufferGL.h"

#include "common/debug.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/gl/ContextGL.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"
#include "libANGLE/renderer/gl/renderergl_utils.h"

namespace rx
{

// Use the GL_COPY_READ_BUFFER binding when two buffers need to be bound simultaneously.
// GL_ELEMENT_ARRAY_BUFFER is supported on more versions but can modify the state of the currently
// bound VAO.  Two simultaneous buffer bindings are only needed for glCopyBufferSubData which also
// adds the GL_COPY_READ_BUFFER binding.
static constexpr gl::BufferBinding SourceBufferOperationTarget = gl::BufferBinding::CopyRead;

// Use the GL_ELEMENT_ARRAY_BUFFER binding for most operations since it's available on all
// supported GL versions and doesn't affect any current state when it changes.
static constexpr gl::BufferBinding DestBufferOperationTarget = gl::BufferBinding::Array;

BufferGL::BufferGL(const gl::BufferState &state, GLuint buffer)
    : BufferImpl(state),
      mIsMapped(false),
      mMapOffset(0),
      mMapSize(0),
      mShadowCopy(),
      mBufferSize(0),
      mBufferID(buffer)
{}

BufferGL::~BufferGL()
{
    ASSERT(mBufferID == 0);
}

void BufferGL::destroy(const gl::Context *context)
{
    StateManagerGL *stateManager = GetStateManagerGL(context);
    stateManager->deleteBuffer(mBufferID);
    mBufferID = 0;
}

angle::Result BufferGL::setData(const gl::Context *context,
                                gl::BufferBinding target,
                                const void *data,
                                size_t size,
                                gl::BufferUsage usage)
{
    ContextGL *contextGL              = GetImplAs<ContextGL>(context);
    const FunctionsGL *functions      = GetFunctionsGL(context);
    StateManagerGL *stateManager      = GetStateManagerGL(context);
    const angle::FeaturesGL &features = GetFeaturesGL(context);

    stateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
    ANGLE_GL_TRY(context, functions->bufferData(gl::ToGLenum(DestBufferOperationTarget), size, data,
                                                ToGLenum(usage)));

    if (features.keepBufferShadowCopy.enabled)
    {
        ANGLE_CHECK_GL_ALLOC(contextGL, mShadowCopy.resize(size));

        if (size > 0 && data != nullptr)
        {
            memcpy(mShadowCopy.data(), data, size);
        }
    }

    mBufferSize = size;

    return angle::Result::Continue;
}

angle::Result BufferGL::setSubData(const gl::Context *context,
                                   gl::BufferBinding target,
                                   const void *data,
                                   size_t size,
                                   size_t offset)
{
    const FunctionsGL *functions      = GetFunctionsGL(context);
    StateManagerGL *stateManager      = GetStateManagerGL(context);
    const angle::FeaturesGL &features = GetFeaturesGL(context);

    stateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
    ANGLE_GL_TRY(context, functions->bufferSubData(gl::ToGLenum(DestBufferOperationTarget), offset,
                                                   size, data));

    if (features.keepBufferShadowCopy.enabled && size > 0)
    {
        memcpy(mShadowCopy.data() + offset, data, size);
    }

    return angle::Result::Continue;
}

angle::Result BufferGL::copySubData(const gl::Context *context,
                                    BufferImpl *source,
                                    GLintptr sourceOffset,
                                    GLintptr destOffset,
                                    GLsizeiptr size)
{
    const FunctionsGL *functions      = GetFunctionsGL(context);
    StateManagerGL *stateManager      = GetStateManagerGL(context);
    const angle::FeaturesGL &features = GetFeaturesGL(context);

    BufferGL *sourceGL = GetAs<BufferGL>(source);

    stateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
    stateManager->bindBuffer(SourceBufferOperationTarget, sourceGL->getBufferID());

    ANGLE_GL_TRY(context, functions->copyBufferSubData(gl::ToGLenum(SourceBufferOperationTarget),
                                                       gl::ToGLenum(DestBufferOperationTarget),
                                                       sourceOffset, destOffset, size));

    if (features.keepBufferShadowCopy.enabled && size > 0)
    {
        ASSERT(sourceGL->mShadowCopy.size() >= static_cast<size_t>(sourceOffset + size));
        memcpy(mShadowCopy.data() + destOffset, sourceGL->mShadowCopy.data() + sourceOffset, size);
    }

    return angle::Result::Continue;
}

angle::Result BufferGL::map(const gl::Context *context, GLenum access, void **mapPtr)
{
    const FunctionsGL *functions      = GetFunctionsGL(context);
    StateManagerGL *stateManager      = GetStateManagerGL(context);
    const angle::FeaturesGL &features = GetFeaturesGL(context);

    if (features.keepBufferShadowCopy.enabled)
    {
        *mapPtr = mShadowCopy.data();
    }
    else if (functions->mapBuffer)
    {
        stateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
        *mapPtr = ANGLE_GL_TRY(
            context, functions->mapBuffer(gl::ToGLenum(DestBufferOperationTarget), access));
    }
    else
    {
        ASSERT(functions->mapBufferRange && access == GL_WRITE_ONLY_OES);
        stateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
        *mapPtr =
            ANGLE_GL_TRY(context, functions->mapBufferRange(gl::ToGLenum(DestBufferOperationTarget),
                                                            0, mBufferSize, GL_MAP_WRITE_BIT));
    }

    mIsMapped  = true;
    mMapOffset = 0;
    mMapSize   = mBufferSize;

    return angle::Result::Continue;
}

angle::Result BufferGL::mapRange(const gl::Context *context,
                                 size_t offset,
                                 size_t length,
                                 GLbitfield access,
                                 void **mapPtr)
{
    const FunctionsGL *functions      = GetFunctionsGL(context);
    StateManagerGL *stateManager      = GetStateManagerGL(context);
    const angle::FeaturesGL &features = GetFeaturesGL(context);

    if (features.keepBufferShadowCopy.enabled)
    {
        *mapPtr = mShadowCopy.data() + offset;
    }
    else
    {
        stateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
        *mapPtr =
            ANGLE_GL_TRY(context, functions->mapBufferRange(gl::ToGLenum(DestBufferOperationTarget),
                                                            offset, length, access));
    }

    mIsMapped  = true;
    mMapOffset = offset;
    mMapSize   = length;

    return angle::Result::Continue;
}

angle::Result BufferGL::unmap(const gl::Context *context, GLboolean *result)
{
    const FunctionsGL *functions      = GetFunctionsGL(context);
    StateManagerGL *stateManager      = GetStateManagerGL(context);
    const angle::FeaturesGL &features = GetFeaturesGL(context);

    ASSERT(result);
    ASSERT(mIsMapped);

    if (features.keepBufferShadowCopy.enabled)
    {
        stateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
        ANGLE_GL_TRY(context,
                     functions->bufferSubData(gl::ToGLenum(DestBufferOperationTarget), mMapOffset,
                                              mMapSize, mShadowCopy.data() + mMapOffset));
        *result = GL_TRUE;
    }
    else
    {
        stateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
        *result =
            ANGLE_GL_TRY(context, functions->unmapBuffer(gl::ToGLenum(DestBufferOperationTarget)));
    }

    mIsMapped = false;
    return angle::Result::Continue;
}

angle::Result BufferGL::getIndexRange(const gl::Context *context,
                                      gl::DrawElementsType type,
                                      size_t offset,
                                      size_t count,
                                      bool primitiveRestartEnabled,
                                      gl::IndexRange *outRange)
{
    const FunctionsGL *functions      = GetFunctionsGL(context);
    StateManagerGL *stateManager      = GetStateManagerGL(context);
    const angle::FeaturesGL &features = GetFeaturesGL(context);

    ASSERT(!mIsMapped);

    if (features.keepBufferShadowCopy.enabled)
    {
        *outRange = gl::ComputeIndexRange(type, mShadowCopy.data() + offset, count,
                                          primitiveRestartEnabled);
    }
    else
    {
        stateManager->bindBuffer(DestBufferOperationTarget, mBufferID);

        const GLuint typeBytes = gl::GetDrawElementsTypeSize(type);
        const uint8_t *bufferData =
            MapBufferRangeWithFallback(functions, gl::ToGLenum(DestBufferOperationTarget), offset,
                                       count * typeBytes, GL_MAP_READ_BIT);
        if (bufferData)
        {
            *outRange = gl::ComputeIndexRange(type, bufferData, count, primitiveRestartEnabled);
            ANGLE_GL_TRY(context, functions->unmapBuffer(gl::ToGLenum(DestBufferOperationTarget)));
        }
        else
        {
            // Workaround the null driver not having map support.
            *outRange = gl::IndexRange(0, 0, 1);
        }
    }

    return angle::Result::Continue;
}

GLuint BufferGL::getBufferID() const
{
    return mBufferID;
}
}  // namespace rx
