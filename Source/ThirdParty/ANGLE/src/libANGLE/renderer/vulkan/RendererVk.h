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
#include <mutex>
#include "vk_ext_provoking_vertex.h"
#include "volk.h"

#include "common/PackedEnums.h"
#include "common/PoolAlloc.h"
#include "common/angleutils.h"
#include "libANGLE/BlobCache.h"
#include "libANGLE/Caps.h"
#include "libANGLE/renderer/vulkan/CommandGraph.h"
#include "libANGLE/renderer/vulkan/QueryVk.h"
#include "libANGLE/renderer/vulkan/UtilsVk.h"
#include "libANGLE/renderer/vulkan/vk_format_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"
#include "libANGLE/renderer/vulkan/vk_internal_shaders_autogen.h"

namespace egl
{
class Display;
class BlobCache;
}  // namespace egl

namespace rx
{
class DisplayVk;
class FramebufferVk;

namespace vk
{
struct Format;
}  // namespace vk

// Supports one semaphore from current surface, and one semaphore passed to
// glSignalSemaphoreEXT.
using SignalSemaphoreVector = angle::FixedVector<VkSemaphore, 2>;

inline void CollectGarbage(std::vector<vk::GarbageObject> *garbageOut) {}

template <typename ArgT, typename... ArgsT>
void CollectGarbage(std::vector<vk::GarbageObject> *garbageOut, ArgT object, ArgsT... objectsIn)
{
    if (object->valid())
    {
        garbageOut->emplace_back(vk::GarbageObject::Get(object));
    }
    CollectGarbage(garbageOut, objectsIn...);
}

class RendererVk : angle::NonCopyable
{
  public:
    RendererVk();
    ~RendererVk();

    angle::Result initialize(DisplayVk *displayVk,
                             egl::Display *display,
                             const char *wsiExtension,
                             const char *wsiLayer);
    // Reload volk vk* function ptrs if needed for an already initialized RendererVk
    void reloadVolkIfNeeded() const;
    void onDestroy(vk::Context *context);

    void notifyDeviceLost();
    bool isDeviceLost() const;

    std::string getVendorString() const;
    std::string getRendererDescription() const;

    gl::Version getMaxSupportedESVersion() const;
    gl::Version getMaxConformantESVersion() const;

    VkInstance getInstance() const { return mInstance; }
    VkPhysicalDevice getPhysicalDevice() const { return mPhysicalDevice; }
    const VkPhysicalDeviceProperties &getPhysicalDeviceProperties() const
    {
        return mPhysicalDeviceProperties;
    }
    const VkPhysicalDeviceSubgroupProperties &getPhysicalDeviceSubgroupProperties() const
    {
        return mPhysicalDeviceSubgroupProperties;
    }
    const VkPhysicalDeviceFeatures &getPhysicalDeviceFeatures() const
    {
        return mPhysicalDeviceFeatures;
    }
    VkDevice getDevice() const { return mDevice; }

    angle::Result selectPresentQueueForSurface(DisplayVk *displayVk,
                                               VkSurfaceKHR surface,
                                               uint32_t *presentQueueOut);

    const gl::Caps &getNativeCaps() const;
    const gl::TextureCapsMap &getNativeTextureCaps() const;
    const gl::Extensions &getNativeExtensions() const;
    const gl::Limitations &getNativeLimitations() const;

    uint32_t getQueueFamilyIndex() const { return mCurrentQueueFamilyIndex; }
    const VkQueueFamilyProperties &getQueueFamilyProperties() const
    {
        return mQueueFamilyProperties[mCurrentQueueFamilyIndex];
    }

    const vk::MemoryProperties &getMemoryProperties() const { return mMemoryProperties; }

    // TODO(jmadill): We could pass angle::FormatID here.
    const vk::Format &getFormat(GLenum internalFormat) const
    {
        return mFormatTable[internalFormat];
    }

    const vk::Format &getFormat(angle::FormatID formatID) const { return mFormatTable[formatID]; }

    // Queries the descriptor set layout cache. Creates the layout if not present.
    angle::Result getDescriptorSetLayout(
        vk::Context *context,
        const vk::DescriptorSetLayoutDesc &desc,
        vk::BindingPointer<vk::DescriptorSetLayout> *descriptorSetLayoutOut);

    // Queries the pipeline layout cache. Creates the layout if not present.
    angle::Result getPipelineLayout(vk::Context *context,
                                    const vk::PipelineLayoutDesc &desc,
                                    const vk::DescriptorSetLayoutPointerArray &descriptorSetLayouts,
                                    vk::BindingPointer<vk::PipelineLayout> *pipelineLayoutOut);

    angle::Result getPipelineCacheSize(DisplayVk *displayVk, size_t *pipelineCacheSizeOut);
    angle::Result syncPipelineCacheVk(DisplayVk *displayVk);

    // Issues a new serial for linked shader modules. Used in the pipeline cache.
    Serial issueShaderSerial();

    const angle::FeaturesVk &getFeatures() const
    {
        ASSERT(mFeaturesInitialized);
        return mFeatures;
    }
    uint32_t getMaxVertexAttribDivisor() const { return mMaxVertexAttribDivisor; }
    VkDeviceSize getMaxVertexAttribStride() const { return mMaxVertexAttribStride; }

    bool isMockICDEnabled() const { return mEnabledICD == vk::ICD::Mock; }

    // Query the format properties for select bits (linearTilingFeatures, optimalTilingFeatures and
    // bufferFeatures).  Looks through mandatory features first, and falls back to querying the
    // device (first time only).
    bool hasLinearImageFormatFeatureBits(VkFormat format, const VkFormatFeatureFlags featureBits);
    VkFormatFeatureFlags getImageFormatFeatureBits(VkFormat format,
                                                   const VkFormatFeatureFlags featureBits);
    bool hasImageFormatFeatureBits(VkFormat format, const VkFormatFeatureFlags featureBits);
    bool hasBufferFormatFeatureBits(VkFormat format, const VkFormatFeatureFlags featureBits);

    ANGLE_INLINE egl::ContextPriority getDriverPriority(egl::ContextPriority priority)
    {
        return mPriorities[priority];
    }

    angle::Result queueSubmit(vk::Context *context,
                              egl::ContextPriority priority,
                              const VkSubmitInfo &submitInfo,
                              const vk::Fence &fence,
                              Serial *serialOut);
    angle::Result queueWaitIdle(vk::Context *context, egl::ContextPriority priority);
    angle::Result deviceWaitIdle(vk::Context *context);
    VkResult queuePresent(egl::ContextPriority priority, const VkPresentInfoKHR &presentInfo);

    angle::Result newSharedFence(vk::Context *context, vk::Shared<vk::Fence> *sharedFenceOut);
    inline void resetSharedFence(vk::Shared<vk::Fence> *sharedFenceIn)
    {
        sharedFenceIn->resetAndRecycle(&mFenceRecycler);
    }

    template <typename... ArgsT>
    void collectGarbageAndReinit(vk::SharedResourceUse *use, ArgsT... garbageIn)
    {
        std::vector<vk::GarbageObject> sharedGarbage;
        CollectGarbage(&sharedGarbage, garbageIn...);
        if (!sharedGarbage.empty())
        {
            collectGarbage(std::move(*use), std::move(sharedGarbage));
        }
        else
        {
            // Force releasing "use" even if no garbage was created.
            use->release();
        }
        // Keep "use" valid.
        use->init();
    }

    void collectGarbage(vk::SharedResourceUse &&use, std::vector<vk::GarbageObject> &&sharedGarbage)
    {
        mSharedGarbage.emplace_back(std::move(use), std::move(sharedGarbage));
    }

    static constexpr size_t kMaxExtensionNames = 200;
    using ExtensionNameList = angle::FixedVector<const char *, kMaxExtensionNames>;

    angle::Result getPipelineCache(vk::PipelineCache **pipelineCache);
    void onNewGraphicsPipeline() { mPipelineCacheDirty = true; }

    void onNewValidationMessage(const std::string &message);
    std::string getAndClearLastValidationMessage(uint32_t *countSinceLastClear);

    uint64_t getMaxFenceWaitTimeNs() const;
    Serial getCurrentQueueSerial() const { return mCurrentQueueSerial; }
    Serial getLastSubmittedQueueSerial() const { return mLastSubmittedQueueSerial; }
    Serial getLastCompletedQueueSerial() const { return mLastCompletedQueueSerial; }

    void onCompletedSerial(Serial serial);

    bool shouldCleanupGarbage()
    {
        return (mSharedGarbage.size() > mGarbageCollectionFlushThreshold);
    }

  private:
    angle::Result initializeDevice(DisplayVk *displayVk, uint32_t queueFamilyIndex);
    void ensureCapsInitialized() const;

    void queryDeviceExtensionFeatures(const ExtensionNameList &deviceExtensionNames);

    void initFeatures(const ExtensionNameList &extensions);
    void initPipelineCacheVkKey();
    angle::Result initPipelineCache(DisplayVk *display,
                                    vk::PipelineCache *pipelineCache,
                                    bool *success);

    template <VkFormatFeatureFlags VkFormatProperties::*features>
    VkFormatFeatureFlags getFormatFeatureBits(VkFormat format,
                                              const VkFormatFeatureFlags featureBits);

    template <VkFormatFeatureFlags VkFormatProperties::*features>
    bool hasFormatFeatureBits(VkFormat format, const VkFormatFeatureFlags featureBits);

    angle::Result cleanupGarbage(vk::Context *context, bool block);

    egl::Display *mDisplay;

    mutable bool mCapsInitialized;
    mutable gl::Caps mNativeCaps;
    mutable gl::TextureCapsMap mNativeTextureCaps;
    mutable gl::Extensions mNativeExtensions;
    mutable gl::Limitations mNativeLimitations;
    mutable bool mFeaturesInitialized;
    mutable angle::FeaturesVk mFeatures;

    VkInstance mInstance;
    bool mEnableValidationLayers;
    vk::ICD mEnabledICD;
    VkDebugUtilsMessengerEXT mDebugUtilsMessenger;
    VkDebugReportCallbackEXT mDebugReportCallback;
    VkPhysicalDevice mPhysicalDevice;
    VkPhysicalDeviceProperties mPhysicalDeviceProperties;
    VkPhysicalDeviceFeatures mPhysicalDeviceFeatures;
    VkPhysicalDeviceLineRasterizationFeaturesEXT mLineRasterizationFeatures;
    VkPhysicalDeviceProvokingVertexFeaturesEXT mProvokingVertexFeatures;
    VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT mVertexAttributeDivisorFeatures;
    VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT mVertexAttributeDivisorProperties;
    VkPhysicalDeviceTransformFeedbackFeaturesEXT mTransformFeedbackFeatures;
    VkPhysicalDeviceSubgroupProperties mPhysicalDeviceSubgroupProperties;
    std::vector<VkQueueFamilyProperties> mQueueFamilyProperties;
    std::mutex mQueueMutex;
    angle::PackedEnumMap<egl::ContextPriority, VkQueue> mQueues;
    angle::PackedEnumMap<egl::ContextPriority, egl::ContextPriority> mPriorities;
    uint32_t mCurrentQueueFamilyIndex;
    uint32_t mMaxVertexAttribDivisor;
    VkDeviceSize mMaxVertexAttribStride;
    VkDevice mDevice;
    AtomicSerialFactory mQueueSerialFactory;
    AtomicSerialFactory mShaderSerialFactory;

    Serial mLastCompletedQueueSerial;
    Serial mLastSubmittedQueueSerial;
    Serial mCurrentQueueSerial;

    bool mDeviceLost;

    vk::Recycler<vk::Fence> mFenceRecycler;

    std::mutex mGarbageMutex;
    vk::SharedGarbageList mSharedGarbage;

    vk::MemoryProperties mMemoryProperties;
    vk::FormatTable mFormatTable;

    // All access to the pipeline cache is done through EGL objects so it is thread safe to not use
    // a lock.
    vk::PipelineCache mPipelineCache;
    egl::BlobCache::Key mPipelineCacheVkBlobKey;
    uint32_t mPipelineCacheVkUpdateTimeout;
    bool mPipelineCacheDirty;
    bool mPipelineCacheInitialized;

    // A cache of VkFormatProperties as queried from the device over time.
    std::array<VkFormatProperties, vk::kNumVkFormats> mFormatProperties;

    // ANGLE uses a PipelineLayout cache to store compatible pipeline layouts.
    std::mutex mPipelineLayoutCacheMutex;
    PipelineLayoutCache mPipelineLayoutCache;

    // DescriptorSetLayouts are also managed in a cache.
    std::mutex mDescriptorSetLayoutCacheMutex;
    DescriptorSetLayoutCache mDescriptorSetLayoutCache;

    // Latest validation data for debug overlay.
    std::string mLastValidationMessage;
    uint32_t mValidationMessageCount;

    // How close to VkPhysicalDeviceLimits::maxMemoryAllocationCount we allow ourselves to get
    static constexpr double kPercentMaxMemoryAllocationCount = 0.3;
    // How many objects to garbage collect before issuing a flush()
    uint32_t mGarbageCollectionFlushThreshold;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_RENDERERVK_H_
