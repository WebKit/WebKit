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

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include "common/PackedEnums.h"
#include "common/WorkerThread.h"
#include "common/angleutils.h"
#include "common/vulkan/vk_headers.h"
#include "common/vulkan/vulkan_icd.h"
#include "libANGLE/BlobCache.h"
#include "libANGLE/Caps.h"
#include "libANGLE/renderer/vulkan/CommandProcessor.h"
#include "libANGLE/renderer/vulkan/DebugAnnotatorVk.h"
#include "libANGLE/renderer/vulkan/MemoryTracking.h"
#include "libANGLE/renderer/vulkan/QueryVk.h"
#include "libANGLE/renderer/vulkan/ResourceVk.h"
#include "libANGLE/renderer/vulkan/UtilsVk.h"
#include "libANGLE/renderer/vulkan/vk_format_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"
#include "libANGLE/renderer/vulkan/vk_internal_shaders_autogen.h"
#include "libANGLE/renderer/vulkan/vk_mem_alloc_wrapper.h"

namespace angle
{
class Library;
struct FrontendFeatures;
}  // namespace angle

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
class Format;

static constexpr size_t kMaxExtensionNames = 400;
using ExtensionNameList                    = angle::FixedVector<const char *, kMaxExtensionNames>;

// Information used to accurately skip known synchronization issues in ANGLE.
struct SkippedSyncvalMessage
{
    const char *messageId;
    const char *messageContents1;
    const char *messageContents2                      = "";
    bool isDueToNonConformantCoherentFramebufferFetch = false;
};

class ImageMemorySuballocator : angle::NonCopyable
{
  public:
    ImageMemorySuballocator();
    ~ImageMemorySuballocator();

    void destroy(RendererVk *renderer);

    // Allocates memory for the image and binds it.
    VkResult allocateAndBindMemory(Context *context,
                                   Image *image,
                                   const VkImageCreateInfo *imageCreateInfo,
                                   VkMemoryPropertyFlags requiredFlags,
                                   VkMemoryPropertyFlags preferredFlags,
                                   MemoryAllocationType memoryAllocationType,
                                   Allocation *allocationOut,
                                   VkMemoryPropertyFlags *memoryFlagsOut,
                                   uint32_t *memoryTypeIndexOut,
                                   VkDeviceSize *sizeOut);

    // Maps the memory to initialize with non-zero value.
    VkResult mapMemoryAndInitWithNonZeroValue(RendererVk *renderer,
                                              Allocation *allocation,
                                              VkDeviceSize size,
                                              int value,
                                              VkMemoryPropertyFlags flags);
};
}  // namespace vk

// Supports one semaphore from current surface, and one semaphore passed to
// glSignalSemaphoreEXT.
using SignalSemaphoreVector = angle::FixedVector<VkSemaphore, 2>;

// Recursive function to process variable arguments for garbage collection
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

// Recursive function to process variable arguments for garbage destroy
inline void DestroyGarbage(VkDevice device) {}
template <typename ArgT, typename... ArgsT>
void DestroyGarbage(VkDevice device, ArgT object, ArgsT... objectsIn)
{
    if (object->valid())
    {
        object->destroy(device);
    }
    DestroyGarbage(device, objectsIn...);
}

class WaitableCompressEvent
{
  public:
    WaitableCompressEvent(std::shared_ptr<angle::WaitableEvent> waitableEvent)
        : mWaitableEvent(waitableEvent)
    {}

    virtual ~WaitableCompressEvent() {}

    void wait() { return mWaitableEvent->wait(); }

    bool isReady() { return mWaitableEvent->isReady(); }

  private:
    std::shared_ptr<angle::WaitableEvent> mWaitableEvent;
};

class OneOffCommandPool : angle::NonCopyable
{
  public:
    OneOffCommandPool();
    void init(vk::ProtectionType protectionType);
    angle::Result getCommandBuffer(vk::Context *context,
                                   vk::PrimaryCommandBuffer *commandBufferOut);
    void releaseCommandBuffer(const QueueSerial &submitQueueSerial,
                              vk::PrimaryCommandBuffer &&primary);
    void destroy(VkDevice device);

  private:
    vk::ProtectionType mProtectionType;
    std::mutex mMutex;
    vk::CommandPool mCommandPool;
    struct PendingOneOffCommands
    {
        vk::ResourceUse use;
        vk::PrimaryCommandBuffer commandBuffer;
    };
    std::deque<PendingOneOffCommands> mPendingCommands;
};

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
    bool hasSharedGarbage();

    std::string getVendorString() const;
    std::string getRendererDescription() const;
    std::string getVersionString(bool includeFullVersion) const;

    gl::Version getMaxSupportedESVersion() const;
    gl::Version getMaxConformantESVersion() const;

    uint32_t getDeviceVersion();
    VkInstance getInstance() const { return mInstance; }
    VkPhysicalDevice getPhysicalDevice() const { return mPhysicalDevice; }
    const VkPhysicalDeviceProperties &getPhysicalDeviceProperties() const
    {
        return mPhysicalDeviceProperties;
    }
    const VkPhysicalDeviceDrmPropertiesEXT &getPhysicalDeviceDrmProperties() const
    {
        return mDrmProperties;
    }
    const VkPhysicalDevicePrimitivesGeneratedQueryFeaturesEXT &
    getPhysicalDevicePrimitivesGeneratedQueryFeatures() const
    {
        return mPrimitivesGeneratedQueryFeatures;
    }
    const VkPhysicalDeviceFeatures &getPhysicalDeviceFeatures() const
    {
        return mPhysicalDeviceFeatures;
    }
    const VkPhysicalDeviceFeatures2KHR &getEnabledFeatures() const { return mEnabledFeatures; }
    VkDevice getDevice() const { return mDevice; }

    bool isVulkan11Instance() const;
    bool isVulkan11Device() const;

    const vk::Allocator &getAllocator() const { return mAllocator; }
    vk::ImageMemorySuballocator &getImageMemorySuballocator() { return mImageMemorySuballocator; }

    angle::Result selectPresentQueueForSurface(DisplayVk *displayVk,
                                               VkSurfaceKHR surface,
                                               uint32_t *presentQueueOut);

    const gl::Caps &getNativeCaps() const;
    const gl::TextureCapsMap &getNativeTextureCaps() const;
    const gl::Extensions &getNativeExtensions() const;
    const gl::Limitations &getNativeLimitations() const;
    const ShPixelLocalStorageOptions &getNativePixelLocalStorageOptions() const;
    void initializeFrontendFeatures(angle::FrontendFeatures *features) const;

    uint32_t getQueueFamilyIndex() const { return mCurrentQueueFamilyIndex; }
    const VkQueueFamilyProperties &getQueueFamilyProperties() const
    {
        return mQueueFamilyProperties[mCurrentQueueFamilyIndex];
    }

    const vk::MemoryProperties &getMemoryProperties() const { return mMemoryProperties; }

    const vk::Format &getFormat(GLenum internalFormat) const
    {
        return mFormatTable[internalFormat];
    }

    const vk::Format &getFormat(angle::FormatID formatID) const { return mFormatTable[formatID]; }

    angle::Result getPipelineCacheSize(DisplayVk *displayVk, size_t *pipelineCacheSizeOut);
    angle::Result syncPipelineCacheVk(DisplayVk *displayVk, const gl::Context *context);

    const angle::FeaturesVk &getFeatures() const { return mFeatures; }
    uint32_t getMaxVertexAttribDivisor() const { return mMaxVertexAttribDivisor; }
    VkDeviceSize getMaxVertexAttribStride() const { return mMaxVertexAttribStride; }

    uint32_t getDefaultUniformBufferSize() const { return mDefaultUniformBufferSize; }

    angle::vk::ICD getEnabledICD() const { return mEnabledICD; }
    bool isMockICDEnabled() const { return mEnabledICD == angle::vk::ICD::Mock; }

    // Query the format properties for select bits (linearTilingFeatures, optimalTilingFeatures
    // and bufferFeatures).  Looks through mandatory features first, and falls back to querying
    // the device (first time only).
    bool hasLinearImageFormatFeatureBits(angle::FormatID format,
                                         const VkFormatFeatureFlags featureBits) const;
    VkFormatFeatureFlags getLinearImageFormatFeatureBits(
        angle::FormatID format,
        const VkFormatFeatureFlags featureBits) const;
    VkFormatFeatureFlags getImageFormatFeatureBits(angle::FormatID format,
                                                   const VkFormatFeatureFlags featureBits) const;
    bool hasImageFormatFeatureBits(angle::FormatID format,
                                   const VkFormatFeatureFlags featureBits) const;
    bool hasBufferFormatFeatureBits(angle::FormatID format,
                                    const VkFormatFeatureFlags featureBits) const;

    bool isAsyncCommandQueueEnabled() const { return mFeatures.asyncCommandQueue.enabled; }
    bool isAsyncCommandBufferResetEnabled() const
    {
        return mFeatures.asyncCommandBufferReset.enabled;
    }

    ANGLE_INLINE egl::ContextPriority getDriverPriority(egl::ContextPriority priority)
    {
        return mCommandQueue.getDriverPriority(priority);
    }
    ANGLE_INLINE uint32_t getDeviceQueueIndex() { return mCommandQueue.getDeviceQueueIndex(); }

    VkQueue getQueue(egl::ContextPriority priority) { return mCommandQueue.getQueue(priority); }

    // This command buffer should be submitted immediately via queueSubmitOneOff.
    angle::Result getCommandBufferOneOff(vk::Context *context,
                                         vk::ProtectionType protectionType,
                                         vk::PrimaryCommandBuffer *commandBufferOut)
    {
        return mOneOffCommandPoolMap[protectionType].getCommandBuffer(context, commandBufferOut);
    }

    // Fire off a single command buffer immediately with default priority.
    // Command buffer must be allocated with getCommandBufferOneOff and is reclaimed.
    angle::Result queueSubmitOneOff(vk::Context *context,
                                    vk::PrimaryCommandBuffer &&primary,
                                    vk::ProtectionType protectionType,
                                    egl::ContextPriority priority,
                                    VkSemaphore waitSemaphore,
                                    VkPipelineStageFlags waitSemaphoreStageMasks,
                                    vk::SubmitPolicy submitPolicy,
                                    QueueSerial *queueSerialOut);

    angle::Result queueSubmitWaitSemaphore(vk::Context *context,
                                           egl::ContextPriority priority,
                                           const vk::Semaphore &waitSemaphore,
                                           VkPipelineStageFlags waitSemaphoreStageMasks,
                                           QueueSerial submitQueueSerial);

    template <typename... ArgsT>
    void collectGarbage(const vk::ResourceUse &use, ArgsT... garbageIn)
    {
        if (hasResourceUseFinished(use))
        {
            DestroyGarbage(mDevice, garbageIn...);
        }
        else
        {
            std::vector<vk::GarbageObject> sharedGarbage;
            CollectGarbage(&sharedGarbage, garbageIn...);
            if (!sharedGarbage.empty())
            {
                collectGarbage(use, std::move(sharedGarbage));
            }
        }
    }

    void collectAllocationGarbage(const vk::ResourceUse &use, vk::Allocation &allocationGarbageIn)
    {
        if (!allocationGarbageIn.valid())
        {
            return;
        }

        if (hasResourceUseFinished(use))
        {
            allocationGarbageIn.destroy(getAllocator());
        }
        else
        {
            std::vector<vk::GarbageObject> sharedGarbage;
            CollectGarbage(&sharedGarbage, &allocationGarbageIn);
            if (!sharedGarbage.empty())
            {
                collectGarbage(use, std::move(sharedGarbage));
            }
        }
    }

    void collectGarbage(const vk::ResourceUse &use, vk::GarbageList &&sharedGarbage)
    {
        ASSERT(!sharedGarbage.empty());
        vk::SharedGarbage garbage(use, std::move(sharedGarbage));
        if (!hasResourceUseSubmitted(use))
        {
            std::unique_lock<std::mutex> lock(mGarbageMutex);
            mPendingSubmissionGarbage.push(std::move(garbage));
        }
        else if (!garbage.destroyIfComplete(this))
        {
            std::unique_lock<std::mutex> lock(mGarbageMutex);
            mSharedGarbage.push(std::move(garbage));
        }
    }

    void collectSuballocationGarbage(const vk::ResourceUse &use,
                                     vk::BufferSuballocation &&suballocation,
                                     vk::Buffer &&buffer)
    {
        if (hasResourceUseFinished(use))
        {
            // mSuballocationGarbageDestroyed is atomic, so we dont need mGarbageMutex to
            // protect it.
            mSuballocationGarbageDestroyed += suballocation.getSize();
            buffer.destroy(mDevice);
            suballocation.destroy(this);
        }
        else
        {
            std::unique_lock<std::mutex> lock(mGarbageMutex);
            if (hasResourceUseSubmitted(use))
            {
                mSuballocationGarbageSizeInBytes += suballocation.getSize();
                mSuballocationGarbage.emplace(use, std::move(suballocation), std::move(buffer));
            }
            else
            {
                mPendingSubmissionSuballocationGarbage.emplace(use, std::move(suballocation),
                                                               std::move(buffer));
            }
        }
    }

    angle::Result getPipelineCache(vk::PipelineCacheAccess *pipelineCacheOut);
    angle::Result mergeIntoPipelineCache(const vk::PipelineCache &pipelineCache);

    void onNewValidationMessage(const std::string &message);
    std::string getAndClearLastValidationMessage(uint32_t *countSinceLastClear);

    const std::vector<const char *> &getSkippedValidationMessages() const
    {
        return mSkippedValidationMessages;
    }
    const std::vector<vk::SkippedSyncvalMessage> &getSkippedSyncvalMessages() const
    {
        return mSkippedSyncvalMessages;
    }

    void onFramebufferFetchUsed();
    bool isFramebufferFetchUsed() const { return mIsFramebufferFetchUsed; }

    uint64_t getMaxFenceWaitTimeNs() const;

    ANGLE_INLINE bool isCommandQueueBusy()
    {
        if (isAsyncCommandQueueEnabled())
        {
            return mCommandProcessor.isBusy(this);
        }
        else
        {
            return mCommandQueue.isBusy(this);
        }
    }

    angle::Result waitForResourceUseToBeSubmittedToDevice(vk::Context *context,
                                                          const vk::ResourceUse &use)
    {
        // This is only needed for async submission code path. For immediate submission, it is a nop
        // since everything is submitted immediately.
        if (isAsyncCommandQueueEnabled())
        {
            ASSERT(mCommandProcessor.hasResourceUseEnqueued(use));
            return mCommandProcessor.waitForResourceUseToBeSubmitted(context, use);
        }
        // This ResourceUse must have been submitted.
        ASSERT(mCommandQueue.hasResourceUseSubmitted(use));
        return angle::Result::Continue;
    }

    angle::Result waitForQueueSerialToBeSubmittedToDevice(vk::Context *context,
                                                          const QueueSerial &queueSerial)
    {
        // This is only needed for async submission code path. For immediate submission, it is a nop
        // since everything is submitted immediately.
        if (isAsyncCommandQueueEnabled())
        {
            ASSERT(mCommandProcessor.hasQueueSerialEnqueued(queueSerial));
            return mCommandProcessor.waitForQueueSerialToBeSubmitted(context, queueSerial);
        }
        // This queueSerial must have been submitted.
        ASSERT(mCommandQueue.hasQueueSerialSubmitted(queueSerial));
        return angle::Result::Continue;
    }

    angle::VulkanPerfCounters getCommandQueuePerfCounters()
    {
        return mCommandQueue.getPerfCounters();
    }
    void resetCommandQueuePerFrameCounters() { mCommandQueue.resetPerFramePerfCounters(); }

    egl::Display *getDisplay() const { return mDisplay; }

    bool enableDebugUtils() const { return mEnableDebugUtils; }
    bool angleDebuggerMode() const { return mAngleDebuggerMode; }

    SamplerCache &getSamplerCache() { return mSamplerCache; }
    SamplerYcbcrConversionCache &getYuvConversionCache() { return mYuvConversionCache; }

    void onAllocateHandle(vk::HandleType handleType);
    void onDeallocateHandle(vk::HandleType handleType);

    bool getEnableValidationLayers() const { return mEnableValidationLayers; }

    vk::ResourceSerialFactory &getResourceSerialFactory() { return mResourceSerialFactory; }

    void setGlobalDebugAnnotator();

    void outputVmaStatString();

    bool haveSameFormatFeatureBits(angle::FormatID formatID1, angle::FormatID formatID2) const;

    void cleanupGarbage();
    void cleanupPendingSubmissionGarbage();

    angle::Result submitCommands(vk::Context *context,
                                 vk::ProtectionType protectionType,
                                 egl::ContextPriority contextPriority,
                                 const vk::Semaphore *signalSemaphore,
                                 const vk::SharedExternalFence *externalFence,
                                 const QueueSerial &submitQueueSerial);

    angle::Result submitPriorityDependency(vk::Context *context,
                                           vk::ProtectionTypes protectionTypes,
                                           egl::ContextPriority srcContextPriority,
                                           egl::ContextPriority dstContextPriority,
                                           SerialIndex index);

    void handleDeviceLost();
    angle::Result finishResourceUse(vk::Context *context, const vk::ResourceUse &use);
    angle::Result finishQueueSerial(vk::Context *context, const QueueSerial &queueSerial);
    angle::Result waitForResourceUseToFinishWithUserTimeout(vk::Context *context,
                                                            const vk::ResourceUse &use,
                                                            uint64_t timeout,
                                                            VkResult *result);
    angle::Result checkCompletedCommands(vk::Context *context);
    angle::Result retireFinishedCommands(vk::Context *context);

    angle::Result flushWaitSemaphores(vk::ProtectionType protectionType,
                                      egl::ContextPriority priority,
                                      std::vector<VkSemaphore> &&waitSemaphores,
                                      std::vector<VkPipelineStageFlags> &&waitSemaphoreStageMasks);
    angle::Result flushRenderPassCommands(vk::Context *context,
                                          vk::ProtectionType protectionType,
                                          egl::ContextPriority priority,
                                          const vk::RenderPass &renderPass,
                                          vk::RenderPassCommandBufferHelper **renderPassCommands);
    angle::Result flushOutsideRPCommands(
        vk::Context *context,
        vk::ProtectionType protectionType,
        egl::ContextPriority priority,
        vk::OutsideRenderPassCommandBufferHelper **outsideRPCommands);

    void queuePresent(vk::Context *context,
                      egl::ContextPriority priority,
                      const VkPresentInfoKHR &presentInfo,
                      vk::SwapchainStatus *swapchainStatus);

    // Only useful if async submission is enabled
    angle::Result waitForPresentToBeSubmitted(vk::SwapchainStatus *swapchainStatus);

    angle::Result getOutsideRenderPassCommandBufferHelper(
        vk::Context *context,
        vk::SecondaryCommandPool *commandPool,
        vk::SecondaryCommandMemoryAllocator *commandsAllocator,
        vk::OutsideRenderPassCommandBufferHelper **commandBufferHelperOut);
    angle::Result getRenderPassCommandBufferHelper(
        vk::Context *context,
        vk::SecondaryCommandPool *commandPool,
        vk::SecondaryCommandMemoryAllocator *commandsAllocator,
        vk::RenderPassCommandBufferHelper **commandBufferHelperOut);

    void recycleOutsideRenderPassCommandBufferHelper(
        vk::OutsideRenderPassCommandBufferHelper **commandBuffer);
    void recycleRenderPassCommandBufferHelper(vk::RenderPassCommandBufferHelper **commandBuffer);

    // Process GPU memory reports
    void processMemoryReportCallback(const VkDeviceMemoryReportCallbackDataEXT &callbackData)
    {
        bool logCallback = getFeatures().logMemoryReportCallbacks.enabled;
        mMemoryReport.processCallback(callbackData, logCallback);
    }

    // Accumulate cache stats for a specific cache
    void accumulateCacheStats(VulkanCacheType cache, const CacheStats &stats)
    {
        std::unique_lock<std::mutex> localLock(mCacheStatsMutex);
        mVulkanCacheStats[cache].accumulate(stats);
    }
    // Log cache stats for all caches
    void logCacheStats() const;

    VkPipelineStageFlags getSupportedVulkanPipelineStageMask() const
    {
        return mSupportedVulkanPipelineStageMask;
    }

    VkShaderStageFlags getSupportedVulkanShaderStageMask() const
    {
        return mSupportedVulkanShaderStageMask;
    }

    angle::Result getFormatDescriptorCountForVkFormat(ContextVk *contextVk,
                                                      VkFormat format,
                                                      uint32_t *descriptorCountOut);

    angle::Result getFormatDescriptorCountForExternalFormat(ContextVk *contextVk,
                                                            uint64_t format,
                                                            uint32_t *descriptorCountOut);

    VkDeviceSize getMaxCopyBytesUsingCPUWhenPreservingBufferData() const
    {
        return mMaxCopyBytesUsingCPUWhenPreservingBufferData;
    }

    const vk::ExtensionNameList &getEnabledInstanceExtensions() const
    {
        return mEnabledInstanceExtensions;
    }

    const vk::ExtensionNameList &getEnabledDeviceExtensions() const
    {
        return mEnabledDeviceExtensions;
    }

    VkDeviceSize getPreferedBufferBlockSize(uint32_t memoryTypeIndex) const;

    size_t getDefaultBufferAlignment() const { return mDefaultBufferAlignment; }

    uint32_t getStagingBufferMemoryTypeIndex(vk::MemoryCoherency coherency) const
    {
        return coherency == vk::MemoryCoherency::Coherent
                   ? mCoherentStagingBufferMemoryTypeIndex
                   : mNonCoherentStagingBufferMemoryTypeIndex;
    }
    size_t getStagingBufferAlignment() const { return mStagingBufferAlignment; }

    uint32_t getVertexConversionBufferMemoryTypeIndex(vk::MemoryHostVisibility hostVisibility) const
    {
        return hostVisibility == vk::MemoryHostVisibility::Visible
                   ? mHostVisibleVertexConversionBufferMemoryTypeIndex
                   : mDeviceLocalVertexConversionBufferMemoryTypeIndex;
    }
    size_t getVertexConversionBufferAlignment() const { return mVertexConversionBufferAlignment; }

    uint32_t getDeviceLocalMemoryTypeIndex() const
    {
        return mDeviceLocalVertexConversionBufferMemoryTypeIndex;
    }

    void addBufferBlockToOrphanList(vk::BufferBlock *block);
    void pruneOrphanedBufferBlocks();

    bool isShadingRateSupported(gl::ShadingRate shadingRate) const
    {
        return mSupportedFragmentShadingRates.test(shadingRate);
    }

    VkDeviceSize getSuballocationDestroyedSize() const
    {
        return mSuballocationGarbageDestroyed.load(std::memory_order_consume);
    }
    void onBufferPoolPrune() { mSuballocationGarbageDestroyed = 0; }
    VkDeviceSize getSuballocationGarbageSize() const
    {
        return mSuballocationGarbageSizeInBytesCachedAtomic.load(std::memory_order_consume);
    }
    size_t getPendingSubmissionGarbageSize() const
    {
        std::unique_lock<std::mutex> lock(mGarbageMutex);
        return mPendingSubmissionGarbage.size();
    }

    ANGLE_INLINE VkFilter getPreferredFilterForYUV(VkFilter defaultFilter)
    {
        return getFeatures().preferLinearFilterForYUV.enabled ? VK_FILTER_LINEAR : defaultFilter;
    }

    // Convenience helpers to check for dynamic state ANGLE features which depend on the more
    // encompassing feature for support of the relevant extension.  When the extension-support
    // feature is disabled, the derived dynamic state is automatically disabled.
    ANGLE_INLINE bool useVertexInputBindingStrideDynamicState()
    {
        return getFeatures().supportsExtendedDynamicState.enabled &&
               getFeatures().useVertexInputBindingStrideDynamicState.enabled;
    }
    ANGLE_INLINE bool useCullModeDynamicState()
    {
        return getFeatures().supportsExtendedDynamicState.enabled &&
               getFeatures().useCullModeDynamicState.enabled;
    }
    ANGLE_INLINE bool useDepthCompareOpDynamicState()
    {
        return getFeatures().supportsExtendedDynamicState.enabled &&
               getFeatures().useDepthCompareOpDynamicState.enabled;
    }
    ANGLE_INLINE bool useDepthTestEnableDynamicState()
    {
        return getFeatures().supportsExtendedDynamicState.enabled &&
               getFeatures().useDepthTestEnableDynamicState.enabled;
    }
    ANGLE_INLINE bool useDepthWriteEnableDynamicState()
    {
        return getFeatures().supportsExtendedDynamicState.enabled &&
               getFeatures().useDepthWriteEnableDynamicState.enabled;
    }
    ANGLE_INLINE bool useFrontFaceDynamicState()
    {
        return getFeatures().supportsExtendedDynamicState.enabled &&
               getFeatures().useFrontFaceDynamicState.enabled;
    }
    ANGLE_INLINE bool useStencilOpDynamicState()
    {
        return getFeatures().supportsExtendedDynamicState.enabled &&
               getFeatures().useStencilOpDynamicState.enabled;
    }
    ANGLE_INLINE bool useStencilTestEnableDynamicState()
    {
        return getFeatures().supportsExtendedDynamicState.enabled &&
               getFeatures().useStencilTestEnableDynamicState.enabled;
    }
    ANGLE_INLINE bool usePrimitiveRestartEnableDynamicState()
    {
        return getFeatures().supportsExtendedDynamicState2.enabled &&
               getFeatures().usePrimitiveRestartEnableDynamicState.enabled;
    }
    ANGLE_INLINE bool useRasterizerDiscardEnableDynamicState()
    {
        return getFeatures().supportsExtendedDynamicState2.enabled &&
               getFeatures().useRasterizerDiscardEnableDynamicState.enabled;
    }
    ANGLE_INLINE bool useDepthBiasEnableDynamicState()
    {
        return getFeatures().supportsExtendedDynamicState2.enabled &&
               getFeatures().useDepthBiasEnableDynamicState.enabled;
    }
    ANGLE_INLINE bool useLogicOpDynamicState()
    {
        return getFeatures().supportsExtendedDynamicState2.enabled &&
               getFeatures().supportsLogicOpDynamicState.enabled;
    }

    angle::Result allocateScopedQueueSerialIndex(vk::ScopedQueueSerialIndex *indexOut);
    angle::Result allocateQueueSerialIndex(SerialIndex *serialIndexOut);
    size_t getLargestQueueSerialIndexEverAllocated() const
    {
        return mQueueSerialIndexAllocator.getLargestIndexEverAllocated();
    }
    void releaseQueueSerialIndex(SerialIndex index);
    Serial generateQueueSerial(SerialIndex index);
    void reserveQueueSerials(SerialIndex index,
                             size_t count,
                             RangedSerialFactory *rangedSerialFactory);

    // Return true if all serials in ResourceUse have been submitted.
    bool hasResourceUseSubmitted(const vk::ResourceUse &use) const;
    bool hasQueueSerialSubmitted(const QueueSerial &queueSerial) const;
    Serial getLastSubmittedSerial(SerialIndex index) const;
    // Return true if all serials in ResourceUse have been finished.
    bool hasResourceUseFinished(const vk::ResourceUse &use) const;
    bool hasQueueSerialFinished(const QueueSerial &queueSerial) const;

    // Memory statistics can be updated on allocation and deallocation.
    template <typename HandleT>
    void onMemoryAlloc(vk::MemoryAllocationType allocType,
                       VkDeviceSize size,
                       uint32_t memoryTypeIndex,
                       HandleT handle)
    {
        mMemoryAllocationTracker.onMemoryAllocImpl(allocType, size, memoryTypeIndex,
                                                   reinterpret_cast<void *>(handle));
    }

    template <typename HandleT>
    void onMemoryDealloc(vk::MemoryAllocationType allocType,
                         VkDeviceSize size,
                         uint32_t memoryTypeIndex,
                         HandleT handle)
    {
        mMemoryAllocationTracker.onMemoryDeallocImpl(allocType, size, memoryTypeIndex,
                                                     reinterpret_cast<void *>(handle));
    }

    MemoryAllocationTracker *getMemoryAllocationTracker() { return &mMemoryAllocationTracker; }

    void requestAsyncCommandsAndGarbageCleanup(vk::Context *context);

    // Try to finish a command batch from the queue and free garbage memory in the event of an OOM
    // error.
    angle::Result finishOneCommandBatchAndCleanup(vk::Context *context, bool *anyBatchCleaned);

    // Static function to get Vulkan object type name.
    static const char *GetVulkanObjectTypeName(VkObjectType type);

  private:
    angle::Result initializeDevice(DisplayVk *displayVk, uint32_t queueFamilyIndex);
    void ensureCapsInitialized() const;
    void initializeValidationMessageSuppressions();

    void queryDeviceExtensionFeatures(const vk::ExtensionNameList &deviceExtensionNames);
    void appendDeviceExtensionFeaturesNotPromoted(const vk::ExtensionNameList &deviceExtensionNames,
                                                  VkPhysicalDeviceFeatures2KHR *deviceFeatures,
                                                  VkPhysicalDeviceProperties2 *deviceProperties);
    void appendDeviceExtensionFeaturesPromotedTo11(
        const vk::ExtensionNameList &deviceExtensionNames,
        VkPhysicalDeviceFeatures2KHR *deviceFeatures,
        VkPhysicalDeviceProperties2 *deviceProperties);
    void appendDeviceExtensionFeaturesPromotedTo12(
        const vk::ExtensionNameList &deviceExtensionNames,
        VkPhysicalDeviceFeatures2KHR *deviceFeatures,
        VkPhysicalDeviceProperties2 *deviceProperties);
    void appendDeviceExtensionFeaturesPromotedTo13(
        const vk::ExtensionNameList &deviceExtensionNames,
        VkPhysicalDeviceFeatures2KHR *deviceFeatures,
        VkPhysicalDeviceProperties2 *deviceProperties);

    angle::Result enableInstanceExtensions(DisplayVk *displayVk,
                                           const VulkanLayerVector &enabledInstanceLayerNames,
                                           const char *wsiExtension,
                                           bool canLoadDebugUtils);
    angle::Result enableDeviceExtensions(DisplayVk *displayVk,
                                         const VulkanLayerVector &enabledDeviceLayerNames);

    void enableDeviceExtensionsNotPromoted(const vk::ExtensionNameList &deviceExtensionNames);
    void enableDeviceExtensionsPromotedTo11(const vk::ExtensionNameList &deviceExtensionNames);
    void enableDeviceExtensionsPromotedTo12(const vk::ExtensionNameList &deviceExtensionNames);
    void enableDeviceExtensionsPromotedTo13(const vk::ExtensionNameList &deviceExtensionNames);

    void initInstanceExtensionEntryPoints();
    void initDeviceExtensionEntryPoints();
    // Initialize extension entry points from core ones if needed
    void initializeInstanceExtensionEntryPointsFromCore() const;
    void initializeDeviceExtensionEntryPointsFromCore() const;

    void initFeatures(DisplayVk *display, const vk::ExtensionNameList &extensions);
    void appBasedFeatureOverrides(DisplayVk *display, const vk::ExtensionNameList &extensions);
    angle::Result initPipelineCache(DisplayVk *display,
                                    vk::PipelineCache *pipelineCache,
                                    bool *success);

    template <VkFormatFeatureFlags VkFormatProperties::*features>
    VkFormatFeatureFlags getFormatFeatureBits(angle::FormatID formatID,
                                              const VkFormatFeatureFlags featureBits) const;

    template <VkFormatFeatureFlags VkFormatProperties::*features>
    bool hasFormatFeatureBits(angle::FormatID formatID,
                              const VkFormatFeatureFlags featureBits) const;

    // Initialize VMA allocator and buffer suballocator related data.
    angle::Result initializeMemoryAllocator(DisplayVk *displayVk);

    // Query and cache supported fragment shading rates
    bool canSupportFragmentShadingRate(const vk::ExtensionNameList &deviceExtensionNames);
    // Prefer host visible device local via device local based on device type and heap size.
    bool canPreferDeviceLocalMemoryHostVisible(VkPhysicalDeviceType deviceType);

    template <typename CommandBufferHelperT, typename RecyclerT>
    angle::Result getCommandBufferImpl(vk::Context *context,
                                       vk::SecondaryCommandPool *commandPool,
                                       vk::SecondaryCommandMemoryAllocator *commandsAllocator,
                                       RecyclerT *recycler,
                                       CommandBufferHelperT **commandBufferHelperOut);

    egl::Display *mDisplay;

    void *mLibVulkanLibrary;

    mutable bool mCapsInitialized;
    mutable gl::Caps mNativeCaps;
    mutable gl::TextureCapsMap mNativeTextureCaps;
    mutable gl::Extensions mNativeExtensions;
    mutable gl::Limitations mNativeLimitations;
    mutable ShPixelLocalStorageOptions mNativePLSOptions;
    mutable angle::FeaturesVk mFeatures;

    // The instance and device versions.  The instance version is the one from the Vulkan loader,
    // while the device version comes from VkPhysicalDeviceProperties::apiVersion.  With instance
    // version 1.0, only device version 1.0 can be used.  If instance version is at least 1.1, any
    // device version (even higher than that) can be used.  Some extensions have been promoted to
    // Vulkan 1.1 or higher, but the version check must be done against the instance or device
    // version, depending on whether it's an instance or device extension.
    //
    // Note that mDeviceVersion is technically redundant with mPhysicalDeviceProperties.apiVersion,
    // but ANGLE may use a smaller version with problematic ICDs.
    uint32_t mInstanceVersion;
    uint32_t mDeviceVersion;

    VkInstance mInstance;
    bool mEnableValidationLayers;
    // True if ANGLE is enabling the VK_EXT_debug_utils extension.
    bool mEnableDebugUtils;
    // True if ANGLE should call the vkCmd*DebugUtilsLabelEXT functions in order to communicate
    // to debuggers (e.g. AGI) the OpenGL ES commands that the application uses.  This is
    // independent of mEnableDebugUtils, as an external graphics debugger can enable the
    // VK_EXT_debug_utils extension and cause this to be set true.
    bool mAngleDebuggerMode;
    angle::vk::ICD mEnabledICD;
    VkDebugUtilsMessengerEXT mDebugUtilsMessenger;
    VkPhysicalDevice mPhysicalDevice;

    VkPhysicalDeviceProperties mPhysicalDeviceProperties;
    VkPhysicalDeviceVulkan11Properties mPhysicalDevice11Properties;

    VkPhysicalDeviceFeatures mPhysicalDeviceFeatures;
    VkPhysicalDeviceVulkan11Features mPhysicalDevice11Features;

    VkPhysicalDeviceLineRasterizationFeaturesEXT mLineRasterizationFeatures;
    VkPhysicalDeviceProvokingVertexFeaturesEXT mProvokingVertexFeatures;
    VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT mVertexAttributeDivisorFeatures;
    VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT mVertexAttributeDivisorProperties;
    VkPhysicalDeviceTransformFeedbackFeaturesEXT mTransformFeedbackFeatures;
    VkPhysicalDeviceIndexTypeUint8FeaturesEXT mIndexTypeUint8Features;
    VkPhysicalDeviceSubgroupProperties mSubgroupProperties;
    VkPhysicalDeviceShaderSubgroupExtendedTypesFeaturesKHR mSubgroupExtendedTypesFeatures;
    VkPhysicalDeviceDeviceMemoryReportFeaturesEXT mMemoryReportFeatures;
    VkDeviceDeviceMemoryReportCreateInfoEXT mMemoryReportCallback;
    VkPhysicalDeviceShaderFloat16Int8FeaturesKHR mShaderFloat16Int8Features;
    VkPhysicalDeviceDepthStencilResolvePropertiesKHR mDepthStencilResolveProperties;
    VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesGOOGLEX
        mMultisampledRenderToSingleSampledFeaturesGOOGLEX;
    VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesEXT
        mMultisampledRenderToSingleSampledFeatures;
    VkPhysicalDeviceImage2DViewOf3DFeaturesEXT mImage2dViewOf3dFeatures;
    VkPhysicalDeviceMultiviewFeatures mMultiviewFeatures;
    VkPhysicalDeviceFeatures2KHR mEnabledFeatures;
    VkPhysicalDeviceMultiviewProperties mMultiviewProperties;
    VkPhysicalDeviceDriverPropertiesKHR mDriverProperties;
    VkPhysicalDeviceCustomBorderColorFeaturesEXT mCustomBorderColorFeatures;
    VkPhysicalDeviceProtectedMemoryFeatures mProtectedMemoryFeatures;
    VkPhysicalDeviceHostQueryResetFeaturesEXT mHostQueryResetFeatures;
    VkPhysicalDeviceDepthClampZeroOneFeaturesEXT mDepthClampZeroOneFeatures;
    VkPhysicalDeviceDepthClipEnableFeaturesEXT mDepthClipEnableFeatures;
    VkPhysicalDeviceDepthClipControlFeaturesEXT mDepthClipControlFeatures;
    VkPhysicalDevicePrimitivesGeneratedQueryFeaturesEXT mPrimitivesGeneratedQueryFeatures;
    VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT mPrimitiveTopologyListRestartFeatures;
    VkPhysicalDeviceSamplerYcbcrConversionFeatures mSamplerYcbcrConversionFeatures;
    VkPhysicalDevicePipelineCreationCacheControlFeaturesEXT mPipelineCreationCacheControlFeatures;
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT mExtendedDynamicStateFeatures;
    VkPhysicalDeviceExtendedDynamicState2FeaturesEXT mExtendedDynamicState2Features;
    VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT mGraphicsPipelineLibraryFeatures;
    VkPhysicalDeviceGraphicsPipelineLibraryPropertiesEXT mGraphicsPipelineLibraryProperties;
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR mFragmentShadingRateFeatures;
    VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT mFragmentShaderInterlockFeatures;
    VkPhysicalDeviceImagelessFramebufferFeaturesKHR mImagelessFramebufferFeatures;
    VkPhysicalDevicePipelineRobustnessFeaturesEXT mPipelineRobustnessFeatures;
    VkPhysicalDevicePipelineProtectedAccessFeaturesEXT mPipelineProtectedAccessFeatures;
    VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT
        mRasterizationOrderAttachmentAccessFeatures;
    VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT mSwapchainMaintenance1Features;
    VkPhysicalDeviceLegacyDitheringFeaturesEXT mDitheringFeatures;
    VkPhysicalDeviceDrmPropertiesEXT mDrmProperties;
    VkPhysicalDeviceTimelineSemaphoreFeaturesKHR mTimelineSemaphoreFeatures;

    angle::PackedEnumBitSet<gl::ShadingRate, uint8_t> mSupportedFragmentShadingRates;
    std::vector<VkQueueFamilyProperties> mQueueFamilyProperties;
    uint32_t mMaxVertexAttribDivisor;
    uint32_t mCurrentQueueFamilyIndex;
    VkDeviceSize mMaxVertexAttribStride;
    uint32_t mDefaultUniformBufferSize;
    VkDevice mDevice;
    VkDeviceSize mMaxCopyBytesUsingCPUWhenPreservingBufferData;

    bool mDeviceLost;

    // We group garbage into four categories: mSharedGarbage is the garbage that has already
    // submitted to vulkan, we expect them to finish in finite time. mPendingSubmissionGarbage
    // is the garbage that is still referenced in the recorded commands. suballocations have its
    // own dedicated garbage list for performance optimization since they tend to be the most
    // common garbage objects. All these four groups of garbage share the same mutex lock.
    mutable std::mutex mGarbageMutex;
    vk::SharedGarbageList mSharedGarbage;
    vk::SharedGarbageList mPendingSubmissionGarbage;
    vk::SharedBufferSuballocationGarbageList mSuballocationGarbage;
    vk::SharedBufferSuballocationGarbageList mPendingSubmissionSuballocationGarbage;
    // Total suballocation garbage size in bytes.
    VkDeviceSize mSuballocationGarbageSizeInBytes;

    // Total bytes of suballocation that been destroyed since last prune call. This can be
    // accessed without mGarbageMutex, thus needs to be atomic to avoid tsan complain.
    std::atomic<VkDeviceSize> mSuballocationGarbageDestroyed;
    // This is the cached value of mSuballocationGarbageSizeInBytes but is accessed with atomic
    // operation. This can be accessed from different threads without mGarbageMutex, so that
    // thread sanitizer won't complain.
    std::atomic<VkDeviceSize> mSuballocationGarbageSizeInBytesCachedAtomic;

    vk::FormatTable mFormatTable;
    // A cache of VkFormatProperties as queried from the device over time.
    mutable angle::FormatMap<VkFormatProperties> mFormatProperties;

    vk::Allocator mAllocator;

    // Used to allocate memory for images using VMA, utilizing suballocation.
    vk::ImageMemorySuballocator mImageMemorySuballocator;

    vk::MemoryProperties mMemoryProperties;
    VkDeviceSize mPreferredLargeHeapBlockSize;

    // The default alignment for BufferVk object
    size_t mDefaultBufferAlignment;
    // The cached memory type index for staging buffer that is host visible.
    uint32_t mCoherentStagingBufferMemoryTypeIndex;
    uint32_t mNonCoherentStagingBufferMemoryTypeIndex;
    size_t mStagingBufferAlignment;
    // For vertex conversion buffers
    uint32_t mHostVisibleVertexConversionBufferMemoryTypeIndex;
    uint32_t mDeviceLocalVertexConversionBufferMemoryTypeIndex;
    size_t mVertexConversionBufferAlignment;

    // Holds orphaned BufferBlocks when ShareGroup gets destroyed
    vk::BufferBlockPointerVector mOrphanedBufferBlocks;

    // All access to the pipeline cache is done through EGL objects so it is thread safe to not
    // use a lock.
    std::mutex mPipelineCacheMutex;
    vk::PipelineCache mPipelineCache;
    uint32_t mPipelineCacheVkUpdateTimeout;
    size_t mPipelineCacheSizeAtLastSync;
    bool mPipelineCacheInitialized;

    // Latest validation data for debug overlay.
    std::string mLastValidationMessage;
    uint32_t mValidationMessageCount;

    // Skipped validation messages.  The exact contents of the list depends on the availability
    // of certain extensions.
    std::vector<const char *> mSkippedValidationMessages;
    // Syncval skipped messages.  The exact contents of the list depends on the availability of
    // certain extensions.
    std::vector<vk::SkippedSyncvalMessage> mSkippedSyncvalMessages;

    // Whether framebuffer fetch has been used, for the purposes of more accurate syncval error
    // filtering.
    bool mIsFramebufferFetchUsed;

    // How close to VkPhysicalDeviceLimits::maxMemoryAllocationCount we allow ourselves to get
    static constexpr double kPercentMaxMemoryAllocationCount = 0.3;
    // How many objects to garbage collect before issuing a flush()
    uint32_t mGarbageCollectionFlushThreshold;

    // Only used for "one off" command buffers.
    angle::PackedEnumMap<vk::ProtectionType, OneOffCommandPool> mOneOffCommandPoolMap;

    // Synchronous Command Queue
    vk::CommandQueue mCommandQueue;

    // Async Command Queue
    vk::CommandProcessor mCommandProcessor;

    // Command buffer pool management.
    vk::CommandBufferRecycler<vk::OutsideRenderPassCommandBufferHelper>
        mOutsideRenderPassCommandBufferRecycler;
    vk::CommandBufferRecycler<vk::RenderPassCommandBufferHelper> mRenderPassCommandBufferRecycler;

    SamplerCache mSamplerCache;
    SamplerYcbcrConversionCache mYuvConversionCache;
    angle::HashMap<VkFormat, uint32_t> mVkFormatDescriptorCountMap;
    vk::ActiveHandleCounter mActiveHandleCounts;
    std::mutex mActiveHandleCountsMutex;

    // Tracks resource serials.
    vk::ResourceSerialFactory mResourceSerialFactory;

    // QueueSerial generator
    vk::QueueSerialIndexAllocator mQueueSerialIndexAllocator;
    std::array<AtomicSerialFactory, kMaxQueueSerialIndexCount> mQueueSerialFactory;

    // Application executable information
    VkApplicationInfo mApplicationInfo;
    // Process GPU memory reports
    vk::MemoryReport mMemoryReport;
    // Helpers for adding trace annotations
    DebugAnnotatorVk mAnnotator;

    // Stats about all Vulkan object caches
    VulkanCacheStats mVulkanCacheStats;
    mutable std::mutex mCacheStatsMutex;

    // A mask to filter out Vulkan pipeline stages that are not supported, applied in situations
    // where multiple stages are prespecified (for example with image layout transitions):
    //
    // - Excludes GEOMETRY if geometry shaders are not supported.
    // - Excludes TESSELLATION_CONTROL and TESSELLATION_EVALUATION if tessellation shaders are
    // not
    //   supported.
    //
    // Note that this mask can have bits set that don't correspond to valid stages, so it's
    // strictly only useful for masking out unsupported stages in an otherwise valid set of
    // stages.
    VkPipelineStageFlags mSupportedVulkanPipelineStageMask;
    VkShaderStageFlags mSupportedVulkanShaderStageMask;

    // Use thread pool to compress cache data.
    std::shared_ptr<rx::WaitableCompressEvent> mCompressEvent;

    vk::ExtensionNameList mEnabledInstanceExtensions;
    vk::ExtensionNameList mEnabledDeviceExtensions;

    // Memory tracker for allocations and deallocations.
    MemoryAllocationTracker mMemoryAllocationTracker;
};

ANGLE_INLINE Serial RendererVk::generateQueueSerial(SerialIndex index)
{
    return mQueueSerialFactory[index].generate();
}

ANGLE_INLINE void RendererVk::reserveQueueSerials(SerialIndex index,
                                                  size_t count,
                                                  RangedSerialFactory *rangedSerialFactory)
{
    mQueueSerialFactory[index].reserve(rangedSerialFactory, count);
}

ANGLE_INLINE bool RendererVk::hasResourceUseSubmitted(const vk::ResourceUse &use) const
{
    if (isAsyncCommandQueueEnabled())
    {
        return mCommandProcessor.hasResourceUseEnqueued(use);
    }
    else
    {
        return mCommandQueue.hasResourceUseSubmitted(use);
    }
}

ANGLE_INLINE bool RendererVk::hasQueueSerialSubmitted(const QueueSerial &queueSerial) const
{
    if (isAsyncCommandQueueEnabled())
    {
        return mCommandProcessor.hasQueueSerialEnqueued(queueSerial);
    }
    else
    {
        return mCommandQueue.hasQueueSerialSubmitted(queueSerial);
    }
}

ANGLE_INLINE Serial RendererVk::getLastSubmittedSerial(SerialIndex index) const
{
    if (isAsyncCommandQueueEnabled())
    {
        return mCommandProcessor.getLastEnqueuedSerial(index);
    }
    else
    {
        return mCommandQueue.getLastSubmittedSerial(index);
    }
}

ANGLE_INLINE bool RendererVk::hasResourceUseFinished(const vk::ResourceUse &use) const
{
    return mCommandQueue.hasResourceUseFinished(use);
}

ANGLE_INLINE bool RendererVk::hasQueueSerialFinished(const QueueSerial &queueSerial) const
{
    return mCommandQueue.hasQueueSerialFinished(queueSerial);
}

ANGLE_INLINE angle::Result RendererVk::waitForPresentToBeSubmitted(
    vk::SwapchainStatus *swapchainStatus)
{
    if (isAsyncCommandQueueEnabled())
    {
        return mCommandProcessor.waitForPresentToBeSubmitted(swapchainStatus);
    }
    ASSERT(!swapchainStatus->isPending);
    return angle::Result::Continue;
}

ANGLE_INLINE void RendererVk::requestAsyncCommandsAndGarbageCleanup(vk::Context *context)
{
    mCommandProcessor.requestCommandsAndGarbageCleanup();
}

ANGLE_INLINE angle::Result RendererVk::checkCompletedCommands(vk::Context *context)
{
    return mCommandQueue.checkAndCleanupCompletedCommands(context);
}

ANGLE_INLINE angle::Result RendererVk::retireFinishedCommands(vk::Context *context)
{
    return mCommandQueue.retireFinishedCommands(context);
}
}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_RENDERERVK_H_
