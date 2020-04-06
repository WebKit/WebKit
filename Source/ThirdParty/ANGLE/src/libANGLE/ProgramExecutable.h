//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramExecutable.h: Collects the information and interfaces common to both Programs and
// ProgramPipelines in order to execute/draw with either.

#ifndef LIBANGLE_PROGRAMEXECUTABLE_H_
#define LIBANGLE_PROGRAMEXECUTABLE_H_

#include "libANGLE/Caps.h"
#include "libANGLE/InfoLog.h"
#include "libANGLE/VaryingPacking.h"
#include "libANGLE/angletypes.h"

namespace gl
{

// This small structure encapsulates binding sampler uniforms to active GL textures.
struct SamplerBinding
{
    SamplerBinding(TextureType textureTypeIn,
                   SamplerFormat formatIn,
                   size_t elementCount,
                   bool unreferenced);
    SamplerBinding(const SamplerBinding &other);
    ~SamplerBinding();

    // Necessary for retrieving active textures from the GL state.
    TextureType textureType;

    SamplerFormat format;

    // List of all textures bound to this sampler, of type textureType.
    std::vector<GLuint> boundTextureUnits;

    // A note if this sampler is an unreferenced uniform.
    bool unreferenced;
};

struct ImageBinding
{
    ImageBinding(size_t count);
    ImageBinding(GLuint imageUnit, size_t count, bool unreferenced);
    ImageBinding(const ImageBinding &other);
    ~ImageBinding();

    std::vector<GLuint> boundImageUnits;

    // A note if this image unit is an unreferenced uniform.
    bool unreferenced;
};

class ProgramState;
class ProgramPipelineState;

class ProgramExecutable
{
  public:
    ProgramExecutable();
    virtual ~ProgramExecutable();

    void reset();

    int getInfoLogLength() const;
    InfoLog &getInfoLog() { return mInfoLog; }
    void getInfoLog(GLsizei bufSize, GLsizei *length, char *infoLog) const;
    std::string getInfoLogString() const;
    void resetInfoLog() { mInfoLog.reset(); }

    const ShaderBitSet &getLinkedShaderStages() const { return mLinkedShaderStages; }
    ShaderBitSet &getLinkedShaderStages() { return mLinkedShaderStages; }
    bool hasLinkedShaderStage(ShaderType shaderType) const
    {
        ASSERT(shaderType != ShaderType::InvalidEnum);
        return mLinkedShaderStages[shaderType];
    }
    size_t getLinkedShaderStageCount() const { return mLinkedShaderStages.count(); }
    bool isCompute() const { return hasLinkedShaderStage(ShaderType::Compute); }

    const AttributesMask &getActiveAttribLocationsMask() const
    {
        return mActiveAttribLocationsMask;
    }
    bool isAttribLocationActive(size_t attribLocation) const;
    const AttributesMask &getNonBuiltinAttribLocationsMask() const { return mAttributesMask; }
    unsigned int getMaxActiveAttribLocation() const { return mMaxActiveAttribLocation; }
    const ComponentTypeMask &getAttributesTypeMask() const { return mAttributesTypeMask; }
    AttributesMask getAttributesMask() const;

    const ActiveTextureMask &getActiveSamplersMask() const { return mActiveSamplersMask; }
    SamplerFormat getSamplerFormatForTextureUnitIndex(size_t textureUnitIndex) const
    {
        return mActiveSamplerFormats[textureUnitIndex];
    }
    const ActiveTextureMask &getActiveImagesMask() const { return mActiveImagesMask; }

    const ActiveTextureArray<TextureType> &getActiveSamplerTypes() const
    {
        return mActiveSamplerTypes;
    }

    bool hasDefaultUniforms(const gl::State &glState) const;
    bool hasTextures(const gl::State &glState) const;
    bool hasUniformBuffers(const gl::State &glState) const;
    bool hasStorageBuffers(const gl::State &glState) const;
    bool hasAtomicCounterBuffers(const gl::State &glState) const;
    bool hasImages(const gl::State &glState) const;
    bool hasTransformFeedbackOutput(const gl::State &glState) const;

    // Count the number of uniform and storage buffer declarations, counting arrays as one.
    size_t getTransformFeedbackBufferCount(const gl::State &glState) const;

    // TODO: http://anglebug.com/4520: Remove mProgramState/mProgramPipelineState
    void setProgramState(ProgramState *state)
    {
        ASSERT(!mProgramState && !mProgramPipelineState);
        mProgramState = state;
    }
    void setProgramPipelineState(ProgramPipelineState *state)
    {
        ASSERT(!mProgramState && !mProgramPipelineState);
        mProgramPipelineState = state;
    }

  private:
    // TODO(timvp): http://anglebug.com/3570: Investigate removing these friend
    // class declarations and accessing the necessary members with getters/setters.
    friend class Program;
    friend class ProgramPipeline;
    friend class ProgramState;

    void updateActiveSamplers(const std::vector<SamplerBinding> &samplerBindings);
    void updateActiveImages(std::vector<ImageBinding> &imageBindings);

    // Scans the sampler bindings for type conflicts with sampler 'textureUnitIndex'.
    void setSamplerUniformTextureTypeAndFormat(size_t textureUnitIndex,
                                               std::vector<SamplerBinding> &samplerBindings);

    // TODO: http://anglebug.com/4520: Remove mProgramState/mProgramPipelineState
    ProgramState *mProgramState;
    ProgramPipelineState *mProgramPipelineState;

    InfoLog mInfoLog;

    ShaderBitSet mLinkedShaderStages;

    angle::BitSet<MAX_VERTEX_ATTRIBS> mActiveAttribLocationsMask;
    unsigned int mMaxActiveAttribLocation;
    ComponentTypeMask mAttributesTypeMask;
    // mAttributesMask is identical to mActiveAttribLocationsMask with built-in attributes removed.
    AttributesMask mAttributesMask;

    // Cached mask of active samplers and sampler types.
    ActiveTextureMask mActiveSamplersMask;
    ActiveTextureArray<uint32_t> mActiveSamplerRefCounts;
    ActiveTextureArray<TextureType> mActiveSamplerTypes;
    ActiveTextureArray<SamplerFormat> mActiveSamplerFormats;

    // Cached mask of active images.
    ActiveTextureMask mActiveImagesMask;
};

}  // namespace gl

#endif  // LIBANGLE_PROGRAMEXECUTABLE_H_
