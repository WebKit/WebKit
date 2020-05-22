//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramExecutable.cpp: Collects the interfaces common to both Programs and
// ProgramPipelines in order to execute/draw with either.

#include "libANGLE/ProgramExecutable.h"

#include "libANGLE/Context.h"
#include "libANGLE/Program.h"
#include "libANGLE/ProgramPipeline.h"
#include "libANGLE/Shader.h"

namespace gl
{

ProgramExecutable::ProgramExecutable()
    : mProgramState(nullptr),
      mProgramPipelineState(nullptr),
      mMaxActiveAttribLocation(0),
      mAttributesTypeMask(0),
      mAttributesMask(0),
      mActiveSamplersMask(0),
      mActiveSamplerRefCounts{},
      mActiveImagesMask(0),
      mCanDrawWith(false),
      mTransformFeedbackBufferMode(GL_INTERLEAVED_ATTRIBS),
      mSamplerUniformRange(0, 0),
      mImageUniformRange(0, 0)
{
    reset();
}

ProgramExecutable::ProgramExecutable(const ProgramExecutable &other)
    : mProgramState(other.mProgramState),
      mProgramPipelineState(other.mProgramPipelineState),
      mLinkedGraphicsShaderStages(other.mLinkedGraphicsShaderStages),
      mLinkedComputeShaderStages(other.mLinkedComputeShaderStages),
      mActiveAttribLocationsMask(other.mActiveAttribLocationsMask),
      mMaxActiveAttribLocation(other.mMaxActiveAttribLocation),
      mAttributesTypeMask(other.mAttributesTypeMask),
      mAttributesMask(other.mAttributesMask),
      mActiveSamplersMask(other.mActiveSamplersMask),
      mActiveSamplerRefCounts(other.mActiveSamplerRefCounts),
      mActiveSamplerTypes(other.mActiveSamplerTypes),
      mActiveSamplerFormats(other.mActiveSamplerFormats),
      mActiveSamplerShaderBits(other.mActiveSamplerShaderBits),
      mActiveImagesMask(other.mActiveImagesMask),
      mActiveImageShaderBits(other.mActiveImageShaderBits),
      mCanDrawWith(other.mCanDrawWith),
      mOutputVariables(other.mOutputVariables),
      mOutputLocations(other.mOutputLocations),
      mProgramInputs(other.mProgramInputs),
      mLinkedTransformFeedbackVaryings(other.mLinkedTransformFeedbackVaryings),
      mTransformFeedbackStrides(other.mTransformFeedbackStrides),
      mTransformFeedbackBufferMode(other.mTransformFeedbackBufferMode),
      mUniforms(other.mUniforms),
      mSamplerUniformRange(other.mSamplerUniformRange),
      mUniformBlocks(other.mUniformBlocks),
      mAtomicCounterBuffers(other.mAtomicCounterBuffers),
      mImageUniformRange(other.mImageUniformRange),
      mShaderStorageBlocks(other.mShaderStorageBlocks)
{
    reset();
}

ProgramExecutable::~ProgramExecutable() = default;

void ProgramExecutable::reset()
{
    resetInfoLog();
    mActiveAttribLocationsMask.reset();
    mAttributesTypeMask.reset();
    mAttributesMask.reset();
    mMaxActiveAttribLocation = 0;

    mActiveSamplersMask.reset();
    mActiveSamplerRefCounts = {};
    mActiveSamplerTypes.fill(TextureType::InvalidEnum);
    mActiveSamplerFormats.fill(SamplerFormat::InvalidEnum);

    mActiveImagesMask.reset();

    mProgramInputs.clear();
    mLinkedTransformFeedbackVaryings.clear();
    mUniforms.clear();
    mUniformBlocks.clear();
    mShaderStorageBlocks.clear();
    mAtomicCounterBuffers.clear();
    mOutputVariables.clear();
    mOutputLocations.clear();
}

void ProgramExecutable::load(gl::BinaryInputStream *stream)
{
    static_assert(MAX_VERTEX_ATTRIBS * 2 <= sizeof(uint32_t) * 8,
                  "Too many vertex attribs for mask: All bits of mAttributesTypeMask types and "
                  "mask fit into 32 bits each");
    mAttributesTypeMask        = gl::ComponentTypeMask(stream->readInt<uint32_t>());
    mAttributesMask            = stream->readInt<gl::AttributesMask>();
    mActiveAttribLocationsMask = stream->readInt<gl::AttributesMask>();
    mMaxActiveAttribLocation   = stream->readInt<unsigned int>();

    mLinkedGraphicsShaderStages = ShaderBitSet(stream->readInt<uint8_t>());
    mLinkedComputeShaderStages  = ShaderBitSet(stream->readInt<uint8_t>());
}

void ProgramExecutable::save(gl::BinaryOutputStream *stream) const
{
    static_assert(MAX_VERTEX_ATTRIBS * 2 <= sizeof(uint32_t) * 8,
                  "All bits of mAttributesTypeMask types and mask fit into 32 bits each");
    stream->writeInt(static_cast<int>(mAttributesTypeMask.to_ulong()));
    stream->writeInt(static_cast<int>(mAttributesMask.to_ulong()));
    stream->writeInt(mActiveAttribLocationsMask.to_ulong());
    stream->writeInt(mMaxActiveAttribLocation);

    stream->writeInt(mLinkedGraphicsShaderStages.bits());
    stream->writeInt(mLinkedComputeShaderStages.bits());
}

const ProgramState *ProgramExecutable::getProgramState(ShaderType shaderType) const
{
    if (mProgramState &&
        (hasLinkedShaderStage(shaderType) || mProgramState->getAttachedShader(shaderType)))
    {
        return mProgramState;
    }
    else if (mProgramPipelineState && (hasLinkedShaderStage(shaderType) ||
                                       mProgramPipelineState->getShaderProgram(shaderType)))
    {
        return &mProgramPipelineState->getShaderProgram(shaderType)->getState();
    }

    return nullptr;
}

bool ProgramExecutable::isCompute() const
{
    ASSERT(mProgramState || mProgramPipelineState);

    if (mProgramState)
    {
        return mProgramState->isCompute();
    }

    return mProgramPipelineState->isCompute();
}

void ProgramExecutable::setIsCompute(bool isComputeIn)
{
    // A Program can only either be graphics or compute, but never both, so it can answer
    // isCompute() based on which shaders it has. However, a PPO can have both graphics and compute
    // programs attached, so we don't know if the PPO is a 'graphics' or 'compute' PPO until the
    // actual draw/dispatch call, which is why only PPOs need to record the type of call here.
    if (mProgramPipelineState)
    {
        mProgramPipelineState->setIsCompute(isComputeIn);
    }
}

int ProgramExecutable::getInfoLogLength() const
{
    return static_cast<int>(mInfoLog.getLength());
}

void ProgramExecutable::getInfoLog(GLsizei bufSize, GLsizei *length, char *infoLog) const
{
    return mInfoLog.getLog(bufSize, length, infoLog);
}

std::string ProgramExecutable::getInfoLogString() const
{
    return mInfoLog.str();
}

bool ProgramExecutable::isAttribLocationActive(size_t attribLocation) const
{
    // TODO(timvp): http://anglebug.com/3570: Enable this assert here somehow.
    //    ASSERT(!mLinkingState);
    ASSERT(attribLocation < mActiveAttribLocationsMask.size());
    return mActiveAttribLocationsMask[attribLocation];
}

AttributesMask ProgramExecutable::getAttributesMask() const
{
    // TODO(timvp): http://anglebug.com/3570: Enable this assert here somehow.
    //    ASSERT(!mLinkingState);
    return mAttributesMask;
}

bool ProgramExecutable::hasDefaultUniforms() const
{
    ASSERT(mProgramState || mProgramPipelineState);
    if (mProgramState)
    {
        return mProgramState->hasDefaultUniforms();
    }

    return mProgramPipelineState->hasDefaultUniforms();
}

bool ProgramExecutable::hasTextures() const
{
    ASSERT(mProgramState || mProgramPipelineState);
    if (mProgramState)
    {
        return mProgramState->hasTextures();
    }

    return mProgramPipelineState->hasTextures();
}

bool ProgramExecutable::hasUniformBuffers() const
{
    ASSERT(mProgramState || mProgramPipelineState);
    if (mProgramState)
    {
        return mProgramState->hasUniformBuffers();
    }

    return mProgramPipelineState->hasUniformBuffers();
}

bool ProgramExecutable::hasStorageBuffers() const
{
    ASSERT(mProgramState || mProgramPipelineState);
    if (mProgramState)
    {
        return mProgramState->hasStorageBuffers();
    }

    return mProgramPipelineState->hasStorageBuffers();
}

bool ProgramExecutable::hasAtomicCounterBuffers() const
{
    ASSERT(mProgramState || mProgramPipelineState);
    if (mProgramState)
    {
        return mProgramState->hasAtomicCounterBuffers();
    }

    return mProgramPipelineState->hasAtomicCounterBuffers();
}

bool ProgramExecutable::hasImages() const
{
    ASSERT(mProgramState || mProgramPipelineState);
    if (mProgramState)
    {
        return mProgramState->hasImages();
    }

    return mProgramPipelineState->hasImages();
}

bool ProgramExecutable::hasTransformFeedbackOutput() const
{
    ASSERT(mProgramState || mProgramPipelineState);
    if (mProgramState)
    {
        return mProgramState->hasTransformFeedbackOutput();
    }

    return mProgramPipelineState->hasTransformFeedbackOutput();
}

size_t ProgramExecutable::getTransformFeedbackBufferCount(const gl::State &glState) const
{
    ASSERT(mProgramState || mProgramPipelineState);
    if (mProgramState)
    {
        return mProgramState->getTransformFeedbackBufferCount();
    }

    // TODO(timvp): http://anglebug.com/3570: Support program pipelines

    return 0;
}

void ProgramExecutable::updateActiveSamplers(const ProgramState &programState)
{
    const std::vector<SamplerBinding> &samplerBindings = programState.getSamplerBindings();

    for (uint32_t samplerIndex = 0; samplerIndex < samplerBindings.size(); ++samplerIndex)
    {
        const SamplerBinding &samplerBinding = samplerBindings[samplerIndex];
        if (samplerBinding.unreferenced)
            continue;

        uint32_t uniformIndex = programState.getUniformIndexFromSamplerIndex(samplerIndex);
        const gl::LinkedUniform &samplerUniform = programState.getUniforms()[uniformIndex];

        for (GLint textureUnit : samplerBinding.boundTextureUnits)
        {
            if (++mActiveSamplerRefCounts[textureUnit] == 1)
            {
                mActiveSamplerTypes[textureUnit]      = samplerBinding.textureType;
                mActiveSamplerFormats[textureUnit]    = samplerBinding.format;
                mActiveSamplerShaderBits[textureUnit] = samplerUniform.activeShaders();
            }
            else
            {
                if (mActiveSamplerTypes[textureUnit] != samplerBinding.textureType)
                {
                    mActiveSamplerTypes[textureUnit] = TextureType::InvalidEnum;
                }
                if (mActiveSamplerFormats[textureUnit] != samplerBinding.format)
                {
                    mActiveSamplerFormats[textureUnit] = SamplerFormat::InvalidEnum;
                }
            }
            mActiveSamplersMask.set(textureUnit);
        }
    }
}

void ProgramExecutable::updateActiveImages(std::vector<ImageBinding> &imageBindings)
{
    const bool compute = isCompute() ? true : false;
    for (uint32_t imageIndex = 0; imageIndex < imageBindings.size(); ++imageIndex)
    {
        const gl::ImageBinding &imageBinding = imageBindings[imageIndex];
        if (imageBinding.unreferenced)
            continue;

        uint32_t uniformIndex = mProgramState->getUniformIndexFromImageIndex(imageIndex);
        const gl::LinkedUniform &imageUniform = mProgramState->getUniforms()[uniformIndex];
        const ShaderBitSet shaderBits         = imageUniform.activeShaders();
        for (GLint imageUnit : imageBinding.boundImageUnits)
        {
            mActiveImagesMask.set(imageUnit);
            if (compute)
                mActiveImageShaderBits[imageUnit].set(gl::ShaderType::Compute);
            else
                mActiveImageShaderBits[imageUnit] = shaderBits;
        }
    }
}

void ProgramExecutable::setSamplerUniformTextureTypeAndFormat(
    size_t textureUnitIndex,
    std::vector<SamplerBinding> &samplerBindings)
{
    bool foundBinding         = false;
    TextureType foundType     = TextureType::InvalidEnum;
    SamplerFormat foundFormat = SamplerFormat::InvalidEnum;

    for (const SamplerBinding &binding : samplerBindings)
    {
        if (binding.unreferenced)
            continue;

        // A conflict exists if samplers of different types are sourced by the same texture unit.
        // We need to check all bound textures to detect this error case.
        for (GLuint textureUnit : binding.boundTextureUnits)
        {
            if (textureUnit == textureUnitIndex)
            {
                if (!foundBinding)
                {
                    foundBinding = true;
                    foundType    = binding.textureType;
                    foundFormat  = binding.format;
                }
                else
                {
                    if (foundType != binding.textureType)
                    {
                        foundType = TextureType::InvalidEnum;
                    }
                    if (foundFormat != binding.format)
                    {
                        foundFormat = SamplerFormat::InvalidEnum;
                    }
                }
            }
        }
    }

    mActiveSamplerTypes[textureUnitIndex]   = foundType;
    mActiveSamplerFormats[textureUnitIndex] = foundFormat;
}

bool ProgramExecutable::linkValidateGlobalNames(InfoLog &infoLog) const
{
    std::unordered_map<std::string, const sh::ShaderVariable *> uniformMap;
    using BlockAndFieldPair = std::pair<const sh::InterfaceBlock *, const sh::ShaderVariable *>;
    std::unordered_map<std::string, std::vector<BlockAndFieldPair>> uniformBlockFieldMap;

    for (ShaderType shaderType : kAllGraphicsShaderTypes)
    {
        const ProgramState *programState = getProgramState(shaderType);
        if (!programState)
        {
            continue;
        }
        Shader *shader = programState->getAttachedShader(shaderType);
        if (!shader)
        {
            continue;
        }

        // Build a map of Uniforms
        const std::vector<sh::ShaderVariable> uniforms = shader->getUniforms();
        for (const auto &uniform : uniforms)
        {
            uniformMap[uniform.name] = &uniform;
        }

        // Build a map of Uniform Blocks
        // This will also detect any field name conflicts between Uniform Blocks without instance
        // names
        const std::vector<sh::InterfaceBlock> &uniformBlocks = shader->getUniformBlocks();
        for (const auto &uniformBlock : uniformBlocks)
        {
            // Only uniform blocks without an instance name can create a conflict with their field
            // names
            if (!uniformBlock.instanceName.empty())
            {
                continue;
            }

            for (const auto &field : uniformBlock.fields)
            {
                if (!uniformBlockFieldMap.count(field.name))
                {
                    // First time we've seen this uniform block field name, so add the
                    // (Uniform Block, Field) pair immediately since there can't be a conflict yet
                    BlockAndFieldPair blockAndFieldPair(&uniformBlock, &field);
                    std::vector<BlockAndFieldPair> newUniformBlockList;
                    newUniformBlockList.push_back(blockAndFieldPair);
                    uniformBlockFieldMap[field.name] = newUniformBlockList;
                    continue;
                }

                // We've seen this name before.
                // We need to check each of the uniform blocks that contain a field with this name
                // to see if there's a conflict or not.
                std::vector<BlockAndFieldPair> prevBlockFieldPairs =
                    uniformBlockFieldMap[field.name];
                for (const auto &prevBlockFieldPair : prevBlockFieldPairs)
                {
                    const sh::InterfaceBlock *prevUniformBlock      = prevBlockFieldPair.first;
                    const sh::ShaderVariable *prevUniformBlockField = prevBlockFieldPair.second;

                    if (uniformBlock.isSameInterfaceBlockAtLinkTime(*prevUniformBlock))
                    {
                        // The same uniform block should, by definition, contain the same field name
                        continue;
                    }

                    // The uniform blocks don't match, so check if the necessary field properties
                    // also match
                    if ((field.name == prevUniformBlockField->name) &&
                        (field.type == prevUniformBlockField->type) &&
                        (field.precision == prevUniformBlockField->precision))
                    {
                        infoLog << "Name conflicts between uniform block field names: "
                                << field.name;
                        return false;
                    }
                }

                // No conflict, so record this pair
                BlockAndFieldPair blockAndFieldPair(&uniformBlock, &field);
                uniformBlockFieldMap[field.name].push_back(blockAndFieldPair);
            }
        }
    }

    // Validate no uniform names conflict with attribute names
    const ProgramState *programState = getProgramState(ShaderType::Vertex);
    if (programState)
    {
        Shader *vertexShader = programState->getAttachedShader(ShaderType::Vertex);
        if (vertexShader)
        {
            for (const auto &attrib : vertexShader->getActiveAttributes())
            {
                if (uniformMap.count(attrib.name))
                {
                    infoLog << "Name conflicts between a uniform and an attribute: " << attrib.name;
                    return false;
                }
            }
        }
    }

    // Validate no Uniform Block fields conflict with other Uniforms
    for (const auto &uniformBlockField : uniformBlockFieldMap)
    {
        const std::string &fieldName = uniformBlockField.first;
        if (uniformMap.count(fieldName))
        {
            infoLog << "Name conflicts between a uniform and a uniform block field: " << fieldName;
            return false;
        }
    }

    return true;
}

void ProgramExecutable::updateCanDrawWith()
{
    mCanDrawWith =
        (hasLinkedShaderStage(ShaderType::Vertex) && hasLinkedShaderStage(ShaderType::Fragment));
}

void ProgramExecutable::saveLinkedStateInfo()
{
    // Only a Program's linked data needs to be saved, not a ProgramPipeline's
    ASSERT(mProgramState);

    for (ShaderType shaderType : getLinkedShaderStages())
    {
        Shader *shader = mProgramState->getAttachedShader(shaderType);
        ASSERT(shader);
        mLinkedOutputVaryings[shaderType] = shader->getOutputVaryings();
        mLinkedInputVaryings[shaderType]  = shader->getInputVaryings();
        mLinkedShaderVersions[shaderType] = shader->getShaderVersion();
    }
}

}  // namespace gl
