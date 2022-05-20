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
#include "libANGLE/renderer/ShaderInterfaceVariableInfoMap.h"
#include "libANGLE/renderer/glslang_wrapper_utils.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/vk_cache_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace rx
{

class ShaderInfo final : angle::NonCopyable
{
  public:
    ShaderInfo();
    ~ShaderInfo();

    angle::Result initShaders(const gl::ShaderBitSet &linkedShaderStages,
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
    uint8_t enableLineRasterEmulation : 1;
    uint8_t removeEarlyFragmentTestsOptimization : 1;
    uint8_t surfaceRotation : 3;
    uint8_t enableDepthCorrection : 1;
    uint8_t removeTransformFeedbackEmulation : 1;
    uint8_t reserved : 1;  // must initialize to zero
    static constexpr uint32_t kPermutationCount = 0x1 << 7;
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
    gl::ShaderMap<vk::RefCounted<vk::ShaderAndSerial>> mShaders;
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

struct ProgramExecutablePerfCounters
{
    DescriptorSetCountList descriptorSetAllocations;
    DescriptorSetCountList descriptorSetCacheHits;
    DescriptorSetCountList descriptorSetCacheMisses;
    DescriptorSetCountList descriptorSetCacheSizes;
    DescriptorSetCountList descriptorSetCacheKeySizesBytes;
};

using DefaultUniformBlockMap = gl::ShaderMap<std::shared_ptr<DefaultUniformBlock>>;

class ProgramExecutableVk
{
  public:
    ProgramExecutableVk();
    virtual ~ProgramExecutableVk();

    void reset(ContextVk *contextVk);

    void save(gl::BinaryOutputStream *stream);
    std::unique_ptr<rx::LinkEvent> load(ContextVk *contextVk,
                                        const gl::ProgramExecutable &glExecutable,
                                        gl::BinaryInputStream *stream);

    void clearVariableInfoMap();

    ProgramInfo &getGraphicsDefaultProgramInfo() { return mGraphicsProgramInfos[0]; }
    ProgramInfo &getGraphicsProgramInfo(ProgramTransformOptions option)
    {
        uint8_t index = gl::bitCast<uint8_t, ProgramTransformOptions>(option);
        return mGraphicsProgramInfos[index];
    }
    ProgramInfo &getComputeProgramInfo() { return mComputeProgramInfo; }
    vk::BufferSerial getCurrentDefaultUniformBufferSerial() const
    {
        return mCurrentDefaultUniformBufferSerial;
    }

    angle::Result getGraphicsPipeline(ContextVk *contextVk,
                                      gl::PrimitiveMode mode,
                                      const vk::GraphicsPipelineDesc &desc,
                                      const gl::ProgramExecutable &glExecutable,
                                      const vk::GraphicsPipelineDesc **descPtrOut,
                                      vk::PipelineHelper **pipelineOut);

    angle::Result getComputePipeline(ContextVk *contextVk, vk::PipelineHelper **pipelineOut);

    const vk::PipelineLayout &getPipelineLayout() const { return mPipelineLayout.get(); }
    angle::Result createPipelineLayout(ContextVk *contextVk,
                                       const gl::ProgramExecutable &glExecutable,
                                       gl::ActiveTextureArray<vk::TextureUnit> *activeTextures);

    angle::Result updateTexturesDescriptorSet(
        vk::Context *context,
        UpdateDescriptorSetsBuilder *updateBuilder,
        vk::ResourceUseList *resourceUseList,
        const gl::ProgramExecutable &executable,
        const gl::ActiveTextureArray<vk::TextureUnit> &activeTextures,
        const vk::DescriptorSetDesc &texturesDesc,
        bool emulateSeamfulCubeMapSampling);
    angle::Result updateShaderResourcesDescriptorSet(
        ContextVk *contextVk,
        const gl::ProgramExecutable *executable,
        UpdateDescriptorSetsBuilder *updateBuilder,
        vk::BufferHelper *emptyBuffer,
        vk::ResourceUseList *resourceUseList,
        FramebufferVk *framebufferVk,
        const vk::DescriptorSetDesc &shaderBuffersDesc);
    angle::Result updateUniformsAndXfbDescriptorSet(vk::Context *context,
                                                    UpdateDescriptorSetsBuilder *updateBuilder,
                                                    vk::ResourceUseList *resourceUseList,
                                                    vk::BufferHelper *emptyBuffer,
                                                    const gl::ProgramExecutable &executable,
                                                    vk::BufferHelper *defaultUniformBuffer,
                                                    const vk::DescriptorSetDesc &uniformsAndXfbDesc,
                                                    bool isTransformFeedbackActiveUnpaused,
                                                    TransformFeedbackVk *transformFeedbackVk);
    angle::Result updateInputAttachmentDescriptorSet(vk::Context *context,
                                                     vk::ResourceUseList *resourceUseList,
                                                     UpdateDescriptorSetsBuilder *updateBuilder,
                                                     const gl::ProgramExecutable &executable,
                                                     gl::ShaderType shaderType,
                                                     FramebufferVk *framebufferVk);

    template <typename CommandBufferT>
    angle::Result updateDescriptorSets(vk::Context *context,
                                       vk::ResourceUseList *resourceUseList,
                                       CommandBufferT *commandBuffer,
                                       PipelineType pipelineType);

    void updateEarlyFragmentTestsOptimization(ContextVk *contextVk,
                                              const gl::ProgramExecutable &glExecutable);

    bool usesDynamicUniformBufferDescriptors() const
    {
        return mUniformBufferDescriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    }
    bool usesDynamicShaderStorageBufferDescriptors() const { return false; }
    bool usesDynamicAtomicCounterBufferDescriptors() const { return false; }

    bool areImmutableSamplersCompatible(
        const ImmutableSamplerIndexMap &immutableSamplerIndexMap) const
    {
        return (mImmutableSamplerIndexMap == immutableSamplerIndexMap);
    }

    void accumulateCacheStats(VulkanCacheType cacheType, const CacheStats &cacheStats);
    ProgramExecutablePerfCounters getAndResetObjectPerfCounters();

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
                                 vk::ResourceUseList *resourceUseList,
                                 vk::BufferHelper *emptyBuffer,
                                 const gl::ProgramExecutable &glExecutable,
                                 vk::DynamicBuffer *defaultUniformStorage,
                                 bool isTransformFeedbackActiveUnpaused,
                                 TransformFeedbackVk *transformFeedbackVk);
    void onProgramBind(const gl::ProgramExecutable &glExecutable);

  private:
    friend class ProgramVk;
    friend class ProgramPipelineVk;

    angle::Result allocUniformAndXfbDescriptorSet(vk::Context *context,
                                                  vk::ResourceUseList *resourceUseList,
                                                  vk::BufferHelper *defaultUniformBuffer,
                                                  const vk::DescriptorSetDesc &xfbBufferDesc,
                                                  bool *newDescriptorSetAllocated);

    angle::Result allocateDescriptorSet(vk::Context *context,
                                        vk::ResourceUseList *resourceUseList,
                                        DescriptorSetIndex descriptorSetIndex);
    angle::Result allocateDescriptorSetAndGetInfo(vk::Context *context,
                                                  vk::ResourceUseList *resourceUseList,
                                                  DescriptorSetIndex descriptorSetIndex,
                                                  bool *newPoolAllocatedOut);
    void addInterfaceBlockDescriptorSetDesc(const std::vector<gl::InterfaceBlock> &blocks,
                                            gl::ShaderType shaderType,
                                            VkDescriptorType descType,
                                            vk::DescriptorSetLayoutDesc *descOut);
    void addAtomicCounterBufferDescriptorSetDesc(
        const std::vector<gl::AtomicCounterBuffer> &atomicCounterBuffers,
        gl::ShaderType shaderType,
        vk::DescriptorSetLayoutDesc *descOut);
    void addImageDescriptorSetDesc(const gl::ProgramExecutable &executable,
                                   vk::DescriptorSetLayoutDesc *descOut);
    void addInputAttachmentDescriptorSetDesc(const gl::ProgramExecutable &executable,
                                             gl::ShaderType shaderType,
                                             vk::DescriptorSetLayoutDesc *descOut);
    angle::Result addTextureDescriptorSetDesc(
        ContextVk *contextVk,
        const gl::ProgramExecutable &executable,
        const gl::ActiveTextureArray<vk::TextureUnit> *activeTextures,
        vk::DescriptorSetLayoutDesc *descOut);

    void resolvePrecisionMismatch(const gl::ProgramMergedVaryings &mergedVaryings);
    void updateDefaultUniformsDescriptorSet(vk::Context *context,
                                            UpdateDescriptorSetsBuilder *updateBuilder,
                                            vk::BufferHelper *emptyBuffer,
                                            vk::ResourceUseList *resourceUseList,
                                            gl::ShaderType shaderType,
                                            const DefaultUniformBlock &defaultUniformBlock,
                                            vk::BufferHelper *defaultUniformBuffer);
    void updateTransformFeedbackDescriptorSetImpl(vk::Context *context,
                                                  UpdateDescriptorSetsBuilder *updateBuilder,
                                                  vk::BufferHelper *emptyBuffer,
                                                  const gl::ProgramExecutable &executable,
                                                  bool isTransformFeedbackActiveUnpaused,
                                                  TransformFeedbackVk *transformFeedbackVk);
    angle::Result allocateShaderResourcesDescriptorSet(
        vk::Context *context,
        vk::ResourceUseList *resourceUseList,
        const vk::DescriptorSetDesc *shaderBuffersDesc);
    angle::Result updateBuffersDescriptorSet(vk::Context *context,
                                             UpdateDescriptorSetsBuilder *updateBuilder,
                                             vk::BufferHelper *emptyBuffer,
                                             vk::ResourceUseList *resourceUseList,
                                             gl::ShaderType shaderType,
                                             const vk::DescriptorSetDesc &shaderBuffersDesc,
                                             const gl::BufferVector &buffers,
                                             const std::vector<gl::InterfaceBlock> &blocks,
                                             VkDescriptorType descriptorType,
                                             VkDeviceSize maxBoundBufferRange,
                                             bool cacheHit);
    angle::Result updateAtomicCounterBuffersDescriptorSet(
        vk::Context *context,
        UpdateDescriptorSetsBuilder *updateBuilder,
        vk::BufferHelper *emptyBuffer,
        vk::ResourceUseList *resourceUseList,
        const gl::BufferVector &atomicCounterBufferBindings,
        const gl::ProgramExecutable &executable,
        gl::ShaderType shaderType,
        const vk::DescriptorSetDesc &shaderBuffersDesc,
        bool cacheHit);
    angle::Result updateImagesDescriptorSet(vk::Context *context,
                                            vk::ResourceUseList *resourceUseList,
                                            UpdateDescriptorSetsBuilder *updateBuilder,
                                            const gl::ActiveTextureArray<TextureVk *> &activeImages,
                                            const std::vector<gl::ImageUnit> &imageUnits,
                                            const gl::ProgramExecutable &executable,
                                            gl::ShaderType shaderType);

    static angle::Result InitDynamicDescriptorPool(
        vk::Context *context,
        vk::DescriptorSetLayoutDesc &descriptorSetLayoutDesc,
        VkDescriptorSetLayout descriptorSetLayout,
        uint32_t descriptorCountMultiplier,
        vk::DynamicDescriptorPool *dynamicDescriptorPool);

    void outputCumulativePerfCounters();

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

    angle::Result resizeUniformBlockMemory(ContextVk *contextVk,
                                           const gl::ProgramExecutable &glExecutable,
                                           const gl::ShaderMap<size_t> &requiredBufferSize);

    // Descriptor sets for uniform blocks and textures for this program.
    vk::DescriptorSetArray<VkDescriptorSet> mDescriptorSets;
    vk::DescriptorSetArray<VkDescriptorSet> mEmptyDescriptorSets;
    uint32_t mNumDefaultUniformDescriptors;
    vk::BufferSerial mCurrentDefaultUniformBufferSerial;

    DescriptorSetCache mUniformsAndXfbDescriptorsCache;
    DescriptorSetCache mTextureDescriptorsCache;
    DescriptorSetCache mShaderBufferDescriptorsCache;

    // We keep a reference to the pipeline and descriptor set layouts. This ensures they don't get
    // deleted while this program is in use.
    uint32_t mImmutableSamplersMaxDescriptorCount;
    ImmutableSamplerIndexMap mImmutableSamplerIndexMap;
    vk::BindingPointer<vk::PipelineLayout> mPipelineLayout;
    vk::DescriptorSetLayoutPointerArray mDescriptorSetLayouts;

    // Keep bindings to the descriptor pools. This ensures the pools stay valid while the Program
    // is in use.
    vk::DescriptorSetArray<vk::RefCountedDescriptorPoolBinding> mDescriptorPoolBindings;

    // Store descriptor pools here. We store the descriptors in the Program to facilitate descriptor
    // cache management. It can also allow fewer descriptors for shaders which use fewer
    // textures/buffers.
    vk::DescriptorSetArray<vk::DynamicDescriptorPool> mDynamicDescriptorPools;

    // A set of dynamic offsets used with vkCmdBindDescriptorSets for the default uniform buffers.
    VkDescriptorType mUniformBufferDescriptorType;
    gl::ShaderVector<uint32_t> mDynamicUniformDescriptorOffsets;
    std::vector<uint32_t> mDynamicShaderBufferDescriptorOffsets;

    // TODO: http://anglebug.com/4524: Need a different hash key than a string,
    // since that's slow to calculate.
    ShaderInterfaceVariableInfoMap mVariableInfoMap;

    // We store all permutations of surface rotation and transformed SPIR-V programs here. We may
    // need some LRU algorithm to free least used programs to reduce the number of programs.
    ProgramInfo mGraphicsProgramInfos[ProgramTransformOptions::kPermutationCount];
    ProgramInfo mComputeProgramInfo;

    ProgramTransformOptions mTransformOptions;

    DefaultUniformBlockMap mDefaultUniformBlocks;
    gl::ShaderBitSet mDefaultUniformBlocksDirty;

    ShaderInfo mOriginalShaderInfo;

    ProgramExecutablePerfCounters mPerfCounters;
    ProgramExecutablePerfCounters mCumulativePerfCounters;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_PROGRAMEXECUTABLEVK_H_
