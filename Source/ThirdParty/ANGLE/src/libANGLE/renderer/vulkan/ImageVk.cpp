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
    : ImageImpl(state), mOwnsImage(false), mImage(nullptr), mContext(context)
{}

ImageVk::~ImageVk() {}

void ImageVk::onDestroy(const egl::Display *display)
{
    DisplayVk *displayVk = vk::GetImpl(display);
    RendererVk *renderer = displayVk->getRenderer();

    if (mImage != nullptr && mOwnsImage)
    {
        // TODO: We need to handle the case that EGLImage used in two context that aren't shared.
        // https://issuetracker.google.com/169868803
        mImage->releaseImage(renderer);
        mImage->releaseStagedUpdates(renderer);
        SafeDelete(mImage);
    }
    else if (egl::IsExternalImageTarget(mState.target))
    {
        ASSERT(mState.source != nullptr);
        ExternalImageSiblingVk *externalImageSibling =
            GetImplAs<ExternalImageSiblingVk>(GetAs<egl::ExternalImageSibling>(mState.source));
        externalImageSibling->release(renderer);
        mImage = nullptr;

        // This is called as a special case where resources may be allocated by the caller, without
        // the caller ever issuing a draw command to free them. Specifically, SurfaceFlinger
        // optimistically allocates EGLImages that it may never draw to.
        renderer->cleanupCompletedCommandsGarbage();
    }
}

egl::Error ImageVk::initialize(const egl::Display *display)
{
    if (egl::IsTextureTarget(mState.target))
    {
        ASSERT(mContext != nullptr);
        ContextVk *contextVk = vk::GetImpl(mContext);
        TextureVk *textureVk = GetImplAs<TextureVk>(GetAs<gl::Texture>(mState.source));

        // Make sure the texture uses renderable format
        TextureUpdateResult updateResult = TextureUpdateResult::ImageUnaffected;
        ANGLE_TRY(ResultToEGL(textureVk->ensureRenderable(contextVk, &updateResult)));

        // Make sure the texture has created its backing storage
        ANGLE_TRY(ResultToEGL(
            textureVk->ensureImageInitialized(contextVk, ImageMipLevels::EnabledLevels)));

        mImage = &textureVk->getImage();

        // The staging buffer for a texture source should already be initialized

        mOwnsImage = false;
    }
    else
    {
        if (egl::IsRenderbufferTarget(mState.target))
        {
            RenderbufferVk *renderbufferVk =
                GetImplAs<RenderbufferVk>(GetAs<gl::Renderbuffer>(mState.source));
            mImage = renderbufferVk->getImage();

            ASSERT(mContext != nullptr);
        }
        else if (egl::IsExternalImageTarget(mState.target))
        {
            const ExternalImageSiblingVk *externalImageSibling =
                GetImplAs<ExternalImageSiblingVk>(GetAs<egl::ExternalImageSibling>(mState.source));
            mImage = externalImageSibling->getImage();

            ASSERT(mContext == nullptr);
        }
        else
        {
            UNREACHABLE();
            return egl::EglBadAccess();
        }

        mOwnsImage = false;
    }

    // mContext is no longer needed, make sure it's not used by accident.
    mContext = nullptr;

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

    // Grab a fence from the releasing context to know when the image is no longer used
    ASSERT(context != nullptr);
    ContextVk *contextVk = vk::GetImpl(context);

    // Flush the context to make sure the fence has been submitted.
    return contextVk->flushImpl(nullptr, RenderPassClosureReason::ImageOrphan);
}

egl::Error ImageVk::exportVkImage(void *vkImage, void *vkImageCreateInfo)
{
    *reinterpret_cast<VkImage *>(vkImage) = mImage->getImage().getHandle();
    auto *info = reinterpret_cast<VkImageCreateInfo *>(vkImageCreateInfo);
    *info      = mImage->getVkImageCreateInfo();
    return egl::NoError();
}

gl::TextureType ImageVk::getImageTextureType() const
{
    return mState.imageIndex.getType();
}

gl::LevelIndex ImageVk::getImageLevel() const
{
    return gl::LevelIndex(mState.imageIndex.getLevelIndex());
}

uint32_t ImageVk::getImageLayer() const
{
    return mState.imageIndex.hasLayer() ? mState.imageIndex.getLayerIndex() : 0;
}

}  // namespace rx
