//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLMemoryVk.h: Defines the class interface for CLMemoryVk, implementing CLMemoryImpl.

#ifndef LIBANGLE_RENDERER_VULKAN_CLMEMORYVK_H_
#define LIBANGLE_RENDERER_VULKAN_CLMEMORYVK_H_

#include "libANGLE/renderer/vulkan/cl_types.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"
#include "libANGLE/renderer/vulkan/vk_wrapper.h"

#include "libANGLE/renderer/CLMemoryImpl.h"

#include "libANGLE/CLBuffer.h"
#include "libANGLE/CLImage.h"
#include "libANGLE/CLMemory.h"
#include "libANGLE/cl_types.h"

#include "vulkan/vulkan_core.h"

namespace rx
{

class CLImageVk;

class CLMemoryVk : public CLMemoryImpl
{
  public:
    ~CLMemoryVk() override;

    // TODO: http://anglebug.com/42267017
    angle::Result createSubBuffer(const cl::Buffer &buffer,
                                  cl::MemFlags flags,
                                  size_t size,
                                  CLMemoryImpl::Ptr *subBufferOut) override;

    // CL Memory object can have a separate user host pointer that is separate from device mapped
    // pointer. As such having two interfaces to get this
    //  - mapForUser() - returns a mapped pointer as expected by the spec.
    //  - mapBufferHelper() - always provides mapped VkMemory object
    angle::Result mapForUser(uint8_t *&ptrOut, size_t offset = 0);
    virtual angle::Result mapBufferHelper(uint8_t *&ptrOut) = 0;
    void unmap() { unmapBufferHelper(); }

    VkBufferUsageFlags getVkUsageFlags();
    VkMemoryPropertyFlags getVkMemPropertyFlags();
    virtual size_t getSize() const = 0;
    size_t getOffset() const { return mMemory.getOffset(); }
    cl::MemFlags getFlags() const { return mMemory.getFlags(); }
    cl::MemObjectType getType() const { return mMemory.getType(); }
    void *getHostPtr() const { return mMemory.getHostPtr(); }

    angle::Result copyTo(void *ptr, size_t offset, size_t size);
    angle::Result copyFrom(const void *ptr, size_t offset, size_t size);

    bool isWritable()
    {
        constexpr VkBufferUsageFlags kWritableUsage =
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        return (getVkUsageFlags() & kWritableUsage) != 0;
    }

    virtual bool isCurrentlyInUse() const = 0;
    bool isMapped() const { return mMappedMemory != nullptr; }

  protected:
    CLMemoryVk(const cl::Memory &memory);

    virtual angle::Result mapParentBufferHelper(uint8_t *&ptrOut) = 0;
    virtual void unmapBufferHelper()                              = 0;

    CLContextVk *mContext;
    vk::Renderer *mRenderer;
    vk::Allocation mAllocation;
    angle::SimpleMutex mMutex;
    uint8_t *mMappedMemory;
    uint32_t mMapCount;
    CLMemoryVk *mParent;
};

class CLBufferVk : public CLMemoryVk
{
  public:
    CLBufferVk(const cl::Buffer &buffer);
    ~CLBufferVk() override;

    vk::BufferHelper &getBuffer();
    CLBufferVk *getParent() { return static_cast<CLBufferVk *>(mParent); }
    const cl::Buffer &getFrontendObject() { return reinterpret_cast<const cl::Buffer &>(mMemory); }

    bool supportsZeroCopy() const;
    bool isHostPtrAligned() const;

    angle::Result create(void *hostPtr);

    angle::Result copyToWithPitch(void *hostPtr,
                                  size_t srcOffset,
                                  size_t size,
                                  size_t rowPitch,
                                  size_t slicePitch,
                                  cl::Extents region,
                                  const size_t elementSize);

    angle::Result fillWithPattern(const void *pattern,
                                  size_t patternSize,
                                  size_t offset,
                                  size_t size)
    {
        getBuffer().fillWithPattern(pattern, patternSize, getOffset() + offset, size);
        return angle::Result::Continue;
    }

    bool isSubBuffer() const { return mParent != nullptr; }

    angle::Result setRect(const void *data,
                          const cl::BufferRect &dataRect,
                          const cl::BufferRect &bufferRect);
    angle::Result getRect(const cl::BufferRect &bufferRect,
                          const cl::BufferRect &dataRect,
                          void *outData);

    bool isCurrentlyInUse() const override;
    size_t getSize() const override { return mMemory.getSize(); }

    // syncHost routines for handling any needed host side updates
    enum class SyncHostDirection
    {
        ToHost,
        FromHost
    };
    angle::Result syncHost(CLBufferVk::SyncHostDirection direction);
    angle::Result syncHost(CLBufferVk::SyncHostDirection direction, size_t offset, size_t size);
    angle::Result syncHost(CLBufferVk::SyncHostDirection direction, cl::BufferRect hostRect);

    angle::Result setImage(CLImageVk *image);
    bool hasImage2DChild() const { return mImage2DFromThisBuffer != nullptr; }
    CLImageVk *getImage() { return mImage2DFromThisBuffer; }

    angle::Result mapBufferHelper(uint8_t *&ptrOut) override;

  private:
    angle::Result mapParentBufferHelper(uint8_t *&ptrOut) override;
    void unmapBufferHelper() override;
    angle::Result createWithProperties();

    enum class UpdateRectOperation
    {
        Read,
        Write
    };
    angle::Result updateRect(UpdateRectOperation readWriteOp,
                             void *data,
                             const cl::BufferRect &dataRect,
                             const cl::BufferRect &bufferRect);

    vk::BufferHelper mBuffer;
    VkBufferCreateInfo mDefaultBufferCreateInfo;

    CLImageVk *mImage2DFromThisBuffer;

    // allows access to private buffer routines in the case where the image's parent memory is a
    // buffer type
    friend class CLImageVk;
};

class CLImageVk : public CLMemoryVk
{
  public:
    CLImageVk(const cl::Image &image);
    ~CLImageVk() override;

    // Create and update routines
    angle::Result create(void *hostPtr);
    angle::Result createFromBuffer();

    angle::Result fillImageWithColor(const cl::Offset &origin,
                                     const cl::Extents &region,
                                     cl::PixelColor packedColor);

    // Copy routines
    angle::Result copyStagingFrom(void *ptr, size_t offset, size_t size);
    angle::Result copyStagingTo(void *ptr, size_t offset, size_t size);
    angle::Result copyStagingToFromWithPitch(void *ptr,
                                             const cl::Extents &region,
                                             const size_t rowPitch,
                                             const size_t slicePitch,
                                             StagingBufferCopyDirection copyStagingTo);

    // Predicate routines
    bool isCurrentlyInUse() const override;
    bool isImage2DFromBuffer() const { return mIsImage2DFromBuffer; }
    bool containsHostMemExtension();

    // State query routines
    vk::ImageHelper &getImage()
    {
        ASSERT(mImage && mImage->valid());
        return *mImage;
    }
    const cl::Image &getFrontendObject() const
    {
        return reinterpret_cast<const cl::Image &>(mMemory);
    }
    cl_image_format getFormat() const { return getFrontendObject().getFormat(); }
    cl::ImageDescriptor getDescriptor() const { return getFrontendObject().getDescriptor(); }
    size_t getElementSize() const { return getFrontendObject().getElementSize(); }
    size_t getArraySize() const { return getFrontendObject().getArraySize(); }
    size_t getSize() const override { return mMemory.getSize(); }
    size_t getRowPitch() const { return getFrontendObject().getRowSize(); }
    size_t getSlicePitch() const { return getFrontendObject().getSliceSize(); }
    size_t getWidth() const { return getFrontendObject().getWidth(); }
    size_t getHeight() const { return getFrontendObject().getHeight(); }
    size_t getDepth() const { return getFrontendObject().getDepth(); }

    cl::MemObjectType getParentType() const;
    template <typename T>
    T *getParent() const;

    VkImageUsageFlags getVkImageUsageFlags() const;
    cl::Extents getImageExtent() const { return mExtent; }
    cl::Offset getOffsetForCopy(const cl::Offset &origin) const;
    cl::Extents getExtentForCopy(const cl::Extents &region) const;
    cl::Extents getExtentForCopy(const size_t size) const;
    VkImageSubresourceLayers getSubresourceLayersForCopy(const cl::Offset &origin,
                                                         const cl::Extents &region,
                                                         cl::MemObjectType copyToType,
                                                         ImageCopyWith imageCopy) const;
    cl::BufferRect getHostRectForCopy(const cl::Offset &origin,
                                      const cl::Extents &region,
                                      size_t hostRowPitch,
                                      size_t hostSlicePitch) const;

    vk::ImageView &getImageView() { return mImageView; }
    angle::Result getBufferView(const vk::BufferView **viewOut);
    angle::Result getOrCreateStagingBuffer(CLBufferVk **clBufferOut);
    angle::Result mapBufferHelper(uint8_t *&ptrOut) override;

  private:
    angle::Result initImageViewImpl();
    angle::Result mapParentBufferHelper(uint8_t *&ptrOut) override;
    void unmapBufferHelper() override;

    std::unique_ptr<vk::ImageHelper> mImage;
    cl::Extents mExtent;
    angle::FormatID mAngleFormat;

    cl::BufferPtr mStagingBuffer;
    vk::ImageView mImageView;
    VkImageViewType mImageViewType;
    bool mIsImage2DFromBuffer;

    // Images created from buffer create texel buffer views. BufferViewHelper contain the view
    // corresponding to the attached buffer.
    vk::BufferViewHelper mBufferViews;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CLMEMORYVK_H_
