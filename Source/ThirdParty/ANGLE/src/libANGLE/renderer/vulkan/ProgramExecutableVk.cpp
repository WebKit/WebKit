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
constexpr gl::ShaderMap<vk::PipelineStage> kPipelineStageShaderMap = {
    {gl::ShaderType::Vertex, vk::PipelineStage::VertexShader},
    {gl::ShaderType::Fragment, vk::PipelineStage::FragmentShader},
    {gl::ShaderType::Geometry, vk::PipelineStage::GeometryShader},
    {gl::ShaderType::Compute, vk::PipelineStage::ComputeShader},
};

VkDeviceSize GetShaderBufferBindingSize(const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding)
{
    if (bufferBinding.getSize() != 0)
    {
        return bufferBinding.getSize();
    }
    // If size is 0, we can't always use VK_WHOLE_SIZE (or bufferHelper.getSize()), as the
    // backing buffer may be larger than max*BufferRange.  In that case, we use the backing
    // buffer size (what's left after offset).
    const gl::Buffer *bufferGL = bufferBinding.get();
    ASSERT(bufferGL);
    ASSERT(bufferGL->getSize() >= bufferBinding.getOffset());
    return bufferGL->getSize() - bufferBinding.getOffset();
}

bool ValidateTransformedSpirV(ContextVk *contextVk,
                              const gl::ShaderBitSet &linkedShaderStages,
                              ProgramExecutableVk *executableVk,
                              const gl::ShaderMap<SpirvBlob> &spirvBlobs)
{
    for (gl::ShaderType shaderType : linkedShaderStages)
    {
        SpirvBlob transformed;
        if (GlslangWrapperVk::TransformSpirV(
                contextVk, shaderType, false,
                executableVk->getShaderInterfaceVariableInfoMap()[shaderType],
                spirvBlobs[shaderType], &transformed) != angle::Result::Continue)
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

angle::Result ShaderInfo::initShaders(ContextVk *contextVk,
                                      const gl::ShaderBitSet &linkedShaderStages,
                                      const gl::ShaderMap<std::string> &shaderSources,
                                      ProgramExecutableVk *executableVk)
{
    ASSERT(!valid());

    ANGLE_TRY(GlslangWrapperVk::GetShaderCode(contextVk, linkedShaderStages, contextVk->getCaps(),
                                              shaderSources, &mSpirvBlobs));

    // Assert that SPIR-V transformation is correct, even if the test never issues a draw call.
    ASSERT(ValidateTransformedSpirV(contextVk, linkedShaderStages, executableVk, mSpirvBlobs));

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
                                       const gl::ShaderType shaderType,
                                       const ShaderInfo &shaderInfo,
                                       ProgramTransformOptions optionBits,
                                       ProgramExecutableVk *executableVk)
{
    const ShaderMapInterfaceVariableInfoMap &variableInfoMap =
        executableVk->getShaderInterfaceVariableInfoMap();
    const gl::ShaderMap<SpirvBlob> &originalSpirvBlobs = shaderInfo.getSpirvBlobs();
    const SpirvBlob &originalSpirvBlob                 = originalSpirvBlobs[shaderType];
    bool removeEarlyFragmentTestsOptimization =
        (shaderType == gl::ShaderType::Fragment && optionBits.removeEarlyFragmentTestsOptimization);
    gl::ShaderMap<SpirvBlob> transformedSpirvBlobs;
    SpirvBlob &transformedSpirvBlob = transformedSpirvBlobs[shaderType];

    ANGLE_TRY(GlslangWrapperVk::TransformSpirV(
        contextVk, shaderType, removeEarlyFragmentTestsOptimization, variableInfoMap[shaderType],
        originalSpirvBlob, &transformedSpirvBlob));
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
      mDynamicBufferOffsets{},
      mProgram(nullptr),
      mProgramPipeline(nullptr),
      mObjectPerfCounters{}
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

    mTextureDescriptorsCache.clear();
    mUniformsAndXfbDescriptorSetCache.clear();

    // Initialize with a unique BufferSerial
    vk::ResourceSerialFactory &factory = contextVk->getRenderer()->getResourceSerialFactory();
    mCurrentDefaultUniformBufferSerial = factory.generateBufferSerial();

    for (ProgramInfo &programInfo : mGraphicsProgramInfos)
    {
        programInfo.release(contextVk);
    }
    mComputeProgramInfo.release(contextVk);
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
            info->activeStages            = gl::ShaderBitSet(stream->readInt<uint8_t>());
            info->xfbBuffer               = stream->readInt<uint32_t>();
            info->xfbOffset               = stream->readInt<uint32_t>();
            info->xfbStride               = stream->readInt<uint32_t>();
            info->useRelaxedPrecision     = stream->readBool();
            info->varyingIsOutput         = stream->readBool();
            info->attributeComponentCount = stream->readInt<uint8_t>();
            info->attributeLocationCount  = stream->readInt<uint8_t>();
        }
    }

    return std::make_unique<LinkEventDone>(angle::Result::Continue);
}

void ProgramExecutableVk::save(gl::BinaryOutputStream *stream)
{
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        stream->writeInt(mVariableInfoMap[shaderType].size());
        for (const auto &it : mVariableInfoMap[shaderType])
        {
            stream->writeString(it.first);
            stream->writeInt(it.second.descriptorSet);
            stream->writeInt(it.second.binding);
            stream->writeInt(it.second.location);
            stream->writeInt(it.second.component);
            // PackedEnumBitSet uses uint8_t
            stream->writeInt(it.second.activeStages.bits());
            stream->writeInt(it.second.xfbBuffer);
            stream->writeInt(it.second.xfbOffset);
            stream->writeInt(it.second.xfbStride);
            stream->writeBool(it.second.useRelaxedPrecision);
            stream->writeBool(it.second.varyingIsOutput);
            stream->writeInt(it.second.attributeComponentCount);
            stream->writeInt(it.second.attributeLocationCount);
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
        return mProgramPipeline->getShaderProgram(glState, shaderType);
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
        mProgramPipeline->fillProgramStateMap(contextVk, programStatesOut);
    }
}

const gl::ProgramExecutable &ProgramExecutableVk::getGlExecutable()
{
    ASSERT(mProgram || mProgramPipeline);
    if (mProgram)
    {
        return mProgram->getState().getExecutable();
    }
    return mProgramPipeline->getState().getProgramExecutable();
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
    const vk::UniformsAndXfbDesc &xfbBufferDesc,
    bool *newDescriptorSetAllocated)
{
    mCurrentDefaultUniformBufferSerial = xfbBufferDesc.getDefaultUniformBufferSerial();

    // Look up in the cache first
    auto iter = mUniformsAndXfbDescriptorSetCache.find(xfbBufferDesc);
    if (iter != mUniformsAndXfbDescriptorSetCache.end())
    {
        *newDescriptorSetAllocated                                        = false;
        mDescriptorSets[ToUnderlying(DescriptorSetIndex::UniformsAndXfb)] = iter->second;
        // The descriptor pool that this descriptor set was allocated from needs to be retained each
        // time the descriptor set is used in a new command.
        mDescriptorPoolBindings[ToUnderlying(DescriptorSetIndex::UniformsAndXfb)].get().retain(
            &contextVk->getResourceUseList());
        return angle::Result::Continue;
    }

    bool newPoolAllocated;
    ANGLE_TRY(allocateDescriptorSetAndGetInfo(contextVk, DescriptorSetIndex::UniformsAndXfb,
                                              &newPoolAllocated));

    // Clear descriptor set cache. It may no longer be valid.
    if (newPoolAllocated)
    {
        mUniformsAndXfbDescriptorSetCache.clear();
    }

    // Add the descriptor set into cache
    mUniformsAndXfbDescriptorSetCache.emplace(
        xfbBufferDesc, mDescriptorSets[ToUnderlying(DescriptorSetIndex::UniformsAndXfb)]);
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
    vk::DynamicDescriptorPool &dynamicDescriptorPool =
        mDynamicDescriptorPools[ToUnderlying(descriptorSetIndex)];

    const vk::DescriptorSetLayout &descriptorSetLayout =
        mDescriptorSetLayouts[ToUnderlying(descriptorSetIndex)].get();
    ANGLE_TRY(dynamicDescriptorPool.allocateSetsAndGetInfo(
        contextVk, descriptorSetLayout.ptr(), 1,
        &mDescriptorPoolBindings[ToUnderlying(descriptorSetIndex)],
        &mDescriptorSets[ToUnderlying(descriptorSetIndex)], newPoolAllocatedOut));
    mEmptyDescriptorSets[ToUnderlying(descriptorSetIndex)] = VK_NULL_HANDLE;

    ++mObjectPerfCounters.descriptorSetsAllocated[ToUnderlying(descriptorSetIndex)];

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

        descOut->update(info.binding, descType, arraySize, gl_vk::kShaderStageMap[shaderType],
                        nullptr);
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
                    gl_vk::kShaderStageMap[shaderType], nullptr);
}

void ProgramExecutableVk::addImageDescriptorSetDesc(const gl::ProgramExecutable &executable,
                                                    bool useOldRewriteStructSamplers,
                                                    vk::DescriptorSetLayoutDesc *descOut)
{
    const std::vector<gl::ImageBinding> &imageBindings = executable.getImageBindings();
    const std::vector<gl::LinkedUniform> &uniforms     = executable.getUniforms();

    for (uint32_t imageIndex = 0; imageIndex < imageBindings.size(); ++imageIndex)
    {
        const gl::ImageBinding &imageBinding = imageBindings[imageIndex];
        uint32_t uniformIndex                = executable.getUniformIndexFromImageIndex(imageIndex);
        const gl::LinkedUniform &imageUniform = uniforms[uniformIndex];

        std::string imageName = useOldRewriteStructSamplers
                                    ? GetMappedSamplerNameOld(imageUniform.name)
                                    : GlslangGetMappedSamplerName(imageUniform.name);

        // The front-end always binds array image units sequentially.
        uint32_t arraySize = static_cast<uint32_t>(imageBinding.boundImageUnits.size());

        if (!useOldRewriteStructSamplers)
        {
            // 2D arrays are split into multiple 1D arrays when generating
            // LinkedUniforms. Since they are flattened into one array, ignore the
            // nonzero elements and expand the array to the total array size.
            if (gl::SamplerNameContainsNonZeroArrayElement(imageUniform.name))
            {
                continue;
            }

            for (unsigned int outerArraySize : imageUniform.outerArraySizes)
            {
                arraySize *= outerArraySize;
            }
        }

        for (const gl::ShaderType shaderType : executable.getLinkedShaderStages())
        {
            if (!imageUniform.isActive(shaderType))
            {
                continue;
            }

            GetImageNameWithoutIndices(&imageName);
            ShaderInterfaceVariableInfo &info = mVariableInfoMap[shaderType][imageName];
            VkShaderStageFlags activeStages   = gl_vk::kShaderStageMap[shaderType];
            descOut->update(info.binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, arraySize, activeStages,
                            nullptr);
        }
    }
}

void ProgramExecutableVk::addTextureDescriptorSetDesc(
    const gl::ProgramState &programState,
    bool useOldRewriteStructSamplers,
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

        for (const gl::ShaderType shaderType : programState.getExecutable().getLinkedShaderStages())
        {
            if (!samplerUniform.isActive(shaderType))
            {
                continue;
            }

            ShaderInterfaceVariableInfo &info = mVariableInfoMap[shaderType][samplerName];
            VkShaderStageFlags activeStages   = gl_vk::kShaderStageMap[shaderType];

            // TODO: https://issuetracker.google.com/issues/158215272: how do we handle array of
            // immutable samplers?
            GLuint textureUnit = samplerBinding.boundTextureUnits[0];
            if (activeTextures &&
                ((*activeTextures)[textureUnit].texture->getImage().hasImmutableSampler()))
            {
                ASSERT(samplerBinding.boundTextureUnits.size() == 1);
                // Always take the texture's sampler, that's only way to get to yuv conversion for
                // externalFormat
                const vk::Sampler &immutableSampler =
                    (*activeTextures)[textureUnit].texture->getSampler().get();
                descOut->update(info.binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, arraySize,
                                activeStages, &immutableSampler);
            }
            else
            {
                descOut->update(info.binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, arraySize,
                                activeStages, nullptr);
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

angle::Result ProgramExecutableVk::getGraphicsPipeline(
    ContextVk *contextVk,
    gl::PrimitiveMode mode,
    const vk::GraphicsPipelineDesc &desc,
    const gl::AttributesMask &activeAttribLocations,
    const vk::GraphicsPipelineDesc **descPtrOut,
    vk::PipelineHelper **pipelineOut)
{
    const gl::State &glState                  = contextVk->getState();
    RendererVk *renderer                      = contextVk->getRenderer();
    vk::PipelineCache *pipelineCache          = nullptr;
    const gl::ProgramExecutable *glExecutable = glState.getProgramExecutable();
    ASSERT(glExecutable && !glExecutable->isCompute());

    mTransformOptions.enableLineRasterEmulation = contextVk->isBresenhamEmulationEnabled(mode);
    mTransformOptions.surfaceRotation           = static_cast<uint8_t>(desc.getSurfaceRotation());

    // This must be called after mTransformOptions have been set.
    ProgramInfo &programInfo = getGraphicsProgramInfo(mTransformOptions);

    for (const gl::ShaderType shaderType : glExecutable->getLinkedShaderStages())
    {
        ProgramVk *programVk = getShaderProgram(glState, shaderType);
        if (programVk)
        {
            ANGLE_TRY(programVk->initGraphicsShaderProgram(contextVk, shaderType, mTransformOptions,
                                                           &programInfo, this));
        }
    }

    vk::ShaderProgramHelper *shaderProgram = programInfo.getShaderProgram();
    ASSERT(shaderProgram);
    ANGLE_TRY(renderer->getPipelineCache(&pipelineCache));
    return shaderProgram->getGraphicsPipeline(
        contextVk, &contextVk->getRenderPassCache(), *pipelineCache, getPipelineLayout(), desc,
        activeAttribLocations, glState.getProgramExecutable()->getAttributesTypeMask(), descPtrOut,
        pipelineOut);
}

angle::Result ProgramExecutableVk::getComputePipeline(ContextVk *contextVk,
                                                      vk::PipelineAndSerial **pipelineOut)
{
    const gl::State &glState                  = contextVk->getState();
    const gl::ProgramExecutable *glExecutable = glState.getProgramExecutable();
    ASSERT(glExecutable && glExecutable->isCompute());

    ProgramVk *programVk = getShaderProgram(glState, gl::ShaderType::Compute);
    ASSERT(programVk);
    ProgramInfo &programInfo = getComputeProgramInfo();
    ANGLE_TRY(programVk->initComputeProgram(contextVk, &programInfo, this));

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

            poolSize.type            = binding.descriptorType;
            poolSize.descriptorCount = binding.descriptorCount;
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
        poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = 1;
        descriptorPoolSizes.emplace_back(poolSize);
    }

    if (!descriptorPoolSizes.empty())
    {
        ANGLE_TRY(mDynamicDescriptorPools[ToUnderlying(descriptorSetIndex)].init(
            contextVk, descriptorPoolSizes.data(), descriptorPoolSizes.size(),
            descriptorSetLayout));
    }

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::createPipelineLayout(
    const gl::Context *glContext,
    gl::ActiveTextureArray<vk::TextureUnit> *activeTextures)
{
    const gl::State &glState                   = glContext->getState();
    ContextVk *contextVk                       = vk::GetImpl(glContext);
    gl::TransformFeedback *transformFeedback   = glState.getCurrentTransformFeedback();
    const gl::ProgramExecutable &glExecutable  = getGlExecutable();
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
        ShaderInterfaceVariableInfo &info  = mVariableInfoMap[shaderType][uniformBlockName];
        if (!info.activeStages[shaderType])
        {
            continue;
        }

        uniformsAndXfbSetDesc.update(info.binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
                                     gl_vk::kShaderStageMap[shaderType], nullptr);
        mNumDefaultUniformDescriptors++;
    }
    bool hasVertexShader = glExecutable.hasLinkedShaderStage(gl::ShaderType::Vertex);
    bool hasXfbVaryings =
        (programStates[gl::ShaderType::Vertex] &&
         !programStates[gl::ShaderType::Vertex]->getLinkedTransformFeedbackVaryings().empty());
    if (hasVertexShader && transformFeedback && hasXfbVaryings)
    {
        const gl::ProgramExecutable &executable =
            programStates[gl::ShaderType::Vertex]->getExecutable();
        size_t xfbBufferCount                    = executable.getTransformFeedbackBufferCount();
        TransformFeedbackVk *transformFeedbackVk = vk::GetImpl(transformFeedback);
        transformFeedbackVk->updateDescriptorSetLayout(contextVk,
                                                       mVariableInfoMap[gl::ShaderType::Vertex],
                                                       xfbBufferCount, &uniformsAndXfbSetDesc);
    }

    ANGLE_TRY(contextVk->getDescriptorSetLayoutCache().getDescriptorSetLayout(
        contextVk, uniformsAndXfbSetDesc,
        &mDescriptorSetLayouts[ToUnderlying(DescriptorSetIndex::UniformsAndXfb)]));

    // Uniform and storage buffers, atomic counter buffers and images:
    vk::DescriptorSetLayoutDesc resourcesSetDesc;

    for (const gl::ShaderType shaderType : linkedShaderStages)
    {
        const gl::ProgramState *programState = programStates[shaderType];
        ASSERT(programState);

        addInterfaceBlockDescriptorSetDesc(programState->getUniformBlocks(), shaderType,
                                           VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &resourcesSetDesc);
        addInterfaceBlockDescriptorSetDesc(programState->getShaderStorageBlocks(), shaderType,
                                           VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resourcesSetDesc);
        addAtomicCounterBufferDescriptorSetDesc(programState->getAtomicCounterBuffers(), shaderType,
                                                &resourcesSetDesc);
    }

    for (const gl::ShaderType shaderType : linkedShaderStages)
    {
        const gl::ProgramState *programState = programStates[shaderType];
        ASSERT(programState);
        addImageDescriptorSetDesc(programState->getExecutable(),
                                  contextVk->useOldRewriteStructSamplers(), &resourcesSetDesc);
    }

    ANGLE_TRY(contextVk->getDescriptorSetLayoutCache().getDescriptorSetLayout(
        contextVk, resourcesSetDesc,
        &mDescriptorSetLayouts[ToUnderlying(DescriptorSetIndex::ShaderResource)]));

    // Textures:
    vk::DescriptorSetLayoutDesc texturesSetDesc;

    for (const gl::ShaderType shaderType : linkedShaderStages)
    {
        const gl::ProgramState *programState = programStates[shaderType];
        ASSERT(programState);
        addTextureDescriptorSetDesc(*programState, contextVk->useOldRewriteStructSamplers(),
                                    activeTextures, &texturesSetDesc);
    }

    ANGLE_TRY(contextVk->getDescriptorSetLayoutCache().getDescriptorSetLayout(
        contextVk, texturesSetDesc,
        &mDescriptorSetLayouts[ToUnderlying(DescriptorSetIndex::Texture)]));

    // Driver uniforms:
    VkShaderStageFlags driverUniformsStages =
        glExecutable.isCompute() ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_ALL_GRAPHICS;
    vk::DescriptorSetLayoutDesc driverUniformsSetDesc =
        contextVk->getDriverUniformsDescriptorSetDesc(driverUniformsStages);
    ANGLE_TRY(contextVk->getDescriptorSetLayoutCache().getDescriptorSetLayout(
        contextVk, driverUniformsSetDesc,
        &mDescriptorSetLayouts[ToUnderlying(DescriptorSetIndex::DriverUniforms)]));

    // Create pipeline layout with these 4 descriptor sets.
    vk::PipelineLayoutDesc pipelineLayoutDesc;
    pipelineLayoutDesc.updateDescriptorSetLayout(DescriptorSetIndex::UniformsAndXfb,
                                                 uniformsAndXfbSetDesc);
    pipelineLayoutDesc.updateDescriptorSetLayout(DescriptorSetIndex::ShaderResource,
                                                 resourcesSetDesc);
    pipelineLayoutDesc.updateDescriptorSetLayout(DescriptorSetIndex::Texture, texturesSetDesc);
    pipelineLayoutDesc.updateDescriptorSetLayout(DescriptorSetIndex::DriverUniforms,
                                                 driverUniformsSetDesc);

    ANGLE_TRY(contextVk->getPipelineLayoutCache().getPipelineLayout(
        contextVk, pipelineLayoutDesc, mDescriptorSetLayouts, &mPipelineLayout));

    // Initialize descriptor pools.
    ANGLE_TRY(initDynamicDescriptorPools(
        contextVk, uniformsAndXfbSetDesc, DescriptorSetIndex::UniformsAndXfb,
        mDescriptorSetLayouts[ToUnderlying(DescriptorSetIndex::UniformsAndXfb)].get().getHandle()));
    ANGLE_TRY(initDynamicDescriptorPools(
        contextVk, resourcesSetDesc, DescriptorSetIndex::ShaderResource,
        mDescriptorSetLayouts[ToUnderlying(DescriptorSetIndex::ShaderResource)].get().getHandle()));
    ANGLE_TRY(initDynamicDescriptorPools(
        contextVk, texturesSetDesc, DescriptorSetIndex::Texture,
        mDescriptorSetLayouts[ToUnderlying(DescriptorSetIndex::Texture)].get().getHandle()));
    ANGLE_TRY(initDynamicDescriptorPools(
        contextVk, driverUniformsSetDesc, DescriptorSetIndex::DriverUniforms,
        mDescriptorSetLayouts[ToUnderlying(DescriptorSetIndex::DriverUniforms)].get().getHandle()));

    mDynamicBufferOffsets.resize(glExecutable.getLinkedShaderStageCount());

    return angle::Result::Continue;
}

void ProgramExecutableVk::resolvePrecisionMismatch(const gl::ProgramMergedVaryings &mergedVaryings)
{
    for (const gl::ProgramVaryingRef &mergedVarying : mergedVaryings)
    {
        if (mergedVarying.frontShader && mergedVarying.backShader)
        {
            GLenum frontPrecision = mergedVarying.frontShader->precision;
            GLenum backPrecision  = mergedVarying.backShader->precision;
            if (frontPrecision != backPrecision)
            {
                ShaderInterfaceVariableInfo *info =
                    &mVariableInfoMap[mergedVarying.frontShaderStage]
                                     [mergedVarying.frontShader->mappedName];
                ASSERT(frontPrecision >= GL_LOW_FLOAT && frontPrecision <= GL_HIGH_INT);
                ASSERT(backPrecision >= GL_LOW_FLOAT && backPrecision <= GL_HIGH_INT);
                if (frontPrecision > backPrecision)
                {
                    // The output is higher precision than the input
                    info->varyingIsOutput     = true;
                    info->useRelaxedPrecision = true;
                }
                else if (backPrecision > frontPrecision)
                {
                    // The output is lower precision than the input, adjust the input
                    info = &mVariableInfoMap[mergedVarying.backShaderStage]
                                            [mergedVarying.backShader->mappedName];
                    info->varyingIsOutput     = false;
                    info->useRelaxedPrecision = true;
                }
            }
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
    ShaderInterfaceVariableInfo &info  = mVariableInfoMap[shaderType][uniformBlockName];
    if (!info.activeStages[shaderType])
    {
        return;
    }

    VkWriteDescriptorSet &writeInfo    = contextVk->allocWriteDescriptorSet();
    VkDescriptorBufferInfo &bufferInfo = contextVk->allocDescriptorBufferInfo();

    if (!defaultUniformBlock.uniformData.empty())
    {
        bufferInfo.buffer = defaultUniformBuffer->getBuffer().getHandle();
    }
    else
    {
        vk::BufferHelper &emptyBuffer = contextVk->getEmptyBuffer();
        emptyBuffer.retain(&contextVk->getResourceUseList());
        bufferInfo.buffer = emptyBuffer.getBuffer().getHandle();
    }

    bufferInfo.offset = 0;
    bufferInfo.range  = VK_WHOLE_SIZE;

    writeInfo.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfo.pNext            = nullptr;
    writeInfo.dstSet           = mDescriptorSets[ToUnderlying(DescriptorSetIndex::UniformsAndXfb)];
    writeInfo.dstBinding       = info.binding;
    writeInfo.dstArrayElement  = 0;
    writeInfo.descriptorCount  = 1;
    writeInfo.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    writeInfo.pImageInfo       = nullptr;
    writeInfo.pBufferInfo      = &bufferInfo;
    writeInfo.pTexelBufferView = nullptr;
}

angle::Result ProgramExecutableVk::updateBuffersDescriptorSet(
    ContextVk *contextVk,
    const gl::ShaderType shaderType,
    vk::ResourceUseList *resourceUseList,
    vk::CommandBufferHelper *commandBufferHelper,
    const std::vector<gl::InterfaceBlock> &blocks,
    VkDescriptorType descriptorType)
{
    if (blocks.empty())
    {
        return angle::Result::Continue;
    }

    ASSERT(descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
           descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    const bool isStorageBuffer = descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    VkDescriptorSet descriptorSet =
        mDescriptorSets[ToUnderlying(DescriptorSetIndex::ShaderResource)];

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

        VkDeviceSize size;
        if (!isStorageBuffer)
        {
            size = block.dataSize;
        }
        else
        {
            size = GetShaderBufferBindingSize(bufferBinding);
        }

        // Make sure there's no possible under/overflow with binding size.
        static_assert(sizeof(VkDeviceSize) >= sizeof(bufferBinding.getSize()),
                      "VkDeviceSize too small");
        ASSERT(bufferBinding.getSize() >= 0);

        VkDescriptorBufferInfo &bufferInfo = contextVk->allocDescriptorBufferInfo();
        VkWriteDescriptorSet &writeInfo    = contextVk->allocWriteDescriptorSet();

        BufferVk *bufferVk             = vk::GetImpl(bufferBinding.get());
        vk::BufferHelper &bufferHelper = bufferVk->getBuffer();

        // Lazily allocate the descriptor set, since we may not need one if all of the buffers are
        // inactive.
        if (descriptorSet == VK_NULL_HANDLE)
        {
            ANGLE_TRY(allocateDescriptorSet(contextVk, DescriptorSetIndex::ShaderResource));
            descriptorSet = mDescriptorSets[ToUnderlying(DescriptorSetIndex::ShaderResource)];
        }
        ASSERT(descriptorSet != VK_NULL_HANDLE);
        WriteBufferDescriptorSetBinding(bufferHelper, bufferBinding.getOffset(), size,
                                        descriptorSet, descriptorType, binding, arrayElement, 0,
                                        &bufferInfo, &writeInfo);

        if (isStorageBuffer)
        {
            // We set the SHADER_READ_BIT to be conservative.
            VkAccessFlags accessFlags = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            commandBufferHelper->bufferWrite(resourceUseList, accessFlags,
                                             kPipelineStageShaderMap[shaderType],
                                             vk::AliasingMode::Allowed, &bufferHelper);
        }
        else
        {
            commandBufferHelper->bufferRead(resourceUseList, VK_ACCESS_UNIFORM_READ_BIT,
                                            kPipelineStageShaderMap[shaderType], &bufferHelper);
        }
    }

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::updateAtomicCounterBuffersDescriptorSet(
    const gl::ProgramState &programState,
    const gl::ShaderType shaderType,
    ContextVk *contextVk,
    vk::ResourceUseList *resourceUseList,
    vk::CommandBufferHelper *commandBufferHelper)
{
    const gl::State &glState = contextVk->getState();
    const std::vector<gl::AtomicCounterBuffer> &atomicCounterBuffers =
        programState.getAtomicCounterBuffers();

    if (atomicCounterBuffers.empty())
    {
        return angle::Result::Continue;
    }

    VkDescriptorSet descriptorSet =
        mDescriptorSets[ToUnderlying(DescriptorSetIndex::ShaderResource)];

    std::string blockName(sh::vk::kAtomicCountersBlockName);
    const ShaderInterfaceVariableInfo &info = mVariableInfoMap[shaderType][blockName];

    if (!info.activeStages[shaderType])
    {
        return angle::Result::Continue;
    }

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

        VkDescriptorBufferInfo &bufferInfo = contextVk->allocDescriptorBufferInfo();
        VkWriteDescriptorSet &writeInfo    = contextVk->allocWriteDescriptorSet();

        BufferVk *bufferVk             = vk::GetImpl(bufferBinding.get());
        vk::BufferHelper &bufferHelper = bufferVk->getBuffer();

        // Lazily allocate the descriptor set, since we may not need one if all of the buffers are
        // inactive.
        if (descriptorSet == VK_NULL_HANDLE)
        {
            ANGLE_TRY(allocateDescriptorSet(contextVk, DescriptorSetIndex::ShaderResource));
            descriptorSet = mDescriptorSets[ToUnderlying(DescriptorSetIndex::ShaderResource)];
        }
        ASSERT(descriptorSet != VK_NULL_HANDLE);
        VkDeviceSize size = GetShaderBufferBindingSize(bufferBinding);
        WriteBufferDescriptorSetBinding(bufferHelper, bufferBinding.getOffset(), size,
                                        descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                        info.binding, binding, requiredOffsetAlignment, &bufferInfo,
                                        &writeInfo);

        // We set SHADER_READ_BIT to be conservative.
        commandBufferHelper->bufferWrite(
            resourceUseList, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            kPipelineStageShaderMap[shaderType], vk::AliasingMode::Allowed, &bufferHelper);

        writtenBindings.set(binding);
    }

    ASSERT(descriptorSet != VK_NULL_HANDLE);

    // Bind the empty buffer to every array slot that's unused.
    vk::BufferHelper &emptyBuffer = contextVk->getEmptyBuffer();
    emptyBuffer.retain(&contextVk->getResourceUseList());
    size_t count                        = (~writtenBindings).count();
    VkDescriptorBufferInfo *bufferInfos = contextVk->allocDescriptorBufferInfos(count);
    VkWriteDescriptorSet *writeInfos    = contextVk->allocWriteDescriptorSets(count);
    size_t writeCount                   = 0;
    for (size_t binding : ~writtenBindings)
    {
        bufferInfos[writeCount].buffer = emptyBuffer.getBuffer().getHandle();
        bufferInfos[writeCount].offset = 0;
        bufferInfos[writeCount].range  = VK_WHOLE_SIZE;

        writeInfos[writeCount].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeInfos[writeCount].pNext            = nullptr;
        writeInfos[writeCount].dstSet           = descriptorSet;
        writeInfos[writeCount].dstBinding       = info.binding;
        writeInfos[writeCount].dstArrayElement  = static_cast<uint32_t>(binding);
        writeInfos[writeCount].descriptorCount  = 1;
        writeInfos[writeCount].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeInfos[writeCount].pImageInfo       = nullptr;
        writeInfos[writeCount].pBufferInfo      = &bufferInfos[writeCount];
        writeInfos[writeCount].pTexelBufferView = nullptr;
        writeCount++;
    }

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::updateImagesDescriptorSet(
    const gl::ProgramExecutable &executable,
    const gl::ShaderType shaderType,
    ContextVk *contextVk)
{
    const gl::State &glState                           = contextVk->getState();
    const std::vector<gl::ImageBinding> &imageBindings = executable.getImageBindings();
    const std::vector<gl::LinkedUniform> &uniforms     = executable.getUniforms();

    if (imageBindings.empty())
    {
        return angle::Result::Continue;
    }

    VkDescriptorSet descriptorSet =
        mDescriptorSets[ToUnderlying(DescriptorSetIndex::ShaderResource)];

    const gl::ActiveTextureArray<TextureVk *> &activeImages = contextVk->getActiveImages();

    bool useOldRewriteStructSamplers = contextVk->useOldRewriteStructSamplers();
    std::unordered_map<std::string, uint32_t> mappedImageNameToArrayOffset;

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

        std::string mappedImageName;
        if (!useOldRewriteStructSamplers)
        {
            mappedImageName = GlslangGetMappedSamplerName(imageUniform.mappedName);
        }
        else
        {
            mappedImageName = imageUniform.mappedName;
        }

        GetImageNameWithoutIndices(&mappedImageName);

        uint32_t arrayOffset = 0;
        uint32_t arraySize   = static_cast<uint32_t>(imageBinding.boundImageUnits.size());

        if (!useOldRewriteStructSamplers)
        {
            arrayOffset = mappedImageNameToArrayOffset[mappedImageName];
            // Front-end generates array elements in order, so we can just increment
            // the offset each time we process a nested array.
            mappedImageNameToArrayOffset[mappedImageName] += arraySize;
        }

        VkDescriptorImageInfo *imageInfos = contextVk->allocDescriptorImageInfos(arraySize);
        VkWriteDescriptorSet *writeInfos  = contextVk->allocWriteDescriptorSets(arraySize);
        for (uint32_t arrayElement = 0; arrayElement < arraySize; ++arrayElement)
        {
            GLuint imageUnit             = imageBinding.boundImageUnits[arrayElement];
            const gl::ImageUnit &binding = glState.getImageUnit(imageUnit);
            TextureVk *textureVk         = activeImages[imageUnit];

            vk::ImageHelper *image         = &textureVk->getImage();
            const vk::ImageView *imageView = nullptr;

            ANGLE_TRY(textureVk->getStorageImageView(contextVk, binding, &imageView));

            // Note: binding.access is unused because it is implied by the shader.

            // Lazily allocate the descriptor set, since we may not need one if all of the image
            // uniforms are inactive.
            if (descriptorSet == VK_NULL_HANDLE)
            {
                ANGLE_TRY(allocateDescriptorSet(contextVk, DescriptorSetIndex::ShaderResource));
                descriptorSet = mDescriptorSets[ToUnderlying(DescriptorSetIndex::ShaderResource)];
            }
            ASSERT(descriptorSet != VK_NULL_HANDLE);

            imageInfos[arrayElement].sampler     = VK_NULL_HANDLE;
            imageInfos[arrayElement].imageView   = imageView->getHandle();
            imageInfos[arrayElement].imageLayout = image->getCurrentLayout();

            ShaderInterfaceVariableInfoMap &variableInfoMap = mVariableInfoMap[shaderType];
            const std::string imageName                     = useOldRewriteStructSamplers
                                              ? GetMappedSamplerNameOld(imageUniform.name)
                                              : GlslangGetMappedSamplerName(imageUniform.name);
            ShaderInterfaceVariableInfo &info = variableInfoMap[imageName];

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
    vk::ResourceUseList *resourceUseList,
    vk::CommandBufferHelper *commandBufferHelper)
{
    const gl::ProgramExecutable *executable = contextVk->getState().getProgramExecutable();
    ASSERT(executable);
    gl::ShaderMap<const gl::ProgramState *> programStates;
    fillProgramStateMap(contextVk, &programStates);

    // Reset the descriptor set handles so we only allocate a new one when necessary.
    mDescriptorSets[ToUnderlying(DescriptorSetIndex::ShaderResource)]      = VK_NULL_HANDLE;
    mEmptyDescriptorSets[ToUnderlying(DescriptorSetIndex::ShaderResource)] = VK_NULL_HANDLE;

    for (const gl::ShaderType shaderType : executable->getLinkedShaderStages())
    {
        const gl::ProgramState *programState = programStates[shaderType];
        ASSERT(programState);

        ANGLE_TRY(updateBuffersDescriptorSet(contextVk, shaderType, resourceUseList,
                                             commandBufferHelper, programState->getUniformBlocks(),
                                             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER));
        ANGLE_TRY(updateBuffersDescriptorSet(
            contextVk, shaderType, resourceUseList, commandBufferHelper,
            programState->getShaderStorageBlocks(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
        ANGLE_TRY(updateAtomicCounterBuffersDescriptorSet(*programState, shaderType, contextVk,
                                                          resourceUseList, commandBufferHelper));
        angle::Result status =
            updateImagesDescriptorSet(programState->getExecutable(), shaderType, contextVk);
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
    vk::BufferHelper *defaultUniformBuffer,
    ContextVk *contextVk,
    const vk::UniformsAndXfbDesc &xfbBufferDesc)
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
                contextVk, executable.getTransformFeedbackBufferCount(),
                mDescriptorSets[ToUnderlying(DescriptorSetIndex::UniformsAndXfb)]);
        }
        return;
    }

    TransformFeedbackVk *transformFeedbackVk = vk::GetImpl(glState.getCurrentTransformFeedback());
    transformFeedbackVk->updateDescriptorSet(
        contextVk, programState, mDescriptorSets[ToUnderlying(DescriptorSetIndex::UniformsAndXfb)]);
}

angle::Result ProgramExecutableVk::updateTexturesDescriptorSet(ContextVk *contextVk)
{
    const gl::ProgramExecutable *executable = contextVk->getState().getProgramExecutable();
    ASSERT(executable);

    if (!executable->hasTextures())
    {
        return angle::Result::Continue;
    }

    const vk::TextureDescriptorDesc &texturesDesc = contextVk->getActiveTexturesDesc();

    auto iter = mTextureDescriptorsCache.find(texturesDesc);
    if (iter != mTextureDescriptorsCache.end())
    {
        mDescriptorSets[ToUnderlying(DescriptorSetIndex::Texture)] = iter->second;
        // The descriptor pool that this descriptor set was allocated from needs to be retained each
        // time the descriptor set is used in a new command.
        mDescriptorPoolBindings[ToUnderlying(DescriptorSetIndex::Texture)].get().retain(
            &contextVk->getResourceUseList());
        return angle::Result::Continue;
    }

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    const gl::ActiveTextureArray<vk::TextureUnit> &activeTextures = contextVk->getActiveTextures();

    bool emulateSeamfulCubeMapSampling = contextVk->emulateSeamfulCubeMapSampling();
    bool useOldRewriteStructSamplers   = contextVk->useOldRewriteStructSamplers();

    gl::ShaderMap<const gl::ProgramState *> programStates;
    fillProgramStateMap(contextVk, &programStates);

    for (const gl::ShaderType shaderType : executable->getLinkedShaderStages())
    {
        std::unordered_map<std::string, uint32_t> mappedSamplerNameToArrayOffset;
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

            uint32_t arrayOffset = 0;
            uint32_t arraySize   = static_cast<uint32_t>(samplerBinding.boundTextureUnits.size());

            if (!useOldRewriteStructSamplers)
            {
                arrayOffset = mappedSamplerNameToArrayOffset[mappedSamplerName];
                // Front-end generates array elements in order, so we can just increment
                // the offset each time we process a nested array.
                mappedSamplerNameToArrayOffset[mappedSamplerName] += arraySize;
            }

            VkDescriptorImageInfo *imageInfos = contextVk->allocDescriptorImageInfos(arraySize);
            VkWriteDescriptorSet *writeInfos  = contextVk->allocWriteDescriptorSets(arraySize);
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
                ShaderInterfaceVariableInfoMap &variableInfoMap = mVariableInfoMap[shaderType];
                const std::string samplerName =
                    contextVk->getRenderer()->getFeatures().forceOldRewriteStructSamplers.enabled
                        ? GetMappedSamplerNameOld(samplerUniform.name)
                        : GlslangGetMappedSamplerName(samplerUniform.name);
                ShaderInterfaceVariableInfo &info = variableInfoMap[samplerName];

                // Lazily allocate the descriptor set, since we may not need one if all of the
                // sampler uniforms are inactive.
                if (descriptorSet == VK_NULL_HANDLE)
                {
                    bool newPoolAllocated;
                    ANGLE_TRY(allocateDescriptorSetAndGetInfo(
                        contextVk, DescriptorSetIndex::Texture, &newPoolAllocated));

                    // Clear descriptor set cache. It may no longer be valid.
                    if (newPoolAllocated)
                    {
                        mTextureDescriptorsCache.clear();
                    }

                    descriptorSet = mDescriptorSets[ToUnderlying(DescriptorSetIndex::Texture)];
                    mTextureDescriptorsCache.emplace(texturesDesc, descriptorSet);
                }
                ASSERT(descriptorSet != VK_NULL_HANDLE);

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
                                                        vk::CommandBuffer *commandBuffer)
{
    // Can probably use better dirty bits here.

    // Find the maximum non-null descriptor set.  This is used in conjunction with a driver
    // workaround to bind empty descriptor sets only for gaps in between 0 and max and avoid
    // binding unnecessary empty descriptor sets for the sets beyond max.
    const size_t descriptorSetStart = ToUnderlying(DescriptorSetIndex::UniformsAndXfb);
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

                ++mObjectPerfCounters.descriptorSetsAllocated[descriptorSetIndex];
            }
            descSet = mEmptyDescriptorSets[descriptorSetIndex];
        }

        // Default uniforms are encompassed in a block per shader stage, and they are assigned
        // through dynamic uniform buffers (requiring dynamic offsets).  No other descriptor
        // requires a dynamic offset.
        const uint32_t uniformBlockOffsetCount =
            descriptorSetIndex == ToUnderlying(DescriptorSetIndex::UniformsAndXfb)
                ? static_cast<uint32_t>(mNumDefaultUniformDescriptors)
                : 0;

        commandBuffer->bindDescriptorSets(getPipelineLayout(), pipelineBindPoint,
                                          static_cast<DescriptorSetIndex>(descriptorSetIndex), 1,
                                          &descSet, uniformBlockOffsetCount,
                                          mDynamicBufferOffsets.data());
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

    {
        std::ostringstream text;

        for (size_t descriptorSetIndex = 0;
             descriptorSetIndex < mObjectPerfCounters.descriptorSetsAllocated.size();
             ++descriptorSetIndex)
        {
            uint32_t count = mObjectPerfCounters.descriptorSetsAllocated[descriptorSetIndex];
            if (count > 0)
            {
                text << "    DescriptorSetIndex " << descriptorSetIndex << ": " << count << "\n";
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
}

}  // namespace rx
