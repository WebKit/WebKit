//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ContextVk.cpp:
//    Implements the class methods for ContextVk.
//

#include "libANGLE/renderer/vulkan/ContextVk.h"

#include "common/bitset_utils.h"
#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/Program.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/CompilerVk.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/DeviceVk.h"
#include "libANGLE/renderer/vulkan/FenceNVVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/ImageVk.h"
#include "libANGLE/renderer/vulkan/ProgramPipelineVk.h"
#include "libANGLE/renderer/vulkan/ProgramVk.h"
#include "libANGLE/renderer/vulkan/QueryVk.h"
#include "libANGLE/renderer/vulkan/RenderbufferVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/SamplerVk.h"
#include "libANGLE/renderer/vulkan/ShaderVk.h"
#include "libANGLE/renderer/vulkan/SyncVk.h"
#include "libANGLE/renderer/vulkan/TextureVk.h"
#include "libANGLE/renderer/vulkan/TransformFeedbackVk.h"
#include "libANGLE/renderer/vulkan/VertexArrayVk.h"
#include "libANGLE/renderer/vulkan/formatutilsvk.h"

namespace rx
{

namespace
{

VkIndexType GetVkIndexType(GLenum glIndexType)
{
    switch (glIndexType)
    {
        case GL_UNSIGNED_SHORT:
            return VK_INDEX_TYPE_UINT16;
        case GL_UNSIGNED_INT:
            return VK_INDEX_TYPE_UINT32;
        default:
            UNREACHABLE();
            return VK_INDEX_TYPE_MAX_ENUM;
    }
}

enum DescriptorPoolIndex : uint8_t
{
    UniformBufferPool = 0,
    TexturePool       = 1,
};

}  // anonymous namespace

ContextVk::ContextVk(const gl::ContextState &state, RendererVk *renderer)
    : ContextImpl(state), mRenderer(renderer), mCurrentDrawMode(GL_NONE)
{
    // The module handle is filled out at draw time.
    mCurrentShaderStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    mCurrentShaderStages[0].pNext  = nullptr;
    mCurrentShaderStages[0].flags  = 0;
    mCurrentShaderStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    mCurrentShaderStages[0].module = VK_NULL_HANDLE;
    mCurrentShaderStages[0].pName  = "main";
    mCurrentShaderStages[0].pSpecializationInfo = nullptr;

    mCurrentShaderStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    mCurrentShaderStages[1].pNext  = nullptr;
    mCurrentShaderStages[1].flags  = 0;
    mCurrentShaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    mCurrentShaderStages[1].module = VK_NULL_HANDLE;
    mCurrentShaderStages[1].pName  = "main";
    mCurrentShaderStages[1].pSpecializationInfo = nullptr;

    // The binding descriptions are filled in at draw time.
    mCurrentVertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    mCurrentVertexInputState.pNext = nullptr;
    mCurrentVertexInputState.flags = 0;
    mCurrentVertexInputState.vertexBindingDescriptionCount   = 0;
    mCurrentVertexInputState.pVertexBindingDescriptions      = nullptr;
    mCurrentVertexInputState.vertexAttributeDescriptionCount = 0;
    mCurrentVertexInputState.pVertexAttributeDescriptions    = nullptr;

    // Primitive topology is filled in at draw time.
    mCurrentInputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    mCurrentInputAssemblyState.pNext = nullptr;
    mCurrentInputAssemblyState.flags = 0;
    mCurrentInputAssemblyState.topology = gl_vk::GetPrimitiveTopology(mCurrentDrawMode);
    mCurrentInputAssemblyState.primitiveRestartEnable = VK_FALSE;

    // Set initial viewport and scissor state.
    mCurrentViewportVk.x        = 0.0f;
    mCurrentViewportVk.y        = 0.0f;
    mCurrentViewportVk.width    = 0.0f;
    mCurrentViewportVk.height   = 0.0f;
    mCurrentViewportVk.minDepth = 0.0f;
    mCurrentViewportVk.maxDepth = 1.0f;

    mCurrentScissorVk.offset.x      = 0;
    mCurrentScissorVk.offset.y      = 0;
    mCurrentScissorVk.extent.width  = 0u;
    mCurrentScissorVk.extent.height = 0u;

    mCurrentViewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    mCurrentViewportState.pNext         = nullptr;
    mCurrentViewportState.flags         = 0;
    mCurrentViewportState.viewportCount = 1;
    mCurrentViewportState.pViewports    = &mCurrentViewportVk;
    mCurrentViewportState.scissorCount  = 1;
    mCurrentViewportState.pScissors     = &mCurrentScissorVk;

    // Set initial rasterizer state.
    // TODO(jmadill): Extra rasterizer state features.
    mCurrentRasterState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    mCurrentRasterState.pNext = nullptr;
    mCurrentRasterState.flags = 0;
    mCurrentRasterState.depthClampEnable        = VK_FALSE;
    mCurrentRasterState.rasterizerDiscardEnable = VK_FALSE;
    mCurrentRasterState.polygonMode             = VK_POLYGON_MODE_FILL;
    mCurrentRasterState.cullMode                = VK_CULL_MODE_NONE;
    mCurrentRasterState.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    mCurrentRasterState.depthBiasEnable         = VK_FALSE;
    mCurrentRasterState.depthBiasConstantFactor = 0.0f;
    mCurrentRasterState.depthBiasClamp          = 0.0f;
    mCurrentRasterState.depthBiasSlopeFactor    = 0.0f;
    mCurrentRasterState.lineWidth               = 1.0f;

    // Initialize a dummy multisample state.
    // TODO(jmadill): Multisample state.
    mCurrentMultisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    mCurrentMultisampleState.pNext = nullptr;
    mCurrentMultisampleState.flags = 0;
    mCurrentMultisampleState.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
    mCurrentMultisampleState.sampleShadingEnable   = VK_FALSE;
    mCurrentMultisampleState.minSampleShading      = 0.0f;
    mCurrentMultisampleState.pSampleMask           = nullptr;
    mCurrentMultisampleState.alphaToCoverageEnable = VK_FALSE;
    mCurrentMultisampleState.alphaToOneEnable      = VK_FALSE;

    // TODO(jmadill): Depth/stencil state.

    // Initialize a dummy MRT blend state.
    // TODO(jmadill): Blend state/MRT.
    mCurrentBlendAttachmentState.blendEnable         = VK_FALSE;
    mCurrentBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    mCurrentBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    mCurrentBlendAttachmentState.colorBlendOp        = VK_BLEND_OP_ADD;
    mCurrentBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    mCurrentBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    mCurrentBlendAttachmentState.alphaBlendOp        = VK_BLEND_OP_ADD;
    mCurrentBlendAttachmentState.colorWriteMask =
        (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
         VK_COLOR_COMPONENT_A_BIT);

    mCurrentBlendState.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    mCurrentBlendState.pNext             = 0;
    mCurrentBlendState.flags             = 0;
    mCurrentBlendState.logicOpEnable     = VK_FALSE;
    mCurrentBlendState.logicOp           = VK_LOGIC_OP_CLEAR;
    mCurrentBlendState.attachmentCount   = 1;
    mCurrentBlendState.pAttachments      = &mCurrentBlendAttachmentState;
    mCurrentBlendState.blendConstants[0] = 0.0f;
    mCurrentBlendState.blendConstants[1] = 0.0f;
    mCurrentBlendState.blendConstants[2] = 0.0f;
    mCurrentBlendState.blendConstants[3] = 0.0f;

    // TODO(jmadill): Dynamic state.

    // The layout and renderpass are filled out at draw time.
    mCurrentPipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    mCurrentPipelineInfo.pNext               = nullptr;
    mCurrentPipelineInfo.flags               = 0;
    mCurrentPipelineInfo.stageCount          = 2;
    mCurrentPipelineInfo.pStages             = mCurrentShaderStages;
    mCurrentPipelineInfo.pVertexInputState   = &mCurrentVertexInputState;
    mCurrentPipelineInfo.pInputAssemblyState = &mCurrentInputAssemblyState;
    mCurrentPipelineInfo.pTessellationState  = nullptr;
    mCurrentPipelineInfo.pViewportState      = &mCurrentViewportState;
    mCurrentPipelineInfo.pRasterizationState = &mCurrentRasterState;
    mCurrentPipelineInfo.pMultisampleState   = &mCurrentMultisampleState;
    mCurrentPipelineInfo.pDepthStencilState  = nullptr;
    mCurrentPipelineInfo.pColorBlendState    = &mCurrentBlendState;
    mCurrentPipelineInfo.pDynamicState       = nullptr;
    mCurrentPipelineInfo.layout              = VK_NULL_HANDLE;
    mCurrentPipelineInfo.renderPass          = VK_NULL_HANDLE;
    mCurrentPipelineInfo.subpass             = 0;
    mCurrentPipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
    mCurrentPipelineInfo.basePipelineIndex   = 0;
}

ContextVk::~ContextVk()
{
    invalidateCurrentPipeline();
}

void ContextVk::onDestroy(const gl::Context *context)
{
    VkDevice device = mRenderer->getDevice();

    mDescriptorPool.destroy(device);
}

gl::Error ContextVk::initialize()
{
    VkDevice device = mRenderer->getDevice();

    VkDescriptorPoolSize poolSizes[2];
    poolSizes[UniformBufferPool].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[UniformBufferPool].descriptorCount = 1024;
    poolSizes[TexturePool].type                  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[TexturePool].descriptorCount       = 1024;

    VkDescriptorPoolCreateInfo descriptorPoolInfo;
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.pNext = nullptr;
    descriptorPoolInfo.flags = 0;

    // TODO(jmadill): Pick non-arbitrary max.
    descriptorPoolInfo.maxSets = 2048;

    // Reserve pools for uniform blocks and textures.
    descriptorPoolInfo.poolSizeCount = 2;
    descriptorPoolInfo.pPoolSizes    = poolSizes;

    ANGLE_TRY(mDescriptorPool.init(device, descriptorPoolInfo));

    return gl::NoError();
}

gl::Error ContextVk::flush(const gl::Context *context)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error ContextVk::finish(const gl::Context *context)
{
    // TODO(jmadill): Implement finish.
    // UNIMPLEMENTED();
    return gl::NoError();
}

gl::Error ContextVk::initPipeline(const gl::Context *context)
{
    ASSERT(!mCurrentPipeline.valid());

    VkDevice device       = mRenderer->getDevice();
    const auto &state     = mState.getState();
    const gl::Program *programGL   = state.getProgram();
    const gl::VertexArray *vao     = state.getVertexArray();
    const gl::Framebuffer *drawFBO = state.getDrawFramebuffer();
    ProgramVk *programVk  = vk::GetImpl(programGL);
    FramebufferVk *vkFBO  = vk::GetImpl(drawFBO);
    VertexArrayVk *vkVAO  = vk::GetImpl(vao);

    // Ensure the attribs and bindings are updated.
    vkVAO->updateVertexDescriptions(context);

    const auto &vertexBindings = vkVAO->getVertexBindingDescs();
    const auto &vertexAttribs  = vkVAO->getVertexAttribDescs();

    // TODO(jmadill): Validate with ASSERT against physical device limits/caps?
    mCurrentVertexInputState.vertexBindingDescriptionCount =
        static_cast<uint32_t>(vertexBindings.size());
    mCurrentVertexInputState.pVertexBindingDescriptions = vertexBindings.data();
    mCurrentVertexInputState.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(vertexAttribs.size());
    mCurrentVertexInputState.pVertexAttributeDescriptions = vertexAttribs.data();

    mCurrentInputAssemblyState.topology = gl_vk::GetPrimitiveTopology(mCurrentDrawMode);

    vk::RenderPass *renderPass = nullptr;
    ANGLE_TRY_RESULT(vkFBO->getRenderPass(context, device), renderPass);
    ASSERT(renderPass && renderPass->valid());

    const vk::PipelineLayout &pipelineLayout = programVk->getPipelineLayout();
    ASSERT(pipelineLayout.valid());

    mCurrentPipelineInfo.layout     = pipelineLayout.getHandle();
    mCurrentPipelineInfo.renderPass = renderPass->getHandle();

    ANGLE_TRY(mCurrentPipeline.initGraphics(device, mCurrentPipelineInfo));

    return gl::NoError();
}

gl::Error ContextVk::setupDraw(const gl::Context *context, GLenum mode)
{
    if (mode != mCurrentDrawMode)
    {
        invalidateCurrentPipeline();
        mCurrentDrawMode = mode;
    }

    if (!mCurrentPipeline.valid())
    {
        ANGLE_TRY(initPipeline(context));
        ASSERT(mCurrentPipeline.valid());
    }

    const auto &state     = mState.getState();
    const gl::Program *programGL = state.getProgram();
    ProgramVk *programVk  = vk::GetImpl(programGL);
    const gl::VertexArray *vao   = state.getVertexArray();
    VertexArrayVk *vkVAO  = vk::GetImpl(vao);
    const auto *drawFBO   = state.getDrawFramebuffer();
    FramebufferVk *vkFBO  = vk::GetImpl(drawFBO);
    Serial queueSerial    = mRenderer->getCurrentQueueSerial();
    uint32_t maxAttrib    = programGL->getState().getMaxActiveAttribLocation();

    // Process vertex attributes. Assume zero offsets for now.
    // TODO(jmadill): Offset handling.
    const std::vector<VkBuffer> &vertexHandles = vkVAO->getCurrentVertexBufferHandlesCache();
    angle::MemoryBuffer *zeroBuf               = nullptr;
    ANGLE_TRY(context->getZeroFilledBuffer(maxAttrib * sizeof(VkDeviceSize), &zeroBuf));

    vk::CommandBufferAndState *commandBuffer = nullptr;
    ANGLE_TRY(mRenderer->getStartedCommandBuffer(&commandBuffer));
    ANGLE_TRY(mRenderer->ensureInRenderPass(context, vkFBO));

    commandBuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, mCurrentPipeline);
    commandBuffer->bindVertexBuffers(0, maxAttrib, vertexHandles.data(),
                                     reinterpret_cast<const VkDeviceSize *>(zeroBuf->data()));

    // TODO(jmadill): the queue serial should be bound to the pipeline.
    setQueueSerial(queueSerial);
    vkVAO->updateCurrentBufferSerials(programGL->getActiveAttribLocationsMask(), queueSerial);

    // TODO(jmadill): Can probably use more dirty bits here.
    ContextVk *contextVk = vk::GetImpl(context);
    ANGLE_TRY(programVk->updateUniforms(contextVk));
    programVk->updateTexturesDescriptorSet(contextVk);

    // Bind the graphics descriptor sets.
    // TODO(jmadill): Handle multiple command buffers.
    const auto &descriptorSets = programVk->getDescriptorSets();
    uint32_t firstSet          = programVk->getDescriptorSetOffset();
    uint32_t setCount          = static_cast<uint32_t>(descriptorSets.size());
    if (!descriptorSets.empty() && ((setCount - firstSet) > 0))
    {
        const vk::PipelineLayout &pipelineLayout = programVk->getPipelineLayout();
        commandBuffer->bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, firstSet,
                                          setCount - firstSet, &descriptorSets[firstSet], 0,
                                          nullptr);
    }

    return gl::NoError();
}

gl::Error ContextVk::drawArrays(const gl::Context *context, GLenum mode, GLint first, GLsizei count)
{
    ANGLE_TRY(setupDraw(context, mode));

    vk::CommandBufferAndState *commandBuffer = nullptr;
    ANGLE_TRY(mRenderer->getStartedCommandBuffer(&commandBuffer));

    commandBuffer->draw(count, 1, first, 0);
    return gl::NoError();
}

gl::Error ContextVk::drawArraysInstanced(const gl::Context *context,
                                         GLenum mode,
                                         GLint first,
                                         GLsizei count,
                                         GLsizei instanceCount)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error ContextVk::drawElements(const gl::Context *context,
                                  GLenum mode,
                                  GLsizei count,
                                  GLenum type,
                                  const void *indices)
{
    ANGLE_TRY(setupDraw(context, mode));

    if (indices)
    {
        // TODO(jmadill): Buffer offsets and immediate data.
        UNIMPLEMENTED();
        return gl::InternalError() << "Only zero-offset index buffers are currently implemented.";
    }

    if (type == GL_UNSIGNED_BYTE)
    {
        // TODO(jmadill): Index translation.
        UNIMPLEMENTED();
        return gl::InternalError() << "Unsigned byte translation is not yet implemented.";
    }

    vk::CommandBufferAndState *commandBuffer = nullptr;
    ANGLE_TRY(mRenderer->getStartedCommandBuffer(&commandBuffer));

    const gl::Buffer *elementArrayBuffer =
        mState.getState().getVertexArray()->getElementArrayBuffer().get();
    ASSERT(elementArrayBuffer);

    BufferVk *elementArrayBufferVk = vk::GetImpl(elementArrayBuffer);

    commandBuffer->bindIndexBuffer(elementArrayBufferVk->getVkBuffer(), 0, GetVkIndexType(type));
    commandBuffer->drawIndexed(count, 1, 0, 0, 0);

    return gl::NoError();
}

gl::Error ContextVk::drawElementsInstanced(const gl::Context *context,
                                           GLenum mode,
                                           GLsizei count,
                                           GLenum type,
                                           const void *indices,
                                           GLsizei instances)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error ContextVk::drawRangeElements(const gl::Context *context,
                                       GLenum mode,
                                       GLuint start,
                                       GLuint end,
                                       GLsizei count,
                                       GLenum type,
                                       const void *indices)
{
    return gl::NoError();
}

VkDevice ContextVk::getDevice() const
{
    return mRenderer->getDevice();
}

vk::Error ContextVk::getStartedCommandBuffer(vk::CommandBufferAndState **commandBufferOut)
{
    return mRenderer->getStartedCommandBuffer(commandBufferOut);
}

vk::Error ContextVk::submitCommands(vk::CommandBufferAndState *commandBuffer)
{
    setQueueSerial(mRenderer->getCurrentQueueSerial());
    ANGLE_TRY(mRenderer->submitCommandBuffer(commandBuffer));
    return vk::NoError();
}

gl::Error ContextVk::drawArraysIndirect(const gl::Context *context,
                                        GLenum mode,
                                        const void *indirect)
{
    UNIMPLEMENTED();
    return gl::InternalError() << "DrawArraysIndirect hasn't been implemented for vulkan backend.";
}

gl::Error ContextVk::drawElementsIndirect(const gl::Context *context,
                                          GLenum mode,
                                          GLenum type,
                                          const void *indirect)
{
    UNIMPLEMENTED();
    return gl::InternalError()
           << "DrawElementsIndirect hasn't been implemented for vulkan backend.";
}

GLenum ContextVk::getResetStatus()
{
    UNIMPLEMENTED();
    return GL_NO_ERROR;
}

std::string ContextVk::getVendorString() const
{
    UNIMPLEMENTED();
    return std::string();
}

std::string ContextVk::getRendererDescription() const
{
    return mRenderer->getRendererDescription();
}

void ContextVk::insertEventMarker(GLsizei length, const char *marker)
{
    UNIMPLEMENTED();
}

void ContextVk::pushGroupMarker(GLsizei length, const char *marker)
{
    UNIMPLEMENTED();
}

void ContextVk::popGroupMarker()
{
    UNIMPLEMENTED();
}

void ContextVk::pushDebugGroup(GLenum source, GLuint id, GLsizei length, const char *message)
{
    UNIMPLEMENTED();
}

void ContextVk::popDebugGroup()
{
    UNIMPLEMENTED();
}

void ContextVk::syncState(const gl::Context *context, const gl::State::DirtyBits &dirtyBits)
{
    if (dirtyBits.any())
    {
        invalidateCurrentPipeline();
    }

    const auto &glState = context->getGLState();

    // TODO(jmadill): Full dirty bits implementation.
    bool dirtyTextures = false;

    for (auto dirtyBit : dirtyBits)
    {
        switch (dirtyBit)
        {
            case gl::State::DIRTY_BIT_SCISSOR_TEST_ENABLED:
                WARN() << "DIRTY_BIT_SCISSOR_TEST_ENABLED unimplemented";
                break;
            case gl::State::DIRTY_BIT_SCISSOR:
                WARN() << "DIRTY_BIT_SCISSOR unimplemented";
                break;
            case gl::State::DIRTY_BIT_VIEWPORT:
            {
                const gl::Rectangle &viewportGL = glState.getViewport();
                mCurrentViewportVk.x            = static_cast<float>(viewportGL.x);
                mCurrentViewportVk.y            = static_cast<float>(viewportGL.y);
                mCurrentViewportVk.width        = static_cast<float>(viewportGL.width);
                mCurrentViewportVk.height       = static_cast<float>(viewportGL.height);
                mCurrentViewportVk.minDepth     = glState.getNearPlane();
                mCurrentViewportVk.maxDepth     = glState.getFarPlane();

                // TODO(jmadill): Scissor.
                mCurrentScissorVk.offset.x      = viewportGL.x;
                mCurrentScissorVk.offset.y      = viewportGL.y;
                mCurrentScissorVk.extent.width  = viewportGL.width;
                mCurrentScissorVk.extent.height = viewportGL.height;
                break;
            }
            case gl::State::DIRTY_BIT_DEPTH_RANGE:
                WARN() << "DIRTY_BIT_DEPTH_RANGE unimplemented";
                break;
            case gl::State::DIRTY_BIT_BLEND_ENABLED:
                WARN() << "DIRTY_BIT_BLEND_ENABLED unimplemented";
                break;
            case gl::State::DIRTY_BIT_BLEND_COLOR:
                WARN() << "DIRTY_BIT_BLEND_COLOR unimplemented";
                break;
            case gl::State::DIRTY_BIT_BLEND_FUNCS:
                WARN() << "DIRTY_BIT_BLEND_FUNCS unimplemented";
                break;
            case gl::State::DIRTY_BIT_BLEND_EQUATIONS:
                WARN() << "DIRTY_BIT_BLEND_EQUATIONS unimplemented";
                break;
            case gl::State::DIRTY_BIT_COLOR_MASK:
                WARN() << "DIRTY_BIT_COLOR_MASK unimplemented";
                break;
            case gl::State::DIRTY_BIT_SAMPLE_ALPHA_TO_COVERAGE_ENABLED:
                WARN() << "DIRTY_BIT_SAMPLE_ALPHA_TO_COVERAGE_ENABLED unimplemented";
                break;
            case gl::State::DIRTY_BIT_SAMPLE_COVERAGE_ENABLED:
                WARN() << "DIRTY_BIT_SAMPLE_COVERAGE_ENABLED unimplemented";
                break;
            case gl::State::DIRTY_BIT_SAMPLE_COVERAGE:
                WARN() << "DIRTY_BIT_SAMPLE_COVERAGE unimplemented";
                break;
            case gl::State::DIRTY_BIT_SAMPLE_MASK_ENABLED:
                WARN() << "DIRTY_BIT_SAMPLE_MASK_ENABLED unimplemented";
                break;
            case gl::State::DIRTY_BIT_SAMPLE_MASK:
                WARN() << "DIRTY_BIT_SAMPLE_MASK unimplemented";
                break;
            case gl::State::DIRTY_BIT_DEPTH_TEST_ENABLED:
                WARN() << "DIRTY_BIT_DEPTH_TEST_ENABLED unimplemented";
                break;
            case gl::State::DIRTY_BIT_DEPTH_FUNC:
                WARN() << "DIRTY_BIT_DEPTH_FUNC unimplemented";
                break;
            case gl::State::DIRTY_BIT_DEPTH_MASK:
                WARN() << "DIRTY_BIT_DEPTH_MASK unimplemented";
                break;
            case gl::State::DIRTY_BIT_STENCIL_TEST_ENABLED:
                WARN() << "DIRTY_BIT_STENCIL_TEST_ENABLED unimplemented";
                break;
            case gl::State::DIRTY_BIT_STENCIL_FUNCS_FRONT:
                WARN() << "DIRTY_BIT_STENCIL_FUNCS_FRONT unimplemented";
                break;
            case gl::State::DIRTY_BIT_STENCIL_FUNCS_BACK:
                WARN() << "DIRTY_BIT_STENCIL_FUNCS_BACK unimplemented";
                break;
            case gl::State::DIRTY_BIT_STENCIL_OPS_FRONT:
                WARN() << "DIRTY_BIT_STENCIL_OPS_FRONT unimplemented";
                break;
            case gl::State::DIRTY_BIT_STENCIL_OPS_BACK:
                WARN() << "DIRTY_BIT_STENCIL_OPS_BACK unimplemented";
                break;
            case gl::State::DIRTY_BIT_STENCIL_WRITEMASK_FRONT:
                WARN() << "DIRTY_BIT_STENCIL_WRITEMASK_FRONT unimplemented";
                break;
            case gl::State::DIRTY_BIT_STENCIL_WRITEMASK_BACK:
                WARN() << "DIRTY_BIT_STENCIL_WRITEMASK_BACK unimplemented";
                break;
            case gl::State::DIRTY_BIT_CULL_FACE_ENABLED:
            case gl::State::DIRTY_BIT_CULL_FACE:
                mCurrentRasterState.cullMode = gl_vk::GetCullMode(glState.getRasterizerState());
                break;
            case gl::State::DIRTY_BIT_FRONT_FACE:
                mCurrentRasterState.frontFace =
                    gl_vk::GetFrontFace(glState.getRasterizerState().frontFace);
                break;
            case gl::State::DIRTY_BIT_POLYGON_OFFSET_FILL_ENABLED:
                WARN() << "DIRTY_BIT_POLYGON_OFFSET_FILL_ENABLED unimplemented";
                break;
            case gl::State::DIRTY_BIT_POLYGON_OFFSET:
                WARN() << "DIRTY_BIT_POLYGON_OFFSET unimplemented";
                break;
            case gl::State::DIRTY_BIT_RASTERIZER_DISCARD_ENABLED:
                WARN() << "DIRTY_BIT_RASTERIZER_DISCARD_ENABLED unimplemented";
                break;
            case gl::State::DIRTY_BIT_LINE_WIDTH:
                mCurrentRasterState.lineWidth = glState.getLineWidth();
                break;
            case gl::State::DIRTY_BIT_PRIMITIVE_RESTART_ENABLED:
                WARN() << "DIRTY_BIT_PRIMITIVE_RESTART_ENABLED unimplemented";
                break;
            case gl::State::DIRTY_BIT_CLEAR_COLOR:
                WARN() << "DIRTY_BIT_CLEAR_COLOR unimplemented";
                break;
            case gl::State::DIRTY_BIT_CLEAR_DEPTH:
                WARN() << "DIRTY_BIT_CLEAR_DEPTH unimplemented";
                break;
            case gl::State::DIRTY_BIT_CLEAR_STENCIL:
                WARN() << "DIRTY_BIT_CLEAR_STENCIL unimplemented";
                break;
            case gl::State::DIRTY_BIT_UNPACK_STATE:
                WARN() << "DIRTY_BIT_UNPACK_STATE unimplemented";
                break;
            case gl::State::DIRTY_BIT_UNPACK_BUFFER_BINDING:
                WARN() << "DIRTY_BIT_UNPACK_BUFFER_BINDING unimplemented";
                break;
            case gl::State::DIRTY_BIT_PACK_STATE:
                WARN() << "DIRTY_BIT_PACK_STATE unimplemented";
                break;
            case gl::State::DIRTY_BIT_PACK_BUFFER_BINDING:
                WARN() << "DIRTY_BIT_PACK_BUFFER_BINDING unimplemented";
                break;
            case gl::State::DIRTY_BIT_DITHER_ENABLED:
                WARN() << "DIRTY_BIT_DITHER_ENABLED unimplemented";
                break;
            case gl::State::DIRTY_BIT_GENERATE_MIPMAP_HINT:
                WARN() << "DIRTY_BIT_GENERATE_MIPMAP_HINT unimplemented";
                break;
            case gl::State::DIRTY_BIT_SHADER_DERIVATIVE_HINT:
                WARN() << "DIRTY_BIT_SHADER_DERIVATIVE_HINT unimplemented";
                break;
            case gl::State::DIRTY_BIT_READ_FRAMEBUFFER_BINDING:
                WARN() << "DIRTY_BIT_READ_FRAMEBUFFER_BINDING unimplemented";
                break;
            case gl::State::DIRTY_BIT_DRAW_FRAMEBUFFER_BINDING:
                WARN() << "DIRTY_BIT_DRAW_FRAMEBUFFER_BINDING unimplemented";
                break;
            case gl::State::DIRTY_BIT_RENDERBUFFER_BINDING:
                WARN() << "DIRTY_BIT_RENDERBUFFER_BINDING unimplemented";
                break;
            case gl::State::DIRTY_BIT_VERTEX_ARRAY_BINDING:
                WARN() << "DIRTY_BIT_VERTEX_ARRAY_BINDING unimplemented";
                break;
            case gl::State::DIRTY_BIT_DRAW_INDIRECT_BUFFER_BINDING:
                WARN() << "DIRTY_BIT_DRAW_INDIRECT_BUFFER_BINDING unimplemented";
                break;
            case gl::State::DIRTY_BIT_PROGRAM_BINDING:
                WARN() << "DIRTY_BIT_PROGRAM_BINDING unimplemented";
                break;
            case gl::State::DIRTY_BIT_PROGRAM_EXECUTABLE:
            {
                // { vertex, fragment }
                ProgramVk *programVk           = vk::GetImpl(glState.getProgram());
                mCurrentShaderStages[0].module = programVk->getLinkedVertexModule().getHandle();
                mCurrentShaderStages[1].module = programVk->getLinkedFragmentModule().getHandle();

                // Also invalidate the vertex descriptions cache in the Vertex Array.
                VertexArrayVk *vaoVk = vk::GetImpl(glState.getVertexArray());
                vaoVk->invalidateVertexDescriptions();

                dirtyTextures = true;
                break;
            }
            case gl::State::DIRTY_BIT_TEXTURE_BINDINGS:
                dirtyTextures = true;
                break;
            case gl::State::DIRTY_BIT_SAMPLER_BINDINGS:
                dirtyTextures = true;
                break;
            case gl::State::DIRTY_BIT_MULTISAMPLING:
                WARN() << "DIRTY_BIT_MULTISAMPLING unimplemented";
                break;
            case gl::State::DIRTY_BIT_SAMPLE_ALPHA_TO_ONE:
                WARN() << "DIRTY_BIT_SAMPLE_ALPHA_TO_ONE unimplemented";
                break;
            case gl::State::DIRTY_BIT_COVERAGE_MODULATION:
                WARN() << "DIRTY_BIT_COVERAGE_MODULATION unimplemented";
                break;
            case gl::State::DIRTY_BIT_PATH_RENDERING_MATRIX_MV:
                WARN() << "DIRTY_BIT_PATH_RENDERING_MATRIX_MV unimplemented";
                break;
            case gl::State::DIRTY_BIT_PATH_RENDERING_MATRIX_PROJ:
                WARN() << "DIRTY_BIT_PATH_RENDERING_MATRIX_PROJ unimplemented";
                break;
            case gl::State::DIRTY_BIT_PATH_RENDERING_STENCIL_STATE:
                WARN() << "DIRTY_BIT_PATH_RENDERING_STENCIL_STATE unimplemented";
                break;
            case gl::State::DIRTY_BIT_FRAMEBUFFER_SRGB:
                WARN() << "DIRTY_BIT_FRAMEBUFFER_SRGB unimplemented";
                break;
            case gl::State::DIRTY_BIT_CURRENT_VALUES:
                WARN() << "DIRTY_BIT_CURRENT_VALUES unimplemented";
                break;
            default:
                UNREACHABLE();
                break;
        }
    }

    if (dirtyTextures)
    {
        ProgramVk *programVk = vk::GetImpl(glState.getProgram());
        programVk->invalidateTextures();
    }
}

GLint ContextVk::getGPUDisjoint()
{
    UNIMPLEMENTED();
    return GLint();
}

GLint64 ContextVk::getTimestamp()
{
    UNIMPLEMENTED();
    return GLint64();
}

void ContextVk::onMakeCurrent(const gl::Context * /*context*/)
{
}

const gl::Caps &ContextVk::getNativeCaps() const
{
    return mRenderer->getNativeCaps();
}

const gl::TextureCapsMap &ContextVk::getNativeTextureCaps() const
{
    return mRenderer->getNativeTextureCaps();
}

const gl::Extensions &ContextVk::getNativeExtensions() const
{
    return mRenderer->getNativeExtensions();
}

const gl::Limitations &ContextVk::getNativeLimitations() const
{
    return mRenderer->getNativeLimitations();
}

CompilerImpl *ContextVk::createCompiler()
{
    return new CompilerVk();
}

ShaderImpl *ContextVk::createShader(const gl::ShaderState &state)
{
    return new ShaderVk(state);
}

ProgramImpl *ContextVk::createProgram(const gl::ProgramState &state)
{
    return new ProgramVk(state);
}

FramebufferImpl *ContextVk::createFramebuffer(const gl::FramebufferState &state)
{
    return FramebufferVk::CreateUserFBO(state);
}

TextureImpl *ContextVk::createTexture(const gl::TextureState &state)
{
    return new TextureVk(state);
}

RenderbufferImpl *ContextVk::createRenderbuffer()
{
    return new RenderbufferVk();
}

BufferImpl *ContextVk::createBuffer(const gl::BufferState &state)
{
    return new BufferVk(state);
}

VertexArrayImpl *ContextVk::createVertexArray(const gl::VertexArrayState &state)
{
    return new VertexArrayVk(state);
}

QueryImpl *ContextVk::createQuery(GLenum type)
{
    return new QueryVk(type);
}

FenceNVImpl *ContextVk::createFenceNV()
{
    return new FenceNVVk();
}

SyncImpl *ContextVk::createSync()
{
    return new SyncVk();
}

TransformFeedbackImpl *ContextVk::createTransformFeedback(const gl::TransformFeedbackState &state)
{
    return new TransformFeedbackVk(state);
}

SamplerImpl *ContextVk::createSampler(const gl::SamplerState &state)
{
    return new SamplerVk(state);
}

ProgramPipelineImpl *ContextVk::createProgramPipeline(const gl::ProgramPipelineState &state)
{
    return new ProgramPipelineVk(state);
}

std::vector<PathImpl *> ContextVk::createPaths(GLsizei)
{
    return std::vector<PathImpl *>();
}

// TODO(jmadill): Use pipeline cache.
void ContextVk::invalidateCurrentPipeline()
{
    mRenderer->releaseResource(*this, &mCurrentPipeline);
}

gl::Error ContextVk::dispatchCompute(const gl::Context *context,
                                     GLuint numGroupsX,
                                     GLuint numGroupsY,
                                     GLuint numGroupsZ)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

vk::DescriptorPool *ContextVk::getDescriptorPool()
{
    return &mDescriptorPool;
}

}  // namespace rx
