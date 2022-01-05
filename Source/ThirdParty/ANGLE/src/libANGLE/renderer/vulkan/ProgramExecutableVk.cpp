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
    const gl::ShaderType lastPreFragmentStage = gl::GetLastPreFragmentStage(linkedShaderStages);

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
    ASSERT(!valid());

    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
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

void ShaderInfo::release(ContextVk *contextVk)
{
    for (angle::spirv::Blob &spirvBlob : mSpirvBlobs)
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
    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
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
                                       const gl::ShaderType shaderType,
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
    options.removeDebugInfo          = !contextVk->getRenderer()->getEnableValidationLayers();
    options.isTransformFeedbackStage = isLastPreFragmentStage && isTransformFeedbackProgram &&
                                       !optionBits.removeTransformFeedbackEmulation;
    options.isTransformFeedbackEmulated = contextVk->getFeatures().emulateTransformFeedback.enabled;
    options.negativeViewportSupported   = contextVk->getFeatures().supportsNegativeViewport.enabled;

    if (isLastPreFragmentStage)
    {
        options.preRotation = static_cast<SurfaceRotation>(optionBits.surfaceRotation);
        options.transformPositionToVulkanClipSpace = optionBits.enableDepthCorrection;
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
      mProgram(nullptr),
      mProgramPipeline(nullptr),
      mPerfCounters{},
      mCumulativePerfCounters{}
{}

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
    mTextureDescriptorsCache.destroy(rendererVk);
    mUniformsAndXfbDescriptorsCache.destroy(rendererVk);
    mShaderBufferDescriptorsCache.destroy(rendererVk);

    // Initialize with a unique BufferSerial
    vk::ResourceSerialFactory &factory = rendererVk->getResourceSerialFactory();
    mCurrentDefaultUniformBufferSerial = factory.generateBufferSerial();

    for (ProgramInfo &programInfo : mGraphicsProgramInfos)
    {
        programInfo.release(contextVk);
    }
    mComputeProgramInfo.release(contextVk);

    contextVk->onProgramExecutableReset(this);
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

    return std::make_unique<LinkEventDone>(angle::Result::Continue);
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
}

void ProgramExecutableVk::clearVariableInfoMap()
{
    mVariableInfoMap.clear();
}

ProgramVk *ProgramExecutableVk::getShaderProgram(const gl::State &glState,
                                                 gl::ShaderType shaderType) const
{
    if (mProgram)
    {
        const gl::ProgramExecutable &glExecutable = mProgram->getState().getExecutable();
        if (glExecutable.hasLinkedShaderStage(shaderType))
        {
            return mProgram;
        }
    }
    else if (mProgramPipeline)
    {
        return mProgramPipeline->getShaderProgram(shaderType);
    }

    return nullptr;
}

// TODO: http://anglebug.com/3570: Move/Copy all of the necessary information into
// the ProgramExecutable, so this function can be removed.
void ProgramExecutableVk::fillProgramStateMap(
    const ContextVk *contextVk,
    gl::ShaderMap<const gl::ProgramState *> *programStatesOut)
{
    ASSERT(mProgram || mProgramPipeline);
    if (mProgram)
    {
        mProgram->fillProgramStateMap(programStatesOut);
    }
    else if (mProgramPipeline)
    {
        mProgramPipeline->fillProgramStateMap(programStatesOut);
    }
}

const gl::ProgramExecutable &ProgramExecutableVk::getGlExecutable()
{
    ASSERT(mProgram || mProgramPipeline);
    if (mProgram)
    {
        return mProgram->getState().getExecutable();
    }
    return mProgramPipeline->getState().getExecutable();
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
    ContextVk *contextVk,
    const vk::UniformsAndXfbDescriptorDesc &xfbBufferDesc,
    bool *newDescriptorSetAllocated)
{
    mCurrentDefaultUniformBufferSerial = xfbBufferDesc.getDefaultUniformBufferSerial();

    // Look up in the cache first
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    if (mUniformsAndXfbDescriptorsCache.get(xfbBufferDesc, &descriptorSet))
    {
        *newDescriptorSetAllocated                          = false;
        mDescriptorSets[DescriptorSetIndex::UniformsAndXfb] = descriptorSet;
        // The descriptor pool that this descriptor set was allocated from needs to be retained each
        // time the descriptor set is used in a new command.
        mDescriptorPoolBindings[DescriptorSetIndex::UniformsAndXfb].get().retain(
            &contextVk->getResourceUseList());
        return angle::Result::Continue;
    }

    bool newPoolAllocated;
    ANGLE_TRY(allocateDescriptorSetAndGetInfo(contextVk, DescriptorSetIndex::UniformsAndXfb,
                                              &newPoolAllocated));

    // Clear descriptor set cache. It may no longer be valid.
    if (newPoolAllocated)
    {
        mUniformsAndXfbDescriptorsCache.destroy(contextVk->getRenderer());
    }

    // Add the descriptor set into cache
    mUniformsAndXfbDescriptorsCache.insert(xfbBufferDesc,
                                           mDescriptorSets[DescriptorSetIndex::UniformsAndXfb]);
    *newDescriptorSetAllocated = true;

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::allocateDescriptorSet(ContextVk *contextVk,
                                                         DescriptorSetIndex descriptorSetIndex)
{
    bool ignoreNewPoolAllocated;
    return allocateDescriptorSetAndGetInfo(contextVk, descriptorSetIndex, &ignoreNewPoolAllocated);
}

angle::Result ProgramExecutableVk::allocateDescriptorSetAndGetInfo(
    ContextVk *contextVk,
    DescriptorSetIndex descriptorSetIndex,
    bool *newPoolAllocatedOut)
{
    vk::DynamicDescriptorPool &dynamicDescriptorPool = mDynamicDescriptorPools[descriptorSetIndex];

    const vk::DescriptorSetLayout &descriptorSetLayout =
        mDescriptorSetLayouts[descriptorSetIndex].get();
    ANGLE_TRY(dynamicDescriptorPool.allocateSetsAndGetInfo(
        contextVk, descriptorSetLayout.ptr(), 1, &mDescriptorPoolBindings[descriptorSetIndex],
        &mDescriptorSets[descriptorSetIndex], newPoolAllocatedOut));
    mEmptyDescriptorSets[descriptorSetIndex] = VK_NULL_HANDLE;

    ++mPerfCounters.descriptorSetAllocations[descriptorSetIndex];

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
    const gl::ShaderType shaderType,
    vk::DescriptorSetLayoutDesc *descOut)
{
    if (atomicCounterBuffers.empty())
    {
        return;
    }

    std::string blockName(sh::vk::kAtomicCountersBlockName);

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

        for (const gl::ShaderType shaderType : executable.getLinkedShaderStages())
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
    const gl::ShaderType shaderType,
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

void ProgramExecutableVk::addTextureDescriptorSetDesc(
    ContextVk *contextVk,
    const gl::ProgramState &programState,
    const gl::ActiveTextureArray<vk::TextureUnit> *activeTextures,
    vk::DescriptorSetLayoutDesc *descOut)
{
    const std::vector<gl::SamplerBinding> &samplerBindings = programState.getSamplerBindings();
    const std::vector<gl::LinkedUniform> &uniforms         = programState.getUniforms();

    for (uint32_t textureIndex = 0; textureIndex < samplerBindings.size(); ++textureIndex)
    {
        const gl::SamplerBinding &samplerBinding = samplerBindings[textureIndex];

        uint32_t uniformIndex = programState.getUniformIndexFromSamplerIndex(textureIndex);
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

        for (const gl::ShaderType shaderType : programState.getExecutable().getLinkedShaderStages())
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
                mImmutableSamplerIndexMap[*textureVk->getImage().getYcbcrConversionDesc()] =
                    textureIndex;
                // The Vulkan spec has the following note -
                // All descriptors in a binding use the same maximum
                // combinedImageSamplerDescriptorCount descriptors to allow implementations to use a
                // uniform stride for dynamic indexing of the descriptors in the binding.
                uint64_t externalFormat        = textureVk->getImage().getExternalFormat();
                VkFormat vkFormat              = textureVk->getImage().getActualVkFormat();
                uint32_t formatDescriptorCount = 0;
                angle::Result result           = angle::Result::Stop;

                if (externalFormat != 0)
                {
                    result = contextVk->getRenderer()->getFormatDescriptorCountForExternalFormat(
                        contextVk, externalFormat, &formatDescriptorCount);
                }
                else
                {
                    ASSERT(vkFormat != 0);
                    result = contextVk->getRenderer()->getFormatDescriptorCountForVkFormat(
                        contextVk, vkFormat, &formatDescriptorCount);
                }

                if (result != angle::Result::Continue)
                {
                    // There was an error querying the descriptor count for this format, treat it as
                    // a non-fatal error and move on.
                    formatDescriptorCount = 1;
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

void ProgramExecutableVk::updateEarlyFragmentTestsOptimization(ContextVk *contextVk)
{
    const gl::State &glState = contextVk->getState();

    mTransformOptions.removeEarlyFragmentTestsOptimization = false;
    if (!glState.canEnableEarlyFragmentTestsOptimization())
    {
        ProgramVk *programVk = getShaderProgram(glState, gl::ShaderType::Fragment);
        if (programVk && programVk->getState().hasEarlyFragmentTestsOptimization())
        {
            mTransformOptions.removeEarlyFragmentTestsOptimization = true;
        }
    }
}

angle::Result ProgramExecutableVk::getGraphicsPipeline(ContextVk *contextVk,
                                                       gl::PrimitiveMode mode,
                                                       const vk::GraphicsPipelineDesc &desc,
                                                       const vk::GraphicsPipelineDesc **descPtrOut,
                                                       vk::PipelineHelper **pipelineOut)
{
    const gl::State &glState                  = contextVk->getState();
    RendererVk *renderer                      = contextVk->getRenderer();
    vk::PipelineCache *pipelineCache          = nullptr;
    const gl::ProgramExecutable *glExecutable = glState.getProgramExecutable();
    ASSERT(glExecutable && glExecutable->hasLinkedShaderStage(gl::ShaderType::Vertex));

    mTransformOptions.enableLineRasterEmulation = contextVk->isBresenhamEmulationEnabled(mode);
    mTransformOptions.surfaceRotation           = ToUnderlying(desc.getSurfaceRotation());
    mTransformOptions.enableDepthCorrection     = !glState.isClipControlDepthZeroToOne();
    mTransformOptions.removeTransformFeedbackEmulation =
        contextVk->getFeatures().emulateTransformFeedback.enabled &&
        !glState.isTransformFeedbackActiveUnpaused();

    // This must be called after mTransformOptions have been set.
    ProgramInfo &programInfo                  = getGraphicsProgramInfo(mTransformOptions);
    const gl::ShaderBitSet linkedShaderStages = glExecutable->getLinkedShaderStages();
    const gl::ShaderType lastPreFragmentStage = gl::GetLastPreFragmentStage(linkedShaderStages);

    for (const gl::ShaderType shaderType : linkedShaderStages)
    {
        ProgramVk *programVk = getShaderProgram(glState, shaderType);
        if (programVk)
        {
            ANGLE_TRY(programVk->initGraphicsShaderProgram(
                contextVk, shaderType, shaderType == lastPreFragmentStage, mTransformOptions,
                &programInfo, mVariableInfoMap));
        }
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

    // Compare the fragment output interface with the framebuffer interface.
    const gl::ProgramExecutable &executable = *glState.getProgramExecutable();

    const gl::AttributesMask &activeAttribLocations = executable.getNonBuiltinAttribLocationsMask();

    // Calculate missing shader outputs.
    const gl::DrawBufferMask &shaderOutMask = executable.getActiveOutputVariablesMask();
    gl::DrawBufferMask framebufferMask      = glState.getDrawFramebuffer()->getDrawBufferMask();
    gl::DrawBufferMask missingOutputsMask   = ~shaderOutMask & framebufferMask;

    ANGLE_TRY(renderer->getPipelineCache(&pipelineCache));
    return shaderProgram->getGraphicsPipeline(
        contextVk, &contextVk->getRenderPassCache(), *pipelineCache, getPipelineLayout(), desc,
        activeAttribLocations, executable.getAttributesTypeMask(), missingOutputsMask, descPtrOut,
        pipelineOut);
}

angle::Result ProgramExecutableVk::getComputePipeline(ContextVk *contextVk,
                                                      vk::PipelineHelper **pipelineOut)
{
    const gl::State &glState                  = contextVk->getState();
    const gl::ProgramExecutable *glExecutable = glState.getProgramExecutable();
    ASSERT(glExecutable && glExecutable->hasLinkedShaderStage(gl::ShaderType::Compute));

    ProgramVk *programVk = getShaderProgram(glState, gl::ShaderType::Compute);
    ASSERT(programVk);
    ProgramInfo &programInfo = getComputeProgramInfo();
    ANGLE_TRY(programVk->initComputeProgram(contextVk, &programInfo, mVariableInfoMap));

    vk::ShaderProgramHelper *shaderProgram = programInfo.getShaderProgram();
    ASSERT(shaderProgram);
    return shaderProgram->getComputePipeline(contextVk, getPipelineLayout(), pipelineOut);
}

angle::Result ProgramExecutableVk::initDynamicDescriptorPools(
    ContextVk *contextVk,
    vk::DescriptorSetLayoutDesc &descriptorSetLayoutDesc,
    DescriptorSetIndex descriptorSetIndex,
    VkDescriptorSetLayout descriptorSetLayout)
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
            poolSize.descriptorCount =
                binding.descriptorCount * mImmutableSamplersMaxDescriptorCount;
            descriptorPoolSizes.emplace_back(poolSize);
        }
    }

    RendererVk *renderer = contextVk->getRenderer();
    if (renderer->getFeatures().bindEmptyForUnusedDescriptorSets.enabled &&
        descriptorPoolSizes.empty())
    {
        // For this workaround, we have to create an empty descriptor set for each descriptor set
        // index, so make sure their pools are initialized.
        VkDescriptorPoolSize poolSize = {};
        // The type doesn't matter, since it's not actually used for anything.
        poolSize.type            = mUniformBufferDescriptorType;
        poolSize.descriptorCount = 1;
        descriptorPoolSizes.emplace_back(poolSize);
    }

    if (!descriptorPoolSizes.empty())
    {
        ANGLE_TRY(mDynamicDescriptorPools[descriptorSetIndex].init(
            contextVk, descriptorPoolSizes.data(), descriptorPoolSizes.size(),
            descriptorSetLayout));
    }

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::createPipelineLayout(
    ContextVk *contextVk,
    const gl::ProgramExecutable &glExecutable,
    gl::ActiveTextureArray<vk::TextureUnit> *activeTextures)
{
    gl::TransformFeedback *transformFeedback = contextVk->getState().getCurrentTransformFeedback();
    const gl::ShaderBitSet &linkedShaderStages = glExecutable.getLinkedShaderStages();
    gl::ShaderMap<const gl::ProgramState *> programStates;
    fillProgramStateMap(contextVk, &programStates);

    reset(contextVk);

    // Store a reference to the pipeline and descriptor set layouts. This will create them if they
    // don't already exist in the cache.

    // Default uniforms and transform feedback:
    vk::DescriptorSetLayoutDesc uniformsAndXfbSetDesc;
    mNumDefaultUniformDescriptors = 0;
    for (const gl::ShaderType shaderType : linkedShaderStages)
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
    bool hasXfbVaryings =
        linkedTransformFeedbackStage != gl::ShaderType::InvalidEnum &&
        !programStates[linkedTransformFeedbackStage]->getLinkedTransformFeedbackVaryings().empty();
    if (transformFeedback && hasXfbVaryings)
    {
        const gl::ProgramExecutable &executable =
            programStates[linkedTransformFeedbackStage]->getExecutable();
        size_t xfbBufferCount                    = executable.getTransformFeedbackBufferCount();
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
    for (const gl::ShaderType shaderType : linkedShaderStages)
    {
        const gl::ProgramState *programState = programStates[shaderType];
        ASSERT(programState);

        const std::vector<gl::InterfaceBlock> &blocks = programState->getUniformBlocks();
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

    for (const gl::ShaderType shaderType : linkedShaderStages)
    {
        const gl::ProgramState *programState = programStates[shaderType];
        ASSERT(programState);

        addInterfaceBlockDescriptorSetDesc(programState->getUniformBlocks(), shaderType,
                                           mUniformBufferDescriptorType, &resourcesSetDesc);
        addInterfaceBlockDescriptorSetDesc(programState->getShaderStorageBlocks(), shaderType,
                                           kStorageBufferDescriptorType, &resourcesSetDesc);
        addAtomicCounterBufferDescriptorSetDesc(programState->getAtomicCounterBuffers(), shaderType,
                                                &resourcesSetDesc);
    }

    for (const gl::ShaderType shaderType : linkedShaderStages)
    {
        const gl::ProgramState *programState = programStates[shaderType];
        ASSERT(programState);
        addImageDescriptorSetDesc(programState->getExecutable(), &resourcesSetDesc);
        addInputAttachmentDescriptorSetDesc(programState->getExecutable(), shaderType,
                                            &resourcesSetDesc);
    }

    ANGLE_TRY(contextVk->getDescriptorSetLayoutCache().getDescriptorSetLayout(
        contextVk, resourcesSetDesc, &mDescriptorSetLayouts[DescriptorSetIndex::ShaderResource]));

    // Textures:
    vk::DescriptorSetLayoutDesc texturesSetDesc;

    for (const gl::ShaderType shaderType : linkedShaderStages)
    {
        const gl::ProgramState *programState = programStates[shaderType];
        ASSERT(programState);
        addTextureDescriptorSetDesc(contextVk, *programState, activeTextures, &texturesSetDesc);
    }

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
    ANGLE_TRY(initDynamicDescriptorPools(
        contextVk, uniformsAndXfbSetDesc, DescriptorSetIndex::UniformsAndXfb,
        mDescriptorSetLayouts[DescriptorSetIndex::UniformsAndXfb].get().getHandle()));
    ANGLE_TRY(initDynamicDescriptorPools(
        contextVk, resourcesSetDesc, DescriptorSetIndex::ShaderResource,
        mDescriptorSetLayouts[DescriptorSetIndex::ShaderResource].get().getHandle()));
    ANGLE_TRY(initDynamicDescriptorPools(
        contextVk, texturesSetDesc, DescriptorSetIndex::Texture,
        mDescriptorSetLayouts[DescriptorSetIndex::Texture].get().getHandle()));
    ANGLE_TRY(initDynamicDescriptorPools(
        contextVk, driverUniformsSetDesc, DescriptorSetIndex::Internal,
        mDescriptorSetLayouts[DescriptorSetIndex::Internal].get().getHandle()));

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
    const gl::ShaderType shaderType,
    const DefaultUniformBlock &defaultUniformBlock,
    vk::BufferHelper *defaultUniformBuffer,
    ContextVk *contextVk)
{
    const std::string uniformBlockName = kDefaultUniformNames[shaderType];

    const ShaderInterfaceVariableInfo &info = mVariableInfoMap.get(shaderType, uniformBlockName);
    if (info.isDuplicate || !info.activeStages[shaderType])
    {
        return;
    }

    VkWriteDescriptorSet &writeInfo    = contextVk->allocWriteDescriptorSet();
    VkDescriptorBufferInfo &bufferInfo = contextVk->allocDescriptorBufferInfo();

    // Size is set to the size of the empty buffer for shader stages with no uniform data,
    // otherwise it is set to the total size of the uniform data in the current shader stage
    VkDeviceSize size              = defaultUniformBlock.uniformData.size();
    vk::BufferHelper *bufferHelper = defaultUniformBuffer;
    if (defaultUniformBlock.uniformData.empty())
    {
        bufferHelper = &contextVk->getEmptyBuffer();
        bufferHelper->retainReadOnly(&contextVk->getResourceUseList());
        size = bufferHelper->getSize();
    }

    WriteBufferDescriptorSetBinding(
        *bufferHelper, 0, size, mDescriptorSets[DescriptorSetIndex::UniformsAndXfb],
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, info.binding, 0, 0, &bufferInfo, &writeInfo);
}

// Lazily allocate the descriptor set. We may not need one if all of the buffers are inactive.
angle::Result ProgramExecutableVk::getOrAllocateShaderResourcesDescriptorSet(
    ContextVk *contextVk,
    const vk::ShaderBuffersDescriptorDesc *shaderBuffersDesc,
    VkDescriptorSet *descriptorSetOut)
{
    if (mDescriptorSets[DescriptorSetIndex::ShaderResource] == VK_NULL_HANDLE)
    {
        bool newPoolAllocated = false;
        ANGLE_TRY(allocateDescriptorSetAndGetInfo(contextVk, DescriptorSetIndex::ShaderResource,
                                                  &newPoolAllocated));

        if (shaderBuffersDesc)
        {
            // Clear descriptor set cache. It may no longer be valid.
            if (newPoolAllocated)
            {
                mShaderBufferDescriptorsCache.destroy(contextVk->getRenderer());
            }

            mShaderBufferDescriptorsCache.insert(
                *shaderBuffersDesc, mDescriptorSets[DescriptorSetIndex::ShaderResource]);
        }
    }
    *descriptorSetOut = mDescriptorSets[DescriptorSetIndex::ShaderResource];
    ASSERT(*descriptorSetOut != VK_NULL_HANDLE);
    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::updateBuffersDescriptorSet(
    ContextVk *contextVk,
    const gl::ShaderType shaderType,
    const vk::ShaderBuffersDescriptorDesc &shaderBuffersDesc,
    const std::vector<gl::InterfaceBlock> &blocks,
    VkDescriptorType descriptorType,
    bool cacheHit)
{
    // Early exit if no blocks or no update needed.
    if (blocks.empty() || (cacheHit && !IsDynamicDescriptor(descriptorType)))
    {
        return angle::Result::Continue;
    }

    ASSERT(descriptorType == mUniformBufferDescriptorType ||
           descriptorType == kStorageBufferDescriptorType);
    const bool isStorageBuffer = descriptorType == kStorageBufferDescriptorType;

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

        const ShaderInterfaceVariableInfo &info =
            mVariableInfoMap.get(shaderType, block.mappedName);
        if (info.isDuplicate)
        {
            continue;
        }

        uint32_t binding      = info.binding;
        uint32_t arrayElement = block.isArray ? block.arrayElement : 0;

        VkDeviceSize size;
        if (!isStorageBuffer)
        {
            size = block.dataSize;
        }
        else
        {
            size = gl::GetBoundBufferAvailableSize(bufferBinding);
        }

        // Make sure there's no possible under/overflow with binding size.
        static_assert(sizeof(VkDeviceSize) >= sizeof(bufferBinding.getSize()),
                      "VkDeviceSize too small");
        ASSERT(bufferBinding.getSize() >= 0);

        BufferVk *bufferVk             = vk::GetImpl(bufferBinding.get());
        vk::BufferHelper &bufferHelper = bufferVk->getBuffer();

        if (!cacheHit)
        {
            VkDescriptorBufferInfo &bufferInfo = contextVk->allocDescriptorBufferInfo();
            VkWriteDescriptorSet &writeInfo    = contextVk->allocWriteDescriptorSet();

            VkDescriptorSet descriptorSet;
            ANGLE_TRY(getOrAllocateShaderResourcesDescriptorSet(contextVk, &shaderBuffersDesc,
                                                                &descriptorSet));
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
    ContextVk *contextVk,
    const gl::ProgramState &programState,
    const gl::ShaderType shaderType,
    const vk::ShaderBuffersDescriptorDesc &shaderBuffersDesc,
    bool cacheHit)
{
    const gl::State &glState = contextVk->getState();
    const std::vector<gl::AtomicCounterBuffer> &atomicCounterBuffers =
        programState.getAtomicCounterBuffers();

    if (atomicCounterBuffers.empty() || cacheHit)
    {
        return angle::Result::Continue;
    }

    std::string blockName(sh::vk::kAtomicCountersBlockName);
    const ShaderInterfaceVariableInfo &info = mVariableInfoMap.get(shaderType, blockName);
    if (info.isDuplicate || !info.activeStages[shaderType])
    {
        return angle::Result::Continue;
    }

    gl::AtomicCounterBufferMask writtenBindings;

    RendererVk *rendererVk = contextVk->getRenderer();
    const VkDeviceSize requiredOffsetAlignment =
        rendererVk->getPhysicalDeviceProperties().limits.minStorageBufferOffsetAlignment;

    VkDescriptorSet descriptorSet;
    ANGLE_TRY(
        getOrAllocateShaderResourcesDescriptorSet(contextVk, &shaderBuffersDesc, &descriptorSet));

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

        static_assert(!IsDynamicDescriptor(kStorageBufferDescriptorType),
                      "updateAtomicCounterBuffersDescriptorSet needs an update to handle dynamic "
                      "descriptors");

        VkDescriptorBufferInfo &bufferInfo = contextVk->allocDescriptorBufferInfo();
        VkWriteDescriptorSet &writeInfo    = contextVk->allocWriteDescriptorSet();

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
    vk::BufferHelper &emptyBuffer = contextVk->getEmptyBuffer();
    emptyBuffer.retainReadOnly(&contextVk->getResourceUseList());
    size_t count                        = (~writtenBindings).count();
    VkDescriptorBufferInfo *bufferInfos = contextVk->allocDescriptorBufferInfos(count);
    VkWriteDescriptorSet *writeInfos    = contextVk->allocWriteDescriptorSets(count);
    size_t writeCount                   = 0;
    for (size_t binding : ~writtenBindings)
    {
        WriteBufferDescriptorSetBinding(emptyBuffer, 0, emptyBuffer.getSize(), descriptorSet,
                                        kStorageBufferDescriptorType, info.binding,
                                        static_cast<uint32_t>(binding), 0, &bufferInfos[writeCount],
                                        &writeInfos[writeCount]);
        writeCount++;
    }

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::updateImagesDescriptorSet(
    ContextVk *contextVk,
    const gl::ProgramExecutable &executable,
    const gl::ShaderType shaderType)
{
    const gl::State &glState                           = contextVk->getState();
    RendererVk *renderer                               = contextVk->getRenderer();
    const std::vector<gl::ImageBinding> &imageBindings = executable.getImageBindings();
    const std::vector<gl::LinkedUniform> &uniforms     = executable.getUniforms();

    if (imageBindings.empty())
    {
        return angle::Result::Continue;
    }

    const gl::ActiveTextureArray<TextureVk *> &activeImages = contextVk->getActiveImages();

    angle::HashMap<std::string, uint32_t> mappedImageNameToArrayOffset;

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

        VkDescriptorSet descriptorSet;
        ANGLE_TRY(getOrAllocateShaderResourcesDescriptorSet(contextVk, nullptr, &descriptorSet));

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
            // Handle format reinterpration by looking for a view with the format specified in
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

            VkWriteDescriptorSet *writeInfos = contextVk->allocWriteDescriptorSets(arraySize);

            for (uint32_t arrayElement = 0; arrayElement < arraySize; ++arrayElement)
            {
                GLuint imageUnit     = imageBinding.boundImageUnits[arrayElement];
                TextureVk *textureVk = activeImages[imageUnit];

                const vk::BufferView *view = nullptr;
                ANGLE_TRY(textureVk->getBufferViewAndRecordUse(contextVk, format, true, &view));

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

        VkWriteDescriptorSet *writeInfos  = contextVk->allocWriteDescriptorSets(arraySize);
        VkDescriptorImageInfo *imageInfos = contextVk->allocDescriptorImageInfos(arraySize);
        for (uint32_t arrayElement = 0; arrayElement < arraySize; ++arrayElement)
        {
            GLuint imageUnit             = imageBinding.boundImageUnits[arrayElement];
            const gl::ImageUnit &binding = glState.getImageUnit(imageUnit);
            TextureVk *textureVk         = activeImages[imageUnit];

            vk::ImageHelper *image         = &textureVk->getImage();
            const vk::ImageView *imageView = nullptr;

            ANGLE_TRY(textureVk->getStorageImageView(contextVk, binding, &imageView));

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
    FramebufferVk *framebufferVk,
    const vk::ShaderBuffersDescriptorDesc &shaderBuffersDesc,
    vk::CommandBufferHelper *commandBufferHelper)
{
    const gl::ProgramExecutable *executable = contextVk->getState().getProgramExecutable();
    ASSERT(executable);

    // Reset the descriptor set handles so we only allocate a new one when necessary.
    mDescriptorSets[DescriptorSetIndex::ShaderResource]      = VK_NULL_HANDLE;
    mEmptyDescriptorSets[DescriptorSetIndex::ShaderResource] = VK_NULL_HANDLE;
    mDynamicShaderBufferDescriptorOffsets.clear();

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
        }
    }

    bool cacheHit = mDescriptorSets[DescriptorSetIndex::ShaderResource] != VK_NULL_HANDLE;

    gl::ShaderMap<const gl::ProgramState *> programStates;
    fillProgramStateMap(contextVk, &programStates);

    for (const gl::ShaderType shaderType : executable->getLinkedShaderStages())
    {
        const gl::ProgramState *programState = programStates[shaderType];
        ASSERT(programState);

        ANGLE_TRY(updateBuffersDescriptorSet(contextVk, shaderType, shaderBuffersDesc,
                                             programState->getUniformBlocks(),
                                             mUniformBufferDescriptorType, cacheHit));
        ANGLE_TRY(updateBuffersDescriptorSet(contextVk, shaderType, shaderBuffersDesc,
                                             programState->getShaderStorageBlocks(),
                                             kStorageBufferDescriptorType, cacheHit));
        ANGLE_TRY(updateAtomicCounterBuffersDescriptorSet(contextVk, *programState, shaderType,
                                                          shaderBuffersDesc, cacheHit));
        ANGLE_TRY(updateImagesDescriptorSet(contextVk, programState->getExecutable(), shaderType));
        ANGLE_TRY(updateInputAttachmentDescriptorSet(programState->getExecutable(), shaderType,
                                                     contextVk, framebufferVk));
    }

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::updateInputAttachmentDescriptorSet(
    const gl::ProgramExecutable &executable,
    const gl::ShaderType shaderType,
    ContextVk *contextVk,
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

    for (size_t colorIndex : framebufferVk->getState().getColorAttachmentsMask())
    {
        VkDescriptorSet descriptorSet;
        ANGLE_TRY(getOrAllocateShaderResourcesDescriptorSet(contextVk, nullptr, &descriptorSet));

        VkWriteDescriptorSet *writeInfos  = contextVk->allocWriteDescriptorSets(1);
        VkDescriptorImageInfo *imageInfos = contextVk->allocDescriptorImageInfos(1);
        RenderTargetVk *renderTargetVk    = framebufferVk->getColorDrawRenderTarget(colorIndex);
        const vk::ImageView *imageView    = nullptr;

        ANGLE_TRY(renderTargetVk->getImageView(contextVk, &imageView));

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

angle::Result ProgramExecutableVk::updateTransformFeedbackDescriptorSet(
    const gl::ProgramState &programState,
    gl::ShaderMap<DefaultUniformBlock> &defaultUniformBlocks,
    vk::BufferHelper *defaultUniformBuffer,
    ContextVk *contextVk,
    const vk::UniformsAndXfbDescriptorDesc &xfbBufferDesc)
{
    const gl::ProgramExecutable &executable = programState.getExecutable();
    ASSERT(executable.hasTransformFeedbackOutput());

    bool newDescriptorSetAllocated;
    ANGLE_TRY(
        allocUniformAndXfbDescriptorSet(contextVk, xfbBufferDesc, &newDescriptorSetAllocated));

    if (newDescriptorSetAllocated)
    {
        for (const gl::ShaderType shaderType : executable.getLinkedShaderStages())
        {
            updateDefaultUniformsDescriptorSet(shaderType, defaultUniformBlocks[shaderType],
                                               defaultUniformBuffer, contextVk);
        }
        updateTransformFeedbackDescriptorSetImpl(programState, contextVk);
    }

    return angle::Result::Continue;
}

void ProgramExecutableVk::updateTransformFeedbackDescriptorSetImpl(
    const gl::ProgramState &programState,
    ContextVk *contextVk)
{
    const gl::State &glState                 = contextVk->getState();
    gl::TransformFeedback *transformFeedback = glState.getCurrentTransformFeedback();
    const gl::ProgramExecutable &executable  = programState.getExecutable();

    if (!executable.hasTransformFeedbackOutput())
    {
        // If xfb has no output there is no need to update descriptor set.
        return;
    }
    if (!glState.isTransformFeedbackActiveUnpaused())
    {
        // We set empty Buffer to xfb descriptor set because xfb descriptor set
        // requires valid buffer bindings, even if they are empty buffer,
        // otherwise Vulkan validation layer generates errors.
        if (transformFeedback)
        {
            TransformFeedbackVk *transformFeedbackVk = vk::GetImpl(transformFeedback);
            transformFeedbackVk->initDescriptorSet(
                contextVk, mVariableInfoMap, executable.getTransformFeedbackBufferCount(),
                mDescriptorSets[DescriptorSetIndex::UniformsAndXfb]);
        }
        return;
    }

    TransformFeedbackVk *transformFeedbackVk = vk::GetImpl(glState.getCurrentTransformFeedback());
    transformFeedbackVk->updateDescriptorSet(contextVk, programState, mVariableInfoMap,
                                             mDescriptorSets[DescriptorSetIndex::UniformsAndXfb]);
}

angle::Result ProgramExecutableVk::updateTexturesDescriptorSet(
    ContextVk *contextVk,
    const vk::TextureDescriptorDesc &texturesDesc)
{
    const gl::ProgramExecutable *executable = contextVk->getState().getProgramExecutable();
    ASSERT(executable);

    if (!executable->hasTextures())
    {
        return angle::Result::Continue;
    }

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    if (mTextureDescriptorsCache.get(texturesDesc, &descriptorSet))
    {
        mDescriptorSets[DescriptorSetIndex::Texture] = descriptorSet;
        // The descriptor pool that this descriptor set was allocated from needs to be retained each
        // time the descriptor set is used in a new command.
        mDescriptorPoolBindings[DescriptorSetIndex::Texture].get().retain(
            &contextVk->getResourceUseList());
        return angle::Result::Continue;
    }

    const gl::ActiveTextureArray<vk::TextureUnit> &activeTextures = contextVk->getActiveTextures();
    bool emulateSeamfulCubeMapSampling = contextVk->emulateSeamfulCubeMapSampling();

    gl::ShaderMap<const gl::ProgramState *> programStates;
    fillProgramStateMap(contextVk, &programStates);

    for (const gl::ShaderType shaderType : executable->getLinkedShaderStages())
    {
        angle::HashMap<std::string, uint32_t> mappedSamplerNameToArrayOffset;
        const gl::ProgramState *programState = programStates[shaderType];
        ASSERT(programState);
        for (uint32_t textureIndex = 0; textureIndex < programState->getSamplerBindings().size();
             ++textureIndex)
        {
            const gl::SamplerBinding &samplerBinding =
                programState->getSamplerBindings()[textureIndex];
            uint32_t uniformIndex = programState->getUniformIndexFromSamplerIndex(textureIndex);
            const gl::LinkedUniform &samplerUniform = programState->getUniforms()[uniformIndex];
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
                ANGLE_TRY(allocateDescriptorSetAndGetInfo(contextVk, DescriptorSetIndex::Texture,
                                                          &newPoolAllocated));

                // Clear descriptor set cache. It may no longer be valid.
                if (newPoolAllocated)
                {
                    mTextureDescriptorsCache.destroy(contextVk->getRenderer());
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

            VkWriteDescriptorSet *writeInfos = contextVk->allocWriteDescriptorSets(arraySize);

            // Texture buffers use buffer views, so they are especially handled.
            if (samplerBinding.textureType == gl::TextureType::Buffer)
            {
                for (uint32_t arrayElement = 0; arrayElement < arraySize; ++arrayElement)
                {
                    GLuint textureUnit         = samplerBinding.boundTextureUnits[arrayElement];
                    TextureVk *textureVk       = activeTextures[textureUnit].texture;
                    const vk::BufferView *view = nullptr;
                    ANGLE_TRY(
                        textureVk->getBufferViewAndRecordUse(contextVk, nullptr, false, &view));

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

            VkDescriptorImageInfo *imageInfos = contextVk->allocDescriptorImageInfos(arraySize);
            for (uint32_t arrayElement = 0; arrayElement < arraySize; ++arrayElement)
            {
                GLuint textureUnit          = samplerBinding.boundTextureUnits[arrayElement];
                const vk::TextureUnit &unit = activeTextures[textureUnit];
                TextureVk *textureVk        = unit.texture;
                const vk::SamplerHelper &samplerHelper = *unit.sampler;

                vk::ImageHelper &image = textureVk->getImage();

                imageInfos[arrayElement].sampler     = samplerHelper.get().getHandle();
                imageInfos[arrayElement].imageLayout = image.getCurrentLayout();

                if (emulateSeamfulCubeMapSampling)
                {
                    // If emulating seamful cubemapping, use the fetch image view.  This is
                    // basically the same image view as read, except it's a 2DArray view for
                    // cube maps.
                    const vk::ImageView &imageView = textureVk->getFetchImageViewAndRecordUse(
                        contextVk, unit.srgbDecode, samplerUniform.texelFetchStaticUse);
                    imageInfos[arrayElement].imageView = imageView.getHandle();
                }
                else
                {
                    const vk::ImageView &imageView = textureVk->getReadImageViewAndRecordUse(
                        contextVk, unit.srgbDecode, samplerUniform.texelFetchStaticUse);
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

angle::Result ProgramExecutableVk::updateDescriptorSets(ContextVk *contextVk,
                                                        vk::CommandBuffer *commandBuffer,
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
    mUniformsAndXfbDescriptorsCache.accumulateCacheStats(this);
    mTextureDescriptorsCache.accumulateCacheStats(this);
    mShaderBufferDescriptorsCache.accumulateCacheStats(this);

    mCumulativePerfCounters.descriptorSetAllocations += mPerfCounters.descriptorSetAllocations;
    mCumulativePerfCounters.descriptorSetCacheHits += mPerfCounters.descriptorSetCacheHits;
    mCumulativePerfCounters.descriptorSetCacheMisses += mPerfCounters.descriptorSetCacheMisses;

    ProgramExecutablePerfCounters counters = mPerfCounters;
    mPerfCounters.descriptorSetAllocations = {};
    mPerfCounters.descriptorSetCacheHits   = {};
    mPerfCounters.descriptorSetCacheMisses = {};
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
}
}  // namespace rx
