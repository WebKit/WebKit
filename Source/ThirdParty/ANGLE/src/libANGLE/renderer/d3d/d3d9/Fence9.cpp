//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Fence9.cpp: Defines the rx::FenceNV9 class.

#include "libANGLE/renderer/d3d/d3d9/Fence9.h"
#include "libANGLE/renderer/d3d/d3d9/renderer9_utils.h"
#include "libANGLE/renderer/d3d/d3d9/Renderer9.h"

namespace rx
{

FenceNV9::FenceNV9(Renderer9 *renderer) : FenceNVImpl(), mRenderer(renderer), mQuery(nullptr)
{
}

FenceNV9::~FenceNV9()
{
    SafeRelease(mQuery);
}

gl::Error FenceNV9::set(GLenum condition)
{
    if (!mQuery)
    {
        gl::Error error = mRenderer->allocateEventQuery(&mQuery);
        if (error.isError())
        {
            return error;
        }
    }

    HRESULT result = mQuery->Issue(D3DISSUE_END);
    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
        SafeRelease(mQuery);
        return gl::OutOfMemory() << "Failed to end event query, " << gl::FmtHR(result);
    }

    return gl::NoError();
}

gl::Error FenceNV9::test(GLboolean *outFinished)
{
    return testHelper(true, outFinished);
}

gl::Error FenceNV9::finish()
{
    GLboolean finished = GL_FALSE;
    while (finished != GL_TRUE)
    {
        gl::Error error = testHelper(true, &finished);
        if (error.isError())
        {
            return error;
        }

        Sleep(0);
    }

    return gl::NoError();
}

gl::Error FenceNV9::testHelper(bool flushCommandBuffer, GLboolean *outFinished)
{
    ASSERT(mQuery);

    DWORD getDataFlags = (flushCommandBuffer ? D3DGETDATA_FLUSH : 0);
    HRESULT result     = mQuery->GetData(nullptr, 0, getDataFlags);

    if (d3d9::isDeviceLostError(result))
    {
        mRenderer->notifyDeviceLost();
        return gl::OutOfMemory() << "Device was lost while querying result of an event query.";
    }
    else if (FAILED(result))
    {
        return gl::OutOfMemory() << "Failed to get query data, " << gl::FmtHR(result);
    }

    ASSERT(result == S_OK || result == S_FALSE);
    *outFinished = ((result == S_OK) ? GL_TRUE : GL_FALSE);
    return gl::NoError();
}

}
