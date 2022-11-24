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
                   GLenum samplerTypeIn,
                   SamplerFormat formatIn,
                   size_t elementCount);
    SamplerBinding(const SamplerBinding &other);
    ~SamplerBinding();

    // Necessary for retrieving active textures from the GL state.
    TextureType textureType;

    GLenum samplerType;

    SamplerFormat format;

    // List of all textures bound to this sampler, of type textureType.
    // Cropped by the amount of unused elements reported by the driver.
    std::vector<GLuint> boundTextureUnits;
};

struct ImageBinding
{
    ImageBinding(size_t count, TextureType textureTypeIn);
    ImageBinding(GLuint imageUnit, size_t count, TextureType textureTypeIn);
    ImageBinding(const ImageBinding &other);
    ~ImageBinding();

    // Necessary for distinguishing between textures with images and texture buffers.
    TextureType textureType;

    // List of all textures bound.
    // Cropped by the amount of unused elements reported by the driver.
    std::vector<GLuint> boundImageUnits;
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
        ASSERT(parent.isShaderIOBlock || !parent.name.empty());
        if (!parent.name.empty())
        {
            name       = parent.name + "." + name;
            mappedName = parent.mappedName + "." + mappedName;
        }
        structOrBlockName       = parent.structOrBlockName;
        mappedStructOrBlockName = parent.mappedStructOrBlockName;
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

class ProgramExecutable final : public angle::Subject
{
  public:
    ProgramExecutable();
    ProgramExecutable(const ProgramExecutable &other);
    ~ProgramExecutable() override;

    void reset(bool clearInfoLog);

    void save(bool isSeparable, gl::BinaryOutputStream *stream) const;
    void load(bool isSeparable, gl::BinaryInputStream *stream);

    int getInfoLogLength() const;
    InfoLog &getInfoLog() { return mInfoLog; }
    void getInfoLog(GLsizei bufSize, GLsizei *length, char *infoLog) const;
    std::string getInfoLogString() const;
    void resetInfoLog() { mInfoLog.reset(); }

    void resetLinkedShaderStages() { mLinkedShaderStages.reset(); }
    const ShaderBitSet &getLinkedShaderStages() const { return mLinkedShaderStages; }
    void setLinkedShaderStages(ShaderType shaderType)
    {
        mLinkedShaderStages.set(shaderType);
        updateCanDrawWith();
    }
    bool hasLinkedShaderStage(ShaderType shaderType) const
    {
        ASSERT(shaderType != ShaderType::InvalidEnum);
        return mLinkedShaderStages[shaderType];
    }
    size_t getLinkedShaderStageCount() const { return mLinkedShaderStages.count(); }
    bool hasLinkedGraphicsShader() const
    {
        return mLinkedShaderStages.any() &&
               mLinkedShaderStages != gl::ShaderBitSet{gl::ShaderType::Compute};
    }
    bool hasLinkedTessellationShader() const
    {
        return mLinkedShaderStages[ShaderType::TessEvaluation];
    }

    ShaderType getLinkedTransformFeedbackStage() const;

    const AttributesMask &getActiveAttribLocationsMask() const
    {
        return mActiveAttribLocationsMask;
    }
    bool isAttribLocationActive(size_t attribLocation) const;
    AttributesMask getNonBuiltinAttribLocationsMask() const { return mAttributesMask; }
    unsigned int getMaxActiveAttribLocation() const { return mMaxActiveAttribLocation; }
    ComponentTypeMask getAttributesTypeMask() const { return mAttributesTypeMask; }
    AttributesMask getAttributesMask() const;

    const ActiveTextureMask &getActiveSamplersMask() const { return mActiveSamplersMask; }
    void setActiveTextureMask(ActiveTextureMask mask) { mActiveSamplersMask = mask; }
    SamplerFormat getSamplerFormatForTextureUnitIndex(size_t textureUnitIndex) const
    {
        return mActiveSamplerFormats[textureUnitIndex];
    }
    const ShaderBitSet getSamplerShaderBitsForTextureUnitIndex(size_t textureUnitIndex) const
    {
        return mActiveSamplerShaderBits[textureUnitIndex];
    }
    const ActiveTextureMask &getActiveImagesMask() const { return mActiveImagesMask; }
    void setActiveImagesMask(ActiveTextureMask mask) { mActiveImagesMask = mask; }
    const ActiveTextureArray<ShaderBitSet> &getActiveImageShaderBits() const
    {
        return mActiveImageShaderBits;
    }

    const ActiveTextureMask &getActiveYUVSamplers() const { return mActiveSamplerYUV; }

    const ActiveTextureArray<TextureType> &getActiveSamplerTypes() const
    {
        return mActiveSamplerTypes;
    }

    void setActive(size_t textureUnit,
                   const SamplerBinding &samplerBinding,
                   const gl::LinkedUniform &samplerUniform);
    void setInactive(size_t textureUnit);
    void hasSamplerTypeConflict(size_t textureUnit);
    void hasSamplerFormatConflict(size_t textureUnit);

    void updateActiveSamplers(const ProgramState &programState);

    bool hasDefaultUniforms() const;
    bool hasTextures() const;
    bool hasUniformBuffers() const;
    bool hasStorageBuffers() const;
    bool hasAtomicCounterBuffers() const;
    bool hasImages() const;
    bool hasTransformFeedbackOutput() const
    {
        return !getLinkedTransformFeedbackVaryings().empty();
    }
    bool usesFramebufferFetch() const;

    // Count the number of uniform and storage buffer declarations, counting arrays as one.
    size_t getTransformFeedbackBufferCount() const { return mTransformFeedbackStrides.size(); }

    void updateCanDrawWith();
    bool hasVertexShader() const { return mCanDrawWith; }

    const std::vector<sh::ShaderVariable> &getProgramInputs() const { return mProgramInputs; }
    const std::vector<sh::ShaderVariable> &getOutputVariables() const { return mOutputVariables; }
    const std::vector<VariableLocation> &getOutputLocations() const { return mOutputLocations; }
    const std::vector<VariableLocation> &getSecondaryOutputLocations() const
    {
        return mSecondaryOutputLocations;
    }
    const std::vector<LinkedUniform> &getUniforms() const { return mUniforms; }
    const std::vector<InterfaceBlock> &getUniformBlocks() const { return mUniformBlocks; }
    const UniformBlockBindingMask &getActiveUniformBlockBindings() const
    {
        return mActiveUniformBlockBindings;
    }
    const std::vector<SamplerBinding> &getSamplerBindings() const { return mSamplerBindings; }
    const std::vector<ImageBinding> &getImageBindings() const { return mImageBindings; }
    std::vector<ImageBinding> *getImageBindings() { return &mImageBindings; }
    const RangeUI &getDefaultUniformRange() const { return mDefaultUniformRange; }
    const RangeUI &getSamplerUniformRange() const { return mSamplerUniformRange; }
    const RangeUI &getImageUniformRange() const { return mImageUniformRange; }
    const RangeUI &getAtomicCounterUniformRange() const { return mAtomicCounterUniformRange; }
    const RangeUI &getFragmentInoutRange() const { return mFragmentInoutRange; }
    bool hasDiscard() const { return mHasDiscard; }
    bool enablesPerSampleShading() const { return mEnablesPerSampleShading; }
    BlendEquationBitSet getAdvancedBlendEquations() const { return mAdvancedBlendEquations; }
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
        size_t shaderStorageBlocksSize = mShaderStorageBlocks.size();
        return static_cast<GLuint>(shaderStorageBlocksSize);
    }

    GLuint getUniformIndexFromImageIndex(GLuint imageIndex) const;

    GLuint getUniformIndexFromSamplerIndex(GLuint samplerIndex) const;

    void saveLinkedStateInfo(const Context *context, const ProgramState &state);
    const std::vector<sh::ShaderVariable> &getLinkedOutputVaryings(ShaderType shaderType) const
    {
        return mLinkedOutputVaryings[shaderType];
    }
    const std::vector<sh::ShaderVariable> &getLinkedInputVaryings(ShaderType shaderType) const
    {
        return mLinkedInputVaryings[shaderType];
    }

    const std::vector<sh::ShaderVariable> &getLinkedUniforms(ShaderType shaderType) const
    {
        return mLinkedUniforms[shaderType];
    }

    const std::vector<sh::InterfaceBlock> &getLinkedUniformBlocks(ShaderType shaderType) const
    {
        return mLinkedUniformBlocks[shaderType];
    }

    int getLinkedShaderVersion(ShaderType shaderType) const
    {
        return mLinkedShaderVersions[shaderType];
    }

    bool isYUVOutput() const;

    PrimitiveMode getGeometryShaderInputPrimitiveType() const
    {
        return mGeometryShaderInputPrimitiveType;
    }

    PrimitiveMode getGeometryShaderOutputPrimitiveType() const
    {
        return mGeometryShaderOutputPrimitiveType;
    }

    int getGeometryShaderInvocations() const { return mGeometryShaderInvocations; }

    int getGeometryShaderMaxVertices() const { return mGeometryShaderMaxVertices; }

    GLenum getTessGenMode() const { return mTessGenMode; }

    void resetCachedValidateSamplersResult() { mCachedValidateSamplersResult.reset(); }
    bool validateSamplers(InfoLog *infoLog, const Caps &caps) const
    {
        // Use the cache if:
        // - we aren't using an info log (which gives the full error).
        // - The sample mapping hasn't changed and we've already validated.
        if (infoLog == nullptr && mCachedValidateSamplersResult.valid())
        {
            return mCachedValidateSamplersResult.value();
        }

        return validateSamplersImpl(infoLog, caps);
    }

    ComponentTypeMask getFragmentOutputsTypeMask() const { return mDrawBufferTypeMask; }
    DrawBufferMask getActiveOutputVariablesMask() const { return mActiveOutputVariablesMask; }

    bool linkUniforms(const Context *context,
                      const ShaderMap<std::vector<sh::ShaderVariable>> &shaderUniforms,
                      InfoLog &infoLog,
                      const ProgramAliasedBindings &uniformLocationBindings,
                      GLuint *combinedImageUniformsCount,
                      std::vector<UnusedUniform> *unusedUniforms,
                      std::vector<VariableLocation> *uniformLocationsOutOrNull);

    void copyInputsFromProgram(const ProgramState &programState);
    void copyShaderBuffersFromProgram(const ProgramState &programState, ShaderType shaderType);
    void clearSamplerBindings();
    void copySamplerBindingsFromProgram(const ProgramState &programState);
    void copyImageBindingsFromProgram(const ProgramState &programState);
    void copyOutputsFromProgram(const ProgramState &programState);
    void copyUniformsFromProgramMap(const ShaderMap<Program *> &programs);

  private:
    friend class Program;
    friend class ProgramPipeline;
    friend class ProgramState;

    void updateActiveImages(const ProgramExecutable &executable);

    // Scans the sampler bindings for type conflicts with sampler 'textureUnitIndex'.
    void setSamplerUniformTextureTypeAndFormat(size_t textureUnitIndex,
                                               std::vector<SamplerBinding> &samplerBindings);

    bool linkMergedVaryings(const Context *context,
                            const ProgramMergedVaryings &mergedVaryings,
                            const std::vector<std::string> &transformFeedbackVaryingNames,
                            const LinkingVariables &linkingVariables,
                            bool isSeparable,
                            ProgramVaryingPacking *varyingPacking);

    bool linkValidateTransformFeedback(
        const Context *context,
        const ProgramMergedVaryings &varyings,
        ShaderType stage,
        const std::vector<std::string> &transformFeedbackVaryingNames);

    void gatherTransformFeedbackVaryings(
        const ProgramMergedVaryings &varyings,
        ShaderType stage,
        const std::vector<std::string> &transformFeedbackVaryingNames);

    void updateTransformFeedbackStrides();

    bool validateSamplersImpl(InfoLog *infoLog, const Caps &caps) const;

    bool linkValidateOutputVariables(const Caps &caps,
                                     const Extensions &extensions,
                                     const Version &version,
                                     GLuint combinedImageUniformsCount,
                                     GLuint combinedShaderStorageBlocksCount,
                                     const std::vector<sh::ShaderVariable> &outputVariables,
                                     int fragmentShaderVersion,
                                     const ProgramAliasedBindings &fragmentOutputLocations,
                                     const ProgramAliasedBindings &fragmentOutputIndices);

    void linkSamplerAndImageBindings(GLuint *combinedImageUniformsCount);
    bool linkAtomicCounterBuffers(const Context *context, InfoLog &infoLog);

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
    ActiveTextureMask mActiveSamplerYUV;
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
    DrawBufferMask mActiveOutputVariablesMask;
    // EXT_blend_func_extended secondary outputs (ones with index 1)
    std::vector<VariableLocation> mSecondaryOutputLocations;
    bool mYUVOutput;
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
    //  5. Subpass Input uniforms (Only for Vulkan)
    //  6. Uniform block uniforms
    // This makes opaque uniform validation easier, since we don't need a separate list.
    // For generating the entries and naming them we follow the spec: GLES 3.1 November 2016 section
    // 7.3.1.1 Naming Active Resources. There's a separate entry for each struct member and each
    // inner array of an array of arrays. Names and mapped names of uniforms that are arrays include
    // [0] in the end. This makes implementation of queries simpler.
    std::vector<LinkedUniform> mUniforms;
    RangeUI mDefaultUniformRange;
    RangeUI mSamplerUniformRange;
    RangeUI mImageUniformRange;
    RangeUI mAtomicCounterUniformRange;
    std::vector<InterfaceBlock> mUniformBlocks;

    // For faster iteration on the blocks currently being bound.
    UniformBlockBindingMask mActiveUniformBlockBindings;

    std::vector<AtomicCounterBuffer> mAtomicCounterBuffers;
    std::vector<InterfaceBlock> mShaderStorageBlocks;

    RangeUI mFragmentInoutRange;
    bool mHasDiscard;
    bool mEnablesPerSampleShading;

    // KHR_blend_equation_advanced supported equation list
    BlendEquationBitSet mAdvancedBlendEquations;

    // An array of the samplers that are used by the program
    std::vector<SamplerBinding> mSamplerBindings;

    // An array of the images that are used by the program
    std::vector<ImageBinding> mImageBindings;

    ShaderMap<std::vector<sh::ShaderVariable>> mLinkedOutputVaryings;
    ShaderMap<std::vector<sh::ShaderVariable>> mLinkedInputVaryings;
    ShaderMap<std::vector<sh::ShaderVariable>> mLinkedUniforms;
    ShaderMap<std::vector<sh::InterfaceBlock>> mLinkedUniformBlocks;

    ShaderMap<int> mLinkedShaderVersions;

    // GL_EXT_geometry_shader.
    PrimitiveMode mGeometryShaderInputPrimitiveType;
    PrimitiveMode mGeometryShaderOutputPrimitiveType;
    int mGeometryShaderInvocations;
    int mGeometryShaderMaxVertices;

    // GL_EXT_tessellation_shader
    int mTessControlShaderVertices;
    GLenum mTessGenMode;
    GLenum mTessGenSpacing;
    GLenum mTessGenVertexOrder;
    GLenum mTessGenPointMode;

    // Fragment output variable base types: FLOAT, INT, or UINT.  Ordered by location.
    std::vector<GLenum> mOutputVariableTypes;
    ComponentTypeMask mDrawBufferTypeMask;

    // Cache for sampler validation
    mutable Optional<bool> mCachedValidateSamplersResult;
};
}  // namespace gl

#endif  // LIBANGLE_PROGRAMEXECUTABLE_H_
