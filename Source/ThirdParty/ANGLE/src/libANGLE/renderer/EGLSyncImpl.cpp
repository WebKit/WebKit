//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// EGLSyncImpl.cpp: Implements the rx::EGLSyncImpl class.

#include "libANGLE/renderer/EGLReusableSync.h"

#include "angle_gl.h"

#include "common/utilities.h"

namespace rx
{

egl::Error EGLSyncImpl::signal(const egl::ThreadSafeDisplay *display,
                               const gl::Context *context,
                               EGLint mode)
{
    UNREACHABLE();
    return egl::Error(EGL_BAD_MATCH);
}

egl::Error EGLSyncImpl::copyMetalSharedEventANGLE(const egl::ThreadSafeDisplay *display,
                                                  void **eventOut) const
{
    UNREACHABLE();
    return egl::Error(EGL_BAD_MATCH);
}

egl::Error EGLSyncImpl::dupNativeFenceFD(const egl::ThreadSafeDisplay *display, EGLint *fdOut) const
{
    UNREACHABLE();
    return egl::Error(EGL_BAD_MATCH);
}

}  // namespace rx
