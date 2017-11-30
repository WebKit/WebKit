//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SyncGL.cpp: Implements the class methods for SyncGL.

#include "libANGLE/renderer/gl/SyncGL.h"

#include "common/debug.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"

namespace rx
{

SyncGL::SyncGL(const FunctionsGL *functions) : SyncImpl(), mFunctions(functions), mSyncObject(0)
{
    ASSERT(mFunctions);
}

SyncGL::~SyncGL()
{
    if (mSyncObject != 0)
    {
        mFunctions->deleteSync(mSyncObject);
    }
}

gl::Error SyncGL::set(GLenum condition, GLbitfield flags)
{
    ASSERT(condition == GL_SYNC_GPU_COMMANDS_COMPLETE && flags == 0);
    mSyncObject = mFunctions->fenceSync(condition, flags);
    if (mSyncObject == 0)
    {
        // if glFenceSync fails, it returns 0.
        return gl::OutOfMemory() << "glFenceSync failed to create a GLsync object.";
    }

    return gl::NoError();
}

gl::Error SyncGL::clientWait(GLbitfield flags, GLuint64 timeout, GLenum *outResult)
{
    ASSERT(mSyncObject != 0);
    *outResult = mFunctions->clientWaitSync(mSyncObject, flags, timeout);
    return gl::NoError();
}

gl::Error SyncGL::serverWait(GLbitfield flags, GLuint64 timeout)
{
    ASSERT(mSyncObject != 0);
    mFunctions->waitSync(mSyncObject, flags, timeout);
    return gl::NoError();
}

gl::Error SyncGL::getStatus(GLint *outResult)
{
    ASSERT(mSyncObject != 0);
    mFunctions->getSynciv(mSyncObject, GL_SYNC_STATUS, 1, nullptr, outResult);
    return gl::NoError();
}
}
