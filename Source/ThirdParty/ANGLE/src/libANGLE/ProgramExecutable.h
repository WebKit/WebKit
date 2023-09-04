//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramExecutable.h: Collects the information and interfaces common to both Programs and
// ProgramPipelines in order to execute/draw with either.

#ifndef LIBANGLE_PROGRAMEXECUTABLE_H_
#define LIBANGLE_PROGRAMEXECUTABLE_H_

#include "common/BinaryStream.h"
#include "libANGLE/Caps.h"
#include "libANGLE/InfoLog.h"
#include "libANGLE/ProgramLinkedResources.h"
#include "libANGLE/Shader.h"
#include "libANGLE/Uniform.h"
#include "libANGLE/VaryingPacking.h"
#include "libANGLE/angletypes.h"

namespace rx
{
class GLImplFactory;
class ProgramExecutableImpl;
}  // namespace rx

namespace gl
{

// This small structure encapsulates binding sampler uniforms to active GL textures.
ANGLE_ENABLE_STRUCT_PADDING_WARNINGS
struct SamplerBinding
{
    SamplerBinding() = default;
    SamplerBinding(TextureType textureTypeIn,
                   GLenum samplerTypeIn,
                   SamplerFormat formatIn,
                   uint16_t startIndex,
                   uint16_t elementCount)
        : textureType(textureTypeIn),
          format(formatIn),
          textureUnitsStartIndex(startIndex),
          textureUnitsCount(elementCount)
    {
        SetBitField(samplerType, samplerTypeIn);
    }
    ~SamplerBinding() = default;

    GLuint getTextureUnit(const std::vector<GLuint> &boundTextureUnits,
                          unsigned int arrayIndex) const
    {
        return boundTextureUnits[textureUnitsStartIndex + arrayIndex];
    }

    // Necessary for retrieving active textures from the GL state.
    TextureType textureType;
    SamplerFormat format;
    uint16_t samplerType;
    // [textureUnitsStartIndex, textureUnitsStartIndex+textureUnitsCount) Points to the subset in
    // mSamplerBoundTextureUnits that stores the texture unit bound to this sampler. Cropped by the
    // amount of unused elements reported by the driver.
    uint16_t textureUnitsStartIndex;
    uint16_t textureUnitsCount;
};
static_assert(std::is_trivially_copyable<SamplerBinding>(), "must be memcpy-able");
ANGLE_DISABLE_STRUCT_PADDING_WARNINGS

struct ImageBinding
{
    ImageBinding();
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

ANGLE_ENABLE_STRUCT_PADDING_WARNINGS
struct ProgramInput
{
    ProgramInput();
    ProgramInput(const sh::ShaderVariable &var);
    ProgramInput(const ProgramInput &other);
    ProgramInput &operator=(const ProgramInput &rhs);

    GLenum getType() const { return basicDataTypeStruct.type; }
    bool isBuiltIn() const { return basicDataTypeStruct.flagBits.isBuiltIn; }
    bool isArray() const { return basicDataTypeStruct.flagBits.isArray; }
    bool isActive() const { return basicDataTypeStruct.flagBits.active; }
    bool isPatch() const { return basicDataTypeStruct.flagBits.isPatch; }
    int getLocation() const { return basicDataTypeStruct.location; }
    unsigned int getBasicTypeElementCount() const
    {
        return basicDataTypeStruct.basicTypeElementCount;
    }
    unsigned int getArraySizeProduct() const { return basicDataTypeStruct.arraySizeProduct; }
    uint32_t getId() const { return basicDataTypeStruct.id; }
    sh::InterpolationType getInterpolation() const
    {
        return static_cast<sh::InterpolationType>(basicDataTypeStruct.interpolation);
    }

    void setLocation(int location) { basicDataTypeStruct.location = location; }
    void resetEffectiveLocation()
    {
        if (basicDataTypeStruct.flagBits.hasImplicitLocation)
        {
            basicDataTypeStruct.location = -1;
        }
    }

    std::string name;
    std::string mappedName;

    // The struct bellow must only contain data of basic type so that entire struct can memcpy-able.
    struct PODStruct
    {
        uint16_t type;  // GLenum
        uint16_t arraySizeProduct;

        int location;

        uint8_t interpolation;  // sh::InterpolationType
        union
        {
            struct
            {
                uint8_t active : 1;
                uint8_t isPatch : 1;
                uint8_t hasImplicitLocation : 1;
                uint8_t isArray : 1;
                uint8_t isBuiltIn : 1;
                uint8_t padding : 3;
            } flagBits;
            uint8_t flagBitsAsUByte;
        };
        int16_t basicTypeElementCount;

        uint32_t id;
    } basicDataTypeStruct;
    static_assert(std::is_trivially_copyable<PODStruct>(), "must be memcpy-able");
};
ANGLE_DISABLE_STRUCT_PADDING_WARNINGS

// A varying with transform feedback enabled. If it's an array, either the whole array or one of its
// elements specified by 'arrayIndex' can set to be enabled.
struct TransformFeedbackVarying : public sh::ShaderVariable
{
    TransformFeedbackVarying() = default;

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
    ProgramExecutable(rx::GLImplFactory *factory, InfoLog *infoLog);
    ~ProgramExecutable() override;

    void destroy(const Context *context);

    ANGLE_INLINE rx::ProgramExecutableImpl *getImplementation() const { return mImplementation; }

    void save(bool isSeparable, gl::BinaryOutputStream *stream) const;
    void load(bool isSeparable, gl::BinaryInputStream *stream);

    InfoLog &getInfoLog() const { return *mInfoLog; }
    std::string getInfoLogString() const;
    void resetInfoLog() const { mInfoLog->reset(); }

    void resetLinkedShaderStages() { mPODStruct.linkedShaderStages.reset(); }
    const ShaderBitSet getLinkedShaderStages() const { return mPODStruct.linkedShaderStages; }
    void setLinkedShaderStages(ShaderType shaderType)
    {
        mPODStruct.linkedShaderStages.set(shaderType);
        updateCanDrawWith();
    }
    bool hasLinkedShaderStage(ShaderType shaderType) const
    {
        ASSERT(shaderType != ShaderType::InvalidEnum);
        return mPODStruct.linkedShaderStages[shaderType];
    }
    size_t getLinkedShaderStageCount() const { return mPODStruct.linkedShaderStages.count(); }
    bool hasLinkedGraphicsShader() const
    {
        return mPODStruct.linkedShaderStages.any() &&
               mPODStruct.linkedShaderStages != gl::ShaderBitSet{gl::ShaderType::Compute};
    }
    bool hasLinkedTessellationShader() const
    {
        return mPODStruct.linkedShaderStages[ShaderType::TessEvaluation];
    }

    ShaderType getLinkedTransformFeedbackStage() const
    {
        return GetLastPreFragmentStage(mPODStruct.linkedShaderStages);
    }

    const AttributesMask &getActiveAttribLocationsMask() const
    {
        return mPODStruct.activeAttribLocationsMask;
    }
    bool isAttribLocationActive(size_t attribLocation) const
    {
        ASSERT(attribLocation < mPODStruct.activeAttribLocationsMask.size());
        return mPODStruct.activeAttribLocationsMask[attribLocation];
    }

    AttributesMask getNonBuiltinAttribLocationsMask() const { return mPODStruct.attributesMask; }
    unsigned int getMaxActiveAttribLocation() const { return mPODStruct.maxActiveAttribLocation; }
    ComponentTypeMask getAttributesTypeMask() const { return mPODStruct.attributesTypeMask; }
    AttributesMask getAttributesMask() const { return mPODStruct.attributesMask; }

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

    bool hasDefaultUniforms() const { return !getDefaultUniformRange().empty(); }
    bool hasTextures() const { return !getSamplerBindings().empty(); }
    bool hasUniformBuffers() const { return !mUniformBlocks.empty(); }
    bool hasStorageBuffers() const { return !mShaderStorageBlocks.empty(); }
    bool hasAtomicCounterBuffers() const { return !mAtomicCounterBuffers.empty(); }
    bool hasImages() const { return !mImageBindings.empty(); }
    bool hasTransformFeedbackOutput() const
    {
        return !getLinkedTransformFeedbackVaryings().empty();
    }
    bool usesFramebufferFetch() const { return (mPODStruct.fragmentInoutRange.length() > 0); }

    // Count the number of uniform and storage buffer declarations, counting arrays as one.
    size_t getTransformFeedbackBufferCount() const { return mTransformFeedbackStrides.size(); }

    void updateCanDrawWith() { mPODStruct.canDrawWith = hasLinkedShaderStage(ShaderType::Vertex); }
    bool hasVertexShader() const { return mPODStruct.canDrawWith; }

    const std::vector<ProgramInput> &getProgramInputs() const { return mProgramInputs; }
    const std::vector<sh::ShaderVariable> &getOutputVariables() const { return mOutputVariables; }
    const std::vector<VariableLocation> &getOutputLocations() const { return mOutputLocations; }
    const std::vector<VariableLocation> &getSecondaryOutputLocations() const
    {
        return mSecondaryOutputLocations;
    }
    const std::vector<LinkedUniform> &getUniforms() const { return mUniforms; }
    const std::vector<std::string> &getUniformNames() const { return mUniformNames; }
    const std::vector<std::string> &getUniformMappedNames() const { return mUniformMappedNames; }
    const std::vector<InterfaceBlock> &getUniformBlocks() const { return mUniformBlocks; }
    const std::vector<VariableLocation> &getUniformLocations() const { return mUniformLocations; }
    const UniformBlockBindingMask &getActiveUniformBlockBindings() const
    {
        return mPODStruct.activeUniformBlockBindings;
    }
    const std::vector<SamplerBinding> &getSamplerBindings() const { return mSamplerBindings; }
    const std::vector<GLuint> &getSamplerBoundTextureUnits() const
    {
        return mSamplerBoundTextureUnits;
    }
    const std::vector<ImageBinding> &getImageBindings() const { return mImageBindings; }
    std::vector<ImageBinding> *getImageBindings() { return &mImageBindings; }
    const RangeUI &getDefaultUniformRange() const { return mPODStruct.defaultUniformRange; }
    const RangeUI &getSamplerUniformRange() const { return mPODStruct.samplerUniformRange; }
    const RangeUI &getImageUniformRange() const { return mPODStruct.imageUniformRange; }
    const RangeUI &getAtomicCounterUniformRange() const
    {
        return mPODStruct.atomicCounterUniformRange;
    }
    const RangeUI &getFragmentInoutRange() const { return mPODStruct.fragmentInoutRange; }
    bool hasClipDistance() const { return mPODStruct.hasClipDistance; }
    bool hasDiscard() const { return mPODStruct.hasDiscard; }
    bool enablesPerSampleShading() const { return mPODStruct.enablesPerSampleShading; }
    BlendEquationBitSet getAdvancedBlendEquations() const
    {
        return mPODStruct.advancedBlendEquations;
    }
    const std::vector<TransformFeedbackVarying> &getLinkedTransformFeedbackVaryings() const
    {
        return mLinkedTransformFeedbackVaryings;
    }
    GLint getTransformFeedbackBufferMode() const { return mPODStruct.transformFeedbackBufferMode; }
    const sh::WorkGroupSize &getComputeShaderLocalSize() const
    {
        return mPODStruct.computeShaderLocalSize;
    }
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
    const std::vector<BufferVariable> &getBufferVariables() const { return mBufferVariables; }
    const LinkedUniform &getUniformByIndex(GLuint index) const
    {
        ASSERT(index < static_cast<size_t>(mUniforms.size()));
        return mUniforms[index];
    }
    const std::string &getUniformNameByIndex(GLuint index) const
    {
        ASSERT(index < static_cast<size_t>(mUniforms.size()));
        return mUniformNames[index];
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

    GLuint getUniformIndexFromImageIndex(GLuint imageIndex) const
    {
        ASSERT(imageIndex < mPODStruct.imageUniformRange.length());
        return imageIndex + mPODStruct.imageUniformRange.low();
    }

    GLuint getUniformIndexFromSamplerIndex(GLuint samplerIndex) const
    {
        ASSERT(samplerIndex < mPODStruct.samplerUniformRange.length());
        return samplerIndex + mPODStruct.samplerUniformRange.low();
    }

    void saveLinkedStateInfo(const ProgramState &state);
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
        return mPODStruct.linkedShaderVersions[shaderType];
    }

    bool isYUVOutput() const { return mPODStruct.hasYUVOutput; }

    PrimitiveMode getGeometryShaderInputPrimitiveType() const
    {
        return mPODStruct.geometryShaderInputPrimitiveType;
    }

    PrimitiveMode getGeometryShaderOutputPrimitiveType() const
    {
        return mPODStruct.geometryShaderOutputPrimitiveType;
    }

    int getGeometryShaderInvocations() const { return mPODStruct.geometryShaderInvocations; }

    int getGeometryShaderMaxVertices() const { return mPODStruct.geometryShaderMaxVertices; }

    GLenum getTessGenMode() const { return mPODStruct.tessGenMode; }

    int getNumViews() const { return mPODStruct.numViews; }
    bool usesMultiview() const { return mPODStruct.numViews != -1; }

    rx::SpecConstUsageBits getSpecConstUsageBits() const { return mPODStruct.specConstUsageBits; }

    int getDrawIDLocation() const { return mPODStruct.drawIDLocation; }

    int getBaseVertexLocation() const { return mPODStruct.baseVertexLocation; }
    int getBaseInstanceLocation() const { return mPODStruct.baseInstanceLocation; }

    void resetCachedValidateSamplersResult() { mCachedValidateSamplersResult.reset(); }
    bool validateSamplers(const Caps &caps) const
    {
        // Use the cache if:
        // - we aren't using an info log (which gives the full error).
        // - The sample mapping hasn't changed and we've already validated.
        if (mCachedValidateSamplersResult.valid())
        {
            return mCachedValidateSamplersResult.value();
        }

        return validateSamplersImpl(caps);
    }

    ComponentTypeMask getFragmentOutputsTypeMask() const { return mPODStruct.drawBufferTypeMask; }
    DrawBufferMask getActiveOutputVariablesMask() const
    {
        return mPODStruct.activeOutputVariablesMask;
    }
    DrawBufferMask getActiveSecondaryOutputVariablesMask() const
    {
        return mPODStruct.activeSecondaryOutputVariablesMask;
    }

    bool linkUniforms(const Caps &caps,
                      const ShaderMap<std::vector<sh::ShaderVariable>> &shaderUniforms,
                      const ProgramAliasedBindings &uniformLocationBindings,
                      GLuint *combinedImageUniformsCount,
                      std::vector<UnusedUniform> *unusedUniforms);

    void copyInputsFromProgram(const ProgramState &programState);
    void copyShaderBuffersFromProgram(const ProgramState &programState, ShaderType shaderType);
    void clearSamplerBindings();
    void copySamplerBindingsFromProgram(const ProgramState &programState);
    void copyImageBindingsFromProgram(const ProgramState &programState);
    void copyOutputsFromProgram(const ProgramState &programState);
    void copyUniformsFromProgramMap(const ShaderMap<Program *> &programs);

    GLuint getAttributeLocation(const std::string &name) const;

  private:
    friend class Program;
    friend class ProgramPipeline;
    friend class ProgramState;
    friend class ProgramPipelineState;

    void reset();

    void updateActiveImages(const ProgramExecutable &executable);

    // Scans the sampler bindings for type conflicts with sampler 'textureUnitIndex'.
    void setSamplerUniformTextureTypeAndFormat(size_t textureUnitIndex,
                                               const std::vector<SamplerBinding> &samplerBindings,
                                               const std::vector<GLuint> &boundTextureUnits);

    bool linkMergedVaryings(const Caps &caps,
                            const Limitations &limitations,
                            const Version &clientVersion,
                            bool webglCompatibility,
                            const ProgramMergedVaryings &mergedVaryings,
                            const std::vector<std::string> &transformFeedbackVaryingNames,
                            const LinkingVariables &linkingVariables,
                            bool isSeparable,
                            ProgramVaryingPacking *varyingPacking);

    bool linkValidateTransformFeedback(
        const Caps &caps,
        const Version &clientVersion,
        const ProgramMergedVaryings &varyings,
        ShaderType stage,
        const std::vector<std::string> &transformFeedbackVaryingNames);

    void gatherTransformFeedbackVaryings(
        const ProgramMergedVaryings &varyings,
        ShaderType stage,
        const std::vector<std::string> &transformFeedbackVaryingNames);

    void updateTransformFeedbackStrides();

    bool validateSamplersImpl(const Caps &caps) const;

    bool linkValidateOutputVariables(const Caps &caps,
                                     const Version &version,
                                     GLuint combinedImageUniformsCount,
                                     GLuint combinedShaderStorageBlocksCount,
                                     const std::vector<sh::ShaderVariable> &outputVariables,
                                     int fragmentShaderVersion,
                                     const ProgramAliasedBindings &fragmentOutputLocations,
                                     const ProgramAliasedBindings &fragmentOutputIndices);

    bool gatherOutputTypes();

    void linkSamplerAndImageBindings(GLuint *combinedImageUniformsCount);
    bool linkAtomicCounterBuffers(const Caps &caps);

    rx::ProgramExecutableImpl *mImplementation;

    // A reference to the owning object's (Program or ProgramPipeline) info log.  It's kept here for
    // convenience as numerous functions reference it.
    InfoLog *mInfoLog;

    // This struct must only contains basic data types so that entire struct can be memcpy.
    struct PODStruct
    {
        ShaderBitSet linkedShaderStages;
        angle::BitSet<MAX_VERTEX_ATTRIBS> activeAttribLocationsMask;
        unsigned int maxActiveAttribLocation;
        ComponentTypeMask attributesTypeMask;
        // attributesMask is identical to mActiveAttribLocationsMask with built-in attributes
        // removed.
        AttributesMask attributesMask;
        DrawBufferMask activeOutputVariablesMask;
        DrawBufferMask activeSecondaryOutputVariablesMask;
        ComponentTypeMask drawBufferTypeMask;

        RangeUI defaultUniformRange;
        RangeUI samplerUniformRange;
        RangeUI imageUniformRange;
        RangeUI atomicCounterUniformRange;
        RangeUI fragmentInoutRange;

        bool hasClipDistance;
        bool hasDiscard;
        bool hasYUVOutput;
        bool enablesPerSampleShading;
        bool canDrawWith;

        // KHR_blend_equation_advanced supported equation list
        BlendEquationBitSet advancedBlendEquations;

        // GL_EXT_geometry_shader.
        PrimitiveMode geometryShaderInputPrimitiveType;
        PrimitiveMode geometryShaderOutputPrimitiveType;
        int geometryShaderInvocations;
        int geometryShaderMaxVertices;

        // GL_OVR_multiview / GL_OVR_multiview2
        int numViews;

        // GL_ANGLE_multi_draw
        int drawIDLocation;

        // GL_ANGLE_base_vertex_base_instance_shader_builtin
        int baseVertexLocation;
        int baseInstanceLocation;

        // GL_EXT_tessellation_shader
        int tessControlShaderVertices;
        GLenum tessGenMode;
        GLenum tessGenSpacing;
        GLenum tessGenVertexOrder;
        GLenum tessGenPointMode;

        GLenum transformFeedbackBufferMode;

        // For faster iteration on the blocks currently being bound.
        UniformBlockBindingMask activeUniformBlockBindings;

        ShaderMap<int> linkedShaderVersions;

        sh::WorkGroupSize computeShaderLocalSize;

        rx::SpecConstUsageBits specConstUsageBits;
    } mPODStruct;
    static_assert(std::is_trivially_copyable<PODStruct>(), "must be memcpy-able");

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

    // Names and mapped names of output variables that are arrays include [0] in the end, similarly
    // to uniforms.
    std::vector<sh::ShaderVariable> mOutputVariables;
    std::vector<VariableLocation> mOutputLocations;
    // EXT_blend_func_extended secondary outputs (ones with index 1)
    std::vector<VariableLocation> mSecondaryOutputLocations;
    // Vertex attributes, Fragment input varyings, etc.
    std::vector<ProgramInput> mProgramInputs;
    std::vector<TransformFeedbackVarying> mLinkedTransformFeedbackVaryings;
    // The size of the data written to each transform feedback buffer per vertex.
    std::vector<GLsizei> mTransformFeedbackStrides;
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
    std::vector<std::string> mUniformNames;
    // Only used by GL and D3D backend
    std::vector<std::string> mUniformMappedNames;
    std::vector<InterfaceBlock> mUniformBlocks;
    std::vector<VariableLocation> mUniformLocations;

    std::vector<AtomicCounterBuffer> mAtomicCounterBuffers;
    std::vector<InterfaceBlock> mShaderStorageBlocks;
    std::vector<BufferVariable> mBufferVariables;

    // An array of the samplers that are used by the program
    std::vector<SamplerBinding> mSamplerBindings;
    // List of all textures bound to all samplers. Each SamplerBinding will point to a subset in
    // this vector.
    std::vector<GLuint> mSamplerBoundTextureUnits;

    // An array of the images that are used by the program
    std::vector<ImageBinding> mImageBindings;

    ShaderMap<std::vector<sh::ShaderVariable>> mLinkedOutputVaryings;
    ShaderMap<std::vector<sh::ShaderVariable>> mLinkedInputVaryings;
    ShaderMap<std::vector<sh::ShaderVariable>> mLinkedUniforms;
    ShaderMap<std::vector<sh::InterfaceBlock>> mLinkedUniformBlocks;

    // Cache for sampler validation
    mutable Optional<bool> mCachedValidateSamplersResult;
};
}  // namespace gl

#endif  // LIBANGLE_PROGRAMEXECUTABLE_H_
