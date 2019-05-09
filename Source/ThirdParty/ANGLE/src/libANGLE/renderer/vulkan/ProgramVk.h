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
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace rx
{
ANGLE_INLINE bool UseLineRaster(const ContextVk *contextVk, gl::PrimitiveMode mode)
{
    return contextVk->getFeatures().basicGLLineRasterization && gl::IsLineMode(mode);
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
    angle::Result updateTexturesDescriptorSet(ContextVk *contextVk,
                                              vk::FramebufferHelper *framebuffer);

    angle::Result updateDescriptorSets(ContextVk *contextVk, vk::CommandBuffer *commandBuffer);

    // For testing only.
    void setDefaultUniformBlocksMinSizeForTesting(size_t minSize);

    const vk::PipelineLayout &getPipelineLayout() const { return mPipelineLayout.get(); }

    bool hasTextures() const { return !mState.getSamplerBindings().empty(); }

    bool dirtyUniforms() const { return mDefaultUniformBlocksDirty.any(); }

    angle::Result getGraphicsPipeline(ContextVk *contextVk,
                                      gl::PrimitiveMode mode,
                                      const vk::GraphicsPipelineDesc &desc,
                                      const gl::AttributesMask &activeAttribLocations,
                                      const vk::GraphicsPipelineDesc **descPtrOut,
                                      vk::PipelineHelper **pipelineOut)
    {
        vk::ShaderProgramHelper *shaderProgram;
        ANGLE_TRY(initShaders(contextVk, mode, &shaderProgram));
        ASSERT(shaderProgram->isGraphicsProgram());
        RendererVk *renderer = contextVk->getRenderer();
        return shaderProgram->getGraphicsPipeline(
            contextVk, &renderer->getRenderPassCache(), renderer->getPipelineCache(),
            renderer->getCurrentQueueSerial(), mPipelineLayout.get(), desc, activeAttribLocations,
            descPtrOut, pipelineOut);
    }

  private:
    template <int cols, int rows>
    void setUniformMatrixfv(GLint location,
                            GLsizei count,
                            GLboolean transpose,
                            const GLfloat *value);

    void reset(RendererVk *renderer);
    angle::Result allocateDescriptorSet(ContextVk *contextVk, uint32_t descriptorSetIndex);
    angle::Result initDefaultUniformBlocks(const gl::Context *glContext);

    angle::Result updateDefaultUniformsDescriptorSet(ContextVk *contextVk);

    template <class T>
    void getUniformImpl(GLint location, T *v, GLenum entryPointType) const;

    template <typename T>
    void setUniformImpl(GLint location, GLsizei count, const T *v, GLenum entryPointType);
    angle::Result linkImpl(const gl::Context *glContext,
                           const gl::ProgramLinkedResources &resources,
                           gl::InfoLog &infoLog);

    ANGLE_INLINE angle::Result initShaders(ContextVk *contextVk,
                                           gl::PrimitiveMode mode,
                                           vk::ShaderProgramHelper **shaderProgramOut)
    {
        if (UseLineRaster(contextVk, mode))
        {
            if (!mLineRasterShaderInfo.valid())
            {
                ANGLE_TRY(mLineRasterShaderInfo.initShaders(contextVk, mVertexSource,
                                                            mFragmentSource, true));
            }

            ASSERT(mLineRasterShaderInfo.valid());
            *shaderProgramOut = &mLineRasterShaderInfo.getShaderProgram();
        }
        else
        {
            if (!mDefaultShaderInfo.valid())
            {
                ANGLE_TRY(mDefaultShaderInfo.initShaders(contextVk, mVertexSource, mFragmentSource,
                                                         false));
            }

            ASSERT(mDefaultShaderInfo.valid());
            *shaderProgramOut = &mDefaultShaderInfo.getShaderProgram();
        }

        return angle::Result::Continue;
    }

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
    gl::ShaderMap<uint32_t> mUniformBlocksOffsets;

    // This is a special "empty" placeholder buffer for when a shader has no uniforms.
    // It is necessary because we want to keep a compatible pipeline layout in all cases,
    // and Vulkan does not tolerate having null handles in a descriptor set.
    vk::BufferHelper mEmptyUniformBlockStorage;

    // Descriptor sets for uniform blocks and textures for this program.
    std::vector<VkDescriptorSet> mDescriptorSets;
    gl::RangeUI mUsedDescriptorSetRange;

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
                                  const std::string &vertexSource,
                                  const std::string &fragmentSource,
                                  bool enableLineRasterEmulation);
        void release(RendererVk *renderer);

        ANGLE_INLINE bool valid() const { return mShaders[gl::ShaderType::Vertex].get().valid(); }

        vk::ShaderProgramHelper &getShaderProgram() { return mProgramHelper; }

      private:
        vk::ShaderProgramHelper mProgramHelper;
        gl::ShaderMap<vk::RefCounted<vk::ShaderAndSerial>> mShaders;
    };

    ShaderInfo mDefaultShaderInfo;
    ShaderInfo mLineRasterShaderInfo;

    // We keep the translated linked shader sources to use with shader draw call patching.
    std::string mVertexSource;
    std::string mFragmentSource;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_PROGRAMVK_H_
