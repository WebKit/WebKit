//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ContextMtl.h:
//    Defines the class interface for ContextMtl, implementing ContextImpl.
//

#ifndef LIBANGLE_RENDERER_METAL_CONTEXTMTL_H_
#define LIBANGLE_RENDERER_METAL_CONTEXTMTL_H_

#import <Metal/Metal.h>

#include "common/Optional.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/ContextImpl.h"
#include "libANGLE/renderer/metal/mtl_buffer_pool.h"
#include "libANGLE/renderer/metal/mtl_command_buffer.h"
#include "libANGLE/renderer/metal/mtl_resources.h"
#include "libANGLE/renderer/metal/mtl_state_cache.h"
#include "libANGLE/renderer/metal/mtl_utils.h"

namespace rx
{
class DisplayMtl;
class FramebufferMtl;
class VertexArrayMtl;
class ProgramMtl;

class ContextMtl : public ContextImpl, public mtl::Context
{
  public:
    ContextMtl(const gl::State &state, gl::ErrorSet *errorSet, DisplayMtl *display);
    ~ContextMtl() override;

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

    // override mtl::ErrorHandler
    void handleError(GLenum error,
                     const char *file,
                     const char *function,
                     unsigned int line) override;
    void handleError(NSError *_Nullable error,
                     const char *file,
                     const char *function,
                     unsigned int line) override;

    using ContextImpl::handleError;

    void invalidateState(const gl::Context *context);
    void invalidateDefaultAttribute(size_t attribIndex);
    void invalidateDefaultAttributes(const gl::AttributesMask &dirtyMask);
    void invalidateCurrentTextures();
    void invalidateDriverUniforms();
    void invalidateRenderPipeline();

    // Call this to notify ContextMtl whenever FramebufferMtl's state changed
    void onDrawFrameBufferChange(const gl::Context *context, FramebufferMtl *framebuffer);

    const MTLClearColor &getClearColorValue() const;
    MTLColorWriteMask getColorMask() const;
    float getClearDepthValue() const;
    uint32_t getClearStencilValue() const;
    // Return front facing stencil write mask
    uint32_t getStencilMask() const;
    bool getDepthMask() const;

    const mtl::Format &getPixelFormat(angle::FormatID angleFormatId) const;
    // See mtl::FormatTable::getVertexFormat()
    const mtl::VertexFormat &getVertexFormat(angle::FormatID angleFormatId,
                                             bool tightlyPacked) const;

    // Recommended to call these methods to end encoding instead of invoking the encoder's
    // endEncoding() directly.
    void endEncoding(mtl::RenderCommandEncoder *encoder);
    // Ends any active command encoder
    void endEncoding(bool forceSaveRenderPassContent);

    void flushCommandBufer();
    void present(const gl::Context *context, id<CAMetalDrawable> presentationDrawable);
    angle::Result finishCommandBuffer();

    // Check whether compatible render pass has been started.
    bool hasStartedRenderPass(const mtl::RenderPassDesc &desc);
    bool hasStartedRenderPass(FramebufferMtl *framebuffer);

    // Get current render encoder. May be nullptr if no render pass has been started.
    mtl::RenderCommandEncoder *getRenderCommandEncoder();

    mtl::RenderCommandEncoder *getCurrentFramebufferRenderCommandEncoder();

    // Will end current command encoder if it is valid, then start new encoder.
    // Unless hasStartedRenderPass(desc) returns true.
    mtl::RenderCommandEncoder *getRenderCommandEncoder(const mtl::RenderPassDesc &desc);

    // Utilities to quickly create render command enconder to a specific texture:
    // The previous content of texture will be loaded if clearColor is not provided
    mtl::RenderCommandEncoder *getRenderCommandEncoder(const mtl::TextureRef &textureTarget,
                                                       const gl::ImageIndex &index,
                                                       const Optional<MTLClearColor> &clearColor);
    // The previous content of texture will be loaded
    mtl::RenderCommandEncoder *getRenderCommandEncoder(const mtl::TextureRef &textureTarget,
                                                       const gl::ImageIndex &index);

    // Will end current command encoder and start new blit command encoder. Unless a blit comamnd
    // encoder is already started.
    mtl::BlitCommandEncoder *getBlitCommandEncoder();

    // Will end current command encoder and start new compute command encoder. Unless a compute
    // command encoder is already started.
    mtl::ComputeCommandEncoder *getComputeCommandEncoder();

  private:
    void ensureCommandBufferValid();
    angle::Result setupDraw(const gl::Context *context,
                            gl::PrimitiveMode mode,
                            GLint firstVertex,
                            GLsizei vertexOrIndexCount,
                            GLsizei instanceCount,
                            gl::DrawElementsType indexTypeOrNone,
                            const void *indices);
    angle::Result genLineLoopLastSegment(const gl::Context *context,
                                         GLint firstVertex,
                                         GLsizei vertexOrIndexCount,
                                         GLsizei instanceCount,
                                         gl::DrawElementsType indexTypeOrNone,
                                         const void *indices,
                                         mtl::BufferRef *lastSegmentIndexBufferOut);

    angle::Result drawTriFanArrays(const gl::Context *context,
                                   GLint first,
                                   GLsizei count,
                                   GLsizei instances);
    angle::Result drawTriFanArraysWithBaseVertex(const gl::Context *context,
                                                 GLint first,
                                                 GLsizei count,
                                                 GLsizei instances);
    angle::Result drawTriFanArraysLegacy(const gl::Context *context,
                                         GLint first,
                                         GLsizei count,
                                         GLsizei instances);
    angle::Result drawTriFanElements(const gl::Context *context,
                                     GLsizei count,
                                     gl::DrawElementsType type,
                                     const void *indices,
                                     GLsizei instances);

    angle::Result drawArraysImpl(const gl::Context *context,
                                 gl::PrimitiveMode mode,
                                 GLint first,
                                 GLsizei count,
                                 GLsizei instanceCount);

    angle::Result drawElementsImpl(const gl::Context *context,
                                   gl::PrimitiveMode mode,
                                   GLsizei count,
                                   gl::DrawElementsType type,
                                   const void *indices,
                                   GLsizei instanceCount);

    void updateExtendedState(const gl::State &glState);

    void updateViewport(FramebufferMtl *framebufferMtl,
                        const gl::Rectangle &viewport,
                        float nearPlane,
                        float farPlane);
    void updateDepthRange(float nearPlane, float farPlane);
    void updateScissor(const gl::State &glState);
    void updateCullMode(const gl::State &glState);
    void updateFrontFace(const gl::State &glState);
    void updateDepthBias(const gl::State &glState);
    void updateDrawFrameBufferBinding(const gl::Context *context);
    void updateProgramExecutable(const gl::Context *context);
    void updateVertexArray(const gl::Context *context);

    angle::Result updateDefaultAttribute(size_t attribIndex);
    angle::Result handleDirtyActiveTextures(const gl::Context *context);
    angle::Result handleDirtyDefaultAttribs(const gl::Context *context);
    angle::Result handleDirtyDriverUniforms(const gl::Context *context);
    angle::Result handleDirtyDepthStencilState(const gl::Context *context);
    angle::Result handleDirtyDepthBias(const gl::Context *context);
    angle::Result checkIfPipelineChanged(const gl::Context *context,
                                         gl::PrimitiveMode primitiveMode,
                                         Optional<mtl::RenderPipelineDesc> *changedPipelineDesc);

    // Dirty bits.
    enum DirtyBitType : size_t
    {
        DIRTY_BIT_DEFAULT_ATTRIBS,
        DIRTY_BIT_TEXTURES,
        DIRTY_BIT_DRIVER_UNIFORMS,
        DIRTY_BIT_DEPTH_STENCIL_DESC,
        DIRTY_BIT_DEPTH_BIAS,
        DIRTY_BIT_STENCIL_REF,
        DIRTY_BIT_BLEND_COLOR,
        DIRTY_BIT_VIEWPORT,
        DIRTY_BIT_SCISSOR,
        DIRTY_BIT_DRAW_FRAMEBUFFER,
        DIRTY_BIT_CULL_MODE,
        DIRTY_BIT_WINDING,
        DIRTY_BIT_RENDER_PIPELINE,
        DIRTY_BIT_MAX,
    };

    // See compiler/translator/TranslatorVulkan.cpp: AddDriverUniformsToShader()
    struct DriverUniforms
    {
        float viewport[4];

        float halfRenderAreaHeight;
        float viewportYScale;
        float negViewportYScale;

        // 32 bits for 32 clip distances
        uint32_t enabledClipDistances;

        // NOTE(hqle): Transform feedsback is not supported yet.
        uint32_t xfbActiveUnpaused;
        uint32_t xfbVerticesPerDraw;
        // NOTE: Explicit padding. Fill in with useful data when needed in the future.
        int32_t padding[2];

        int32_t xfbBufferOffsets[4];
        uint32_t acbBufferOffsets[4];

        // We'll use x, y, z, w for near / far / diff / zscale respectively.
        float depthRange[4];

        // Used to pre-rotate gl_Position for Vulkan swapchain images on Android (a mat2, which is
        // padded to the size of two vec4's).
        float preRotation[8];
    };

    struct DefaultAttribute
    {
        // NOTE(hqle): Support integer default attributes in ES 3.0
        float values[4];
    };

    mtl::CommandBuffer mCmdBuffer;
    mtl::RenderCommandEncoder mRenderEncoder;
    mtl::BlitCommandEncoder mBlitEncoder;
    mtl::ComputeCommandEncoder mComputeEncoder;

    // Cached back-end objects
    FramebufferMtl *mDrawFramebuffer = nullptr;
    VertexArrayMtl *mVertexArray     = nullptr;
    ProgramMtl *mProgram             = nullptr;

    // Special flag to indicate current draw framebuffer is default framebuffer.
    // We need this instead of calling mDrawFramebuffer->getState().isDefault() because
    // mDrawFramebuffer might point to a deleted object, ContextMtl only knows about this very late,
    // only during syncState() function call.
    bool mDrawFramebufferIsDefault = true;

    using DirtyBits = angle::BitSet<DIRTY_BIT_MAX>;

    gl::AttributesMask mDirtyDefaultAttribsMask;
    DirtyBits mDirtyBits;

    // State
    mtl::RenderPipelineDesc mRenderPipelineDesc;
    mtl::DepthStencilDesc mDepthStencilDesc;
    mtl::BlendDesc mBlendDesc;
    MTLClearColor mClearColor;
    uint32_t mClearStencil    = 0;
    uint32_t mStencilRefFront = 0;
    uint32_t mStencilRefBack  = 0;
    MTLViewport mViewport;
    MTLScissorRect mScissorRect;
    MTLWinding mWinding;
    MTLCullMode mCullMode;
    bool mCullAllPolygons = false;

    // Lineloop and TriFan index buffer
    mtl::BufferPool mLineLoopIndexBuffer;
    mtl::BufferPool mTriFanIndexBuffer;
    // one buffer can be reused for any starting vertex in DrawArrays()
    mtl::BufferRef mTriFanArraysIndexBuffer;

    DriverUniforms mDriverUniforms;

    DefaultAttribute mDefaultAttributes[mtl::kMaxVertexAttribs];
};

}  // namespace rx

#endif /* LIBANGLE_RENDERER_METAL_CONTEXTMTL_H_ */
