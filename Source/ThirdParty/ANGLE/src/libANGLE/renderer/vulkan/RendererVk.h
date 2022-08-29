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
#include "common/PoolAlloc.h"
#include "common/angleutils.h"
#include "common/vulkan/vk_headers.h"
#include "common/vulkan/vulkan_icd.h"
#include "libANGLE/BlobCache.h"
#include "libANGLE/Caps.h"
#include "libANGLE/WorkerThread.h"
#include "libANGLE/renderer/vulkan/CommandProcessor.h"
#include "libANGLE/renderer/vulkan/DebugAnnotatorVk.h"
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

// Process GPU memory reports
class MemoryReport final : angle::NonCopyable
{
  public:
    MemoryReport();
    void processCallback(const VkDeviceMemoryReportCallbackDataEXT &callbackData, bool logCallback);
    void logMemoryReportStats() const;

  private:
    struct MemorySizes
    {
        VkDeviceSize allocatedMemory;
        VkDeviceSize allocatedMemoryMax;
        VkDeviceSize importedMemory;
        VkDeviceSize importedMemoryMax;
    };
    mutable std::mutex mMemoryReportMutex;
    VkDeviceSize mCurrentTotalAllocatedMemory;
    VkDeviceSize mMaxTotalAllocatedMemory;
    angle::HashMap<VkObjectType, MemorySizes> mSizesPerType;
    VkDeviceSize mCurrentTotalImportedMemory;
    VkDeviceSize mMaxTotalImportedMemory;
    angle::HashMap<uint64_t, int> mUniqueIDCounts;
};

// Information used to accurately skip known synchronization issues in ANGLE.
struct SkippedSyncvalMessage
{
    const char *messageId;
    const char *messageContents1;
    const char *messageContents2                      = "";
    bool isDueToNonConformantCoherentFramebufferFetch = false;
};
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

class WaitableCompressEvent
{
  public:
    WaitableCompressEvent(std::shared_ptr<angle::WaitableEvent> waitableEvent)
        : mWaitableEvent(waitableEvent)
    {}

    virtual ~WaitableCompressEvent() {}

    void wait() { return mWaitableEvent->wait(); }

    bool isReady() { return mWaitableEvent->isReady(); }

    virtual bool getResult() = 0;

  private:
    std::shared_ptr<angle::WaitableEvent> mWaitableEvent;
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
    void releaseSharedResources(vk::ResourceUseList *resourceList);

    std::string getVendorString() const;
    std::string getRendererDescription() const;
    std::string getVersionString(bool includeFullVersion) const;

    gl::Version getMaxSupportedESVersion() const;
    gl::Version getMaxConformantESVersion() const;

    uint32_t getApiVersion() const { return mApiVersion; }
    VkInstance getInstance() const { return mInstance; }
    VkPhysicalDevice getPhysicalDevice() const { return mPhysicalDevice; }
    const VkPhysicalDeviceProperties &getPhysicalDeviceProperties() const
    {
        return mPhysicalDeviceProperties;
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

    const vk::Allocator &getAllocator() const { return mAllocator; }

    angle::Result selectPresentQueueForSurface(DisplayVk *displayVk,
                                               VkSurfaceKHR surface,
                                               uint32_t *presentQueueOut);

    const gl::Caps &getNativeCaps() const;
    const gl::TextureCapsMap &getNativeTextureCaps() const;
    const gl::Extensions &getNativeExtensions() const;
    const gl::Limitations &getNativeLimitations() const;
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

    // Issues a new serial for linked shader modules. Used in the pipeline cache.
    Serial issueShaderSerial();

    const angle::FeaturesVk &getFeatures() const { return mFeatures; }
    uint32_t getMaxVertexAttribDivisor() const { return mMaxVertexAttribDivisor; }
    VkDeviceSize getMaxVertexAttribStride() const { return mMaxVertexAttribStride; }

    VkDeviceSize getMinImportedHostPointerAlignment() const
    {
        return mMinImportedHostPointerAlignment;
    }
    uint32_t getDefaultUniformBufferSize() const { return mDefaultUniformBufferSize; }

    angle::vk::ICD getEnabledICD() const { return mEnabledICD; }
    bool isMockICDEnabled() const { return mEnabledICD == angle::vk::ICD::Mock; }

    // Query the format properties for select bits (linearTilingFeatures, optimalTilingFeatures and
    // bufferFeatures).  Looks through mandatory features first, and falls back to querying the
    // device (first time only).
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

    ANGLE_INLINE egl::ContextPriority getDriverPriority(egl::ContextPriority priority)
    {
        if (isAsyncCommandQueueEnabled())
        {
            return mCommandProcessor.getDriverPriority(priority);
        }
        else
        {
            return mCommandQueue.getDriverPriority(priority);
        }
    }
    ANGLE_INLINE uint32_t getDeviceQueueIndex()
    {
        if (isAsyncCommandQueueEnabled())
        {
            return mCommandProcessor.getDeviceQueueIndex();
        }
        else
        {
            return mCommandQueue.getDeviceQueueIndex();
        }
    }

    VkQueue getQueue(egl::ContextPriority priority)
    {
        if (isAsyncCommandQueueEnabled())
        {
            return mCommandProcessor.getQueue(priority);
        }
        else
        {
            return mCommandQueue.getQueue(priority);
        }
    }

    // This command buffer should be submitted immediately via queueSubmitOneOff.
    angle::Result getCommandBufferOneOff(vk::Context *context,
                                         bool hasProtectedContent,
                                         vk::PrimaryCommandBuffer *commandBufferOut);

    void resetOutsideRenderPassCommandBuffer(vk::OutsideRenderPassCommandBuffer &&commandBuffer)
    {
        mOutsideRenderPassCommandBufferRecycler.resetCommandBuffer(std::move(commandBuffer));
    }

    void resetRenderPassCommandBuffer(vk::RenderPassCommandBuffer &&commandBuffer)
    {
        mRenderPassCommandBufferRecycler.resetCommandBuffer(std::move(commandBuffer));
    }

    // Fire off a single command buffer immediately with default priority.
    // Command buffer must be allocated with getCommandBufferOneOff and is reclaimed.
    angle::Result queueSubmitOneOff(vk::Context *context,
                                    vk::PrimaryCommandBuffer &&primary,
                                    bool hasProtectedContent,
                                    egl::ContextPriority priority,
                                    const vk::Semaphore *waitSemaphore,
                                    VkPipelineStageFlags waitSemaphoreStageMasks,
                                    const vk::Fence *fence,
                                    vk::SubmitPolicy submitPolicy,
                                    Serial *serialOut);

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
        if (!sharedGarbage.empty())
        {
            vk::SharedGarbage garbage(std::move(use), std::move(sharedGarbage));
            if (garbage.usedInRecordedCommands())
            {
                std::unique_lock<std::mutex> lock(mGarbageMutex);
                mPendingSubmissionGarbage.push(std::move(garbage));
            }
            else if (!garbage.destroyIfComplete(this, getLastCompletedQueueSerial()))
            {
                std::unique_lock<std::mutex> lock(mGarbageMutex);
                mSharedGarbage.push(std::move(garbage));
            }
        }
    }

    void collectSuballocationGarbage(vk::SharedResourceUse &use,
                                     vk::BufferSuballocation &&suballocation,
                                     vk::Buffer &&buffer)
    {
        if (use.isCurrentlyInUse(getLastCompletedQueueSerial()))
        {
            std::unique_lock<std::mutex> lock(mGarbageMutex);
            if (use.usedInRecordedCommands())
            {
                mPendingSubmissionSuballocationGarbage.emplace(
                    std::move(use), std::move(suballocation), std::move(buffer));
            }
            else
            {
                mSuballocationGarbageSizeInBytes += suballocation.getSize();
                mSuballocationGarbage.emplace(std::move(use), std::move(suballocation),
                                              std::move(buffer));
            }
            use.init();
        }
        else
        {
            // mSuballocationGarbageDestroyed is atomic, so we dont need mGarbageMutex to protect
            // it.
            mSuballocationGarbageDestroyed += suballocation.getSize();
            buffer.destroy(mDevice);
            suballocation.destroy(this);
        }
    }

    angle::Result getPipelineCache(PipelineCacheAccess *pipelineCacheOut);
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

    ANGLE_INLINE Serial getLastCompletedQueueSerial() const
    {
        if (isAsyncCommandQueueEnabled())
        {
            return mCommandProcessor.getLastCompletedQueueSerial();
        }
        else
        {
            return mCommandQueue.getLastCompletedQueueSerial();
        }
    }

    ANGLE_INLINE bool isCommandQueueBusy()
    {
        std::unique_lock<std::mutex> lock(mCommandQueueMutex);
        if (isAsyncCommandQueueEnabled())
        {
            return mCommandProcessor.isBusy();
        }
        else
        {
            return mCommandQueue.isBusy();
        }
    }

    angle::Result ensureNoPendingWork(vk::Context *context)
    {
        if (isAsyncCommandQueueEnabled())
        {
            return mCommandProcessor.ensureNoPendingWork(context);
        }
        else
        {
            return mCommandQueue.ensureNoPendingWork(context);
        }
    }

    angle::VulkanPerfCounters getCommandQueuePerfCounters()
    {
        std::unique_lock<std::mutex> lock(mCommandQueueMutex);
        if (isAsyncCommandQueueEnabled())
        {
            return mCommandProcessor.getPerfCounters();
        }
        else
        {
            return mCommandQueue.getPerfCounters();
        }
    }
    void resetCommandQueuePerFrameCounters()
    {
        std::unique_lock<std::mutex> lock(mCommandQueueMutex);
        if (isAsyncCommandQueueEnabled())
        {
            mCommandProcessor.resetPerFramePerfCounters();
        }
        else
        {
            mCommandQueue.resetPerFramePerfCounters();
        }
    }

    egl::Display *getDisplay() const { return mDisplay; }

    VkResult getLastPresentResult(VkSwapchainKHR swapchain)
    {
        return mCommandProcessor.getLastPresentResult(swapchain);
    }

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

    void cleanupGarbage(Serial lastCompletedQueueSerial);
    void cleanupCompletedCommandsGarbage();
    void cleanupPendingSubmissionGarbage();

    angle::Result submitFrame(vk::Context *context,
                              bool hasProtectedContent,
                              egl::ContextPriority contextPriority,
                              std::vector<VkSemaphore> &&waitSemaphores,
                              std::vector<VkPipelineStageFlags> &&waitSemaphoreStageMasks,
                              const vk::Semaphore *signalSemaphore,
                              vk::GarbageList &&currentGarbage,
                              vk::SecondaryCommandPools *commandPools,
                              Serial *submitSerialOut);

    void handleDeviceLost();
    angle::Result finishToSerial(vk::Context *context, Serial serial);
    angle::Result waitForSerialWithUserTimeout(vk::Context *context,
                                               Serial serial,
                                               uint64_t timeout,
                                               VkResult *result);
    angle::Result finish(vk::Context *context, bool hasProtectedContent);
    angle::Result checkCompletedCommands(vk::Context *context);

    angle::Result flushRenderPassCommands(vk::Context *context,
                                          bool hasProtectedContent,
                                          const vk::RenderPass &renderPass,
                                          vk::RenderPassCommandBufferHelper **renderPassCommands);
    angle::Result flushOutsideRPCommands(
        vk::Context *context,
        bool hasProtectedContent,
        vk::OutsideRenderPassCommandBufferHelper **outsideRPCommands);

    VkResult queuePresent(vk::Context *context,
                          egl::ContextPriority priority,
                          const VkPresentInfoKHR &presentInfo);

    angle::Result getOutsideRenderPassCommandBufferHelper(
        vk::Context *context,
        vk::CommandPool *commandPool,
        vk::OutsideRenderPassCommandBufferHelper **commandBufferHelperOut);
    angle::Result getRenderPassCommandBufferHelper(
        vk::Context *context,
        vk::CommandPool *commandPool,
        vk::RenderPassCommandBufferHelper **commandBufferHelperOut);

    void recycleOutsideRenderPassCommandBufferHelper(
        VkDevice device,
        vk::OutsideRenderPassCommandBufferHelper **commandBuffer);
    void recycleRenderPassCommandBufferHelper(VkDevice device,
                                              vk::RenderPassCommandBufferHelper **commandBuffer);

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

    ANGLE_INLINE VkFilter getPreferredFilterForYUV(VkFilter defaultFilter)
    {
        return getFeatures().preferLinearFilterForYUV.enabled ? VK_FILTER_LINEAR : defaultFilter;
    }

  private:
    angle::Result initializeDevice(DisplayVk *displayVk, uint32_t queueFamilyIndex);
    void ensureCapsInitialized() const;
    void initializeValidationMessageSuppressions();

    void queryDeviceExtensionFeatures(const vk::ExtensionNameList &deviceExtensionNames);

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

    template <typename CommandBufferHelperT, typename RecyclerT>
    angle::Result getCommandBufferImpl(vk::Context *context,
                                       vk::CommandPool *commandPool,
                                       RecyclerT *recycler,
                                       CommandBufferHelperT **commandBufferHelperOut);

    egl::Display *mDisplay;

    void *mLibVulkanLibrary;

    mutable bool mCapsInitialized;
    mutable gl::Caps mNativeCaps;
    mutable gl::TextureCapsMap mNativeTextureCaps;
    mutable gl::Extensions mNativeExtensions;
    mutable gl::Limitations mNativeLimitations;
    mutable angle::FeaturesVk mFeatures;

    uint32_t mApiVersion;
    VkInstance mInstance;
    bool mEnableValidationLayers;
    // True if ANGLE is enabling the VK_EXT_debug_utils extension.
    bool mEnableDebugUtils;
    // True if ANGLE should call the vkCmd*DebugUtilsLabelEXT functions in order to communicate to
    // debuggers (e.g. AGI) the OpenGL ES commands that the application uses.  This is independent
    // of mEnableDebugUtils, as an external graphics debugger can enable the VK_EXT_debug_utils
    // extension and cause this to be set true.
    bool mAngleDebuggerMode;
    angle::vk::ICD mEnabledICD;
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
    VkPhysicalDeviceIndexTypeUint8FeaturesEXT mIndexTypeUint8Features;
    VkPhysicalDeviceSubgroupProperties mSubgroupProperties;
    VkPhysicalDeviceShaderSubgroupExtendedTypesFeaturesKHR mSubgroupExtendedTypesFeatures;
    VkPhysicalDeviceDeviceMemoryReportFeaturesEXT mMemoryReportFeatures;
    VkDeviceDeviceMemoryReportCreateInfoEXT mMemoryReportCallback;
    VkPhysicalDeviceExternalMemoryHostPropertiesEXT mExternalMemoryHostProperties;
    VkPhysicalDeviceShaderFloat16Int8FeaturesKHR mShaderFloat16Int8Features;
    VkPhysicalDeviceDepthStencilResolvePropertiesKHR mDepthStencilResolveProperties;
    VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesGoogleX
        mMultisampledRenderToSingleSampledFeatures;
    VkPhysicalDeviceImage2DViewOf3DFeaturesEXT mImage2dViewOf3dFeatures;
    VkPhysicalDeviceMultiviewFeatures mMultiviewFeatures;
    VkPhysicalDeviceFeatures2KHR mEnabledFeatures;
    VkPhysicalDeviceMultiviewProperties mMultiviewProperties;
    VkPhysicalDeviceDriverPropertiesKHR mDriverProperties;
    VkPhysicalDeviceCustomBorderColorFeaturesEXT mCustomBorderColorFeatures;
    VkPhysicalDeviceProtectedMemoryFeatures mProtectedMemoryFeatures;
    VkPhysicalDeviceProtectedMemoryProperties mProtectedMemoryProperties;
    VkPhysicalDeviceHostQueryResetFeaturesEXT mHostQueryResetFeatures;
    VkPhysicalDeviceDepthClipControlFeaturesEXT mDepthClipControlFeatures;
    VkPhysicalDevicePrimitivesGeneratedQueryFeaturesEXT mPrimitivesGeneratedQueryFeatures;
    VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT mPrimitiveTopologyListRestartFeatures;
    VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT mBlendOperationAdvancedFeatures;
    VkPhysicalDeviceSamplerYcbcrConversionFeatures mSamplerYcbcrConversionFeatures;
    VkPhysicalDevicePipelineCreationCacheControlFeaturesEXT mPipelineCreationCacheControlFeatures;
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT mExtendedDynamicStateFeatures;
    VkPhysicalDeviceExtendedDynamicState2FeaturesEXT mExtendedDynamicState2Features;
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR mFragmentShadingRateFeatures;
    VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT mFragmentShaderInterlockFeatures;
    VkPhysicalDeviceImagelessFramebufferFeaturesKHR mImagelessFramebufferFeatures;
    angle::PackedEnumBitSet<gl::ShadingRate, uint8_t> mSupportedFragmentShadingRates;
    std::vector<VkQueueFamilyProperties> mQueueFamilyProperties;
    uint32_t mMaxVertexAttribDivisor;
    uint32_t mCurrentQueueFamilyIndex;
    VkDeviceSize mMaxVertexAttribStride;
    VkDeviceSize mMinImportedHostPointerAlignment;
    uint32_t mDefaultUniformBufferSize;
    VkDevice mDevice;
    AtomicSerialFactory mShaderSerialFactory;
    VkDeviceSize mMaxCopyBytesUsingCPUWhenPreservingBufferData;

    bool mDeviceLost;

    // We group garbage into four categories: mSharedGarbage is the garbage that has already
    // submitted to vulkan, we expect them to finish in finite time. mPendingSubmissionGarbage is
    // the garbage that is still referenced in the recorded commands. suballocations have its own
    // dedicated garbage list for performance optimization since they tend to be the most common
    // garbage objects. All these four groups of garbage share the same mutex lock.
    std::mutex mGarbageMutex;
    vk::SharedGarbageList mSharedGarbage;
    vk::SharedGarbageList mPendingSubmissionGarbage;
    vk::SharedBufferSuballocationGarbageList mSuballocationGarbage;
    vk::SharedBufferSuballocationGarbageList mPendingSubmissionSuballocationGarbage;
    // Total suballocation garbage size in bytes.
    VkDeviceSize mSuballocationGarbageSizeInBytes;

    // Total bytes of suballocation that been destroyed since last prune call. This can be accessed
    // without mGarbageMutex, thus needs to be atomic to avoid tsan complain.
    std::atomic<VkDeviceSize> mSuballocationGarbageDestroyed;
    // This is the cached value of mSuballocationGarbageSizeInBytes but is accessed with atomic
    // operation. This can be accessed from different threads without mGarbageMutex, so that thread
    // sanitizer won't complain.
    std::atomic<VkDeviceSize> mSuballocationGarbageSizeInBytesCachedAtomic;

    vk::FormatTable mFormatTable;
    // A cache of VkFormatProperties as queried from the device over time.
    mutable angle::FormatMap<VkFormatProperties> mFormatProperties;

    vk::Allocator mAllocator;
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

    // All access to the pipeline cache is done through EGL objects so it is thread safe to not use
    // a lock.
    std::mutex mPipelineCacheMutex;
    vk::PipelineCache mPipelineCache;
    uint32_t mPipelineCacheVkUpdateTimeout;
    size_t mPipelineCacheSizeAtLastSync;
    bool mPipelineCacheInitialized;

    // Latest validation data for debug overlay.
    std::string mLastValidationMessage;
    uint32_t mValidationMessageCount;

    // Skipped validation messages.  The exact contents of the list depends on the availability of
    // certain extensions.
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
    vk::CommandPool mOneOffCommandPool;

    struct PendingOneOffCommands
    {
        Serial serial;
        vk::PrimaryCommandBuffer commandBuffer;
    };
    std::deque<PendingOneOffCommands> mPendingOneOffCommands;

    // Synchronous Command Queue
    std::mutex mCommandQueueMutex;
    vk::CommandQueue mCommandQueue;

    // Async Command Queue
    vk::CommandProcessor mCommandProcessor;

    // Command buffer pool management.
    std::mutex mCommandBufferRecyclerMutex;
    vk::CommandBufferHandleAllocator mCommandBufferHandleAllocator;
    vk::CommandBufferRecycler<vk::OutsideRenderPassCommandBuffer,
                              vk::OutsideRenderPassCommandBufferHelper>
        mOutsideRenderPassCommandBufferRecycler;
    vk::CommandBufferRecycler<vk::RenderPassCommandBuffer, vk::RenderPassCommandBufferHelper>
        mRenderPassCommandBufferRecycler;

    SamplerCache mSamplerCache;
    SamplerYcbcrConversionCache mYuvConversionCache;
    angle::HashMap<VkFormat, uint32_t> mVkFormatDescriptorCountMap;
    vk::ActiveHandleCounter mActiveHandleCounts;
    std::mutex mActiveHandleCountsMutex;

    // Tracks resource serials.
    vk::ResourceSerialFactory mResourceSerialFactory;

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
    // - Excludes TESSELLATION_CONTROL and TESSELLATION_EVALUATION if tessellation shaders are not
    //   supported.
    //
    // Note that this mask can have bits set that don't correspond to valid stages, so it's strictly
    // only useful for masking out unsupported stages in an otherwise valid set of stages.
    VkPipelineStageFlags mSupportedVulkanPipelineStageMask;
    VkShaderStageFlags mSupportedVulkanShaderStageMask;

    // Use thread pool to compress cache data.
    std::shared_ptr<rx::WaitableCompressEvent> mCompressEvent;

    vk::ExtensionNameList mEnabledInstanceExtensions;
    vk::ExtensionNameList mEnabledDeviceExtensions;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_RENDERERVK_H_
