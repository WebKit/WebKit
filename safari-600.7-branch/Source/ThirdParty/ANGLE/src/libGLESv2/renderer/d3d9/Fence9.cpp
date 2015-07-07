#include "precompiled.h"
//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Fence9.cpp: Defines the rx::Fence9 class.

#include "libGLESv2/renderer/d3d9/Fence9.h"
#include "libGLESv2/main.h"
#include "libGLESv2/renderer/d3d9/renderer9_utils.h"
#include "libGLESv2/renderer/d3d9/Renderer9.h"

namespace rx
{

Fence9::Fence9(rx::Renderer9 *renderer)
{
    mRenderer = renderer;
    mQuery = NULL;
}

Fence9::~Fence9()
{
    SafeRelease(mQuery);
}

bool Fence9::isSet() const
{
    return mQuery != NULL;
}

void Fence9::set()
{
    if (!mQuery)
    {
        mQuery = mRenderer->allocateEventQuery();
        if (!mQuery)
        {
            return gl::error(GL_OUT_OF_MEMORY);
        }
    }

    HRESULT result = mQuery->Issue(D3DISSUE_END);
    ASSERT(SUCCEEDED(result));
}

bool Fence9::test(bool flushCommandBuffer)
{
    ASSERT(mQuery);

    DWORD getDataFlags = (flushCommandBuffer ? D3DGETDATA_FLUSH : 0);
    HRESULT result = mQuery->GetData(NULL, 0, getDataFlags);

    if (d3d9::isDeviceLostError(result))
    {
        mRenderer->notifyDeviceLost();
        return gl::error(GL_OUT_OF_MEMORY, true);
    }

    ASSERT(result == S_OK || result == S_FALSE);

    return (result == S_OK);
}

bool Fence9::hasError() const
{
    return mRenderer->isDeviceLost();
}

}
