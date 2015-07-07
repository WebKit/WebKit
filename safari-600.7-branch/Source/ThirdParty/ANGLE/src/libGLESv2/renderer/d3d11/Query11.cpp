#include "precompiled.h"
//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Query11.cpp: Defines the rx::Query11 class which implements rx::QueryImpl.

#include "libGLESv2/renderer/d3d11/Query11.h"
#include "libGLESv2/renderer/d3d11/Renderer11.h"
#include "libGLESv2/renderer/d3d11/renderer11_utils.h"
#include "libGLESv2/main.h"

namespace rx
{

static bool checkOcclusionQuery(ID3D11DeviceContext *context, ID3D11Query *query, UINT64 *numPixels)
{
    HRESULT result = context->GetData(query, numPixels, sizeof(UINT64), 0);
    return (result == S_OK);
}

static bool checkStreamOutPrimitivesWritten(ID3D11DeviceContext *context, ID3D11Query *query, UINT64 *numPrimitives)
{
    D3D11_QUERY_DATA_SO_STATISTICS soStats = { 0 };
    HRESULT result = context->GetData(query, &soStats, sizeof(D3D11_QUERY_DATA_SO_STATISTICS), 0);
    *numPrimitives = soStats.NumPrimitivesWritten;
    return (result == S_OK);
}

Query11::Query11(rx::Renderer11 *renderer, GLenum type) : QueryImpl(type)
{
    mRenderer = renderer;
    mQuery = NULL;
}

Query11::~Query11()
{
    SafeRelease(mQuery);
}

void Query11::begin()
{
    if (mQuery == NULL)
    {
        D3D11_QUERY_DESC queryDesc;
        queryDesc.Query = gl_d3d11::ConvertQueryType(getType());
        queryDesc.MiscFlags = 0;

        if (FAILED(mRenderer->getDevice()->CreateQuery(&queryDesc, &mQuery)))
        {
            return gl::error(GL_OUT_OF_MEMORY);
        }
    }

    mRenderer->getDeviceContext()->Begin(mQuery);
}

void Query11::end()
{
    if (mQuery == NULL)
    {
        return gl::error(GL_INVALID_OPERATION);
    }

    mRenderer->getDeviceContext()->End(mQuery);

    mStatus = GL_FALSE;
    mResult = GL_FALSE;
}

GLuint Query11::getResult()
{
    if (mQuery != NULL)
    {
        while (!testQuery())
        {
            Sleep(0);
            // explicitly check for device loss, some drivers seem to return S_FALSE
            // if the device is lost
            if (mRenderer->testDeviceLost(true))
            {
                return gl::error(GL_OUT_OF_MEMORY, 0);
            }
        }
    }

    return mResult;
}

GLboolean Query11::isResultAvailable()
{
    if (mQuery != NULL)
    {
        testQuery();
    }

    return mStatus;
}

GLboolean Query11::testQuery()
{
    if (mQuery != NULL && mStatus != GL_TRUE)
    {
        ID3D11DeviceContext *context = mRenderer->getDeviceContext();

        bool queryFinished = false;
        switch (getType())
        {
          case GL_ANY_SAMPLES_PASSED_EXT:
          case GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT:
            {
                UINT64 numPixels = 0;
                queryFinished = checkOcclusionQuery(context, mQuery, &numPixels);
                if (queryFinished)
                {
                    mResult = (numPixels > 0) ? GL_TRUE : GL_FALSE;
                }
            }
            break;

          case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
            {
                UINT64 numPrimitives = 0;
                queryFinished = checkStreamOutPrimitivesWritten(context, mQuery, &numPrimitives);
                if (queryFinished)
                {
                    mResult = static_cast<GLuint>(numPrimitives);
                }
            }
            break;

        default:
            UNREACHABLE();
            break;
        }

        if (queryFinished)
        {
            mStatus = GL_TRUE;
        }
        else if (mRenderer->testDeviceLost(true))
        {
            return gl::error(GL_OUT_OF_MEMORY, GL_TRUE);
        }

        return mStatus;
    }

    return GL_TRUE; // prevent blocking when query is null
}

}
