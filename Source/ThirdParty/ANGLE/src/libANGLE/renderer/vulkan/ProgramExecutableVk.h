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
    void release(ContextVk *contextVk);

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
                              const gl::ShaderType shaderType,
                              bool isLastPreFragmentStage,
                              bool isTransformFeedbackProgram,
                              const ShaderInfo &shaderInfo,
                              ProgramTransformOptions optionBits,
                              const ShaderInterfaceVariableInfoMap &variableInfoMap);
    void release(ContextVk *contextVk);

    ANGLE_INLINE bool valid(const gl::ShaderType shaderType) const
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
};

class ProgramExecutableVk
{
  public:
    ProgramExecutableVk();
    virtual ~ProgramExecutableVk();

    void reset(ContextVk *contextVk);

    void save(gl::BinaryOutputStream *stream);
    std::unique_ptr<rx::LinkEvent> load(gl::BinaryInputStream *stream);

    void clearVariableInfoMap();

    ProgramVk *getShaderProgram(const gl::State &glState, gl::ShaderType shaderType) const;

    void fillProgramStateMap(const ContextVk *contextVk,
                             gl::ShaderMap<const gl::ProgramState *> *programStatesOut);
    const gl::ProgramExecutable &getGlExecutable();

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
                                      const vk::GraphicsPipelineDesc **descPtrOut,
                                      vk::PipelineHelper **pipelineOut);

    angle::Result getComputePipeline(ContextVk *contextVk, vk::PipelineHelper **pipelineOut);

    const vk::PipelineLayout &getPipelineLayout() const { return mPipelineLayout.get(); }
    angle::Result createPipelineLayout(ContextVk *contextVk,
                                       const gl::ProgramExecutable &glExecutable,
                                       gl::ActiveTextureArray<vk::TextureUnit> *activeTextures);

    angle::Result updateTexturesDescriptorSet(ContextVk *contextVk,
                                              const vk::TextureDescriptorDesc &texturesDesc);
    angle::Result updateShaderResourcesDescriptorSet(
        ContextVk *contextVk,
        FramebufferVk *framebufferVk,
        const vk::ShaderBuffersDescriptorDesc &shaderBuffersDesc,
        vk::CommandBufferHelper *commandBufferHelper);
    angle::Result updateTransformFeedbackDescriptorSet(
        const gl::ProgramState &programState,
        gl::ShaderMap<DefaultUniformBlock> &defaultUniformBlocks,
        vk::BufferHelper *defaultUniformBuffer,
        ContextVk *contextVk,
        const vk::UniformsAndXfbDescriptorDesc &xfbBufferDesc);
    angle::Result updateInputAttachmentDescriptorSet(const gl::ProgramExecutable &executable,
                                                     const gl::ShaderType shaderType,
                                                     ContextVk *contextVk,
                                                     FramebufferVk *framebufferVk);

    angle::Result updateDescriptorSets(ContextVk *contextVk,
                                       vk::CommandBuffer *commandBuffer,
                                       PipelineType pipelineType);

    void updateEarlyFragmentTestsOptimization(ContextVk *contextVk);

    void setProgram(ProgramVk *program)
    {
        ASSERT(!mProgram && !mProgramPipeline);
        mProgram = program;
    }
    void setProgramPipeline(ProgramPipelineVk *pipeline)
    {
        ASSERT(!mProgram && !mProgramPipeline);
        mProgramPipeline = pipeline;
    }

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

  private:
    friend class ProgramVk;
    friend class ProgramPipelineVk;

    angle::Result allocUniformAndXfbDescriptorSet(
        ContextVk *contextVk,
        const vk::UniformsAndXfbDescriptorDesc &xfbBufferDesc,
        bool *newDescriptorSetAllocated);

    angle::Result allocateDescriptorSet(ContextVk *contextVk,
                                        DescriptorSetIndex descriptorSetIndex);
    angle::Result allocateDescriptorSetAndGetInfo(ContextVk *contextVk,
                                                  DescriptorSetIndex descriptorSetIndex,
                                                  bool *newPoolAllocatedOut);
    void addInterfaceBlockDescriptorSetDesc(const std::vector<gl::InterfaceBlock> &blocks,
                                            const gl::ShaderType shaderType,
                                            VkDescriptorType descType,
                                            vk::DescriptorSetLayoutDesc *descOut);
    void addAtomicCounterBufferDescriptorSetDesc(
        const std::vector<gl::AtomicCounterBuffer> &atomicCounterBuffers,
        const gl::ShaderType shaderType,
        vk::DescriptorSetLayoutDesc *descOut);
    void addImageDescriptorSetDesc(const gl::ProgramExecutable &executable,
                                   vk::DescriptorSetLayoutDesc *descOut);
    void addInputAttachmentDescriptorSetDesc(const gl::ProgramExecutable &executable,
                                             const gl::ShaderType shaderType,
                                             vk::DescriptorSetLayoutDesc *descOut);
    void addTextureDescriptorSetDesc(ContextVk *contextVk,
                                     const gl::ProgramState &programState,
                                     const gl::ActiveTextureArray<vk::TextureUnit> *activeTextures,
                                     vk::DescriptorSetLayoutDesc *descOut);

    void resolvePrecisionMismatch(const gl::ProgramMergedVaryings &mergedVaryings);
    void updateDefaultUniformsDescriptorSet(const gl::ShaderType shaderType,
                                            const DefaultUniformBlock &defaultUniformBlock,
                                            vk::BufferHelper *defaultUniformBuffer,
                                            ContextVk *contextVk);
    void updateTransformFeedbackDescriptorSetImpl(const gl::ProgramState &programState,
                                                  ContextVk *contextVk);
    angle::Result getOrAllocateShaderResourcesDescriptorSet(
        ContextVk *contextVk,
        const vk::ShaderBuffersDescriptorDesc *shaderBuffersDesc,
        VkDescriptorSet *descriptorSetOut);
    angle::Result updateBuffersDescriptorSet(
        ContextVk *contextVk,
        const gl::ShaderType shaderType,
        const vk::ShaderBuffersDescriptorDesc &shaderBuffersDesc,
        const std::vector<gl::InterfaceBlock> &blocks,
        VkDescriptorType descriptorType,
        bool cacheHit);
    angle::Result updateAtomicCounterBuffersDescriptorSet(
        ContextVk *contextVk,
        const gl::ProgramState &programState,
        const gl::ShaderType shaderType,
        const vk::ShaderBuffersDescriptorDesc &shaderBuffersDesc,
        bool cacheHit);
    angle::Result updateImagesDescriptorSet(ContextVk *contextVk,
                                            const gl::ProgramExecutable &executable,
                                            const gl::ShaderType shaderType);
    angle::Result initDynamicDescriptorPools(ContextVk *contextVk,
                                             vk::DescriptorSetLayoutDesc &descriptorSetLayoutDesc,
                                             DescriptorSetIndex descriptorSetIndex,
                                             VkDescriptorSetLayout descriptorSetLayout);

    void outputCumulativePerfCounters();

    // Descriptor sets for uniform blocks and textures for this program.
    vk::DescriptorSetArray<VkDescriptorSet> mDescriptorSets;
    vk::DescriptorSetArray<VkDescriptorSet> mEmptyDescriptorSets;
    uint32_t mNumDefaultUniformDescriptors;
    vk::BufferSerial mCurrentDefaultUniformBufferSerial;

    DescriptorSetCache<vk::UniformsAndXfbDescriptorDesc, VulkanCacheType::UniformsAndXfbDescriptors>
        mUniformsAndXfbDescriptorsCache;
    DescriptorSetCache<vk::TextureDescriptorDesc, VulkanCacheType::TextureDescriptors>
        mTextureDescriptorsCache;
    DescriptorSetCache<vk::ShaderBuffersDescriptorDesc, VulkanCacheType::ShaderBuffersDescriptors>
        mShaderBufferDescriptorsCache;

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

    ProgramVk *mProgram;
    ProgramPipelineVk *mProgramPipeline;

    ProgramExecutablePerfCounters mPerfCounters;
    ProgramExecutablePerfCounters mCumulativePerfCounters;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_PROGRAMEXECUTABLEVK_H_
