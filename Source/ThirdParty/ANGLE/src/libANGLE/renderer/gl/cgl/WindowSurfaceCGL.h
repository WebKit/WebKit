//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// WindowSurfaceCGL.h: CGL implementation of egl::Surface for windows

#ifndef LIBANGLE_RENDERER_GL_CGL_WINDOWSURFACECGL_H_
#define LIBANGLE_RENDERER_GL_CGL_WINDOWSURFACECGL_H_

#include "libANGLE/renderer/gl/SurfaceGL.h"

struct _CGLContextObject;
typedef _CGLContextObject *CGLContextObj;
@class CALayer;
struct __IOSurface;
typedef __IOSurface *IOSurfaceRef;

@class SwapLayer;

namespace rx
{

class DisplayCGL;
class FramebufferGL;
class FunctionsGL;
class StateManagerGL;
struct WorkaroundsGL;

struct SharedSwapState
{
    struct SwapTexture
    {
        GLuint texture;
        unsigned int width;
        unsigned int height;
        uint64_t swapId;
    };

    SwapTexture textures[3];

    // This code path is not going to be used by Chrome so we take the liberty
    // to use pthreads directly instead of using mutexes and condition variables
    // via the Platform API.
    pthread_mutex_t mutex;
    // The following members should be accessed only when holding the mutex
    // (or doing construction / destruction)
    SwapTexture *beingRendered;
    SwapTexture *lastRendered;
    SwapTexture *beingPresented;
};

class WindowSurfaceCGL : public SurfaceGL
{
  public:
    WindowSurfaceCGL(const egl::SurfaceState &state,
                     RendererGL *renderer,
                     CALayer *layer,
                     const FunctionsGL *functions,
                     CGLContextObj context);
    ~WindowSurfaceCGL() override;

    egl::Error initialize() override;
    egl::Error makeCurrent() override;

    egl::Error swap() override;
    egl::Error postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height) override;
    egl::Error querySurfacePointerANGLE(EGLint attribute, void **value) override;
    egl::Error bindTexImage(gl::Texture *texture, EGLint buffer) override;
    egl::Error releaseTexImage(EGLint buffer) override;
    void setSwapInterval(EGLint interval) override;

    EGLint getWidth() const override;
    EGLint getHeight() const override;

    EGLint isPostSubBufferSupported() const override;
    EGLint getSwapBehavior() const override;

    FramebufferImpl *createDefaultFramebuffer(const gl::FramebufferState &state) override;

  private:
    SwapLayer *mSwapLayer;
    SharedSwapState mSwapState;
    uint64_t mCurrentSwapId;

    CALayer *mLayer;
    CGLContextObj mContext;
    const FunctionsGL *mFunctions;
    StateManagerGL *mStateManager;
    RendererGL *mRenderer;
    const WorkaroundsGL &mWorkarounds;

    GLuint mFramebuffer;
    GLuint mDSRenderbuffer;
};

}

#endif // LIBANGLE_RENDERER_GL_CGL_WINDOWSURFACECGL_H_
