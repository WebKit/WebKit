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

ProgramPipelineState::ProgramPipelineState(rx::GLImplFactory *factory)
    : mLabel(),
      mActiveShaderProgram(nullptr),
      mValid(false),
      mExecutable(new ProgramExecutable(factory, &mInfoLog)),
      mIsLinked(false)
{
    for (const ShaderType shaderType : gl::AllShaderTypes())
    {
        mPrograms[shaderType] = nullptr;
    }
}

ProgramPipelineState::~ProgramPipelineState() {}

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
    const gl::ShaderBitSet &shaderTypes,
    Program *shaderProgram,
    std::vector<angle::ObserverBinding> *programObserverBindings)
{
    for (ShaderType shaderType : shaderTypes)
    {
        useProgramStage(context, shaderType, shaderProgram,
                        &programObserverBindings->at(static_cast<size_t>(shaderType)));
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
        mExecutable->setActiveTextureMask(mExecutable->getActiveSamplersMask() |
                                          program->getExecutable().getActiveSamplersMask());
        mExecutable->setActiveImagesMask(mExecutable->getActiveImagesMask() |
                                         program->getExecutable().getActiveImagesMask());
        // Updates mActiveSamplerRefCounts, mActiveSamplerTypes, and mActiveSamplerFormats
        mExecutable->updateActiveSamplers(program->getState());
    }
}

void ProgramPipelineState::updateExecutableSpecConstUsageBits()
{
    rx::SpecConstUsageBits specConstUsageBits;
    for (const ShaderType shaderType : mExecutable->getLinkedShaderStages())
    {
        const Program *program = getShaderProgram(shaderType);
        ASSERT(program);
        specConstUsageBits |= program->getState().getSpecConstUsageBits();
    }
    mExecutable->mPODStruct.specConstUsageBits = specConstUsageBits;
}

ProgramPipeline::ProgramPipeline(rx::GLImplFactory *factory, ProgramPipelineID handle)
    : RefCountObject(factory->generateSerial(), handle),
      mProgramPipelineImpl(factory->createProgramPipeline(mState)),
      mState(factory),
      mExecutableObserverBinding(this, kExecutableSubjectIndex)
{
    ASSERT(mProgramPipelineImpl);

    for (const ShaderType shaderType : gl::AllShaderTypes())
    {
        mProgramObserverBindings.emplace_back(this, static_cast<angle::SubjectIndex>(shaderType));
    }
    mExecutableObserverBinding.bind(mState.mExecutable.get());
}

ProgramPipeline::~ProgramPipeline()
{
    mProgramPipelineImpl.reset(nullptr);
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
    mState.mExecutable->destroy(context);
}

angle::Result ProgramPipeline::setLabel(const Context *context, const std::string &label)
{
    mState.mLabel = label;

    if (mProgramPipelineImpl)
    {
        return mProgramPipelineImpl->onLabelUpdate(context);
    }
    return angle::Result::Continue;
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

angle::Result ProgramPipeline::useProgramStages(const Context *context,
                                                GLbitfield stages,
                                                Program *shaderProgram)
{
    bool needToUpdatePipelineState = false;
    gl::ShaderBitSet shaderTypes;
    if (stages != GL_ALL_SHADER_BITS)
    {
        ASSERT(stages < 256u);
        for (size_t singleShaderBit : angle::BitSet<8>(stages))
        {
            // Cast back to a bit after the iterator returns an index.
            ShaderType shaderType = GetShaderTypeFromBitfield(angle::Bit<size_t>(singleShaderBit));
            ASSERT(shaderType != ShaderType::InvalidEnum);
            shaderTypes.set(shaderType);
        }
    }
    else
    {
        shaderTypes.set();
    }
    ASSERT(shaderTypes.any());

    for (ShaderType shaderType : shaderTypes)
    {
        if (mState.getShaderProgram(shaderType) != shaderProgram ||
            (shaderProgram && shaderProgram->hasAnyDirtyBit()))
        {
            needToUpdatePipelineState = true;
            break;
        }
    }

    if (!needToUpdatePipelineState)
    {
        return angle::Result::Continue;
    }

    mState.useProgramStages(context, shaderTypes, shaderProgram, &mProgramObserverBindings);
    updateLinkedShaderStages();

    mState.mIsLinked = false;
    onStateChange(angle::SubjectMessage::SubjectChanged);

    return angle::Result::Continue;
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

    const ProgramExecutable &vertexExecutable = vertexProgram->getExecutable();
    mState.mExecutable->mPODStruct.activeAttribLocationsMask =
        vertexExecutable.mPODStruct.activeAttribLocationsMask;
    mState.mExecutable->mPODStruct.maxActiveAttribLocation =
        vertexExecutable.mPODStruct.maxActiveAttribLocation;
    mState.mExecutable->mPODStruct.attributesTypeMask =
        vertexExecutable.mPODStruct.attributesTypeMask;
    mState.mExecutable->mPODStruct.attributesMask = vertexExecutable.mPODStruct.attributesMask;
    mState.mExecutable->mProgramInputs            = vertexExecutable.mProgramInputs;

    mState.mExecutable->mPODStruct.numViews       = vertexExecutable.mPODStruct.numViews;
    mState.mExecutable->mPODStruct.drawIDLocation = vertexExecutable.mPODStruct.drawIDLocation;
    mState.mExecutable->mPODStruct.baseVertexLocation =
        vertexExecutable.mPODStruct.baseVertexLocation;
    mState.mExecutable->mPODStruct.baseInstanceLocation =
        vertexExecutable.mPODStruct.baseInstanceLocation;
}

void ProgramPipeline::updateTransformFeedbackMembers()
{
    ShaderType lastVertexProcessingStage =
        gl::GetLastPreFragmentStage(getExecutable().getLinkedShaderStages());
    if (lastVertexProcessingStage == ShaderType::InvalidEnum)
    {
        return;
    }

    Program *shaderProgram = getShaderProgram(lastVertexProcessingStage);
    ASSERT(shaderProgram);

    const ProgramExecutable &lastPreFragmentExecutable = shaderProgram->getExecutable();
    mState.mExecutable->mTransformFeedbackStrides =
        lastPreFragmentExecutable.mTransformFeedbackStrides;
    mState.mExecutable->mLinkedTransformFeedbackVaryings =
        lastPreFragmentExecutable.mLinkedTransformFeedbackVaryings;
}

void ProgramPipeline::updateShaderStorageBlocks()
{
    mState.mExecutable->mShaderStorageBlocks.clear();

    // Only copy the storage blocks from each Program in the PPO once, since each Program could
    // contain multiple shader stages.
    ShaderBitSet handledStages;

    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const Program *shaderProgram = getShaderProgram(shaderType);
        if (shaderProgram && !handledStages.test(shaderType))
        {
            // Only add each Program's blocks once.
            handledStages |= shaderProgram->getExecutable().getLinkedShaderStages();

            for (const InterfaceBlock &block :
                 shaderProgram->getExecutable().getShaderStorageBlocks())
            {
                mState.mExecutable->mShaderStorageBlocks.emplace_back(block);
            }
        }
    }
}

void ProgramPipeline::updateImageBindings()
{
    mState.mExecutable->mImageBindings.clear();
    mState.mExecutable->mActiveImageShaderBits.fill({});

    // Only copy the storage blocks from each Program in the PPO once, since each Program could
    // contain multiple shader stages.
    ShaderBitSet handledStages;

    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const Program *shaderProgram = getShaderProgram(shaderType);
        if (shaderProgram && !handledStages.test(shaderType))
        {
            // Only add each Program's blocks once.
            handledStages |= shaderProgram->getExecutable().getLinkedShaderStages();

            for (const ImageBinding &imageBinding : shaderProgram->getState().getImageBindings())
            {
                mState.mExecutable->mImageBindings.emplace_back(imageBinding);
            }

            mState.mExecutable->updateActiveImages(shaderProgram->getExecutable());
        }
    }
}

void ProgramPipeline::updateExecutableGeometryProperties()
{
    Program *geometryProgram = getShaderProgram(gl::ShaderType::Geometry);

    if (!geometryProgram)
    {
        return;
    }

    const ProgramExecutable &geometryExecutable = geometryProgram->getExecutable();
    mState.mExecutable->mPODStruct.geometryShaderInputPrimitiveType =
        geometryExecutable.mPODStruct.geometryShaderInputPrimitiveType;
    mState.mExecutable->mPODStruct.geometryShaderOutputPrimitiveType =
        geometryExecutable.mPODStruct.geometryShaderOutputPrimitiveType;
    mState.mExecutable->mPODStruct.geometryShaderInvocations =
        geometryExecutable.mPODStruct.geometryShaderInvocations;
    mState.mExecutable->mPODStruct.geometryShaderMaxVertices =
        geometryExecutable.mPODStruct.geometryShaderMaxVertices;
}

void ProgramPipeline::updateExecutableTessellationProperties()
{
    Program *tessControlProgram = getShaderProgram(gl::ShaderType::TessControl);
    Program *tessEvalProgram    = getShaderProgram(gl::ShaderType::TessEvaluation);

    if (tessControlProgram)
    {
        const ProgramExecutable &tessControlExecutable = tessControlProgram->getExecutable();
        mState.mExecutable->mPODStruct.tessControlShaderVertices =
            tessControlExecutable.mPODStruct.tessControlShaderVertices;
    }

    if (tessEvalProgram)
    {
        const ProgramExecutable &tessEvalExecutable = tessEvalProgram->getExecutable();
        mState.mExecutable->mPODStruct.tessGenMode  = tessEvalExecutable.mPODStruct.tessGenMode;
        mState.mExecutable->mPODStruct.tessGenSpacing =
            tessEvalExecutable.mPODStruct.tessGenSpacing;
        mState.mExecutable->mPODStruct.tessGenVertexOrder =
            tessEvalExecutable.mPODStruct.tessGenVertexOrder;
        mState.mExecutable->mPODStruct.tessGenPointMode =
            tessEvalExecutable.mPODStruct.tessGenPointMode;
    }
}

void ProgramPipeline::updateFragmentInoutRangeAndEnablesPerSampleShading()
{
    Program *fragmentProgram = getShaderProgram(gl::ShaderType::Fragment);

    if (!fragmentProgram)
    {
        return;
    }

    const ProgramExecutable &fragmentExecutable = fragmentProgram->getExecutable();
    mState.mExecutable->mPODStruct.fragmentInoutRange =
        fragmentExecutable.mPODStruct.fragmentInoutRange;
    mState.mExecutable->mPODStruct.hasDiscard = fragmentExecutable.mPODStruct.hasDiscard;
    mState.mExecutable->mPODStruct.enablesPerSampleShading =
        fragmentExecutable.mPODStruct.enablesPerSampleShading;
}

void ProgramPipeline::updateLinkedVaryings()
{
    // Need to check all of the shader stages, not just linked, so we handle Compute correctly.
    for (const gl::ShaderType shaderType : kAllGraphicsShaderTypes)
    {
        const Program *shaderProgram = getShaderProgram(shaderType);
        if (shaderProgram && shaderProgram->isLinked())
        {
            const ProgramExecutable &executable = shaderProgram->getExecutable();
            mState.mExecutable->mLinkedOutputVaryings[shaderType] =
                executable.getLinkedOutputVaryings(shaderType);
            mState.mExecutable->mLinkedInputVaryings[shaderType] =
                executable.getLinkedInputVaryings(shaderType);
        }
    }

    const Program *computeProgram = getShaderProgram(ShaderType::Compute);
    if (computeProgram && computeProgram->isLinked())
    {
        const ProgramExecutable &executable = computeProgram->getExecutable();
        mState.mExecutable->mLinkedOutputVaryings[ShaderType::Compute] =
            executable.getLinkedOutputVaryings(ShaderType::Compute);
        mState.mExecutable->mLinkedInputVaryings[ShaderType::Compute] =
            executable.getLinkedInputVaryings(ShaderType::Compute);
    }
}

void ProgramPipeline::updateExecutable()
{
    // Vertex Shader ProgramExecutable properties
    updateExecutableAttributes();
    updateTransformFeedbackMembers();
    updateShaderStorageBlocks();
    updateImageBindings();

    // Geometry Shader ProgramExecutable properties
    updateExecutableGeometryProperties();

    // Tessellation Shaders ProgramExecutable properties
    updateExecutableTessellationProperties();

    // Fragment Shader ProgramExecutable properties
    updateFragmentInoutRangeAndEnablesPerSampleShading();

    // All Shader ProgramExecutable properties
    mState.updateExecutableTextures();
    mState.updateExecutableSpecConstUsageBits();
    updateLinkedVaryings();
}

// The attached shaders are checked for linking errors by matching up their variables.
// Uniform, input and output variables get collected.
// The code gets compiled into binaries.
angle::Result ProgramPipeline::link(const Context *context)
{
    ASSERT(!mState.mIsLinked);

    ProgramMergedVaryings mergedVaryings;
    ProgramVaryingPacking varyingPacking;
    LinkingVariables linkingVariables;

    mState.mExecutable->reset();
    mState.mInfoLog.reset();

    linkingVariables.initForProgramPipeline(mState);

    const Caps &caps               = context->getCaps();
    const Limitations &limitations = context->getLimitations();
    const Version &clientVersion   = context->getClientVersion();
    const bool isWebGL             = context->isWebGL();

    if (mState.mExecutable->hasLinkedShaderStage(gl::ShaderType::Vertex))
    {
        if (!linkVaryings())
        {
            return angle::Result::Stop;
        }

        if (!LinkValidateProgramGlobalNames(mState.mInfoLog, getExecutable(), linkingVariables))
        {
            return angle::Result::Stop;
        }

        Program *fragmentShaderProgram = getShaderProgram(ShaderType::Fragment);
        if (fragmentShaderProgram)
        {
            // We should also be validating SSBO and image uniform counts.
            const GLuint combinedImageUniforms          = 0;
            const GLuint combinedShaderStorageBlocks    = 0;
            const ProgramExecutable &fragmentExecutable = fragmentShaderProgram->getExecutable();
            if (!mState.mExecutable->linkValidateOutputVariables(
                    caps, clientVersion, combinedImageUniforms, combinedShaderStorageBlocks,
                    fragmentExecutable.getOutputVariables(),
                    fragmentExecutable.getLinkedShaderVersion(ShaderType::Fragment),
                    ProgramAliasedBindings(), ProgramAliasedBindings()))
            {
                return angle::Result::Continue;
            }
        }
        mergedVaryings = GetMergedVaryingsFromLinkingVariables(linkingVariables);
        // If separable program objects are in use, the set of attributes captured is taken
        // from the program object active on the last vertex processing stage.
        ShaderType lastVertexProcessingStage =
            gl::GetLastPreFragmentStage(getExecutable().getLinkedShaderStages());
        if (lastVertexProcessingStage == ShaderType::InvalidEnum)
        {
            //  If there is no active program for the vertex or fragment shader stages, the results
            //  of vertex and fragment shader execution will respectively be undefined. However,
            //  this is not an error.
            return angle::Result::Continue;
        }

        Program *tfProgram = getShaderProgram(lastVertexProcessingStage);
        ASSERT(tfProgram);

        if (!tfProgram)
        {
            tfProgram = mState.mPrograms[ShaderType::Vertex];
        }

        const std::vector<std::string> &transformFeedbackVaryingNames =
            tfProgram->getState().getTransformFeedbackVaryingNames();

        if (!mState.mExecutable->linkMergedVaryings(caps, limitations, clientVersion, isWebGL,
                                                    mergedVaryings, transformFeedbackVaryingNames,
                                                    linkingVariables, false, &varyingPacking))
        {
            return angle::Result::Stop;
        }
    }

    // Merge uniforms.
    mState.mExecutable->copyUniformsFromProgramMap(mState.mPrograms);

    if (mState.mExecutable->hasLinkedShaderStage(gl::ShaderType::Vertex))
    {
        const ProgramState &programState = mState.mPrograms[gl::ShaderType::Vertex]->getState();
        mState.mExecutable->copyInputsFromProgram(programState);
    }

    // Merge shader buffers (UBOs, SSBOs, and atomic counter buffers) into the executable.
    // Also copy over image and sampler bindings.
    for (ShaderType shaderType : mState.mExecutable->getLinkedShaderStages())
    {
        const ProgramState &programState = mState.mPrograms[shaderType]->getState();
        mState.mExecutable->copyShaderBuffersFromProgram(programState, shaderType);
        mState.mExecutable->copySamplerBindingsFromProgram(programState);
        mState.mExecutable->copyImageBindingsFromProgram(programState);
    }

    if (mState.mExecutable->hasLinkedShaderStage(gl::ShaderType::Fragment))
    {
        const ProgramState &programState = mState.mPrograms[gl::ShaderType::Fragment]->getState();
        mState.mExecutable->copyOutputsFromProgram(programState);
    }

    if (mState.mExecutable->hasLinkedShaderStage(gl::ShaderType::Vertex) ||
        mState.mExecutable->hasLinkedShaderStage(gl::ShaderType::Compute))
    {
        ANGLE_TRY(getImplementation()->link(context, mergedVaryings, varyingPacking));
    }

    mState.mExecutable->mActiveSamplerRefCounts.fill(0);
    updateExecutable();

    mState.mIsLinked = true;
    onStateChange(angle::SubjectMessage::SubjectChanged);

    return angle::Result::Continue;
}

int ProgramPipeline::getInfoLogLength() const
{
    return static_cast<int>(mState.mInfoLog.getLength());
}

void ProgramPipeline::getInfoLog(GLsizei bufSize, GLsizei *length, char *infoLog) const
{
    return mState.mInfoLog.getLog(bufSize, length, infoLog);
}

bool ProgramPipeline::linkVaryings()
{
    ShaderType previousShaderType = ShaderType::InvalidEnum;
    for (ShaderType shaderType : kAllGraphicsShaderTypes)
    {
        Program *program = getShaderProgram(shaderType);
        if (!program)
        {
            continue;
        }
        ProgramExecutable &executable = program->getExecutable();

        if (previousShaderType != ShaderType::InvalidEnum)
        {
            Program *previousProgram = getShaderProgram(previousShaderType);
            ASSERT(previousProgram);
            const ProgramExecutable &previousExecutable = previousProgram->getExecutable();

            if (!LinkValidateShaderInterfaceMatching(
                    previousExecutable.getLinkedOutputVaryings(previousShaderType),
                    executable.getLinkedInputVaryings(shaderType), previousShaderType, shaderType,
                    previousExecutable.getLinkedShaderVersion(previousShaderType),
                    executable.getLinkedShaderVersion(shaderType), true, mState.mInfoLog))
            {
                return false;
            }
        }
        previousShaderType = shaderType;
    }

    // TODO: http://anglebug.com/3571 and http://anglebug.com/3572
    // Need to move logic of validating builtin varyings inside the for-loop above.
    // This is because the built-in symbols `gl_ClipDistance` and `gl_CullDistance`
    // can be redeclared in Geometry or Tessellation shaders as well.
    Program *vertexProgram   = mState.mPrograms[ShaderType::Vertex];
    Program *fragmentProgram = mState.mPrograms[ShaderType::Fragment];
    if (!vertexProgram || !fragmentProgram)
    {
        return true;
    }
    ProgramExecutable &vertexExecutable   = vertexProgram->getExecutable();
    ProgramExecutable &fragmentExecutable = fragmentProgram->getExecutable();
    return LinkValidateBuiltInVaryings(
        vertexExecutable.getLinkedOutputVaryings(ShaderType::Vertex),
        fragmentExecutable.getLinkedInputVaryings(ShaderType::Fragment), ShaderType::Vertex,
        ShaderType::Fragment, vertexExecutable.getLinkedShaderVersion(ShaderType::Vertex),
        fragmentExecutable.getLinkedShaderVersion(ShaderType::Fragment), mState.mInfoLog);
}

void ProgramPipeline::validate(const gl::Context *context)
{
    const Caps &caps = context->getCaps();
    mState.mValid    = true;
    mState.mInfoLog.reset();

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
                mState.mInfoLog << shaderInfoString << "\n";
                return;
            }
            if (!shaderProgram->isSeparable())
            {
                mState.mValid = false;
                mState.mInfoLog << GetShaderTypeString(shaderType) << " is not marked separable."
                                << "\n";
                return;
            }
        }
    }

    intptr_t programPipelineError = context->getStateCache().getProgramPipelineError(context);
    if (programPipelineError)
    {
        mState.mValid            = false;
        const char *errorMessage = reinterpret_cast<const char *>(programPipelineError);
        mState.mInfoLog << errorMessage << "\n";
        return;
    }

    if (!linkVaryings())
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
                mState.mInfoLog << shaderInfoString << "\n";
            }
        }
    }
}

angle::Result ProgramPipeline::syncState(const Context *context)
{
    Program::DirtyBits dirtyBits;
    for (const ShaderType shaderType : mState.mExecutable->getLinkedShaderStages())
    {
        Program *shaderProgram = mState.mPrograms[shaderType];
        if (shaderProgram)
        {
            dirtyBits |= shaderProgram->mDirtyBits;
        }
    }
    if (dirtyBits.any())
    {
        ANGLE_TRY(mProgramPipelineImpl->syncState(context, dirtyBits));
    }

    return angle::Result::Continue;
}

void ProgramPipeline::onUniformBufferStateChange(size_t uniformBufferIndex)
{
    for (const ShaderType shaderType : mState.mExecutable->getLinkedShaderStages())
    {
        Program *shaderProgram = mState.mPrograms[shaderType];
        if (shaderProgram)
        {
            shaderProgram->onUniformBufferStateChange(uniformBufferIndex);
        }
    }
}

void ProgramPipeline::onSubjectStateChange(angle::SubjectIndex index, angle::SubjectMessage message)
{
    switch (message)
    {
        case angle::SubjectMessage::ProgramTextureOrImageBindingChanged:
            mState.mExecutable->mActiveSamplerRefCounts.fill(0);
            mState.updateExecutableTextures();
            break;

        case angle::SubjectMessage::ProgramRelinked:
            mState.mIsLinked = false;
            onStateChange(angle::SubjectMessage::ProgramRelinked);
            break;
        case angle::SubjectMessage::SamplerUniformsUpdated:
            mState.mExecutable->clearSamplerBindings();
            for (ShaderType shaderType : mState.mExecutable->getLinkedShaderStages())
            {
                const ProgramState &programState = mState.mPrograms[shaderType]->getState();
                mState.mExecutable->copySamplerBindingsFromProgram(programState);
            }
            mState.mExecutable->mActiveSamplerRefCounts.fill(0);
            mState.updateExecutableTextures();
            break;
        case angle::SubjectMessage::ProgramUniformUpdated:
            mProgramPipelineImpl->onProgramUniformUpdate(static_cast<ShaderType>(index));
            break;
        default:
            UNREACHABLE();
            break;
    }
}
}  // namespace gl
