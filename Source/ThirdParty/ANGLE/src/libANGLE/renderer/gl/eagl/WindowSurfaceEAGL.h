//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// WindowSurfaceEAGL.h: EAGL implementation of egl::Surface

#ifndef LIBANGLE_RENDERER_GL_EAGL_WINDOWSURFACEEAGL_H_
#define LIBANGLE_RENDERER_GL_EAGL_WINDOWSURFACEEAGL_H_

#include "libANGLE/renderer/gl/SurfaceGL.h"

#ifdef __OBJC__
@class EAGLContext;
typedef EAGLContext *EAGLContextObj;
#else
typedef void *EAGLContextObj;
#endif
@class CALayer;
struct __IOSurface;
typedef __IOSurface *IOSurfaceRef;

// WebKit's build process requires that every Objective-C class name has the prefix "Web".
@class WebSwapLayerEAGL;

namespace rx
{

class DisplayEAGL;
class FramebufferGL;
class FunctionsGL;
class RendererGL;
class StateManagerGL;

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

class WindowSurfaceEAGL : public SurfaceGL
{
  public:
    WindowSurfaceEAGL(const egl::SurfaceState &state,
                      RendererGL *renderer,
                      EGLNativeWindowType layer,
                      EAGLContextObj context);
    ~WindowSurfaceEAGL() override;

    egl::Error initialize(const egl::Display *display) override;
    egl::Error makeCurrent(const gl::Context *context) override;

    egl::Error swap(const gl::Context *context) override;
    egl::Error postSubBuffer(const gl::Context *context,
                             EGLint x,
                             EGLint y,
                             EGLint width,
                             EGLint height) override;
    egl::Error querySurfacePointerANGLE(EGLint attribute, void **value) override;
    egl::Error bindTexImage(const gl::Context *context,
                            gl::Texture *texture,
                            EGLint buffer) override;
    egl::Error releaseTexImage(const gl::Context *context, EGLint buffer) override;
    void setSwapInterval(EGLint interval) override;

    EGLint getWidth() const override;
    EGLint getHeight() const override;

    EGLint isPostSubBufferSupported() const override;
    EGLint getSwapBehavior() const override;

    FramebufferImpl *createDefaultFramebuffer(const gl::Context *context,
                                              const gl::FramebufferState &state) override;

  private:
    WebSwapLayerEAGL *mSwapLayer;
    SharedSwapState mSwapState;
    uint64_t mCurrentSwapId;

    CALayer *mLayer;
    EAGLContextObj mContext;
    const FunctionsGL *mFunctions;
    StateManagerGL *mStateManager;

    GLuint mDSRenderbuffer;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_EAGL_WINDOWSURFACEEAGL_H_
