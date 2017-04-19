//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayGL.h: GL implementation of egl::Display

#include "libANGLE/renderer/gl/DisplayGL.h"

#include "libANGLE/AttributeMap.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/gl/ContextGL.h"
#include "libANGLE/renderer/gl/RendererGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"
#include "libANGLE/renderer/gl/SurfaceGL.h"

#include <EGL/eglext.h>

namespace rx
{

DisplayGL::DisplayGL(const egl::DisplayState &state)
    : DisplayImpl(state), mRenderer(nullptr), mCurrentDrawSurface(nullptr)
{
}

DisplayGL::~DisplayGL()
{
}

egl::Error DisplayGL::initialize(egl::Display *display)
{
    mRenderer = new RendererGL(getFunctionsGL(), display->getAttributeMap());

    const gl::Version &maxVersion = mRenderer->getMaxSupportedESVersion();
    if (maxVersion < gl::Version(2, 0))
    {
        return egl::Error(EGL_NOT_INITIALIZED, "OpenGL ES 2.0 is not supportable.");
    }

    return egl::Error(EGL_SUCCESS);
}

void DisplayGL::terminate()
{
    SafeDelete(mRenderer);
}

ImageImpl *DisplayGL::createImage(EGLenum target,
                                  egl::ImageSibling *buffer,
                                  const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return nullptr;
}

ContextImpl *DisplayGL::createContext(const gl::ContextState &state)
{
    ASSERT(mRenderer != nullptr);
    return new ContextGL(state, mRenderer);
}

StreamProducerImpl *DisplayGL::createStreamProducerD3DTextureNV12(
    egl::Stream::ConsumerType consumerType,
    const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return nullptr;
}

egl::Error DisplayGL::makeCurrent(egl::Surface *drawSurface, egl::Surface *readSurface, gl::Context *context)
{
    // Notify the previous surface (if it still exists) that it is no longer current
    if (mCurrentDrawSurface &&
        mState.surfaceSet.find(mCurrentDrawSurface) != mState.surfaceSet.end())
    {
        ANGLE_TRY(GetImplAs<SurfaceGL>(mCurrentDrawSurface)->unMakeCurrent());
    }
    mCurrentDrawSurface = nullptr;

    if (!context)
    {
        return egl::Error(EGL_SUCCESS);
    }

    // Pause transform feedback before making a new surface current, to workaround anglebug.com/1426
    ContextGL *glContext = GetImplAs<ContextGL>(context);
    glContext->getStateManager()->pauseTransformFeedback();

    if (drawSurface != nullptr)
    {
        SurfaceGL *glDrawSurface = GetImplAs<SurfaceGL>(drawSurface);
        ANGLE_TRY(glDrawSurface->makeCurrent());
        mCurrentDrawSurface = drawSurface;
        return egl::Error(EGL_SUCCESS);
    }
    else
    {
        return makeCurrentSurfaceless(context);
    }
}

gl::Version DisplayGL::getMaxSupportedESVersion() const
{
    ASSERT(mRenderer != nullptr);
    return mRenderer->getMaxSupportedESVersion();
}

egl::Error DisplayGL::makeCurrentSurfaceless(gl::Context *context)
{
    UNIMPLEMENTED();
    return egl::NoError();
}
}
