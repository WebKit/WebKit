//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Fence11.cpp: Defines the rx::FenceNV11 and rx::Sync11 classes which implement
// rx::FenceNVImpl and rx::SyncImpl.

#include "libANGLE/renderer/d3d/d3d11/Fence11.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"

#include "common/utilities.h"

namespace rx
{

static const int kDeviceLostCheckPeriod = 64;

//
// Template helpers for set and test operations.
//

template <class FenceClass>
gl::Error FenceSetHelper(FenceClass *fence)
{
    if (!fence->mQuery)
    {
        D3D11_QUERY_DESC queryDesc;
        queryDesc.Query     = D3D11_QUERY_EVENT;
        queryDesc.MiscFlags = 0;

        HRESULT result = fence->mRenderer->getDevice()->CreateQuery(&queryDesc, &fence->mQuery);
        if (FAILED(result))
        {
            return gl::OutOfMemory() << "Failed to create event query, " << gl::FmtHR(result);
        }
    }

    fence->mRenderer->getDeviceContext()->End(fence->mQuery);
    return gl::NoError();
}

template <class FenceClass>
gl::Error FenceTestHelper(FenceClass *fence, bool flushCommandBuffer, GLboolean *outFinished)
{
    ASSERT(fence->mQuery);

    UINT getDataFlags = (flushCommandBuffer ? 0 : D3D11_ASYNC_GETDATA_DONOTFLUSH);
    HRESULT result =
        fence->mRenderer->getDeviceContext()->GetData(fence->mQuery, nullptr, 0, getDataFlags);

    if (FAILED(result))
    {
        return gl::OutOfMemory() << "Failed to get query data, " << gl::FmtHR(result);
    }

    ASSERT(result == S_OK || result == S_FALSE);
    *outFinished = ((result == S_OK) ? GL_TRUE : GL_FALSE);
    return gl::NoError();
}

//
// FenceNV11
//

FenceNV11::FenceNV11(Renderer11 *renderer) : FenceNVImpl(), mRenderer(renderer), mQuery(nullptr)
{
}

FenceNV11::~FenceNV11()
{
    SafeRelease(mQuery);
}

gl::Error FenceNV11::set(GLenum condition)
{
    return FenceSetHelper(this);
}

gl::Error FenceNV11::test(GLboolean *outFinished)
{
    return FenceTestHelper(this, true, outFinished);
}

gl::Error FenceNV11::finish()
{
    GLboolean finished = GL_FALSE;

    int loopCount = 0;
    while (finished != GL_TRUE)
    {
        loopCount++;
        ANGLE_TRY(FenceTestHelper(this, true, &finished));

        if (loopCount % kDeviceLostCheckPeriod == 0 && mRenderer->testDeviceLost())
        {
            return gl::OutOfMemory() << "Device was lost while querying result of an event query.";
        }

        ScheduleYield();
    }

    return gl::NoError();
}

//
// Sync11
//

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

Sync11::Sync11(Renderer11 *renderer) : SyncImpl(), mRenderer(renderer), mQuery(nullptr)
{
    LARGE_INTEGER counterFreqency = {};
    BOOL success                  = QueryPerformanceFrequency(&counterFreqency);
    ASSERT(success);

    mCounterFrequency = counterFreqency.QuadPart;
}

Sync11::~Sync11()
{
    SafeRelease(mQuery);
}

gl::Error Sync11::set(GLenum condition, GLbitfield flags)
{
    ASSERT(condition == GL_SYNC_GPU_COMMANDS_COMPLETE && flags == 0);
    return FenceSetHelper(this);
}

gl::Error Sync11::clientWait(GLbitfield flags, GLuint64 timeout, GLenum *outResult)
{
    ASSERT(outResult);

    bool flushCommandBuffer = ((flags & GL_SYNC_FLUSH_COMMANDS_BIT) != 0);

    GLboolean result = GL_FALSE;
    gl::Error error  = FenceTestHelper(this, flushCommandBuffer, &result);
    if (error.isError())
    {
        *outResult = GL_WAIT_FAILED;
        return error;
    }

    if (result == GL_TRUE)
    {
        *outResult = GL_ALREADY_SIGNALED;
        return gl::NoError();
    }

    if (timeout == 0)
    {
        *outResult = GL_TIMEOUT_EXPIRED;
        return gl::NoError();
    }

    LARGE_INTEGER currentCounter = {};
    BOOL success                 = QueryPerformanceCounter(&currentCounter);
    ASSERT(success);

    LONGLONG timeoutInSeconds = static_cast<LONGLONG>(timeout / 1000000000ull);
    LONGLONG endCounter       = currentCounter.QuadPart + mCounterFrequency * timeoutInSeconds;

    // Extremely unlikely, but if mCounterFrequency is large enough, endCounter can wrap
    if (endCounter < currentCounter.QuadPart)
    {
        endCounter = MAXLONGLONG;
    }

    int loopCount = 0;
    while (currentCounter.QuadPart < endCounter && !result)
    {
        loopCount++;
        ScheduleYield();
        success = QueryPerformanceCounter(&currentCounter);
        ASSERT(success);

        error = FenceTestHelper(this, flushCommandBuffer, &result);
        if (error.isError())
        {
            *outResult = GL_WAIT_FAILED;
            return error;
        }

        if ((loopCount % kDeviceLostCheckPeriod) == 0 && mRenderer->testDeviceLost())
        {
            *outResult = GL_WAIT_FAILED;
            return gl::OutOfMemory() << "Device was lost while querying result of an event query.";
        }
    }

    if (currentCounter.QuadPart >= endCounter)
    {
        *outResult = GL_TIMEOUT_EXPIRED;
    }
    else
    {
        *outResult = GL_CONDITION_SATISFIED;
    }

    return gl::NoError();
}

gl::Error Sync11::serverWait(GLbitfield flags, GLuint64 timeout)
{
    // Because our API is currently designed to be called from a single thread, we don't need to do
    // extra work for a server-side fence. GPU commands issued after the fence is created will
    // always be processed after the fence is signaled.
    return gl::NoError();
}

gl::Error Sync11::getStatus(GLint *outResult)
{
    GLboolean result = GL_FALSE;
    gl::Error error  = FenceTestHelper(this, false, &result);
    if (error.isError())
    {
        // The spec does not specify any way to report errors during the status test (e.g. device
        // lost) so we report the fence is unblocked in case of error or signaled.
        *outResult = GL_SIGNALED;

        return error;
    }

    *outResult = (result ? GL_SIGNALED : GL_UNSIGNALED);
    return gl::NoError();
}

}  // namespace rx
