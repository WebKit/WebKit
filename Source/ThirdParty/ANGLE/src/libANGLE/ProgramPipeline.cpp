//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramPipeline.cpp: Implements the gl::ProgramPipeline class.
// Implements GL program pipeline objects and related functionality.
// [OpenGL ES 3.1] section 7.4 page 105.

#include "libANGLE/ProgramPipeline.h"

#include <algorithm>

#include "libANGLE/Context.h"
#include "libANGLE/Program.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/GLImplFactory.h"
#include "libANGLE/renderer/ProgramPipelineImpl.h"

namespace gl
{

enum SubjectIndexes : angle::SubjectIndex
{
    kExecutableSubjectIndex = 0
};

ProgramPipelineState::ProgramPipelineState()
    : mLabel(), mActiveShaderProgram(nullptr), mValid(false), mExecutable(new ProgramExecutable())
{
    for (const ShaderType shaderType : gl::AllShaderTypes())
    {
        mPrograms[shaderType] = nullptr;
    }
}

ProgramPipelineState::~ProgramPipelineState()
{
    SafeDelete(mExecutable);
}

const std::string &ProgramPipelineState::getLabel() const
{
    return mLabel;
}

void ProgramPipelineState::activeShaderProgram(Program *shaderProgram)
{
    mActiveShaderProgram = shaderProgram;
}

void ProgramPipelineState::useProgramStage(const Context *context,
                                           const ShaderType shaderType,
                                           Program *shaderProgram,
                                           angle::ObserverBinding *programObserverBindings)
{
    Program *oldProgram = mPrograms[shaderType];
    if (oldProgram)
    {
        oldProgram->release(context);
    }

    // If program refers to a program object with a valid shader attached for the indicated shader
    // stage, glUseProgramStages installs the executable code for that stage in the indicated
    // program pipeline object pipeline.
    if (shaderProgram && (shaderProgram->id().value != 0) &&
        shaderProgram->getExecutable().hasLinkedShaderStage(shaderType))
    {
        mPrograms[shaderType] = shaderProgram;
        shaderProgram->addRef();
    }
    else
    {
        // If program is zero, or refers to a program object with no valid shader executable for the
        // given stage, it is as if the pipeline object has no programmable stage configured for the
        // indicated shader stage.
        mPrograms[shaderType] = nullptr;
    }

    Program *program = mPrograms[shaderType];
    programObserverBindings->bind(program);
}

void ProgramPipelineState::useProgramStages(
    const Context *context,
    GLbitfield stages,
    Program *shaderProgram,
    std::vector<angle::ObserverBinding> *programObserverBindings)
{
    if (stages == GL_ALL_SHADER_BITS)
    {
        for (const ShaderType shaderType : gl::AllShaderTypes())
        {
            size_t index = static_cast<size_t>(shaderType);
            ASSERT(index < programObserverBindings->size());
            useProgramStage(context, shaderType, shaderProgram,
                            &programObserverBindings->at(index));
        }
    }
    else
    {
        if (stages & GL_VERTEX_SHADER_BIT)
        {
            size_t index = static_cast<size_t>(ShaderType::Vertex);
            ASSERT(index < programObserverBindings->size());
            useProgramStage(context, ShaderType::Vertex, shaderProgram,
                            &programObserverBindings->at(index));
        }

        if (stages & GL_FRAGMENT_SHADER_BIT)
        {
            size_t index = static_cast<size_t>(ShaderType::Fragment);
            ASSERT(index < programObserverBindings->size());
            useProgramStage(context, ShaderType::Fragment, shaderProgram,
                            &programObserverBindings->at(index));
        }

        if (stages & GL_COMPUTE_SHADER_BIT)
        {
            size_t index = static_cast<size_t>(ShaderType::Compute);
            ASSERT(index < programObserverBindings->size());
            useProgramStage(context, ShaderType::Compute, shaderProgram,
                            &programObserverBindings->at(index));
        }
    }
}

bool ProgramPipelineState::usesShaderProgram(ShaderProgramID programId) const
{
    for (const Program *program : mPrograms)
    {
        if (program && (program->id() == programId))
        {
            return true;
        }
    }

    return false;
}

void ProgramPipelineState::updateExecutableTextures()
{
    for (const ShaderType shaderType : mExecutable->getLinkedShaderStages())
    {
        const Program *program = getShaderProgram(shaderType);
        ASSERT(program);
        mExecutable->setActiveTextureMask(program->getExecutable().getActiveSamplersMask());
        mExecutable->setActiveImagesMask(program->getExecutable().getActiveImagesMask());
        // Updates mActiveSamplerRefCounts, mActiveSamplerTypes, and mActiveSamplerFormats
        mExecutable->updateActiveSamplers(program->getState());
    }
}

ProgramPipeline::ProgramPipeline(rx::GLImplFactory *factory, ProgramPipelineID handle)
    : RefCountObject(factory->generateSerial(), handle),
      mProgramPipelineImpl(factory->createProgramPipeline(mState)),
      mExecutableObserverBinding(this, kExecutableSubjectIndex)
{
    ASSERT(mProgramPipelineImpl);

    for (const ShaderType shaderType : gl::AllShaderTypes())
    {
        mProgramObserverBindings.emplace_back(this, static_cast<angle::SubjectIndex>(shaderType));
    }
    mExecutableObserverBinding.bind(mState.mExecutable);
}

ProgramPipeline::~ProgramPipeline()
{
    mProgramPipelineImpl.release();
}

void ProgramPipeline::onDestroy(const Context *context)
{
    for (Program *program : mState.mPrograms)
    {
        if (program)
        {
            ASSERT(program->getRefCount());
            program->release(context);
        }
    }

    getImplementation()->destroy(context);
}

void ProgramPipeline::setLabel(const Context *context, const std::string &label)
{
    mState.mLabel = label;
}

const std::string &ProgramPipeline::getLabel() const
{
    return mState.mLabel;
}

rx::ProgramPipelineImpl *ProgramPipeline::getImplementation() const
{
    return mProgramPipelineImpl.get();
}

void ProgramPipeline::activeShaderProgram(Program *shaderProgram)
{
    mState.activeShaderProgram(shaderProgram);
}

void ProgramPipeline::useProgramStages(const Context *context,
                                       GLbitfield stages,
                                       Program *shaderProgram)
{
    mState.useProgramStages(context, stages, shaderProgram, &mProgramObserverBindings);
    updateLinkedShaderStages();
    updateExecutable();

    mDirtyBits.set(DIRTY_BIT_PROGRAM_STAGE);
}

void ProgramPipeline::updateLinkedShaderStages()
{
    mState.mExecutable->resetLinkedShaderStages();

    for (const ShaderType shaderType : gl::AllShaderTypes())
    {
        Program *program = mState.mPrograms[shaderType];
        if (program)
        {
            mState.mExecutable->setLinkedShaderStages(shaderType);
        }
    }

    mState.mExecutable->updateCanDrawWith();
}

void ProgramPipeline::updateExecutableAttributes()
{
    Program *vertexProgram = getShaderProgram(gl::ShaderType::Vertex);

    if (!vertexProgram)
    {
        return;
    }

    const ProgramExecutable &vertexExecutable      = vertexProgram->getExecutable();
    mState.mExecutable->mActiveAttribLocationsMask = vertexExecutable.mActiveAttribLocationsMask;
    mState.mExecutable->mMaxActiveAttribLocation   = vertexExecutable.mMaxActiveAttribLocation;
    mState.mExecutable->mAttributesTypeMask        = vertexExecutable.mAttributesTypeMask;
    mState.mExecutable->mAttributesMask            = vertexExecutable.mAttributesMask;
}

void ProgramPipeline::updateTransformFeedbackMembers()
{
    Program *vertexProgram = getShaderProgram(gl::ShaderType::Vertex);

    if (!vertexProgram)
    {
        return;
    }

    const ProgramExecutable &vertexExecutable     = vertexProgram->getExecutable();
    mState.mExecutable->mTransformFeedbackStrides = vertexExecutable.mTransformFeedbackStrides;
    mState.mExecutable->mLinkedTransformFeedbackVaryings =
        vertexExecutable.mLinkedTransformFeedbackVaryings;
}

void ProgramPipeline::updateHasBooleans()
{
    // Need to check all of the shader stages, not just linked, so we handle Compute correctly.
    for (const gl::ShaderType shaderType : kAllGraphicsShaderTypes)
    {
        const Program *shaderProgram = getShaderProgram(shaderType);
        if (shaderProgram)
        {
            const ProgramExecutable &executable = shaderProgram->getExecutable();

            if (executable.hasUniformBuffers())
            {
                mState.mExecutable->mPipelineHasGraphicsUniformBuffers = true;
            }
            if (executable.hasStorageBuffers())
            {
                mState.mExecutable->mPipelineHasGraphicsStorageBuffers = true;
            }
            if (executable.hasAtomicCounterBuffers())
            {
                mState.mExecutable->mPipelineHasGraphicsAtomicCounterBuffers = true;
            }
            if (executable.hasDefaultUniforms())
            {
                mState.mExecutable->mPipelineHasGraphicsDefaultUniforms = true;
            }
            if (executable.hasTextures())
            {
                mState.mExecutable->mPipelineHasGraphicsTextures = true;
            }
            if (executable.hasImages())
            {
                mState.mExecutable->mPipelineHasGraphicsImages = true;
            }
        }
    }

    const Program *computeProgram = getShaderProgram(ShaderType::Compute);
    if (computeProgram)
    {
        const ProgramExecutable &executable = computeProgram->getExecutable();

        if (executable.hasUniformBuffers())
        {
            mState.mExecutable->mPipelineHasComputeUniformBuffers = true;
        }
        if (executable.hasStorageBuffers())
        {
            mState.mExecutable->mPipelineHasComputeStorageBuffers = true;
        }
        if (executable.hasAtomicCounterBuffers())
        {
            mState.mExecutable->mPipelineHasComputeAtomicCounterBuffers = true;
        }
        if (executable.hasDefaultUniforms())
        {
            mState.mExecutable->mPipelineHasComputeDefaultUniforms = true;
        }
        if (executable.hasTextures())
        {
            mState.mExecutable->mPipelineHasComputeTextures = true;
        }
        if (executable.hasImages())
        {
            mState.mExecutable->mPipelineHasComputeImages = true;
        }
    }
}

void ProgramPipeline::updateExecutable()
{
    mState.mExecutable->reset();

    // Vertex Shader ProgramExecutable properties
    updateExecutableAttributes();
    updateTransformFeedbackMembers();

    // All Shader ProgramExecutable properties
    mState.updateExecutableTextures();
    updateHasBooleans();
}

ProgramMergedVaryings ProgramPipeline::getMergedVaryings() const
{
    ASSERT(!mState.mExecutable->isCompute());

    // Varyings are matched between pairs of consecutive stages, by location if assigned or
    // by name otherwise.  Note that it's possible for one stage to specify location and the other
    // not: https://cvs.khronos.org/bugzilla/show_bug.cgi?id=16261

    // Map stages to the previous active stage in the rendering pipeline.  When looking at input
    // varyings of a stage, this is used to find the stage whose output varyings are being linked
    // with them.
    ShaderMap<ShaderType> previousActiveStage;

    // Note that kAllGraphicsShaderTypes is sorted according to the rendering pipeline.
    ShaderType lastActiveStage = ShaderType::InvalidEnum;
    for (ShaderType shaderType : getExecutable().getLinkedShaderStages())
    {
        previousActiveStage[shaderType] = lastActiveStage;

        const Program *program = getShaderProgram(shaderType);
        ASSERT(program);
        lastActiveStage = shaderType;
    }

    // First, go through output varyings and create two maps (one by name, one by location) for
    // faster lookup when matching input varyings.

    ShaderMap<std::map<std::string, size_t>> outputVaryingNameToIndexShaderMap;
    ShaderMap<std::map<int, size_t>> outputVaryingLocationToIndexShaderMap;

    ProgramMergedVaryings merged;

    // Gather output varyings.
    for (ShaderType shaderType : getExecutable().getLinkedShaderStages())
    {
        const Program *program = getShaderProgram(shaderType);
        ASSERT(program);
        Shader *shader = program->getState().getAttachedShader(shaderType);
        ASSERT(shader);

        for (const sh::ShaderVariable &varying : shader->getOutputVaryings())
        {
            merged.push_back({});
            ProgramVaryingRef *ref = &merged.back();

            ref->frontShader      = &varying;
            ref->frontShaderStage = shaderType;

            // Always map by name.  Even if location is provided in this stage, it may not be in the
            // paired stage.
            outputVaryingNameToIndexShaderMap[shaderType][varying.name] = merged.size() - 1;

            // If location is provided, also keep it in a map by location.
            if (varying.location != -1)
            {
                outputVaryingLocationToIndexShaderMap[shaderType][varying.location] =
                    merged.size() - 1;
            }
        }
    }

    // Gather input varyings, and match them with output varyings of the previous stage.
    for (ShaderType shaderType : getExecutable().getLinkedShaderStages())
    {
        const Program *program = getShaderProgram(shaderType);
        ASSERT(program);
        Shader *shader = program->getState().getAttachedShader(shaderType);
        ASSERT(shader);
        ShaderType previousStage = previousActiveStage[shaderType];

        for (const sh::ShaderVariable &varying : shader->getInputVaryings())
        {
            size_t mergedIndex = merged.size();
            if (previousStage != ShaderType::InvalidEnum)
            {
                // If location is provided, see if we can match by location.
                if (varying.location != -1)
                {
                    std::map<int, size_t> outputVaryingLocationToIndex =
                        outputVaryingLocationToIndexShaderMap[previousStage];
                    auto byLocationIter = outputVaryingLocationToIndex.find(varying.location);
                    if (byLocationIter != outputVaryingLocationToIndex.end())
                    {
                        mergedIndex = byLocationIter->second;
                    }
                }

                // If not found, try to match by name.
                if (mergedIndex == merged.size())
                {
                    std::map<std::string, size_t> outputVaryingNameToIndex =
                        outputVaryingNameToIndexShaderMap[previousStage];
                    auto byNameIter = outputVaryingNameToIndex.find(varying.name);
                    if (byNameIter != outputVaryingNameToIndex.end())
                    {
                        mergedIndex = byNameIter->second;
                    }
                }
            }

            // If no previous stage, or not matched by location or name, create a new entry for it.
            if (mergedIndex == merged.size())
            {
                merged.push_back({});
                mergedIndex = merged.size() - 1;
            }

            ProgramVaryingRef *ref = &merged[mergedIndex];

            ref->backShader      = &varying;
            ref->backShaderStage = shaderType;
        }
    }

    return merged;
}

// The attached shaders are checked for linking errors by matching up their variables.
// Uniform, input and output variables get collected.
// The code gets compiled into binaries.
angle::Result ProgramPipeline::link(const Context *context)
{
    if (!getExecutable().isCompute())
    {
        InfoLog &infoLog = mState.mExecutable->getInfoLog();
        infoLog.reset();
        const State &state = context->getState();

        // Map the varyings to the register file
        gl::PackMode packMode = PackMode::ANGLE_RELAXED;
        if (state.getLimitations().noFlexibleVaryingPacking)
        {
            // D3D9 pack mode is strictly more strict than WebGL, so takes priority.
            packMode = PackMode::ANGLE_NON_CONFORMANT_D3D9;
        }
        else if (state.getExtensions().webglCompatibility)
        {
            // In WebGL, we use a slightly different handling for packing variables.
            packMode = PackMode::WEBGL_STRICT;
        }

        if (!linkVaryings(infoLog))
        {
            return angle::Result::Stop;
        }

        gl::ShaderMap<const gl::ProgramState *> programStates;
        fillProgramStateMap(&programStates);
        if (!mState.mExecutable->linkValidateGlobalNames(infoLog, programStates))
        {
            return angle::Result::Stop;
        }

        GLuint maxVaryingVectors =
            static_cast<GLuint>(context->getState().getCaps().maxVaryingVectors);
        VaryingPacking varyingPacking(maxVaryingVectors, packMode);

        const ProgramMergedVaryings &mergedVaryings = getMergedVaryings();
        for (ShaderType shaderType : getExecutable().getLinkedShaderStages())
        {
            Program *program = mState.mPrograms[shaderType];
            ASSERT(program);
            program->getExecutable().getResources().varyingPacking.reset();
            ANGLE_TRY(
                program->linkMergedVaryings(context, program->getExecutable(), mergedVaryings));
        }
    }

    ANGLE_TRY(getImplementation()->link(context));

    return angle::Result::Continue;
}

bool ProgramPipeline::linkVaryings(InfoLog &infoLog) const
{
    ShaderType previousShaderType = ShaderType::InvalidEnum;
    for (ShaderType shaderType : getExecutable().getLinkedShaderStages())
    {
        Program *program = getShaderProgram(shaderType);
        ASSERT(program);
        ProgramExecutable &executable = program->getExecutable();

        if (previousShaderType != ShaderType::InvalidEnum)
        {
            Program *previousProgram = getShaderProgram(previousShaderType);
            ASSERT(previousProgram);
            ProgramExecutable &previousExecutable = previousProgram->getExecutable();

            if (!Program::linkValidateShaderInterfaceMatching(
                    previousExecutable.getLinkedOutputVaryings(previousShaderType),
                    executable.getLinkedInputVaryings(shaderType), previousShaderType, shaderType,
                    previousExecutable.getLinkedShaderVersion(previousShaderType),
                    executable.getLinkedShaderVersion(shaderType), true, infoLog))
            {
                return false;
            }
        }
        previousShaderType = shaderType;
    }

    Program *vertexProgram   = mState.mPrograms[ShaderType::Vertex];
    Program *fragmentProgram = mState.mPrograms[ShaderType::Fragment];
    if (!vertexProgram || !fragmentProgram)
    {
        return false;
    }
    ProgramExecutable &vertexExecutable   = vertexProgram->getExecutable();
    ProgramExecutable &fragmentExecutable = fragmentProgram->getExecutable();
    return Program::linkValidateBuiltInVaryings(
        vertexExecutable.getLinkedOutputVaryings(ShaderType::Vertex),
        fragmentExecutable.getLinkedInputVaryings(ShaderType::Fragment),
        vertexExecutable.getLinkedShaderVersion(ShaderType::Vertex), infoLog);
}

void ProgramPipeline::validate(const gl::Context *context)
{
    const Caps &caps = context->getCaps();
    mState.mValid    = true;
    InfoLog &infoLog = mState.mExecutable->getInfoLog();
    infoLog.reset();

    for (const ShaderType shaderType : mState.mExecutable->getLinkedShaderStages())
    {
        Program *shaderProgram = mState.mPrograms[shaderType];
        if (shaderProgram)
        {
            shaderProgram->resolveLink(context);
            shaderProgram->validate(caps);
            std::string shaderInfoString = shaderProgram->getExecutable().getInfoLogString();
            if (shaderInfoString.length())
            {
                mState.mValid = false;
                infoLog << shaderInfoString << "\n";
                return;
            }
            if (!shaderProgram->isSeparable())
            {
                mState.mValid = false;
                infoLog << GetShaderTypeString(shaderType) << " is not marked separable."
                        << "\n";
                return;
            }
        }
    }

    if (!linkVaryings(infoLog))
    {
        mState.mValid = false;

        for (const ShaderType shaderType : mState.mExecutable->getLinkedShaderStages())
        {
            Program *shaderProgram = mState.mPrograms[shaderType];
            ASSERT(shaderProgram);
            shaderProgram->validate(caps);
            std::string shaderInfoString = shaderProgram->getExecutable().getInfoLogString();
            if (shaderInfoString.length())
            {
                infoLog << shaderInfoString << "\n";
            }
        }
    }
}

bool ProgramPipeline::validateSamplers(InfoLog *infoLog, const Caps &caps)
{
    for (const ShaderType shaderType : gl::AllShaderTypes())
    {
        Program *shaderProgram = mState.mPrograms[shaderType];
        if (shaderProgram && !shaderProgram->validateSamplers(infoLog, caps))
        {
            return false;
        }
    }

    return true;
}

angle::Result ProgramPipeline::syncState(const Context *context)
{
    if (mDirtyBits.any())
    {
        mDirtyBits.reset();

        // If there's a Program bound, we still want to link the PPO so we don't
        // lose the dirty bit, but, we don't want to signal any errors if it fails
        // since the failure would be unrelated to drawing with the Program.
        bool goodResult = link(context) == angle::Result::Continue;
        if (!context->getState().getProgram())
        {
            ANGLE_CHECK(const_cast<Context *>(context), goodResult, "Program pipeline link failed",
                        GL_INVALID_OPERATION);
        }
    }

    return angle::Result::Continue;
}

void ProgramPipeline::onSubjectStateChange(angle::SubjectIndex index, angle::SubjectMessage message)
{
    switch (message)
    {
        case angle::SubjectMessage::SubjectChanged:
            setDirtyBit(ProgramPipeline::DirtyBitType::DIRTY_BIT_PROGRAM_STAGE);
            mState.updateExecutableTextures();
            onStateChange(angle::SubjectMessage::DirtyBitsFlagged);
            break;
        default:
            UNREACHABLE();
            break;
    }
}

void ProgramPipeline::fillProgramStateMap(ShaderMap<const ProgramState *> *programStatesOut)
{
    for (ShaderType shaderType : AllShaderTypes())
    {
        (*programStatesOut)[shaderType] = nullptr;

        Program *program = getShaderProgram(shaderType);
        if (program)
        {
            (*programStatesOut)[shaderType] = &program->getState();
        }
    }
}

}  // namespace gl
