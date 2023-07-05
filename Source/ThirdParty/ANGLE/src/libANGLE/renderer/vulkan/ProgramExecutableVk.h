//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramExecutableVk.h: Collects the information and interfaces common to both ProgramVks and
// ProgramPipelineVks in order to execute/draw with either.

#ifndef LIBANGLE_RENDERER_VULKAN_PROGRAMEXECUTABLEVK_H_
#define LIBANGLE_RENDERER_VULKAN_PROGRAMEXECUTABLEVK_H_

#include "common/bitset_utils.h"
#include "common/mathutil.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/InfoLog.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/ShaderInterfaceVariableInfoMap.h"
#include "libANGLE/renderer/vulkan/spv_utils.h"
#include "libANGLE/renderer/vulkan/vk_cache_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace rx
{

class ShaderInfo final : angle::NonCopyable
{
  public:
    ShaderInfo();
    ~ShaderInfo();

    angle::Result initShaders(ContextVk *contextVk,
                              const gl::ShaderBitSet &linkedShaderStages,
                              const gl::ShaderMap<const angle::spirv::Blob *> &spirvBlobs,
                              const ShaderInterfaceVariableInfoMap &variableInfoMap);
    void initShaderFromProgram(gl::ShaderType shaderType, const ShaderInfo &programShaderInfo);
    void clear();

    ANGLE_INLINE bool valid() const { return mIsInitialized; }

    const gl::ShaderMap<angle::spirv::Blob> &getSpirvBlobs() const { return mSpirvBlobs; }

    // Save and load implementation for GLES Program Binary support.
    void load(gl::BinaryInputStream *stream);
    void save(gl::BinaryOutputStream *stream);

  private:
    gl::ShaderMap<angle::spirv::Blob> mSpirvBlobs;
    bool mIsInitialized = false;
};

struct ProgramTransformOptions final
{
    uint8_t surfaceRotation : 1;
    uint8_t removeTransformFeedbackEmulation : 1;
    uint8_t multiSampleFramebufferFetch : 1;
    uint8_t enableSampleShading : 1;
    uint8_t reserved : 4;  // must initialize to zero
    static constexpr uint32_t kPermutationCount = 0x1 << 4;
};
static_assert(sizeof(ProgramTransformOptions) == 1, "Size check failed");
static_assert(static_cast<int>(SurfaceRotation::EnumCount) <= 8, "Size check failed");

class ProgramInfo final : angle::NonCopyable
{
  public:
    ProgramInfo();
    ~ProgramInfo();

    angle::Result initProgram(ContextVk *contextVk,
                              gl::ShaderType shaderType,
                              bool isLastPreFragmentStage,
                              bool isTransformFeedbackProgram,
                              const ShaderInfo &shaderInfo,
                              ProgramTransformOptions optionBits,
                              const ShaderInterfaceVariableInfoMap &variableInfoMap);
    void release(ContextVk *contextVk);

    ANGLE_INLINE bool valid(gl::ShaderType shaderType) const
    {
        return mProgramHelper.valid(shaderType);
    }

    vk::ShaderProgramHelper *getShaderProgram() { return &mProgramHelper; }

  private:
    vk::ShaderProgramHelper mProgramHelper;
    gl::ShaderMap<vk::RefCounted<vk::ShaderModule>> mShaders;
};

// State for the default uniform blocks.
struct DefaultUniformBlock final : private angle::NonCopyable
{
    DefaultUniformBlock();
    ~DefaultUniformBlock();

    // Shadow copies of the shader uniform data.
    angle::MemoryBuffer uniformData;

    // Since the default blocks are laid out in std140, this tells us where to write on a call
    // to a setUniform method. They are arranged in uniform location order.
    std::vector<sh::BlockMemberInfo> uniformLayout;
};

// Performance and resource counters.
using DescriptorSetCountList   = angle::PackedEnumMap<DescriptorSetIndex, uint32_t>;
using ImmutableSamplerIndexMap = angle::HashMap<vk::YcbcrConversionDesc, uint32_t>;

using DefaultUniformBlockMap = gl::ShaderMap<std::shared_ptr<DefaultUniformBlock>>;

class ProgramExecutableVk
{
  public:
    ProgramExecutableVk();
    virtual ~ProgramExecutableVk();

    void reset(ContextVk *contextVk);

    void save(ContextVk *contextVk, bool isSeparable, gl::BinaryOutputStream *stream);
    std::unique_ptr<rx::LinkEvent> load(ContextVk *contextVk,
                                        const gl::ProgramExecutable &glExecutable,
                                        bool isSeparable,
                                        gl::BinaryInputStream *stream);

    void clearVariableInfoMap();

    vk::BufferSerial getCurrentDefaultUniformBufferSerial() const
    {
        return mCurrentDefaultUniformBufferSerial;
    }

    // Get the graphics pipeline if already created.
    angle::Result getGraphicsPipeline(ContextVk *contextVk,
                                      vk::GraphicsPipelineSubset pipelineSubset,
                                      const vk::GraphicsPipelineDesc &desc,
                                      const gl::ProgramExecutable &glExecutable,
                                      const vk::GraphicsPipelineDesc **descPtrOut,
                                      vk::PipelineHelper **pipelineOut);

    angle::Result createGraphicsPipeline(ContextVk *contextVk,
                                         vk::GraphicsPipelineSubset pipelineSubset,
                                         vk::PipelineCacheAccess *pipelineCache,
                                         PipelineSource source,
                                         const vk::GraphicsPipelineDesc &desc,
                                         const gl::ProgramExecutable &glExecutable,
                                         const vk::GraphicsPipelineDesc **descPtrOut,
                                         vk::PipelineHelper **pipelineOut);

    angle::Result linkGraphicsPipelineLibraries(ContextVk *contextVk,
                                                vk::PipelineCacheAccess *pipelineCache,
                                                const vk::GraphicsPipelineDesc &desc,
                                                const gl::ProgramExecutable &glExecutable,
                                                vk::PipelineHelper *vertexInputPipeline,
                                                vk::PipelineHelper *shadersPipeline,
                                                vk::PipelineHelper *fragmentOutputPipeline,
                                                const vk::GraphicsPipelineDesc **descPtrOut,
                                                vk::PipelineHelper **pipelineOut);

    angle::Result getOrCreateComputePipeline(ContextVk *contextVk,
                                             vk::PipelineCacheAccess *pipelineCache,
                                             PipelineSource source,
                                             const gl::ProgramExecutable &glExecutable,
                                             vk::PipelineHelper **pipelineOut);

    const vk::PipelineLayout &getPipelineLayout() const { return mPipelineLayout.get(); }
    angle::Result createPipelineLayout(ContextVk *contextVk,
                                       const gl::ProgramExecutable &glExecutable,
                                       gl::ActiveTextureArray<TextureVk *> *activeTextures);

    angle::Result updateTexturesDescriptorSet(vk::Context *context,
                                              const gl::ProgramExecutable &executable,
                                              const gl::ActiveTextureArray<TextureVk *> &textures,
                                              const gl::SamplerBindingVector &samplers,
                                              bool emulateSeamfulCubeMapSampling,
                                              PipelineType pipelineType,
                                              UpdateDescriptorSetsBuilder *updateBuilder,
                                              vk::CommandBufferHelperCommon *commandBufferHelper,
                                              const vk::DescriptorSetDesc &texturesDesc);

    angle::Result updateShaderResourcesDescriptorSet(
        vk::Context *context,
        UpdateDescriptorSetsBuilder *updateBuilder,
        const vk::WriteDescriptorDescs &writeDescriptorDescs,
        vk::CommandBufferHelperCommon *commandBufferHelper,
        const vk::DescriptorSetDescBuilder &shaderResourcesDesc,
        vk::SharedDescriptorSetCacheKey *newSharedCacheKeyOut);

    angle::Result updateUniformsAndXfbDescriptorSet(
        vk::Context *context,
        UpdateDescriptorSetsBuilder *updateBuilder,
        const vk::WriteDescriptorDescs &writeDescriptorDescs,
        vk::CommandBufferHelperCommon *commandBufferHelper,
        vk::BufferHelper *defaultUniformBuffer,
        vk::DescriptorSetDescBuilder *uniformsAndXfbDesc,
        vk::SharedDescriptorSetCacheKey *sharedCacheKeyOut);

    template <typename CommandBufferT>
    angle::Result bindDescriptorSets(vk::Context *context,
                                     vk::CommandBufferHelperCommon *commandBufferHelper,
                                     CommandBufferT *commandBuffer,
                                     PipelineType pipelineType);

    bool usesDynamicUniformBufferDescriptors() const
    {
        return mUniformBufferDescriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    }
    VkDescriptorType getUniformBufferDescriptorType() const { return mUniformBufferDescriptorType; }
    bool usesDynamicShaderStorageBufferDescriptors() const { return false; }
    VkDescriptorType getStorageBufferDescriptorType() const
    {
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    VkDescriptorType getAtomicCounterBufferDescriptorType() const
    {
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    bool usesDynamicAtomicCounterBufferDescriptors() const { return false; }

    bool areImmutableSamplersCompatible(
        const ImmutableSamplerIndexMap &immutableSamplerIndexMap) const
    {
        return (mImmutableSamplerIndexMap == immutableSamplerIndexMap);
    }

    size_t getDefaultUniformAlignedSize(vk::Context *context, gl::ShaderType shaderType) const
    {
        RendererVk *renderer = context->getRenderer();
        size_t alignment     = static_cast<size_t>(
            renderer->getPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment);
        return roundUp(mDefaultUniformBlocks[shaderType]->uniformData.size(), alignment);
    }

    std::shared_ptr<DefaultUniformBlock> &getSharedDefaultUniformBlock(gl::ShaderType shaderType)
    {
        return mDefaultUniformBlocks[shaderType];
    }

    bool hasDirtyUniforms() const { return mDefaultUniformBlocksDirty.any(); }

    void setAllDefaultUniformsDirty(const gl::ProgramExecutable &executable);
    angle::Result updateUniforms(vk::Context *context,
                                 UpdateDescriptorSetsBuilder *updateBuilder,
                                 vk::CommandBufferHelperCommon *commandBufferHelper,
                                 vk::BufferHelper *emptyBuffer,
                                 const gl::ProgramExecutable &glExecutable,
                                 vk::DynamicBuffer *defaultUniformStorage,
                                 bool isTransformFeedbackActiveUnpaused,
                                 TransformFeedbackVk *transformFeedbackVk);
    void onProgramBind(const gl::ProgramExecutable &glExecutable);

    const ShaderInterfaceVariableInfoMap &getVariableInfoMap() const { return mVariableInfoMap; }

    angle::Result warmUpPipelineCache(ContextVk *contextVk,
                                      const gl::ProgramExecutable &glExecutable);

    const vk::WriteDescriptorDescs &getShaderResourceWriteDescriptorDescs() const
    {
        return mShaderResourceWriteDescriptorDescs;
    }
    const vk::WriteDescriptorDescs &getDefaultUniformWriteDescriptorDescs(
        TransformFeedbackVk *transformFeedbackVk) const
    {
        return transformFeedbackVk == nullptr ? mDefaultUniformWriteDescriptorDescs
                                              : mDefaultUniformAndXfbWriteDescriptorDescs;
    }

    const vk::WriteDescriptorDescs &getTextureWriteDescriptorDescs() const
    {
        return mTextureWriteDescriptorDescs;
    }
    const gl::Program::DirtyBits &getDirtyBits() const { return mDirtyBits; }
    void resetUniformBufferDirtyBits() { mDirtyBits.reset(); }

  private:
    friend class ProgramVk;
    friend class ProgramPipelineVk;

    void addInterfaceBlockDescriptorSetDesc(const std::vector<gl::InterfaceBlock> &blocks,
                                            gl::ShaderBitSet shaderTypes,
                                            VkDescriptorType descType,
                                            vk::DescriptorSetLayoutDesc *descOut);
    void addAtomicCounterBufferDescriptorSetDesc(
        const std::vector<gl::AtomicCounterBuffer> &atomicCounterBuffers,
        vk::DescriptorSetLayoutDesc *descOut);
    void addImageDescriptorSetDesc(const gl::ProgramExecutable &executable,
                                   vk::DescriptorSetLayoutDesc *descOut);
    void addInputAttachmentDescriptorSetDesc(const gl::ProgramExecutable &executable,
                                             vk::DescriptorSetLayoutDesc *descOut);
    angle::Result addTextureDescriptorSetDesc(
        ContextVk *contextVk,
        const gl::ProgramExecutable &executable,
        const gl::ActiveTextureArray<TextureVk *> *activeTextures,
        vk::DescriptorSetLayoutDesc *descOut);

    void resolvePrecisionMismatch(const gl::ProgramMergedVaryings &mergedVaryings);

    size_t calcUniformUpdateRequiredSpace(vk::Context *context,
                                          const gl::ProgramExecutable &glExecutable,
                                          gl::ShaderMap<VkDeviceSize> *uniformOffsets) const;

    ANGLE_INLINE angle::Result initProgram(ContextVk *contextVk,
                                           gl::ShaderType shaderType,
                                           bool isLastPreFragmentStage,
                                           bool isTransformFeedbackProgram,
                                           ProgramTransformOptions optionBits,
                                           ProgramInfo *programInfo,
                                           const ShaderInterfaceVariableInfoMap &variableInfoMap)
    {
        ASSERT(mOriginalShaderInfo.valid());

        // Create the program pipeline.  This is done lazily and once per combination of
        // specialization constants.
        if (!programInfo->valid(shaderType))
        {
            ANGLE_TRY(programInfo->initProgram(contextVk, shaderType, isLastPreFragmentStage,
                                               isTransformFeedbackProgram, mOriginalShaderInfo,
                                               optionBits, variableInfoMap));
        }
        ASSERT(programInfo->valid(shaderType));

        return angle::Result::Continue;
    }

    ANGLE_INLINE angle::Result initGraphicsShaderProgram(
        ContextVk *contextVk,
        gl::ShaderType shaderType,
        bool isLastPreFragmentStage,
        bool isTransformFeedbackProgram,
        ProgramTransformOptions optionBits,
        ProgramInfo *programInfo,
        const ShaderInterfaceVariableInfoMap &variableInfoMap)
    {
        return initProgram(contextVk, shaderType, isLastPreFragmentStage,
                           isTransformFeedbackProgram, optionBits, programInfo, variableInfoMap);
    }

    ANGLE_INLINE angle::Result initComputeProgram(
        ContextVk *contextVk,
        ProgramInfo *programInfo,
        const ShaderInterfaceVariableInfoMap &variableInfoMap)
    {
        ProgramTransformOptions optionBits = {};
        return initProgram(contextVk, gl::ShaderType::Compute, false, false, optionBits,
                           programInfo, variableInfoMap);
    }

    ProgramTransformOptions getTransformOptions(ContextVk *contextVk,
                                                const vk::GraphicsPipelineDesc &desc,
                                                const gl::ProgramExecutable &glExecutable);
    angle::Result initGraphicsShaderPrograms(ContextVk *contextVk,
                                             ProgramTransformOptions transformOptions,
                                             const gl::ProgramExecutable &glExecutable,
                                             vk::ShaderProgramHelper **shaderProgramOut);
    angle::Result createGraphicsPipelineImpl(ContextVk *contextVk,
                                             ProgramTransformOptions transformOptions,
                                             vk::GraphicsPipelineSubset pipelineSubset,
                                             vk::PipelineCacheAccess *pipelineCache,
                                             PipelineSource source,
                                             const vk::GraphicsPipelineDesc &desc,
                                             const gl::ProgramExecutable &glExecutable,
                                             const vk::GraphicsPipelineDesc **descPtrOut,
                                             vk::PipelineHelper **pipelineOut);

    angle::Result resizeUniformBlockMemory(ContextVk *contextVk,
                                           const gl::ProgramExecutable &glExecutable,
                                           const gl::ShaderMap<size_t> &requiredBufferSize);

    angle::Result getOrAllocateDescriptorSet(vk::Context *context,
                                             UpdateDescriptorSetsBuilder *updateBuilder,
                                             vk::CommandBufferHelperCommon *commandBufferHelper,
                                             const vk::DescriptorSetDescBuilder &descriptorSetDesc,
                                             const vk::WriteDescriptorDescs &writeDescriptorDescs,
                                             DescriptorSetIndex setIndex,
                                             vk::SharedDescriptorSetCacheKey *newSharedCacheKeyOut);

    // When loading from cache / binary, initialize the pipeline cache with given data.  Otherwise
    // the cache is lazily created as needed.
    angle::Result initializePipelineCache(ContextVk *contextVk,
                                          bool compressed,
                                          const std::vector<uint8_t> &pipelineData);
    angle::Result ensurePipelineCacheInitialized(ContextVk *contextVk);

    void resetLayout(ContextVk *contextVk);
    void initializeWriteDescriptorDesc(ContextVk *contextVk,
                                       const gl::ProgramExecutable &glExecutable);

    // Descriptor sets and pools for shader resources for this program.
    vk::DescriptorSetArray<VkDescriptorSet> mDescriptorSets;
    vk::DescriptorSetArray<vk::DescriptorPoolPointer> mDescriptorPools;
    vk::DescriptorSetArray<vk::RefCountedDescriptorPoolBinding> mDescriptorPoolBindings;
    uint32_t mNumDefaultUniformDescriptors;
    vk::BufferSerial mCurrentDefaultUniformBufferSerial;

    // We keep a reference to the pipeline and descriptor set layouts. This ensures they don't get
    // deleted while this program is in use.
    uint32_t mImmutableSamplersMaxDescriptorCount;
    ImmutableSamplerIndexMap mImmutableSamplerIndexMap;
    vk::BindingPointer<vk::PipelineLayout> mPipelineLayout;
    vk::DescriptorSetLayoutPointerArray mDescriptorSetLayouts;

    // A set of dynamic offsets used with vkCmdBindDescriptorSets for the default uniform buffers.
    VkDescriptorType mUniformBufferDescriptorType;
    gl::ShaderVector<uint32_t> mDynamicUniformDescriptorOffsets;
    std::vector<uint32_t> mDynamicShaderResourceDescriptorOffsets;

    ShaderInterfaceVariableInfoMap mVariableInfoMap;

    // We store all permutations of surface rotation and transformed SPIR-V programs here. We may
    // need some LRU algorithm to free least used programs to reduce the number of programs.
    ProgramInfo mGraphicsProgramInfos[ProgramTransformOptions::kPermutationCount];
    ProgramInfo mComputeProgramInfo;

    // Pipeline caches.  The pipelines are tightly coupled with the shaders they are created for, so
    // they live in the program executable.  With VK_EXT_graphics_pipeline_library, the pipeline is
    // divided in subsets; the "shaders" subset is created based on the shaders, so its cache lives
    // in the program executable.  The "vertex input" and "fragment output" pipelines are
    // independent, and live in the context.
    CompleteGraphicsPipelineCache
        mCompleteGraphicsPipelines[ProgramTransformOptions::kPermutationCount];
    ShadersGraphicsPipelineCache
        mShadersGraphicsPipelines[ProgramTransformOptions::kPermutationCount];
    vk::ComputePipelineCache mComputePipelines;

    DefaultUniformBlockMap mDefaultUniformBlocks;
    gl::ShaderBitSet mDefaultUniformBlocksDirty;

    ShaderInfo mOriginalShaderInfo;

    // The pipeline cache specific to this program executable.  Currently:
    //
    // - This is used during warm up (at link time)
    // - The contents are merged to RendererVk's pipeline cache immediately after warm up
    // - The contents are returned as part of program binary
    // - Draw-time pipeline creation uses RendererVk's cache
    //
    // Without VK_EXT_graphics_pipeline_library, this cache is not used for draw-time pipeline
    // creations to allow reuse of other blobs that are independent of the actual shaders; vertex
    // input fetch, fragment output and blend.
    //
    // With VK_EXT_graphics_pipeline_library, this cache is used for the "shaders" subset of the
    // pipeline.
    vk::PipelineCache mPipelineCache;

    // The "layout" information for descriptorSets
    vk::WriteDescriptorDescs mShaderResourceWriteDescriptorDescs;
    vk::WriteDescriptorDescs mTextureWriteDescriptorDescs;
    vk::WriteDescriptorDescs mDefaultUniformWriteDescriptorDescs;
    vk::WriteDescriptorDescs mDefaultUniformAndXfbWriteDescriptorDescs;

    gl::Program::DirtyBits mDirtyBits;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_PROGRAMEXECUTABLEVK_H_
