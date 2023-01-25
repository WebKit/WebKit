//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramExecutable.cpp: Collects the interfaces common to both Programs and
// ProgramPipelines in order to execute/draw with either.

#include "libANGLE/ProgramExecutable.h"

#include "common/string_utils.h"
#include "libANGLE/Context.h"
#include "libANGLE/Program.h"
#include "libANGLE/Shader.h"

namespace gl
{
namespace
{
bool IncludeSameArrayElement(const std::set<std::string> &nameSet, const std::string &name)
{
    std::vector<unsigned int> subscripts;
    std::string baseName = ParseResourceName(name, &subscripts);
    for (const std::string &nameInSet : nameSet)
    {
        std::vector<unsigned int> arrayIndices;
        std::string arrayName = ParseResourceName(nameInSet, &arrayIndices);
        if (baseName == arrayName &&
            (subscripts.empty() || arrayIndices.empty() || subscripts == arrayIndices))
        {
            return true;
        }
    }
    return false;
}

// Find the matching varying or field by name.
const sh::ShaderVariable *FindOutputVaryingOrField(const ProgramMergedVaryings &varyings,
                                                   ShaderType stage,
                                                   const std::string &name)
{
    const sh::ShaderVariable *var = nullptr;
    for (const ProgramVaryingRef &ref : varyings)
    {
        if (ref.frontShaderStage != stage)
        {
            continue;
        }

        const sh::ShaderVariable *varying = ref.get(stage);
        if (varying->name == name)
        {
            var = varying;
            break;
        }
        GLuint fieldIndex = 0;
        var               = varying->findField(name, &fieldIndex);
        if (var != nullptr)
        {
            break;
        }
    }
    return var;
}

bool FindUsedOutputLocation(std::vector<VariableLocation> &outputLocations,
                            unsigned int baseLocation,
                            unsigned int elementCount,
                            const std::vector<VariableLocation> &reservedLocations,
                            unsigned int variableIndex)
{
    if (baseLocation + elementCount > outputLocations.size())
    {
        elementCount = baseLocation < outputLocations.size()
                           ? static_cast<unsigned int>(outputLocations.size() - baseLocation)
                           : 0;
    }
    for (unsigned int elementIndex = 0; elementIndex < elementCount; elementIndex++)
    {
        const unsigned int location = baseLocation + elementIndex;
        if (outputLocations[location].used())
        {
            VariableLocation locationInfo(elementIndex, variableIndex);
            if (std::find(reservedLocations.begin(), reservedLocations.end(), locationInfo) ==
                reservedLocations.end())
            {
                return true;
            }
        }
    }
    return false;
}

void AssignOutputLocations(std::vector<VariableLocation> &outputLocations,
                           unsigned int baseLocation,
                           unsigned int elementCount,
                           const std::vector<VariableLocation> &reservedLocations,
                           unsigned int variableIndex,
                           sh::ShaderVariable &outputVariable)
{
    if (baseLocation + elementCount > outputLocations.size())
    {
        outputLocations.resize(baseLocation + elementCount);
    }
    for (unsigned int elementIndex = 0; elementIndex < elementCount; elementIndex++)
    {
        VariableLocation locationInfo(elementIndex, variableIndex);
        if (std::find(reservedLocations.begin(), reservedLocations.end(), locationInfo) ==
            reservedLocations.end())
        {
            outputVariable.location     = baseLocation;
            const unsigned int location = baseLocation + elementIndex;
            outputLocations[location]   = locationInfo;
        }
    }
}

int GetOutputLocationForLink(const ProgramAliasedBindings &fragmentOutputLocations,
                             const sh::ShaderVariable &outputVariable)
{
    if (outputVariable.location != -1)
    {
        return outputVariable.location;
    }
    int apiLocation = fragmentOutputLocations.getBinding(outputVariable);
    if (apiLocation != -1)
    {
        return apiLocation;
    }
    return -1;
}

bool IsOutputSecondaryForLink(const ProgramAliasedBindings &fragmentOutputIndexes,
                              const sh::ShaderVariable &outputVariable)
{
    if (outputVariable.index != -1)
    {
        ASSERT(outputVariable.index == 0 || outputVariable.index == 1);
        return (outputVariable.index == 1);
    }
    int apiIndex = fragmentOutputIndexes.getBinding(outputVariable);
    if (apiIndex != -1)
    {
        // Index layout qualifier from the shader takes precedence, so the index from the API is
        // checked only if the index was not set in the shader. This is not specified in the EXT
        // spec, but is specified in desktop OpenGL specs.
        return (apiIndex == 1);
    }
    // EXT_blend_func_extended: Outputs get index 0 by default.
    return false;
}

RangeUI AddUniforms(const ShaderMap<Program *> &programs,
                    ShaderBitSet activeShaders,
                    std::vector<LinkedUniform> &outputUniforms,
                    const std::function<RangeUI(const ProgramState &)> &getRange)
{
    unsigned int startRange = static_cast<unsigned int>(outputUniforms.size());
    for (ShaderType shaderType : activeShaders)
    {
        const ProgramState &programState                  = programs[shaderType]->getState();
        const std::vector<LinkedUniform> &programUniforms = programState.getUniforms();
        const RangeUI uniformRange                        = getRange(programState);

        outputUniforms.insert(outputUniforms.end(), programUniforms.begin() + uniformRange.low(),
                              programUniforms.begin() + uniformRange.high());
    }
    return RangeUI(startRange, static_cast<unsigned int>(outputUniforms.size()));
}

template <typename BlockT>
void AppendActiveBlocks(ShaderType shaderType,
                        const std::vector<BlockT> &blocksIn,
                        std::vector<BlockT> &blocksOut)
{
    for (const BlockT &block : blocksIn)
    {
        if (block.isActive(shaderType))
        {
            blocksOut.push_back(block);
        }
    }
}
}  // anonymous namespace

ProgramExecutable::ProgramExecutable()
    : mMaxActiveAttribLocation(0),
      mAttributesTypeMask(0),
      mAttributesMask(0),
      mActiveSamplerRefCounts{},
      mCanDrawWith(false),
      mYUVOutput(false),
      mTransformFeedbackBufferMode(GL_INTERLEAVED_ATTRIBS),
      mDefaultUniformRange(0, 0),
      mSamplerUniformRange(0, 0),
      mImageUniformRange(0, 0),
      mAtomicCounterUniformRange(0, 0),
      mFragmentInoutRange(0, 0),
      mHasClipDistance(false),
      mHasDiscard(false),
      mEnablesPerSampleShading(false),
      // [GL_EXT_geometry_shader] Table 20.22
      mGeometryShaderInputPrimitiveType(PrimitiveMode::Triangles),
      mGeometryShaderOutputPrimitiveType(PrimitiveMode::TriangleStrip),
      mGeometryShaderInvocations(1),
      mGeometryShaderMaxVertices(0),
      mTessControlShaderVertices(0),
      mTessGenMode(GL_NONE),
      mTessGenSpacing(GL_NONE),
      mTessGenVertexOrder(GL_NONE),
      mTessGenPointMode(GL_NONE)
{
    reset(true);
}

ProgramExecutable::ProgramExecutable(const ProgramExecutable &other)
    : mLinkedShaderStages(other.mLinkedShaderStages),
      mActiveAttribLocationsMask(other.mActiveAttribLocationsMask),
      mMaxActiveAttribLocation(other.mMaxActiveAttribLocation),
      mAttributesTypeMask(other.mAttributesTypeMask),
      mAttributesMask(other.mAttributesMask),
      mActiveSamplersMask(other.mActiveSamplersMask),
      mActiveSamplerRefCounts(other.mActiveSamplerRefCounts),
      mActiveSamplerTypes(other.mActiveSamplerTypes),
      mActiveSamplerYUV(other.mActiveSamplerYUV),
      mActiveSamplerFormats(other.mActiveSamplerFormats),
      mActiveSamplerShaderBits(other.mActiveSamplerShaderBits),
      mActiveImagesMask(other.mActiveImagesMask),
      mActiveImageShaderBits(other.mActiveImageShaderBits),
      mCanDrawWith(other.mCanDrawWith),
      mOutputVariables(other.mOutputVariables),
      mOutputLocations(other.mOutputLocations),
      mSecondaryOutputLocations(other.mSecondaryOutputLocations),
      mYUVOutput(other.mYUVOutput),
      mProgramInputs(other.mProgramInputs),
      mLinkedTransformFeedbackVaryings(other.mLinkedTransformFeedbackVaryings),
      mTransformFeedbackStrides(other.mTransformFeedbackStrides),
      mTransformFeedbackBufferMode(other.mTransformFeedbackBufferMode),
      mUniforms(other.mUniforms),
      mDefaultUniformRange(other.mDefaultUniformRange),
      mSamplerUniformRange(other.mSamplerUniformRange),
      mImageUniformRange(other.mImageUniformRange),
      mAtomicCounterUniformRange(other.mAtomicCounterUniformRange),
      mUniformBlocks(other.mUniformBlocks),
      mActiveUniformBlockBindings(other.mActiveUniformBlockBindings),
      mAtomicCounterBuffers(other.mAtomicCounterBuffers),
      mShaderStorageBlocks(other.mShaderStorageBlocks),
      mFragmentInoutRange(other.mFragmentInoutRange),
      mHasClipDistance(other.mHasClipDistance),
      mHasDiscard(other.mHasDiscard),
      mEnablesPerSampleShading(other.mEnablesPerSampleShading),
      mAdvancedBlendEquations(other.mAdvancedBlendEquations)
{
    reset(true);
}

ProgramExecutable::~ProgramExecutable() = default;

void ProgramExecutable::reset(bool clearInfoLog)
{
    if (clearInfoLog)
    {
        resetInfoLog();
    }
    mActiveAttribLocationsMask.reset();
    mAttributesTypeMask.reset();
    mAttributesMask.reset();
    mMaxActiveAttribLocation = 0;

    mActiveSamplersMask.reset();
    mActiveSamplerRefCounts = {};
    mActiveSamplerTypes.fill(TextureType::InvalidEnum);
    mActiveSamplerYUV.reset();
    mActiveSamplerFormats.fill(SamplerFormat::InvalidEnum);

    mActiveImagesMask.reset();

    mProgramInputs.clear();
    mLinkedTransformFeedbackVaryings.clear();
    mTransformFeedbackStrides.clear();
    mUniforms.clear();
    mUniformBlocks.clear();
    mActiveUniformBlockBindings.reset();
    mShaderStorageBlocks.clear();
    mAtomicCounterBuffers.clear();
    mOutputVariables.clear();
    mOutputLocations.clear();
    mActiveOutputVariablesMask.reset();
    mSecondaryOutputLocations.clear();
    mYUVOutput = false;
    mSamplerBindings.clear();
    mImageBindings.clear();

    mDefaultUniformRange       = RangeUI(0, 0);
    mSamplerUniformRange       = RangeUI(0, 0);
    mImageUniformRange         = RangeUI(0, 0);
    mAtomicCounterUniformRange = RangeUI(0, 0);

    mFragmentInoutRange      = RangeUI(0, 0);
    mHasClipDistance         = false;
    mHasDiscard              = false;
    mEnablesPerSampleShading = false;
    mAdvancedBlendEquations.reset();

    mGeometryShaderInputPrimitiveType  = PrimitiveMode::Triangles;
    mGeometryShaderOutputPrimitiveType = PrimitiveMode::TriangleStrip;
    mGeometryShaderInvocations         = 1;
    mGeometryShaderMaxVertices         = 0;

    mTessControlShaderVertices = 0;
    mTessGenMode               = GL_NONE;
    mTessGenSpacing            = GL_NONE;
    mTessGenVertexOrder        = GL_NONE;
    mTessGenPointMode          = GL_NONE;

    mOutputVariableTypes.clear();
    mDrawBufferTypeMask.reset();
}

void ProgramExecutable::load(bool isSeparable, gl::BinaryInputStream *stream)
{
    static_assert(MAX_VERTEX_ATTRIBS * 2 <= sizeof(uint32_t) * 8,
                  "Too many vertex attribs for mask: All bits of mAttributesTypeMask types and "
                  "mask fit into 32 bits each");
    mAttributesTypeMask        = gl::ComponentTypeMask(stream->readInt<uint32_t>());
    mAttributesMask            = gl::AttributesMask(stream->readInt<uint32_t>());
    mActiveAttribLocationsMask = gl::AttributesMask(stream->readInt<uint32_t>());
    mMaxActiveAttribLocation   = stream->readInt<unsigned int>();

    unsigned int fragmentInoutRangeLow  = stream->readInt<uint32_t>();
    unsigned int fragmentInoutRangeHigh = stream->readInt<uint32_t>();
    mFragmentInoutRange                 = RangeUI(fragmentInoutRangeLow, fragmentInoutRangeHigh);

    mHasClipDistance = stream->readBool();

    mHasDiscard              = stream->readBool();
    mEnablesPerSampleShading = stream->readBool();

    static_assert(sizeof(mAdvancedBlendEquations.bits()) == sizeof(uint32_t));
    mAdvancedBlendEquations = BlendEquationBitSet(stream->readInt<uint32_t>());

    mLinkedShaderStages = ShaderBitSet(stream->readInt<uint8_t>());

    mGeometryShaderInputPrimitiveType  = stream->readEnum<PrimitiveMode>();
    mGeometryShaderOutputPrimitiveType = stream->readEnum<PrimitiveMode>();
    mGeometryShaderInvocations         = stream->readInt<int>();
    mGeometryShaderMaxVertices         = stream->readInt<int>();

    mTessControlShaderVertices = stream->readInt<int>();
    mTessGenMode               = stream->readInt<GLenum>();
    mTessGenSpacing            = stream->readInt<GLenum>();
    mTessGenVertexOrder        = stream->readInt<GLenum>();
    mTessGenPointMode          = stream->readInt<GLenum>();

    size_t attribCount = stream->readInt<size_t>();
    ASSERT(getProgramInputs().empty());
    for (size_t attribIndex = 0; attribIndex < attribCount; ++attribIndex)
    {
        sh::ShaderVariable attrib;
        LoadShaderVar(stream, &attrib);
        attrib.location = stream->readInt<int>();
        mProgramInputs.push_back(attrib);
    }

    size_t uniformCount = stream->readInt<size_t>();
    ASSERT(getUniforms().empty());
    for (size_t uniformIndex = 0; uniformIndex < uniformCount; ++uniformIndex)
    {
        LinkedUniform uniform;
        LoadShaderVar(stream, &uniform);

        uniform.bufferIndex = stream->readInt<int>();
        LoadBlockMemberInfo(stream, &uniform.blockInfo);

        stream->readIntVector<unsigned int>(&uniform.outerArraySizes);
        uniform.outerArrayOffset = stream->readInt<unsigned int>();

        uniform.typeInfo = &GetUniformTypeInfo(uniform.type);

        // Active shader info
        for (ShaderType shaderType : gl::AllShaderTypes())
        {
            uniform.setActive(shaderType, stream->readBool());
        }

        mUniforms.push_back(uniform);
    }

    size_t uniformBlockCount = stream->readInt<size_t>();
    ASSERT(getUniformBlocks().empty());
    for (size_t uniformBlockIndex = 0; uniformBlockIndex < uniformBlockCount; ++uniformBlockIndex)
    {
        InterfaceBlock uniformBlock;
        LoadInterfaceBlock(stream, &uniformBlock);
        mUniformBlocks.push_back(uniformBlock);

        mActiveUniformBlockBindings.set(uniformBlockIndex, uniformBlock.binding != 0);
    }

    size_t shaderStorageBlockCount = stream->readInt<size_t>();
    ASSERT(getShaderStorageBlocks().empty());
    for (size_t shaderStorageBlockIndex = 0; shaderStorageBlockIndex < shaderStorageBlockCount;
         ++shaderStorageBlockIndex)
    {
        InterfaceBlock shaderStorageBlock;
        LoadInterfaceBlock(stream, &shaderStorageBlock);
        mShaderStorageBlocks.push_back(shaderStorageBlock);
    }

    size_t atomicCounterBufferCount = stream->readInt<size_t>();
    ASSERT(getAtomicCounterBuffers().empty());
    for (size_t bufferIndex = 0; bufferIndex < atomicCounterBufferCount; ++bufferIndex)
    {
        AtomicCounterBuffer atomicCounterBuffer;
        LoadShaderVariableBuffer(stream, &atomicCounterBuffer);

        mAtomicCounterBuffers.push_back(atomicCounterBuffer);
    }

    size_t transformFeedbackVaryingCount = stream->readInt<size_t>();
    ASSERT(mLinkedTransformFeedbackVaryings.empty());
    for (size_t transformFeedbackVaryingIndex = 0;
         transformFeedbackVaryingIndex < transformFeedbackVaryingCount;
         ++transformFeedbackVaryingIndex)
    {
        sh::ShaderVariable varying;
        stream->readIntVector<unsigned int>(&varying.arraySizes);
        stream->readInt(&varying.type);
        stream->readString(&varying.name);

        GLuint arrayIndex = stream->readInt<GLuint>();

        mLinkedTransformFeedbackVaryings.emplace_back(varying, arrayIndex);
    }

    mTransformFeedbackBufferMode = stream->readInt<GLint>();

    size_t outputCount = stream->readInt<size_t>();
    ASSERT(getOutputVariables().empty());
    for (size_t outputIndex = 0; outputIndex < outputCount; ++outputIndex)
    {
        sh::ShaderVariable output;
        LoadShaderVar(stream, &output);
        output.location = stream->readInt<int>();
        output.index    = stream->readInt<int>();
        mOutputVariables.push_back(output);
    }

    size_t outputVarCount = stream->readInt<size_t>();
    ASSERT(getOutputLocations().empty());
    for (size_t outputIndex = 0; outputIndex < outputVarCount; ++outputIndex)
    {
        VariableLocation locationData;
        stream->readInt(&locationData.arrayIndex);
        stream->readInt(&locationData.index);
        stream->readBool(&locationData.ignored);
        mOutputLocations.push_back(locationData);
    }

    mActiveOutputVariablesMask =
        gl::DrawBufferMask(stream->readInt<gl::DrawBufferMask::value_type>());

    size_t outputTypeCount = stream->readInt<size_t>();
    for (size_t outputIndex = 0; outputIndex < outputTypeCount; ++outputIndex)
    {
        mOutputVariableTypes.push_back(stream->readInt<GLenum>());
    }

    static_assert(IMPLEMENTATION_MAX_DRAW_BUFFERS * 2 <= 8 * sizeof(uint32_t),
                  "All bits of mDrawBufferTypeMask and mActiveOutputVariables types and mask fit "
                  "into 32 bits each");
    mDrawBufferTypeMask = gl::ComponentTypeMask(stream->readInt<uint32_t>());

    stream->readBool(&mYUVOutput);

    size_t secondaryOutputVarCount = stream->readInt<size_t>();
    ASSERT(getSecondaryOutputLocations().empty());
    for (size_t outputIndex = 0; outputIndex < secondaryOutputVarCount; ++outputIndex)
    {
        VariableLocation locationData;
        stream->readInt(&locationData.arrayIndex);
        stream->readInt(&locationData.index);
        stream->readBool(&locationData.ignored);
        mSecondaryOutputLocations.push_back(locationData);
    }

    unsigned int defaultUniformRangeLow  = stream->readInt<unsigned int>();
    unsigned int defaultUniformRangeHigh = stream->readInt<unsigned int>();
    mDefaultUniformRange                 = RangeUI(defaultUniformRangeLow, defaultUniformRangeHigh);

    unsigned int samplerRangeLow  = stream->readInt<unsigned int>();
    unsigned int samplerRangeHigh = stream->readInt<unsigned int>();
    mSamplerUniformRange          = RangeUI(samplerRangeLow, samplerRangeHigh);

    size_t samplerCount = stream->readInt<size_t>();
    for (size_t samplerIndex = 0; samplerIndex < samplerCount; ++samplerIndex)
    {
        TextureType textureType = stream->readEnum<TextureType>();
        GLenum samplerType      = stream->readInt<GLenum>();
        SamplerFormat format    = stream->readEnum<SamplerFormat>();
        size_t bindingCount     = stream->readInt<size_t>();
        mSamplerBindings.emplace_back(textureType, samplerType, format, bindingCount);
    }

    unsigned int imageRangeLow  = stream->readInt<unsigned int>();
    unsigned int imageRangeHigh = stream->readInt<unsigned int>();
    mImageUniformRange          = RangeUI(imageRangeLow, imageRangeHigh);

    size_t imageBindingCount = stream->readInt<size_t>();
    for (size_t imageIndex = 0; imageIndex < imageBindingCount; ++imageIndex)
    {
        size_t elementCount     = stream->readInt<size_t>();
        TextureType textureType = static_cast<TextureType>(stream->readInt<unsigned int>());
        ImageBinding imageBinding(elementCount, textureType);
        for (size_t elementIndex = 0; elementIndex < elementCount; ++elementIndex)
        {
            imageBinding.boundImageUnits[elementIndex] = stream->readInt<unsigned int>();
        }
        mImageBindings.emplace_back(imageBinding);
    }

    unsigned int atomicCounterRangeLow  = stream->readInt<unsigned int>();
    unsigned int atomicCounterRangeHigh = stream->readInt<unsigned int>();
    mAtomicCounterUniformRange          = RangeUI(atomicCounterRangeLow, atomicCounterRangeHigh);

    // These values are currently only used by PPOs, so only load them when the program is marked
    // separable to save memory.
    if (isSeparable)
    {
        for (ShaderType shaderType : mLinkedShaderStages)
        {
            mLinkedOutputVaryings[shaderType].resize(stream->readInt<size_t>());
            for (sh::ShaderVariable &variable : mLinkedOutputVaryings[shaderType])
            {
                LoadShaderVar(stream, &variable);
            }
            mLinkedInputVaryings[shaderType].resize(stream->readInt<size_t>());
            for (sh::ShaderVariable &variable : mLinkedInputVaryings[shaderType])
            {
                LoadShaderVar(stream, &variable);
            }
            mLinkedUniforms[shaderType].resize(stream->readInt<size_t>());
            for (sh::ShaderVariable &variable : mLinkedUniforms[shaderType])
            {
                LoadShaderVar(stream, &variable);
            }
            mLinkedUniformBlocks[shaderType].resize(stream->readInt<size_t>());
            for (sh::InterfaceBlock &shaderStorageBlock : mLinkedUniformBlocks[shaderType])
            {
                LoadShInterfaceBlock(stream, &shaderStorageBlock);
            }
            mLinkedShaderVersions[shaderType] = stream->readInt<int>();
        }
    }
}

void ProgramExecutable::save(bool isSeparable, gl::BinaryOutputStream *stream) const
{
    static_assert(MAX_VERTEX_ATTRIBS * 2 <= sizeof(uint32_t) * 8,
                  "All bits of mAttributesTypeMask types and mask fit into 32 bits each");
    stream->writeInt(static_cast<uint32_t>(mAttributesTypeMask.to_ulong()));
    stream->writeInt(static_cast<uint32_t>(mAttributesMask.to_ulong()));
    stream->writeInt(static_cast<uint32_t>(mActiveAttribLocationsMask.to_ulong()));
    stream->writeInt(mMaxActiveAttribLocation);

    stream->writeInt(mFragmentInoutRange.low());
    stream->writeInt(mFragmentInoutRange.high());

    stream->writeBool(mHasClipDistance);

    stream->writeBool(mHasDiscard);
    stream->writeBool(mEnablesPerSampleShading);
    stream->writeInt(mAdvancedBlendEquations.bits());

    stream->writeInt(mLinkedShaderStages.bits());

    ASSERT(mGeometryShaderInvocations >= 1 && mGeometryShaderMaxVertices >= 0);
    stream->writeEnum(mGeometryShaderInputPrimitiveType);
    stream->writeEnum(mGeometryShaderOutputPrimitiveType);
    stream->writeInt(mGeometryShaderInvocations);
    stream->writeInt(mGeometryShaderMaxVertices);

    stream->writeInt(mTessControlShaderVertices);
    stream->writeInt(mTessGenMode);
    stream->writeInt(mTessGenSpacing);
    stream->writeInt(mTessGenVertexOrder);
    stream->writeInt(mTessGenPointMode);

    stream->writeInt(getProgramInputs().size());
    for (const sh::ShaderVariable &attrib : getProgramInputs())
    {
        WriteShaderVar(stream, attrib);
        stream->writeInt(attrib.location);
    }

    stream->writeInt(getUniforms().size());
    for (const LinkedUniform &uniform : getUniforms())
    {
        WriteShaderVar(stream, uniform);

        stream->writeInt(uniform.bufferIndex);
        WriteBlockMemberInfo(stream, uniform.blockInfo);

        stream->writeIntVector(uniform.outerArraySizes);
        stream->writeInt(uniform.outerArrayOffset);

        // Active shader info
        for (ShaderType shaderType : gl::AllShaderTypes())
        {
            stream->writeBool(uniform.isActive(shaderType));
        }
    }

    stream->writeInt(getUniformBlocks().size());
    for (const InterfaceBlock &uniformBlock : getUniformBlocks())
    {
        WriteInterfaceBlock(stream, uniformBlock);
    }

    stream->writeInt(getShaderStorageBlocks().size());
    for (const InterfaceBlock &shaderStorageBlock : getShaderStorageBlocks())
    {
        WriteInterfaceBlock(stream, shaderStorageBlock);
    }

    stream->writeInt(mAtomicCounterBuffers.size());
    for (const AtomicCounterBuffer &atomicCounterBuffer : getAtomicCounterBuffers())
    {
        WriteShaderVariableBuffer(stream, atomicCounterBuffer);
    }

    stream->writeInt(getLinkedTransformFeedbackVaryings().size());
    for (const auto &var : getLinkedTransformFeedbackVaryings())
    {
        stream->writeIntVector(var.arraySizes);
        stream->writeInt(var.type);
        stream->writeString(var.name);

        stream->writeIntOrNegOne(var.arrayIndex);
    }

    stream->writeInt(getTransformFeedbackBufferMode());

    stream->writeInt(getOutputVariables().size());
    for (const sh::ShaderVariable &output : getOutputVariables())
    {
        WriteShaderVar(stream, output);
        stream->writeInt(output.location);
        stream->writeInt(output.index);
    }

    stream->writeInt(getOutputLocations().size());
    for (const auto &outputVar : getOutputLocations())
    {
        stream->writeInt(outputVar.arrayIndex);
        stream->writeIntOrNegOne(outputVar.index);
        stream->writeBool(outputVar.ignored);
    }

    stream->writeInt(static_cast<int>(mActiveOutputVariablesMask.to_ulong()));

    stream->writeInt(mOutputVariableTypes.size());
    for (const auto &outputVariableType : mOutputVariableTypes)
    {
        stream->writeInt(outputVariableType);
    }

    static_assert(
        IMPLEMENTATION_MAX_DRAW_BUFFERS * 2 <= 8 * sizeof(uint32_t),
        "All bits of mDrawBufferTypeMask and mActiveOutputVariables can be contained in 32 bits");
    stream->writeInt(static_cast<int>(mDrawBufferTypeMask.to_ulong()));

    stream->writeBool(mYUVOutput);

    stream->writeInt(getSecondaryOutputLocations().size());
    for (const auto &outputVar : getSecondaryOutputLocations())
    {
        stream->writeInt(outputVar.arrayIndex);
        stream->writeIntOrNegOne(outputVar.index);
        stream->writeBool(outputVar.ignored);
    }

    stream->writeInt(getDefaultUniformRange().low());
    stream->writeInt(getDefaultUniformRange().high());

    stream->writeInt(getSamplerUniformRange().low());
    stream->writeInt(getSamplerUniformRange().high());

    stream->writeInt(getSamplerBindings().size());
    for (const auto &samplerBinding : getSamplerBindings())
    {
        stream->writeEnum(samplerBinding.textureType);
        stream->writeInt(samplerBinding.samplerType);
        stream->writeEnum(samplerBinding.format);
        stream->writeInt(samplerBinding.boundTextureUnits.size());
    }

    stream->writeInt(getImageUniformRange().low());
    stream->writeInt(getImageUniformRange().high());

    stream->writeInt(getImageBindings().size());
    for (const auto &imageBinding : getImageBindings())
    {
        stream->writeInt(imageBinding.boundImageUnits.size());
        stream->writeInt(static_cast<unsigned int>(imageBinding.textureType));
        for (size_t i = 0; i < imageBinding.boundImageUnits.size(); ++i)
        {
            stream->writeInt(imageBinding.boundImageUnits[i]);
        }
    }

    stream->writeInt(getAtomicCounterUniformRange().low());
    stream->writeInt(getAtomicCounterUniformRange().high());

    // These values are currently only used by PPOs, so only save them when the program is marked
    // separable to save memory.
    if (isSeparable)
    {
        for (ShaderType shaderType : mLinkedShaderStages)
        {
            stream->writeInt(mLinkedOutputVaryings[shaderType].size());
            for (const sh::ShaderVariable &shaderVariable : mLinkedOutputVaryings[shaderType])
            {
                WriteShaderVar(stream, shaderVariable);
            }
            stream->writeInt(mLinkedInputVaryings[shaderType].size());
            for (const sh::ShaderVariable &shaderVariable : mLinkedInputVaryings[shaderType])
            {
                WriteShaderVar(stream, shaderVariable);
            }
            stream->writeInt(mLinkedUniforms[shaderType].size());
            for (const sh::ShaderVariable &shaderVariable : mLinkedUniforms[shaderType])
            {
                WriteShaderVar(stream, shaderVariable);
            }
            stream->writeInt(mLinkedUniformBlocks[shaderType].size());
            for (const sh::InterfaceBlock &shaderStorageBlock : mLinkedUniformBlocks[shaderType])
            {
                WriteShInterfaceBlock(stream, shaderStorageBlock);
            }
            stream->writeInt(mLinkedShaderVersions[shaderType]);
        }
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
    ASSERT(attribLocation < mActiveAttribLocationsMask.size());
    return mActiveAttribLocationsMask[attribLocation];
}

AttributesMask ProgramExecutable::getAttributesMask() const
{
    return mAttributesMask;
}

bool ProgramExecutable::hasDefaultUniforms() const
{
    return !getDefaultUniformRange().empty();
}

bool ProgramExecutable::hasTextures() const
{
    return !getSamplerBindings().empty();
}

bool ProgramExecutable::hasUniformBuffers() const
{
    return !mUniformBlocks.empty();
}

bool ProgramExecutable::hasStorageBuffers() const
{
    return !mShaderStorageBlocks.empty();
}

bool ProgramExecutable::hasAtomicCounterBuffers() const
{
    return !mAtomicCounterBuffers.empty();
}

bool ProgramExecutable::hasImages() const
{
    return !mImageBindings.empty();
}

bool ProgramExecutable::usesFramebufferFetch() const
{
    return (mFragmentInoutRange.length() > 0);
}

GLuint ProgramExecutable::getUniformIndexFromImageIndex(GLuint imageIndex) const
{
    ASSERT(imageIndex < mImageUniformRange.length());
    return imageIndex + mImageUniformRange.low();
}

GLuint ProgramExecutable::getUniformIndexFromSamplerIndex(GLuint samplerIndex) const
{
    ASSERT(samplerIndex < mSamplerUniformRange.length());
    return samplerIndex + mSamplerUniformRange.low();
}

void ProgramExecutable::setActive(size_t textureUnit,
                                  const SamplerBinding &samplerBinding,
                                  const gl::LinkedUniform &samplerUniform)
{
    mActiveSamplersMask.set(textureUnit);
    mActiveSamplerTypes[textureUnit]      = samplerBinding.textureType;
    mActiveSamplerYUV[textureUnit]        = IsSamplerYUVType(samplerBinding.samplerType);
    mActiveSamplerFormats[textureUnit]    = samplerBinding.format;
    mActiveSamplerShaderBits[textureUnit] = samplerUniform.activeShaders();
}

void ProgramExecutable::setInactive(size_t textureUnit)
{
    mActiveSamplersMask.reset(textureUnit);
    mActiveSamplerTypes[textureUnit] = TextureType::InvalidEnum;
    mActiveSamplerYUV.reset(textureUnit);
    mActiveSamplerFormats[textureUnit] = SamplerFormat::InvalidEnum;
    mActiveSamplerShaderBits[textureUnit].reset();
}

void ProgramExecutable::hasSamplerTypeConflict(size_t textureUnit)
{
    // Conflicts are marked with InvalidEnum
    mActiveSamplerYUV.reset(textureUnit);
    mActiveSamplerTypes[textureUnit] = TextureType::InvalidEnum;
}

void ProgramExecutable::hasSamplerFormatConflict(size_t textureUnit)
{
    // Conflicts are marked with InvalidEnum
    mActiveSamplerFormats[textureUnit] = SamplerFormat::InvalidEnum;
}

void ProgramExecutable::updateActiveSamplers(const ProgramState &programState)
{
    const std::vector<SamplerBinding> &samplerBindings = programState.getSamplerBindings();

    for (uint32_t samplerIndex = 0; samplerIndex < samplerBindings.size(); ++samplerIndex)
    {
        const SamplerBinding &samplerBinding = samplerBindings[samplerIndex];

        for (GLint textureUnit : samplerBinding.boundTextureUnits)
        {
            if (++mActiveSamplerRefCounts[textureUnit] == 1)
            {
                uint32_t uniformIndex = programState.getUniformIndexFromSamplerIndex(samplerIndex);
                setActive(textureUnit, samplerBinding, programState.getUniforms()[uniformIndex]);
            }
            else
            {
                if (mActiveSamplerTypes[textureUnit] != samplerBinding.textureType ||
                    mActiveSamplerYUV.test(textureUnit) !=
                        IsSamplerYUVType(samplerBinding.samplerType))
                {
                    hasSamplerTypeConflict(textureUnit);
                }

                if (mActiveSamplerFormats[textureUnit] != samplerBinding.format)
                {
                    hasSamplerFormatConflict(textureUnit);
                }
            }
            mActiveSamplersMask.set(textureUnit);
        }
    }

    // Invalidate the validation cache.
    resetCachedValidateSamplersResult();
}

void ProgramExecutable::updateActiveImages(const ProgramExecutable &executable)
{
    const std::vector<ImageBinding> &imageBindings = executable.getImageBindings();
    for (uint32_t imageIndex = 0; imageIndex < imageBindings.size(); ++imageIndex)
    {
        const gl::ImageBinding &imageBinding = imageBindings.at(imageIndex);

        uint32_t uniformIndex = executable.getUniformIndexFromImageIndex(imageIndex);
        const gl::LinkedUniform &imageUniform = executable.getUniforms()[uniformIndex];
        const ShaderBitSet shaderBits         = imageUniform.activeShaders();
        for (GLint imageUnit : imageBinding.boundImageUnits)
        {
            mActiveImagesMask.set(imageUnit);
            mActiveImageShaderBits[imageUnit] |= shaderBits;
        }
    }
}

void ProgramExecutable::setSamplerUniformTextureTypeAndFormat(
    size_t textureUnitIndex,
    std::vector<SamplerBinding> &samplerBindings)
{
    bool foundBinding         = false;
    TextureType foundType     = TextureType::InvalidEnum;
    bool foundYUV             = false;
    SamplerFormat foundFormat = SamplerFormat::InvalidEnum;

    for (uint32_t samplerIndex = 0; samplerIndex < samplerBindings.size(); ++samplerIndex)
    {
        const SamplerBinding &binding = samplerBindings[samplerIndex];

        // A conflict exists if samplers of different types are sourced by the same texture unit.
        // We need to check all bound textures to detect this error case.
        for (GLuint textureUnit : binding.boundTextureUnits)
        {
            if (textureUnit != textureUnitIndex)
            {
                continue;
            }

            if (!foundBinding)
            {
                foundBinding          = true;
                foundType             = binding.textureType;
                foundYUV              = IsSamplerYUVType(binding.samplerType);
                foundFormat           = binding.format;
                uint32_t uniformIndex = getUniformIndexFromSamplerIndex(samplerIndex);
                setActive(textureUnit, binding, mUniforms[uniformIndex]);
            }
            else
            {
                if (foundType != binding.textureType ||
                    foundYUV != IsSamplerYUVType(binding.samplerType))
                {
                    hasSamplerTypeConflict(textureUnit);
                }

                if (foundFormat != binding.format)
                {
                    hasSamplerFormatConflict(textureUnit);
                }
            }
        }
    }
}

void ProgramExecutable::updateCanDrawWith()
{
    mCanDrawWith = hasLinkedShaderStage(ShaderType::Vertex);
}

void ProgramExecutable::saveLinkedStateInfo(const Context *context, const ProgramState &state)
{
    for (ShaderType shaderType : getLinkedShaderStages())
    {
        Shader *shader = state.getAttachedShader(shaderType);
        ASSERT(shader);
        mLinkedOutputVaryings[shaderType] = shader->getOutputVaryings(context);
        mLinkedInputVaryings[shaderType]  = shader->getInputVaryings(context);
        mLinkedShaderVersions[shaderType] = shader->getShaderVersion(context);
        mLinkedUniforms[shaderType]       = shader->getUniforms(context);
        mLinkedUniformBlocks[shaderType]  = shader->getUniformBlocks(context);
    }
}

bool ProgramExecutable::isYUVOutput() const
{
    return mYUVOutput;
}

ShaderType ProgramExecutable::getLinkedTransformFeedbackStage() const
{
    return GetLastPreFragmentStage(mLinkedShaderStages);
}

bool ProgramExecutable::linkMergedVaryings(
    const Context *context,
    const ProgramMergedVaryings &mergedVaryings,
    const std::vector<std::string> &transformFeedbackVaryingNames,
    const LinkingVariables &linkingVariables,
    bool isSeparable,
    ProgramVaryingPacking *varyingPacking)
{
    ShaderType tfStage = GetLastPreFragmentStage(linkingVariables.isShaderStageUsedBitset);

    if (!linkValidateTransformFeedback(context, mergedVaryings, tfStage,
                                       transformFeedbackVaryingNames))
    {
        return false;
    }

    // Map the varyings to the register file
    // In WebGL, we use a slightly different handling for packing variables.
    gl::PackMode packMode = PackMode::ANGLE_RELAXED;
    if (context->getLimitations().noFlexibleVaryingPacking)
    {
        // D3D9 pack mode is strictly more strict than WebGL, so takes priority.
        packMode = PackMode::ANGLE_NON_CONFORMANT_D3D9;
    }
    else if (context->isWebGL())
    {
        packMode = PackMode::WEBGL_STRICT;
    }

    // Build active shader stage map.
    ShaderBitSet activeShadersMask;
    for (ShaderType shaderType : kAllGraphicsShaderTypes)
    {
        // - Check for attached shaders to handle the case of a Program linking the currently
        // attached shaders.
        // - Check for linked shaders to handle the case of a PPO linking separable programs before
        // drawing.
        if (linkingVariables.isShaderStageUsedBitset[shaderType] ||
            getLinkedShaderStages().test(shaderType))
        {
            activeShadersMask[shaderType] = true;
        }
    }

    if (!varyingPacking->collectAndPackUserVaryings(mInfoLog, context->getCaps(), packMode,
                                                    activeShadersMask, mergedVaryings,
                                                    transformFeedbackVaryingNames, isSeparable))
    {
        return false;
    }

    gatherTransformFeedbackVaryings(mergedVaryings, tfStage, transformFeedbackVaryingNames);
    updateTransformFeedbackStrides();

    return true;
}

bool ProgramExecutable::linkValidateTransformFeedback(
    const Context *context,
    const ProgramMergedVaryings &varyings,
    ShaderType stage,
    const std::vector<std::string> &transformFeedbackVaryingNames)
{
    const Version &version = context->getClientVersion();

    // Validate the tf names regardless of the actual program varyings.
    std::set<std::string> uniqueNames;
    for (const std::string &tfVaryingName : transformFeedbackVaryingNames)
    {
        if (version < Version(3, 1) && tfVaryingName.find('[') != std::string::npos)
        {
            mInfoLog << "Capture of array elements is undefined and not supported.";
            return false;
        }
        if (version >= Version(3, 1))
        {
            if (IncludeSameArrayElement(uniqueNames, tfVaryingName))
            {
                mInfoLog << "Two transform feedback varyings include the same array element ("
                         << tfVaryingName << ").";
                return false;
            }
        }
        else
        {
            if (uniqueNames.count(tfVaryingName) > 0)
            {
                mInfoLog << "Two transform feedback varyings specify the same output variable ("
                         << tfVaryingName << ").";
                return false;
            }
        }
        uniqueNames.insert(tfVaryingName);
    }

    // From OpneGLES spec. 11.1.2.1: A program will fail to link if:
    // the count specified by TransformFeedbackVaryings is non-zero, but the
    // program object has no vertex, tessellation evaluation, or geometry shader
    if (transformFeedbackVaryingNames.size() > 0 &&
        !gl::ShaderTypeSupportsTransformFeedback(getLinkedTransformFeedbackStage()))
    {
        mInfoLog << "Linked transform feedback stage " << getLinkedTransformFeedbackStage()
                 << " does not support transform feedback varying.";
        return false;
    }

    // Validate against program varyings.
    size_t totalComponents = 0;
    for (const std::string &tfVaryingName : transformFeedbackVaryingNames)
    {
        std::vector<unsigned int> subscripts;
        std::string baseName = ParseResourceName(tfVaryingName, &subscripts);

        const sh::ShaderVariable *var = FindOutputVaryingOrField(varyings, stage, baseName);
        if (var == nullptr)
        {
            mInfoLog << "Transform feedback varying " << tfVaryingName
                     << " does not exist in the vertex shader.";
            return false;
        }

        // Validate the matching variable.
        if (var->isStruct())
        {
            mInfoLog << "Struct cannot be captured directly (" << baseName << ").";
            return false;
        }

        size_t elementCount   = 0;
        size_t componentCount = 0;

        if (var->isArray())
        {
            if (version < Version(3, 1))
            {
                mInfoLog << "Capture of arrays is undefined and not supported.";
                return false;
            }

            // GLSL ES 3.10 section 4.3.6: A vertex output can't be an array of arrays.
            ASSERT(!var->isArrayOfArrays());

            if (!subscripts.empty() && subscripts[0] >= var->getOutermostArraySize())
            {
                mInfoLog << "Cannot capture outbound array element '" << tfVaryingName << "'.";
                return false;
            }
            elementCount = (subscripts.empty() ? var->getOutermostArraySize() : 1);
        }
        else
        {
            if (!subscripts.empty())
            {
                mInfoLog << "Varying '" << baseName
                         << "' is not an array to be captured by element.";
                return false;
            }
            elementCount = 1;
        }

        const Caps &caps = context->getCaps();

        // TODO(jmadill): Investigate implementation limits on D3D11
        componentCount = VariableComponentCount(var->type) * elementCount;
        if (mTransformFeedbackBufferMode == GL_SEPARATE_ATTRIBS &&
            componentCount > static_cast<GLuint>(caps.maxTransformFeedbackSeparateComponents))
        {
            mInfoLog << "Transform feedback varying " << tfVaryingName << " components ("
                     << componentCount << ") exceed the maximum separate components ("
                     << caps.maxTransformFeedbackSeparateComponents << ").";
            return false;
        }

        totalComponents += componentCount;
        if (mTransformFeedbackBufferMode == GL_INTERLEAVED_ATTRIBS &&
            totalComponents > static_cast<GLuint>(caps.maxTransformFeedbackInterleavedComponents))
        {
            mInfoLog << "Transform feedback varying total components (" << totalComponents
                     << ") exceed the maximum interleaved components ("
                     << caps.maxTransformFeedbackInterleavedComponents << ").";
            return false;
        }
    }
    return true;
}

void ProgramExecutable::gatherTransformFeedbackVaryings(
    const ProgramMergedVaryings &varyings,
    ShaderType stage,
    const std::vector<std::string> &transformFeedbackVaryingNames)
{
    // Gather the linked varyings that are used for transform feedback, they should all exist.
    mLinkedTransformFeedbackVaryings.clear();
    for (const std::string &tfVaryingName : transformFeedbackVaryingNames)
    {
        std::vector<unsigned int> subscripts;
        std::string baseName = ParseResourceName(tfVaryingName, &subscripts);
        size_t subscript     = GL_INVALID_INDEX;
        if (!subscripts.empty())
        {
            subscript = subscripts.back();
        }
        for (const ProgramVaryingRef &ref : varyings)
        {
            if (ref.frontShaderStage != stage)
            {
                continue;
            }

            const sh::ShaderVariable *varying = ref.get(stage);
            if (baseName == varying->name)
            {
                mLinkedTransformFeedbackVaryings.emplace_back(*varying,
                                                              static_cast<GLuint>(subscript));
                break;
            }
            else if (varying->isStruct())
            {
                GLuint fieldIndex = 0;
                const auto *field = varying->findField(tfVaryingName, &fieldIndex);
                if (field != nullptr)
                {
                    mLinkedTransformFeedbackVaryings.emplace_back(*field, *varying);
                    break;
                }
            }
        }
    }
}

void ProgramExecutable::updateTransformFeedbackStrides()
{
    if (mLinkedTransformFeedbackVaryings.empty())
    {
        return;
    }

    if (mTransformFeedbackBufferMode == GL_INTERLEAVED_ATTRIBS)
    {
        mTransformFeedbackStrides.resize(1);
        size_t totalSize = 0;
        for (const TransformFeedbackVarying &varying : mLinkedTransformFeedbackVaryings)
        {
            totalSize += varying.size() * VariableExternalSize(varying.type);
        }
        mTransformFeedbackStrides[0] = static_cast<GLsizei>(totalSize);
    }
    else
    {
        mTransformFeedbackStrides.resize(mLinkedTransformFeedbackVaryings.size());
        for (size_t i = 0; i < mLinkedTransformFeedbackVaryings.size(); i++)
        {
            TransformFeedbackVarying &varying = mLinkedTransformFeedbackVaryings[i];
            mTransformFeedbackStrides[i] =
                static_cast<GLsizei>(varying.size() * VariableExternalSize(varying.type));
        }
    }
}

bool ProgramExecutable::validateSamplersImpl(InfoLog *infoLog, const Caps &caps) const
{
    // if any two active samplers in a program are of different types, but refer to the same
    // texture image unit, and this is the current program, then ValidateProgram will fail, and
    // DrawArrays and DrawElements will issue the INVALID_OPERATION error.
    for (size_t textureUnit : mActiveSamplersMask)
    {
        if (mActiveSamplerTypes[textureUnit] == TextureType::InvalidEnum)
        {
            if (infoLog)
            {
                (*infoLog) << "Samplers of conflicting types refer to the same texture "
                              "image unit ("
                           << textureUnit << ").";
            }

            mCachedValidateSamplersResult = false;
            return false;
        }

        if (mActiveSamplerFormats[textureUnit] == SamplerFormat::InvalidEnum)
        {
            if (infoLog)
            {
                (*infoLog) << "Samplers of conflicting formats refer to the same texture "
                              "image unit ("
                           << textureUnit << ").";
            }

            mCachedValidateSamplersResult = false;
            return false;
        }
    }

    mCachedValidateSamplersResult = true;
    return true;
}

bool ProgramExecutable::linkValidateOutputVariables(
    const Caps &caps,
    const Extensions &extensions,
    const Version &version,
    GLuint combinedImageUniformsCount,
    GLuint combinedShaderStorageBlocksCount,
    const std::vector<sh::ShaderVariable> &outputVariables,
    int fragmentShaderVersion,
    const ProgramAliasedBindings &fragmentOutputLocations,
    const ProgramAliasedBindings &fragmentOutputIndices)
{
    ASSERT(mOutputVariableTypes.empty());
    ASSERT(mActiveOutputVariablesMask.none());
    ASSERT(mDrawBufferTypeMask.none());
    ASSERT(!mYUVOutput);

    // Gather output variable types
    for (const sh::ShaderVariable &outputVariable : outputVariables)
    {
        if (outputVariable.isBuiltIn() && outputVariable.name != "gl_FragColor" &&
            outputVariable.name != "gl_FragData")
        {
            continue;
        }

        unsigned int baseLocation =
            (outputVariable.location == -1 ? 0u
                                           : static_cast<unsigned int>(outputVariable.location));

        // GLSL ES 3.10 section 4.3.6: Output variables cannot be arrays of arrays or arrays of
        // structures, so we may use getBasicTypeElementCount().
        unsigned int elementCount = outputVariable.getBasicTypeElementCount();
        for (unsigned int elementIndex = 0; elementIndex < elementCount; elementIndex++)
        {
            const unsigned int location = baseLocation + elementIndex;
            if (location >= mOutputVariableTypes.size())
            {
                mOutputVariableTypes.resize(location + 1, GL_NONE);
            }
            ASSERT(location < mActiveOutputVariablesMask.size());
            mActiveOutputVariablesMask.set(location);
            mOutputVariableTypes[location] = VariableComponentType(outputVariable.type);
            ComponentType componentType    = GLenumToComponentType(mOutputVariableTypes[location]);
            SetComponentTypeMask(componentType, location, &mDrawBufferTypeMask);
        }

        if (outputVariable.yuv)
        {
            ASSERT(outputVariables.size() == 1);
            mYUVOutput = true;
        }
    }

    if (version >= ES_3_1)
    {
        // [OpenGL ES 3.1] Chapter 8.22 Page 203:
        // A link error will be generated if the sum of the number of active image uniforms used in
        // all shaders, the number of active shader storage blocks, and the number of active
        // fragment shader outputs exceeds the implementation-dependent value of
        // MAX_COMBINED_SHADER_OUTPUT_RESOURCES.
        if (combinedImageUniformsCount + combinedShaderStorageBlocksCount +
                mActiveOutputVariablesMask.count() >
            static_cast<GLuint>(caps.maxCombinedShaderOutputResources))
        {
            mInfoLog
                << "The sum of the number of active image uniforms, active shader storage blocks "
                   "and active fragment shader outputs exceeds "
                   "MAX_COMBINED_SHADER_OUTPUT_RESOURCES ("
                << caps.maxCombinedShaderOutputResources << ")";
            return false;
        }
    }

    mOutputVariables = outputVariables;

    if (fragmentShaderVersion == 100)
    {
        return true;
    }

    // EXT_blend_func_extended doesn't specify anything related to binding specific elements of an
    // output array in explicit terms.
    //
    // Assuming fragData is an output array, you can defend the position that:
    // P1) you must support binding "fragData" because it's specified
    // P2) you must support querying "fragData[x]" because it's specified
    // P3) you must support binding "fragData[0]" because it's a frequently used pattern
    //
    // Then you can make the leap of faith:
    // P4) you must support binding "fragData[x]" because you support "fragData[0]"
    // P5) you must support binding "fragData[x]" because you support querying "fragData[x]"
    //
    // The spec brings in the "world of arrays" when it mentions binding the arrays and the
    // automatic binding. Thus it must be interpreted that the thing is not undefined, rather you
    // must infer the only possible interpretation (?). Note again: this need of interpretation
    // might be completely off of what GL spec logic is.
    //
    // The other complexity is that unless you implement this feature, it's hard to understand what
    // should happen when the client invokes the feature. You cannot add an additional error as it
    // is not specified. One can ignore it, but obviously it creates the discrepancies...

    std::vector<VariableLocation> reservedLocations;

    // Process any output API bindings for arrays that don't alias to the first element.
    for (const auto &bindingPair : fragmentOutputLocations)
    {
        const std::string &name       = bindingPair.first;
        const ProgramBinding &binding = bindingPair.second;

        size_t nameLengthWithoutArrayIndex;
        unsigned int arrayIndex = ParseArrayIndex(name, &nameLengthWithoutArrayIndex);
        if (arrayIndex == 0 || arrayIndex == GL_INVALID_INDEX)
        {
            continue;
        }
        for (unsigned int outputVariableIndex = 0; outputVariableIndex < mOutputVariables.size();
             outputVariableIndex++)
        {
            const sh::ShaderVariable &outputVariable = mOutputVariables[outputVariableIndex];
            // Check that the binding corresponds to an output array and its array index fits.
            if (outputVariable.isBuiltIn() || !outputVariable.isArray() ||
                !angle::BeginsWith(outputVariable.name, name, nameLengthWithoutArrayIndex) ||
                arrayIndex >= outputVariable.getOutermostArraySize())
            {
                continue;
            }

            // Get the API index that corresponds to this exact binding.
            // This index may differ from the index used for the array's base.
            std::vector<VariableLocation> &outputLocations =
                fragmentOutputIndices.getBindingByName(name) == 1 ? mSecondaryOutputLocations
                                                                  : mOutputLocations;
            unsigned int location = binding.location;
            VariableLocation locationInfo(arrayIndex, outputVariableIndex);
            if (location >= outputLocations.size())
            {
                outputLocations.resize(location + 1);
            }
            if (outputLocations[location].used())
            {
                mInfoLog << "Location of variable " << outputVariable.name
                         << " conflicts with another variable.";
                return false;
            }
            outputLocations[location] = locationInfo;

            // Note the array binding location so that it can be skipped later.
            reservedLocations.push_back(locationInfo);
        }
    }

    // Reserve locations for output variables whose location is fixed in the shader or through the
    // API. Otherwise, the remaining unallocated outputs will be processed later.
    for (unsigned int outputVariableIndex = 0; outputVariableIndex < mOutputVariables.size();
         outputVariableIndex++)
    {
        const sh::ShaderVariable &outputVariable = mOutputVariables[outputVariableIndex];

        // Don't store outputs for gl_FragDepth, gl_FragColor, etc.
        if (outputVariable.isBuiltIn())
            continue;

        int fixedLocation = GetOutputLocationForLink(fragmentOutputLocations, outputVariable);
        if (fixedLocation == -1)
        {
            // Here we're only reserving locations for variables whose location is fixed.
            continue;
        }
        unsigned int baseLocation = static_cast<unsigned int>(fixedLocation);

        std::vector<VariableLocation> &outputLocations =
            IsOutputSecondaryForLink(fragmentOutputIndices, outputVariable)
                ? mSecondaryOutputLocations
                : mOutputLocations;

        // GLSL ES 3.10 section 4.3.6: Output variables cannot be arrays of arrays or arrays of
        // structures, so we may use getBasicTypeElementCount().
        unsigned int elementCount = outputVariable.getBasicTypeElementCount();
        if (FindUsedOutputLocation(outputLocations, baseLocation, elementCount, reservedLocations,
                                   outputVariableIndex))
        {
            mInfoLog << "Location of variable " << outputVariable.name
                     << " conflicts with another variable.";
            return false;
        }
        AssignOutputLocations(outputLocations, baseLocation, elementCount, reservedLocations,
                              outputVariableIndex, mOutputVariables[outputVariableIndex]);
    }

    // Here we assign locations for the output variables that don't yet have them. Note that we're
    // not necessarily able to fit the variables optimally, since then we might have to try
    // different arrangements of output arrays. Now we just assign the locations in the order that
    // we got the output variables. The spec isn't clear on what kind of algorithm is required for
    // finding locations for the output variables, so this should be acceptable at least for now.
    GLuint maxLocation = static_cast<GLuint>(caps.maxDrawBuffers);
    if (!mSecondaryOutputLocations.empty())
    {
        // EXT_blend_func_extended: Program outputs will be validated against
        // MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT if there's even one output with index one.
        maxLocation = caps.maxDualSourceDrawBuffers;
    }

    for (unsigned int outputVariableIndex = 0; outputVariableIndex < mOutputVariables.size();
         outputVariableIndex++)
    {
        const sh::ShaderVariable &outputVariable = mOutputVariables[outputVariableIndex];

        // Don't store outputs for gl_FragDepth, gl_FragColor, etc.
        if (outputVariable.isBuiltIn())
            continue;

        int fixedLocation = GetOutputLocationForLink(fragmentOutputLocations, outputVariable);
        std::vector<VariableLocation> &outputLocations =
            IsOutputSecondaryForLink(fragmentOutputIndices, outputVariable)
                ? mSecondaryOutputLocations
                : mOutputLocations;
        unsigned int baseLocation = 0;
        unsigned int elementCount = outputVariable.getBasicTypeElementCount();
        if (fixedLocation != -1)
        {
            // Secondary inputs might have caused the max location to drop below what has already
            // been explicitly assigned locations. Check for any fixed locations above the max
            // that should cause linking to fail.
            baseLocation = static_cast<unsigned int>(fixedLocation);
        }
        else
        {
            // No fixed location, so try to fit the output in unassigned locations.
            // Try baseLocations starting from 0 one at a time and see if the variable fits.
            while (FindUsedOutputLocation(outputLocations, baseLocation, elementCount,
                                          reservedLocations, outputVariableIndex))
            {
                baseLocation++;
            }
            AssignOutputLocations(outputLocations, baseLocation, elementCount, reservedLocations,
                                  outputVariableIndex, mOutputVariables[outputVariableIndex]);
        }

        // Check for any elements assigned above the max location that are actually used.
        if (baseLocation + elementCount > maxLocation &&
            (baseLocation >= maxLocation ||
             FindUsedOutputLocation(outputLocations, maxLocation,
                                    baseLocation + elementCount - maxLocation, reservedLocations,
                                    outputVariableIndex)))
        {
            // EXT_blend_func_extended: Linking can fail:
            // "if the explicit binding assignments do not leave enough space for the linker to
            // automatically assign a location for a varying out array, which requires multiple
            // contiguous locations."
            mInfoLog << "Could not fit output variable into available locations: "
                     << outputVariable.name;
            return false;
        }
    }

    return true;
}

bool ProgramExecutable::linkUniforms(
    const Context *context,
    const ShaderMap<std::vector<sh::ShaderVariable>> &shaderUniforms,
    InfoLog &infoLog,
    const ProgramAliasedBindings &uniformLocationBindings,
    GLuint *combinedImageUniformsCountOut,
    std::vector<UnusedUniform> *unusedUniformsOutOrNull,
    std::vector<VariableLocation> *uniformLocationsOutOrNull)
{
    UniformLinker linker(mLinkedShaderStages, shaderUniforms);
    if (!linker.link(context->getCaps(), infoLog, uniformLocationBindings))
    {
        return false;
    }

    linker.getResults(&mUniforms, unusedUniformsOutOrNull, uniformLocationsOutOrNull);

    linkSamplerAndImageBindings(combinedImageUniformsCountOut);

    if (!linkAtomicCounterBuffers(context, infoLog))
    {
        return false;
    }

    return true;
}

void ProgramExecutable::linkSamplerAndImageBindings(GLuint *combinedImageUniforms)
{
    ASSERT(combinedImageUniforms);

    // Iterate over mExecutable->mUniforms from the back, and find the range of subpass inputs,
    // atomic counters, images and samplers in that order.
    auto highIter = mUniforms.rbegin();
    auto lowIter  = highIter;

    unsigned int high = static_cast<unsigned int>(mUniforms.size());
    unsigned int low  = high;

    // Note that uniform block uniforms are not yet appended to this list.
    ASSERT(mUniforms.empty() || highIter->isAtomicCounter() || highIter->isImage() ||
           highIter->isSampler() || highIter->isInDefaultBlock() || highIter->isFragmentInOut);

    for (; lowIter != mUniforms.rend() && lowIter->isFragmentInOut; ++lowIter)
    {
        --low;
    }

    mFragmentInoutRange = RangeUI(low, high);

    highIter = lowIter;
    high     = low;

    for (; lowIter != mUniforms.rend() && lowIter->isAtomicCounter(); ++lowIter)
    {
        --low;
    }

    mAtomicCounterUniformRange = RangeUI(low, high);

    highIter = lowIter;
    high     = low;

    for (; lowIter != mUniforms.rend() && lowIter->isImage(); ++lowIter)
    {
        --low;
    }

    mImageUniformRange     = RangeUI(low, high);
    *combinedImageUniforms = 0u;
    // If uniform is a image type, insert it into the mImageBindings array.
    for (unsigned int imageIndex : mImageUniformRange)
    {
        // ES3.1 (section 7.6.1) and GLSL ES3.1 (section 4.4.5), Uniform*i{v} commands
        // cannot load values into a uniform defined as an image. if declare without a
        // binding qualifier, any uniform image variable (include all elements of
        // unbound image array) should be bound to unit zero.
        auto &imageUniform      = mUniforms[imageIndex];
        TextureType textureType = ImageTypeToTextureType(imageUniform.type);
        const GLuint arraySize  = imageUniform.isArray() ? imageUniform.arraySizes[0] : 1u;

        if (imageUniform.binding == -1)
        {
            mImageBindings.emplace_back(
                ImageBinding(imageUniform.getBasicTypeElementCount(), textureType));
        }
        else
        {
            // The arrays of arrays are flattened to arrays, it needs to record the array offset for
            // the correct binding image unit.
            mImageBindings.emplace_back(
                ImageBinding(imageUniform.binding + imageUniform.parentArrayIndex() * arraySize,
                             imageUniform.getBasicTypeElementCount(), textureType));
        }

        *combinedImageUniforms += imageUniform.activeShaderCount() * arraySize;
    }

    highIter = lowIter;
    high     = low;

    for (; lowIter != mUniforms.rend() && lowIter->isSampler(); ++lowIter)
    {
        --low;
    }

    mSamplerUniformRange = RangeUI(low, high);

    // If uniform is a sampler type, insert it into the mSamplerBindings array.
    for (unsigned int samplerIndex : mSamplerUniformRange)
    {
        const auto &samplerUniform = mUniforms[samplerIndex];
        TextureType textureType    = SamplerTypeToTextureType(samplerUniform.type);
        GLenum samplerType         = samplerUniform.typeInfo->type;
        unsigned int elementCount  = samplerUniform.getBasicTypeElementCount();
        SamplerFormat format       = samplerUniform.typeInfo->samplerFormat;
        mSamplerBindings.emplace_back(textureType, samplerType, format, elementCount);
    }

    // Whatever is left constitutes the default uniforms.
    mDefaultUniformRange = RangeUI(0, low);
}

bool ProgramExecutable::linkAtomicCounterBuffers(const Context *context, InfoLog &infoLog)
{
    for (unsigned int index : mAtomicCounterUniformRange)
    {
        auto &uniform                      = mUniforms[index];
        uniform.blockInfo.offset           = uniform.offset;
        uniform.blockInfo.arrayStride      = (uniform.isArray() ? 4 : 0);
        uniform.blockInfo.matrixStride     = 0;
        uniform.blockInfo.isRowMajorMatrix = false;

        bool found = false;
        for (unsigned int bufferIndex = 0; bufferIndex < getActiveAtomicCounterBufferCount();
             ++bufferIndex)
        {
            auto &buffer = mAtomicCounterBuffers[bufferIndex];
            if (buffer.binding == uniform.binding)
            {
                buffer.memberIndexes.push_back(index);
                uniform.bufferIndex = bufferIndex;
                found               = true;
                buffer.unionReferencesWith(uniform);
                break;
            }
        }
        if (!found)
        {
            AtomicCounterBuffer atomicCounterBuffer;
            atomicCounterBuffer.binding = uniform.binding;
            atomicCounterBuffer.memberIndexes.push_back(index);
            atomicCounterBuffer.unionReferencesWith(uniform);
            mAtomicCounterBuffers.push_back(atomicCounterBuffer);
            uniform.bufferIndex = static_cast<int>(getActiveAtomicCounterBufferCount() - 1);
        }
    }

    // Count each atomic counter buffer to validate against
    // per-stage and combined gl_Max*AtomicCounterBuffers.
    GLint combinedShaderACBCount           = 0;
    gl::ShaderMap<GLint> perShaderACBCount = {};
    for (unsigned int bufferIndex = 0; bufferIndex < getActiveAtomicCounterBufferCount();
         ++bufferIndex)
    {
        AtomicCounterBuffer &acb        = mAtomicCounterBuffers[bufferIndex];
        const ShaderBitSet shaderStages = acb.activeShaders();
        for (gl::ShaderType shaderType : shaderStages)
        {
            ++perShaderACBCount[shaderType];
        }
        ++combinedShaderACBCount;
    }
    const Caps &caps = context->getCaps();
    if (combinedShaderACBCount > caps.maxCombinedAtomicCounterBuffers)
    {
        infoLog << " combined AtomicCounterBuffers count exceeds limit";
        return false;
    }
    for (gl::ShaderType stage : gl::AllShaderTypes())
    {
        if (perShaderACBCount[stage] > caps.maxShaderAtomicCounterBuffers[stage])
        {
            infoLog << GetShaderTypeString(stage)
                    << " shader AtomicCounterBuffers count exceeds limit";
            return false;
        }
    }
    return true;
}

void ProgramExecutable::copyInputsFromProgram(const ProgramState &programState)
{
    mProgramInputs = programState.getProgramInputs();
}

void ProgramExecutable::copyShaderBuffersFromProgram(const ProgramState &programState,
                                                     ShaderType shaderType)
{
    AppendActiveBlocks(shaderType, programState.getUniformBlocks(), mUniformBlocks);
    AppendActiveBlocks(shaderType, programState.getShaderStorageBlocks(), mShaderStorageBlocks);
    AppendActiveBlocks(shaderType, programState.getAtomicCounterBuffers(), mAtomicCounterBuffers);
}

void ProgramExecutable::clearSamplerBindings()
{
    mSamplerBindings.clear();
}

void ProgramExecutable::copySamplerBindingsFromProgram(const ProgramState &programState)
{
    const std::vector<SamplerBinding> &bindings = programState.getSamplerBindings();
    mSamplerBindings.insert(mSamplerBindings.end(), bindings.begin(), bindings.end());
}

void ProgramExecutable::copyImageBindingsFromProgram(const ProgramState &programState)
{
    const std::vector<ImageBinding> &bindings = programState.getImageBindings();
    mImageBindings.insert(mImageBindings.end(), bindings.begin(), bindings.end());
}

void ProgramExecutable::copyOutputsFromProgram(const ProgramState &programState)
{
    mOutputVariables          = programState.getOutputVariables();
    mOutputLocations          = programState.getOutputLocations();
    mSecondaryOutputLocations = programState.getSecondaryOutputLocations();
}

void ProgramExecutable::copyUniformsFromProgramMap(const ShaderMap<Program *> &programs)
{
    // Merge default uniforms.
    auto getDefaultRange = [](const ProgramState &state) { return state.getDefaultUniformRange(); };
    mDefaultUniformRange = AddUniforms(programs, mLinkedShaderStages, mUniforms, getDefaultRange);

    // Merge sampler uniforms.
    auto getSamplerRange = [](const ProgramState &state) { return state.getSamplerUniformRange(); };
    mSamplerUniformRange = AddUniforms(programs, mLinkedShaderStages, mUniforms, getSamplerRange);

    // Merge image uniforms.
    auto getImageRange = [](const ProgramState &state) { return state.getImageUniformRange(); };
    mImageUniformRange = AddUniforms(programs, mLinkedShaderStages, mUniforms, getImageRange);

    // Merge atomic counter uniforms.
    auto getAtomicRange = [](const ProgramState &state) {
        return state.getAtomicCounterUniformRange();
    };
    mAtomicCounterUniformRange =
        AddUniforms(programs, mLinkedShaderStages, mUniforms, getAtomicRange);

    // Merge fragment in/out uniforms.
    auto getInoutRange  = [](const ProgramState &state) { return state.getFragmentInoutRange(); };
    mFragmentInoutRange = AddUniforms(programs, mLinkedShaderStages, mUniforms, getInoutRange);
}
}  // namespace gl
