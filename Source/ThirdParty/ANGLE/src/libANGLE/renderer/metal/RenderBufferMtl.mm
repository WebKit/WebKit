//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RenderBufferMtl.mm:
//    Implements the class methods for RenderBufferMtl.
//

#include "libANGLE/renderer/metal/RenderBufferMtl.h"

#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/mtl_format_utils.h"
#include "libANGLE/renderer/metal/mtl_utils.h"

namespace rx
{

RenderbufferMtl::RenderbufferMtl(const gl::RenderbufferState &state) : RenderbufferImpl(state) {}

RenderbufferMtl::~RenderbufferMtl() {}

void RenderbufferMtl::onDestroy(const gl::Context *context)
{
    releaseTexture();
}

void RenderbufferMtl::releaseTexture()
{
    mTexture           = nullptr;
    mImplicitMSTexture = nullptr;
}

angle::Result RenderbufferMtl::setStorageImpl(const gl::Context *context,
                                              GLsizei samples,
                                              GLenum internalformat,
                                              GLsizei width,
                                              GLsizei height)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);

    if (mTexture != nullptr && mTexture->valid())
    {
        // Check against the state if we need to recreate the storage.
        if (internalformat != mState.getFormat().info->internalFormat ||
            static_cast<GLsizei>(width) != mState.getWidth() ||
            static_cast<GLsizei>(height) != mState.getHeight() ||
            static_cast<GLsizei>(samples) != mState.getSamples())
        {
            releaseTexture();
        }
    }

    const gl::InternalFormat &internalFormat = gl::GetSizedInternalFormatInfo(internalformat);
    angle::FormatID angleFormatId =
        angle::Format::InternalFormatToID(internalFormat.sizedInternalFormat);
    mFormat = contextMtl->getPixelFormat(angleFormatId);

    uint32_t actualSamples;
    if (samples == 0)
    {
        actualSamples = 1;
    }
    else
    {
        // We always start at at least 2 samples
        actualSamples = static_cast<uint32_t>(std::max<size_t>(2, samples));

        const gl::TextureCaps &textureCaps =
            contextMtl->getTextureCaps().get(mFormat.intendedFormatId);
        actualSamples = textureCaps.getNearestSamples(actualSamples);
        ANGLE_MTL_CHECK(contextMtl, actualSamples != 0, GL_INVALID_VALUE);
    }

    if ((mTexture == nullptr || !mTexture->valid()) && (width != 0 && height != 0))
    {
        if (actualSamples == 1 || (mFormat.hasDepthAndStencilBits() && mFormat.getCaps().resolve))
        {
            ANGLE_TRY(mtl::Texture::Make2DTexture(contextMtl, mFormat, static_cast<uint32_t>(width),
                                                  static_cast<uint32_t>(height), 1,
                                                  /* renderTargetOnly */ false,
                                                  /* allowFormatView */ false, &mTexture));

            // Use implicit resolve for depth stencil texture whenever possible. This is because
            // for depth stencil texture, if stencil needs to be blitted, a formatted clone has
            // to be created. And it is expensive to clone a multisample texture.
            if (actualSamples > 1)
            {
                // This format must supports implicit resolve
                ASSERT(mFormat.getCaps().resolve);

                ANGLE_TRY(mtl::Texture::Make2DMSTexture(
                    contextMtl, mFormat, static_cast<uint32_t>(width),
                    static_cast<uint32_t>(height), actualSamples,
                    /* renderTargetOnly */ true,
                    /* allowFormatView */ false, &mImplicitMSTexture));
            }
        }
        else
        {
            ANGLE_TRY(mtl::Texture::Make2DMSTexture(contextMtl, mFormat,
                                                    static_cast<uint32_t>(width),
                                                    static_cast<uint32_t>(height), actualSamples,
                                                    /* renderTargetOnly */ false,
                                                    /* allowFormatView */ false, &mTexture));
        }

        mRenderTarget.setWithImplicitMSTexture(mTexture, mImplicitMSTexture, mtl::kZeroNativeMipLevel, 0, mFormat);

        // For emulated channels that GL texture intends to not have,
        // we need to initialize their content.
        bool emulatedChannels = mtl::IsFormatEmulated(mFormat);
        bool isDepthStencil = mFormat.hasDepthOrStencilBits();
        if (emulatedChannels)
        {
            gl::ImageIndex index;

            if (actualSamples > 1)
            {
                index = gl::ImageIndex::Make2DMultisample();
            }
            else
            {
                index = gl::ImageIndex::Make2D(0);
            }

            ANGLE_TRY(mtl::InitializeTextureContents(context, mTexture, mFormat, mtl::ImageNativeIndex(index, 0)));
            if (mImplicitMSTexture)
            {
                ANGLE_TRY(mtl::InitializeTextureContents(context, mImplicitMSTexture, mFormat,
                                                         mtl::ImageNativeIndex(gl::ImageIndex::Make2DMultisample(), 0)));
            }
        }  // if (emulatedChannels)
        if(isDepthStencil)
        {
            gl::ImageIndex index;

            if (actualSamples > 1)
            {
                index = gl::ImageIndex::Make2DMultisample();
            }
            else
            {
                index = gl::ImageIndex::Make2D(0);
            }
            ANGLE_TRY(mtl::InitializeDepthStencilTextureContentsGPU(context, mTexture, mFormat, mtl::ImageNativeIndex(index, 0)));
            if (mImplicitMSTexture)
            {
                ANGLE_TRY(mtl::InitializeDepthStencilTextureContentsGPU(context, mImplicitMSTexture, mFormat,
                                                         mtl::ImageNativeIndex(gl::ImageIndex::Make2DMultisample(), 0)));
            }
        }
    }

    return angle::Result::Continue;
}

angle::Result RenderbufferMtl::setStorage(const gl::Context *context,
                                          GLenum internalformat,
                                          GLsizei width,
                                          GLsizei height)
{
    return setStorageImpl(context, 0, internalformat, width, height);
}

angle::Result RenderbufferMtl::setStorageMultisample(const gl::Context *context,
                                                     GLsizei samples,
                                                     GLenum internalformat,
                                                     GLsizei width,
                                                     GLsizei height,
                                                     gl::MultisamplingMode mode)
{
    return setStorageImpl(context, samples, internalformat, width, height);
}

angle::Result RenderbufferMtl::setStorageEGLImageTarget(const gl::Context *context,
                                                        egl::Image *image)
{
    // NOTE(hqle): Support EGLimage
    UNIMPLEMENTED();
    return angle::Result::Stop;
}

angle::Result RenderbufferMtl::getAttachmentRenderTarget(const gl::Context *context,
                                                         GLenum binding,
                                                         const gl::ImageIndex &imageIndex,
                                                         GLsizei samples,
                                                         FramebufferAttachmentRenderTarget **rtOut)
{
    ASSERT(mTexture && mTexture->valid());
    *rtOut = &mRenderTarget;
    return angle::Result::Continue;
}

angle::Result RenderbufferMtl::initializeContents(const gl::Context *context,
                                                  const gl::ImageIndex &imageIndex)
{
    if (imageIndex.valid())
        return mtl::InitializeTextureContents(context, mTexture, mFormat,
                                          mtl::ImageNativeIndex::FromBaseZeroGLIndex(imageIndex));
    else
        return mtl::InitializeTextureContents(context, mTexture, mFormat,
                                          mtl::ImageNativeIndex::FromBaseZeroGLIndex(gl::ImageIndex::Make2D(0)));
}
}
