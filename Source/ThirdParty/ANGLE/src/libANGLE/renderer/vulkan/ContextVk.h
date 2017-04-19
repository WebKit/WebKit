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

    // Flush and finish.
    gl::Error flush() override;
    gl::Error finish() override;

    // Drawing methods.
    gl::Error drawArrays(GLenum mode, GLint first, GLsizei count) override;
    gl::Error drawArraysInstanced(GLenum mode,
                                  GLint first,
                                  GLsizei count,
                                  GLsizei instanceCount) override;

    gl::Error drawElements(GLenum mode,
                           GLsizei count,
                           GLenum type,
                           const GLvoid *indices,
                           const gl::IndexRange &indexRange) override;
    gl::Error drawElementsInstanced(GLenum mode,
                                    GLsizei count,
                                    GLenum type,
                                    const GLvoid *indices,
                                    GLsizei instances,
                                    const gl::IndexRange &indexRange) override;
    gl::Error drawRangeElements(GLenum mode,
                                GLuint start,
                                GLuint end,
                                GLsizei count,
                                GLenum type,
                                const GLvoid *indices,
                                const gl::IndexRange &indexRange) override;
    gl::Error drawArraysIndirect(GLenum mode, const GLvoid *indirect) override;
    gl::Error drawElementsIndirect(GLenum mode, GLenum type, const GLvoid *indirect) override;

    // Device loss
    GLenum getResetStatus() override;

    // Vendor and description strings.
    std::string getVendorString() const override;
    std::string getRendererDescription() const override;

    // Debug markers.
    void insertEventMarker(GLsizei length, const char *marker) override;
    void pushGroupMarker(GLsizei length, const char *marker) override;
    void popGroupMarker() override;

    // State sync with dirty bits.
    void syncState(const gl::State::DirtyBits &dirtyBits) override;

    // Disjoint timer queries
    GLint getGPUDisjoint() override;
    GLint64 getTimestamp() override;

    // Context switching
    void onMakeCurrent(const gl::ContextState &data) override;

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
    FenceSyncImpl *createFenceSync() override;

    // Transform Feedback creation
    TransformFeedbackImpl *createTransformFeedback(
        const gl::TransformFeedbackState &state) override;

    // Sampler object creation
    SamplerImpl *createSampler() override;

    // Path object creation
    std::vector<PathImpl *> createPaths(GLsizei) override;

    VkDevice getDevice() const;
    vk::CommandBuffer *getCommandBuffer();
    vk::Error submitCommands(const vk::CommandBuffer &commandBuffer);

    RendererVk *getRenderer() { return mRenderer; }

    // TODO(jmadill): Use pipeline cache.
    void invalidateCurrentPipeline();

    gl::Error dispatchCompute(GLuint numGroupsX, GLuint numGroupsY, GLuint numGroupsZ) override;

  private:
    gl::Error initPipeline();

    RendererVk *mRenderer;
    vk::Pipeline mCurrentPipeline;
    GLenum mCurrentDrawMode;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CONTEXTVK_H_
