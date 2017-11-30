//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Fence.cpp: Implements the gl::FenceNV and gl::Sync classes, which support the GL_NV_fence
// extension and GLES3 sync objects.

#include "libANGLE/Fence.h"

#include "angle_gl.h"

#include "common/utilities.h"
#include "libANGLE/renderer/FenceNVImpl.h"
#include "libANGLE/renderer/SyncImpl.h"

namespace gl
{

FenceNV::FenceNV(rx::FenceNVImpl *impl)
    : mFence(impl),
      mIsSet(false),
      mStatus(GL_FALSE),
      mCondition(GL_NONE)
{
}

FenceNV::~FenceNV()
{
    SafeDelete(mFence);
}

Error FenceNV::set(GLenum condition)
{
    Error error = mFence->set(condition);
    if (error.isError())
    {
        return error;
    }

    mCondition = condition;
    mStatus = GL_FALSE;
    mIsSet = true;

    return NoError();
}

Error FenceNV::test(GLboolean *outResult)
{
    // Flush the command buffer by default
    Error error = mFence->test(&mStatus);
    if (error.isError())
    {
        return error;
    }

    *outResult = mStatus;
    return NoError();
}

Error FenceNV::finish()
{
    ASSERT(mIsSet);

    gl::Error error = mFence->finish();
    if (error.isError())
    {
        return error;
    }

    mStatus = GL_TRUE;

    return NoError();
}

Sync::Sync(rx::SyncImpl *impl, GLuint id)
    : RefCountObject(id),
      mFence(impl),
      mLabel(),
      mCondition(GL_SYNC_GPU_COMMANDS_COMPLETE),
      mFlags(0)
{
}

Error Sync::onDestroy(const Context *context)
{
    return NoError();
}

Sync::~Sync()
{
    SafeDelete(mFence);
}

void Sync::setLabel(const std::string &label)
{
    mLabel = label;
}

const std::string &Sync::getLabel() const
{
    return mLabel;
}

Error Sync::set(GLenum condition, GLbitfield flags)
{
    Error error = mFence->set(condition, flags);
    if (error.isError())
    {
        return error;
    }

    mCondition = condition;
    mFlags = flags;
    return NoError();
}

Error Sync::clientWait(GLbitfield flags, GLuint64 timeout, GLenum *outResult)
{
    ASSERT(mCondition != GL_NONE);
    return mFence->clientWait(flags, timeout, outResult);
}

Error Sync::serverWait(GLbitfield flags, GLuint64 timeout)
{
    return mFence->serverWait(flags, timeout);
}

Error Sync::getStatus(GLint *outResult) const
{
    return mFence->getStatus(outResult);
}

}  // namespace gl
