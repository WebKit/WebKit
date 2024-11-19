//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLMemoryVk.cpp: Implements the class methods for CLMemoryVk.

#include "libANGLE/renderer/vulkan/CLMemoryVk.h"
#include "CL/cl.h"
#include "CL/cl_half.h"
#include "common/aligned_memory.h"
#include "libANGLE/Error.h"
#include "libANGLE/renderer/vulkan/CLContextVk.h"
#include "libANGLE/renderer/vulkan/vk_cl_utils.h"
#include "libANGLE/renderer/vulkan/vk_renderer.h"

#include "libANGLE/renderer/CLMemoryImpl.h"
#include "libANGLE/renderer/Format.h"
#include "libANGLE/renderer/FormatID_autogen.h"

#include "libANGLE/CLBuffer.h"
#include "libANGLE/CLContext.h"
#include "libANGLE/CLImage.h"
#include "libANGLE/CLMemory.h"
#include "libANGLE/Error.h"
#include "libANGLE/cl_utils.h"

namespace rx
{
namespace
{
cl_int NormalizeFloatValue(float value, float maximum)
{
    if (value < 0)
    {
        return 0;
    }
    if (value > 1.f)
    {
        return static_cast<cl_int>(maximum);
    }
    float valueToRound = (value * maximum);

    if (fabsf(valueToRound) < 0x1.0p23f)
    {
        constexpr float magic[2] = {0x1.0p23f, -0x1.0p23f};
        float magicVal           = magic[valueToRound < 0.0f];
        valueToRound += magicVal;
        valueToRound -= magicVal;
    }

    return static_cast<cl_int>(valueToRound);
}
}  // anonymous namespace

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

VkBufferUsageFlags CLMemoryVk::getVkUsageFlags()
{
    return cl_vk::GetBufferUsageFlags(mMemory.getFlags());
}

VkMemoryPropertyFlags CLMemoryVk::getVkMemPropertyFlags()
{
    return cl_vk::GetMemoryPropertyFlags(mMemory.getFlags());
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

// Create a sub-buffer from the given buffer object
angle::Result CLMemoryVk::createSubBuffer(const cl::Buffer &buffer,
                                          cl::MemFlags flags,
                                          size_t size,
                                          CLMemoryImpl::Ptr *subBufferOut)
{
    ASSERT(buffer.isSubBuffer());

    CLBufferVk *bufferVk = new CLBufferVk(buffer);
    if (!bufferVk)
    {
        ANGLE_CL_RETURN_ERROR(CL_OUT_OF_HOST_MEMORY);
    }
    ANGLE_TRY(bufferVk->create(nullptr));
    *subBufferOut = CLMemoryImpl::Ptr(bufferVk);

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

vk::BufferHelper &CLBufferVk::getBuffer()
{
    if (isSubBuffer())
    {
        return getParent()->getBuffer();
    }
    return mBuffer;
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
        ANGLE_TRY(mParent->map(mMappedMemory, getOffset()));
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

    mStagingBufferInitialized = true;
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

angle::Result CLImageVk::copyStagingTo(void *ptr, size_t offset, size_t size)
{
    uint8_t *ptrOut;
    ANGLE_TRY(getStagingBuffer().map(mContext, &ptrOut));
    ANGLE_TRY(getStagingBuffer().invalidate(mRenderer));
    std::memcpy(ptr, ptrOut + offset, size);
    getStagingBuffer().unmap(mContext->getRenderer());
    return angle::Result::Continue;
}

angle::Result CLImageVk::copyStagingToFromWithPitch(void *hostPtr,
                                                    const cl::Coordinate &region,
                                                    const size_t rowPitch,
                                                    const size_t slicePitch,
                                                    StagingBufferCopyDirection copyStagingTo)
{
    uint8_t *ptrInBase  = nullptr;
    uint8_t *ptrOutBase = nullptr;
    cl::BufferBox stagingBufferBox{
        {},
        {static_cast<int>(region.x), static_cast<int>(region.y), static_cast<int>(region.z)},
        0,
        0,
        getElementSize()};

    if (copyStagingTo == StagingBufferCopyDirection::ToHost)
    {
        ptrOutBase = static_cast<uint8_t *>(hostPtr);
        ANGLE_TRY(getStagingBuffer().map(mContext, &ptrInBase));
    }
    else
    {
        ptrInBase = static_cast<uint8_t *>(hostPtr);
        ANGLE_TRY(getStagingBuffer().map(mContext, &ptrOutBase));
    }
    for (size_t slice = 0; slice < region.z; slice++)
    {
        for (size_t row = 0; row < region.y; row++)
        {
            size_t stagingBufferOffset = stagingBufferBox.getRowOffset(slice, row);
            size_t hostPtrOffset       = (slice * slicePitch + row * rowPitch);
            uint8_t *dst               = (copyStagingTo == StagingBufferCopyDirection::ToHost)
                                             ? ptrOutBase + hostPtrOffset
                                             : ptrOutBase + stagingBufferOffset;
            uint8_t *src               = (copyStagingTo == StagingBufferCopyDirection::ToHost)
                                             ? ptrInBase + stagingBufferOffset
                                             : ptrInBase + hostPtrOffset;
            memcpy(dst, src, region.x * getElementSize());
        }
    }
    getStagingBuffer().unmap(mContext->getRenderer());
    return angle::Result::Continue;
}

CLImageVk::CLImageVk(const cl::Image &image)
    : CLMemoryVk(image),
      mFormat(angle::FormatID::NONE),
      mArrayLayers(1),
      mImageSize(0),
      mElementSize(0),
      mDesc(image.getDescriptor()),
      mStagingBufferInitialized(false)
{}

CLImageVk::~CLImageVk()
{
    if (isMapped())
    {
        unmap();
    }

    mImage.destroy(mRenderer);
    mImageView.destroy(mContext->getDevice());
    if (isStagingBufferInitialized())
    {
        mStagingBuffer.destroy(mRenderer);
    }
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
        case CL_BGRA:
            mFormat = angle::Format::CLBGRAFormatToID(format.image_channel_data_type);
            break;
        case CL_sRGBA:
            mFormat = angle::Format::CLsRGBAFormatToID(format.image_channel_data_type);
            break;
        case CL_DEPTH:
            mFormat = angle::Format::CLDEPTHFormatToID(format.image_channel_data_type);
            break;
        case CL_DEPTH_STENCIL:
            mFormat = angle::Format::CLDEPTHSTENCILFormatToID(format.image_channel_data_type);
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

    switch (desc.type)
    {
        case cl::MemObjectType::Image1D_Buffer:
        case cl::MemObjectType::Image1D:
            mImageViewType = VK_IMAGE_VIEW_TYPE_1D;
            break;
        case cl::MemObjectType::Image1D_Array:
            mImageViewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
            break;
        case cl::MemObjectType::Image2D:
            mImageViewType = VK_IMAGE_VIEW_TYPE_2D;
            break;
        case cl::MemObjectType::Image2D_Array:
            mImageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            break;
        case cl::MemObjectType::Image3D:
            mImageViewType = VK_IMAGE_VIEW_TYPE_3D;
            break;
        default:
            UNREACHABLE();
    }

    mElementSize = cl::GetElementSize(format);
    mDesc        = desc;
    mImageFormat = format;

    if (desc.slicePitch > 0)
    {
        mImageSize = (desc.slicePitch * mExtent.depth);
    }
    else if (desc.rowPitch > 0)
    {
        mImageSize = (desc.rowPitch * mExtent.height * mExtent.depth);
    }
    else
    {
        mImageSize = (mExtent.height * mExtent.width * mExtent.depth * mElementSize);
    }

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
        if (mDesc.rowPitch == 0 && mDesc.slicePitch == 0)
        {
            ANGLE_CL_IMPL_TRY_ERROR(copyStagingFrom(hostPtr, 0, mImageSize), CL_OUT_OF_RESOURCES);
        }
        else
        {
            ANGLE_TRY(copyStagingToFromWithPitch(
                hostPtr, {mExtent.width, mExtent.height, mExtent.depth}, mDesc.rowPitch,
                mDesc.slicePitch, StagingBufferCopyDirection::ToStagingBuffer));
        }
        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset      = 0;
        copyRegion.bufferRowLength   = 0;
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageExtent = getExtentForCopy({mExtent.width, mExtent.height, mExtent.depth});
        copyRegion.imageOffset = getOffsetForCopy({0, 0, 0});
        copyRegion.imageSubresource =
            getSubresourceLayersForCopy({0, 0, 0}, {mExtent.width, mExtent.height, mExtent.depth},
                                        mDesc.type, ImageCopyWith::Buffer);

        ANGLE_CL_IMPL_TRY_ERROR(mImage.copyToBufferOneOff(mContext, &mStagingBuffer, copyRegion),
                                CL_OUT_OF_RESOURCES);
    }

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.flags                 = 0;
    viewInfo.image                 = getImage().getImage().getHandle();
    viewInfo.format                = getImage().getActualVkFormat();

    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = static_cast<uint32_t>(getArraySize());
    viewInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;

    viewInfo.viewType = mImageViewType;
    ANGLE_VK_TRY(mContext, mImageView.init(mContext->getDevice(), viewInfo));

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

void CLImageVk::packPixels(const void *fillColor, PixelColor *packedColor)
{
    size_t channelCount = cl::GetChannelCount(mImageFormat.image_channel_order);

    switch (mImageFormat.image_channel_data_type)
    {
        case CL_UNORM_INT8:
        {
            float *srcVector = static_cast<float *>(const_cast<void *>(fillColor));
            if (mImageFormat.image_channel_order == CL_BGRA)
            {
                packedColor->u8[0] =
                    static_cast<unsigned char>(NormalizeFloatValue(srcVector[2], 255.f));
                packedColor->u8[1] =
                    static_cast<unsigned char>(NormalizeFloatValue(srcVector[1], 255.f));
                packedColor->u8[2] =
                    static_cast<unsigned char>(NormalizeFloatValue(srcVector[0], 255.f));
                packedColor->u8[3] =
                    static_cast<unsigned char>(NormalizeFloatValue(srcVector[3], 255.f));
            }
            else
            {
                for (unsigned int i = 0; i < channelCount; i++)
                    packedColor->u8[i] =
                        static_cast<unsigned char>(NormalizeFloatValue(srcVector[i], 255.f));
            }
            break;
        }
        case CL_SIGNED_INT8:
        {
            int *srcVector = static_cast<int *>(const_cast<void *>(fillColor));
            for (unsigned int i = 0; i < channelCount; i++)
                packedColor->s8[i] = static_cast<char>(std::clamp(srcVector[i], -128, 127));
            break;
        }
        case CL_UNSIGNED_INT8:
        {
            unsigned int *srcVector = static_cast<unsigned int *>(const_cast<void *>(fillColor));
            for (unsigned int i = 0; i < channelCount; i++)
                packedColor->u8[i] = static_cast<unsigned char>(
                    std::clamp(static_cast<unsigned int>(srcVector[i]),
                               static_cast<unsigned int>(0), static_cast<unsigned int>(255)));
            break;
        }
        case CL_UNORM_INT16:
        {
            float *srcVector = static_cast<float *>(const_cast<void *>(fillColor));
            for (unsigned int i = 0; i < channelCount; i++)
                packedColor->u16[i] =
                    static_cast<unsigned short>(NormalizeFloatValue(srcVector[i], 65535.f));
            break;
        }
        case CL_SIGNED_INT16:
        {
            int *srcVector = static_cast<int *>(const_cast<void *>(fillColor));
            for (unsigned int i = 0; i < channelCount; i++)
                packedColor->s16[i] = static_cast<short>(std::clamp(srcVector[i], -32768, 32767));
            break;
        }
        case CL_UNSIGNED_INT16:
        {
            unsigned int *srcVector = static_cast<unsigned int *>(const_cast<void *>(fillColor));
            for (unsigned int i = 0; i < channelCount; i++)
                packedColor->u16[i] = static_cast<unsigned short>(
                    std::clamp(static_cast<unsigned int>(srcVector[i]),
                               static_cast<unsigned int>(0), static_cast<unsigned int>(65535)));
            break;
        }
        case CL_HALF_FLOAT:
        {
            float *srcVector = static_cast<float *>(const_cast<void *>(fillColor));
            for (unsigned int i = 0; i < channelCount; i++)
                packedColor->fp16[i] = cl_half_from_float(srcVector[i], CL_HALF_RTE);
            break;
        }
        case CL_SIGNED_INT32:
        {
            int *srcVector = static_cast<int *>(const_cast<void *>(fillColor));
            for (unsigned int i = 0; i < channelCount; i++)
                packedColor->s32[i] = static_cast<int>(srcVector[i]);
            break;
        }
        case CL_UNSIGNED_INT32:
        {
            unsigned int *srcVector = static_cast<unsigned int *>(const_cast<void *>(fillColor));
            for (unsigned int i = 0; i < channelCount; i++)
                packedColor->u32[i] = static_cast<unsigned int>(srcVector[i]);
            break;
        }
        case CL_FLOAT:
        {
            float *srcVector = static_cast<float *>(const_cast<void *>(fillColor));
            for (unsigned int i = 0; i < channelCount; i++)
                packedColor->fp32[i] = srcVector[i];
            break;
        }
        default:
            UNIMPLEMENTED();
            break;
    }
}

void CLImageVk::fillImageWithColor(const cl::MemOffsets &origin,
                                   const cl::Coordinate &region,
                                   uint8_t *imagePtr,
                                   PixelColor *packedColor)
{
    size_t elementSize = getElementSize();
    cl::BufferBox stagingBufferBox{
        {},
        {static_cast<int>(mExtent.width), static_cast<int>(mExtent.height),
         static_cast<int>(mExtent.depth)},
        0,
        0,
        elementSize};
    uint8_t *ptrBase = imagePtr + (origin.z * stagingBufferBox.getSlicePitch()) +
                       (origin.y * stagingBufferBox.getRowPitch()) + (origin.x * elementSize);
    for (size_t slice = 0; slice < region.z; slice++)
    {
        for (size_t row = 0; row < region.y; row++)
        {
            size_t stagingBufferOffset = stagingBufferBox.getRowOffset(slice, row);
            uint8_t *pixelPtr          = ptrBase + stagingBufferOffset;
            for (size_t x = 0; x < region.x; x++)
            {
                memcpy(pixelPtr, packedColor, mElementSize);
                pixelPtr += mElementSize;
            }
        }
    }
}

VkExtent3D CLImageVk::getExtentForCopy(const cl::Coordinate &region)
{
    VkExtent3D extent = {};
    extent.width      = static_cast<uint32_t>(region.x);
    extent.height     = static_cast<uint32_t>(region.y);
    extent.depth      = static_cast<uint32_t>(region.z);
    switch (mDesc.type)
    {
        case cl::MemObjectType::Image1D_Array:

            extent.height = 1;
            extent.depth  = 1;
            break;
        case cl::MemObjectType::Image2D_Array:
            extent.depth = 1;
            break;
        default:
            break;
    }
    return extent;
}

VkOffset3D CLImageVk::getOffsetForCopy(const cl::MemOffsets &origin)
{
    VkOffset3D offset = {};
    offset.x          = static_cast<int32_t>(origin.x);
    offset.y          = static_cast<int32_t>(origin.y);
    offset.z          = static_cast<int32_t>(origin.z);
    switch (mDesc.type)
    {
        case cl::MemObjectType::Image1D_Array:
            offset.y = 0;
            offset.z = 0;
            break;
        case cl::MemObjectType::Image2D_Array:
            offset.z = 0;
            break;
        default:
            break;
    }
    return offset;
}

VkImageSubresourceLayers CLImageVk::getSubresourceLayersForCopy(const cl::MemOffsets &origin,
                                                                const cl::Coordinate &region,
                                                                cl::MemObjectType copyToType,
                                                                ImageCopyWith imageCopy)
{
    VkImageSubresourceLayers subresource = {};
    subresource.aspectMask               = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource.mipLevel                 = 0;
    switch (mDesc.type)
    {
        case cl::MemObjectType::Image1D_Array:
            subresource.baseArrayLayer = static_cast<uint32_t>(origin.y);
            if (imageCopy == ImageCopyWith::Image)
            {
                subresource.layerCount = static_cast<uint32_t>(region.y);
            }
            else
            {
                subresource.layerCount = static_cast<uint32_t>(mArrayLayers);
            }
            break;
        case cl::MemObjectType::Image2D_Array:
            subresource.baseArrayLayer = static_cast<uint32_t>(origin.z);
            if (copyToType == cl::MemObjectType::Image2D ||
                copyToType == cl::MemObjectType::Image3D)
            {
                subresource.layerCount = 1;
            }
            else if (imageCopy == ImageCopyWith::Image)
            {
                subresource.layerCount = static_cast<uint32_t>(region.z);
            }
            else
            {
                subresource.layerCount = static_cast<uint32_t>(mArrayLayers);
            }
            break;
        default:
            subresource.baseArrayLayer = 0;
            subresource.layerCount     = 1;
            break;
    }
    return subresource;
}

angle::Result CLImageVk::mapImpl()
{
    ASSERT(!isMapped());

    ASSERT(isStagingBufferInitialized());
    ANGLE_TRY(getStagingBuffer().map(mContext, &mMappedMemory));

    return angle::Result::Continue;
}
void CLImageVk::unmapImpl()
{
    getStagingBuffer().unmap(mContext->getRenderer());
    mMappedMemory = nullptr;
}

size_t CLImageVk::calculateRowPitch()
{
    return (mExtent.width * mElementSize);
}

size_t CLImageVk::calculateSlicePitch(size_t imageRowPitch)
{
    size_t slicePitch = 0;

    switch (mDesc.type)
    {
        case cl::MemObjectType::Image1D:
        case cl::MemObjectType::Image1D_Buffer:
        case cl::MemObjectType::Image2D:
            break;
        case cl::MemObjectType::Image2D_Array:
        case cl::MemObjectType::Image3D:
            slicePitch = (mExtent.height * imageRowPitch);
            break;
        case cl::MemObjectType::Image1D_Array:
            slicePitch = imageRowPitch;
            break;
        default:
            UNREACHABLE();
            break;
    }

    return slicePitch;
}

size_t CLImageVk::getRowPitch()
{
    size_t mRowPitch = mDesc.rowPitch;
    if (mRowPitch == 0)
    {
        mRowPitch = calculateRowPitch();
    }

    return mRowPitch;
}

size_t CLImageVk::getSlicePitch(size_t imageRowPitch)
{
    size_t mSlicePitch = mDesc.slicePitch;

    if (mSlicePitch == 0)
    {
        mSlicePitch = calculateSlicePitch(imageRowPitch);
    }

    return mSlicePitch;
}

}  // namespace rx
