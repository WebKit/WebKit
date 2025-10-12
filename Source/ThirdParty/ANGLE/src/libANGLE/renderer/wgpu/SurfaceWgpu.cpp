//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SurfaceWgpu.cpp:
//    Implements the class methods for SurfaceWgpu.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "libANGLE/renderer/wgpu/SurfaceWgpu.h"

#include "common/debug.h"

#include "libANGLE/Display.h"
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/wgpu/DisplayWgpu.h"
#include "libANGLE/renderer/wgpu/FramebufferWgpu.h"

namespace rx
{

constexpr WGPUTextureUsage kSurfaceTextureUsage =
    WGPUTextureUsage_TextureBinding | WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc |
    WGPUTextureUsage_CopyDst;

SurfaceWgpu::SurfaceWgpu(const egl::SurfaceState &surfaceState) : SurfaceImpl(surfaceState) {}

SurfaceWgpu::~SurfaceWgpu() {}

angle::Result SurfaceWgpu::createDepthStencilAttachment(const egl::Display *display,
                                                        uint32_t width,
                                                        uint32_t height,
                                                        const webgpu::Format &webgpuFormat,
                                                        webgpu::DeviceHandle device,
                                                        AttachmentImage *outDepthStencilAttachment)
{
    const DisplayWgpu *displayWgpu = webgpu::GetImpl(display);
    const DawnProcTable *wgpu      = displayWgpu->getProcs();

    WGPUTextureDescriptor desc = outDepthStencilAttachment->texture.createTextureDescriptor(
        kSurfaceTextureUsage, WGPUTextureDimension_2D, {width, height, 1},
        webgpuFormat.getActualWgpuTextureFormat(), 1, 1);

    constexpr uint32_t level = 0;
    constexpr uint32_t layer = 0;

    ANGLE_TRY(outDepthStencilAttachment->texture.initImage(wgpu, webgpuFormat.getIntendedFormatID(),
                                                           webgpuFormat.getActualImageFormatID(),
                                                           device, gl::LevelIndex(level), desc));

    webgpu::TextureViewHandle view;
    ANGLE_TRY(outDepthStencilAttachment->texture.createTextureViewSingleLevel(gl::LevelIndex(level),
                                                                              layer, view));
    outDepthStencilAttachment->renderTarget.set(
        &outDepthStencilAttachment->texture, view, webgpu::LevelIndex(level), layer,
        outDepthStencilAttachment->texture.toWgpuTextureFormat());
    return angle::Result::Continue;
}

OffscreenSurfaceWgpu::OffscreenSurfaceWgpu(const egl::SurfaceState &surfaceState,
                                           EGLenum clientBufferType,
                                           EGLClientBuffer clientBuffer)
    : SurfaceWgpu(surfaceState),
      mWidth(surfaceState.attributes.getAsInt(EGL_WIDTH, 0)),
      mHeight(surfaceState.attributes.getAsInt(EGL_HEIGHT, 0)),
      mClientBufferType(clientBufferType),
      mClientBuffer(clientBuffer)
{}

OffscreenSurfaceWgpu::~OffscreenSurfaceWgpu() {}

egl::Error OffscreenSurfaceWgpu::initialize(const egl::Display *display)
{
    return angle::ResultToEGL(initializeImpl(display));
}

egl::Error OffscreenSurfaceWgpu::swap(const gl::Context *context, SurfaceSwapFeedback *feedback)
{
    UNREACHABLE();
    return egl::NoError();
}

egl::Error OffscreenSurfaceWgpu::bindTexImage(const gl::Context *context,
                                              gl::Texture *texture,
                                              EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::NoError();
}

egl::Error OffscreenSurfaceWgpu::releaseTexImage(const gl::Context *context, EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::NoError();
}

void OffscreenSurfaceWgpu::setSwapInterval(const egl::Display *display, EGLint interval) {}

gl::Extents OffscreenSurfaceWgpu::getSize() const
{
    return gl::Extents(mWidth, mHeight, 1);
}


EGLint OffscreenSurfaceWgpu::getSwapBehavior() const
{
    return EGL_BUFFER_DESTROYED;
}

angle::Result OffscreenSurfaceWgpu::initializeContents(const gl::Context *context,
                                                       GLenum binding,
                                                       const gl::ImageIndex &imageIndex)
{
    UNIMPLEMENTED();
    return angle::Result::Continue;
}

egl::Error OffscreenSurfaceWgpu::attachToFramebuffer(const gl::Context *context,
                                                     gl::Framebuffer *framebuffer)
{
    UNIMPLEMENTED();
    return egl::NoError();
}

egl::Error OffscreenSurfaceWgpu::detachFromFramebuffer(const gl::Context *context,
                                                       gl::Framebuffer *framebuffer)
{
    UNIMPLEMENTED();
    return egl::NoError();
}

angle::Result OffscreenSurfaceWgpu::getAttachmentRenderTarget(
    const gl::Context *context,
    GLenum binding,
    const gl::ImageIndex &imageIndex,
    GLsizei samples,
    FramebufferAttachmentRenderTarget **rtOut)
{
    if (binding == GL_BACK)
    {
        *rtOut = &mColorAttachment.renderTarget;
    }
    else
    {
        ASSERT(binding == GL_DEPTH || binding == GL_STENCIL || binding == GL_DEPTH_STENCIL);
        *rtOut = &mDepthStencilAttachment.renderTarget;
    }

    return angle::Result::Continue;
}

angle::Result OffscreenSurfaceWgpu::initializeImpl(const egl::Display *display)
{
    DisplayWgpu *displayWgpu = webgpu::GetImpl(display);
    const DawnProcTable *wgpu   = displayWgpu->getProcs();
    webgpu::DeviceHandle device = displayWgpu->getDevice();

    if (mClientBufferType == EGL_WEBGPU_TEXTURE_ANGLE)
    {
        webgpu::TextureHandle externalTexture =
            webgpu::TextureHandle::Acquire(wgpu, reinterpret_cast<WGPUTexture>(mClientBuffer));
        ASSERT(externalTexture != nullptr);

        // Explicitly add-ref to hold on to the external reference
        wgpu->textureAddRef(externalTexture.get());

        const webgpu::Format *webgpuFormat = displayWgpu->getFormatForImportedTexture(
            mState.attributes, wgpu->textureGetFormat(externalTexture.get()));
        ASSERT(webgpuFormat);
        ANGLE_TRY(mColorAttachment.texture.initExternal(wgpu, webgpuFormat->getIntendedFormatID(),
                                                        webgpuFormat->getActualImageFormatID(),
                                                        externalTexture));

        webgpu::TextureViewHandle view;
        ANGLE_TRY(
            mColorAttachment.texture.createTextureViewSingleLevel(gl::LevelIndex(0), 0, view));

        mColorAttachment.renderTarget.set(&mColorAttachment.texture, view, webgpu::LevelIndex(0), 0,
                                          mColorAttachment.texture.toWgpuTextureFormat());

        mWidth  = mColorAttachment.texture.getSize().width;
        mHeight = mColorAttachment.texture.getSize().height;
    }
    else
    {
        const egl::Config *config = mState.config;

        if (config->renderTargetFormat != GL_NONE)
        {
            const webgpu::Format &webgpuFormat = displayWgpu->getFormat(config->renderTargetFormat);
            WGPUTextureDescriptor desc         = mColorAttachment.texture.createTextureDescriptor(
                kSurfaceTextureUsage, WGPUTextureDimension_2D,
                {static_cast<uint32_t>(mWidth), static_cast<uint32_t>(mHeight), 1},
                webgpuFormat.getActualWgpuTextureFormat(), 1, 1);

            constexpr uint32_t level = 0;
            constexpr uint32_t layer = 0;

            ANGLE_TRY(mColorAttachment.texture.initImage(wgpu, webgpuFormat.getIntendedFormatID(),
                                                         webgpuFormat.getActualImageFormatID(),
                                                         device, gl::LevelIndex(level), desc));

            webgpu::TextureViewHandle view;
            ANGLE_TRY(mColorAttachment.texture.createTextureViewSingleLevel(gl::LevelIndex(level),
                                                                            layer, view));
            mColorAttachment.renderTarget.set(&mColorAttachment.texture, view,
                                              webgpu::LevelIndex(level), layer,
                                              mColorAttachment.texture.toWgpuTextureFormat());
        }

        if (config->depthStencilFormat != GL_NONE)
        {
            const webgpu::Format &webgpuFormat = displayWgpu->getFormat(config->depthStencilFormat);
            ANGLE_TRY(createDepthStencilAttachment(display, static_cast<uint32_t>(mWidth),
                                                   static_cast<uint32_t>(mHeight), webgpuFormat,
                                                   device, &mDepthStencilAttachment));
        }
    }

    return angle::Result::Continue;
}

WindowSurfaceWgpu::WindowSurfaceWgpu(const egl::SurfaceState &surfaceState,
                                     EGLNativeWindowType window)
    : SurfaceWgpu(surfaceState), mNativeWindow(window)
{}

WindowSurfaceWgpu::~WindowSurfaceWgpu() {}

egl::Error WindowSurfaceWgpu::initialize(const egl::Display *display)
{
    return angle::ResultToEGL(initializeImpl(display));
}

void WindowSurfaceWgpu::destroy(const egl::Display *display)
{
    mSurface   = nullptr;
    mColorAttachment.renderTarget.reset();
    mColorAttachment.texture.resetImage();
    mDepthStencilAttachment.renderTarget.reset();
    mDepthStencilAttachment.texture.resetImage();
}

egl::Error WindowSurfaceWgpu::swap(const gl::Context *context, SurfaceSwapFeedback *feedback)
{
    return angle::ResultToEGL(swapImpl(context));
}

egl::Error WindowSurfaceWgpu::bindTexImage(const gl::Context *context,
                                           gl::Texture *texture,
                                           EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::NoError();
}

egl::Error WindowSurfaceWgpu::releaseTexImage(const gl::Context *context, EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::NoError();
}

void WindowSurfaceWgpu::setSwapInterval(const egl::Display *display, EGLint interval)
{
    UNIMPLEMENTED();
}

gl::Extents WindowSurfaceWgpu::getSize() const
{
    return mCurrentSurfaceSize;
}

EGLint WindowSurfaceWgpu::getSwapBehavior() const
{
    UNIMPLEMENTED();
    return 0;
}

angle::Result WindowSurfaceWgpu::initializeContents(const gl::Context *context,
                                                    GLenum binding,
                                                    const gl::ImageIndex &imageIndex)
{
    UNIMPLEMENTED();
    return angle::Result::Continue;
}

egl::Error WindowSurfaceWgpu::attachToFramebuffer(const gl::Context *context,
                                                  gl::Framebuffer *framebuffer)
{
    FramebufferWgpu *framebufferWgpu = GetImplAs<FramebufferWgpu>(framebuffer);
    framebufferWgpu->setFlipY(true);
    return egl::NoError();
}

egl::Error WindowSurfaceWgpu::detachFromFramebuffer(const gl::Context *context,
                                                    gl::Framebuffer *framebuffer)
{
    FramebufferWgpu *framebufferWgpu = GetImplAs<FramebufferWgpu>(framebuffer);
    framebufferWgpu->setFlipY(false);
    return egl::NoError();
}

angle::Result WindowSurfaceWgpu::getAttachmentRenderTarget(
    const gl::Context *context,
    GLenum binding,
    const gl::ImageIndex &imageIndex,
    GLsizei samples,
    FramebufferAttachmentRenderTarget **rtOut)
{
    if (binding == GL_BACK)
    {
        *rtOut = &mColorAttachment.renderTarget;
    }
    else
    {
        ASSERT(binding == GL_DEPTH || binding == GL_STENCIL || binding == GL_DEPTH_STENCIL);
        *rtOut = &mDepthStencilAttachment.renderTarget;
    }

    return angle::Result::Continue;
}

angle::Result WindowSurfaceWgpu::initializeImpl(const egl::Display *display)
{
    DisplayWgpu *displayWgpu = webgpu::GetImpl(display);
    const DawnProcTable *wgpu     = displayWgpu->getProcs();
    webgpu::AdapterHandle adapter = displayWgpu->getAdapter();

    ANGLE_TRY(createWgpuSurface(display, &mSurface));

    gl::Extents size;
    ANGLE_TRY(getCurrentWindowSize(display, &size));

    WGPUSurfaceCapabilities surfaceCapabilities = WGPU_SURFACE_CAPABILITIES_INIT;
    WGPUStatus getCapabilitiesStatus =
        wgpu->surfaceGetCapabilities(mSurface.get(), adapter.get(), &surfaceCapabilities);
    if (getCapabilitiesStatus != WGPUStatus_Success)
    {
        ERR() << "wgpuSurfaceGetCapabilities failed: "
              << gl::FmtHex(static_cast<uint32_t>(getCapabilitiesStatus));
        return angle::Result::Stop;
    }

    const egl::Config *config = mState.config;
    ASSERT(config->renderTargetFormat != GL_NONE);
    mSurfaceTextureFormat = &displayWgpu->getFormat(config->renderTargetFormat);
    ASSERT(std::find(surfaceCapabilities.formats,
                     surfaceCapabilities.formats + surfaceCapabilities.formatCount,
                     mSurfaceTextureFormat->getActualWgpuTextureFormat()) !=
           (surfaceCapabilities.formats + surfaceCapabilities.formatCount));

    mSurfaceTextureUsage =
        WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst;
    ASSERT((surfaceCapabilities.usages & mSurfaceTextureUsage) == mSurfaceTextureUsage);

    // Default to the always supported Fifo present mode. Use Mailbox if it's available.
    mPresentMode = WGPUPresentMode_Fifo;
    for (size_t i = 0; i < surfaceCapabilities.presentModeCount; i++)
    {
        if (surfaceCapabilities.presentModes[i] == WGPUPresentMode_Mailbox)
        {
            mPresentMode = WGPUPresentMode_Mailbox;
        }
    }

    if (config->depthStencilFormat != GL_NONE)
    {
        mDepthStencilFormat = &displayWgpu->getFormat(config->depthStencilFormat);
    }
    else
    {
        mDepthStencilFormat = nullptr;
    }

    ANGLE_TRY(configureSurface(display, size));
    ANGLE_TRY(updateCurrentTexture(display));

    return angle::Result::Continue;
}

angle::Result WindowSurfaceWgpu::swapImpl(const gl::Context *context)
{
    const egl::Display *display = context->getDisplay();
    ContextWgpu *contextWgpu    = webgpu::GetImpl(context);
    const DawnProcTable *wgpu   = webgpu::GetProcs(contextWgpu);

    ANGLE_TRY(contextWgpu->flush(webgpu::RenderPassClosureReason::EGLSwapBuffers));

    wgpu->surfacePresent(mSurface.get());

    gl::Extents size;
    ANGLE_TRY(getCurrentWindowSize(display, &size));
    if (size != mCurrentSurfaceSize)
    {
        ANGLE_TRY(configureSurface(display, size));
    }

    ANGLE_TRY(updateCurrentTexture(display));

    return angle::Result::Continue;
}

angle::Result WindowSurfaceWgpu::configureSurface(const egl::Display *display,
                                                  const gl::Extents &size)
{
    DisplayWgpu *displayWgpu = webgpu::GetImpl(display);
    const DawnProcTable *wgpu   = displayWgpu->getProcs();
    webgpu::DeviceHandle device = displayWgpu->getDevice();

    ASSERT(mSurfaceTextureFormat != nullptr);

    WGPUSurfaceConfiguration surfaceConfig   = WGPU_SURFACE_CONFIGURATION_INIT;
    surfaceConfig.device                     = device.get();
    surfaceConfig.format                     = mSurfaceTextureFormat->getActualWgpuTextureFormat();
    surfaceConfig.usage                      = mSurfaceTextureUsage;
    surfaceConfig.width                      = size.width;
    surfaceConfig.height                     = size.height;
    surfaceConfig.presentMode                = mPresentMode;

    wgpu->surfaceConfigure(mSurface.get(), &surfaceConfig);

    if (mDepthStencilFormat)
    {
        ANGLE_TRY(createDepthStencilAttachment(
            display, static_cast<uint32_t>(size.width), static_cast<uint32_t>(size.height),
            *mDepthStencilFormat, device, &mDepthStencilAttachment));
    }

    mCurrentSurfaceSize = size;
    return angle::Result::Continue;
}

angle::Result WindowSurfaceWgpu::updateCurrentTexture(const egl::Display *display)
{
    DisplayWgpu *displayWgpu  = webgpu::GetImpl(display);
    const DawnProcTable *wgpu = displayWgpu->getProcs();

    WGPUSurfaceTexture surfaceTexture = WGPU_SURFACE_TEXTURE_INIT;
    wgpu->surfaceGetCurrentTexture(mSurface.get(), &surfaceTexture);
    webgpu::TextureHandle texture = webgpu::TextureHandle::Acquire(wgpu, surfaceTexture.texture);
    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal)
    {
        ERR() << "wgpuSurfaceGetCurrentTexture failed: "
              << gl::FmtHex(static_cast<uint32_t>(surfaceTexture.status));
        return angle::Result::Stop;
    }

    WGPUTextureFormat wgpuFormat   = wgpu->textureGetFormat(texture.get());
    angle::FormatID angleFormat    = webgpu::GetFormatIDFromWgpuTextureFormat(wgpuFormat);

    ANGLE_TRY(mColorAttachment.texture.initExternal(wgpu, angleFormat, angleFormat, texture));

    webgpu::TextureViewHandle view;
    ANGLE_TRY(mColorAttachment.texture.createTextureViewSingleLevel(gl::LevelIndex(0), 0, view));

    mColorAttachment.renderTarget.set(&mColorAttachment.texture, view, webgpu::LevelIndex(0), 0,
                                      wgpuFormat);

    return angle::Result::Continue;
}

}  // namespace rx
