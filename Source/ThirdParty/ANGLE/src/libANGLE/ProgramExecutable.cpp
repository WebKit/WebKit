//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramExecutable.cpp: Collects the interfaces common to both Programs and
// ProgramPipelines in order to execute/draw with either.

#include "libANGLE/ProgramExecutable.h"

#include "libANGLE/Context.h"

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
      mActiveImagesMask(0)
{
    mActiveSamplerTypes.fill(TextureType::InvalidEnum);
    mActiveSamplerFormats.fill(SamplerFormat::InvalidEnum);
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
    //    ASSERT(mLinkResolved);
    ASSERT(attribLocation < mActiveAttribLocationsMask.size());
    return mActiveAttribLocationsMask[attribLocation];
}

AttributesMask ProgramExecutable::getAttributesMask() const
{
    // TODO(timvp): http://anglebug.com/3570: Enable this assert here somehow.
    //    ASSERT(mLinkResolved);
    return mAttributesMask;
}

bool ProgramExecutable::hasDefaultUniforms(const gl::State &glState) const
{
    ASSERT(mProgramState || mProgramPipelineState);
    if (mProgramState)
    {
        return mProgramState->hasDefaultUniforms();
    }

    // TODO(timvp): http://anglebug.com/3570: Support program pipelines

    return false;
}

bool ProgramExecutable::hasTextures(const gl::State &glState) const
{
    ASSERT(mProgramState || mProgramPipelineState);
    if (mProgramState)
    {
        return mProgramState->hasTextures();
    }

    // TODO(timvp): http://anglebug.com/3570: Support program pipelines

    return false;
}

bool ProgramExecutable::hasUniformBuffers(const gl::State &glState) const
{
    ASSERT(mProgramState || mProgramPipelineState);
    if (mProgramState)
    {
        return mProgramState->hasUniformBuffers();
    }

    // TODO(timvp): http://anglebug.com/3570: Support program pipelines

    return false;
}

bool ProgramExecutable::hasStorageBuffers(const gl::State &glState) const
{
    ASSERT(mProgramState || mProgramPipelineState);
    if (mProgramState)
    {
        return mProgramState->hasStorageBuffers();
    }

    // TODO(timvp): http://anglebug.com/3570: Support program pipelines

    return false;
}

bool ProgramExecutable::hasAtomicCounterBuffers(const gl::State &glState) const
{
    ASSERT(mProgramState || mProgramPipelineState);
    if (mProgramState)
    {
        return mProgramState->hasAtomicCounterBuffers();
    }

    // TODO(timvp): http://anglebug.com/3570: Support program pipelines

    return false;
}

bool ProgramExecutable::hasImages(const gl::State &glState) const
{
    ASSERT(mProgramState || mProgramPipelineState);
    if (mProgramState)
    {
        return mProgramState->hasImages();
    }

    // TODO(timvp): http://anglebug.com/3570: Support program pipelines

    return false;
}

bool ProgramExecutable::hasTransformFeedbackOutput(const gl::State &glState) const
{
    ASSERT(mProgramState || mProgramPipelineState);
    if (mProgramState)
    {
        return mProgramState->hasTransformFeedbackOutput();
    }

    // TODO(timvp): http://anglebug.com/3570: Support program pipelines

    return false;
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

void ProgramExecutable::updateActiveSamplers(const std::vector<SamplerBinding> &samplerBindings)
{
    for (const SamplerBinding &samplerBinding : samplerBindings)
    {
        if (samplerBinding.unreferenced)
            continue;

        for (GLint textureUnit : samplerBinding.boundTextureUnits)
        {
            if (++mActiveSamplerRefCounts[textureUnit] == 1)
            {
                mActiveSamplerTypes[textureUnit]   = samplerBinding.textureType;
                mActiveSamplerFormats[textureUnit] = samplerBinding.format;
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
    for (ImageBinding &imageBinding : imageBindings)
    {
        if (imageBinding.unreferenced)
            continue;

        for (GLint imageUnit : imageBinding.boundImageUnits)
        {
            mActiveImagesMask.set(imageUnit);
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

}  // namespace gl
