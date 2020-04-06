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
#include "libANGLE/renderer/vulkan/ProgramExecutableVk.h"
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

    angle::Result updateUniforms(ContextVk *contextVk);

    // For testing only.
    void setDefaultUniformBlocksMinSizeForTesting(size_t minSize);

    const vk::PipelineLayout &getPipelineLayout() const
    {
        return mExecutable.mPipelineLayout.get();
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
            contextVk->getCurrentQueueSerial(), mExecutable.mPipelineLayout.get(), desc,
            activeAttribLocations, mState.getProgramExecutable().getAttributesTypeMask(),
            descPtrOut, pipelineOut);
    }

    angle::Result getComputePipeline(ContextVk *contextVk, vk::PipelineAndSerial **pipelineOut)
    {
        vk::ShaderProgramHelper *shaderProgram;
        ANGLE_TRY(initComputeProgram(contextVk, &shaderProgram));
        ASSERT(!shaderProgram->isGraphicsProgram());
        return shaderProgram->getComputePipeline(contextVk, mExecutable.mPipelineLayout.get(),
                                                 pipelineOut);
    }

    // Used in testing only.
    vk::DynamicDescriptorPool *getDynamicDescriptorPool(uint32_t poolIndex)
    {
        return &mExecutable.mDynamicDescriptorPools[poolIndex];
    }

    const ProgramExecutableVk &getExecutable() const { return mExecutable; }
    ProgramExecutableVk &getExecutable() { return mExecutable; }

    gl::ShaderMap<DefaultUniformBlock> &getDefaultUniformBlocks() { return mDefaultUniformBlocks; }

  private:
    template <int cols, int rows>
    void setUniformMatrixfv(GLint location,
                            GLsizei count,
                            GLboolean transpose,
                            const GLfloat *value);

    void reset(ContextVk *contextVk);
    angle::Result initDefaultUniformBlocks(const gl::Context *glContext);
    void generateUniformLayoutMapping(gl::ShaderMap<sh::BlockLayoutMap> &layoutMap,
                                      gl::ShaderMap<size_t> &requiredBufferSize);
    void initDefaultUniformLayoutMapping(gl::ShaderMap<sh::BlockLayoutMap> &layoutMap);
    angle::Result resizeUniformBlockMemory(ContextVk *contextVk,
                                           gl::ShaderMap<size_t> &requiredBufferSize);

    template <class T>
    void getUniformImpl(GLint location, T *v, GLenum entryPointType) const;

    template <typename T>
    void setUniformImpl(GLint location, GLsizei count, const T *v, GLenum entryPointType);
    angle::Result linkImpl(const gl::Context *glContext, gl::InfoLog &infoLog);
    void linkResources(const gl::ProgramLinkedResources &resources);

    ANGLE_INLINE angle::Result initProgram(ContextVk *contextVk,
                                           bool enableLineRasterEmulation,
                                           ProgramInfo *programInfo,
                                           vk::ShaderProgramHelper **shaderProgramOut)
    {
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

        ProgramInfo &programInfo = enableLineRasterEmulation ? mExecutable.mLineRasterProgramInfo
                                                             : mExecutable.mDefaultProgramInfo;

        return initProgram(contextVk, enableLineRasterEmulation, &programInfo, shaderProgramOut);
    }

    ANGLE_INLINE angle::Result initComputeProgram(ContextVk *contextVk,
                                                  vk::ShaderProgramHelper **shaderProgramOut)
    {
        return initProgram(contextVk, false, &mExecutable.mDefaultProgramInfo, shaderProgramOut);
    }

    gl::ShaderMap<DefaultUniformBlock> mDefaultUniformBlocks;
    gl::ShaderBitSet mDefaultUniformBlocksDirty;

    // We keep the SPIR-V code to use for draw call pipeline creation.
    ShaderInfo mShaderInfo;

    GlslangProgramInterfaceInfo mGlslangProgramInterfaceInfo;

    ProgramExecutableVk mExecutable;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_PROGRAMVK_H_
