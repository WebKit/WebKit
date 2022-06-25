//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramPipelineVk.cpp:
//    Implements the class methods for ProgramPipelineVk.
//

#include "libANGLE/renderer/vulkan/ProgramPipelineVk.h"

#include "libANGLE/renderer/glslang_wrapper_utils.h"
#include "libANGLE/renderer/vulkan/GlslangWrapperVk.h"

namespace rx
{

ProgramPipelineVk::ProgramPipelineVk(const gl::ProgramPipelineState &state)
    : ProgramPipelineImpl(state)
{}

ProgramPipelineVk::~ProgramPipelineVk() {}

void ProgramPipelineVk::destroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);
    reset(contextVk);
}

void ProgramPipelineVk::reset(ContextVk *contextVk)
{
    mExecutable.reset(contextVk);
}

angle::Result ProgramPipelineVk::link(const gl::Context *glContext,
                                      const gl::ProgramMergedVaryings &mergedVaryings,
                                      const gl::ProgramVaryingPacking &varyingPacking)
{
    ContextVk *contextVk                      = vk::GetImpl(glContext);
    const gl::ProgramExecutable &glExecutable = mState.getExecutable();
    GlslangSourceOptions options =
        GlslangWrapperVk::CreateSourceOptions(contextVk->getRenderer()->getFeatures());
    GlslangProgramInterfaceInfo glslangProgramInterfaceInfo;
    GlslangWrapperVk::ResetGlslangProgramInterfaceInfo(&glslangProgramInterfaceInfo);

    mExecutable.clearVariableInfoMap();

    // Now that the program pipeline has all of the programs attached, the various descriptor
    // set/binding locations need to be re-assigned to their correct values.
    const gl::ShaderType linkedTransformFeedbackStage =
        glExecutable.getLinkedTransformFeedbackStage();

    // This should be done before assigning varying locations. Otherwise, we can encounter shader
    // interface mismatching problems when the transform feedback stage is not the vertex stage.
    if (options.supportsTransformFeedbackExtension)
    {
        for (const gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
        {
            const gl::Program *glProgram = mState.getShaderProgram(shaderType);
            if (glProgram && gl::ShaderTypeSupportsTransformFeedback(shaderType))
            {
                const bool isTransformFeedbackStage =
                    shaderType == linkedTransformFeedbackStage &&
                    !glProgram->getState().getLinkedTransformFeedbackVaryings().empty();

                GlslangAssignTransformFeedbackLocations(
                    shaderType, glProgram->getExecutable(), isTransformFeedbackStage,
                    &glslangProgramInterfaceInfo, &mExecutable.mVariableInfoMap);
            }
        }
    }

    mExecutable.mOriginalShaderInfo.clear();

    gl::ShaderType frontShaderType = gl::ShaderType::InvalidEnum;
    UniformBindingIndexMap uniformBindingIndexMap;
    for (const gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
    {
        const bool isTransformFeedbackStage =
            shaderType == linkedTransformFeedbackStage &&
            !glExecutable.getLinkedTransformFeedbackVaryings().empty();

        GlslangAssignLocations(options, glExecutable, varyingPacking, shaderType, frontShaderType,
                               isTransformFeedbackStage, &glslangProgramInterfaceInfo,
                               &uniformBindingIndexMap, &mExecutable.mVariableInfoMap);
        frontShaderType = shaderType;

        const gl::Program *program               = mState.getShaderProgram(shaderType);
        ProgramVk *programVk                     = vk::GetImpl(program);
        ProgramExecutableVk &programExecutableVk = programVk->getExecutable();
        mExecutable.mDefaultUniformBlocks[shaderType] =
            programExecutableVk.getSharedDefaultUniformBlock(shaderType);

        mExecutable.mOriginalShaderInfo.initShaderFromProgram(
            shaderType, programExecutableVk.mOriginalShaderInfo);
    }

    mExecutable.setAllDefaultUniformsDirty(glExecutable);

    if (contextVk->getFeatures().enablePrecisionQualifiers.enabled)
    {
        mExecutable.resolvePrecisionMismatch(mergedVaryings);
    }

    return mExecutable.createPipelineLayout(contextVk, mState.getExecutable(), nullptr);
}  // namespace rx

void ProgramPipelineVk::onProgramUniformUpdate(gl::ShaderType shaderType)
{
    mExecutable.mDefaultUniformBlocksDirty.set(shaderType);
}
}  // namespace rx
