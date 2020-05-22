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
{
    mExecutable.setProgramPipeline(this);
}

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

// TODO: http://anglebug.com/3570: Move/Copy all of the necessary information into
// the ProgramExecutable, so this function can be removed.
void ProgramPipelineVk::fillProgramStateMap(
    const ContextVk *contextVk,
    gl::ShaderMap<const gl::ProgramState *> *programStatesOut)
{
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        (*programStatesOut)[shaderType] = nullptr;

        ProgramVk *programVk = getShaderProgram(contextVk->getState(), shaderType);
        if (programVk)
        {
            (*programStatesOut)[shaderType] = &programVk->getState();
        }
    }
}

angle::Result ProgramPipelineVk::link(const gl::Context *glContext)
{
    ContextVk *contextVk                  = vk::GetImpl(glContext);
    const gl::State &glState              = glContext->getState();
    const gl::ProgramPipeline *glPipeline = glState.getProgramPipeline();
    GlslangSourceOptions options =
        GlslangWrapperVk::CreateSourceOptions(contextVk->getRenderer()->getFeatures());
    GlslangProgramInterfaceInfo glslangProgramInterfaceInfo;
    GlslangWrapperVk::ResetGlslangProgramInterfaceInfo(&glslangProgramInterfaceInfo);

    mExecutable.clearVariableInfoMap();

    // Now that the program pipeline has all of the programs attached, the various descriptor
    // set/binding locations need to be re-assigned to their correct values.
    for (const gl::ShaderType shaderType : glPipeline->getExecutable().getLinkedShaderStages())
    {
        gl::Program *glProgram =
            const_cast<gl::Program *>(glPipeline->getShaderProgram(shaderType));
        if (glProgram)
        {
            // The program interface info must survive across shaders, except
            // for some program-specific values.
            ProgramVk *programVk = vk::GetImpl(glProgram);
            GlslangProgramInterfaceInfo &programProgramInterfaceInfo =
                programVk->getGlslangProgramInterfaceInfo();
            glslangProgramInterfaceInfo.locationsUsedForXfbExtension =
                programProgramInterfaceInfo.locationsUsedForXfbExtension;

            GlslangAssignLocations(options, glProgram->getState().getExecutable(), shaderType,
                                   &glslangProgramInterfaceInfo,
                                   &mExecutable.getShaderInterfaceVariableInfoMap());
        }
    }

    ANGLE_TRY(transformShaderSpirV(glContext));

    return mExecutable.createPipelineLayout(glContext);
}

angle::Result ProgramPipelineVk::transformShaderSpirV(const gl::Context *glContext)
{
    ContextVk *contextVk                    = vk::GetImpl(glContext);
    const gl::ProgramExecutable *executable = contextVk->getState().getProgramExecutable();
    ASSERT(executable);

    for (const gl::ShaderType shaderType : executable->getLinkedShaderStages())
    {
        ProgramVk *programVk = getShaderProgram(contextVk->getState(), shaderType);
        if (programVk)
        {
            ShaderInterfaceVariableInfoMap &variableInfoMap =
                mExecutable.mVariableInfoMap[shaderType];
            std::vector<uint32_t> transformedSpirvBlob;

            // We skip early fragment tests optimization modification here since we need to keep
            // original spriv blob here.
            ANGLE_TRY(GlslangWrapperVk::TransformSpirV(
                contextVk, shaderType, false, variableInfoMap,
                programVk->getShaderInfo().getSpirvBlobs()[shaderType], &transformedSpirvBlob));

            // Save the newly transformed SPIR-V
            // TODO: http://anglebug.com/4513: Keep the original SPIR-V and
            // translated SPIR-V in separate buffers in ShaderInfo to avoid the
            // extra copy here.
            programVk->getShaderInfo().getSpirvBlobs()[shaderType] = transformedSpirvBlob;
        }
    }
    return angle::Result::Continue;
}

angle::Result ProgramPipelineVk::updateUniforms(ContextVk *contextVk)
{
    uint32_t offsetIndex                      = 0;
    bool anyNewBufferAllocated                = false;
    const gl::ProgramExecutable *glExecutable = contextVk->getState().getProgramExecutable();

    for (const gl::ShaderType shaderType : glExecutable->getLinkedShaderStages())
    {
        ProgramVk *programVk = getShaderProgram(contextVk->getState(), shaderType);
        if (programVk && programVk->dirtyUniforms())
        {
            ANGLE_TRY(programVk->updateShaderUniforms(
                contextVk, shaderType, &mExecutable.mDynamicBufferOffsets[offsetIndex],
                &anyNewBufferAllocated));
        }
        ++offsetIndex;
    }

    // The PPO's list of descriptor sets being empty without a new buffer being allocated indicates
    // a Program that was already used in a draw command (and thus already allocated uniform
    // buffers) has been bound to this PPO.
    if (anyNewBufferAllocated || mExecutable.mDescriptorSets.empty())
    {
        // We need to reinitialize the descriptor sets if we newly allocated buffers since we can't
        // modify the descriptor sets once initialized.
        ANGLE_TRY(mExecutable.allocateDescriptorSet(contextVk, kUniformsAndXfbDescriptorSetIndex));

        mExecutable.mDescriptorBuffersCache.clear();
        for (const gl::ShaderType shaderType : glExecutable->getLinkedShaderStages())
        {
            ProgramVk *programVk = getShaderProgram(contextVk->getState(), shaderType);
            if (programVk)
            {
                mExecutable.updateDefaultUniformsDescriptorSet(
                    shaderType, programVk->getDefaultUniformBlocks(), contextVk);
                mExecutable.updateTransformFeedbackDescriptorSetImpl(programVk->getState(),
                                                                     contextVk);
            }
        }
    }

    return angle::Result::Continue;
}

bool ProgramPipelineVk::dirtyUniforms(const gl::State &glState)
{
    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const ProgramVk *program = getShaderProgram(glState, shaderType);
        if (program && program->dirtyUniforms())
        {
            return true;
        }
    }

    return false;
}

}  // namespace rx
