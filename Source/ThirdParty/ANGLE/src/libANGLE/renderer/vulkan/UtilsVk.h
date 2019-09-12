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
//    - Buffer copy:
//      * Used by VertexArrayVk::updateIndexTranslation() to convert a ubyte index array to ushort
//    - Convert vertex attribute:
//      * Used by VertexArrayVk::convertVertexBuffer() to convert vertex attributes from unsupported
//        formats to their fallbacks.
//    - Image clear: Used by FramebufferVk::clearWithDraw().
//    - Image copy: Not yet implemented
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

    struct CopyParameters
    {
        size_t destOffset;
        size_t srcOffset;
        size_t size;
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

        const vk::RenderPassDesc *renderPassDesc;
        GLint renderAreaHeight;
        gl::Rectangle clearArea;

        // Note that depth clear is never needed to be done with a draw call.
        bool clearColor;
        bool clearStencil;

        uint8_t stencilMask;
        VkColorComponentFlags colorMaskFlags;
        uint32_t colorAttachmentIndex;
        const angle::Format *colorFormat;

        VkClearColorValue colorClearValue;
        uint8_t stencilClearValue;
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

    angle::Result clearBuffer(vk::Context *context,
                              vk::BufferHelper *dest,
                              const ClearParameters &params);
    angle::Result copyBuffer(vk::Context *context,
                             vk::BufferHelper *dest,
                             vk::BufferHelper *src,
                             const CopyParameters &params);
    angle::Result convertVertexBuffer(vk::Context *context,
                                      vk::BufferHelper *dest,
                                      vk::BufferHelper *src,
                                      const ConvertVertexParameters &params);

    angle::Result clearFramebuffer(ContextVk *contextVk,
                                   FramebufferVk *framebuffer,
                                   const ClearFramebufferParameters &params);

    angle::Result copyImage(ContextVk *contextVk,
                            vk::ImageHelper *dest,
                            const vk::ImageView *destView,
                            vk::ImageHelper *src,
                            const vk::ImageView *srcView,
                            const CopyImageParameters &params);

  private:
    struct BufferUtilsShaderParams
    {
        // Structure matching PushConstants in BufferUtils.comp
        uint32_t destOffset          = 0;
        uint32_t size                = 0;
        uint32_t srcOffset           = 0;
        uint32_t padding             = 0;
        VkClearColorValue clearValue = {};
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

    // Functions implemented by the class:
    enum class Function
    {
        // Functions implemented in graphics
        ImageClear = 0,
        ImageCopy  = 1,

        // Functions implemented in compute
        ComputeStartIndex   = 2,  // Special value to separate draw and dispatch functions.
        BufferClear         = 2,
        BufferCopy          = 3,
        ConvertVertexBuffer = 4,

        InvalidEnum = 5,
        EnumCount   = 5,
    };

    // Common function that creates the pipeline for the specified function, binds it and prepares
    // the draw/dispatch call.  If function >= ComputeStartIndex, fsCsShader is expected to be a
    // compute shader, vsShader and pipelineDesc should be nullptr, and this will set up a dispatch
    // call. Otherwise fsCsShader is expected to be a fragment shader and this will set up a draw
    // call.
    angle::Result setupProgram(vk::Context *context,
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
    angle::Result ensureResourcesInitialized(vk::Context *context,
                                             Function function,
                                             VkDescriptorPoolSize *setSizes,
                                             size_t setSizesCount,
                                             size_t pushConstantsSize);

    // Initializers corresponding to functions, calling into ensureResourcesInitialized with the
    // appropriate parameters.
    angle::Result ensureBufferClearResourcesInitialized(vk::Context *context);
    angle::Result ensureBufferCopyResourcesInitialized(vk::Context *context);
    angle::Result ensureConvertVertexResourcesInitialized(vk::Context *context);
    angle::Result ensureImageClearResourcesInitialized(vk::Context *context);
    angle::Result ensureImageCopyResourcesInitialized(vk::Context *context);

    angle::Result startRenderPass(ContextVk *contextVk,
                                  vk::ImageHelper *image,
                                  const vk::ImageView *imageView,
                                  const vk::RenderPassDesc &renderPassDesc,
                                  const gl::Rectangle &renderArea,
                                  vk::CommandBuffer **commandBufferOut);

    angle::PackedEnumMap<Function, vk::DescriptorSetLayoutPointerArray> mDescriptorSetLayouts;
    angle::PackedEnumMap<Function, vk::BindingPointer<vk::PipelineLayout>> mPipelineLayouts;
    angle::PackedEnumMap<Function, vk::DynamicDescriptorPool> mDescriptorPools;

    vk::ShaderProgramHelper
        mBufferUtilsPrograms[vk::InternalShader::BufferUtils_comp::kFlagsMask |
                             vk::InternalShader::BufferUtils_comp::kFunctionMask |
                             vk::InternalShader::BufferUtils_comp::kFormatMask];
    vk::ShaderProgramHelper
        mConvertVertexPrograms[vk::InternalShader::ConvertVertex_comp::kFlagsMask |
                               vk::InternalShader::ConvertVertex_comp::kConversionMask];
    vk::ShaderProgramHelper mImageClearProgramVSOnly;
    vk::ShaderProgramHelper
        mImageClearProgram[vk::InternalShader::ImageClear_frag::kAttachmentIndexMask |
                           vk::InternalShader::ImageClear_frag::kFormatMask];
    vk::ShaderProgramHelper mImageCopyPrograms[vk::InternalShader::ImageCopy_frag::kFlagsMask |
                                               vk::InternalShader::ImageCopy_frag::kSrcFormatMask |
                                               vk::InternalShader::ImageCopy_frag::kDestFormatMask];
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_UTILSVK_H_
