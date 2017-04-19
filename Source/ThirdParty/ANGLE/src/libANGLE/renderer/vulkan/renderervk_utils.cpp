//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// renderervk_utils:
//    Helper functions for the Vulkan Renderer.
//

#include "renderervk_utils.h"

#include "libANGLE/renderer/vulkan/RendererVk.h"

namespace rx
{

namespace
{
GLenum DefaultGLErrorCode(VkResult result)
{
    switch (result)
    {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        case VK_ERROR_TOO_MANY_OBJECTS:
            return GL_OUT_OF_MEMORY;
        default:
            return GL_INVALID_OPERATION;
    }
}

EGLint DefaultEGLErrorCode(VkResult result)
{
    switch (result)
    {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        case VK_ERROR_TOO_MANY_OBJECTS:
            return EGL_BAD_ALLOC;
        case VK_ERROR_INITIALIZATION_FAILED:
            return EGL_NOT_INITIALIZED;
        case VK_ERROR_SURFACE_LOST_KHR:
        case VK_ERROR_DEVICE_LOST:
            return EGL_CONTEXT_LOST;
        default:
            return EGL_BAD_ACCESS;
    }
}

// Gets access flags that are common between source and dest layouts.
VkAccessFlags GetBasicLayoutAccessFlags(VkImageLayout layout)
{
    switch (layout)
    {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            return VK_ACCESS_MEMORY_READ_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case VK_IMAGE_LAYOUT_UNDEFINED:
        case VK_IMAGE_LAYOUT_GENERAL:
            return 0;
        default:
            // TODO(jmadill): Investigate other flags.
            UNREACHABLE();
            return 0;
    }
}
}  // anonymous namespace

// Mirrors std_validation_str in loader.h
// TODO(jmadill): Possibly wrap the loader into a safe source file. Can't be included trivially.
const char *g_VkStdValidationLayerName = "VK_LAYER_LUNARG_standard_validation";

const char *VulkanResultString(VkResult result)
{
    switch (result)
    {
        case VK_SUCCESS:
            return "Command successfully completed.";
        case VK_NOT_READY:
            return "A fence or query has not yet completed.";
        case VK_TIMEOUT:
            return "A wait operation has not completed in the specified time.";
        case VK_EVENT_SET:
            return "An event is signaled.";
        case VK_EVENT_RESET:
            return "An event is unsignaled.";
        case VK_INCOMPLETE:
            return "A return array was too small for the result.";
        case VK_SUBOPTIMAL_KHR:
            return "A swapchain no longer matches the surface properties exactly, but can still be "
                   "used to present to the surface successfully.";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "A host memory allocation has failed.";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "A device memory allocation has failed.";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "Initialization of an object could not be completed for implementation-specific "
                   "reasons.";
        case VK_ERROR_DEVICE_LOST:
            return "The logical or physical device has been lost.";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "Mapping of a memory object has failed.";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "A requested layer is not present or could not be loaded.";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "A requested extension is not supported.";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "A requested feature is not supported.";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "The requested version of Vulkan is not supported by the driver or is otherwise "
                   "incompatible for implementation-specific reasons.";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "Too many objects of the type have already been created.";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "A requested format is not supported on this device.";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "A surface is no longer available.";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            return "The requested window is already connected to a VkSurfaceKHR, or to some other "
                   "non-Vulkan API.";
        case VK_ERROR_OUT_OF_DATE_KHR:
            return "A surface has changed in such a way that it is no longer compatible with the "
                   "swapchain.";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            return "The display used by a swapchain does not use the same presentable image "
                   "layout, or is incompatible in a way that prevents sharing an image.";
        case VK_ERROR_VALIDATION_FAILED_EXT:
            return "The validation layers detected invalid API usage.";
        default:
            return "Unknown vulkan error code.";
    }
}

bool HasStandardValidationLayer(const std::vector<VkLayerProperties> &layerProps)
{
    for (const auto &layerProp : layerProps)
    {
        if (std::string(layerProp.layerName) == g_VkStdValidationLayerName)
        {
            return true;
        }
    }

    return false;
}

namespace vk
{

Error::Error(VkResult result) : mResult(result), mFile(nullptr), mLine(0)
{
    ASSERT(result == VK_SUCCESS);
}

Error::Error(VkResult result, const char *file, unsigned int line)
    : mResult(result), mFile(file), mLine(line)
{
}

Error::~Error()
{
}

Error::Error(const Error &other) = default;
Error &Error::operator=(const Error &other) = default;

gl::Error Error::toGL(GLenum glErrorCode) const
{
    if (!isError())
    {
        return gl::NoError();
    }

    // TODO(jmadill): Set extended error code to 'vulkan internal error'.
    const std::string &message = toString();
    return gl::Error(glErrorCode, message.c_str());
}

egl::Error Error::toEGL(EGLint eglErrorCode) const
{
    if (!isError())
    {
        return egl::Error(EGL_SUCCESS);
    }

    // TODO(jmadill): Set extended error code to 'vulkan internal error'.
    const std::string &message = toString();
    return egl::Error(eglErrorCode, message.c_str());
}

std::string Error::toString() const
{
    std::stringstream errorStream;
    errorStream << "Internal Vulkan error: " << VulkanResultString(mResult) << ", in " << mFile
                << ", line " << mLine << ".";
    return errorStream.str();
}

Error::operator gl::Error() const
{
    return toGL(DefaultGLErrorCode(mResult));
}

Error::operator egl::Error() const
{
    return toEGL(DefaultEGLErrorCode(mResult));
}

bool Error::isError() const
{
    return (mResult != VK_SUCCESS);
}

// CommandPool implementation.
CommandPool::CommandPool()
{
}

void CommandPool::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyCommandPool(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

Error CommandPool::init(VkDevice device, const VkCommandPoolCreateInfo &createInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(vkCreateCommandPool(device, &createInfo, nullptr, &mHandle));
    return NoError();
}

// CommandBuffer implementation.
CommandBuffer::CommandBuffer() : mCommandPool(nullptr)
{
}

void CommandBuffer::setCommandPool(CommandPool *commandPool)
{
    ASSERT(!mCommandPool && commandPool->valid());
    mCommandPool = commandPool;
}

Error CommandBuffer::begin(VkDevice device)
{
    if (mHandle == VK_NULL_HANDLE)
    {
        VkCommandBufferAllocateInfo commandBufferInfo;
        commandBufferInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferInfo.pNext              = nullptr;
        commandBufferInfo.commandPool        = mCommandPool->getHandle();
        commandBufferInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferInfo.commandBufferCount = 1;

        ANGLE_VK_TRY(vkAllocateCommandBuffers(device, &commandBufferInfo, &mHandle));
    }
    else
    {
        reset();
    }

    VkCommandBufferBeginInfo beginInfo;
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    // TODO(jmadill): Use other flags?
    beginInfo.flags            = 0;
    beginInfo.pInheritanceInfo = nullptr;

    ANGLE_VK_TRY(vkBeginCommandBuffer(mHandle, &beginInfo));

    return NoError();
}

Error CommandBuffer::end()
{
    ASSERT(valid());
    ANGLE_VK_TRY(vkEndCommandBuffer(mHandle));
    return NoError();
}

Error CommandBuffer::reset()
{
    ASSERT(valid());
    ANGLE_VK_TRY(vkResetCommandBuffer(mHandle, 0));
    return NoError();
}

void CommandBuffer::singleImageBarrier(VkPipelineStageFlags srcStageMask,
                                       VkPipelineStageFlags dstStageMask,
                                       VkDependencyFlags dependencyFlags,
                                       const VkImageMemoryBarrier &imageMemoryBarrier)
{
    ASSERT(valid());
    vkCmdPipelineBarrier(mHandle, srcStageMask, dstStageMask, dependencyFlags, 0, nullptr, 0,
                         nullptr, 1, &imageMemoryBarrier);
}

void CommandBuffer::destroy(VkDevice device)
{
    if (valid())
    {
        ASSERT(mCommandPool && mCommandPool->valid());
        vkFreeCommandBuffers(device, mCommandPool->getHandle(), 1, &mHandle);
        mHandle = VK_NULL_HANDLE;
    }
}

void CommandBuffer::clearSingleColorImage(const vk::Image &image, const VkClearColorValue &color)
{
    ASSERT(valid());
    ASSERT(image.getCurrentLayout() == VK_IMAGE_LAYOUT_GENERAL ||
           image.getCurrentLayout() == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkImageSubresourceRange range;
    range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel   = 0;
    range.levelCount     = 1;
    range.baseArrayLayer = 0;
    range.layerCount     = 1;

    vkCmdClearColorImage(mHandle, image.getHandle(), image.getCurrentLayout(), &color, 1, &range);
}

void CommandBuffer::copySingleImage(const vk::Image &srcImage,
                                    const vk::Image &destImage,
                                    const gl::Box &copyRegion,
                                    VkImageAspectFlags aspectMask)
{
    ASSERT(valid());
    ASSERT(srcImage.getCurrentLayout() == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ||
           srcImage.getCurrentLayout() == VK_IMAGE_LAYOUT_GENERAL);
    ASSERT(destImage.getCurrentLayout() == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ||
           destImage.getCurrentLayout() == VK_IMAGE_LAYOUT_GENERAL);

    VkImageCopy region;
    region.srcSubresource.aspectMask     = aspectMask;
    region.srcSubresource.mipLevel       = 0;
    region.srcSubresource.baseArrayLayer = 0;
    region.srcSubresource.layerCount     = 1;
    region.srcOffset.x                   = copyRegion.x;
    region.srcOffset.y                   = copyRegion.y;
    region.srcOffset.z                   = copyRegion.z;
    region.dstSubresource.aspectMask     = aspectMask;
    region.dstSubresource.mipLevel       = 0;
    region.dstSubresource.baseArrayLayer = 0;
    region.dstSubresource.layerCount     = 1;
    region.dstOffset.x                   = copyRegion.x;
    region.dstOffset.y                   = copyRegion.y;
    region.dstOffset.z                   = copyRegion.z;
    region.extent.width                  = copyRegion.width;
    region.extent.height                 = copyRegion.height;
    region.extent.depth                  = copyRegion.depth;

    vkCmdCopyImage(mHandle, srcImage.getHandle(), srcImage.getCurrentLayout(),
                   destImage.getHandle(), destImage.getCurrentLayout(), 1, &region);
}

void CommandBuffer::beginRenderPass(const RenderPass &renderPass,
                                    const Framebuffer &framebuffer,
                                    const gl::Rectangle &renderArea,
                                    const std::vector<VkClearValue> &clearValues)
{
    ASSERT(!clearValues.empty());
    ASSERT(mHandle != VK_NULL_HANDLE);

    VkRenderPassBeginInfo beginInfo;
    beginInfo.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.pNext                    = nullptr;
    beginInfo.renderPass               = renderPass.getHandle();
    beginInfo.framebuffer              = framebuffer.getHandle();
    beginInfo.renderArea.offset.x      = static_cast<uint32_t>(renderArea.x);
    beginInfo.renderArea.offset.y      = static_cast<uint32_t>(renderArea.y);
    beginInfo.renderArea.extent.width  = static_cast<uint32_t>(renderArea.width);
    beginInfo.renderArea.extent.height = static_cast<uint32_t>(renderArea.height);
    beginInfo.clearValueCount          = static_cast<uint32_t>(clearValues.size());
    beginInfo.pClearValues             = clearValues.data();

    vkCmdBeginRenderPass(mHandle, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void CommandBuffer::endRenderPass()
{
    ASSERT(mHandle != VK_NULL_HANDLE);
    vkCmdEndRenderPass(mHandle);
}

void CommandBuffer::draw(uint32_t vertexCount,
                         uint32_t instanceCount,
                         uint32_t firstVertex,
                         uint32_t firstInstance)
{
    ASSERT(valid());
    vkCmdDraw(mHandle, vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandBuffer::bindPipeline(VkPipelineBindPoint pipelineBindPoint,
                                 const vk::Pipeline &pipeline)
{
    ASSERT(valid() && pipeline.valid());
    vkCmdBindPipeline(mHandle, pipelineBindPoint, pipeline.getHandle());
}

void CommandBuffer::bindVertexBuffers(uint32_t firstBinding,
                                      const std::vector<VkBuffer> &buffers,
                                      const std::vector<VkDeviceSize> &offsets)
{
    ASSERT(valid() && buffers.size() == offsets.size());
    vkCmdBindVertexBuffers(mHandle, firstBinding, static_cast<uint32_t>(buffers.size()),
                           buffers.data(), offsets.data());
}

// Image implementation.
Image::Image() : mCurrentLayout(VK_IMAGE_LAYOUT_UNDEFINED)
{
}

Image::Image(VkImage image) : WrappedObject(image), mCurrentLayout(VK_IMAGE_LAYOUT_UNDEFINED)
{
}

void Image::retain(VkDevice device, Image &&other)
{
    WrappedObject::retain(device, std::move(other));
    std::swap(mCurrentLayout, other.mCurrentLayout);
}

void Image::reset()
{
    mHandle = VK_NULL_HANDLE;
}

void Image::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyImage(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

Error Image::init(VkDevice device, const VkImageCreateInfo &createInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(vkCreateImage(device, &createInfo, nullptr, &mHandle));
    return NoError();
}

void Image::changeLayoutTop(VkImageAspectFlags aspectMask,
                            VkImageLayout newLayout,
                            CommandBuffer *commandBuffer)
{
    if (newLayout == mCurrentLayout)
    {
        // No-op.
        return;
    }

    changeLayoutWithStages(aspectMask, newLayout, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, commandBuffer);
}

void Image::changeLayoutWithStages(VkImageAspectFlags aspectMask,
                                   VkImageLayout newLayout,
                                   VkPipelineStageFlags srcStageMask,
                                   VkPipelineStageFlags dstStageMask,
                                   CommandBuffer *commandBuffer)
{
    VkImageMemoryBarrier imageMemoryBarrier;
    imageMemoryBarrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.pNext               = nullptr;
    imageMemoryBarrier.srcAccessMask       = 0;
    imageMemoryBarrier.dstAccessMask       = 0;
    imageMemoryBarrier.oldLayout           = mCurrentLayout;
    imageMemoryBarrier.newLayout           = newLayout;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.image               = mHandle;

    // TODO(jmadill): Is this needed for mipped/layer images?
    imageMemoryBarrier.subresourceRange.aspectMask     = aspectMask;
    imageMemoryBarrier.subresourceRange.baseMipLevel   = 0;
    imageMemoryBarrier.subresourceRange.levelCount     = 1;
    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    imageMemoryBarrier.subresourceRange.layerCount     = 1;

    // TODO(jmadill): Test all the permutations of the access flags.
    imageMemoryBarrier.srcAccessMask = GetBasicLayoutAccessFlags(mCurrentLayout);

    if (mCurrentLayout == VK_IMAGE_LAYOUT_PREINITIALIZED)
    {
        imageMemoryBarrier.srcAccessMask |= VK_ACCESS_HOST_WRITE_BIT;
    }

    imageMemoryBarrier.dstAccessMask = GetBasicLayoutAccessFlags(newLayout);

    if (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        imageMemoryBarrier.srcAccessMask |=
            (VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        imageMemoryBarrier.dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
    }

    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        imageMemoryBarrier.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    commandBuffer->singleImageBarrier(srcStageMask, dstStageMask, 0, imageMemoryBarrier);

    mCurrentLayout = newLayout;
}

void Image::getMemoryRequirements(VkDevice device, VkMemoryRequirements *requirementsOut) const
{
    ASSERT(valid());
    vkGetImageMemoryRequirements(device, mHandle, requirementsOut);
}

Error Image::bindMemory(VkDevice device, const vk::DeviceMemory &deviceMemory)
{
    ASSERT(valid() && deviceMemory.valid());
    ANGLE_VK_TRY(vkBindImageMemory(device, mHandle, deviceMemory.getHandle(), 0));
    return NoError();
}

// ImageView implementation.
ImageView::ImageView()
{
}

void ImageView::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyImageView(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

Error ImageView::init(VkDevice device, const VkImageViewCreateInfo &createInfo)
{
    ANGLE_VK_TRY(vkCreateImageView(device, &createInfo, nullptr, &mHandle));
    return NoError();
}

// Semaphore implementation.
Semaphore::Semaphore()
{
}

void Semaphore::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroySemaphore(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

Error Semaphore::init(VkDevice device)
{
    ASSERT(!valid());

    VkSemaphoreCreateInfo semaphoreInfo;
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = nullptr;
    semaphoreInfo.flags = 0;

    ANGLE_VK_TRY(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &mHandle));

    return NoError();
}

// Framebuffer implementation.
Framebuffer::Framebuffer()
{
}

void Framebuffer::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyFramebuffer(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

Error Framebuffer::init(VkDevice device, const VkFramebufferCreateInfo &createInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(vkCreateFramebuffer(device, &createInfo, nullptr, &mHandle));
    return NoError();
}

// DeviceMemory implementation.
DeviceMemory::DeviceMemory()
{
}

void DeviceMemory::destroy(VkDevice device)
{
    if (valid())
    {
        vkFreeMemory(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

Error DeviceMemory::allocate(VkDevice device, const VkMemoryAllocateInfo &allocInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(vkAllocateMemory(device, &allocInfo, nullptr, &mHandle));
    return NoError();
}

Error DeviceMemory::map(VkDevice device,
                        VkDeviceSize offset,
                        VkDeviceSize size,
                        VkMemoryMapFlags flags,
                        uint8_t **mapPointer)
{
    ASSERT(valid());
    ANGLE_VK_TRY(
        vkMapMemory(device, mHandle, offset, size, flags, reinterpret_cast<void **>(mapPointer)));
    return NoError();
}

void DeviceMemory::unmap(VkDevice device)
{
    ASSERT(valid());
    vkUnmapMemory(device, mHandle);
}

// RenderPass implementation.
RenderPass::RenderPass()
{
}

void RenderPass::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyRenderPass(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

Error RenderPass::init(VkDevice device, const VkRenderPassCreateInfo &createInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(vkCreateRenderPass(device, &createInfo, nullptr, &mHandle));
    return NoError();
}

// StagingImage implementation.
StagingImage::StagingImage() : mSize(0)
{
}

StagingImage::StagingImage(StagingImage &&other)
    : mImage(std::move(other.mImage)),
      mDeviceMemory(std::move(other.mDeviceMemory)),
      mSize(other.mSize)
{
    other.mSize = 0;
}

void StagingImage::destroy(VkDevice device)
{
    mImage.destroy(device);
    mDeviceMemory.destroy(device);
}

void StagingImage::retain(VkDevice device, StagingImage &&other)
{
    mImage.retain(device, std::move(other.mImage));
    mDeviceMemory.retain(device, std::move(other.mDeviceMemory));
    std::swap(mSize, other.mSize);
}

Error StagingImage::init(VkDevice device,
                         uint32_t queueFamilyIndex,
                         uint32_t hostVisibleMemoryIndex,
                         TextureDimension dimension,
                         VkFormat format,
                         const gl::Extents &extent)
{
    VkImageCreateInfo createInfo;

    createInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.pNext         = nullptr;
    createInfo.flags         = 0;
    createInfo.imageType     = VK_IMAGE_TYPE_2D;
    createInfo.format        = format;
    createInfo.extent.width  = static_cast<uint32_t>(extent.width);
    createInfo.extent.height = static_cast<uint32_t>(extent.height);
    createInfo.extent.depth  = static_cast<uint32_t>(extent.depth);
    createInfo.mipLevels     = 1;
    createInfo.arrayLayers   = 1;
    createInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling        = VK_IMAGE_TILING_LINEAR;
    createInfo.usage         = (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    createInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 1;
    createInfo.pQueueFamilyIndices   = &queueFamilyIndex;
    createInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;

    ANGLE_TRY(mImage.init(device, createInfo));

    VkMemoryRequirements memoryRequirements;
    mImage.getMemoryRequirements(device, &memoryRequirements);

    // Ensure we can read this memory.
    ANGLE_VK_CHECK((memoryRequirements.memoryTypeBits & (1 << hostVisibleMemoryIndex)) != 0,
                   VK_ERROR_VALIDATION_FAILED_EXT);

    VkMemoryAllocateInfo allocateInfo;
    allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext           = nullptr;
    allocateInfo.allocationSize  = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = hostVisibleMemoryIndex;

    ANGLE_TRY(mDeviceMemory.allocate(device, allocateInfo));
    ANGLE_TRY(mImage.bindMemory(device, mDeviceMemory));

    mSize = memoryRequirements.size;

    return NoError();
}

// Buffer implementation.
Buffer::Buffer()
{
}

void Buffer::destroy(VkDevice device)
{
    if (valid())
    {
        mMemory.destroy(device);

        vkDestroyBuffer(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

void Buffer::retain(VkDevice device, Buffer &&other)
{
    WrappedObject::retain(device, std::move(other));
    mMemory.retain(device, std::move(other.mMemory));
}

Error Buffer::init(VkDevice device, const VkBufferCreateInfo &createInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(vkCreateBuffer(device, &createInfo, nullptr, &mHandle));
    return NoError();
}

Error Buffer::bindMemory(VkDevice device)
{
    ASSERT(valid() && mMemory.valid());
    ANGLE_VK_TRY(vkBindBufferMemory(device, mHandle, mMemory.getHandle(), 0));
    return NoError();
}

// ShaderModule implementation.
ShaderModule::ShaderModule()
{
}

void ShaderModule::destroy(VkDevice device)
{
    if (mHandle != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

Error ShaderModule::init(VkDevice device, const VkShaderModuleCreateInfo &createInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(vkCreateShaderModule(device, &createInfo, nullptr, &mHandle));
    return NoError();
}

// Pipeline implementation.
Pipeline::Pipeline()
{
}

void Pipeline::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyPipeline(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

Error Pipeline::initGraphics(VkDevice device, const VkGraphicsPipelineCreateInfo &createInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &mHandle));
    return NoError();
}

// PipelineLayout implementation.
PipelineLayout::PipelineLayout()
{
}

void PipelineLayout::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyPipelineLayout(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

Error PipelineLayout::init(VkDevice device, const VkPipelineLayoutCreateInfo &createInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(vkCreatePipelineLayout(device, &createInfo, nullptr, &mHandle));
    return NoError();
}

// Fence implementation.
Fence::Fence()
{
}

void Fence::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyFence(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

Error Fence::init(VkDevice device, const VkFenceCreateInfo &createInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(vkCreateFence(device, &createInfo, nullptr, &mHandle));
    return NoError();
}

VkResult Fence::getStatus(VkDevice device) const
{
    return vkGetFenceStatus(device, mHandle);
}

// FenceAndCommandBuffer implementation.
FenceAndCommandBuffer::FenceAndCommandBuffer(Serial queueSerial,
                                             Fence &&fence,
                                             CommandBuffer &&commandBuffer)
    : mQueueSerial(queueSerial), mFence(std::move(fence)), mCommandBuffer(std::move(commandBuffer))
{
}

FenceAndCommandBuffer::FenceAndCommandBuffer(FenceAndCommandBuffer &&other)
    : mQueueSerial(std::move(other.mQueueSerial)),
      mFence(std::move(other.mFence)),
      mCommandBuffer(std::move(other.mCommandBuffer))
{
}

void FenceAndCommandBuffer::destroy(VkDevice device)
{
    mFence.destroy(device);
    mCommandBuffer.destroy(device);
}

vk::ErrorOrResult<bool> FenceAndCommandBuffer::finished(VkDevice device) const
{
    VkResult result = mFence.getStatus(device);
    // Should this be a part of ANGLE_VK_TRY?
    if (result == VK_NOT_READY)
    {
        return false;
    }
    ANGLE_VK_TRY(result);
    return true;
}

FenceAndCommandBuffer &FenceAndCommandBuffer::operator=(FenceAndCommandBuffer &&other)
{
    std::swap(mQueueSerial, other.mQueueSerial);
    mFence         = std::move(other.mFence);
    mCommandBuffer = std::move(other.mCommandBuffer);
    return *this;
}

}  // namespace vk

Optional<uint32_t> FindMemoryType(const VkPhysicalDeviceMemoryProperties &memoryProps,
                                  const VkMemoryRequirements &requirements,
                                  uint32_t propertyFlagMask)
{
    for (uint32_t typeIndex = 0; typeIndex < memoryProps.memoryTypeCount; ++typeIndex)
    {
        if ((requirements.memoryTypeBits & (1u << typeIndex)) != 0 &&
            ((memoryProps.memoryTypes[typeIndex].propertyFlags & propertyFlagMask) ==
             propertyFlagMask))
        {
            return typeIndex;
        }
    }

    return Optional<uint32_t>::Invalid();
}

namespace gl_vk
{
VkPrimitiveTopology GetPrimitiveTopology(GLenum mode)
{
    switch (mode)
    {
        case GL_TRIANGLES:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case GL_POINTS:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case GL_LINES:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case GL_LINE_STRIP:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case GL_TRIANGLE_FAN:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        case GL_TRIANGLE_STRIP:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case GL_LINE_LOOP:
            // TODO(jmadill): Implement line loop support.
            UNIMPLEMENTED();
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        default:
            UNREACHABLE();
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }
}

VkCullModeFlags GetCullMode(const gl::RasterizerState &rasterState)
{
    if (!rasterState.cullFace)
    {
        return VK_CULL_MODE_NONE;
    }

    switch (rasterState.cullMode)
    {
        case GL_FRONT:
            return VK_CULL_MODE_FRONT_BIT;
        case GL_BACK:
            return VK_CULL_MODE_BACK_BIT;
        case GL_FRONT_AND_BACK:
            return VK_CULL_MODE_FRONT_AND_BACK;
        default:
            UNREACHABLE();
            return VK_CULL_MODE_NONE;
    }
}

VkFrontFace GetFrontFace(GLenum frontFace)
{
    switch (frontFace)
    {
        case GL_CW:
            return VK_FRONT_FACE_CLOCKWISE;
        case GL_CCW:
            return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        default:
            UNREACHABLE();
            return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
}

}  // namespace gl_vk

}  // namespace rx

std::ostream &operator<<(std::ostream &stream, const rx::vk::Error &error)
{
    stream << error.toString();
    return stream;
}
