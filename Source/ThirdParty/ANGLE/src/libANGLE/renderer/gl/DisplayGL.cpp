//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayGL.h: GL implementation of egl::Display

#include "libANGLE/renderer/gl/DisplayGL.h"

#include "libANGLE/AttributeMap.h"
#include "libANGLE/Context.h"
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/gl/RendererGL.h"
#include "libANGLE/renderer/gl/SurfaceGL.h"

#include <EGL/eglext.h>

namespace rx
{

DisplayGL::DisplayGL()
    : mRenderer(nullptr)
{
}

DisplayGL::~DisplayGL()
{
}

egl::Error DisplayGL::initialize(egl::Display *display)
{
    mRenderer = new RendererGL(getFunctionsGL());
    return egl::Error(EGL_SUCCESS);
}

void DisplayGL::terminate()
{
    SafeDelete(mRenderer);
}

egl::Error DisplayGL::createContext(const egl::Config *config, const gl::Context *shareContext, const egl::AttributeMap &attribs, gl::Context **outContext)
{
    ASSERT(mRenderer != nullptr);

    EGLint clientVersion = attribs.get(EGL_CONTEXT_CLIENT_VERSION, 1);
    bool notifyResets = (attribs.get(EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT, EGL_NO_RESET_NOTIFICATION_EXT) == EGL_LOSE_CONTEXT_ON_RESET_EXT);
    bool robustAccess = (attribs.get(EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT, EGL_FALSE) == EGL_TRUE);

    *outContext = new gl::Context(config, clientVersion, shareContext, mRenderer, notifyResets, robustAccess);
    return egl::Error(EGL_SUCCESS);
}

egl::Error DisplayGL::makeCurrent(egl::Surface *drawSurface, egl::Surface *readSurface, gl::Context *context)
{
    if (!drawSurface)
    {
        return egl::Error(EGL_SUCCESS);
    }

    SurfaceGL *glDrawSurface = GetImplAs<SurfaceGL>(drawSurface);
    return glDrawSurface->makeCurrent();
}

}
