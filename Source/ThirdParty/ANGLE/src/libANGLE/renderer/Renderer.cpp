//
// Copyright (c) 2012-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Renderer.cpp: Implements EGL dependencies for creating and destroying Renderer instances.

#include "common/utilities.h"
#include "libANGLE/AttributeMap.h"
#include "libANGLE/renderer/Renderer.h"

#include <EGL/eglext.h>

namespace rx
{

Renderer::Renderer()
    : mCapsInitialized(false),
      mWorkaroundsInitialized(false)
{
}

Renderer::~Renderer()
{
}

const gl::Caps &Renderer::getRendererCaps() const
{
    if (!mCapsInitialized)
    {
        generateCaps(&mCaps, &mTextureCaps, &mExtensions);
        mCapsInitialized = true;
    }

    return mCaps;
}

const gl::TextureCapsMap &Renderer::getRendererTextureCaps() const
{
    if (!mCapsInitialized)
    {
        generateCaps(&mCaps, &mTextureCaps, &mExtensions);
        mCapsInitialized = true;
    }

    return mTextureCaps;
}

const gl::Extensions &Renderer::getRendererExtensions() const
{
    if (!mCapsInitialized)
    {
        generateCaps(&mCaps, &mTextureCaps, &mExtensions);
        mCapsInitialized = true;
    }

    return mExtensions;
}

const Workarounds &Renderer::getWorkarounds() const
{
    if (!mWorkaroundsInitialized)
    {
        mWorkarounds = generateWorkarounds();
        mWorkaroundsInitialized = true;
    }

    return mWorkarounds;
}

}
