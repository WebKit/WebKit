//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Context9:
//   D3D9-specific functionality associated with a GL Context.
//

#ifndef LIBANGLE_RENDERER_D3D_D3D9_CONTEXT9_H_
#define LIBANGLE_RENDERER_D3D_D3D9_CONTEXT9_H_

#include <stack>
#include "libANGLE/renderer/d3d/ContextD3D.h"

namespace rx
{
class Renderer9;

class Context9 : public ContextD3D
{
  public:
    Context9(const gl::State &state, gl::ErrorSet *errorSet, Renderer9 *renderer);
    ~Context9() override;

    angle::Result initialize() override;
    void onDestroy(const gl::Context *context) override;

    // Shader creation
    CompilerImpl *createCompiler() override;
    ShaderImpl *createShader(const gl::ShaderState &data) override;
    ProgramImpl *createProgram(const gl::ProgramState &data) override;

    // Framebuffer creation
    FramebufferImpl *createFramebuffer(const gl::FramebufferState &data) override;

    // Texture creation
    TextureImpl *createTexture(const gl::TextureState &state) override;

    // Renderbuffer creation
    RenderbufferImpl *createRenderbuffer(const gl::RenderbufferState &state) override;

    // Buffer creation
    BufferImpl *createBuffer(const gl::BufferState &state) override;

    // Vertex Array creation
    VertexArrayImpl *createVertexArray(const gl::VertexArrayState &data) override;

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
                                        GLsizei instances) override;
    angle::Result drawElementsInstancedBaseVertex(const gl::Context *context,
                                                  gl::PrimitiveMode mode,
                                                  GLsizei count,
                                                  gl::DrawElementsType type,
                                                  const void *indices,
                                                  GLsizei instances,
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

    // Device loss
    gl::GraphicsResetStatus getResetStatus() override;

    // EXT_debug_marker
    angle::Result insertEventMarker(GLsizei length, const char *marker) override;
    angle::Result pushGroupMarker(GLsizei length, const char *marker) override;
    angle::Result popGroupMarker() override;

    // KHR_debug
    angle::Result pushDebugGroup(const gl::Context *context,
                                 GLenum source,
                                 GLuint id,
                                 const std::string &message) override;
    angle::Result popDebugGroup(const gl::Context *context) override;

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

    // Caps queries
    gl::Caps getNativeCaps() const override;
    const gl::TextureCapsMap &getNativeTextureCaps() const override;
    const gl::Extensions &getNativeExtensions() const override;
    const gl::Limitations &getNativeLimitations() const override;
    ShPixelLocalStorageType getNativePixelLocalStorageType() const override;

    angle::Result dispatchCompute(const gl::Context *context,
                                  GLuint numGroupsX,
                                  GLuint numGroupsY,
                                  GLuint numGroupsZ) override;
    angle::Result dispatchComputeIndirect(const gl::Context *context, GLintptr indirect) override;

    angle::Result memoryBarrier(const gl::Context *context, GLbitfield barriers) override;
    angle::Result memoryBarrierByRegion(const gl::Context *context, GLbitfield barriers) override;

    Renderer9 *getRenderer() const { return mRenderer; }

    angle::Result getIncompleteTexture(const gl::Context *context,
                                       gl::TextureType type,
                                       gl::Texture **textureOut);

    void handleResult(HRESULT hr,
                      const char *message,
                      const char *file,
                      const char *function,
                      unsigned int line) override;

  private:
    Renderer9 *mRenderer;
    IncompleteTextureSet mIncompleteTextures;
    std::stack<std::string> mMarkerStack;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_D3D_D3D9_CONTEXT9_H_
