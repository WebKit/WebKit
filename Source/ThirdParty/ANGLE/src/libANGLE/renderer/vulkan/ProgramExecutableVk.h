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
#include "libANGLE/ProgramExecutable.h"
#include "libANGLE/renderer/ProgramExecutableImpl.h"
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

    angle::Result initShaders(vk::Context *context,
                              const gl::ShaderBitSet &linkedShaderStages,
                              const gl::ShaderMap<const angle::spirv::Blob *> &spirvBlobs,
                              const ShaderInterfaceVariableInfoMap &variableInfoMap,
                              bool isGLES1);
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

    angle::Result initProgram(vk::Context *context,
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
struct DefaultUniformBlockVk final : private angle::NonCopyable
{
    DefaultUniformBlockVk();
    ~DefaultUniformBlockVk();

    // Shadow copies of the shader uniform data.
    angle::MemoryBuffer uniformData;

    // Since the default blocks are laid out in std140, this tells us where to write on a call
    // to a setUniform method. They are arranged in uniform location order.
    std::vector<sh::BlockMemberInfo> uniformLayout;
};

// Performance and resource counters.
using DescriptorSetCountList   = angle::PackedEnumMap<DescriptorSetIndex, uint32_t>;
using ImmutableSamplerIndexMap = angle::HashMap<vk::YcbcrConversionDesc, uint32_t>;

using DefaultUniformBlockMap = gl::ShaderMap<std::shared_ptr<DefaultUniformBlockVk>>;

class ProgramExecutableVk : public ProgramExecutableImpl
{
  public:
    ProgramExecutableVk(const gl::ProgramExecutable *executable);
    ~ProgramExecutableVk() override;

    void destroy(const gl::Context *context) override;

    void save(ContextVk *contextVk, bool isSeparable, gl::BinaryOutputStream *stream);
    angle::Result load(ContextVk *contextVk,
                       bool isSeparable,
                       gl::BinaryInputStream *stream,
                       egl::CacheGetResult *resultOut);

    void setUniform1fv(GLint location, GLsizei count, const GLfloat *v) override;
    void setUniform2fv(GLint location, GLsizei count, const GLfloat *v) override;
    void setUniform3fv(GLint location, GLsizei count, const GLfloat *v) override;
    void setUniform4fv(GLint location, GLsizei count, const GLfloat *v) override;
    void setUniform1iv(GLint location, GLsizei count, const GLint *v) override;
    void setUniform2iv(GLint location, GLsizei count, const GLint *v) override;
    void setUniform3iv(GLint location, GLsizei count, const GLint *v) override;
    void setUniform4iv(GLint location, GLsizei count, const GLint *v) override;
    void setUniform1uiv(GLint location, GLsizei count, const GLuint *v) override;
    void setUniform2uiv(GLint location, GLsizei count, const GLuint *v) override;
    void setUniform3uiv(GLint location, GLsizei count, const GLuint *v) override;
    void setUniform4uiv(GLint location, GLsizei count, const GLuint *v) override;
    void setUniformMatrix2fv(GLint location,
                             GLsizei count,
                             GLboolean transpose,
                             const GLfloat *value) override;
    void setUniformMatrix3fv(GLint location,
                             GLsizei count,
                             GLboolean transpose,
                             const GLfloat *value) override;
    void setUniformMatrix4fv(GLint location,
                             GLsizei count,
                             GLboolean transpose,
                             const GLfloat *value) override;
    void setUniformMatrix2x3fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;
    void setUniformMatrix3x2fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;
    void setUniformMatrix2x4fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;
    void setUniformMatrix4x2fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;
    void setUniformMatrix3x4fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;
    void setUniformMatrix4x3fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;

    void getUniformfv(const gl::Context *context, GLint location, GLfloat *params) const override;
    void getUniformiv(const gl::Context *context, GLint location, GLint *params) const override;
    void getUniformuiv(const gl::Context *context, GLint location, GLuint *params) const override;

    void clearVariableInfoMap();

    vk::BufferSerial getCurrentDefaultUniformBufferSerial() const
    {
        return mCurrentDefaultUniformBufferSerial;
    }

    // Get the graphics pipeline if already created.
    angle::Result getGraphicsPipeline(ContextVk *contextVk,
                                      vk::GraphicsPipelineSubset pipelineSubset,
                                      const vk::GraphicsPipelineDesc &desc,
                                      const vk::GraphicsPipelineDesc **descPtrOut,
                                      vk::PipelineHelper **pipelineOut);

    angle::Result createGraphicsPipeline(ContextVk *contextVk,
                                         vk::GraphicsPipelineSubset pipelineSubset,
                                         vk::PipelineCacheAccess *pipelineCache,
                                         PipelineSource source,
                                         const vk::GraphicsPipelineDesc &desc,
                                         const vk::GraphicsPipelineDesc **descPtrOut,
                                         vk::PipelineHelper **pipelineOut);

    angle::Result linkGraphicsPipelineLibraries(ContextVk *contextVk,
                                                vk::PipelineCacheAccess *pipelineCache,
                                                const vk::GraphicsPipelineDesc &desc,
                                                vk::PipelineHelper *vertexInputPipeline,
                                                vk::PipelineHelper *shadersPipeline,
                                                vk::PipelineHelper *fragmentOutputPipeline,
                                                const vk::GraphicsPipelineDesc **descPtrOut,
                                                vk::PipelineHelper **pipelineOut);

    angle::Result getOrCreateComputePipeline(vk::Context *context,
                                             vk::PipelineCacheAccess *pipelineCache,
                                             PipelineSource source,
                                             vk::PipelineRobustness pipelineRobustness,
                                             vk::PipelineProtectedAccess pipelineProtectedAccess,
                                             vk::PipelineHelper **pipelineOut);

    const vk::PipelineLayout &getPipelineLayout() const { return mPipelineLayout.get(); }
    void resetLayout(ContextVk *contextVk);
    angle::Result createPipelineLayout(vk::Context *context,
                                       PipelineLayoutCache *pipelineLayoutCache,
                                       DescriptorSetLayoutCache *descriptorSetLayoutCache,
                                       gl::ActiveTextureArray<TextureVk *> *activeTextures);
    angle::Result initializeDescriptorPools(
        vk::Context *context,
        DescriptorSetLayoutCache *descriptorSetLayoutCache,
        vk::DescriptorSetArray<vk::MetaDescriptorPool> *metaDescriptorPools);

    angle::Result updateTexturesDescriptorSet(vk::Context *context,
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

    std::shared_ptr<DefaultUniformBlockVk> &getSharedDefaultUniformBlock(gl::ShaderType shaderType)
    {
        return mDefaultUniformBlocks[shaderType];
    }

    bool hasDirtyUniforms() const { return mDefaultUniformBlocksDirty.any(); }

    void setAllDefaultUniformsDirty();
    angle::Result updateUniforms(vk::Context *context,
                                 UpdateDescriptorSetsBuilder *updateBuilder,
                                 vk::CommandBufferHelperCommon *commandBufferHelper,
                                 vk::BufferHelper *emptyBuffer,
                                 vk::DynamicBuffer *defaultUniformStorage,
                                 bool isTransformFeedbackActiveUnpaused,
                                 TransformFeedbackVk *transformFeedbackVk);
    void onProgramBind();

    const ShaderInterfaceVariableInfoMap &getVariableInfoMap() const { return mVariableInfoMap; }

    angle::Result warmUpPipelineCache(vk::Context *context,
                                      vk::PipelineRobustness pipelineRobustness,
                                      vk::PipelineProtectedAccess pipelineProtectedAccess,
                                      vk::RenderPass *temporaryCompatibleRenderPassOut);

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
    // The following functions are for internal use of programs, including from a threaded link job:
    angle::Result resizeUniformBlockMemory(vk::Context *context,
                                           const gl::ShaderMap<size_t> &requiredBufferSize);
    void resolvePrecisionMismatch(const gl::ProgramMergedVaryings &mergedVaryings);
    angle::Result initShaders(vk::Context *context,
                              const gl::ShaderBitSet &linkedShaderStages,
                              const gl::ShaderMap<const angle::spirv::Blob *> &spirvBlobs,
                              bool isGLES1)
    {
        return mOriginalShaderInfo.initShaders(context, linkedShaderStages, spirvBlobs,
                                               mVariableInfoMap, isGLES1);
    }
    void assignAllSpvLocations(vk::Context *context,
                               const gl::ProgramState &programState,
                               const gl::ProgramLinkedResources &resources)
    {
        SpvSourceOptions options = SpvCreateSourceOptions(context->getFeatures());
        SpvAssignAllLocations(options, programState, resources, &mVariableInfoMap);
    }

  private:
    friend class ProgramVk;
    friend class ProgramPipelineVk;

    void reset(ContextVk *contextVk);

    template <int cols, int rows>
    void setUniformMatrixfv(GLint location,
                            GLsizei count,
                            GLboolean transpose,
                            const GLfloat *value);

    template <class T>
    void getUniformImpl(GLint location, T *v, GLenum entryPointType) const;

    template <typename T>
    void setUniformImpl(GLint location, GLsizei count, const T *v, GLenum entryPointType);

    void addInterfaceBlockDescriptorSetDesc(const std::vector<gl::InterfaceBlock> &blocks,
                                            gl::ShaderBitSet shaderTypes,
                                            VkDescriptorType descType,
                                            vk::DescriptorSetLayoutDesc *descOut);
    void addAtomicCounterBufferDescriptorSetDesc(
        const std::vector<gl::AtomicCounterBuffer> &atomicCounterBuffers,
        vk::DescriptorSetLayoutDesc *descOut);
    void addImageDescriptorSetDesc(vk::DescriptorSetLayoutDesc *descOut);
    void addInputAttachmentDescriptorSetDesc(vk::DescriptorSetLayoutDesc *descOut);
    angle::Result addTextureDescriptorSetDesc(
        vk::Context *context,
        const gl::ActiveTextureArray<TextureVk *> *activeTextures,
        vk::DescriptorSetLayoutDesc *descOut);

    size_t calcUniformUpdateRequiredSpace(vk::Context *context,
                                          gl::ShaderMap<VkDeviceSize> *uniformOffsets) const;

    ANGLE_INLINE angle::Result initProgram(vk::Context *context,
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
            ANGLE_TRY(programInfo->initProgram(context, shaderType, isLastPreFragmentStage,
                                               isTransformFeedbackProgram, mOriginalShaderInfo,
                                               optionBits, variableInfoMap));
        }
        ASSERT(programInfo->valid(shaderType));

        return angle::Result::Continue;
    }

    ANGLE_INLINE angle::Result initGraphicsShaderProgram(
        vk::Context *context,
        gl::ShaderType shaderType,
        bool isLastPreFragmentStage,
        bool isTransformFeedbackProgram,
        ProgramTransformOptions optionBits,
        ProgramInfo *programInfo,
        const ShaderInterfaceVariableInfoMap &variableInfoMap)
    {
        return initProgram(context, shaderType, isLastPreFragmentStage, isTransformFeedbackProgram,
                           optionBits, programInfo, variableInfoMap);
    }

    ANGLE_INLINE angle::Result initComputeProgram(
        vk::Context *context,
        ProgramInfo *programInfo,
        const ShaderInterfaceVariableInfoMap &variableInfoMap)
    {
        ProgramTransformOptions optionBits = {};
        return initProgram(context, gl::ShaderType::Compute, false, false, optionBits, programInfo,
                           variableInfoMap);
    }

    ProgramTransformOptions getTransformOptions(ContextVk *contextVk,
                                                const vk::GraphicsPipelineDesc &desc);
    angle::Result initGraphicsShaderPrograms(vk::Context *context,
                                             ProgramTransformOptions transformOptions,
                                             vk::ShaderProgramHelper **shaderProgramOut);
    angle::Result createGraphicsPipelineImpl(vk::Context *context,
                                             ProgramTransformOptions transformOptions,
                                             vk::GraphicsPipelineSubset pipelineSubset,
                                             vk::PipelineCacheAccess *pipelineCache,
                                             PipelineSource source,
                                             const vk::GraphicsPipelineDesc &desc,
                                             const vk::RenderPass &compatibleRenderPass,
                                             const vk::GraphicsPipelineDesc **descPtrOut,
                                             vk::PipelineHelper **pipelineOut);

    angle::Result getOrAllocateDescriptorSet(vk::Context *context,
                                             UpdateDescriptorSetsBuilder *updateBuilder,
                                             vk::CommandBufferHelperCommon *commandBufferHelper,
                                             const vk::DescriptorSetDescBuilder &descriptorSetDesc,
                                             const vk::WriteDescriptorDescs &writeDescriptorDescs,
                                             DescriptorSetIndex setIndex,
                                             vk::SharedDescriptorSetCacheKey *newSharedCacheKeyOut);

    // When loading from cache / binary, initialize the pipeline cache with given data.  Otherwise
    // the cache is lazily created as needed.
    angle::Result initializePipelineCache(vk::Context *context,
                                          bool compressed,
                                          const std::vector<uint8_t> &pipelineData);
    angle::Result ensurePipelineCacheInitialized(vk::Context *context);

    void initializeWriteDescriptorDesc(vk::Context *context);

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
    vk::AtomicBindingPointer<vk::PipelineLayout> mPipelineLayout;
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

    vk::DescriptorSetLayoutDesc mShaderResourceSetDesc;
    vk::DescriptorSetLayoutDesc mTextureSetDesc;
    vk::DescriptorSetLayoutDesc mDefaultUniformAndXfbSetDesc;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_PROGRAMEXECUTABLEVK_H_
