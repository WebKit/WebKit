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

#include <vulkan/vulkan.h>
#include <memory>

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
    void onDestroy(vk::Context *context);

    void notifyDeviceLost();
    bool isDeviceLost() const;

    std::string getVendorString() const;
    std::string getRendererDescription() const;

    gl::Version getMaxSupportedESVersion() const;

    VkInstance getInstance() const { return mInstance; }
    VkPhysicalDevice getPhysicalDevice() const { return mPhysicalDevice; }
    const VkPhysicalDeviceProperties &getPhysicalDeviceProperties() const
    {
        return mPhysicalDeviceProperties;
    }
    const VkPhysicalDeviceFeatures &getPhysicalDeviceFeatures() const
    {
        return mPhysicalDeviceFeatures;
    }
    VkQueue getQueue() const { return mQueue; }
    VkDevice getDevice() const { return mDevice; }

    angle::Result selectPresentQueueForSurface(DisplayVk *displayVk,
                                               VkSurfaceKHR surface,
                                               uint32_t *presentQueueOut);

    angle::Result finish(vk::Context *context,
                         const vk::Semaphore *waitSemaphore,
                         const vk::Semaphore *signalSemaphore);
    angle::Result flush(vk::Context *context,
                        const vk::Semaphore *waitSemaphore,
                        const vk::Semaphore *signalSemaphore);

    const vk::CommandPool &getCommandPool() const;

    const gl::Caps &getNativeCaps() const;
    const gl::TextureCapsMap &getNativeTextureCaps() const;
    const gl::Extensions &getNativeExtensions() const;
    const gl::Limitations &getNativeLimitations() const;
    uint32_t getMaxActiveTextures();

    Serial getCurrentQueueSerial() const { return mCurrentQueueSerial; }
    Serial getLastSubmittedQueueSerial() const { return mLastSubmittedQueueSerial; }
    Serial getLastCompletedQueueSerial() const { return mLastCompletedQueueSerial; }

    bool isSerialInUse(Serial serial) const;

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

    // Check to see which batches have finished completion (forward progress for
    // mLastCompletedQueueSerial, for example for when the application busy waits on a query
    // result).
    angle::Result checkCompletedCommands(vk::Context *context);

    // Wait for completion of batches until (at least) batch with given serial is finished.
    angle::Result finishToSerial(vk::Context *context, Serial serial);

    uint32_t getQueueFamilyIndex() const { return mCurrentQueueFamilyIndex; }

    const vk::MemoryProperties &getMemoryProperties() const { return mMemoryProperties; }

    // TODO(jmadill): We could pass angle::FormatID here.
    const vk::Format &getFormat(GLenum internalFormat) const
    {
        return mFormatTable[internalFormat];
    }

    const vk::Format &getFormat(angle::FormatID formatID) const { return mFormatTable[formatID]; }

    angle::Result getCompatibleRenderPass(vk::Context *context,
                                          const vk::RenderPassDesc &desc,
                                          vk::RenderPass **renderPassOut);
    angle::Result getRenderPassWithOps(vk::Context *context,
                                       const vk::RenderPassDesc &desc,
                                       const vk::AttachmentOpsArray &ops,
                                       vk::RenderPass **renderPassOut);

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

    angle::Result syncPipelineCacheVk(DisplayVk *displayVk);

    // Request a semaphore, that is expected to be signaled externally.  The next submission will
    // wait on it.
    angle::Result allocateSubmitWaitSemaphore(vk::Context *context,
                                              const vk::Semaphore **outSemaphore);
    // Get the last signaled semaphore to wait on externally.  The semaphore will not be waited on
    // by next submission.
    const vk::Semaphore *getSubmitLastSignaledSemaphore(vk::Context *context);

    // Get (or allocate) the fence that will be signaled on next submission.
    angle::Result getSubmitFence(vk::Context *context, vk::Shared<vk::Fence> *sharedFenceOut);

    // This should only be called from ResourceVk.
    // TODO(jmadill): Keep in ContextVk to enable threaded rendering.
    vk::CommandGraph *getCommandGraph();

    // Issues a new serial for linked shader modules. Used in the pipeline cache.
    Serial issueShaderSerial();

    vk::ShaderLibrary &getShaderLibrary() { return mShaderLibrary; }
    UtilsVk &getUtils() { return mUtils; }
    const angle::FeaturesVk &getFeatures() const
    {
        ASSERT(mFeaturesInitialized);
        return mFeatures;
    }

    angle::Result getTimestamp(vk::Context *context, uint64_t *timestampOut);

    // Create Begin/End/Instant GPU trace events, which take their timestamps from GPU queries.
    // The events are queued until the query results are available.  Possible values for `phase`
    // are TRACE_EVENT_PHASE_*
    ANGLE_INLINE angle::Result traceGpuEvent(vk::Context *context,
                                             vk::PrimaryCommandBuffer *commandBuffer,
                                             char phase,
                                             const char *name)
    {
        if (mGpuEventsEnabled)
            return traceGpuEventImpl(context, commandBuffer, phase, name);
        return angle::Result::Continue;
    }

    bool isMockICDEnabled() const { return mEnableMockICD; }

    RenderPassCache &getRenderPassCache() { return mRenderPassCache; }
    const vk::PipelineCache &getPipelineCache() const { return mPipelineCache; }

    // Query the format properties for select bits (linearTilingFeatures, optimalTilingFeatures and
    // bufferFeatures).  Looks through mandatory features first, and falls back to querying the
    // device (first time only).
    bool hasLinearImageFormatFeatureBits(VkFormat format, const VkFormatFeatureFlags featureBits);
    VkFormatFeatureFlags getImageFormatFeatureBits(VkFormat format,
                                                   const VkFormatFeatureFlags featureBits);
    bool hasImageFormatFeatureBits(VkFormat format, const VkFormatFeatureFlags featureBits);
    bool hasBufferFormatFeatureBits(VkFormat format, const VkFormatFeatureFlags featureBits);

    void insertDebugMarker(GLenum source, GLuint id, std::string &&marker);
    void pushDebugMarker(GLenum source, GLuint id, std::string &&marker);
    void popDebugMarker();

    static constexpr size_t kMaxExtensionNames = 200;
    using ExtensionNameList = angle::FixedVector<const char *, kMaxExtensionNames>;

  private:
    // Number of semaphores for external entities to renderer to issue a wait, such as surface's
    // image acquire.
    static constexpr size_t kMaxExternalSemaphores = 64;
    // Total possible number of semaphores a submission can wait on.  +1 is for the semaphore
    // signaled in the last submission.
    static constexpr size_t kMaxWaitSemaphores = kMaxExternalSemaphores + 1;

    angle::Result initializeDevice(DisplayVk *displayVk, uint32_t queueFamilyIndex);
    void ensureCapsInitialized() const;
    angle::Result submitFrame(vk::Context *context,
                              const VkSubmitInfo &submitInfo,
                              vk::PrimaryCommandBuffer &&commandBuffer);
    void freeAllInFlightResources();
    angle::Result flushCommandGraph(vk::Context *context, vk::PrimaryCommandBuffer *commandBatch);
    void initFeatures(const ExtensionNameList &extensions);
    void initPipelineCacheVkKey();
    angle::Result initPipelineCache(DisplayVk *display);

    angle::Result synchronizeCpuGpuTime(vk::Context *context,
                                        const vk::Semaphore *waitSemaphore,
                                        const vk::Semaphore *signalSemaphore);
    angle::Result traceGpuEventImpl(vk::Context *context,
                                    vk::PrimaryCommandBuffer *commandBuffer,
                                    char phase,
                                    const char *name);
    angle::Result checkCompletedGpuEvents(vk::Context *context);
    void flushGpuEvents(double nextSyncGpuTimestampS, double nextSyncCpuTimestampS);

    template <VkFormatFeatureFlags VkFormatProperties::*features>
    VkFormatFeatureFlags getFormatFeatureBits(VkFormat format,
                                              const VkFormatFeatureFlags featureBits);

    template <VkFormatFeatureFlags VkFormatProperties::*features>
    bool hasFormatFeatureBits(VkFormat format, const VkFormatFeatureFlags featureBits);

    void nextSerial();

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
    bool mEnableMockICD;
    VkDebugUtilsMessengerEXT mDebugUtilsMessenger;
    VkDebugReportCallbackEXT mDebugReportCallback;
    VkPhysicalDevice mPhysicalDevice;
    VkPhysicalDeviceProperties mPhysicalDeviceProperties;
    VkPhysicalDeviceFeatures mPhysicalDeviceFeatures;
    std::vector<VkQueueFamilyProperties> mQueueFamilyProperties;
    VkQueue mQueue;
    uint32_t mCurrentQueueFamilyIndex;
    uint32_t mMaxVertexAttribDivisor;
    VkDevice mDevice;
    vk::CommandPool mCommandPool;
    SerialFactory mQueueSerialFactory;
    SerialFactory mShaderSerialFactory;
    Serial mLastCompletedQueueSerial;
    Serial mLastSubmittedQueueSerial;
    Serial mCurrentQueueSerial;

    bool mDeviceLost;

    struct CommandBatch final : angle::NonCopyable
    {
        CommandBatch();
        ~CommandBatch();
        CommandBatch(CommandBatch &&other);
        CommandBatch &operator=(CommandBatch &&other);

        void destroy(VkDevice device);

        vk::CommandPool commandPool;
        vk::Shared<vk::Fence> fence;
        Serial serial;
    };

    std::vector<CommandBatch> mInFlightCommands;
    std::vector<vk::GarbageObject> mGarbage;
    vk::MemoryProperties mMemoryProperties;
    vk::FormatTable mFormatTable;

    RenderPassCache mRenderPassCache;

    vk::PipelineCache mPipelineCache;
    egl::BlobCache::Key mPipelineCacheVkBlobKey;
    uint32_t mPipelineCacheVkUpdateTimeout;

    // A cache of VkFormatProperties as queried from the device over time.
    std::array<VkFormatProperties, vk::kNumVkFormats> mFormatProperties;

    // mSubmitFence is the fence that's going to be signaled at the next submission.  This is used
    // to support SyncVk objects, which may outlive the context (as EGLSync objects).
    //
    // TODO(geofflang): this is in preparation for moving RendererVk functionality to ContextVk, and
    // is otherwise unnecessary as the SyncVk objects don't actually outlive the renderer currently.
    // http://anglebug.com/2701
    vk::Shared<vk::Fence> mSubmitFence;

    // Pool allocator used for command graph but may be expanded to other allocations
    angle::PoolAllocator mPoolAllocator;

    // See CommandGraph.h for a desription of the Command Graph.
    vk::CommandGraph mCommandGraph;

    // ANGLE uses a PipelineLayout cache to store compatible pipeline layouts.
    PipelineLayoutCache mPipelineLayoutCache;

    // DescriptorSetLayouts are also managed in a cache.
    DescriptorSetLayoutCache mDescriptorSetLayoutCache;

    // Internal shader library.
    vk::ShaderLibrary mShaderLibrary;
    UtilsVk mUtils;

    // The GpuEventQuery struct holds together a timestamp query and enough data to create a
    // trace event based on that. Use traceGpuEvent to insert such queries.  They will be readback
    // when the results are available, without inserting a GPU bubble.
    //
    // - eventName will be the reported name of the event
    // - phase is either 'B' (duration begin), 'E' (duration end) or 'i' (instant // event).
    //   See Google's "Trace Event Format":
    //   https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU
    // - serial is the serial of the batch the query was submitted on.  Until the batch is
    //   submitted, the query is not checked to avoid incuring a flush.
    struct GpuEventQuery final
    {
        const char *name;
        char phase;

        uint32_t queryIndex;
        size_t queryPoolIndex;

        Serial serial;
    };

    // Once a query result is available, the timestamp is read and a GpuEvent object is kept until
    // the next clock sync, at which point the clock drift is compensated in the results before
    // handing them off to the application.
    struct GpuEvent final
    {
        uint64_t gpuTimestampCycles;
        const char *name;
        char phase;
    };

    bool mGpuEventsEnabled;
    vk::DynamicQueryPool mGpuEventQueryPool;
    // A list of queries that have yet to be turned into an event (their result is not yet
    // available).
    std::vector<GpuEventQuery> mInFlightGpuEventQueries;
    // A list of gpu events since the last clock sync.
    std::vector<GpuEvent> mGpuEvents;

    // Hold information from the last gpu clock sync for future gpu-to-cpu timestamp conversions.
    struct GpuClockSyncInfo
    {
        double gpuTimestampS;
        double cpuTimestampS;
    };
    GpuClockSyncInfo mGpuClockSync;

    // The very first timestamp queried for a GPU event is used as origin, so event timestamps would
    // have a value close to zero, to avoid losing 12 bits when converting these 64 bit values to
    // double.
    uint64_t mGpuEventTimestampOrigin;
};

uint32_t GetUniformBufferDescriptorCount();

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_RENDERERVK_H_
