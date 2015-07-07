#include "precompiled.h"
//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Image.h: Implements the rx::Image class, an abstract base class for the 
// renderer-specific classes which will define the interface to the underlying
// surfaces or resources.

#include "libGLESv2/renderer/Image.h"

namespace rx
{

Image::Image()
{
    mWidth = 0; 
    mHeight = 0;
    mDepth = 0;
    mInternalFormat = GL_NONE;
    mActualFormat = GL_NONE;
    mTarget = GL_NONE;
    mRenderable = false;
    mDirty = false;
}

}
