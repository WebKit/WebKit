//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_render_utils.h:
//    Defines the class interface for RenderUtils.
//

#ifndef LIBANGLE_RENDERER_METAL_MTL_RENDER_UTILS_H_
#define LIBANGLE_RENDERER_METAL_MTL_RENDER_UTILS_H_

#import <Metal/Metal.h>

#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/metal/mtl_command_buffer.h"
#include "libANGLE/renderer/metal/mtl_state_cache.h"
#include "libANGLE/renderer/metal/shaders/constants.h"

namespace rx
{

class BufferMtl;
class ContextMtl;
class DisplayMtl;

namespace mtl
{
struct ClearRectParams : public ClearOptions
{
    gl::Rectangle clearArea;

    bool flipY = false;
};

struct BlitParams
{
    gl::Offset dstOffset;
    // Destination texture needs to have viewport Y flipped?
    // The difference between this param and unpackFlipY is that unpackFlipY is from
    // glCopyImageCHROMIUM(), and dstFlipY controls whether the final viewport needs to be
    // flipped when drawing to destination texture.
    bool dstFlipY = false;

    MTLColorWriteMask dstColorMask = MTLColorWriteMaskAll;

    TextureRef src;
    uint32_t srcLevel = 0;
    gl::Rectangle srcRect;
    bool srcYFlipped            = false;  // source texture has data flipped in Y direction
    bool unpackFlipY            = false;  // flip texture data copying process in Y direction
    bool unpackPremultiplyAlpha = false;
    bool unpackUnmultiplyAlpha  = false;
    bool dstLuminance           = false;
};

struct TriFanFromArrayParams
{
    uint32_t firstVertex;
    uint32_t vertexCount;
    BufferRef dstBuffer;
    // Must be multiples of kIndexBufferOffsetAlignment
    uint32_t dstOffset;
};

struct IndexConversionParams
{

    gl::DrawElementsType srcType;
    uint32_t indexCount;
    const BufferRef &srcBuffer;
    uint32_t srcOffset;
    const BufferRef &dstBuffer;
    // Must be multiples of kIndexBufferOffsetAlignment
    uint32_t dstOffset;
};

struct IndexGenerationParams
{
    gl::DrawElementsType srcType;
    GLsizei indexCount;
    const void *indices;
    BufferRef dstBuffer;
    uint32_t dstOffset;
};

// Utils class for clear & blitting
class ClearUtils
{
  public:
    ClearUtils();

    void onDestroy();

    // Clear current framebuffer
    angle::Result clearWithDraw(const gl::Context *context,
                                RenderCommandEncoder *cmdEncoder,
                                const ClearRectParams &params);

  private:
    // Defer loading of render pipeline state cache.
    void ensureRenderPipelineStateCacheInitialized(ContextMtl *ctx);

    void setupClearWithDraw(const gl::Context *context,
                            RenderCommandEncoder *cmdEncoder,
                            const ClearRectParams &params);
    id<MTLDepthStencilState> getClearDepthStencilState(const gl::Context *context,
                                                       const ClearRectParams &params);
    id<MTLRenderPipelineState> getClearRenderPipelineState(const gl::Context *context,
                                                           RenderCommandEncoder *cmdEncoder,
                                                           const ClearRectParams &params);

    // Render pipeline cache for clear with draw:
    RenderPipelineCache mClearRenderPipelineCache;
};

class ColorBlitUtils
{
  public:
    ColorBlitUtils();

    void onDestroy();

    // Blit texture data to current framebuffer
    angle::Result blitWithDraw(const gl::Context *context,
                               RenderCommandEncoder *cmdEncoder,
                               const BlitParams &params);

  private:
    // Defer loading of render pipeline state cache.
    void ensureRenderPipelineStateCacheInitialized(ContextMtl *ctx,
                                                   int alphaPremultiplyType,
                                                   int textureType,
                                                   RenderPipelineCache *cacheOut);

    void setupBlitWithDraw(const gl::Context *context,
                           RenderCommandEncoder *cmdEncoder,
                           const BlitParams &params);

    id<MTLRenderPipelineState> getBlitRenderPipelineState(const gl::Context *context,
                                                          RenderCommandEncoder *cmdEncoder,
                                                          const BlitParams &params);

    // Blit with draw pipeline caches:
    // - array dimension: source texture type (2d, ms, array, 3d, etc)
    using ColorBlitRenderPipelineCacheArray =
        std::array<RenderPipelineCache, mtl_shader::kTextureTypeCount>;
    ColorBlitRenderPipelineCacheArray mBlitRenderPipelineCache;
    ColorBlitRenderPipelineCacheArray mBlitPremultiplyAlphaRenderPipelineCache;
    ColorBlitRenderPipelineCacheArray mBlitUnmultiplyAlphaRenderPipelineCache;
};

// util class for generating index buffer
class IndexGeneratorUtils
{
  public:
    void onDestroy();

    // Convert index buffer.
    angle::Result convertIndexBufferGPU(ContextMtl *contextMtl,
                                        const IndexConversionParams &params);
    // Generate triangle fan index buffer for glDrawArrays().
    angle::Result generateTriFanBufferFromArrays(ContextMtl *contextMtl,
                                                 const TriFanFromArrayParams &params);
    // Generate triangle fan index buffer for glDrawElements().
    angle::Result generateTriFanBufferFromElementsArray(ContextMtl *contextMtl,
                                                        const IndexGenerationParams &params);

    // Generate line loop's last segment index buffer for glDrawArrays().
    angle::Result generateLineLoopLastSegment(ContextMtl *contextMtl,
                                              uint32_t firstVertex,
                                              uint32_t lastVertex,
                                              const BufferRef &dstBuffer,
                                              uint32_t dstOffset);
    // Generate line loop's last segment index buffer for glDrawElements().
    angle::Result generateLineLoopLastSegmentFromElementsArray(ContextMtl *contextMtl,
                                                               const IndexGenerationParams &params);

  private:
    // Index generator compute pipelines:
    //  - First dimension: index type.
    //  - second dimension: source buffer's offset is aligned or not.
    using IndexConversionPipelineArray =
        std::array<std::array<AutoObjCPtr<id<MTLComputePipelineState>>, 2>,
                   angle::EnumSize<gl::DrawElementsType>()>;

    // Get compute pipeline to convert index between buffers.
    AutoObjCPtr<id<MTLComputePipelineState>> getIndexConversionPipeline(
        ContextMtl *contextMtl,
        gl::DrawElementsType srcType,
        uint32_t srcOffset);
    // Get compute pipeline to generate tri fan index for glDrawElements().
    AutoObjCPtr<id<MTLComputePipelineState>> getTriFanFromElemArrayGeneratorPipeline(
        ContextMtl *contextMtl,
        gl::DrawElementsType srcType,
        uint32_t srcOffset);
    // Defer loading of compute pipeline to generate tri fan index for glDrawArrays().
    void ensureTriFanFromArrayGeneratorInitialized(ContextMtl *contextMtl);

    angle::Result generateTriFanBufferFromElementsArrayGPU(
        ContextMtl *contextMtl,
        gl::DrawElementsType srcType,
        uint32_t indexCount,
        const BufferRef &srcBuffer,
        uint32_t srcOffset,
        const BufferRef &dstBuffer,
        // Must be multiples of kIndexBufferOffsetAlignment
        uint32_t dstOffset);
    angle::Result generateTriFanBufferFromElementsArrayCPU(ContextMtl *contextMtl,
                                                           const IndexGenerationParams &params);

    angle::Result generateLineLoopLastSegmentFromElementsArrayCPU(
        ContextMtl *contextMtl,
        const IndexGenerationParams &params);

    IndexConversionPipelineArray mIndexConversionPipelineCaches;

    IndexConversionPipelineArray mTriFanFromElemArrayGeneratorPipelineCaches;
    AutoObjCPtr<id<MTLComputePipelineState>> mTriFanFromArraysGeneratorPipeline;
};

// RenderUtils: container class of various util classes above
class RenderUtils : public Context, angle::NonCopyable
{
  public:
    RenderUtils(DisplayMtl *display);
    ~RenderUtils() override;

    angle::Result initialize();
    void onDestroy();

    // Clear current framebuffer
    angle::Result clearWithDraw(const gl::Context *context,
                                RenderCommandEncoder *cmdEncoder,
                                const ClearRectParams &params);
    // Blit texture data to current framebuffer
    angle::Result blitWithDraw(const gl::Context *context,
                               RenderCommandEncoder *cmdEncoder,
                               const BlitParams &params);
    // Same as above but blit the whole texture to the whole of current framebuffer.
    // This function assumes the framebuffer and the source texture have same size.
    angle::Result blitWithDraw(const gl::Context *context,
                               RenderCommandEncoder *cmdEncoder,
                               const TextureRef &srcTexture);

    // See IndexGeneratorUtils
    angle::Result convertIndexBufferGPU(ContextMtl *contextMtl,
                                        const IndexConversionParams &params);
    angle::Result generateTriFanBufferFromArrays(ContextMtl *contextMtl,
                                                 const TriFanFromArrayParams &params);
    angle::Result generateTriFanBufferFromElementsArray(ContextMtl *contextMtl,
                                                        const IndexGenerationParams &params);
    angle::Result generateLineLoopLastSegment(ContextMtl *contextMtl,
                                              uint32_t firstVertex,
                                              uint32_t lastVertex,
                                              const BufferRef &dstBuffer,
                                              uint32_t dstOffset);
    angle::Result generateLineLoopLastSegmentFromElementsArray(ContextMtl *contextMtl,
                                                               const IndexGenerationParams &params);

  private:
    // override ErrorHandler
    void handleError(GLenum error,
                     const char *file,
                     const char *function,
                     unsigned int line) override;
    void handleError(NSError *_Nullable error,
                     const char *file,
                     const char *function,
                     unsigned int line) override;

    ClearUtils mClearUtils;
    ColorBlitUtils mColorBlitUtils;
    IndexGeneratorUtils mIndexUtils;
};

}  // namespace mtl
}  // namespace rx

#endif /* LIBANGLE_RENDERER_METAL_MTL_RENDER_UTILS_H_ */
