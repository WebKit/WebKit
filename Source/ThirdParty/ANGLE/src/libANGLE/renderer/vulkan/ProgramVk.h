//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramVk.h:
//    Defines the class interface for ProgramVk, implementing ProgramImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_PROGRAMVK_H_
#define LIBANGLE_RENDERER_VULKAN_PROGRAMVK_H_

#include <array>

#include "common/utilities.h"
#include "libANGLE/renderer/ProgramImpl.h"
#include "libANGLE/renderer/glslang_wrapper_utils.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/TransformFeedbackVk.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace rx
{
ANGLE_INLINE bool UseLineRaster(const ContextVk *contextVk, gl::PrimitiveMode mode)
{
    return contextVk->getFeatures().basicGLLineRasterization.enabled && gl::IsLineMode(mode);
}

class ProgramVk : public ProgramImpl
{
  public:
    ProgramVk(const gl::ProgramState &state);
    ~ProgramVk() override;
    void destroy(const gl::Context *context) override;

    std::unique_ptr<LinkEvent> load(const gl::Context *context,
                                    gl::BinaryInputStream *stream,
                                    gl::InfoLog &infoLog) override;
    void save(const gl::Context *context, gl::BinaryOutputStream *stream) override;
    void setBinaryRetrievableHint(bool retrievable) override;
    void setSeparable(bool separable) override;

    std::unique_ptr<LinkEvent> link(const gl::Context *context,
                                    const gl::ProgramLinkedResources &resources,
                                    gl::InfoLog &infoLog) override;
    GLboolean validate(const gl::Caps &caps, gl::InfoLog *infoLog) override;

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

    void setPathFragmentInputGen(const std::string &inputName,
                                 GLenum genMode,
                                 GLint components,
                                 const GLfloat *coeffs) override;

    // Also initializes the pipeline layout, descriptor set layouts, and used descriptor ranges.

    angle::Result updateUniforms(ContextVk *contextVk);
    angle::Result updateTexturesDescriptorSet(ContextVk *contextVk);
    angle::Result updateShaderResourcesDescriptorSet(ContextVk *contextVk,
                                                     vk::CommandGraphResource *recorder);
    angle::Result updateTransformFeedbackDescriptorSet(ContextVk *contextVk,
                                                       vk::FramebufferHelper *framebuffer);

    angle::Result updateDescriptorSets(ContextVk *contextVk, vk::CommandBuffer *commandBuffer);

    // For testing only.
    void setDefaultUniformBlocksMinSizeForTesting(size_t minSize);

    const vk::PipelineLayout &getPipelineLayout() const { return mPipelineLayout.get(); }

    bool hasDefaultUniforms() const { return !mState.getDefaultUniformRange().empty(); }
    bool hasTextures() const { return !mState.getSamplerBindings().empty(); }
    bool hasUniformBuffers() const { return !mState.getUniformBlocks().empty(); }
    bool hasStorageBuffers() const { return !mState.getShaderStorageBlocks().empty(); }
    bool hasAtomicCounterBuffers() const { return !mState.getAtomicCounterBuffers().empty(); }
    bool hasImages() const { return !mState.getImageBindings().empty(); }
    bool hasTransformFeedbackOutput() const
    {
        return !mState.getLinkedTransformFeedbackVaryings().empty();
    }

    bool dirtyUniforms() const { return mDefaultUniformBlocksDirty.any(); }

    angle::Result getGraphicsPipeline(ContextVk *contextVk,
                                      gl::PrimitiveMode mode,
                                      const vk::GraphicsPipelineDesc &desc,
                                      const gl::AttributesMask &activeAttribLocations,
                                      const vk::GraphicsPipelineDesc **descPtrOut,
                                      vk::PipelineHelper **pipelineOut)
    {
        vk::ShaderProgramHelper *shaderProgram;
        ANGLE_TRY(initGraphicsProgram(contextVk, mode, &shaderProgram));
        ASSERT(shaderProgram->isGraphicsProgram());
        RendererVk *renderer             = contextVk->getRenderer();
        vk::PipelineCache *pipelineCache = nullptr;
        ANGLE_TRY(renderer->getPipelineCache(&pipelineCache));
        return shaderProgram->getGraphicsPipeline(
            contextVk, &contextVk->getRenderPassCache(), *pipelineCache,
            contextVk->getCurrentQueueSerial(), mPipelineLayout.get(), desc, activeAttribLocations,
            mState.getAttributesTypeMask(), descPtrOut, pipelineOut);
    }

    angle::Result getComputePipeline(ContextVk *contextVk, vk::PipelineAndSerial **pipelineOut)
    {
        vk::ShaderProgramHelper *shaderProgram;
        ANGLE_TRY(initComputeProgram(contextVk, &shaderProgram));
        ASSERT(!shaderProgram->isGraphicsProgram());
        return shaderProgram->getComputePipeline(contextVk, mPipelineLayout.get(), pipelineOut);
    }

    // Used in testing only.
    vk::DynamicDescriptorPool *getDynamicDescriptorPool(uint32_t poolIndex)
    {
        return &mDynamicDescriptorPools[poolIndex];
    }

  private:
    template <int cols, int rows>
    void setUniformMatrixfv(GLint location,
                            GLsizei count,
                            GLboolean transpose,
                            const GLfloat *value);

    void reset(ContextVk *contextVk);
    angle::Result allocateDescriptorSet(ContextVk *contextVk, uint32_t descriptorSetIndex);
    angle::Result allocateDescriptorSetAndGetInfo(ContextVk *contextVk,
                                                  uint32_t descriptorSetIndex,
                                                  bool *newPoolAllocatedOut);
    angle::Result initDefaultUniformBlocks(const gl::Context *glContext);
    void generateUniformLayoutMapping(gl::ShaderMap<sh::BlockLayoutMap> &layoutMap,
                                      gl::ShaderMap<size_t> &requiredBufferSize);
    void initDefaultUniformLayoutMapping(gl::ShaderMap<sh::BlockLayoutMap> &layoutMap);
    angle::Result resizeUniformBlockMemory(ContextVk *contextVk,
                                           gl::ShaderMap<size_t> &requiredBufferSize);

    void updateDefaultUniformsDescriptorSet(ContextVk *contextVk);
    void updateTransformFeedbackDescriptorSetImpl(ContextVk *contextVk);
    void updateBuffersDescriptorSet(ContextVk *contextVk,
                                    vk::CommandGraphResource *recorder,
                                    const std::vector<gl::InterfaceBlock> &blocks,
                                    VkDescriptorType descriptorType);
    void updateAtomicCounterBuffersDescriptorSet(ContextVk *contextVk,
                                                 vk::CommandGraphResource *recorder);
    angle::Result updateImagesDescriptorSet(ContextVk *contextVk,
                                            vk::CommandGraphResource *recorder);

    template <class T>
    void getUniformImpl(GLint location, T *v, GLenum entryPointType) const;

    template <typename T>
    void setUniformImpl(GLint location, GLsizei count, const T *v, GLenum entryPointType);
    angle::Result linkImpl(const gl::Context *glContext, gl::InfoLog &infoLog);
    void linkResources(const gl::ProgramLinkedResources &resources);

    void updateBindingOffsets();
    uint32_t getUniformBlockBindingsOffset() const { return 0; }
    uint32_t getStorageBlockBindingsOffset() const { return mStorageBlockBindingsOffset; }
    uint32_t getAtomicCounterBufferBindingsOffset() const
    {
        return mAtomicCounterBufferBindingsOffset;
    }
    uint32_t getImageBindingsOffset() const { return mImageBindingsOffset; }

    class ProgramInfo;
    ANGLE_INLINE angle::Result initProgram(ContextVk *contextVk,
                                           bool enableLineRasterEmulation,
                                           ProgramInfo *programInfo,
                                           vk::ShaderProgramHelper **shaderProgramOut)
    {
        // Compile shaders if not already.  This is done only once regardless of specialization
        // constants.
        if (!mShaderInfo.valid())
        {
            ANGLE_TRY(mShaderInfo.initShaders(contextVk, mShaderSources, mVariableInfoMap,
                                              &mShaderInfo.getSpirvBlobs()));
        }
        ASSERT(mShaderInfo.valid());

        // Create the program pipeline.  This is done lazily and once per combination of
        // specialization constants.
        if (!programInfo->valid())
        {
            ANGLE_TRY(programInfo->initProgram(contextVk, mShaderInfo, enableLineRasterEmulation));
        }
        ASSERT(programInfo->valid());

        *shaderProgramOut = programInfo->getShaderProgram();
        return angle::Result::Continue;
    }

    ANGLE_INLINE angle::Result initGraphicsProgram(ContextVk *contextVk,
                                                   gl::PrimitiveMode mode,
                                                   vk::ShaderProgramHelper **shaderProgramOut)
    {
        bool enableLineRasterEmulation = UseLineRaster(contextVk, mode);

        ProgramInfo &programInfo =
            enableLineRasterEmulation ? mLineRasterProgramInfo : mDefaultProgramInfo;

        return initProgram(contextVk, enableLineRasterEmulation, &programInfo, shaderProgramOut);
    }

    ANGLE_INLINE angle::Result initComputeProgram(ContextVk *contextVk,
                                                  vk::ShaderProgramHelper **shaderProgramOut)
    {
        return initProgram(contextVk, false, &mDefaultProgramInfo, shaderProgramOut);
    }

    // Save and load implementation for GLES Program Binary support.
    angle::Result loadSpirvBlob(ContextVk *contextVk, gl::BinaryInputStream *stream);
    void saveSpirvBlob(gl::BinaryOutputStream *stream);

    // State for the default uniform blocks.
    struct DefaultUniformBlock final : private angle::NonCopyable
    {
        DefaultUniformBlock();
        ~DefaultUniformBlock();

        vk::DynamicBuffer storage;

        // Shadow copies of the shader uniform data.
        angle::MemoryBuffer uniformData;

        // Since the default blocks are laid out in std140, this tells us where to write on a call
        // to a setUniform method. They are arranged in uniform location order.
        std::vector<sh::BlockMemberInfo> uniformLayout;
    };

    gl::ShaderMap<DefaultUniformBlock> mDefaultUniformBlocks;
    gl::ShaderBitSet mDefaultUniformBlocksDirty;

    gl::ShaderVector<uint32_t> mDynamicBufferOffsets;

    // This is a special "empty" placeholder buffer for when a shader has no uniforms or doesn't
    // use all slots in the atomic counter buffer array.
    //
    // It is necessary because we want to keep a compatible pipeline layout in all cases,
    // and Vulkan does not tolerate having null handles in a descriptor set.
    vk::BufferHelper mEmptyBuffer;

    // Descriptor sets for uniform blocks and textures for this program.
    std::vector<VkDescriptorSet> mDescriptorSets;
    vk::DescriptorSetLayoutArray<VkDescriptorSet> mEmptyDescriptorSets;
    std::vector<vk::BufferHelper *> mDescriptorBuffersCache;

    std::unordered_map<vk::TextureDescriptorDesc, VkDescriptorSet> mTextureDescriptorsCache;

    // We keep a reference to the pipeline and descriptor set layouts. This ensures they don't get
    // deleted while this program is in use.
    vk::BindingPointer<vk::PipelineLayout> mPipelineLayout;
    vk::DescriptorSetLayoutPointerArray mDescriptorSetLayouts;

    // Keep bindings to the descriptor pools. This ensures the pools stay valid while the Program
    // is in use.
    vk::DescriptorSetLayoutArray<vk::RefCountedDescriptorPoolBinding> mDescriptorPoolBindings;

    class ShaderInfo final : angle::NonCopyable
    {
      public:
        ShaderInfo();
        ~ShaderInfo();

        angle::Result initShaders(ContextVk *contextVk,
                                  const gl::ShaderMap<std::string> &shaderSources,
                                  const ShaderInterfaceVariableInfoMap &variableInfoMap,
                                  gl::ShaderMap<SpirvBlob> *spirvBlobsOut);
        void release(ContextVk *contextVk);

        ANGLE_INLINE bool valid() const { return mIsInitialized; }

        gl::ShaderMap<SpirvBlob> &getSpirvBlobs() { return mSpirvBlobs; }
        const gl::ShaderMap<SpirvBlob> &getSpirvBlobs() const { return mSpirvBlobs; }

      private:
        gl::ShaderMap<SpirvBlob> mSpirvBlobs;
        bool mIsInitialized = false;
    };

    class ProgramInfo final : angle::NonCopyable
    {
      public:
        ProgramInfo();
        ~ProgramInfo();

        angle::Result initProgram(ContextVk *contextVk,
                                  const ShaderInfo &shaderInfo,
                                  bool enableLineRasterEmulation);
        void release(ContextVk *contextVk);

        ANGLE_INLINE bool valid() const { return mProgramHelper.valid(); }

        vk::ShaderProgramHelper *getShaderProgram() { return &mProgramHelper; }

      private:
        vk::ShaderProgramHelper mProgramHelper;
        gl::ShaderMap<vk::RefCounted<vk::ShaderAndSerial>> mShaders;
    };

    ProgramInfo mDefaultProgramInfo;
    ProgramInfo mLineRasterProgramInfo;

    // We keep the translated linked shader sources and the expected location/set/binding mapping to
    // use with shader draw call compilation.
    // TODO(syoussefi): Remove when shader compilation is done at link time.
    // http://anglebug.com/3394
    gl::ShaderMap<std::string> mShaderSources;
    ShaderInterfaceVariableInfoMap mVariableInfoMap;
    // We keep the SPIR-V code to use for draw call pipeline creation.
    ShaderInfo mShaderInfo;

    // In their descriptor set, uniform buffers are placed first, then storage buffers, then atomic
    // counter buffers and then images.  These cached values contain the offsets where storage
    // buffer, atomic counter buffer and image bindings start.
    uint32_t mStorageBlockBindingsOffset;
    uint32_t mAtomicCounterBufferBindingsOffset;
    uint32_t mImageBindingsOffset;

    // Store descriptor pools here. We store the descriptors in the Program to facilitate descriptor
    // cache management. It can also allow fewer descriptors for shaders which use fewer
    // textures/buffers.
    vk::DescriptorSetLayoutArray<vk::DynamicDescriptorPool> mDynamicDescriptorPools;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_PROGRAMVK_H_
