//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SurfaceWgpu.h:
//    Defines the class interface for SurfaceWgpu, implementing SurfaceImpl.
//

#ifndef LIBANGLE_RENDERER_WGPU_SURFACEWGPU_H_
#define LIBANGLE_RENDERER_WGPU_SURFACEWGPU_H_

#include "libANGLE/renderer/SurfaceImpl.h"

#include "libANGLE/renderer/wgpu/RenderTargetWgpu.h"
#include "libANGLE/renderer/wgpu/wgpu_helpers.h"

#include <webgpu/webgpu.h>

namespace rx
{

class SurfaceWgpu : public SurfaceImpl
{
  public:
    SurfaceWgpu(const egl::SurfaceState &surfaceState);
    ~SurfaceWgpu() override;

  protected:
    struct AttachmentImage
    {
        webgpu::ImageHelper texture;
        RenderTargetWgpu renderTarget;
    };
    angle::Result createDepthStencilAttachment(const egl::Display *display,
                                               uint32_t width,
                                               uint32_t height,
                                               const webgpu::Format &webgpuFormat,
                                               webgpu::DeviceHandle device,
                                               AttachmentImage *outDepthStencilAttachment);
};

class OffscreenSurfaceWgpu : public SurfaceWgpu
{
  public:
    OffscreenSurfaceWgpu(const egl::SurfaceState &surfaceState,
                         EGLenum clientBufferType,
                         EGLClientBuffer clientBuffer);
    ~OffscreenSurfaceWgpu() override;

    egl::Error initialize(const egl::Display *display) override;
    egl::Error swap(const gl::Context *context, SurfaceSwapFeedback *feedback) override;
    egl::Error bindTexImage(const gl::Context *context,
                            gl::Texture *texture,
                            EGLint buffer) override;
    egl::Error releaseTexImage(const gl::Context *context, EGLint buffer) override;
    void setSwapInterval(const egl::Display *display, EGLint interval) override;

    // size can change with client window resizing
    gl::Extents getSize() const override;

    EGLint getSwapBehavior() const override;

    angle::Result initializeContents(const gl::Context *context,
                                     GLenum binding,
                                     const gl::ImageIndex &imageIndex) override;

    egl::Error attachToFramebuffer(const gl::Context *context,
                                   gl::Framebuffer *framebuffer) override;
    egl::Error detachFromFramebuffer(const gl::Context *context,
                                     gl::Framebuffer *framebuffer) override;

    angle::Result getAttachmentRenderTarget(const gl::Context *context,
                                            GLenum binding,
                                            const gl::ImageIndex &imageIndex,
                                            GLsizei samples,
                                            FramebufferAttachmentRenderTarget **rtOut) override;

  private:
    angle::Result initializeImpl(const egl::Display *display);

    EGLint mWidth;
    EGLint mHeight;

    EGLenum mClientBufferType     = EGL_NONE;
    EGLClientBuffer mClientBuffer = nullptr;

    AttachmentImage mColorAttachment;
    AttachmentImage mDepthStencilAttachment;
};

class WindowSurfaceWgpu : public SurfaceWgpu
{
  public:
    WindowSurfaceWgpu(const egl::SurfaceState &surfaceState, EGLNativeWindowType window);
    ~WindowSurfaceWgpu() override;

    egl::Error initialize(const egl::Display *display) override;
    void destroy(const egl::Display *display) override;
    egl::Error swap(const gl::Context *context, SurfaceSwapFeedback *feedback) override;
    egl::Error bindTexImage(const gl::Context *context,
                            gl::Texture *texture,
                            EGLint buffer) override;
    egl::Error releaseTexImage(const gl::Context *context, EGLint buffer) override;
    void setSwapInterval(const egl::Display *display, EGLint interval) override;

    // size can change with client window resizing
    gl::Extents getSize() const override;

    EGLint getSwapBehavior() const override;

    angle::Result initializeContents(const gl::Context *context,
                                     GLenum binding,
                                     const gl::ImageIndex &imageIndex) override;

    egl::Error attachToFramebuffer(const gl::Context *context,
                                   gl::Framebuffer *framebuffer) override;
    egl::Error detachFromFramebuffer(const gl::Context *context,
                                     gl::Framebuffer *framebuffer) override;

    angle::Result getAttachmentRenderTarget(const gl::Context *context,
                                            GLenum binding,
                                            const gl::ImageIndex &imageIndex,
                                            GLsizei samples,
                                            FramebufferAttachmentRenderTarget **rtOut) override;

  protected:
    EGLNativeWindowType getNativeWindow() const { return mNativeWindow; }

  private:
    angle::Result initializeImpl(const egl::Display *display);

    angle::Result swapImpl(const gl::Context *context);

    angle::Result configureSurface(const egl::Display *display, const gl::Extents &size);
    angle::Result updateCurrentTexture(const egl::Display *display);

    virtual angle::Result createWgpuSurface(const egl::Display *display,
                                            webgpu::SurfaceHandle *outSurface) = 0;
    virtual angle::Result getCurrentWindowSize(const egl::Display *display,
                                               gl::Extents *outSize)   = 0;

    EGLNativeWindowType mNativeWindow;
    webgpu::SurfaceHandle mSurface;

    const webgpu::Format *mSurfaceTextureFormat = nullptr;
    WGPUTextureUsage mSurfaceTextureUsage;
    WGPUPresentMode mPresentMode;

    const webgpu::Format *mDepthStencilFormat = nullptr;

    gl::Extents mCurrentSurfaceSize;

    AttachmentImage mColorAttachment;
    AttachmentImage mDepthStencilAttachment;
};

WindowSurfaceWgpu *CreateWgpuWindowSurface(const egl::SurfaceState &surfaceState,
                                           EGLNativeWindowType window);

}  // namespace rx

#endif  // LIBANGLE_RENDERER_WGPU_SURFACEWGPU_H_
