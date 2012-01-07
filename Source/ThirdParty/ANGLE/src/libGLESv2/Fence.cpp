//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Fence.cpp: Implements the gl::Fence class, which supports the GL_NV_fence extension.

#include "libGLESv2/Fence.h"

#include "libGLESv2/main.h"

namespace gl
{

Fence::Fence()
{ 
    mQuery = NULL;
    mCondition = GL_NONE;
    mStatus = GL_FALSE;
}

Fence::~Fence()
{
    if (mQuery != NULL)
    {
        mQuery->Release();
        mQuery = NULL;
    }
}

GLboolean Fence::isFence()
{
    // GL_NV_fence spec:
    // A name returned by GenFencesNV, but not yet set via SetFenceNV, is not the name of an existing fence.
    return mQuery != NULL;
}

void Fence::setFence(GLenum condition)
{
    if (mQuery != NULL)
    {
        mQuery->Release();
        mQuery = NULL;
    }

    if (FAILED(getDevice()->CreateQuery(D3DQUERYTYPE_EVENT, &mQuery)))
    {
        return error(GL_OUT_OF_MEMORY);
    }

    HRESULT result = mQuery->Issue(D3DISSUE_END);
    ASSERT(SUCCEEDED(result));

    mCondition = condition;
    mStatus = GL_FALSE;
}

GLboolean Fence::testFence()
{
    if (mQuery == NULL)
    {
        return error(GL_INVALID_OPERATION, GL_TRUE);
    }

    HRESULT result = mQuery->GetData(NULL, 0, D3DGETDATA_FLUSH);

    if (checkDeviceLost(result))
    {
       return error(GL_OUT_OF_MEMORY, GL_TRUE);
    }

    ASSERT(result == S_OK || result == S_FALSE);
    mStatus = result == S_OK;
    return mStatus;
}

void Fence::finishFence()
{
    if (mQuery == NULL)
    {
        return error(GL_INVALID_OPERATION);
    }

    while (!testFence())
    {
        Sleep(0);
    }
}

void Fence::getFenceiv(GLenum pname, GLint *params)
{
    if (mQuery == NULL)
    {
        return error(GL_INVALID_OPERATION);
    }

    switch (pname)
    {
        case GL_FENCE_STATUS_NV:
        {
            // GL_NV_fence spec:
            // Once the status of a fence has been finished (via FinishFenceNV) or tested and the returned status is TRUE (via either TestFenceNV
            // or GetFenceivNV querying the FENCE_STATUS_NV), the status remains TRUE until the next SetFenceNV of the fence.
            if (mStatus)
            {
                params[0] = GL_TRUE;
                return;
            }
            
            HRESULT result = mQuery->GetData(NULL, 0, 0);
            
            if (checkDeviceLost(result))
            {
                params[0] = GL_TRUE;
                return error(GL_OUT_OF_MEMORY);
            }

            ASSERT(result == S_OK || result == S_FALSE);
            mStatus = result == S_OK;
            params[0] = mStatus;
            
            break;
        }
        case GL_FENCE_CONDITION_NV:
            params[0] = mCondition;
            break;
        default:
            return error(GL_INVALID_ENUM);
            break;
    }
}

}
