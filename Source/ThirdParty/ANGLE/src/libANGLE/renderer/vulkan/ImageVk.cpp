//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ImageVk.cpp:
//    Implements the class methods for ImageVk.
//

#include "libANGLE/renderer/vulkan/ImageVk.h"

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/RenderbufferVk.h"
#include "libANGLE/renderer/vulkan/TextureVk.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

namespace rx
{

ImageVk::ImageVk(const egl::ImageState &state, const gl::Context *context)
    : ImageImpl(state), mImageLevel(0), mOwnsImage(false), mImage(nullptr), mContext(context)
{}

ImageVk::~ImageVk() {}

void ImageVk::onDestroy(const egl::Display *display)
{
    DisplayVk *displayVk = vk::GetImpl(display);
    RendererVk *renderer = displayVk->getRenderer();

    if (mImage != nullptr && mOwnsImage)
    {
        mImage->releaseImage(renderer);
        mImage->releaseStagingBuffer(renderer);
        delete mImage;
    }
    mImage = nullptr;
}

egl::Error ImageVk::initialize(const egl::Display *display)
{
    if (egl::IsTextureTarget(mState.target))
    {
        TextureVk *textureVk = GetImplAs<TextureVk>(GetAs<gl::Texture>(mState.source));

        // Make sure the texture has created its backing storage
        ASSERT(mContext != nullptr);
        ContextVk *contextVk = vk::GetImpl(mContext);
        ANGLE_TRY(ResultToEGL(textureVk->ensureImageInitialized(contextVk)));

        mImage = &textureVk->getImage();

        // The staging buffer for a texture source should already be initialized

        mOwnsImage = false;

        mImageTextureType = mState.imageIndex.getType();
        mImageLevel       = mState.imageIndex.getLevelIndex();
        mImageLayer       = mState.imageIndex.hasLayer() ? mState.imageIndex.getLayerIndex() : 0;
    }
    else
    {
        RendererVk *renderer = nullptr;
        if (egl::IsRenderbufferTarget(mState.target))
        {
            RenderbufferVk *renderbufferVk =
                GetImplAs<RenderbufferVk>(GetAs<gl::Renderbuffer>(mState.source));
            mImage = renderbufferVk->getImage();

            ASSERT(mContext != nullptr);
            renderer = vk::GetImpl(mContext)->getRenderer();
            ;
        }
        else if (egl::IsExternalImageTarget(mState.target))
        {
            const ExternalImageSiblingVk *externalImageSibling =
                GetImplAs<ExternalImageSiblingVk>(GetAs<egl::ExternalImageSibling>(mState.source));
            mImage = externalImageSibling->getImage();

            ASSERT(mContext == nullptr);
            renderer = vk::GetImpl(display)->getRenderer();
        }
        else
        {
            UNREACHABLE();
            return egl::EglBadAccess();
        }

        // Make sure a staging buffer is ready to use to upload data
        mImage->initStagingBuffer(renderer, mImage->getFormat());

        mOwnsImage = false;

        mImageTextureType = gl::TextureType::_2D;
        mImageLevel       = 0;
        mImageLayer       = 0;
    }

    return egl::NoError();
}

angle::Result ImageVk::orphan(const gl::Context *context, egl::ImageSibling *sibling)
{
    if (sibling == mState.source)
    {
        if (egl::IsTextureTarget(mState.target))
        {
            TextureVk *textureVk = GetImplAs<TextureVk>(GetAs<gl::Texture>(mState.source));
            ASSERT(mImage == &textureVk->getImage());
            textureVk->releaseOwnershipOfImage(context);
            mOwnsImage = true;
        }
        else if (egl::IsRenderbufferTarget(mState.target))
        {
            RenderbufferVk *renderbufferVk =
                GetImplAs<RenderbufferVk>(GetAs<gl::Renderbuffer>(mState.source));
            ASSERT(mImage == renderbufferVk->getImage());
            renderbufferVk->releaseOwnershipOfImage(context);
            mOwnsImage = true;
        }
        else
        {
            ANGLE_VK_UNREACHABLE(vk::GetImpl(context));
            return angle::Result::Stop;
        }
    }

    return angle::Result::Continue;
}

}  // namespace rx
