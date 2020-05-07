//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// UtilsVk.cpp:
//    Implements the UtilsVk class.
//

#include "libANGLE/renderer/vulkan/UtilsVk.h"

#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/RenderTargetVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"

namespace rx
{

namespace BufferUtils_comp                = vk::InternalShader::BufferUtils_comp;
namespace ConvertVertex_comp              = vk::InternalShader::ConvertVertex_comp;
namespace ImageClear_frag                 = vk::InternalShader::ImageClear_frag;
namespace ImageCopy_frag                  = vk::InternalShader::ImageCopy_frag;
namespace BlitResolve_frag                = vk::InternalShader::BlitResolve_frag;
namespace BlitResolveStencilNoExport_comp = vk::InternalShader::BlitResolveStencilNoExport_comp;
namespace OverlayCull_comp                = vk::InternalShader::OverlayCull_comp;
namespace OverlayDraw_comp                = vk::InternalShader::OverlayDraw_comp;

namespace
{
// All internal shaders assume there is only one descriptor set, indexed at 0
constexpr uint32_t kSetIndex = 0;

constexpr uint32_t kBufferClearOutputBinding                 = 0;
constexpr uint32_t kConvertIndexDestinationBinding           = 0;
constexpr uint32_t kConvertVertexDestinationBinding          = 0;
constexpr uint32_t kConvertVertexSourceBinding               = 1;
constexpr uint32_t kImageCopySourceBinding                   = 0;
constexpr uint32_t kBlitResolveColorOrDepthBinding           = 0;
constexpr uint32_t kBlitResolveStencilBinding                = 1;
constexpr uint32_t kBlitResolveSamplerBinding                = 2;
constexpr uint32_t kBlitResolveStencilNoExportDestBinding    = 0;
constexpr uint32_t kBlitResolveStencilNoExportSrcBinding     = 1;
constexpr uint32_t kBlitResolveStencilNoExportSamplerBinding = 2;
constexpr uint32_t kOverlayCullCulledWidgetsBinding          = 0;
constexpr uint32_t kOverlayCullWidgetCoordsBinding           = 1;
constexpr uint32_t kOverlayDrawOutputBinding                 = 0;
constexpr uint32_t kOverlayDrawTextWidgetsBinding            = 1;
constexpr uint32_t kOverlayDrawGraphWidgetsBinding           = 2;
constexpr uint32_t kOverlayDrawCulledWidgetsBinding          = 3;
constexpr uint32_t kOverlayDrawFontBinding                   = 4;

uint32_t GetBufferUtilsFlags(size_t dispatchSize, const vk::Format &format)
{
    uint32_t flags                    = dispatchSize % 64 == 0 ? BufferUtils_comp::kIsAligned : 0;
    const angle::Format &bufferFormat = format.actualBufferFormat();

    if (bufferFormat.isSint())
    {
        flags |= BufferUtils_comp::kIsSint;
    }
    else if (bufferFormat.isUint())
    {
        flags |= BufferUtils_comp::kIsUint;
    }
    else
    {
        flags |= BufferUtils_comp::kIsFloat;
    }

    return flags;
}

uint32_t GetConvertVertexFlags(const UtilsVk::ConvertVertexParameters &params)
{
    bool srcIsSint  = params.srcFormat->isSint();
    bool srcIsUint  = params.srcFormat->isUint();
    bool srcIsSnorm = params.srcFormat->isSnorm();
    bool srcIsUnorm = params.srcFormat->isUnorm();
    bool srcIsFixed = params.srcFormat->isFixed;
    bool srcIsFloat = params.srcFormat->isFloat();
    bool srcIsA2BGR10 =
        params.srcFormat->vertexAttribType == gl::VertexAttribType::UnsignedInt2101010 ||
        params.srcFormat->vertexAttribType == gl::VertexAttribType::Int2101010;
    bool srcIsRGB10A2 =
        params.srcFormat->vertexAttribType == gl::VertexAttribType::UnsignedInt1010102 ||
        params.srcFormat->vertexAttribType == gl::VertexAttribType::Int1010102;
    bool srcIsHalfFloat = params.srcFormat->isVertexTypeHalfFloat();

    bool destIsSint      = params.destFormat->isSint();
    bool destIsUint      = params.destFormat->isUint();
    bool destIsFloat     = params.destFormat->isFloat();
    bool destIsHalfFloat = params.destFormat->isVertexTypeHalfFloat();

    // Assert on the types to make sure the shader supports its.  These are based on
    // ConvertVertex_comp::Conversion values.
    ASSERT(!destIsSint || srcIsSint);           // If destination is sint, src must be sint too
    ASSERT(!destIsUint || srcIsUint);           // If destination is uint, src must be uint too
    ASSERT(!srcIsFixed || destIsFloat);         // If source is fixed, dest must be float
    ASSERT(srcIsHalfFloat == destIsHalfFloat);  // Both src and dest are half float or neither

    // One of each bool set must be true
    ASSERT(srcIsSint || srcIsUint || srcIsSnorm || srcIsUnorm || srcIsFixed || srcIsFloat);
    ASSERT(destIsSint || destIsUint || destIsFloat);

    // We currently don't have any big-endian devices in the list of supported platforms.  The
    // shader is capable of supporting big-endian architectures, but the relevant flag (IsBigEndian)
    // is not added to the build configuration file (to reduce binary size).  If necessary, add
    // IsBigEndian to ConvertVertex.comp.json and select the appropriate flag based on the
    // endian-ness test here.
    ASSERT(IsLittleEndian());

    uint32_t flags = 0;

    if (srcIsA2BGR10)
    {
        if (srcIsSint && destIsSint)
        {
            flags |= ConvertVertex_comp::kA2BGR10SintToSint;
        }
        else if (srcIsUint && destIsUint)
        {
            flags |= ConvertVertex_comp::kA2BGR10UintToUint;
        }
        else if (srcIsSint)
        {
            flags |= ConvertVertex_comp::kA2BGR10SintToFloat;
        }
        else if (srcIsUint)
        {
            flags |= ConvertVertex_comp::kA2BGR10UintToFloat;
        }
        else if (srcIsSnorm)
        {
            flags |= ConvertVertex_comp::kA2BGR10SnormToFloat;
        }
        else
        {
            UNREACHABLE();
        }
    }
    else if (srcIsRGB10A2)
    {
        if (srcIsSint)
        {
            flags |= ConvertVertex_comp::kRGB10A2SintToFloat;
        }
        else if (srcIsUint)
        {
            flags |= ConvertVertex_comp::kRGB10A2UintToFloat;
        }
        else if (srcIsSnorm)
        {
            flags |= ConvertVertex_comp::kRGB10A2SnormToFloat;
        }
        else if (srcIsUnorm)
        {
            flags |= ConvertVertex_comp::kRGB10A2UnormToFloat;
        }
        else
        {
            UNREACHABLE();
        }
    }
    else if (srcIsHalfFloat && destIsHalfFloat)
    {
        // Note that HalfFloat conversion uses the same shader as Uint.
        flags |= ConvertVertex_comp::kUintToUint;
    }
    else if (srcIsSint && destIsSint)
    {
        flags |= ConvertVertex_comp::kSintToSint;
    }
    else if (srcIsUint && destIsUint)
    {
        flags |= ConvertVertex_comp::kUintToUint;
    }
    else if (srcIsSint)
    {
        flags |= ConvertVertex_comp::kSintToFloat;
    }
    else if (srcIsUint)
    {
        flags |= ConvertVertex_comp::kUintToFloat;
    }
    else if (srcIsSnorm)
    {
        flags |= ConvertVertex_comp::kSnormToFloat;
    }
    else if (srcIsUnorm)
    {
        flags |= ConvertVertex_comp::kUnormToFloat;
    }
    else if (srcIsFixed)
    {
        flags |= ConvertVertex_comp::kFixedToFloat;
    }
    else if (srcIsFloat)
    {
        flags |= ConvertVertex_comp::kFloatToFloat;
    }
    else
    {
        UNREACHABLE();
    }

    return flags;
}

uint32_t GetImageClearFlags(const angle::Format &format, uint32_t attachmentIndex)
{
    constexpr uint32_t kAttachmentFlagStep =
        ImageClear_frag::kAttachment1 - ImageClear_frag::kAttachment0;

    static_assert(gl::IMPLEMENTATION_MAX_DRAW_BUFFERS == 8,
                  "ImageClear shader assumes maximum 8 draw buffers");
    static_assert(
        ImageClear_frag::kAttachment0 + 7 * kAttachmentFlagStep == ImageClear_frag::kAttachment7,
        "ImageClear AttachmentN flag calculation needs correction");

    uint32_t flags = ImageClear_frag::kAttachment0 + attachmentIndex * kAttachmentFlagStep;

    if (format.isSint())
    {
        flags |= ImageClear_frag::kIsSint;
    }
    else if (format.isUint())
    {
        flags |= ImageClear_frag::kIsUint;
    }
    else
    {
        flags |= ImageClear_frag::kIsFloat;
    }

    return flags;
}

uint32_t GetFormatFlags(const angle::Format &format,
                        uint32_t intFlag,
                        uint32_t uintFlag,
                        uint32_t floatFlag)
{
    if (format.isSint())
    {
        return intFlag;
    }
    if (format.isUint())
    {
        return uintFlag;
    }
    return floatFlag;
}

uint32_t GetImageCopyFlags(const vk::Format &srcFormat, const vk::Format &dstFormat)
{
    const angle::Format &srcIntendedFormat = srcFormat.intendedFormat();
    const angle::Format &dstIntendedFormat = dstFormat.intendedFormat();

    uint32_t flags = 0;

    flags |= GetFormatFlags(srcIntendedFormat, ImageCopy_frag::kSrcIsSint,
                            ImageCopy_frag::kSrcIsUint, ImageCopy_frag::kSrcIsFloat);
    flags |= GetFormatFlags(dstIntendedFormat, ImageCopy_frag::kDestIsSint,
                            ImageCopy_frag::kDestIsUint, ImageCopy_frag::kDestIsFloat);

    return flags;
}

uint32_t GetBlitResolveFlags(bool blitColor,
                             bool blitDepth,
                             bool blitStencil,
                             const vk::Format &format)
{
    if (blitColor)
    {
        const angle::Format &intendedFormat = format.intendedFormat();

        return GetFormatFlags(intendedFormat, BlitResolve_frag::kBlitColorInt,
                              BlitResolve_frag::kBlitColorUint, BlitResolve_frag::kBlitColorFloat);
    }

    if (blitDepth)
    {
        if (blitStencil)
        {
            return BlitResolve_frag::kBlitDepthStencil;
        }
        else
        {
            return BlitResolve_frag::kBlitDepth;
        }
    }
    else
    {
        return BlitResolve_frag::kBlitStencil;
    }
}

uint32_t GetFormatDefaultChannelMask(const vk::Format &format)
{
    uint32_t mask = 0;

    const angle::Format &intendedFormat = format.intendedFormat();
    const angle::Format &imageFormat    = format.actualImageFormat();

    // Red can never be introduced due to format emulation (except for luma which is handled
    // especially)
    ASSERT(((intendedFormat.redBits > 0) == (imageFormat.redBits > 0)) || intendedFormat.isLUMA());
    mask |= intendedFormat.greenBits == 0 && imageFormat.greenBits > 0 ? 2 : 0;
    mask |= intendedFormat.blueBits == 0 && imageFormat.blueBits > 0 ? 4 : 0;
    mask |= intendedFormat.alphaBits == 0 && imageFormat.alphaBits > 0 ? 8 : 0;

    return mask;
}

// Calculate the transformation offset for blit/resolve.  See BlitResolve.frag for details on how
// these values are derived.
void CalculateBlitOffset(const UtilsVk::BlitResolveParameters &params, float offset[2])
{
    int srcOffsetFactorX = params.flipX ? -1 : 1;
    int srcOffsetFactorY = params.flipY ? -1 : 1;

    offset[0] = params.destOffset[0] * params.stretch[0] - params.srcOffset[0] * srcOffsetFactorX;
    offset[1] = params.destOffset[1] * params.stretch[1] - params.srcOffset[1] * srcOffsetFactorY;
}

void CalculateResolveOffset(const UtilsVk::BlitResolveParameters &params, int32_t offset[2])
{
    int srcOffsetFactorX = params.flipX ? -1 : 1;
    int srcOffsetFactorY = params.flipY ? -1 : 1;

    // There's no stretching in resolve.
    offset[0] = params.destOffset[0] - params.srcOffset[0] * srcOffsetFactorX;
    offset[1] = params.destOffset[1] - params.srcOffset[1] * srcOffsetFactorY;
}
}  // namespace

UtilsVk::ConvertVertexShaderParams::ConvertVertexShaderParams() = default;

UtilsVk::ImageCopyShaderParams::ImageCopyShaderParams() = default;

UtilsVk::UtilsVk() = default;

UtilsVk::~UtilsVk() = default;

void UtilsVk::destroy(VkDevice device)
{
    for (Function f : angle::AllEnums<Function>())
    {
        for (auto &descriptorSetLayout : mDescriptorSetLayouts[f])
        {
            descriptorSetLayout.reset();
        }
        mPipelineLayouts[f].reset();
        mDescriptorPools[f].destroy(device);
    }

    for (vk::ShaderProgramHelper &program : mBufferUtilsPrograms)
    {
        program.destroy(device);
    }
    for (vk::ShaderProgramHelper &program : mConvertIndexPrograms)
    {
        program.destroy(device);
    }
    for (vk::ShaderProgramHelper &program : mConvertIndirectLineLoopPrograms)
    {
        program.destroy(device);
    }
    for (vk::ShaderProgramHelper &program : mConvertIndexIndirectLineLoopPrograms)
    {
        program.destroy(device);
    }
    for (vk::ShaderProgramHelper &program : mConvertVertexPrograms)
    {
        program.destroy(device);
    }
    mImageClearProgramVSOnly.destroy(device);
    for (vk::ShaderProgramHelper &program : mImageClearProgram)
    {
        program.destroy(device);
    }
    for (vk::ShaderProgramHelper &program : mImageCopyPrograms)
    {
        program.destroy(device);
    }
    for (vk::ShaderProgramHelper &program : mBlitResolvePrograms)
    {
        program.destroy(device);
    }
    for (vk::ShaderProgramHelper &program : mBlitResolveStencilNoExportPrograms)
    {
        program.destroy(device);
    }
    for (vk::ShaderProgramHelper &program : mOverlayCullPrograms)
    {
        program.destroy(device);
    }
    for (vk::ShaderProgramHelper &program : mOverlayDrawPrograms)
    {
        program.destroy(device);
    }

    mPointSampler.destroy(device);
    mLinearSampler.destroy(device);
}

angle::Result UtilsVk::ensureResourcesInitialized(ContextVk *contextVk,
                                                  Function function,
                                                  VkDescriptorPoolSize *setSizes,
                                                  size_t setSizesCount,
                                                  size_t pushConstantsSize)
{
    RendererVk *renderer = contextVk->getRenderer();

    vk::DescriptorSetLayoutDesc descriptorSetDesc;
    bool isCompute = function >= Function::ComputeStartIndex;
    const VkShaderStageFlags descStages =
        isCompute ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;

    uint32_t currentBinding = 0;
    for (size_t i = 0; i < setSizesCount; ++i)
    {
        descriptorSetDesc.update(currentBinding, setSizes[i].type, setSizes[i].descriptorCount,
                                 descStages);
        currentBinding += setSizes[i].descriptorCount;
    }

    ANGLE_TRY(renderer->getDescriptorSetLayout(contextVk, descriptorSetDesc,
                                               &mDescriptorSetLayouts[function][kSetIndex]));

    gl::ShaderType pushConstantsShaderStage =
        isCompute ? gl::ShaderType::Compute : gl::ShaderType::Fragment;

    // Corresponding pipeline layouts:
    vk::PipelineLayoutDesc pipelineLayoutDesc;

    pipelineLayoutDesc.updateDescriptorSetLayout(kSetIndex, descriptorSetDesc);
    if (pushConstantsSize)
    {
        pipelineLayoutDesc.updatePushConstantRange(pushConstantsShaderStage, 0,
                                                   static_cast<uint32_t>(pushConstantsSize));
    }

    ANGLE_TRY(renderer->getPipelineLayout(contextVk, pipelineLayoutDesc,
                                          mDescriptorSetLayouts[function],
                                          &mPipelineLayouts[function]));

    if (setSizesCount > 0)
    {
        ANGLE_TRY(mDescriptorPools[function].init(contextVk, setSizes,
                                                  static_cast<uint32_t>(setSizesCount)));
    }

    return angle::Result::Continue;
}

angle::Result UtilsVk::ensureBufferClearResourcesInitialized(ContextVk *contextVk)
{
    if (mPipelineLayouts[Function::BufferClear].valid())
    {
        return angle::Result::Continue;
    }

    VkDescriptorPoolSize setSizes[1] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1},
    };

    return ensureResourcesInitialized(contextVk, Function::BufferClear, setSizes,
                                      ArraySize(setSizes), sizeof(BufferUtilsShaderParams));
}

angle::Result UtilsVk::ensureConvertIndexResourcesInitialized(ContextVk *contextVk)
{
    if (mPipelineLayouts[Function::ConvertIndexBuffer].valid())
    {
        return angle::Result::Continue;
    }

    VkDescriptorPoolSize setSizes[2] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
    };

    return ensureResourcesInitialized(contextVk, Function::ConvertIndexBuffer, setSizes,
                                      ArraySize(setSizes), sizeof(ConvertIndexShaderParams));
}

angle::Result UtilsVk::ensureConvertIndexIndirectResourcesInitialized(ContextVk *contextVk)
{
    if (mPipelineLayouts[Function::ConvertIndexIndirectBuffer].valid())
    {
        return angle::Result::Continue;
    }

    VkDescriptorPoolSize setSizes[4] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // dest index buffer
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // source index buffer
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // src indirect buffer
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // dest indirect buffer
    };

    return ensureResourcesInitialized(contextVk, Function::ConvertIndexIndirectBuffer, setSizes,
                                      ArraySize(setSizes),
                                      sizeof(ConvertIndexIndirectShaderParams));
}

angle::Result UtilsVk::ensureConvertIndexIndirectLineLoopResourcesInitialized(ContextVk *contextVk)
{
    if (mPipelineLayouts[Function::ConvertIndexIndirectLineLoopBuffer].valid())
    {
        return angle::Result::Continue;
    }

    VkDescriptorPoolSize setSizes[4] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // cmd buffer
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // dest cmd buffer
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // source index buffer
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // dest index buffer
    };

    return ensureResourcesInitialized(contextVk, Function::ConvertIndexIndirectLineLoopBuffer,
                                      setSizes, ArraySize(setSizes),
                                      sizeof(ConvertIndexIndirectLineLoopShaderParams));
}

angle::Result UtilsVk::ensureConvertIndirectLineLoopResourcesInitialized(ContextVk *contextVk)
{
    if (mPipelineLayouts[Function::ConvertIndirectLineLoopBuffer].valid())
    {
        return angle::Result::Continue;
    }

    VkDescriptorPoolSize setSizes[3] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // cmd buffer
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // dest cmd buffer
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // dest index buffer
    };

    return ensureResourcesInitialized(contextVk, Function::ConvertIndirectLineLoopBuffer, setSizes,
                                      ArraySize(setSizes),
                                      sizeof(ConvertIndirectLineLoopShaderParams));
}

angle::Result UtilsVk::ensureConvertVertexResourcesInitialized(ContextVk *contextVk)
{
    if (mPipelineLayouts[Function::ConvertVertexBuffer].valid())
    {
        return angle::Result::Continue;
    }

    VkDescriptorPoolSize setSizes[2] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
    };

    return ensureResourcesInitialized(contextVk, Function::ConvertVertexBuffer, setSizes,
                                      ArraySize(setSizes), sizeof(ConvertVertexShaderParams));
}

angle::Result UtilsVk::ensureImageClearResourcesInitialized(ContextVk *contextVk)
{
    if (mPipelineLayouts[Function::ImageClear].valid())
    {
        return angle::Result::Continue;
    }

    // The shader does not use any descriptor sets.
    return ensureResourcesInitialized(contextVk, Function::ImageClear, nullptr, 0,
                                      sizeof(ImageClearShaderParams));
}

angle::Result UtilsVk::ensureImageCopyResourcesInitialized(ContextVk *contextVk)
{
    if (mPipelineLayouts[Function::ImageCopy].valid())
    {
        return angle::Result::Continue;
    }

    VkDescriptorPoolSize setSizes[1] = {
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1},
    };

    return ensureResourcesInitialized(contextVk, Function::ImageCopy, setSizes, ArraySize(setSizes),
                                      sizeof(ImageCopyShaderParams));
}

angle::Result UtilsVk::ensureBlitResolveResourcesInitialized(ContextVk *contextVk)
{
    if (!mPipelineLayouts[Function::BlitResolve].valid())
    {
        VkDescriptorPoolSize setSizes[3] = {
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1},
            {VK_DESCRIPTOR_TYPE_SAMPLER, 1},
        };

        ANGLE_TRY(ensureResourcesInitialized(contextVk, Function::BlitResolve, setSizes,
                                             ArraySize(setSizes), sizeof(BlitResolveShaderParams)));
    }

    return ensureSamplersInitialized(contextVk);
}

angle::Result UtilsVk::ensureBlitResolveStencilNoExportResourcesInitialized(ContextVk *contextVk)
{
    if (!mPipelineLayouts[Function::BlitResolveStencilNoExport].valid())
    {
        VkDescriptorPoolSize setSizes[3] = {
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1},
            {VK_DESCRIPTOR_TYPE_SAMPLER, 1},
        };

        ANGLE_TRY(ensureResourcesInitialized(contextVk, Function::BlitResolveStencilNoExport,
                                             setSizes, ArraySize(setSizes),
                                             sizeof(BlitResolveStencilNoExportShaderParams)));
    }

    return ensureSamplersInitialized(contextVk);
}

angle::Result UtilsVk::ensureOverlayCullResourcesInitialized(ContextVk *contextVk)
{
    if (mPipelineLayouts[Function::OverlayCull].valid())
    {
        return angle::Result::Continue;
    }

    VkDescriptorPoolSize setSizes[2] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
    };

    return ensureResourcesInitialized(contextVk, Function::OverlayCull, setSizes,
                                      ArraySize(setSizes), 0);
}

angle::Result UtilsVk::ensureOverlayDrawResourcesInitialized(ContextVk *contextVk)
{
    if (!mPipelineLayouts[Function::OverlayDraw].valid())
    {
        VkDescriptorPoolSize setSizes[5] = {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},  {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}, {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1},
        };

        ANGLE_TRY(ensureResourcesInitialized(contextVk, Function::OverlayDraw, setSizes,
                                             ArraySize(setSizes), sizeof(OverlayDrawShaderParams)));
    }

    return ensureSamplersInitialized(contextVk);
}

angle::Result UtilsVk::ensureSamplersInitialized(ContextVk *contextVk)
{
    VkSamplerCreateInfo samplerInfo     = {};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.flags                   = 0;
    samplerInfo.magFilter               = VK_FILTER_NEAREST;
    samplerInfo.minFilter               = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipLodBias              = 0.0f;
    samplerInfo.anisotropyEnable        = VK_FALSE;
    samplerInfo.maxAnisotropy           = 1;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerInfo.minLod                  = 0;
    samplerInfo.maxLod                  = 0;
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    if (!mPointSampler.valid())
    {
        ANGLE_VK_TRY(contextVk, mPointSampler.init(contextVk->getDevice(), samplerInfo));
    }

    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;

    if (!mLinearSampler.valid())
    {
        ANGLE_VK_TRY(contextVk, mLinearSampler.init(contextVk->getDevice(), samplerInfo));
    }

    return angle::Result::Continue;
}

angle::Result UtilsVk::setupProgram(ContextVk *contextVk,
                                    Function function,
                                    vk::RefCounted<vk::ShaderAndSerial> *fsCsShader,
                                    vk::RefCounted<vk::ShaderAndSerial> *vsShader,
                                    vk::ShaderProgramHelper *program,
                                    const vk::GraphicsPipelineDesc *pipelineDesc,
                                    const VkDescriptorSet descriptorSet,
                                    const void *pushConstants,
                                    size_t pushConstantsSize,
                                    vk::CommandBuffer *commandBuffer)
{
    RendererVk *renderer = contextVk->getRenderer();

    const bool isCompute = function >= Function::ComputeStartIndex;
    const VkShaderStageFlags pushConstantsShaderStage =
        isCompute ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
    const VkPipelineBindPoint pipelineBindPoint =
        isCompute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;

    // If compute, vsShader and pipelineDesc should be nullptr, and if not compute they shouldn't
    // be.
    ASSERT(isCompute != (vsShader && pipelineDesc));

    const vk::BindingPointer<vk::PipelineLayout> &pipelineLayout = mPipelineLayouts[function];

    Serial serial = contextVk->getCurrentQueueSerial();

    if (isCompute)
    {
        vk::PipelineAndSerial *pipelineAndSerial;
        program->setShader(gl::ShaderType::Compute, fsCsShader);
        ANGLE_TRY(program->getComputePipeline(contextVk, pipelineLayout.get(), &pipelineAndSerial));
        pipelineAndSerial->updateSerial(serial);
        commandBuffer->bindComputePipeline(pipelineAndSerial->get());
    }
    else
    {
        program->setShader(gl::ShaderType::Vertex, vsShader);
        if (fsCsShader)
        {
            program->setShader(gl::ShaderType::Fragment, fsCsShader);
        }

        // This value is not used but is passed to getGraphicsPipeline to avoid a nullptr check.
        const vk::GraphicsPipelineDesc *descPtr;
        vk::PipelineHelper *helper;
        vk::PipelineCache *pipelineCache = nullptr;
        ANGLE_TRY(renderer->getPipelineCache(&pipelineCache));
        ANGLE_TRY(program->getGraphicsPipeline(contextVk, &contextVk->getRenderPassCache(),
                                               *pipelineCache, serial, pipelineLayout.get(),
                                               *pipelineDesc, gl::AttributesMask(),
                                               gl::ComponentTypeMask(), &descPtr, &helper));
        helper->updateSerial(serial);
        commandBuffer->bindGraphicsPipeline(helper->getPipeline());
    }

    if (descriptorSet != VK_NULL_HANDLE)
    {
        commandBuffer->bindDescriptorSets(pipelineLayout.get(), pipelineBindPoint, 0, 1,
                                          &descriptorSet, 0, nullptr);
        if (isCompute)
        {
            contextVk->invalidateComputeDescriptorSet(0);
        }
        else
        {
            contextVk->invalidateGraphicsDescriptorSet(0);
        }
    }

    if (pushConstants)
    {
        commandBuffer->pushConstants(pipelineLayout.get(), pushConstantsShaderStage, 0,
                                     static_cast<uint32_t>(pushConstantsSize), pushConstants);
    }

    return angle::Result::Continue;
}

angle::Result UtilsVk::clearBuffer(ContextVk *contextVk,
                                   vk::BufferHelper *dest,
                                   const ClearParameters &params)
{
    ANGLE_TRY(ensureBufferClearResourcesInitialized(contextVk));

    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(dest->recordCommands(contextVk, &commandBuffer));

    // Tell dest it's being written to.
    dest->onSelfReadWrite(contextVk, VK_ACCESS_SHADER_WRITE_BIT);

    const vk::Format &destFormat = dest->getViewFormat();

    uint32_t flags = BufferUtils_comp::kIsClear | GetBufferUtilsFlags(params.size, destFormat);

    BufferUtilsShaderParams shaderParams;
    shaderParams.destOffset = static_cast<uint32_t>(params.offset);
    shaderParams.size       = static_cast<uint32_t>(params.size);
    shaderParams.clearValue = params.clearValue;

    VkDescriptorSet descriptorSet;
    vk::RefCountedDescriptorPoolBinding descriptorPoolBinding;
    ANGLE_TRY(allocateDescriptorSet(contextVk, Function::BufferClear, &descriptorPoolBinding,
                                    &descriptorSet));

    VkWriteDescriptorSet writeInfo = {};

    writeInfo.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfo.dstSet           = descriptorSet;
    writeInfo.dstBinding       = kBufferClearOutputBinding;
    writeInfo.descriptorCount  = 1;
    writeInfo.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    writeInfo.pTexelBufferView = dest->getBufferView().ptr();

    vkUpdateDescriptorSets(contextVk->getDevice(), 1, &writeInfo, 0, nullptr);

    vk::RefCounted<vk::ShaderAndSerial> *shader = nullptr;
    ANGLE_TRY(contextVk->getShaderLibrary().getBufferUtils_comp(contextVk, flags, &shader));

    ANGLE_TRY(setupProgram(contextVk, Function::BufferClear, shader, nullptr,
                           &mBufferUtilsPrograms[flags], nullptr, descriptorSet, &shaderParams,
                           sizeof(shaderParams), commandBuffer));

    commandBuffer->dispatch(UnsignedCeilDivide(static_cast<uint32_t>(params.size), 64), 1, 1);

    descriptorPoolBinding.reset();

    return angle::Result::Continue;
}

angle::Result UtilsVk::convertIndexBuffer(ContextVk *contextVk,
                                          vk::BufferHelper *dest,
                                          vk::BufferHelper *src,
                                          const ConvertIndexParameters &params)
{
    ANGLE_TRY(ensureConvertIndexResourcesInitialized(contextVk));

    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(dest->recordCommands(contextVk, &commandBuffer));

    // Tell src we are going to read from it and dest it's being written to.
    src->onReadByBuffer(contextVk, dest, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT);

    VkDescriptorSet descriptorSet;
    vk::RefCountedDescriptorPoolBinding descriptorPoolBinding;
    ANGLE_TRY(allocateDescriptorSet(contextVk, Function::ConvertIndexBuffer, &descriptorPoolBinding,
                                    &descriptorSet));

    std::array<VkDescriptorBufferInfo, 2> buffers = {{
        {dest->getBuffer().getHandle(), 0, VK_WHOLE_SIZE},
        {src->getBuffer().getHandle(), 0, VK_WHOLE_SIZE},
    }};

    VkWriteDescriptorSet writeInfo = {};
    writeInfo.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfo.dstSet               = descriptorSet;
    writeInfo.dstBinding           = kConvertIndexDestinationBinding;
    writeInfo.descriptorCount      = 2;
    writeInfo.descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeInfo.pBufferInfo          = buffers.data();

    vkUpdateDescriptorSets(contextVk->getDevice(), 1, &writeInfo, 0, nullptr);

    ConvertIndexShaderParams shaderParams = {params.srcOffset, params.dstOffset >> 2,
                                             params.maxIndex, 0};

    uint32_t flags = 0;
    if (contextVk->getState().isPrimitiveRestartEnabled())
    {
        flags |= vk::InternalShader::ConvertIndex_comp::kIsPrimitiveRestartEnabled;
    }

    vk::RefCounted<vk::ShaderAndSerial> *shader = nullptr;
    ANGLE_TRY(contextVk->getShaderLibrary().getConvertIndex_comp(contextVk, flags, &shader));

    ANGLE_TRY(setupProgram(contextVk, Function::ConvertIndexBuffer, shader, nullptr,
                           &mConvertIndexPrograms[flags], nullptr, descriptorSet, &shaderParams,
                           sizeof(ConvertIndexShaderParams), commandBuffer));

    constexpr uint32_t kInvocationsPerGroup = 64;
    constexpr uint32_t kInvocationsPerIndex = 2;
    const uint32_t kIndexCount              = params.maxIndex - params.srcOffset;
    const uint32_t kGroupCount =
        UnsignedCeilDivide(kIndexCount * kInvocationsPerIndex, kInvocationsPerGroup);
    commandBuffer->dispatch(kGroupCount, 1, 1);

    descriptorPoolBinding.reset();

    return angle::Result::Continue;
}

angle::Result UtilsVk::convertIndexIndirectBuffer(ContextVk *contextVk,
                                                  vk::BufferHelper *srcIndirectBuf,
                                                  vk::BufferHelper *srcIndexBuf,
                                                  vk::BufferHelper *dstIndirectBuf,
                                                  vk::BufferHelper *dstIndexBuf,
                                                  const ConvertIndexIndirectParameters &params)
{
    ANGLE_TRY(ensureConvertIndexIndirectResourcesInitialized(contextVk));

    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(dstIndexBuf->recordCommands(contextVk, &commandBuffer));

    // Tell src we are going to read from it and dest it's being written to.
    srcIndexBuf->onReadByBuffer(contextVk, dstIndexBuf, VK_ACCESS_SHADER_READ_BIT,
                                VK_ACCESS_SHADER_WRITE_BIT);
    srcIndirectBuf->onReadByBuffer(contextVk, dstIndexBuf, VK_ACCESS_SHADER_READ_BIT,
                                   VK_ACCESS_SHADER_WRITE_BIT);

    ANGLE_TRY(dstIndirectBuf->recordCommands(contextVk, &commandBuffer));
    srcIndirectBuf->onReadByBuffer(contextVk, dstIndirectBuf, VK_ACCESS_SHADER_READ_BIT,
                                   VK_ACCESS_SHADER_WRITE_BIT);

    VkDescriptorSet descriptorSet;
    vk::RefCountedDescriptorPoolBinding descriptorPoolBinding;
    ANGLE_TRY(allocateDescriptorSet(contextVk, Function::ConvertIndexIndirectBuffer,
                                    &descriptorPoolBinding, &descriptorSet));

    std::array<VkDescriptorBufferInfo, 4> buffers = {{
        {dstIndexBuf->getBuffer().getHandle(), 0, VK_WHOLE_SIZE},
        {srcIndexBuf->getBuffer().getHandle(), 0, VK_WHOLE_SIZE},
        {srcIndirectBuf->getBuffer().getHandle(), 0, VK_WHOLE_SIZE},
        {dstIndirectBuf->getBuffer().getHandle(), 0, VK_WHOLE_SIZE},
    }};

    VkWriteDescriptorSet writeInfo = {};
    writeInfo.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfo.dstSet               = descriptorSet;
    writeInfo.dstBinding           = kConvertIndexDestinationBinding;
    writeInfo.descriptorCount      = 4;
    writeInfo.descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeInfo.pBufferInfo          = buffers.data();

    vkUpdateDescriptorSets(contextVk->getDevice(), 1, &writeInfo, 0, nullptr);

    ConvertIndexIndirectShaderParams shaderParams = {params.srcIndirectBufOffset >> 2,
                                                     params.dstIndexBufOffset >> 2, params.maxIndex,
                                                     params.dstIndirectBufOffset >> 2};

    uint32_t flags = vk::InternalShader::ConvertIndex_comp::kIsIndirect;
    if (contextVk->getState().isPrimitiveRestartEnabled())
    {
        flags |= vk::InternalShader::ConvertIndex_comp::kIsPrimitiveRestartEnabled;
    }

    vk::RefCounted<vk::ShaderAndSerial> *shader = nullptr;
    ANGLE_TRY(contextVk->getShaderLibrary().getConvertIndex_comp(contextVk, flags, &shader));

    ANGLE_TRY(setupProgram(contextVk, Function::ConvertIndexIndirectBuffer, shader, nullptr,
                           &mConvertIndexPrograms[flags], nullptr, descriptorSet, &shaderParams,
                           sizeof(ConvertIndexIndirectShaderParams), commandBuffer));

    constexpr uint32_t kInvocationsPerGroup = 64;
    constexpr uint32_t kInvocationsPerIndex = 2;
    const uint32_t kIndexCount              = params.maxIndex;
    const uint32_t kGroupCount =
        UnsignedCeilDivide(kIndexCount * kInvocationsPerIndex, kInvocationsPerGroup);
    commandBuffer->dispatch(kGroupCount, 1, 1);

    descriptorPoolBinding.reset();

    return angle::Result::Continue;
}

angle::Result UtilsVk::convertLineLoopIndexIndirectBuffer(
    ContextVk *contextVk,
    vk::BufferHelper *srcIndirectBuffer,
    vk::BufferHelper *dstIndirectBuffer,
    vk::BufferHelper *dstIndexBuffer,
    vk::BufferHelper *srcIndexBuffer,
    const ConvertLineLoopIndexIndirectParameters &params)
{
    ANGLE_TRY(ensureConvertIndexIndirectLineLoopResourcesInitialized(contextVk));

    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(dstIndexBuffer->recordCommands(contextVk, &commandBuffer));

    // Tell src we are going to read from it and dest it's being written to.
    srcIndexBuffer->onReadByBuffer(contextVk, dstIndexBuffer, VK_ACCESS_SHADER_READ_BIT,
                                   VK_ACCESS_SHADER_WRITE_BIT);
    srcIndirectBuffer->onReadByBuffer(contextVk, dstIndexBuffer, VK_ACCESS_SHADER_READ_BIT,
                                      VK_ACCESS_SHADER_WRITE_BIT);

    ANGLE_TRY(dstIndirectBuffer->recordCommands(contextVk, &commandBuffer));

    srcIndirectBuffer->onReadByBuffer(contextVk, dstIndirectBuffer, VK_ACCESS_SHADER_READ_BIT,
                                      VK_ACCESS_SHADER_WRITE_BIT);

    VkDescriptorSet descriptorSet;
    vk::RefCountedDescriptorPoolBinding descriptorPoolBinding;
    ANGLE_TRY(allocateDescriptorSet(contextVk, Function::ConvertIndexIndirectLineLoopBuffer,
                                    &descriptorPoolBinding, &descriptorSet));

    std::array<VkDescriptorBufferInfo, 4> buffers = {{
        {dstIndexBuffer->getBuffer().getHandle(), 0, VK_WHOLE_SIZE},
        {srcIndexBuffer->getBuffer().getHandle(), 0, VK_WHOLE_SIZE},
        {srcIndirectBuffer->getBuffer().getHandle(), 0, VK_WHOLE_SIZE},
        {dstIndirectBuffer->getBuffer().getHandle(), 0, VK_WHOLE_SIZE},
    }};

    VkWriteDescriptorSet writeInfo = {};
    writeInfo.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfo.dstSet               = descriptorSet;
    writeInfo.dstBinding           = kConvertIndexDestinationBinding;
    writeInfo.descriptorCount      = 4;
    writeInfo.descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeInfo.pBufferInfo          = buffers.data();

    vkUpdateDescriptorSets(contextVk->getDevice(), 1, &writeInfo, 0, nullptr);

    ConvertIndexIndirectLineLoopShaderParams shaderParams = {
        params.indirectBufferOffset >> 2, params.dstIndirectBufferOffset >> 2,
        params.dstIndexBufferOffset >> 2, contextVk->getState().isPrimitiveRestartEnabled()};

    uint32_t flags = 0;
    if (params.is32Bit)
    {
        flags |= vk::InternalShader::ConvertIndexIndirectLineLoop_comp::kIs32Bit;
    }

    vk::RefCounted<vk::ShaderAndSerial> *shader = nullptr;
    ANGLE_TRY(contextVk->getShaderLibrary().getConvertIndexIndirectLineLoop_comp(contextVk, flags,
                                                                                 &shader));

    ANGLE_TRY(setupProgram(contextVk, Function::ConvertIndexIndirectLineLoopBuffer, shader, nullptr,
                           &mConvertIndexIndirectLineLoopPrograms[flags], nullptr, descriptorSet,
                           &shaderParams, sizeof(ConvertIndexIndirectLineLoopShaderParams),
                           commandBuffer));

    commandBuffer->dispatch(1, 1, 1);

    descriptorPoolBinding.reset();

    return angle::Result::Continue;
}

angle::Result UtilsVk::convertLineLoopArrayIndirectBuffer(
    ContextVk *contextVk,
    vk::BufferHelper *srcIndirectBuffer,
    vk::BufferHelper *destIndirectBuffer,
    vk::BufferHelper *destIndexBuffer,
    const ConvertLineLoopArrayIndirectParameters &params)
{
    ANGLE_TRY(ensureConvertIndirectLineLoopResourcesInitialized(contextVk));

    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(destIndexBuffer->recordCommands(contextVk, &commandBuffer));

    // Tell src we are going to read from it and dest it's being written to.
    srcIndirectBuffer->onReadByBuffer(contextVk, destIndexBuffer, VK_ACCESS_SHADER_READ_BIT,
                                      VK_ACCESS_SHADER_WRITE_BIT);

    ANGLE_TRY(destIndirectBuffer->recordCommands(contextVk, &commandBuffer));

    srcIndirectBuffer->onReadByBuffer(contextVk, destIndirectBuffer, VK_ACCESS_SHADER_READ_BIT,
                                      VK_ACCESS_SHADER_WRITE_BIT);

    VkDescriptorSet descriptorSet;
    vk::RefCountedDescriptorPoolBinding descriptorPoolBinding;
    ANGLE_TRY(allocateDescriptorSet(contextVk, Function::ConvertIndirectLineLoopBuffer,
                                    &descriptorPoolBinding, &descriptorSet));

    std::array<VkDescriptorBufferInfo, 3> buffers = {{
        {srcIndirectBuffer->getBuffer().getHandle(), 0, VK_WHOLE_SIZE},
        {destIndirectBuffer->getBuffer().getHandle(), 0, VK_WHOLE_SIZE},
        {destIndexBuffer->getBuffer().getHandle(), 0, VK_WHOLE_SIZE},
    }};

    VkWriteDescriptorSet writeInfo = {};
    writeInfo.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfo.dstSet               = descriptorSet;
    writeInfo.dstBinding           = kConvertIndexDestinationBinding;
    writeInfo.descriptorCount      = 3;
    writeInfo.descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeInfo.pBufferInfo          = buffers.data();

    vkUpdateDescriptorSets(contextVk->getDevice(), 1, &writeInfo, 0, nullptr);

    ConvertIndirectLineLoopShaderParams shaderParams = {params.indirectBufferOffset >> 2,
                                                        params.dstIndirectBufferOffset >> 2,
                                                        params.dstIndexBufferOffset >> 2};

    uint32_t flags = 0;

    vk::RefCounted<vk::ShaderAndSerial> *shader = nullptr;
    ANGLE_TRY(
        contextVk->getShaderLibrary().getConvertIndirectLineLoop_comp(contextVk, flags, &shader));

    ANGLE_TRY(setupProgram(contextVk, Function::ConvertIndirectLineLoopBuffer, shader, nullptr,
                           &mConvertIndirectLineLoopPrograms[flags], nullptr, descriptorSet,
                           &shaderParams, sizeof(ConvertIndirectLineLoopShaderParams),
                           commandBuffer));

    commandBuffer->dispatch(1, 1, 1);

    descriptorPoolBinding.reset();

    return angle::Result::Continue;
}

angle::Result UtilsVk::convertVertexBuffer(ContextVk *contextVk,
                                           vk::BufferHelper *dest,
                                           vk::BufferHelper *src,
                                           const ConvertVertexParameters &params)
{
    ANGLE_TRY(ensureConvertVertexResourcesInitialized(contextVk));

    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(dest->recordCommands(contextVk, &commandBuffer));

    // Tell src we are going to read from it and dest it's being written to.
    src->onReadByBuffer(contextVk, dest, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT);

    ConvertVertexShaderParams shaderParams;
    shaderParams.Ns = params.srcFormat->channelCount;
    shaderParams.Bs = params.srcFormat->pixelBytes / params.srcFormat->channelCount;
    shaderParams.Ss = static_cast<uint32_t>(params.srcStride);
    shaderParams.Nd = params.destFormat->channelCount;
    shaderParams.Bd = params.destFormat->pixelBytes / params.destFormat->channelCount;
    shaderParams.Sd = shaderParams.Nd * shaderParams.Bd;
    // The component size is expected to either be 1, 2 or 4 bytes.
    ASSERT(4 % shaderParams.Bs == 0);
    ASSERT(4 % shaderParams.Bd == 0);
    shaderParams.Es = 4 / shaderParams.Bs;
    shaderParams.Ed = 4 / shaderParams.Bd;
    // Total number of output components is simply the number of vertices by number of components in
    // each.
    shaderParams.componentCount = static_cast<uint32_t>(params.vertexCount * shaderParams.Nd);
    // Total number of 4-byte outputs is the number of components divided by how many components can
    // fit in a 4-byte value.  Note that this value is also the invocation size of the shader.
    shaderParams.outputCount = shaderParams.componentCount / shaderParams.Ed;
    shaderParams.srcOffset   = static_cast<uint32_t>(params.srcOffset);
    shaderParams.destOffset  = static_cast<uint32_t>(params.destOffset);

    uint32_t flags = GetConvertVertexFlags(params);

    VkDescriptorSet descriptorSet;
    vk::RefCountedDescriptorPoolBinding descriptorPoolBinding;
    ANGLE_TRY(allocateDescriptorSet(contextVk, Function::ConvertVertexBuffer,
                                    &descriptorPoolBinding, &descriptorSet));

    VkWriteDescriptorSet writeInfo    = {};
    VkDescriptorBufferInfo buffers[2] = {
        {dest->getBuffer().getHandle(), 0, VK_WHOLE_SIZE},
        {src->getBuffer().getHandle(), 0, VK_WHOLE_SIZE},
    };
    static_assert(kConvertVertexDestinationBinding + 1 == kConvertVertexSourceBinding,
                  "Update write info");

    writeInfo.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfo.dstSet          = descriptorSet;
    writeInfo.dstBinding      = kConvertVertexDestinationBinding;
    writeInfo.descriptorCount = 2;
    writeInfo.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeInfo.pBufferInfo     = buffers;

    vkUpdateDescriptorSets(contextVk->getDevice(), 1, &writeInfo, 0, nullptr);

    vk::RefCounted<vk::ShaderAndSerial> *shader = nullptr;
    ANGLE_TRY(contextVk->getShaderLibrary().getConvertVertex_comp(contextVk, flags, &shader));

    ANGLE_TRY(setupProgram(contextVk, Function::ConvertVertexBuffer, shader, nullptr,
                           &mConvertVertexPrograms[flags], nullptr, descriptorSet, &shaderParams,
                           sizeof(shaderParams), commandBuffer));

    commandBuffer->dispatch(UnsignedCeilDivide(shaderParams.outputCount, 64), 1, 1);

    descriptorPoolBinding.reset();

    return angle::Result::Continue;
}

angle::Result UtilsVk::startRenderPass(ContextVk *contextVk,
                                       vk::ImageHelper *image,
                                       const vk::ImageView *imageView,
                                       const vk::RenderPassDesc &renderPassDesc,
                                       const gl::Rectangle &renderArea,
                                       vk::CommandBuffer **commandBufferOut)
{
    vk::RenderPass *compatibleRenderPass = nullptr;
    ANGLE_TRY(contextVk->getCompatibleRenderPass(renderPassDesc, &compatibleRenderPass));

    VkFramebufferCreateInfo framebufferInfo = {};

    // Minimize the framebuffer coverage to only cover up to the render area.
    framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.flags           = 0;
    framebufferInfo.renderPass      = compatibleRenderPass->getHandle();
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments    = imageView->ptr();
    framebufferInfo.width           = renderArea.x + renderArea.width;
    framebufferInfo.height          = renderArea.y + renderArea.height;
    framebufferInfo.layers          = 1;

    vk::Framebuffer framebuffer;
    ANGLE_VK_TRY(contextVk, framebuffer.init(contextVk->getDevice(), framebufferInfo));

    vk::AttachmentOpsArray renderPassAttachmentOps;
    std::vector<VkClearValue> clearValues = {{}};
    ASSERT(clearValues.size() == 1);

    renderPassAttachmentOps.initWithLoadStore(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    ANGLE_TRY(image->beginRenderPass(contextVk, framebuffer, renderArea, renderPassDesc,
                                     renderPassAttachmentOps, clearValues, commandBufferOut));

    contextVk->addGarbage(&framebuffer);

    return angle::Result::Continue;
}

angle::Result UtilsVk::clearFramebuffer(ContextVk *contextVk,
                                        FramebufferVk *framebuffer,
                                        const ClearFramebufferParameters &params)
{
    ANGLE_TRY(ensureImageClearResourcesInitialized(contextVk));

    const gl::Rectangle &scissoredRenderArea = params.clearArea;

    vk::CommandBuffer *commandBuffer;
    if (!framebuffer->appendToStartedRenderPass(&contextVk->getResourceUseList(),
                                                scissoredRenderArea, &commandBuffer))
    {
        ANGLE_TRY(framebuffer->startNewRenderPass(contextVk, scissoredRenderArea, &commandBuffer));
    }

    ImageClearShaderParams shaderParams;
    shaderParams.clearValue = params.colorClearValue;

    vk::GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.initDefaults();
    pipelineDesc.setCullMode(VK_CULL_MODE_NONE);
    pipelineDesc.setColorWriteMask(0, gl::DrawBufferMask());
    pipelineDesc.setSingleColorWriteMask(params.colorAttachmentIndexGL, params.colorMaskFlags);
    pipelineDesc.setRasterizationSamples(framebuffer->getSamples());
    pipelineDesc.setRenderPassDesc(framebuffer->getRenderPassDesc());
    // Note: depth test is disabled by default so this should be unnecessary, but works around an
    // Intel bug on windows.  http://anglebug.com/3348
    pipelineDesc.setDepthWriteEnabled(false);

    // Clear stencil by enabling stencil write with the right mask.
    if (params.clearStencil)
    {
        const uint8_t compareMask       = 0xFF;
        const uint8_t clearStencilValue = params.stencilClearValue;

        pipelineDesc.setStencilTestEnabled(true);
        pipelineDesc.setStencilFrontFuncs(clearStencilValue, VK_COMPARE_OP_ALWAYS, compareMask);
        pipelineDesc.setStencilBackFuncs(clearStencilValue, VK_COMPARE_OP_ALWAYS, compareMask);
        pipelineDesc.setStencilFrontOps(VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE,
                                        VK_STENCIL_OP_REPLACE);
        pipelineDesc.setStencilBackOps(VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE,
                                       VK_STENCIL_OP_REPLACE);
        pipelineDesc.setStencilFrontWriteMask(params.stencilMask);
        pipelineDesc.setStencilBackWriteMask(params.stencilMask);
    }

    VkViewport viewport;
    gl::Rectangle completeRenderArea = framebuffer->getCompleteRenderArea();
    bool invertViewport              = contextVk->isViewportFlipEnabledForDrawFBO();
    gl_vk::GetViewport(completeRenderArea, 0.0f, 1.0f, invertViewport, completeRenderArea.height,
                       &viewport);
    pipelineDesc.setViewport(viewport);

    pipelineDesc.setScissor(gl_vk::GetRect(params.clearArea));

    vk::ShaderLibrary &shaderLibrary                    = contextVk->getShaderLibrary();
    vk::RefCounted<vk::ShaderAndSerial> *vertexShader   = nullptr;
    vk::RefCounted<vk::ShaderAndSerial> *fragmentShader = nullptr;
    vk::ShaderProgramHelper *imageClearProgram          = &mImageClearProgramVSOnly;

    ANGLE_TRY(shaderLibrary.getFullScreenQuad_vert(contextVk, 0, &vertexShader));
    if (params.clearColor)
    {
        uint32_t flags = GetImageClearFlags(*params.colorFormat, params.colorAttachmentIndexGL);
        ANGLE_TRY(shaderLibrary.getImageClear_frag(contextVk, flags, &fragmentShader));
        imageClearProgram = &mImageClearProgram[flags];
    }

    ANGLE_TRY(setupProgram(contextVk, Function::ImageClear, fragmentShader, vertexShader,
                           imageClearProgram, &pipelineDesc, VK_NULL_HANDLE, &shaderParams,
                           sizeof(shaderParams), commandBuffer));
    commandBuffer->draw(6, 0);
    return angle::Result::Continue;
}

angle::Result UtilsVk::colorBlitResolve(ContextVk *contextVk,
                                        FramebufferVk *framebuffer,
                                        vk::ImageHelper *src,
                                        const vk::ImageView *srcView,
                                        const BlitResolveParameters &params)
{
    return blitResolveImpl(contextVk, framebuffer, src, srcView, nullptr, nullptr, params);
}

angle::Result UtilsVk::depthStencilBlitResolve(ContextVk *contextVk,
                                               FramebufferVk *framebuffer,
                                               vk::ImageHelper *src,
                                               const vk::ImageView *srcDepthView,
                                               const vk::ImageView *srcStencilView,
                                               const BlitResolveParameters &params)
{
    return blitResolveImpl(contextVk, framebuffer, src, nullptr, srcDepthView, srcStencilView,
                           params);
}

angle::Result UtilsVk::blitResolveImpl(ContextVk *contextVk,
                                       FramebufferVk *framebuffer,
                                       vk::ImageHelper *src,
                                       const vk::ImageView *srcColorView,
                                       const vk::ImageView *srcDepthView,
                                       const vk::ImageView *srcStencilView,
                                       const BlitResolveParameters &params)
{
    // Possible ways to resolve color are:
    //
    // - vkCmdResolveImage: This is by far the easiest method, but lacks the ability to flip
    //   images during resolve.
    // - Manual resolve: A shader can read all samples from input, average them and output.
    // - Using subpass resolve attachment: A shader can transform the sample colors from source to
    //   destination coordinates and the subpass resolve would finish the job.
    //
    // The first method is unable to handle flipping, so it's not generally applicable.  The last
    // method would have been great were we able to modify the last render pass that rendered into
    // source, but still wouldn't be able to handle flipping.  The second method is implemented in
    // this function for complete control.

    // Possible ways to resolve depth/stencil are:
    //
    // - Manual resolve: A shader can read a samples from input and choose that for output.
    // - Using subpass resolve attachment through VkSubpassDescriptionDepthStencilResolveKHR: This
    //   requires an extension that's not very well supported.
    //
    // The first method is implemented in this function.

    // Possible ways to blit color, depth or stencil are:
    //
    // - vkCmdBlitImage: This function works if the source and destination formats have the blit
    //   feature.
    // - Manual blit: A shader can sample from the source image and write it to the destination.
    //
    // The first method has a serious shortcoming.  GLES allows blit parameters to exceed the
    // source or destination boundaries.  The actual blit is clipped to these limits, but the
    // scaling applied is determined solely by the input areas.  Vulkan requires the blit parameters
    // to be within the source and destination bounds.  This makes it hard to keep the scaling
    // constant.
    //
    // The second method is implemented in this function, which shares code with the resolve method.

    ANGLE_TRY(ensureBlitResolveResourcesInitialized(contextVk));

    bool isResolve = src->getSamples() > 1;

    BlitResolveShaderParams shaderParams;
    if (isResolve)
    {
        CalculateResolveOffset(params, shaderParams.offset.resolve);
    }
    else
    {
        CalculateBlitOffset(params, shaderParams.offset.blit);
    }
    shaderParams.stretch[0]      = params.stretch[0];
    shaderParams.stretch[1]      = params.stretch[1];
    shaderParams.invSrcExtent[0] = 1.0f / params.srcExtents[0];
    shaderParams.invSrcExtent[1] = 1.0f / params.srcExtents[1];
    shaderParams.srcLayer        = params.srcLayer;
    shaderParams.samples         = src->getSamples();
    shaderParams.invSamples      = 1.0f / shaderParams.samples;
    shaderParams.outputMask =
        static_cast<uint32_t>(framebuffer->getState().getEnabledDrawBuffers().to_ulong());
    shaderParams.flipX = params.flipX;
    shaderParams.flipY = params.flipY;

    bool blitColor   = srcColorView != nullptr;
    bool blitDepth   = srcDepthView != nullptr;
    bool blitStencil = srcStencilView != nullptr;

    // Either color is blitted/resolved or depth/stencil, but not both.
    ASSERT(blitColor != (blitDepth || blitStencil));

    // Linear sampling is only valid with color blitting.
    ASSERT((blitColor && !isResolve) || !params.linear);

    uint32_t flags = GetBlitResolveFlags(blitColor, blitDepth, blitStencil, src->getFormat());
    flags |= src->getLayerCount() > 1 ? BlitResolve_frag::kSrcIsArray : 0;
    flags |= isResolve ? BlitResolve_frag::kIsResolve : 0;

    VkDescriptorSet descriptorSet;
    vk::RefCountedDescriptorPoolBinding descriptorPoolBinding;
    ANGLE_TRY(allocateDescriptorSet(contextVk, Function::BlitResolve, &descriptorPoolBinding,
                                    &descriptorSet));

    constexpr VkColorComponentFlags kAllColorComponents =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    vk::GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.initDefaults();
    if (blitColor)
    {
        pipelineDesc.setColorWriteMask(kAllColorComponents,
                                       framebuffer->getEmulatedAlphaAttachmentMask());
    }
    else
    {
        pipelineDesc.setColorWriteMask(0, gl::DrawBufferMask());
    }
    pipelineDesc.setCullMode(VK_CULL_MODE_NONE);
    pipelineDesc.setRenderPassDesc(framebuffer->getRenderPassDesc());
    pipelineDesc.setDepthTestEnabled(blitDepth);
    pipelineDesc.setDepthWriteEnabled(blitDepth);
    pipelineDesc.setDepthFunc(VK_COMPARE_OP_ALWAYS);

    if (blitStencil)
    {
        ASSERT(contextVk->getRenderer()->getFeatures().supportsShaderStencilExport.enabled);

        const uint8_t completeMask    = 0xFF;
        const uint8_t unusedReference = 0x00;

        pipelineDesc.setStencilTestEnabled(true);
        pipelineDesc.setStencilFrontFuncs(unusedReference, VK_COMPARE_OP_ALWAYS, completeMask);
        pipelineDesc.setStencilBackFuncs(unusedReference, VK_COMPARE_OP_ALWAYS, completeMask);
        pipelineDesc.setStencilFrontOps(VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE,
                                        VK_STENCIL_OP_REPLACE);
        pipelineDesc.setStencilBackOps(VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE,
                                       VK_STENCIL_OP_REPLACE);
        pipelineDesc.setStencilFrontWriteMask(completeMask);
        pipelineDesc.setStencilBackWriteMask(completeMask);
    }

    VkViewport viewport;
    gl::Rectangle completeRenderArea = framebuffer->getCompleteRenderArea();
    gl_vk::GetViewport(completeRenderArea, 0.0f, 1.0f, false, completeRenderArea.height, &viewport);
    pipelineDesc.setViewport(viewport);

    pipelineDesc.setScissor(gl_vk::GetRect(params.blitArea));

    // Change source layout outside render pass
    if (src->isLayoutChangeNecessary(vk::ImageLayout::AllGraphicsShadersReadOnly))
    {
        vk::CommandBuffer *srcLayoutChange;
        ANGLE_TRY(src->recordCommands(contextVk, &srcLayoutChange));
        src->changeLayout(src->getAspectFlags(), vk::ImageLayout::AllGraphicsShadersReadOnly,
                          srcLayoutChange);
    }

    vk::CommandBuffer *commandBuffer;
    if (!framebuffer->appendToStartedRenderPass(&contextVk->getResourceUseList(), params.blitArea,
                                                &commandBuffer))
    {
        ANGLE_TRY(framebuffer->startNewRenderPass(contextVk, params.blitArea, &commandBuffer));
    }

    // Source's layout change should happen before rendering
    src->addReadDependency(contextVk, framebuffer->getFramebuffer());

    VkDescriptorImageInfo imageInfos[2] = {};

    if (blitColor)
    {
        imageInfos[0].imageView   = srcColorView->getHandle();
        imageInfos[0].imageLayout = src->getCurrentLayout();
    }
    if (blitDepth)
    {
        imageInfos[0].imageView   = srcDepthView->getHandle();
        imageInfos[0].imageLayout = src->getCurrentLayout();
    }
    if (blitStencil)
    {
        imageInfos[1].imageView   = srcStencilView->getHandle();
        imageInfos[1].imageLayout = src->getCurrentLayout();
    }

    VkDescriptorImageInfo samplerInfo = {};
    samplerInfo.sampler = params.linear ? mLinearSampler.getHandle() : mPointSampler.getHandle();

    VkWriteDescriptorSet writeInfos[3] = {};
    writeInfos[0].sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfos[0].dstSet               = descriptorSet;
    writeInfos[0].dstBinding           = kBlitResolveColorOrDepthBinding;
    writeInfos[0].descriptorCount      = 1;
    writeInfos[0].descriptorType       = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writeInfos[0].pImageInfo           = &imageInfos[0];

    writeInfos[1]            = writeInfos[0];
    writeInfos[1].dstBinding = kBlitResolveStencilBinding;
    writeInfos[1].pImageInfo = &imageInfos[1];

    writeInfos[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfos[2].dstSet          = descriptorSet;
    writeInfos[2].dstBinding      = kBlitResolveSamplerBinding;
    writeInfos[2].descriptorCount = 1;
    writeInfos[2].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
    writeInfos[2].pImageInfo      = &samplerInfo;

    // If resolving color, there's one write info; index 0
    // If resolving depth, write info index 0 must be written
    // If resolving stencil, write info index 1 must also be written
    //
    // Note again that resolving color and depth/stencil are mutually exclusive here.
    uint32_t writeInfoOffset = blitDepth || blitColor ? 0 : 1;
    uint32_t writeInfoCount  = blitColor + blitDepth + blitStencil;

    vkUpdateDescriptorSets(contextVk->getDevice(), writeInfoCount, writeInfos + writeInfoOffset, 0,
                           nullptr);
    vkUpdateDescriptorSets(contextVk->getDevice(), 1, &writeInfos[2], 0, nullptr);

    vk::ShaderLibrary &shaderLibrary                    = contextVk->getShaderLibrary();
    vk::RefCounted<vk::ShaderAndSerial> *vertexShader   = nullptr;
    vk::RefCounted<vk::ShaderAndSerial> *fragmentShader = nullptr;
    ANGLE_TRY(shaderLibrary.getFullScreenQuad_vert(contextVk, 0, &vertexShader));
    ANGLE_TRY(shaderLibrary.getBlitResolve_frag(contextVk, flags, &fragmentShader));

    ANGLE_TRY(setupProgram(contextVk, Function::BlitResolve, fragmentShader, vertexShader,
                           &mBlitResolvePrograms[flags], &pipelineDesc, descriptorSet,
                           &shaderParams, sizeof(shaderParams), commandBuffer));
    commandBuffer->draw(6, 0);
    descriptorPoolBinding.reset();

    return angle::Result::Continue;
}

angle::Result UtilsVk::stencilBlitResolveNoShaderExport(ContextVk *contextVk,
                                                        FramebufferVk *framebuffer,
                                                        vk::ImageHelper *src,
                                                        const vk::ImageView *srcStencilView,
                                                        const BlitResolveParameters &params)
{
    // When VK_EXT_shader_stencil_export is not available, stencil is blitted/resolved into a
    // temporary buffer which is then copied into the stencil aspect of the image.

    ANGLE_TRY(ensureBlitResolveStencilNoExportResourcesInitialized(contextVk));

    bool isResolve = src->getSamples() > 1;

    VkDescriptorSet descriptorSet;
    vk::RefCountedDescriptorPoolBinding descriptorPoolBinding;
    ANGLE_TRY(allocateDescriptorSet(contextVk, Function::BlitResolveStencilNoExport,
                                    &descriptorPoolBinding, &descriptorSet));

    // Create a temporary buffer to blit/resolve stencil into.
    vk::RendererScoped<vk::BufferHelper> blitBuffer(contextVk->getRenderer());

    uint32_t bufferRowLengthInUints = UnsignedCeilDivide(params.blitArea.width, sizeof(uint32_t));
    VkDeviceSize bufferSize = bufferRowLengthInUints * sizeof(uint32_t) * params.blitArea.height;

    VkBufferCreateInfo blitBufferInfo = {};
    blitBufferInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    blitBufferInfo.flags              = 0;
    blitBufferInfo.size               = bufferSize;
    blitBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    blitBufferInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    blitBufferInfo.queueFamilyIndexCount = 0;
    blitBufferInfo.pQueueFamilyIndices   = nullptr;

    ANGLE_TRY(
        blitBuffer.get().init(contextVk, blitBufferInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    blitBuffer.get().onResourceAccess(&contextVk->getResourceUseList());

    BlitResolveStencilNoExportShaderParams shaderParams;
    if (isResolve)
    {
        CalculateResolveOffset(params, shaderParams.offset.resolve);
    }
    else
    {
        CalculateBlitOffset(params, shaderParams.offset.blit);
    }
    shaderParams.stretch[0]      = params.stretch[0];
    shaderParams.stretch[1]      = params.stretch[1];
    shaderParams.invSrcExtent[0] = 1.0f / params.srcExtents[0];
    shaderParams.invSrcExtent[1] = 1.0f / params.srcExtents[1];
    shaderParams.srcLayer        = params.srcLayer;
    shaderParams.srcWidth        = params.srcExtents[0];
    shaderParams.destPitch       = bufferRowLengthInUints;
    shaderParams.blitArea[0]     = params.blitArea.x;
    shaderParams.blitArea[1]     = params.blitArea.y;
    shaderParams.blitArea[2]     = params.blitArea.width;
    shaderParams.blitArea[3]     = params.blitArea.height;
    shaderParams.flipX           = params.flipX;
    shaderParams.flipY           = params.flipY;

    // Linear sampling is only valid with color blitting.
    ASSERT(!params.linear);

    uint32_t flags = src->getLayerCount() > 1 ? BlitResolveStencilNoExport_comp::kSrcIsArray : 0;
    flags |= isResolve ? BlitResolve_frag::kIsResolve : 0;

    // Change source layout prior to computation.
    if (src->isLayoutChangeNecessary(vk::ImageLayout::ComputeShaderReadOnly))
    {
        vk::CommandBuffer *srcLayoutChange;
        ANGLE_TRY(src->recordCommands(contextVk, &srcLayoutChange));
        src->changeLayout(src->getAspectFlags(), vk::ImageLayout::ComputeShaderReadOnly,
                          srcLayoutChange);
    }

    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(framebuffer->getFramebuffer()->recordCommands(contextVk, &commandBuffer));

    src->addReadDependency(contextVk, framebuffer->getFramebuffer());

    // Blit/resolve stencil into the buffer.
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageView             = srcStencilView->getHandle();
    imageInfo.imageLayout           = src->getCurrentLayout();

    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer                 = blitBuffer.get().getBuffer().getHandle();
    bufferInfo.offset                 = 0;
    bufferInfo.range                  = VK_WHOLE_SIZE;

    VkDescriptorImageInfo samplerInfo = {};
    samplerInfo.sampler = params.linear ? mLinearSampler.getHandle() : mPointSampler.getHandle();

    VkWriteDescriptorSet writeInfos[3] = {};
    writeInfos[0].sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfos[0].dstSet               = descriptorSet;
    writeInfos[0].dstBinding           = kBlitResolveStencilNoExportDestBinding;
    writeInfos[0].descriptorCount      = 1;
    writeInfos[0].descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeInfos[0].pBufferInfo          = &bufferInfo;

    writeInfos[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfos[1].dstSet          = descriptorSet;
    writeInfos[1].dstBinding      = kBlitResolveStencilNoExportSrcBinding;
    writeInfos[1].descriptorCount = 1;
    writeInfos[1].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writeInfos[1].pImageInfo      = &imageInfo;

    writeInfos[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfos[2].dstSet          = descriptorSet;
    writeInfos[2].dstBinding      = kBlitResolveStencilNoExportSamplerBinding;
    writeInfos[2].descriptorCount = 1;
    writeInfos[2].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
    writeInfos[2].pImageInfo      = &samplerInfo;

    vkUpdateDescriptorSets(contextVk->getDevice(), 3, writeInfos, 0, nullptr);

    vk::RefCounted<vk::ShaderAndSerial> *shader = nullptr;
    ANGLE_TRY(contextVk->getShaderLibrary().getBlitResolveStencilNoExport_comp(contextVk, flags,
                                                                               &shader));

    ANGLE_TRY(setupProgram(contextVk, Function::BlitResolveStencilNoExport, shader, nullptr,
                           &mBlitResolveStencilNoExportPrograms[flags], nullptr, descriptorSet,
                           &shaderParams, sizeof(shaderParams), commandBuffer));
    commandBuffer->dispatch(UnsignedCeilDivide(bufferRowLengthInUints, 8),
                            UnsignedCeilDivide(params.blitArea.height, 8), 1);
    descriptorPoolBinding.reset();

    // Add a barrier prior to copy.
    VkMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType           = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask   = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask   = VK_ACCESS_TRANSFER_READ_BIT;

    // Use the all pipe stage to keep the state management simple.
    commandBuffer->pipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                   VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &memoryBarrier, 0, nullptr,
                                   0, nullptr);

    // Copy the resulting buffer into dest.
    RenderTargetVk *depthStencilRenderTarget = framebuffer->getDepthStencilRenderTarget();
    ASSERT(depthStencilRenderTarget != nullptr);
    vk::ImageHelper *depthStencilImage = &depthStencilRenderTarget->getImage();

    depthStencilImage->changeLayout(depthStencilImage->getAspectFlags(),
                                    vk::ImageLayout::TransferDst, commandBuffer);

    VkBufferImageCopy region               = {};
    region.bufferOffset                    = 0;
    region.bufferRowLength                 = bufferRowLengthInUints * sizeof(uint32_t);
    region.bufferImageHeight               = params.blitArea.height;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_STENCIL_BIT;
    region.imageSubresource.mipLevel       = depthStencilRenderTarget->getLevelIndex();
    region.imageSubresource.baseArrayLayer = depthStencilRenderTarget->getLayerIndex();
    region.imageSubresource.layerCount     = 1;
    region.imageOffset.x                   = params.blitArea.x;
    region.imageOffset.y                   = params.blitArea.y;
    region.imageOffset.z                   = 0;
    region.imageExtent.width               = params.blitArea.width;
    region.imageExtent.height              = params.blitArea.height;
    region.imageExtent.depth               = 1;

    commandBuffer->copyBufferToImage(blitBuffer.get().getBuffer().getHandle(),
                                     depthStencilImage->getImage(),
                                     depthStencilImage->getCurrentLayout(), 1, &region);

    return angle::Result::Continue;
}

angle::Result UtilsVk::copyImage(ContextVk *contextVk,
                                 vk::ImageHelper *dest,
                                 const vk::ImageView *destView,
                                 vk::ImageHelper *src,
                                 const vk::ImageView *srcView,
                                 const CopyImageParameters &params)
{
    ANGLE_TRY(ensureImageCopyResourcesInitialized(contextVk));

    const vk::Format &srcFormat            = src->getFormat();
    const vk::Format &dstFormat            = dest->getFormat();
    const angle::Format &dstIntendedFormat = dstFormat.intendedFormat();

    ImageCopyShaderParams shaderParams;
    shaderParams.flipY            = params.srcFlipY || params.destFlipY;
    shaderParams.premultiplyAlpha = params.srcPremultiplyAlpha;
    shaderParams.unmultiplyAlpha  = params.srcUnmultiplyAlpha;
    shaderParams.destHasLuminance = dstIntendedFormat.luminanceBits > 0;
    shaderParams.destIsAlpha      = dstIntendedFormat.isLUMA() && dstIntendedFormat.alphaBits > 0;
    shaderParams.destDefaultChannelsMask = GetFormatDefaultChannelMask(dstFormat);
    shaderParams.srcMip                  = params.srcMip;
    shaderParams.srcLayer                = params.srcLayer;
    shaderParams.srcOffset[0]            = params.srcOffset[0];
    shaderParams.srcOffset[1]            = params.srcOffset[1];
    shaderParams.destOffset[0]           = params.destOffset[0];
    shaderParams.destOffset[1]           = params.destOffset[1];

    ASSERT(!(params.srcFlipY && params.destFlipY));
    if (params.srcFlipY)
    {
        // If viewport is flipped, the shader expects srcOffset[1] to have the
        // last row's index instead of the first's.
        shaderParams.srcOffset[1] = params.srcHeight - params.srcOffset[1] - 1;
    }
    else if (params.destFlipY)
    {
        // If image is flipped during copy, the shader uses the same code path as above,
        // with srcOffset being set to the last row's index instead of the first's.
        shaderParams.srcOffset[1] = params.srcOffset[1] + params.srcExtents[1] - 1;
    }

    uint32_t flags = GetImageCopyFlags(srcFormat, dstFormat);
    flags |= src->getLayerCount() > 1 ? ImageCopy_frag::kSrcIsArray : 0;

    VkDescriptorSet descriptorSet;
    vk::RefCountedDescriptorPoolBinding descriptorPoolBinding;
    ANGLE_TRY(allocateDescriptorSet(contextVk, Function::ImageCopy, &descriptorPoolBinding,
                                    &descriptorSet));

    vk::RenderPassDesc renderPassDesc;
    renderPassDesc.setSamples(dest->getSamples());
    renderPassDesc.packColorAttachment(0, dstFormat.intendedFormatID);

    // Multisampled copy is not yet supported.
    ASSERT(src->getSamples() == 1 && dest->getSamples() == 1);

    vk::GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.initDefaults();
    pipelineDesc.setCullMode(VK_CULL_MODE_NONE);
    pipelineDesc.setRenderPassDesc(renderPassDesc);

    gl::Rectangle renderArea;
    renderArea.x      = params.destOffset[0];
    renderArea.y      = params.destOffset[1];
    renderArea.width  = params.srcExtents[0];
    renderArea.height = params.srcExtents[1];

    VkViewport viewport;
    gl_vk::GetViewport(renderArea, 0.0f, 1.0f, false, dest->getExtents().height, &viewport);
    pipelineDesc.setViewport(viewport);

    VkRect2D scissor = gl_vk::GetRect(renderArea);
    pipelineDesc.setScissor(scissor);

    // Change source layout outside render pass
    if (src->isLayoutChangeNecessary(vk::ImageLayout::AllGraphicsShadersReadOnly))
    {
        vk::CommandBuffer *srcLayoutChange;
        ANGLE_TRY(src->recordCommands(contextVk, &srcLayoutChange));
        src->changeLayout(VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::AllGraphicsShadersReadOnly,
                          srcLayoutChange);
    }

    // Change destination layout outside render pass as well
    vk::CommandBuffer *destLayoutChange;
    ANGLE_TRY(dest->recordCommands(contextVk, &destLayoutChange));

    dest->changeLayout(VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::ColorAttachment,
                       destLayoutChange);

    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(
        startRenderPass(contextVk, dest, destView, renderPassDesc, renderArea, &commandBuffer));

    // Source's layout change should happen before rendering
    src->addReadDependency(contextVk, dest);

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageView             = srcView->getHandle();
    imageInfo.imageLayout           = src->getCurrentLayout();

    VkWriteDescriptorSet writeInfo = {};
    writeInfo.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfo.dstSet               = descriptorSet;
    writeInfo.dstBinding           = kImageCopySourceBinding;
    writeInfo.descriptorCount      = 1;
    writeInfo.descriptorType       = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writeInfo.pImageInfo           = &imageInfo;

    vkUpdateDescriptorSets(contextVk->getDevice(), 1, &writeInfo, 0, nullptr);

    vk::ShaderLibrary &shaderLibrary                    = contextVk->getShaderLibrary();
    vk::RefCounted<vk::ShaderAndSerial> *vertexShader   = nullptr;
    vk::RefCounted<vk::ShaderAndSerial> *fragmentShader = nullptr;
    ANGLE_TRY(shaderLibrary.getFullScreenQuad_vert(contextVk, 0, &vertexShader));
    ANGLE_TRY(shaderLibrary.getImageCopy_frag(contextVk, flags, &fragmentShader));

    ANGLE_TRY(setupProgram(contextVk, Function::ImageCopy, fragmentShader, vertexShader,
                           &mImageCopyPrograms[flags], &pipelineDesc, descriptorSet, &shaderParams,
                           sizeof(shaderParams), commandBuffer));
    commandBuffer->draw(6, 0);
    descriptorPoolBinding.reset();

    return angle::Result::Continue;
}

angle::Result UtilsVk::cullOverlayWidgets(ContextVk *contextVk,
                                          vk::BufferHelper *enabledWidgetsBuffer,
                                          vk::ImageHelper *dest,
                                          const vk::ImageView *destView,
                                          const OverlayCullParameters &params)
{
    ANGLE_TRY(ensureOverlayCullResourcesInitialized(contextVk));

    ASSERT(params.subgroupSize[0] == 8 &&
           (params.subgroupSize[1] == 8 || params.subgroupSize[1] == 4));
    uint32_t flags =
        params.subgroupSize[1] == 8 ? OverlayCull_comp::kIs8x8 : OverlayCull_comp::kIs8x4;
    if (params.supportsSubgroupBallot)
    {
        flags |= OverlayCull_comp::kSupportsBallot;
    }
    else if (params.supportsSubgroupBallot)
    {
        flags |= OverlayCull_comp::kSupportsArithmetic;
    }
    else
    {
        flags |= OverlayCull_comp::kSupportsNone;
    }

    VkDescriptorSet descriptorSet;
    vk::RefCountedDescriptorPoolBinding descriptorPoolBinding;
    ANGLE_TRY(allocateDescriptorSet(contextVk, Function::OverlayCull, &descriptorPoolBinding,
                                    &descriptorSet));

    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(dest->recordCommands(contextVk, &commandBuffer));
    dest->changeLayout(VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::ComputeShaderWrite,
                       commandBuffer);

    enabledWidgetsBuffer->onRead(contextVk, dest, VK_ACCESS_SHADER_READ_BIT);

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageView             = destView->getHandle();
    imageInfo.imageLayout           = dest->getCurrentLayout();

    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer                 = enabledWidgetsBuffer->getBuffer().getHandle();
    bufferInfo.offset                 = 0;
    bufferInfo.range                  = VK_WHOLE_SIZE;

    VkWriteDescriptorSet writeInfos[2] = {};
    writeInfos[0].sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfos[0].dstSet               = descriptorSet;
    writeInfos[0].dstBinding           = kOverlayCullCulledWidgetsBinding;
    writeInfos[0].descriptorCount      = 1;
    writeInfos[0].descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writeInfos[0].pImageInfo           = &imageInfo;

    writeInfos[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfos[1].dstSet          = descriptorSet;
    writeInfos[1].dstBinding      = kOverlayCullWidgetCoordsBinding;
    writeInfos[1].descriptorCount = 1;
    writeInfos[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeInfos[1].pBufferInfo     = &bufferInfo;

    vkUpdateDescriptorSets(contextVk->getDevice(), 2, writeInfos, 0, nullptr);

    vk::RefCounted<vk::ShaderAndSerial> *shader = nullptr;
    ANGLE_TRY(contextVk->getShaderLibrary().getOverlayCull_comp(contextVk, flags, &shader));

    ANGLE_TRY(setupProgram(contextVk, Function::OverlayCull, shader, nullptr,
                           &mOverlayCullPrograms[flags], nullptr, descriptorSet, nullptr, 0,
                           commandBuffer));
    commandBuffer->dispatch(dest->getExtents().width, dest->getExtents().height, 1);
    descriptorPoolBinding.reset();

    dest->changeLayout(VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::ComputeShaderReadOnly,
                       commandBuffer);

    return angle::Result::Continue;
}

angle::Result UtilsVk::drawOverlay(ContextVk *contextVk,
                                   vk::BufferHelper *textWidgetsBuffer,
                                   vk::BufferHelper *graphWidgetsBuffer,
                                   vk::ImageHelper *font,
                                   const vk::ImageView *fontView,
                                   vk::ImageHelper *culledWidgets,
                                   const vk::ImageView *culledWidgetsView,
                                   vk::ImageHelper *dest,
                                   const vk::ImageView *destView,
                                   const OverlayDrawParameters &params)
{
    ANGLE_TRY(ensureOverlayDrawResourcesInitialized(contextVk));

    OverlayDrawShaderParams shaderParams;
    shaderParams.outputSize[0] = dest->getExtents().width;
    shaderParams.outputSize[1] = dest->getExtents().height;

    ASSERT(params.subgroupSize[0] == 8 &&
           (params.subgroupSize[1] == 8 || params.subgroupSize[1] == 4));
    uint32_t flags =
        params.subgroupSize[1] == 8 ? OverlayDraw_comp::kIs8x8 : OverlayDraw_comp::kIs8x4;

    VkDescriptorSet descriptorSet;
    vk::RefCountedDescriptorPoolBinding descriptorPoolBinding;
    ANGLE_TRY(allocateDescriptorSet(contextVk, Function::OverlayDraw, &descriptorPoolBinding,
                                    &descriptorSet));

    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(dest->recordCommands(contextVk, &commandBuffer));
    dest->changeLayout(VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::ComputeShaderWrite,
                       commandBuffer);

    culledWidgets->addReadDependency(contextVk, dest);
    font->addReadDependency(contextVk, dest);
    textWidgetsBuffer->onRead(contextVk, dest, VK_ACCESS_SHADER_READ_BIT);
    graphWidgetsBuffer->onRead(contextVk, dest, VK_ACCESS_SHADER_READ_BIT);

    VkDescriptorImageInfo imageInfos[3] = {};
    imageInfos[0].imageView             = destView->getHandle();
    imageInfos[0].imageLayout           = dest->getCurrentLayout();

    imageInfos[1].imageView   = culledWidgetsView->getHandle();
    imageInfos[1].imageLayout = culledWidgets->getCurrentLayout();

    imageInfos[2].imageView   = fontView->getHandle();
    imageInfos[2].imageLayout = font->getCurrentLayout();

    VkDescriptorBufferInfo bufferInfos[2] = {};
    bufferInfos[0].buffer                 = textWidgetsBuffer->getBuffer().getHandle();
    bufferInfos[0].offset                 = 0;
    bufferInfos[0].range                  = VK_WHOLE_SIZE;

    bufferInfos[1].buffer = graphWidgetsBuffer->getBuffer().getHandle();
    bufferInfos[1].offset = 0;
    bufferInfos[1].range  = VK_WHOLE_SIZE;

    VkWriteDescriptorSet writeInfos[5] = {};
    writeInfos[0].sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfos[0].dstSet               = descriptorSet;
    writeInfos[0].dstBinding           = kOverlayDrawOutputBinding;
    writeInfos[0].descriptorCount      = 1;
    writeInfos[0].descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writeInfos[0].pImageInfo           = &imageInfos[0];

    writeInfos[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfos[1].dstSet          = descriptorSet;
    writeInfos[1].dstBinding      = kOverlayDrawCulledWidgetsBinding;
    writeInfos[1].descriptorCount = 1;
    writeInfos[1].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writeInfos[1].pImageInfo      = &imageInfos[1];

    writeInfos[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfos[2].dstSet          = descriptorSet;
    writeInfos[2].dstBinding      = kOverlayDrawFontBinding;
    writeInfos[2].descriptorCount = 1;
    writeInfos[2].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writeInfos[2].pImageInfo      = &imageInfos[2];

    writeInfos[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfos[3].dstSet          = descriptorSet;
    writeInfos[3].dstBinding      = kOverlayDrawTextWidgetsBinding;
    writeInfos[3].descriptorCount = 1;
    writeInfos[3].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeInfos[3].pBufferInfo     = &bufferInfos[0];

    writeInfos[4]             = writeInfos[3];
    writeInfos[4].dstBinding  = kOverlayDrawGraphWidgetsBinding;
    writeInfos[4].pBufferInfo = &bufferInfos[1];

    vkUpdateDescriptorSets(contextVk->getDevice(), 5, writeInfos, 0, nullptr);

    vk::RefCounted<vk::ShaderAndSerial> *shader = nullptr;
    ANGLE_TRY(contextVk->getShaderLibrary().getOverlayDraw_comp(contextVk, flags, &shader));

    ANGLE_TRY(setupProgram(contextVk, Function::OverlayDraw, shader, nullptr,
                           &mOverlayDrawPrograms[flags], nullptr, descriptorSet, &shaderParams,
                           sizeof(shaderParams), commandBuffer));

    // Every pixel of culledWidgets corresponds to one workgroup, so we can use that as dispatch
    // size.
    commandBuffer->dispatch(culledWidgets->getExtents().width, culledWidgets->getExtents().height,
                            1);
    descriptorPoolBinding.reset();

    return angle::Result::Continue;
}

angle::Result UtilsVk::allocateDescriptorSet(ContextVk *contextVk,
                                             Function function,
                                             vk::RefCountedDescriptorPoolBinding *bindingOut,
                                             VkDescriptorSet *descriptorSetOut)
{
    ANGLE_TRY(mDescriptorPools[function].allocateSets(
        contextVk, mDescriptorSetLayouts[function][kSetIndex].get().ptr(), 1, bindingOut,
        descriptorSetOut));
    bindingOut->get().updateSerial(contextVk->getCurrentQueueSerial());
    return angle::Result::Continue;
}

UtilsVk::ClearFramebufferParameters::ClearFramebufferParameters()
    : clearColor(false),
      clearStencil(false),
      stencilMask(0),
      colorMaskFlags(0),
      colorAttachmentIndexGL(0),
      colorFormat(nullptr),
      colorClearValue{},
      stencilClearValue(0)
{}

}  // namespace rx
