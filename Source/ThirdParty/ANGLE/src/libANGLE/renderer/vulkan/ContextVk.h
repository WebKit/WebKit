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

#include <condition_variable>

#include "common/PackedEnums.h"
#include "common/vulkan/vk_headers.h"
#include "libANGLE/renderer/ContextImpl.h"
#include "libANGLE/renderer/renderer_utils.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
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
class ProgramExecutableVk;
class RendererVk;
class WindowSurfaceVk;
class ShareGroupVk;

static constexpr uint32_t kMaxGpuEventNameLen = 32;
using EventName                               = std::array<char, kMaxGpuEventNameLen>;

// If the total size of copyBufferToImage commands in the outside command buffer reaches the
// threshold below, the latter is flushed.
static constexpr VkDeviceSize kMaxBufferToImageCopySize = 1 << 28;

using ContextVkDescriptorSetList = angle::PackedEnumMap<PipelineType, uint32_t>;

struct ContextVkPerfCounters
{
    ContextVkDescriptorSetList descriptorSetsAllocated;
};

enum class GraphicsEventCmdBuf
{
    NotInQueryCmd              = 0,
    InOutsideCmdBufQueryCmd    = 1,
    InRenderPassCmdBufQueryCmd = 2,

    InvalidEnum = 3,
    EnumCount   = 3,
};

enum class QueueSubmitType
{
    PerformQueueSubmit,
    SkipQueueSubmit,
};

class UpdateDescriptorSetsBuilder final : angle::NonCopyable
{
  public:
    UpdateDescriptorSetsBuilder();
    ~UpdateDescriptorSetsBuilder();

    VkDescriptorBufferInfo *allocDescriptorBufferInfos(size_t count);
    VkDescriptorImageInfo *allocDescriptorImageInfos(size_t count);
    VkWriteDescriptorSet *allocWriteDescriptorSets(size_t count);

    VkDescriptorBufferInfo &allocDescriptorBufferInfo() { return *allocDescriptorBufferInfos(1); }
    VkDescriptorImageInfo &allocDescriptorImageInfo() { return *allocDescriptorImageInfos(1); }
    VkWriteDescriptorSet &allocWriteDescriptorSet() { return *allocWriteDescriptorSets(1); }

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
};

enum class SubmitFrameType
{
    OutsideAndRPCommands,
    OutsideRPCommandsOnly,
};

class ContextVk : public ContextImpl, public vk::Context, public MultisampleTextureInitializer
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

    angle::Result multiDrawArrays(const gl::Context *context,
                                  gl::PrimitiveMode mode,
                                  const GLint *firsts,
                                  const GLsizei *counts,
                                  GLsizei drawcount) override;
    angle::Result multiDrawArraysInstanced(const gl::Context *context,
                                           gl::PrimitiveMode mode,
                                           const GLint *firsts,
                                           const GLsizei *counts,
                                           const GLsizei *instanceCounts,
                                           GLsizei drawcount) override;
    angle::Result multiDrawArraysIndirect(const gl::Context *context,
                                          gl::PrimitiveMode mode,
                                          const void *indirect,
                                          GLsizei drawcount,
                                          GLsizei stride) override;
    angle::Result multiDrawElements(const gl::Context *context,
                                    gl::PrimitiveMode mode,
                                    const GLsizei *counts,
                                    gl::DrawElementsType type,
                                    const GLvoid *const *indices,
                                    GLsizei drawcount) override;
    angle::Result multiDrawElementsInstanced(const gl::Context *context,
                                             gl::PrimitiveMode mode,
                                             const GLsizei *counts,
                                             gl::DrawElementsType type,
                                             const GLvoid *const *indices,
                                             const GLsizei *instanceCounts,
                                             GLsizei drawcount) override;
    angle::Result multiDrawElementsIndirect(const gl::Context *context,
                                            gl::PrimitiveMode mode,
                                            gl::DrawElementsType type,
                                            const void *indirect,
                                            GLsizei drawcount,
                                            GLsizei stride) override;
    angle::Result multiDrawArraysInstancedBaseInstance(const gl::Context *context,
                                                       gl::PrimitiveMode mode,
                                                       const GLint *firsts,
                                                       const GLsizei *counts,
                                                       const GLsizei *instanceCounts,
                                                       const GLuint *baseInstances,
                                                       GLsizei drawcount) override;
    angle::Result multiDrawElementsInstancedBaseVertexBaseInstance(const gl::Context *context,
                                                                   gl::PrimitiveMode mode,
                                                                   const GLsizei *counts,
                                                                   gl::DrawElementsType type,
                                                                   const GLvoid *const *indices,
                                                                   const GLsizei *instanceCounts,
                                                                   const GLint *baseVertices,
                                                                   const GLuint *baseInstances,
                                                                   GLsizei drawcount) override;

    // MultiDrawIndirect helper functions
    angle::Result multiDrawElementsIndirectHelper(const gl::Context *context,
                                                  gl::PrimitiveMode mode,
                                                  gl::DrawElementsType type,
                                                  const void *indirect,
                                                  GLsizei drawcount,
                                                  GLsizei stride);
    angle::Result multiDrawArraysIndirectHelper(const gl::Context *context,
                                                gl::PrimitiveMode mode,
                                                const void *indirect,
                                                GLsizei drawcount,
                                                GLsizei stride);

    // ShareGroup
    ShareGroupVk *getShareGroupVk() { return mShareGroupVk; }
    PipelineLayoutCache &getPipelineLayoutCache()
    {
        return mShareGroupVk->getPipelineLayoutCache();
    }
    DescriptorSetLayoutCache &getDescriptorSetLayoutCache()
    {
        return mShareGroupVk->getDescriptorSetLayoutCache();
    }

    // Device loss
    gl::GraphicsResetStatus getResetStatus() override;

    // EXT_debug_marker
    angle::Result insertEventMarker(GLsizei length, const char *marker) override;
    angle::Result pushGroupMarker(GLsizei length, const char *marker) override;
    angle::Result popGroupMarker() override;

    void insertEventMarkerImpl(GLenum source, const char *marker);

    // KHR_debug
    angle::Result pushDebugGroup(const gl::Context *context,
                                 GLenum source,
                                 GLuint id,
                                 const std::string &message) override;
    angle::Result popDebugGroup(const gl::Context *context) override;

    // Record GL API calls for debuggers
    void logEvent(const char *eventString);
    void endEventLog(angle::EntryPoint entryPoint, PipelineType pipelineType);
    void endEventLogForClearOrQuery();

    bool isViewportFlipEnabledForDrawFBO() const;
    bool isViewportFlipEnabledForReadFBO() const;
    // When the device/surface is rotated such that the surface's aspect ratio is different than
    // the native device (e.g. 90 degrees), the width and height of the viewport, scissor, and
    // render area must be swapped.
    bool isRotatedAspectRatioForDrawFBO() const;
    bool isRotatedAspectRatioForReadFBO() const;
    SurfaceRotation getRotationDrawFramebuffer() const;
    SurfaceRotation getRotationReadFramebuffer() const;

    // View port (x, y, w, h) will be determined by a combination of -
    // 1. clip space origin
    // 2. isViewportFlipEnabledForDrawFBO
    // For userdefined FBOs it will be based on the value of isViewportFlipEnabledForDrawFBO.
    // For default FBOs it will be XOR of ClipOrigin and isViewportFlipEnabledForDrawFBO.
    // isYFlipEnabledForDrawFBO indicates the rendered image is upside-down.
    ANGLE_INLINE bool isYFlipEnabledForDrawFBO() const
    {
        return mState.getClipSpaceOrigin() == gl::ClipSpaceOrigin::UpperLeft
                   ? !isViewportFlipEnabledForDrawFBO()
                   : isViewportFlipEnabledForDrawFBO();
    }

    // State sync with dirty bits.
    angle::Result syncState(const gl::Context *context,
                            const gl::State::DirtyBits &dirtyBits,
                            const gl::State::DirtyBits &bitMask,
                            gl::Command command) override;

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

    ANGLE_INLINE void invalidateTexture(gl::TextureType target) override {}

    // EXT_shader_framebuffer_fetch_non_coherent
    void framebufferFetchBarrier() override;

    // KHR_blend_equation_advanced
    void blendBarrier() override;

    // GL_ANGLE_vulkan_image
    angle::Result acquireTextures(const gl::Context *context,
                                  const gl::TextureBarrierVector &textureBarriers) override;
    angle::Result releaseTextures(const gl::Context *context,
                                  gl::TextureBarrierVector *textureBarriers) override;

    VkDevice getDevice() const;
    egl::ContextPriority getPriority() const { return mContextPriority; }
    bool hasProtectedContent() const { return mState.hasProtectedContent(); }

    ANGLE_INLINE const angle::FeaturesVk &getFeatures() const { return mRenderer->getFeatures(); }

    ANGLE_INLINE void invalidateVertexAndIndexBuffers()
    {
        mGraphicsDirtyBits |= kIndexAndVertexDirtyBits;
    }

    angle::Result onVertexBufferChange(const vk::BufferHelper *vertexBuffer);

    angle::Result onVertexAttributeChange(size_t attribIndex,
                                          GLuint stride,
                                          GLuint divisor,
                                          angle::FormatID format,
                                          bool compressed,
                                          GLuint relativeOffset,
                                          const vk::BufferHelper *vertexBuffer);

    void invalidateDefaultAttribute(size_t attribIndex);
    void invalidateDefaultAttributes(const gl::AttributesMask &dirtyMask);
    angle::Result onFramebufferChange(FramebufferVk *framebufferVk, gl::Command command);
    void onDrawFramebufferRenderPassDescChange(FramebufferVk *framebufferVk,
                                               bool *renderPassDescChangedOut);
    void onHostVisibleBufferWrite() { mIsAnyHostVisibleBufferWritten = true; }

    void invalidateCurrentTransformFeedbackBuffers();
    void onTransformFeedbackStateChanged();
    angle::Result onBeginTransformFeedback(
        size_t bufferCount,
        const gl::TransformFeedbackBuffersArray<vk::BufferHelper *> &buffers,
        const gl::TransformFeedbackBuffersArray<vk::BufferHelper> &counterBuffers);
    void onEndTransformFeedback();
    angle::Result onPauseTransformFeedback();
    void pauseTransformFeedbackIfActiveUnpaused();

    // When UtilsVk issues draw or dispatch calls, it binds a new pipeline and descriptor sets that
    // the context is not aware of.  These functions are called to make sure the pipeline and
    // affected descriptor set bindings are dirtied for the next application draw/dispatch call.
    void invalidateGraphicsPipelineBinding();
    void invalidateComputePipelineBinding();
    void invalidateGraphicsDescriptorSet(DescriptorSetIndex usedDescriptorSet);
    void invalidateComputeDescriptorSet(DescriptorSetIndex usedDescriptorSet);
    void invalidateViewportAndScissor();

    void optimizeRenderPassForPresent(VkFramebuffer framebufferHandle, vk::ImageHelper *colorImage);

    vk::DynamicQueryPool *getQueryPool(gl::QueryType queryType);

    const VkClearValue &getClearColorValue() const;
    const VkClearValue &getClearDepthStencilValue() const;
    gl::BlendStateExt::ColorMaskStorage::Type getClearColorMasks() const;
    const VkRect2D &getScissor() const { return mScissor; }
    angle::Result getIncompleteTexture(const gl::Context *context,
                                       gl::TextureType type,
                                       gl::SamplerFormat format,
                                       gl::Texture **textureOut);
    void updateColorMasks();
    void updateBlendFuncsAndEquations();
    void updateSampleMaskWithRasterizationSamples(const uint32_t rasterizationSamples);

    void handleError(VkResult errorCode,
                     const char *file,
                     const char *function,
                     unsigned int line) override;
    const gl::ActiveTextureArray<vk::TextureUnit> &getActiveTextures() const
    {
        return mActiveTextures;
    }
    const gl::ActiveTextureArray<TextureVk *> &getActiveImages() const { return mActiveImages; }

    angle::Result onIndexBufferChange(const vk::BufferHelper *currentIndexBuffer);

    angle::Result flushImpl(const vk::Semaphore *semaphore,
                            RenderPassClosureReason renderPassClosureReason);
    angle::Result flushAndGetSerial(const vk::Semaphore *semaphore,
                                    Serial *submitSerialOut,
                                    RenderPassClosureReason renderPassClosureReason);
    angle::Result finishImpl(RenderPassClosureReason renderPassClosureReason);

    void addWaitSemaphore(VkSemaphore semaphore, VkPipelineStageFlags stageMask);

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

    vk::ShaderLibrary &getShaderLibrary() { return mShaderLibrary; }
    UtilsVk &getUtils() { return mUtils; }

    angle::Result getTimestamp(uint64_t *timestampOut);

    // Create Begin/End/Instant GPU trace events, which take their timestamps from GPU queries.
    // The events are queued until the query results are available.  Possible values for `phase`
    // are TRACE_EVENT_PHASE_*
    ANGLE_INLINE angle::Result traceGpuEvent(vk::OutsideRenderPassCommandBuffer *commandBuffer,
                                             char phase,
                                             const EventName &name)
    {
        if (mGpuEventsEnabled)
            return traceGpuEventImpl(commandBuffer, phase, name);
        return angle::Result::Continue;
    }

    RenderPassCache &getRenderPassCache() { return mRenderPassCache; }

    vk::DescriptorSetLayoutDesc getDriverUniformsDescriptorSetDesc() const;

    void updateScissor(const gl::State &glState);

    void updateDepthStencil(const gl::State &glState);

    bool emulateSeamfulCubeMapSampling() const { return mEmulateSeamfulCubeMapSampling; }

    const gl::Debug &getDebug() const { return mState.getDebug(); }
    const gl::OverlayType *getOverlay() const { return mState.getOverlay(); }

    vk::ResourceUseList &getResourceUseList() { return mResourceUseList; }

    angle::Result onBufferReleaseToExternal(const vk::BufferHelper &buffer);
    angle::Result onImageReleaseToExternal(const vk::ImageHelper &image);

    void onImageRenderPassRead(VkImageAspectFlags aspectFlags,
                               vk::ImageLayout imageLayout,
                               vk::ImageHelper *image)
    {
        ASSERT(mRenderPassCommands->started());
        mRenderPassCommands->imageRead(this, aspectFlags, imageLayout, image);
    }

    void onImageRenderPassWrite(gl::LevelIndex level,
                                uint32_t layerStart,
                                uint32_t layerCount,
                                VkImageAspectFlags aspectFlags,
                                vk::ImageLayout imageLayout,
                                vk::ImageHelper *image)
    {
        ASSERT(mRenderPassCommands->started());
        mRenderPassCommands->imageWrite(this, level, layerStart, layerCount, aspectFlags,
                                        imageLayout, vk::AliasingMode::Allowed, image);
    }

    void onColorDraw(vk::ImageHelper *image,
                     vk::ImageHelper *resolveImage,
                     vk::PackedAttachmentIndex packedAttachmentIndex)
    {
        ASSERT(mRenderPassCommands->started());
        mRenderPassCommands->colorImagesDraw(&mResourceUseList, image, resolveImage,
                                             packedAttachmentIndex);
    }
    void onDepthStencilDraw(gl::LevelIndex level,
                            uint32_t layerStart,
                            uint32_t layerCount,
                            vk::ImageHelper *image,
                            vk::ImageHelper *resolveImage)
    {
        ASSERT(mRenderPassCommands->started());
        mRenderPassCommands->depthStencilImagesDraw(&mResourceUseList, level, layerStart,
                                                    layerCount, image, resolveImage);
    }

    void finalizeImageLayout(const vk::ImageHelper *image)
    {
        if (mRenderPassCommands->started())
        {
            mRenderPassCommands->finalizeImageLayout(this, image);
        }
    }

    angle::Result getOutsideRenderPassCommandBuffer(
        const vk::CommandBufferAccess &access,
        vk::OutsideRenderPassCommandBuffer **commandBufferOut)
    {
        ANGLE_TRY(onResourceAccess(access));
        *commandBufferOut = &mOutsideRenderPassCommands->getCommandBuffer();
        return angle::Result::Continue;
    }

    angle::Result submitStagedTextureUpdates()
    {
        // Staged updates are recorded in outside RP cammand buffer, submit them.
        return flushOutsideRenderPassCommands();
    }

    angle::Result beginNewRenderPass(const vk::Framebuffer &framebuffer,
                                     const gl::Rectangle &renderArea,
                                     const vk::RenderPassDesc &renderPassDesc,
                                     const vk::AttachmentOpsArray &renderPassAttachmentOps,
                                     const vk::PackedAttachmentCount colorAttachmentCount,
                                     const vk::PackedAttachmentIndex depthStencilAttachmentIndex,
                                     const vk::PackedClearValuesArray &clearValues,
                                     vk::RenderPassCommandBuffer **commandBufferOut);

    // Only returns true if we have a started RP and we've run setupDraw.
    bool hasStartedRenderPass() const
    {
        // Checking mRenderPassCommandBuffer ensures we've called setupDraw.
        return mRenderPassCommandBuffer && mRenderPassCommands->started();
    }

    bool hasStartedRenderPassWithFramebuffer(vk::Framebuffer *framebuffer)
    {
        return hasStartedRenderPass() &&
               mRenderPassCommands->getFramebufferHandle() == framebuffer->getHandle();
    }

    bool hasStartedRenderPassWithCommands() const
    {
        return hasStartedRenderPass() && !mRenderPassCommands->getCommandBuffer().empty();
    }

    vk::RenderPassCommandBufferHelper &getStartedRenderPassCommands()
    {
        ASSERT(mRenderPassCommands->started());
        return *mRenderPassCommands;
    }

    // TODO(https://anglebug.com/4968): Support multiple open render passes.
    void restoreFinishedRenderPass(vk::Framebuffer *framebuffer);

    uint32_t getCurrentSubpassIndex() const;
    uint32_t getCurrentViewCount() const;

    egl::ContextPriority getContextPriority() const override { return mContextPriority; }
    angle::Result startRenderPass(gl::Rectangle renderArea,
                                  vk::RenderPassCommandBuffer **commandBufferOut,
                                  bool *renderPassDescChangedOut);
    angle::Result startNextSubpass();
    angle::Result flushCommandsAndEndRenderPass(RenderPassClosureReason reason);
    angle::Result flushCommandsAndEndRenderPassWithoutQueueSubmit(RenderPassClosureReason reason);
    angle::Result submitOutsideRenderPassCommandsImpl();

    angle::Result syncExternalMemory();

    void addCommandBufferDiagnostics(const std::string &commandBufferDiagnostics);

    VkIndexType getVkIndexType(gl::DrawElementsType glIndexType) const;
    size_t getVkIndexTypeSize(gl::DrawElementsType glIndexType) const;
    bool shouldConvertUint8VkIndexType(gl::DrawElementsType glIndexType) const;

    ANGLE_INLINE bool isBresenhamEmulationEnabled(const gl::PrimitiveMode mode)
    {
        return getFeatures().basicGLLineRasterization.enabled && gl::IsLineMode(mode);
    }

    ProgramExecutableVk *getExecutable() const;

    bool isRobustResourceInitEnabled() const;

    // Queries that begin and end automatically with render pass start and end
    angle::Result beginRenderPassQuery(QueryVk *queryVk);
    angle::Result endRenderPassQuery(QueryVk *queryVk);
    void pauseRenderPassQueriesIfActive();
    angle::Result resumeRenderPassQueriesIfActive();
    angle::Result resumeXfbRenderPassQueriesIfActive();
    bool doesPrimitivesGeneratedQuerySupportRasterizerDiscard() const;
    bool isEmulatingRasterizerDiscardDuringPrimitivesGeneratedQuery(
        bool isPrimitivesGeneratedQueryActive) const;

    // Used by QueryVk to share query helpers between transform feedback queries.
    QueryVk *getActiveRenderPassQuery(gl::QueryType queryType) const;

    void syncObjectPerfCounters();
    void updateOverlayOnPresent();
    void addOverlayUsedBuffersCount(vk::CommandBufferHelperCommon *commandBuffer);

    // For testing only.
    void setDefaultUniformBlocksMinSizeForTesting(size_t minSize);

    vk::BufferHelper &getEmptyBuffer() { return mEmptyBuffer; }

    // Keeping track of the buffer copy size. Used to determine when to submit the outside command
    // buffer.
    angle::Result onCopyUpdate(VkDeviceSize size);
    void resetTotalBufferToImageCopySize() { mTotalBufferToImageCopySize = 0; }
    VkDeviceSize getTotalBufferToImageCopySize() const { return mTotalBufferToImageCopySize; }

    // Implementation of MultisampleTextureInitializer
    angle::Result initializeMultisampleTextureToBlack(const gl::Context *context,
                                                      gl::Texture *glTexture) override;

    // TODO(http://anglebug.com/5624): rework updateActiveTextures(), createPipelineLayout(),
    // handleDirtyGraphicsPipeline(), and ProgramPipelineVk::link().
    void resetCurrentGraphicsPipeline() { mCurrentGraphicsPipeline = nullptr; }

    void onProgramExecutableReset(ProgramExecutableVk *executableVk);

    angle::Result handleGraphicsEventLog(GraphicsEventCmdBuf queryEventType);

    void flushDescriptorSetUpdates();

    vk::BufferPool *getDefaultBufferPool(VkDeviceSize size, uint32_t memoryTypeIndex)
    {
        return mShareGroupVk->getDefaultBufferPool(mRenderer, size, memoryTypeIndex);
    }

    angle::Result allocateStreamedVertexBuffer(size_t attribIndex,
                                               size_t bytesToAllocate,
                                               vk::BufferHelper **vertexBufferOut)
    {
        bool newBufferOut;
        ANGLE_TRY(mStreamedVertexBuffers[attribIndex].allocate(this, bytesToAllocate,
                                                               vertexBufferOut, &newBufferOut));
        if (newBufferOut)
        {
            mHasInFlightStreamedVertexBuffers.set(attribIndex);
        }
        return angle::Result::Continue;
    }

    const angle::PerfMonitorCounterGroups &getPerfMonitorCounters() override;

  private:
    // Dirty bits.
    enum DirtyBitType : size_t
    {
        // A glMemoryBarrier has been called and command buffers may need flushing.
        DIRTY_BIT_MEMORY_BARRIER,
        // Dirty bits that must be processed before the render pass is started.  The handlers for
        // these dirty bits don't record any commands.
        DIRTY_BIT_DEFAULT_ATTRIBS,
        // The pipeline has changed and needs to be recreated.  This dirty bit may close the render
        // pass.
        DIRTY_BIT_PIPELINE_DESC,

        // Start the render pass.
        DIRTY_BIT_RENDER_PASS,

        // Dirty bits that must be processed after the render pass is started.  Their handlers
        // record commands.
        DIRTY_BIT_EVENT_LOG,
        // Pipeline needs to rebind because a new command buffer has been allocated, or UtilsVk has
        // changed the binding.  The pipeline itself doesn't need to be recreated.
        DIRTY_BIT_PIPELINE_BINDING,
        DIRTY_BIT_TEXTURES,
        DIRTY_BIT_VERTEX_BUFFERS,
        DIRTY_BIT_INDEX_BUFFER,
        DIRTY_BIT_DRIVER_UNIFORMS,
        DIRTY_BIT_DRIVER_UNIFORMS_BINDING,
        // Shader resources excluding textures, which are handled separately.
        DIRTY_BIT_SHADER_RESOURCES,
        DIRTY_BIT_TRANSFORM_FEEDBACK_BUFFERS,
        DIRTY_BIT_TRANSFORM_FEEDBACK_RESUME,
        DIRTY_BIT_DESCRIPTOR_SETS,
        DIRTY_BIT_FRAMEBUFFER_FETCH_BARRIER,
        DIRTY_BIT_BLEND_BARRIER,
        // Dynamic viewport/scissor
        DIRTY_BIT_VIEWPORT,
        DIRTY_BIT_SCISSOR,
        DIRTY_BIT_MAX,
    };

    using DirtyBits = angle::BitSet<DIRTY_BIT_MAX>;

    using GraphicsDirtyBitHandler = angle::Result (
        ContextVk::*)(DirtyBits::Iterator *dirtyBitsIterator, DirtyBits dirtyBitMask);
    using ComputeDirtyBitHandler = angle::Result (ContextVk::*)();

    struct DriverUniformsDescriptorSet
    {
        vk::DynamicBuffer dynamicBuffer;
        VkDescriptorSet descriptorSet;
        vk::BufferHelper *currentBuffer;
        vk::BindingPointer<vk::DescriptorSetLayout> descriptorSetLayout;
        vk::RefCountedDescriptorPoolBinding descriptorPoolBinding;
        DriverUniformsDescriptorSetCache descriptorSetCache;

        DriverUniformsDescriptorSet();
        ~DriverUniformsDescriptorSet();

        void init(RendererVk *rendererVk);
        void destroy(RendererVk *rendererVk);
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
        EventName name;
        char phase;
        vk::QueryHelper queryHelper;
    };

    // Once a query result is available, the timestamp is read and a GpuEvent object is kept until
    // the next clock sync, at which point the clock drift is compensated in the results before
    // handing them off to the application.
    struct GpuEvent final
    {
        uint64_t gpuTimestampCycles;
        std::array<char, kMaxGpuEventNameLen> name;
        char phase;
    };

    struct GpuClockSyncInfo
    {
        double gpuTimestampS;
        double cpuTimestampS;
    };

    class ScopedDescriptorSetUpdates;

    angle::Result setupDraw(const gl::Context *context,
                            gl::PrimitiveMode mode,
                            GLint firstVertexOrInvalid,
                            GLsizei vertexOrIndexCount,
                            GLsizei instanceCount,
                            gl::DrawElementsType indexTypeOrInvalid,
                            const void *indices,
                            DirtyBits dirtyBitMask);

    angle::Result setupIndexedDraw(const gl::Context *context,
                                   gl::PrimitiveMode mode,
                                   GLsizei indexCount,
                                   GLsizei instanceCount,
                                   gl::DrawElementsType indexType,
                                   const void *indices);
    angle::Result setupIndirectDraw(const gl::Context *context,
                                    gl::PrimitiveMode mode,
                                    DirtyBits dirtyBitMask,
                                    vk::BufferHelper *indirectBuffer);
    angle::Result setupIndexedIndirectDraw(const gl::Context *context,
                                           gl::PrimitiveMode mode,
                                           gl::DrawElementsType indexType,
                                           vk::BufferHelper *indirectBuffer);

    angle::Result setupLineLoopIndexedIndirectDraw(const gl::Context *context,
                                                   gl::PrimitiveMode mode,
                                                   gl::DrawElementsType indexType,
                                                   vk::BufferHelper *srcIndirectBuf,
                                                   VkDeviceSize indirectBufferOffset,
                                                   vk::BufferHelper **indirectBufferOut);
    angle::Result setupLineLoopIndirectDraw(const gl::Context *context,
                                            gl::PrimitiveMode mode,
                                            vk::BufferHelper *indirectBuffer,
                                            VkDeviceSize indirectBufferOffset,
                                            vk::BufferHelper **indirectBufferOut);

    angle::Result setupLineLoopDraw(const gl::Context *context,
                                    gl::PrimitiveMode mode,
                                    GLint firstVertex,
                                    GLsizei vertexOrIndexCount,
                                    gl::DrawElementsType indexTypeOrInvalid,
                                    const void *indices,
                                    uint32_t *numIndicesOut);

    angle::Result setupDispatch(const gl::Context *context);

    gl::Rectangle getCorrectedViewport(const gl::Rectangle &viewport) const;
    void updateViewport(FramebufferVk *framebufferVk,
                        const gl::Rectangle &viewport,
                        float nearPlane,
                        float farPlane);
    void updateDepthRange(float nearPlane, float farPlane);
    void updateFlipViewportDrawFramebuffer(const gl::State &glState);
    void updateFlipViewportReadFramebuffer(const gl::State &glState);
    void updateSurfaceRotationDrawFramebuffer(const gl::State &glState);
    void updateSurfaceRotationReadFramebuffer(const gl::State &glState);

    angle::Result updateActiveTextures(const gl::Context *context, gl::Command command);
    template <typename CommandBufferHelperT>
    angle::Result updateActiveImages(CommandBufferHelperT *commandBufferHelper);

    ANGLE_INLINE void invalidateCurrentGraphicsPipeline()
    {
        // Note: DIRTY_BIT_PIPELINE_BINDING will be automatically set if pipeline bind is necessary.
        mGraphicsDirtyBits.set(DIRTY_BIT_PIPELINE_DESC);
    }

    ANGLE_INLINE void invalidateCurrentComputePipeline()
    {
        mComputeDirtyBits |= kPipelineDescAndBindingDirtyBits;
        mCurrentComputePipeline = nullptr;
    }

    angle::Result invalidateProgramExecutableHelper(const gl::Context *context);
    angle::Result checkAndUpdateFramebufferFetchStatus(const gl::ProgramExecutable *executable);

    void invalidateCurrentDefaultUniforms();
    angle::Result invalidateCurrentTextures(const gl::Context *context, gl::Command command);
    angle::Result invalidateCurrentShaderResources(gl::Command command);
    void invalidateGraphicsDriverUniforms();
    void invalidateDriverUniforms();

    angle::Result handleNoopDrawEvent() override;

    // Handlers for graphics pipeline dirty bits.
    angle::Result handleDirtyGraphicsMemoryBarrier(DirtyBits::Iterator *dirtyBitsIterator,
                                                   DirtyBits dirtyBitMask);
    angle::Result handleDirtyGraphicsEventLog(DirtyBits::Iterator *dirtyBitsIterator,
                                              DirtyBits dirtyBitMask);
    angle::Result handleDirtyGraphicsDefaultAttribs(DirtyBits::Iterator *dirtyBitsIterator,
                                                    DirtyBits dirtyBitMask);
    angle::Result handleDirtyGraphicsPipelineDesc(DirtyBits::Iterator *dirtyBitsIterator,
                                                  DirtyBits dirtyBitMask);
    angle::Result handleDirtyGraphicsRenderPass(DirtyBits::Iterator *dirtyBitsIterator,
                                                DirtyBits dirtyBitMask);
    angle::Result handleDirtyGraphicsPipelineBinding(DirtyBits::Iterator *dirtyBitsIterator,
                                                     DirtyBits dirtyBitMask);
    angle::Result handleDirtyGraphicsTextures(DirtyBits::Iterator *dirtyBitsIterator,
                                              DirtyBits dirtyBitMask);
    angle::Result handleDirtyGraphicsVertexBuffers(DirtyBits::Iterator *dirtyBitsIterator,
                                                   DirtyBits dirtyBitMask);
    angle::Result handleDirtyGraphicsIndexBuffer(DirtyBits::Iterator *dirtyBitsIterator,
                                                 DirtyBits dirtyBitMask);
    angle::Result handleDirtyGraphicsDriverUniforms(DirtyBits::Iterator *dirtyBitsIterator,
                                                    DirtyBits dirtyBitMask);
    angle::Result handleDirtyGraphicsDriverUniformsBinding(DirtyBits::Iterator *dirtyBitsIterator,
                                                           DirtyBits dirtyBitMask);
    angle::Result handleDirtyGraphicsShaderResources(DirtyBits::Iterator *dirtyBitsIterator,
                                                     DirtyBits dirtyBitMask);
    angle::Result handleDirtyGraphicsFramebufferFetchBarrier(DirtyBits::Iterator *dirtyBitsIterator,
                                                             DirtyBits dirtyBitMask);
    angle::Result handleDirtyGraphicsBlendBarrier(DirtyBits::Iterator *dirtyBitsIterator,
                                                  DirtyBits dirtyBitMask);
    angle::Result handleDirtyGraphicsTransformFeedbackBuffersEmulation(
        DirtyBits::Iterator *dirtyBitsIterator,
        DirtyBits dirtyBitMask);
    angle::Result handleDirtyGraphicsTransformFeedbackBuffersExtension(
        DirtyBits::Iterator *dirtyBitsIterator,
        DirtyBits dirtyBitMask);
    angle::Result handleDirtyGraphicsTransformFeedbackResume(DirtyBits::Iterator *dirtyBitsIterator,
                                                             DirtyBits dirtyBitMask);
    angle::Result handleDirtyGraphicsDescriptorSets(DirtyBits::Iterator *dirtyBitsIterator,
                                                    DirtyBits dirtyBitMask);
    angle::Result handleDirtyGraphicsViewport(DirtyBits::Iterator *dirtyBitsIterator,
                                              DirtyBits dirtyBitMask);
    angle::Result handleDirtyGraphicsScissor(DirtyBits::Iterator *dirtyBitsIterator,
                                             DirtyBits dirtyBitMask);

    // Handlers for compute pipeline dirty bits.
    angle::Result handleDirtyComputeMemoryBarrier();
    angle::Result handleDirtyComputeEventLog();
    angle::Result handleDirtyComputePipelineDesc();
    angle::Result handleDirtyComputePipelineBinding();
    angle::Result handleDirtyComputeTextures();
    angle::Result handleDirtyComputeDriverUniforms();
    angle::Result handleDirtyComputeDriverUniformsBinding();
    angle::Result handleDirtyComputeShaderResources();
    angle::Result handleDirtyComputeDescriptorSets();

    // Common parts of the common dirty bit handlers.
    angle::Result handleDirtyMemoryBarrierImpl(DirtyBits::Iterator *dirtyBitsIterator,
                                               DirtyBits dirtyBitMask);
    template <typename CommandBufferT>
    angle::Result handleDirtyEventLogImpl(CommandBufferT *commandBuffer);
    template <typename CommandBufferHelperT>
    angle::Result handleDirtyTexturesImpl(CommandBufferHelperT *commandBufferHelper,
                                          PipelineType pipelineType);
    template <typename CommandBufferHelperT>
    angle::Result handleDirtyShaderResourcesImpl(CommandBufferHelperT *commandBufferHelper,
                                                 PipelineType pipelineType);
    void handleDirtyShaderBufferResourcesImpl(vk::CommandBufferHelperCommon *commandBufferHelper);
    template <typename CommandBufferT>
    void handleDirtyDriverUniformsBindingImpl(CommandBufferT *commandBuffer,
                                              VkPipelineBindPoint bindPoint,
                                              DriverUniformsDescriptorSet *driverUniforms);
    template <typename CommandBufferT>
    angle::Result handleDirtyDescriptorSetsImpl(CommandBufferT *commandBuffer,
                                                PipelineType pipelineType);
    void handleDirtyGraphicsScissorImpl(bool isPrimitivesGeneratedQueryActive);

    angle::Result allocateDriverUniforms(size_t driverUniformsSize,
                                         DriverUniformsDescriptorSet *driverUniforms,
                                         uint8_t **ptrOut,
                                         bool *newBufferOut);
    angle::Result updateDriverUniformsDescriptorSet(bool newBuffer,
                                                    size_t driverUniformsSize,
                                                    PipelineType pipelineType);

    void writeAtomicCounterBufferDriverUniformOffsets(uint32_t *offsetsOut, size_t offsetsSize);

    angle::Result submitFrame(const vk::Semaphore *signalSemaphore, Serial *submitSerialOut);
    angle::Result submitFrameOutsideCommandBufferOnly(Serial *submitSerialOut);
    angle::Result submitFrameImpl(const vk::Semaphore *signalSemaphore,
                                  Serial *submitSerialOut,
                                  SubmitFrameType submitFrameType);

    angle::Result synchronizeCpuGpuTime();
    angle::Result traceGpuEventImpl(vk::OutsideRenderPassCommandBuffer *commandBuffer,
                                    char phase,
                                    const EventName &name);
    angle::Result checkCompletedGpuEvents();
    void flushGpuEvents(double nextSyncGpuTimestampS, double nextSyncCpuTimestampS);
    void handleDeviceLost();
    bool shouldEmulateSeamfulCubeMapSampling() const;
    void clearAllGarbage();
    void dumpCommandStreamDiagnostics();
    angle::Result flushOutsideRenderPassCommands();
    // Flush commands and end render pass without setting any dirty bits.
    // flushCommandsAndEndRenderPass() and flushDirtyGraphicsRenderPass() will set the dirty bits
    // directly or through the iterator respectively.  Outside those two functions, this shouldn't
    // be called directly.
    angle::Result flushCommandsAndEndRenderPassImpl(QueueSubmitType queueSubmit,
                                                    RenderPassClosureReason reason);
    angle::Result flushDirtyGraphicsRenderPass(DirtyBits::Iterator *dirtyBitsIterator,
                                               DirtyBits dirtyBitMask,
                                               RenderPassClosureReason reason);

    void onRenderPassFinished(RenderPassClosureReason reason);

    void initIndexTypeMap();

    VertexArrayVk *getVertexArray() const;
    FramebufferVk *getDrawFramebuffer() const;
    ProgramVk *getProgram() const;
    ProgramPipelineVk *getProgramPipeline() const;

    // Read-after-write hazards are generally handled with |glMemoryBarrier| when the source of
    // write is storage output.  When the write is outside render pass, the natural placement of the
    // render pass after the current outside render pass commands ensures that the memory barriers
    // and image layout transitions automatically take care of such synchronizations.
    //
    // There are a number of read-after-write cases that require breaking the render pass however to
    // preserve the order of operations:
    //
    // - Transform feedback write (in render pass), then vertex/index read (in render pass)
    // - Transform feedback write (in render pass), then ubo read (outside render pass)
    // - Framebuffer attachment write (in render pass), then texture sample (outside render pass)
    //   * Note that texture sampling inside render pass would cause a feedback loop
    //
    angle::Result endRenderPassIfTransformFeedbackBuffer(const vk::BufferHelper *buffer);
    angle::Result endRenderPassIfComputeReadAfterTransformFeedbackWrite();
    angle::Result endRenderPassIfComputeReadAfterAttachmentWrite();

    void populateTransformFeedbackBufferSet(
        size_t bufferCount,
        const gl::TransformFeedbackBuffersArray<vk::BufferHelper *> &buffers);

    angle::Result updateRenderPassDepthStencilAccess();
    bool shouldSwitchToReadOnlyDepthFeedbackLoopMode(gl::Texture *texture,
                                                     gl::Command command) const;

    angle::Result onResourceAccess(const vk::CommandBufferAccess &access);
    angle::Result flushCommandBuffersIfNecessary(const vk::CommandBufferAccess &access);
    bool renderPassUsesStorageResources() const;

    angle::Result pushDebugGroupImpl(GLenum source, GLuint id, const char *message);
    angle::Result popDebugGroupImpl();

    void outputCumulativePerfCounters();

    void updateSampleShadingWithRasterizationSamples(const uint32_t rasterizationSamples);
    void updateRasterizationSamples(const uint32_t rasterizationSamples);
    void updateRasterizerDiscardEnabled(bool isPrimitivesGeneratedQueryActive);

    void updateDither();

    SpecConstUsageBits getCurrentProgramSpecConstUsageBits() const;
    void updateGraphicsPipelineDescWithSpecConstUsageBits(SpecConstUsageBits usageBits);

    void updateShaderResourcesDescriptorDesc(PipelineType pipelineType);

    ContextVkPerfCounters getAndResetObjectPerfCounters();

    std::array<GraphicsDirtyBitHandler, DIRTY_BIT_MAX> mGraphicsDirtyBitHandlers;
    std::array<ComputeDirtyBitHandler, DIRTY_BIT_MAX> mComputeDirtyBitHandlers;

    vk::RenderPassCommandBuffer *mRenderPassCommandBuffer;

    vk::PipelineHelper *mCurrentGraphicsPipeline;
    vk::PipelineHelper *mCurrentComputePipeline;
    gl::PrimitiveMode mCurrentDrawMode;

    WindowSurfaceVk *mCurrentWindowSurface;
    // Records the current rotation of the surface (draw/read) framebuffer, derived from
    // mCurrentWindowSurface->getPreTransform().
    SurfaceRotation mCurrentRotationDrawFramebuffer;
    SurfaceRotation mCurrentRotationReadFramebuffer;

    // Keep a cached pipeline description structure that can be used to query the pipeline cache.
    // Kept in a pointer so allocations can be aligned, and structs can be portably packed.
    std::unique_ptr<vk::GraphicsPipelineDesc> mGraphicsPipelineDesc;
    vk::GraphicsPipelineTransitionBits mGraphicsPipelineTransition;

    // These pools are externally synchronized, so cannot be accessed from different
    // threads simultaneously. Hence, we keep them in the ContextVk instead of the RendererVk.
    // Note that this implementation would need to change in shared resource scenarios. Likely
    // we'd instead share a single set of pools between the share groups.
    angle::PackedEnumMap<PipelineType, vk::DynamicDescriptorPool> mDriverUniformsDescriptorPools;
    gl::QueryTypeMap<vk::DynamicQueryPool> mQueryPools;

    // Queries that need to be closed and reopened with the render pass:
    //
    // - Occlusion queries
    // - Transform feedback queries, if not emulated
    gl::QueryTypeMap<QueryVk *> mActiveRenderPassQueries;

    // Dirty bits.
    DirtyBits mGraphicsDirtyBits;
    DirtyBits mComputeDirtyBits;
    DirtyBits mNonIndexedDirtyBitsMask;
    DirtyBits mIndexedDirtyBitsMask;
    DirtyBits mNewGraphicsCommandBufferDirtyBits;
    DirtyBits mNewComputeCommandBufferDirtyBits;
    static constexpr DirtyBits kIndexAndVertexDirtyBits{DIRTY_BIT_VERTEX_BUFFERS,
                                                        DIRTY_BIT_INDEX_BUFFER};
    static constexpr DirtyBits kPipelineDescAndBindingDirtyBits{DIRTY_BIT_PIPELINE_DESC,
                                                                DIRTY_BIT_PIPELINE_BINDING};
    static constexpr DirtyBits kTexturesAndDescSetDirtyBits{DIRTY_BIT_TEXTURES,
                                                            DIRTY_BIT_DESCRIPTOR_SETS};
    static constexpr DirtyBits kResourcesAndDescSetDirtyBits{DIRTY_BIT_SHADER_RESOURCES,
                                                             DIRTY_BIT_DESCRIPTOR_SETS};
    static constexpr DirtyBits kXfbBuffersAndDescSetDirtyBits{DIRTY_BIT_TRANSFORM_FEEDBACK_BUFFERS,
                                                              DIRTY_BIT_DESCRIPTOR_SETS};
    static constexpr DirtyBits kDriverUniformsAndBindingDirtyBits{
        DIRTY_BIT_DRIVER_UNIFORMS, DIRTY_BIT_DRIVER_UNIFORMS_BINDING};

    // The offset we had the last time we bound the index buffer.
    const GLvoid *mLastIndexBufferOffset;
    VkDeviceSize mCurrentIndexBufferOffset;
    gl::DrawElementsType mCurrentDrawElementsType;
    angle::PackedEnumMap<gl::DrawElementsType, VkIndexType> mIndexTypeMap;

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
    gl::BlendStateExt::ColorMaskStorage::Type mClearColorMasks;

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

    angle::PackedEnumMap<PipelineType, DriverUniformsDescriptorSet> mDriverUniforms;

    // This cache should also probably include the texture index (shader location) and array
    // index (also in the shader). This info is used in the descriptor update step.
    gl::ActiveTextureArray<vk::TextureUnit> mActiveTextures;

    // We use textureSerial to optimize texture binding updates. Each permutation of a
    // {VkImage/VkSampler} generates a unique serial. These object ids are combined to form a unique
    // signature for each descriptor set. This allows us to keep a cache of descriptor sets and
    // avoid calling vkAllocateDesctiporSets each texture update.
    vk::DescriptorSetDesc mActiveTexturesDesc;

    vk::DescriptorSetDesc mShaderBuffersDescriptorDesc;

    gl::ActiveTextureArray<TextureVk *> mActiveImages;

    // "Current Value" aka default vertex attribute state.
    gl::AttributesMask mDirtyDefaultAttribsMask;

    // DynamicBuffers for streaming vertex data from client memory pointer as well as for default
    // attributes. mHasInFlightStreamedVertexBuffers indicates if the dynamic buffer has any
    // inflight buffer or not that we need to release at submission time.
    gl::AttribArray<vk::DynamicBuffer> mStreamedVertexBuffers;
    gl::AttributesMask mHasInFlightStreamedVertexBuffers;

    // We use a single pool for recording commands. We also keep a free list for pool recycling.
    vk::SecondaryCommandPools mCommandPools;

    vk::GarbageList mCurrentGarbage;

    RenderPassCache mRenderPassCache;

    vk::OutsideRenderPassCommandBufferHelper *mOutsideRenderPassCommands;
    vk::RenderPassCommandBufferHelper *mRenderPassCommands;

    // The following is used when creating debug-util markers for graphics debuggers (e.g. AGI).  A
    // given gl{Begin|End}Query command may result in commands being submitted to the outside or
    // render-pass command buffer.  The ContextVk::handleGraphicsEventLog() method records the
    // appropriate command buffer for use by ContextVk::endEventLogForQuery().  The knowledge of
    // which command buffer to use depends on the particular type of query (e.g. samples
    // vs. timestamp), and is only known by the query code, which is what calls
    // ContextVk::handleGraphicsEventLog().  After all back-end processing of the gl*Query command
    // is complete, the front-end calls ContextVk::endEventLogForQuery(), which needs to know which
    // command buffer to call endDebugUtilsLabelEXT() for.
    GraphicsEventCmdBuf mQueryEventType;

    // Transform feedback buffers.
    angle::FlatUnorderedSet<const vk::BufferHelper *,
                            gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS>
        mCurrentTransformFeedbackBuffers;

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

    // Cached value of the color attachment mask of the current draw framebuffer.  This is used to
    // know which attachment indices have their blend state set in |mGraphicsPipelineDesc|, and
    // subsequently is used to clear the blend state for attachments that no longer exist when a new
    // framebuffer is bound.
    gl::DrawBufferMask mCachedDrawFramebufferColorAttachmentMask;

    bool mHasDeferredFlush;

    // GL_EXT_shader_framebuffer_fetch_non_coherent
    bool mLastProgramUsesFramebufferFetch;

    // The size of copy commands issued between buffers and images. Used to submit the command
    // buffer for the outside render pass.
    VkDeviceSize mTotalBufferToImageCopySize = 0;

    // Semaphores that must be waited on in the next submission.
    std::vector<VkSemaphore> mWaitSemaphores;
    std::vector<VkPipelineStageFlags> mWaitSemaphoreStageMasks;

    // Hold information from the last gpu clock sync for future gpu-to-cpu timestamp conversions.
    GpuClockSyncInfo mGpuClockSync;

    // The very first timestamp queried for a GPU event is used as origin, so event timestamps would
    // have a value close to zero, to avoid losing 12 bits when converting these 64 bit values to
    // double.
    uint64_t mGpuEventTimestampOrigin;

    // A mix of per-frame and per-run counters.
    angle::PerfMonitorCounterGroups mPerfMonitorCounters;
    ContextVkPerfCounters mContextPerfCounters;
    ContextVkPerfCounters mCumulativeContextPerfCounters;

    gl::State::DirtyBits mPipelineDirtyBitsMask;

    // List of all resources currently being used by this ContextVk's recorded commands.
    vk::ResourceUseList mResourceUseList;

    egl::ContextPriority mContextPriority;

    // Storage for vkUpdateDescriptorSets
    UpdateDescriptorSetsBuilder mUpdateDescriptorSetsBuilder;

    ShareGroupVk *mShareGroupVk;

    // This is a special "empty" placeholder buffer for use when we just need a placeholder buffer
    // but not the data. Examples are shader that has no uniform or doesn't use all slots in the
    // atomic counter buffer array, or places where there is no vertex buffer since Vulkan does not
    // allow binding a null vertex buffer.
    vk::BufferHelper mEmptyBuffer;

    // Storage for default uniforms of ProgramVks and ProgramPipelineVks.
    vk::DynamicBuffer mDefaultUniformStorage;

    std::vector<std::string> mCommandBufferDiagnostics;

    // Record GL API calls for debuggers
    std::vector<std::string> mEventLog;

    // Viewport and scissor are handled as dynamic state.
    VkViewport mViewport;
    VkRect2D mScissor;
};

ANGLE_INLINE angle::Result ContextVk::endRenderPassIfTransformFeedbackBuffer(
    const vk::BufferHelper *buffer)
{
    if (!buffer || !mCurrentTransformFeedbackBuffers.contains(buffer))
    {
        return angle::Result::Continue;
    }

    return flushCommandsAndEndRenderPass(RenderPassClosureReason::XfbWriteThenVertexIndexBuffer);
}

ANGLE_INLINE angle::Result ContextVk::onIndexBufferChange(
    const vk::BufferHelper *currentIndexBuffer)
{
    mGraphicsDirtyBits.set(DIRTY_BIT_INDEX_BUFFER);
    mLastIndexBufferOffset = reinterpret_cast<const void *>(angle::DirtyPointer);
    return endRenderPassIfTransformFeedbackBuffer(currentIndexBuffer);
}

ANGLE_INLINE angle::Result ContextVk::onVertexBufferChange(const vk::BufferHelper *vertexBuffer)
{
    mGraphicsDirtyBits.set(DIRTY_BIT_VERTEX_BUFFERS);
    return endRenderPassIfTransformFeedbackBuffer(vertexBuffer);
}

ANGLE_INLINE angle::Result ContextVk::onVertexAttributeChange(size_t attribIndex,
                                                              GLuint stride,
                                                              GLuint divisor,
                                                              angle::FormatID format,
                                                              bool compressed,
                                                              GLuint relativeOffset,
                                                              const vk::BufferHelper *vertexBuffer)
{
    invalidateCurrentGraphicsPipeline();
    // Set divisor to 1 for attribs with emulated divisor
    mGraphicsPipelineDesc->updateVertexInput(
        &mGraphicsPipelineTransition, static_cast<uint32_t>(attribIndex), stride,
        divisor > mRenderer->getMaxVertexAttribDivisor() ? 1 : divisor, format, compressed,
        relativeOffset);
    return onVertexBufferChange(vertexBuffer);
}

ANGLE_INLINE bool UseLineRaster(const ContextVk *contextVk, gl::PrimitiveMode mode)
{
    return contextVk->getFeatures().basicGLLineRasterization.enabled && gl::IsLineMode(mode);
}
}  // namespace rx

// Generate a perf warning, and insert an event marker in the command buffer.
#define ANGLE_VK_PERF_WARNING(contextVk, severity, ...)                         \
    do                                                                          \
    {                                                                           \
        char ANGLE_MESSAGE[100];                                                \
        snprintf(ANGLE_MESSAGE, sizeof(ANGLE_MESSAGE), __VA_ARGS__);            \
        ANGLE_PERF_WARNING(contextVk->getDebug(), severity, ANGLE_MESSAGE);     \
                                                                                \
        contextVk->insertEventMarkerImpl(GL_DEBUG_SOURCE_OTHER, ANGLE_MESSAGE); \
    } while (0)

// Generate a trace event for graphics profiler, and insert an event marker in the command buffer.
#define ANGLE_VK_TRACE_EVENT_AND_MARKER(contextVk, ...)                         \
    do                                                                          \
    {                                                                           \
        char ANGLE_MESSAGE[100];                                                \
        snprintf(ANGLE_MESSAGE, sizeof(ANGLE_MESSAGE), __VA_ARGS__);            \
        ANGLE_TRACE_EVENT0("gpu.angle", ANGLE_MESSAGE);                         \
                                                                                \
        contextVk->insertEventMarkerImpl(GL_DEBUG_SOURCE_OTHER, ANGLE_MESSAGE); \
    } while (0)

#endif  // LIBANGLE_RENDERER_VULKAN_CONTEXTVK_H_
