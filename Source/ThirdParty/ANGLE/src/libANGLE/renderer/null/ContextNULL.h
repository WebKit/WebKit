//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ContextNULL.h:
//    Defines the class interface for ContextNULL, implementing ContextImpl.
//

#ifndef LIBANGLE_RENDERER_NULL_CONTEXTNULL_H_
#define LIBANGLE_RENDERER_NULL_CONTEXTNULL_H_

#include "libANGLE/renderer/ContextImpl.h"

namespace rx
{

class AllocationTrackerNULL : angle::NonCopyable
{
  public:
    explicit AllocationTrackerNULL(size_t maxTotalAllocationSize);
    ~AllocationTrackerNULL();

    // Check if it is possible to change an allocation from oldSize to newSize.  If it is possible,
    // the allocation is registered and true is returned else false is returned.
    bool updateMemoryAllocation(size_t oldSize, size_t newSize);

  private:
    size_t mAllocatedBytes;
    const size_t mMaxBytes;
};

class ContextNULL : public ContextImpl
{
  public:
    ContextNULL(const gl::ContextState &state, AllocationTrackerNULL *allocationTracker);
    ~ContextNULL() override;

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

    // CHROMIUM_path_rendering path drawing methods.
    void stencilFillPath(const gl::Path *path, GLenum fillMode, GLuint mask) override;
    void stencilStrokePath(const gl::Path *path, GLint reference, GLuint mask) override;
    void coverFillPath(const gl::Path *path, GLenum coverMode) override;
    void coverStrokePath(const gl::Path *path, GLenum coverMode) override;
    void stencilThenCoverFillPath(const gl::Path *path,
                                  GLenum fillMode,
                                  GLuint mask,
                                  GLenum coverMode) override;

    void stencilThenCoverStrokePath(const gl::Path *path,
                                    GLint reference,
                                    GLuint mask,
                                    GLenum coverMode) override;

    void coverFillPathInstanced(const std::vector<gl::Path *> &paths,
                                GLenum coverMode,
                                GLenum transformType,
                                const GLfloat *transformValues) override;
    void coverStrokePathInstanced(const std::vector<gl::Path *> &paths,
                                  GLenum coverMode,
                                  GLenum transformType,
                                  const GLfloat *transformValues) override;
    void stencilFillPathInstanced(const std::vector<gl::Path *> &paths,
                                  GLenum fillMode,
                                  GLuint mask,
                                  GLenum transformType,
                                  const GLfloat *transformValues) override;
    void stencilStrokePathInstanced(const std::vector<gl::Path *> &paths,
                                    GLint reference,
                                    GLuint mask,
                                    GLenum transformType,
                                    const GLfloat *transformValues) override;
    void stencilThenCoverFillPathInstanced(const std::vector<gl::Path *> &paths,
                                           GLenum coverMode,
                                           GLenum fillMode,
                                           GLuint mask,
                                           GLenum transformType,
                                           const GLfloat *transformValues) override;
    void stencilThenCoverStrokePathInstanced(const std::vector<gl::Path *> &paths,
                                             GLenum coverMode,
                                             GLint reference,
                                             GLuint mask,
                                             GLenum transformType,
                                             const GLfloat *transformValues) override;

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
    ShaderImpl *createShader(const gl::ShaderState &data) override;
    ProgramImpl *createProgram(const gl::ProgramState &data) override;

    // Framebuffer creation
    FramebufferImpl *createFramebuffer(const gl::FramebufferState &data) override;

    // Texture creation
    TextureImpl *createTexture(const gl::TextureState &state) override;

    // Renderbuffer creation
    RenderbufferImpl *createRenderbuffer() override;

    // Buffer creation
    BufferImpl *createBuffer(const gl::BufferState &state) override;

    // Vertex Array creation
    VertexArrayImpl *createVertexArray(const gl::VertexArrayState &data) override;

    // Query and Fence creation
    QueryImpl *createQuery(GLenum type) override;
    FenceNVImpl *createFenceNV() override;
    FenceSyncImpl *createFenceSync() override;

    // Transform Feedback creation
    TransformFeedbackImpl *createTransformFeedback(
        const gl::TransformFeedbackState &state) override;

    // Sampler object creation
    SamplerImpl *createSampler() override;

    std::vector<PathImpl *> createPaths(GLsizei range) override;

    gl::Error dispatchCompute(GLuint numGroupsX, GLuint numGroupsY, GLuint numGroupsZ) override;

  private:
    gl::Caps mCaps;
    gl::TextureCapsMap mTextureCaps;
    gl::Extensions mExtensions;
    gl::Limitations mLimitations;

    AllocationTrackerNULL *mAllocationTracker;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_NULL_CONTEXTNULL_H_
