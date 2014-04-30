#include "precompiled.h"
//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Fence.cpp: Implements the gl::Fence class, which supports the GL_NV_fence extension.

// Important note on accurate timers in Windows:
//
// QueryPerformanceCounter has a few major issues, including being 10x as expensive to call
// as timeGetTime on laptops and "jumping" during certain hardware events.
//
// See the comments at the top of the Chromium source file "chromium/src/base/time/time_win.cc"
//   https://code.google.com/p/chromium/codesearch#chromium/src/base/time/time_win.cc
//
// We still opt to use QPC. In the present and moving forward, most newer systems will not suffer
// from buggy implementations.

#include "libGLESv2/Fence.h"
#include "libGLESv2/renderer/FenceImpl.h"
#include "libGLESv2/renderer/Renderer.h"
#include "libGLESv2/main.h"

namespace gl
{

FenceNV::FenceNV(rx::Renderer *renderer)
{
    mFence = renderer->createFence();
}

FenceNV::~FenceNV()
{
    delete mFence;
}

GLboolean FenceNV::isFence() const
{
    // GL_NV_fence spec:
    // A name returned by GenFencesNV, but not yet set via SetFenceNV, is not the name of an existing fence.
    return (mFence->isSet() ? GL_TRUE : GL_FALSE);
}

void FenceNV::setFence(GLenum condition)
{
    mFence->set();

    mCondition = condition;
    mStatus = GL_FALSE;
}

GLboolean FenceNV::testFence()
{
    // Flush the command buffer by default
    bool result = mFence->test(true);

    mStatus = (result ? GL_TRUE : GL_FALSE);
    return mStatus;
}

void FenceNV::finishFence()
{
    ASSERT(mFence->isSet());

    while (!mFence->test(true))
    {
        Sleep(0);
    }
}

GLint FenceNV::getFencei(GLenum pname)
{
    ASSERT(mFence->isSet());

    switch (pname)
    {
      case GL_FENCE_STATUS_NV:
        {
            // GL_NV_fence spec:
            // Once the status of a fence has been finished (via FinishFenceNV) or tested and the returned status is TRUE (via either TestFenceNV
            // or GetFenceivNV querying the FENCE_STATUS_NV), the status remains TRUE until the next SetFenceNV of the fence.
            if (mStatus == GL_TRUE)
            {
                return GL_TRUE;
            }

            mStatus = (mFence->test(false) ? GL_TRUE : GL_FALSE);
            return mStatus;
        }

      case GL_FENCE_CONDITION_NV:
        return mCondition;

      default: UNREACHABLE(); return 0;
    }
}

FenceSync::FenceSync(rx::Renderer *renderer, GLuint id)
    : RefCountObject(id)
{
    mFence = renderer->createFence();

    LARGE_INTEGER counterFreqency = { 0 };
    BOOL success = QueryPerformanceFrequency(&counterFreqency);
    ASSERT(success);

    mCounterFrequency = counterFreqency.QuadPart;
}

FenceSync::~FenceSync()
{
    delete mFence;
}

void FenceSync::set(GLenum condition)
{
    mCondition = condition;
    mFence->set();
}

GLenum FenceSync::clientWait(GLbitfield flags, GLuint64 timeout)
{
    ASSERT(mFence->isSet());

    bool flushCommandBuffer = ((flags & GL_SYNC_FLUSH_COMMANDS_BIT) != 0);

    if (mFence->test(flushCommandBuffer))
    {
        return GL_ALREADY_SIGNALED;
    }

    if (mFence->hasError())
    {
        return GL_WAIT_FAILED;
    }

    if (timeout == 0)
    {
        return GL_TIMEOUT_EXPIRED;
    }

    LARGE_INTEGER currentCounter = { 0 };
    BOOL success = QueryPerformanceCounter(&currentCounter);
    ASSERT(success);

    LONGLONG timeoutInSeconds = static_cast<LONGLONG>(timeout) * static_cast<LONGLONG>(1000000ll);
    LONGLONG endCounter = currentCounter.QuadPart + mCounterFrequency * timeoutInSeconds;

    while (currentCounter.QuadPart < endCounter && !mFence->test(flushCommandBuffer))
    {
        Sleep(0);
        BOOL success = QueryPerformanceCounter(&currentCounter);
        ASSERT(success);
    }

    if (mFence->hasError())
    {
        return GL_WAIT_FAILED;
    }

    if (currentCounter.QuadPart >= endCounter)
    {
        return GL_TIMEOUT_EXPIRED;
    }

    return GL_CONDITION_SATISFIED;
}

void FenceSync::serverWait()
{
    // Because our API is currently designed to be called from a single thread, we don't need to do
    // extra work for a server-side fence. GPU commands issued after the fence is created will always
    // be processed after the fence is signaled.
}

GLenum FenceSync::getStatus() const
{
    if (mFence->test(false))
    {
        // The spec does not specify any way to report errors during the status test (e.g. device lost)
        // so we report the fence is unblocked in case of error or signaled.
        return GL_SIGNALED;
    }

    return GL_UNSIGNALED;
}

}
