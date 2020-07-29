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

    return mExecutable.createPipelineLayout(glContext);
}

size_t ProgramPipelineVk::calcUniformUpdateRequiredSpace(
    ContextVk *contextVk,
    const gl::ProgramExecutable &glExecutable,
    const gl::State &glState,
    gl::ShaderMap<VkDeviceSize> *uniformOffsets) const
{
    size_t requiredSpace = 0;
    for (const gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
    {
        ProgramVk *programVk = getShaderProgram(glState, shaderType);
        ASSERT(programVk);
        if (programVk->isShaderUniformDirty(shaderType))
        {
            (*uniformOffsets)[shaderType] = requiredSpace;
            requiredSpace += programVk->getDefaultUniformAlignedSize(contextVk, shaderType);
        }
    }
    return requiredSpace;
}

angle::Result ProgramPipelineVk::updateUniforms(ContextVk *contextVk)
{
    const gl::State &glState                  = contextVk->getState();
    const gl::ProgramExecutable &glExecutable = *glState.getProgramExecutable();
    vk::DynamicBuffer *defaultUniformStorage  = contextVk->getDefaultUniformStorage();
    uint8_t *bufferData                       = nullptr;
    VkDeviceSize bufferOffset                 = 0;
    uint32_t offsetIndex                      = 0;
    bool anyNewBufferAllocated                = false;
    gl::ShaderMap<VkDeviceSize> offsets;  // offset to the beginning of bufferData
    size_t requiredSpace;

    // We usually only update uniform data for shader stages that are actually dirty. But when the
    // buffer for uniform data have switched, because all shader stages are using the same buffer,
    // we then must update uniform data for all shader stages to keep all shader stages' unform data
    // in the same buffer.
    requiredSpace = calcUniformUpdateRequiredSpace(contextVk, glExecutable, glState, &offsets);
    ASSERT(requiredSpace > 0);

    // Allocate space from dynamicBuffer. Always try to allocate from the current buffer first.
    // If that failed, we deal with fall out and try again.
    if (!defaultUniformStorage->allocateFromCurrentBuffer(requiredSpace, &bufferData,
                                                          &bufferOffset))
    {
        setAllDefaultUniformsDirty(contextVk->getState());

        requiredSpace = calcUniformUpdateRequiredSpace(contextVk, glExecutable, glState, &offsets);
        ANGLE_TRY(defaultUniformStorage->allocate(contextVk, requiredSpace, &bufferData, nullptr,
                                                  &bufferOffset, &anyNewBufferAllocated));
    }

    for (const gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
    {
        ProgramVk *programVk = getShaderProgram(glState, shaderType);
        ASSERT(programVk);
        if (programVk->isShaderUniformDirty(shaderType))
        {
            const angle::MemoryBuffer &uniformData =
                programVk->getDefaultUniformBlocks()[shaderType].uniformData;
            memcpy(&bufferData[offsets[shaderType]], uniformData.data(), uniformData.size());
            mExecutable.mDynamicBufferOffsets[offsetIndex] =
                static_cast<uint32_t>(bufferOffset + offsets[shaderType]);
            programVk->clearShaderUniformDirtyBit(shaderType);
        }
        ++offsetIndex;
    }
    ANGLE_TRY(defaultUniformStorage->flush(contextVk));

    // Because the uniform buffers are per context, we can't rely on dynamicBuffer's allocate
    // function to tell us if you have got a new buffer or not. Other program's use of the buffer
    // might already pushed dynamicBuffer to a new buffer. We record which buffer (represented by
    // the unique BufferSerial number) we were using with the current descriptor set and then we
    // use that recorded BufferSerial compare to the current uniform buffer to quickly detect if
    // there is a buffer switch or not. We need to retrieve from the descriptor set cache or
    // allocate a new descriptor set whenever there is uniform buffer switch.
    vk::BufferHelper *defaultUniformBuffer = defaultUniformStorage->getCurrentBuffer();
    if (mExecutable.getCurrentDefaultUniformBufferSerial() !=
        defaultUniformBuffer->getBufferSerial())
    {
        // We need to reinitialize the descriptor sets if we newly allocated buffers since we can't
        // modify the descriptor sets once initialized.
        vk::UniformsAndXfbDesc defaultUniformsDesc;
        vk::UniformsAndXfbDesc *uniformsAndXfbBufferDesc;

        if (glExecutable.hasTransformFeedbackOutput())
        {
            TransformFeedbackVk *transformFeedbackVk =
                vk::GetImpl(glState.getCurrentTransformFeedback());
            uniformsAndXfbBufferDesc = &transformFeedbackVk->getTransformFeedbackDesc();
            uniformsAndXfbBufferDesc->updateDefaultUniformBuffer(
                defaultUniformBuffer->getBufferSerial());
        }
        else
        {
            defaultUniformsDesc.updateDefaultUniformBuffer(defaultUniformBuffer->getBufferSerial());
            uniformsAndXfbBufferDesc = &defaultUniformsDesc;
        }

        bool newDescriptorSetAllocated;
        ANGLE_TRY(mExecutable.allocUniformAndXfbDescriptorSet(contextVk, *uniformsAndXfbBufferDesc,
                                                              &newDescriptorSetAllocated));

        if (newDescriptorSetAllocated)
        {
            for (const gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
            {
                ProgramVk *programVk = getShaderProgram(glState, shaderType);
                mExecutable.updateDefaultUniformsDescriptorSet(
                    shaderType, programVk->getDefaultUniformBlocks()[shaderType],
                    defaultUniformBuffer, contextVk);
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

void ProgramPipelineVk::setAllDefaultUniformsDirty(const gl::State &glState)
{
    const gl::ProgramExecutable &glExecutable = *glState.getProgramExecutable();

    for (const gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
    {
        ProgramVk *programVk = getShaderProgram(glState, shaderType);
        ASSERT(programVk);
        programVk->setShaderUniformDirtyBit(shaderType);
    }
}

void ProgramPipelineVk::onProgramBind(ContextVk *contextVk)
{
    setAllDefaultUniformsDirty(contextVk->getState());
}

}  // namespace rx
