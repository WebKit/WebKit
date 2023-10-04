//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramExecutableVk.cpp: Collects the information and interfaces common to both ProgramVks and
// ProgramPipelineVks in order to execute/draw with either.

#include "libANGLE/renderer/vulkan/ProgramExecutableVk.h"

#include "common/string_utils.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
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

// Limit decompressed vulkan pipelines to 10MB per program.
static constexpr size_t kMaxLocalPipelineCacheSize = 10 * 1024 * 1024;

uint8_t GetGraphicsProgramIndex(ProgramTransformOptions transformOptions)
{
    return gl::bitCast<uint8_t, ProgramTransformOptions>(transformOptions);
}

bool ValidateTransformedSpirV(vk::Context *context,
                              const gl::ShaderBitSet &linkedShaderStages,
                              const ShaderInterfaceVariableInfoMap &variableInfoMap,
                              const gl::ShaderMap<angle::spirv::Blob> &spirvBlobs)
{
    gl::ShaderType lastPreFragmentStage = gl::GetLastPreFragmentStage(linkedShaderStages);

    for (gl::ShaderType shaderType : linkedShaderStages)
    {
        SpvTransformOptions options;
        options.shaderType = shaderType;
        options.isLastPreFragmentStage =
            shaderType == lastPreFragmentStage && shaderType != gl::ShaderType::TessControl;
        options.isTransformFeedbackStage = options.isLastPreFragmentStage;
        options.useSpirvVaryingPrecisionFixer =
            context->getFeatures().varyingsRequireMatchingPrecisionInSpirv.enabled;

        angle::spirv::Blob transformed;
        if (SpvTransformSpirvCode(options, variableInfoMap, spirvBlobs[shaderType], &transformed) !=
            angle::Result::Continue)
        {
            return false;
        }
    }
    return true;
}

uint32_t GetInterfaceBlockArraySize(const std::vector<gl::InterfaceBlock> &blocks,
                                    uint32_t bufferIndex)
{
    const gl::InterfaceBlock &block = blocks[bufferIndex];

    if (!block.pod.isArray)
    {
        return 1;
    }

    ASSERT(block.pod.arrayElement == 0);

    // Search consecutively until all array indices of this block are visited.
    uint32_t arraySize;
    for (arraySize = 1; bufferIndex + arraySize < blocks.size(); ++arraySize)
    {
        const gl::InterfaceBlock &nextBlock = blocks[bufferIndex + arraySize];

        if (nextBlock.pod.arrayElement != arraySize)
        {
            break;
        }

        // It's unexpected for an array to start at a non-zero array size, so we can always rely on
        // the sequential `arrayElement`s to belong to the same block.
        ASSERT(nextBlock.name == block.name);
        ASSERT(nextBlock.pod.isArray);
    }

    return arraySize;
}

void SetupDefaultPipelineState(const vk::Context *context,
                               const gl::ProgramExecutable &glExecutable,
                               gl::PrimitiveMode mode,
                               vk::PipelineRobustness pipelineRobustness,
                               vk::PipelineProtectedAccess pipelineProtectedAccess,
                               vk::GraphicsPipelineDesc *graphicsPipelineDescOut)
{
    graphicsPipelineDescOut->initDefaults(context, vk::GraphicsPipelineSubset::Complete,
                                          pipelineRobustness, pipelineProtectedAccess);
    graphicsPipelineDescOut->setTopology(mode);
    graphicsPipelineDescOut->setRenderPassSampleCount(1);
    graphicsPipelineDescOut->setRenderPassFramebufferFetchMode(glExecutable.usesFramebufferFetch());

    graphicsPipelineDescOut->setVertexShaderComponentTypes(
        glExecutable.getNonBuiltinAttribLocationsMask(), glExecutable.getAttributesTypeMask());

    const std::vector<gl::ProgramOutput> &outputVariables    = glExecutable.getOutputVariables();
    const std::vector<gl::VariableLocation> &outputLocations = glExecutable.getOutputLocations();

    for (const gl::VariableLocation &outputLocation : outputLocations)
    {
        if (outputLocation.arrayIndex == 0 && outputLocation.used() && !outputLocation.ignored)
        {
            const gl::ProgramOutput &outputVar = outputVariables[outputLocation.index];

            if (angle::BeginsWith(outputVar.name, "gl_") && outputVar.name != "gl_FragColor")
            {
                continue;
            }

            uint32_t location = 0;
            if (outputVar.pod.location != -1)
            {
                location = outputVar.pod.location;
            }

            GLenum type            = gl::VariableComponentType(outputVar.pod.type);
            angle::FormatID format = angle::FormatID::R8G8B8A8_UNORM;
            if (type == GL_INT)
            {
                format = angle::FormatID::R8G8B8A8_SINT;
            }
            else if (type == GL_UNSIGNED_INT)
            {
                format = angle::FormatID::R8G8B8A8_UINT;
            }

            const size_t arraySize = outputVar.isArray() ? outputVar.getOutermostArraySize() : 1;
            for (size_t arrayIndex = 0; arrayIndex < arraySize; ++arrayIndex)
            {
                graphicsPipelineDescOut->setRenderPassColorAttachmentFormat(location + arrayIndex,
                                                                            format);
            }
        }
    }

    for (const gl::ProgramOutput &outputVar : outputVariables)
    {
        if (outputVar.name == "gl_FragColor" || outputVar.name == "gl_FragData")
        {
            const size_t arraySize = outputVar.isArray() ? outputVar.getOutermostArraySize() : 1;
            for (size_t arrayIndex = 0; arrayIndex < arraySize; ++arrayIndex)
            {
                graphicsPipelineDescOut->setRenderPassColorAttachmentFormat(
                    arrayIndex, angle::FormatID::R8G8B8A8_UNORM);
            }
        }
    }
}

void GetPipelineCacheData(ContextVk *contextVk,
                          const vk::PipelineCache &pipelineCache,
                          angle::MemoryBuffer *cacheDataOut)
{
    ASSERT(pipelineCache.valid() || contextVk->getState().isGLES1() ||
           !contextVk->getFeatures().warmUpPipelineCacheAtLink.enabled ||
           !contextVk->getFeatures().hasEffectivePipelineCacheSerialization.enabled);
    if (!pipelineCache.valid() ||
        !contextVk->getFeatures().hasEffectivePipelineCacheSerialization.enabled)
    {
        return;
    }

    // Extract the pipeline data.  If failed, or empty, it's simply not stored on disk.
    size_t pipelineCacheSize = 0;
    VkResult result =
        pipelineCache.getCacheData(contextVk->getDevice(), &pipelineCacheSize, nullptr);
    if (result != VK_SUCCESS || pipelineCacheSize == 0)
    {
        return;
    }

    if (contextVk->getFeatures().enablePipelineCacheDataCompression.enabled)
    {
        std::vector<uint8_t> pipelineCacheData(pipelineCacheSize);
        result = pipelineCache.getCacheData(contextVk->getDevice(), &pipelineCacheSize,
                                            pipelineCacheData.data());
        if (result != VK_SUCCESS && result != VK_INCOMPLETE)
        {
            return;
        }

        // Compress it.
        if (!egl::CompressBlobCacheData(pipelineCacheData.size(), pipelineCacheData.data(),
                                        cacheDataOut))
        {
            cacheDataOut->clear();
        }
    }
    else
    {
        if (!cacheDataOut->resize(pipelineCacheSize))
        {
            ERR() << "Failed to allocate memory for pipeline cache data.";
            return;
        }
        result = pipelineCache.getCacheData(contextVk->getDevice(), &pipelineCacheSize,
                                            cacheDataOut->data());
        if (result != VK_SUCCESS && result != VK_INCOMPLETE)
        {
            cacheDataOut->clear();
        }
    }
}

vk::SpecializationConstants MakeSpecConsts(ProgramTransformOptions transformOptions,
                                           const vk::GraphicsPipelineDesc &desc)
{
    vk::SpecializationConstants specConsts;

    specConsts.surfaceRotation = transformOptions.surfaceRotation;
    specConsts.dither          = desc.getEmulatedDitherControl();

    return specConsts;
}

template <typename T>
void UpdateDefaultUniformBlock(GLsizei count,
                               uint32_t arrayIndex,
                               int componentCount,
                               const T *v,
                               const sh::BlockMemberInfo &layoutInfo,
                               angle::MemoryBuffer *uniformData)
{
    const int elementSize = sizeof(T) * componentCount;

    uint8_t *dst = uniformData->data() + layoutInfo.offset;
    if (layoutInfo.arrayStride == 0 || layoutInfo.arrayStride == elementSize)
    {
        uint32_t arrayOffset = arrayIndex * layoutInfo.arrayStride;
        uint8_t *writePtr    = dst + arrayOffset;
        ASSERT(writePtr + (elementSize * count) <= uniformData->data() + uniformData->size());
        memcpy(writePtr, v, elementSize * count);
    }
    else
    {
        // Have to respect the arrayStride between each element of the array.
        int maxIndex = arrayIndex + count;
        for (int writeIndex = arrayIndex, readIndex = 0; writeIndex < maxIndex;
             writeIndex++, readIndex++)
        {
            const int arrayOffset = writeIndex * layoutInfo.arrayStride;
            uint8_t *writePtr     = dst + arrayOffset;
            const T *readPtr      = v + (readIndex * componentCount);
            ASSERT(writePtr + elementSize <= uniformData->data() + uniformData->size());
            memcpy(writePtr, readPtr, elementSize);
        }
    }
}

template <typename T>
void ReadFromDefaultUniformBlock(int componentCount,
                                 uint32_t arrayIndex,
                                 T *dst,
                                 const sh::BlockMemberInfo &layoutInfo,
                                 const angle::MemoryBuffer *uniformData)
{
    ASSERT(layoutInfo.offset != -1);

    const int elementSize = sizeof(T) * componentCount;
    const uint8_t *source = uniformData->data() + layoutInfo.offset;

    if (layoutInfo.arrayStride == 0 || layoutInfo.arrayStride == elementSize)
    {
        const uint8_t *readPtr = source + arrayIndex * layoutInfo.arrayStride;
        memcpy(dst, readPtr, elementSize);
    }
    else
    {
        // Have to respect the arrayStride between each element of the array.
        const int arrayOffset  = arrayIndex * layoutInfo.arrayStride;
        const uint8_t *readPtr = source + arrayOffset;
        memcpy(dst, readPtr, elementSize);
    }
}
}  // namespace

DefaultUniformBlockVk::DefaultUniformBlockVk() = default;

DefaultUniformBlockVk::~DefaultUniformBlockVk() = default;

// ShaderInfo implementation.
ShaderInfo::ShaderInfo() {}

ShaderInfo::~ShaderInfo() = default;

angle::Result ShaderInfo::initShaders(vk::Context *context,
                                      const gl::ShaderBitSet &linkedShaderStages,
                                      const gl::ShaderMap<const angle::spirv::Blob *> &spirvBlobs,
                                      const ShaderInterfaceVariableInfoMap &variableInfoMap,
                                      bool isGLES1)
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
    // Don't validate GLES1 programs because they are always created right before a draw, so they
    // will naturally be validated.  This improves GLES1 test run times.
    if (!isGLES1)
    {
        ASSERT(ValidateTransformedSpirV(context, linkedShaderStages, variableInfoMap, mSpirvBlobs));
    }

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
        stream->readVector(&mSpirvBlobs[shaderType]);
    }

    mIsInitialized = true;
}

void ShaderInfo::save(gl::BinaryOutputStream *stream)
{
    ASSERT(valid());

    // Write out shader codes for all shader types
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        stream->writeVector(mSpirvBlobs[shaderType]);
    }
}

// ProgramInfo implementation.
ProgramInfo::ProgramInfo() {}

ProgramInfo::~ProgramInfo() = default;

angle::Result ProgramInfo::initProgram(vk::Context *context,
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

    SpvTransformOptions options;
    options.shaderType               = shaderType;
    options.isLastPreFragmentStage   = isLastPreFragmentStage;
    options.isTransformFeedbackStage = isLastPreFragmentStage && isTransformFeedbackProgram &&
                                       !optionBits.removeTransformFeedbackEmulation;
    options.isTransformFeedbackEmulated = context->getFeatures().emulateTransformFeedback.enabled;
    options.isMultisampledFramebufferFetch =
        optionBits.multiSampleFramebufferFetch && shaderType == gl::ShaderType::Fragment;
    options.enableSampleShading = optionBits.enableSampleShading;

    options.useSpirvVaryingPrecisionFixer =
        context->getFeatures().varyingsRequireMatchingPrecisionInSpirv.enabled;

    ANGLE_TRY(
        SpvTransformSpirvCode(options, variableInfoMap, originalSpirvBlob, &transformedSpirvBlob));
    ANGLE_TRY(vk::InitShaderModule(context, &mShaders[shaderType].get(),
                                   transformedSpirvBlob.data(),
                                   transformedSpirvBlob.size() * sizeof(uint32_t)));

    mProgramHelper.setShader(shaderType, &mShaders[shaderType]);

    return angle::Result::Continue;
}

void ProgramInfo::release(ContextVk *contextVk)
{
    mProgramHelper.release(contextVk);

    for (vk::RefCounted<vk::ShaderModule> &shader : mShaders)
    {
        shader.get().destroy(contextVk->getDevice());
    }
}

ProgramExecutableVk::ProgramExecutableVk(const gl::ProgramExecutable *executable)
    : ProgramExecutableImpl(executable),
      mNumDefaultUniformDescriptors(0),
      mImmutableSamplersMaxDescriptorCount(1),
      mUniformBufferDescriptorType(VK_DESCRIPTOR_TYPE_MAX_ENUM),
      mDynamicUniformDescriptorOffsets{}
{
    mDescriptorSets.fill(VK_NULL_HANDLE);
    for (std::shared_ptr<DefaultUniformBlockVk> &defaultBlock : mDefaultUniformBlocks)
    {
        defaultBlock = std::make_shared<DefaultUniformBlockVk>();
    }
}

ProgramExecutableVk::~ProgramExecutableVk()
{
    ASSERT(!mPipelineCache.valid());
}

void ProgramExecutableVk::destroy(const gl::Context *context)
{
    reset(vk::GetImpl(context));
}

void ProgramExecutableVk::resetLayout(ContextVk *contextVk)
{
    for (auto &descriptorSetLayout : mDescriptorSetLayouts)
    {
        descriptorSetLayout.reset();
    }
    mImmutableSamplersMaxDescriptorCount = 1;
    mImmutableSamplerIndexMap.clear();

    mDescriptorSets.fill(VK_NULL_HANDLE);
    mNumDefaultUniformDescriptors = 0;

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

    for (CompleteGraphicsPipelineCache &pipelines : mCompleteGraphicsPipelines)
    {
        pipelines.release(contextVk);
    }
    for (ShadersGraphicsPipelineCache &pipelines : mShadersGraphicsPipelines)
    {
        pipelines.release(contextVk);
    }
    for (vk::PipelineHelper &pipeline : mComputePipelines)
    {
        pipeline.release(contextVk);
    }

    // Program infos and pipeline layout must be released after pipelines are; they might be having
    // pending jobs that are referencing them.
    for (ProgramInfo &programInfo : mGraphicsProgramInfos)
    {
        programInfo.release(contextVk);
    }
    mComputeProgramInfo.release(contextVk);

    mPipelineLayout.reset();

    contextVk->onProgramExecutableReset(this);
}

void ProgramExecutableVk::reset(ContextVk *contextVk)
{
    resetLayout(contextVk);

    if (mPipelineCache.valid())
    {
        mPipelineCache.destroy(contextVk->getDevice());
    }
}

angle::Result ProgramExecutableVk::initializePipelineCache(vk::Context *context,
                                                           bool compressed,
                                                           const std::vector<uint8_t> &pipelineData)
{
    ASSERT(!mPipelineCache.valid());

    size_t dataSize            = pipelineData.size();
    const uint8_t *dataPointer = pipelineData.data();

    angle::MemoryBuffer uncompressedData;
    if (compressed)
    {
        if (!egl::DecompressBlobCacheData(dataPointer, dataSize, kMaxLocalPipelineCacheSize,
                                          &uncompressedData))
        {
            return angle::Result::Stop;
        }
        dataSize    = uncompressedData.size();
        dataPointer = uncompressedData.data();
    }

    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    pipelineCacheCreateInfo.sType           = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    pipelineCacheCreateInfo.initialDataSize = dataSize;
    pipelineCacheCreateInfo.pInitialData    = dataPointer;

    if (context->getFeatures().supportsPipelineCreationCacheControl.enabled)
    {
        pipelineCacheCreateInfo.flags |= VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT_EXT;
    }

    ANGLE_VK_TRY(context, mPipelineCache.init(context->getDevice(), pipelineCacheCreateInfo));

    // Merge the pipeline cache into RendererVk's.
    if (context->getFeatures().mergeProgramPipelineCachesToGlobalCache.enabled)
    {
        ANGLE_TRY(context->getRenderer()->mergeIntoPipelineCache(mPipelineCache));
    }

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::ensurePipelineCacheInitialized(vk::Context *context)
{
    if (!mPipelineCache.valid())
    {
        VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
        pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

        if (context->getFeatures().supportsPipelineCreationCacheControl.enabled)
        {
            pipelineCacheCreateInfo.flags |=
                VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT_EXT;
        }

        ANGLE_VK_TRY(context, mPipelineCache.init(context->getDevice(), pipelineCacheCreateInfo));
    }

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::load(ContextVk *contextVk,
                                        bool isSeparable,
                                        gl::BinaryInputStream *stream)
{
    mVariableInfoMap.load(stream);
    mOriginalShaderInfo.load(stream);

    // Deserializes the uniformLayout data of mDefaultUniformBlocks
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        stream->readVector(&mDefaultUniformBlocks[shaderType]->uniformLayout);
    }

    // Deserializes required uniform block memory sizes
    gl::ShaderMap<size_t> requiredBufferSize;
    stream->readPackedEnumMap(&requiredBufferSize);

    if (!isSeparable)
    {
        size_t compressedPipelineDataSize = 0;
        stream->readInt<size_t>(&compressedPipelineDataSize);

        std::vector<uint8_t> compressedPipelineData(compressedPipelineDataSize);
        if (compressedPipelineDataSize > 0)
        {
            bool compressedData = false;
            stream->readBool(&compressedData);
            stream->readBytes(compressedPipelineData.data(), compressedPipelineDataSize);
            // Initialize the pipeline cache based on cached data.
            ANGLE_TRY(initializePipelineCache(contextVk, compressedData, compressedPipelineData));
        }
    }

    // Initialize and resize the mDefaultUniformBlocks' memory
    ANGLE_TRY(resizeUniformBlockMemory(contextVk, requiredBufferSize));

    resetLayout(contextVk);
    ANGLE_TRY(createPipelineLayout(contextVk, &contextVk->getPipelineLayoutCache(),
                                   &contextVk->getDescriptorSetLayoutCache(), nullptr));

    return initializeDescriptorPools(contextVk, &contextVk->getDescriptorSetLayoutCache(),
                                     &contextVk->getMetaDescriptorPools());
}

void ProgramExecutableVk::save(ContextVk *contextVk,
                               bool isSeparable,
                               gl::BinaryOutputStream *stream)
{
    mVariableInfoMap.save(stream);
    mOriginalShaderInfo.save(stream);

    // Serializes the uniformLayout data of mDefaultUniformBlocks
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        stream->writeVector(mDefaultUniformBlocks[shaderType]->uniformLayout);
    }

    // Serializes required uniform block memory sizes
    gl::ShaderMap<size_t> uniformDataSize;
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        uniformDataSize[shaderType] = mDefaultUniformBlocks[shaderType]->uniformData.size();
    }
    stream->writePackedEnumMap(uniformDataSize);

    // Compress and save mPipelineCache.  Separable programs don't warm up the cache, while program
    // pipelines do.  However, currently ANGLE doesn't sync program pipelines to cache.  ANGLE could
    // potentially use VK_EXT_graphics_pipeline_library to create separate pipelines for
    // pre-rasterization and fragment subsets, but currently those subsets are bundled together.
    if (!isSeparable)
    {
        angle::MemoryBuffer cacheData;

        GetPipelineCacheData(contextVk, mPipelineCache, &cacheData);
        stream->writeInt(cacheData.size());
        if (cacheData.size() > 0)
        {
            stream->writeBool(contextVk->getFeatures().enablePipelineCacheDataCompression.enabled);
            stream->writeBytes(cacheData.data(), cacheData.size());
        }
    }
}

void ProgramExecutableVk::clearVariableInfoMap()
{
    mVariableInfoMap.clear();
}

angle::Result ProgramExecutableVk::warmUpPipelineCache(
    vk::Context *context,
    vk::PipelineRobustness pipelineRobustness,
    vk::PipelineProtectedAccess pipelineProtectedAccess,
    vk::RenderPass *temporaryCompatibleRenderPassOut)
{
    if (!context->getFeatures().warmUpPipelineCacheAtLink.enabled)
    {
        return angle::Result::Continue;
    }

    ANGLE_TRY(ensurePipelineCacheInitialized(context));

    // No synchronization necessary when accessing the program executable's cache as there is no
    // access to it from other threads at this point.
    vk::PipelineCacheAccess pipelineCache;
    pipelineCache.init(&mPipelineCache, nullptr);

    // Create a set of pipelines.  Ideally, that would be the entire set of possible pipelines so
    // there would be none created at draw time.  This is gated on the removal of some
    // specialization constants and adoption of VK_EXT_graphics_pipeline_library.
    const bool isCompute = mExecutable->hasLinkedShaderStage(gl::ShaderType::Compute);
    if (isCompute)
    {
        // There is no state associated with compute programs, so only one pipeline needs creation
        // to warm up the cache.
        vk::PipelineHelper *pipeline = nullptr;
        ANGLE_TRY(getOrCreateComputePipeline(context, &pipelineCache, PipelineSource::WarmUp,
                                             pipelineRobustness, pipelineProtectedAccess,
                                             &pipeline));

        // Merge the cache with RendererVk's
        if (context->getFeatures().mergeProgramPipelineCachesToGlobalCache.enabled)
        {
            ANGLE_TRY(context->getRenderer()->mergeIntoPipelineCache(mPipelineCache));
        }

        return angle::Result::Continue;
    }

    const vk::GraphicsPipelineDesc *descPtr = nullptr;
    vk::PipelineHelper *pipeline            = nullptr;
    vk::GraphicsPipelineDesc graphicsPipelineDesc;

    // It is only at drawcall time that we will have complete information required to build the
    // graphics pipeline descriptor. Use the most "commonly seen" state values and create the
    // pipeline.
    gl::PrimitiveMode mode = (mExecutable->hasLinkedShaderStage(gl::ShaderType::TessControl) ||
                              mExecutable->hasLinkedShaderStage(gl::ShaderType::TessEvaluation))
                                 ? gl::PrimitiveMode::Patches
                                 : gl::PrimitiveMode::TriangleStrip;
    SetupDefaultPipelineState(context, *mExecutable, mode, pipelineRobustness,
                              pipelineProtectedAccess, &graphicsPipelineDesc);

    // Create a temporary compatible RenderPass.  The render pass cache in ContextVk cannot be used
    // because this function may be called from a worker thread.
    vk::AttachmentOpsArray ops;
    RenderPassCache::InitializeOpsForCompatibleRenderPass(graphicsPipelineDesc.getRenderPassDesc(),
                                                          &ops);
    ANGLE_TRY(RenderPassCache::MakeRenderPass(context, graphicsPipelineDesc.getRenderPassDesc(),
                                              ops, temporaryCompatibleRenderPassOut, nullptr));

    // Variations that definitely matter:
    //
    // - PreRotation: It's a boolean specialization constant
    // - Depth correction: It's a SPIR-V transformation
    //
    // There are a number of states that are not currently dynamic (and may never be, such as sample
    // shading), but pre-creating shaders for them is impractical.  Most such state is likely unused
    // by most applications, but variations can be added here for certain apps that are known to
    // benefit from it.
    ProgramTransformOptions transformOptions = {};

    angle::FixedVector<bool, 2> surfaceRotationVariations = {false};
    if (context->getFeatures().enablePreRotateSurfaces.enabled &&
        !context->getFeatures().preferDriverUniformOverSpecConst.enabled)
    {
        surfaceRotationVariations.push_back(true);
    }

    // Only build the shaders subset of the pipeline if VK_EXT_graphics_pipeline_library is
    // supported, especially since the vertex input and fragment output state set up here is
    // completely bogus.
    vk::GraphicsPipelineSubset subset =
        context->getFeatures().supportsGraphicsPipelineLibrary.enabled
            ? vk::GraphicsPipelineSubset::Shaders
            : vk::GraphicsPipelineSubset::Complete;

    for (bool rotation : surfaceRotationVariations)
    {
        transformOptions.surfaceRotation = rotation;

        ANGLE_TRY(createGraphicsPipelineImpl(
            context, transformOptions, subset, &pipelineCache, PipelineSource::WarmUp,
            graphicsPipelineDesc, *temporaryCompatibleRenderPassOut, &descPtr, &pipeline));
    }

    // Merge the cache with RendererVk's
    if (context->getFeatures().mergeProgramPipelineCachesToGlobalCache.enabled)
    {
        ANGLE_TRY(context->getRenderer()->mergeIntoPipelineCache(mPipelineCache));
    }

    return angle::Result::Continue;
}

void ProgramExecutableVk::addInterfaceBlockDescriptorSetDesc(
    const std::vector<gl::InterfaceBlock> &blocks,
    gl::ShaderBitSet shaderTypes,
    VkDescriptorType descType,
    vk::DescriptorSetLayoutDesc *descOut)
{
    for (uint32_t bufferIndex = 0, arraySize = 0; bufferIndex < blocks.size();
         bufferIndex += arraySize)
    {
        gl::InterfaceBlock block = blocks[bufferIndex];
        arraySize                = GetInterfaceBlockArraySize(blocks, bufferIndex);

        if (block.activeShaders().none())
        {
            continue;
        }

        const gl::ShaderType firstShaderType = block.getFirstActiveShaderType();
        const ShaderInterfaceVariableInfo &info =
            mVariableInfoMap.getVariableById(firstShaderType, block.getId(firstShaderType));

        const VkShaderStageFlags activeStages = gl_vk::GetShaderStageFlags(info.activeStages);

        descOut->update(info.binding, descType, arraySize, activeStages, nullptr);
    }
}

void ProgramExecutableVk::addAtomicCounterBufferDescriptorSetDesc(
    const std::vector<gl::AtomicCounterBuffer> &atomicCounterBuffers,
    vk::DescriptorSetLayoutDesc *descOut)
{
    if (atomicCounterBuffers.empty())
    {
        return;
    }

    const ShaderInterfaceVariableInfo &info =
        mVariableInfoMap.getAtomicCounterInfo(atomicCounterBuffers[0].getFirstActiveShaderType());
    VkShaderStageFlags activeStages = gl_vk::GetShaderStageFlags(info.activeStages);

    // A single storage buffer array is used for all stages for simplicity.
    descOut->update(info.binding, vk::kStorageBufferDescriptorType,
                    gl::IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS, activeStages, nullptr);
}

void ProgramExecutableVk::addImageDescriptorSetDesc(vk::DescriptorSetLayoutDesc *descOut)
{
    const std::vector<gl::ImageBinding> &imageBindings = mExecutable->getImageBindings();
    const std::vector<gl::LinkedUniform> &uniforms     = mExecutable->getUniforms();

    for (uint32_t imageIndex = 0; imageIndex < imageBindings.size(); ++imageIndex)
    {
        uint32_t uniformIndex = mExecutable->getUniformIndexFromImageIndex(imageIndex);
        const gl::LinkedUniform &imageUniform = uniforms[uniformIndex];

        // 2D arrays are split into multiple 1D arrays when generating LinkedUniforms. Since they
        // are flattened into one array, ignore the nonzero elements and expand the array to the
        // total array size.
        if (imageUniform.activeShaders().none() || imageUniform.getOuterArrayOffset() > 0)
        {
            ASSERT(gl::SamplerNameContainsNonZeroArrayElement(
                mExecutable->getUniformNameByIndex(uniformIndex)));
            continue;
        }

        ASSERT(!gl::SamplerNameContainsNonZeroArrayElement(
            mExecutable->getUniformNameByIndex(uniformIndex)));

        // The front-end always binds array image units sequentially.
        const gl::ImageBinding &imageBinding = imageBindings[imageIndex];
        uint32_t arraySize = static_cast<uint32_t>(imageBinding.boundImageUnits.size());
        arraySize *= imageUniform.getOuterArraySizeProduct();

        const gl::ShaderType firstShaderType = imageUniform.getFirstActiveShaderType();
        const ShaderInterfaceVariableInfo &info =
            mVariableInfoMap.getVariableById(firstShaderType, imageUniform.getId(firstShaderType));

        const VkShaderStageFlags activeStages = gl_vk::GetShaderStageFlags(info.activeStages);

        const VkDescriptorType descType = imageBinding.textureType == gl::TextureType::Buffer
                                              ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
                                              : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descOut->update(info.binding, descType, arraySize, activeStages, nullptr);
    }
}

void ProgramExecutableVk::addInputAttachmentDescriptorSetDesc(vk::DescriptorSetLayoutDesc *descOut)
{
    if (!mExecutable->getLinkedShaderStages()[gl::ShaderType::Fragment])
    {
        return;
    }

    if (!mExecutable->usesFramebufferFetch())
    {
        return;
    }

    const std::vector<gl::LinkedUniform> &uniforms = mExecutable->getUniforms();
    const uint32_t baseUniformIndex                = mExecutable->getFragmentInoutRange().low();
    const gl::LinkedUniform &baseInputAttachment   = uniforms.at(baseUniformIndex);

    const ShaderInterfaceVariableInfo &baseInfo = mVariableInfoMap.getVariableById(
        gl::ShaderType::Fragment, baseInputAttachment.getId(gl::ShaderType::Fragment));

    uint32_t baseBinding = baseInfo.binding - baseInputAttachment.getLocation();

    for (uint32_t colorIndex = 0; colorIndex < gl::IMPLEMENTATION_MAX_DRAW_BUFFERS; ++colorIndex)
    {
        descOut->update(baseBinding, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1,
                        VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
        baseBinding++;
    }
}

angle::Result ProgramExecutableVk::addTextureDescriptorSetDesc(
    vk::Context *context,
    const gl::ActiveTextureArray<TextureVk *> *activeTextures,
    vk::DescriptorSetLayoutDesc *descOut)
{
    const std::vector<gl::SamplerBinding> &samplerBindings = mExecutable->getSamplerBindings();
    const std::vector<gl::LinkedUniform> &uniforms         = mExecutable->getUniforms();
    const std::vector<GLuint> &samplerBoundTextureUnits =
        mExecutable->getSamplerBoundTextureUnits();

    for (uint32_t textureIndex = 0; textureIndex < samplerBindings.size(); ++textureIndex)
    {
        uint32_t uniformIndex = mExecutable->getUniformIndexFromSamplerIndex(textureIndex);
        const gl::LinkedUniform &samplerUniform = uniforms[uniformIndex];

        // 2D arrays are split into multiple 1D arrays when generating LinkedUniforms. Since they
        // are flattened into one array, ignore the nonzero elements and expand the array to the
        // total array size.
        if (samplerUniform.activeShaders().none() || samplerUniform.getOuterArrayOffset() > 0)
        {
            ASSERT(gl::SamplerNameContainsNonZeroArrayElement(
                mExecutable->getUniformNameByIndex(uniformIndex)));
            continue;
        }

        ASSERT(!gl::SamplerNameContainsNonZeroArrayElement(
            mExecutable->getUniformNameByIndex(uniformIndex)));

        // The front-end always binds array sampler units sequentially.
        const gl::SamplerBinding &samplerBinding = samplerBindings[textureIndex];
        uint32_t arraySize = static_cast<uint32_t>(samplerBinding.textureUnitsCount);
        arraySize *= samplerUniform.getOuterArraySizeProduct();

        const gl::ShaderType firstShaderType    = samplerUniform.getFirstActiveShaderType();
        const ShaderInterfaceVariableInfo &info = mVariableInfoMap.getVariableById(
            firstShaderType, samplerUniform.getId(firstShaderType));

        const VkShaderStageFlags activeStages = gl_vk::GetShaderStageFlags(info.activeStages);

        // TODO: https://issuetracker.google.com/issues/158215272: how do we handle array of
        // immutable samplers?
        GLuint textureUnit = samplerBinding.getTextureUnit(samplerBoundTextureUnits, 0);
        if (activeTextures != nullptr &&
            (*activeTextures)[textureUnit]->getImage().hasImmutableSampler())
        {
            ASSERT(samplerBinding.textureUnitsCount == 1);

            // In the case of samplerExternal2DY2YEXT, we need
            // samplerYcbcrConversion object with IDENTITY conversion model
            bool isSamplerExternalY2Y =
                samplerBinding.samplerType == GL_SAMPLER_EXTERNAL_2D_Y2Y_EXT;

            // Always take the texture's sampler, that's only way to get to yuv conversion for
            // externalFormat
            const TextureVk *textureVk          = (*activeTextures)[textureUnit];
            const vk::Sampler &immutableSampler = textureVk->getSampler(isSamplerExternalY2Y).get();
            descOut->update(info.binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, arraySize,
                            activeStages, &immutableSampler);
            const vk::ImageHelper &image = textureVk->getImage();
            const vk::YcbcrConversionDesc ycbcrConversionDesc =
                isSamplerExternalY2Y ? image.getY2YConversionDesc()
                                     : image.getYcbcrConversionDesc();
            mImmutableSamplerIndexMap[ycbcrConversionDesc] = textureIndex;
            // The Vulkan spec has the following note -
            // All descriptors in a binding use the same maximum
            // combinedImageSamplerDescriptorCount descriptors to allow implementations to use a
            // uniform stride for dynamic indexing of the descriptors in the binding.
            uint64_t externalFormat        = image.getExternalFormat();
            uint32_t formatDescriptorCount = 0;

            RendererVk *renderer = context->getRenderer();

            if (externalFormat != 0)
            {
                ANGLE_TRY(renderer->getFormatDescriptorCountForExternalFormat(
                    context, externalFormat, &formatDescriptorCount));
            }
            else
            {
                VkFormat vkFormat = image.getActualVkFormat();
                ASSERT(vkFormat != 0);
                ANGLE_TRY(renderer->getFormatDescriptorCountForVkFormat(context, vkFormat,
                                                                        &formatDescriptorCount));
            }

            ASSERT(formatDescriptorCount > 0);
            mImmutableSamplersMaxDescriptorCount =
                std::max(mImmutableSamplersMaxDescriptorCount, formatDescriptorCount);
        }
        else
        {
            const VkDescriptorType descType = samplerBinding.textureType == gl::TextureType::Buffer
                                                  ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
                                                  : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descOut->update(info.binding, descType, arraySize, activeStages, nullptr);
        }
    }

    return angle::Result::Continue;
}

void ProgramExecutableVk::initializeWriteDescriptorDesc(vk::Context *context)
{
    const gl::ShaderBitSet &linkedShaderStages = mExecutable->getLinkedShaderStages();

    // Update mShaderResourceWriteDescriptorDescBuilder
    mShaderResourceWriteDescriptorDescs.reset();
    mShaderResourceWriteDescriptorDescs.updateShaderBuffers(
        mVariableInfoMap, mExecutable->getUniformBlocks(), getUniformBufferDescriptorType());
    mShaderResourceWriteDescriptorDescs.updateShaderBuffers(
        mVariableInfoMap, mExecutable->getShaderStorageBlocks(), getStorageBufferDescriptorType());
    mShaderResourceWriteDescriptorDescs.updateAtomicCounters(
        mVariableInfoMap, mExecutable->getAtomicCounterBuffers());
    mShaderResourceWriteDescriptorDescs.updateImages(*mExecutable, mVariableInfoMap);
    mShaderResourceWriteDescriptorDescs.updateDynamicDescriptorsCount();

    // Update mTextureWriteDescriptors
    mTextureWriteDescriptorDescs.reset();
    mTextureWriteDescriptorDescs.updateExecutableActiveTextures(mVariableInfoMap, *mExecutable);
    mTextureWriteDescriptorDescs.updateDynamicDescriptorsCount();

    // Update mDefaultUniformWriteDescriptors
    mDefaultUniformWriteDescriptorDescs.reset();
    mDefaultUniformWriteDescriptorDescs.updateDefaultUniform(linkedShaderStages, mVariableInfoMap,
                                                             *mExecutable);
    mDefaultUniformWriteDescriptorDescs.updateDynamicDescriptorsCount();

    mDefaultUniformAndXfbWriteDescriptorDescs.reset();
    if (mExecutable->hasTransformFeedbackOutput() &&
        context->getRenderer()->getFeatures().emulateTransformFeedback.enabled)
    {
        // Update mDefaultUniformAndXfbWriteDescriptorDescs for the emulation code path.
        mDefaultUniformAndXfbWriteDescriptorDescs.updateDefaultUniform(
            linkedShaderStages, mVariableInfoMap, *mExecutable);
        if (linkedShaderStages[gl::ShaderType::Vertex])
        {
            mDefaultUniformAndXfbWriteDescriptorDescs.updateTransformFeedbackWrite(mVariableInfoMap,
                                                                                   *mExecutable);
        }
        mDefaultUniformAndXfbWriteDescriptorDescs.updateDynamicDescriptorsCount();
    }
    else
    {
        // Otherwise it will be the same as default uniform
        mDefaultUniformAndXfbWriteDescriptorDescs = mDefaultUniformWriteDescriptorDescs;
    }
}

ProgramTransformOptions ProgramExecutableVk::getTransformOptions(
    ContextVk *contextVk,
    const vk::GraphicsPipelineDesc &desc)
{
    ProgramTransformOptions transformOptions = {};

    transformOptions.surfaceRotation = desc.getSurfaceRotation();
    transformOptions.removeTransformFeedbackEmulation =
        contextVk->getFeatures().emulateTransformFeedback.enabled &&
        !contextVk->getState().isTransformFeedbackActiveUnpaused();
    FramebufferVk *drawFrameBuffer = vk::GetImpl(contextVk->getState().getDrawFramebuffer());
    const bool hasFramebufferFetch = mExecutable->usesFramebufferFetch();
    const bool isMultisampled      = drawFrameBuffer->getSamples() > 1;
    transformOptions.multiSampleFramebufferFetch = hasFramebufferFetch && isMultisampled;
    transformOptions.enableSampleShading =
        contextVk->getState().isSampleShadingEnabled() && isMultisampled;

    return transformOptions;
}

angle::Result ProgramExecutableVk::initGraphicsShaderPrograms(
    vk::Context *context,
    ProgramTransformOptions transformOptions,
    vk::ShaderProgramHelper **shaderProgramOut)
{
    ASSERT(mExecutable->hasLinkedShaderStage(gl::ShaderType::Vertex));

    const uint8_t programIndex                = GetGraphicsProgramIndex(transformOptions);
    ProgramInfo &programInfo                  = mGraphicsProgramInfos[programIndex];
    const gl::ShaderBitSet linkedShaderStages = mExecutable->getLinkedShaderStages();
    gl::ShaderType lastPreFragmentStage       = gl::GetLastPreFragmentStage(linkedShaderStages);

    const bool isTransformFeedbackProgram =
        !mExecutable->getLinkedTransformFeedbackVaryings().empty();

    for (gl::ShaderType shaderType : linkedShaderStages)
    {
        ANGLE_TRY(initGraphicsShaderProgram(context, shaderType, shaderType == lastPreFragmentStage,
                                            isTransformFeedbackProgram, transformOptions,
                                            &programInfo, mVariableInfoMap));
    }

    *shaderProgramOut = programInfo.getShaderProgram();
    ASSERT(*shaderProgramOut);

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::createGraphicsPipelineImpl(
    vk::Context *context,
    ProgramTransformOptions transformOptions,
    vk::GraphicsPipelineSubset pipelineSubset,
    vk::PipelineCacheAccess *pipelineCache,
    PipelineSource source,
    const vk::GraphicsPipelineDesc &desc,
    const vk::RenderPass &compatibleRenderPass,
    const vk::GraphicsPipelineDesc **descPtrOut,
    vk::PipelineHelper **pipelineOut)
{
    vk::ShaderProgramHelper *shaderProgram = nullptr;
    ANGLE_TRY(initGraphicsShaderPrograms(context, transformOptions, &shaderProgram));

    const uint8_t programIndex = GetGraphicsProgramIndex(transformOptions);

    // Set specialization constants.  These are also a part of GraphicsPipelineDesc, so that a
    // change in specialization constants also results in a new pipeline.
    vk::SpecializationConstants specConsts = MakeSpecConsts(transformOptions, desc);

    if (pipelineSubset == vk::GraphicsPipelineSubset::Complete)
    {
        CompleteGraphicsPipelineCache &pipelines = mCompleteGraphicsPipelines[programIndex];
        return shaderProgram->createGraphicsPipeline(
            context, &pipelines, pipelineCache, compatibleRenderPass, getPipelineLayout(), source,
            desc, specConsts, descPtrOut, pipelineOut);
    }
    else
    {
        // Vertex input and fragment output subsets are independent of shaders, and are not created
        // through the program executable.
        ASSERT(pipelineSubset == vk::GraphicsPipelineSubset::Shaders);

        ShadersGraphicsPipelineCache &pipelines = mShadersGraphicsPipelines[programIndex];
        return shaderProgram->createGraphicsPipeline(
            context, &pipelines, pipelineCache, compatibleRenderPass, getPipelineLayout(), source,
            desc, specConsts, descPtrOut, pipelineOut);
    }
}

angle::Result ProgramExecutableVk::getGraphicsPipeline(ContextVk *contextVk,
                                                       vk::GraphicsPipelineSubset pipelineSubset,
                                                       const vk::GraphicsPipelineDesc &desc,
                                                       const vk::GraphicsPipelineDesc **descPtrOut,
                                                       vk::PipelineHelper **pipelineOut)
{
    ProgramTransformOptions transformOptions = getTransformOptions(contextVk, desc);

    vk::ShaderProgramHelper *shaderProgram = nullptr;
    ANGLE_TRY(initGraphicsShaderPrograms(contextVk, transformOptions, &shaderProgram));

    const uint8_t programIndex = GetGraphicsProgramIndex(transformOptions);

    *descPtrOut  = nullptr;
    *pipelineOut = nullptr;

    if (pipelineSubset == vk::GraphicsPipelineSubset::Complete)
    {
        mCompleteGraphicsPipelines[programIndex].getPipeline(desc, descPtrOut, pipelineOut);
    }
    else
    {
        // Vertex input and fragment output subsets are independent of shaders, and are not created
        // through the program executable.
        ASSERT(pipelineSubset == vk::GraphicsPipelineSubset::Shaders);

        mShadersGraphicsPipelines[programIndex].getPipeline(desc, descPtrOut, pipelineOut);
    }

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::createGraphicsPipeline(
    ContextVk *contextVk,
    vk::GraphicsPipelineSubset pipelineSubset,
    vk::PipelineCacheAccess *pipelineCache,
    PipelineSource source,
    const vk::GraphicsPipelineDesc &desc,
    const vk::GraphicsPipelineDesc **descPtrOut,
    vk::PipelineHelper **pipelineOut)
{
    ProgramTransformOptions transformOptions = getTransformOptions(contextVk, desc);

    // When creating monolithic pipelines, the renderer's pipeline cache is used as passed in.
    // When creating the shaders subset of pipelines, the program's own pipeline cache is used.
    vk::PipelineCacheAccess perProgramPipelineCache;
    const bool useProgramPipelineCache = pipelineSubset == vk::GraphicsPipelineSubset::Shaders;
    if (useProgramPipelineCache)
    {
        ANGLE_TRY(ensurePipelineCacheInitialized(contextVk));

        perProgramPipelineCache.init(&mPipelineCache, nullptr);
        pipelineCache = &perProgramPipelineCache;
    }

    // Pull in a compatible RenderPass.
    const vk::RenderPass *compatibleRenderPass = nullptr;
    ANGLE_TRY(contextVk->getRenderPassCache().getCompatibleRenderPass(
        contextVk, desc.getRenderPassDesc(), &compatibleRenderPass));

    ANGLE_TRY(createGraphicsPipelineImpl(contextVk, transformOptions, pipelineSubset, pipelineCache,
                                         source, desc, *compatibleRenderPass, descPtrOut,
                                         pipelineOut));

    if (useProgramPipelineCache &&
        contextVk->getFeatures().mergeProgramPipelineCachesToGlobalCache.enabled)
    {
        ANGLE_TRY(contextVk->getRenderer()->mergeIntoPipelineCache(mPipelineCache));
    }

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::linkGraphicsPipelineLibraries(
    ContextVk *contextVk,
    vk::PipelineCacheAccess *pipelineCache,
    const vk::GraphicsPipelineDesc &desc,
    vk::PipelineHelper *vertexInputPipeline,
    vk::PipelineHelper *shadersPipeline,
    vk::PipelineHelper *fragmentOutputPipeline,
    const vk::GraphicsPipelineDesc **descPtrOut,
    vk::PipelineHelper **pipelineOut)
{
    ProgramTransformOptions transformOptions = getTransformOptions(contextVk, desc);
    const uint8_t programIndex               = GetGraphicsProgramIndex(transformOptions);

    ANGLE_TRY(mCompleteGraphicsPipelines[programIndex].linkLibraries(
        contextVk, pipelineCache, desc, getPipelineLayout(), vertexInputPipeline, shadersPipeline,
        fragmentOutputPipeline, descPtrOut, pipelineOut));

    // If monolithic pipelines are preferred over libraries, create a task so that it can be created
    // asynchronously.
    if (contextVk->getFeatures().preferMonolithicPipelinesOverLibraries.enabled)
    {
        vk::SpecializationConstants specConsts = MakeSpecConsts(transformOptions, desc);

        mGraphicsProgramInfos[programIndex]
            .getShaderProgram()
            ->createMonolithicPipelineCreationTask(contextVk, pipelineCache, desc,
                                                   getPipelineLayout(), specConsts, *pipelineOut);
    }

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::getOrCreateComputePipeline(
    vk::Context *context,
    vk::PipelineCacheAccess *pipelineCache,
    PipelineSource source,
    vk::PipelineRobustness pipelineRobustness,
    vk::PipelineProtectedAccess pipelineProtectedAccess,
    vk::PipelineHelper **pipelineOut)
{
    ASSERT(mExecutable->hasLinkedShaderStage(gl::ShaderType::Compute));

    ANGLE_TRY(initComputeProgram(context, &mComputeProgramInfo, mVariableInfoMap));

    vk::ComputePipelineFlags pipelineFlags = {};
    if (pipelineRobustness == vk::PipelineRobustness::Robust)
    {
        pipelineFlags.set(vk::ComputePipelineFlag::Robust);
    }
    if (pipelineProtectedAccess == vk::PipelineProtectedAccess::Protected)
    {
        pipelineFlags.set(vk::ComputePipelineFlag::Protected);
    }

    vk::ShaderProgramHelper *shaderProgram = mComputeProgramInfo.getShaderProgram();
    ASSERT(shaderProgram);
    return shaderProgram->getOrCreateComputePipeline(context, &mComputePipelines, pipelineCache,
                                                     getPipelineLayout(), pipelineFlags, source,
                                                     pipelineOut);
}

angle::Result ProgramExecutableVk::createPipelineLayout(
    vk::Context *context,
    PipelineLayoutCache *pipelineLayoutCache,
    DescriptorSetLayoutCache *descriptorSetLayoutCache,
    gl::ActiveTextureArray<TextureVk *> *activeTextures)
{
    const gl::ShaderBitSet &linkedShaderStages = mExecutable->getLinkedShaderStages();

    // Store a reference to the pipeline and descriptor set layouts. This will create them if they
    // don't already exist in the cache.

    // Default uniforms and transform feedback:
    mDefaultUniformAndXfbSetDesc  = {};
    mNumDefaultUniformDescriptors = 0;
    for (gl::ShaderType shaderType : linkedShaderStages)
    {
        const ShaderInterfaceVariableInfo &info =
            mVariableInfoMap.getDefaultUniformInfo(shaderType);
        // Note that currently the default uniform block is added unconditionally.
        ASSERT(info.activeStages[shaderType]);

        mDefaultUniformAndXfbSetDesc.update(info.binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                            1, gl_vk::kShaderStageMap[shaderType], nullptr);
        mNumDefaultUniformDescriptors++;
    }

    gl::ShaderType linkedTransformFeedbackStage = mExecutable->getLinkedTransformFeedbackStage();
    bool hasXfbVaryings = linkedTransformFeedbackStage != gl::ShaderType::InvalidEnum &&
                          !mExecutable->getLinkedTransformFeedbackVaryings().empty();
    if (context->getFeatures().emulateTransformFeedback.enabled && hasXfbVaryings)
    {
        size_t xfbBufferCount = mExecutable->getTransformFeedbackBufferCount();
        for (uint32_t bufferIndex = 0; bufferIndex < xfbBufferCount; ++bufferIndex)
        {
            const uint32_t binding = mVariableInfoMap.getEmulatedXfbBufferBinding(bufferIndex);
            ASSERT(binding != std::numeric_limits<uint32_t>::max());

            mDefaultUniformAndXfbSetDesc.update(binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                                VK_SHADER_STAGE_VERTEX_BIT, nullptr);
        }
    }

    ANGLE_TRY(descriptorSetLayoutCache->getDescriptorSetLayout(
        context, mDefaultUniformAndXfbSetDesc,
        &mDescriptorSetLayouts[DescriptorSetIndex::UniformsAndXfb]));

    // Uniform and storage buffers, atomic counter buffers and images:
    mShaderResourceSetDesc = {};

    // Count the number of active uniform buffer descriptors.
    uint32_t numActiveUniformBufferDescriptors    = 0;
    const std::vector<gl::InterfaceBlock> &blocks = mExecutable->getUniformBlocks();
    for (uint32_t bufferIndex = 0; bufferIndex < blocks.size();)
    {
        const gl::InterfaceBlock &block = blocks[bufferIndex];
        const uint32_t arraySize        = GetInterfaceBlockArraySize(blocks, bufferIndex);
        bufferIndex += arraySize;

        if (block.activeShaders().any())
        {
            numActiveUniformBufferDescriptors += arraySize;
        }
    }

    // Decide if we should use dynamic or fixed descriptor types.
    VkPhysicalDeviceLimits limits = context->getRenderer()->getPhysicalDeviceProperties().limits;
    uint32_t totalDynamicUniformBufferCount =
        numActiveUniformBufferDescriptors + mNumDefaultUniformDescriptors;
    if (totalDynamicUniformBufferCount <= limits.maxDescriptorSetUniformBuffersDynamic)
    {
        mUniformBufferDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    }
    else
    {
        mUniformBufferDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }

    addInterfaceBlockDescriptorSetDesc(mExecutable->getUniformBlocks(), linkedShaderStages,
                                       mUniformBufferDescriptorType, &mShaderResourceSetDesc);
    addInterfaceBlockDescriptorSetDesc(mExecutable->getShaderStorageBlocks(), linkedShaderStages,
                                       vk::kStorageBufferDescriptorType, &mShaderResourceSetDesc);
    addAtomicCounterBufferDescriptorSetDesc(mExecutable->getAtomicCounterBuffers(),
                                            &mShaderResourceSetDesc);
    addImageDescriptorSetDesc(&mShaderResourceSetDesc);
    addInputAttachmentDescriptorSetDesc(&mShaderResourceSetDesc);

    ANGLE_TRY(descriptorSetLayoutCache->getDescriptorSetLayout(
        context, mShaderResourceSetDesc,
        &mDescriptorSetLayouts[DescriptorSetIndex::ShaderResource]));

    // Textures:
    mTextureSetDesc = {};
    ANGLE_TRY(addTextureDescriptorSetDesc(context, activeTextures, &mTextureSetDesc));

    ANGLE_TRY(descriptorSetLayoutCache->getDescriptorSetLayout(
        context, mTextureSetDesc, &mDescriptorSetLayouts[DescriptorSetIndex::Texture]));

    // Create pipeline layout with these 3 descriptor sets.
    vk::PipelineLayoutDesc pipelineLayoutDesc;
    pipelineLayoutDesc.updateDescriptorSetLayout(DescriptorSetIndex::UniformsAndXfb,
                                                 mDefaultUniformAndXfbSetDesc);
    pipelineLayoutDesc.updateDescriptorSetLayout(DescriptorSetIndex::ShaderResource,
                                                 mShaderResourceSetDesc);
    pipelineLayoutDesc.updateDescriptorSetLayout(DescriptorSetIndex::Texture, mTextureSetDesc);

    // Set up driver uniforms as push constants. The size is set for a graphics pipeline, as there
    // are more driver uniforms for a graphics pipeline than there are for a compute pipeline. As
    // for the shader stages, both graphics and compute stages are used.
    VkShaderStageFlags pushConstantShaderStageFlags =
        context->getRenderer()->getSupportedVulkanShaderStageMask();

    uint32_t pushConstantSize = GetDriverUniformSize(context, PipelineType::Graphics);
    pipelineLayoutDesc.updatePushConstantRange(pushConstantShaderStageFlags, 0, pushConstantSize);

    ANGLE_TRY(pipelineLayoutCache->getPipelineLayout(context, pipelineLayoutDesc,
                                                     mDescriptorSetLayouts, &mPipelineLayout));

    mDynamicUniformDescriptorOffsets.clear();
    mDynamicUniformDescriptorOffsets.resize(mExecutable->getLinkedShaderStageCount(), 0);

    initializeWriteDescriptorDesc(context);

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::initializeDescriptorPools(
    vk::Context *context,
    DescriptorSetLayoutCache *descriptorSetLayoutCache,
    vk::DescriptorSetArray<vk::MetaDescriptorPool> *metaDescriptorPools)
{
    ANGLE_TRY((*metaDescriptorPools)[DescriptorSetIndex::UniformsAndXfb].bindCachedDescriptorPool(
        context, mDefaultUniformAndXfbSetDesc, 1, descriptorSetLayoutCache,
        &mDescriptorPools[DescriptorSetIndex::UniformsAndXfb]));
    ANGLE_TRY((*metaDescriptorPools)[DescriptorSetIndex::Texture].bindCachedDescriptorPool(
        context, mTextureSetDesc, mImmutableSamplersMaxDescriptorCount, descriptorSetLayoutCache,
        &mDescriptorPools[DescriptorSetIndex::Texture]));
    return (*metaDescriptorPools)[DescriptorSetIndex::ShaderResource].bindCachedDescriptorPool(
        context, mShaderResourceSetDesc, 1, descriptorSetLayoutCache,
        &mDescriptorPools[DescriptorSetIndex::ShaderResource]);
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
                mergedVarying.frontShaderStage, mergedVarying.frontShader->id);
            info.varyingIsOutput     = true;
            info.useRelaxedPrecision = true;
        }
        else
        {
            // The output is lower precision than the input, adjust the input
            ASSERT(backPrecision > frontPrecision);
            ShaderInterfaceVariableInfo &info = mVariableInfoMap.getMutable(
                mergedVarying.backShaderStage, mergedVarying.backShader->id);
            info.varyingIsInput      = true;
            info.useRelaxedPrecision = true;
        }
    }
}

angle::Result ProgramExecutableVk::getOrAllocateDescriptorSet(
    vk::Context *context,
    UpdateDescriptorSetsBuilder *updateBuilder,
    vk::CommandBufferHelperCommon *commandBufferHelper,
    const vk::DescriptorSetDescBuilder &descriptorSetDesc,
    const vk::WriteDescriptorDescs &writeDescriptorDescs,
    DescriptorSetIndex setIndex,
    vk::SharedDescriptorSetCacheKey *newSharedCacheKeyOut)
{
    ANGLE_TRY(mDescriptorPools[setIndex].get().getOrAllocateDescriptorSet(
        context, commandBufferHelper, descriptorSetDesc.getDesc(),
        mDescriptorSetLayouts[setIndex].get(), &mDescriptorPoolBindings[setIndex],
        &mDescriptorSets[setIndex], newSharedCacheKeyOut));
    ASSERT(mDescriptorSets[setIndex] != VK_NULL_HANDLE);

    if (*newSharedCacheKeyOut != nullptr)
    {
        // Cache miss. A new cache entry has been created.
        descriptorSetDesc.updateDescriptorSet(context, writeDescriptorDescs, updateBuilder,
                                              mDescriptorSets[setIndex]);
    }
    else
    {
        commandBufferHelper->retainResource(&mDescriptorPoolBindings[setIndex].get());
    }

    return angle::Result::Continue;
}

angle::Result ProgramExecutableVk::updateShaderResourcesDescriptorSet(
    vk::Context *context,
    UpdateDescriptorSetsBuilder *updateBuilder,
    const vk::WriteDescriptorDescs &writeDescriptorDescs,
    vk::CommandBufferHelperCommon *commandBufferHelper,
    const vk::DescriptorSetDescBuilder &shaderResourcesDesc,
    vk::SharedDescriptorSetCacheKey *newSharedCacheKeyOut)
{
    if (!mDescriptorPools[DescriptorSetIndex::ShaderResource].get().valid())
    {
        *newSharedCacheKeyOut = nullptr;
        return angle::Result::Continue;
    }

    ANGLE_TRY(getOrAllocateDescriptorSet(context, updateBuilder, commandBufferHelper,
                                         shaderResourcesDesc, writeDescriptorDescs,
                                         DescriptorSetIndex::ShaderResource, newSharedCacheKeyOut));

    size_t numOffsets = writeDescriptorDescs.getDynamicDescriptorSetCount();
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
    const vk::WriteDescriptorDescs &writeDescriptorDescs,
    vk::CommandBufferHelperCommon *commandBufferHelper,
    vk::BufferHelper *defaultUniformBuffer,
    vk::DescriptorSetDescBuilder *uniformsAndXfbDesc,
    vk::SharedDescriptorSetCacheKey *sharedCacheKeyOut)
{
    mCurrentDefaultUniformBufferSerial =
        defaultUniformBuffer ? defaultUniformBuffer->getBufferSerial() : vk::kInvalidBufferSerial;

    return getOrAllocateDescriptorSet(context, updateBuilder, commandBufferHelper,
                                      *uniformsAndXfbDesc, writeDescriptorDescs,
                                      DescriptorSetIndex::UniformsAndXfb, sharedCacheKeyOut);
}

angle::Result ProgramExecutableVk::updateTexturesDescriptorSet(
    vk::Context *context,
    const gl::ActiveTextureArray<TextureVk *> &textures,
    const gl::SamplerBindingVector &samplers,
    bool emulateSeamfulCubeMapSampling,
    PipelineType pipelineType,
    UpdateDescriptorSetsBuilder *updateBuilder,
    vk::CommandBufferHelperCommon *commandBufferHelper,
    const vk::DescriptorSetDesc &texturesDesc)
{
    vk::SharedDescriptorSetCacheKey newSharedCacheKey;
    ANGLE_TRY(mDescriptorPools[DescriptorSetIndex::Texture].get().getOrAllocateDescriptorSet(
        context, commandBufferHelper, texturesDesc,
        mDescriptorSetLayouts[DescriptorSetIndex::Texture].get(),
        &mDescriptorPoolBindings[DescriptorSetIndex::Texture],
        &mDescriptorSets[DescriptorSetIndex::Texture], &newSharedCacheKey));
    ASSERT(mDescriptorSets[DescriptorSetIndex::Texture] != VK_NULL_HANDLE);

    if (newSharedCacheKey != nullptr)
    {
        vk::DescriptorSetDescBuilder fullDesc(
            mTextureWriteDescriptorDescs.getTotalDescriptorCount());
        // Cache miss. A new cache entry has been created.
        ANGLE_TRY(fullDesc.updateFullActiveTextures(
            context, mVariableInfoMap, mTextureWriteDescriptorDescs, *mExecutable, textures,
            samplers, emulateSeamfulCubeMapSampling, pipelineType, newSharedCacheKey));
        fullDesc.updateDescriptorSet(context, mTextureWriteDescriptorDescs, updateBuilder,
                                     mDescriptorSets[DescriptorSetIndex::Texture]);
    }
    else
    {
        commandBufferHelper->retainResource(
            &mDescriptorPoolBindings[DescriptorSetIndex::Texture].get());
    }

    return angle::Result::Continue;
}

template <typename CommandBufferT>
angle::Result ProgramExecutableVk::bindDescriptorSets(
    vk::Context *context,
    vk::CommandBufferHelperCommon *commandBufferHelper,
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
        if (ToUnderlying(descriptorSetIndex) > ToUnderlying(lastNonNullDescriptorSetIndex))
        {
            continue;
        }

        VkDescriptorSet descSet = mDescriptorSets[descriptorSetIndex];
        if (descSet == VK_NULL_HANDLE)
        {
            continue;
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
    vk::CommandBufferHelperCommon *commandBufferHelper,
    vk::priv::SecondaryCommandBuffer *commandBuffer,
    PipelineType pipelineType);
template angle::Result ProgramExecutableVk::bindDescriptorSets<vk::VulkanSecondaryCommandBuffer>(
    vk::Context *context,
    vk::CommandBufferHelperCommon *commandBufferHelper,
    vk::VulkanSecondaryCommandBuffer *commandBuffer,
    PipelineType pipelineType);

void ProgramExecutableVk::setAllDefaultUniformsDirty()
{
    mDefaultUniformBlocksDirty.reset();
    for (gl::ShaderType shaderType : mExecutable->getLinkedShaderStages())
    {
        if (!mDefaultUniformBlocks[shaderType]->uniformData.empty())
        {
            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }
}

angle::Result ProgramExecutableVk::updateUniforms(
    vk::Context *context,
    UpdateDescriptorSetsBuilder *updateBuilder,
    vk::CommandBufferHelperCommon *commandBufferHelper,
    vk::BufferHelper *emptyBuffer,
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
    requiredSpace = calcUniformUpdateRequiredSpace(context, &offsets);
    ASSERT(requiredSpace > 0);

    // Allocate space from dynamicBuffer. Always try to allocate from the current buffer first.
    // If that failed, we deal with fall out and try again.
    if (!defaultUniformStorage->allocateFromCurrentBuffer(requiredSpace, &defaultUniformBuffer))
    {
        setAllDefaultUniformsDirty();

        requiredSpace = calcUniformUpdateRequiredSpace(context, &offsets);
        ANGLE_TRY(defaultUniformStorage->allocate(context, requiredSpace, &defaultUniformBuffer,
                                                  &anyNewBufferAllocated));
    }

    ASSERT(defaultUniformBuffer);

    uint8_t *bufferData       = defaultUniformBuffer->getMappedMemory();
    VkDeviceSize bufferOffset = defaultUniformBuffer->getOffset();
    for (gl::ShaderType shaderType : mExecutable->getLinkedShaderStages())
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
        const vk::WriteDescriptorDescs &writeDescriptorDescs =
            getDefaultUniformWriteDescriptorDescs(transformFeedbackVk);

        vk::DescriptorSetDescBuilder uniformsAndXfbDesc(
            writeDescriptorDescs.getTotalDescriptorCount());
        uniformsAndXfbDesc.updateUniformsAndXfb(
            context, *mExecutable, writeDescriptorDescs, defaultUniformBuffer, *emptyBuffer,
            isTransformFeedbackActiveUnpaused,
            mExecutable->hasTransformFeedbackOutput() ? transformFeedbackVk : nullptr);

        vk::SharedDescriptorSetCacheKey newSharedCacheKey;
        ANGLE_TRY(updateUniformsAndXfbDescriptorSet(context, updateBuilder, writeDescriptorDescs,
                                                    commandBufferHelper, defaultUniformBuffer,
                                                    &uniformsAndXfbDesc, &newSharedCacheKey));
        if (newSharedCacheKey)
        {
            defaultUniformBuffer->getBufferBlock()->onNewDescriptorSet(newSharedCacheKey);
            if (mExecutable->hasTransformFeedbackOutput() &&
                context->getFeatures().emulateTransformFeedback.enabled)
            {
                transformFeedbackVk->onNewDescriptorSet(*mExecutable, newSharedCacheKey);
            }
        }
    }

    return angle::Result::Continue;
}

size_t ProgramExecutableVk::calcUniformUpdateRequiredSpace(
    vk::Context *context,
    gl::ShaderMap<VkDeviceSize> *uniformOffsets) const
{
    size_t requiredSpace = 0;
    for (gl::ShaderType shaderType : mExecutable->getLinkedShaderStages())
    {
        if (mDefaultUniformBlocksDirty[shaderType])
        {
            (*uniformOffsets)[shaderType] = requiredSpace;
            requiredSpace += getDefaultUniformAlignedSize(context, shaderType);
        }
    }
    return requiredSpace;
}

void ProgramExecutableVk::onProgramBind()
{
    // Because all programs share default uniform buffers, when we switch programs, we have to
    // re-update all uniform data. We could do more tracking to avoid update if the context's
    // current uniform buffer is still the same buffer we last time used and buffer has not been
    // recycled. But statistics gathered on gfxbench shows that app always update uniform data on
    // program bind anyway, so not really worth it to add more tracking logic here.
    setAllDefaultUniformsDirty();
}

angle::Result ProgramExecutableVk::resizeUniformBlockMemory(
    vk::Context *context,
    const gl::ShaderMap<size_t> &requiredBufferSize)
{
    for (gl::ShaderType shaderType : mExecutable->getLinkedShaderStages())
    {
        if (requiredBufferSize[shaderType] > 0)
        {
            if (!mDefaultUniformBlocks[shaderType]->uniformData.resize(
                    requiredBufferSize[shaderType]))
            {
                ANGLE_VK_CHECK(context, false, VK_ERROR_OUT_OF_HOST_MEMORY);
            }

            // Initialize uniform buffer memory to zero by default.
            mDefaultUniformBlocks[shaderType]->uniformData.fill(0);
            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }

    return angle::Result::Continue;
}

template <typename T>
void ProgramExecutableVk::setUniformImpl(GLint location,
                                         GLsizei count,
                                         const T *v,
                                         GLenum entryPointType)
{
    const gl::VariableLocation &locationInfo = mExecutable->getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform   = mExecutable->getUniforms()[locationInfo.index];

    ASSERT(!linkedUniform.isSampler());

    if (linkedUniform.pod.type == entryPointType)
    {
        for (const gl::ShaderType shaderType : mExecutable->getLinkedShaderStages())
        {
            DefaultUniformBlockVk &uniformBlock   = *mDefaultUniformBlocks[shaderType];
            const sh::BlockMemberInfo &layoutInfo = uniformBlock.uniformLayout[location];

            // Assume an offset of -1 means the block is unused.
            if (layoutInfo.offset == -1)
            {
                continue;
            }

            const GLint componentCount = linkedUniform.getElementComponents();
            UpdateDefaultUniformBlock(count, locationInfo.arrayIndex, componentCount, v, layoutInfo,
                                      &uniformBlock.uniformData);
            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }
    else
    {
        for (const gl::ShaderType shaderType : mExecutable->getLinkedShaderStages())
        {
            DefaultUniformBlockVk &uniformBlock   = *mDefaultUniformBlocks[shaderType];
            const sh::BlockMemberInfo &layoutInfo = uniformBlock.uniformLayout[location];

            // Assume an offset of -1 means the block is unused.
            if (layoutInfo.offset == -1)
            {
                continue;
            }

            const GLint componentCount = linkedUniform.getElementComponents();

            ASSERT(linkedUniform.pod.type == gl::VariableBoolVectorType(entryPointType));

            GLint initialArrayOffset =
                locationInfo.arrayIndex * layoutInfo.arrayStride + layoutInfo.offset;
            for (GLint i = 0; i < count; i++)
            {
                GLint elementOffset = i * layoutInfo.arrayStride + initialArrayOffset;
                GLint *dst =
                    reinterpret_cast<GLint *>(uniformBlock.uniformData.data() + elementOffset);
                const T *source = v + i * componentCount;

                for (int c = 0; c < componentCount; c++)
                {
                    dst[c] = (source[c] == static_cast<T>(0)) ? GL_FALSE : GL_TRUE;
                }
            }

            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }
}

template <typename T>
void ProgramExecutableVk::getUniformImpl(GLint location, T *v, GLenum entryPointType) const
{
    const gl::VariableLocation &locationInfo = mExecutable->getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform   = mExecutable->getUniforms()[locationInfo.index];

    ASSERT(!linkedUniform.isSampler() && !linkedUniform.isImage());

    const gl::ShaderType shaderType = linkedUniform.getFirstActiveShaderType();
    ASSERT(shaderType != gl::ShaderType::InvalidEnum);

    const DefaultUniformBlockVk &uniformBlock = *mDefaultUniformBlocks[shaderType];
    const sh::BlockMemberInfo &layoutInfo     = uniformBlock.uniformLayout[location];

    ASSERT(gl::GetUniformTypeInfo(linkedUniform.pod.type).componentType == entryPointType ||
           gl::GetUniformTypeInfo(linkedUniform.pod.type).componentType ==
               gl::VariableBoolVectorType(entryPointType));

    if (gl::IsMatrixType(linkedUniform.getType()))
    {
        const uint8_t *ptrToElement = uniformBlock.uniformData.data() + layoutInfo.offset +
                                      (locationInfo.arrayIndex * layoutInfo.arrayStride);
        GetMatrixUniform(linkedUniform.getType(), v, reinterpret_cast<const T *>(ptrToElement),
                         false);
    }
    else
    {
        ReadFromDefaultUniformBlock(linkedUniform.getElementComponents(), locationInfo.arrayIndex,
                                    v, layoutInfo, &uniformBlock.uniformData);
    }
}

void ProgramExecutableVk::setUniform1fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformImpl(location, count, v, GL_FLOAT);
}

void ProgramExecutableVk::setUniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformImpl(location, count, v, GL_FLOAT_VEC2);
}

void ProgramExecutableVk::setUniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformImpl(location, count, v, GL_FLOAT_VEC3);
}

void ProgramExecutableVk::setUniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformImpl(location, count, v, GL_FLOAT_VEC4);
}

void ProgramExecutableVk::setUniform1iv(GLint location, GLsizei count, const GLint *v)
{
    const gl::VariableLocation &locationInfo = mExecutable->getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform   = mExecutable->getUniforms()[locationInfo.index];
    if (linkedUniform.isSampler())
    {
        // We could potentially cache some indexing here. For now this is a no-op since the mapping
        // is handled entirely in ContextVk.
        return;
    }

    setUniformImpl(location, count, v, GL_INT);
}

void ProgramExecutableVk::setUniform2iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformImpl(location, count, v, GL_INT_VEC2);
}

void ProgramExecutableVk::setUniform3iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformImpl(location, count, v, GL_INT_VEC3);
}

void ProgramExecutableVk::setUniform4iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformImpl(location, count, v, GL_INT_VEC4);
}

void ProgramExecutableVk::setUniform1uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformImpl(location, count, v, GL_UNSIGNED_INT);
}

void ProgramExecutableVk::setUniform2uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformImpl(location, count, v, GL_UNSIGNED_INT_VEC2);
}

void ProgramExecutableVk::setUniform3uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformImpl(location, count, v, GL_UNSIGNED_INT_VEC3);
}

void ProgramExecutableVk::setUniform4uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformImpl(location, count, v, GL_UNSIGNED_INT_VEC4);
}

template <int cols, int rows>
void ProgramExecutableVk::setUniformMatrixfv(GLint location,
                                             GLsizei count,
                                             GLboolean transpose,
                                             const GLfloat *value)
{
    const gl::VariableLocation &locationInfo = mExecutable->getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform   = mExecutable->getUniforms()[locationInfo.index];

    for (const gl::ShaderType shaderType : mExecutable->getLinkedShaderStages())
    {
        DefaultUniformBlockVk &uniformBlock   = *mDefaultUniformBlocks[shaderType];
        const sh::BlockMemberInfo &layoutInfo = uniformBlock.uniformLayout[location];

        // Assume an offset of -1 means the block is unused.
        if (layoutInfo.offset == -1)
        {
            continue;
        }

        SetFloatUniformMatrixGLSL<cols, rows>::Run(
            locationInfo.arrayIndex, linkedUniform.getBasicTypeElementCount(), count, transpose,
            value, uniformBlock.uniformData.data() + layoutInfo.offset);

        mDefaultUniformBlocksDirty.set(shaderType);
    }
}

void ProgramExecutableVk::setUniformMatrix2fv(GLint location,
                                              GLsizei count,
                                              GLboolean transpose,
                                              const GLfloat *value)
{
    setUniformMatrixfv<2, 2>(location, count, transpose, value);
}

void ProgramExecutableVk::setUniformMatrix3fv(GLint location,
                                              GLsizei count,
                                              GLboolean transpose,
                                              const GLfloat *value)
{
    setUniformMatrixfv<3, 3>(location, count, transpose, value);
}

void ProgramExecutableVk::setUniformMatrix4fv(GLint location,
                                              GLsizei count,
                                              GLboolean transpose,
                                              const GLfloat *value)
{
    setUniformMatrixfv<4, 4>(location, count, transpose, value);
}

void ProgramExecutableVk::setUniformMatrix2x3fv(GLint location,
                                                GLsizei count,
                                                GLboolean transpose,
                                                const GLfloat *value)
{
    setUniformMatrixfv<2, 3>(location, count, transpose, value);
}

void ProgramExecutableVk::setUniformMatrix3x2fv(GLint location,
                                                GLsizei count,
                                                GLboolean transpose,
                                                const GLfloat *value)
{
    setUniformMatrixfv<3, 2>(location, count, transpose, value);
}

void ProgramExecutableVk::setUniformMatrix2x4fv(GLint location,
                                                GLsizei count,
                                                GLboolean transpose,
                                                const GLfloat *value)
{
    setUniformMatrixfv<2, 4>(location, count, transpose, value);
}

void ProgramExecutableVk::setUniformMatrix4x2fv(GLint location,
                                                GLsizei count,
                                                GLboolean transpose,
                                                const GLfloat *value)
{
    setUniformMatrixfv<4, 2>(location, count, transpose, value);
}

void ProgramExecutableVk::setUniformMatrix3x4fv(GLint location,
                                                GLsizei count,
                                                GLboolean transpose,
                                                const GLfloat *value)
{
    setUniformMatrixfv<3, 4>(location, count, transpose, value);
}

void ProgramExecutableVk::setUniformMatrix4x3fv(GLint location,
                                                GLsizei count,
                                                GLboolean transpose,
                                                const GLfloat *value)
{
    setUniformMatrixfv<4, 3>(location, count, transpose, value);
}

void ProgramExecutableVk::getUniformfv(const gl::Context *context,
                                       GLint location,
                                       GLfloat *params) const
{
    getUniformImpl(location, params, GL_FLOAT);
}

void ProgramExecutableVk::getUniformiv(const gl::Context *context,
                                       GLint location,
                                       GLint *params) const
{
    getUniformImpl(location, params, GL_INT);
}

void ProgramExecutableVk::getUniformuiv(const gl::Context *context,
                                        GLint location,
                                        GLuint *params) const
{
    getUniformImpl(location, params, GL_UNSIGNED_INT);
}
}  // namespace rx
