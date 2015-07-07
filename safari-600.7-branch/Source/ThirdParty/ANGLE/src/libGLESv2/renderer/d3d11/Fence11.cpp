#include "precompiled.h"
//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Fence11.cpp: Defines the rx::Fence11 class which implements rx::FenceImpl.

#include "libGLESv2/renderer/d3d11/Fence11.h"
#include "libGLESv2/main.h"
#include "libGLESv2/renderer/d3d11/Renderer11.h"

namespace rx
{

Fence11::Fence11(rx::Renderer11 *renderer)
{
    mRenderer = renderer;
    mQuery = NULL;
}

Fence11::~Fence11()
{
    SafeRelease(mQuery);
}

bool Fence11::isSet() const
{
    return mQuery != NULL;
}

void Fence11::set()
{
    if (!mQuery)
    {
        D3D11_QUERY_DESC queryDesc;
        queryDesc.Query = D3D11_QUERY_EVENT;
        queryDesc.MiscFlags = 0;

        if (FAILED(mRenderer->getDevice()->CreateQuery(&queryDesc, &mQuery)))
        {
            return gl::error(GL_OUT_OF_MEMORY);
        }
    }

    mRenderer->getDeviceContext()->End(mQuery);
}

bool Fence11::test(bool flushCommandBuffer)
{
    ASSERT(mQuery);

    UINT getDataFlags = (flushCommandBuffer ? 0 : D3D11_ASYNC_GETDATA_DONOTFLUSH);
    HRESULT result = mRenderer->getDeviceContext()->GetData(mQuery, NULL, 0, getDataFlags);

    if (mRenderer->isDeviceLost())
    {
       return gl::error(GL_OUT_OF_MEMORY, true);
    }

    ASSERT(result == S_OK || result == S_FALSE);
    return (result == S_OK);
}

bool Fence11::hasError() const
{
    return mRenderer->isDeviceLost();
}

}
