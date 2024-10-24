//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLMemoryVk.cpp: Implements the class methods for CLMemoryVk.

#include "libANGLE/renderer/vulkan/CLMemoryVk.h"
#include <cstdint>
#include "common/aligned_memory.h"
#include "libANGLE/Error.h"
#include "libANGLE/renderer/vulkan/CLContextVk.h"
#include "libANGLE/renderer/vulkan/vk_renderer.h"

#include "libANGLE/renderer/Format.h"
#include "libANGLE/renderer/FormatID_autogen.h"

#include "libANGLE/CLBuffer.h"
#include "libANGLE/CLContext.h"
#include "libANGLE/CLImage.h"
#include "libANGLE/CLMemory.h"
#include "libANGLE/cl_utils.h"

namespace rx
{

CLMemoryVk::CLMemoryVk(const cl::Memory &memory)
    : CLMemoryImpl(memory),
      mContext(&memory.getContext().getImpl<CLContextVk>()),
      mRenderer(mContext->getRenderer()),
      mMappedMemory(nullptr),
      mMapCount(0),
      mParent(nullptr)
{}

CLMemoryVk::~CLMemoryVk()
{
    mContext->mAssociatedObjects->mMemories.erase(mMemory.getNative());
}

angle::Result CLMemoryVk::createSubBuffer(const cl::Buffer &buffer,
                                          cl::MemFlags flags,
                                          size_t size,
                                          CLMemoryImpl::Ptr *subBufferOut)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

VkBufferUsageFlags CLMemoryVk::getVkUsageFlags()
{
    VkBufferUsageFlags usageFlags =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (mMemory.getFlags().intersects(CL_MEM_WRITE_ONLY))
    {
        usageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    else if (mMemory.getFlags().intersects(CL_MEM_READ_ONLY))
    {
        usageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    else
    {
        usageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }

    return usageFlags;
}

VkMemoryPropertyFlags CLMemoryVk::getVkMemPropertyFlags()
{
    // TODO: http://anglebug.com/42267018
    VkMemoryPropertyFlags propFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    if (mMemory.getFlags().intersects(CL_MEM_USE_HOST_PTR | CL_MEM_ALLOC_HOST_PTR |
                                      CL_MEM_COPY_HOST_PTR))
    {
        propFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }

    return propFlags;
}

angle::Result CLMemoryVk::map(uint8_t *&ptrOut, size_t offset)
{
    if (!isMapped())
    {
        ANGLE_TRY(mapImpl());
    }
    ptrOut = mMappedMemory + offset;
    return angle::Result::Continue;
}

angle::Result CLMemoryVk::copyTo(void *dst, size_t srcOffset, size_t size)
{
    uint8_t *src = nullptr;
    ANGLE_TRY(map(src, srcOffset));
    std::memcpy(dst, src, size);
    unmap();
    return angle::Result::Continue;
}

angle::Result CLMemoryVk::copyTo(CLMemoryVk *dst, size_t srcOffset, size_t dstOffset, size_t size)
{
    uint8_t *dstPtr = nullptr;
    ANGLE_TRY(dst->map(dstPtr, dstOffset));
    ANGLE_TRY(copyTo(dstPtr, srcOffset, size));
    dst->unmap();
    return angle::Result::Continue;
}

angle::Result CLMemoryVk::copyFrom(const void *src, size_t srcOffset, size_t size)
{
    uint8_t *dst = nullptr;
    ANGLE_TRY(map(dst, srcOffset));
    std::memcpy(dst, src, size);
    unmap();
    return angle::Result::Continue;
}

CLBufferVk::CLBufferVk(const cl::Buffer &buffer) : CLMemoryVk(buffer)
{
    if (buffer.isSubBuffer())
    {
        mParent = &buffer.getParent()->getImpl<CLBufferVk>();
    }
    mDefaultBufferCreateInfo             = {};
    mDefaultBufferCreateInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    mDefaultBufferCreateInfo.size        = buffer.getSize();
    mDefaultBufferCreateInfo.usage       = getVkUsageFlags();
    mDefaultBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
}

CLBufferVk::~CLBufferVk()
{
    if (isMapped())
    {
        unmap();
    }
    mBuffer.destroy(mRenderer);
}

angle::Result CLBufferVk::createSubBuffer(const cl::Buffer &buffer,
                                          cl::MemFlags flags,
                                          size_t size,
                                          CLMemoryImpl::Ptr *subBufferOut)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLBufferVk::create(void *hostPtr)
{
    if (!isSubBuffer())
    {
        VkBufferCreateInfo createInfo  = mDefaultBufferCreateInfo;
        createInfo.size                = getSize();
        VkMemoryPropertyFlags memFlags = getVkMemPropertyFlags();
        if (IsError(mBuffer.init(mContext, createInfo, memFlags)))
        {
            ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
        }
        if (mMemory.getFlags().intersects(CL_MEM_USE_HOST_PTR | CL_MEM_COPY_HOST_PTR))
        {
            ASSERT(hostPtr);
            ANGLE_CL_IMPL_TRY_ERROR(setDataImpl(static_cast<uint8_t *>(hostPtr), getSize(), 0),
                                    CL_OUT_OF_RESOURCES);
        }
    }
    return angle::Result::Continue;
}

angle::Result CLBufferVk::mapImpl()
{
    ASSERT(!isMapped());

    if (isSubBuffer())
    {
        ANGLE_TRY(mParent->map(mMappedMemory, static_cast<size_t>(mBuffer.getOffset())));
        return angle::Result::Continue;
    }
    ANGLE_TRY(mBuffer.map(mContext, &mMappedMemory));
    return angle::Result::Continue;
}

void CLBufferVk::unmapImpl()
{
    if (!isSubBuffer())
    {
        mBuffer.unmap(mRenderer);
    }
    mMappedMemory = nullptr;
}

angle::Result CLBufferVk::setDataImpl(const uint8_t *data, size_t size, size_t offset)
{
    // buffer cannot be in use state
    ASSERT(mBuffer.valid());
    ASSERT(!isCurrentlyInUse());
    ASSERT(size + offset <= getSize());
    ASSERT(data != nullptr);

    // Assuming host visible buffers for now
    // TODO: http://anglebug.com/42267019
    if (!mBuffer.isHostVisible())
    {
        UNIMPLEMENTED();
        ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
    }

    uint8_t *mapPointer = nullptr;
    ANGLE_TRY(mBuffer.mapWithOffset(mContext, &mapPointer, offset));
    ASSERT(mapPointer != nullptr);

    std::memcpy(mapPointer, data, size);
    mBuffer.unmap(mRenderer);

    return angle::Result::Continue;
}

bool CLBufferVk::isCurrentlyInUse() const
{
    return !mRenderer->hasResourceUseFinished(mBuffer.getResourceUse());
}

VkImageUsageFlags CLImageVk::getVkImageUsageFlags()
{
    VkImageUsageFlags usageFlags =
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (mMemory.getFlags().intersects(CL_MEM_WRITE_ONLY))
    {
        usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    else if (mMemory.getFlags().intersects(CL_MEM_READ_ONLY))
    {
        usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    else
    {
        usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    }

    return usageFlags;
}

VkImageType CLImageVk::getVkImageType(const cl::ImageDescriptor &desc)
{
    VkImageType imageType = VK_IMAGE_TYPE_MAX_ENUM;

    switch (desc.type)
    {
        case cl::MemObjectType::Image1D_Buffer:
        case cl::MemObjectType::Image1D:
        case cl::MemObjectType::Image1D_Array:
            return VK_IMAGE_TYPE_1D;
        case cl::MemObjectType::Image2D:
        case cl::MemObjectType::Image2D_Array:
            return VK_IMAGE_TYPE_2D;
        case cl::MemObjectType::Image3D:
            return VK_IMAGE_TYPE_3D;
        default:
            UNREACHABLE();
    }

    return imageType;
}

angle::Result CLImageVk::createStagingBuffer(size_t size)
{
    const VkBufferCreateInfo createInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                           nullptr,
                                           0,
                                           size,
                                           getVkUsageFlags(),
                                           VK_SHARING_MODE_EXCLUSIVE,
                                           0,
                                           nullptr};
    if (IsError(mStagingBuffer.init(mContext, createInfo, getVkMemPropertyFlags())))
    {
        ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
    }

    return angle::Result::Continue;
}

angle::Result CLImageVk::copyStagingFrom(void *ptr, size_t offset, size_t size)
{
    uint8_t *ptrOut;
    uint8_t *ptrIn = static_cast<uint8_t *>(ptr);
    ANGLE_TRY(getStagingBuffer().map(mContext, &ptrOut));
    std::memcpy(ptrOut, ptrIn + offset, size);
    ANGLE_TRY(getStagingBuffer().flush(mRenderer));
    getStagingBuffer().unmap(mContext->getRenderer());
    return angle::Result::Continue;
}

CLImageVk::CLImageVk(const cl::Image &image)
    : CLMemoryVk(image),
      mFormat(angle::FormatID::NONE),
      mArrayLayers(1),
      mImageSize(0),
      mNumberChannels(0),
      mBytesPerChannel(0)
{}

CLImageVk::~CLImageVk()
{
    if (isMapped())
    {
        unmap();
    }
    mImage.destroy(mRenderer);
}

angle::Result CLImageVk::create(void *hostPtr)
{
    const cl::Image &image         = reinterpret_cast<const cl::Image &>(mMemory);
    const cl::ImageDescriptor desc = image.getDescriptor();
    const cl_image_format format   = image.getFormat();
    switch (format.image_channel_order)
    {
        case CL_R:
        case CL_LUMINANCE:
        case CL_INTENSITY:
            mFormat = angle::Format::CLRFormatToID(format.image_channel_data_type);
            break;
        case CL_RG:
            mFormat = angle::Format::CLRGFormatToID(format.image_channel_data_type);
            break;
        case CL_RGB:
            mFormat = angle::Format::CLRGBFormatToID(format.image_channel_data_type);
            break;
        case CL_RGBA:
            mFormat = angle::Format::CLRGBAFormatToID(format.image_channel_data_type);
            break;
        default:
            UNIMPLEMENTED();
            break;
    }

    mExtent.width  = static_cast<uint32_t>(desc.width);
    mExtent.height = static_cast<uint32_t>(desc.height);
    mExtent.depth  = static_cast<uint32_t>(desc.depth);
    switch (desc.type)
    {
        case cl::MemObjectType::Image1D_Buffer:
        case cl::MemObjectType::Image1D:
        case cl::MemObjectType::Image1D_Array:
            mExtent.height = 1;
            mExtent.depth  = 1;
            break;
        case cl::MemObjectType::Image2D:
        case cl::MemObjectType::Image2D_Array:
            mExtent.depth = 1;
            break;
        case cl::MemObjectType::Image3D:
            break;
        default:
            UNREACHABLE();
    }

    switch (format.image_channel_order)
    {
        case CL_R:
        case CL_A:
        case CL_DEPTH:
        case CL_LUMINANCE:
        case CL_INTENSITY:
            mNumberChannels = 1;
            break;
        case CL_RG:
        case CL_RA:
        case CL_Rx:
            mNumberChannels = 2;
            break;
        case CL_RGB:
        case CL_RGx:
            mNumberChannels = 3;
            break;
        case CL_RGBA:
        case CL_BGRA:
            mNumberChannels = 4;
            break;
        default:
            UNREACHABLE();
    }

    switch (format.image_channel_data_type)
    {
        case CL_SNORM_INT8:
        case CL_UNORM_INT8:
        case CL_SIGNED_INT8:
        case CL_UNSIGNED_INT8:
            mBytesPerChannel = 1;
            break;
        case CL_SNORM_INT16:
        case CL_UNORM_INT16:
        case CL_SIGNED_INT16:
        case CL_UNSIGNED_INT16:
        case CL_HALF_FLOAT:
        case CL_UNORM_SHORT_565:
        case CL_UNORM_SHORT_555:
            mBytesPerChannel = 2;
            break;
        case CL_SIGNED_INT32:
        case CL_UNSIGNED_INT32:
        case CL_FLOAT:
            mBytesPerChannel = 4;
            break;
        default:
            UNREACHABLE();
    }

    mImageSize =
        (mExtent.height * mExtent.width * mExtent.depth * mNumberChannels * mBytesPerChannel);
    if ((desc.type == cl::MemObjectType::Image1D_Array) ||
        (desc.type == cl::MemObjectType::Image2D_Array))
    {
        mArrayLayers = static_cast<uint32_t>(desc.arraySize);
        mImageSize *= desc.arraySize;
    }

    ANGLE_CL_IMPL_TRY_ERROR(
        mImage.initStaging(mContext, false, mRenderer->getMemoryProperties(), getVkImageType(desc),
                           mExtent, mFormat, mFormat, VK_SAMPLE_COUNT_1_BIT, getVkImageUsageFlags(),
                           1, mArrayLayers),
        CL_OUT_OF_RESOURCES);

    if (mMemory.getFlags().intersects(CL_MEM_USE_HOST_PTR | CL_MEM_COPY_HOST_PTR))
    {
        ASSERT(hostPtr);
        ANGLE_CL_IMPL_TRY_ERROR(createStagingBuffer(mImageSize), CL_OUT_OF_RESOURCES);
        ANGLE_CL_IMPL_TRY_ERROR(copyStagingFrom(hostPtr, 0, mImageSize), CL_OUT_OF_RESOURCES);

        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset      = 0;
        copyRegion.bufferRowLength   = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel       = (int)desc.numMipLevels;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount     = 1;

        copyRegion.imageOffset = {0, 0, 0};
        copyRegion.imageExtent = {mExtent.width, mExtent.height, mExtent.depth};

        ANGLE_CL_IMPL_TRY_ERROR(mImage.copyToBufferOneOff(mContext, &mStagingBuffer, copyRegion),
                                CL_OUT_OF_RESOURCES);
    }

    return angle::Result::Continue;
}

bool CLImageVk::isCurrentlyInUse() const
{
    return !mRenderer->hasResourceUseFinished(mImage.getResourceUse());
}

bool CLImageVk::containsHostMemExtension()
{
    const vk::ExtensionNameList &enabledDeviceExtensions = mRenderer->getEnabledDeviceExtensions();
    return std::find(enabledDeviceExtensions.begin(), enabledDeviceExtensions.end(),
                     "VK_EXT_external_memory_host") != enabledDeviceExtensions.end();
}

angle::Result CLImageVk::mapImpl()
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}
void CLImageVk::unmapImpl()
{
    UNIMPLEMENTED();
}

}  // namespace rx
