//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramExecutableVk.cpp: Collects the information and interfaces common to both ProgramVks and
// ProgramPipelineVks in order to execute/draw with either.

#include "libANGLE/renderer/vulkan/ProgramExecutableVk.h"

#include "libANGLE/renderer/glslang_wrapper_utils.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/GlslangWrapperVk.h"
#include "libANGLE/renderer/vulkan/TextureVk.h"
#include "libANGLE/renderer/vulkan/TransformFeedbackVk.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

namespace rx
{

DefaultUniformBlock::DefaultUniformBlock() = default;

DefaultUniformBlock::~DefaultUniformBlock() = default;

// ShaderInfo implementation.
ShaderInfo::ShaderInfo() {}

ShaderInfo::~ShaderInfo() = default;

angle::Result ShaderInfo::initShaders(ContextVk *contextVk,
                                      const gl::ShaderMap<std::string> &shaderSources,
                                      const ShaderMapInterfaceVariableInfoMap &variableInfoMap)
{
    ASSERT(!valid());

    ANGLE_TRY(GlslangWrapperVk::GetShaderCode(contextVk, contextVk->getCaps(), shaderSources,
                                              variableInfoMap, &mSpirvBlobs));

    mIsInitialized = true;
    return angle::Result::Continue;
}

void ShaderInfo::release(ContextVk *contextVk)
{
    for (SpirvBlob &spirvBlob : mSpirvBlobs)
    {
        spirvBlob.clear();
    }
    mIsInitialized = false;
}

void ShaderInfo::load(gl::BinaryInputStream *stream)
{
    // Read in shader codes for all shader types
    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        SpirvBlob *spirvBlob = &mSpirvBlobs[shaderType];

        // Read the SPIR-V
        stream->readIntVector<uint32_t>(spirvBlob);
    }

    mIsInitialized = true;
}

void ShaderInfo::save(gl::BinaryOutputStream *stream)
{
    ASSERT(valid());

    // Write out shader codes for all shader types
    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const SpirvBlob &spirvBlob = mSpirvBlobs[shaderType];

        // Write the SPIR-V
        stream->writeIntVector(spirvBlob);
    }
}

// ProgramInfo implementation.
ProgramInfo::ProgramInfo() {}

ProgramInfo::~ProgramInfo() = default;

angle::Result ProgramInfo::initProgram(ContextVk *contextVk,
                                       const ShaderInfo &shaderInfo,
                                       bool enableLineRasterEmulation)
{
    const gl::ShaderMap<SpirvBlob> &spirvBlobs = shaderInfo.getSpirvBlobs();

    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const SpirvBlob &spirvBlob = spirvBlobs[shaderType];

        if (!spirvBlob.empty())
        {
            ANGLE_TRY(vk::InitShaderAndSerial(contextVk, &mShaders[shaderType].get(),
                                              spirvBlob.data(),
                                              spirvBlob.size() * sizeof(uint32_t)));

            mProgramHelper.setShader(shaderType, &mShaders[shaderType]);
        }
    }

    if (enableLineRasterEmulation)
    {
        mProgramHelper.enableSpecializationConstant(
            sh::vk::SpecializationConstantId::LineRasterEmulation);
    }

    return angle::Result::Continue;
}

void ProgramInfo::release(ContextVk *contextVk)
{
    mProgramHelper.release(contextVk);

    for (vk::RefCounted<vk::ShaderAndSerial> &shader : mShaders)
    {
        shader.get().destroy(contextVk->getDevice());
    }
}

ProgramExecutableVk::ProgramExecutableVk()
    : mEmptyDescriptorSets{}, mNumDefaultUniformDescriptors(0), mDynamicBufferOffsets{}
{}

ProgramExecutableVk::~ProgramExecutableVk() = default;

void ProgramExecutableVk::reset(ContextVk *contextVk)
{
    RendererVk *renderer = contextVk->getRenderer();

    for (auto &descriptorSetLayout : mDescriptorSetLayouts)
    {
        descriptorSetLayout.reset();
    }
    mPipelineLayout.reset();

    mEmptyBuffer.release(renderer);

    mDescriptorSets.clear();
    mEmptyDescriptorSets.fill(VK_NULL_HANDLE);
    mNumDefaultUniformDescriptors = 0;

    for (vk::RefCountedDescriptorPoolBinding &binding : mDescriptorPoolBindings)
    {
        binding.reset();
    }

    for (vk::DynamicDescriptorPool &descriptorPool : mDynamicDescriptorPools)
    {
        descriptorPool.release(contextVk);
    }

    mTextureDescriptorsCache.clear();
    mDescriptorBuffersCache.clear();

    mDefaultProgramInfo.release(contextVk);
    mLineRasterProgramInfo.release(contextVk);
}

std::unique_ptr<rx::LinkEvent> ProgramExecutableVk::load(gl::BinaryInputStream *stream)
{
    clearVariableInfoMap();

    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        size_t variableInfoMapSize = stream->readInt<size_t>();

        for (size_t i = 0; i < variableInfoMapSize; ++i)
        {
            const std::string variableName    = stream->readString();
            ShaderInterfaceVariableInfo *info = &mVariableInfoMap[shaderType][variableName];

            info->descriptorSet = stream->readInt<uint32_t>();
            info->binding       = stream->readInt<uint32_t>();
            info->location      = stream->readInt<uint32_t>();
            info->component     = stream->readInt<uint32_t>();
            // PackedEnumBitSet uses uint8_t
            info->activeStages = gl::ShaderBitSet(stream->readInt<uint8_t>());
            info->xfbBuffer    = stream->readInt<uint32_t>();
            info->xfbOffset    = stream->readInt<uint32_t>();
            info->xfbStride    = stream->readInt<uint32_t>();
        }
    }

    return std::make_unique<LinkEventDone>(angle::Result::Continue);
}

void ProgramExecutableVk::save(gl::BinaryOutputStream *stream)
{
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        stream->writeInt<size_t>(mVariableInfoMap[shaderType].size());
        for (const auto &it : mVariableInfoMap[shaderType])
        {
            stream->writeString(it.first);
            stream->writeInt<uint32_t>(it.second.descriptorSet);
            stream->writeInt<uint32_t>(it.second.binding);
            stream->writeInt<uint32_t>(it.second.location);
            stream->writeInt<uint32_t>(it.second.component);
            // PackedEnumBitSet uses uint8_t
            stream->writeInt<uint8_t>(it.second.activeStages.bits());
            stream->writeInt<uint32_t>(it.second.xfbBuffer);
            stream->writeInt<uint32_t>(it.second.xfbOffset);
            stream->writeInt<uint32_t>(it.second.xfbStride);
        }
    }
}

void ProgramExecutableVk::clearVariableInfoMap()
{
    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mVariableInfoMap[shaderType].clear();
    }
}

uint32_t GetInterfaceBlockArraySize(const std::vector<gl::InterfaceBlock> &blocks,
                                    uint32_t bufferIndex)
{
    const gl::InterfaceBlock &block = blocks[bufferIndex];

    if (!block.isArray)
    {
        return 1;
    }

    ASSERT(block.arrayElement == 0);

    // Search consecutively until all array indices of this block are visited.
    uint32_t arraySize;
    for (arraySize = 1; bufferIndex + arraySize < blocks.size(); ++arraySize)
    {
        const gl::InterfaceBlock &nextBlock = blocks[bufferIndex + arraySize];

        if (nextBlock.arrayElement != arraySize)
        {
            break;
        }

        // It's unexpected for an array to start at a non-zero array size, so we can always rely on
        // the sequential `arrayElement`s to belong to the same block.
        ASSERT(nextBlock.name == block.name);
        ASSERT(nextBlock.isArray);
    }

    return arraySize;
}

angle::Result ProgramExecutableVk::allocateDescriptorSet(ContextVk *contextVk,
                                                         uint32_t descriptorSetIndex)
{
    bool ignoreNewPoolAllocated;
    return allocateDescriptorSetAndGetInfo(contextVk, descriptorSetIndex, &ignoreNewPoolAllocated);
}

angle::Result ProgramExecutableVk::allocateDescriptorSetAndGetInfo(ContextVk *contextVk,
                                                                   uint32_t descriptorSetIndex,
                                                                   bool *newPoolAllocatedOut)
{
    vk::DynamicDescriptorPool &dynamicDescriptorPool = mDynamicDescriptorPools[descriptorSetIndex];

    uint32_t potentialNewCount = descriptorSetIndex + 1;
    if (potentialNewCount > mDescriptorSets.size())
    {
        mDescriptorSets.resize(potentialNewCount, VK_NULL_HANDLE);
    }

    const vk::DescriptorSetLayout &descriptorSetLayout =
        mDescriptorSetLayouts[descriptorSetIndex].get();
    ANGLE_TRY(dynamicDescriptorPool.allocateSetsAndGetInfo(
        contextVk, descriptorSetLayout.ptr(), 1, &mDescriptorPoolBindings[descriptorSetIndex],
        &mDescriptorSets[descriptorSetIndex], newPoolAllocatedOut));
    mEmptyDescriptorSets[descriptorSetIndex] = VK_NULL_HANDLE;

    return angle::Result::Continue;
}

void ProgramExecutableVk::addInterfaceBlockDescriptorSetDesc(
    const std::vector<gl::InterfaceBlock> &blocks,
    const gl::ShaderType shaderType,
    VkDescriptorType descType,
    vk::DescriptorSetLayoutDesc *descOut)
{
    for (uint32_t bufferIndex = 0; bufferIndex < blocks.size();)
    {
        gl::InterfaceBlock block = blocks[bufferIndex];
        const uint32_t arraySize = GetInterfaceBlockArraySize(blocks, bufferIndex);
        bufferIndex += arraySize;

        if (!block.isActive(shaderType))
        {
            continue;
        }

        const std::string blockName             = block.mappedName;
        const ShaderInterfaceVariableInfo &info = mVariableInfoMap[shaderType][blockName];

        descOut->update(info.binding, descType, arraySize, gl_vk::kShaderStageMap[shaderType]);
    }
}

void ProgramExecutableVk::addAtomicCounterBufferDescriptorSetDesc(
    const std::vector<gl::AtomicCounterBuffer> &atomicCounterBuffers,
    const gl::ShaderType shaderType,
    vk::DescriptorSetLayoutDesc *descOut)
{
    if (atomicCounterBuffers.empty())
    {
        return;
    }

    std::string blockName(sh::vk::kAtomicCountersBlockName);
    const ShaderInterfaceVariableInfo &info = mVariableInfoMap[shaderType][blockName];

    if (!info.activeStages[shaderType])
    {
        return;
    }

    // A single storage buffer array is used for all stages for simplicity.
    descOut->update(info.binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    gl::IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS,
                    gl_vk::kShaderStageMap[shaderType]);
}

void ProgramExecutableVk::addImageDescriptorSetDesc(const gl::ProgramState &programState,
                                                    vk::DescriptorSetLayoutDesc *descOut)
{
    const std::vector<gl::ImageBinding> &imageBindings = programState.getImageBindings();
    const std::vector<gl::LinkedUniform> &uniforms     = programState.getUniforms();

    for (uint32_t imageIndex = 0; imageIndex < imageBindings.size(); ++imageIndex)
    {
        const gl::ImageBinding &imageBinding = imageBindings[imageIndex];

        uint32_t uniformIndex = programState.getUniformIndexFromImageIndex(imageIndex);
        const gl::LinkedUniform &imageUniform = uniforms[uniformIndex];

        // The front-end always binds array image units sequentially.
        uint32_t arraySize = static_cast<uint32_t>(imageBinding.boundImageUnits.size());

        for (const gl::ShaderType shaderType :
             programState.getProgramExecutable().getLinkedShaderStages())
        {
            if (!imageUniform.isActive(shaderType))
            {
                continue;
            }

            std::string name = imageUniform.mappedName;
            GetImageNameWithoutIndices(&name);
            ShaderInterfaceVariableInfo &info = mVariableInfoMap[shaderType][name];
            VkShaderStageFlags activeStages   = gl_vk::kShaderStageMap[shaderType];
            descOut->update(info.binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, arraySize,
                            activeStages);
        }
    }
}

void ProgramExecutableVk::addTextureDescriptorSetDesc(const gl::ProgramState &programState,
                                                      bool useOldRewriteStructSamplers,
                                                      vk::DescriptorSetLayoutDesc *descOut)
{
    const std::vector<gl::SamplerBinding> &samplerBindings = programState.getSamplerBindings();
    const std::vector<gl::LinkedUniform> &uniforms         = programState.getUniforms();

    for (uint32_t textureIndex = 0; textureIndex < samplerBindings.size(); ++textureIndex)
    {
        const gl::SamplerBinding &samplerBinding = samplerBindings[textureIndex];

        uint32_t uniformIndex = programState.getUniformIndexFromSamplerIndex(textureIndex);
        const gl::LinkedUniform &samplerUniform = uniforms[uniformIndex];

        const std::string samplerName = useOldRewriteStructSamplers
                                            ? GetMappedSamplerNameOld(samplerUniform.name)
                                            : GlslangGetMappedSamplerName(samplerUniform.name);

        // The front-end always binds array sampler units sequentially.
        uint32_t arraySize = static_cast<uint32_t>(samplerBinding.boundTextureUnits.size());

        if (!useOldRewriteStructSamplers)
        {
            // 2D arrays are split into multiple 1D arrays when generating
            // LinkedUniforms. Since they are flattened into one array, ignore the
            // nonzero elements and expand the array to the total array size.
            if (gl::SamplerNameContainsNonZeroArrayElement(samplerUniform.name))
            {
                continue;
            }

            for (unsigned int outerArraySize : samplerUniform.outerArraySizes)
            {
                arraySize *= outerArraySize;
            }
        }

        for (const gl::ShaderType shaderType :
             programState.getProgramExecutable().getLinkedShaderStages())
        {
            if (!samplerUniform.isActive(shaderType))
            {
                continue;
            }

            ShaderInterfaceVariableInfo &info = mVariableInfoMap[shaderType][samplerName];
            VkShaderStageFlags activeStages   = gl_vk::kShaderStageMap[shaderType];

            descOut->update(info.binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, arraySize,
                            activeStages);
        }
    }
}

void WriteBufferDescriptorSetBinding(const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding,
                                     VkDeviceSize maxSize,
                                     VkDescriptorSet descSet,
                                     VkDescriptorType descType,
                                     uint32_t bindingIndex,
                                     uint32_t arrayElement,
                                     VkDeviceSize requiredOffsetAlignment,
                                     VkDescriptorBufferInfo *bufferInfoOut,
                                     VkWriteDescriptorSet *writeInfoOut)
{
    gl::Buffer *buffer = bufferBinding.get();
    ASSERT(buffer != nullptr);

    // Make sure there's no possible under/overflow with binding size.
    static_assert(sizeof(VkDeviceSize) >= sizeof(bufferBinding.getSize()),
                  "VkDeviceSize too small");
    ASSERT(bufferBinding.getSize() >= 0);

    BufferVk *bufferVk             = vk::GetImpl(buffer);
    VkDeviceSize offset            = bufferBinding.getOffset();
    VkDeviceSize size              = bufferBinding.getSize();
    vk::BufferHelper &bufferHelper = bufferVk->getBuffer();

    // If size is 0, we can't always use VK_WHOLE_SIZE (or bufferHelper.getSize()), as the
    // backing buffer may be larger than max*BufferRange.  In that case, we use the minimum of
    // the backing buffer size (what's left after offset) and the buffer size as defined by the
    // shader.  That latter is only valid for UBOs, as SSBOs may have variable length arrays.
    size = size > 0 ? size : (bufferHelper.getSize() - offset);
    if (maxSize > 0)
    {
        size = std::min(size, maxSize);
    }

    // If requiredOffsetAlignment is 0, the buffer offset is guaranteed to have the necessary
    // alignment through other means (the backend specifying the alignment through a GLES limit that
    // the frontend then enforces).  If it's not 0, we need to bind the buffer at an offset that's
    // aligned.  The difference in offsets is communicated to the shader via driver uniforms.
    if (requiredOffsetAlignment)
    {
        VkDeviceSize alignedOffset = (offset / requiredOffsetAlignment) * requiredOffsetAlignment;
        VkDeviceSize offsetDiff    = offset - alignedOffset;

        offset = alignedOffset;
        size += offsetDiff;
    }

    bufferInfoOut->buffer = bufferHelper.getBuffer().getHandle();
    bufferInfoOut->offset = offset;
    bufferInfoOut->range  = size;

    writeInfoOut->sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfoOut->pNext            = nullptr;
    writeInfoOut->dstSet           = descSet;
    writeInfoOut->dstBinding       = bindingIndex;
    writeInfoOut->dstArrayElement  = arrayElement;
    writeInfoOut->descriptorCount  = 1;
    writeInfoOut->descriptorType   = descType;
    writeInfoOut->pImageInfo       = nullptr;
    writeInfoOut->pBufferInfo      = bufferInfoOut;
    writeInfoOut->pTexelBufferView = nullptr;
    ASSERT(writeInfoOut->pBufferInfo[0].buffer != VK_NULL_HANDLE);
}

angle::Result ProgramExecutableVk::createPipelineLayout(const gl::Context *glContext,
                                                        const gl::ProgramExecutable &glExecutable,
                                                        const gl::ProgramState &programState)
{
    const gl::State &glState                   = glContext->getState();
    ContextVk *contextVk                       = vk::GetImpl(glContext);
    RendererVk *renderer                       = contextVk->getRenderer();
    gl::TransformFeedback *transformFeedback   = glState.getCurrentTransformFeedback();
    const gl::ShaderBitSet &linkedShaderStages = glExecutable.getLinkedShaderStages();

    reset(contextVk);

    // Store a reference to the pipeline and descriptor set layouts. This will create them if they
    // don't already exist in the cache.

    // Default uniforms and transform feedback:
    vk::DescriptorSetLayoutDesc uniformsAndXfbSetDesc;
    for (const gl::ShaderType shaderType : linkedShaderStages)
    {
        const std::string uniformBlockName = kDefaultUniformNames[shaderType];
        ShaderInterfaceVariableInfo &info  = mVariableInfoMap[shaderType][uniformBlockName];
        if (!info.activeStages[shaderType])
        {
            continue;
        }

        uniformsAndXfbSetDesc.update(info.binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
                                     gl_vk::kShaderStageMap[shaderType]);
        mNumDefaultUniformDescriptors++;
    }
    bool hasVertexShader = glExecutable.hasLinkedShaderStage(gl::ShaderType::Vertex);
    bool hasXfbVaryings  = !programState.getLinkedTransformFeedbackVaryings().empty();
    if (hasVertexShader && transformFeedback && hasXfbVaryings)
    {
        size_t xfbBufferCount                    = programState.getTransformFeedbackBufferCount();
        TransformFeedbackVk *transformFeedbackVk = vk::GetImpl(transformFeedback);
        transformFeedbackVk->updateDescriptorSetLayout(contextVk,
                                                       mVariableInfoMap[gl::ShaderType::Vertex],
                                                       xfbBufferCount, &uniformsAndXfbSetDesc);
    }

    ANGLE_TRY(renderer->getDescriptorSetLayout(
        contextVk, uniformsAndXfbSetDesc,
        &mDescriptorSetLayouts[kUniformsAndXfbDescriptorSetIndex]));

    // Uniform and storage buffers, atomic counter buffers and images:
    vk::DescriptorSetLayoutDesc resourcesSetDesc;

    for (const gl::ShaderType shaderType : linkedShaderStages)
    {
        addInterfaceBlockDescriptorSetDesc(programState.getUniformBlocks(), shaderType,
                                           VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &resourcesSetDesc);
        addInterfaceBlockDescriptorSetDesc(programState.getShaderStorageBlocks(), shaderType,
                                           VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resourcesSetDesc);
        addAtomicCounterBufferDescriptorSetDesc(programState.getAtomicCounterBuffers(), shaderType,
                                                &resourcesSetDesc);
    }

    addImageDescriptorSetDesc(programState, &resourcesSetDesc);

    ANGLE_TRY(renderer->getDescriptorSetLayout(
        contextVk, resourcesSetDesc, &mDescriptorSetLayouts[kShaderResourceDescriptorSetIndex]));

    // Textures:
    vk::DescriptorSetLayoutDesc texturesSetDesc;

    addTextureDescriptorSetDesc(programState, contextVk->useOldRewriteStructSamplers(),
                                &texturesSetDesc);

    ANGLE_TRY(renderer->getDescriptorSetLayout(contextVk, texturesSetDesc,
                                               &mDescriptorSetLayouts[kTextureDescriptorSetIndex]));

    // Driver uniforms:
    VkShaderStageFlags driverUniformsStages =
        glExecutable.isCompute() ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_ALL_GRAPHICS;
    vk::DescriptorSetLayoutDesc driverUniformsSetDesc =
        contextVk->getDriverUniformsDescriptorSetDesc(driverUniformsStages);
    ANGLE_TRY(renderer->getDescriptorSetLayout(
        contextVk, driverUniformsSetDesc,
        &mDescriptorSetLayouts[kDriverUniformsDescriptorSetIndex]));

    // Create pipeline layout with these 4 descriptor sets.
    vk::PipelineLayoutDesc pipelineLayoutDesc;
    pipelineLayoutDesc.updateDescriptorSetLayout(kUniformsAndXfbDescriptorSetIndex,
                                                 uniformsAndXfbSetDesc);
    pipelineLayoutDesc.updateDescriptorSetLayout(kShaderResourceDescriptorSetIndex,
                                                 resourcesSetDesc);
    pipelineLayoutDesc.updateDescriptorSetLayout(kTextureDescriptorSetIndex, texturesSetDesc);
    pipelineLayoutDesc.updateDescriptorSetLayout(kDriverUniformsDescriptorSetIndex,
                                                 driverUniformsSetDesc);

    ANGLE_TRY(renderer->getPipelineLayout(contextVk, pipelineLayoutDesc, mDescriptorSetLayouts,
                                          &mPipelineLayout));

    // Initialize descriptor pools.
    std::array<VkDescriptorPoolSize, 2> uniformAndXfbSetSize = {
        {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
          static_cast<uint32_t>(mNumDefaultUniformDescriptors)},
         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS}}};

    uint32_t uniformBlockCount = static_cast<uint32_t>(programState.getUniformBlocks().size());
    uint32_t storageBlockCount =
        static_cast<uint32_t>(programState.getShaderStorageBlocks().size());
    uint32_t atomicCounterBufferCount =
        static_cast<uint32_t>(programState.getAtomicCounterBuffers().size());
    uint32_t imageCount   = static_cast<uint32_t>(programState.getImageBindings().size());
    uint32_t textureCount = static_cast<uint32_t>(programState.getSamplerBindings().size());

    if (renderer->getFeatures().bindEmptyForUnusedDescriptorSets.enabled)
    {
        // For this workaround, we have to create an empty descriptor set for each descriptor set
        // index, so make sure their pools are initialized.
        uniformBlockCount = std::max(uniformBlockCount, 1u);
        textureCount      = std::max(textureCount, 1u);
    }

    constexpr size_t kResourceTypesInResourcesSet = 3;
    angle::FixedVector<VkDescriptorPoolSize, kResourceTypesInResourcesSet> resourceSetSize;
    if (uniformBlockCount > 0)
    {
        resourceSetSize.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniformBlockCount);
    }
    if (storageBlockCount > 0 || atomicCounterBufferCount > 0)
    {
        // Note that we always use an array of IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS storage
        // buffers for emulating atomic counters, so if there are any atomic counter buffers, we
        // need to allocate IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS descriptors.
        const uint32_t atomicCounterStorageBufferCount =
            atomicCounterBufferCount > 0 ? gl::IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS : 0;
        const uint32_t storageBufferDescCount = storageBlockCount + atomicCounterStorageBufferCount;
        resourceSetSize.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, storageBufferDescCount);
    }
    if (imageCount > 0)
    {
        resourceSetSize.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, imageCount);
    }

    VkDescriptorPoolSize textureSetSize = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, textureCount};

    ANGLE_TRY(mDynamicDescriptorPools[kUniformsAndXfbDescriptorSetIndex].init(
        contextVk, uniformAndXfbSetSize.data(), uniformAndXfbSetSize.size()));
    if (resourceSetSize.size() > 0)
    {
        ANGLE_TRY(mDynamicDescriptorPools[kShaderResourceDescriptorSetIndex].init(
            contextVk, resourceSetSize.data(), static_cast<uint32_t>(resourceSetSize.size())));
    }
    if (textureCount > 0)
    {
        ANGLE_TRY(mDynamicDescriptorPools[kTextureDescriptorSetIndex].init(contextVk,
                                                                           &textureSetSize, 1));
    }

    mDynamicBufferOffsets.resize(glExecutable.getLinkedShaderStageCount());

    // Initialize an "empty" buffer for use with default uniform blocks where there are no uniforms,
    // or atomic counter buffer array indices that are unused.
    constexpr VkBufferUsageFlags kEmptyBufferUsage =
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    VkBufferCreateInfo emptyBufferInfo    = {};
    emptyBufferInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    emptyBufferInfo.flags                 = 0;
    emptyBufferInfo.size                  = 4;
    emptyBufferInfo.usage                 = kEmptyBufferUsage;
    emptyBufferInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    emptyBufferInfo.queueFamilyIndexCount = 0;
    emptyBufferInfo.pQueueFamilyIndices   = nullptr;

    constexpr VkMemoryPropertyFlags kMemoryType = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    return mEmptyBuffer.init(contextVk, emptyBufferInfo, kMemoryType);
}

void ProgramExecutableVk::updateDefaultUniformsDescriptorSet(
    const gl::ProgramState &programState,
    gl::ShaderMap<DefaultUniformBlock> &defaultUniformBlocks,
    ContextVk *contextVk)
{
    uint32_t shaderStageCount =
        static_cast<uint32_t>(programState.getProgramExecutable().getLinkedShaderStageCount());

    gl::ShaderVector<VkDescriptorBufferInfo> descriptorBufferInfo(shaderStageCount);
    gl::ShaderVector<VkWriteDescriptorSet> writeDescriptorInfo(shaderStageCount);

    mDescriptorBuffersCache.clear();

    // Write default uniforms for each shader type.
    uint32_t writeInfoCount = 0;
    for (const gl::ShaderType shaderType :
         programState.getProgramExecutable().getLinkedShaderStages())
    {
        const std::string uniformBlockName = kDefaultUniformNames[shaderType];
        ShaderInterfaceVariableInfo &info  = mVariableInfoMap[shaderType][uniformBlockName];
        if (!info.activeStages[shaderType])
        {
            return;
        }

        DefaultUniformBlock &uniformBlock  = defaultUniformBlocks[shaderType];
        VkDescriptorBufferInfo &bufferInfo = descriptorBufferInfo[writeInfoCount];
        VkWriteDescriptorSet &writeInfo    = writeDescriptorInfo[writeInfoCount];

        if (!uniformBlock.uniformData.empty())
        {
            vk::BufferHelper *bufferHelper = uniformBlock.storage.getCurrentBuffer();
            bufferInfo.buffer              = bufferHelper->getBuffer().getHandle();
            mDescriptorBuffersCache.emplace_back(bufferHelper);
        }
        else
        {
            mEmptyBuffer.retain(&contextVk->getResourceUseList());
            bufferInfo.buffer = mEmptyBuffer.getBuffer().getHandle();
            mDescriptorBuffersCache.emplace_back(&mEmptyBuffer);
        }

        bufferInfo.offset = 0;
        bufferInfo.range  = VK_WHOLE_SIZE;

        writeInfo.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeInfo.pNext            = nullptr;
        writeInfo.dstSet           = mDescriptorSets[kUniformsAndXfbDescriptorSetIndex];
        writeInfo.dstBinding       = info.binding;
        writeInfo.dstArrayElement  = 0;
        writeInfo.descriptorCount  = 1;
        writeInfo.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        writeInfo.pImageInfo       = nullptr;
        writeInfo.pBufferInfo      = &bufferInfo;
        writeInfo.pTexelBufferView = nullptr;

        ++writeInfoCount;
    }

    VkDevice device = contextVk->getDevice();

    ASSERT(writeInfoCount <= kReservedDefaultUniformBindingCount);

    vkUpdateDescriptorSets(device, writeInfoCount, writeDescriptorInfo.data(), 0, nullptr);
}

void ProgramExecutableVk::updateBuffersDescriptorSet(ContextVk *contextVk,
                                                     const gl::ShaderType shaderType,
                                                     vk::ResourceUseList *resourceUseList,
                                                     CommandBufferHelper *commandBufferHelper,
                                                     const std::vector<gl::InterfaceBlock> &blocks,
                                                     VkDescriptorType descriptorType)
{
    if (blocks.empty())
    {
        return;
    }

    VkDescriptorSet descriptorSet = mDescriptorSets[kShaderResourceDescriptorSetIndex];

    ASSERT(descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
           descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    const bool isStorageBuffer = descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    static_assert(
        gl::IMPLEMENTATION_MAX_SHADER_STORAGE_BUFFER_BINDINGS >=
            gl::IMPLEMENTATION_MAX_UNIFORM_BUFFER_BINDINGS,
        "The descriptor arrays here would have inadequate size for uniform buffer objects");

    gl::StorageBuffersArray<VkDescriptorBufferInfo> descriptorBufferInfo;
    gl::StorageBuffersArray<VkWriteDescriptorSet> writeDescriptorInfo;
    uint32_t writeCount = 0;

    // Write uniform or storage buffers.
    const gl::State &glState = contextVk->getState();
    for (uint32_t bufferIndex = 0; bufferIndex < blocks.size(); ++bufferIndex)
    {
        const gl::InterfaceBlock &block = blocks[bufferIndex];
        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding =
            isStorageBuffer ? glState.getIndexedShaderStorageBuffer(block.binding)
                            : glState.getIndexedUniformBuffer(block.binding);

        if (!block.isActive(shaderType))
        {
            continue;
        }

        if (bufferBinding.get() == nullptr)
        {
            continue;
        }

        ShaderInterfaceVariableInfo info = mVariableInfoMap[shaderType][block.mappedName];
        uint32_t binding                 = info.binding;
        uint32_t arrayElement            = block.isArray ? block.arrayElement : 0;
        VkDeviceSize maxBlockSize        = isStorageBuffer ? 0 : block.dataSize;

        VkDescriptorBufferInfo &bufferInfo = descriptorBufferInfo[writeCount];
        VkWriteDescriptorSet &writeInfo    = writeDescriptorInfo[writeCount];

        WriteBufferDescriptorSetBinding(bufferBinding, maxBlockSize, descriptorSet, descriptorType,
                                        binding, arrayElement, 0, &bufferInfo, &writeInfo);

        BufferVk *bufferVk             = vk::GetImpl(bufferBinding.get());
        vk::BufferHelper &bufferHelper = bufferVk->getBuffer();

        if (isStorageBuffer)
        {
            // We set the SHADER_READ_BIT to be conservative.
            VkAccessFlags accessFlags = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            commandBufferHelper->bufferWrite(resourceUseList, accessFlags, &bufferHelper);
        }
        else
        {
            commandBufferHelper->bufferRead(resourceUseList, VK_ACCESS_UNIFORM_READ_BIT,
                                            &bufferHelper);
        }

        ++writeCount;
    }

    VkDevice device = contextVk->getDevice();

    vkUpdateDescriptorSets(device, writeCount, writeDescriptorInfo.data(), 0, nullptr);
}

void ProgramExecutableVk::updateAtomicCounterBuffersDescriptorSet(
    const gl::ProgramState &programState,
    const gl::ShaderType shaderType,
    ContextVk *contextVk,
    vk::ResourceUseList *resourceUseList,
    CommandBufferHelper *commandBufferHelper)
{
    const gl::State &glState = contextVk->getState();
    const std::vector<gl::AtomicCounterBuffer> &atomicCounterBuffers =
        programState.getAtomicCounterBuffers();

    if (atomicCounterBuffers.empty())
    {
        return;
    }

    VkDescriptorSet descriptorSet = mDescriptorSets[kShaderResourceDescriptorSetIndex];

    std::string blockName(sh::vk::kAtomicCountersBlockName);
    const ShaderInterfaceVariableInfo &info = mVariableInfoMap[shaderType][blockName];

    if (!info.activeStages[shaderType])
    {
        return;
    }

    gl::AtomicCounterBuffersArray<VkDescriptorBufferInfo> descriptorBufferInfo;
    gl::AtomicCounterBuffersArray<VkWriteDescriptorSet> writeDescriptorInfo;
    gl::AtomicCounterBufferMask writtenBindings;

    RendererVk *rendererVk = contextVk->getRenderer();
    const VkDeviceSize requiredOffsetAlignment =
        rendererVk->getPhysicalDeviceProperties().limits.minStorageBufferOffsetAlignment;

    // Write atomic counter buffers.
    for (uint32_t bufferIndex = 0; bufferIndex < atomicCounterBuffers.size(); ++bufferIndex)
    {
        const gl::AtomicCounterBuffer &atomicCounterBuffer = atomicCounterBuffers[bufferIndex];
        uint32_t binding                                   = atomicCounterBuffer.binding;
        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding =
            glState.getIndexedAtomicCounterBuffer(binding);

        if (bufferBinding.get() == nullptr)
        {
            continue;
        }

        VkDescriptorBufferInfo &bufferInfo = descriptorBufferInfo[binding];
        VkWriteDescriptorSet &writeInfo    = writeDescriptorInfo[binding];

        WriteBufferDescriptorSetBinding(bufferBinding, 0, descriptorSet,
                                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, info.binding, binding,
                                        requiredOffsetAlignment, &bufferInfo, &writeInfo);

        BufferVk *bufferVk             = vk::GetImpl(bufferBinding.get());
        vk::BufferHelper &bufferHelper = bufferVk->getBuffer();

        // We set SHADER_READ_BIT to be conservative.
        commandBufferHelper->bufferWrite(
            resourceUseList, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, &bufferHelper);

        writtenBindings.set(binding);
    }

    // Bind the empty buffer to every array slot that's unused.
    mEmptyBuffer.retain(&contextVk->getResourceUseList());
    for (size_t binding : ~writtenBindings)
    {
        VkDescriptorBufferInfo &bufferInfo = descriptorBufferInfo[binding];
        VkWriteDescriptorSet &writeInfo    = writeDescriptorInfo[binding];

        bufferInfo.buffer = mEmptyBuffer.getBuffer().getHandle();
        bufferInfo.offset = 0;
        bufferInfo.range  = VK_WHOLE_SIZE;

        writeInfo.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeInfo.pNext            = nullptr;
        writeInfo.dstSet           = descriptorSet;
        writeInfo.dstBinding       = info.binding;
        writeInfo.dstArrayElement  = static_cast<uint32_t>(binding);
        writeInfo.descriptorCount  = 1;
        writeInfo.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeInfo.pImageInfo       = nullptr;
        writeInfo.pBufferInfo      = &bufferInfo;
        writeInfo.pTexelBufferView = nullptr;
    }

    VkDevice device = contextVk->getDevice();

    vkUpdateDescriptorSets(device, gl::IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS,
                           writeDescriptorInfo.data(), 0, nullptr);
}

angle::Result ProgramExecutableVk::updateImagesDescriptorSet(const gl::ProgramState &programState,
                                                             const gl::ShaderType shaderType,
                                                             ContextVk *contextVk)
{
    const gl::State &glState                           = contextVk->getState();
    const std::vector<gl::ImageBinding> &imageBindings = programState.getImageBindings();
    const std::vector<gl::LinkedUniform> &uniforms     = programState.getUniforms();

    if (imageBindings.empty())
    {
        return angle::Result::Continue;
    }

    VkDescriptorSet descriptorSet = mDescriptorSets[kShaderResourceDescriptorSetIndex];

    const gl::ActiveTextureArray<TextureVk *> &activeImages = contextVk->getActiveImages();

    gl::ImagesArray<VkDescriptorImageInfo> descriptorImageInfo;
    gl::ImagesArray<VkWriteDescriptorSet> writeDescriptorInfo;
    uint32_t writeCount = 0;

    // Write images.
    for (uint32_t imageIndex = 0; imageIndex < imageBindings.size(); ++imageIndex)
    {
        const gl::ImageBinding &imageBinding = imageBindings[imageIndex];
        uint32_t uniformIndex = programState.getUniformIndexFromImageIndex(imageIndex);
        const gl::LinkedUniform &imageUniform = uniforms[uniformIndex];

        if (!imageUniform.isActive(shaderType))
        {
            continue;
        }

        std::string name = imageUniform.mappedName;
        GetImageNameWithoutIndices(&name);
        ShaderInterfaceVariableInfo &info = mVariableInfoMap[shaderType][name];

        ASSERT(!imageBinding.unreferenced);

        for (uint32_t arrayElement = 0; arrayElement < imageBinding.boundImageUnits.size();
             ++arrayElement)
        {
            GLuint imageUnit             = imageBinding.boundImageUnits[arrayElement];
            const gl::ImageUnit &binding = glState.getImageUnit(imageUnit);
            TextureVk *textureVk         = activeImages[imageUnit];

            vk::ImageHelper *image         = &textureVk->getImage();
            const vk::ImageView *imageView = nullptr;

            ANGLE_TRY(textureVk->getStorageImageView(contextVk, (binding.layered == GL_TRUE),
                                                     binding.level, binding.layer, &imageView));

            // Note: binding.access is unused because it is implied by the shader.

            // TODO(syoussefi): Support image data reinterpretation by using binding.format.
            // http://anglebug.com/3563

            VkDescriptorImageInfo &imageInfo = descriptorImageInfo[writeCount];
            VkWriteDescriptorSet &writeInfo  = writeDescriptorInfo[writeCount];

            imageInfo.sampler     = VK_NULL_HANDLE;
            imageInfo.imageView   = imageView->getHandle();
            imageInfo.imageLayout = image->getCurrentLayout();

            writeInfo.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeInfo.pNext            = nullptr;
            writeInfo.dstSet           = descriptorSet;
            writeInfo.dstBinding       = info.binding;
            writeInfo.dstArrayElement  = arrayElement;
            writeInfo.descriptorCount  = 1;
            writeInfo.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writeInfo.pImageInfo       = &imageInfo;
            writeInfo.pBufferInfo      = nullptr;
            writeInfo.pTexelBufferView = nullptr;

            ++writeCount;
        }
    }

    VkDevice device = contextVk->getDevice();

    vkUpdateDescriptorSets(device, writeCount, writeDescriptorInfo.data(), 0, nullptr);

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::updateShaderResourcesDescriptorSet(
    const gl::ProgramState &programState,
    ContextVk *contextVk,
    vk::ResourceUseList *resourceUseList,
    CommandBufferHelper *commandBufferHelper)
{
    const gl::ProgramExecutable *executable = contextVk->getState().getProgramExecutable();
    ASSERT(executable);

    ANGLE_TRY(allocateDescriptorSet(contextVk, kShaderResourceDescriptorSetIndex));

    for (const gl::ShaderType shaderType : executable->getLinkedShaderStages())
    {
        updateBuffersDescriptorSet(contextVk, shaderType, resourceUseList, commandBufferHelper,
                                   programState.getUniformBlocks(),
                                   VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        updateBuffersDescriptorSet(contextVk, shaderType, resourceUseList, commandBufferHelper,
                                   programState.getShaderStorageBlocks(),
                                   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        updateAtomicCounterBuffersDescriptorSet(programState, shaderType, contextVk,
                                                resourceUseList, commandBufferHelper);
        angle::Result status = updateImagesDescriptorSet(programState, shaderType, contextVk);
        if (status != angle::Result::Continue)
        {
            return status;
        }
    }

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::updateTransformFeedbackDescriptorSet(
    const gl::ProgramState &programState,
    gl::ShaderMap<DefaultUniformBlock> &defaultUniformBlocks,
    ContextVk *contextVk)
{
    const gl::State &glState                = contextVk->getState();
    const gl::ProgramExecutable &executable = programState.getProgramExecutable();
    ASSERT(executable.hasTransformFeedbackOutput(glState));

    ANGLE_TRY(allocateDescriptorSet(contextVk, kUniformsAndXfbDescriptorSetIndex));

    updateDefaultUniformsDescriptorSet(programState, defaultUniformBlocks, contextVk);
    updateTransformFeedbackDescriptorSetImpl(programState, contextVk);

    return angle::Result::Continue;
}

void ProgramExecutableVk::updateTransformFeedbackDescriptorSetImpl(
    const gl::ProgramState &programState,
    ContextVk *contextVk)
{
    const gl::State &glState                 = contextVk->getState();
    gl::TransformFeedback *transformFeedback = glState.getCurrentTransformFeedback();
    const gl::ProgramExecutable &executable  = programState.getProgramExecutable();

    if (!executable.hasTransformFeedbackOutput(glState))
    {
        // If xfb has no output there is no need to update descriptor set.
        return;
    }
    if (!glState.isTransformFeedbackActive())
    {
        // We set empty Buffer to xfb descriptor set because xfb descriptor set
        // requires valid buffer bindings, even if they are empty buffer,
        // otherwise Vulkan validation layer generates errors.
        if (transformFeedback)
        {
            TransformFeedbackVk *transformFeedbackVk = vk::GetImpl(transformFeedback);
            transformFeedbackVk->initDescriptorSet(
                contextVk, programState.getTransformFeedbackBufferCount(), &mEmptyBuffer,
                mDescriptorSets[kUniformsAndXfbDescriptorSetIndex]);
        }
        return;
    }

    TransformFeedbackVk *transformFeedbackVk = vk::GetImpl(glState.getCurrentTransformFeedback());
    transformFeedbackVk->updateDescriptorSet(contextVk, programState,
                                             mDescriptorSets[kUniformsAndXfbDescriptorSetIndex]);
}

angle::Result ProgramExecutableVk::updateTexturesDescriptorSet(const gl::ProgramState &programState,
                                                               ContextVk *contextVk)
{
    const gl::State &glState                      = contextVk->getState();
    const vk::TextureDescriptorDesc &texturesDesc = contextVk->getActiveTexturesDesc();
    const gl::ProgramExecutable &executable       = programState.getProgramExecutable();

    auto iter = mTextureDescriptorsCache.find(texturesDesc);
    if (iter != mTextureDescriptorsCache.end())
    {
        mDescriptorSets[kTextureDescriptorSetIndex] = iter->second;
        return angle::Result::Continue;
    }

    ASSERT(executable.hasTextures(glState));
    bool newPoolAllocated;
    ANGLE_TRY(
        allocateDescriptorSetAndGetInfo(contextVk, kTextureDescriptorSetIndex, &newPoolAllocated));

    // Clear descriptor set cache. It may no longer be valid.
    if (newPoolAllocated)
    {
        mTextureDescriptorsCache.clear();
    }

    VkDescriptorSet descriptorSet = mDescriptorSets[kTextureDescriptorSetIndex];

    gl::ActiveTextureArray<VkDescriptorImageInfo> descriptorImageInfo;
    gl::ActiveTextureArray<VkWriteDescriptorSet> writeDescriptorInfo;
    uint32_t writeCount = 0;

    const gl::ActiveTextureArray<vk::TextureUnit> &activeTextures = contextVk->getActiveTextures();

    bool emulateSeamfulCubeMapSampling = contextVk->emulateSeamfulCubeMapSampling();
    bool useOldRewriteStructSamplers   = contextVk->useOldRewriteStructSamplers();

    for (const gl::ShaderType shaderType : executable.getLinkedShaderStages())
    {
        std::unordered_map<std::string, uint32_t> mappedSamplerNameToArrayOffset;
        for (uint32_t textureIndex = 0; textureIndex < programState.getSamplerBindings().size();
             ++textureIndex)
        {
            const gl::SamplerBinding &samplerBinding =
                programState.getSamplerBindings()[textureIndex];

            ASSERT(!samplerBinding.unreferenced);

            uint32_t uniformIndex = programState.getUniformIndexFromSamplerIndex(textureIndex);
            const gl::LinkedUniform &samplerUniform = programState.getUniforms()[uniformIndex];
            std::string mappedSamplerName = GlslangGetMappedSamplerName(samplerUniform.name);

            if (!samplerUniform.isActive(shaderType))
            {
                continue;
            }

            uint32_t arrayOffset = 0;
            uint32_t arraySize   = static_cast<uint32_t>(samplerBinding.boundTextureUnits.size());

            if (!useOldRewriteStructSamplers)
            {
                arrayOffset = mappedSamplerNameToArrayOffset[mappedSamplerName];
                // Front-end generates array elements in order, so we can just increment
                // the offset each time we process a nested array.
                mappedSamplerNameToArrayOffset[mappedSamplerName] += arraySize;
            }

            for (uint32_t arrayElement = 0; arrayElement < arraySize; ++arrayElement)
            {
                GLuint textureUnit   = samplerBinding.boundTextureUnits[arrayElement];
                TextureVk *textureVk = activeTextures[textureUnit].texture;
                SamplerVk *samplerVk = activeTextures[textureUnit].sampler;

                vk::ImageHelper &image = textureVk->getImage();

                VkDescriptorImageInfo &imageInfo = descriptorImageInfo[writeCount];

                // Use bound sampler object if one present, otherwise use texture's sampler
                const vk::Sampler &sampler =
                    (samplerVk != nullptr) ? samplerVk->getSampler() : textureVk->getSampler();

                imageInfo.sampler     = sampler.getHandle();
                imageInfo.imageLayout = image.getCurrentLayout();

                if (emulateSeamfulCubeMapSampling)
                {
                    // If emulating seamful cubemapping, use the fetch image view.  This is
                    // basically the same image view as read, except it's a 2DArray view for
                    // cube maps.
                    imageInfo.imageView =
                        textureVk->getFetchImageViewAndRecordUse(contextVk).getHandle();
                }
                else
                {
                    imageInfo.imageView =
                        textureVk->getReadImageViewAndRecordUse(contextVk).getHandle();
                }

                ShaderInterfaceVariableInfoMap &variableInfoMap = mVariableInfoMap[shaderType];
                const std::string samplerName =
                    contextVk->getRenderer()->getFeatures().forceOldRewriteStructSamplers.enabled
                        ? GetMappedSamplerNameOld(samplerUniform.name)
                        : GlslangGetMappedSamplerName(samplerUniform.name);
                ShaderInterfaceVariableInfo &info = variableInfoMap[samplerName];

                VkWriteDescriptorSet &writeInfo = writeDescriptorInfo[writeCount];

                writeInfo.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeInfo.pNext            = nullptr;
                writeInfo.dstSet           = descriptorSet;
                writeInfo.dstBinding       = info.binding;
                writeInfo.dstArrayElement  = arrayOffset + arrayElement;
                writeInfo.descriptorCount  = 1;
                writeInfo.descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                writeInfo.pImageInfo       = &imageInfo;
                writeInfo.pBufferInfo      = nullptr;
                writeInfo.pTexelBufferView = nullptr;

                ++writeCount;
            }
        }
    }

    VkDevice device = contextVk->getDevice();

    ASSERT(writeCount > 0);

    vkUpdateDescriptorSets(device, writeCount, writeDescriptorInfo.data(), 0, nullptr);

    mTextureDescriptorsCache.emplace(texturesDesc, descriptorSet);

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::updateDescriptorSets(ContextVk *contextVk,
                                                        vk::CommandBuffer *commandBuffer)
{
    // Can probably use better dirty bits here.

    if (mDescriptorSets.empty())
        return angle::Result::Continue;

    // Find the maximum non-null descriptor set.  This is used in conjunction with a driver
    // workaround to bind empty descriptor sets only for gaps in between 0 and max and avoid
    // binding unnecessary empty descriptor sets for the sets beyond max.
    const size_t descriptorSetStart = kUniformsAndXfbDescriptorSetIndex;
    size_t descriptorSetRange       = 0;
    for (size_t descriptorSetIndex = descriptorSetStart;
         descriptorSetIndex < mDescriptorSets.size(); ++descriptorSetIndex)
    {
        if (mDescriptorSets[descriptorSetIndex] != VK_NULL_HANDLE)
        {
            descriptorSetRange = descriptorSetIndex + 1;
        }
    }

    const gl::State &glState                    = contextVk->getState();
    const VkPipelineBindPoint pipelineBindPoint = glState.getProgramExecutable()->isCompute()
                                                      ? VK_PIPELINE_BIND_POINT_COMPUTE
                                                      : VK_PIPELINE_BIND_POINT_GRAPHICS;

    for (uint32_t descriptorSetIndex = descriptorSetStart; descriptorSetIndex < descriptorSetRange;
         ++descriptorSetIndex)
    {
        VkDescriptorSet descSet = mDescriptorSets[descriptorSetIndex];
        if (descSet == VK_NULL_HANDLE)
        {
            if (!contextVk->getRenderer()->getFeatures().bindEmptyForUnusedDescriptorSets.enabled)
            {
                continue;
            }

            // Workaround a driver bug where missing (though unused) descriptor sets indices cause
            // later sets to misbehave.
            if (mEmptyDescriptorSets[descriptorSetIndex] == VK_NULL_HANDLE)
            {
                const vk::DescriptorSetLayout &descriptorSetLayout =
                    mDescriptorSetLayouts[descriptorSetIndex].get();

                ANGLE_TRY(mDynamicDescriptorPools[descriptorSetIndex].allocateSets(
                    contextVk, descriptorSetLayout.ptr(), 1,
                    &mDescriptorPoolBindings[descriptorSetIndex],
                    &mEmptyDescriptorSets[descriptorSetIndex]));
            }
            descSet = mEmptyDescriptorSets[descriptorSetIndex];
        }

        // Default uniforms are encompassed in a block per shader stage, and they are assigned
        // through dynamic uniform buffers (requiring dynamic offsets).  No other descriptor
        // requires a dynamic offset.
        const uint32_t uniformBlockOffsetCount =
            descriptorSetIndex == kUniformsAndXfbDescriptorSetIndex
                ? static_cast<uint32_t>(mNumDefaultUniformDescriptors)
                : 0;

        commandBuffer->bindDescriptorSets(getPipelineLayout(), pipelineBindPoint,
                                          descriptorSetIndex, 1, &descSet, uniformBlockOffsetCount,
                                          mDynamicBufferOffsets.data());
    }

    for (vk::BufferHelper *buffer : mDescriptorBuffersCache)
    {
        buffer->retain(&contextVk->getResourceUseList());
    }

    return angle::Result::Continue;
}

}  // namespace rx
