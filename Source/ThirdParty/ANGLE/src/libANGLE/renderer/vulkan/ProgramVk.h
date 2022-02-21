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

    void fillProgramStateMap(gl::ShaderMap<const gl::ProgramState *> *programStatesOut);

    std::unique_ptr<LinkEvent> link(const gl::Context *context,
                                    const gl::ProgramLinkedResources &resources,
                                    gl::InfoLog &infoLog,
                                    const gl::ProgramMergedVaryings &mergedVaryings) override;
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

    void setAllDefaultUniformsDirty();
    bool hasDirtyUniforms() const { return mDefaultUniformBlocksDirty.any(); }
    bool isShaderUniformDirty(gl::ShaderType shaderType) const
    {
        return mDefaultUniformBlocksDirty[shaderType];
    }
    void setShaderUniformDirtyBit(gl::ShaderType shaderType)
    {
        if (!mDefaultUniformBlocks[shaderType].uniformData.empty())
        {
            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }
    void clearShaderUniformDirtyBit(gl::ShaderType shaderType)
    {
        mDefaultUniformBlocksDirty.reset(shaderType);
    }
    void onProgramBind();

    const ProgramExecutableVk &getExecutable() const { return mExecutable; }
    ProgramExecutableVk &getExecutable() { return mExecutable; }

    gl::ShaderMap<DefaultUniformBlock> &getDefaultUniformBlocks() { return mDefaultUniformBlocks; }
    size_t getDefaultUniformAlignedSize(ContextVk *contextVk, const gl::ShaderType shaderType) const
    {
        RendererVk *renderer = contextVk->getRenderer();
        size_t alignment     = static_cast<size_t>(
            renderer->getPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment);
        return roundUp(mDefaultUniformBlocks[shaderType].uniformData.size(), alignment);
    }

    size_t calcUniformUpdateRequiredSpace(ContextVk *contextVk,
                                          const gl::ProgramExecutable &glExecutable,
                                          gl::ShaderMap<VkDeviceSize> &uniformOffsets) const;

    ANGLE_INLINE angle::Result initGraphicsShaderProgram(
        ContextVk *contextVk,
        const gl::ShaderType shaderType,
        bool isLastPreFragmentStage,
        ProgramTransformOptions optionBits,
        ProgramInfo *programInfo,
        const ShaderInterfaceVariableInfoMap &variableInfoMap)
    {
        return initProgram(contextVk, shaderType, isLastPreFragmentStage, optionBits, programInfo,
                           variableInfoMap);
    }

    ANGLE_INLINE angle::Result initComputeProgram(
        ContextVk *contextVk,
        ProgramInfo *programInfo,
        const ShaderInterfaceVariableInfoMap &variableInfoMap)
    {
        ProgramTransformOptions optionBits = {};
        return initProgram(contextVk, gl::ShaderType::Compute, false, optionBits, programInfo,
                           variableInfoMap);
    }

    const GlslangProgramInterfaceInfo &getGlslangProgramInterfaceInfo()
    {
        return mGlslangProgramInterfaceInfo;
    }

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
    void linkResources(const gl::ProgramLinkedResources &resources);

    ANGLE_INLINE angle::Result initProgram(ContextVk *contextVk,
                                           const gl::ShaderType shaderType,
                                           bool isLastPreFragmentStage,
                                           ProgramTransformOptions optionBits,
                                           ProgramInfo *programInfo,
                                           const ShaderInterfaceVariableInfoMap &variableInfoMap)
    {
        ASSERT(mOriginalShaderInfo.valid());

        // Create the program pipeline.  This is done lazily and once per combination of
        // specialization constants.
        if (!programInfo->valid(shaderType))
        {
            const bool isTransformFeedbackProgram =
                !mState.getLinkedTransformFeedbackVaryings().empty();
            ANGLE_TRY(programInfo->initProgram(contextVk, shaderType, isLastPreFragmentStage,
                                               isTransformFeedbackProgram, mOriginalShaderInfo,
                                               optionBits, variableInfoMap));
        }
        ASSERT(programInfo->valid(shaderType));

        return angle::Result::Continue;
    }

    gl::ShaderMap<DefaultUniformBlock> mDefaultUniformBlocks;
    gl::ShaderBitSet mDefaultUniformBlocksDirty;

    // We keep the SPIR-V code to use for draw call pipeline creation.
    ShaderInfo mOriginalShaderInfo;

    GlslangProgramInterfaceInfo mGlslangProgramInterfaceInfo;

    ProgramExecutableVk mExecutable;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_PROGRAMVK_H_
