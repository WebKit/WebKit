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
#include "libANGLE/renderer/vulkan/renderervk_utils.h"

namespace egl
{
class AttributeMap;
}

namespace rx
{
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

    vk::Error initialize(const egl::AttributeMap &attribs);

    std::string getVendorString() const;
    std::string getRendererDescription() const;

    VkInstance getInstance() const { return mInstance; }
    VkPhysicalDevice getPhysicalDevice() const { return mPhysicalDevice; }
    VkQueue getQueue() const { return mQueue; }
    VkDevice getDevice() const { return mDevice; }

    vk::ErrorOrResult<uint32_t> selectPresentQueueForSurface(VkSurfaceKHR surface);

    // TODO(jmadill): Use ContextImpl for command buffers to enable threaded contexts.
    vk::CommandBuffer *getCommandBuffer();
    vk::Error submitCommandBuffer(const vk::CommandBuffer &commandBuffer);
    vk::Error submitAndFinishCommandBuffer(const vk::CommandBuffer &commandBuffer);
    vk::Error submitCommandsWithSync(const vk::CommandBuffer &commandBuffer,
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
                                 vk::StagingImage *imageOut);

    GlslangWrapper *getGlslangWrapper();

    Serial getCurrentQueueSerial() const;

    template <typename T>
    void enqueueGarbage(Serial serial, T &&object)
    {
        mGarbage.emplace_back(std::unique_ptr<vk::GarbageObject<T>>(
            new vk::GarbageObject<T>(serial, std::move(object))));
    }

    template <typename T>
    void enqueueGarbageOrDeleteNow(const ResourceVk &resouce, T &&object)
    {
        if (resouce.getDeleteSchedule(mLastCompletedQueueSerial) == DeleteSchedule::NOW)
        {
            object.destroy(mDevice);
        }
        else
        {
            enqueueGarbage(resouce.getStoredQueueSerial(), std::move(object));
        }
    }

  private:
    void ensureCapsInitialized() const;
    void generateCaps(gl::Caps *outCaps,
                      gl::TextureCapsMap *outTextureCaps,
                      gl::Extensions *outExtensions,
                      gl::Limitations *outLimitations) const;
    vk::Error submit(const VkSubmitInfo &submitInfo);
    vk::Error checkInFlightCommands();

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
    vk::CommandBuffer mCommandBuffer;
    uint32_t mHostVisibleMemoryIndex;
    GlslangWrapper *mGlslangWrapper;
    Serial mCurrentQueueSerial;
    Serial mLastCompletedQueueSerial;
    std::vector<vk::FenceAndCommandBuffer> mInFlightCommands;
    std::vector<std::unique_ptr<vk::IGarbageObject>> mGarbage;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_RENDERERVK_H_
