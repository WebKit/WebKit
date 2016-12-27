//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DXGISwapChainWindowSurfaceWGL.h: WGL implementation of egl::Surface for windows using a DXGI
// swapchain.

#ifndef LIBANGLE_RENDERER_GL_WGL_DXGISWAPCHAINSURFACEWGL_H_
#define LIBANGLE_RENDERER_GL_WGL_DXGISWAPCHAINSURFACEWGL_H_

#include "libANGLE/renderer/gl/SurfaceGL.h"

#include <GL/wglext.h>

namespace rx
{

class FunctionsGL;
class FunctionsWGL;
class DisplayWGL;
class StateManagerGL;
struct WorkaroundsGL;

class DXGISwapChainWindowSurfaceWGL : public SurfaceGL
{
  public:
    DXGISwapChainWindowSurfaceWGL(const egl::SurfaceState &state,
                                  RendererGL *renderer,
                                  EGLNativeWindowType window,
                                  ID3D11Device *device,
                                  HANDLE deviceHandle,
                                  HGLRC wglContext,
                                  HDC deviceContext,
                                  const FunctionsGL *functionsGL,
                                  const FunctionsWGL *functionsWGL,
                                  EGLint orientation);
    ~DXGISwapChainWindowSurfaceWGL() override;

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

    FramebufferImpl *createDefaultFramebuffer(const gl::FramebufferState &data) override;

  private:
    egl::Error setObjectsLocked(bool locked);
    egl::Error checkForResize();

    egl::Error createSwapChain();

    EGLNativeWindowType mWindow;

    StateManagerGL *mStateManager;
    const WorkaroundsGL &mWorkarounds;
    RendererGL *mRenderer;
    const FunctionsGL *mFunctionsGL;
    const FunctionsWGL *mFunctionsWGL;

    ID3D11Device *mDevice;
    HANDLE mDeviceHandle;

    HDC mWGLDevice;
    HGLRC mWGLContext;

    DXGI_FORMAT mSwapChainFormat;
    UINT mSwapChainFlags;
    GLenum mDepthBufferFormat;

    bool mFirstSwap;
    IDXGISwapChain *mSwapChain;
    IDXGISwapChain1 *mSwapChain1;

    GLuint mColorRenderbufferID;
    HANDLE mRenderbufferBufferHandle;

    GLuint mDepthRenderbufferID;

    GLuint mFramebufferID;

    GLuint mTextureID;
    HANDLE mTextureHandle;

    size_t mWidth;
    size_t mHeight;

    EGLint mSwapInterval;

    EGLint mOrientation;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_WGL_DXGISWAPCHAINSURFACEWGL_H_
