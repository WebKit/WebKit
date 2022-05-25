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
    options.removeDebugInfo          = !contextVk->getFeatures().retainSPIRVDebugInfo.enabled;
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
      mDynamicUniformDescriptorOffsets{}
{
    for (std::shared_ptr<DefaultUniformBlock> &defaultBlock : mDefaultUniformBlocks)
    {
        defaultBlock = std::make_shared<DefaultUniformBlock>();
    }
}

ProgramExecutableVk::~ProgramExecutableVk() {}

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

    for (vk::DescriptorPoolPointer &pool : mDescriptorPools)
    {
        pool.reset();
    }

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
    gl::ShaderMap<ShaderInterfaceVariableInfoMap::VariableTypeToInfoMap> data;
    gl::ShaderMap<ShaderInterfaceVariableInfoMap::NameToTypeAndIndexMap> nameToTypeAndIndexMap;
    gl::ShaderMap<ShaderInterfaceVariableInfoMap::VariableTypeToIndexMap> indexedResourceMap;

    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        size_t nameCount = stream->readInt<size_t>();
        for (size_t nameIndex = 0; nameIndex < nameCount; ++nameIndex)
        {
            const std::string variableName  = stream->readString();
            ShaderVariableType variableType = stream->readEnum<ShaderVariableType>();
            uint32_t index                  = stream->readInt<uint32_t>();
            nameToTypeAndIndexMap[shaderType][variableName] = {variableType, index};
        }

        for (ShaderVariableType variableType : angle::AllEnums<ShaderVariableType>())
        {
            size_t infoArraySize = stream->readInt<size_t>();
            for (size_t infoIndex = 0; infoIndex < infoArraySize; ++infoIndex)
            {
                ShaderInterfaceVariableInfo info;

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

                data[shaderType][variableType].push_back(info);
            }

            uint32_t resourceMapSize = stream->readInt<uint32_t>();
            for (uint32_t resourceIndex = 0; resourceIndex < resourceMapSize; ++resourceIndex)
            {
                uint32_t variableIndex = stream->readInt<uint32_t>();
                indexedResourceMap[shaderType][variableType][resourceIndex] = variableIndex;
            }
        }
    }

    mVariableInfoMap.load(data, nameToTypeAndIndexMap, indexedResourceMap);

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
    const gl::ShaderMap<ShaderInterfaceVariableInfoMap::VariableTypeToInfoMap> &data =
        mVariableInfoMap.getData();
    const gl::ShaderMap<ShaderInterfaceVariableInfoMap::NameToTypeAndIndexMap>
        &nameToTypeAndIndexMap = mVariableInfoMap.getNameToTypeAndIndexMap();
    const gl::ShaderMap<ShaderInterfaceVariableInfoMap::VariableTypeToIndexMap>
        &indexedResourceMap = mVariableInfoMap.getIndexedResourceMap();

    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        stream->writeInt(nameToTypeAndIndexMap[shaderType].size());
        for (const auto &iter : nameToTypeAndIndexMap[shaderType])
        {
            const std::string &name          = iter.first;
            const TypeAndIndex &typeAndIndex = iter.second;
            stream->writeString(name);
            stream->writeEnum(typeAndIndex.variableType);
            stream->writeInt(typeAndIndex.index);
        }

        for (ShaderVariableType variableType : angle::AllEnums<ShaderVariableType>())
        {
            const ShaderInterfaceVariableInfoMap::VariableInfoArray &infoArray =
                data[shaderType][variableType];

            stream->writeInt(infoArray.size());
            for (const ShaderInterfaceVariableInfo &info : infoArray)
            {
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

            const ShaderInterfaceVariableInfoMap::ResourceIndexMap &resourceIndexMap =
                indexedResourceMap[shaderType][variableType];
            stream->writeInt(static_cast<uint32_t>(resourceIndexMap.size()));
            for (uint32_t resourceIndex = 0; resourceIndex < resourceIndexMap.size();
                 ++resourceIndex)
            {
                stream->writeInt(resourceIndexMap[resourceIndex]);
            }
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

void ProgramExecutableVk::addInterfaceBlockDescriptorSetDesc(
    const std::vector<gl::InterfaceBlock> &blocks,
    gl::ShaderType shaderType,
    ShaderVariableType variableType,
    VkDescriptorType descType,
    vk::DescriptorSetLayoutDesc *descOut)
{
    for (uint32_t bufferIndex = 0, arraySize = 0; bufferIndex < blocks.size();
         bufferIndex += arraySize)
    {
        gl::InterfaceBlock block = blocks[bufferIndex];
        arraySize                = GetInterfaceBlockArraySize(blocks, bufferIndex);

        if (!block.isActive(shaderType))
        {
            continue;
        }

        const ShaderInterfaceVariableInfo &info =
            mVariableInfoMap.getIndexedVariableInfo(shaderType, variableType, bufferIndex);
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
    if (atomicCounterBuffers.empty() || !mVariableInfoMap.hasAtomicCounterInfo(shaderType))
    {
        return;
    }

    const ShaderInterfaceVariableInfo &info = mVariableInfoMap.getAtomicCounterInfo(shaderType);
    if (info.isDuplicate || !info.activeStages[shaderType])
    {
        return;
    }

    VkShaderStageFlags activeStages = gl_vk::GetShaderStageFlags(info.activeStages);

    // A single storage buffer array is used for all stages for simplicity.
    descOut->update(info.binding, vk::kStorageBufferDescriptorType,
                    gl::IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS, activeStages, nullptr);
}

void ProgramExecutableVk::addImageDescriptorSetDesc(const gl::ProgramExecutable &executable,
                                                    vk::DescriptorSetLayoutDesc *descOut)
{
    const std::vector<gl::ImageBinding> &imageBindings = executable.getImageBindings();
    const std::vector<gl::LinkedUniform> &uniforms     = executable.getUniforms();

    for (uint32_t imageIndex = 0; imageIndex < imageBindings.size(); ++imageIndex)
    {
        uint32_t uniformIndex = executable.getUniformIndexFromImageIndex(imageIndex);
        const gl::LinkedUniform &imageUniform = uniforms[uniformIndex];

        // 2D arrays are split into multiple 1D arrays when generating LinkedUniforms. Since they
        // are flattened into one array, ignore the nonzero elements and expand the array to the
        // total array size.
        if (imageUniform.outerArrayOffset > 0)
        {
            ASSERT(gl::SamplerNameContainsNonZeroArrayElement(imageUniform.name));
            continue;
        }

        ASSERT(!gl::SamplerNameContainsNonZeroArrayElement(imageUniform.name));

        // The front-end always binds array image units sequentially.
        const gl::ImageBinding &imageBinding = imageBindings[imageIndex];
        uint32_t arraySize = static_cast<uint32_t>(imageBinding.boundImageUnits.size());
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

            const ShaderInterfaceVariableInfo &info = mVariableInfoMap.getIndexedVariableInfo(
                shaderType, ShaderVariableType::Image, imageIndex);
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

    if (!executable.usesFramebufferFetch())
    {
        return;
    }

    const ShaderInterfaceVariableInfo &baseInfo =
        mVariableInfoMap.getFramebufferFetchInfo(shaderType);
    if (baseInfo.isDuplicate)
    {
        return;
    }

    const std::vector<gl::LinkedUniform> &uniforms = executable.getUniforms();
    const uint32_t baseUniformIndex                = executable.getFragmentInoutRange().low();
    const gl::LinkedUniform &baseInputAttachment   = uniforms.at(baseUniformIndex);
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
    const gl::ActiveTextureArray<TextureVk *> *activeTextures,
    vk::DescriptorSetLayoutDesc *descOut)
{
    const std::vector<gl::SamplerBinding> &samplerBindings = executable.getSamplerBindings();
    const std::vector<gl::LinkedUniform> &uniforms         = executable.getUniforms();

    for (uint32_t textureIndex = 0; textureIndex < samplerBindings.size(); ++textureIndex)
    {
        uint32_t uniformIndex = executable.getUniformIndexFromSamplerIndex(textureIndex);
        const gl::LinkedUniform &samplerUniform = uniforms[uniformIndex];

        // 2D arrays are split into multiple 1D arrays when generating LinkedUniforms. Since they
        // are flattened into one array, ignore the nonzero elements and expand the array to the
        // total array size.
        if (samplerUniform.outerArrayOffset > 0)
        {
            ASSERT(gl::SamplerNameContainsNonZeroArrayElement(samplerUniform.name));
            continue;
        }

        ASSERT(!gl::SamplerNameContainsNonZeroArrayElement(samplerUniform.name));

        // The front-end always binds array sampler units sequentially.
        const gl::SamplerBinding &samplerBinding = samplerBindings[textureIndex];
        uint32_t arraySize = static_cast<uint32_t>(samplerBinding.boundTextureUnits.size());
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

            const ShaderInterfaceVariableInfo &info = mVariableInfoMap.getIndexedVariableInfo(
                shaderType, ShaderVariableType::Texture, textureIndex);
            if (info.isDuplicate)
            {
                continue;
            }

            VkShaderStageFlags activeStages = gl_vk::GetShaderStageFlags(info.activeStages);

            // TODO: https://issuetracker.google.com/issues/158215272: how do we handle array of
            // immutable samplers?
            GLuint textureUnit = samplerBinding.boundTextureUnits[0];
            if (activeTextures &&
                ((*activeTextures)[textureUnit]->getImage().hasImmutableSampler()))
            {
                ASSERT(samplerBinding.boundTextureUnits.size() == 1);
                // Always take the texture's sampler, that's only way to get to yuv conversion for
                // externalFormat
                const TextureVk *textureVk          = (*activeTextures)[textureUnit];
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
    ProgramInfo &programInfo                  = getGraphicsProgramInfo();
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

angle::Result ProgramExecutableVk::createPipelineLayout(
    ContextVk *contextVk,
    const gl::ProgramExecutable &glExecutable,
    gl::ActiveTextureArray<TextureVk *> *activeTextures)
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
        const ShaderInterfaceVariableInfo &info =
            mVariableInfoMap.getDefaultUniformInfo(shaderType);
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
                                           ShaderVariableType::UniformBuffer,
                                           mUniformBufferDescriptorType, &resourcesSetDesc);
        addInterfaceBlockDescriptorSetDesc(glExecutable.getShaderStorageBlocks(), shaderType,
                                           ShaderVariableType::ShaderStorageBuffer,
                                           vk::kStorageBufferDescriptorType, &resourcesSetDesc);
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
    ANGLE_TRY(contextVk->bindCachedDescriptorPool(
        DescriptorSetIndex::UniformsAndXfb, uniformsAndXfbSetDesc, 1,
        &mDescriptorPools[DescriptorSetIndex::UniformsAndXfb]));
    ANGLE_TRY(contextVk->bindCachedDescriptorPool(DescriptorSetIndex::Texture, texturesSetDesc,
                                                  mImmutableSamplersMaxDescriptorCount,
                                                  &mDescriptorPools[DescriptorSetIndex::Texture]));
    ANGLE_TRY(
        contextVk->bindCachedDescriptorPool(DescriptorSetIndex::ShaderResource, resourcesSetDesc, 1,
                                            &mDescriptorPools[DescriptorSetIndex::ShaderResource]));

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
            ShaderInterfaceVariableInfo &info = mVariableInfoMap.getMutable(
                mergedVarying.frontShaderStage, ShaderVariableType::Varying,
                mergedVarying.frontShader->mappedName);
            info.varyingIsOutput     = true;
            info.useRelaxedPrecision = true;
        }
        else
        {
            // The output is lower precision than the input, adjust the input
            ASSERT(backPrecision > frontPrecision);
            ShaderInterfaceVariableInfo &info = mVariableInfoMap.getMutable(
                mergedVarying.backShaderStage, ShaderVariableType::Varying,
                mergedVarying.backShader->mappedName);
            info.varyingIsInput      = true;
            info.useRelaxedPrecision = true;
        }
    }
}

angle::Result ProgramExecutableVk::getOrAllocateDescriptorSet(
    vk::Context *context,
    UpdateDescriptorSetsBuilder *updateBuilder,
    vk::ResourceUseList *resourceUseList,
    const vk::DescriptorSetDescBuilder &descriptorSetDesc,
    DescriptorSetIndex setIndex)
{
    vk::DescriptorCacheResult cacheResult;
    ANGLE_TRY(mDescriptorPools[setIndex].get().getOrAllocateDescriptorSet(
        context, resourceUseList, descriptorSetDesc.getDesc(),
        mDescriptorSetLayouts[setIndex].get(), &mDescriptorPoolBindings[setIndex],
        &mDescriptorSets[setIndex], &cacheResult));
    ASSERT(mDescriptorSets[setIndex] != VK_NULL_HANDLE);

    if (cacheResult == vk::DescriptorCacheResult::NewAllocation)
    {
        descriptorSetDesc.updateDescriptorSet(updateBuilder, mDescriptorSets[setIndex]);
    }
    else
    {
        mDescriptorPoolBindings[setIndex].get().retain(resourceUseList);
    }

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::updateShaderResourcesDescriptorSet(
    ContextVk *contextVk,
    UpdateDescriptorSetsBuilder *updateBuilder,
    vk::ResourceUseList *resourceUseList,
    const vk::DescriptorSetDescBuilder &shaderResourcesDesc)
{
    if (!mDescriptorPools[DescriptorSetIndex::ShaderResource].get().valid())
    {
        return angle::Result::Continue;
    }

    ANGLE_TRY(getOrAllocateDescriptorSet(contextVk, updateBuilder, resourceUseList,
                                         shaderResourcesDesc, DescriptorSetIndex::ShaderResource));

    size_t numOffsets = shaderResourcesDesc.getDynamicOffsetsSize();
    mDynamicShaderResourceDescriptorOffsets.resize(numOffsets);
    if (numOffsets > 0)
    {
        memcpy(mDynamicShaderResourceDescriptorOffsets.data(),
               shaderResourcesDesc.getDynamicOffsets(), numOffsets * sizeof(uint32_t));
    }

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::updateUniformsAndXfbDescriptorSet(
    vk::Context *context,
    UpdateDescriptorSetsBuilder *updateBuilder,
    vk::ResourceUseList *resourceUseList,
    vk::BufferHelper *defaultUniformBuffer,
    const vk::DescriptorSetDescBuilder &uniformsAndXfbDesc)
{
    mCurrentDefaultUniformBufferSerial =
        defaultUniformBuffer ? defaultUniformBuffer->getBufferSerial() : vk::kInvalidBufferSerial;

    return getOrAllocateDescriptorSet(context, updateBuilder, resourceUseList, uniformsAndXfbDesc,
                                      DescriptorSetIndex::UniformsAndXfb);
}

angle::Result ProgramExecutableVk::updateTexturesDescriptorSet(
    vk::Context *context,
    const gl::ProgramExecutable &executable,
    const gl::ActiveTextureArray<TextureVk *> &textures,
    const gl::SamplerBindingVector &samplers,
    bool emulateSeamfulCubeMapSampling,
    PipelineType pipelineType,
    UpdateDescriptorSetsBuilder *updateBuilder,
    vk::ResourceUseList *resourceUseList,
    const vk::DescriptorSetDesc &texturesDesc)
{
    vk::DescriptorCacheResult cacheResult;
    ANGLE_TRY(mDescriptorPools[DescriptorSetIndex::Texture].get().getOrAllocateDescriptorSet(
        context, resourceUseList, texturesDesc,
        mDescriptorSetLayouts[DescriptorSetIndex::Texture].get(),
        &mDescriptorPoolBindings[DescriptorSetIndex::Texture],
        &mDescriptorSets[DescriptorSetIndex::Texture], &cacheResult));
    ASSERT(mDescriptorSets[DescriptorSetIndex::Texture] != VK_NULL_HANDLE);

    if (cacheResult == vk::DescriptorCacheResult::NewAllocation)
    {
        vk::DescriptorSetDescBuilder fullDesc;
        ANGLE_TRY(fullDesc.updateFullActiveTextures(context, mVariableInfoMap, executable, textures,
                                                    samplers, emulateSeamfulCubeMapSampling,
                                                    pipelineType));
        fullDesc.updateDescriptorSet(updateBuilder, mDescriptorSets[DescriptorSetIndex::Texture]);
    }
    else
    {
        mDescriptorPoolBindings[DescriptorSetIndex::Texture].get().retain(resourceUseList);
    }

    return angle::Result::Continue;
}

template <typename CommandBufferT>
angle::Result ProgramExecutableVk::bindDescriptorSets(vk::Context *context,
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
                ANGLE_TRY(mDescriptorPools[descriptorSetIndex].get().allocateDescriptorSets(
                    context, resourceUseList, mDescriptorSetLayouts[descriptorSetIndex].get(), 1,
                    &mDescriptorPoolBindings[descriptorSetIndex],
                    &mEmptyDescriptorSets[descriptorSetIndex]));
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
                static_cast<uint32_t>(mDynamicShaderResourceDescriptorOffsets.size()),
                mDynamicShaderResourceDescriptorOffsets.data());
        }
        else
        {
            commandBuffer->bindDescriptorSets(getPipelineLayout(), pipelineBindPoint,
                                              descriptorSetIndex, 1, &descSet, 0, nullptr);
        }
    }

    return angle::Result::Continue;
}

template angle::Result ProgramExecutableVk::bindDescriptorSets<vk::priv::SecondaryCommandBuffer>(
    vk::Context *context,
    vk::ResourceUseList *resourceUseList,
    vk::priv::SecondaryCommandBuffer *commandBuffer,
    PipelineType pipelineType);
template angle::Result ProgramExecutableVk::bindDescriptorSets<vk::VulkanSecondaryCommandBuffer>(
    vk::Context *context,
    vk::ResourceUseList *resourceUseList,
    vk::VulkanSecondaryCommandBuffer *commandBuffer,
    PipelineType pipelineType);

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

    ASSERT(defaultUniformBuffer);

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
        vk::DescriptorSetDescBuilder uniformsAndXfbDesc;
        uniformsAndXfbDesc.updateUniformsAndXfb(
            context, glExecutable, *this, defaultUniformBuffer, *emptyBuffer,
            isTransformFeedbackActiveUnpaused,
            glExecutable.hasTransformFeedbackOutput() ? transformFeedbackVk : nullptr);

        ANGLE_TRY(updateUniformsAndXfbDescriptorSet(context, updateBuilder, resourceUseList,
                                                    defaultUniformBuffer, uniformsAndXfbDesc));
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
