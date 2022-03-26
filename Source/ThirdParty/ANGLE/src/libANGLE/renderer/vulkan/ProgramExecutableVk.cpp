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
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/GlslangWrapperVk.h"
#include "libANGLE/renderer/vulkan/ProgramPipelineVk.h"
#include "libANGLE/renderer/vulkan/ProgramVk.h"
#include "libANGLE/renderer/vulkan/TextureVk.h"
#include "libANGLE/renderer/vulkan/TransformFeedbackVk.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

namespace rx
{
namespace
{
void LoadShaderInterfaceVariableXfbInfo(gl::BinaryInputStream *stream,
                                        ShaderInterfaceVariableXfbInfo *xfb)
{
    xfb->buffer        = stream->readInt<uint32_t>();
    xfb->offset        = stream->readInt<uint32_t>();
    xfb->stride        = stream->readInt<uint32_t>();
    xfb->arraySize     = stream->readInt<uint32_t>();
    xfb->columnCount   = stream->readInt<uint32_t>();
    xfb->rowCount      = stream->readInt<uint32_t>();
    xfb->arrayIndex    = stream->readInt<uint32_t>();
    xfb->componentType = stream->readInt<uint32_t>();
    xfb->arrayElements.resize(stream->readInt<size_t>());
    for (ShaderInterfaceVariableXfbInfo &arrayElement : xfb->arrayElements)
    {
        LoadShaderInterfaceVariableXfbInfo(stream, &arrayElement);
    }
}

void SaveShaderInterfaceVariableXfbInfo(const ShaderInterfaceVariableXfbInfo &xfb,
                                        gl::BinaryOutputStream *stream)
{
    stream->writeInt(xfb.buffer);
    stream->writeInt(xfb.offset);
    stream->writeInt(xfb.stride);
    stream->writeInt(xfb.arraySize);
    stream->writeInt(xfb.columnCount);
    stream->writeInt(xfb.rowCount);
    stream->writeInt(xfb.arrayIndex);
    stream->writeInt(xfb.componentType);
    stream->writeInt(xfb.arrayElements.size());
    for (const ShaderInterfaceVariableXfbInfo &arrayElement : xfb.arrayElements)
    {
        SaveShaderInterfaceVariableXfbInfo(arrayElement, stream);
    }
}

bool ValidateTransformedSpirV(const gl::ShaderBitSet &linkedShaderStages,
                              const ShaderInterfaceVariableInfoMap &variableInfoMap,
                              const gl::ShaderMap<angle::spirv::Blob> &spirvBlobs)
{
    gl::ShaderType lastPreFragmentStage = gl::GetLastPreFragmentStage(linkedShaderStages);

    for (gl::ShaderType shaderType : linkedShaderStages)
    {
        GlslangSpirvOptions options;
        options.shaderType                         = shaderType;
        options.preRotation                        = SurfaceRotation::FlippedRotated90Degrees;
        options.negativeViewportSupported          = false;
        options.transformPositionToVulkanClipSpace = true;
        options.removeDebugInfo                    = true;
        options.isTransformFeedbackStage           = shaderType == lastPreFragmentStage;

        angle::spirv::Blob transformed;
        if (GlslangWrapperVk::TransformSpirV(options, variableInfoMap, spirvBlobs[shaderType],
                                             &transformed) != angle::Result::Continue)
        {
            return false;
        }
    }
    return true;
}

constexpr bool IsDynamicDescriptor(VkDescriptorType descriptorType)
{
    switch (descriptorType)
    {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            return true;
        default:
            return false;
    }
}

constexpr VkDescriptorType kStorageBufferDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

DescriptorSetIndex CacheTypeToDescriptorSetIndex(VulkanCacheType cacheType)
{
    switch (cacheType)
    {
        case VulkanCacheType::TextureDescriptors:
            return DescriptorSetIndex::Texture;
        case VulkanCacheType::ShaderBuffersDescriptors:
            return DescriptorSetIndex::ShaderResource;
        case VulkanCacheType::UniformsAndXfbDescriptors:
            return DescriptorSetIndex::UniformsAndXfb;
        case VulkanCacheType::DriverUniformsDescriptors:
            return DescriptorSetIndex::Internal;
        default:
            UNREACHABLE();
            return DescriptorSetIndex::InvalidEnum;
    }
}
}  // namespace

DefaultUniformBlock::DefaultUniformBlock() = default;

DefaultUniformBlock::~DefaultUniformBlock() = default;

// ShaderInfo implementation.
ShaderInfo::ShaderInfo() {}

ShaderInfo::~ShaderInfo() = default;

angle::Result ShaderInfo::initShaders(const gl::ShaderBitSet &linkedShaderStages,
                                      const gl::ShaderMap<const angle::spirv::Blob *> &spirvBlobs,
                                      const ShaderInterfaceVariableInfoMap &variableInfoMap)
{
    clear();

    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        if (spirvBlobs[shaderType] != nullptr)
        {
            mSpirvBlobs[shaderType] = *spirvBlobs[shaderType];
        }
    }

    // Assert that SPIR-V transformation is correct, even if the test never issues a draw call.
    ASSERT(ValidateTransformedSpirV(linkedShaderStages, variableInfoMap, mSpirvBlobs));

    mIsInitialized = true;
    return angle::Result::Continue;
}

void ShaderInfo::initShaderFromProgram(gl::ShaderType shaderType,
                                       const ShaderInfo &programShaderInfo)
{
    mSpirvBlobs[shaderType] = programShaderInfo.mSpirvBlobs[shaderType];
    mIsInitialized          = true;
}

void ShaderInfo::clear()
{
    for (angle::spirv::Blob &spirvBlob : mSpirvBlobs)
    {
        spirvBlob.clear();
    }
    mIsInitialized = false;
}

void ShaderInfo::load(gl::BinaryInputStream *stream)
{
    clear();

    // Read in shader codes for all shader types
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        angle::spirv::Blob *spirvBlob = &mSpirvBlobs[shaderType];

        // Read the SPIR-V
        stream->readIntVector<uint32_t>(spirvBlob);
    }

    mIsInitialized = true;
}

void ShaderInfo::save(gl::BinaryOutputStream *stream)
{
    ASSERT(valid());

    // Write out shader codes for all shader types
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const angle::spirv::Blob &spirvBlob = mSpirvBlobs[shaderType];

        // Write the SPIR-V
        stream->writeIntVector(spirvBlob);
    }
}

// ProgramInfo implementation.
ProgramInfo::ProgramInfo() {}

ProgramInfo::~ProgramInfo() = default;

angle::Result ProgramInfo::initProgram(ContextVk *contextVk,
                                       gl::ShaderType shaderType,
                                       bool isLastPreFragmentStage,
                                       bool isTransformFeedbackProgram,
                                       const ShaderInfo &shaderInfo,
                                       ProgramTransformOptions optionBits,
                                       const ShaderInterfaceVariableInfoMap &variableInfoMap)
{
    const gl::ShaderMap<angle::spirv::Blob> &originalSpirvBlobs = shaderInfo.getSpirvBlobs();
    const angle::spirv::Blob &originalSpirvBlob                 = originalSpirvBlobs[shaderType];
    gl::ShaderMap<angle::spirv::Blob> transformedSpirvBlobs;
    angle::spirv::Blob &transformedSpirvBlob = transformedSpirvBlobs[shaderType];

    GlslangSpirvOptions options;
    options.shaderType = shaderType;
    options.removeEarlyFragmentTestsOptimization =
        shaderType == gl::ShaderType::Fragment && optionBits.removeEarlyFragmentTestsOptimization;
    options.removeDebugInfo          = !contextVk->getFeatures().retainSpirvDebugInfo.enabled;
    options.isTransformFeedbackStage = isLastPreFragmentStage && isTransformFeedbackProgram &&
                                       !optionBits.removeTransformFeedbackEmulation;
    options.isTransformFeedbackEmulated = contextVk->getFeatures().emulateTransformFeedback.enabled;
    options.negativeViewportSupported   = contextVk->getFeatures().supportsNegativeViewport.enabled;

    if (isLastPreFragmentStage)
    {
        options.preRotation = static_cast<SurfaceRotation>(optionBits.surfaceRotation);
        options.transformPositionToVulkanClipSpace =
            optionBits.enableDepthCorrection &&
            !contextVk->getFeatures().supportsDepthClipControl.enabled;
    }

    ANGLE_TRY(GlslangWrapperVk::TransformSpirV(options, variableInfoMap, originalSpirvBlob,
                                               &transformedSpirvBlob));
    ANGLE_TRY(vk::InitShaderAndSerial(contextVk, &mShaders[shaderType].get(),
                                      transformedSpirvBlob.data(),
                                      transformedSpirvBlob.size() * sizeof(uint32_t)));

    mProgramHelper.setShader(shaderType, &mShaders[shaderType]);

    mProgramHelper.setSpecializationConstant(sh::vk::SpecializationConstantId::LineRasterEmulation,
                                             optionBits.enableLineRasterEmulation);
    mProgramHelper.setSpecializationConstant(sh::vk::SpecializationConstantId::SurfaceRotation,
                                             optionBits.surfaceRotation);

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
    : mEmptyDescriptorSets{},
      mNumDefaultUniformDescriptors(0),
      mImmutableSamplersMaxDescriptorCount(1),
      mUniformBufferDescriptorType(VK_DESCRIPTOR_TYPE_MAX_ENUM),
      mDynamicUniformDescriptorOffsets{},
      mPerfCounters{},
      mCumulativePerfCounters{}
{
    for (std::shared_ptr<DefaultUniformBlock> &defaultBlock : mDefaultUniformBlocks)
    {
        defaultBlock = std::make_shared<DefaultUniformBlock>();
    }
}

ProgramExecutableVk::~ProgramExecutableVk()
{
    outputCumulativePerfCounters();
}

void ProgramExecutableVk::reset(ContextVk *contextVk)
{
    for (auto &descriptorSetLayout : mDescriptorSetLayouts)
    {
        descriptorSetLayout.reset();
    }
    mImmutableSamplersMaxDescriptorCount = 1;
    mImmutableSamplerIndexMap.clear();
    mPipelineLayout.reset();

    mDescriptorSets.fill(VK_NULL_HANDLE);
    mEmptyDescriptorSets.fill(VK_NULL_HANDLE);
    mNumDefaultUniformDescriptors = 0;
    mTransformOptions             = {};

    for (vk::RefCountedDescriptorPoolBinding &binding : mDescriptorPoolBindings)
    {
        binding.reset();
    }

    for (vk::DynamicDescriptorPool &descriptorPool : mDynamicDescriptorPools)
    {
        descriptorPool.release(contextVk);
    }

    RendererVk *rendererVk = contextVk->getRenderer();
    mTextureDescriptorsCache.destroy(rendererVk, VulkanCacheType::TextureDescriptors);
    mUniformsAndXfbDescriptorsCache.destroy(rendererVk, VulkanCacheType::UniformsAndXfbDescriptors);
    mShaderBufferDescriptorsCache.destroy(rendererVk, VulkanCacheType::ShaderBuffersDescriptors);

    // Initialize with an invalid BufferSerial
    mCurrentDefaultUniformBufferSerial = vk::BufferSerial();

    for (ProgramInfo &programInfo : mGraphicsProgramInfos)
    {
        programInfo.release(contextVk);
    }
    mComputeProgramInfo.release(contextVk);

    contextVk->onProgramExecutableReset(this);
}

std::unique_ptr<rx::LinkEvent> ProgramExecutableVk::load(ContextVk *contextVk,
                                                         const gl::ProgramExecutable &glExecutable,
                                                         gl::BinaryInputStream *stream)
{
    clearVariableInfoMap();

    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        size_t variableInfoMapSize = stream->readInt<size_t>();

        for (size_t i = 0; i < variableInfoMapSize; ++i)
        {
            const std::string variableName    = stream->readString();
            ShaderInterfaceVariableInfo &info = mVariableInfoMap.add(shaderType, variableName);

            info.descriptorSet = stream->readInt<uint32_t>();
            info.binding       = stream->readInt<uint32_t>();
            info.location      = stream->readInt<uint32_t>();
            info.component     = stream->readInt<uint32_t>();
            info.index         = stream->readInt<uint32_t>();
            // PackedEnumBitSet uses uint8_t
            info.activeStages = gl::ShaderBitSet(stream->readInt<uint8_t>());
            LoadShaderInterfaceVariableXfbInfo(stream, &info.xfb);
            info.fieldXfb.resize(stream->readInt<size_t>());
            for (ShaderInterfaceVariableXfbInfo &xfb : info.fieldXfb)
            {
                LoadShaderInterfaceVariableXfbInfo(stream, &xfb);
            }
            info.useRelaxedPrecision     = stream->readBool();
            info.varyingIsInput          = stream->readBool();
            info.varyingIsOutput         = stream->readBool();
            info.attributeComponentCount = stream->readInt<uint8_t>();
            info.attributeLocationCount  = stream->readInt<uint8_t>();
            info.isDuplicate             = stream->readBool();
        }
    }

    mOriginalShaderInfo.load(stream);

    // Deserializes the uniformLayout data of mDefaultUniformBlocks
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const size_t uniformCount = stream->readInt<size_t>();
        for (unsigned int uniformIndex = 0; uniformIndex < uniformCount; ++uniformIndex)
        {
            sh::BlockMemberInfo blockInfo;
            gl::LoadBlockMemberInfo(stream, &blockInfo);
            mDefaultUniformBlocks[shaderType]->uniformLayout.push_back(blockInfo);
        }
    }

    gl::ShaderMap<size_t> requiredBufferSize;
    requiredBufferSize.fill(0);
    // Deserializes required uniform block memory sizes
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        requiredBufferSize[shaderType] = stream->readInt<size_t>();
    }

    // Initialize and resize the mDefaultUniformBlocks' memory
    angle::Result status = resizeUniformBlockMemory(contextVk, glExecutable, requiredBufferSize);
    if (status != angle::Result::Continue)
    {
        return std::make_unique<LinkEventDone>(status);
    }

    status = createPipelineLayout(contextVk, glExecutable, nullptr);
    return std::make_unique<LinkEventDone>(status);
}

void ProgramExecutableVk::save(gl::BinaryOutputStream *stream)
{
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        stream->writeInt(mVariableInfoMap.variableCount(shaderType));
        for (const auto &it : mVariableInfoMap.getIterator(shaderType))
        {
            const std::string &name                 = it.first;
            const ShaderInterfaceVariableInfo &info = it.second;

            stream->writeString(name);
            stream->writeInt(info.descriptorSet);
            stream->writeInt(info.binding);
            stream->writeInt(info.location);
            stream->writeInt(info.component);
            stream->writeInt(info.index);
            // PackedEnumBitSet uses uint8_t
            stream->writeInt(info.activeStages.bits());
            SaveShaderInterfaceVariableXfbInfo(info.xfb, stream);
            stream->writeInt(info.fieldXfb.size());
            for (const ShaderInterfaceVariableXfbInfo &xfb : info.fieldXfb)
            {
                SaveShaderInterfaceVariableXfbInfo(xfb, stream);
            }
            stream->writeBool(info.useRelaxedPrecision);
            stream->writeBool(info.varyingIsInput);
            stream->writeBool(info.varyingIsOutput);
            stream->writeInt(info.attributeComponentCount);
            stream->writeInt(info.attributeLocationCount);
            stream->writeBool(info.isDuplicate);
        }
    }

    mOriginalShaderInfo.save(stream);

    // Serializes the uniformLayout data of mDefaultUniformBlocks
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const size_t uniformCount = mDefaultUniformBlocks[shaderType]->uniformLayout.size();
        stream->writeInt(uniformCount);
        for (unsigned int uniformIndex = 0; uniformIndex < uniformCount; ++uniformIndex)
        {
            sh::BlockMemberInfo &blockInfo =
                mDefaultUniformBlocks[shaderType]->uniformLayout[uniformIndex];
            gl::WriteBlockMemberInfo(stream, blockInfo);
        }
    }

    // Serializes required uniform block memory sizes
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        stream->writeInt(mDefaultUniformBlocks[shaderType]->uniformData.size());
    }
}

void ProgramExecutableVk::clearVariableInfoMap()
{
    mVariableInfoMap.clear();
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

angle::Result ProgramExecutableVk::allocUniformAndXfbDescriptorSet(
    vk::Context *context,
    vk::ResourceUseList *resourceUseList,
    vk::BufferHelper *defaultUniformBuffer,
    const vk::DescriptorSetDesc &xfbBufferDesc,
    bool *newDescriptorSetAllocated)
{
    mCurrentDefaultUniformBufferSerial =
        defaultUniformBuffer ? defaultUniformBuffer->getBufferSerial() : vk::kInvalidBufferSerial;

    // Look up in the cache first
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    if (mUniformsAndXfbDescriptorsCache.get(xfbBufferDesc, &descriptorSet))
    {
        *newDescriptorSetAllocated                          = false;
        mDescriptorSets[DescriptorSetIndex::UniformsAndXfb] = descriptorSet;
        // The descriptor pool that this descriptor set was allocated from needs to be retained each
        // time the descriptor set is used in a new command.
        mDescriptorPoolBindings[DescriptorSetIndex::UniformsAndXfb].get().retain(resourceUseList);
        return angle::Result::Continue;
    }

    bool newPoolAllocated;
    ANGLE_TRY(allocateDescriptorSetAndGetInfo(
        context, resourceUseList, DescriptorSetIndex::UniformsAndXfb, &newPoolAllocated));

    // Clear descriptor set cache. It may no longer be valid.
    if (newPoolAllocated)
    {
        mUniformsAndXfbDescriptorsCache.destroy(context->getRenderer(),
                                                VulkanCacheType::UniformsAndXfbDescriptors);
    }

    // Add the descriptor set into cache
    mUniformsAndXfbDescriptorsCache.insert(xfbBufferDesc,
                                           mDescriptorSets[DescriptorSetIndex::UniformsAndXfb]);
    *newDescriptorSetAllocated = true;

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::allocateDescriptorSet(vk::Context *context,
                                                         vk::ResourceUseList *resourceUseList,
                                                         DescriptorSetIndex descriptorSetIndex)
{
    bool ignoreNewPoolAllocated;
    return allocateDescriptorSetAndGetInfo(context, resourceUseList, descriptorSetIndex,
                                           &ignoreNewPoolAllocated);
}

angle::Result ProgramExecutableVk::allocateDescriptorSetAndGetInfo(
    vk::Context *context,
    vk::ResourceUseList *resourceUseList,
    DescriptorSetIndex descriptorSetIndex,
    bool *newPoolAllocatedOut)
{
    vk::DynamicDescriptorPool &dynamicDescriptorPool = mDynamicDescriptorPools[descriptorSetIndex];

    const vk::DescriptorSetLayout &descriptorSetLayout =
        mDescriptorSetLayouts[descriptorSetIndex].get();
    ANGLE_TRY(dynamicDescriptorPool.allocateSetsAndGetInfo(
        context, resourceUseList, descriptorSetLayout, 1,
        &mDescriptorPoolBindings[descriptorSetIndex], &mDescriptorSets[descriptorSetIndex],
        newPoolAllocatedOut));
    mEmptyDescriptorSets[descriptorSetIndex] = VK_NULL_HANDLE;

    ++mPerfCounters.descriptorSetAllocations[descriptorSetIndex];

    return angle::Result::Continue;
}

void ProgramExecutableVk::addInterfaceBlockDescriptorSetDesc(
    const std::vector<gl::InterfaceBlock> &blocks,
    gl::ShaderType shaderType,
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

        const std::string blockName = block.mappedName;

        const ShaderInterfaceVariableInfo &info = mVariableInfoMap.get(shaderType, blockName);
        if (info.isDuplicate)
        {
            continue;
        }

        VkShaderStageFlags activeStages = gl_vk::GetShaderStageFlags(info.activeStages);

        descOut->update(info.binding, descType, arraySize, activeStages, nullptr);
    }
}

void ProgramExecutableVk::addAtomicCounterBufferDescriptorSetDesc(
    const std::vector<gl::AtomicCounterBuffer> &atomicCounterBuffers,
    gl::ShaderType shaderType,
    vk::DescriptorSetLayoutDesc *descOut)
{
    if (atomicCounterBuffers.empty())
    {
        return;
    }

    std::string blockName(sh::vk::kAtomicCountersBlockName);

    if (!mVariableInfoMap.contains(shaderType, blockName))
    {
        return;
    }

    const ShaderInterfaceVariableInfo &info = mVariableInfoMap.get(shaderType, blockName);
    if (info.isDuplicate || !info.activeStages[shaderType])
    {
        return;
    }

    VkShaderStageFlags activeStages = gl_vk::GetShaderStageFlags(info.activeStages);

    // A single storage buffer array is used for all stages for simplicity.
    descOut->update(info.binding, kStorageBufferDescriptorType,
                    gl::IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS, activeStages, nullptr);
}

void ProgramExecutableVk::addImageDescriptorSetDesc(const gl::ProgramExecutable &executable,
                                                    vk::DescriptorSetLayoutDesc *descOut)
{
    const std::vector<gl::ImageBinding> &imageBindings = executable.getImageBindings();
    const std::vector<gl::LinkedUniform> &uniforms     = executable.getUniforms();

    for (uint32_t imageIndex = 0; imageIndex < imageBindings.size(); ++imageIndex)
    {
        const gl::ImageBinding &imageBinding = imageBindings[imageIndex];
        uint32_t uniformIndex                = executable.getUniformIndexFromImageIndex(imageIndex);
        const gl::LinkedUniform &imageUniform = uniforms[uniformIndex];

        std::string imageName = GlslangGetMappedSamplerName(imageUniform.name);

        // The front-end always binds array image units sequentially.
        uint32_t arraySize = static_cast<uint32_t>(imageBinding.boundImageUnits.size());

        // 2D arrays are split into multiple 1D arrays when generating LinkedUniforms. Since they
        // are flattened into one array, ignore the nonzero elements and expand the array to the
        // total array size.
        if (gl::SamplerNameContainsNonZeroArrayElement(imageUniform.name))
        {
            continue;
        }

        for (unsigned int outerArraySize : imageUniform.outerArraySizes)
        {
            arraySize *= outerArraySize;
        }

        for (gl::ShaderType shaderType : executable.getLinkedShaderStages())
        {
            if (!imageUniform.isActive(shaderType))
            {
                continue;
            }

            GetImageNameWithoutIndices(&imageName);
            const ShaderInterfaceVariableInfo &info = mVariableInfoMap.get(shaderType, imageName);
            if (info.isDuplicate)
            {
                continue;
            }

            VkShaderStageFlags activeStages = gl_vk::GetShaderStageFlags(info.activeStages);

            const VkDescriptorType descType = imageBinding.textureType == gl::TextureType::Buffer
                                                  ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
                                                  : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descOut->update(info.binding, descType, arraySize, activeStages, nullptr);
        }
    }
}

void ProgramExecutableVk::addInputAttachmentDescriptorSetDesc(
    const gl::ProgramExecutable &executable,
    gl::ShaderType shaderType,
    vk::DescriptorSetLayoutDesc *descOut)
{
    if (shaderType != gl::ShaderType::Fragment)
    {
        return;
    }

    const std::vector<gl::LinkedUniform> &uniforms = executable.getUniforms();

    if (!executable.usesFramebufferFetch())
    {
        return;
    }

    const uint32_t baseUniformIndex              = executable.getFragmentInoutRange().low();
    const gl::LinkedUniform &baseInputAttachment = uniforms.at(baseUniformIndex);
    std::string baseMappedName                   = baseInputAttachment.mappedName;
    ShaderInterfaceVariableInfo &baseInfo        = mVariableInfoMap.get(shaderType, baseMappedName);

    if (baseInfo.isDuplicate)
    {
        return;
    }

    uint32_t baseBinding = baseInfo.binding - baseInputAttachment.location;

    for (uint32_t colorIndex = 0; colorIndex < gl::IMPLEMENTATION_MAX_DRAW_BUFFERS; ++colorIndex)
    {
        descOut->update(baseBinding, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1,
                        VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
        baseBinding++;
    }
}

angle::Result ProgramExecutableVk::addTextureDescriptorSetDesc(
    ContextVk *contextVk,
    const gl::ProgramExecutable &executable,
    const gl::ActiveTextureArray<vk::TextureUnit> *activeTextures,
    vk::DescriptorSetLayoutDesc *descOut)
{
    const std::vector<gl::SamplerBinding> &samplerBindings = executable.getSamplerBindings();
    const std::vector<gl::LinkedUniform> &uniforms         = executable.getUniforms();

    for (uint32_t textureIndex = 0; textureIndex < samplerBindings.size(); ++textureIndex)
    {
        const gl::SamplerBinding &samplerBinding = samplerBindings[textureIndex];

        uint32_t uniformIndex = executable.getUniformIndexFromSamplerIndex(textureIndex);
        const gl::LinkedUniform &samplerUniform = uniforms[uniformIndex];

        const std::string samplerName = GlslangGetMappedSamplerName(samplerUniform.name);

        // The front-end always binds array sampler units sequentially.
        uint32_t arraySize = static_cast<uint32_t>(samplerBinding.boundTextureUnits.size());

        // 2D arrays are split into multiple 1D arrays when generating LinkedUniforms. Since they
        // are flattened into one array, ignore the nonzero elements and expand the array to the
        // total array size.
        if (gl::SamplerNameContainsNonZeroArrayElement(samplerUniform.name))
        {
            continue;
        }

        for (unsigned int outerArraySize : samplerUniform.outerArraySizes)
        {
            arraySize *= outerArraySize;
        }

        for (gl::ShaderType shaderType : executable.getLinkedShaderStages())
        {
            if (!samplerUniform.isActive(shaderType))
            {
                continue;
            }

            const ShaderInterfaceVariableInfo &info = mVariableInfoMap.get(shaderType, samplerName);
            if (info.isDuplicate)
            {
                continue;
            }

            VkShaderStageFlags activeStages = gl_vk::GetShaderStageFlags(info.activeStages);

            // TODO: https://issuetracker.google.com/issues/158215272: how do we handle array of
            // immutable samplers?
            GLuint textureUnit = samplerBinding.boundTextureUnits[0];
            if (activeTextures &&
                ((*activeTextures)[textureUnit].texture->getImage().hasImmutableSampler()))
            {
                ASSERT(samplerBinding.boundTextureUnits.size() == 1);
                // Always take the texture's sampler, that's only way to get to yuv conversion for
                // externalFormat
                const TextureVk *textureVk          = (*activeTextures)[textureUnit].texture;
                const vk::Sampler &immutableSampler = textureVk->getSampler().get();
                descOut->update(info.binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, arraySize,
                                activeStages, &immutableSampler);
                const vk::ImageHelper &image                              = textureVk->getImage();
                mImmutableSamplerIndexMap[image.getYcbcrConversionDesc()] = textureIndex;
                // The Vulkan spec has the following note -
                // All descriptors in a binding use the same maximum
                // combinedImageSamplerDescriptorCount descriptors to allow implementations to use a
                // uniform stride for dynamic indexing of the descriptors in the binding.
                uint64_t externalFormat        = image.getExternalFormat();
                uint32_t formatDescriptorCount = 0;

                RendererVk *renderer = contextVk->getRenderer();

                if (externalFormat != 0)
                {
                    ANGLE_TRY(renderer->getFormatDescriptorCountForExternalFormat(
                        contextVk, externalFormat, &formatDescriptorCount));
                }
                else
                {
                    VkFormat vkFormat = image.getActualVkFormat();
                    ASSERT(vkFormat != 0);
                    ANGLE_TRY(renderer->getFormatDescriptorCountForVkFormat(
                        contextVk, vkFormat, &formatDescriptorCount));
                }

                ASSERT(formatDescriptorCount > 0);
                mImmutableSamplersMaxDescriptorCount =
                    std::max(mImmutableSamplersMaxDescriptorCount, formatDescriptorCount);
            }
            else
            {
                const VkDescriptorType descType =
                    samplerBinding.textureType == gl::TextureType::Buffer
                        ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
                        : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descOut->update(info.binding, descType, arraySize, activeStages, nullptr);
            }
        }
    }

    return angle::Result::Continue;
}

void WriteBufferDescriptorSetBinding(const vk::BufferHelper &buffer,
                                     VkDeviceSize offset,
                                     VkDeviceSize size,
                                     VkDescriptorSet descSet,
                                     VkDescriptorType descType,
                                     uint32_t bindingIndex,
                                     uint32_t arrayElement,
                                     VkDeviceSize requiredOffsetAlignment,
                                     VkDescriptorBufferInfo *bufferInfoOut,
                                     VkWriteDescriptorSet *writeInfoOut)
{
    if (IsDynamicDescriptor(descType))
    {
        ASSERT(offset == 0);
    }
    else
    {
        // Adjust offset with buffer's offset since we are sending down buffer.getBuffer() below.
        offset = buffer.getOffset() + offset;

        // If requiredOffsetAlignment is 0, the buffer offset is guaranteed to have the necessary
        // alignment through other means (the backend specifying the alignment through a GLES limit
        // that the frontend then enforces).  If it's not 0, we need to bind the buffer at an offset
        // that's aligned.  The difference in offsets is communicated to the shader via driver
        // uniforms.
        if (requiredOffsetAlignment)
        {
            VkDeviceSize alignedOffset =
                (offset / requiredOffsetAlignment) * requiredOffsetAlignment;
            VkDeviceSize offsetDiff = offset - alignedOffset;

            offset = alignedOffset;
            size += offsetDiff;
        }
    }

    bufferInfoOut->buffer = buffer.getBuffer().getHandle();
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

void ProgramExecutableVk::updateEarlyFragmentTestsOptimization(
    ContextVk *contextVk,
    const gl::ProgramExecutable &glExecutable)
{
    const gl::State &glState = contextVk->getState();

    mTransformOptions.removeEarlyFragmentTestsOptimization = false;
    if (!glState.canEnableEarlyFragmentTestsOptimization())
    {
        if (glExecutable.usesEarlyFragmentTestsOptimization())
        {
            mTransformOptions.removeEarlyFragmentTestsOptimization = true;
        }
    }
}

angle::Result ProgramExecutableVk::getGraphicsPipeline(ContextVk *contextVk,
                                                       gl::PrimitiveMode mode,
                                                       const vk::GraphicsPipelineDesc &desc,
                                                       const gl::ProgramExecutable &glExecutable,
                                                       const vk::GraphicsPipelineDesc **descPtrOut,
                                                       vk::PipelineHelper **pipelineOut)
{
    const gl::State &glState         = contextVk->getState();
    RendererVk *renderer             = contextVk->getRenderer();
    vk::PipelineCache *pipelineCache = nullptr;

    ASSERT(glExecutable.hasLinkedShaderStage(gl::ShaderType::Vertex));

    mTransformOptions.enableLineRasterEmulation = contextVk->isBresenhamEmulationEnabled(mode);
    mTransformOptions.surfaceRotation           = ToUnderlying(desc.getSurfaceRotation());
    mTransformOptions.enableDepthCorrection     = !glState.isClipControlDepthZeroToOne();
    mTransformOptions.removeTransformFeedbackEmulation =
        contextVk->getFeatures().emulateTransformFeedback.enabled &&
        !glState.isTransformFeedbackActiveUnpaused();

    // This must be called after mTransformOptions have been set.
    ProgramInfo &programInfo                  = getGraphicsProgramInfo(mTransformOptions);
    const gl::ShaderBitSet linkedShaderStages = glExecutable.getLinkedShaderStages();
    gl::ShaderType lastPreFragmentStage       = gl::GetLastPreFragmentStage(linkedShaderStages);

    const bool isTransformFeedbackProgram =
        !glExecutable.getLinkedTransformFeedbackVaryings().empty();

    for (gl::ShaderType shaderType : linkedShaderStages)
    {
        ANGLE_TRY(initGraphicsShaderProgram(
            contextVk, shaderType, shaderType == lastPreFragmentStage, isTransformFeedbackProgram,
            mTransformOptions, &programInfo, mVariableInfoMap));
    }

    vk::ShaderProgramHelper *shaderProgram = programInfo.getShaderProgram();
    ASSERT(shaderProgram);

    // Drawable size is part of specialization constant, but does not have its own dedicated
    // programInfo entry. We pick the programInfo entry based on the mTransformOptions and then
    // update drawable width/height specialization constant. It will go through desc matching and if
    // spec constant does not match, it will recompile pipeline program.
    const vk::PackedExtent &dimensions = desc.getDrawableSize();
    shaderProgram->setSpecializationConstant(sh::vk::SpecializationConstantId::DrawableWidth,
                                             dimensions.width);
    shaderProgram->setSpecializationConstant(sh::vk::SpecializationConstantId::DrawableHeight,
                                             dimensions.height);
    // Similarly with the required dither based on the bound framebuffer attachment formats.
    shaderProgram->setSpecializationConstant(sh::vk::SpecializationConstantId::Dither,
                                             desc.getEmulatedDitherControl());

    // Compare the fragment output interface with the framebuffer interface.
    const gl::AttributesMask &activeAttribLocations =
        glExecutable.getNonBuiltinAttribLocationsMask();

    // Calculate missing shader outputs.
    const gl::DrawBufferMask &shaderOutMask = glExecutable.getActiveOutputVariablesMask();
    gl::DrawBufferMask framebufferMask      = glState.getDrawFramebuffer()->getDrawBufferMask();
    gl::DrawBufferMask missingOutputsMask   = ~shaderOutMask & framebufferMask;

    ANGLE_TRY(renderer->getPipelineCache(&pipelineCache));
    return shaderProgram->getGraphicsPipeline(
        contextVk, &contextVk->getRenderPassCache(), *pipelineCache, getPipelineLayout(), desc,
        activeAttribLocations, glExecutable.getAttributesTypeMask(), missingOutputsMask, descPtrOut,
        pipelineOut);
}

angle::Result ProgramExecutableVk::getComputePipeline(ContextVk *contextVk,
                                                      vk::PipelineHelper **pipelineOut)
{
    const gl::State &glState                  = contextVk->getState();
    const gl::ProgramExecutable *glExecutable = glState.getProgramExecutable();
    ASSERT(glExecutable && glExecutable->hasLinkedShaderStage(gl::ShaderType::Compute));

    ANGLE_TRY(initComputeProgram(contextVk, &mComputeProgramInfo, mVariableInfoMap));

    vk::ShaderProgramHelper *shaderProgram = mComputeProgramInfo.getShaderProgram();
    ASSERT(shaderProgram);
    return shaderProgram->getComputePipeline(contextVk, getPipelineLayout(), pipelineOut);
}

// static
angle::Result ProgramExecutableVk::InitDynamicDescriptorPool(
    vk::Context *context,
    vk::DescriptorSetLayoutDesc &descriptorSetLayoutDesc,
    VkDescriptorSetLayout descriptorSetLayout,
    uint32_t descriptorCountMultiplier,
    vk::DynamicDescriptorPool *dynamicDescriptorPool)
{
    std::vector<VkDescriptorPoolSize> descriptorPoolSizes;
    vk::DescriptorSetLayoutBindingVector bindingVector;
    std::vector<VkSampler> immutableSamplers;

    descriptorSetLayoutDesc.unpackBindings(&bindingVector, &immutableSamplers);

    for (const VkDescriptorSetLayoutBinding &binding : bindingVector)
    {
        if (binding.descriptorCount > 0)
        {
            VkDescriptorPoolSize poolSize = {};
            poolSize.type                 = binding.descriptorType;
            poolSize.descriptorCount      = binding.descriptorCount * descriptorCountMultiplier;
            descriptorPoolSizes.emplace_back(poolSize);
        }
    }

    if (descriptorPoolSizes.empty())
    {
        if (context->getRenderer()->getFeatures().bindEmptyForUnusedDescriptorSets.enabled)
        {
            // For this workaround, we have to create an empty descriptor set for each descriptor
            // set index, so make sure their pools are initialized.
            VkDescriptorPoolSize poolSize = {};
            // The type doesn't matter, since it's not actually used for anything.
            poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            poolSize.descriptorCount = 1;
            descriptorPoolSizes.emplace_back(poolSize);
        }
        else
        {
            return angle::Result::Continue;
        }
    }

    ASSERT(!descriptorPoolSizes.empty());

    ANGLE_TRY(dynamicDescriptorPool->init(context, descriptorPoolSizes.data(),
                                          descriptorPoolSizes.size(), descriptorSetLayout));

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::createPipelineLayout(
    ContextVk *contextVk,
    const gl::ProgramExecutable &glExecutable,
    gl::ActiveTextureArray<vk::TextureUnit> *activeTextures)
{
    gl::TransformFeedback *transformFeedback = contextVk->getState().getCurrentTransformFeedback();
    const gl::ShaderBitSet &linkedShaderStages = glExecutable.getLinkedShaderStages();

    reset(contextVk);

    // Store a reference to the pipeline and descriptor set layouts. This will create them if they
    // don't already exist in the cache.

    // Default uniforms and transform feedback:
    vk::DescriptorSetLayoutDesc uniformsAndXfbSetDesc;
    mNumDefaultUniformDescriptors = 0;
    for (gl::ShaderType shaderType : linkedShaderStages)
    {
        const std::string uniformBlockName = kDefaultUniformNames[shaderType];
        const ShaderInterfaceVariableInfo &info =
            mVariableInfoMap.get(shaderType, uniformBlockName);
        if (info.isDuplicate || !info.activeStages[shaderType])
        {
            continue;
        }

        uniformsAndXfbSetDesc.update(info.binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
                                     gl_vk::kShaderStageMap[shaderType], nullptr);
        mNumDefaultUniformDescriptors++;
    }

    gl::ShaderType linkedTransformFeedbackStage = glExecutable.getLinkedTransformFeedbackStage();
    bool hasXfbVaryings = linkedTransformFeedbackStage != gl::ShaderType::InvalidEnum &&
                          !glExecutable.getLinkedTransformFeedbackVaryings().empty();
    if (transformFeedback && hasXfbVaryings)
    {
        size_t xfbBufferCount                    = glExecutable.getTransformFeedbackBufferCount();
        TransformFeedbackVk *transformFeedbackVk = vk::GetImpl(transformFeedback);
        transformFeedbackVk->updateDescriptorSetLayout(contextVk, mVariableInfoMap, xfbBufferCount,
                                                       &uniformsAndXfbSetDesc);
    }

    ANGLE_TRY(contextVk->getDescriptorSetLayoutCache().getDescriptorSetLayout(
        contextVk, uniformsAndXfbSetDesc,
        &mDescriptorSetLayouts[DescriptorSetIndex::UniformsAndXfb]));

    // Uniform and storage buffers, atomic counter buffers and images:
    vk::DescriptorSetLayoutDesc resourcesSetDesc;

    // Count the number of active uniform buffer descriptors.
    uint32_t numActiveUniformBufferDescriptors = 0;
    for (gl::ShaderType shaderType : linkedShaderStages)
    {
        const std::vector<gl::InterfaceBlock> &blocks = glExecutable.getUniformBlocks();
        for (uint32_t bufferIndex = 0; bufferIndex < blocks.size();)
        {
            const gl::InterfaceBlock &block = blocks[bufferIndex];
            const uint32_t arraySize        = GetInterfaceBlockArraySize(blocks, bufferIndex);
            bufferIndex += arraySize;

            if (!block.isActive(shaderType))
            {
                continue;
            }

            numActiveUniformBufferDescriptors += arraySize;
        }
    }

    // Decide if we should use dynamic or fixed descriptor types.
    VkPhysicalDeviceLimits limits = contextVk->getRenderer()->getPhysicalDeviceProperties().limits;
    uint32_t totalDynamicUniformBufferCount = numActiveUniformBufferDescriptors +
                                              mNumDefaultUniformDescriptors +
                                              kReservedDriverUniformBindingCount;
    if (totalDynamicUniformBufferCount <= limits.maxDescriptorSetUniformBuffersDynamic)
    {
        mUniformBufferDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    }
    else
    {
        mUniformBufferDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }

    for (gl::ShaderType shaderType : linkedShaderStages)
    {
        addInterfaceBlockDescriptorSetDesc(glExecutable.getUniformBlocks(), shaderType,
                                           mUniformBufferDescriptorType, &resourcesSetDesc);
        addInterfaceBlockDescriptorSetDesc(glExecutable.getShaderStorageBlocks(), shaderType,
                                           kStorageBufferDescriptorType, &resourcesSetDesc);
        addAtomicCounterBufferDescriptorSetDesc(glExecutable.getAtomicCounterBuffers(), shaderType,
                                                &resourcesSetDesc);
    }

    for (gl::ShaderType shaderType : linkedShaderStages)
    {
        addImageDescriptorSetDesc(glExecutable, &resourcesSetDesc);
        addInputAttachmentDescriptorSetDesc(glExecutable, shaderType, &resourcesSetDesc);
    }

    ANGLE_TRY(contextVk->getDescriptorSetLayoutCache().getDescriptorSetLayout(
        contextVk, resourcesSetDesc, &mDescriptorSetLayouts[DescriptorSetIndex::ShaderResource]));

    // Textures:
    vk::DescriptorSetLayoutDesc texturesSetDesc;
    ANGLE_TRY(
        addTextureDescriptorSetDesc(contextVk, glExecutable, activeTextures, &texturesSetDesc));

    ANGLE_TRY(contextVk->getDescriptorSetLayoutCache().getDescriptorSetLayout(
        contextVk, texturesSetDesc, &mDescriptorSetLayouts[DescriptorSetIndex::Texture]));

    // Driver uniforms
    vk::DescriptorSetLayoutDesc driverUniformsSetDesc =
        contextVk->getDriverUniformsDescriptorSetDesc();
    ANGLE_TRY(contextVk->getDescriptorSetLayoutCache().getDescriptorSetLayout(
        contextVk, driverUniformsSetDesc, &mDescriptorSetLayouts[DescriptorSetIndex::Internal]));

    // Create pipeline layout with these 4 descriptor sets.
    vk::PipelineLayoutDesc pipelineLayoutDesc;
    pipelineLayoutDesc.updateDescriptorSetLayout(DescriptorSetIndex::UniformsAndXfb,
                                                 uniformsAndXfbSetDesc);
    pipelineLayoutDesc.updateDescriptorSetLayout(DescriptorSetIndex::ShaderResource,
                                                 resourcesSetDesc);
    pipelineLayoutDesc.updateDescriptorSetLayout(DescriptorSetIndex::Texture, texturesSetDesc);
    pipelineLayoutDesc.updateDescriptorSetLayout(DescriptorSetIndex::Internal,
                                                 driverUniformsSetDesc);

    ANGLE_TRY(contextVk->getPipelineLayoutCache().getPipelineLayout(
        contextVk, pipelineLayoutDesc, mDescriptorSetLayouts, &mPipelineLayout));

    // Initialize descriptor pools.
    ANGLE_TRY(InitDynamicDescriptorPool(
        contextVk, uniformsAndXfbSetDesc,
        mDescriptorSetLayouts[DescriptorSetIndex::UniformsAndXfb].get().getHandle(), 1,
        &mDynamicDescriptorPools[DescriptorSetIndex::UniformsAndXfb]));
    ANGLE_TRY(InitDynamicDescriptorPool(
        contextVk, resourcesSetDesc,
        mDescriptorSetLayouts[DescriptorSetIndex::ShaderResource].get().getHandle(), 1,
        &mDynamicDescriptorPools[DescriptorSetIndex::ShaderResource]));
    ANGLE_TRY(InitDynamicDescriptorPool(
        contextVk, texturesSetDesc,
        mDescriptorSetLayouts[DescriptorSetIndex::Texture].get().getHandle(),
        mImmutableSamplersMaxDescriptorCount,
        &mDynamicDescriptorPools[DescriptorSetIndex::Texture]));
    ANGLE_TRY(InitDynamicDescriptorPool(
        contextVk, driverUniformsSetDesc,
        mDescriptorSetLayouts[DescriptorSetIndex::Internal].get().getHandle(), 1,
        &mDynamicDescriptorPools[DescriptorSetIndex::Internal]));

    mDynamicUniformDescriptorOffsets.clear();
    mDynamicUniformDescriptorOffsets.resize(glExecutable.getLinkedShaderStageCount(), 0);

    return angle::Result::Continue;
}

void ProgramExecutableVk::resolvePrecisionMismatch(const gl::ProgramMergedVaryings &mergedVaryings)
{
    for (const gl::ProgramVaryingRef &mergedVarying : mergedVaryings)
    {
        if (!mergedVarying.frontShader || !mergedVarying.backShader)
        {
            continue;
        }

        GLenum frontPrecision = mergedVarying.frontShader->precision;
        GLenum backPrecision  = mergedVarying.backShader->precision;
        if (frontPrecision == backPrecision)
        {
            continue;
        }

        ASSERT(frontPrecision >= GL_LOW_FLOAT && frontPrecision <= GL_HIGH_INT);
        ASSERT(backPrecision >= GL_LOW_FLOAT && backPrecision <= GL_HIGH_INT);

        if (frontPrecision > backPrecision)
        {
            // The output is higher precision than the input
            ShaderInterfaceVariableInfo &info = mVariableInfoMap.get(
                mergedVarying.frontShaderStage, mergedVarying.frontShader->mappedName);
            info.varyingIsOutput     = true;
            info.useRelaxedPrecision = true;
        }
        else
        {
            // The output is lower precision than the input, adjust the input
            ASSERT(backPrecision > frontPrecision);
            ShaderInterfaceVariableInfo &info = mVariableInfoMap.get(
                mergedVarying.backShaderStage, mergedVarying.backShader->mappedName);
            info.varyingIsInput      = true;
            info.useRelaxedPrecision = true;
        }
    }
}

void ProgramExecutableVk::updateDefaultUniformsDescriptorSet(
    vk::Context *context,
    UpdateDescriptorSetsBuilder *updateBuilder,
    vk::BufferHelper *emptyBuffer,
    vk::ResourceUseList *resourceUseList,
    gl::ShaderType shaderType,
    const DefaultUniformBlock &defaultUniformBlock,
    vk::BufferHelper *defaultUniformBuffer)
{
    const std::string uniformBlockName = kDefaultUniformNames[shaderType];

    const ShaderInterfaceVariableInfo &info = mVariableInfoMap.get(shaderType, uniformBlockName);
    if (info.isDuplicate || !info.activeStages[shaderType])
    {
        return;
    }

    VkWriteDescriptorSet &writeInfo    = updateBuilder->allocWriteDescriptorSet();
    VkDescriptorBufferInfo &bufferInfo = updateBuilder->allocDescriptorBufferInfo();

    VkDeviceSize size              = getDefaultUniformAlignedSize(context, shaderType);
    vk::BufferHelper *bufferHelper = defaultUniformBuffer;
    if (defaultUniformBlock.uniformData.empty())
    {
        bufferHelper = emptyBuffer;
        bufferHelper->retainReadOnly(resourceUseList);
        size = bufferHelper->getSize();
    }

    WriteBufferDescriptorSetBinding(
        *bufferHelper, 0, size, mDescriptorSets[DescriptorSetIndex::UniformsAndXfb],
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, info.binding, 0, 0, &bufferInfo, &writeInfo);
}

// Lazily allocate the descriptor set. We may not need one if all of the buffers are inactive.
angle::Result ProgramExecutableVk::allocateShaderResourcesDescriptorSet(
    vk::Context *context,
    vk::ResourceUseList *resourceUseList,
    const vk::DescriptorSetDesc *shaderBuffersDesc)
{
    bool newPoolAllocated = false;
    ANGLE_TRY(allocateDescriptorSetAndGetInfo(
        context, resourceUseList, DescriptorSetIndex::ShaderResource, &newPoolAllocated));

    if (shaderBuffersDesc)
    {
        // Clear descriptor set cache. It may no longer be valid.
        if (newPoolAllocated)
        {
            mShaderBufferDescriptorsCache.destroy(context->getRenderer(),
                                                  VulkanCacheType::ShaderBuffersDescriptors);
        }

        mShaderBufferDescriptorsCache.insert(*shaderBuffersDesc,
                                             mDescriptorSets[DescriptorSetIndex::ShaderResource]);
    }
    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::updateBuffersDescriptorSet(
    vk::Context *context,
    UpdateDescriptorSetsBuilder *updateBuilder,
    vk::BufferHelper *emptyBuffer,
    vk::ResourceUseList *resourceUseList,
    gl::ShaderType shaderType,
    const vk::DescriptorSetDesc &shaderBuffersDesc,
    const gl::BufferVector &buffers,
    const std::vector<gl::InterfaceBlock> &blocks,
    VkDescriptorType descriptorType,
    VkDeviceSize maxBoundBufferRange,
    bool cacheHit)
{
    // Early exit if no blocks or no update needed.
    if (blocks.empty() || (cacheHit && !IsDynamicDescriptor(descriptorType)))
    {
        return angle::Result::Continue;
    }

    ASSERT(descriptorType == mUniformBufferDescriptorType ||
           descriptorType == kStorageBufferDescriptorType);

    VkDescriptorSet descriptorSet = mDescriptorSets[DescriptorSetIndex::ShaderResource];
    ASSERT(descriptorSet != VK_NULL_HANDLE);

    // Write uniform or storage buffers.
    for (const gl::InterfaceBlock &block : blocks)
    {
        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding = buffers[block.binding];

        if (!block.isActive(shaderType))
        {
            continue;
        }

        const ShaderInterfaceVariableInfo &info =
            mVariableInfoMap.get(shaderType, block.mappedName);
        if (info.isDuplicate)
        {
            continue;
        }

        uint32_t binding      = info.binding;
        uint32_t arrayElement = block.isArray ? block.arrayElement : 0;

        if (bufferBinding.get() == nullptr)
        {
            if (!cacheHit)
            {
                VkDescriptorBufferInfo &bufferInfo = updateBuilder->allocDescriptorBufferInfo();
                VkWriteDescriptorSet &writeInfo    = updateBuilder->allocWriteDescriptorSet();

                emptyBuffer->retainReadOnly(resourceUseList);
                WriteBufferDescriptorSetBinding(*emptyBuffer, 0, emptyBuffer->getSize(),
                                                descriptorSet, descriptorType, binding,
                                                arrayElement, 0, &bufferInfo, &writeInfo);
            }
            if (IsDynamicDescriptor(descriptorType))
            {
                mDynamicShaderBufferDescriptorOffsets.push_back(
                    static_cast<uint32_t>(emptyBuffer->getOffset()));
            }
            continue;
        }

        // Limit bound buffer size to maximum resource binding size.
        GLsizeiptr boundBufferSize = gl::GetBoundBufferAvailableSize(bufferBinding);
        VkDeviceSize size          = std::min<VkDeviceSize>(boundBufferSize, maxBoundBufferRange);

        // Make sure there's no possible under/overflow with binding size.
        static_assert(sizeof(VkDeviceSize) >= sizeof(bufferBinding.getSize()),
                      "VkDeviceSize too small");
        ASSERT(bufferBinding.getSize() >= 0);

        BufferVk *bufferVk             = vk::GetImpl(bufferBinding.get());
        vk::BufferHelper &bufferHelper = bufferVk->getBuffer();

        if (!cacheHit)
        {
            VkDescriptorBufferInfo &bufferInfo = updateBuilder->allocDescriptorBufferInfo();
            VkWriteDescriptorSet &writeInfo    = updateBuilder->allocWriteDescriptorSet();

            VkDeviceSize offset =
                IsDynamicDescriptor(descriptorType) ? 0 : bufferBinding.getOffset();
            WriteBufferDescriptorSetBinding(bufferHelper, offset, size, descriptorSet,
                                            descriptorType, binding, arrayElement, 0, &bufferInfo,
                                            &writeInfo);
        }
        if (IsDynamicDescriptor(descriptorType))
        {
            mDynamicShaderBufferDescriptorOffsets.push_back(
                static_cast<uint32_t>(bufferHelper.getOffset() + bufferBinding.getOffset()));
        }
    }

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::updateAtomicCounterBuffersDescriptorSet(
    vk::Context *context,
    UpdateDescriptorSetsBuilder *updateBuilder,
    vk::BufferHelper *emptyBuffer,
    vk::ResourceUseList *resourceUseList,
    const gl::BufferVector &atomicCounterBufferBindings,
    const gl::ProgramExecutable &executable,
    gl::ShaderType shaderType,
    const vk::DescriptorSetDesc &shaderBuffersDesc,
    bool cacheHit)
{
    const std::vector<gl::AtomicCounterBuffer> &atomicCounterBuffers =
        executable.getAtomicCounterBuffers();

    if (atomicCounterBuffers.empty() || cacheHit)
    {
        return angle::Result::Continue;
    }

    std::string blockName(sh::vk::kAtomicCountersBlockName);

    if (!mVariableInfoMap.contains(shaderType, blockName))
    {
        return angle::Result::Continue;
    }

    const ShaderInterfaceVariableInfo &info = mVariableInfoMap.get(shaderType, blockName);
    if (info.isDuplicate || !info.activeStages[shaderType])
    {
        return angle::Result::Continue;
    }

    gl::AtomicCounterBufferMask writtenBindings;

    RendererVk *rendererVk = context->getRenderer();
    const VkDeviceSize requiredOffsetAlignment =
        rendererVk->getPhysicalDeviceProperties().limits.minStorageBufferOffsetAlignment;

    VkDescriptorSet descriptorSet = mDescriptorSets[DescriptorSetIndex::ShaderResource];

    // Write atomic counter buffers.
    for (const gl::AtomicCounterBuffer &atomicCounterBuffer : atomicCounterBuffers)
    {
        uint32_t binding = atomicCounterBuffer.binding;
        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding =
            atomicCounterBufferBindings[binding];

        if (bufferBinding.get() == nullptr)
        {
            continue;
        }

        static_assert(!IsDynamicDescriptor(kStorageBufferDescriptorType),
                      "updateAtomicCounterBuffersDescriptorSet needs an update to handle dynamic "
                      "descriptors");

        VkDescriptorBufferInfo &bufferInfo = updateBuilder->allocDescriptorBufferInfo();
        VkWriteDescriptorSet &writeInfo    = updateBuilder->allocWriteDescriptorSet();

        BufferVk *bufferVk             = vk::GetImpl(bufferBinding.get());
        vk::BufferHelper &bufferHelper = bufferVk->getBuffer();

        VkDeviceSize size = gl::GetBoundBufferAvailableSize(bufferBinding);
        WriteBufferDescriptorSetBinding(bufferHelper,
                                        static_cast<uint32_t>(bufferBinding.getOffset()), size,
                                        descriptorSet, kStorageBufferDescriptorType, info.binding,
                                        binding, requiredOffsetAlignment, &bufferInfo, &writeInfo);

        writtenBindings.set(binding);
    }

    // Bind the empty buffer to every array slot that's unused.
    emptyBuffer->retainReadOnly(resourceUseList);
    size_t count                        = (~writtenBindings).count();
    VkDescriptorBufferInfo *bufferInfos = updateBuilder->allocDescriptorBufferInfos(count);
    VkWriteDescriptorSet *writeInfos    = updateBuilder->allocWriteDescriptorSets(count);
    size_t writeCount                   = 0;
    for (size_t binding : ~writtenBindings)
    {
        WriteBufferDescriptorSetBinding(*emptyBuffer, 0, emptyBuffer->getSize(), descriptorSet,
                                        kStorageBufferDescriptorType, info.binding,
                                        static_cast<uint32_t>(binding), 0, &bufferInfos[writeCount],
                                        &writeInfos[writeCount]);
        writeCount++;
    }

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::updateImagesDescriptorSet(
    vk::Context *context,
    vk::ResourceUseList *resourceUseList,
    UpdateDescriptorSetsBuilder *updateBuilder,
    const gl::ActiveTextureArray<TextureVk *> &activeImages,
    const std::vector<gl::ImageUnit> &imageUnits,
    const gl::ProgramExecutable &executable,
    gl::ShaderType shaderType)
{
    RendererVk *renderer                               = context->getRenderer();
    const std::vector<gl::ImageBinding> &imageBindings = executable.getImageBindings();
    const std::vector<gl::LinkedUniform> &uniforms     = executable.getUniforms();

    if (imageBindings.empty())
    {
        return angle::Result::Continue;
    }

    angle::HashMap<std::string, uint32_t> mappedImageNameToArrayOffset;

    VkDescriptorSet descriptorSet = mDescriptorSets[DescriptorSetIndex::ShaderResource];

    // Write images.
    for (uint32_t imageIndex = 0; imageIndex < imageBindings.size(); ++imageIndex)
    {
        const gl::ImageBinding &imageBinding = imageBindings[imageIndex];
        uint32_t uniformIndex                = executable.getUniformIndexFromImageIndex(imageIndex);
        const gl::LinkedUniform &imageUniform = uniforms[uniformIndex];

        if (!imageUniform.isActive(shaderType))
        {
            continue;
        }

        std::string mappedImageName = GlslangGetMappedSamplerName(imageUniform.name);

        GetImageNameWithoutIndices(&mappedImageName);

        uint32_t arrayOffset = 0;
        uint32_t arraySize   = static_cast<uint32_t>(imageBinding.boundImageUnits.size());

        arrayOffset = mappedImageNameToArrayOffset[mappedImageName];
        // Front-end generates array elements in order, so we can just increment the offset each
        // time we process a nested array.
        mappedImageNameToArrayOffset[mappedImageName] += arraySize;

        // Texture buffers use buffer views, so they are especially handled.
        if (imageBinding.textureType == gl::TextureType::Buffer)
        {
            // Handle format reinterpretation by looking for a view with the format specified in
            // the shader (if any, instead of the format specified to glTexBuffer).
            const vk::Format *format = nullptr;
            if (imageUniform.imageUnitFormat != GL_NONE)
            {
                format = &renderer->getFormat(imageUniform.imageUnitFormat);
            }

            const ShaderInterfaceVariableInfo &info =
                mVariableInfoMap.get(shaderType, mappedImageName);
            if (info.isDuplicate)
            {
                continue;
            }

            VkWriteDescriptorSet *writeInfos = updateBuilder->allocWriteDescriptorSets(arraySize);

            for (uint32_t arrayElement = 0; arrayElement < arraySize; ++arrayElement)
            {
                GLuint imageUnit     = imageBinding.boundImageUnits[arrayElement];
                TextureVk *textureVk = activeImages[imageUnit];

                const vk::BufferView *view = nullptr;
                ANGLE_TRY(textureVk->getBufferViewAndRecordUse(context, format, true, &view));

                writeInfos[arrayElement].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeInfos[arrayElement].pNext            = nullptr;
                writeInfos[arrayElement].dstSet           = descriptorSet;
                writeInfos[arrayElement].dstBinding       = info.binding;
                writeInfos[arrayElement].dstArrayElement  = arrayOffset + arrayElement;
                writeInfos[arrayElement].descriptorCount  = 1;
                writeInfos[arrayElement].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
                writeInfos[arrayElement].pImageInfo       = nullptr;
                writeInfos[arrayElement].pBufferInfo      = nullptr;
                writeInfos[arrayElement].pTexelBufferView = view->ptr();
            }
            continue;
        }

        const std::string imageName             = GlslangGetMappedSamplerName(imageUniform.name);
        const ShaderInterfaceVariableInfo &info = mVariableInfoMap.get(shaderType, imageName);
        if (info.isDuplicate)
        {
            continue;
        }

        VkWriteDescriptorSet *writeInfos  = updateBuilder->allocWriteDescriptorSets(arraySize);
        VkDescriptorImageInfo *imageInfos = updateBuilder->allocDescriptorImageInfos(arraySize);
        for (uint32_t arrayElement = 0; arrayElement < arraySize; ++arrayElement)
        {
            GLuint imageUnit             = imageBinding.boundImageUnits[arrayElement];
            const gl::ImageUnit &binding = imageUnits[imageUnit];
            TextureVk *textureVk         = activeImages[imageUnit];

            vk::ImageHelper *image         = &textureVk->getImage();
            const vk::ImageView *imageView = nullptr;

            ANGLE_TRY(textureVk->getStorageImageView(context, binding, &imageView));

            // Note: binding.access is unused because it is implied by the shader.

            imageInfos[arrayElement].sampler     = VK_NULL_HANDLE;
            imageInfos[arrayElement].imageView   = imageView->getHandle();
            imageInfos[arrayElement].imageLayout = image->getCurrentLayout();

            writeInfos[arrayElement].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeInfos[arrayElement].pNext            = nullptr;
            writeInfos[arrayElement].dstSet           = descriptorSet;
            writeInfos[arrayElement].dstBinding       = info.binding;
            writeInfos[arrayElement].dstArrayElement  = arrayOffset + arrayElement;
            writeInfos[arrayElement].descriptorCount  = 1;
            writeInfos[arrayElement].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writeInfos[arrayElement].pImageInfo       = &imageInfos[arrayElement];
            writeInfos[arrayElement].pBufferInfo      = nullptr;
            writeInfos[arrayElement].pTexelBufferView = nullptr;
        }
    }

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::updateShaderResourcesDescriptorSet(
    ContextVk *contextVk,
    const gl::ProgramExecutable *executable,
    UpdateDescriptorSetsBuilder *updateBuilder,
    vk::BufferHelper *emptyBuffer,
    vk::ResourceUseList *resourceUseList,
    FramebufferVk *framebufferVk,
    const vk::DescriptorSetDesc &shaderBuffersDesc)
{
    // Reset the descriptor set handles so we only allocate a new one when necessary.
    mDescriptorSets[DescriptorSetIndex::ShaderResource]      = VK_NULL_HANDLE;
    mEmptyDescriptorSets[DescriptorSetIndex::ShaderResource] = VK_NULL_HANDLE;
    mDynamicShaderBufferDescriptorOffsets.clear();

    if (!mDynamicDescriptorPools[DescriptorSetIndex::ShaderResource].valid())
    {
        return angle::Result::Continue;
    }

    bool cacheHit = false;

    if (!executable->hasImages() && !executable->usesFramebufferFetch())
    {
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        if (mShaderBufferDescriptorsCache.get(shaderBuffersDesc, &descriptorSet))
        {
            mDescriptorSets[DescriptorSetIndex::ShaderResource] = descriptorSet;
            // The descriptor pool that this descriptor set was allocated from needs to be retained
            // each time the descriptor set is used in a new command.
            mDescriptorPoolBindings[DescriptorSetIndex::ShaderResource].get().retain(
                &contextVk->getResourceUseList());
            cacheHit = true;
        }
        else
        {
            ANGLE_TRY(allocateShaderResourcesDescriptorSet(contextVk, resourceUseList,
                                                           &shaderBuffersDesc));
        }
    }
    else
    {
        ANGLE_TRY(allocateShaderResourcesDescriptorSet(contextVk, resourceUseList, nullptr));
    }

    ASSERT(mDescriptorSets[DescriptorSetIndex::ShaderResource] != VK_NULL_HANDLE);

    const VkPhysicalDeviceLimits &limits =
        contextVk->getRenderer()->getPhysicalDeviceProperties().limits;

    const gl::State &glState = contextVk->getState();
    const gl::BufferVector &atomicCounterBufferBindings =
        glState.getOffsetBindingPointerAtomicCounterBuffers();

    const gl::BufferVector &uniformBuffers = glState.getOffsetBindingPointerUniformBuffers();
    const gl::BufferVector &storageBuffers = glState.getOffsetBindingPointerShaderStorageBuffers();
    const gl::ActiveTextureArray<TextureVk *> &activeImages = contextVk->getActiveImages();

    for (gl::ShaderType shaderType : executable->getLinkedShaderStages())
    {
        ANGLE_TRY(updateBuffersDescriptorSet(
            contextVk, updateBuilder, emptyBuffer, resourceUseList, shaderType, shaderBuffersDesc,
            uniformBuffers, executable->getUniformBlocks(), mUniformBufferDescriptorType,
            limits.maxUniformBufferRange, cacheHit));
        ANGLE_TRY(updateBuffersDescriptorSet(
            contextVk, updateBuilder, emptyBuffer, resourceUseList, shaderType, shaderBuffersDesc,
            storageBuffers, executable->getShaderStorageBlocks(), kStorageBufferDescriptorType,
            limits.maxStorageBufferRange, cacheHit));
        ANGLE_TRY(updateAtomicCounterBuffersDescriptorSet(
            contextVk, updateBuilder, emptyBuffer, resourceUseList, atomicCounterBufferBindings,
            *executable, shaderType, shaderBuffersDesc, cacheHit));
        ANGLE_TRY(updateImagesDescriptorSet(contextVk, resourceUseList, updateBuilder, activeImages,
                                            glState.getImageUnits(), *executable, shaderType));
        ANGLE_TRY(updateInputAttachmentDescriptorSet(contextVk, resourceUseList, updateBuilder,
                                                     *executable, shaderType, framebufferVk));
    }

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::updateInputAttachmentDescriptorSet(
    vk::Context *context,
    vk::ResourceUseList *resourceUseList,
    UpdateDescriptorSetsBuilder *updateBuilder,
    const gl::ProgramExecutable &executable,
    gl::ShaderType shaderType,
    FramebufferVk *framebufferVk)
{
    if (shaderType != gl::ShaderType::Fragment)
    {
        return angle::Result::Continue;
    }

    const std::vector<gl::LinkedUniform> &uniforms = executable.getUniforms();

    if (!executable.usesFramebufferFetch())
    {
        return angle::Result::Continue;
    }

    const uint32_t baseUniformIndex              = executable.getFragmentInoutRange().low();
    const gl::LinkedUniform &baseInputAttachment = uniforms.at(baseUniformIndex);
    std::string baseMappedName                   = baseInputAttachment.mappedName;

    ShaderInterfaceVariableInfo &baseInfo = mVariableInfoMap.get(shaderType, baseMappedName);
    if (baseInfo.isDuplicate)
    {
        return angle::Result::Continue;
    }

    uint32_t baseBinding = baseInfo.binding - baseInputAttachment.location;

    VkDescriptorSet descriptorSet = mDescriptorSets[DescriptorSetIndex::ShaderResource];

    for (size_t colorIndex : framebufferVk->getState().getColorAttachmentsMask())
    {
        VkWriteDescriptorSet *writeInfos  = updateBuilder->allocWriteDescriptorSets(1);
        VkDescriptorImageInfo *imageInfos = updateBuilder->allocDescriptorImageInfos(1);
        RenderTargetVk *renderTargetVk    = framebufferVk->getColorDrawRenderTarget(colorIndex);
        const vk::ImageView *imageView    = nullptr;

        ANGLE_TRY(renderTargetVk->getImageView(context, &imageView));

        imageInfos[0].sampler     = VK_NULL_HANDLE;
        imageInfos[0].imageView   = imageView->getHandle();
        imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        writeInfos[0].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeInfos[0].pNext            = nullptr;
        writeInfos[0].dstSet           = descriptorSet;
        writeInfos[0].dstBinding       = baseBinding + static_cast<uint32_t>(colorIndex);
        writeInfos[0].dstArrayElement  = 0;
        writeInfos[0].descriptorCount  = 1;
        writeInfos[0].descriptorType   = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        writeInfos[0].pImageInfo       = &imageInfos[0];
        writeInfos[0].pBufferInfo      = nullptr;
        writeInfos[0].pTexelBufferView = nullptr;
    }

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::updateUniformsAndXfbDescriptorSet(
    vk::Context *context,
    UpdateDescriptorSetsBuilder *updateBuilder,
    vk::ResourceUseList *resourceUseList,
    vk::BufferHelper *emptyBuffer,
    const gl::ProgramExecutable &executable,
    vk::BufferHelper *defaultUniformBuffer,
    const vk::DescriptorSetDesc &uniformsAndXfbDesc,
    bool isTransformFeedbackActiveUnpaused,
    TransformFeedbackVk *transformFeedbackVk)
{
    ASSERT(executable.hasTransformFeedbackOutput());

    bool newDescriptorSetAllocated;
    ANGLE_TRY(allocUniformAndXfbDescriptorSet(context, resourceUseList, defaultUniformBuffer,
                                              uniformsAndXfbDesc, &newDescriptorSetAllocated));

    if (newDescriptorSetAllocated)
    {
        for (gl::ShaderType shaderType : executable.getLinkedShaderStages())
        {
            updateDefaultUniformsDescriptorSet(context, updateBuilder, emptyBuffer, resourceUseList,
                                               shaderType, *mDefaultUniformBlocks[shaderType],
                                               defaultUniformBuffer);
        }
        updateTransformFeedbackDescriptorSetImpl(context, updateBuilder, emptyBuffer, executable,
                                                 isTransformFeedbackActiveUnpaused,
                                                 transformFeedbackVk);
    }

    return angle::Result::Continue;
}

void ProgramExecutableVk::updateTransformFeedbackDescriptorSetImpl(
    vk::Context *context,
    UpdateDescriptorSetsBuilder *updateBuilder,
    vk::BufferHelper *emptyBuffer,
    const gl::ProgramExecutable &executable,
    bool isTransformFeedbackActiveUnpaused,
    TransformFeedbackVk *transformFeedbackVk)
{
    if (!executable.hasTransformFeedbackOutput())
    {
        // If xfb has no output there is no need to update descriptor set.
        return;
    }
    if (!isTransformFeedbackActiveUnpaused)
    {
        // We set empty Buffer to xfb descriptor set because xfb descriptor set
        // requires valid buffer bindings, even if they are empty buffer,
        // otherwise Vulkan validation layer generates errors.
        if (transformFeedbackVk)
        {
            transformFeedbackVk->initDescriptorSet(
                context, updateBuilder, emptyBuffer, mVariableInfoMap,
                executable.getTransformFeedbackBufferCount(),
                mDescriptorSets[DescriptorSetIndex::UniformsAndXfb]);
        }
        return;
    }
    transformFeedbackVk->updateDescriptorSet(context, updateBuilder, executable, mVariableInfoMap,
                                             mDescriptorSets[DescriptorSetIndex::UniformsAndXfb]);
}

angle::Result ProgramExecutableVk::updateTexturesDescriptorSet(
    vk::Context *context,
    UpdateDescriptorSetsBuilder *updateBuilder,
    vk::ResourceUseList *resourceUseList,
    const gl::ProgramExecutable &executable,
    const gl::ActiveTextureArray<vk::TextureUnit> &activeTextures,
    const vk::DescriptorSetDesc &texturesDesc,
    bool emulateSeamfulCubeMapSampling)
{
    if (!executable.hasTextures())
    {
        return angle::Result::Continue;
    }

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    if (mTextureDescriptorsCache.get(texturesDesc, &descriptorSet))
    {
        mDescriptorSets[DescriptorSetIndex::Texture] = descriptorSet;
        // The descriptor pool that this descriptor set was allocated from needs to be retained each
        // time the descriptor set is used in a new command.
        mDescriptorPoolBindings[DescriptorSetIndex::Texture].get().retain(resourceUseList);
        return angle::Result::Continue;
    }

    const std::vector<gl::SamplerBinding> &samplerBindings = executable.getSamplerBindings();

    for (gl::ShaderType shaderType : executable.getLinkedShaderStages())
    {
        angle::HashMap<std::string, uint32_t> mappedSamplerNameToArrayOffset;
        for (uint32_t textureIndex = 0; textureIndex < executable.getSamplerBindings().size();
             ++textureIndex)
        {
            const gl::SamplerBinding &samplerBinding = samplerBindings[textureIndex];
            uint32_t uniformIndex = executable.getUniformIndexFromSamplerIndex(textureIndex);
            const gl::LinkedUniform &samplerUniform = executable.getUniforms()[uniformIndex];
            std::string mappedSamplerName = GlslangGetMappedSamplerName(samplerUniform.name);

            if (!samplerUniform.isActive(shaderType))
            {
                continue;
            }

            const std::string samplerName = GlslangGetMappedSamplerName(samplerUniform.name);

            const ShaderInterfaceVariableInfo &info = mVariableInfoMap.get(shaderType, samplerName);
            if (info.isDuplicate)
            {
                continue;
            }

            // Lazily allocate the descriptor set, since we may not need one if all of the
            // sampler uniforms are inactive.
            if (descriptorSet == VK_NULL_HANDLE)
            {
                bool newPoolAllocated;
                ANGLE_TRY(allocateDescriptorSetAndGetInfo(
                    context, resourceUseList, DescriptorSetIndex::Texture, &newPoolAllocated));

                // Clear descriptor set cache. It may no longer be valid.
                if (newPoolAllocated)
                {
                    mTextureDescriptorsCache.destroy(context->getRenderer(),
                                                     VulkanCacheType::TextureDescriptors);
                }

                descriptorSet = mDescriptorSets[DescriptorSetIndex::Texture];
                mTextureDescriptorsCache.insert(texturesDesc, descriptorSet);
            }
            ASSERT(descriptorSet != VK_NULL_HANDLE);

            uint32_t arrayOffset = 0;
            uint32_t arraySize   = static_cast<uint32_t>(samplerBinding.boundTextureUnits.size());

            arrayOffset = mappedSamplerNameToArrayOffset[mappedSamplerName];
            // Front-end generates array elements in order, so we can just increment the offset each
            // time we process a nested array.
            mappedSamplerNameToArrayOffset[mappedSamplerName] += arraySize;

            VkWriteDescriptorSet *writeInfos = updateBuilder->allocWriteDescriptorSets(arraySize);

            // Texture buffers use buffer views, so they are especially handled.
            if (samplerBinding.textureType == gl::TextureType::Buffer)
            {
                for (uint32_t arrayElement = 0; arrayElement < arraySize; ++arrayElement)
                {
                    GLuint textureUnit         = samplerBinding.boundTextureUnits[arrayElement];
                    TextureVk *textureVk       = activeTextures[textureUnit].texture;
                    const vk::BufferView *view = nullptr;
                    ANGLE_TRY(textureVk->getBufferViewAndRecordUse(context, nullptr, false, &view));

                    writeInfos[arrayElement].sType      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    writeInfos[arrayElement].pNext      = nullptr;
                    writeInfos[arrayElement].dstSet     = descriptorSet;
                    writeInfos[arrayElement].dstBinding = info.binding;
                    writeInfos[arrayElement].dstArrayElement = arrayOffset + arrayElement;
                    writeInfos[arrayElement].descriptorCount = 1;
                    writeInfos[arrayElement].descriptorType =
                        VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
                    writeInfos[arrayElement].pImageInfo       = nullptr;
                    writeInfos[arrayElement].pBufferInfo      = nullptr;
                    writeInfos[arrayElement].pTexelBufferView = view->ptr();
                }
                continue;
            }

            VkDescriptorImageInfo *imageInfos = updateBuilder->allocDescriptorImageInfos(arraySize);
            for (uint32_t arrayElement = 0; arrayElement < arraySize; ++arrayElement)
            {
                GLuint textureUnit          = samplerBinding.boundTextureUnits[arrayElement];
                const vk::TextureUnit &unit = activeTextures[textureUnit];
                TextureVk *textureVk        = unit.texture;
                const vk::SamplerHelper &samplerHelper = *unit.sampler;

                ASSERT(textureVk);

                vk::ImageHelper &image = textureVk->getImage();

                imageInfos[arrayElement].sampler     = samplerHelper.get().getHandle();
                imageInfos[arrayElement].imageLayout = image.getCurrentLayout();

                if (emulateSeamfulCubeMapSampling)
                {
                    // If emulating seamful cube mapping, use the fetch image view.  This is
                    // basically the same image view as read, except it's a 2DArray view for
                    // cube maps.
                    const vk::ImageView &imageView = textureVk->getFetchImageView(
                        context, unit.srgbDecode, samplerUniform.texelFetchStaticUse);
                    imageInfos[arrayElement].imageView = imageView.getHandle();
                }
                else
                {
                    const vk::ImageView &imageView = textureVk->getReadImageView(
                        context, unit.srgbDecode, samplerUniform.texelFetchStaticUse);
                    imageInfos[arrayElement].imageView = imageView.getHandle();
                }

                if (textureVk->getImage().hasImmutableSampler())
                {
                    imageInfos[arrayElement].sampler = textureVk->getSampler().get().getHandle();
                }

                writeInfos[arrayElement].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeInfos[arrayElement].pNext           = nullptr;
                writeInfos[arrayElement].dstSet          = descriptorSet;
                writeInfos[arrayElement].dstBinding      = info.binding;
                writeInfos[arrayElement].dstArrayElement = arrayOffset + arrayElement;
                writeInfos[arrayElement].descriptorCount = 1;
                writeInfos[arrayElement].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                writeInfos[arrayElement].pImageInfo     = &imageInfos[arrayElement];
                writeInfos[arrayElement].pBufferInfo    = nullptr;
                writeInfos[arrayElement].pTexelBufferView = nullptr;
            }
        }
    }

    return angle::Result::Continue;
}

template <typename CommandBufferT>
angle::Result ProgramExecutableVk::updateDescriptorSets(vk::Context *context,
                                                        vk::ResourceUseList *resourceUseList,
                                                        CommandBufferT *commandBuffer,
                                                        PipelineType pipelineType)
{
    // Can probably use better dirty bits here.

    // Find the maximum non-null descriptor set.  This is used in conjunction with a driver
    // workaround to bind empty descriptor sets only for gaps in between 0 and max and avoid
    // binding unnecessary empty descriptor sets for the sets beyond max.
    DescriptorSetIndex lastNonNullDescriptorSetIndex = DescriptorSetIndex::InvalidEnum;
    for (DescriptorSetIndex descriptorSetIndex : angle::AllEnums<DescriptorSetIndex>())
    {
        if (descriptorSetIndex == DescriptorSetIndex::Internal)
        {
            continue;
        }
        if (mDescriptorSets[descriptorSetIndex] != VK_NULL_HANDLE)
        {
            lastNonNullDescriptorSetIndex = descriptorSetIndex;
        }
    }

    const VkPipelineBindPoint pipelineBindPoint = pipelineType == PipelineType::Compute
                                                      ? VK_PIPELINE_BIND_POINT_COMPUTE
                                                      : VK_PIPELINE_BIND_POINT_GRAPHICS;

    for (DescriptorSetIndex descriptorSetIndex : angle::AllEnums<DescriptorSetIndex>())
    {
        if (descriptorSetIndex == DescriptorSetIndex::Internal ||
            ToUnderlying(descriptorSetIndex) > ToUnderlying(lastNonNullDescriptorSetIndex))
        {
            continue;
        }

        VkDescriptorSet descSet = mDescriptorSets[descriptorSetIndex];
        if (descSet == VK_NULL_HANDLE)
        {
            if (!context->getRenderer()->getFeatures().bindEmptyForUnusedDescriptorSets.enabled)
            {
                continue;
            }

            // Workaround a driver bug where missing (though unused) descriptor sets indices cause
            // later sets to misbehave.
            if (mEmptyDescriptorSets[descriptorSetIndex] == VK_NULL_HANDLE)
            {
                const vk::DescriptorSetLayout &descriptorSetLayout =
                    mDescriptorSetLayouts[descriptorSetIndex].get();

                ANGLE_TRY(mDynamicDescriptorPools[descriptorSetIndex].allocateDescriptorSets(
                    context, resourceUseList, descriptorSetLayout, 1,
                    &mDescriptorPoolBindings[descriptorSetIndex],
                    &mEmptyDescriptorSets[descriptorSetIndex]));

                ++mPerfCounters.descriptorSetAllocations[descriptorSetIndex];
            }
            descSet = mEmptyDescriptorSets[descriptorSetIndex];
        }

        // Default uniforms are encompassed in a block per shader stage, and they are assigned
        // through dynamic uniform buffers (requiring dynamic offsets).  No other descriptor
        // requires a dynamic offset.
        if (descriptorSetIndex == DescriptorSetIndex::UniformsAndXfb)
        {
            commandBuffer->bindDescriptorSets(
                getPipelineLayout(), pipelineBindPoint, descriptorSetIndex, 1, &descSet,
                static_cast<uint32_t>(mDynamicUniformDescriptorOffsets.size()),
                mDynamicUniformDescriptorOffsets.data());
        }
        else if (descriptorSetIndex == DescriptorSetIndex::ShaderResource)
        {
            commandBuffer->bindDescriptorSets(
                getPipelineLayout(), pipelineBindPoint, descriptorSetIndex, 1, &descSet,
                static_cast<uint32_t>(mDynamicShaderBufferDescriptorOffsets.size()),
                mDynamicShaderBufferDescriptorOffsets.data());
        }
        else
        {
            commandBuffer->bindDescriptorSets(getPipelineLayout(), pipelineBindPoint,
                                              descriptorSetIndex, 1, &descSet, 0, nullptr);
        }
    }

    return angle::Result::Continue;
}

template angle::Result ProgramExecutableVk::updateDescriptorSets<vk::priv::SecondaryCommandBuffer>(
    vk::Context *context,
    vk::ResourceUseList *resourceUseList,
    vk::priv::SecondaryCommandBuffer *commandBuffer,
    PipelineType pipelineType);
template angle::Result ProgramExecutableVk::updateDescriptorSets<vk::VulkanSecondaryCommandBuffer>(
    vk::Context *context,
    vk::ResourceUseList *resourceUseList,
    vk::VulkanSecondaryCommandBuffer *commandBuffer,
    PipelineType pipelineType);

// Requires that trace is enabled to see the output, which is supported with is_debug=true
void ProgramExecutableVk::outputCumulativePerfCounters()
{
    if (!vk::kOutputCumulativePerfCounters)
    {
        return;
    }

    std::ostringstream text;

    for (DescriptorSetIndex descriptorSetIndex : angle::AllEnums<DescriptorSetIndex>())
    {
        uint32_t count = mCumulativePerfCounters.descriptorSetAllocations[descriptorSetIndex];
        if (count > 0)
        {
            text << "    DescriptorSetIndex " << ToUnderlying(descriptorSetIndex) << ": " << count
                 << "\n";
        }
    }

    // Only output information for programs that allocated descriptor sets.
    std::string textStr = text.str();
    if (!textStr.empty())
    {
        INFO() << "ProgramExecutable: " << this << ":";

        // Output each descriptor set allocation on a single line, so they're prefixed with the
        // INFO information (file, line number, etc.).
        // https://stackoverflow.com/a/12514641
        std::istringstream iss(textStr);
        for (std::string line; std::getline(iss, line);)
        {
            INFO() << line;
        }
    }
}

ProgramExecutablePerfCounters ProgramExecutableVk::getAndResetObjectPerfCounters()
{
    mUniformsAndXfbDescriptorsCache.accumulateCacheStats(VulkanCacheType::UniformsAndXfbDescriptors,
                                                         this);
    mTextureDescriptorsCache.accumulateCacheStats(VulkanCacheType::TextureDescriptors, this);
    mShaderBufferDescriptorsCache.accumulateCacheStats(VulkanCacheType::ShaderBuffersDescriptors,
                                                       this);

    mPerfCounters.descriptorSetCacheKeySizesBytes[DescriptorSetIndex::UniformsAndXfb] =
        static_cast<uint32_t>(mUniformsAndXfbDescriptorsCache.getTotalCacheKeySizeBytes());
    mPerfCounters.descriptorSetCacheKeySizesBytes[DescriptorSetIndex::Texture] =
        static_cast<uint32_t>(mTextureDescriptorsCache.getTotalCacheKeySizeBytes());
    mPerfCounters.descriptorSetCacheKeySizesBytes[DescriptorSetIndex::ShaderResource] =
        static_cast<uint32_t>(mShaderBufferDescriptorsCache.getTotalCacheKeySizeBytes());

    mCumulativePerfCounters.descriptorSetAllocations += mPerfCounters.descriptorSetAllocations;
    mCumulativePerfCounters.descriptorSetCacheHits += mPerfCounters.descriptorSetCacheHits;
    mCumulativePerfCounters.descriptorSetCacheMisses += mPerfCounters.descriptorSetCacheMisses;

    ProgramExecutablePerfCounters counters        = mPerfCounters;
    mPerfCounters.descriptorSetAllocations        = {};
    mPerfCounters.descriptorSetCacheHits          = {};
    mPerfCounters.descriptorSetCacheMisses        = {};
    mPerfCounters.descriptorSetCacheKeySizesBytes = {};
    return counters;
}

void ProgramExecutableVk::accumulateCacheStats(VulkanCacheType cacheType,
                                               const CacheStats &cacheStats)
{
    DescriptorSetIndex dsIndex = CacheTypeToDescriptorSetIndex(cacheType);

    mPerfCounters.descriptorSetCacheHits[dsIndex] +=
        static_cast<uint32_t>(cacheStats.getHitCount());
    mPerfCounters.descriptorSetCacheMisses[dsIndex] +=
        static_cast<uint32_t>(cacheStats.getMissCount());
    mPerfCounters.descriptorSetCacheSizes[dsIndex] = static_cast<uint32_t>(cacheStats.getSize());
}

void ProgramExecutableVk::setAllDefaultUniformsDirty(const gl::ProgramExecutable &executable)
{
    mDefaultUniformBlocksDirty.reset();
    for (gl::ShaderType shaderType : executable.getLinkedShaderStages())
    {
        if (!mDefaultUniformBlocks[shaderType]->uniformData.empty())
        {
            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }
}

angle::Result ProgramExecutableVk::updateUniforms(vk::Context *context,
                                                  UpdateDescriptorSetsBuilder *updateBuilder,
                                                  vk::ResourceUseList *resourceUseList,
                                                  vk::BufferHelper *emptyBuffer,
                                                  const gl::ProgramExecutable &glExecutable,
                                                  vk::DynamicBuffer *defaultUniformStorage,
                                                  bool isTransformFeedbackActiveUnpaused,
                                                  TransformFeedbackVk *transformFeedbackVk)
{
    ASSERT(hasDirtyUniforms());

    vk::BufferHelper *defaultUniformBuffer;
    bool anyNewBufferAllocated          = false;
    gl::ShaderMap<VkDeviceSize> offsets = {};  // offset to the beginning of bufferData
    uint32_t offsetIndex                = 0;
    size_t requiredSpace;

    // We usually only update uniform data for shader stages that are actually dirty. But when the
    // buffer for uniform data have switched, because all shader stages are using the same buffer,
    // we then must update uniform data for all shader stages to keep all shader stages' uniform
    // data in the same buffer.
    requiredSpace = calcUniformUpdateRequiredSpace(context, glExecutable, &offsets);
    ASSERT(requiredSpace > 0);

    // Allocate space from dynamicBuffer. Always try to allocate from the current buffer first.
    // If that failed, we deal with fall out and try again.
    if (!defaultUniformStorage->allocateFromCurrentBuffer(requiredSpace, &defaultUniformBuffer))
    {
        setAllDefaultUniformsDirty(glExecutable);

        requiredSpace = calcUniformUpdateRequiredSpace(context, glExecutable, &offsets);
        ANGLE_TRY(defaultUniformStorage->allocate(context, requiredSpace, &defaultUniformBuffer,
                                                  &anyNewBufferAllocated));
    }

    uint8_t *bufferData       = defaultUniformBuffer->getMappedMemory();
    VkDeviceSize bufferOffset = defaultUniformBuffer->getOffset();
    for (gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
    {
        if (mDefaultUniformBlocksDirty[shaderType])
        {
            const angle::MemoryBuffer &uniformData = mDefaultUniformBlocks[shaderType]->uniformData;
            memcpy(&bufferData[offsets[shaderType]], uniformData.data(), uniformData.size());
            mDynamicUniformDescriptorOffsets[offsetIndex] =
                static_cast<uint32_t>(bufferOffset + offsets[shaderType]);
            mDefaultUniformBlocksDirty.reset(shaderType);
        }
        ++offsetIndex;
    }
    ANGLE_TRY(defaultUniformBuffer->flush(context->getRenderer()));

    // Because the uniform buffers are per context, we can't rely on dynamicBuffer's allocate
    // function to tell us if you have got a new buffer or not. Other program's use of the buffer
    // might already pushed dynamicBuffer to a new buffer. We record which buffer (represented by
    // the unique BufferSerial number) we were using with the current descriptor set and then we
    // use that recorded BufferSerial compare to the current uniform buffer to quickly detect if
    // there is a buffer switch or not. We need to retrieve from the descriptor set cache or
    // allocate a new descriptor set whenever there is uniform buffer switch.
    if (mCurrentDefaultUniformBufferSerial != defaultUniformBuffer->getBufferSerial())
    {
        // We need to reinitialize the descriptor sets if we newly allocated buffers since we can't
        // modify the descriptor sets once initialized.
        vk::DescriptorSetDesc defaultUniformsDesc;
        vk::DescriptorSetDesc *uniformsAndXfbBufferDesc;

        if (glExecutable.hasTransformFeedbackOutput())
        {
            uniformsAndXfbBufferDesc = &transformFeedbackVk->getTransformFeedbackDesc();
            uniformsAndXfbBufferDesc->updateDefaultUniformBuffer(
                defaultUniformBuffer->getBufferSerial());
        }
        else
        {
            defaultUniformsDesc.updateDefaultUniformBuffer(defaultUniformBuffer->getBufferSerial());
            uniformsAndXfbBufferDesc = &defaultUniformsDesc;
        }

        bool newDescriptorSetAllocated;
        ANGLE_TRY(allocUniformAndXfbDescriptorSet(context, resourceUseList, defaultUniformBuffer,
                                                  *uniformsAndXfbBufferDesc,
                                                  &newDescriptorSetAllocated));

        if (newDescriptorSetAllocated)
        {
            for (gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
            {
                updateDefaultUniformsDescriptorSet(
                    context, updateBuilder, emptyBuffer, resourceUseList, shaderType,
                    *mDefaultUniformBlocks[shaderType], defaultUniformBuffer);
                updateTransformFeedbackDescriptorSetImpl(
                    context, updateBuilder, emptyBuffer, glExecutable,
                    isTransformFeedbackActiveUnpaused, transformFeedbackVk);
            }
        }
    }

    return angle::Result::Continue;
}

size_t ProgramExecutableVk::calcUniformUpdateRequiredSpace(
    vk::Context *context,
    const gl::ProgramExecutable &glExecutable,
    gl::ShaderMap<VkDeviceSize> *uniformOffsets) const
{
    size_t requiredSpace = 0;
    for (gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
    {
        if (mDefaultUniformBlocksDirty[shaderType])
        {
            (*uniformOffsets)[shaderType] = requiredSpace;
            requiredSpace += getDefaultUniformAlignedSize(context, shaderType);
        }
    }
    return requiredSpace;
}

void ProgramExecutableVk::onProgramBind(const gl::ProgramExecutable &glExecutable)
{
    // Because all programs share default uniform buffers, when we switch programs, we have to
    // re-update all uniform data. We could do more tracking to avoid update if the context's
    // current uniform buffer is still the same buffer we last time used and buffer has not been
    // recycled. But statistics gathered on gfxbench shows that app always update uniform data on
    // program bind anyway, so not really worth it to add more tracking logic here.
    setAllDefaultUniformsDirty(glExecutable);
}

angle::Result ProgramExecutableVk::resizeUniformBlockMemory(
    ContextVk *contextVk,
    const gl::ProgramExecutable &glExecutable,
    const gl::ShaderMap<size_t> &requiredBufferSize)
{
    for (gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
    {
        if (requiredBufferSize[shaderType] > 0)
        {
            if (!mDefaultUniformBlocks[shaderType]->uniformData.resize(
                    requiredBufferSize[shaderType]))
            {
                ANGLE_VK_CHECK(contextVk, false, VK_ERROR_OUT_OF_HOST_MEMORY);
            }

            // Initialize uniform buffer memory to zero by default.
            mDefaultUniformBlocks[shaderType]->uniformData.fill(0);
            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }

    return angle::Result::Continue;
}
}  // namespace rx
