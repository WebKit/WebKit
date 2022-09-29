//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// UtilsVk.cpp:
//    Implements the UtilsVk class.
//

#include "libANGLE/renderer/vulkan/UtilsVk.h"

#include "common/spirv/spirv_instruction_builder_autogen.h"

#include "libANGLE/renderer/glslang_wrapper_utils.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/RenderTargetVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/SurfaceVk.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

namespace rx
{

namespace ConvertVertex_comp                = vk::InternalShader::ConvertVertex_comp;
namespace ImageClear_frag                   = vk::InternalShader::ImageClear_frag;
namespace ImageCopy_frag                    = vk::InternalShader::ImageCopy_frag;
namespace BlitResolve_frag                  = vk::InternalShader::BlitResolve_frag;
namespace BlitResolveStencilNoExport_comp   = vk::InternalShader::BlitResolveStencilNoExport_comp;
namespace ConvertIndexIndirectLineLoop_comp = vk::InternalShader::ConvertIndexIndirectLineLoop_comp;
namespace GenerateMipmap_comp               = vk::InternalShader::GenerateMipmap_comp;

namespace spirv = angle::spirv;

namespace
{
constexpr uint32_t kConvertIndexDestinationBinding = 0;

constexpr uint32_t kConvertVertexDestinationBinding = 0;
constexpr uint32_t kConvertVertexSourceBinding      = 1;

constexpr uint32_t kImageCopySourceBinding = 0;

constexpr uint32_t kBlitResolveColorOrDepthBinding = 0;
constexpr uint32_t kBlitResolveStencilBinding      = 1;
constexpr uint32_t kBlitResolveSamplerBinding      = 2;

constexpr uint32_t kBlitResolveStencilNoExportDestBinding    = 0;
constexpr uint32_t kBlitResolveStencilNoExportSrcBinding     = 1;
constexpr uint32_t kBlitResolveStencilNoExportSamplerBinding = 2;

constexpr uint32_t kOverlayDrawTextWidgetsBinding  = 0;
constexpr uint32_t kOverlayDrawGraphWidgetsBinding = 1;
constexpr uint32_t kOverlayDrawFontBinding         = 2;

constexpr uint32_t kGenerateMipmapDestinationBinding = 0;
constexpr uint32_t kGenerateMipmapSourceBinding      = 1;

bool ValidateFloatOneAsUint()
{
    union
    {
        uint32_t asUint;
        float asFloat;
    } one;
    one.asUint = gl::Float32One;
    return one.asFloat == 1.0f;
}

uint32_t GetConvertVertexFlags(const UtilsVk::ConvertVertexParameters &params)
{
    bool srcIsSint      = params.srcFormat->isSint();
    bool srcIsUint      = params.srcFormat->isUint();
    bool srcIsSnorm     = params.srcFormat->isSnorm();
    bool srcIsUnorm     = params.srcFormat->isUnorm();
    bool srcIsFixed     = params.srcFormat->isFixed;
    bool srcIsFloat     = params.srcFormat->isFloat();
    bool srcIsHalfFloat = params.srcFormat->isVertexTypeHalfFloat();

    bool dstIsSint      = params.dstFormat->isSint();
    bool dstIsUint      = params.dstFormat->isUint();
    bool dstIsSnorm     = params.dstFormat->isSnorm();
    bool dstIsUnorm     = params.dstFormat->isUnorm();
    bool dstIsFloat     = params.dstFormat->isFloat();
    bool dstIsHalfFloat = params.dstFormat->isVertexTypeHalfFloat();

    // Assert on the types to make sure the shader supports its.  These are based on
    // ConvertVertex_comp::Conversion values.
    ASSERT(!dstIsSint || srcIsSint);    // If destination is sint, src must be sint too
    ASSERT(!dstIsUint || srcIsUint);    // If destination is uint, src must be uint too
    ASSERT(!srcIsFixed || dstIsFloat);  // If source is fixed, dst must be float

    // One of each bool set must be true
    ASSERT(srcIsSint || srcIsUint || srcIsSnorm || srcIsUnorm || srcIsFixed || srcIsFloat);
    ASSERT(dstIsSint || dstIsUint || dstIsSnorm || dstIsUnorm || dstIsFloat || dstIsHalfFloat);

    // We currently don't have any big-endian devices in the list of supported platforms.  The
    // shader is capable of supporting big-endian architectures, but the relevant flag (IsBigEndian)
    // is not added to the build configuration file (to reduce binary size).  If necessary, add
    // IsBigEndian to ConvertVertex.comp.json and select the appropriate flag based on the
    // endian-ness test here.
    ASSERT(IsLittleEndian());

    uint32_t flags = 0;

    if (srcIsHalfFloat && dstIsHalfFloat)
    {
        // Note that HalfFloat conversion uses the same shader as Uint.
        flags = ConvertVertex_comp::kUintToUint;
    }
    else if ((srcIsSnorm && dstIsSnorm) || (srcIsUnorm && dstIsUnorm))
    {
        // Do snorm->snorm and unorm->unorm copies using the uint->uint shader.  Currently only
        // supported for same-width formats, so it's only used when adding channels.
        ASSERT(params.srcFormat->redBits == params.dstFormat->redBits);
        flags = ConvertVertex_comp::kUintToUint;
    }
    else if (srcIsSint && dstIsSint)
    {
        flags = ConvertVertex_comp::kSintToSint;
    }
    else if (srcIsUint && dstIsUint)
    {
        flags = ConvertVertex_comp::kUintToUint;
    }
    else if (srcIsSint)
    {
        flags = ConvertVertex_comp::kSintToFloat;
    }
    else if (srcIsUint)
    {
        flags = ConvertVertex_comp::kUintToFloat;
    }
    else if (srcIsSnorm)
    {
        flags = ConvertVertex_comp::kSnormToFloat;
    }
    else if (srcIsUnorm)
    {
        flags = ConvertVertex_comp::kUnormToFloat;
    }
    else if (srcIsFixed)
    {
        flags = ConvertVertex_comp::kFixedToFloat;
    }
    else if (srcIsFloat)
    {
        flags = ConvertVertex_comp::kFloatToFloat;
    }
    else
    {
        UNREACHABLE();
    }

    return flags;
}

uint32_t GetImageClearFlags(const angle::Format &format, uint32_t attachmentIndex, bool clearDepth)
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

    if (clearDepth)
    {
        flags |= ImageClear_frag::kClearDepth;
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

uint32_t GetImageCopyFlags(const angle::Format &srcIntendedFormat,
                           const angle::Format &dstIntendedFormat)
{
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
                             const angle::Format &intendedFormat)
{
    if (blitColor)
    {
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

uint32_t GetConvertIndexIndirectLineLoopFlag(uint32_t indicesBitsWidth)
{
    switch (indicesBitsWidth)
    {
        case 8:
            return ConvertIndexIndirectLineLoop_comp::kIs8Bits;
        case 16:
            return ConvertIndexIndirectLineLoop_comp::kIs16Bits;
        case 32:
            return ConvertIndexIndirectLineLoop_comp::kIs32Bits;
        default:
            UNREACHABLE();
            return 0;
    }
}

uint32_t GetGenerateMipmapFlags(ContextVk *contextVk, const angle::Format &actualFormat)
{
    uint32_t flags = 0;

    // Note: If bits-per-component is 8 or 16 and float16 is supported in the shader, use that for
    // faster math.
    const bool hasShaderFloat16 =
        contextVk->getRenderer()->getFeatures().supportsShaderFloat16.enabled;

    if (actualFormat.redBits <= 8)
    {
        flags = hasShaderFloat16 ? GenerateMipmap_comp::kIsRGBA8_UseHalf
                                 : GenerateMipmap_comp::kIsRGBA8;
    }
    else if (actualFormat.redBits <= 16)
    {
        flags = hasShaderFloat16 ? GenerateMipmap_comp::kIsRGBA16_UseHalf
                                 : GenerateMipmap_comp::kIsRGBA16;
    }
    else
    {
        flags = GenerateMipmap_comp::kIsRGBA32F;
    }

    flags |= UtilsVk::GetGenerateMipmapMaxLevels(contextVk) == UtilsVk::kGenerateMipmapMaxLevels
                 ? GenerateMipmap_comp::kDestSize6
                 : GenerateMipmap_comp::kDestSize4;

    return flags;
}

enum UnresolveColorAttachmentType
{
    kUnresolveTypeUnused = 0,
    kUnresolveTypeFloat  = 1,
    kUnresolveTypeSint   = 2,
    kUnresolveTypeUint   = 3,
};

uint32_t GetUnresolveFlags(uint32_t colorAttachmentCount,
                           const gl::DrawBuffersArray<vk::ImageHelper *> &colorSrc,
                           bool unresolveDepth,
                           bool unresolveStencil,
                           gl::DrawBuffersArray<UnresolveColorAttachmentType> *attachmentTypesOut)
{
    uint32_t flags = 0;

    for (uint32_t attachmentIndex = 0; attachmentIndex < colorAttachmentCount; ++attachmentIndex)
    {
        const angle::Format &format = colorSrc[attachmentIndex]->getIntendedFormat();

        UnresolveColorAttachmentType type = kUnresolveTypeFloat;
        if (format.isSint())
        {
            type = kUnresolveTypeSint;
        }
        else if (format.isUint())
        {
            type = kUnresolveTypeUint;
        }

        (*attachmentTypesOut)[attachmentIndex] = type;

        // |flags| is comprised of |colorAttachmentCount| values from
        // |UnresolveColorAttachmentType|, each taking up 2 bits.
        flags |= type << (2 * attachmentIndex);
    }

    // Additionally, two bits are used for depth and stencil unresolve.
    constexpr uint32_t kDepthUnresolveFlagBit   = 2 * gl::IMPLEMENTATION_MAX_DRAW_BUFFERS;
    constexpr uint32_t kStencilUnresolveFlagBit = kDepthUnresolveFlagBit + 1;
    if (unresolveDepth)
    {
        flags |= 1 << kDepthUnresolveFlagBit;
    }

    if (unresolveStencil)
    {
        flags |= 1 << kStencilUnresolveFlagBit;
    }

    return flags;
}

uint32_t GetFormatDefaultChannelMask(const angle::Format &intendedImageFormat,
                                     const angle::Format &actualImageFormat)
{
    uint32_t mask = 0;

    // Red can never be introduced due to format emulation (except for luma which is handled
    // especially)
    ASSERT(((intendedImageFormat.redBits > 0) == (actualImageFormat.redBits > 0)) ||
           intendedImageFormat.isLUMA());
    mask |= intendedImageFormat.greenBits == 0 && actualImageFormat.greenBits > 0 ? 2 : 0;
    mask |= intendedImageFormat.blueBits == 0 && actualImageFormat.blueBits > 0 ? 4 : 0;
    mask |= intendedImageFormat.alphaBits == 0 && actualImageFormat.alphaBits > 0 ? 8 : 0;

    return mask;
}

// Calculate the transformation offset for blit/resolve.  See BlitResolve.frag for details on how
// these values are derived.
void CalculateBlitOffset(const UtilsVk::BlitResolveParameters &params, float offset[2])
{
    int srcOffsetFactorX = params.flipX ? -1 : 1;
    int srcOffsetFactorY = params.flipY ? -1 : 1;

    offset[0] = params.dstOffset[0] * params.stretch[0] - params.srcOffset[0] * srcOffsetFactorX;
    offset[1] = params.dstOffset[1] * params.stretch[1] - params.srcOffset[1] * srcOffsetFactorY;
}

void CalculateResolveOffset(const UtilsVk::BlitResolveParameters &params, int32_t offset[2])
{
    int srcOffsetFactorX = params.flipX ? -1 : 1;
    int srcOffsetFactorY = params.flipY ? -1 : 1;

    // There's no stretching in resolve.
    offset[0] = params.dstOffset[0] - params.srcOffset[0] * srcOffsetFactorX;
    offset[1] = params.dstOffset[1] - params.srcOffset[1] * srcOffsetFactorY;
}

// Sets the appropriate settings in the pipeline for either the shader to output stencil, regardless
// of whether its done through the reference value or the shader stencil export extension.
void SetStencilStateForWrite(vk::GraphicsPipelineDesc *desc)
{
    desc->setStencilTestEnabled(true);
    desc->setStencilFrontFuncs(VK_COMPARE_OP_ALWAYS);
    desc->setStencilBackFuncs(VK_COMPARE_OP_ALWAYS);
    desc->setStencilFrontOps(VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE);
    desc->setStencilBackOps(VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE);
}
void SetStencilDynamicStateForWrite(vk::RenderPassCommandBuffer *commandBuffer)
{
    commandBuffer->setStencilTestEnable(true);
    commandBuffer->setStencilOp(VK_STENCIL_FACE_FRONT_BIT, VK_STENCIL_OP_REPLACE,
                                VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE, VK_COMPARE_OP_ALWAYS);
    commandBuffer->setStencilOp(VK_STENCIL_FACE_BACK_BIT, VK_STENCIL_OP_REPLACE,
                                VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE, VK_COMPARE_OP_ALWAYS);
}

namespace unresolve
{
// The unresolve shader looks like the following, based on the number and types of unresolve
// attachments.
//
//     #version 450 core
//     #extension GL_ARB_shader_stencil_export : require
//
//     layout(location = 0) out vec4 colorOut0;
//     layout(location = 1) out ivec4 colorOut1;
//     layout(location = 2) out uvec4 colorOut2;
//     layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput colorIn0;
//     layout(input_attachment_index = 1, set = 0, binding = 1) uniform isubpassInput colorIn1;
//     layout(input_attachment_index = 2, set = 0, binding = 2) uniform usubpassInput colorIn2;
//     layout(input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput depthIn;
//     layout(input_attachment_index = 3, set = 0, binding = 4) uniform usubpassInput stencilIn;
//
//     void main()
//     {
//         colorOut0 = subpassLoad(colorIn0);
//         colorOut1 = subpassLoad(colorIn1);
//         colorOut2 = subpassLoad(colorIn2);
//         gl_FragDepth = subpassLoad(depthIn).x;
//         gl_FragStencilRefARB = int(subpassLoad(stencilIn).x);
//     }
//
// This shader compiles to the following SPIR-V:
//
//           OpCapability Shader                              \
//           OpCapability InputAttachment                      \
//           OpCapability StencilExportEXT                      \   Preamble.  Mostly fixed, except
//           OpExtension "SPV_EXT_shader_stencil_export"         \  OpEntryPoint should enumerate
//      %1 = OpExtInstImport "GLSL.std.450"                       \ out variables, stencil export
//           OpMemoryModel Logical GLSL450                        / is conditional to stencil
//           OpEntryPoint Fragment %4 "main" %26 %27 %28 %29 %30 /  unresolve, and depth replacing
//           OpExecutionMode %4 OriginUpperLeft                 /   conditional to depth unresolve.
//           OpExecutionMode %4 DepthReplacing                 /
//           OpSource GLSL 450                                /
//
//           OpName %4 "main"              \
//           OpName %26 "colorOut0"         \
//           OpName %27 "colorOut1"          \
//           OpName %28 "colorOut2"           \
//           OpName %29 "gl_FragDepth"         \ Debug information.  Not generated here.
//           OpName %30 "gl_FragStencilRefARB" /
//           OpName %31 "colorIn0"            /
//           OpName %32 "colorIn1"           /
//           OpName %33 "colorIn2"          /
//           OpName %34 "depthIn"          /
//           OpName %35 "stencilIn"       /
//
//           OpDecorate %26 Location 0      \
//           OpDecorate %27 Location 1       \ Location decoration of out variables.
//           OpDecorate %28 Location 2       /
//
//           OpDecorate %29 BuiltIn FragDepth          \ Builtin outputs, conditional to depth
//           OpDecorate %30 BuiltIn FragStencilRefEXT  / and stencil unresolve.
//
//           OpDecorate %31 DescriptorSet 0        \
//           OpDecorate %31 Binding 0               \
//           OpDecorate %31 InputAttachmentIndex 0   \
//           OpDecorate %32 DescriptorSet 0           \
//           OpDecorate %32 Binding 1                  \
//           OpDecorate %32 InputAttachmentIndex 1      \
//           OpDecorate %33 DescriptorSet 0              \  set, binding and input_attachment
//           OpDecorate %33 Binding 2                     \ decorations of the subpassInput
//           OpDecorate %33 InputAttachmentIndex 2        / variables.
//           OpDecorate %34 DescriptorSet 0              /
//           OpDecorate %34 Binding 3                   /
//           OpDecorate %34 InputAttachmentIndex 3     /
//           OpDecorate %35 DescriptorSet 0           /
//           OpDecorate %35 Binding 4                /
//           OpDecorate %35 InputAttachmentIndex 3  /
//
//      %2 = OpTypeVoid         \ Type of main().  Fixed.
//      %3 = OpTypeFunction %2  /
//
//      %6 = OpTypeFloat 32                             \
//      %7 = OpTypeVector %6 4                           \
//      %8 = OpTypePointer Output %7                      \ Type declaration for "out vec4"
//      %9 = OpTypeImage %6 SubpassData 0 0 0 2 Unknown   / and "subpassInput".  Fixed.
//     %10 = OpTypePointer UniformConstant %9            /
//
//     %11 = OpTypeInt 32 1                              \
//     %12 = OpTypeVector %11 4                           \
//     %13 = OpTypePointer Output %12                      \ Type declaration for "out ivec4"
//     %14 = OpTypeImage %11 SubpassData 0 0 0 2 Unknown   / and "isubpassInput".  Fixed.
//     %15 = OpTypePointer UniformConstant %14            /
//
//     %16 = OpTypeInt 32 0                              \
//     %17 = OpTypeVector %16 4                           \
//     %18 = OpTypePointer Output %17                      \ Type declaration for "out uvec4"
//     %19 = OpTypeImage %16 SubpassData 0 0 0 2 Unknown   / and "usubpassInput".  Fixed.
//     %20 = OpTypePointer UniformConstant %19            /
//
//     %21 = OpTypePointer Output %6         \ Type declaraions for depth and stencil. Fixed.
//     %22 = OpTypePointer Output %11        /
//
//     %23 = OpConstant %11 0                \
//     %24 = OpTypeVector %11 2               \ ivec2(0) for OpImageRead.  subpassLoad
//     %25 = OpConstantComposite %22 %21 %21  / doesn't require coordinates.  Fixed.
//
//     %26 = OpVariable %8 Output            \
//     %27 = OpVariable %13 Output            \
//     %28 = OpVariable %18 Output             \
//     %29 = OpVariable %21 Output              \
//     %30 = OpVariable %22 Output               \ Actual "out" and "*subpassInput"
//     %31 = OpVariable %10 UniformConstant      / variable declarations.
//     %32 = OpVariable %15 UniformConstant     /
//     %33 = OpVariable %20 UniformConstant    /
//     %34 = OpVariable %10 UniformConstant   /
//     %35 = OpVariable %20 UniformConstant  /
//
//      %4 = OpFunction %2 None %3   \ Top of main().  Fixed.
//      %5 = OpLabel                 /
//
//     %36 = OpLoad %9 %31           \
//     %37 = OpImageRead %7 %36 %23   \ colorOut0 = subpassLoad(colorIn0);
//           OpStore %26 %37          /
//
//     %38 = OpLoad %14 %32          \
//     %39 = OpImageRead %12 %38 %23  \ colorOut1 = subpassLoad(colorIn1);
//           OpStore %27 %39          /
//
//     %40 = OpLoad %19 %33          \
//     %41 = OpImageRead %17 %40 %23  \ colorOut2 = subpassLoad(colorIn2);
//           OpStore %28 %41          /
//
//     %42 = OpLoad %9 %34              \
//     %43 = OpImageRead %7 %42 %23      \ gl_FragDepth = subpassLoad(depthIn).x;
//     %44 = OpCompositeExtract %6 %43 0 /
//           OpStore %29 %44            /
//
//     %45 = OpLoad %19 %35              \
//     %46 = OpImageRead %17 %45 %23      \
//     %47 = OpCompositeExtract %16 %46 0  \ gl_FragStencilRefARB = int(subpassLoad(stencilIn).x);
//     %48 = OpBitcast %11 %47             /
//           OpStore %30 %48              /
//
//           OpReturn           \ Bottom of main().  Fixed.
//           OpFunctionEnd      /
//
// What makes the generation of this shader manageable is that the majority of it is constant
// between the different variations of the shader.  The rest are repeating patterns with different
// ids or indices.

enum
{
    // main() ids
    kIdExtInstImport = 1,
    kIdVoid,
    kIdMainType,
    kIdMain,
    kIdMainLabel,

    // Types for "out vec4" and "subpassInput"
    kIdFloatType,
    kIdFloat4Type,
    kIdFloat4OutType,
    kIdFloatSubpassImageType,
    kIdFloatSubpassInputType,

    // Types for "out ivec4" and "isubpassInput"
    kIdSIntType,
    kIdSInt4Type,
    kIdSInt4OutType,
    kIdSIntSubpassImageType,
    kIdSIntSubpassInputType,

    // Types for "out uvec4" and "usubpassInput"
    kIdUIntType,
    kIdUInt4Type,
    kIdUInt4OutType,
    kIdUIntSubpassImageType,
    kIdUIntSubpassInputType,

    // Types for gl_FragDepth && gl_FragStencilRefARB
    kIdFloatOutType,
    kIdSIntOutType,

    // ivec2(0) constant
    kIdSIntZero,
    kIdSInt2Type,
    kIdSInt2Zero,

    // Output variable ids
    kIdColor0Out,
    kIdDepthOut = kIdColor0Out + gl::IMPLEMENTATION_MAX_DRAW_BUFFERS,
    kIdStencilOut,

    // Input variable ids
    kIdColor0In,
    kIdDepthIn = kIdColor0In + gl::IMPLEMENTATION_MAX_DRAW_BUFFERS,
    kIdStencilIn,

    // Ids for temp variables
    kIdColor0Load,
    // 2 temp ids per color unresolve
    kIdDepthLoad = kIdColor0Load + gl::IMPLEMENTATION_MAX_DRAW_BUFFERS * 2,
    // 3 temp ids for depth unresolve
    kIdStencilLoad = kIdDepthLoad + 3,
    // Total number of ids used
    // 4 temp ids for stencil unresolve
    kIdCount = kIdStencilLoad + 4,
};

void InsertPreamble(uint32_t colorAttachmentCount,
                    bool unresolveDepth,
                    bool unresolveStencil,
                    angle::spirv::Blob *blobOut)
{
    spirv::WriteCapability(blobOut, spv::CapabilityShader);
    spirv::WriteCapability(blobOut, spv::CapabilityInputAttachment);
    if (unresolveStencil)
    {
        spirv::WriteCapability(blobOut, spv::CapabilityStencilExportEXT);
        spirv::WriteExtension(blobOut, "SPV_EXT_shader_stencil_export");
    }
    // OpExtInstImport is actually not needed by this shader.  We don't use any instructions from
    // GLSL.std.450.
    spirv::WriteMemoryModel(blobOut, spv::AddressingModelLogical, spv::MemoryModelGLSL450);

    // Create the list of entry point ids, including only the out variables.
    spirv::IdRefList entryPointIds;
    for (uint32_t colorIndex = 0; colorIndex < colorAttachmentCount; ++colorIndex)
    {
        entryPointIds.push_back(spirv::IdRef(kIdColor0Out + colorIndex));
    }
    if (unresolveDepth)
    {
        entryPointIds.push_back(spirv::IdRef(kIdDepthOut));
    }
    if (unresolveStencil)
    {
        entryPointIds.push_back(spirv::IdRef(kIdStencilOut));
    }
    spirv::WriteEntryPoint(blobOut, spv::ExecutionModelFragment, spirv::IdRef(kIdMain), "main",
                           entryPointIds);

    spirv::WriteExecutionMode(blobOut, spirv::IdRef(kIdMain), spv::ExecutionModeOriginUpperLeft,
                              {});
    if (unresolveDepth)
    {
        spirv::WriteExecutionMode(blobOut, spirv::IdRef(kIdMain), spv::ExecutionModeDepthReplacing,
                                  {});
    }
    spirv::WriteSource(blobOut, spv::SourceLanguageGLSL, spirv::LiteralInteger(450), nullptr,
                       nullptr);
}

void InsertInputDecorations(spirv::IdRef id,
                            uint32_t attachmentIndex,
                            uint32_t binding,
                            angle::spirv::Blob *blobOut)
{
    spirv::WriteDecorate(blobOut, id, spv::DecorationDescriptorSet,
                         {spirv::LiteralInteger(ToUnderlying(DescriptorSetIndex::Internal))});
    spirv::WriteDecorate(blobOut, id, spv::DecorationBinding, {spirv::LiteralInteger(binding)});
    spirv::WriteDecorate(blobOut, id, spv::DecorationInputAttachmentIndex,
                         {spirv::LiteralInteger(attachmentIndex)});
}

void InsertColorDecorations(uint32_t colorIndex, angle::spirv::Blob *blobOut)
{
    // Decorate the output color attachment with Location
    spirv::WriteDecorate(blobOut, spirv::IdRef(kIdColor0Out + colorIndex), spv::DecorationLocation,
                         {spirv::LiteralInteger(colorIndex)});
    // Decorate the subpasss input color attachment with Set/Binding/InputAttachmentIndex.
    InsertInputDecorations(spirv::IdRef(kIdColor0In + colorIndex), colorIndex, colorIndex, blobOut);
}

void InsertDepthStencilDecorations(uint32_t depthStencilInputIndex,
                                   uint32_t depthStencilBindingIndex,
                                   bool unresolveDepth,
                                   bool unresolveStencil,
                                   angle::spirv::Blob *blobOut)
{
    if (unresolveDepth)
    {
        // Decorate the output depth attachment with Location
        spirv::WriteDecorate(blobOut, spirv::IdRef(kIdDepthOut), spv::DecorationBuiltIn,
                             {spirv::LiteralInteger(spv::BuiltInFragDepth)});
        // Decorate the subpasss input depth attachment with Set/Binding/InputAttachmentIndex.
        InsertInputDecorations(spirv::IdRef(kIdDepthIn), depthStencilInputIndex,
                               depthStencilBindingIndex, blobOut);
        // Advance the binding.  Note that the depth/stencil attachment has the same input
        // attachment index (it's the same attachment in the subpass), but different bindings (one
        // aspect per image view).
        ++depthStencilBindingIndex;
    }
    if (unresolveStencil)
    {
        // Decorate the output stencil attachment with Location
        spirv::WriteDecorate(blobOut, spirv::IdRef(kIdStencilOut), spv::DecorationBuiltIn,
                             {spirv::LiteralInteger(spv::BuiltInFragStencilRefEXT)});
        // Decorate the subpasss input stencil attachment with Set/Binding/InputAttachmentIndex.
        InsertInputDecorations(spirv::IdRef(kIdStencilIn), depthStencilInputIndex,
                               depthStencilBindingIndex, blobOut);
    }
}

void InsertDerivativeTypes(spirv::IdRef baseId,
                           spirv::IdRef vec4Id,
                           spirv::IdRef vec4OutId,
                           spirv::IdRef imageTypeId,
                           spirv::IdRef inputTypeId,
                           angle::spirv::Blob *blobOut)
{
    spirv::WriteTypeVector(blobOut, vec4Id, baseId, spirv::LiteralInteger(4));
    spirv::WriteTypePointer(blobOut, vec4OutId, spv::StorageClassOutput, vec4Id);
    spirv::WriteTypeImage(blobOut, imageTypeId, baseId, spv::DimSubpassData,
                          // Unused with subpass inputs
                          spirv::LiteralInteger(0),
                          // Not arrayed
                          spirv::LiteralInteger(0),
                          // Not multisampled
                          spirv::LiteralInteger(0),
                          // Used without a sampler
                          spirv::LiteralInteger(2), spv::ImageFormatUnknown, nullptr);
    spirv::WriteTypePointer(blobOut, inputTypeId, spv::StorageClassUniformConstant, imageTypeId);
}

void InsertCommonTypes(angle::spirv::Blob *blobOut)
{
    // Types to support main().
    spirv::WriteTypeVoid(blobOut, spirv::IdRef(kIdVoid));
    spirv::WriteTypeFunction(blobOut, spirv::IdRef(kIdMainType), spirv::IdRef(kIdVoid), {});

    // Float types
    spirv::WriteTypeFloat(blobOut, spirv::IdRef(kIdFloatType), spirv::LiteralInteger(32));
    InsertDerivativeTypes(spirv::IdRef(kIdFloatType), spirv::IdRef(kIdFloat4Type),
                          spirv::IdRef(kIdFloat4OutType), spirv::IdRef(kIdFloatSubpassImageType),
                          spirv::IdRef(kIdFloatSubpassInputType), blobOut);

    // Int types
    spirv::WriteTypeInt(blobOut, spirv::IdRef(kIdSIntType), spirv::LiteralInteger(32),
                        spirv::LiteralInteger(1));
    InsertDerivativeTypes(spirv::IdRef(kIdSIntType), spirv::IdRef(kIdSInt4Type),
                          spirv::IdRef(kIdSInt4OutType), spirv::IdRef(kIdSIntSubpassImageType),
                          spirv::IdRef(kIdSIntSubpassInputType), blobOut);

    // Unsigned int types
    spirv::WriteTypeInt(blobOut, spirv::IdRef(kIdUIntType), spirv::LiteralInteger(32),
                        spirv::LiteralInteger(0));
    InsertDerivativeTypes(spirv::IdRef(kIdUIntType), spirv::IdRef(kIdUInt4Type),
                          spirv::IdRef(kIdUInt4OutType), spirv::IdRef(kIdUIntSubpassImageType),
                          spirv::IdRef(kIdUIntSubpassInputType), blobOut);

    // Types to support depth/stencil
    spirv::WriteTypePointer(blobOut, spirv::IdRef(kIdFloatOutType), spv::StorageClassOutput,
                            spirv::IdRef(kIdFloatType));
    spirv::WriteTypePointer(blobOut, spirv::IdRef(kIdSIntOutType), spv::StorageClassOutput,
                            spirv::IdRef(kIdSIntType));

    // Constants used to load from subpass inputs
    spirv::WriteConstant(blobOut, spirv::IdRef(kIdSIntType), spirv::IdRef(kIdSIntZero),
                         spirv::LiteralInteger(0));
    spirv::WriteTypeVector(blobOut, spirv::IdRef(kIdSInt2Type), spirv::IdRef(kIdSIntType),
                           spirv::LiteralInteger(2));
    spirv::WriteConstantComposite(blobOut, spirv::IdRef(kIdSInt2Type), spirv::IdRef(kIdSInt2Zero),
                                  {spirv::IdRef(kIdSIntZero), spirv::IdRef(kIdSIntZero)});
}

void InsertVariableDecl(spirv::IdRef outType,
                        spirv::IdRef outId,
                        spirv::IdRef inType,
                        spirv::IdRef inId,
                        angle::spirv::Blob *blobOut)
{
    // Declare both the output and subpass input variables.
    spirv::WriteVariable(blobOut, outType, outId, spv::StorageClassOutput, nullptr);
    spirv::WriteVariable(blobOut, inType, inId, spv::StorageClassUniformConstant, nullptr);
}

void InsertColorVariableDecl(uint32_t colorIndex,
                             UnresolveColorAttachmentType type,
                             angle::spirv::Blob *blobOut)
{
    // Find the correct types for color variable declarations.
    spirv::IdRef outType(kIdFloat4OutType);
    spirv::IdRef outId(kIdColor0Out + colorIndex);
    spirv::IdRef inType(kIdFloatSubpassInputType);
    spirv::IdRef inId(kIdColor0In + colorIndex);
    switch (type)
    {
        case kUnresolveTypeSint:
            outType = spirv::IdRef(kIdSInt4OutType);
            inType  = spirv::IdRef(kIdSIntSubpassInputType);
            break;
        case kUnresolveTypeUint:
            outType = spirv::IdRef(kIdUInt4OutType);
            inType  = spirv::IdRef(kIdUIntSubpassInputType);
            break;
        default:
            break;
    }
    InsertVariableDecl(outType, outId, inType, inId, blobOut);
}

void InsertDepthStencilVariableDecl(bool unresolveDepth,
                                    bool unresolveStencil,
                                    angle::spirv::Blob *blobOut)
{
    if (unresolveDepth)
    {
        InsertVariableDecl(spirv::IdRef(kIdFloatOutType), spirv::IdRef(kIdDepthOut),
                           spirv::IdRef(kIdFloatSubpassInputType), spirv::IdRef(kIdDepthIn),
                           blobOut);
    }
    if (unresolveStencil)
    {
        InsertVariableDecl(spirv::IdRef(kIdSIntOutType), spirv::IdRef(kIdStencilOut),
                           spirv::IdRef(kIdUIntSubpassInputType), spirv::IdRef(kIdStencilIn),
                           blobOut);
    }
}

void InsertTopOfMain(angle::spirv::Blob *blobOut)
{
    spirv::WriteFunction(blobOut, spirv::IdRef(kIdVoid), spirv::IdRef(kIdMain),
                         spv::FunctionControlMaskNone, spirv::IdRef(kIdMainType));
    spirv::WriteLabel(blobOut, spirv::IdRef(kIdMainLabel));
}

void InsertColorUnresolveLoadStore(uint32_t colorIndex,
                                   UnresolveColorAttachmentType type,
                                   angle::spirv::Blob *blobOut)
{
    spirv::IdRef loadResult(kIdColor0Load + colorIndex * 2);
    spirv::IdRef imageReadResult(loadResult + 1);

    // Find the correct types for load/store.
    spirv::IdRef loadType(kIdFloatSubpassImageType);
    spirv::IdRef readType(kIdFloat4Type);
    spirv::IdRef inId(kIdColor0In + colorIndex);
    spirv::IdRef outId(kIdColor0Out + colorIndex);
    switch (type)
    {
        case kUnresolveTypeSint:
            loadType = spirv::IdRef(kIdSIntSubpassImageType);
            readType = spirv::IdRef(kIdSInt4Type);
            break;
        case kUnresolveTypeUint:
            loadType = spirv::IdRef(kIdUIntSubpassImageType);
            readType = spirv::IdRef(kIdUInt4Type);
            break;
        default:
            break;
    }

    // Load the subpass input image, read from it, and store in output.
    spirv::WriteLoad(blobOut, loadType, loadResult, inId, nullptr);
    spirv::WriteImageRead(blobOut, readType, imageReadResult, loadResult,
                          spirv::IdRef(kIdSInt2Zero), nullptr, {});
    spirv::WriteStore(blobOut, outId, imageReadResult, nullptr);
}

void InsertDepthStencilUnresolveLoadStore(bool unresolveDepth,
                                          bool unresolveStencil,
                                          angle::spirv::Blob *blobOut)
{
    if (unresolveDepth)
    {
        spirv::IdRef loadResult(kIdDepthLoad);
        spirv::IdRef imageReadResult(loadResult + 1);
        spirv::IdRef extractResult(imageReadResult + 1);

        spirv::IdRef loadType(kIdFloatSubpassImageType);
        spirv::IdRef readType(kIdFloat4Type);
        spirv::IdRef inId(kIdDepthIn);
        spirv::IdRef outId(kIdDepthOut);

        // Load the subpass input image, read from it, select .x, and store in output.
        spirv::WriteLoad(blobOut, loadType, loadResult, inId, nullptr);
        spirv::WriteImageRead(blobOut, readType, imageReadResult, loadResult,
                              spirv::IdRef(kIdSInt2Zero), nullptr, {});
        spirv::WriteCompositeExtract(blobOut, spirv::IdRef(kIdFloatType), extractResult,
                                     imageReadResult, {spirv::LiteralInteger(0)});
        spirv::WriteStore(blobOut, outId, extractResult, nullptr);
    }
    if (unresolveStencil)
    {
        spirv::IdRef loadResult(kIdStencilLoad);
        spirv::IdRef imageReadResult(loadResult + 1);
        spirv::IdRef extractResult(imageReadResult + 1);
        spirv::IdRef bitcastResult(extractResult + 1);

        spirv::IdRef loadType(kIdUIntSubpassImageType);
        spirv::IdRef readType(kIdUInt4Type);
        spirv::IdRef inId(kIdStencilIn);
        spirv::IdRef outId(kIdStencilOut);

        // Load the subpass input image, read from it, select .x, and store in output.  There's a
        // bitcast involved since the stencil subpass input has unsigned type, while
        // gl_FragStencilRefARB is signed!
        spirv::WriteLoad(blobOut, loadType, loadResult, inId, nullptr);
        spirv::WriteImageRead(blobOut, readType, imageReadResult, loadResult,
                              spirv::IdRef(kIdSInt2Zero), nullptr, {});
        spirv::WriteCompositeExtract(blobOut, spirv::IdRef(kIdUIntType), extractResult,
                                     imageReadResult, {spirv::LiteralInteger(0)});
        spirv::WriteBitcast(blobOut, spirv::IdRef(kIdSIntType), bitcastResult, extractResult);
        spirv::WriteStore(blobOut, outId, bitcastResult, nullptr);
    }
}

void InsertBottomOfMain(angle::spirv::Blob *blobOut)
{
    spirv::WriteReturn(blobOut);
    spirv::WriteFunctionEnd(blobOut);
}

angle::spirv::Blob MakeFragShader(
    uint32_t colorAttachmentCount,
    gl::DrawBuffersArray<UnresolveColorAttachmentType> &colorAttachmentTypes,
    bool unresolveDepth,
    bool unresolveStencil)
{
    angle::spirv::Blob code;

    // Reserve a sensible amount of memory.  A single-attachment shader is 169 words.
    code.reserve(169);

    // Header
    spirv::WriteSpirvHeader(&code, kIdCount);

    // The preamble
    InsertPreamble(colorAttachmentCount, unresolveDepth, unresolveStencil, &code);

    // Color attachment decorations
    for (uint32_t colorIndex = 0; colorIndex < colorAttachmentCount; ++colorIndex)
    {
        InsertColorDecorations(colorIndex, &code);
    }

    const uint32_t depthStencilInputIndex = colorAttachmentCount;
    uint32_t depthStencilBindingIndex     = colorAttachmentCount;
    InsertDepthStencilDecorations(depthStencilInputIndex, depthStencilBindingIndex, unresolveDepth,
                                  unresolveStencil, &code);

    // Common types
    InsertCommonTypes(&code);

    // Attachment declarations
    for (uint32_t colorIndex = 0; colorIndex < colorAttachmentCount; ++colorIndex)
    {
        InsertColorVariableDecl(colorIndex, colorAttachmentTypes[colorIndex], &code);
    }
    InsertDepthStencilVariableDecl(unresolveDepth, unresolveStencil, &code);

    // Top of main
    InsertTopOfMain(&code);

    // Load and store for each attachment
    for (uint32_t colorIndex = 0; colorIndex < colorAttachmentCount; ++colorIndex)
    {
        InsertColorUnresolveLoadStore(colorIndex, colorAttachmentTypes[colorIndex], &code);
    }
    InsertDepthStencilUnresolveLoadStore(unresolveDepth, unresolveStencil, &code);

    // Bottom of main
    InsertBottomOfMain(&code);

    return code;
}
}  // namespace unresolve

angle::Result GetUnresolveFrag(
    vk::Context *context,
    uint32_t colorAttachmentCount,
    gl::DrawBuffersArray<UnresolveColorAttachmentType> &colorAttachmentTypes,
    bool unresolveDepth,
    bool unresolveStencil,
    vk::RefCounted<vk::ShaderAndSerial> *shader)
{
    if (shader->get().valid())
    {
        return angle::Result::Continue;
    }

    angle::spirv::Blob shaderCode = unresolve::MakeFragShader(
        colorAttachmentCount, colorAttachmentTypes, unresolveDepth, unresolveStencil);

    ASSERT(spirv::Validate(shaderCode));

    // Create shader lazily. Access will need to be locked for multi-threading.
    return vk::InitShaderAndSerial(context, &shader->get(), shaderCode.data(),
                                   shaderCode.size() * 4);
}

gl::DrawBufferMask MakeColorBufferMask(uint32_t colorAttachmentIndexGL)
{
    gl::DrawBufferMask mask;
    mask.set(colorAttachmentIndexGL);
    return mask;
}

void UpdateColorAccess(ContextVk *contextVk,
                       gl::DrawBufferMask colorAttachmentMask,
                       gl::DrawBufferMask colorEnabledMask)
{
    vk::RenderPassCommandBufferHelper *renderPassCommands =
        &contextVk->getStartedRenderPassCommands();

    // Explicitly mark a color write because we are modifying the color buffer.
    vk::PackedAttachmentIndex colorIndexVk(0);
    for (size_t colorIndexGL : colorAttachmentMask)
    {
        if (colorEnabledMask.test(colorIndexGL))
        {
            renderPassCommands->onColorAccess(colorIndexVk, vk::ResourceAccess::Write);
        }
        ++colorIndexVk;
    }
}

void UpdateDepthStencilAccess(ContextVk *contextVk,
                              FramebufferVk *framebuffer,
                              bool depthWrite,
                              bool stencilWrite)
{
    vk::RenderPassCommandBufferHelper *renderPassCommands =
        &contextVk->getStartedRenderPassCommands();

    if (depthWrite)
    {
        // Explicitly mark a depth write because we are modifying the depth buffer.
        renderPassCommands->onDepthAccess(vk::ResourceAccess::Write);
    }
    if (stencilWrite)
    {
        // Explicitly mark a stencil write because we are modifying the stencil buffer.
        renderPassCommands->onStencilAccess(vk::ResourceAccess::Write);
    }
    if (depthWrite || stencilWrite)
    {
        // Because we may have changed the depth stencil access mode, update read only depth mode
        // now.
        framebuffer->updateRenderPassReadOnlyDepthMode(contextVk, renderPassCommands);
    }
}

void ResetDynamicState(ContextVk *contextVk, vk::RenderPassCommandBuffer *commandBuffer)
{
    // Reset dynamic state that might affect UtilsVk.  Mark all dynamic state dirty for simplicity.
    // Ideally, only dynamic state that is changed by UtilsVk will be marked dirty but, until such
    // time as extensive transition tests are written, this approach is less bug-prone.

    // Notes: the following dynamic state doesn't apply to UtilsVk functions:
    //
    // - line width: UtilsVk doesn't use line primitives
    // - depth bias: UtilsVk doesn't enable depth bias
    // - blend constants: UtilsVk doesn't enable blending
    // - logic op: UtilsVk doesn't enable logic op
    //
    // The following dynamic state is always set by UtilsVk when effective:
    //
    // - depth write mask: UtilsVk sets this when enabling depth test
    // - depth compare op: UtilsVk sets this when enabling depth test
    // - stencil compare mask: UtilsVk sets this when enabling stencil test
    // - stencil write mask: UtilsVk sets this when enabling stencil test
    // - stencil reference: UtilsVk sets this when enabling stencil test
    // - stencil func: UtilsVk sets this when enabling stencil test
    // - stencil ops: UtilsVk sets this when enabling stencil test

    // Reset all other dynamic state, since it can affect UtilsVk functions:
    if (contextVk->getFeatures().supportsExtendedDynamicState.enabled)
    {
        commandBuffer->setCullMode(VK_CULL_MODE_NONE);
        commandBuffer->setFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE);
        commandBuffer->setDepthTestEnable(VK_FALSE);
        commandBuffer->setStencilTestEnable(VK_FALSE);
    }
    if (contextVk->getFeatures().supportsExtendedDynamicState2.enabled)
    {
        commandBuffer->setRasterizerDiscardEnable(VK_FALSE);
        commandBuffer->setDepthBiasEnable(VK_FALSE);
        commandBuffer->setPrimitiveRestartEnable(VK_FALSE);
    }
    if (contextVk->getFeatures().supportsFragmentShadingRate.enabled)
    {
        VkExtent2D fragmentSize                                     = {1, 1};
        VkFragmentShadingRateCombinerOpKHR shadingRateCombinerOp[2] = {
            VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,
            VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR};
        commandBuffer->setFragmentShadingRate(&fragmentSize, shadingRateCombinerOp);
    }
    if (contextVk->getFeatures().supportsLogicOpDynamicState.enabled)
    {
        commandBuffer->setLogicOp(VK_LOGIC_OP_COPY);
    }

    // Let ContextVk know that it should refresh all dynamic state.
    contextVk->invalidateAllDynamicState();
}
}  // namespace

UtilsVk::ConvertVertexShaderParams::ConvertVertexShaderParams() = default;

UtilsVk::ImageCopyShaderParams::ImageCopyShaderParams() = default;

uint32_t UtilsVk::GetGenerateMipmapMaxLevels(ContextVk *contextVk)
{
    RendererVk *renderer = contextVk->getRenderer();

    uint32_t maxPerStageDescriptorStorageImages =
        renderer->getPhysicalDeviceProperties().limits.maxPerStageDescriptorStorageImages;

    // Vulkan requires that there be support for at least 4 storage images per stage.
    constexpr uint32_t kMinimumStorageImagesLimit = 4;
    ASSERT(maxPerStageDescriptorStorageImages >= kMinimumStorageImagesLimit);

    // If fewer than max-levels are supported, use 4 levels (which is the minimum required number
    // of storage image bindings).
    return maxPerStageDescriptorStorageImages < kGenerateMipmapMaxLevels
               ? kMinimumStorageImagesLimit
               : kGenerateMipmapMaxLevels;
}

UtilsVk::UtilsVk() = default;

UtilsVk::~UtilsVk() = default;

void UtilsVk::destroy(RendererVk *renderer)
{
    VkDevice device = renderer->getDevice();

    for (Function f : angle::AllEnums<Function>())
    {
        for (auto &descriptorSetLayout : mDescriptorSetLayouts[f])
        {
            descriptorSetLayout.reset();
        }
        mPipelineLayouts[f].reset();
        mDescriptorPools[f].destroy(renderer);
    }

    for (vk::ShaderProgramHelper &program : mConvertIndexPrograms)
    {
        program.destroy(renderer);
    }
    for (vk::ShaderProgramHelper &program : mConvertIndirectLineLoopPrograms)
    {
        program.destroy(renderer);
    }
    for (vk::ShaderProgramHelper &program : mConvertIndexIndirectLineLoopPrograms)
    {
        program.destroy(renderer);
    }
    for (vk::ShaderProgramHelper &program : mConvertVertexPrograms)
    {
        program.destroy(renderer);
    }
    mImageClearProgramVSOnly.destroy(renderer);
    for (vk::ShaderProgramHelper &program : mImageClearPrograms)
    {
        program.destroy(renderer);
    }
    for (vk::ShaderProgramHelper &program : mImageCopyPrograms)
    {
        program.destroy(renderer);
    }
    for (vk::ShaderProgramHelper &program : mBlitResolvePrograms)
    {
        program.destroy(renderer);
    }
    for (vk::ShaderProgramHelper &program : mBlitResolveStencilNoExportPrograms)
    {
        program.destroy(renderer);
    }
    mOverlayDrawProgram.destroy(renderer);
    for (vk::ShaderProgramHelper &program : mGenerateMipmapPrograms)
    {
        program.destroy(renderer);
    }

    for (auto &programIter : mUnresolvePrograms)
    {
        vk::ShaderProgramHelper &program = programIter.second;
        program.destroy(renderer);
    }
    mUnresolvePrograms.clear();

    for (auto &shaderIter : mUnresolveFragShaders)
    {
        vk::RefCounted<vk::ShaderAndSerial> &shader = shaderIter.second;
        shader.get().destroy(device);
    }
    mUnresolveFragShaders.clear();

    mPointSampler.destroy(device);
    mLinearSampler.destroy(device);
}

angle::Result UtilsVk::ensureResourcesInitialized(ContextVk *contextVk,
                                                  Function function,
                                                  VkDescriptorPoolSize *setSizes,
                                                  size_t setSizesCount,
                                                  size_t pushConstantsSize)
{
    vk::DescriptorSetLayoutDesc descriptorSetDesc;
    bool isCompute = function >= Function::ComputeStartIndex;
    VkShaderStageFlags descStages =
        isCompute ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
    if (function == Function::OverlayDraw)
    {
        descStages |= VK_SHADER_STAGE_VERTEX_BIT;
    }

    uint32_t currentBinding = 0;
    for (size_t i = 0; i < setSizesCount; ++i)
    {
        descriptorSetDesc.update(currentBinding, setSizes[i].type, setSizes[i].descriptorCount,
                                 descStages, nullptr);
        ++currentBinding;
    }

    ANGLE_TRY(contextVk->getDescriptorSetLayoutCache().getDescriptorSetLayout(
        contextVk, descriptorSetDesc,
        &mDescriptorSetLayouts[function][DescriptorSetIndex::Internal]));

    vk::DescriptorSetLayoutBindingVector bindingVector;
    std::vector<VkSampler> immutableSamplers;
    descriptorSetDesc.unpackBindings(&bindingVector, &immutableSamplers);
    std::vector<VkDescriptorPoolSize> descriptorPoolSizes;

    for (const VkDescriptorSetLayoutBinding &binding : bindingVector)
    {
        if (binding.descriptorCount > 0)
        {
            VkDescriptorPoolSize poolSize = {};

            poolSize.type            = binding.descriptorType;
            poolSize.descriptorCount = binding.descriptorCount;
            descriptorPoolSizes.emplace_back(poolSize);
        }
    }
    if (!descriptorPoolSizes.empty())
    {
        ANGLE_TRY(mDescriptorPools[function].init(
            contextVk, descriptorPoolSizes.data(), descriptorPoolSizes.size(),
            mDescriptorSetLayouts[function][DescriptorSetIndex::Internal].get()));
    }

    // Corresponding pipeline layouts:
    vk::PipelineLayoutDesc pipelineLayoutDesc;

    pipelineLayoutDesc.updateDescriptorSetLayout(DescriptorSetIndex::Internal, descriptorSetDesc);
    if (pushConstantsSize)
    {
        pipelineLayoutDesc.updatePushConstantRange(descStages, 0,
                                                   static_cast<uint32_t>(pushConstantsSize));
    }

    ANGLE_TRY(contextVk->getPipelineLayoutCache().getPipelineLayout(contextVk, pipelineLayoutDesc,
                                                                    mDescriptorSetLayouts[function],
                                                                    &mPipelineLayouts[function]));

    return angle::Result::Continue;
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
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // dst index buffer
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // source index buffer
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // src indirect buffer
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // dst indirect buffer
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
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // dst cmd buffer
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // source index buffer
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // dst index buffer
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
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // dst cmd buffer
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},  // dst index buffer
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

angle::Result UtilsVk::ensureOverlayDrawResourcesInitialized(ContextVk *contextVk)
{
    if (!mPipelineLayouts[Function::OverlayDraw].valid())
    {
        VkDescriptorPoolSize setSizes[3] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1},
        };

        ANGLE_TRY(ensureResourcesInitialized(contextVk, Function::OverlayDraw, setSizes,
                                             ArraySize(setSizes), sizeof(OverlayDrawShaderParams)));
    }

    return ensureSamplersInitialized(contextVk);
}

angle::Result UtilsVk::ensureGenerateMipmapResourcesInitialized(ContextVk *contextVk)
{
    if (mPipelineLayouts[Function::GenerateMipmap].valid())
    {
        return angle::Result::Continue;
    }

    VkDescriptorPoolSize setSizes[2] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, GetGenerateMipmapMaxLevels(contextVk)},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
    };

    return ensureResourcesInitialized(contextVk, Function::GenerateMipmap, setSizes,
                                      ArraySize(setSizes), sizeof(GenerateMipmapShaderParams));
}

angle::Result UtilsVk::ensureUnresolveResourcesInitialized(ContextVk *contextVk,
                                                           Function function,
                                                           uint32_t attachmentCount)
{
    ASSERT(static_cast<uint32_t>(function) -
               static_cast<uint32_t>(Function::Unresolve1Attachment) ==
           attachmentCount - 1);

    if (mPipelineLayouts[function].valid())
    {
        return angle::Result::Continue;
    }

    vk::FramebufferAttachmentArray<VkDescriptorPoolSize> setSizes;
    std::fill(setSizes.begin(), setSizes.end(),
              VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1});

    return ensureResourcesInitialized(contextVk, function, setSizes.data(), attachmentCount, 0);
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

angle::Result UtilsVk::setupComputeProgram(
    ContextVk *contextVk,
    Function function,
    vk::RefCounted<vk::ShaderAndSerial> *csShader,
    vk::ShaderProgramHelper *program,
    const VkDescriptorSet descriptorSet,
    const void *pushConstants,
    size_t pushConstantsSize,
    vk::OutsideRenderPassCommandBufferHelper *commandBufferHelper)
{
    RendererVk *renderer = contextVk->getRenderer();

    ASSERT(function >= Function::ComputeStartIndex);

    const vk::BindingPointer<vk::PipelineLayout> &pipelineLayout = mPipelineLayouts[function];

    program->setShader(gl::ShaderType::Compute, csShader);

    vk::PipelineHelper *pipeline;
    PipelineCacheAccess pipelineCache;
    ANGLE_TRY(renderer->getPipelineCache(&pipelineCache));
    ANGLE_TRY(program->getComputePipeline(contextVk, &pipelineCache, pipelineLayout.get(),
                                          PipelineSource::Utils, &pipeline));
    commandBufferHelper->retainResource(pipeline);

    vk::OutsideRenderPassCommandBuffer *commandBuffer = &commandBufferHelper->getCommandBuffer();
    commandBuffer->bindComputePipeline(pipeline->getPipeline());

    contextVk->invalidateComputePipelineBinding();

    if (descriptorSet != VK_NULL_HANDLE)
    {
        commandBuffer->bindDescriptorSets(pipelineLayout.get(), VK_PIPELINE_BIND_POINT_COMPUTE,
                                          DescriptorSetIndex::Internal, 1, &descriptorSet, 0,
                                          nullptr);
        contextVk->invalidateComputeDescriptorSet(DescriptorSetIndex::Internal);
    }

    if (pushConstants)
    {
        commandBuffer->pushConstants(pipelineLayout.get(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                     static_cast<uint32_t>(pushConstantsSize), pushConstants);
    }

    return angle::Result::Continue;
}

angle::Result UtilsVk::setupGraphicsProgram(ContextVk *contextVk,
                                            Function function,
                                            vk::RefCounted<vk::ShaderAndSerial> *vsShader,
                                            vk::RefCounted<vk::ShaderAndSerial> *fsShader,
                                            vk::ShaderProgramHelper *program,
                                            const vk::GraphicsPipelineDesc *pipelineDesc,
                                            const VkDescriptorSet descriptorSet,
                                            const void *pushConstants,
                                            size_t pushConstantsSize,
                                            vk::RenderPassCommandBuffer *commandBuffer)
{
    RendererVk *renderer = contextVk->getRenderer();

    ASSERT(function < Function::ComputeStartIndex);

    const vk::BindingPointer<vk::PipelineLayout> &pipelineLayout = mPipelineLayouts[function];

    program->setShader(gl::ShaderType::Vertex, vsShader);
    if (fsShader)
    {
        program->setShader(gl::ShaderType::Fragment, fsShader);
    }

    // This value is not used but is passed to getGraphicsPipeline to avoid a nullptr check.
    const vk::GraphicsPipelineDesc *descPtr;
    vk::PipelineHelper *helper;
    PipelineCacheAccess pipelineCache;
    ANGLE_TRY(renderer->getPipelineCache(&pipelineCache));
    ANGLE_TRY(program->getGraphicsPipeline(
        contextVk, &contextVk->getRenderPassCache(), &pipelineCache, pipelineLayout.get(),
        PipelineSource::Utils, *pipelineDesc, gl::AttributesMask(), gl::ComponentTypeMask(),
        gl::DrawBufferMask(), &descPtr, &helper));
    contextVk->getStartedRenderPassCommands().retainResource(helper);
    commandBuffer->bindGraphicsPipeline(helper->getPipeline());

    contextVk->invalidateGraphicsPipelineBinding();

    if (descriptorSet != VK_NULL_HANDLE)
    {
        commandBuffer->bindDescriptorSets(pipelineLayout.get(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                          DescriptorSetIndex::Internal, 1, &descriptorSet, 0,
                                          nullptr);
        contextVk->invalidateGraphicsDescriptorSet(DescriptorSetIndex::Internal);
    }

    if (pushConstants)
    {
        commandBuffer->pushConstants(pipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                     static_cast<uint32_t>(pushConstantsSize), pushConstants);
    }

    ResetDynamicState(contextVk, commandBuffer);

    return angle::Result::Continue;
}

angle::Result UtilsVk::convertIndexBuffer(ContextVk *contextVk,
                                          vk::BufferHelper *dst,
                                          vk::BufferHelper *src,
                                          const ConvertIndexParameters &params)
{
    ANGLE_TRY(ensureConvertIndexResourcesInitialized(contextVk));

    vk::CommandBufferAccess access;
    access.onBufferComputeShaderRead(src);
    access.onBufferComputeShaderWrite(dst);

    vk::OutsideRenderPassCommandBufferHelper *commandBufferHelper;
    vk::OutsideRenderPassCommandBuffer *commandBuffer;
    ANGLE_TRY(contextVk->getOutsideRenderPassCommandBufferHelper(access, &commandBufferHelper));
    commandBuffer = &commandBufferHelper->getCommandBuffer();

    VkDescriptorSet descriptorSet;
    ANGLE_TRY(allocateDescriptorSet(contextVk, commandBufferHelper, Function::ConvertIndexBuffer,
                                    &descriptorSet));

    std::array<VkDescriptorBufferInfo, 2> buffers = {{
        {dst->getBuffer().getHandle(), dst->getOffset(), dst->getSize()},
        {src->getBuffer().getHandle(), src->getOffset(), src->getSize()},
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

    ANGLE_TRY(setupComputeProgram(contextVk, Function::ConvertIndexBuffer, shader,
                                  &mConvertIndexPrograms[flags], descriptorSet, &shaderParams,
                                  sizeof(ConvertIndexShaderParams), commandBufferHelper));

    constexpr uint32_t kInvocationsPerGroup = 64;
    constexpr uint32_t kInvocationsPerIndex = 2;
    const uint32_t kIndexCount              = params.maxIndex;
    const uint32_t kGroupCount =
        UnsignedCeilDivide(kIndexCount * kInvocationsPerIndex, kInvocationsPerGroup);
    commandBuffer->dispatch(kGroupCount, 1, 1);

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

    vk::CommandBufferAccess access;
    access.onBufferComputeShaderRead(srcIndirectBuf);
    access.onBufferComputeShaderRead(srcIndexBuf);
    access.onBufferComputeShaderWrite(dstIndirectBuf);
    access.onBufferComputeShaderWrite(dstIndexBuf);

    vk::OutsideRenderPassCommandBufferHelper *commandBufferHelper;
    vk::OutsideRenderPassCommandBuffer *commandBuffer;
    ANGLE_TRY(contextVk->getOutsideRenderPassCommandBufferHelper(access, &commandBufferHelper));
    commandBuffer = &commandBufferHelper->getCommandBuffer();

    VkDescriptorSet descriptorSet;
    ANGLE_TRY(allocateDescriptorSet(contextVk, commandBufferHelper,
                                    Function::ConvertIndexIndirectBuffer, &descriptorSet));

    std::array<VkDescriptorBufferInfo, 4> buffers = {{
        {dstIndexBuf->getBuffer().getHandle(), dstIndexBuf->getOffset(), dstIndexBuf->getSize()},
        {srcIndexBuf->getBuffer().getHandle(), srcIndexBuf->getOffset(), srcIndexBuf->getSize()},
        {srcIndirectBuf->getBuffer().getHandle(), srcIndirectBuf->getOffset(),
         srcIndirectBuf->getSize()},
        {dstIndirectBuf->getBuffer().getHandle(), dstIndirectBuf->getOffset(),
         dstIndirectBuf->getSize()},
    }};

    VkWriteDescriptorSet writeInfo = {};
    writeInfo.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfo.dstSet               = descriptorSet;
    writeInfo.dstBinding           = kConvertIndexDestinationBinding;
    writeInfo.descriptorCount      = 4;
    writeInfo.descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeInfo.pBufferInfo          = buffers.data();

    vkUpdateDescriptorSets(contextVk->getDevice(), 1, &writeInfo, 0, nullptr);

    ConvertIndexIndirectShaderParams shaderParams = {
        params.srcIndirectBufOffset >> 2, params.srcIndexBufOffset, params.dstIndexBufOffset >> 2,
        params.maxIndex, params.dstIndirectBufOffset >> 2};

    uint32_t flags = vk::InternalShader::ConvertIndex_comp::kIsIndirect;
    if (contextVk->getState().isPrimitiveRestartEnabled())
    {
        flags |= vk::InternalShader::ConvertIndex_comp::kIsPrimitiveRestartEnabled;
    }

    vk::RefCounted<vk::ShaderAndSerial> *shader = nullptr;
    ANGLE_TRY(contextVk->getShaderLibrary().getConvertIndex_comp(contextVk, flags, &shader));

    ANGLE_TRY(setupComputeProgram(contextVk, Function::ConvertIndexIndirectBuffer, shader,
                                  &mConvertIndexPrograms[flags], descriptorSet, &shaderParams,
                                  sizeof(ConvertIndexIndirectShaderParams), commandBufferHelper));

    constexpr uint32_t kInvocationsPerGroup = 64;
    constexpr uint32_t kInvocationsPerIndex = 2;
    const uint32_t kIndexCount              = params.maxIndex;
    const uint32_t kGroupCount =
        UnsignedCeilDivide(kIndexCount * kInvocationsPerIndex, kInvocationsPerGroup);
    commandBuffer->dispatch(kGroupCount, 1, 1);

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

    vk::CommandBufferAccess access;
    access.onBufferComputeShaderRead(srcIndirectBuffer);
    access.onBufferComputeShaderRead(srcIndexBuffer);
    access.onBufferComputeShaderWrite(dstIndirectBuffer);
    access.onBufferComputeShaderWrite(dstIndexBuffer);

    vk::OutsideRenderPassCommandBufferHelper *commandBufferHelper;
    vk::OutsideRenderPassCommandBuffer *commandBuffer;
    ANGLE_TRY(contextVk->getOutsideRenderPassCommandBufferHelper(access, &commandBufferHelper));
    commandBuffer = &commandBufferHelper->getCommandBuffer();

    VkDescriptorSet descriptorSet;
    ANGLE_TRY(allocateDescriptorSet(contextVk, commandBufferHelper,
                                    Function::ConvertIndexIndirectLineLoopBuffer, &descriptorSet));

    std::array<VkDescriptorBufferInfo, 4> buffers = {{
        {dstIndexBuffer->getBuffer().getHandle(), dstIndexBuffer->getOffset(),
         dstIndexBuffer->getSize()},
        {srcIndexBuffer->getBuffer().getHandle(), srcIndexBuffer->getOffset(),
         srcIndexBuffer->getSize()},
        {srcIndirectBuffer->getBuffer().getHandle(), srcIndirectBuffer->getOffset(),
         srcIndirectBuffer->getSize()},
        {dstIndirectBuffer->getBuffer().getHandle(), dstIndirectBuffer->getOffset(),
         dstIndirectBuffer->getSize()},
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
        params.srcIndexBufferOffset, params.dstIndexBufferOffset >> 2,
        contextVk->getState().isPrimitiveRestartEnabled()};

    uint32_t flags = GetConvertIndexIndirectLineLoopFlag(params.indicesBitsWidth);

    vk::RefCounted<vk::ShaderAndSerial> *shader = nullptr;
    ANGLE_TRY(contextVk->getShaderLibrary().getConvertIndexIndirectLineLoop_comp(contextVk, flags,
                                                                                 &shader));

    ANGLE_TRY(setupComputeProgram(contextVk, Function::ConvertIndexIndirectLineLoopBuffer, shader,
                                  &mConvertIndexIndirectLineLoopPrograms[flags], descriptorSet,
                                  &shaderParams, sizeof(ConvertIndexIndirectLineLoopShaderParams),
                                  commandBufferHelper));

    commandBuffer->dispatch(1, 1, 1);

    return angle::Result::Continue;
}

angle::Result UtilsVk::convertLineLoopArrayIndirectBuffer(
    ContextVk *contextVk,
    vk::BufferHelper *srcIndirectBuffer,
    vk::BufferHelper *dstIndirectBuffer,
    vk::BufferHelper *dstIndexBuffer,
    const ConvertLineLoopArrayIndirectParameters &params)
{
    ANGLE_TRY(ensureConvertIndirectLineLoopResourcesInitialized(contextVk));

    vk::CommandBufferAccess access;
    access.onBufferComputeShaderRead(srcIndirectBuffer);
    access.onBufferComputeShaderWrite(dstIndirectBuffer);
    access.onBufferComputeShaderWrite(dstIndexBuffer);

    vk::OutsideRenderPassCommandBufferHelper *commandBufferHelper;
    vk::OutsideRenderPassCommandBuffer *commandBuffer;
    ANGLE_TRY(contextVk->getOutsideRenderPassCommandBufferHelper(access, &commandBufferHelper));
    commandBuffer = &commandBufferHelper->getCommandBuffer();

    VkDescriptorSet descriptorSet;
    ANGLE_TRY(allocateDescriptorSet(contextVk, commandBufferHelper,
                                    Function::ConvertIndirectLineLoopBuffer, &descriptorSet));

    std::array<VkDescriptorBufferInfo, 3> buffers = {{
        {srcIndirectBuffer->getBuffer().getHandle(), srcIndirectBuffer->getOffset(),
         srcIndirectBuffer->getSize()},
        {dstIndirectBuffer->getBuffer().getHandle(), dstIndirectBuffer->getOffset(),
         dstIndirectBuffer->getSize()},
        {dstIndexBuffer->getBuffer().getHandle(), dstIndexBuffer->getOffset(),
         dstIndexBuffer->getSize()},
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

    ANGLE_TRY(setupComputeProgram(contextVk, Function::ConvertIndirectLineLoopBuffer, shader,
                                  &mConvertIndirectLineLoopPrograms[flags], descriptorSet,
                                  &shaderParams, sizeof(ConvertIndirectLineLoopShaderParams),
                                  commandBufferHelper));

    commandBuffer->dispatch(1, 1, 1);

    return angle::Result::Continue;
}

angle::Result UtilsVk::convertVertexBuffer(ContextVk *contextVk,
                                           vk::BufferHelper *dst,
                                           vk::BufferHelper *src,
                                           const ConvertVertexParameters &params)
{
    vk::CommandBufferAccess access;
    access.onBufferComputeShaderRead(src);
    access.onBufferComputeShaderWrite(dst);

    vk::OutsideRenderPassCommandBufferHelper *commandBufferHelper;
    ANGLE_TRY(contextVk->getOutsideRenderPassCommandBufferHelper(access, &commandBufferHelper));

    ConvertVertexShaderParams shaderParams;
    shaderParams.Ns = params.srcFormat->channelCount;
    shaderParams.Bs = params.srcFormat->pixelBytes / params.srcFormat->channelCount;
    shaderParams.Ss = static_cast<uint32_t>(params.srcStride);
    shaderParams.Nd = params.dstFormat->channelCount;
    shaderParams.Bd = params.dstFormat->pixelBytes / params.dstFormat->channelCount;
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
    shaderParams.outputCount = UnsignedCeilDivide(shaderParams.componentCount, shaderParams.Ed);
    shaderParams.srcOffset   = static_cast<uint32_t>(params.srcOffset);
    shaderParams.dstOffset   = static_cast<uint32_t>(params.dstOffset);

    bool isSrcA2BGR10 =
        params.srcFormat->vertexAttribType == gl::VertexAttribType::UnsignedInt2101010 ||
        params.srcFormat->vertexAttribType == gl::VertexAttribType::Int2101010;
    bool isSrcRGB10A2 =
        params.srcFormat->vertexAttribType == gl::VertexAttribType::UnsignedInt1010102 ||
        params.srcFormat->vertexAttribType == gl::VertexAttribType::Int1010102;

    shaderParams.isSrcHDR     = isSrcA2BGR10 || isSrcRGB10A2;
    shaderParams.isSrcA2BGR10 = isSrcA2BGR10;

    uint32_t flags = GetConvertVertexFlags(params);

    // See GLES3.0 section 2.9.1 Transferring Array Elements
    const uint32_t srcValueBits = shaderParams.isSrcHDR ? 2 : shaderParams.Bs * 8;
    const uint32_t srcValueMask =
        srcValueBits == 32 ? 0xFFFFFFFFu : angle::BitMask<uint32_t>(srcValueBits);
    switch (flags)
    {
        case ConvertVertex_comp::kSintToSint:
        case ConvertVertex_comp::kSintToFloat:
        case ConvertVertex_comp::kUintToFloat:
            // For integers, alpha should take a value of 1.
            shaderParams.srcEmulatedAlpha = 1;
            break;

        case ConvertVertex_comp::kUintToUint:
            // For integers, alpha should take a value of 1.  However, uint->uint is also used to
            // add channels to RGB snorm, unorm and half formats.
            if (params.dstFormat->isSnorm())
            {
                // See case ConvertVertex_comp::kSnormToFloat below.
                shaderParams.srcEmulatedAlpha = srcValueMask >> 1;
            }
            else if (params.dstFormat->isUnorm())
            {
                // See case ConvertVertex_comp::kUnormToFloat below.
                shaderParams.srcEmulatedAlpha = srcValueMask;
            }
            else if (params.dstFormat->isVertexTypeHalfFloat())
            {
                shaderParams.srcEmulatedAlpha = gl::Float16One;
            }
            else
            {
                shaderParams.srcEmulatedAlpha = 1;
            }
            break;

        case ConvertVertex_comp::kSnormToFloat:
            // The largest signed number with as many bits as the alpha channel of the source is
            // 0b011...1 which is srcValueMask >> 1
            shaderParams.srcEmulatedAlpha = srcValueMask >> 1;
            break;

        case ConvertVertex_comp::kUnormToFloat:
            // The largest unsigned number with as many bits as the alpha channel of the source is
            // 0b11...1 which is srcValueMask
            shaderParams.srcEmulatedAlpha = srcValueMask;
            break;

        case ConvertVertex_comp::kFixedToFloat:
            // 1.0 in fixed point is 0x10000
            shaderParams.srcEmulatedAlpha = 0x10000;
            break;

        case ConvertVertex_comp::kFloatToFloat:
            ASSERT(ValidateFloatOneAsUint());
            shaderParams.srcEmulatedAlpha = gl::Float32One;
            break;

        default:
            UNREACHABLE();
    }

    return convertVertexBufferImpl(contextVk, dst, src, flags, commandBufferHelper, shaderParams);
}

angle::Result UtilsVk::convertVertexBufferImpl(
    ContextVk *contextVk,
    vk::BufferHelper *dst,
    vk::BufferHelper *src,
    uint32_t flags,
    vk::OutsideRenderPassCommandBufferHelper *commandBufferHelper,
    const ConvertVertexShaderParams &shaderParams)
{
    ANGLE_TRY(ensureConvertVertexResourcesInitialized(contextVk));

    vk::OutsideRenderPassCommandBuffer *commandBuffer;
    commandBuffer = &commandBufferHelper->getCommandBuffer();

    VkDescriptorSet descriptorSet;
    ANGLE_TRY(allocateDescriptorSet(contextVk, commandBufferHelper, Function::ConvertVertexBuffer,
                                    &descriptorSet));

    VkWriteDescriptorSet writeInfo    = {};
    VkDescriptorBufferInfo buffers[2] = {
        {dst->getBuffer().getHandle(), dst->getOffset(), dst->getSize()},
        {src->getBuffer().getHandle(), src->getOffset(), src->getSize()},
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

    ANGLE_TRY(setupComputeProgram(contextVk, Function::ConvertVertexBuffer, shader,
                                  &mConvertVertexPrograms[flags], descriptorSet, &shaderParams,
                                  sizeof(shaderParams), commandBufferHelper));

    commandBuffer->dispatch(UnsignedCeilDivide(shaderParams.outputCount, 64), 1, 1);

    return angle::Result::Continue;
}

angle::Result UtilsVk::startRenderPass(ContextVk *contextVk,
                                       vk::ImageHelper *image,
                                       const vk::ImageView *imageView,
                                       const vk::RenderPassDesc &renderPassDesc,
                                       const gl::Rectangle &renderArea,
                                       vk::RenderPassCommandBuffer **commandBufferOut)
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
    vk::PackedClearValuesArray clearValues;
    clearValues.store(vk::kAttachmentIndexZero, VK_IMAGE_ASPECT_COLOR_BIT, {});

    renderPassAttachmentOps.initWithLoadStore(vk::kAttachmentIndexZero,
                                              vk::ImageLayout::ColorAttachment,
                                              vk::ImageLayout::ColorAttachment);

    ANGLE_TRY(contextVk->beginNewRenderPass(
        framebuffer, renderArea, renderPassDesc, renderPassAttachmentOps,
        vk::PackedAttachmentCount(1), vk::kAttachmentIndexInvalid, clearValues, commandBufferOut));

    contextVk->addGarbage(&framebuffer);

    return angle::Result::Continue;
}

angle::Result UtilsVk::clearFramebuffer(ContextVk *contextVk,
                                        FramebufferVk *framebuffer,
                                        const ClearFramebufferParameters &params)
{
    ANGLE_TRY(ensureImageClearResourcesInitialized(contextVk));

    const gl::Rectangle &scissoredRenderArea = params.clearArea;
    vk::Framebuffer *currentFramebuffer      = nullptr;
    vk::RenderPassCommandBuffer *commandBuffer;

    // Start a new render pass if not already started
    ANGLE_TRY(framebuffer->getFramebuffer(contextVk, &currentFramebuffer, nullptr,
                                          SwapchainResolveMode::Disabled));
    if (contextVk->hasStartedRenderPassWithFramebuffer(currentFramebuffer))
    {
        vk::RenderPassCommandBufferHelper *renderPassCommands =
            &contextVk->getStartedRenderPassCommands();
        renderPassCommands->growRenderArea(contextVk, scissoredRenderArea);

        commandBuffer = &renderPassCommands->getCommandBuffer();
    }
    else
    {
        ANGLE_TRY(contextVk->startRenderPass(scissoredRenderArea, &commandBuffer, nullptr));
    }

    UpdateColorAccess(contextVk, framebuffer->getState().getColorAttachmentsMask(),
                      MakeColorBufferMask(params.colorAttachmentIndexGL));
    UpdateDepthStencilAccess(contextVk, framebuffer, params.clearDepth, params.clearStencil);

    ImageClearShaderParams shaderParams;
    shaderParams.clearValue = params.colorClearValue;
    shaderParams.clearDepth = params.depthStencilClearValue.depth;

    vk::GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.initDefaults(contextVk);
    pipelineDesc.setColorWriteMasks(0, gl::DrawBufferMask(), gl::DrawBufferMask());
    pipelineDesc.setSingleColorWriteMask(params.colorAttachmentIndexGL, params.colorMaskFlags);
    pipelineDesc.setRasterizationSamples(framebuffer->getSamples());
    pipelineDesc.setRenderPassDesc(framebuffer->getRenderPassDesc());
    // Clears can be done on a currently open render pass, so make sure the correct subpass index is
    // used.
    pipelineDesc.setSubpass(contextVk->getCurrentSubpassIndex());

    // Clear depth by enabling depth clamping and setting the viewport depth range to the clear
    // value if possible.  Otherwise use the shader to export depth.
    const bool supportsDepthClamp =
        contextVk->getRenderer()->getPhysicalDeviceFeatures().depthClamp == VK_TRUE;
    if (params.clearDepth)
    {
        if (!contextVk->getFeatures().supportsExtendedDynamicState.enabled)
        {
            pipelineDesc.setDepthTestEnabled(true);
            pipelineDesc.setDepthWriteEnabled(true);
            pipelineDesc.setDepthFunc(VK_COMPARE_OP_ALWAYS);
        }
        if (supportsDepthClamp)
        {
            // Note: this path requires the depthClamp Vulkan feature.
            pipelineDesc.setDepthClampEnabled(true);
        }
    }

    // Clear stencil by enabling stencil write with the right mask.
    if (params.clearStencil && !contextVk->getFeatures().supportsExtendedDynamicState.enabled)
    {
        SetStencilStateForWrite(&pipelineDesc);
    }

    vk::ShaderLibrary &shaderLibrary                    = contextVk->getShaderLibrary();
    vk::RefCounted<vk::ShaderAndSerial> *vertexShader   = nullptr;
    vk::RefCounted<vk::ShaderAndSerial> *fragmentShader = nullptr;
    vk::ShaderProgramHelper *imageClearProgram          = &mImageClearProgramVSOnly;

    ANGLE_TRY(shaderLibrary.getFullScreenTri_vert(contextVk, 0, &vertexShader));
    if (params.clearColor)
    {
        const uint32_t flags =
            GetImageClearFlags(*params.colorFormat, params.colorAttachmentIndexGL,
                               params.clearDepth && !supportsDepthClamp);
        ANGLE_TRY(shaderLibrary.getImageClear_frag(contextVk, flags, &fragmentShader));
        imageClearProgram = &mImageClearPrograms[flags];
    }

    // Make sure transform feedback is paused.  Needs to be done before binding the pipeline as
    // that's not allowed in Vulkan.
    const bool isTransformFeedbackActiveUnpaused =
        contextVk->getStartedRenderPassCommands().isTransformFeedbackActiveUnpaused();
    contextVk->pauseTransformFeedbackIfActiveUnpaused();

    ANGLE_TRY(setupGraphicsProgram(contextVk, Function::ImageClear, vertexShader, fragmentShader,
                                   imageClearProgram, &pipelineDesc, VK_NULL_HANDLE, &shaderParams,
                                   sizeof(shaderParams), commandBuffer));

    // Set dynamic state
    VkViewport viewport;
    gl::Rectangle completeRenderArea = framebuffer->getRotatedCompleteRenderArea(contextVk);
    bool invertViewport              = contextVk->isViewportFlipEnabledForDrawFBO();
    bool clipSpaceOriginUpperLeft =
        contextVk->getState().getClipSpaceOrigin() == gl::ClipSpaceOrigin::UpperLeft;
    // Set depth range to clear value.  If clearing depth, the vertex shader depth output is clamped
    // to this value, thus clearing the depth buffer to the desired clear value.
    const float clearDepthValue = params.depthStencilClearValue.depth;
    gl_vk::GetViewport(completeRenderArea, clearDepthValue, clearDepthValue, invertViewport,
                       clipSpaceOriginUpperLeft, completeRenderArea.height, &viewport);
    commandBuffer->setViewport(0, 1, &viewport);

    const VkRect2D scissor = gl_vk::GetRect(params.clearArea);
    commandBuffer->setScissor(0, 1, &scissor);

    if (params.clearDepth && contextVk->getFeatures().supportsExtendedDynamicState.enabled)
    {
        commandBuffer->setDepthTestEnable(VK_TRUE);
        commandBuffer->setDepthWriteEnable(VK_TRUE);
        commandBuffer->setDepthCompareOp(VK_COMPARE_OP_ALWAYS);
    }

    if (params.clearStencil)
    {
        constexpr uint8_t compareMask = 0xFF;
        const uint8_t clearStencilValue =
            static_cast<uint8_t>(params.depthStencilClearValue.stencil);

        commandBuffer->setStencilCompareMask(compareMask, compareMask);
        commandBuffer->setStencilWriteMask(params.stencilMask, params.stencilMask);
        commandBuffer->setStencilReference(clearStencilValue, clearStencilValue);

        if (contextVk->getFeatures().supportsExtendedDynamicState.enabled)
        {
            SetStencilDynamicStateForWrite(commandBuffer);
        }
    }

    // Make sure this draw call doesn't count towards occlusion query results.
    contextVk->pauseRenderPassQueriesIfActive();

    commandBuffer->draw(3, 0);
    ANGLE_TRY(contextVk->resumeRenderPassQueriesIfActive());

    // If transform feedback was active, we can't pause and resume it in the same render pass
    // because we can't insert a memory barrier for the counter buffers.  In that case, break the
    // render pass.
    if (isTransformFeedbackActiveUnpaused)
    {
        ANGLE_TRY(contextVk->flushCommandsAndEndRenderPass(
            RenderPassClosureReason::XfbResumeAfterDrawBasedClear));
    }

    return angle::Result::Continue;
}

angle::Result UtilsVk::clearImage(ContextVk *contextVk,
                                  vk::ImageHelper *dst,
                                  const ClearImageParameters &params)
{
    ANGLE_TRY(ensureImageClearResourcesInitialized(contextVk));

    const angle::Format &dstActualFormat = dst->getActualFormat();

    // Currently, this function is only used to clear emulated channels of color images.
    ASSERT(!dstActualFormat.hasDepthOrStencilBits());

    // TODO: currently this function is only implemented for images that are drawable.  If needed,
    // for images that are not drawable, the following algorithm can be used.
    //
    // - Copy image to temp buffer
    // - Use convertVertexBufferImpl to overwrite the alpha channel
    // - Copy the result back to the image
    //
    // Note that the following check is not enough; if the image is AHB-imported, then the draw path
    // cannot be taken if AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER hasn't been specified, even if the
    // format is renderable.
    //
    // http://anglebug.com/6151
    if (!vk::FormatHasNecessaryFeature(contextVk->getRenderer(), dstActualFormat.id,
                                       dst->getTilingMode(),
                                       VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }

    vk::DeviceScoped<vk::ImageView> destView(contextVk->getDevice());
    const gl::TextureType destViewType = vk::Get2DTextureType(1, dst->getSamples());

    ANGLE_TRY(dst->initLayerImageView(contextVk, destViewType, VK_IMAGE_ASPECT_COLOR_BIT,
                                      gl::SwizzleState(), &destView.get(), params.dstMip, 1,
                                      params.dstLayer, 1, gl::SrgbWriteControlMode::Default,
                                      gl::YuvSamplingMode::Default));

    const gl::Rectangle &renderArea = params.clearArea;

    ImageClearShaderParams shaderParams;
    shaderParams.clearValue = params.colorClearValue;
    shaderParams.clearDepth = 0;

    vk::RenderPassDesc renderPassDesc;
    renderPassDesc.setSamples(dst->getSamples());
    renderPassDesc.packColorAttachment(0, dstActualFormat.id);

    vk::GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.initDefaults(contextVk);
    pipelineDesc.setSingleColorWriteMask(0, params.colorMaskFlags);
    pipelineDesc.setRasterizationSamples(dst->getSamples());
    pipelineDesc.setRenderPassDesc(renderPassDesc);

    vk::RenderPassCommandBuffer *commandBuffer;
    ANGLE_TRY(startRenderPass(contextVk, dst, &destView.get(), renderPassDesc, renderArea,
                              &commandBuffer));

    UpdateColorAccess(contextVk, MakeColorBufferMask(0), MakeColorBufferMask(0));

    contextVk->onImageRenderPassWrite(dst->toGLLevel(params.dstMip), params.dstLayer, 1,
                                      VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::ColorAttachment,
                                      dst);

    const uint32_t flags = GetImageClearFlags(dstActualFormat, 0, false);

    vk::ShaderLibrary &shaderLibrary                    = contextVk->getShaderLibrary();
    vk::RefCounted<vk::ShaderAndSerial> *vertexShader   = nullptr;
    vk::RefCounted<vk::ShaderAndSerial> *fragmentShader = nullptr;
    ANGLE_TRY(shaderLibrary.getFullScreenTri_vert(contextVk, 0, &vertexShader));
    ANGLE_TRY(shaderLibrary.getImageClear_frag(contextVk, flags, &fragmentShader));

    ANGLE_TRY(setupGraphicsProgram(contextVk, Function::ImageClear, vertexShader, fragmentShader,
                                   &mImageClearPrograms[flags], &pipelineDesc, VK_NULL_HANDLE,
                                   &shaderParams, sizeof(shaderParams), commandBuffer));

    // Set dynamic state
    VkViewport viewport;
    gl_vk::GetViewport(renderArea, 0.0f, 1.0f, false, false, dst->getExtents().height, &viewport);
    commandBuffer->setViewport(0, 1, &viewport);

    VkRect2D scissor = gl_vk::GetRect(renderArea);
    commandBuffer->setScissor(0, 1, &scissor);

    // Note: this utility creates its own framebuffer, thus bypassing ContextVk::startRenderPass.
    // As such, occlusion queries are not enabled.
    commandBuffer->draw(3, 0);

    vk::ImageView destViewObject = destView.release();
    contextVk->addGarbage(&destViewObject);

    // Close the render pass for this temporary framebuffer.
    return contextVk->flushCommandsAndEndRenderPass(
        RenderPassClosureReason::TemporaryForImageClear);
}

angle::Result UtilsVk::colorBlitResolve(ContextVk *contextVk,
                                        FramebufferVk *framebuffer,
                                        vk::ImageHelper *src,
                                        const vk::ImageView *srcView,
                                        const BlitResolveParameters &params)
{
    // The views passed to this function are already retained, so a render pass cannot be already
    // open.  Otherwise, this function closes the render pass, which may incur a vkQueueSubmit and
    // then the views are used in a new command buffer without having been retained for it.
    // http://crbug.com/1272266#c22
    //
    // Note that depth/stencil views for blit are not derived from a ResourceVk object and are
    // retained differently.
    ASSERT(!contextVk->hasStartedRenderPass());

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
    // Note: adjustments made for pre-rotatation in FramebufferVk::blit() affect these
    // Calculate*Offset() functions.
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
    shaderParams.outputMask      = framebuffer->getState().getEnabledDrawBuffers().bits();
    shaderParams.flipX           = params.flipX;
    shaderParams.flipY           = params.flipY;
    shaderParams.rotateXY        = 0;

    // Potentially make adjustments for pre-rotation.  Depending on the angle some of the
    // shaderParams need to be adjusted.
    switch (params.rotation)
    {
        case SurfaceRotation::Identity:
        case SurfaceRotation::Rotated90Degrees:
            break;
        case SurfaceRotation::Rotated180Degrees:
        case SurfaceRotation::Rotated270Degrees:
            if (isResolve)
            {
                // Align the offset with minus 1, or the sample position near the edge will be
                // wrong.
                shaderParams.offset.resolve[0] += params.rotatedOffsetFactor[0] - 1;
                shaderParams.offset.resolve[1] += params.rotatedOffsetFactor[1] - 1;
            }
            else
            {
                shaderParams.offset.blit[0] += params.rotatedOffsetFactor[0];
                shaderParams.offset.blit[1] += params.rotatedOffsetFactor[1];
            }
            break;
        default:
            UNREACHABLE();
            break;
    }

    shaderParams.rotateXY = IsRotatedAspectRatio(params.rotation);

    bool blitColor   = srcColorView != nullptr;
    bool blitDepth   = srcDepthView != nullptr;
    bool blitStencil = srcStencilView != nullptr;

    // Either color is blitted/resolved or depth/stencil, but not both.
    ASSERT(blitColor != (blitDepth || blitStencil));

    // Linear sampling is only valid with color blitting.
    ASSERT((blitColor && !isResolve) || !params.linear);

    uint32_t flags =
        GetBlitResolveFlags(blitColor, blitDepth, blitStencil, src->getIntendedFormat());
    flags |= src->getLayerCount() > 1 ? BlitResolve_frag::kSrcIsArray : 0;
    flags |= isResolve ? BlitResolve_frag::kIsResolve : 0;

    vk::GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.initDefaults(contextVk);
    if (blitColor)
    {
        constexpr VkColorComponentFlags kAllColorComponents =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;

        pipelineDesc.setColorWriteMasks(
            gl::BlendStateExt::ColorMaskStorage::GetReplicatedValue(
                kAllColorComponents, gl::BlendStateExt::ColorMaskStorage::GetMask(
                                         framebuffer->getRenderPassDesc().colorAttachmentRange())),
            framebuffer->getEmulatedAlphaAttachmentMask(), ~gl::DrawBufferMask());
    }
    else
    {
        pipelineDesc.setColorWriteMasks(0, gl::DrawBufferMask(), gl::DrawBufferMask());
    }
    pipelineDesc.setRenderPassDesc(framebuffer->getRenderPassDesc());
    if (blitDepth && !contextVk->getFeatures().supportsExtendedDynamicState.enabled)
    {
        pipelineDesc.setDepthTestEnabled(VK_TRUE);
        pipelineDesc.setDepthWriteEnabled(VK_TRUE);
        pipelineDesc.setDepthFunc(VK_COMPARE_OP_ALWAYS);
    }

    if (blitStencil && !contextVk->getFeatures().supportsExtendedDynamicState.enabled)
    {
        SetStencilStateForWrite(&pipelineDesc);
    }

    vk::RenderPassCommandBuffer *commandBuffer;
    ANGLE_TRY(framebuffer->startNewRenderPass(contextVk, params.blitArea, &commandBuffer, nullptr));

    VkDescriptorSet descriptorSet;
    ANGLE_TRY(allocateDescriptorSet(contextVk, &contextVk->getStartedRenderPassCommands(),
                                    Function::BlitResolve, &descriptorSet));

    contextVk->onImageRenderPassRead(src->getAspectFlags(), vk::ImageLayout::FragmentShaderReadOnly,
                                     src);

    UpdateColorAccess(contextVk, framebuffer->getState().getColorAttachmentsMask(),
                      framebuffer->getState().getEnabledDrawBuffers());
    UpdateDepthStencilAccess(contextVk, framebuffer, blitDepth, blitStencil);

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
    ANGLE_TRY(shaderLibrary.getFullScreenTri_vert(contextVk, 0, &vertexShader));
    ANGLE_TRY(shaderLibrary.getBlitResolve_frag(contextVk, flags, &fragmentShader));

    ANGLE_TRY(setupGraphicsProgram(contextVk, Function::BlitResolve, vertexShader, fragmentShader,
                                   &mBlitResolvePrograms[flags], &pipelineDesc, descriptorSet,
                                   &shaderParams, sizeof(shaderParams), commandBuffer));

    // Set dynamic state
    VkViewport viewport;
    gl::Rectangle completeRenderArea = framebuffer->getRotatedCompleteRenderArea(contextVk);
    gl_vk::GetViewport(completeRenderArea, 0.0f, 1.0f, false, false, completeRenderArea.height,
                       &viewport);
    commandBuffer->setViewport(0, 1, &viewport);

    VkRect2D scissor = gl_vk::GetRect(params.blitArea);
    commandBuffer->setScissor(0, 1, &scissor);

    if (blitDepth && contextVk->getFeatures().supportsExtendedDynamicState.enabled)
    {
        commandBuffer->setDepthTestEnable(VK_TRUE);
        commandBuffer->setDepthWriteEnable(VK_TRUE);
        commandBuffer->setDepthCompareOp(VK_COMPARE_OP_ALWAYS);
    }

    if (blitStencil)
    {
        constexpr uint8_t completeMask    = 0xFF;
        constexpr uint8_t unusedReference = 0x00;

        commandBuffer->setStencilCompareMask(completeMask, completeMask);
        commandBuffer->setStencilWriteMask(completeMask, completeMask);
        commandBuffer->setStencilReference(unusedReference, unusedReference);

        if (contextVk->getFeatures().supportsExtendedDynamicState.enabled)
        {
            SetStencilDynamicStateForWrite(commandBuffer);
        }
    }

    // Note: this utility starts the render pass directly, thus bypassing
    // ContextVk::startRenderPass. As such, occlusion queries are not enabled.
    commandBuffer->draw(3, 0);

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

    ANGLE_TRY(blitBuffer.get().initSuballocation(
        contextVk, contextVk->getRenderer()->getDeviceLocalMemoryTypeIndex(),
        static_cast<size_t>(bufferSize), contextVk->getRenderer()->getDefaultBufferAlignment()));

    BlitResolveStencilNoExportShaderParams shaderParams;
    // Note: adjustments made for pre-rotatation in FramebufferVk::blit() affect these
    // Calculate*Offset() functions.
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
    shaderParams.dstPitch        = bufferRowLengthInUints;
    shaderParams.blitArea[0]     = params.blitArea.x;
    shaderParams.blitArea[1]     = params.blitArea.y;
    shaderParams.blitArea[2]     = params.blitArea.width;
    shaderParams.blitArea[3]     = params.blitArea.height;
    shaderParams.flipX           = params.flipX;
    shaderParams.flipY           = params.flipY;
    shaderParams.rotateXY        = 0;

    // Potentially make adjustments for pre-rotatation.  Depending on the angle some of the
    // shaderParams need to be adjusted.
    switch (params.rotation)
    {
        case SurfaceRotation::Identity:
        case SurfaceRotation::Rotated90Degrees:
            break;
        case SurfaceRotation::Rotated180Degrees:
        case SurfaceRotation::Rotated270Degrees:
            if (isResolve)
            {
                // Align the offset with minus 1, or the sample position near the edge will be
                // wrong.
                shaderParams.offset.resolve[0] += params.rotatedOffsetFactor[0] - 1;
                shaderParams.offset.resolve[1] += params.rotatedOffsetFactor[1] - 1;
            }
            else
            {
                shaderParams.offset.blit[0] += params.rotatedOffsetFactor[0];
                shaderParams.offset.blit[1] += params.rotatedOffsetFactor[1];
            }
            break;
        default:
            UNREACHABLE();
            break;
    }

    shaderParams.rotateXY = IsRotatedAspectRatio(params.rotation);

    // Linear sampling is only valid with color blitting.
    ASSERT(!params.linear);

    uint32_t flags = src->getLayerCount() > 1 ? BlitResolveStencilNoExport_comp::kSrcIsArray : 0;
    flags |= isResolve ? BlitResolve_frag::kIsResolve : 0;

    RenderTargetVk *depthStencilRenderTarget = framebuffer->getDepthStencilRenderTarget();
    ASSERT(depthStencilRenderTarget != nullptr);
    vk::ImageHelper *depthStencilImage = &depthStencilRenderTarget->getImageForWrite();

    // Change layouts prior to computation.
    vk::CommandBufferAccess access;
    access.onImageComputeShaderRead(src->getAspectFlags(), src);
    access.onImageTransferWrite(depthStencilRenderTarget->getLevelIndex(), 1,
                                depthStencilRenderTarget->getLayerIndex(), 1,
                                depthStencilImage->getAspectFlags(), depthStencilImage);
    access.onBufferComputeShaderWrite(&blitBuffer.get());

    VkDescriptorSet descriptorSet;
    vk::OutsideRenderPassCommandBufferHelper *commandBufferHelper;
    vk::OutsideRenderPassCommandBuffer *commandBuffer;
    ANGLE_TRY(contextVk->getOutsideRenderPassCommandBufferHelper(access, &commandBufferHelper));
    commandBuffer = &commandBufferHelper->getCommandBuffer();
    ANGLE_TRY(allocateDescriptorSet(contextVk, commandBufferHelper,
                                    Function::BlitResolveStencilNoExport, &descriptorSet));

    // Blit/resolve stencil into the buffer.
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageView             = srcStencilView->getHandle();
    imageInfo.imageLayout           = src->getCurrentLayout();

    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer                 = blitBuffer.get().getBuffer().getHandle();
    bufferInfo.offset                 = blitBuffer.get().getOffset();
    bufferInfo.range                  = blitBuffer.get().getSize();

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

    ANGLE_TRY(setupComputeProgram(contextVk, Function::BlitResolveStencilNoExport, shader,
                                  &mBlitResolveStencilNoExportPrograms[flags], descriptorSet,
                                  &shaderParams, sizeof(shaderParams), commandBufferHelper));
    commandBuffer->dispatch(UnsignedCeilDivide(bufferRowLengthInUints, 8),
                            UnsignedCeilDivide(params.blitArea.height, 8), 1);

    // Add a barrier prior to copy.
    VkMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType           = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask   = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask   = VK_ACCESS_TRANSFER_READ_BIT;

    commandBuffer->memoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, &memoryBarrier);

    // Copy the resulting buffer into dst.
    VkBufferImageCopy region           = {};
    region.bufferOffset                = blitBuffer.get().getOffset();
    region.bufferRowLength             = bufferRowLengthInUints * sizeof(uint32_t);
    region.bufferImageHeight           = params.blitArea.height;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    region.imageSubresource.mipLevel =
        depthStencilImage->toVkLevel(depthStencilRenderTarget->getLevelIndex()).get();
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
                                 vk::ImageHelper *dst,
                                 const vk::ImageView *destView,
                                 vk::ImageHelper *src,
                                 const vk::ImageView *srcView,
                                 const CopyImageParameters &params)
{
    // The views passed to this function are already retained, so a render pass cannot be already
    // open.  Otherwise, this function closes the render pass, which may incur a vkQueueSubmit and
    // then the views are used in a new command buffer without having been retained for it.
    // http://crbug.com/1272266#c22
    ASSERT(!contextVk->hasStartedRenderPass());

    ANGLE_TRY(ensureImageCopyResourcesInitialized(contextVk));

    const angle::Format &srcIntendedFormat = src->getIntendedFormat();
    const angle::Format &dstIntendedFormat = dst->getIntendedFormat();

    ImageCopyShaderParams shaderParams;
    shaderParams.flipX            = 0;
    shaderParams.flipY            = params.srcFlipY || params.dstFlipY;
    shaderParams.premultiplyAlpha = params.srcPremultiplyAlpha;
    shaderParams.unmultiplyAlpha  = params.srcUnmultiplyAlpha;
    shaderParams.dstHasLuminance  = dstIntendedFormat.luminanceBits > 0;
    shaderParams.dstIsAlpha       = dstIntendedFormat.isLUMA() && dstIntendedFormat.alphaBits > 0;
    shaderParams.dstDefaultChannelsMask =
        GetFormatDefaultChannelMask(dst->getIntendedFormat(), dst->getActualFormat());
    shaderParams.srcMip       = params.srcMip;
    shaderParams.srcLayer     = params.srcLayer;
    shaderParams.srcOffset[0] = params.srcOffset[0];
    shaderParams.srcOffset[1] = params.srcOffset[1];
    shaderParams.dstOffset[0] = params.dstOffset[0];
    shaderParams.dstOffset[1] = params.dstOffset[1];
    shaderParams.rotateXY     = 0;

    shaderParams.srcIsSRGB = params.srcColorEncoding == GL_SRGB;
    shaderParams.dstIsSRGB = params.dstColorEncoding == GL_SRGB;

    // If both src and dst are sRGB, and there is no alpha multiplication/division necessary, then
    // the shader can work with sRGB data and pretend they are linear.
    if (shaderParams.srcIsSRGB && shaderParams.dstIsSRGB && !shaderParams.premultiplyAlpha &&
        !shaderParams.unmultiplyAlpha)
    {
        shaderParams.srcIsSRGB = false;
        shaderParams.dstIsSRGB = false;
    }

    ASSERT(!(params.srcFlipY && params.dstFlipY));
    if (params.srcFlipY)
    {
        // If viewport is flipped, the shader expects srcOffset[1] to have the
        // last row's index instead of the first's.
        shaderParams.srcOffset[1] = params.srcHeight - params.srcOffset[1] - 1;
    }
    else if (params.dstFlipY)
    {
        // If image is flipped during copy, the shader uses the same code path as above,
        // with srcOffset being set to the last row's index instead of the first's.
        shaderParams.srcOffset[1] = params.srcOffset[1] + params.srcExtents[1] - 1;
    }

    switch (params.srcRotation)
    {
        case SurfaceRotation::Identity:
            break;
        case SurfaceRotation::Rotated90Degrees:
            shaderParams.rotateXY = 1;
            break;
        case SurfaceRotation::Rotated180Degrees:
            shaderParams.flipX = true;
            ASSERT(shaderParams.flipY);
            shaderParams.flipY = false;
            shaderParams.srcOffset[0] += params.srcExtents[0];
            shaderParams.srcOffset[1] -= params.srcExtents[1];
            break;
        case SurfaceRotation::Rotated270Degrees:
            shaderParams.flipX = true;
            ASSERT(!shaderParams.flipY);
            shaderParams.flipY = true;
            shaderParams.srcOffset[0] += params.srcExtents[0];
            shaderParams.srcOffset[1] += params.srcExtents[1];
            shaderParams.rotateXY = 1;
            break;
        default:
            UNREACHABLE();
            break;
    }

    uint32_t flags = GetImageCopyFlags(srcIntendedFormat, dstIntendedFormat);
    if (src->getType() == VK_IMAGE_TYPE_3D)
    {
        flags |= ImageCopy_frag::kSrcIs3D;
    }
    else if (src->getLayerCount() > 1)
    {
        flags |= ImageCopy_frag::kSrcIs2DArray;
    }
    else
    {
        flags |= ImageCopy_frag::kSrcIs2D;
    }

    vk::RenderPassDesc renderPassDesc;
    renderPassDesc.setSamples(dst->getSamples());
    renderPassDesc.packColorAttachment(0, dst->getActualFormatID());

    // Copy from multisampled image is not supported.
    ASSERT(src->getSamples() == 1);

    vk::GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.initDefaults(contextVk);
    pipelineDesc.setRenderPassDesc(renderPassDesc);
    pipelineDesc.setRasterizationSamples(dst->getSamples());

    gl::Rectangle renderArea;
    renderArea.x      = params.dstOffset[0];
    renderArea.y      = params.dstOffset[1];
    renderArea.width  = params.srcExtents[0];
    renderArea.height = params.srcExtents[1];
    if ((params.srcRotation == SurfaceRotation::Rotated90Degrees) ||
        (params.srcRotation == SurfaceRotation::Rotated270Degrees))
    {
        // The surface is rotated 90/270 degrees.  This changes the aspect ratio of the surface.
        std::swap(renderArea.x, renderArea.y);
        std::swap(renderArea.width, renderArea.height);
    }

    vk::RenderPassCommandBuffer *commandBuffer;
    ANGLE_TRY(
        startRenderPass(contextVk, dst, destView, renderPassDesc, renderArea, &commandBuffer));

    VkDescriptorSet descriptorSet;
    ANGLE_TRY(allocateDescriptorSet(contextVk, &contextVk->getStartedRenderPassCommands(),
                                    Function::ImageCopy, &descriptorSet));

    UpdateColorAccess(contextVk, MakeColorBufferMask(0), MakeColorBufferMask(0));

    // Change source layout inside render pass.
    contextVk->onImageRenderPassRead(VK_IMAGE_ASPECT_COLOR_BIT,
                                     vk::ImageLayout::FragmentShaderReadOnly, src);
    contextVk->onImageRenderPassWrite(params.dstMip, params.dstLayer, 1, VK_IMAGE_ASPECT_COLOR_BIT,
                                      vk::ImageLayout::ColorAttachment, dst);

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
    ANGLE_TRY(shaderLibrary.getFullScreenTri_vert(contextVk, 0, &vertexShader));
    ANGLE_TRY(shaderLibrary.getImageCopy_frag(contextVk, flags, &fragmentShader));

    ANGLE_TRY(setupGraphicsProgram(contextVk, Function::ImageCopy, vertexShader, fragmentShader,
                                   &mImageCopyPrograms[flags], &pipelineDesc, descriptorSet,
                                   &shaderParams, sizeof(shaderParams), commandBuffer));

    // Set dynamic state
    VkViewport viewport;
    gl_vk::GetViewport(renderArea, 0.0f, 1.0f, false, false, dst->getExtents().height, &viewport);
    commandBuffer->setViewport(0, 1, &viewport);

    VkRect2D scissor = gl_vk::GetRect(renderArea);
    commandBuffer->setScissor(0, 1, &scissor);

    // Note: this utility creates its own framebuffer, thus bypassing ContextVk::startRenderPass.
    // As such, occlusion queries are not enabled.
    commandBuffer->draw(3, 0);

    // Close the render pass for this temporary framebuffer.
    return contextVk->flushCommandsAndEndRenderPass(RenderPassClosureReason::TemporaryForImageCopy);
}

angle::Result UtilsVk::copyImageBits(ContextVk *contextVk,
                                     vk::ImageHelper *dst,
                                     vk::ImageHelper *src,
                                     const CopyImageBitsParameters &params)
{
    // This function is used to copy the bit representation of an image to another, and is used to
    // support EXT_copy_image when a format is emulated.  Currently, only RGB->RGBA emulation is
    // possible, and so this function is tailored to this specific kind of emulation.
    //
    // The copy can be done with various degrees of efficiency:
    //
    // - If the UINT reinterpretation format for src supports SAMPLED usage, texels can be read
    //   directly from that.  Otherwise vkCmdCopyImageToBuffer can be used and data then read from
    //   the buffer.
    // - If the UINT reinterpretation format for dst supports STORAGE usage, texels can be written
    //   directly to that.  Otherwise conversion can be done to a buffer and then
    //   vkCmdCopyBufferToImage used.
    //
    // This requires four different shaders.  For simplicity, this function unconditionally copies
    // src to a temp buffer, transforms to another temp buffer and copies to the dst.  No known
    // applications use EXT_copy_image on RGB formats, so no further optimization is currently
    // necessary.
    //
    // The conversion between buffers can be done with ConvertVertex.comp in UintToUint mode, so no
    // new shader is necessary.  The srcEmulatedAlpha parameter is used to make sure the destination
    // alpha value is correct, if dst is RGBA.

    // This path should only be necessary for when RGBA is used as fallback for RGB.  No other
    // format which can be used with EXT_copy_image has a fallback.
    ASSERT(src->getIntendedFormat().blueBits > 0 && src->getIntendedFormat().alphaBits == 0);
    ASSERT(dst->getIntendedFormat().blueBits > 0 && dst->getIntendedFormat().alphaBits == 0);

    const angle::Format &srcImageFormat = src->getActualFormat();
    const angle::Format &dstImageFormat = dst->getActualFormat();

    // Create temporary buffers.
    vk::RendererScoped<vk::BufferHelper> srcBuffer(contextVk->getRenderer());
    vk::RendererScoped<vk::BufferHelper> dstBuffer(contextVk->getRenderer());

    const uint32_t srcPixelBytes = srcImageFormat.pixelBytes;
    const uint32_t dstPixelBytes = dstImageFormat.pixelBytes;

    const uint32_t totalPixelCount =
        params.copyExtents[0] * params.copyExtents[1] * params.copyExtents[2];
    // Note that buffer sizes are rounded up a multiple of uint size, as that the granularity in
    // which the compute shader accesses these buffers.
    const VkDeviceSize srcBufferSize =
        roundUpPow2<uint32_t>(srcPixelBytes * totalPixelCount, sizeof(uint32_t));
    const VkDeviceSize dstBufferSize =
        roundUpPow2<uint32_t>(dstPixelBytes * totalPixelCount, sizeof(uint32_t));

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.flags              = 0;
    bufferInfo.size               = srcBufferSize;
    bufferInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferInfo.queueFamilyIndexCount = 0;
    bufferInfo.pQueueFamilyIndices   = nullptr;

    ANGLE_TRY(srcBuffer.get().init(contextVk, bufferInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    bufferInfo.size  = dstBufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    ANGLE_TRY(dstBuffer.get().init(contextVk, bufferInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    bool isSrc3D = src->getType() == VK_IMAGE_TYPE_3D;
    bool isDst3D = dst->getType() == VK_IMAGE_TYPE_3D;

    // Change layouts prior to computation.
    vk::CommandBufferAccess access;
    access.onImageTransferRead(src->getAspectFlags(), src);
    access.onImageTransferWrite(params.dstLevel, 1, isDst3D ? 0 : params.dstOffset[2],
                                isDst3D ? 1 : params.copyExtents[2], VK_IMAGE_ASPECT_COLOR_BIT,
                                dst);

    // srcBuffer is the destination of copyImageToBuffer() below.
    access.onBufferTransferWrite(&srcBuffer.get());
    access.onBufferComputeShaderWrite(&dstBuffer.get());

    vk::OutsideRenderPassCommandBufferHelper *commandBufferHelper;
    vk::OutsideRenderPassCommandBuffer *commandBuffer;
    ANGLE_TRY(contextVk->getOutsideRenderPassCommandBufferHelper(access, &commandBufferHelper));
    commandBuffer = &commandBufferHelper->getCommandBuffer();

    // Copy src into buffer, completely packed.
    VkBufferImageCopy srcRegion               = {};
    srcRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    srcRegion.imageSubresource.mipLevel       = src->toVkLevel(params.srcLevel).get();
    srcRegion.imageSubresource.baseArrayLayer = isSrc3D ? 0 : params.srcOffset[2];
    srcRegion.imageSubresource.layerCount     = isSrc3D ? 1 : params.copyExtents[2];
    srcRegion.imageOffset.x                   = params.srcOffset[0];
    srcRegion.imageOffset.y                   = params.srcOffset[1];
    srcRegion.imageOffset.z                   = isSrc3D ? params.srcOffset[2] : 0;
    srcRegion.imageExtent.width               = params.copyExtents[0];
    srcRegion.imageExtent.height              = params.copyExtents[1];
    srcRegion.imageExtent.depth               = isSrc3D ? params.copyExtents[2] : 1;

    commandBuffer->copyImageToBuffer(src->getImage(), src->getCurrentLayout(),
                                     srcBuffer.get().getBuffer().getHandle(), 1, &srcRegion);

    // Add a barrier prior to dispatch call.
    VkMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType           = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask   = VK_ACCESS_TRANSFER_WRITE_BIT;
    memoryBarrier.dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;

    commandBuffer->memoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, &memoryBarrier);

    // Set up ConvertVertex shader to convert between the formats.  Only the following three cases
    // are possible:
    //
    // - RGB -> RGBA: Ns = 3, Ss = src.pixelBytes,
    //                Nd = 4, Sd = dst.pixelBytes, use srcEmulatedAlpha
    //
    // - RGBA -> RGBA: Ns = 3, Ss = src.pixelBytes,
    //                 Nd = 4, Sd = dst.pixelBytes, use srcEmulatedAlpha
    //
    // - RGBA -> RGB:  Ns = 3, Ss = src.pixelBytes,
    //                 Nd = 3, Sd = dst.pixelBytes
    //
    // The trick here is with RGBA -> RGBA, where Ns is specified as 3, so that the emulated alpha
    // from source is not taken (as uint), but rather one is provided such that the destination
    // alpha would contain the correct emulated alpha.
    //
    ConvertVertexShaderParams shaderParams;
    shaderParams.Ns = 3;
    shaderParams.Bs = srcImageFormat.pixelBytes / srcImageFormat.channelCount;
    shaderParams.Ss = srcImageFormat.pixelBytes;
    shaderParams.Nd = dstImageFormat.channelCount;
    shaderParams.Bd = dstImageFormat.pixelBytes / dstImageFormat.channelCount;
    shaderParams.Sd = shaderParams.Nd * shaderParams.Bd;
    // The component size is expected to either be 1, 2 or 4 bytes.
    ASSERT(4 % shaderParams.Bs == 0);
    ASSERT(4 % shaderParams.Bd == 0);
    shaderParams.Es = 4 / shaderParams.Bs;
    shaderParams.Ed = 4 / shaderParams.Bd;
    // Total number of output components is simply the number of pixels by number of components in
    // each.
    shaderParams.componentCount = totalPixelCount * shaderParams.Nd;
    // Total number of 4-byte outputs is the number of components divided by how many components can
    // fit in a 4-byte value.  Note that this value is also the invocation size of the shader.
    shaderParams.outputCount  = UnsignedCeilDivide(shaderParams.componentCount, shaderParams.Ed);
    shaderParams.srcOffset    = 0;
    shaderParams.dstOffset    = 0;
    shaderParams.isSrcHDR     = 0;
    shaderParams.isSrcA2BGR10 = 0;

    // Due to the requirements of EXT_copy_image, the channel size of src and dst must be
    // identical.  Usage of srcEmulatedAlpha relies on this as it's used to output an alpha value in
    // dst through the source.
    ASSERT(shaderParams.Bs == shaderParams.Bd);

    // The following RGB formats are allowed in EXT_copy_image:
    //
    // - RGB32F, RGB32UI, RGB32I
    // - RGB16F, RGB16UI, RGB16I
    // - RGB8, RGB8_SNORM, SRGB8, RGB8UI, RGB8I
    //
    // The value of emulated alpha is:
    //
    // - 1 for all RGB*I and RGB*UI formats
    // - bit representation of 1.0f for RGB32F
    // - bit representation of half-float 1.0f for RGB16F
    // - 0xFF for RGB8 and SRGB8
    // - 0x7F for RGB8_SNORM
    if (dstImageFormat.isInt())
    {
        shaderParams.srcEmulatedAlpha = 1;
    }
    else if (dstImageFormat.isUnorm())
    {
        ASSERT(shaderParams.Bd == 1);
        shaderParams.srcEmulatedAlpha = 0xFF;
    }
    else if (dstImageFormat.isSnorm())
    {
        ASSERT(shaderParams.Bd == 1);
        shaderParams.srcEmulatedAlpha = 0x7F;
    }
    else if (shaderParams.Bd == 2)
    {
        ASSERT(dstImageFormat.isFloat());
        shaderParams.srcEmulatedAlpha = gl::Float16One;
    }
    else if (shaderParams.Bd == 4)
    {
        ASSERT(dstImageFormat.isFloat());
        ASSERT(ValidateFloatOneAsUint());
        shaderParams.srcEmulatedAlpha = gl::Float32One;
    }
    else
    {
        UNREACHABLE();
    }

    // Use UintToUint conversion to preserve the bit pattern during transfer.
    const uint32_t flags = ConvertVertex_comp::kUintToUint;

    ANGLE_TRY(convertVertexBufferImpl(contextVk, &dstBuffer.get(), &srcBuffer.get(), flags,
                                      commandBufferHelper, shaderParams));

    // Add a barrier prior to copy.
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    commandBuffer->memoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, &memoryBarrier);

    // Copy buffer into dst.  It's completely packed.
    VkBufferImageCopy dstRegion               = {};
    dstRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    dstRegion.imageSubresource.mipLevel       = dst->toVkLevel(params.dstLevel).get();
    dstRegion.imageSubresource.baseArrayLayer = isDst3D ? 0 : params.dstOffset[2];
    dstRegion.imageSubresource.layerCount     = isDst3D ? 1 : params.copyExtents[2];
    dstRegion.imageOffset.x                   = params.dstOffset[0];
    dstRegion.imageOffset.y                   = params.dstOffset[1];
    dstRegion.imageOffset.z                   = isDst3D ? params.dstOffset[2] : 0;
    dstRegion.imageExtent.width               = params.copyExtents[0];
    dstRegion.imageExtent.height              = params.copyExtents[1];
    dstRegion.imageExtent.depth               = isDst3D ? params.copyExtents[2] : 1;

    commandBuffer->copyBufferToImage(dstBuffer.get().getBuffer().getHandle(), dst->getImage(),
                                     dst->getCurrentLayout(), 1, &dstRegion);

    return angle::Result::Continue;
}

angle::Result UtilsVk::generateMipmap(ContextVk *contextVk,
                                      vk::ImageHelper *src,
                                      const vk::ImageView *srcLevelZeroView,
                                      vk::ImageHelper *dst,
                                      const GenerateMipmapDestLevelViews &destLevelViews,
                                      const vk::Sampler &sampler,
                                      const GenerateMipmapParameters &params)
{
    ANGLE_TRY(ensureGenerateMipmapResourcesInitialized(contextVk));

    const gl::Extents &srcExtents = src->getLevelExtents(vk::LevelIndex(params.srcLevel));
    ASSERT(srcExtents.depth == 1);

    // Each workgroup processes a 64x64 tile of the image.
    constexpr uint32_t kPixelWorkgroupRatio = 64;
    const uint32_t workGroupX = UnsignedCeilDivide(srcExtents.width, kPixelWorkgroupRatio);
    const uint32_t workGroupY = UnsignedCeilDivide(srcExtents.height, kPixelWorkgroupRatio);

    GenerateMipmapShaderParams shaderParams;
    shaderParams.invSrcExtent[0] = 1.0f / srcExtents.width;
    shaderParams.invSrcExtent[1] = 1.0f / srcExtents.height;
    shaderParams.levelCount      = params.dstLevelCount;

    uint32_t flags = GetGenerateMipmapFlags(contextVk, src->getActualFormat());

    vk::OutsideRenderPassCommandBufferHelper *commandBufferHelper;
    ANGLE_TRY(contextVk->getOutsideRenderPassCommandBufferHelper({}, &commandBufferHelper));

    VkDescriptorSet descriptorSet;
    ANGLE_TRY(allocateDescriptorSet(contextVk, commandBufferHelper, Function::GenerateMipmap,
                                    &descriptorSet));

    VkDescriptorImageInfo destImageInfos[kGenerateMipmapMaxLevels] = {};
    for (uint32_t level = 0; level < kGenerateMipmapMaxLevels; ++level)
    {
        destImageInfos[level].imageView   = destLevelViews[level]->getHandle();
        destImageInfos[level].imageLayout = dst->getCurrentLayout();
    }

    VkDescriptorImageInfo srcImageInfo = {};
    srcImageInfo.imageView             = srcLevelZeroView->getHandle();
    srcImageInfo.imageLayout           = src->getCurrentLayout();
    srcImageInfo.sampler               = sampler.getHandle();

    VkWriteDescriptorSet writeInfos[2] = {};
    writeInfos[0].sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfos[0].dstSet               = descriptorSet;
    writeInfos[0].dstBinding           = kGenerateMipmapDestinationBinding;
    writeInfos[0].descriptorCount      = GetGenerateMipmapMaxLevels(contextVk);
    writeInfos[0].descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writeInfos[0].pImageInfo           = destImageInfos;

    writeInfos[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfos[1].dstSet          = descriptorSet;
    writeInfos[1].dstBinding      = kGenerateMipmapSourceBinding;
    writeInfos[1].descriptorCount = 1;
    writeInfos[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeInfos[1].pImageInfo      = &srcImageInfo;

    vkUpdateDescriptorSets(contextVk->getDevice(), 2, writeInfos, 0, nullptr);

    vk::RefCounted<vk::ShaderAndSerial> *shader = nullptr;
    ANGLE_TRY(contextVk->getShaderLibrary().getGenerateMipmap_comp(contextVk, flags, &shader));

    // Note: onImageRead/onImageWrite is expected to be called by the caller.  This avoids inserting
    // barriers between calls for each layer of the image.
    vk::OutsideRenderPassCommandBuffer *commandBuffer;
    commandBuffer = &commandBufferHelper->getCommandBuffer();

    ANGLE_TRY(setupComputeProgram(contextVk, Function::GenerateMipmap, shader,
                                  &mGenerateMipmapPrograms[flags], descriptorSet, &shaderParams,
                                  sizeof(shaderParams), commandBufferHelper));

    commandBuffer->dispatch(workGroupX, workGroupY, 1);

    return angle::Result::Continue;
}

angle::Result UtilsVk::unresolve(ContextVk *contextVk,
                                 const FramebufferVk *framebuffer,
                                 const UnresolveParameters &params)
{
    // Get attachment count and pointers to resolve images and views.
    gl::DrawBuffersArray<vk::ImageHelper *> colorSrc         = {};
    gl::DrawBuffersArray<const vk::ImageView *> colorSrcView = {};

    vk::DeviceScoped<vk::ImageView> depthView(contextVk->getDevice());
    vk::DeviceScoped<vk::ImageView> stencilView(contextVk->getDevice());

    const vk::ImageView *depthSrcView   = nullptr;
    const vk::ImageView *stencilSrcView = nullptr;

    // The subpass that initializes the multisampled-render-to-texture attachments packs the
    // attachments that need to be unresolved, so the attachment indices of this subpass are not the
    // same.  See InitializeUnresolveSubpass for details.
    vk::PackedAttachmentIndex colorIndexVk(0);
    for (size_t colorIndexGL : params.unresolveColorMask)
    {
        RenderTargetVk *colorRenderTarget = framebuffer->getColorDrawRenderTarget(colorIndexGL);

        ASSERT(colorRenderTarget->hasResolveAttachment());
        ASSERT(colorRenderTarget->isImageTransient());

        colorSrc[colorIndexVk.get()] = &colorRenderTarget->getResolveImageForRenderPass();
        ANGLE_TRY(
            colorRenderTarget->getResolveImageView(contextVk, &colorSrcView[colorIndexVk.get()]));

        ++colorIndexVk;
    }

    if (params.unresolveDepth || params.unresolveStencil)
    {
        RenderTargetVk *depthStencilRenderTarget = framebuffer->getDepthStencilRenderTarget();

        ASSERT(depthStencilRenderTarget->hasResolveAttachment());
        ASSERT(depthStencilRenderTarget->isImageTransient());

        vk::ImageHelper *depthStencilSrc =
            &depthStencilRenderTarget->getResolveImageForRenderPass();

        // The resolved depth/stencil image is necessarily single-sampled.
        ASSERT(depthStencilSrc->getSamples() == 1);
        gl::TextureType textureType = vk::Get2DTextureType(depthStencilSrc->getLayerCount(), 1);

        const vk::LevelIndex levelIndex =
            depthStencilSrc->toVkLevel(depthStencilRenderTarget->getLevelIndex());
        const uint32_t layerIndex = depthStencilRenderTarget->getLayerIndex();

        if (params.unresolveDepth)
        {
            ANGLE_TRY(depthStencilSrc->initLayerImageView(
                contextVk, textureType, VK_IMAGE_ASPECT_DEPTH_BIT, gl::SwizzleState(),
                &depthView.get(), levelIndex, 1, layerIndex, 1, gl::SrgbWriteControlMode::Default,
                gl::YuvSamplingMode::Default));
            depthSrcView = &depthView.get();
        }

        if (params.unresolveStencil)
        {
            ANGLE_TRY(depthStencilSrc->initLayerImageView(
                contextVk, textureType, VK_IMAGE_ASPECT_STENCIL_BIT, gl::SwizzleState(),
                &stencilView.get(), levelIndex, 1, layerIndex, 1, gl::SrgbWriteControlMode::Default,
                gl::YuvSamplingMode::Default));
            stencilSrcView = &stencilView.get();
        }
    }

    const uint32_t colorAttachmentCount = colorIndexVk.get();
    const uint32_t depthStencilBindingCount =
        (params.unresolveDepth ? 1 : 0) + (params.unresolveStencil ? 1 : 0);
    const uint32_t totalBindingCount = colorAttachmentCount + depthStencilBindingCount;

    ASSERT(totalBindingCount > 0);
    const Function function = static_cast<Function>(
        static_cast<uint32_t>(Function::Unresolve1Attachment) + totalBindingCount - 1);

    ANGLE_TRY(ensureUnresolveResourcesInitialized(contextVk, function, totalBindingCount));

    vk::GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.initDefaults(contextVk);
    pipelineDesc.setRasterizationSamples(framebuffer->getSamples());
    pipelineDesc.setRenderPassDesc(framebuffer->getRenderPassDesc());
    if (params.unresolveDepth && !contextVk->getFeatures().supportsExtendedDynamicState.enabled)
    {
        pipelineDesc.setDepthTestEnabled(VK_TRUE);
        pipelineDesc.setDepthWriteEnabled(VK_TRUE);
        pipelineDesc.setDepthFunc(VK_COMPARE_OP_ALWAYS);
    }

    if (params.unresolveStencil && !contextVk->getFeatures().supportsExtendedDynamicState.enabled)
    {
        SetStencilStateForWrite(&pipelineDesc);
    }

    vk::RenderPassCommandBuffer *commandBuffer =
        &contextVk->getStartedRenderPassCommands().getCommandBuffer();

    VkDescriptorSet descriptorSet;
    ANGLE_TRY(allocateDescriptorSet(contextVk, &contextVk->getStartedRenderPassCommands(), function,
                                    &descriptorSet));

    vk::FramebufferAttachmentArray<VkDescriptorImageInfo> inputImageInfo = {};
    for (uint32_t attachmentIndex = 0; attachmentIndex < colorAttachmentCount; ++attachmentIndex)
    {
        inputImageInfo[attachmentIndex].imageView   = colorSrcView[attachmentIndex]->getHandle();
        inputImageInfo[attachmentIndex].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    uint32_t depthStencilBindingIndex = colorAttachmentCount;
    if (params.unresolveDepth)
    {
        inputImageInfo[depthStencilBindingIndex].imageView = depthSrcView->getHandle();
        inputImageInfo[depthStencilBindingIndex].imageLayout =
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        ++depthStencilBindingIndex;
    }
    if (params.unresolveStencil)
    {
        inputImageInfo[depthStencilBindingIndex].imageView = stencilSrcView->getHandle();
        inputImageInfo[depthStencilBindingIndex].imageLayout =
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkWriteDescriptorSet writeInfo = {};
    writeInfo.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfo.dstSet               = descriptorSet;
    writeInfo.dstBinding           = 0;
    writeInfo.descriptorCount      = totalBindingCount;
    writeInfo.descriptorType       = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    writeInfo.pImageInfo           = inputImageInfo.data();

    vkUpdateDescriptorSets(contextVk->getDevice(), 1, &writeInfo, 0, nullptr);

    gl::DrawBuffersArray<UnresolveColorAttachmentType> colorAttachmentTypes;
    uint32_t flags = GetUnresolveFlags(colorAttachmentCount, colorSrc, params.unresolveDepth,
                                       params.unresolveStencil, &colorAttachmentTypes);

    vk::ShaderLibrary &shaderLibrary                    = contextVk->getShaderLibrary();
    vk::RefCounted<vk::ShaderAndSerial> *vertexShader   = nullptr;
    vk::RefCounted<vk::ShaderAndSerial> *fragmentShader = &mUnresolveFragShaders[flags];
    ANGLE_TRY(shaderLibrary.getFullScreenTri_vert(contextVk, 0, &vertexShader));
    ANGLE_TRY(GetUnresolveFrag(contextVk, colorAttachmentCount, colorAttachmentTypes,
                               params.unresolveDepth, params.unresolveStencil, fragmentShader));

    ANGLE_TRY(setupGraphicsProgram(contextVk, function, vertexShader, fragmentShader,
                                   &mUnresolvePrograms[flags], &pipelineDesc, descriptorSet,
                                   nullptr, 0, commandBuffer));

    // Set dynamic state
    VkViewport viewport;
    gl::Rectangle completeRenderArea = framebuffer->getRotatedCompleteRenderArea(contextVk);
    bool invertViewport              = contextVk->isViewportFlipEnabledForDrawFBO();
    bool clipSpaceOriginUpperLeft =
        contextVk->getState().getClipSpaceOrigin() == gl::ClipSpaceOrigin::UpperLeft;
    gl_vk::GetViewport(completeRenderArea, 0.0f, 1.0f, invertViewport, clipSpaceOriginUpperLeft,
                       completeRenderArea.height, &viewport);
    commandBuffer->setViewport(0, 1, &viewport);

    VkRect2D scissor = gl_vk::GetRect(completeRenderArea);
    commandBuffer->setScissor(0, 1, &scissor);

    if (params.unresolveDepth && contextVk->getFeatures().supportsExtendedDynamicState.enabled)
    {
        commandBuffer->setDepthTestEnable(VK_TRUE);
        commandBuffer->setDepthWriteEnable(VK_TRUE);
        commandBuffer->setDepthCompareOp(VK_COMPARE_OP_ALWAYS);
    }

    if (params.unresolveStencil)
    {
        constexpr uint8_t completeMask    = 0xFF;
        constexpr uint8_t unusedReference = 0x00;

        commandBuffer->setStencilCompareMask(completeMask, completeMask);
        commandBuffer->setStencilWriteMask(completeMask, completeMask);
        commandBuffer->setStencilReference(unusedReference, unusedReference);

        if (contextVk->getFeatures().supportsExtendedDynamicState.enabled)
        {
            SetStencilDynamicStateForWrite(commandBuffer);
        }
    }

    // This draw call is made before ContextVk gets a chance to start the occlusion query.  As such,
    // occlusion queries are not enabled.
    commandBuffer->draw(3, 0);

    // Release temporary views
    vk::ImageView depthViewObject   = depthView.release();
    vk::ImageView stencilViewObject = stencilView.release();

    contextVk->addGarbage(&depthViewObject);
    contextVk->addGarbage(&stencilViewObject);

    return angle::Result::Continue;
}

angle::Result UtilsVk::drawOverlay(ContextVk *contextVk,
                                   vk::BufferHelper *textWidgetsBuffer,
                                   vk::BufferHelper *graphWidgetsBuffer,
                                   vk::ImageHelper *font,
                                   const vk::ImageView *fontView,
                                   vk::ImageHelper *dst,
                                   const vk::ImageView *destView,
                                   const OverlayDrawParameters &params)
{
    ANGLE_TRY(ensureOverlayDrawResourcesInitialized(contextVk));

    OverlayDrawShaderParams shaderParams;
    shaderParams.viewportSize[0] = dst->getExtents().width;
    shaderParams.viewportSize[1] = dst->getExtents().height;
    shaderParams.isText          = false;
    shaderParams.rotateXY        = params.rotateXY;
    if (params.rotateXY)
    {
        std::swap(shaderParams.viewportSize[0], shaderParams.viewportSize[1]);
    }

    ASSERT(dst->getLevelCount() == 1 && dst->getLayerCount() == 1 &&
           dst->getFirstAllocatedLevel() == gl::LevelIndex(0));

    vk::RenderPassDesc renderPassDesc;
    renderPassDesc.setSamples(1);
    renderPassDesc.packColorAttachment(0, dst->getActualFormatID());

    vk::GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.initDefaults(contextVk);
    pipelineDesc.setRenderPassDesc(renderPassDesc);
    pipelineDesc.setTopology(gl::PrimitiveMode::TriangleStrip);
    pipelineDesc.setSingleBlend(0, true, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_SRC_ALPHA,
                                VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);

    gl::Rectangle renderArea;
    renderArea.x      = 0;
    renderArea.y      = 0;
    renderArea.width  = shaderParams.viewportSize[0];
    renderArea.height = shaderParams.viewportSize[1];

    // A potential optimization is to reuse the already open render pass if it belongs to the
    // swapchain.
    vk::RenderPassCommandBuffer *commandBuffer;
    ANGLE_TRY(
        startRenderPass(contextVk, dst, destView, renderPassDesc, renderArea, &commandBuffer));

    vk::RenderPassCommandBufferHelper *commandBufferHelper =
        &contextVk->getStartedRenderPassCommands();

    VkDescriptorSet descriptorSet;
    ANGLE_TRY(allocateDescriptorSet(contextVk, commandBufferHelper, Function::OverlayDraw,
                                    &descriptorSet));

    UpdateColorAccess(contextVk, MakeColorBufferMask(0), MakeColorBufferMask(0));

    commandBufferHelper->retainReadOnlyResource(textWidgetsBuffer);
    commandBufferHelper->retainReadOnlyResource(graphWidgetsBuffer);
    contextVk->onImageRenderPassRead(VK_IMAGE_ASPECT_COLOR_BIT,
                                     vk::ImageLayout::FragmentShaderReadOnly, font);
    contextVk->onImageRenderPassWrite(gl::LevelIndex(0), 0, 1, VK_IMAGE_ASPECT_COLOR_BIT,
                                      vk::ImageLayout::ColorAttachment, dst);

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageView             = fontView->getHandle();
    imageInfo.imageLayout           = font->getCurrentLayout();

    VkDescriptorBufferInfo bufferInfos[2] = {};
    bufferInfos[0].buffer                 = textWidgetsBuffer->getBuffer().getHandle();
    bufferInfos[0].offset                 = textWidgetsBuffer->getOffset();
    bufferInfos[0].range                  = textWidgetsBuffer->getSize();

    bufferInfos[1].buffer = graphWidgetsBuffer->getBuffer().getHandle();
    bufferInfos[1].offset = graphWidgetsBuffer->getOffset();
    bufferInfos[1].range  = graphWidgetsBuffer->getSize();

    VkWriteDescriptorSet writeInfos[3] = {};
    writeInfos[0].sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfos[0].dstSet               = descriptorSet;
    writeInfos[0].dstBinding           = kOverlayDrawTextWidgetsBinding;
    writeInfos[0].descriptorCount      = 1;
    writeInfos[0].descriptorType       = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeInfos[0].pBufferInfo          = &bufferInfos[0];

    writeInfos[1]             = writeInfos[0];
    writeInfos[1].dstBinding  = kOverlayDrawGraphWidgetsBinding;
    writeInfos[1].pBufferInfo = &bufferInfos[1];

    writeInfos[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfos[2].dstSet          = descriptorSet;
    writeInfos[2].dstBinding      = kOverlayDrawFontBinding;
    writeInfos[2].descriptorCount = 1;
    writeInfos[2].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writeInfos[2].pImageInfo      = &imageInfo;

    vkUpdateDescriptorSets(contextVk->getDevice(), 3, writeInfos, 0, nullptr);

    vk::ShaderLibrary &shaderLibrary                    = contextVk->getShaderLibrary();
    vk::RefCounted<vk::ShaderAndSerial> *vertexShader   = nullptr;
    vk::RefCounted<vk::ShaderAndSerial> *fragmentShader = nullptr;
    ANGLE_TRY(shaderLibrary.getOverlayDraw_vert(contextVk, 0, &vertexShader));
    ANGLE_TRY(shaderLibrary.getOverlayDraw_frag(contextVk, 0, &fragmentShader));

    ANGLE_TRY(setupGraphicsProgram(contextVk, Function::OverlayDraw, vertexShader, fragmentShader,
                                   &mOverlayDrawProgram, &pipelineDesc, descriptorSet, nullptr, 0,
                                   commandBuffer));

    // Set dynamic state
    VkViewport viewport;
    gl_vk::GetViewport(renderArea, 0.0f, 1.0f, false, false, dst->getExtents().height, &viewport);
    commandBuffer->setViewport(0, 1, &viewport);

    VkRect2D scissor = gl_vk::GetRect(renderArea);
    commandBuffer->setScissor(0, 1, &scissor);

    // Draw all the graph widgets.
    if (params.graphWidgetCount > 0)
    {
        shaderParams.isText = false;
        commandBuffer->pushConstants(mPipelineLayouts[Function::OverlayDraw].get(),
                                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                     sizeof(shaderParams), &shaderParams);
        commandBuffer->drawInstanced(4, params.graphWidgetCount, 0);
    }
    // Draw all the text widgets.
    if (params.textWidgetCount > 0)
    {
        shaderParams.isText = true;
        commandBuffer->pushConstants(mPipelineLayouts[Function::OverlayDraw].get(),
                                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                     sizeof(shaderParams), &shaderParams);
        commandBuffer->drawInstanced(4, params.textWidgetCount, 0);
    }

    // Overlay is always drawn as the last render pass before present.  Automatically move the
    // layout to PresentSrc.
    contextVk->onColorDraw(gl::LevelIndex(0), 0, 1, dst, nullptr, vk::PackedAttachmentIndex(0));
    contextVk->getStartedRenderPassCommands().setImageOptimizeForPresent(dst);
    contextVk->finalizeImageLayout(dst);

    // Close the render pass for this temporary framebuffer.
    return contextVk->flushCommandsAndEndRenderPass(
        RenderPassClosureReason::TemporaryForOverlayDraw);
}

angle::Result UtilsVk::allocateDescriptorSet(ContextVk *contextVk,
                                             vk::CommandBufferHelperCommon *commandBufferHelper,
                                             Function function,
                                             VkDescriptorSet *descriptorSetOut)
{
    vk::RefCountedDescriptorPoolBinding descriptorPoolBinding;

    ANGLE_TRY(mDescriptorPools[function].allocateDescriptorSet(
        contextVk, mDescriptorSetLayouts[function][DescriptorSetIndex::Internal].get(),
        &descriptorPoolBinding, descriptorSetOut));

    // Add the individual descriptorSet in the resource use list. Because this is a one time use
    // descriptorSet, we immediately put in the garbage list for recycle.
    vk::DescriptorSetHelper descriptorSetHelper(*descriptorSetOut);
    commandBufferHelper->retainResource(&descriptorSetHelper);
    descriptorPoolBinding.get().addGarbage(std::move(descriptorSetHelper));

    // Since the eviction is relying on the pool's mUse, we need to update pool's mUse here.
    commandBufferHelper->retainResource(&descriptorPoolBinding.get());
    descriptorPoolBinding.reset();

    return angle::Result::Continue;
}

UtilsVk::ClearFramebufferParameters::ClearFramebufferParameters()
    : clearColor(false),
      clearDepth(false),
      clearStencil(false),
      stencilMask(0),
      colorMaskFlags(0),
      colorAttachmentIndexGL(0),
      colorFormat(nullptr),
      colorClearValue{},
      depthStencilClearValue{}
{}
}  // namespace rx
