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
    : DisplayImpl(state), mCurrentDrawSurface(nullptr)
{}

DisplayGL::~DisplayGL() {}

egl::Error DisplayGL::initialize(egl::Display *display)
{
    return egl::NoError();
}

void DisplayGL::terminate() {}

ImageImpl *DisplayGL::createImage(const egl::ImageState &state,
                                  const gl::Context *context,
                                  EGLenum target,
                                  const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return nullptr;
}

StreamProducerImpl *DisplayGL::createStreamProducerD3DTexture(
    egl::Stream::ConsumerType consumerType,
    const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return nullptr;
}

egl::Error DisplayGL::makeCurrent(egl::Surface *drawSurface,
                                  egl::Surface *readSurface,
                                  gl::Context *context)
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
        return egl::NoError();
    }

    // Pause transform feedback before making a new surface current, to workaround anglebug.com/1426
    ContextGL *glContext = GetImplAs<ContextGL>(context);
    glContext->getStateManager()->pauseTransformFeedback();

    if (drawSurface != nullptr)
    {
        SurfaceGL *glDrawSurface = GetImplAs<SurfaceGL>(drawSurface);
        ANGLE_TRY(glDrawSurface->makeCurrent(context));
        mCurrentDrawSurface = drawSurface;
        return egl::NoError();
    }
    else
    {
        return makeCurrentSurfaceless(context);
    }
}

void DisplayGL::generateExtensions(egl::DisplayExtensions *outExtensions) const
{
    // Advertise robust resource initialization on all OpenGL backends for testing even though it is
    // not fully implemented.
    outExtensions->robustResourceInitialization = true;
}

egl::Error DisplayGL::makeCurrentSurfaceless(gl::Context *context)
{
    UNIMPLEMENTED();
    return egl::NoError();
}
}  // namespace rx
