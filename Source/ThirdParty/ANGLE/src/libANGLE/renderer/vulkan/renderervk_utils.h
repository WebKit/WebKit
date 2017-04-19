//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// renderervk_utils:
//    Helper functions for the Vulkan Renderer.
//

#ifndef LIBANGLE_RENDERER_VULKAN_RENDERERVK_UTILS_H_
#define LIBANGLE_RENDERER_VULKAN_RENDERERVK_UTILS_H_

#include <vulkan/vulkan.h>

#include "common/debug.h"
#include "common/Optional.h"
#include "libANGLE/Error.h"

namespace gl
{
struct Box;
struct Extents;
struct RasterizerState;
struct Rectangle;
}

namespace rx
{
const char *VulkanResultString(VkResult result);
bool HasStandardValidationLayer(const std::vector<VkLayerProperties> &layerProps);

extern const char *g_VkStdValidationLayerName;

enum class TextureDimension
{
    TEX_2D,
    TEX_CUBE,
    TEX_3D,
    TEX_2D_ARRAY,
};

enum DeleteSchedule
{
    NOW,
    LATER,
};

// A serial supports a few operations - comparison, increment, and assignment.
// TODO(jmadill): Verify it's not easy to overflow the queue serial.
class Serial final
{
  public:
    Serial() : mValue(0) {}
    Serial(const Serial &other) : mValue(other.mValue) {}
    Serial(Serial &&other) : mValue(other.mValue) { other.mValue = 0; }
    Serial &operator=(const Serial &other)
    {
        mValue = other.mValue;
        return *this;
    }
    bool operator>=(Serial other) const { return mValue >= other.mValue; }
    bool operator>(Serial other) const { return mValue > other.mValue; }

    // This function fails if we're at the limits of our counting.
    bool operator++()
    {
        if (mValue == std::numeric_limits<uint32_t>::max())
            return false;
        mValue++;
        return true;
    }

  private:
    uint32_t mValue;
};

// This is a small helper mixin for any GL object used in Vk command buffers. It records a serial
// at command submission times indicating it's order in the queue. We will use Fences to detect
// when commands are finished, and then handle lifetime management for the resources.
// Note that we use a queue order serial instead of a command buffer id serial since a queue can
// submit multiple command buffers in one API call.
class ResourceVk
{
  public:
    void setQueueSerial(Serial queueSerial)
    {
        ASSERT(queueSerial >= mStoredQueueSerial);
        mStoredQueueSerial = queueSerial;
    }

    DeleteSchedule getDeleteSchedule(Serial lastCompletedQueueSerial) const
    {
        if (lastCompletedQueueSerial >= mStoredQueueSerial)
        {
            return DeleteSchedule::NOW;
        }
        else
        {
            return DeleteSchedule::LATER;
        }
    }

    Serial getStoredQueueSerial() const { return mStoredQueueSerial; }

  private:
    Serial mStoredQueueSerial;
};

namespace vk
{
class DeviceMemory;
class Framebuffer;
class Image;
class Pipeline;
class RenderPass;

class Error final
{
  public:
    Error(VkResult result);
    Error(VkResult result, const char *file, unsigned int line);
    ~Error();

    Error(const Error &other);
    Error &operator=(const Error &other);

    gl::Error toGL(GLenum glErrorCode) const;
    egl::Error toEGL(EGLint eglErrorCode) const;

    operator gl::Error() const;
    operator egl::Error() const;
    template <typename T>
    operator gl::ErrorOrResult<T>() const
    {
        return static_cast<gl::Error>(*this);
    }

    bool isError() const;

    std::string toString() const;

  private:
    VkResult mResult;
    const char *mFile;
    unsigned int mLine;
};

template <typename ResultT>
using ErrorOrResult = angle::ErrorOrResultBase<Error, ResultT, VkResult, VK_SUCCESS>;

// Avoid conflicting with X headers which define "Success".
inline Error NoError()
{
    return Error(VK_SUCCESS);
}

template <typename DerivedT, typename HandleT>
class WrappedObject : angle::NonCopyable
{
  public:
    HandleT getHandle() const { return mHandle; }
    bool valid() const { return (mHandle != VK_NULL_HANDLE); }

    const HandleT *ptr() const { return &mHandle; }

  protected:
    WrappedObject() : mHandle(VK_NULL_HANDLE) {}
    WrappedObject(HandleT handle) : mHandle(handle) {}
    ~WrappedObject() { ASSERT(!valid()); }

    WrappedObject(WrappedObject &&other) : mHandle(other.mHandle)
    {
        other.mHandle = VK_NULL_HANDLE;
    }

    // Only works to initialize empty objects, since we don't have the device handle.
    WrappedObject &operator=(WrappedObject &&other)
    {
        ASSERT(!valid());
        std::swap(mHandle, other.mHandle);
        return *this;
    }

    void retain(VkDevice device, DerivedT &&other)
    {
        if (valid())
        {
            static_cast<DerivedT *>(this)->destroy(device);
        }
        std::swap(mHandle, other.mHandle);
    }

    HandleT mHandle;
};

class CommandPool final : public WrappedObject<CommandPool, VkCommandPool>
{
  public:
    CommandPool();

    void destroy(VkDevice device);

    Error init(VkDevice device, const VkCommandPoolCreateInfo &createInfo);
};

// Helper class that wraps a Vulkan command buffer.
class CommandBuffer final : public WrappedObject<CommandBuffer, VkCommandBuffer>
{
  public:
    CommandBuffer();

    void destroy(VkDevice device);
    using WrappedObject::operator=;

    void setCommandPool(CommandPool *commandPool);
    Error begin(VkDevice device);
    Error end();
    Error reset();

    void singleImageBarrier(VkPipelineStageFlags srcStageMask,
                            VkPipelineStageFlags dstStageMask,
                            VkDependencyFlags dependencyFlags,
                            const VkImageMemoryBarrier &imageMemoryBarrier);

    void clearSingleColorImage(const vk::Image &image, const VkClearColorValue &color);

    void copySingleImage(const vk::Image &srcImage,
                         const vk::Image &destImage,
                         const gl::Box &copyRegion,
                         VkImageAspectFlags aspectMask);

    void beginRenderPass(const RenderPass &renderPass,
                         const Framebuffer &framebuffer,
                         const gl::Rectangle &renderArea,
                         const std::vector<VkClearValue> &clearValues);
    void endRenderPass();

    void draw(uint32_t vertexCount,
              uint32_t instanceCount,
              uint32_t firstVertex,
              uint32_t firstInstance);

    void bindPipeline(VkPipelineBindPoint pipelineBindPoint, const vk::Pipeline &pipeline);
    void bindVertexBuffers(uint32_t firstBinding,
                           const std::vector<VkBuffer> &buffers,
                           const std::vector<VkDeviceSize> &offsets);

  private:
    CommandPool *mCommandPool;
};

class Image final : public WrappedObject<Image, VkImage>
{
  public:
    // Use this constructor if the lifetime of the image is not controlled by ANGLE. (SwapChain)
    Image();
    explicit Image(VkImage image);

    // Called on shutdown when the helper class *doesn't* own the handle to the image resource.
    void reset();

    // Called on shutdown when the helper class *does* own the handle to the image resource.
    void destroy(VkDevice device);

    void retain(VkDevice device, Image &&other);

    Error init(VkDevice device, const VkImageCreateInfo &createInfo);

    void changeLayoutTop(VkImageAspectFlags aspectMask,
                         VkImageLayout newLayout,
                         CommandBuffer *commandBuffer);

    void changeLayoutWithStages(VkImageAspectFlags aspectMask,
                                VkImageLayout newLayout,
                                VkPipelineStageFlags srcStageMask,
                                VkPipelineStageFlags dstStageMask,
                                CommandBuffer *commandBuffer);

    void getMemoryRequirements(VkDevice device, VkMemoryRequirements *requirementsOut) const;
    Error bindMemory(VkDevice device, const vk::DeviceMemory &deviceMemory);

    VkImageLayout getCurrentLayout() const { return mCurrentLayout; }
    void updateLayout(VkImageLayout layout) { mCurrentLayout = layout; }

  private:
    VkImageLayout mCurrentLayout;
};

class ImageView final : public WrappedObject<ImageView, VkImageView>
{
  public:
    ImageView();
    void destroy(VkDevice device);
    using WrappedObject::retain;

    Error init(VkDevice device, const VkImageViewCreateInfo &createInfo);
};

class Semaphore final : public WrappedObject<Semaphore, VkSemaphore>
{
  public:
    Semaphore();
    void destroy(VkDevice device);
    using WrappedObject::retain;

    Error init(VkDevice device);
};

class Framebuffer final : public WrappedObject<Framebuffer, VkFramebuffer>
{
  public:
    Framebuffer();
    void destroy(VkDevice device);
    using WrappedObject::retain;

    Error init(VkDevice device, const VkFramebufferCreateInfo &createInfo);
};

class DeviceMemory final : public WrappedObject<DeviceMemory, VkDeviceMemory>
{
  public:
    DeviceMemory();
    void destroy(VkDevice device);
    using WrappedObject::retain;

    Error allocate(VkDevice device, const VkMemoryAllocateInfo &allocInfo);
    Error map(VkDevice device,
              VkDeviceSize offset,
              VkDeviceSize size,
              VkMemoryMapFlags flags,
              uint8_t **mapPointer);
    void unmap(VkDevice device);
};

class RenderPass final : public WrappedObject<RenderPass, VkRenderPass>
{
  public:
    RenderPass();
    void destroy(VkDevice device);
    using WrappedObject::retain;

    Error init(VkDevice device, const VkRenderPassCreateInfo &createInfo);
};

class StagingImage final : angle::NonCopyable
{
  public:
    StagingImage();
    StagingImage(StagingImage &&other);
    void destroy(VkDevice device);
    void retain(VkDevice device, StagingImage &&other);

    vk::Error init(VkDevice device,
                   uint32_t queueFamilyIndex,
                   uint32_t hostVisibleMemoryIndex,
                   TextureDimension dimension,
                   VkFormat format,
                   const gl::Extents &extent);

    Image &getImage() { return mImage; }
    const Image &getImage() const { return mImage; }
    DeviceMemory &getDeviceMemory() { return mDeviceMemory; }
    const DeviceMemory &getDeviceMemory() const { return mDeviceMemory; }
    VkDeviceSize getSize() const { return mSize; }

  private:
    Image mImage;
    DeviceMemory mDeviceMemory;
    VkDeviceSize mSize;
};

class Buffer final : public WrappedObject<Buffer, VkBuffer>
{
  public:
    Buffer();
    void destroy(VkDevice device);
    void retain(VkDevice device, Buffer &&other);

    Error init(VkDevice device, const VkBufferCreateInfo &createInfo);
    Error bindMemory(VkDevice device);

    DeviceMemory &getMemory() { return mMemory; }
    const DeviceMemory &getMemory() const { return mMemory; }

  private:
    DeviceMemory mMemory;
};

class ShaderModule final : public WrappedObject<ShaderModule, VkShaderModule>
{
  public:
    ShaderModule();
    void destroy(VkDevice device);
    using WrappedObject::retain;

    Error init(VkDevice device, const VkShaderModuleCreateInfo &createInfo);
};

class Pipeline final : public WrappedObject<Pipeline, VkPipeline>
{
  public:
    Pipeline();
    void destroy(VkDevice device);
    using WrappedObject::retain;

    Error initGraphics(VkDevice device, const VkGraphicsPipelineCreateInfo &createInfo);
};

class PipelineLayout final : public WrappedObject<PipelineLayout, VkPipelineLayout>
{
  public:
    PipelineLayout();
    void destroy(VkDevice device);
    using WrappedObject::retain;

    Error init(VkDevice device, const VkPipelineLayoutCreateInfo &createInfo);
};

class Fence final : public WrappedObject<Fence, VkFence>
{
  public:
    Fence();
    void destroy(VkDevice fence);
    using WrappedObject::retain;
    using WrappedObject::operator=;

    Error init(VkDevice device, const VkFenceCreateInfo &createInfo);
    VkResult getStatus(VkDevice device) const;
};

class FenceAndCommandBuffer final : angle::NonCopyable
{
  public:
    FenceAndCommandBuffer(Serial queueSerial, Fence &&fence, CommandBuffer &&commandBuffer);
    FenceAndCommandBuffer(FenceAndCommandBuffer &&other);
    FenceAndCommandBuffer &operator=(FenceAndCommandBuffer &&other);

    void destroy(VkDevice device);
    vk::ErrorOrResult<bool> finished(VkDevice device) const;

    Serial queueSerial() const { return mQueueSerial; }

  private:
    Serial mQueueSerial;
    Fence mFence;
    CommandBuffer mCommandBuffer;
};

class IGarbageObject : angle::NonCopyable
{
  public:
    virtual bool destroyIfComplete(VkDevice device, Serial completedSerial) = 0;
};

template <typename T>
class GarbageObject final : public IGarbageObject
{
  public:
    GarbageObject(Serial serial, T &&object) : mSerial(serial), mObject(std::move(object)) {}

    bool destroyIfComplete(VkDevice device, Serial completedSerial) override
    {
        if (completedSerial >= mSerial)
        {
            mObject.destroy(device);
            return true;
        }

        return false;
    }

  private:
    Serial mSerial;
    T mObject;
};

}  // namespace vk

Optional<uint32_t> FindMemoryType(const VkPhysicalDeviceMemoryProperties &memoryProps,
                                  const VkMemoryRequirements &requirements,
                                  uint32_t propertyFlagMask);

namespace gl_vk
{
VkPrimitiveTopology GetPrimitiveTopology(GLenum mode);
VkCullModeFlags GetCullMode(const gl::RasterizerState &rasterState);
VkFrontFace GetFrontFace(GLenum frontFace);
}  // namespace gl_vk

}  // namespace rx

#define ANGLE_VK_TRY(command)                                          \
    {                                                                  \
        auto ANGLE_LOCAL_VAR = command;                                \
        if (ANGLE_LOCAL_VAR != VK_SUCCESS)                             \
        {                                                              \
            return rx::vk::Error(ANGLE_LOCAL_VAR, __FILE__, __LINE__); \
        }                                                              \
    }                                                                  \
    ANGLE_EMPTY_STATEMENT

#define ANGLE_VK_CHECK(test, error) ANGLE_VK_TRY(test ? VK_SUCCESS : error)

std::ostream &operator<<(std::ostream &stream, const rx::vk::Error &error);

#endif  // LIBANGLE_RENDERER_VULKAN_RENDERERVK_UTILS_H_
