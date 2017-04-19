//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Query9.cpp: Defines the rx::Query9 class which implements rx::QueryImpl.

#include "libANGLE/renderer/d3d/d3d9/Query9.h"
#include "libANGLE/renderer/d3d/d3d9/renderer9_utils.h"
#include "libANGLE/renderer/d3d/d3d9/Renderer9.h"

#include <GLES2/gl2ext.h>

namespace rx
{
Query9::Query9(Renderer9 *renderer, GLenum type)
    : QueryImpl(type),
      mResult(GL_FALSE),
      mQueryFinished(false),
      mRenderer(renderer),
      mQuery(NULL)
{
}

Query9::~Query9()
{
    SafeRelease(mQuery);
}

gl::Error Query9::begin()
{
    D3DQUERYTYPE d3dQueryType = gl_d3d9::ConvertQueryType(getType());
    if (mQuery == nullptr)
    {
        HRESULT result = mRenderer->getDevice()->CreateQuery(d3dQueryType, &mQuery);
        if (FAILED(result))
        {
            return gl::Error(GL_OUT_OF_MEMORY, "Internal query creation failed, result: 0x%X.", result);
        }
    }

    if (d3dQueryType != D3DQUERYTYPE_EVENT)
    {
        HRESULT result = mQuery->Issue(D3DISSUE_BEGIN);
        ASSERT(SUCCEEDED(result));
        if (FAILED(result))
        {
            return gl::Error(GL_OUT_OF_MEMORY, "Failed to begin internal query, result: 0x%X.",
                             result);
        }
    }

    return gl::NoError();
}

gl::Error Query9::end()
{
    ASSERT(mQuery);

    HRESULT result = mQuery->Issue(D3DISSUE_END);
    ASSERT(SUCCEEDED(result));
    if (FAILED(result))
    {
        return gl::Error(GL_OUT_OF_MEMORY, "Failed to end internal query, result: 0x%X.", result);
    }

    mQueryFinished = false;
    mResult = GL_FALSE;

    return gl::NoError();
}

gl::Error Query9::queryCounter()
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION, "Unimplemented");
}

template <typename T>
gl::Error Query9::getResultBase(T *params)
{
    while (!mQueryFinished)
    {
        gl::Error error = testQuery();
        if (error.isError())
        {
            return error;
        }

        if (!mQueryFinished)
        {
            Sleep(0);
        }
    }

    ASSERT(mQueryFinished);
    *params = static_cast<T>(mResult);
    return gl::NoError();
}

gl::Error Query9::getResult(GLint *params)
{
    return getResultBase(params);
}

gl::Error Query9::getResult(GLuint *params)
{
    return getResultBase(params);
}

gl::Error Query9::getResult(GLint64 *params)
{
    return getResultBase(params);
}

gl::Error Query9::getResult(GLuint64 *params)
{
    return getResultBase(params);
}

gl::Error Query9::isResultAvailable(bool *available)
{
    gl::Error error = testQuery();
    if (error.isError())
    {
        return error;
    }

    *available = mQueryFinished;

    return gl::NoError();
}

gl::Error Query9::testQuery()
{
    if (!mQueryFinished)
    {
        ASSERT(mQuery);

        HRESULT result = S_OK;
        switch (getType())
        {
            case GL_ANY_SAMPLES_PASSED_EXT:
            case GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT:
            {
                DWORD numPixels = 0;
                result = mQuery->GetData(&numPixels, sizeof(numPixels), D3DGETDATA_FLUSH);
                if (result == S_OK)
                {
                    mQueryFinished = true;
                    mResult        = (numPixels > 0) ? GL_TRUE : GL_FALSE;
                }
                break;
            }

            case GL_COMMANDS_COMPLETED_CHROMIUM:
            {
                BOOL completed = FALSE;
                result = mQuery->GetData(&completed, sizeof(completed), D3DGETDATA_FLUSH);
                if (result == S_OK)
                {
                    mQueryFinished = true;
                    mResult        = (completed == TRUE) ? GL_TRUE : GL_FALSE;
                }
                break;
            }

            default:
                UNREACHABLE();
                break;
        }

        if (d3d9::isDeviceLostError(result))
        {
            mRenderer->notifyDeviceLost();
            return gl::Error(GL_OUT_OF_MEMORY, "Failed to test get query result, device is lost.");
        }
        else if (mRenderer->testDeviceLost())
        {
            mRenderer->notifyDeviceLost();
            return gl::Error(GL_OUT_OF_MEMORY, "Failed to test get query result, device is lost.");
        }
    }

    return gl::NoError();
}

}
