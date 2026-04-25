//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ImageWgpu.cpp:
//    Implements the class methods for ImageWgpu.
//

#include "libANGLE/renderer/wgpu/ImageWgpu.h"
#include "libANGLE/Display.h"
#include "libANGLE/renderer/wgpu/DisplayWgpu.h"
#include "libANGLE/renderer/wgpu/RenderbufferWgpu.h"
#include "libANGLE/renderer/wgpu/TextureWgpu.h"

#include "common/debug.h"

namespace rx
{
WebGPUTextureImageSiblingWgpu::WebGPUTextureImageSiblingWgpu(EGLClientBuffer buffer,
                                                             const egl::AttributeMap &attribs)
    : mBuffer(buffer), mAttribs(attribs)
{}
WebGPUTextureImageSiblingWgpu::~WebGPUTextureImageSiblingWgpu() {}

egl::Error WebGPUTextureImageSiblingWgpu::initialize(const egl::Display *display)
{
    return angle::ResultToEGL(initializeImpl(display));
}

angle::Result WebGPUTextureImageSiblingWgpu::initializeImpl(const egl::Display *display)
{
    DisplayWgpu *displayWgpu  = webgpu::GetImpl(display);
    const DawnProcTable *wgpu = displayWgpu->getProcs();

    webgpu::TextureHandle externalTexture =
        webgpu::TextureHandle::Acquire(wgpu, reinterpret_cast<WGPUTexture>(mBuffer));
    ASSERT(externalTexture != nullptr);

    // Explicitly add-ref to hold on to the external reference
    wgpu->textureAddRef(externalTexture.get());

    const webgpu::Format *webgpuFormat = displayWgpu->getFormatForImportedTexture(
        mAttribs, wgpu->textureGetFormat(externalTexture.get()));
    ASSERT(webgpuFormat != nullptr);
    ANGLE_TRY(mImage.initExternal(wgpu, webgpuFormat->getIntendedFormatID(),
                                  webgpuFormat->getActualImageFormatID(), externalTexture));
    return angle::Result::Continue;
}

void WebGPUTextureImageSiblingWgpu::onDestroy(const egl::Display *display)
{
    mImage.resetImage();
}

gl::Format WebGPUTextureImageSiblingWgpu::getFormat() const
{
    const angle::Format &angleFormat = angle::Format::Get(mImage.getIntendedFormatID());
    return gl::Format(angleFormat.glInternalFormat);
}

bool WebGPUTextureImageSiblingWgpu::isRenderable(const gl::Context *context) const
{
    constexpr WGPUTextureUsage kRenderableRequiredUsages =
        WGPUTextureUsage_CopySrc | WGPUTextureUsage_RenderAttachment;
    return (mImage.getUsage() & kRenderableRequiredUsages) == kRenderableRequiredUsages;
}

bool WebGPUTextureImageSiblingWgpu::isTexturable(const gl::Context *context) const
{
    constexpr WGPUTextureUsage kTexturableRequiredUsages =
        WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding;
    return (mImage.getUsage() & kTexturableRequiredUsages) == kTexturableRequiredUsages;
}

bool WebGPUTextureImageSiblingWgpu::isYUV() const
{
    const angle::Format &actualFormat = angle::Format::Get(mImage.getActualFormatID());
    return actualFormat.isYUV;
}

bool WebGPUTextureImageSiblingWgpu::hasFrontBufferUsage() const
{
    return false;
}

bool WebGPUTextureImageSiblingWgpu::isCubeMap() const
{
    return false;
}

bool WebGPUTextureImageSiblingWgpu::hasProtectedContent() const
{
    return false;
}

gl::Extents WebGPUTextureImageSiblingWgpu::getSize() const
{
    return wgpu_gl::GetExtents(mImage.getSize());
}

size_t WebGPUTextureImageSiblingWgpu::getSamples() const
{
    // GL treats a sample count of 1 as multisampled. When the WebGPU texture has a sample count of
    // 1, force it to 0.
    uint32_t samples = mImage.getSamples();
    return samples > 1 ? samples : 0;
}

uint32_t WebGPUTextureImageSiblingWgpu::getLevelCount() const
{
    return mImage.getLevelCount();
}

webgpu::ImageHelper *WebGPUTextureImageSiblingWgpu::getImage() const
{
    return const_cast<webgpu::ImageHelper *>(&mImage);
}

ImageWgpu::ImageWgpu(const egl::ImageState &state, const gl::Context *context)
    : ImageImpl(state), mContext(context)
{}

ImageWgpu::~ImageWgpu() {}

egl::Error ImageWgpu::initialize(const egl::Display *display)
{
    if (egl::IsTextureTarget(mState.target))
    {
        if (mState.imageIndex.getLevelIndex() != 0)
        {
            UNIMPLEMENTED();
            return egl::Error(EGL_BAD_ACCESS,
                              "Creation of EGLImages from non-zero mip levels is unimplemented.");
        }
        if (mState.imageIndex.getType() != gl::TextureType::_2D)
        {
            UNIMPLEMENTED();
            return egl::Error(EGL_BAD_ACCESS,
                              "Creation of EGLImages from non-2D textures is unimplemented.");
        }

        TextureWgpu *textureWgpu = webgpu::GetImpl(GetAs<gl::Texture>(mState.source));

        ASSERT(mContext != nullptr);
        angle::Result initResult = textureWgpu->ensureImageInitialized(mContext);
        if (initResult != angle::Result::Continue)
        {
            return egl::Error(EGL_BAD_ACCESS, "Failed to initialize source texture.");
        }

        mImage = textureWgpu->getImage();
    }
    else if (egl::IsRenderbufferTarget(mState.target))
    {
        RenderbufferWgpu *renderbufferWgpu =
            webgpu::GetImpl(GetAs<gl::Renderbuffer>(mState.source));
        mImage = renderbufferWgpu->getImage();

        ASSERT(mContext != nullptr);
    }
    else if (egl::IsExternalImageTarget(mState.target))
    {
        const ExternalImageSiblingWgpu *externalImageSibling =
            webgpu::GetImpl(GetAs<egl::ExternalImageSibling>(mState.source));
        mImage = externalImageSibling->getImage();

        ASSERT(mContext == nullptr);
    }
    else
    {
        UNREACHABLE();
        return egl::Error(EGL_BAD_ACCESS);
    }

    ASSERT(mImage->isInitialized());
    mOwnsImage = false;

    return egl::NoError();
}

angle::Result ImageWgpu::orphan(const gl::Context *context, egl::ImageSibling *sibling)
{
    if (sibling == mState.source)
    {
        if (egl::IsTextureTarget(mState.target))
        {
            TextureWgpu *textureWgpu = webgpu::GetImpl(GetAs<gl::Texture>(mState.source));
            ASSERT(mImage == textureWgpu->getImage());
            textureWgpu->releaseOwnershipOfImage(context);
        }
        else if (egl::IsRenderbufferTarget(mState.target))
        {
            RenderbufferWgpu *renderbufferWgpu =
                webgpu::GetImpl(GetAs<gl::Renderbuffer>(mState.source));
            ASSERT(mImage == renderbufferWgpu->getImage());
            renderbufferWgpu->releaseOwnershipOfImage(context);
        }
        else
        {
            UNREACHABLE();
            return angle::Result::Stop;
        }

        mOwnsImage = true;
    }

    return angle::Result::Continue;
}

}  // namespace rx
