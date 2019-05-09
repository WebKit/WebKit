//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vk_cache_utils.cpp:
//    Contains the classes for the Pipeline State Object cache as well as the RenderPass cache.
//    Also contains the structures for the packed descriptions for the RenderPass and Pipeline.
//

#include "libANGLE/renderer/vulkan/vk_cache_utils.h"

#include "common/aligned_memory.h"
#include "libANGLE/BlobCache.h"
#include "libANGLE/VertexAttribute.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/ProgramVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/vk_format_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

#include <type_traits>

namespace rx
{
namespace vk
{

namespace
{

uint8_t PackGLBlendOp(GLenum blendOp)
{
    switch (blendOp)
    {
        case GL_FUNC_ADD:
            return static_cast<uint8_t>(VK_BLEND_OP_ADD);
        case GL_FUNC_SUBTRACT:
            return static_cast<uint8_t>(VK_BLEND_OP_SUBTRACT);
        case GL_FUNC_REVERSE_SUBTRACT:
            return static_cast<uint8_t>(VK_BLEND_OP_REVERSE_SUBTRACT);
        default:
            UNREACHABLE();
            return 0;
    }
}

uint8_t PackGLBlendFactor(GLenum blendFactor)
{
    switch (blendFactor)
    {
        case GL_ZERO:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_ZERO);
        case GL_ONE:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_ONE);
        case GL_SRC_COLOR:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_SRC_COLOR);
        case GL_DST_COLOR:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_DST_COLOR);
        case GL_ONE_MINUS_SRC_COLOR:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR);
        case GL_SRC_ALPHA:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_SRC_ALPHA);
        case GL_ONE_MINUS_SRC_ALPHA:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
        case GL_DST_ALPHA:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_DST_ALPHA);
        case GL_ONE_MINUS_DST_ALPHA:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA);
        case GL_ONE_MINUS_DST_COLOR:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR);
        case GL_SRC_ALPHA_SATURATE:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_SRC_ALPHA_SATURATE);
        case GL_CONSTANT_COLOR:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_CONSTANT_COLOR);
        case GL_CONSTANT_ALPHA:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_CONSTANT_ALPHA);
        case GL_ONE_MINUS_CONSTANT_COLOR:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR);
        case GL_ONE_MINUS_CONSTANT_ALPHA:
            return static_cast<uint8_t>(VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA);
        default:
            UNREACHABLE();
            return 0;
    }
}

VkStencilOp PackGLStencilOp(GLenum compareOp)
{
    switch (compareOp)
    {
        case GL_KEEP:
            return VK_STENCIL_OP_KEEP;
        case GL_ZERO:
            return VK_STENCIL_OP_ZERO;
        case GL_REPLACE:
            return VK_STENCIL_OP_REPLACE;
        case GL_INCR:
            return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case GL_DECR:
            return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case GL_INCR_WRAP:
            return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case GL_DECR_WRAP:
            return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        case GL_INVERT:
            return VK_STENCIL_OP_INVERT;
        default:
            UNREACHABLE();
            return VK_STENCIL_OP_KEEP;
    }
}

VkCompareOp PackGLCompareFunc(GLenum compareFunc)
{
    switch (compareFunc)
    {
        case GL_NEVER:
            return VK_COMPARE_OP_NEVER;
        case GL_ALWAYS:
            return VK_COMPARE_OP_ALWAYS;
        case GL_LESS:
            return VK_COMPARE_OP_LESS;
        case GL_LEQUAL:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case GL_EQUAL:
            return VK_COMPARE_OP_EQUAL;
        case GL_GREATER:
            return VK_COMPARE_OP_GREATER;
        case GL_GEQUAL:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case GL_NOTEQUAL:
            return VK_COMPARE_OP_NOT_EQUAL;
        default:
            UNREACHABLE();
            return VK_COMPARE_OP_NEVER;
    }
}

void UnpackAttachmentDesc(VkAttachmentDescription *desc,
                          const vk::Format &format,
                          uint8_t samples,
                          const vk::PackedAttachmentOpsDesc &ops)
{
    // We would only need this flag for duplicated attachments. Apply it conservatively.
    desc->flags          = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;
    desc->format         = format.vkImageFormat;
    desc->samples        = gl_vk::GetSamples(samples);
    desc->loadOp         = static_cast<VkAttachmentLoadOp>(ops.loadOp);
    desc->storeOp        = static_cast<VkAttachmentStoreOp>(ops.storeOp);
    desc->stencilLoadOp  = static_cast<VkAttachmentLoadOp>(ops.stencilLoadOp);
    desc->stencilStoreOp = static_cast<VkAttachmentStoreOp>(ops.stencilStoreOp);
    desc->initialLayout  = static_cast<VkImageLayout>(ops.initialLayout);
    desc->finalLayout    = static_cast<VkImageLayout>(ops.finalLayout);
}

void UnpackStencilState(const vk::PackedStencilOpState &packedState,
                        uint8_t stencilReference,
                        VkStencilOpState *stateOut)
{
    stateOut->failOp      = static_cast<VkStencilOp>(packedState.ops.fail);
    stateOut->passOp      = static_cast<VkStencilOp>(packedState.ops.pass);
    stateOut->depthFailOp = static_cast<VkStencilOp>(packedState.ops.depthFail);
    stateOut->compareOp   = static_cast<VkCompareOp>(packedState.ops.compare);
    stateOut->compareMask = packedState.compareMask;
    stateOut->writeMask   = packedState.writeMask;
    stateOut->reference   = stencilReference;
}

void UnpackBlendAttachmentState(const vk::PackedColorBlendAttachmentState &packedState,
                                VkPipelineColorBlendAttachmentState *stateOut)
{
    stateOut->srcColorBlendFactor = static_cast<VkBlendFactor>(packedState.srcColorBlendFactor);
    stateOut->dstColorBlendFactor = static_cast<VkBlendFactor>(packedState.dstColorBlendFactor);
    stateOut->colorBlendOp        = static_cast<VkBlendOp>(packedState.colorBlendOp);
    stateOut->srcAlphaBlendFactor = static_cast<VkBlendFactor>(packedState.srcAlphaBlendFactor);
    stateOut->dstAlphaBlendFactor = static_cast<VkBlendFactor>(packedState.dstAlphaBlendFactor);
    stateOut->alphaBlendOp        = static_cast<VkBlendOp>(packedState.alphaBlendOp);
}

angle::Result InitializeRenderPassFromDesc(vk::Context *context,
                                           const RenderPassDesc &desc,
                                           const AttachmentOpsArray &ops,
                                           RenderPass *renderPass)
{
    size_t attachmentCount = desc.attachmentCount();

    // Unpack the packed and split representation into the format required by Vulkan.
    gl::DrawBuffersVector<VkAttachmentReference> colorAttachmentRefs;
    VkAttachmentReference depthStencilAttachmentRef = {VK_ATTACHMENT_UNUSED};
    gl::AttachmentArray<VkAttachmentDescription> attachmentDescs;
    for (uint32_t attachmentIndex = 0; attachmentIndex < attachmentCount; ++attachmentIndex)
    {
        angle::FormatID formatID = desc[attachmentIndex];
        ASSERT(formatID != angle::FormatID::NONE);
        const vk::Format &format = context->getRenderer()->getFormat(formatID);

        if (!format.angleFormat().hasDepthOrStencilBits())
        {
            VkAttachmentReference colorRef = colorAttachmentRefs[attachmentIndex];
            colorRef.attachment            = attachmentIndex;
            colorRef.layout                = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            colorAttachmentRefs.push_back(colorRef);
        }
        else
        {
            ASSERT(depthStencilAttachmentRef.attachment == VK_ATTACHMENT_UNUSED);
            depthStencilAttachmentRef.attachment = attachmentIndex;
            depthStencilAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        UnpackAttachmentDesc(&attachmentDescs[attachmentIndex], format, desc.samples(),
                             ops[attachmentIndex]);
    }

    VkSubpassDescription subpassDesc = {};

    subpassDesc.flags                = 0;
    subpassDesc.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc.inputAttachmentCount = 0;
    subpassDesc.pInputAttachments    = nullptr;
    subpassDesc.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size());
    subpassDesc.pColorAttachments    = colorAttachmentRefs.data();
    subpassDesc.pResolveAttachments  = nullptr;
    subpassDesc.pDepthStencilAttachment =
        (depthStencilAttachmentRef.attachment != VK_ATTACHMENT_UNUSED ? &depthStencilAttachmentRef
                                                                      : nullptr);
    subpassDesc.preserveAttachmentCount = 0;
    subpassDesc.pPreserveAttachments    = nullptr;

    VkRenderPassCreateInfo createInfo = {};
    createInfo.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.flags                  = 0;
    createInfo.attachmentCount        = attachmentCount;
    createInfo.pAttachments           = attachmentDescs.data();
    createInfo.subpassCount           = 1;
    createInfo.pSubpasses             = &subpassDesc;
    createInfo.dependencyCount        = 0;
    createInfo.pDependencies          = nullptr;

    ANGLE_VK_TRY(context, renderPass->init(context->getDevice(), createInfo));
    return angle::Result::Continue;
}

// Utility for setting a value on a packed 4-bit integer array.
template <typename SrcT>
void Int4Array_Set(uint8_t *arrayBytes, uint32_t arrayIndex, SrcT value)
{
    uint32_t byteIndex = arrayIndex >> 1;
    ASSERT(value < 16);

    if ((arrayIndex & 1) == 0)
    {
        arrayBytes[byteIndex] &= 0xF0;
        arrayBytes[byteIndex] |= static_cast<uint8_t>(value);
    }
    else
    {
        arrayBytes[byteIndex] &= 0x0F;
        arrayBytes[byteIndex] |= static_cast<uint8_t>(value) << 4;
    }
}

// Utility for getting a value from a packed 4-bit integer array.
template <typename DestT>
DestT Int4Array_Get(const uint8_t *arrayBytes, uint32_t arrayIndex)
{
    uint32_t byteIndex = arrayIndex >> 1;

    if ((arrayIndex & 1) == 0)
    {
        return static_cast<DestT>(arrayBytes[byteIndex] & 0xF);
    }
    else
    {
        return static_cast<DestT>(arrayBytes[byteIndex] >> 4);
    }
}

// Helper macro that casts to a bitfield type then verifies no bits were dropped.
#define SetBitField(lhs, rhs)                                         \
    lhs = static_cast<typename std::decay<decltype(lhs)>::type>(rhs); \
    ASSERT(static_cast<decltype(rhs)>(lhs) == (rhs))

// When converting a byte number to a transition bit index we can shift instead of divide.
constexpr size_t kTransitionByteShift = Log2(kGraphicsPipelineDirtyBitBytes);

// When converting a number of bits offset to a transition bit index we can also shift.
constexpr size_t kBitsPerByte        = 8;
constexpr size_t kTransitionBitShift = kTransitionByteShift + Log2(kBitsPerByte);

// Helper macro to map from a PipelineDesc struct and field to a dirty bit index.
// Uses the 'offsetof' macro to compute the offset 'Member' within the PipelineDesc
// and the offset of 'Field' within 'Member'. We can optimize the dirty bit setting by computing
// the shifted dirty bit at compile time instead of calling "set".
#define ANGLE_GET_TRANSITION_BIT(Member, Field)                                      \
    ((offsetof(GraphicsPipelineDesc, Member) + offsetof(decltype(Member), Field)) >> \
     kTransitionByteShift)

// Indexed dirty bits cannot be entirely computed at compile time since the index is passed to
// the update function.
#define ANGLE_GET_INDEXED_TRANSITION_BIT(Member, Field, Index, BitWidth) \
    (((BitWidth * Index) >> kTransitionBitShift) + ANGLE_GET_TRANSITION_BIT(Member, Field))
}  // anonymous namespace

// RenderPassDesc implementation.
RenderPassDesc::RenderPassDesc()
{
    memset(this, 0, sizeof(RenderPassDesc));
}

RenderPassDesc::~RenderPassDesc() = default;

RenderPassDesc::RenderPassDesc(const RenderPassDesc &other)
{
    memcpy(this, &other, sizeof(RenderPassDesc));
}

size_t RenderPassDesc::colorAttachmentCount() const
{
    return mColorAttachmentCount;
}

size_t RenderPassDesc::attachmentCount() const
{
    return mColorAttachmentCount + mDepthStencilAttachmentCount;
}

void RenderPassDesc::setSamples(GLint samples)
{
    ASSERT(samples < std::numeric_limits<uint8_t>::max());
    mSamples = static_cast<uint8_t>(samples);
}

void RenderPassDesc::packAttachment(const Format &format)
{
    ASSERT(attachmentCount() < mAttachmentFormats.size() - 1);
    static_assert(angle::kNumANGLEFormats < std::numeric_limits<uint8_t>::max(),
                  "Too many ANGLE formats to fit in uint8_t");
    uint8_t &packedFormat = mAttachmentFormats[attachmentCount()];
    SetBitField(packedFormat, format.angleFormatID);
    if (format.angleFormat().hasDepthOrStencilBits())
    {
        mDepthStencilAttachmentCount++;
    }
    else
    {
        // Force the user to pack the depth/stencil attachment last.
        ASSERT(mDepthStencilAttachmentCount == 0);
        mColorAttachmentCount++;
    }
}

RenderPassDesc &RenderPassDesc::operator=(const RenderPassDesc &other)
{
    memcpy(this, &other, sizeof(RenderPassDesc));
    return *this;
}

size_t RenderPassDesc::hash() const
{
    return angle::ComputeGenericHash(*this);
}

bool operator==(const RenderPassDesc &lhs, const RenderPassDesc &rhs)
{
    return (memcmp(&lhs, &rhs, sizeof(RenderPassDesc)) == 0);
}

// GraphicsPipelineDesc implementation.
// Use aligned allocation and free so we can use the alignas keyword.
void *GraphicsPipelineDesc::operator new(std::size_t size)
{
    return angle::AlignedAlloc(size, 32);
}

void GraphicsPipelineDesc::operator delete(void *ptr)
{
    return angle::AlignedFree(ptr);
}

GraphicsPipelineDesc::GraphicsPipelineDesc()
{
    memset(this, 0, sizeof(GraphicsPipelineDesc));
}

GraphicsPipelineDesc::~GraphicsPipelineDesc() = default;

GraphicsPipelineDesc::GraphicsPipelineDesc(const GraphicsPipelineDesc &other)
{
    memcpy(this, &other, sizeof(GraphicsPipelineDesc));
}

GraphicsPipelineDesc &GraphicsPipelineDesc::operator=(const GraphicsPipelineDesc &other)
{
    memcpy(this, &other, sizeof(GraphicsPipelineDesc));
    return *this;
}

size_t GraphicsPipelineDesc::hash() const
{
    return angle::ComputeGenericHash(*this);
}

bool GraphicsPipelineDesc::operator==(const GraphicsPipelineDesc &other) const
{
    return (memcmp(this, &other, sizeof(GraphicsPipelineDesc)) == 0);
}

// TODO(jmadill): We should prefer using Packed GLenums. http://anglebug.com/2169

void GraphicsPipelineDesc::initDefaults()
{
    mRasterizationAndMultisampleStateInfo.bits.depthClampEnable           = 0;
    mRasterizationAndMultisampleStateInfo.bits.rasterizationDiscardEnable = 0;
    SetBitField(mRasterizationAndMultisampleStateInfo.bits.polygonMode, VK_POLYGON_MODE_FILL);
    SetBitField(mRasterizationAndMultisampleStateInfo.bits.cullMode, VK_CULL_MODE_NONE);
    SetBitField(mRasterizationAndMultisampleStateInfo.bits.frontFace, VK_FRONT_FACE_CLOCKWISE);
    mRasterizationAndMultisampleStateInfo.bits.depthBiasEnable    = 0;
    mRasterizationAndMultisampleStateInfo.depthBiasConstantFactor = 0.0f;
    mRasterizationAndMultisampleStateInfo.depthBiasClamp          = 0.0f;
    mRasterizationAndMultisampleStateInfo.depthBiasSlopeFactor    = 0.0f;
    mRasterizationAndMultisampleStateInfo.lineWidth               = 1.0f;

    mRasterizationAndMultisampleStateInfo.bits.rasterizationSamples = 1;
    mRasterizationAndMultisampleStateInfo.bits.sampleShadingEnable  = 0;
    mRasterizationAndMultisampleStateInfo.minSampleShading          = 0.0f;
    for (uint32_t &sampleMask : mRasterizationAndMultisampleStateInfo.sampleMask)
    {
        sampleMask = 0;
    }
    mRasterizationAndMultisampleStateInfo.bits.alphaToCoverageEnable = 0;
    mRasterizationAndMultisampleStateInfo.bits.alphaToOneEnable      = 0;

    mDepthStencilStateInfo.enable.depthTest  = 0;
    mDepthStencilStateInfo.enable.depthWrite = 1;
    SetBitField(mDepthStencilStateInfo.depthCompareOp, VK_COMPARE_OP_LESS);
    mDepthStencilStateInfo.enable.depthBoundsTest = 0;
    mDepthStencilStateInfo.enable.stencilTest     = 0;
    mDepthStencilStateInfo.minDepthBounds         = 0.0f;
    mDepthStencilStateInfo.maxDepthBounds         = 0.0f;
    SetBitField(mDepthStencilStateInfo.front.ops.fail, VK_STENCIL_OP_KEEP);
    SetBitField(mDepthStencilStateInfo.front.ops.pass, VK_STENCIL_OP_KEEP);
    SetBitField(mDepthStencilStateInfo.front.ops.depthFail, VK_STENCIL_OP_KEEP);
    SetBitField(mDepthStencilStateInfo.front.ops.compare, VK_COMPARE_OP_ALWAYS);
    SetBitField(mDepthStencilStateInfo.front.compareMask, 0xFF);
    SetBitField(mDepthStencilStateInfo.front.writeMask, 0xFF);
    mDepthStencilStateInfo.frontStencilReference = 0;
    SetBitField(mDepthStencilStateInfo.back.ops.fail, VK_STENCIL_OP_KEEP);
    SetBitField(mDepthStencilStateInfo.back.ops.pass, VK_STENCIL_OP_KEEP);
    SetBitField(mDepthStencilStateInfo.back.ops.depthFail, VK_STENCIL_OP_KEEP);
    SetBitField(mDepthStencilStateInfo.back.ops.compare, VK_COMPARE_OP_ALWAYS);
    SetBitField(mDepthStencilStateInfo.back.compareMask, 0xFF);
    SetBitField(mDepthStencilStateInfo.back.writeMask, 0xFF);
    mDepthStencilStateInfo.backStencilReference = 0;

    PackedInputAssemblyAndColorBlendStateInfo &inputAndBlend = mInputAssemblyAndColorBlendStateInfo;
    inputAndBlend.logic.opEnable                             = 0;
    inputAndBlend.logic.op          = static_cast<uint32_t>(VK_LOGIC_OP_CLEAR);
    inputAndBlend.blendEnableMask   = 0;
    inputAndBlend.blendConstants[0] = 0.0f;
    inputAndBlend.blendConstants[1] = 0.0f;
    inputAndBlend.blendConstants[2] = 0.0f;
    inputAndBlend.blendConstants[3] = 0.0f;

    VkFlags allColorBits = (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

    for (uint32_t colorIndex = 0; colorIndex < gl::IMPLEMENTATION_MAX_DRAW_BUFFERS; ++colorIndex)
    {
        Int4Array_Set(inputAndBlend.colorWriteMaskBits, colorIndex, allColorBits);
    }

    PackedColorBlendAttachmentState blendAttachmentState;
    SetBitField(blendAttachmentState.srcColorBlendFactor, VK_BLEND_FACTOR_ONE);
    SetBitField(blendAttachmentState.dstColorBlendFactor, VK_BLEND_FACTOR_ONE);
    SetBitField(blendAttachmentState.colorBlendOp, VK_BLEND_OP_ADD);
    SetBitField(blendAttachmentState.srcAlphaBlendFactor, VK_BLEND_FACTOR_ONE);
    SetBitField(blendAttachmentState.dstAlphaBlendFactor, VK_BLEND_FACTOR_ONE);
    SetBitField(blendAttachmentState.alphaBlendOp, VK_BLEND_OP_ADD);

    std::fill(&inputAndBlend.attachments[0],
              &inputAndBlend.attachments[gl::IMPLEMENTATION_MAX_DRAW_BUFFERS],
              blendAttachmentState);

    inputAndBlend.primitive.topology = static_cast<uint16_t>(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    inputAndBlend.primitive.restartEnable = 0;
}

angle::Result GraphicsPipelineDesc::initializePipeline(
    vk::Context *context,
    const vk::PipelineCache &pipelineCacheVk,
    const RenderPass &compatibleRenderPass,
    const PipelineLayout &pipelineLayout,
    const gl::AttributesMask &activeAttribLocationsMask,
    const ShaderModule *vertexModule,
    const ShaderModule *fragmentModule,
    Pipeline *pipelineOut) const
{
    angle::FixedVector<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    VkPipelineVertexInputStateCreateInfo vertexInputState     = {};
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
    VkPipelineViewportStateCreateInfo viewportState           = {};
    VkPipelineRasterizationStateCreateInfo rasterState        = {};
    VkPipelineMultisampleStateCreateInfo multisampleState     = {};
    VkPipelineDepthStencilStateCreateInfo depthStencilState   = {};
    std::array<VkPipelineColorBlendAttachmentState, gl::IMPLEMENTATION_MAX_DRAW_BUFFERS>
        blendAttachmentState;
    VkPipelineColorBlendStateCreateInfo blendState = {};
    VkGraphicsPipelineCreateInfo createInfo        = {};

    // Vertex shader is always expected to be present.
    ASSERT(vertexModule != nullptr);
    VkPipelineShaderStageCreateInfo vertexStage = {};
    vertexStage.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexStage.flags               = 0;
    vertexStage.stage               = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStage.module              = vertexModule->getHandle();
    vertexStage.pName               = "main";
    vertexStage.pSpecializationInfo = nullptr;
    shaderStages.push_back(vertexStage);

    // Fragment shader is optional.
    if (fragmentModule)
    {
        VkPipelineShaderStageCreateInfo fragmentStage = {};
        fragmentStage.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragmentStage.flags               = 0;
        fragmentStage.stage               = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentStage.module              = fragmentModule->getHandle();
        fragmentStage.pName               = "main";
        fragmentStage.pSpecializationInfo = nullptr;
        shaderStages.push_back(fragmentStage);
    }

    // TODO(jmadill): Possibly use different path for ES 3.1 split bindings/attribs.
    gl::AttribArray<VkVertexInputBindingDescription> bindingDescs;
    gl::AttribArray<VkVertexInputAttributeDescription> attributeDescs;

    uint32_t vertexAttribCount = 0;

    size_t unpackedSize = sizeof(shaderStages) + sizeof(vertexInputState) +
                          sizeof(inputAssemblyState) + sizeof(viewportState) + sizeof(rasterState) +
                          sizeof(multisampleState) + sizeof(depthStencilState) +
                          sizeof(blendAttachmentState) + sizeof(blendState) + sizeof(bindingDescs) +
                          sizeof(attributeDescs);
    ANGLE_UNUSED_VARIABLE(unpackedSize);

    gl::AttribArray<VkVertexInputBindingDivisorDescriptionEXT> divisorDesc;
    VkPipelineVertexInputDivisorStateCreateInfoEXT divisorState = {};
    divisorState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT;
    divisorState.pVertexBindingDivisors = divisorDesc.data();

    for (size_t attribIndexSizeT : activeAttribLocationsMask)
    {
        const uint32_t attribIndex = static_cast<uint32_t>(attribIndexSizeT);

        VkVertexInputBindingDescription &bindingDesc  = bindingDescs[vertexAttribCount];
        VkVertexInputAttributeDescription &attribDesc = attributeDescs[vertexAttribCount];
        const PackedAttribDesc &packedAttrib          = mVertexInputAttribs.attribs[attribIndex];

        bindingDesc.binding = attribIndex;
        bindingDesc.stride  = static_cast<uint32_t>(packedAttrib.stride);
        if (packedAttrib.divisor != 0)
        {
            bindingDesc.inputRate = static_cast<VkVertexInputRate>(VK_VERTEX_INPUT_RATE_INSTANCE);
            divisorDesc[divisorState.vertexBindingDivisorCount].binding = bindingDesc.binding;
            divisorDesc[divisorState.vertexBindingDivisorCount].divisor = packedAttrib.divisor;
            ++divisorState.vertexBindingDivisorCount;
        }
        else
        {
            bindingDesc.inputRate = static_cast<VkVertexInputRate>(VK_VERTEX_INPUT_RATE_VERTEX);
        }

        // The binding index could become more dynamic in ES 3.1.
        attribDesc.binding  = attribIndex;
        attribDesc.format   = static_cast<VkFormat>(packedAttrib.format);
        attribDesc.location = static_cast<uint32_t>(attribIndex);
        attribDesc.offset   = packedAttrib.offset;

        vertexAttribCount++;
    }

    // The binding descriptions are filled in at draw time.
    vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputState.flags = 0;
    vertexInputState.vertexBindingDescriptionCount   = vertexAttribCount;
    vertexInputState.pVertexBindingDescriptions      = bindingDescs.data();
    vertexInputState.vertexAttributeDescriptionCount = vertexAttribCount;
    vertexInputState.pVertexAttributeDescriptions    = attributeDescs.data();
    if (divisorState.vertexBindingDivisorCount)
        vertexInputState.pNext = &divisorState;

    // Primitive topology is filled in at draw time.
    inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState.flags = 0;
    inputAssemblyState.topology =
        static_cast<VkPrimitiveTopology>(mInputAssemblyAndColorBlendStateInfo.primitive.topology);
    inputAssemblyState.primitiveRestartEnable =
        static_cast<VkBool32>(mInputAssemblyAndColorBlendStateInfo.primitive.restartEnable);

    // Set initial viewport and scissor state.

    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.flags         = 0;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &mViewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &mScissor;

    const PackedRasterizationAndMultisampleStateInfo &rasterAndMS =
        mRasterizationAndMultisampleStateInfo;

    // Rasterizer state.
    rasterState.sType            = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterState.flags            = 0;
    rasterState.depthClampEnable = static_cast<VkBool32>(rasterAndMS.bits.depthClampEnable);
    rasterState.rasterizerDiscardEnable =
        static_cast<VkBool32>(rasterAndMS.bits.rasterizationDiscardEnable);
    rasterState.polygonMode             = static_cast<VkPolygonMode>(rasterAndMS.bits.polygonMode);
    rasterState.cullMode                = static_cast<VkCullModeFlags>(rasterAndMS.bits.cullMode);
    rasterState.frontFace               = static_cast<VkFrontFace>(rasterAndMS.bits.frontFace);
    rasterState.depthBiasEnable         = static_cast<VkBool32>(rasterAndMS.bits.depthBiasEnable);
    rasterState.depthBiasConstantFactor = rasterAndMS.depthBiasConstantFactor;
    rasterState.depthBiasClamp          = rasterAndMS.depthBiasClamp;
    rasterState.depthBiasSlopeFactor    = rasterAndMS.depthBiasSlopeFactor;
    rasterState.lineWidth               = rasterAndMS.lineWidth;

    // Multisample state.
    multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.flags = 0;
    multisampleState.rasterizationSamples =
        gl_vk::GetSamples(rasterAndMS.bits.rasterizationSamples);
    multisampleState.sampleShadingEnable =
        static_cast<VkBool32>(rasterAndMS.bits.sampleShadingEnable);
    multisampleState.minSampleShading = rasterAndMS.minSampleShading;
    // TODO(jmadill): sample masks
    multisampleState.pSampleMask = nullptr;
    multisampleState.alphaToCoverageEnable =
        static_cast<VkBool32>(rasterAndMS.bits.alphaToCoverageEnable);
    multisampleState.alphaToOneEnable = static_cast<VkBool32>(rasterAndMS.bits.alphaToOneEnable);

    // Depth/stencil state.
    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.flags = 0;
    depthStencilState.depthTestEnable =
        static_cast<VkBool32>(mDepthStencilStateInfo.enable.depthTest);
    depthStencilState.depthWriteEnable =
        static_cast<VkBool32>(mDepthStencilStateInfo.enable.depthWrite);
    depthStencilState.depthCompareOp =
        static_cast<VkCompareOp>(mDepthStencilStateInfo.depthCompareOp);
    depthStencilState.depthBoundsTestEnable =
        static_cast<VkBool32>(mDepthStencilStateInfo.enable.depthBoundsTest);
    depthStencilState.stencilTestEnable =
        static_cast<VkBool32>(mDepthStencilStateInfo.enable.stencilTest);
    UnpackStencilState(mDepthStencilStateInfo.front, mDepthStencilStateInfo.frontStencilReference,
                       &depthStencilState.front);
    UnpackStencilState(mDepthStencilStateInfo.back, mDepthStencilStateInfo.backStencilReference,
                       &depthStencilState.back);
    depthStencilState.minDepthBounds = mDepthStencilStateInfo.minDepthBounds;
    depthStencilState.maxDepthBounds = mDepthStencilStateInfo.maxDepthBounds;

    const PackedInputAssemblyAndColorBlendStateInfo &inputAndBlend =
        mInputAssemblyAndColorBlendStateInfo;

    blendState.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blendState.flags           = 0;
    blendState.logicOpEnable   = static_cast<VkBool32>(inputAndBlend.logic.opEnable);
    blendState.logicOp         = static_cast<VkLogicOp>(inputAndBlend.logic.op);
    blendState.attachmentCount = mRenderPassDesc.colorAttachmentCount();
    blendState.pAttachments    = blendAttachmentState.data();

    for (int i = 0; i < 4; i++)
    {
        blendState.blendConstants[i] = inputAndBlend.blendConstants[i];
    }

    const gl::DrawBufferMask blendEnableMask(inputAndBlend.blendEnableMask);

    for (uint32_t colorIndex = 0; colorIndex < blendState.attachmentCount; ++colorIndex)
    {
        VkPipelineColorBlendAttachmentState &state = blendAttachmentState[colorIndex];

        state.blendEnable = blendEnableMask[colorIndex] ? VK_TRUE : VK_FALSE;
        state.colorWriteMask =
            Int4Array_Get<VkColorComponentFlags>(inputAndBlend.colorWriteMaskBits, colorIndex);
        UnpackBlendAttachmentState(inputAndBlend.attachments[colorIndex], &state);
    }

    // We would define dynamic state here if it were to be used.

    createInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.flags               = 0;
    createInfo.stageCount          = shaderStages.size();
    createInfo.pStages             = shaderStages.data();
    createInfo.pVertexInputState   = &vertexInputState;
    createInfo.pInputAssemblyState = &inputAssemblyState;
    createInfo.pTessellationState  = nullptr;
    createInfo.pViewportState      = &viewportState;
    createInfo.pRasterizationState = &rasterState;
    createInfo.pMultisampleState   = &multisampleState;
    createInfo.pDepthStencilState  = &depthStencilState;
    createInfo.pColorBlendState    = &blendState;
    createInfo.pDynamicState       = nullptr;
    createInfo.layout              = pipelineLayout.getHandle();
    createInfo.renderPass          = compatibleRenderPass.getHandle();
    createInfo.subpass             = 0;
    createInfo.basePipelineHandle  = VK_NULL_HANDLE;
    createInfo.basePipelineIndex   = 0;

    ANGLE_VK_TRY(context,
                 pipelineOut->initGraphics(context->getDevice(), createInfo, pipelineCacheVk));
    return angle::Result::Continue;
}

void GraphicsPipelineDesc::updateVertexInput(GraphicsPipelineTransitionBits *transition,
                                             uint32_t attribIndex,
                                             GLuint stride,
                                             GLuint divisor,
                                             VkFormat format,
                                             GLuint relativeOffset)
{
    vk::PackedAttribDesc &packedAttrib = mVertexInputAttribs.attribs[attribIndex];

    // TODO: Handle the case where the divisor overflows the field that holds it.
    // http://anglebug.com/2672
    ASSERT(divisor <= std::numeric_limits<decltype(packedAttrib.divisor)>::max());

    SetBitField(packedAttrib.stride, stride);
    SetBitField(packedAttrib.divisor, divisor);

    if (format == VK_FORMAT_UNDEFINED)
    {
        UNIMPLEMENTED();
    }

    SetBitField(packedAttrib.format, format);
    SetBitField(packedAttrib.offset, relativeOffset);

    constexpr size_t kAttribBits = kPackedAttribDescSize * kBitsPerByte;
    const size_t kBit =
        ANGLE_GET_INDEXED_TRANSITION_BIT(mVertexInputAttribs, attribs, attribIndex, kAttribBits);

    // Cover the next dirty bit conservatively. Because each attribute is 6 bytes.
    transition->set(kBit);
    transition->set(kBit + 1);
}

void GraphicsPipelineDesc::updateTopology(GraphicsPipelineTransitionBits *transition,
                                          gl::PrimitiveMode drawMode)
{
    VkPrimitiveTopology vkTopology = gl_vk::GetPrimitiveTopology(drawMode);
    SetBitField(mInputAssemblyAndColorBlendStateInfo.primitive.topology, vkTopology);

    transition->set(ANGLE_GET_TRANSITION_BIT(mInputAssemblyAndColorBlendStateInfo, primitive));
}

void GraphicsPipelineDesc::updateCullMode(GraphicsPipelineTransitionBits *transition,
                                          const gl::RasterizerState &rasterState)
{
    mRasterizationAndMultisampleStateInfo.bits.cullMode =
        static_cast<uint16_t>(gl_vk::GetCullMode(rasterState));
    transition->set(ANGLE_GET_TRANSITION_BIT(mRasterizationAndMultisampleStateInfo, bits));
}

void GraphicsPipelineDesc::updateFrontFace(GraphicsPipelineTransitionBits *transition,
                                           const gl::RasterizerState &rasterState,
                                           bool invertFrontFace)
{
    mRasterizationAndMultisampleStateInfo.bits.frontFace =
        static_cast<uint16_t>(gl_vk::GetFrontFace(rasterState.frontFace, invertFrontFace));
    transition->set(ANGLE_GET_TRANSITION_BIT(mRasterizationAndMultisampleStateInfo, bits));
}

void GraphicsPipelineDesc::updateLineWidth(GraphicsPipelineTransitionBits *transition,
                                           float lineWidth)
{
    mRasterizationAndMultisampleStateInfo.lineWidth = lineWidth;
    transition->set(ANGLE_GET_TRANSITION_BIT(mRasterizationAndMultisampleStateInfo, lineWidth));
}

void GraphicsPipelineDesc::updateBlendColor(GraphicsPipelineTransitionBits *transition,
                                            const gl::ColorF &color)
{
    mInputAssemblyAndColorBlendStateInfo.blendConstants[0] = color.red;
    mInputAssemblyAndColorBlendStateInfo.blendConstants[1] = color.green;
    mInputAssemblyAndColorBlendStateInfo.blendConstants[2] = color.blue;
    mInputAssemblyAndColorBlendStateInfo.blendConstants[3] = color.alpha;
    constexpr size_t kSize = sizeof(mInputAssemblyAndColorBlendStateInfo.blendConstants[0]) * 8;

    for (int index = 0; index < 4; ++index)
    {
        const size_t kBit = ANGLE_GET_INDEXED_TRANSITION_BIT(mInputAssemblyAndColorBlendStateInfo,
                                                             blendConstants, index, kSize);
        transition->set(kBit);
    }
}

void GraphicsPipelineDesc::updateBlendEnabled(GraphicsPipelineTransitionBits *transition,
                                              bool isBlendEnabled)
{
    gl::DrawBufferMask blendEnabled;
    if (isBlendEnabled)
        blendEnabled.set();
    mInputAssemblyAndColorBlendStateInfo.blendEnableMask =
        static_cast<uint8_t>(blendEnabled.bits());
    transition->set(
        ANGLE_GET_TRANSITION_BIT(mInputAssemblyAndColorBlendStateInfo, blendEnableMask));
}

void GraphicsPipelineDesc::updateBlendEquations(GraphicsPipelineTransitionBits *transition,
                                                const gl::BlendState &blendState)
{
    constexpr size_t kSize = sizeof(PackedColorBlendAttachmentState) * 8;

    for (size_t attachmentIndex = 0; attachmentIndex < gl::IMPLEMENTATION_MAX_DRAW_BUFFERS;
         ++attachmentIndex)
    {
        PackedColorBlendAttachmentState &blendAttachmentState =
            mInputAssemblyAndColorBlendStateInfo.attachments[attachmentIndex];
        blendAttachmentState.colorBlendOp = PackGLBlendOp(blendState.blendEquationRGB);
        blendAttachmentState.alphaBlendOp = PackGLBlendOp(blendState.blendEquationAlpha);
        transition->set(ANGLE_GET_INDEXED_TRANSITION_BIT(mInputAssemblyAndColorBlendStateInfo,
                                                         attachments, attachmentIndex, kSize));
    }
}

void GraphicsPipelineDesc::updateBlendFuncs(GraphicsPipelineTransitionBits *transition,
                                            const gl::BlendState &blendState)
{
    constexpr size_t kSize = sizeof(PackedColorBlendAttachmentState) * 8;
    for (size_t attachmentIndex = 0; attachmentIndex < gl::IMPLEMENTATION_MAX_DRAW_BUFFERS;
         ++attachmentIndex)
    {
        PackedColorBlendAttachmentState &blendAttachmentState =
            mInputAssemblyAndColorBlendStateInfo.attachments[attachmentIndex];
        blendAttachmentState.srcColorBlendFactor = PackGLBlendFactor(blendState.sourceBlendRGB);
        blendAttachmentState.dstColorBlendFactor = PackGLBlendFactor(blendState.destBlendRGB);
        blendAttachmentState.srcAlphaBlendFactor = PackGLBlendFactor(blendState.sourceBlendAlpha);
        blendAttachmentState.dstAlphaBlendFactor = PackGLBlendFactor(blendState.destBlendAlpha);
        transition->set(ANGLE_GET_INDEXED_TRANSITION_BIT(mInputAssemblyAndColorBlendStateInfo,
                                                         attachments, attachmentIndex, kSize));
    }
}

void GraphicsPipelineDesc::setColorWriteMask(VkColorComponentFlags colorComponentFlags,
                                             const gl::DrawBufferMask &alphaMask)
{
    PackedInputAssemblyAndColorBlendStateInfo &inputAndBlend = mInputAssemblyAndColorBlendStateInfo;
    uint8_t colorMask = static_cast<uint8_t>(colorComponentFlags);

    for (size_t colorIndex = 0; colorIndex < gl::IMPLEMENTATION_MAX_DRAW_BUFFERS; colorIndex++)
    {
        uint8_t mask = alphaMask[colorIndex] ? (colorMask & ~VK_COLOR_COMPONENT_A_BIT) : colorMask;
        Int4Array_Set(inputAndBlend.colorWriteMaskBits, colorIndex, mask);
    }
}

void GraphicsPipelineDesc::setSingleColorWriteMask(uint32_t colorIndex,
                                                   VkColorComponentFlags colorComponentFlags)
{
    PackedInputAssemblyAndColorBlendStateInfo &inputAndBlend = mInputAssemblyAndColorBlendStateInfo;
    uint8_t colorMask = static_cast<uint8_t>(colorComponentFlags);
    Int4Array_Set(inputAndBlend.colorWriteMaskBits, colorIndex, colorMask);
}

void GraphicsPipelineDesc::updateColorWriteMask(GraphicsPipelineTransitionBits *transition,
                                                VkColorComponentFlags colorComponentFlags,
                                                const gl::DrawBufferMask &alphaMask)
{
    setColorWriteMask(colorComponentFlags, alphaMask);

    for (size_t colorIndex = 0; colorIndex < gl::IMPLEMENTATION_MAX_DRAW_BUFFERS; colorIndex++)
    {
        transition->set(ANGLE_GET_INDEXED_TRANSITION_BIT(mInputAssemblyAndColorBlendStateInfo,
                                                         colorWriteMaskBits, colorIndex, 4));
    }
}

void GraphicsPipelineDesc::setDepthTestEnabled(bool enabled)
{
    mDepthStencilStateInfo.enable.depthTest = enabled;
}

void GraphicsPipelineDesc::setDepthWriteEnabled(bool enabled)
{
    mDepthStencilStateInfo.enable.depthWrite = enabled;
}

void GraphicsPipelineDesc::setDepthFunc(VkCompareOp op)
{
    SetBitField(mDepthStencilStateInfo.depthCompareOp, op);
}

void GraphicsPipelineDesc::setStencilTestEnabled(bool enabled)
{
    mDepthStencilStateInfo.enable.stencilTest = enabled;
}

void GraphicsPipelineDesc::setStencilFrontFuncs(uint8_t reference,
                                                VkCompareOp compareOp,
                                                uint8_t compareMask)
{
    mDepthStencilStateInfo.frontStencilReference = reference;
    mDepthStencilStateInfo.front.compareMask     = compareMask;
    SetBitField(mDepthStencilStateInfo.front.ops.compare, compareOp);
}

void GraphicsPipelineDesc::setStencilBackFuncs(uint8_t reference,
                                               VkCompareOp compareOp,
                                               uint8_t compareMask)
{
    mDepthStencilStateInfo.backStencilReference = reference;
    mDepthStencilStateInfo.back.compareMask     = compareMask;
    SetBitField(mDepthStencilStateInfo.back.ops.compare, compareOp);
}

void GraphicsPipelineDesc::setStencilFrontOps(VkStencilOp failOp,
                                              VkStencilOp passOp,
                                              VkStencilOp depthFailOp)
{
    SetBitField(mDepthStencilStateInfo.front.ops.fail, failOp);
    SetBitField(mDepthStencilStateInfo.front.ops.pass, passOp);
    SetBitField(mDepthStencilStateInfo.front.ops.depthFail, depthFailOp);
}

void GraphicsPipelineDesc::setStencilBackOps(VkStencilOp failOp,
                                             VkStencilOp passOp,
                                             VkStencilOp depthFailOp)
{
    SetBitField(mDepthStencilStateInfo.back.ops.fail, failOp);
    SetBitField(mDepthStencilStateInfo.back.ops.pass, passOp);
    SetBitField(mDepthStencilStateInfo.back.ops.depthFail, depthFailOp);
}

void GraphicsPipelineDesc::setStencilFrontWriteMask(uint8_t mask)
{
    mDepthStencilStateInfo.front.writeMask = mask;
}

void GraphicsPipelineDesc::setStencilBackWriteMask(uint8_t mask)
{
    mDepthStencilStateInfo.back.writeMask = mask;
}

void GraphicsPipelineDesc::updateDepthTestEnabled(GraphicsPipelineTransitionBits *transition,
                                                  const gl::DepthStencilState &depthStencilState,
                                                  const gl::Framebuffer *drawFramebuffer)
{
    // Only enable the depth test if the draw framebuffer has a depth buffer.  It's possible that
    // we're emulating a stencil-only buffer with a depth-stencil buffer
    setDepthTestEnabled(depthStencilState.depthTest && drawFramebuffer->hasDepth());
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, enable));
}

void GraphicsPipelineDesc::updateDepthFunc(GraphicsPipelineTransitionBits *transition,
                                           const gl::DepthStencilState &depthStencilState)
{
    setDepthFunc(PackGLCompareFunc(depthStencilState.depthFunc));
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, depthCompareOp));
}

void GraphicsPipelineDesc::updateDepthWriteEnabled(GraphicsPipelineTransitionBits *transition,
                                                   const gl::DepthStencilState &depthStencilState,
                                                   const gl::Framebuffer *drawFramebuffer)
{
    // Don't write to depth buffers that should not exist
    setDepthWriteEnabled(drawFramebuffer->hasDepth() ? depthStencilState.depthMask : false);
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, enable));
}

void GraphicsPipelineDesc::updateStencilTestEnabled(GraphicsPipelineTransitionBits *transition,
                                                    const gl::DepthStencilState &depthStencilState,
                                                    const gl::Framebuffer *drawFramebuffer)
{
    // Only enable the stencil test if the draw framebuffer has a stencil buffer.  It's possible
    // that we're emulating a depth-only buffer with a depth-stencil buffer
    setStencilTestEnabled(depthStencilState.stencilTest && drawFramebuffer->hasStencil());
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, enable));
}

void GraphicsPipelineDesc::updateStencilFrontFuncs(GraphicsPipelineTransitionBits *transition,
                                                   GLint ref,
                                                   const gl::DepthStencilState &depthStencilState)
{
    setStencilFrontFuncs(static_cast<uint8_t>(ref),
                         PackGLCompareFunc(depthStencilState.stencilFunc),
                         static_cast<uint8_t>(depthStencilState.stencilMask));
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, front));
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, frontStencilReference));
}

void GraphicsPipelineDesc::updateStencilBackFuncs(GraphicsPipelineTransitionBits *transition,
                                                  GLint ref,
                                                  const gl::DepthStencilState &depthStencilState)
{
    setStencilBackFuncs(static_cast<uint8_t>(ref),
                        PackGLCompareFunc(depthStencilState.stencilBackFunc),
                        static_cast<uint8_t>(depthStencilState.stencilBackMask));
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, back));
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, backStencilReference));
}

void GraphicsPipelineDesc::updateStencilFrontOps(GraphicsPipelineTransitionBits *transition,
                                                 const gl::DepthStencilState &depthStencilState)
{
    setStencilFrontOps(PackGLStencilOp(depthStencilState.stencilFail),
                       PackGLStencilOp(depthStencilState.stencilPassDepthPass),
                       PackGLStencilOp(depthStencilState.stencilPassDepthFail));
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, front));
}

void GraphicsPipelineDesc::updateStencilBackOps(GraphicsPipelineTransitionBits *transition,
                                                const gl::DepthStencilState &depthStencilState)
{
    setStencilBackOps(PackGLStencilOp(depthStencilState.stencilBackFail),
                      PackGLStencilOp(depthStencilState.stencilBackPassDepthPass),
                      PackGLStencilOp(depthStencilState.stencilBackPassDepthFail));
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, back));
}

void GraphicsPipelineDesc::updateStencilFrontWriteMask(
    GraphicsPipelineTransitionBits *transition,
    const gl::DepthStencilState &depthStencilState,
    const gl::Framebuffer *drawFramebuffer)
{
    // Don't write to stencil buffers that should not exist
    setStencilFrontWriteMask(static_cast<uint8_t>(
        drawFramebuffer->hasStencil() ? depthStencilState.stencilWritemask : 0));
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, front));
}

void GraphicsPipelineDesc::updateStencilBackWriteMask(
    GraphicsPipelineTransitionBits *transition,
    const gl::DepthStencilState &depthStencilState,
    const gl::Framebuffer *drawFramebuffer)
{
    // Don't write to stencil buffers that should not exist
    setStencilBackWriteMask(static_cast<uint8_t>(
        drawFramebuffer->hasStencil() ? depthStencilState.stencilBackWritemask : 0));
    transition->set(ANGLE_GET_TRANSITION_BIT(mDepthStencilStateInfo, back));
}

void GraphicsPipelineDesc::updatePolygonOffsetFillEnabled(
    GraphicsPipelineTransitionBits *transition,
    bool enabled)
{
    mRasterizationAndMultisampleStateInfo.bits.depthBiasEnable = enabled;
    transition->set(ANGLE_GET_TRANSITION_BIT(mRasterizationAndMultisampleStateInfo, bits));
}

void GraphicsPipelineDesc::updatePolygonOffset(GraphicsPipelineTransitionBits *transition,
                                               const gl::RasterizerState &rasterState)
{
    mRasterizationAndMultisampleStateInfo.depthBiasSlopeFactor    = rasterState.polygonOffsetFactor;
    mRasterizationAndMultisampleStateInfo.depthBiasConstantFactor = rasterState.polygonOffsetUnits;
    transition->set(
        ANGLE_GET_TRANSITION_BIT(mRasterizationAndMultisampleStateInfo, depthBiasSlopeFactor));
    transition->set(
        ANGLE_GET_TRANSITION_BIT(mRasterizationAndMultisampleStateInfo, depthBiasConstantFactor));
}

void GraphicsPipelineDesc::setRenderPassDesc(const RenderPassDesc &renderPassDesc)
{
    mRenderPassDesc = renderPassDesc;
}

void GraphicsPipelineDesc::setViewport(const VkViewport &viewport)
{
    mViewport = viewport;
}

void GraphicsPipelineDesc::updateViewport(GraphicsPipelineTransitionBits *transition,
                                          const VkViewport &viewport)
{
    mViewport = viewport;
    transition->set(ANGLE_GET_TRANSITION_BIT(mViewport, x));
    transition->set(ANGLE_GET_TRANSITION_BIT(mViewport, y));
    transition->set(ANGLE_GET_TRANSITION_BIT(mViewport, width));
    transition->set(ANGLE_GET_TRANSITION_BIT(mViewport, height));
    transition->set(ANGLE_GET_TRANSITION_BIT(mViewport, minDepth));
    transition->set(ANGLE_GET_TRANSITION_BIT(mViewport, maxDepth));
}

void GraphicsPipelineDesc::updateDepthRange(GraphicsPipelineTransitionBits *transition,
                                            float nearPlane,
                                            float farPlane)
{
    // GLES2.0 Section 2.12.1: Each of n and f are clamped to lie within [0, 1], as are all
    // arguments of type clampf.
    mViewport.minDepth = gl::clamp01(nearPlane);
    mViewport.maxDepth = gl::clamp01(farPlane);
    transition->set(ANGLE_GET_TRANSITION_BIT(mViewport, minDepth));
    transition->set(ANGLE_GET_TRANSITION_BIT(mViewport, maxDepth));
}

void GraphicsPipelineDesc::setScissor(const VkRect2D &scissor)
{
    mScissor = scissor;
}

void GraphicsPipelineDesc::updateScissor(GraphicsPipelineTransitionBits *transition,
                                         const VkRect2D &scissor)
{
    mScissor = scissor;
    transition->set(ANGLE_GET_TRANSITION_BIT(mScissor, offset.x));
    transition->set(ANGLE_GET_TRANSITION_BIT(mScissor, offset.y));
    transition->set(ANGLE_GET_TRANSITION_BIT(mScissor, extent.width));
    transition->set(ANGLE_GET_TRANSITION_BIT(mScissor, extent.height));
}

void GraphicsPipelineDesc::updateRenderPassDesc(GraphicsPipelineTransitionBits *transition,
                                                const RenderPassDesc &renderPassDesc)
{
    setRenderPassDesc(renderPassDesc);

    // The RenderPass is a special case where it spans multiple bits but has no member.
    constexpr size_t kFirstBit =
        offsetof(GraphicsPipelineDesc, mRenderPassDesc) >> kTransitionByteShift;
    constexpr size_t kBitCount = kRenderPassDescSize >> kTransitionByteShift;
    for (size_t bit = 0; bit < kBitCount; ++bit)
    {
        transition->set(kFirstBit + bit);
    }
}

// AttachmentOpsArray implementation.
AttachmentOpsArray::AttachmentOpsArray()
{
    memset(&mOps, 0, sizeof(PackedAttachmentOpsDesc) * mOps.size());
}

AttachmentOpsArray::~AttachmentOpsArray() = default;

AttachmentOpsArray::AttachmentOpsArray(const AttachmentOpsArray &other)
{
    memcpy(&mOps, &other.mOps, sizeof(PackedAttachmentOpsDesc) * mOps.size());
}

AttachmentOpsArray &AttachmentOpsArray::operator=(const AttachmentOpsArray &other)
{
    memcpy(&mOps, &other.mOps, sizeof(PackedAttachmentOpsDesc) * mOps.size());
    return *this;
}

const PackedAttachmentOpsDesc &AttachmentOpsArray::operator[](size_t index) const
{
    return mOps[index];
}

PackedAttachmentOpsDesc &AttachmentOpsArray::operator[](size_t index)
{
    return mOps[index];
}

void AttachmentOpsArray::initDummyOp(size_t index,
                                     VkImageLayout initialLayout,
                                     VkImageLayout finalLayout)
{
    PackedAttachmentOpsDesc &ops = mOps[index];

    SetBitField(ops.initialLayout, initialLayout);
    SetBitField(ops.finalLayout, finalLayout);
    SetBitField(ops.loadOp, VK_ATTACHMENT_LOAD_OP_LOAD);
    SetBitField(ops.stencilLoadOp, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
    SetBitField(ops.storeOp, VK_ATTACHMENT_STORE_OP_STORE);
    SetBitField(ops.stencilStoreOp, VK_ATTACHMENT_STORE_OP_DONT_CARE);
}

void AttachmentOpsArray::initWithLoadStore(size_t index,
                                           VkImageLayout initialLayout,
                                           VkImageLayout finalLayout)
{
    PackedAttachmentOpsDesc &ops = mOps[index];

    SetBitField(ops.initialLayout, initialLayout);
    SetBitField(ops.finalLayout, finalLayout);
    SetBitField(ops.loadOp, VK_ATTACHMENT_LOAD_OP_LOAD);
    SetBitField(ops.stencilLoadOp, VK_ATTACHMENT_LOAD_OP_LOAD);
    SetBitField(ops.storeOp, VK_ATTACHMENT_STORE_OP_STORE);
    SetBitField(ops.stencilStoreOp, VK_ATTACHMENT_STORE_OP_STORE);
}

size_t AttachmentOpsArray::hash() const
{
    return angle::ComputeGenericHash(mOps);
}

bool operator==(const AttachmentOpsArray &lhs, const AttachmentOpsArray &rhs)
{
    return (memcmp(&lhs, &rhs, sizeof(AttachmentOpsArray)) == 0);
}

// DescriptorSetLayoutDesc implementation.
DescriptorSetLayoutDesc::DescriptorSetLayoutDesc() : mPackedDescriptorSetLayout{} {}

DescriptorSetLayoutDesc::~DescriptorSetLayoutDesc() = default;

DescriptorSetLayoutDesc::DescriptorSetLayoutDesc(const DescriptorSetLayoutDesc &other) = default;

DescriptorSetLayoutDesc &DescriptorSetLayoutDesc::operator=(const DescriptorSetLayoutDesc &other) =
    default;

size_t DescriptorSetLayoutDesc::hash() const
{
    return angle::ComputeGenericHash(mPackedDescriptorSetLayout);
}

bool DescriptorSetLayoutDesc::operator==(const DescriptorSetLayoutDesc &other) const
{
    return (memcmp(&mPackedDescriptorSetLayout, &other.mPackedDescriptorSetLayout,
                   sizeof(mPackedDescriptorSetLayout)) == 0);
}

void DescriptorSetLayoutDesc::update(uint32_t bindingIndex, VkDescriptorType type, uint32_t count)
{
    ASSERT(static_cast<size_t>(type) < std::numeric_limits<uint16_t>::max());
    ASSERT(count < std::numeric_limits<uint16_t>::max());

    PackedDescriptorSetBinding &packedBinding = mPackedDescriptorSetLayout[bindingIndex];

    packedBinding.type  = static_cast<uint16_t>(type);
    packedBinding.count = static_cast<uint16_t>(count);
}

void DescriptorSetLayoutDesc::unpackBindings(DescriptorSetLayoutBindingVector *bindings) const
{
    for (uint32_t bindingIndex = 0; bindingIndex < kMaxDescriptorSetLayoutBindings; ++bindingIndex)
    {
        const PackedDescriptorSetBinding &packedBinding = mPackedDescriptorSetLayout[bindingIndex];
        if (packedBinding.count == 0)
            continue;

        VkDescriptorSetLayoutBinding binding = {};
        binding.binding                      = bindingIndex;
        binding.descriptorCount              = packedBinding.count;
        binding.descriptorType               = static_cast<VkDescriptorType>(packedBinding.type);
        binding.stageFlags =
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
        binding.pImmutableSamplers = nullptr;

        bindings->push_back(binding);
    }
}

// PipelineLayoutDesc implementation.
PipelineLayoutDesc::PipelineLayoutDesc() : mDescriptorSetLayouts{}, mPushConstantRanges{} {}

PipelineLayoutDesc::~PipelineLayoutDesc() = default;

PipelineLayoutDesc::PipelineLayoutDesc(const PipelineLayoutDesc &other) = default;

PipelineLayoutDesc &PipelineLayoutDesc::operator=(const PipelineLayoutDesc &rhs)
{
    mDescriptorSetLayouts = rhs.mDescriptorSetLayouts;
    mPushConstantRanges   = rhs.mPushConstantRanges;
    return *this;
}

size_t PipelineLayoutDesc::hash() const
{
    return angle::ComputeGenericHash(*this);
}

bool PipelineLayoutDesc::operator==(const PipelineLayoutDesc &other) const
{
    return memcmp(this, &other, sizeof(PipelineLayoutDesc)) == 0;
}

void PipelineLayoutDesc::updateDescriptorSetLayout(uint32_t setIndex,
                                                   const DescriptorSetLayoutDesc &desc)
{
    ASSERT(setIndex < mDescriptorSetLayouts.size());
    mDescriptorSetLayouts[setIndex] = desc;
}

void PipelineLayoutDesc::updatePushConstantRange(gl::ShaderType shaderType,
                                                 uint32_t offset,
                                                 uint32_t size)
{
    ASSERT(shaderType == gl::ShaderType::Vertex || shaderType == gl::ShaderType::Fragment ||
           shaderType == gl::ShaderType::Compute);
    PackedPushConstantRange &packed = mPushConstantRanges[static_cast<size_t>(shaderType)];
    packed.offset                   = offset;
    packed.size                     = size;
}

const PushConstantRangeArray<PackedPushConstantRange> &PipelineLayoutDesc::getPushConstantRanges()
    const
{
    return mPushConstantRanges;
}

// PipelineHelper implementation.
PipelineHelper::PipelineHelper() = default;

PipelineHelper::~PipelineHelper() = default;

void PipelineHelper::destroy(VkDevice device)
{
    mPipeline.destroy(device);
}

void PipelineHelper::addTransition(GraphicsPipelineTransitionBits bits,
                                   const GraphicsPipelineDesc *desc,
                                   PipelineHelper *pipeline)
{
    mTransitions.emplace_back(bits, desc, pipeline);
}
}  // namespace vk

// RenderPassCache implementation.
RenderPassCache::RenderPassCache() = default;

RenderPassCache::~RenderPassCache()
{
    ASSERT(mPayload.empty());
}

void RenderPassCache::destroy(VkDevice device)
{
    for (auto &outerIt : mPayload)
    {
        for (auto &innerIt : outerIt.second)
        {
            innerIt.second.get().destroy(device);
        }
    }
    mPayload.clear();
}

angle::Result RenderPassCache::addRenderPass(vk::Context *context,
                                             Serial serial,
                                             const vk::RenderPassDesc &desc,
                                             vk::RenderPass **renderPassOut)
{
    // Insert some dummy attachment ops.
    // It would be nice to pre-populate the cache in the Renderer so we rarely miss here.
    vk::AttachmentOpsArray ops;

    for (uint32_t attachmentIndex = 0; attachmentIndex < desc.attachmentCount(); ++attachmentIndex)
    {
        angle::FormatID formatID = desc[attachmentIndex];
        ASSERT(formatID != angle::FormatID::NONE);
        const vk::Format &format = context->getRenderer()->getFormat(formatID);
        if (!format.angleFormat().hasDepthOrStencilBits())
        {
            ops.initDummyOp(attachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }
        else
        {
            ops.initDummyOp(attachmentIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }
    }

    return getRenderPassWithOps(context, serial, desc, ops, renderPassOut);
}

angle::Result RenderPassCache::getRenderPassWithOps(vk::Context *context,
                                                    Serial serial,
                                                    const vk::RenderPassDesc &desc,
                                                    const vk::AttachmentOpsArray &attachmentOps,
                                                    vk::RenderPass **renderPassOut)
{
    auto outerIt = mPayload.find(desc);
    if (outerIt != mPayload.end())
    {
        InnerCache &innerCache = outerIt->second;

        auto innerIt = innerCache.find(attachmentOps);
        if (innerIt != innerCache.end())
        {
            // Update the serial before we return.
            // TODO(jmadill): Could possibly use an MRU cache here.
            innerIt->second.updateSerial(serial);
            *renderPassOut = &innerIt->second.get();
            return angle::Result::Continue;
        }
    }
    else
    {
        auto emplaceResult = mPayload.emplace(desc, InnerCache());
        outerIt            = emplaceResult.first;
    }

    vk::RenderPass newRenderPass;
    ANGLE_TRY(vk::InitializeRenderPassFromDesc(context, desc, attachmentOps, &newRenderPass));

    vk::RenderPassAndSerial withSerial(std::move(newRenderPass), serial);

    InnerCache &innerCache = outerIt->second;
    auto insertPos         = innerCache.emplace(attachmentOps, std::move(withSerial));
    *renderPassOut         = &insertPos.first->second.get();

    // TODO(jmadill): Trim cache, and pre-populate with the most common RPs on startup.
    return angle::Result::Continue;
}

// GraphicsPipelineCache implementation.
GraphicsPipelineCache::GraphicsPipelineCache() = default;

GraphicsPipelineCache::~GraphicsPipelineCache()
{
    ASSERT(mPayload.empty());
}

void GraphicsPipelineCache::destroy(VkDevice device)
{
    for (auto &item : mPayload)
    {
        vk::PipelineHelper &pipeline = item.second;
        pipeline.destroy(device);
    }

    mPayload.clear();
}

void GraphicsPipelineCache::release(RendererVk *renderer)
{
    for (auto &item : mPayload)
    {
        vk::PipelineHelper &pipeline = item.second;
        renderer->releaseObject(pipeline.getSerial(), &pipeline.getPipeline());
    }

    mPayload.clear();
}

angle::Result GraphicsPipelineCache::insertPipeline(
    vk::Context *context,
    const vk::PipelineCache &pipelineCacheVk,
    const vk::RenderPass &compatibleRenderPass,
    const vk::PipelineLayout &pipelineLayout,
    const gl::AttributesMask &activeAttribLocationsMask,
    const vk::ShaderModule *vertexModule,
    const vk::ShaderModule *fragmentModule,
    const vk::GraphicsPipelineDesc &desc,
    const vk::GraphicsPipelineDesc **descPtrOut,
    vk::PipelineHelper **pipelineOut)
{
    vk::Pipeline newPipeline;

    // This "if" is left here for the benefit of VulkanPipelineCachePerfTest.
    if (context != nullptr)
    {
        ANGLE_TRY(desc.initializePipeline(context, pipelineCacheVk, compatibleRenderPass,
                                          pipelineLayout, activeAttribLocationsMask, vertexModule,
                                          fragmentModule, &newPipeline));
    }

    // The Serial will be updated outside of this query.
    auto insertedItem = mPayload.emplace(desc, std::move(newPipeline));
    *descPtrOut       = &insertedItem.first->first;
    *pipelineOut      = &insertedItem.first->second;

    return angle::Result::Continue;
}

void GraphicsPipelineCache::populate(const vk::GraphicsPipelineDesc &desc, vk::Pipeline &&pipeline)
{
    auto item = mPayload.find(desc);
    if (item != mPayload.end())
    {
        return;
    }

    mPayload.emplace(desc, std::move(pipeline));
}

// DescriptorSetLayoutCache implementation.
DescriptorSetLayoutCache::DescriptorSetLayoutCache() = default;

DescriptorSetLayoutCache::~DescriptorSetLayoutCache()
{
    ASSERT(mPayload.empty());
}

void DescriptorSetLayoutCache::destroy(VkDevice device)
{
    for (auto &item : mPayload)
    {
        vk::RefCountedDescriptorSetLayout &layout = item.second;
        ASSERT(!layout.isReferenced());
        layout.get().destroy(device);
    }

    mPayload.clear();
}

angle::Result DescriptorSetLayoutCache::getDescriptorSetLayout(
    vk::Context *context,
    const vk::DescriptorSetLayoutDesc &desc,
    vk::BindingPointer<vk::DescriptorSetLayout> *descriptorSetLayoutOut)
{
    auto iter = mPayload.find(desc);
    if (iter != mPayload.end())
    {
        vk::RefCountedDescriptorSetLayout &layout = iter->second;
        descriptorSetLayoutOut->set(&layout);
        return angle::Result::Continue;
    }

    // We must unpack the descriptor set layout description.
    vk::DescriptorSetLayoutBindingVector bindings;
    desc.unpackBindings(&bindings);

    VkDescriptorSetLayoutCreateInfo createInfo = {};
    createInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.flags        = 0;
    createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    createInfo.pBindings    = bindings.data();

    vk::DescriptorSetLayout newLayout;
    ANGLE_VK_TRY(context, newLayout.init(context->getDevice(), createInfo));

    auto insertedItem =
        mPayload.emplace(desc, vk::RefCountedDescriptorSetLayout(std::move(newLayout)));
    vk::RefCountedDescriptorSetLayout &insertedLayout = insertedItem.first->second;
    descriptorSetLayoutOut->set(&insertedLayout);

    return angle::Result::Continue;
}

// PipelineLayoutCache implementation.
PipelineLayoutCache::PipelineLayoutCache() = default;

PipelineLayoutCache::~PipelineLayoutCache()
{
    ASSERT(mPayload.empty());
}

void PipelineLayoutCache::destroy(VkDevice device)
{
    for (auto &item : mPayload)
    {
        vk::RefCountedPipelineLayout &layout = item.second;
        layout.get().destroy(device);
    }

    mPayload.clear();
}

angle::Result PipelineLayoutCache::getPipelineLayout(
    vk::Context *context,
    const vk::PipelineLayoutDesc &desc,
    const vk::DescriptorSetLayoutPointerArray &descriptorSetLayouts,
    vk::BindingPointer<vk::PipelineLayout> *pipelineLayoutOut)
{
    auto iter = mPayload.find(desc);
    if (iter != mPayload.end())
    {
        vk::RefCountedPipelineLayout &layout = iter->second;
        pipelineLayoutOut->set(&layout);
        return angle::Result::Continue;
    }

    // Note this does not handle gaps in descriptor set layouts gracefully.
    angle::FixedVector<VkDescriptorSetLayout, vk::kMaxDescriptorSetLayouts> setLayoutHandles;
    for (const vk::BindingPointer<vk::DescriptorSetLayout> &layoutPtr : descriptorSetLayouts)
    {
        if (layoutPtr.valid())
        {
            VkDescriptorSetLayout setLayout = layoutPtr.get().getHandle();
            if (setLayout != VK_NULL_HANDLE)
            {
                setLayoutHandles.push_back(setLayout);
            }
        }
    }

    angle::FixedVector<VkPushConstantRange, vk::kMaxPushConstantRanges> pushConstantRanges;
    const vk::PushConstantRangeArray<vk::PackedPushConstantRange> &descPushConstantRanges =
        desc.getPushConstantRanges();
    for (size_t shaderIndex = 0; shaderIndex < descPushConstantRanges.size(); ++shaderIndex)
    {
        const vk::PackedPushConstantRange &pushConstantDesc = descPushConstantRanges[shaderIndex];
        if (pushConstantDesc.size > 0)
        {
            static constexpr VkShaderStageFlagBits kShaderStages[vk::kMaxPushConstantRanges] = {
                VK_SHADER_STAGE_VERTEX_BIT,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                VK_SHADER_STAGE_GEOMETRY_BIT,
                VK_SHADER_STAGE_COMPUTE_BIT,
            };
            static_assert(static_cast<uint32_t>(gl::ShaderType::Vertex) == 0, "Fix this table");
            static_assert(static_cast<uint32_t>(gl::ShaderType::Fragment) == 1, "Fix this table");
            static_assert(static_cast<uint32_t>(gl::ShaderType::Geometry) == 2, "Fix this table");
            static_assert(static_cast<uint32_t>(gl::ShaderType::Compute) == 3, "Fix this table");
            VkPushConstantRange pushConstantRange = {};
            pushConstantRange.stageFlags          = kShaderStages[shaderIndex];
            pushConstantRange.offset              = pushConstantDesc.offset;
            pushConstantRange.size                = pushConstantDesc.size;

            pushConstantRanges.push_back(pushConstantRange);
        }
    }

    // No pipeline layout found. We must create a new one.
    VkPipelineLayoutCreateInfo createInfo = {};
    createInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfo.flags                      = 0;
    createInfo.setLayoutCount             = static_cast<uint32_t>(setLayoutHandles.size());
    createInfo.pSetLayouts                = setLayoutHandles.data();
    createInfo.pushConstantRangeCount     = static_cast<uint32_t>(pushConstantRanges.size());
    createInfo.pPushConstantRanges        = pushConstantRanges.data();

    vk::PipelineLayout newLayout;
    ANGLE_VK_TRY(context, newLayout.init(context->getDevice(), createInfo));

    auto insertedItem = mPayload.emplace(desc, vk::RefCountedPipelineLayout(std::move(newLayout)));
    vk::RefCountedPipelineLayout &insertedLayout = insertedItem.first->second;
    pipelineLayoutOut->set(&insertedLayout);

    return angle::Result::Continue;
}
}  // namespace rx
