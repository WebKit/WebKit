//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TextureVk.cpp:
//    Implements the class methods for TextureVk.
//

#include "libANGLE/renderer/vulkan/TextureVk.h"

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/formatutilsvk.h"

namespace rx
{

TextureVk::TextureVk(const gl::TextureState &state) : TextureImpl(state)
{
}

TextureVk::~TextureVk()
{
}

gl::Error TextureVk::onDestroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    renderer->releaseResource(*this, &mImage);
    renderer->releaseResource(*this, &mDeviceMemory);
    renderer->releaseResource(*this, &mImageView);
    renderer->releaseResource(*this, &mSampler);

    return gl::NoError();
}

gl::Error TextureVk::setImage(const gl::Context *context,
                              GLenum target,
                              size_t level,
                              GLenum internalFormat,
                              const gl::Extents &size,
                              GLenum format,
                              GLenum type,
                              const gl::PixelUnpackState &unpack,
                              const uint8_t *pixels)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();
    VkDevice device      = contextVk->getDevice();

    // TODO(jmadill): support multi-level textures.
    ASSERT(level == 0);

    if (mImage.valid())
    {
        const gl::ImageDesc &desc = mState.getImageDesc(target, level);

        // TODO(jmadill): Consider comparing stored vk::Format.
        if (desc.size != size ||
            !gl::Format::SameSized(desc.format, gl::Format(internalFormat, type)))
        {
            renderer->releaseResource(*this, &mImage);
            renderer->releaseResource(*this, &mDeviceMemory);
            renderer->releaseResource(*this, &mImageView);
        }
    }

    // TODO(jmadill): support other types of textures.
    ASSERT(target == GL_TEXTURE_2D);

    // Convert internalFormat to sized internal format.
    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(internalFormat, type);
    const vk::Format &vkFormat           = renderer->getFormat(formatInfo.sizedInternalFormat);

    if (!mImage.valid())
    {
        ASSERT(!mDeviceMemory.valid() && !mImageView.valid());

        VkImageCreateInfo imageInfo;
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.pNext         = nullptr;
        imageInfo.flags         = 0;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.format        = vkFormat.vkTextureFormat;
        imageInfo.extent.width  = size.width;
        imageInfo.extent.height = size.height;
        imageInfo.extent.depth  = size.depth;
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = 1;
        imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;

        // TODO(jmadill): Are all these image transfer bits necessary?
        imageInfo.usage = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        imageInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.queueFamilyIndexCount = 0;
        imageInfo.pQueueFamilyIndices   = nullptr;
        imageInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;

        ANGLE_TRY(mImage.init(device, imageInfo));

        // Allocate the device memory for the image.
        // TODO(jmadill): Use more intelligent device memory allocation.
        VkMemoryRequirements memoryRequirements;
        mImage.getMemoryRequirements(device, &memoryRequirements);

        uint32_t memoryIndex = renderer->getMemoryProperties().findCompatibleMemoryIndex(
            memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryAllocateInfo allocateInfo;
        allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.pNext           = nullptr;
        allocateInfo.allocationSize  = memoryRequirements.size;
        allocateInfo.memoryTypeIndex = memoryIndex;

        ANGLE_TRY(mDeviceMemory.allocate(device, allocateInfo));
        ANGLE_TRY(mImage.bindMemory(device, mDeviceMemory));

        VkImageViewCreateInfo viewInfo;
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.pNext                           = nullptr;
        viewInfo.flags                           = 0;
        viewInfo.image                           = mImage.getHandle();
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = vkFormat.vkTextureFormat;
        viewInfo.components.r                    = VK_COMPONENT_SWIZZLE_R;
        viewInfo.components.g                    = VK_COMPONENT_SWIZZLE_G;
        viewInfo.components.b                    = VK_COMPONENT_SWIZZLE_B;
        viewInfo.components.a                    = VK_COMPONENT_SWIZZLE_A;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        ANGLE_TRY(mImageView.init(device, viewInfo));
    }

    if (!mSampler.valid())
    {
        // Create a simple sampler. Force basic parameter settings.
        // TODO(jmadill): Sampler parameters.
        VkSamplerCreateInfo samplerInfo;
        samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.pNext                   = nullptr;
        samplerInfo.flags                   = 0;
        samplerInfo.magFilter               = VK_FILTER_NEAREST;
        samplerInfo.minFilter               = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.mipLodBias              = 0.0f;
        samplerInfo.anisotropyEnable        = VK_FALSE;
        samplerInfo.maxAnisotropy           = 1.0f;
        samplerInfo.compareEnable           = VK_FALSE;
        samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
        samplerInfo.minLod                  = 0.0f;
        samplerInfo.maxLod                  = 1.0f;
        samplerInfo.borderColor             = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        ANGLE_TRY(mSampler.init(device, samplerInfo));
    }

    mRenderTarget.image     = &mImage;
    mRenderTarget.imageView = &mImageView;
    mRenderTarget.format    = &vkFormat;
    mRenderTarget.extents   = size;
    mRenderTarget.samples   = VK_SAMPLE_COUNT_1_BIT;
    mRenderTarget.resource  = this;

    // Handle initial data.
    // TODO(jmadill): Consider re-using staging texture.
    if (pixels)
    {
        vk::StagingImage stagingImage;
        ANGLE_TRY(renderer->createStagingImage(TextureDimension::TEX_2D, vkFormat, size,
                                               vk::StagingUsage::Write, &stagingImage));

        GLuint inputRowPitch = 0;
        ANGLE_TRY_RESULT(
            formatInfo.computeRowPitch(type, size.width, unpack.alignment, unpack.rowLength),
            inputRowPitch);

        GLuint inputDepthPitch = 0;
        ANGLE_TRY_RESULT(
            formatInfo.computeDepthPitch(size.height, unpack.imageHeight, inputRowPitch),
            inputDepthPitch);

        // TODO(jmadill): skip images for 3D Textures.
        bool applySkipImages = false;

        GLuint inputSkipBytes = 0;
        ANGLE_TRY_RESULT(
            formatInfo.computeSkipBytes(inputRowPitch, inputDepthPitch, unpack, applySkipImages),
            inputSkipBytes);

        auto loadFunction = vkFormat.loadFunctions(type);

        uint8_t *mapPointer = nullptr;
        ANGLE_TRY(stagingImage.getDeviceMemory().map(device, 0, VK_WHOLE_SIZE, 0, &mapPointer));

        const uint8_t *source = pixels + inputSkipBytes;

        // Get the subresource layout. This has important parameters like row pitch.
        // TODO(jmadill): Fill out this structure based on input parameters.
        VkImageSubresource subresource;
        subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresource.mipLevel   = 0;
        subresource.arrayLayer = 0;

        VkSubresourceLayout subresourceLayout;
        vkGetImageSubresourceLayout(device, stagingImage.getImage().getHandle(), &subresource,
                                    &subresourceLayout);

        loadFunction.loadFunction(size.width, size.height, size.depth, source, inputRowPitch,
                                  inputDepthPitch, mapPointer,
                                  static_cast<size_t>(subresourceLayout.rowPitch),
                                  static_cast<size_t>(subresourceLayout.depthPitch));

        stagingImage.getDeviceMemory().unmap(device);

        vk::CommandBufferAndState *commandBuffer = nullptr;
        ANGLE_TRY(contextVk->getStartedCommandBuffer(&commandBuffer));
        setQueueSerial(renderer->getCurrentQueueSerial());

        // Ensure we aren't in a render pass.
        // TODO(jmadill): Command reordering.
        renderer->endRenderPass();

        stagingImage.getImage().changeLayoutWithStages(
            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, commandBuffer);
        mImage.changeLayoutWithStages(
            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, commandBuffer);

        gl::Box wholeRegion(0, 0, 0, size.width, size.height, size.depth);
        commandBuffer->copySingleImage(stagingImage.getImage(), mImage, wholeRegion,
                                       VK_IMAGE_ASPECT_COLOR_BIT);

        // TODO(jmadill): Re-use staging images.
        renderer->releaseObject(renderer->getCurrentQueueSerial(), &stagingImage);
    }

    return gl::NoError();
}

gl::Error TextureVk::setSubImage(const gl::Context *context,
                                 GLenum target,
                                 size_t level,
                                 const gl::Box &area,
                                 GLenum format,
                                 GLenum type,
                                 const gl::PixelUnpackState &unpack,
                                 const uint8_t *pixels)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::setCompressedImage(const gl::Context *context,
                                        GLenum target,
                                        size_t level,
                                        GLenum internalFormat,
                                        const gl::Extents &size,
                                        const gl::PixelUnpackState &unpack,
                                        size_t imageSize,
                                        const uint8_t *pixels)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::setCompressedSubImage(const gl::Context *context,
                                           GLenum target,
                                           size_t level,
                                           const gl::Box &area,
                                           GLenum format,
                                           const gl::PixelUnpackState &unpack,
                                           size_t imageSize,
                                           const uint8_t *pixels)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::copyImage(const gl::Context *context,
                               GLenum target,
                               size_t level,
                               const gl::Rectangle &sourceArea,
                               GLenum internalFormat,
                               const gl::Framebuffer *source)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::copySubImage(const gl::Context *context,
                                  GLenum target,
                                  size_t level,
                                  const gl::Offset &destOffset,
                                  const gl::Rectangle &sourceArea,
                                  const gl::Framebuffer *source)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::setStorage(const gl::Context *context,
                                GLenum target,
                                size_t levels,
                                GLenum internalFormat,
                                const gl::Extents &size)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::setEGLImageTarget(const gl::Context *context, GLenum target, egl::Image *image)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::setImageExternal(const gl::Context *context,
                                      GLenum target,
                                      egl::Stream *stream,
                                      const egl::Stream::GLTextureDescription &desc)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::generateMipmap(const gl::Context *context)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::setBaseLevel(const gl::Context *context, GLuint baseLevel)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::bindTexImage(const gl::Context *context, egl::Surface *surface)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::releaseTexImage(const gl::Context *context)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::getAttachmentRenderTarget(const gl::Context *context,
                                               GLenum binding,
                                               const gl::ImageIndex &imageIndex,
                                               FramebufferAttachmentRenderTarget **rtOut)
{
    ASSERT(imageIndex.type == GL_TEXTURE_2D);
    ASSERT(imageIndex.mipIndex == 0 && imageIndex.layerIndex == gl::ImageIndex::ENTIRE_LEVEL);
    *rtOut = &mRenderTarget;
    return gl::NoError();
}

void TextureVk::syncState(const gl::Texture::DirtyBits &dirtyBits)
{
    // TODO(jmadill): Texture sync state.
}

gl::Error TextureVk::setStorageMultisample(const gl::Context *context,
                                           GLenum target,
                                           GLsizei samples,
                                           GLint internalformat,
                                           const gl::Extents &size,
                                           bool fixedSampleLocations)
{
    UNIMPLEMENTED();
    return gl::InternalError() << "setStorageMultisample is unimplemented.";
}

gl::Error TextureVk::initializeContents(const gl::Context *context,
                                        const gl::ImageIndex &imageIndex)
{
    UNIMPLEMENTED();
    return gl::NoError();
}

const vk::Image &TextureVk::getImage() const
{
    ASSERT(mImage.valid());
    return mImage;
}

const vk::ImageView &TextureVk::getImageView() const
{
    ASSERT(mImageView.valid());
    return mImageView;
}

const vk::Sampler &TextureVk::getSampler() const
{
    ASSERT(mSampler.valid());
    return mSampler;
}

}  // namespace rx
