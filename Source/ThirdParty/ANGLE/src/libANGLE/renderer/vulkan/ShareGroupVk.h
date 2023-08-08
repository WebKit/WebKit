//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ShareGroupVk.h:
//    Defines the class interface for ShareGroupVk, implementing ShareGroupImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_SHAREGROUPVK_H_
#define LIBANGLE_RENDERER_VULKAN_SHAREGROUPVK_H_

#include "libANGLE/renderer/ShareGroupImpl.h"
#include "libANGLE/renderer/vulkan/ResourceVk.h"
#include "libANGLE/renderer/vulkan/vk_cache_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

namespace rx
{
constexpr VkDeviceSize kMaxTotalEmptyBufferBytes = 16 * 1024 * 1024;

class RendererVk;

class TextureUpload
{
  public:
    TextureUpload() { mPrevUploadedMutableTexture = nullptr; }
    ~TextureUpload() { resetPrevTexture(); }
    angle::Result onMutableTextureUpload(ContextVk *contextVk, TextureVk *newTexture);
    void onTextureRelease(TextureVk *textureVk);
    void resetPrevTexture() { mPrevUploadedMutableTexture = nullptr; }

  private:
    // Keep track of the previously stored texture. Used to flush mutable textures.
    TextureVk *mPrevUploadedMutableTexture;
};

class UpdateDescriptorSetsBuilder final : angle::NonCopyable
{
  public:
    UpdateDescriptorSetsBuilder();
    ~UpdateDescriptorSetsBuilder();

    VkDescriptorBufferInfo *allocDescriptorBufferInfos(size_t count);
    VkDescriptorImageInfo *allocDescriptorImageInfos(size_t count);
    VkWriteDescriptorSet *allocWriteDescriptorSets(size_t count);
    VkBufferView *allocBufferViews(size_t count);

    VkDescriptorBufferInfo &allocDescriptorBufferInfo() { return *allocDescriptorBufferInfos(1); }
    VkDescriptorImageInfo &allocDescriptorImageInfo() { return *allocDescriptorImageInfos(1); }
    VkWriteDescriptorSet &allocWriteDescriptorSet() { return *allocWriteDescriptorSets(1); }
    VkBufferView &allocBufferView() { return *allocBufferViews(1); }

    // Returns the number of written descriptor sets.
    uint32_t flushDescriptorSetUpdates(VkDevice device);

  private:
    template <typename T, const T *VkWriteDescriptorSet::*pInfo>
    T *allocDescriptorInfos(std::vector<T> *descriptorVector, size_t count);
    template <typename T, const T *VkWriteDescriptorSet::*pInfo>
    void growDescriptorCapacity(std::vector<T> *descriptorVector, size_t newSize);

    std::vector<VkDescriptorBufferInfo> mDescriptorBufferInfos;
    std::vector<VkDescriptorImageInfo> mDescriptorImageInfos;
    std::vector<VkWriteDescriptorSet> mWriteDescriptorSets;
    std::vector<VkBufferView> mBufferViews;
};

class ShareGroupVk : public ShareGroupImpl
{
  public:
    ShareGroupVk(const egl::ShareGroupState &state);
    void onDestroy(const egl::Display *display) override;

    void onContextAdd() override;

    FramebufferCache &getFramebufferCache() { return mFramebufferCache; }

    bool hasAnyContextWithRobustness() const { return mState.hasAnyContextWithRobustness(); }

    // PipelineLayoutCache and DescriptorSetLayoutCache can be shared between multiple threads
    // accessing them via shared contexts. The ShareGroup locks around gl entrypoints ensuring
    // synchronous update to the caches.
    PipelineLayoutCache &getPipelineLayoutCache() { return mPipelineLayoutCache; }
    DescriptorSetLayoutCache &getDescriptorSetLayoutCache() { return mDescriptorSetLayoutCache; }
    const egl::ContextMap &getContexts() const { return mState.getContexts(); }
    vk::MetaDescriptorPool &getMetaDescriptorPool(DescriptorSetIndex descriptorSetIndex)
    {
        return mMetaDescriptorPools[descriptorSetIndex];
    }

    // Used to flush the mutable textures more often.
    angle::Result onMutableTextureUpload(ContextVk *contextVk, TextureVk *newTexture);

    vk::BufferPool *getDefaultBufferPool(RendererVk *renderer,
                                         VkDeviceSize size,
                                         uint32_t memoryTypeIndex,
                                         BufferUsageType usageType);
    void pruneDefaultBufferPools(RendererVk *renderer);
    bool isDueForBufferPoolPrune(RendererVk *renderer);

    void calculateTotalBufferCount(size_t *bufferCount, VkDeviceSize *totalSize) const;
    void logBufferPools() const;

    // Temporary workaround until VkSemaphore(s) will be used between different priorities.
    angle::Result unifyContextsPriority(ContextVk *newContextVk);
    // Temporary workaround until VkSemaphore(s) will be used between different priorities.
    angle::Result lockDefaultContextsPriority(ContextVk *contextVk);

    UpdateDescriptorSetsBuilder *getUpdateDescriptorSetsBuilder()
    {
        return &mUpdateDescriptorSetsBuilder;
    }

    void onTextureRelease(TextureVk *textureVk);

    angle::Result scheduleMonolithicPipelineCreationTask(
        ContextVk *contextVk,
        vk::WaitableMonolithicPipelineCreationTask *taskOut);
    void waitForCurrentMonolithicPipelineCreationTask();

  private:
    angle::Result updateContextsPriority(ContextVk *contextVk, egl::ContextPriority newPriority);

    // VkFramebuffer caches
    FramebufferCache mFramebufferCache;

    void resetPrevTexture() { mTextureUpload.resetPrevTexture(); }

    // ANGLE uses a PipelineLayout cache to store compatible pipeline layouts.
    PipelineLayoutCache mPipelineLayoutCache;

    // DescriptorSetLayouts are also managed in a cache.
    DescriptorSetLayoutCache mDescriptorSetLayoutCache;

    // Descriptor set caches
    vk::DescriptorSetArray<vk::MetaDescriptorPool> mMetaDescriptorPools;

    // Priority of all Contexts in the context set
    egl::ContextPriority mContextsPriority;
    bool mIsContextsPriorityLocked;

    // Storage for vkUpdateDescriptorSets
    UpdateDescriptorSetsBuilder mUpdateDescriptorSetsBuilder;

    // The per shared group buffer pools that all buffers should sub-allocate from.
    enum class SuballocationAlgorithm : uint8_t
    {
        Buddy       = 0,
        General     = 1,
        InvalidEnum = 2,
        EnumCount   = InvalidEnum,
    };
    angle::PackedEnumMap<SuballocationAlgorithm, vk::BufferPoolPointerArray> mDefaultBufferPools;
    angle::PackedEnumMap<BufferUsageType, size_t> mSizeLimitForBuddyAlgorithm;

    // The system time when last pruneEmptyBuffer gets called.
    double mLastPruneTime;

    // The system time when the last monolithic pipeline creation job was launched.  This is
    // rate-limited to avoid hogging all cores and interfering with the application threads.  A
    // single pipeline creation job is currently supported.
    double mLastMonolithicPipelineJobTime;
    std::shared_ptr<angle::WaitableEvent> mMonolithicPipelineCreationEvent;

    // Texture update manager used to flush uploaded mutable textures.
    TextureUpload mTextureUpload;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_SHAREGROUPVK_H_
