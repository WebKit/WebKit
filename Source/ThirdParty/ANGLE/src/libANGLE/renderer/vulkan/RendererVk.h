//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RendererVk.h:
//    Defines the class interface for RendererVk.
//

#ifndef LIBANGLE_RENDERER_VULKAN_RENDERERVK_H_
#define LIBANGLE_RENDERER_VULKAN_RENDERERVK_H_

#include <memory>
#include <vulkan/vulkan.h>

#include "common/angleutils.h"
#include "libANGLE/Caps.h"
#include "libANGLE/renderer/vulkan/formatutilsvk.h"
#include "libANGLE/renderer/vulkan/renderervk_utils.h"

namespace egl
{
class AttributeMap;
}

namespace rx
{
class FramebufferVk;
class GlslangWrapper;

namespace vk
{
struct Format;
}

class RendererVk : angle::NonCopyable
{
  public:
    RendererVk();
    ~RendererVk();

    vk::Error initialize(const egl::AttributeMap &attribs, const char *wsiName);

    std::string getVendorString() const;
    std::string getRendererDescription() const;

    VkInstance getInstance() const { return mInstance; }
    VkPhysicalDevice getPhysicalDevice() const { return mPhysicalDevice; }
    VkQueue getQueue() const { return mQueue; }
    VkDevice getDevice() const { return mDevice; }

    vk::ErrorOrResult<uint32_t> selectPresentQueueForSurface(VkSurfaceKHR surface);

    // TODO(jmadill): Use ContextImpl for command buffers to enable threaded contexts.
    vk::Error getStartedCommandBuffer(vk::CommandBufferAndState **commandBufferOut);
    vk::Error submitCommandBuffer(vk::CommandBufferAndState *commandBuffer);
    vk::Error submitAndFinishCommandBuffer(vk::CommandBufferAndState *commandBuffer);
    vk::Error submitCommandsWithSync(vk::CommandBufferAndState *commandBuffer,
                                     const vk::Semaphore &waitSemaphore,
                                     const vk::Semaphore &signalSemaphore);
    vk::Error finish();

    const gl::Caps &getNativeCaps() const;
    const gl::TextureCapsMap &getNativeTextureCaps() const;
    const gl::Extensions &getNativeExtensions() const;
    const gl::Limitations &getNativeLimitations() const;

    vk::Error createStagingImage(TextureDimension dimension,
                                 const vk::Format &format,
                                 const gl::Extents &extent,
                                 vk::StagingUsage usage,
                                 vk::StagingImage *imageOut);

    GlslangWrapper *getGlslangWrapper();

    Serial getCurrentQueueSerial() const;

    bool isResourceInUse(const ResourceVk &resource);
    bool isSerialInUse(Serial serial);

    template <typename T>
    void releaseResource(const ResourceVk &resource, T *object)
    {
        Serial resourceSerial = resource.getQueueSerial();
        releaseObject(resourceSerial, object);
    }

    template <typename T>
    void releaseObject(Serial resourceSerial, T *object)
    {
        if (!isSerialInUse(resourceSerial))
        {
            object->destroy(mDevice);
        }
        else
        {
            object->dumpResources(resourceSerial, &mGarbage);
        }
    }

    uint32_t getQueueFamilyIndex() const { return mCurrentQueueFamilyIndex; }

    const vk::MemoryProperties &getMemoryProperties() const { return mMemoryProperties; }

    // TODO(jmadill): Don't keep a single renderpass in the Renderer.
    gl::Error ensureInRenderPass(const gl::Context *context, FramebufferVk *framebufferVk);
    void endRenderPass();

    // This is necessary to update the cached current RenderPass Framebuffer.
    void onReleaseRenderPass(const FramebufferVk *framebufferVk);

    // TODO(jmadill): We could pass angle::Format::ID here.
    const vk::Format &getFormat(GLenum internalFormat) const
    {
        return mFormatTable[internalFormat];
    }

  private:
    void ensureCapsInitialized() const;
    void generateCaps(gl::Caps *outCaps,
                      gl::TextureCapsMap *outTextureCaps,
                      gl::Extensions *outExtensions,
                      gl::Limitations *outLimitations) const;
    vk::Error submit(const VkSubmitInfo &submitInfo);
    vk::Error submitFrame(const VkSubmitInfo &submitInfo);
    vk::Error checkInFlightCommands();
    void freeAllInFlightResources();

    mutable bool mCapsInitialized;
    mutable gl::Caps mNativeCaps;
    mutable gl::TextureCapsMap mNativeTextureCaps;
    mutable gl::Extensions mNativeExtensions;
    mutable gl::Limitations mNativeLimitations;

    vk::Error initializeDevice(uint32_t queueFamilyIndex);

    VkInstance mInstance;
    bool mEnableValidationLayers;
    VkDebugReportCallbackEXT mDebugReportCallback;
    VkPhysicalDevice mPhysicalDevice;
    VkPhysicalDeviceProperties mPhysicalDeviceProperties;
    std::vector<VkQueueFamilyProperties> mQueueFamilyProperties;
    VkQueue mQueue;
    uint32_t mCurrentQueueFamilyIndex;
    VkDevice mDevice;
    vk::CommandPool mCommandPool;
    vk::CommandBufferAndState mCommandBuffer;
    GlslangWrapper *mGlslangWrapper;
    SerialFactory mQueueSerialFactory;
    Serial mLastCompletedQueueSerial;
    Serial mCurrentQueueSerial;
    std::vector<vk::CommandBufferAndSerial> mInFlightCommands;
    std::vector<vk::FenceAndSerial> mInFlightFences;
    std::vector<vk::GarbageObject> mGarbage;
    vk::MemoryProperties mMemoryProperties;
    vk::FormatTable mFormatTable;

    // TODO(jmadill): Don't keep a single renderpass in the Renderer.
    FramebufferVk *mCurrentRenderPassFramebuffer;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_RENDERERVK_H_
