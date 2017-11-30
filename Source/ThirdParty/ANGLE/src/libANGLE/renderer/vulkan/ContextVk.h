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

#include <vulkan/vulkan.h>

#include "libANGLE/renderer/ContextImpl.h"
#include "libANGLE/renderer/vulkan/renderervk_utils.h"

namespace rx
{
class RendererVk;

class ContextVk : public ContextImpl, public ResourceVk
{
  public:
    ContextVk(const gl::ContextState &state, RendererVk *renderer);
    ~ContextVk() override;

    gl::Error initialize() override;

    void onDestroy(const gl::Context *context) override;

    // Flush and finish.
    gl::Error flush(const gl::Context *context) override;
    gl::Error finish(const gl::Context *context) override;

    // Drawing methods.
    gl::Error drawArrays(const gl::Context *context,
                         GLenum mode,
                         GLint first,
                         GLsizei count) override;
    gl::Error drawArraysInstanced(const gl::Context *context,
                                  GLenum mode,
                                  GLint first,
                                  GLsizei count,
                                  GLsizei instanceCount) override;

    gl::Error drawElements(const gl::Context *context,
                           GLenum mode,
                           GLsizei count,
                           GLenum type,
                           const void *indices) override;
    gl::Error drawElementsInstanced(const gl::Context *context,
                                    GLenum mode,
                                    GLsizei count,
                                    GLenum type,
                                    const void *indices,
                                    GLsizei instances) override;
    gl::Error drawRangeElements(const gl::Context *context,
                                GLenum mode,
                                GLuint start,
                                GLuint end,
                                GLsizei count,
                                GLenum type,
                                const void *indices) override;
    gl::Error drawArraysIndirect(const gl::Context *context,
                                 GLenum mode,
                                 const void *indirect) override;
    gl::Error drawElementsIndirect(const gl::Context *context,
                                   GLenum mode,
                                   GLenum type,
                                   const void *indirect) override;

    // Device loss
    GLenum getResetStatus() override;

    // Vendor and description strings.
    std::string getVendorString() const override;
    std::string getRendererDescription() const override;

    // EXT_debug_marker
    void insertEventMarker(GLsizei length, const char *marker) override;
    void pushGroupMarker(GLsizei length, const char *marker) override;
    void popGroupMarker() override;

    // KHR_debug
    void pushDebugGroup(GLenum source, GLuint id, GLsizei length, const char *message) override;
    void popDebugGroup() override;

    // State sync with dirty bits.
    void syncState(const gl::Context *context, const gl::State::DirtyBits &dirtyBits) override;

    // Disjoint timer queries
    GLint getGPUDisjoint() override;
    GLint64 getTimestamp() override;

    // Context switching
    void onMakeCurrent(const gl::Context *context) override;

    // Native capabilities, unmodified by gl::Context.
    const gl::Caps &getNativeCaps() const override;
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
    RenderbufferImpl *createRenderbuffer() override;

    // Buffer creation
    BufferImpl *createBuffer(const gl::BufferState &state) override;

    // Vertex Array creation
    VertexArrayImpl *createVertexArray(const gl::VertexArrayState &state) override;

    // Query and Fence creation
    QueryImpl *createQuery(GLenum type) override;
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

    VkDevice getDevice() const;
    vk::Error getStartedCommandBuffer(vk::CommandBufferAndState **commandBufferOut);
    vk::Error submitCommands(vk::CommandBufferAndState *commandBuffer);

    RendererVk *getRenderer() { return mRenderer; }

    // TODO(jmadill): Use pipeline cache.
    void invalidateCurrentPipeline();

    gl::Error dispatchCompute(const gl::Context *context,
                              GLuint numGroupsX,
                              GLuint numGroupsY,
                              GLuint numGroupsZ) override;

    vk::DescriptorPool *getDescriptorPool();

  private:
    gl::Error initPipeline(const gl::Context *context);
    gl::Error setupDraw(const gl::Context *context, GLenum mode);

    RendererVk *mRenderer;
    vk::Pipeline mCurrentPipeline;
    GLenum mCurrentDrawMode;

    // Keep CreateInfo structures cached so that we can quickly update them when creating
    // updated pipelines. When we move to a pipeline cache, we will want to use a more compact
    // structure that we can use to query the pipeline cache in the Renderer.
    // TODO(jmadill): Update this when we move to a pipeline cache.
    VkPipelineShaderStageCreateInfo mCurrentShaderStages[2];
    VkPipelineVertexInputStateCreateInfo mCurrentVertexInputState;
    VkPipelineInputAssemblyStateCreateInfo mCurrentInputAssemblyState;
    VkViewport mCurrentViewportVk;
    VkRect2D mCurrentScissorVk;
    VkPipelineViewportStateCreateInfo mCurrentViewportState;
    VkPipelineRasterizationStateCreateInfo mCurrentRasterState;
    VkPipelineMultisampleStateCreateInfo mCurrentMultisampleState;
    VkPipelineColorBlendAttachmentState mCurrentBlendAttachmentState;
    VkPipelineColorBlendStateCreateInfo mCurrentBlendState;
    VkGraphicsPipelineCreateInfo mCurrentPipelineInfo;

    // The descriptor pool is externally sychronized, so cannot be accessed from different threads
    // simulataneously. Hence, we keep it in the ContextVk instead of the RendererVk.
    vk::DescriptorPool mDescriptorPool;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CONTEXTVK_H_
