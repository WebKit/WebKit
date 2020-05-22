//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramExecutable.h: Collects the information and interfaces common to both Programs and
// ProgramPipelines in order to execute/draw with either.

#ifndef LIBANGLE_PROGRAMEXECUTABLE_H_
#define LIBANGLE_PROGRAMEXECUTABLE_H_

#include "BinaryStream.h"
#include "libANGLE/Caps.h"
#include "libANGLE/InfoLog.h"
#include "libANGLE/ProgramLinkedResources.h"
#include "libANGLE/Shader.h"
#include "libANGLE/Uniform.h"
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

// A varying with transform feedback enabled. If it's an array, either the whole array or one of its
// elements specified by 'arrayIndex' can set to be enabled.
struct TransformFeedbackVarying : public sh::ShaderVariable
{
    TransformFeedbackVarying(const sh::ShaderVariable &varyingIn, GLuint arrayIndexIn)
        : sh::ShaderVariable(varyingIn), arrayIndex(arrayIndexIn)
    {
        ASSERT(!isArrayOfArrays());
    }

    TransformFeedbackVarying(const sh::ShaderVariable &field, const sh::ShaderVariable &parent)
        : arrayIndex(GL_INVALID_INDEX)
    {
        sh::ShaderVariable *thisVar = this;
        *thisVar                    = field;
        interpolation               = parent.interpolation;
        isInvariant                 = parent.isInvariant;
        name                        = parent.name + "." + name;
        mappedName                  = parent.mappedName + "." + mappedName;
    }

    std::string nameWithArrayIndex() const
    {
        std::stringstream fullNameStr;
        fullNameStr << name;
        if (arrayIndex != GL_INVALID_INDEX)
        {
            fullNameStr << "[" << arrayIndex << "]";
        }
        return fullNameStr.str();
    }
    GLsizei size() const
    {
        return (isArray() && arrayIndex == GL_INVALID_INDEX ? getOutermostArraySize() : 1);
    }

    GLuint arrayIndex;
};

class ProgramState;
class ProgramPipelineState;

class ProgramExecutable
{
  public:
    ProgramExecutable();
    ProgramExecutable(const ProgramExecutable &other);
    virtual ~ProgramExecutable();

    void reset();

    void save(gl::BinaryOutputStream *stream) const;
    void load(gl::BinaryInputStream *stream);

    const ProgramState *getProgramState(ShaderType shaderType) const;

    int getInfoLogLength() const;
    InfoLog &getInfoLog() { return mInfoLog; }
    void getInfoLog(GLsizei bufSize, GLsizei *length, char *infoLog) const;
    std::string getInfoLogString() const;
    void resetInfoLog() { mInfoLog.reset(); }

    void resetLinkedShaderStages()
    {
        mLinkedComputeShaderStages.reset();
        mLinkedGraphicsShaderStages.reset();
    }
    const ShaderBitSet &getLinkedShaderStages() const
    {
        return isCompute() ? mLinkedComputeShaderStages : mLinkedGraphicsShaderStages;
    }
    void setLinkedShaderStages(ShaderType shaderType)
    {
        if (shaderType == ShaderType::Compute)
        {
            mLinkedComputeShaderStages.set(ShaderType::Compute);
        }
        else
        {
            mLinkedGraphicsShaderStages.set(shaderType);
        }

        updateCanDrawWith();
    }
    bool hasLinkedShaderStage(ShaderType shaderType) const
    {
        ASSERT(shaderType != ShaderType::InvalidEnum);
        return (shaderType == ShaderType::Compute) ? mLinkedComputeShaderStages[shaderType]
                                                   : mLinkedGraphicsShaderStages[shaderType];
    }
    size_t getLinkedShaderStageCount() const
    {
        return isCompute() ? mLinkedComputeShaderStages.count()
                           : mLinkedGraphicsShaderStages.count();
    }
    bool isCompute() const;

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
    const ShaderBitSet getSamplerShaderBitsForTextureUnitIndex(size_t textureUnitIndex) const
    {
        return mActiveSamplerShaderBits[textureUnitIndex];
    }
    const ActiveTextureMask &getActiveImagesMask() const { return mActiveImagesMask; }
    const ActiveTextureArray<ShaderBitSet> &getActiveImageShaderBits() const
    {
        return mActiveImageShaderBits;
    }

    const ActiveTextureArray<TextureType> &getActiveSamplerTypes() const
    {
        return mActiveSamplerTypes;
    }

    bool hasDefaultUniforms() const;
    bool hasTextures() const;
    bool hasUniformBuffers() const;
    bool hasStorageBuffers() const;
    bool hasAtomicCounterBuffers() const;
    bool hasImages() const;
    bool hasTransformFeedbackOutput() const;

    // Count the number of uniform and storage buffer declarations, counting arrays as one.
    size_t getTransformFeedbackBufferCount(const gl::State &glState) const;

    bool linkValidateGlobalNames(InfoLog &infoLog) const;

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

    void setIsCompute(bool isComputeIn);

    void updateCanDrawWith();
    bool hasVertexAndFragmentShader() const { return mCanDrawWith; }

    const std::vector<sh::ShaderVariable> &getProgramInputs() const { return mProgramInputs; }
    const std::vector<sh::ShaderVariable> &getOutputVariables() const { return mOutputVariables; }
    const std::vector<VariableLocation> &getOutputLocations() const { return mOutputLocations; }
    const std::vector<LinkedUniform> &getUniforms() const { return mUniforms; }
    const std::vector<InterfaceBlock> &getUniformBlocks() const { return mUniformBlocks; }
    const RangeUI &getSamplerUniformRange() const { return mSamplerUniformRange; }
    const RangeUI &getImageUniformRange() const { return mImageUniformRange; }
    const std::vector<TransformFeedbackVarying> &getLinkedTransformFeedbackVaryings() const
    {
        return mLinkedTransformFeedbackVaryings;
    }
    GLint getTransformFeedbackBufferMode() const { return mTransformFeedbackBufferMode; }
    GLuint getUniformBlockBinding(GLuint uniformBlockIndex) const
    {
        ASSERT(uniformBlockIndex < mUniformBlocks.size());
        return mUniformBlocks[uniformBlockIndex].binding;
    }
    GLuint getShaderStorageBlockBinding(GLuint blockIndex) const
    {
        ASSERT(blockIndex < mShaderStorageBlocks.size());
        return mShaderStorageBlocks[blockIndex].binding;
    }
    const std::vector<GLsizei> &getTransformFeedbackStrides() const
    {
        return mTransformFeedbackStrides;
    }
    const std::vector<AtomicCounterBuffer> &getAtomicCounterBuffers() const
    {
        return mAtomicCounterBuffers;
    }
    const std::vector<InterfaceBlock> &getShaderStorageBlocks() const
    {
        return mShaderStorageBlocks;
    }
    const LinkedUniform &getUniformByIndex(GLuint index) const
    {
        ASSERT(index < static_cast<size_t>(mUniforms.size()));
        return mUniforms[index];
    }

    ANGLE_INLINE GLuint getActiveUniformBlockCount() const
    {
        return static_cast<GLuint>(mUniformBlocks.size());
    }

    ANGLE_INLINE GLuint getActiveAtomicCounterBufferCount() const
    {
        return static_cast<GLuint>(mAtomicCounterBuffers.size());
    }

    ANGLE_INLINE GLuint getActiveShaderStorageBlockCount() const
    {
        return static_cast<GLuint>(mShaderStorageBlocks.size());
    }

    gl::ProgramLinkedResources &getResources() const
    {
        ASSERT(mResources);
        return *mResources;
    }

    void saveLinkedStateInfo();
    std::vector<sh::ShaderVariable> getLinkedOutputVaryings(ShaderType shaderType)
    {
        return mLinkedOutputVaryings[shaderType];
    }
    std::vector<sh::ShaderVariable> getLinkedInputVaryings(ShaderType shaderType)
    {
        return mLinkedInputVaryings[shaderType];
    }
    int getLinkedShaderVersion(ShaderType shaderType) { return mLinkedShaderVersions[shaderType]; }

  private:
    // TODO(timvp): http://anglebug.com/3570: Investigate removing these friend
    // class declarations and accessing the necessary members with getters/setters.
    friend class Program;
    friend class ProgramPipeline;
    friend class ProgramState;

    void updateActiveSamplers(const ProgramState &programState);
    void updateActiveImages(std::vector<ImageBinding> &imageBindings);

    // Scans the sampler bindings for type conflicts with sampler 'textureUnitIndex'.
    void setSamplerUniformTextureTypeAndFormat(size_t textureUnitIndex,
                                               std::vector<SamplerBinding> &samplerBindings);

    // TODO: http://anglebug.com/4520: Remove mProgramState/mProgramPipelineState
    ProgramState *mProgramState;
    ProgramPipelineState *mProgramPipelineState;

    InfoLog mInfoLog;

    ShaderBitSet mLinkedGraphicsShaderStages;
    ShaderBitSet mLinkedComputeShaderStages;

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
    ActiveTextureArray<ShaderBitSet> mActiveSamplerShaderBits;

    // Cached mask of active images.
    ActiveTextureMask mActiveImagesMask;
    ActiveTextureArray<ShaderBitSet> mActiveImageShaderBits;

    bool mCanDrawWith;

    // Names and mapped names of output variables that are arrays include [0] in the end, similarly
    // to uniforms.
    std::vector<sh::ShaderVariable> mOutputVariables;
    std::vector<VariableLocation> mOutputLocations;
    // Vertex attributes, Fragment input varyings, etc.
    std::vector<sh::ShaderVariable> mProgramInputs;
    std::vector<TransformFeedbackVarying> mLinkedTransformFeedbackVaryings;
    // The size of the data written to each transform feedback buffer per vertex.
    std::vector<GLsizei> mTransformFeedbackStrides;
    GLenum mTransformFeedbackBufferMode;
    // Uniforms are sorted in order:
    //  1. Non-opaque uniforms
    //  2. Sampler uniforms
    //  3. Image uniforms
    //  4. Atomic counter uniforms
    //  5. Uniform block uniforms
    // This makes opaque uniform validation easier, since we don't need a separate list.
    // For generating the entries and naming them we follow the spec: GLES 3.1 November 2016 section
    // 7.3.1.1 Naming Active Resources. There's a separate entry for each struct member and each
    // inner array of an array of arrays. Names and mapped names of uniforms that are arrays include
    // [0] in the end. This makes implementation of queries simpler.
    std::vector<LinkedUniform> mUniforms;
    RangeUI mSamplerUniformRange;
    std::vector<InterfaceBlock> mUniformBlocks;
    std::vector<AtomicCounterBuffer> mAtomicCounterBuffers;
    RangeUI mImageUniformRange;
    std::vector<InterfaceBlock> mShaderStorageBlocks;

    ShaderMap<std::vector<sh::ShaderVariable>> mLinkedOutputVaryings;
    ShaderMap<std::vector<sh::ShaderVariable>> mLinkedInputVaryings;
    ShaderMap<int> mLinkedShaderVersions;
    // TODO: http://anglebug.com/4514: Remove
    std::unique_ptr<gl::ProgramLinkedResources> mResources;
};

}  // namespace gl

#endif  // LIBANGLE_PROGRAMEXECUTABLE_H_
