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
    : mMaxActiveAttribLocation(0),
      mAttributesTypeMask(0),
      mAttributesMask(0),
      mActiveSamplersMask(0),
      mActiveSamplerRefCounts{},
      mActiveImagesMask(0),
      mCanDrawWith(false),
      mTransformFeedbackBufferMode(GL_INTERLEAVED_ATTRIBS),
      mDefaultUniformRange(0, 0),
      mSamplerUniformRange(0, 0),
      mImageUniformRange(0, 0),
      mPipelineHasGraphicsUniformBuffers(false),
      mPipelineHasComputeUniformBuffers(false),
      mPipelineHasGraphicsStorageBuffers(false),
      mPipelineHasComputeStorageBuffers(false),
      mPipelineHasGraphicsAtomicCounterBuffers(false),
      mPipelineHasComputeAtomicCounterBuffers(false),
      mPipelineHasGraphicsDefaultUniforms(false),
      mPipelineHasComputeDefaultUniforms(false),
      mPipelineHasGraphicsTextures(false),
      mPipelineHasComputeTextures(false),
      mPipelineHasGraphicsImages(false),
      mPipelineHasComputeImages(false),
      mIsCompute(false)
{
    reset();
}

ProgramExecutable::ProgramExecutable(const ProgramExecutable &other)
    : mLinkedGraphicsShaderStages(other.mLinkedGraphicsShaderStages),
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
      mDefaultUniformRange(other.mDefaultUniformRange),
      mSamplerUniformRange(other.mSamplerUniformRange),
      mUniformBlocks(other.mUniformBlocks),
      mAtomicCounterBuffers(other.mAtomicCounterBuffers),
      mImageUniformRange(other.mImageUniformRange),
      mShaderStorageBlocks(other.mShaderStorageBlocks),
      mPipelineHasGraphicsUniformBuffers(other.mPipelineHasGraphicsUniformBuffers),
      mPipelineHasComputeUniformBuffers(other.mPipelineHasComputeUniformBuffers),
      mPipelineHasGraphicsStorageBuffers(other.mPipelineHasGraphicsStorageBuffers),
      mPipelineHasComputeStorageBuffers(other.mPipelineHasComputeStorageBuffers),
      mPipelineHasGraphicsAtomicCounterBuffers(other.mPipelineHasGraphicsAtomicCounterBuffers),
      mPipelineHasComputeAtomicCounterBuffers(other.mPipelineHasComputeAtomicCounterBuffers),
      mPipelineHasGraphicsDefaultUniforms(other.mPipelineHasGraphicsDefaultUniforms),
      mPipelineHasComputeDefaultUniforms(other.mPipelineHasComputeDefaultUniforms),
      mPipelineHasGraphicsTextures(other.mPipelineHasGraphicsTextures),
      mPipelineHasComputeTextures(other.mPipelineHasComputeTextures),
      mPipelineHasGraphicsImages(other.mPipelineHasGraphicsImages),
      mPipelineHasComputeImages(other.mPipelineHasComputeImages),
      mIsCompute(other.mIsCompute)
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
    mSamplerBindings.clear();
    mImageBindings.clear();

    mPipelineHasGraphicsUniformBuffers       = false;
    mPipelineHasComputeUniformBuffers        = false;
    mPipelineHasGraphicsStorageBuffers       = false;
    mPipelineHasComputeStorageBuffers        = false;
    mPipelineHasGraphicsAtomicCounterBuffers = false;
    mPipelineHasComputeAtomicCounterBuffers  = false;
    mPipelineHasGraphicsDefaultUniforms      = false;
    mPipelineHasComputeDefaultUniforms       = false;
    mPipelineHasGraphicsTextures             = false;
    mPipelineHasComputeTextures              = false;
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
    mIsCompute                  = stream->readBool();

    mPipelineHasGraphicsUniformBuffers       = stream->readBool();
    mPipelineHasComputeUniformBuffers        = stream->readBool();
    mPipelineHasGraphicsStorageBuffers       = stream->readBool();
    mPipelineHasComputeStorageBuffers        = stream->readBool();
    mPipelineHasGraphicsAtomicCounterBuffers = stream->readBool();
    mPipelineHasComputeAtomicCounterBuffers  = stream->readBool();
    mPipelineHasGraphicsDefaultUniforms      = stream->readBool();
    mPipelineHasComputeDefaultUniforms       = stream->readBool();
    mPipelineHasGraphicsTextures             = stream->readBool();
    mPipelineHasComputeTextures              = stream->readBool();
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
    stream->writeInt(static_cast<bool>(mIsCompute));

    stream->writeInt(static_cast<bool>(mPipelineHasGraphicsUniformBuffers));
    stream->writeInt(static_cast<bool>(mPipelineHasComputeUniformBuffers));
    stream->writeInt(static_cast<bool>(mPipelineHasGraphicsStorageBuffers));
    stream->writeInt(static_cast<bool>(mPipelineHasComputeStorageBuffers));
    stream->writeInt(static_cast<bool>(mPipelineHasGraphicsAtomicCounterBuffers));
    stream->writeInt(static_cast<bool>(mPipelineHasComputeAtomicCounterBuffers));
    stream->writeInt(static_cast<bool>(mPipelineHasGraphicsDefaultUniforms));
    stream->writeInt(static_cast<bool>(mPipelineHasComputeDefaultUniforms));
    stream->writeInt(static_cast<bool>(mPipelineHasGraphicsTextures));
    stream->writeInt(static_cast<bool>(mPipelineHasComputeTextures));
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
    return !getDefaultUniformRange().empty() ||
           (isCompute() ? mPipelineHasComputeDefaultUniforms : mPipelineHasGraphicsDefaultUniforms);
}

bool ProgramExecutable::hasTextures() const
{
    return !getSamplerBindings().empty() ||
           (isCompute() ? mPipelineHasComputeTextures : mPipelineHasGraphicsTextures);
}

// TODO: http://anglebug.com/3570: Remove mHas*UniformBuffers once PPO's have valid data in
// mUniformBlocks
bool ProgramExecutable::hasUniformBuffers() const
{
    return !getUniformBlocks().empty() ||
           (isCompute() ? mPipelineHasComputeUniformBuffers : mPipelineHasGraphicsUniformBuffers);
}

bool ProgramExecutable::hasStorageBuffers() const
{
    return !getShaderStorageBlocks().empty() ||
           (isCompute() ? mPipelineHasComputeStorageBuffers : mPipelineHasGraphicsStorageBuffers);
}

bool ProgramExecutable::hasAtomicCounterBuffers() const
{
    return !getAtomicCounterBuffers().empty() ||
           (isCompute() ? mPipelineHasComputeAtomicCounterBuffers
                        : mPipelineHasGraphicsAtomicCounterBuffers);
}

bool ProgramExecutable::hasImages() const
{
    return !getImageBindings().empty() ||
           (isCompute() ? mPipelineHasComputeImages : mPipelineHasGraphicsImages);
}

GLuint ProgramExecutable::getUniformIndexFromImageIndex(GLuint imageIndex) const
{
    ASSERT(imageIndex < mImageUniformRange.length());
    return imageIndex + mImageUniformRange.low();
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

void ProgramExecutable::updateActiveImages(const ProgramExecutable &executable)
{
    for (uint32_t imageIndex = 0; imageIndex < mImageBindings.size(); ++imageIndex)
    {
        const gl::ImageBinding &imageBinding = mImageBindings[imageIndex];
        if (imageBinding.unreferenced)
        {
            continue;
        }

        uint32_t uniformIndex = executable.getUniformIndexFromImageIndex(imageIndex);
        const gl::LinkedUniform &imageUniform = executable.getUniforms()[uniformIndex];
        const ShaderBitSet shaderBits         = imageUniform.activeShaders();
        for (GLint imageUnit : imageBinding.boundImageUnits)
        {
            mActiveImagesMask.set(imageUnit);
            if (isCompute())
            {
                mActiveImageShaderBits[imageUnit].set(gl::ShaderType::Compute);
            }
            else
            {
                mActiveImageShaderBits[imageUnit] = shaderBits;
            }
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

bool ProgramExecutable::linkValidateGlobalNames(
    InfoLog &infoLog,
    const ShaderMap<const ProgramState *> &programStates) const
{
    std::unordered_map<std::string, const sh::ShaderVariable *> uniformMap;
    using BlockAndFieldPair = std::pair<const sh::InterfaceBlock *, const sh::ShaderVariable *>;
    std::unordered_map<std::string, std::vector<BlockAndFieldPair>> uniformBlockFieldMap;

    for (ShaderType shaderType : kAllGraphicsShaderTypes)
    {
        const ProgramState *programState = programStates[shaderType];
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
    const ProgramState *programState = programStates[ShaderType::Vertex];
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

void ProgramExecutable::saveLinkedStateInfo(const ProgramState &state)
{
    for (ShaderType shaderType : getLinkedShaderStages())
    {
        Shader *shader = state.getAttachedShader(shaderType);
        ASSERT(shader);
        mLinkedOutputVaryings[shaderType] = shader->getOutputVaryings();
        mLinkedInputVaryings[shaderType]  = shader->getInputVaryings();
        mLinkedShaderVersions[shaderType] = shader->getShaderVersion();
    }
}

}  // namespace gl
