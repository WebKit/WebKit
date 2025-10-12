//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RenderbufferWgpu.cpp:
//    Implements the class methods for RenderbufferWgpu.
//

#include "libANGLE/renderer/wgpu/RenderbufferWgpu.h"

#include "common/debug.h"
#include "libANGLE/renderer/wgpu/ImageWgpu.h"

namespace rx
{
namespace
{
constexpr angle::SubjectIndex kRenderbufferImageSubjectIndex = 0;
}

RenderbufferWgpu::RenderbufferWgpu(const gl::RenderbufferState &state)
    : RenderbufferImpl(state), mImageObserverBinding(this, kRenderbufferImageSubjectIndex)
{
    setImageHelper(new webgpu::ImageHelper(), true);
}

RenderbufferWgpu::~RenderbufferWgpu() {}

void RenderbufferWgpu::onDestroy(const gl::Context *context)
{
    setImageHelper(nullptr, true);
}

angle::Result RenderbufferWgpu::setStorage(const gl::Context *context,
                                           GLenum internalformat,
                                           GLsizei width,
                                           GLsizei height)
{
    return setStorageMultisample(context, 1, internalformat, width, height,
                                 gl::MultisamplingMode::Regular);
}

angle::Result RenderbufferWgpu::setStorageMultisample(const gl::Context *context,
                                                      GLsizei samples,
                                                      GLenum internalformat,
                                                      GLsizei width,
                                                      GLsizei height,
                                                      gl::MultisamplingMode mode)
{
    ASSERT(mode == gl::MultisamplingMode::Regular);

    if (width == 0 || height == 0)
    {
        mImage->resetImage();
        return angle::Result::Continue;
    }

    const DawnProcTable *wgpu          = webgpu::GetProcs(context);
    ContextWgpu *contextWgpu           = webgpu::GetImpl(context);
    const webgpu::Format &webgpuFormat = contextWgpu->getFormat(internalformat);

    constexpr WGPUTextureUsage kUsage = WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst |
                                        WGPUTextureUsage_RenderAttachment |
                                        WGPUTextureUsage_TextureBinding;

    WGPUTextureDescriptor desc = mImage->createTextureDescriptor(
        kUsage, WGPUTextureDimension_2D, gl_wgpu::GetExtent3D(gl::Extents(width, height, 1)),
        webgpuFormat.getActualWgpuTextureFormat(), /*mipLevelCount*/ 1, samples);

    ANGLE_TRY(mImage->initImage(wgpu, webgpuFormat.getIntendedFormatID(),
                                webgpuFormat.getActualImageFormatID(), contextWgpu->getDevice(),
                                gl::LevelIndex(0), desc));

    return angle::Result::Continue;
}

angle::Result RenderbufferWgpu::setStorageEGLImageTarget(const gl::Context *context,
                                                         egl::Image *image)
{
    ImageWgpu *imageWgpu = webgpu::GetImpl(image);
    setImageHelper(imageWgpu->getImage(), false);
    ASSERT(mImage->isInitialized());

    return angle::Result::Continue;
}

angle::Result RenderbufferWgpu::initializeContents(const gl::Context *context,
                                                   GLenum binding,
                                                   const gl::ImageIndex &imageIndex)
{
    UNIMPLEMENTED();
    return angle::Result::Continue;
}

angle::Result RenderbufferWgpu::getAttachmentRenderTarget(const gl::Context *context,
                                                          GLenum binding,
                                                          const gl::ImageIndex &imageIndex,
                                                          GLsizei samples,
                                                          FramebufferAttachmentRenderTarget **rtOut)
{
    gl::LevelIndex level(0);

    webgpu::TextureViewHandle textureView;
    ANGLE_TRY(mImage->createTextureViewSingleLevel(level, 0, textureView));

    mRenderTarget.set(mImage, textureView, mImage->toWgpuLevel(level), 0,
                      mImage->toWgpuTextureFormat());

    *rtOut = &mRenderTarget;
    return angle::Result::Continue;
}

void RenderbufferWgpu::releaseOwnershipOfImage(const gl::Context *context)
{
    mOwnsImage = false;
    setImageHelper(nullptr, true);
}

void RenderbufferWgpu::onSubjectStateChange(angle::SubjectIndex index,
                                            angle::SubjectMessage message)
{
    ASSERT(index == kRenderbufferImageSubjectIndex &&
           (message == angle::SubjectMessage::SubjectChanged ||
            message == angle::SubjectMessage::InitializationComplete));

    // Forward the notification to the parent that the internal storage changed.
    onStateChange(message);
}

void RenderbufferWgpu::setImageHelper(webgpu::ImageHelper *imageHelper, bool ownsImageHelper)
{
    if (mOwnsImage && mImage)
    {
        mImageObserverBinding.bind(nullptr);
        SafeDelete(mImage);
    }

    mImage     = imageHelper;
    mOwnsImage = ownsImageHelper;

    if (mImage)
    {
        mImageObserverBinding.bind(mImage);
    }

    onStateChange(angle::SubjectMessage::SubjectChanged);
}
}  // namespace rx
