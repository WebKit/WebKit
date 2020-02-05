//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ContextVk.h:
//    Defines the class interface for ContextVk, implementing ContextImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_CONTEXTVK_H_
#define LIBANGLE_RENDERER_VULKAN_CONTEXTVK_H_

#include "volk.h"

#include "common/PackedEnums.h"
#include "libANGLE/renderer/ContextImpl.h"
#include "libANGLE/renderer/vulkan/OverlayVk.h"
#include "libANGLE/renderer/vulkan/PersistentCommandPool.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace angle
{
struct FeaturesVk;
}  // namespace angle

namespace rx
{
class RendererVk;
class WindowSurfaceVk;

struct CommandBatch final : angle::NonCopyable
{
    CommandBatch();
    ~CommandBatch();
    CommandBatch(CommandBatch &&other);
    CommandBatch &operator=(CommandBatch &&other);

    void destroy(VkDevice device);

    vk::PrimaryCommandBuffer primaryCommands;
    // commandPool is for secondary CommandBuffer allocation
    vk::CommandPool commandPool;
    vk::Shared<vk::Fence> fence;
    Serial serial;
};

class CommandQueue final : angle::NonCopyable
{
  public:
    CommandQueue();
    ~CommandQueue();

    angle::Result init(vk::Context *context);
    void destroy(VkDevice device);
    void handleDeviceLost(RendererVk *renderer);

    bool hasInFlightCommands() const;

    angle::Result allocatePrimaryCommandBuffer(vk::Context *context,
                                               const vk::CommandPool &commandPool,
                                               vk::PrimaryCommandBuffer *commandBufferOut);
    angle::Result releasePrimaryCommandBuffer(vk::Context *context,
                                              vk::PrimaryCommandBuffer &&commandBuffer);

    void clearAllGarbage(VkDevice device);

    angle::Result finishToSerial(vk::Context *context, Serial serial, uint64_t timeout);

    angle::Result submitFrame(vk::Context *context,
                              egl::ContextPriority priority,
                              const VkSubmitInfo &submitInfo,
                              const vk::Shared<vk::Fence> &sharedFence,
                              vk::GarbageList *currentGarbage,
                              vk::CommandPool *commandPool,
                              vk::PrimaryCommandBuffer &&commandBuffer);

    vk::Shared<vk::Fence> getLastSubmittedFence(const vk::Context *context) const;

    // Check to see which batches have finished completion (forward progress for
    // mLastCompletedQueueSerial, for example for when the application busy waits on a query
    // result). It would be nice if we didn't have to expose this for QueryVk::getResult.
    angle::Result checkCompletedCommands(vk::Context *context);

  private:
    angle::Result releaseToCommandBatch(vk::Context *context,
                                        vk::PrimaryCommandBuffer &&commandBuffer,
                                        vk::CommandPool *commandPool,
                                        CommandBatch *batch);

    vk::GarbageQueue mGarbageQueue;
    std::vector<CommandBatch> mInFlightCommands;

    // Keeps a free list of reusable primary command buffers.
    vk::PersistentCommandPool mPrimaryCommandPool;
};

class OutsideRenderPassCommandBuffer final : angle::NonCopyable
{
  public:
    OutsideRenderPassCommandBuffer();
    ~OutsideRenderPassCommandBuffer();

    void bufferRead(VkAccessFlags readAccessType, vk::BufferHelper *buffer);
    void bufferWrite(VkAccessFlags writeAccessType, vk::BufferHelper *buffer);

    void flushToPrimary(vk::PrimaryCommandBuffer *primary);

    vk::CommandBuffer &getCommandBuffer() { return mCommandBuffer; }

    bool empty() const { return mCommandBuffer.empty(); }
    void reset();

  private:
    VkFlags mGlobalMemoryBarrierSrcAccess;
    VkFlags mGlobalMemoryBarrierDstAccess;
    VkPipelineStageFlags mGlobalMemoryBarrierStages;

    vk::CommandBuffer mCommandBuffer;
};

class RenderPassCommandBuffer final : angle::NonCopyable
{
  public:
    RenderPassCommandBuffer();
    ~RenderPassCommandBuffer();

    void initialize(angle::PoolAllocator *poolAllocator);

    void beginRenderPass(const vk::Framebuffer &framebuffer,
                         const gl::Rectangle &renderArea,
                         const vk::RenderPassDesc &renderPassDesc,
                         const vk::AttachmentOpsArray &renderPassAttachmentOps,
                         const std::vector<VkClearValue> &clearValues,
                         vk::CommandBuffer **commandBufferOut);

    void clearRenderPassColorAttachment(size_t attachmentIndex, const VkClearColorValue &clearValue)
    {
        mAttachmentOps[attachmentIndex].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        mClearValues[attachmentIndex].color    = clearValue;
    }

    void clearRenderPassDepthAttachment(size_t attachmentIndex, float depth)
    {
        mAttachmentOps[attachmentIndex].loadOp           = VK_ATTACHMENT_LOAD_OP_CLEAR;
        mClearValues[attachmentIndex].depthStencil.depth = depth;
    }

    void clearRenderPassStencilAttachment(size_t attachmentIndex, uint32_t stencil)
    {
        mAttachmentOps[attachmentIndex].stencilLoadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
        mClearValues[attachmentIndex].depthStencil.stencil = stencil;
    }

    void invalidateRenderPassColorAttachment(size_t attachmentIndex)
    {
        mAttachmentOps[attachmentIndex].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }

    void invalidateRenderPassDepthAttachment(size_t attachmentIndex)
    {
        mAttachmentOps[attachmentIndex].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }

    void invalidateRenderPassStencilAttachment(size_t attachmentIndex)
    {
        mAttachmentOps[attachmentIndex].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }

    angle::Result flushToPrimary(ContextVk *contextVk, vk::PrimaryCommandBuffer *primary);

    bool empty() const { return mCommandBuffer.empty(); }
    void reset();

  private:
    vk::RenderPassDesc mRenderPassDesc;
    vk::AttachmentOpsArray mAttachmentOps;
    vk::Framebuffer mFramebuffer;
    gl::Rectangle mRenderArea;
    gl::AttachmentArray<VkClearValue> mClearValues;
    vk::CommandBuffer mCommandBuffer;
};

class ContextVk : public ContextImpl, public vk::Context, public vk::RenderPassOwner
{
  public:
    ContextVk(const gl::State &state, gl::ErrorSet *errorSet, RendererVk *renderer);
    ~ContextVk() override;

    angle::Result initialize() override;

    void onDestroy(const gl::Context *context) override;

    // Flush and finish.
    angle::Result flush(const gl::Context *context) override;
    angle::Result finish(const gl::Context *context) override;

    // Drawing methods.
    angle::Result drawArrays(const gl::Context *context,
                             gl::PrimitiveMode mode,
                             GLint first,
                             GLsizei count) override;
    angle::Result drawArraysInstanced(const gl::Context *context,
                                      gl::PrimitiveMode mode,
                                      GLint first,
                                      GLsizei count,
                                      GLsizei instanceCount) override;
    angle::Result drawArraysInstancedBaseInstance(const gl::Context *context,
                                                  gl::PrimitiveMode mode,
                                                  GLint first,
                                                  GLsizei count,
                                                  GLsizei instanceCount,
                                                  GLuint baseInstance) override;

    angle::Result drawElements(const gl::Context *context,
                               gl::PrimitiveMode mode,
                               GLsizei count,
                               gl::DrawElementsType type,
                               const void *indices) override;
    angle::Result drawElementsBaseVertex(const gl::Context *context,
                                         gl::PrimitiveMode mode,
                                         GLsizei count,
                                         gl::DrawElementsType type,
                                         const void *indices,
                                         GLint baseVertex) override;
    angle::Result drawElementsInstanced(const gl::Context *context,
                                        gl::PrimitiveMode mode,
                                        GLsizei count,
                                        gl::DrawElementsType type,
                                        const void *indices,
                                        GLsizei instanceCount) override;
    angle::Result drawElementsInstancedBaseVertex(const gl::Context *context,
                                                  gl::PrimitiveMode mode,
                                                  GLsizei count,
                                                  gl::DrawElementsType type,
                                                  const void *indices,
                                                  GLsizei instanceCount,
                                                  GLint baseVertex) override;
    angle::Result drawElementsInstancedBaseVertexBaseInstance(const gl::Context *context,
                                                              gl::PrimitiveMode mode,
                                                              GLsizei count,
                                                              gl::DrawElementsType type,
                                                              const void *indices,
                                                              GLsizei instances,
                                                              GLint baseVertex,
                                                              GLuint baseInstance) override;
    angle::Result drawRangeElements(const gl::Context *context,
                                    gl::PrimitiveMode mode,
                                    GLuint start,
                                    GLuint end,
                                    GLsizei count,
                                    gl::DrawElementsType type,
                                    const void *indices) override;
    angle::Result drawRangeElementsBaseVertex(const gl::Context *context,
                                              gl::PrimitiveMode mode,
                                              GLuint start,
                                              GLuint end,
                                              GLsizei count,
                                              gl::DrawElementsType type,
                                              const void *indices,
                                              GLint baseVertex) override;
    angle::Result drawArraysIndirect(const gl::Context *context,
                                     gl::PrimitiveMode mode,
                                     const void *indirect) override;
    angle::Result drawElementsIndirect(const gl::Context *context,
                                       gl::PrimitiveMode mode,
                                       gl::DrawElementsType type,
                                       const void *indirect) override;

    // Device loss
    gl::GraphicsResetStatus getResetStatus() override;

    // Vendor and description strings.
    std::string getVendorString() const override;
    std::string getRendererDescription() const override;

    // EXT_debug_marker
    void insertEventMarker(GLsizei length, const char *marker) override;
    void pushGroupMarker(GLsizei length, const char *marker) override;
    void popGroupMarker() override;

    // KHR_debug
    void pushDebugGroup(GLenum source, GLuint id, const std::string &message) override;
    void popDebugGroup() override;

    bool isViewportFlipEnabledForDrawFBO() const;
    bool isViewportFlipEnabledForReadFBO() const;

    // State sync with dirty bits.
    angle::Result syncState(const gl::Context *context,
                            const gl::State::DirtyBits &dirtyBits,
                            const gl::State::DirtyBits &bitMask) override;

    // Disjoint timer queries
    GLint getGPUDisjoint() override;
    GLint64 getTimestamp() override;

    // Context switching
    angle::Result onMakeCurrent(const gl::Context *context) override;
    angle::Result onUnMakeCurrent(const gl::Context *context) override;

    // Native capabilities, unmodified by gl::Context.
    gl::Caps getNativeCaps() const override;
    const gl::TextureCapsMap &getNativeTextureCaps() const override;
    const gl::Extensions &getNativeExtensions() const override;
    const gl::Limitations &getNativeLimitations() const override;

    // Shader creation
    CompilerImpl *createCompiler() override;
    ShaderImpl *createShader(const gl::ShaderState &state) override;
    ProgramImpl *createProgram(const gl::ProgramState &state) override;

    // Framebuffer creation
    FramebufferImpl *createFramebuffer(const gl::FramebufferState &state) override;

    // Texture creation
    TextureImpl *createTexture(const gl::TextureState &state) override;

    // Renderbuffer creation
    RenderbufferImpl *createRenderbuffer(const gl::RenderbufferState &state) override;

    // Buffer creation
    BufferImpl *createBuffer(const gl::BufferState &state) override;

    // Vertex Array creation
    VertexArrayImpl *createVertexArray(const gl::VertexArrayState &state) override;

    // Query and Fence creation
    QueryImpl *createQuery(gl::QueryType type) override;
    FenceNVImpl *createFenceNV() override;
    SyncImpl *createSync() override;

    // Transform Feedback creation
    TransformFeedbackImpl *createTransformFeedback(
        const gl::TransformFeedbackState &state) override;

    // Sampler object creation
    SamplerImpl *createSampler(const gl::SamplerState &state) override;

    // Program Pipeline object creation
    ProgramPipelineImpl *createProgramPipeline(const gl::ProgramPipelineState &data) override;

    // Path object creation
    std::vector<PathImpl *> createPaths(GLsizei) override;

    // Memory object creation.
    MemoryObjectImpl *createMemoryObject() override;

    // Semaphore creation.
    SemaphoreImpl *createSemaphore() override;

    // Overlay creation.
    OverlayImpl *createOverlay(const gl::OverlayState &state) override;

    angle::Result dispatchCompute(const gl::Context *context,
                                  GLuint numGroupsX,
                                  GLuint numGroupsY,
                                  GLuint numGroupsZ) override;
    angle::Result dispatchComputeIndirect(const gl::Context *context, GLintptr indirect) override;

    angle::Result memoryBarrier(const gl::Context *context, GLbitfield barriers) override;
    angle::Result memoryBarrierByRegion(const gl::Context *context, GLbitfield barriers) override;

    VkDevice getDevice() const;
    egl::ContextPriority getPriority() const { return mContextPriority; }

    ANGLE_INLINE const angle::FeaturesVk &getFeatures() const { return mRenderer->getFeatures(); }
    ANGLE_INLINE bool commandGraphEnabled() const { return getFeatures().commandGraph.enabled; }

    ANGLE_INLINE void invalidateVertexAndIndexBuffers()
    {
        // TODO: Make the pipeline invalidate more fine-grained. Only need to dirty here if PSO
        //  VtxInput state (stride, fmt, inputRate...) has changed. http://anglebug.com/3256
        invalidateCurrentGraphicsPipeline();
        mGraphicsDirtyBits.set(DIRTY_BIT_VERTEX_BUFFERS);
        mGraphicsDirtyBits.set(DIRTY_BIT_INDEX_BUFFER);
    }

    ANGLE_INLINE void invalidateVertexBuffers()
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_VERTEX_BUFFERS);
    }

    ANGLE_INLINE void onVertexAttributeChange(size_t attribIndex,
                                              GLuint stride,
                                              GLuint divisor,
                                              angle::FormatID format,
                                              GLuint relativeOffset)
    {
        invalidateVertexAndIndexBuffers();
        // Set divisor to 1 for attribs with emulated divisor
        mGraphicsPipelineDesc->updateVertexInput(
            &mGraphicsPipelineTransition, static_cast<uint32_t>(attribIndex), stride,
            divisor > mRenderer->getMaxVertexAttribDivisor() ? 1 : divisor, format, relativeOffset);
    }

    void invalidateDefaultAttribute(size_t attribIndex);
    void invalidateDefaultAttributes(const gl::AttributesMask &dirtyMask);
    void onDrawFramebufferChange(FramebufferVk *framebufferVk);
    void onHostVisibleBufferWrite() { mIsAnyHostVisibleBufferWritten = true; }

    void invalidateCurrentTransformFeedbackBuffers();
    void invalidateCurrentTransformFeedbackState();
    void onTransformFeedbackStateChanged();

    // When UtilsVk issues draw or dispatch calls, it binds descriptor sets that the context is not
    // aware of.  This function is called to make sure affected descriptor set bindings are dirtied
    // for the next application draw/dispatch call.
    void invalidateGraphicsDescriptorSet(uint32_t usedDescriptorSet);
    void invalidateComputeDescriptorSet(uint32_t usedDescriptorSet);

    vk::DynamicQueryPool *getQueryPool(gl::QueryType queryType);

    const VkClearValue &getClearColorValue() const;
    const VkClearValue &getClearDepthStencilValue() const;
    VkColorComponentFlags getClearColorMask() const;
    angle::Result getIncompleteTexture(const gl::Context *context,
                                       gl::TextureType type,
                                       gl::Texture **textureOut);
    void updateColorMask(const gl::BlendState &blendState);
    void updateSampleMask(const gl::State &glState);

    void handleError(VkResult errorCode,
                     const char *file,
                     const char *function,
                     unsigned int line) override;
    const gl::ActiveTextureArray<vk::TextureUnit> &getActiveTextures() const
    {
        return mActiveTextures;
    }
    const gl::ActiveTextureArray<TextureVk *> &getActiveImages() const { return mActiveImages; }

    void setIndexBufferDirty()
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_INDEX_BUFFER);
        mLastIndexBufferOffset = reinterpret_cast<const void *>(angle::DirtyPointer);
    }

    void insertWaitSemaphore(const vk::Semaphore *waitSemaphore);

    bool shouldFlush();
    angle::Result flushImpl(const vk::Semaphore *semaphore);
    angle::Result finishImpl();

    void addWaitSemaphore(VkSemaphore semaphore);

    const vk::CommandPool &getCommandPool() const;

    Serial getCurrentQueueSerial() const { return mRenderer->getCurrentQueueSerial(); }
    Serial getLastSubmittedQueueSerial() const { return mRenderer->getLastSubmittedQueueSerial(); }
    Serial getLastCompletedQueueSerial() const { return mRenderer->getLastCompletedQueueSerial(); }

    bool isSerialInUse(Serial serial) const;

    template <typename T>
    void addGarbage(T *object)
    {
        if (object->valid())
        {
            mCurrentGarbage.emplace_back(vk::GetGarbage(object));
        }
    }

    // It would be nice if we didn't have to expose this for QueryVk::getResult.
    angle::Result checkCompletedCommands();

    // Wait for completion of batches until (at least) batch with given serial is finished.
    angle::Result finishToSerial(Serial serial);

    angle::Result getCompatibleRenderPass(const vk::RenderPassDesc &desc,
                                          vk::RenderPass **renderPassOut);
    angle::Result getRenderPassWithOps(const vk::RenderPassDesc &desc,
                                       const vk::AttachmentOpsArray &ops,
                                       vk::RenderPass **renderPassOut);

    // Get (or allocate) the fence that will be signaled on next submission.
    angle::Result getNextSubmitFence(vk::Shared<vk::Fence> *sharedFenceOut);
    vk::Shared<vk::Fence> getLastSubmittedFence() const;

    // This should only be called from ResourceVk.
    vk::CommandGraph *getCommandGraph() { return &mCommandGraph; }

    vk::ShaderLibrary &getShaderLibrary() { return mShaderLibrary; }
    UtilsVk &getUtils() { return mUtils; }

    angle::Result getTimestamp(uint64_t *timestampOut);

    // Create Begin/End/Instant GPU trace events, which take their timestamps from GPU queries.
    // The events are queued until the query results are available.  Possible values for `phase`
    // are TRACE_EVENT_PHASE_*
    ANGLE_INLINE angle::Result traceGpuEvent(vk::PrimaryCommandBuffer *commandBuffer,
                                             char phase,
                                             const char *name)
    {
        if (mGpuEventsEnabled)
            return traceGpuEventImpl(commandBuffer, phase, name);
        return angle::Result::Continue;
    }

    RenderPassCache &getRenderPassCache() { return mRenderPassCache; }

    vk::DescriptorSetLayoutDesc getDriverUniformsDescriptorSetDesc(
        VkShaderStageFlags shaderStages) const;

    // We use texture serials to optimize texture binding updates. Each permutation of a
    // {VkImage/VkSampler} generates a unique serial. These serials are combined to form a unique
    // signature for each descriptor set. This allows us to keep a cache of descriptor sets and
    // avoid calling vkAllocateDesctiporSets each texture update.
    Serial generateTextureSerial() { return mTextureSerialFactory.generate(); }
    const vk::TextureDescriptorDesc &getActiveTexturesDesc() const { return mActiveTexturesDesc; }

    void updateScissor(const gl::State &glState);

    bool emulateSeamfulCubeMapSampling() const { return mEmulateSeamfulCubeMapSampling; }

    bool useOldRewriteStructSamplers() const { return mUseOldRewriteStructSamplers; }

    const gl::OverlayType *getOverlay() const { return mState.getOverlay(); }

    vk::ResourceUseList &getResourceUseList() { return mResourceUseList; }

    void onBufferRead(VkAccessFlags readAccessType, vk::BufferHelper *buffer);
    void onBufferWrite(VkAccessFlags writeAccessType, vk::BufferHelper *buffer);
    angle::Result getOutsideRenderPassCommandBuffer(vk::CommandBuffer **commandBufferOut)
    {
        if (!mRenderPassCommands.empty())
        {
            ANGLE_TRY(mRenderPassCommands.flushToPrimary(this, &mPrimaryCommands));
        }
        *commandBufferOut = &mOutsideRenderPassCommands.getCommandBuffer();
        return angle::Result::Continue;
    }

    void beginRenderPass(const vk::Framebuffer &framebuffer,
                         const gl::Rectangle &renderArea,
                         const vk::RenderPassDesc &renderPassDesc,
                         const vk::AttachmentOpsArray &renderPassAttachmentOps,
                         const std::vector<VkClearValue> &clearValues,
                         vk::CommandBuffer **commandBufferOut);

    RenderPassCommandBuffer &getRenderPassCommandBuffer()
    {
        if (!mOutsideRenderPassCommands.empty())
        {
            mOutsideRenderPassCommands.flushToPrimary(&mPrimaryCommands);
        }
        return mRenderPassCommands;
    }

    egl::ContextPriority getContextPriority() const override { return mContextPriority; }

  private:
    // Dirty bits.
    enum DirtyBitType : size_t
    {
        DIRTY_BIT_DEFAULT_ATTRIBS,
        DIRTY_BIT_PIPELINE,
        DIRTY_BIT_TEXTURES,
        DIRTY_BIT_VERTEX_BUFFERS,
        DIRTY_BIT_INDEX_BUFFER,
        DIRTY_BIT_DRIVER_UNIFORMS,
        DIRTY_BIT_DRIVER_UNIFORMS_BINDING,
        DIRTY_BIT_SHADER_RESOURCES,  // excluding textures, which are handled separately.
        DIRTY_BIT_TRANSFORM_FEEDBACK_BUFFERS,
        DIRTY_BIT_TRANSFORM_FEEDBACK_STATE,
        DIRTY_BIT_DESCRIPTOR_SETS,
        DIRTY_BIT_MAX,
    };

    using DirtyBits = angle::BitSet<DIRTY_BIT_MAX>;

    using DirtyBitHandler = angle::Result (ContextVk::*)(const gl::Context *,
                                                         vk::CommandBuffer *commandBuffer);

    struct DriverUniformsDescriptorSet
    {
        vk::DynamicBuffer dynamicBuffer;
        VkDescriptorSet descriptorSet;
        uint32_t dynamicOffset;
        vk::BindingPointer<vk::DescriptorSetLayout> descriptorSetLayout;
        vk::RefCountedDescriptorPoolBinding descriptorPoolBinding;

        DriverUniformsDescriptorSet();
        ~DriverUniformsDescriptorSet();

        void init(RendererVk *rendererVk);
        void destroy(VkDevice device);
    };

    enum class PipelineType
    {
        Graphics = 0,
        Compute  = 1,

        InvalidEnum = 2,
        EnumCount   = 2,
    };

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

    struct GpuClockSyncInfo
    {
        double gpuTimestampS;
        double cpuTimestampS;
    };

    angle::Result setupDraw(const gl::Context *context,
                            gl::PrimitiveMode mode,
                            GLint firstVertexOrInvalid,
                            GLsizei vertexOrIndexCount,
                            GLsizei instanceCount,
                            gl::DrawElementsType indexTypeOrInvalid,
                            const void *indices,
                            DirtyBits dirtyBitMask,
                            vk::CommandBuffer **commandBufferOut);
    angle::Result setupIndexedDraw(const gl::Context *context,
                                   gl::PrimitiveMode mode,
                                   GLsizei indexCount,
                                   GLsizei instanceCount,
                                   gl::DrawElementsType indexType,
                                   const void *indices,
                                   vk::CommandBuffer **commandBufferOut);
    angle::Result setupIndirectDraw(const gl::Context *context,
                                    gl::PrimitiveMode mode,
                                    DirtyBits dirtyBitMask,
                                    vk::BufferHelper *indirectBuffer,
                                    VkDeviceSize indirectBufferOffset,
                                    vk::CommandBuffer **commandBufferOut);
    angle::Result setupIndexedIndirectDraw(const gl::Context *context,
                                           gl::PrimitiveMode mode,
                                           gl::DrawElementsType indexType,
                                           vk::BufferHelper *indirectBuffer,
                                           VkDeviceSize indirectBufferOffset,
                                           vk::CommandBuffer **commandBufferOut);

    angle::Result setupLineLoopIndexedIndirectDraw(const gl::Context *context,
                                                   gl::PrimitiveMode mode,
                                                   gl::DrawElementsType indexType,
                                                   vk::BufferHelper *srcIndirectBuf,
                                                   VkDeviceSize indirectBufferOffset,
                                                   vk::CommandBuffer **commandBufferOut,
                                                   vk::BufferHelper **indirectBufferOut,
                                                   VkDeviceSize *indirectBufferOffsetOut);
    angle::Result setupLineLoopIndirectDraw(const gl::Context *context,
                                            gl::PrimitiveMode mode,
                                            vk::BufferHelper *indirectBuffer,
                                            VkDeviceSize indirectBufferOffset,
                                            vk::CommandBuffer **commandBufferOut,
                                            vk::BufferHelper **indirectBufferOut,
                                            VkDeviceSize *indirectBufferOffsetOut);

    angle::Result setupLineLoopDraw(const gl::Context *context,
                                    gl::PrimitiveMode mode,
                                    GLint firstVertex,
                                    GLsizei vertexOrIndexCount,
                                    gl::DrawElementsType indexTypeOrInvalid,
                                    const void *indices,
                                    vk::CommandBuffer **commandBufferOut,
                                    uint32_t *numIndicesOut);
    angle::Result setupDispatch(const gl::Context *context, vk::CommandBuffer **commandBufferOut);

    void updateViewport(FramebufferVk *framebufferVk,
                        const gl::Rectangle &viewport,
                        float nearPlane,
                        float farPlane,
                        bool invertViewport);
    void updateDepthRange(float nearPlane, float farPlane);
    void updateFlipViewportDrawFramebuffer(const gl::State &glState);
    void updateFlipViewportReadFramebuffer(const gl::State &glState);

    angle::Result updateActiveTextures(const gl::Context *context);
    angle::Result updateActiveImages(const gl::Context *context,
                                     vk::CommandGraphResource *recorder);
    angle::Result updateDefaultAttribute(size_t attribIndex);

    ANGLE_INLINE void invalidateCurrentGraphicsPipeline()
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_PIPELINE);
    }
    ANGLE_INLINE void invalidateCurrentComputePipeline()
    {
        mComputeDirtyBits.set(DIRTY_BIT_PIPELINE);
        mCurrentComputePipeline = nullptr;
    }

    void invalidateCurrentDefaultUniforms();
    angle::Result invalidateCurrentTextures(const gl::Context *context);
    void invalidateCurrentShaderResources();
    void invalidateGraphicsDriverUniforms();
    void invalidateDriverUniforms();

    // Handlers for graphics pipeline dirty bits.
    angle::Result handleDirtyGraphicsDefaultAttribs(const gl::Context *context,
                                                    vk::CommandBuffer *commandBuffer);
    angle::Result handleDirtyGraphicsPipeline(const gl::Context *context,
                                              vk::CommandBuffer *commandBuffer);
    angle::Result handleDirtyGraphicsTextures(const gl::Context *context,
                                              vk::CommandBuffer *commandBuffer);
    angle::Result handleDirtyGraphicsVertexBuffers(const gl::Context *context,
                                                   vk::CommandBuffer *commandBuffer);
    angle::Result handleDirtyGraphicsIndexBuffer(const gl::Context *context,
                                                 vk::CommandBuffer *commandBuffer);
    angle::Result handleDirtyGraphicsDriverUniforms(const gl::Context *context,
                                                    vk::CommandBuffer *commandBuffer);
    angle::Result handleDirtyGraphicsDriverUniformsBinding(const gl::Context *context,
                                                           vk::CommandBuffer *commandBuffer);
    angle::Result handleDirtyGraphicsShaderResources(const gl::Context *context,
                                                     vk::CommandBuffer *commandBuffer);
    angle::Result handleDirtyGraphicsTransformFeedbackBuffersEmulation(
        const gl::Context *context,
        vk::CommandBuffer *commandBuffer);
    angle::Result handleDirtyGraphicsTransformFeedbackBuffersExtension(
        const gl::Context *context,
        vk::CommandBuffer *commandBuffer);
    angle::Result handleDirtyGraphicsTransformFeedbackState(const gl::Context *context,
                                                            vk::CommandBuffer *commandBuffer);

    // Handlers for compute pipeline dirty bits.
    angle::Result handleDirtyComputePipeline(const gl::Context *context,
                                             vk::CommandBuffer *commandBuffer);
    angle::Result handleDirtyComputeTextures(const gl::Context *context,
                                             vk::CommandBuffer *commandBuffer);
    angle::Result handleDirtyComputeDriverUniforms(const gl::Context *context,
                                                   vk::CommandBuffer *commandBuffer);
    angle::Result handleDirtyComputeDriverUniformsBinding(const gl::Context *context,
                                                          vk::CommandBuffer *commandBuffer);
    angle::Result handleDirtyComputeShaderResources(const gl::Context *context,
                                                    vk::CommandBuffer *commandBuffer);

    // Common parts of the common dirty bit handlers.
    angle::Result handleDirtyTexturesImpl(const gl::Context *context,
                                          vk::CommandBuffer *commandBuffer,
                                          vk::CommandGraphResource *recorder);
    angle::Result handleDirtyShaderResourcesImpl(const gl::Context *context,
                                                 vk::CommandBuffer *commandBuffer,
                                                 vk::CommandGraphResource *recorder);
    void handleDirtyDriverUniformsBindingImpl(vk::CommandBuffer *commandBuffer,
                                              VkPipelineBindPoint bindPoint,
                                              const DriverUniformsDescriptorSet &driverUniforms);
    angle::Result handleDirtyDescriptorSets(const gl::Context *context,
                                            vk::CommandBuffer *commandBuffer);
    angle::Result allocateDriverUniforms(size_t driverUniformsSize,
                                         DriverUniformsDescriptorSet *driverUniforms,
                                         VkBuffer *bufferOut,
                                         uint8_t **ptrOut,
                                         bool *newBufferOut);
    angle::Result updateDriverUniformsDescriptorSet(VkBuffer buffer,
                                                    bool newBuffer,
                                                    size_t driverUniformsSize,
                                                    DriverUniformsDescriptorSet *driverUniforms);

    void writeAtomicCounterBufferDriverUniformOffsets(uint32_t *offsetsOut, size_t offsetsSize);

    angle::Result submitFrame(const VkSubmitInfo &submitInfo,
                              vk::PrimaryCommandBuffer &&commandBuffer);
    angle::Result flushCommandGraph(vk::PrimaryCommandBuffer *commandBatch);
    void memoryBarrierImpl(GLbitfield barriers, VkPipelineStageFlags stageMask);

    angle::Result synchronizeCpuGpuTime();
    angle::Result traceGpuEventImpl(vk::PrimaryCommandBuffer *commandBuffer,
                                    char phase,
                                    const char *name);
    angle::Result checkCompletedGpuEvents();
    void flushGpuEvents(double nextSyncGpuTimestampS, double nextSyncCpuTimestampS);
    void handleDeviceLost();
    void waitForSwapchainImageIfNecessary();
    bool shouldEmulateSeamfulCubeMapSampling() const;
    bool shouldUseOldRewriteStructSamplers() const;
    void clearAllGarbage();
    angle::Result ensureSubmitFenceInitialized();
    angle::Result startPrimaryCommandBuffer();
    bool hasRecordedCommands();

    std::array<DirtyBitHandler, DIRTY_BIT_MAX> mGraphicsDirtyBitHandlers;
    std::array<DirtyBitHandler, DIRTY_BIT_MAX> mComputeDirtyBitHandlers;

    vk::PipelineHelper *mCurrentGraphicsPipeline;
    vk::PipelineAndSerial *mCurrentComputePipeline;
    gl::PrimitiveMode mCurrentDrawMode;

    WindowSurfaceVk *mCurrentWindowSurface;

    // Keep a cached pipeline description structure that can be used to query the pipeline cache.
    // Kept in a pointer so allocations can be aligned, and structs can be portably packed.
    std::unique_ptr<vk::GraphicsPipelineDesc> mGraphicsPipelineDesc;
    vk::GraphicsPipelineTransitionBits mGraphicsPipelineTransition;

    // These pools are externally sychronized, so cannot be accessed from different
    // threads simultaneously. Hence, we keep them in the ContextVk instead of the RendererVk.
    // Note that this implementation would need to change in shared resource scenarios. Likely
    // we'd instead share a single set of pools between the share groups.
    vk::DynamicDescriptorPool mDriverUniformsDescriptorPool;
    angle::PackedEnumMap<gl::QueryType, vk::DynamicQueryPool> mQueryPools;

    // Dirty bits.
    DirtyBits mGraphicsDirtyBits;
    DirtyBits mComputeDirtyBits;
    DirtyBits mNonIndexedDirtyBitsMask;
    DirtyBits mIndexedDirtyBitsMask;
    DirtyBits mNewGraphicsCommandBufferDirtyBits;
    DirtyBits mNewComputeCommandBufferDirtyBits;

    // Cached back-end objects.
    VertexArrayVk *mVertexArray;
    FramebufferVk *mDrawFramebuffer;
    ProgramVk *mProgram;

    // Graph resource used to record dispatch commands and hold resource dependencies.
    vk::DispatchHelper mDispatcher;

    // The offset we had the last time we bound the index buffer.
    const GLvoid *mLastIndexBufferOffset;
    gl::DrawElementsType mCurrentDrawElementsType;

    // Cache the current draw call's firstVertex to be passed to
    // TransformFeedbackVk::getBufferOffsets.  Unfortunately, gl_BaseVertex support in Vulkan is
    // not yet ubiquitous, which would have otherwise removed the need for this value to be passed
    // as a uniform.
    GLint mXfbBaseVertex;
    // Cache the current draw call's vertex count as well to support instanced draw calls
    GLuint mXfbVertexCountPerInstance;

    // Cached clear value/mask for color and depth/stencil.
    VkClearValue mClearColorValue;
    VkClearValue mClearDepthStencilValue;
    VkColorComponentFlags mClearColorMask;

    IncompleteTextureSet mIncompleteTextures;

    // If the current surface bound to this context wants to have all rendering flipped vertically.
    // Updated on calls to onMakeCurrent.
    bool mFlipYForCurrentSurface;
    bool mFlipViewportForDrawFramebuffer;
    bool mFlipViewportForReadFramebuffer;

    // If any host-visible buffer is written by the GPU since last submission, a barrier is inserted
    // at the end of the command buffer to make that write available to the host.
    bool mIsAnyHostVisibleBufferWritten;

    // Whether this context should do seamful cube map sampling emulation.
    bool mEmulateSeamfulCubeMapSampling;

    // Whether this context should use the old version of the
    // RewriteStructSamplers pass.
    bool mUseOldRewriteStructSamplers;

    angle::PackedEnumMap<PipelineType, DriverUniformsDescriptorSet> mDriverUniforms;

    // This cache should also probably include the texture index (shader location) and array
    // index (also in the shader). This info is used in the descriptor update step.
    gl::ActiveTextureArray<vk::TextureUnit> mActiveTextures;
    vk::TextureDescriptorDesc mActiveTexturesDesc;

    gl::ActiveTextureArray<TextureVk *> mActiveImages;

    // "Current Value" aka default vertex attribute state.
    gl::AttributesMask mDirtyDefaultAttribsMask;
    gl::AttribArray<vk::DynamicBuffer> mDefaultAttribBuffers;

    // We use a single pool for recording commands. We also keep a free list for pool recycling.
    vk::CommandPool mCommandPool;

    CommandQueue mCommandQueue;
    vk::GarbageList mCurrentGarbage;

    RenderPassCache mRenderPassCache;

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

    // When the command graph is disabled we record commands completely linearly. We have plans to
    // reorder independent draws so that we can create fewer RenderPasses in some scenarios.
    OutsideRenderPassCommandBuffer mOutsideRenderPassCommands;
    RenderPassCommandBuffer mRenderPassCommands;
    vk::PrimaryCommandBuffer mPrimaryCommands;

    // Internal shader library.
    vk::ShaderLibrary mShaderLibrary;
    UtilsVk mUtils;

    bool mGpuEventsEnabled;
    vk::DynamicQueryPool mGpuEventQueryPool;
    // A list of queries that have yet to be turned into an event (their result is not yet
    // available).
    std::vector<GpuEventQuery> mInFlightGpuEventQueries;
    // A list of gpu events since the last clock sync.
    std::vector<GpuEvent> mGpuEvents;

    // Semaphores that must be waited on in the next submission.
    std::vector<VkSemaphore> mWaitSemaphores;
    std::vector<VkPipelineStageFlags> mWaitSemaphoreStageMasks;

    // Hold information from the last gpu clock sync for future gpu-to-cpu timestamp conversions.
    GpuClockSyncInfo mGpuClockSync;

    // The very first timestamp queried for a GPU event is used as origin, so event timestamps would
    // have a value close to zero, to avoid losing 12 bits when converting these 64 bit values to
    // double.
    uint64_t mGpuEventTimestampOrigin;

    // Generator for texure serials.
    SerialFactory mTextureSerialFactory;

    gl::State::DirtyBits mPipelineDirtyBitsMask;

    // List of all resources currently being used by this ContextVk's recorded commands.
    vk::ResourceUseList mResourceUseList;

    egl::ContextPriority mContextPriority;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CONTEXTVK_H_
