//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SurfaceImpl.cpp: Implementation of Surface stub method class

#include "libANGLE/renderer/SurfaceImpl.h"

namespace rx
{

SurfaceImpl::SurfaceImpl(const egl::SurfaceState &state) : mState(state)
{
}

SurfaceImpl::~SurfaceImpl()
{
}

egl::Error SurfaceImpl::swapWithDamage(const gl::Context *context, EGLint *rects, EGLint n_rects)
{
    UNREACHABLE();
    return egl::EglBadSurface() << "swapWithDamage implementation missing.";
}

}  // namespace rx
