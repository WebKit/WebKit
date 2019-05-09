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
#include "libANGLE/renderer/vulkan/RendererVk.h"

namespace rx
{

namespace BufferUtils_comp   = vk::InternalShader::BufferUtils_comp;
namespace ConvertVertex_comp = vk::InternalShader::ConvertVertex_comp;
namespace ImageClear_frag    = vk::InternalShader::ImageClear_frag;
namespace ImageCopy_frag     = vk::InternalShader::ImageCopy_frag;

namespace
{
// All internal shaders assume there is only one descriptor set, indexed at 0
constexpr uint32_t kSetIndex = 0;

constexpr uint32_t kBufferClearOutputBinding        = 0;
constexpr uint32_t kBufferCopyDestinationBinding    = 0;
constexpr uint32_t kBufferCopySourceBinding         = 1;
constexpr uint32_t kConvertVertexDestinationBinding = 0;
constexpr uint32_t kConvertVertexSourceBinding      = 1;
constexpr uint32_t kImageCopySourceBinding          = 0;

uint32_t GetBufferUtilsFlags(size_t dispatchSize, const vk::Format &format)
{
    uint32_t flags                    = dispatchSize % 64 == 0 ? BufferUtils_comp::kIsAligned : 0;
    const angle::Format &bufferFormat = format.bufferFormat();

    flags |= bufferFormat.isInt()
                 ? BufferUtils_comp::kIsInt
                 : bufferFormat.isUint() ? BufferUtils_comp::kIsUint : BufferUtils_comp::kIsFloat;

    return flags;
}

uint32_t GetConvertVertexFlags(const UtilsVk::ConvertVertexParameters &params)
{
    bool srcIsInt   = params.srcFormat->isInt();
    bool srcIsUint  = params.srcFormat->isUint();
    bool srcIsSnorm = params.srcFormat->isSnorm();
    bool srcIsUnorm = params.srcFormat->isUnorm();
    bool srcIsFixed = params.srcFormat->isFixed;
    bool srcIsFloat = params.srcFormat->isFloat();

    bool destIsInt   = params.destFormat->isInt();
    bool destIsUint  = params.destFormat->isUint();
    bool destIsFloat = params.destFormat->isFloat();

    // Assert on the types to make sure the shader supports its.  These are based on
    // ConvertVertex_comp::Conversion values.
    ASSERT(!destIsInt || srcIsInt);      // If destination is int, src must be int too
    ASSERT(!destIsUint || srcIsUint);    // If destination is uint, src must be uint too
    ASSERT(!srcIsFixed || destIsFloat);  // If source is fixed, dest must be float
    // One of each bool set must be true
    ASSERT(srcIsInt || srcIsUint || srcIsSnorm || srcIsUnorm || srcIsFixed || srcIsFloat);
    ASSERT(destIsInt || destIsUint || destIsFloat);

    // We currently don't have any big-endian devices in the list of supported platforms.  The
    // shader is capable of supporting big-endian architectures, but the relevant flag (IsBigEndian)
    // is not added to the build configuration file (to reduce binary size).  If necessary, add
    // IsBigEndian to ConvertVertex.comp.json and select the appropriate flag based on the
    // endian-ness test here.
    uint32_t endiannessTest                       = 0;
    *reinterpret_cast<uint8_t *>(&endiannessTest) = 1;
    ASSERT(endiannessTest == 1);

    uint32_t flags = 0;

    if (srcIsInt && destIsInt)
    {
        flags |= ConvertVertex_comp::kIntToInt;
    }
    else if (srcIsUint && destIsUint)
    {
        flags |= ConvertVertex_comp::kUintToUint;
    }
    else if (srcIsInt)
    {
        flags |= ConvertVertex_comp::kIntToFloat;
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

    flags |= format.isInt()
                 ? ImageClear_frag::kIsInt
                 : format.isUint() ? ImageClear_frag::kIsUint : ImageClear_frag::kIsFloat;

    return flags;
}
uint32_t GetImageCopyFlags(const vk::Format &srcFormat, const vk::Format &destFormat)
{
    const angle::Format &srcAngleFormat  = srcFormat.angleFormat();
    const angle::Format &destAngleFormat = destFormat.angleFormat();

    uint32_t flags = 0;

    flags |= srcAngleFormat.isInt() ? ImageCopy_frag::kSrcIsInt
                                    : srcAngleFormat.isUint() ? ImageCopy_frag::kSrcIsUint
                                                              : ImageCopy_frag::kSrcIsFloat;
    flags |= destAngleFormat.isInt() ? ImageCopy_frag::kDestIsInt
                                     : destAngleFormat.isUint() ? ImageCopy_frag::kDestIsUint
                                                                : ImageCopy_frag::kDestIsFloat;

    return flags;
}

uint32_t GetFormatDefaultChannelMask(const vk::Format &format)
{
    uint32_t mask = 0;

    const angle::Format &angleFormat   = format.angleFormat();
    const angle::Format &textureFormat = format.imageFormat();

    // Red can never be introduced due to format emulation (except for luma which is handled
    // especially)
    ASSERT(((angleFormat.redBits > 0) == (textureFormat.redBits > 0)) || angleFormat.isLUMA());
    mask |= angleFormat.greenBits == 0 && textureFormat.greenBits > 0 ? 2 : 0;
    mask |= angleFormat.blueBits == 0 && textureFormat.blueBits > 0 ? 4 : 0;
    mask |= angleFormat.alphaBits == 0 && textureFormat.alphaBits > 0 ? 8 : 0;

    return mask;
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
}

angle::Result UtilsVk::ensureResourcesInitialized(vk::Context *context,
                                                  Function function,
                                                  VkDescriptorPoolSize *setSizes,
                                                  size_t setSizesCount,
                                                  size_t pushConstantsSize)
{
    RendererVk *renderer = context->getRenderer();

    vk::DescriptorSetLayoutDesc descriptorSetDesc;

    uint32_t currentBinding = 0;
    for (size_t i = 0; i < setSizesCount; ++i)
    {
        descriptorSetDesc.update(currentBinding, setSizes[i].type, setSizes[i].descriptorCount);
        currentBinding += setSizes[i].descriptorCount;
    }

    ANGLE_TRY(renderer->getDescriptorSetLayout(context, descriptorSetDesc,
                                               &mDescriptorSetLayouts[function][kSetIndex]));

    gl::ShaderType pushConstantsShaderStage = function >= Function::ComputeStartIndex
                                                  ? gl::ShaderType::Compute
                                                  : gl::ShaderType::Fragment;

    // Corresponding pipeline layouts:
    vk::PipelineLayoutDesc pipelineLayoutDesc;

    pipelineLayoutDesc.updateDescriptorSetLayout(kSetIndex, descriptorSetDesc);
    pipelineLayoutDesc.updatePushConstantRange(pushConstantsShaderStage, 0, pushConstantsSize);

    ANGLE_TRY(renderer->getPipelineLayout(
        context, pipelineLayoutDesc, mDescriptorSetLayouts[function], &mPipelineLayouts[function]));

    if (setSizesCount > 0)
    {
        ANGLE_TRY(mDescriptorPools[function].init(context, setSizes, setSizesCount));
    }

    return angle::Result::Continue;
}

angle::Result UtilsVk::ensureBufferClearResourcesInitialized(vk::Context *context)
{
    if (mPipelineLayouts[Function::BufferClear].valid())
    {
        return angle::Result::Continue;
    }

    VkDescriptorPoolSize setSizes[1] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1},
    };

    return ensureResourcesInitialized(context, Function::BufferClear, setSizes, ArraySize(setSizes),
                                      sizeof(BufferUtilsShaderParams));
}

angle::Result UtilsVk::ensureBufferCopyResourcesInitialized(vk::Context *context)
{
    if (mPipelineLayouts[Function::BufferCopy].valid())
    {
        return angle::Result::Continue;
    }

    VkDescriptorPoolSize setSizes[2] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1},
    };

    return ensureResourcesInitialized(context, Function::BufferCopy, setSizes, ArraySize(setSizes),
                                      sizeof(BufferUtilsShaderParams));
}

angle::Result UtilsVk::ensureConvertVertexResourcesInitialized(vk::Context *context)
{
    if (mPipelineLayouts[Function::ConvertVertexBuffer].valid())
    {
        return angle::Result::Continue;
    }

    VkDescriptorPoolSize setSizes[2] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
    };

    return ensureResourcesInitialized(context, Function::ConvertVertexBuffer, setSizes,
                                      ArraySize(setSizes), sizeof(ConvertVertexShaderParams));
}

angle::Result UtilsVk::ensureImageClearResourcesInitialized(vk::Context *context)
{
    if (mPipelineLayouts[Function::ImageClear].valid())
    {
        return angle::Result::Continue;
    }

    // The shader does not use any descriptor sets.
    return ensureResourcesInitialized(context, Function::ImageClear, nullptr, 0,
                                      sizeof(ImageClearShaderParams));
}

angle::Result UtilsVk::ensureImageCopyResourcesInitialized(vk::Context *context)
{
    if (mPipelineLayouts[Function::ImageCopy].valid())
    {
        return angle::Result::Continue;
    }

    VkDescriptorPoolSize setSizes[1] = {
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1},
    };

    return ensureResourcesInitialized(context, Function::ImageCopy, setSizes, ArraySize(setSizes),
                                      sizeof(ImageCopyShaderParams));
}

angle::Result UtilsVk::setupProgram(vk::Context *context,
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
    RendererVk *renderer = context->getRenderer();

    bool isCompute = function >= Function::ComputeStartIndex;
    VkShaderStageFlags pushConstantsShaderStage =
        isCompute ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;

    // If compute, vsShader and pipelineDesc should be nullptr, and if not compute they shouldn't
    // be.
    ASSERT(isCompute != (vsShader && pipelineDesc));

    const vk::BindingPointer<vk::PipelineLayout> &pipelineLayout = mPipelineLayouts[function];

    Serial serial = renderer->getCurrentQueueSerial();

    if (isCompute)
    {
        vk::PipelineAndSerial *pipelineAndSerial;
        program->setShader(gl::ShaderType::Compute, fsCsShader);
        ANGLE_TRY(program->getComputePipeline(context, pipelineLayout.get(), &pipelineAndSerial));
        pipelineAndSerial->updateSerial(serial);
        commandBuffer->bindComputePipeline(pipelineAndSerial->get());
        if (descriptorSet != VK_NULL_HANDLE)
        {
            commandBuffer->bindComputeDescriptorSets(pipelineLayout.get(), &descriptorSet);
        }
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

        ANGLE_TRY(program->getGraphicsPipeline(
            context, &renderer->getRenderPassCache(), renderer->getPipelineCache(), serial,
            pipelineLayout.get(), *pipelineDesc, gl::AttributesMask(), &descPtr, &helper));
        helper->updateSerial(serial);
        commandBuffer->bindGraphicsPipeline(helper->getPipeline());
        if (descriptorSet != VK_NULL_HANDLE)
        {
            commandBuffer->bindGraphicsDescriptorSets(pipelineLayout.get(), 0, 1, &descriptorSet, 0,
                                                      nullptr);
        }
    }

    commandBuffer->pushConstants(pipelineLayout.get(), pushConstantsShaderStage, 0,
                                 pushConstantsSize, pushConstants);

    return angle::Result::Continue;
}

angle::Result UtilsVk::clearBuffer(vk::Context *context,
                                   vk::BufferHelper *dest,
                                   const ClearParameters &params)
{
    RendererVk *renderer = context->getRenderer();

    ANGLE_TRY(ensureBufferClearResourcesInitialized(context));

    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(dest->recordCommands(context, &commandBuffer));

    // Tell dest it's being written to.
    dest->onWrite(VK_ACCESS_SHADER_WRITE_BIT);

    const vk::Format &destFormat = dest->getViewFormat();

    uint32_t flags = BufferUtils_comp::kIsClear | GetBufferUtilsFlags(params.size, destFormat);

    BufferUtilsShaderParams shaderParams;
    shaderParams.destOffset = params.offset;
    shaderParams.size       = params.size;
    shaderParams.clearValue = params.clearValue;

    VkDescriptorSet descriptorSet;
    vk::RefCountedDescriptorPoolBinding descriptorPoolBinding;
    ANGLE_TRY(mDescriptorPools[Function::BufferClear].allocateSets(
        context, mDescriptorSetLayouts[Function::BufferClear][kSetIndex].get().ptr(), 1,
        &descriptorPoolBinding, &descriptorSet));
    descriptorPoolBinding.get().updateSerial(context->getRenderer()->getCurrentQueueSerial());

    VkWriteDescriptorSet writeInfo = {};

    writeInfo.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfo.dstSet           = descriptorSet;
    writeInfo.dstBinding       = kBufferClearOutputBinding;
    writeInfo.descriptorCount  = 1;
    writeInfo.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    writeInfo.pTexelBufferView = dest->getBufferView().ptr();

    vkUpdateDescriptorSets(context->getDevice(), 1, &writeInfo, 0, nullptr);

    vk::RefCounted<vk::ShaderAndSerial> *shader = nullptr;
    ANGLE_TRY(renderer->getShaderLibrary().getBufferUtils_comp(context, flags, &shader));

    ANGLE_TRY(setupProgram(context, Function::BufferClear, shader, nullptr,
                           &mBufferUtilsPrograms[flags], nullptr, descriptorSet, &shaderParams,
                           sizeof(shaderParams), commandBuffer));

    commandBuffer->dispatch(UnsignedCeilDivide(params.size, 64), 1, 1);

    descriptorPoolBinding.reset();

    return angle::Result::Continue;
}

angle::Result UtilsVk::copyBuffer(vk::Context *context,
                                  vk::BufferHelper *dest,
                                  vk::BufferHelper *src,
                                  const CopyParameters &params)
{
    RendererVk *renderer = context->getRenderer();

    ANGLE_TRY(ensureBufferCopyResourcesInitialized(context));

    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(dest->recordCommands(context, &commandBuffer));

    // Tell src we are going to read from it.
    src->onRead(dest, VK_ACCESS_SHADER_READ_BIT);
    // Tell dest it's being written to.
    dest->onWrite(VK_ACCESS_SHADER_WRITE_BIT);

    const vk::Format &destFormat = dest->getViewFormat();
    const vk::Format &srcFormat  = src->getViewFormat();

    ASSERT(destFormat.vkFormatIsInt == srcFormat.vkFormatIsInt);
    ASSERT(destFormat.vkFormatIsUnsigned == srcFormat.vkFormatIsUnsigned);

    uint32_t flags = BufferUtils_comp::kIsCopy | GetBufferUtilsFlags(params.size, destFormat);

    BufferUtilsShaderParams shaderParams;
    shaderParams.destOffset = params.destOffset;
    shaderParams.size       = params.size;
    shaderParams.srcOffset  = params.srcOffset;

    VkDescriptorSet descriptorSet;
    vk::RefCountedDescriptorPoolBinding descriptorPoolBinding;
    ANGLE_TRY(mDescriptorPools[Function::BufferCopy].allocateSets(
        context, mDescriptorSetLayouts[Function::BufferCopy][kSetIndex].get().ptr(), 1,
        &descriptorPoolBinding, &descriptorSet));
    descriptorPoolBinding.get().updateSerial(context->getRenderer()->getCurrentQueueSerial());

    VkWriteDescriptorSet writeInfo[2] = {};

    writeInfo[0].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfo[0].dstSet           = descriptorSet;
    writeInfo[0].dstBinding       = kBufferCopyDestinationBinding;
    writeInfo[0].descriptorCount  = 1;
    writeInfo[0].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    writeInfo[0].pTexelBufferView = dest->getBufferView().ptr();

    writeInfo[1].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfo[1].dstSet           = descriptorSet;
    writeInfo[1].dstBinding       = kBufferCopySourceBinding;
    writeInfo[1].descriptorCount  = 1;
    writeInfo[1].descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    writeInfo[1].pTexelBufferView = src->getBufferView().ptr();

    vkUpdateDescriptorSets(context->getDevice(), 2, writeInfo, 0, nullptr);

    vk::RefCounted<vk::ShaderAndSerial> *shader = nullptr;
    ANGLE_TRY(renderer->getShaderLibrary().getBufferUtils_comp(context, flags, &shader));

    ANGLE_TRY(setupProgram(context, Function::BufferCopy, shader, nullptr,
                           &mBufferUtilsPrograms[flags], nullptr, descriptorSet, &shaderParams,
                           sizeof(shaderParams), commandBuffer));

    commandBuffer->dispatch(UnsignedCeilDivide(params.size, 64), 1, 1);

    descriptorPoolBinding.reset();

    return angle::Result::Continue;
}

angle::Result UtilsVk::convertVertexBuffer(vk::Context *context,
                                           vk::BufferHelper *dest,
                                           vk::BufferHelper *src,
                                           const ConvertVertexParameters &params)
{
    RendererVk *renderer = context->getRenderer();

    ANGLE_TRY(ensureConvertVertexResourcesInitialized(context));

    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(dest->recordCommands(context, &commandBuffer));

    // Tell src we are going to read from it.
    src->onRead(dest, VK_ACCESS_SHADER_READ_BIT);
    // Tell dest it's being written to.
    dest->onWrite(VK_ACCESS_SHADER_WRITE_BIT);

    ConvertVertexShaderParams shaderParams;
    shaderParams.Ns = params.srcFormat->channelCount();
    shaderParams.Bs = params.srcFormat->pixelBytes / params.srcFormat->channelCount();
    shaderParams.Ss = params.srcStride;
    shaderParams.Nd = params.destFormat->channelCount();
    shaderParams.Bd = params.destFormat->pixelBytes / params.destFormat->channelCount();
    shaderParams.Sd = shaderParams.Nd * shaderParams.Bd;
    // The component size is expected to either be 1, 2 or 4 bytes.
    ASSERT(4 % shaderParams.Bs == 0);
    ASSERT(4 % shaderParams.Bd == 0);
    shaderParams.Es = 4 / shaderParams.Bs;
    shaderParams.Ed = 4 / shaderParams.Bd;
    // Total number of output components is simply the number of vertices by number of components in
    // each.
    shaderParams.componentCount = params.vertexCount * shaderParams.Nd;
    // Total number of 4-byte outputs is the number of components divided by how many components can
    // fit in a 4-byte value.  Note that this value is also the invocation size of the shader.
    shaderParams.outputCount = shaderParams.componentCount / shaderParams.Ed;
    shaderParams.srcOffset   = params.srcOffset;
    shaderParams.destOffset  = params.destOffset;

    uint32_t flags = GetConvertVertexFlags(params);

    bool isAligned =
        shaderParams.outputCount % 64 == 0 && shaderParams.componentCount % shaderParams.Ed == 0;
    flags |= isAligned ? ConvertVertex_comp::kIsAligned : 0;

    VkDescriptorSet descriptorSet;
    vk::RefCountedDescriptorPoolBinding descriptorPoolBinding;
    ANGLE_TRY(mDescriptorPools[Function::ConvertVertexBuffer].allocateSets(
        context, mDescriptorSetLayouts[Function::ConvertVertexBuffer][kSetIndex].get().ptr(), 1,
        &descriptorPoolBinding, &descriptorSet));
    descriptorPoolBinding.get().updateSerial(context->getRenderer()->getCurrentQueueSerial());

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

    vkUpdateDescriptorSets(context->getDevice(), 1, &writeInfo, 0, nullptr);

    vk::RefCounted<vk::ShaderAndSerial> *shader = nullptr;
    ANGLE_TRY(renderer->getShaderLibrary().getConvertVertex_comp(context, flags, &shader));

    ANGLE_TRY(setupProgram(context, Function::ConvertVertexBuffer, shader, nullptr,
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
    RendererVk *renderer = contextVk->getRenderer();

    vk::RenderPass *compatibleRenderPass = nullptr;
    ANGLE_TRY(renderer->getCompatibleRenderPass(contextVk, renderPassDesc, &compatibleRenderPass));

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

    renderer->releaseObject(renderer->getCurrentQueueSerial(), &framebuffer);

    return angle::Result::Continue;
}

angle::Result UtilsVk::clearFramebuffer(ContextVk *contextVk,
                                        FramebufferVk *framebuffer,
                                        const ClearFramebufferParameters &params)
{
    RendererVk *renderer = contextVk->getRenderer();

    ANGLE_TRY(ensureImageClearResourcesInitialized(contextVk));

    const gl::Rectangle &scissoredRenderArea = params.clearArea;

    vk::CommandBuffer *commandBuffer;
    if (!framebuffer->appendToStartedRenderPass(renderer->getCurrentQueueSerial(),
                                                scissoredRenderArea, &commandBuffer))
    {
        ANGLE_TRY(framebuffer->startNewRenderPass(contextVk, scissoredRenderArea, &commandBuffer));
    }

    ImageClearShaderParams shaderParams;
    shaderParams.clearValue = params.colorClearValue;

    vk::GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.initDefaults();
    pipelineDesc.setColorWriteMask(0, gl::DrawBufferMask());
    pipelineDesc.setSingleColorWriteMask(params.colorAttachmentIndex, params.colorMaskFlags);
    pipelineDesc.setRenderPassDesc(*params.renderPassDesc);
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
    gl_vk::GetViewport(completeRenderArea, 0.0f, 1.0f, invertViewport, params.renderAreaHeight,
                       &viewport);
    pipelineDesc.setViewport(viewport);

    pipelineDesc.setScissor(gl_vk::GetRect(params.clearArea));

    vk::ShaderLibrary &shaderLibrary                    = renderer->getShaderLibrary();
    vk::RefCounted<vk::ShaderAndSerial> *vertexShader   = nullptr;
    vk::RefCounted<vk::ShaderAndSerial> *fragmentShader = nullptr;
    vk::ShaderProgramHelper *imageClearProgram          = &mImageClearProgramVSOnly;

    ANGLE_TRY(shaderLibrary.getFullScreenQuad_vert(contextVk, 0, &vertexShader));
    if (params.clearColor)
    {
        uint32_t flags = GetImageClearFlags(*params.colorFormat, params.colorAttachmentIndex);
        ANGLE_TRY(shaderLibrary.getImageClear_frag(contextVk, flags, &fragmentShader));
        imageClearProgram = &mImageClearProgram[flags];
    }

    ANGLE_TRY(setupProgram(contextVk, Function::ImageClear, fragmentShader, vertexShader,
                           imageClearProgram, &pipelineDesc, VK_NULL_HANDLE, &shaderParams,
                           sizeof(shaderParams), commandBuffer));
    commandBuffer->draw(6, 0);
    return angle::Result::Continue;
}

angle::Result UtilsVk::copyImage(ContextVk *contextVk,
                                 vk::ImageHelper *dest,
                                 const vk::ImageView *destView,
                                 vk::ImageHelper *src,
                                 const vk::ImageView *srcView,
                                 const CopyImageParameters &params)
{
    RendererVk *renderer = contextVk->getRenderer();

    ANGLE_TRY(ensureImageCopyResourcesInitialized(contextVk));

    const vk::Format &srcFormat  = src->getFormat();
    const vk::Format &destFormat = dest->getFormat();

    ImageCopyShaderParams shaderParams;
    shaderParams.flipY            = params.srcFlipY || params.destFlipY;
    shaderParams.premultiplyAlpha = params.srcPremultiplyAlpha;
    shaderParams.unmultiplyAlpha  = params.srcUnmultiplyAlpha;
    shaderParams.destHasLuminance = destFormat.angleFormat().luminanceBits > 0;
    shaderParams.destIsAlpha =
        destFormat.angleFormat().isLUMA() && destFormat.angleFormat().alphaBits > 0;
    shaderParams.destDefaultChannelsMask = GetFormatDefaultChannelMask(destFormat);
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

    uint32_t flags = GetImageCopyFlags(srcFormat, destFormat);
    flags |= src->getLayerCount() > 1 ? ImageCopy_frag::kSrcIsArray : 0;

    VkDescriptorSet descriptorSet;
    vk::RefCountedDescriptorPoolBinding descriptorPoolBinding;
    ANGLE_TRY(mDescriptorPools[Function::ImageCopy].allocateSets(
        contextVk, mDescriptorSetLayouts[Function::ImageCopy][kSetIndex].get().ptr(), 1,
        &descriptorPoolBinding, &descriptorSet));
    descriptorPoolBinding.get().updateSerial(contextVk->getRenderer()->getCurrentQueueSerial());

    vk::RenderPassDesc renderPassDesc;
    renderPassDesc.setSamples(dest->getSamples());
    renderPassDesc.packAttachment(destFormat);

    vk::GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.initDefaults();
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
    if (src->isLayoutChangeNecessary(vk::ImageLayout::FragmentShaderReadOnly))
    {
        vk::CommandBuffer *srcLayoutChange;
        ANGLE_TRY(src->recordCommands(contextVk, &srcLayoutChange));
        src->changeLayout(VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::FragmentShaderReadOnly,
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
    src->addReadDependency(dest);

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

    vk::ShaderLibrary &shaderLibrary                    = renderer->getShaderLibrary();
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

UtilsVk::ClearFramebufferParameters::ClearFramebufferParameters()
    : renderPassDesc(nullptr),
      renderAreaHeight(0),
      clearColor(false),
      clearStencil(false),
      stencilMask(0),
      colorMaskFlags(0),
      colorAttachmentIndex(0),
      colorFormat(nullptr),
      colorClearValue{},
      stencilClearValue(0)
{}

}  // namespace rx
