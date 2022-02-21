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
    gl::ShaderMap<const gl::ProgramState *> *programStatesOut)
{
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        (*programStatesOut)[shaderType] = nullptr;

        ProgramVk *programVk = getShaderProgram(shaderType);
        if (programVk)
        {
            (*programStatesOut)[shaderType] = &programVk->getState();
        }
    }
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
                    shaderType, glProgram->getState(), isTransformFeedbackStage,
                    &glslangProgramInterfaceInfo, &mExecutable.mVariableInfoMap);
            }
        }
    }

    gl::ShaderType frontShaderType = gl::ShaderType::InvalidEnum;
    UniformBindingIndexMap uniformBindingIndexMap;
    for (const gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
    {
        const gl::Program *glProgram = mState.getShaderProgram(shaderType);
        if (glProgram)
        {
            const bool isTransformFeedbackStage =
                shaderType == linkedTransformFeedbackStage &&
                !glProgram->getState().getLinkedTransformFeedbackVaryings().empty();

            GlslangAssignLocations(options, glProgram->getState(), varyingPacking, shaderType,
                                   frontShaderType, isTransformFeedbackStage,
                                   &glslangProgramInterfaceInfo, &uniformBindingIndexMap,
                                   &mExecutable.mVariableInfoMap);
            frontShaderType = shaderType;
        }
    }

    if (contextVk->getFeatures().enablePrecisionQualifiers.enabled)
    {
        mExecutable.resolvePrecisionMismatch(mergedVaryings);
    }

    return mExecutable.createPipelineLayout(contextVk, mState.getExecutable(), nullptr);
}

size_t ProgramPipelineVk::calcUniformUpdateRequiredSpace(
    ContextVk *contextVk,
    gl::ShaderMap<VkDeviceSize> *uniformOffsets) const
{
    size_t requiredSpace = 0;
    for (const gl::ShaderType shaderType : mState.getExecutable().getLinkedShaderStages())
    {
        ProgramVk *programVk = getShaderProgram(shaderType);
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
    const gl::ProgramExecutable &glExecutable = mState.getExecutable();
    vk::DynamicBuffer *defaultUniformStorage  = contextVk->getDefaultUniformStorage();
    uint8_t *bufferData                       = nullptr;
    VkDeviceSize bufferOffset                 = 0;
    uint32_t offsetIndex                      = 0;
    bool anyNewBufferAllocated                = false;
    gl::ShaderMap<VkDeviceSize> offsets;  // offset to the beginning of bufferData
    size_t requiredSpace;

    // We usually only update uniform data for shader stages that are actually dirty. But when the
    // buffer for uniform data have switched, because all shader stages are using the same buffer,
    // we then must update uniform data for all shader stages to keep all shader stages' uniform
    // data in the same buffer.
    requiredSpace = calcUniformUpdateRequiredSpace(contextVk, &offsets);
    ASSERT(requiredSpace > 0);

    // Allocate space from dynamicBuffer. Always try to allocate from the current buffer first.
    // If that failed, we deal with fall out and try again.
    if (!defaultUniformStorage->allocateFromCurrentBuffer(requiredSpace, &bufferData,
                                                          &bufferOffset))
    {
        setAllDefaultUniformsDirty();

        requiredSpace = calcUniformUpdateRequiredSpace(contextVk, &offsets);
        ANGLE_TRY(defaultUniformStorage->allocate(contextVk, requiredSpace, &bufferData, nullptr,
                                                  &bufferOffset, &anyNewBufferAllocated));
    }

    for (const gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
    {
        ProgramVk *programVk = getShaderProgram(shaderType);
        ASSERT(programVk);
        if (programVk->isShaderUniformDirty(shaderType))
        {
            const angle::MemoryBuffer &uniformData =
                programVk->getDefaultUniformBlocks()[shaderType].uniformData;
            memcpy(&bufferData[offsets[shaderType]], uniformData.data(), uniformData.size());
            mExecutable.mDynamicUniformDescriptorOffsets[offsetIndex] =
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
        vk::UniformsAndXfbDescriptorDesc defaultUniformsDesc;
        vk::UniformsAndXfbDescriptorDesc *uniformsAndXfbBufferDesc;

        if (glExecutable.hasTransformFeedbackOutput())
        {
            TransformFeedbackVk *transformFeedbackVk =
                vk::GetImpl(contextVk->getState().getCurrentTransformFeedback());
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
                ProgramVk *programVk = getShaderProgram(shaderType);
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

bool ProgramPipelineVk::hasDirtyUniforms() const
{
    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const ProgramVk *program = getShaderProgram(shaderType);
        if (program && program->hasDirtyUniforms())
        {
            return true;
        }
    }

    return false;
}

void ProgramPipelineVk::setAllDefaultUniformsDirty()
{
    const gl::ProgramExecutable &glExecutable = mState.getExecutable();

    for (const gl::ShaderType shaderType : glExecutable.getLinkedShaderStages())
    {
        ProgramVk *programVk = getShaderProgram(shaderType);
        ASSERT(programVk);
        programVk->setShaderUniformDirtyBit(shaderType);
    }
}

void ProgramPipelineVk::onProgramBind()
{
    setAllDefaultUniformsDirty();
}

}  // namespace rx
