//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// UtilsVk.h:
//    Defines the UtilsVk class, a helper for various internal draw/dispatch utilities such as
//    buffer clear and copy, image clear and copy, texture mip map generation, etc.
//
//    - Buffer clear: Implemented, but no current users
//    - Convert index buffer:
//      * Used by VertexArrayVk::convertIndexBufferGPU() to convert a ubyte element array to ushort
//    - Convert vertex buffer:
//      * Used by VertexArrayVk::convertVertexBufferGPU() to convert vertex attributes from
//        unsupported formats to their fallbacks.
//    - Image clear: Used by FramebufferVk::clearWithDraw().
//    - Image copy: Used by TextureVk::copySubImageImplWithDraw().
//    - Color blit/resolve: Used by FramebufferVk::blit() to implement blit or multisample resolve
//      on color images.
//    - Depth/Stencil blit/resolve: Used by FramebufferVk::blit() to implement blit or multisample
//      resolve on depth/stencil images.
//    - Overlay Cull/Draw: Used by OverlayVk to efficiently draw a UI for debugging.
//    - Mipmap generation: Not yet implemented
//

#ifndef LIBANGLE_RENDERER_VULKAN_UTILSVK_H_
#define LIBANGLE_RENDERER_VULKAN_UTILSVK_H_

#include "libANGLE/renderer/vulkan/vk_cache_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"
#include "libANGLE/renderer/vulkan/vk_internal_shaders_autogen.h"

namespace rx
{
class UtilsVk : angle::NonCopyable
{
  public:
    UtilsVk();
    ~UtilsVk();

    void destroy(VkDevice device);

    struct ClearParameters
    {
        VkClearColorValue clearValue;
        size_t offset;
        size_t size;
    };

    struct ConvertIndexParameters
    {
        uint32_t srcOffset = 0;
        uint32_t dstOffset = 0;
        uint32_t maxIndex  = 0;
    };

    struct ConvertIndexIndirectParameters
    {
        uint32_t srcIndirectBufOffset = 0;
        uint32_t dstIndexBufOffset    = 0;
        uint32_t maxIndex             = 0;
        uint32_t dstIndirectBufOffset = 0;
    };

    struct ConvertLineLoopIndexIndirectParameters
    {
        uint32_t indirectBufferOffset    = 0;
        uint32_t dstIndirectBufferOffset = 0;
        uint32_t dstIndexBufferOffset    = 0;
        uint32_t indicesBitsWidth        = 0;
    };

    struct ConvertLineLoopArrayIndirectParameters
    {
        uint32_t indirectBufferOffset    = 0;
        uint32_t dstIndirectBufferOffset = 0;
        uint32_t dstIndexBufferOffset    = 0;
    };

    struct ConvertVertexParameters
    {
        size_t vertexCount;
        const angle::Format *srcFormat;
        const angle::Format *destFormat;
        size_t srcStride;
        size_t srcOffset;
        size_t destOffset;
    };

    struct ClearFramebufferParameters
    {
        // Satisfy chromium-style with a constructor that does what = {} was already doing in a
        // safer way.
        ClearFramebufferParameters();

        gl::Rectangle clearArea;

        // Note that depth clear is never needed to be done with a draw call.
        bool clearColor;
        bool clearStencil;

        uint8_t stencilMask;
        VkColorComponentFlags colorMaskFlags;
        uint32_t colorAttachmentIndexGL;
        const angle::Format *colorFormat;

        VkClearColorValue colorClearValue;
        uint8_t stencilClearValue;
    };

    struct BlitResolveParameters
    {
        // |srcOffset| and |dstIndexBufferOffset| define the original blit/resolve offsets, possibly
        // flipped.
        int srcOffset[2];
        int destOffset[2];
        // |stretch| is SourceDimension / DestDimension used to transfer dest coordinates to source.
        float stretch[2];
        // |srcExtents| is used to normalize source coordinates for sampling.
        int srcExtents[2];
        // |blitArea| is the area in destination where blit happens.  It's expected that scissor
        // and source clipping effects have already been applied to it.
        gl::Rectangle blitArea;
        int srcLayer;
        // Whether linear or point sampling should be used.
        bool linear;
        bool flipX;
        bool flipY;
    };

    struct CopyImageParameters
    {
        int srcOffset[2];
        int srcExtents[2];
        int destOffset[2];
        int srcMip;
        int srcLayer;
        int srcHeight;
        bool srcPremultiplyAlpha;
        bool srcUnmultiplyAlpha;
        bool srcFlipY;
        bool destFlipY;
    };

    struct OverlayCullParameters
    {
        uint32_t subgroupSize[2];
        bool supportsSubgroupBallot;
        bool supportsSubgroupArithmetic;
    };

    struct OverlayDrawParameters
    {
        uint32_t subgroupSize[2];
    };

    angle::Result clearBuffer(ContextVk *contextVk,
                              vk::BufferHelper *dest,
                              const ClearParameters &params);
    angle::Result convertIndexBuffer(ContextVk *contextVk,
                                     vk::BufferHelper *dest,
                                     vk::BufferHelper *src,
                                     const ConvertIndexParameters &params);
    angle::Result convertIndexIndirectBuffer(ContextVk *contextVk,
                                             vk::BufferHelper *srcIndirectBuf,
                                             vk::BufferHelper *srcIndexBuf,
                                             vk::BufferHelper *dstIndirectBuf,
                                             vk::BufferHelper *dstIndexBuf,
                                             const ConvertIndexIndirectParameters &params);

    angle::Result convertLineLoopIndexIndirectBuffer(
        ContextVk *contextVk,
        vk::BufferHelper *srcIndirectBuffer,
        vk::BufferHelper *destIndirectBuffer,
        vk::BufferHelper *destIndexBuffer,
        vk::BufferHelper *srcIndexBuffer,
        const ConvertLineLoopIndexIndirectParameters &params);

    angle::Result convertLineLoopArrayIndirectBuffer(
        ContextVk *contextVk,
        vk::BufferHelper *srcIndirectBuffer,
        vk::BufferHelper *destIndirectBuffer,
        vk::BufferHelper *destIndexBuffer,
        const ConvertLineLoopArrayIndirectParameters &params);

    angle::Result convertVertexBuffer(ContextVk *contextVk,
                                      vk::BufferHelper *dest,
                                      vk::BufferHelper *src,
                                      const ConvertVertexParameters &params);

    angle::Result clearFramebuffer(ContextVk *contextVk,
                                   FramebufferVk *framebuffer,
                                   const ClearFramebufferParameters &params);

    // Resolve images if multisampled.  Blit otherwise.
    angle::Result colorBlitResolve(ContextVk *contextVk,
                                   FramebufferVk *framebuffer,
                                   vk::ImageHelper *src,
                                   const vk::ImageView *srcView,
                                   const BlitResolveParameters &params);
    angle::Result depthStencilBlitResolve(ContextVk *contextVk,
                                          FramebufferVk *framebuffer,
                                          vk::ImageHelper *src,
                                          const vk::ImageView *srcDepthView,
                                          const vk::ImageView *srcStencilView,
                                          const BlitResolveParameters &params);
    angle::Result stencilBlitResolveNoShaderExport(ContextVk *contextVk,
                                                   FramebufferVk *framebuffer,
                                                   vk::ImageHelper *src,
                                                   const vk::ImageView *srcStencilView,
                                                   const BlitResolveParameters &params);

    angle::Result copyImage(ContextVk *contextVk,
                            vk::ImageHelper *dest,
                            const vk::ImageView *destView,
                            vk::ImageHelper *src,
                            const vk::ImageView *srcView,
                            const CopyImageParameters &params);

    // Overlay utilities.
    angle::Result cullOverlayWidgets(ContextVk *contextVk,
                                     vk::BufferHelper *enabledWidgetsBuffer,
                                     vk::ImageHelper *dest,
                                     const vk::ImageView *destView,
                                     const OverlayCullParameters &params);

    angle::Result drawOverlay(ContextVk *contextVk,
                              vk::BufferHelper *textWidgetsBuffer,
                              vk::BufferHelper *graphWidgetsBuffer,
                              vk::ImageHelper *font,
                              const vk::ImageView *fontView,
                              vk::ImageHelper *culledWidgets,
                              const vk::ImageView *culledWidgetsView,
                              vk::ImageHelper *dest,
                              const vk::ImageView *destView,
                              const OverlayDrawParameters &params);

  private:
    ANGLE_ENABLE_STRUCT_PADDING_WARNINGS

    struct BufferUtilsShaderParams
    {
        // Structure matching PushConstants in BufferUtils.comp
        uint32_t destOffset          = 0;
        uint32_t size                = 0;
        uint32_t srcOffset           = 0;
        uint32_t padding             = 0;
        VkClearColorValue clearValue = {};
    };

    struct ConvertIndexShaderParams
    {
        uint32_t srcOffset     = 0;
        uint32_t dstOffsetDiv4 = 0;
        uint32_t maxIndex      = 0;
        uint32_t _padding      = 0;
    };

    struct ConvertIndexIndirectShaderParams
    {
        uint32_t srcIndirectOffsetDiv4 = 0;
        uint32_t dstOffsetDiv4         = 0;
        uint32_t maxIndex              = 0;
        uint32_t dstIndirectOffsetDiv4 = 0;
    };

    struct ConvertIndexIndirectLineLoopShaderParams
    {
        uint32_t cmdOffsetDiv4    = 0;
        uint32_t dstCmdOffsetDiv4 = 0;
        uint32_t dstOffsetDiv4    = 0;
        uint32_t isRestartEnabled = 0;
    };

    struct ConvertIndirectLineLoopShaderParams
    {
        uint32_t cmdOffsetDiv4    = 0;
        uint32_t dstCmdOffsetDiv4 = 0;
        uint32_t dstOffsetDiv4    = 0;
    };

    struct ConvertVertexShaderParams
    {
        ConvertVertexShaderParams();

        // Structure matching PushConstants in ConvertVertex.comp
        uint32_t outputCount    = 0;
        uint32_t componentCount = 0;
        uint32_t srcOffset      = 0;
        uint32_t destOffset     = 0;
        uint32_t Ns             = 0;
        uint32_t Bs             = 0;
        uint32_t Ss             = 0;
        uint32_t Es             = 0;
        uint32_t Nd             = 0;
        uint32_t Bd             = 0;
        uint32_t Sd             = 0;
        uint32_t Ed             = 0;
    };

    struct ImageClearShaderParams
    {
        // Structure matching PushConstants in ImageClear.frag
        VkClearColorValue clearValue = {};
    };

    struct ImageCopyShaderParams
    {
        ImageCopyShaderParams();

        // Structure matching PushConstants in ImageCopy.frag
        int32_t srcOffset[2]             = {};
        int32_t destOffset[2]            = {};
        int32_t srcMip                   = 0;
        int32_t srcLayer                 = 0;
        uint32_t flipY                   = 0;
        uint32_t premultiplyAlpha        = 0;
        uint32_t unmultiplyAlpha         = 0;
        uint32_t destHasLuminance        = 0;
        uint32_t destIsAlpha             = 0;
        uint32_t destDefaultChannelsMask = 0;
    };

    union BlitResolveOffset
    {
        int32_t resolve[2];
        float blit[2];
    };

    struct BlitResolveShaderParams
    {
        // Structure matching PushConstants in BlitResolve.frag
        BlitResolveOffset offset = {};
        float stretch[2]         = {};
        float invSrcExtent[2]    = {};
        int32_t srcLayer         = 0;
        int32_t samples          = 0;
        float invSamples         = 0;
        uint32_t outputMask      = 0;
        uint32_t flipX           = 0;
        uint32_t flipY           = 0;
    };

    struct BlitResolveStencilNoExportShaderParams
    {
        // Structure matching PushConstants in BlitResolveStencilNoExport.comp
        BlitResolveOffset offset = {};
        float stretch[2]         = {};
        float invSrcExtent[2]    = {};
        int32_t srcLayer         = 0;
        int32_t srcWidth         = 0;
        int32_t blitArea[4]      = {};
        int32_t destPitch        = 0;
        uint32_t flipX           = 0;
        uint32_t flipY           = 0;
    };

    struct OverlayDrawShaderParams
    {
        // Structure matching PushConstants in OverlayDraw.comp
        uint32_t outputSize[2] = {};
    };

    ANGLE_DISABLE_STRUCT_PADDING_WARNINGS

    // Functions implemented by the class:
    enum class Function
    {
        // Functions implemented in graphics
        ImageClear  = 0,
        ImageCopy   = 1,
        BlitResolve = 2,

        // Functions implemented in compute
        ComputeStartIndex          = 3,  // Special value to separate draw and dispatch functions.
        BufferClear                = 3,
        ConvertIndexBuffer         = 4,
        ConvertVertexBuffer        = 5,
        BlitResolveStencilNoExport = 6,
        OverlayCull                = 7,
        OverlayDraw                = 8,
        ConvertIndexIndirectBuffer = 9,
        ConvertIndexIndirectLineLoopBuffer = 10,
        ConvertIndirectLineLoopBuffer      = 11,

        InvalidEnum = 12,
        EnumCount   = 12,
    };

    // Common function that creates the pipeline for the specified function, binds it and prepares
    // the draw/dispatch call.  If function >= ComputeStartIndex, fsCsShader is expected to be a
    // compute shader, vsShader and pipelineDesc should be nullptr, and this will set up a dispatch
    // call. Otherwise fsCsShader is expected to be a fragment shader and this will set up a draw
    // call.
    angle::Result setupProgram(ContextVk *contextVk,
                               Function function,
                               vk::RefCounted<vk::ShaderAndSerial> *fsCsShader,
                               vk::RefCounted<vk::ShaderAndSerial> *vsShader,
                               vk::ShaderProgramHelper *program,
                               const vk::GraphicsPipelineDesc *pipelineDesc,
                               const VkDescriptorSet descriptorSet,
                               const void *pushConstants,
                               size_t pushConstantsSize,
                               vk::CommandBuffer *commandBuffer);

    // Initializes descriptor set layout, pipeline layout and descriptor pool corresponding to given
    // function, if not already initialized.  Uses setSizes to create the layout.  For example, if
    // this array has two entries {STORAGE_TEXEL_BUFFER, 1} and {UNIFORM_TEXEL_BUFFER, 3}, then the
    // created set layout would be binding 0 for storage texel buffer and bindings 1 through 3 for
    // uniform texel buffer.  All resources are put in set 0.
    angle::Result ensureResourcesInitialized(ContextVk *contextVk,
                                             Function function,
                                             VkDescriptorPoolSize *setSizes,
                                             size_t setSizesCount,
                                             size_t pushConstantsSize);

    // Initializers corresponding to functions, calling into ensureResourcesInitialized with the
    // appropriate parameters.
    angle::Result ensureBufferClearResourcesInitialized(ContextVk *contextVk);
    angle::Result ensureConvertIndexResourcesInitialized(ContextVk *contextVk);
    angle::Result ensureConvertIndexIndirectResourcesInitialized(ContextVk *contextVk);
    angle::Result ensureConvertIndexIndirectLineLoopResourcesInitialized(ContextVk *contextVk);
    angle::Result ensureConvertIndirectLineLoopResourcesInitialized(ContextVk *contextVk);
    angle::Result ensureConvertVertexResourcesInitialized(ContextVk *contextVk);
    angle::Result ensureImageClearResourcesInitialized(ContextVk *contextVk);
    angle::Result ensureImageCopyResourcesInitialized(ContextVk *contextVk);
    angle::Result ensureBlitResolveResourcesInitialized(ContextVk *contextVk);
    angle::Result ensureBlitResolveStencilNoExportResourcesInitialized(ContextVk *contextVk);
    angle::Result ensureOverlayCullResourcesInitialized(ContextVk *contextVk);
    angle::Result ensureOverlayDrawResourcesInitialized(ContextVk *contextVk);

    angle::Result ensureSamplersInitialized(ContextVk *context);

    angle::Result startRenderPass(ContextVk *contextVk,
                                  vk::ImageHelper *image,
                                  const vk::ImageView *imageView,
                                  const vk::RenderPassDesc &renderPassDesc,
                                  const gl::Rectangle &renderArea,
                                  vk::CommandBuffer **commandBufferOut);

    // Blits or resolves either color or depth/stencil, based on which view is given.
    angle::Result blitResolveImpl(ContextVk *contextVk,
                                  FramebufferVk *framebuffer,
                                  vk::ImageHelper *src,
                                  const vk::ImageView *srcColorView,
                                  const vk::ImageView *srcDepthView,
                                  const vk::ImageView *srcStencilView,
                                  const BlitResolveParameters &params);

    // Allocates a single descriptor set.
    angle::Result allocateDescriptorSet(ContextVk *contextVk,
                                        Function function,
                                        vk::RefCountedDescriptorPoolBinding *bindingOut,
                                        VkDescriptorSet *descriptorSetOut);

    angle::PackedEnumMap<Function, vk::DescriptorSetLayoutPointerArray> mDescriptorSetLayouts;
    angle::PackedEnumMap<Function, vk::BindingPointer<vk::PipelineLayout>> mPipelineLayouts;
    angle::PackedEnumMap<Function, vk::DynamicDescriptorPool> mDescriptorPools;

    vk::ShaderProgramHelper mBufferUtilsPrograms[vk::InternalShader::BufferUtils_comp::kArrayLen];
    vk::ShaderProgramHelper mConvertIndexPrograms[vk::InternalShader::ConvertIndex_comp::kArrayLen];
    vk::ShaderProgramHelper mConvertIndexIndirectLineLoopPrograms
        [vk::InternalShader::ConvertIndexIndirectLineLoop_comp::kArrayLen];
    vk::ShaderProgramHelper mConvertIndirectLineLoopPrograms
        [vk::InternalShader::ConvertIndirectLineLoop_comp::kArrayLen];
    vk::ShaderProgramHelper
        mConvertVertexPrograms[vk::InternalShader::ConvertVertex_comp::kArrayLen];
    vk::ShaderProgramHelper mImageClearProgramVSOnly;
    vk::ShaderProgramHelper mImageClearProgram[vk::InternalShader::ImageClear_frag::kArrayLen];
    vk::ShaderProgramHelper mImageCopyPrograms[vk::InternalShader::ImageCopy_frag::kArrayLen];
    vk::ShaderProgramHelper mBlitResolvePrograms[vk::InternalShader::BlitResolve_frag::kArrayLen];
    vk::ShaderProgramHelper mBlitResolveStencilNoExportPrograms
        [vk::InternalShader::BlitResolveStencilNoExport_comp::kArrayLen];
    vk::ShaderProgramHelper mOverlayCullPrograms[vk::InternalShader::OverlayCull_comp::kArrayLen];
    vk::ShaderProgramHelper mOverlayDrawPrograms[vk::InternalShader::OverlayDraw_comp::kArrayLen];

    vk::Sampler mPointSampler;
    vk::Sampler mLinearSampler;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_UTILSVK_H_
