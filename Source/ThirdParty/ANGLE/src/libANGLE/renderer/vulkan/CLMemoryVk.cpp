//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLMemoryVk.cpp: Implements the class methods for CLMemoryVk.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "common/log_utils.h"

#include <cstddef>
#include "libANGLE/renderer/vulkan/CLContextVk.h"
#include "libANGLE/renderer/vulkan/CLMemoryVk.h"
#include "libANGLE/renderer/vulkan/vk_cl_utils.h"
#include "libANGLE/renderer/vulkan/vk_renderer.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"
#include "libANGLE/renderer/vulkan/vk_wrapper.h"

#include "libANGLE/renderer/CLMemoryImpl.h"
#include "libANGLE/renderer/Format.h"
#include "libANGLE/renderer/FormatID_autogen.h"

#include "libANGLE/CLBuffer.h"
#include "libANGLE/CLContext.h"
#include "libANGLE/CLImage.h"
#include "libANGLE/CLMemory.h"
#include "libANGLE/Error.h"
#include "libANGLE/cl_types.h"
#include "libANGLE/cl_utils.h"

#include "CL/cl_half.h"

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

angle::FormatID CLImageFormatToAngleFormat(cl_image_format format)
{
    switch (format.image_channel_order)
    {
        case CL_R:
        case CL_LUMINANCE:
        case CL_INTENSITY:
            return angle::Format::CLRFormatToID(format.image_channel_data_type);
        case CL_RG:
            return angle::Format::CLRGFormatToID(format.image_channel_data_type);
        case CL_RGB:
            return angle::Format::CLRGBFormatToID(format.image_channel_data_type);
        case CL_RGBA:
            return angle::Format::CLRGBAFormatToID(format.image_channel_data_type);
        case CL_BGRA:
            return angle::Format::CLBGRAFormatToID(format.image_channel_data_type);
        case CL_sRGBA:
            return angle::Format::CLsRGBAFormatToID(format.image_channel_data_type);
        case CL_DEPTH:
            return angle::Format::CLDEPTHFormatToID(format.image_channel_data_type);
        case CL_DEPTH_STENCIL:
            return angle::Format::CLDEPTHSTENCILFormatToID(format.image_channel_data_type);
        default:
            return angle::FormatID::NONE;
    }
}

bool GetExternalMemoryHandleInfo(const cl_mem_properties *properties,
                                 VkExternalMemoryHandleTypeFlagBits *vkExtMemoryHandleType,
                                 int32_t *fd)
{
    bool propertyStatus = true;
    const cl::NameValueProperty *propertyIterator =
        reinterpret_cast<const cl::NameValueProperty *>(properties);
    while (propertyIterator->name != 0)
    {
        switch (propertyIterator->name)
        {
            case CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_FD_KHR:
                *vkExtMemoryHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
                break;
            case CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR:
                *vkExtMemoryHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
                break;
            case CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR:
                *vkExtMemoryHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
                break;
            case CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KMT_KHR:
                *vkExtMemoryHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT;
                break;
            default:
                propertyStatus = false;
                break;
        }

        if (propertyStatus)
        {
            *fd = *(reinterpret_cast<int32_t *>(propertyIterator->value));
            if (*fd < 0)
            {
                propertyStatus = false;
            }
            break;
        }
        propertyIterator++;
    }

    return propertyStatus;
}

}  // namespace

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
    return cl_vk::GetBufferUsageFlags(mMemory.getFlags(),
                                      mContext->getFeatures().supportsBufferDeviceAddress.enabled);
}

VkMemoryPropertyFlags CLMemoryVk::getVkMemPropertyFlags()
{
    return cl_vk::GetMemoryPropertyFlags(mMemory.getFlags());
}

angle::Result CLMemoryVk::map(uint8_t *&ptrOut, size_t offset)
{
    if (getFlags().intersects(CL_MEM_USE_HOST_PTR))
    {
        // as per spec, the returned pointer for USE_HOST_PTR will be derived from the hostptr...
        ASSERT(mMemory.getHostPtr());
        ptrOut = static_cast<uint8_t *>(mMemory.getHostPtr()) + offset;
    }
    else
    {
        // ...otherwise we just map the VK memory to cpu va space
        ANGLE_TRY(mapBufferHelper(ptrOut));
        ptrOut += offset;
    }

    return angle::Result::Continue;
}

angle::Result CLMemoryVk::copyTo(void *dst, size_t srcOffset, size_t size)
{
    uint8_t *src = nullptr;
    ANGLE_TRY(mapBufferHelper(src));
    src += srcOffset;
    std::memcpy(dst, src, size);
    unmapBufferHelper();
    return angle::Result::Continue;
}

angle::Result CLMemoryVk::copyFrom(const void *src, size_t srcOffset, size_t size)
{
    uint8_t *dst = nullptr;
    ANGLE_TRY(mapBufferHelper(dst));
    dst += srcOffset;
    std::memcpy(dst, src, size);
    unmapBufferHelper();
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
        unmapBufferHelper();
    }
    mBuffer.destroy(mRenderer);
}

bool CLBufferVk::isHostPtrAligned() const
{
    VkDeviceSize alignment =
        mRenderer->getPhysicalDeviceExternalMemoryHostProperties().minImportedHostPointerAlignment;
    return reinterpret_cast<uintptr_t>(mMemory.getHostPtr()) % alignment == 0 &&
           getSize() % alignment == 0;
}

bool CLBufferVk::supportsZeroCopy() const
{
    return mRenderer->getFeatures().supportsExternalMemoryHost.enabled &&
           mMemory.getFlags().intersects(CL_MEM_USE_HOST_PTR) && isHostPtrAligned();
}

vk::BufferHelper &CLBufferVk::getBuffer()
{
    if (isSubBuffer())
    {
        return getParent()->getBuffer();
    }
    return mBuffer;
}

// For UHP buffers, the buffer contents and hostptr have to be in sync at appropriate times. Ensure
// that if zero copy is not supported.
angle::Result CLBufferVk::syncHost(CLBufferVk::SyncHostDirection direction)
{
    switch (direction)
    {
        case CLBufferVk::SyncHostDirection::FromHost:
            if (getFlags().intersects(CL_MEM_USE_HOST_PTR) && !supportsZeroCopy())
            {
                ANGLE_CL_IMPL_TRY_ERROR(
                    setDataImpl(static_cast<const uint8_t *>(getHostPtr()), getSize(), 0),
                    CL_OUT_OF_RESOURCES);
            }
            break;
        case CLBufferVk::SyncHostDirection::ToHost:
            if (getFlags().intersects(CL_MEM_USE_HOST_PTR) && !supportsZeroCopy())
            {
                ANGLE_TRY(copyTo(getHostPtr(), 0, getSize()));
            }
            break;
        default:
            UNREACHABLE();
            break;
    }
    return angle::Result::Continue;
}

// This is to sync only a rectangular region between hostptr and buffer contents. Intended to be
// used for READ/WRITE_RECT.
angle::Result CLBufferVk::syncHost(CLBufferVk::SyncHostDirection direction, cl::BufferRect hostRect)
{
    switch (direction)
    {
        case CLBufferVk::SyncHostDirection::FromHost:
            if (getFlags().intersects(CL_MEM_USE_HOST_PTR) && !supportsZeroCopy())
            {
                ANGLE_TRY(setRect(getHostPtr(), hostRect, hostRect));
            }
            break;
        case CLBufferVk::SyncHostDirection::ToHost:
            if (getFlags().intersects(CL_MEM_USE_HOST_PTR) && !supportsZeroCopy())
            {
                ANGLE_TRY(getRect(hostRect, hostRect, getHostPtr()));
            }
            break;
        default:
            UNREACHABLE();
            break;
    }
    return angle::Result::Continue;
}

angle::Result CLBufferVk::create(void *hostPtr)
{
    const cl_mem_properties *properties = getFrontendObject().getProperties().data();
    if (properties)
    {
        const cl::NameValueProperty *property =
            reinterpret_cast<const cl::NameValueProperty *>(properties);
        if (property->name != 0)
        {
            return createWithProperties();
        }
    }

    if (!isSubBuffer())
    {
        VkBufferCreateInfo createInfo  = mDefaultBufferCreateInfo;
        createInfo.size                = getSize();
        VkMemoryPropertyFlags memFlags = getVkMemPropertyFlags();

        if (supportsZeroCopy())
        {
            return mBuffer.initHostExternal(mContext, memFlags, createInfo, hostPtr);
        }

        ANGLE_CL_IMPL_TRY_ERROR(mBuffer.init(mContext, createInfo, memFlags), CL_OUT_OF_RESOURCES);
        // We need to copy the data from hostptr in the case of CHP buffer.
        if (getFlags().intersects(CL_MEM_COPY_HOST_PTR))
        {
            ANGLE_CL_IMPL_TRY_ERROR(setDataImpl(static_cast<uint8_t *>(hostPtr), getSize(), 0),
                                    CL_OUT_OF_RESOURCES);
        }
        ANGLE_TRY(syncHost(CLBufferVk::SyncHostDirection::FromHost));
    }
    return angle::Result::Continue;
}

angle::Result CLBufferVk::createWithProperties()
{
    ASSERT(!isSubBuffer());

    int32_t sharedBufferFD = -1;
    VkExternalMemoryHandleTypeFlagBits vkExtMemoryHandleType;
    const cl_mem_properties *properties = getFrontendObject().getProperties().data();
    if (GetExternalMemoryHandleInfo(properties, &vkExtMemoryHandleType, &sharedBufferFD))
    {
#if defined(ANGLE_PLATFORM_WINDOWS)
        UNIMPLEMENTED();
        ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
#else
        VkBufferCreateInfo createInfo  = mDefaultBufferCreateInfo;
        createInfo.size                = getSize();
        VkMemoryPropertyFlags memFlags = getVkMemPropertyFlags();

        // VK_KHR_external_memory assumes the ownership of the buffer as part of the import
        // operation. No such requirement is present with cl_khr_external_memory and
        // cl_arm_import_memory extension. So we dup the fd for now, and let the application still
        // hold on to the fd.
        if (IsError(mBuffer.initAndAcquireFromExternalMemory(
                mContext, memFlags, createInfo, vkExtMemoryHandleType, dup(sharedBufferFD))))
        {
            ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
        }
#endif
    }
    else
    {
        // Don't expect to be here, as validation layer should have caught unsupported uses.
        UNREACHABLE();
    }

    return angle::Result::Continue;
}

angle::Result CLBufferVk::copyToWithPitch(void *hostPtr,
                                          size_t srcOffset,
                                          size_t size,
                                          size_t rowPitch,
                                          size_t slicePitch,
                                          cl::Extents region,
                                          const size_t elementSize)
{
    uint8_t *ptrInBase  = nullptr;
    uint8_t *ptrOutBase = nullptr;
    cl::BufferRect stagingBufferRect{
        {static_cast<int>(0), static_cast<int>(0), static_cast<int>(0)},
        {region.width, region.height, region.depth},
        0,
        0,
        elementSize};

    ptrOutBase = static_cast<uint8_t *>(hostPtr);
    ANGLE_TRY(mapBufferHelper(ptrInBase));
    cl::Defer deferUnmap([this]() { unmapBufferHelper(); });

    for (size_t slice = 0; slice < region.depth; slice++)
    {
        for (size_t row = 0; row < region.height; row++)
        {
            size_t stagingBufferOffset = stagingBufferRect.getRowOffset(slice, row);
            size_t hostPtrOffset       = (slice * slicePitch + row * rowPitch);
            uint8_t *dst               = ptrOutBase + hostPtrOffset;
            uint8_t *src               = ptrInBase + stagingBufferOffset;
            memcpy(dst, src, region.width * elementSize);
        }
    }
    return angle::Result::Continue;
}

angle::Result CLBufferVk::mapBufferHelper(uint8_t *&ptrOut)
{
    if (!isMapped())
    {
        if (isSubBuffer())
        {
            return mapParentBufferHelper(ptrOut);
        }

        std::lock_guard<angle::SimpleMutex> lock(mMutex);
        ANGLE_TRY(mBuffer.map(mContext, &mMappedMemory));
        ++mMapCount;
    }
    ptrOut = mMappedMemory;
    ASSERT(ptrOut);

    return angle::Result::Continue;
}

angle::Result CLBufferVk::mapParentBufferHelper(uint8_t *&ptrOut)
{
    ASSERT(getParent());

    ANGLE_TRY(getParent()->mapBufferHelper(ptrOut));
    ptrOut += getOffset();
    return angle::Result::Continue;
}

void CLBufferVk::unmapBufferHelper()
{
    if (isSubBuffer())
    {
        getParent()->unmapBufferHelper();
        return;
    }

    std::lock_guard<angle::SimpleMutex> lock(mMutex);
    getBuffer().unmap(mContext->getRenderer());
    if (--mMapCount == 0u)
    {
        mMappedMemory = nullptr;
    }
}

angle::Result CLBufferVk::updateRect(UpdateRectOperation op,
                                     void *data,
                                     const cl::BufferRect &dataRect,
                                     const cl::BufferRect &bufferRect)
{
    ASSERT(dataRect.valid() && bufferRect.valid());
    ASSERT(dataRect.mSize == bufferRect.mSize && dataRect.mElementSize == bufferRect.mElementSize);

    uint8_t *bufferPtr = nullptr;
    ANGLE_TRY(mapBufferHelper(bufferPtr));
    cl::Defer deferUnmap([this]() { unmapBufferHelper(); });

    uint8_t *dataUint8Ptr = reinterpret_cast<uint8_t *>(data);
    for (size_t slice = 0; slice < bufferRect.mSize.depth; slice++)
    {
        for (size_t row = 0; row < bufferRect.mSize.height; row++)
        {
            uint8_t *offsetDataPtr   = dataUint8Ptr + dataRect.getRowOffset(slice, row);
            uint8_t *offsetBufferPtr = bufferPtr + bufferRect.getRowOffset(slice, row);
            size_t updateSize        = dataRect.mSize.width * dataRect.mElementSize;

            switch (op)
            {
                case UpdateRectOperation::Read:
                {
                    // Read from this buffer
                    memcpy(offsetDataPtr, offsetBufferPtr, updateSize);
                    break;
                }
                case UpdateRectOperation::Write:
                {
                    // Write to this buffer
                    memcpy(offsetBufferPtr, offsetDataPtr, updateSize);
                    break;
                }
                default:
                {
                    UNREACHABLE();
                    break;
                }
            }
        }
    }
    return angle::Result::Continue;
}

angle::Result CLBufferVk::setRect(const void *data,
                                  const cl::BufferRect &dataRect,
                                  const cl::BufferRect &bufferRect)
{
    return updateRect(UpdateRectOperation::Write, const_cast<void *>(data), dataRect, bufferRect);
}

angle::Result CLBufferVk::getRect(const cl::BufferRect &bufferRect,
                                  const cl::BufferRect &dataRect,
                                  void *outData)
{
    return updateRect(UpdateRectOperation::Read, outData, dataRect, bufferRect);
}

// offset is for mapped pointer
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

template <>
CLBufferVk *CLImageVk::getParent<CLBufferVk>() const
{
    if (mParent)
    {
        ASSERT(cl::IsBufferType(getParentType()));
        return static_cast<CLBufferVk *>(mParent);
    }
    return nullptr;
}

template <>
CLImageVk *CLImageVk::getParent<CLImageVk>() const
{
    if (mParent)
    {
        ASSERT(cl::IsImageType(getParentType()));
        return static_cast<CLImageVk *>(mParent);
    }
    return nullptr;
}

VkImageUsageFlags CLImageVk::getVkImageUsageFlags()
{
    VkImageUsageFlags usageFlags = vk::kImageUsageTransferBits;

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

angle::Result CLImageVk::getOrCreateStagingBuffer(CLBufferVk **clBufferOut)
{
    ASSERT(clBufferOut && "cannot pass nullptr to clBufferOut!");

    std::lock_guard<angle::SimpleMutex> lock(mMutex);

    if (!cl::Buffer::IsValid(mStagingBuffer))
    {
        mStagingBuffer = cl::Buffer::Cast(mContext->getFrontendObject().createBuffer(
            nullptr, cl::MemFlags(CL_MEM_READ_WRITE), getSize(), nullptr));
        if (!cl::Buffer::IsValid(mStagingBuffer))
        {
            ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
        }
    }
    *clBufferOut = &mStagingBuffer->getImpl<CLBufferVk>();
    return angle::Result::Continue;
}

angle::Result CLImageVk::copyStagingFrom(void *ptr, size_t offset, size_t size)
{
    uint8_t *ptrOut;
    uint8_t *ptrIn = static_cast<uint8_t *>(ptr);

    ANGLE_TRY(mapBufferHelper(ptrOut));
    cl::Defer deferUnmap([this]() { unmapBufferHelper(); });

    std::memcpy(ptrOut, ptrIn + offset, size);

    return angle::Result::Continue;
}

angle::Result CLImageVk::copyStagingTo(void *ptr, size_t offset, size_t size)
{
    uint8_t *ptrOut;

    ANGLE_TRY(mapBufferHelper(ptrOut));
    cl::Defer deferUnmap([this]() { unmapBufferHelper(); });

    std::memcpy(ptr, ptrOut + offset, size);

    return angle::Result::Continue;
}

angle::Result CLImageVk::copyStagingToFromWithPitch(void *hostPtr,
                                                    const cl::Extents &region,
                                                    const size_t rowPitch,
                                                    const size_t slicePitch,
                                                    StagingBufferCopyDirection copyStagingTo)
{
    uint8_t *ptrInBase  = nullptr;
    uint8_t *ptrOutBase = nullptr;
    cl::BufferRect stagingBufferRect{
        {}, {region.width, region.height, region.depth}, 0, 0, getElementSize()};

    if (copyStagingTo == StagingBufferCopyDirection::ToHost)
    {
        ptrOutBase = static_cast<uint8_t *>(hostPtr);
        ANGLE_TRY(mapBufferHelper(ptrInBase));
    }
    else
    {
        ptrInBase = static_cast<uint8_t *>(hostPtr);
        ANGLE_TRY(mapBufferHelper(ptrOutBase));
    }
    cl::Defer deferUnmap([this]() { unmapBufferHelper(); });

    for (size_t slice = 0; slice < region.depth; slice++)
    {
        for (size_t row = 0; row < region.height; row++)
        {
            size_t stagingBufferOffset = stagingBufferRect.getRowOffset(slice, row);
            size_t hostPtrOffset       = (slice * slicePitch + row * rowPitch);
            uint8_t *dst               = (copyStagingTo == StagingBufferCopyDirection::ToHost)
                                             ? ptrOutBase + hostPtrOffset
                                             : ptrOutBase + stagingBufferOffset;
            uint8_t *src               = (copyStagingTo == StagingBufferCopyDirection::ToHost)
                                             ? ptrInBase + stagingBufferOffset
                                             : ptrInBase + hostPtrOffset;
            memcpy(dst, src, region.width * getElementSize());
        }
    }
    return angle::Result::Continue;
}

CLImageVk::CLImageVk(const cl::Image &image)
    : CLMemoryVk(image),
      mExtent(cl::GetExtentFromDescriptor(image.getDescriptor())),
      mAngleFormat(CLImageFormatToAngleFormat(image.getFormat())),
      mStagingBuffer(nullptr),
      mImageViewType(cl_vk::GetImageViewType(image.getDescriptor().type))
{
    if (image.getParent())
    {
        mParent = &image.getParent()->getImpl<CLMemoryVk>();
    }
}

CLImageVk::~CLImageVk()
{
    if (isMapped())
    {
        unmap();
    }

    if (mBufferViews.isInitialized())
    {
        mBufferViews.release(mContext->getRenderer());
    }

    mImage.destroy(mRenderer);
    mImageView.destroy(mContext->getDevice());
    if (cl::Memory::IsValid(mStagingBuffer) && mStagingBuffer->release())
    {
        SafeDelete(mStagingBuffer);
    }
}

angle::Result CLImageVk::createFromBuffer()
{
    ASSERT(mParent);
    ASSERT(IsBufferType(getParentType()));

    // initialize the buffer views
    mBufferViews.init(mContext->getRenderer(), 0, getSize());

    return angle::Result::Continue;
}

angle::Result CLImageVk::create(void *hostPtr)
{
    if (mParent)
    {
        if (getType() == cl::MemObjectType::Image1D_Buffer)
        {
            return createFromBuffer();
        }
        else
        {
            UNIMPLEMENTED();
            ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
        }
    }

    ANGLE_CL_IMPL_TRY_ERROR(mImage.initStaging(mContext, false, getVkImageType(getDescriptor()),
                                               cl_vk::GetExtent(mExtent), mAngleFormat,
                                               mAngleFormat, VK_SAMPLE_COUNT_1_BIT,
                                               getVkImageUsageFlags(), 1, (uint32_t)getArraySize()),
                            CL_OUT_OF_RESOURCES);

    if (mMemory.getFlags().intersects(CL_MEM_USE_HOST_PTR | CL_MEM_COPY_HOST_PTR))
    {
        ASSERT(hostPtr);

        if (getDescriptor().rowPitch == 0 && getDescriptor().slicePitch == 0)
        {
            ANGLE_CL_IMPL_TRY_ERROR(copyStagingFrom(hostPtr, 0, getSize()), CL_OUT_OF_RESOURCES);
        }
        else
        {
            ANGLE_TRY(copyStagingToFromWithPitch(
                hostPtr, {mExtent.width, mExtent.height, mExtent.depth}, getDescriptor().rowPitch,
                getDescriptor().slicePitch, StagingBufferCopyDirection::ToStagingBuffer));
        }
        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset      = 0;
        copyRegion.bufferRowLength   = 0;
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageExtent =
            cl_vk::GetExtent(getExtentForCopy({mExtent.width, mExtent.height, mExtent.depth}));
        copyRegion.imageOffset      = cl_vk::GetOffset(cl::kOffsetZero);
        copyRegion.imageSubresource = getSubresourceLayersForCopy(
            cl::kOffsetZero, {mExtent.width, mExtent.height, mExtent.depth}, getType(),
            ImageCopyWith::Buffer);

        // copy over the hostptr bits here to image in a one-off copy cmd
        CLBufferVk *stagingBuffer = nullptr;
        ANGLE_TRY(getOrCreateStagingBuffer(&stagingBuffer));
        ASSERT(stagingBuffer);
        ANGLE_CL_IMPL_TRY_ERROR(
            mImage.copyToBufferOneOff(mContext, &stagingBuffer->getBuffer(), copyRegion),
            CL_OUT_OF_RESOURCES);
    }

    ANGLE_TRY(initImageViewImpl());
    return angle::Result::Continue;
}

angle::Result CLImageVk::initImageViewImpl()
{
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.flags                 = 0;
    viewInfo.image                 = getImage().getImage().getHandle();
    viewInfo.format                = getImage().getActualVkFormat(mContext->getRenderer());
    viewInfo.viewType              = mImageViewType;

    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    // We don't support mip map levels and should have been validated
    ASSERT(getDescriptor().numMipLevels == 0);
    viewInfo.subresourceRange.baseMipLevel   = getDescriptor().numMipLevels;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = static_cast<uint32_t>(getArraySize());

    // no swizzle support for now
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    VkImageViewUsageCreateInfo imageViewUsageCreateInfo = {};
    imageViewUsageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO;
    imageViewUsageCreateInfo.usage = getVkImageUsageFlags();

    viewInfo.pNext = &imageViewUsageCreateInfo;

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
    size_t channelCount = cl::GetChannelCount(getFormat().image_channel_order);

    switch (getFormat().image_channel_data_type)
    {
        case CL_UNORM_INT8:
        {
            float *srcVector = static_cast<float *>(const_cast<void *>(fillColor));
            if (getFormat().image_channel_order == CL_BGRA)
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
                {
                    packedColor->u8[i] =
                        static_cast<unsigned char>(NormalizeFloatValue(srcVector[i], 255.f));
                }
            }
            break;
        }
        case CL_SIGNED_INT8:
        {
            int *srcVector = static_cast<int *>(const_cast<void *>(fillColor));
            for (unsigned int i = 0; i < channelCount; i++)
            {
                packedColor->s8[i] = static_cast<char>(std::clamp(srcVector[i], -128, 127));
            }
            break;
        }
        case CL_UNSIGNED_INT8:
        {
            unsigned int *srcVector = static_cast<unsigned int *>(const_cast<void *>(fillColor));
            for (unsigned int i = 0; i < channelCount; i++)
            {
                packedColor->u8[i] = static_cast<unsigned char>(
                    std::clamp(static_cast<unsigned int>(srcVector[i]),
                               static_cast<unsigned int>(0), static_cast<unsigned int>(255)));
            }
            break;
        }
        case CL_UNORM_INT16:
        {
            float *srcVector = static_cast<float *>(const_cast<void *>(fillColor));
            for (unsigned int i = 0; i < channelCount; i++)
            {
                packedColor->u16[i] =
                    static_cast<unsigned short>(NormalizeFloatValue(srcVector[i], 65535.f));
            }
            break;
        }
        case CL_SIGNED_INT16:
        {
            int *srcVector = static_cast<int *>(const_cast<void *>(fillColor));
            for (unsigned int i = 0; i < channelCount; i++)
            {
                packedColor->s16[i] = static_cast<short>(std::clamp(srcVector[i], -32768, 32767));
            }
            break;
        }
        case CL_UNSIGNED_INT16:
        {
            unsigned int *srcVector = static_cast<unsigned int *>(const_cast<void *>(fillColor));
            for (unsigned int i = 0; i < channelCount; i++)
            {
                packedColor->u16[i] = static_cast<unsigned short>(
                    std::clamp(static_cast<unsigned int>(srcVector[i]),
                               static_cast<unsigned int>(0), static_cast<unsigned int>(65535)));
            }
            break;
        }
        case CL_HALF_FLOAT:
        {
            float *srcVector = static_cast<float *>(const_cast<void *>(fillColor));
            for (unsigned int i = 0; i < channelCount; i++)
            {
                packedColor->fp16[i] = cl_half_from_float(srcVector[i], CL_HALF_RTE);
            }
            break;
        }
        case CL_SIGNED_INT32:
        {
            int *srcVector = static_cast<int *>(const_cast<void *>(fillColor));
            for (unsigned int i = 0; i < channelCount; i++)
            {
                packedColor->s32[i] = static_cast<int>(srcVector[i]);
            }
            break;
        }
        case CL_UNSIGNED_INT32:
        {
            unsigned int *srcVector = static_cast<unsigned int *>(const_cast<void *>(fillColor));
            for (unsigned int i = 0; i < channelCount; i++)
            {
                packedColor->u32[i] = static_cast<unsigned int>(srcVector[i]);
            }
            break;
        }
        case CL_FLOAT:
        {
            float *srcVector = static_cast<float *>(const_cast<void *>(fillColor));
            for (unsigned int i = 0; i < channelCount; i++)
            {
                packedColor->fp32[i] = srcVector[i];
            }
            break;
        }
        default:
            UNIMPLEMENTED();
            break;
    }
}

angle::Result CLImageVk::fillImageWithColor(const cl::Offset &origin,
                                            const cl::Extents &region,
                                            PixelColor *packedColor)
{
    size_t elementSize = getElementSize();
    cl::BufferRect stagingBufferRect{
        {}, {mExtent.width, mExtent.height, mExtent.depth}, 0, 0, elementSize};

    uint8_t *imagePtr = nullptr;
    ANGLE_TRY(mapBufferHelper(imagePtr));
    cl::Defer deferUnmap([this]() { unmapBufferHelper(); });

    uint8_t *ptrBase = imagePtr + (origin.z * stagingBufferRect.getSlicePitch()) +
                       (origin.y * stagingBufferRect.getRowPitch()) + (origin.x * elementSize);

    for (size_t slice = 0; slice < region.depth; slice++)
    {
        for (size_t row = 0; row < region.height; row++)
        {
            size_t stagingBufferOffset = stagingBufferRect.getRowOffset(slice, row);
            uint8_t *pixelPtr          = ptrBase + stagingBufferOffset;
            for (size_t x = 0; x < region.width; x++)
            {
                memcpy(pixelPtr, packedColor, elementSize);
                pixelPtr += elementSize;
            }
        }
    }

    return angle::Result::Continue;
}

cl::Extents CLImageVk::getExtentForCopy(const cl::Extents &region)
{
    cl::Extents extent = {};
    extent.width       = region.width;
    extent.height      = region.height;
    extent.depth       = region.depth;
    switch (getDescriptor().type)
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

cl::Offset CLImageVk::getOffsetForCopy(const cl::Offset &origin)
{
    cl::Offset offset = {};
    offset.x          = origin.x;
    offset.y          = origin.y;
    offset.z          = origin.z;
    switch (getDescriptor().type)
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

VkImageSubresourceLayers CLImageVk::getSubresourceLayersForCopy(const cl::Offset &origin,
                                                                const cl::Extents &region,
                                                                cl::MemObjectType copyToType,
                                                                ImageCopyWith imageCopy)
{
    VkImageSubresourceLayers subresource = {};
    subresource.aspectMask               = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource.mipLevel                 = 0;
    switch (getDescriptor().type)
    {
        case cl::MemObjectType::Image1D_Array:
            subresource.baseArrayLayer = static_cast<uint32_t>(origin.y);
            if (imageCopy == ImageCopyWith::Image)
            {
                subresource.layerCount = static_cast<uint32_t>(region.height);
            }
            else
            {
                subresource.layerCount = static_cast<uint32_t>(getArraySize());
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
                subresource.layerCount = static_cast<uint32_t>(region.depth);
            }
            else
            {
                subresource.layerCount = static_cast<uint32_t>(getArraySize());
            }
            break;
        default:
            subresource.baseArrayLayer = 0;
            subresource.layerCount     = 1;
            break;
    }
    return subresource;
}

angle::Result CLImageVk::mapBufferHelper(uint8_t *&ptrOut)
{
    if (!isMapped())
    {
        if (mParent)
        {
            return mapParentBufferHelper(ptrOut);
        }

        CLBufferVk *stagingBuffer = nullptr;
        ANGLE_TRY(getOrCreateStagingBuffer(&stagingBuffer));
        ASSERT(stagingBuffer);
        ANGLE_TRY(stagingBuffer->mapBufferHelper(mMappedMemory));
    }
    ASSERT(mMappedMemory);
    ptrOut = mMappedMemory;

    return angle::Result::Continue;
}

angle::Result CLImageVk::mapParentBufferHelper(uint8_t *&ptrOut)
{
    ASSERT(mParent);

    if (cl::IsBufferType(getParentType()))
    {
        ANGLE_TRY(getParent<CLBufferVk>()->mapBufferHelper(ptrOut));
    }
    else if (cl::IsImageType(getParentType()))
    {
        ANGLE_TRY(getParent<CLImageVk>()->mapBufferHelper(ptrOut));
    }
    else
    {
        UNREACHABLE();
    }
    ptrOut += getOffset();
    return angle::Result::Continue;
}

void CLImageVk::unmapBufferHelper()
{
    if (mParent)
    {
        getParent<CLImageVk>()->unmapBufferHelper();
        return;
    }
    ASSERT(mStagingBuffer);
    mStagingBuffer->getImpl<CLBufferVk>().unmapBufferHelper();
}

size_t CLImageVk::getRowPitch() const
{
    return getFrontendObject().getRowSize();
}

size_t CLImageVk::getSlicePitch() const
{
    return getFrontendObject().getSliceSize();
}

cl::MemObjectType CLImageVk::getParentType() const
{
    if (mParent)
    {
        return mParent->getType();
    }
    return cl::MemObjectType::InvalidEnum;
}

angle::Result CLImageVk::getBufferView(const vk::BufferView **viewOut)
{
    if (!mBufferViews.isInitialized())
    {
        ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
    }

    CLBufferVk *parent = getParent<CLBufferVk>();

    return mBufferViews.getView(
        mContext, parent->getBuffer(), parent->getOffset(),
        mContext->getRenderer()->getFormat(CLImageFormatToAngleFormat(getFormat())), viewOut,
        nullptr);
}

}  // namespace rx
