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
#include "libANGLE/renderer/GLImplFactory.h"
#include "libANGLE/renderer/ProgramExecutableImpl.h"

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

RangeUI AddUniforms(const ShaderMap<SharedProgramExecutable> &executables,
                    ShaderBitSet activeShaders,
                    std::vector<LinkedUniform> *outputUniforms,
                    std::vector<std::string> *outputUniformNames,
                    std::vector<std::string> *outputUniformMappedNames,
                    const std::function<RangeUI(const ProgramExecutable &)> &getRange)
{
    unsigned int startRange = static_cast<unsigned int>(outputUniforms->size());
    for (ShaderType shaderType : activeShaders)
    {
        const ProgramExecutable &executable = *executables[shaderType];
        const RangeUI uniformRange          = getRange(executable);

        const std::vector<LinkedUniform> &programUniforms = executable.getUniforms();
        outputUniforms->insert(outputUniforms->end(), programUniforms.begin() + uniformRange.low(),
                               programUniforms.begin() + uniformRange.high());

        const std::vector<std::string> &uniformNames = executable.getUniformNames();
        outputUniformNames->insert(outputUniformNames->end(),
                                   uniformNames.begin() + uniformRange.low(),
                                   uniformNames.begin() + uniformRange.high());

        const std::vector<std::string> &uniformMappedNames = executable.getUniformMappedNames();
        outputUniformMappedNames->insert(outputUniformMappedNames->end(),
                                         uniformMappedNames.begin() + uniformRange.low(),
                                         uniformMappedNames.begin() + uniformRange.high());
    }
    return RangeUI(startRange, static_cast<unsigned int>(outputUniforms->size()));
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

void SaveProgramInputs(BinaryOutputStream *stream, const std::vector<ProgramInput> &programInputs)
{
    stream->writeInt(programInputs.size());
    for (const ProgramInput &attrib : programInputs)
    {
        stream->writeString(attrib.name);
        stream->writeString(attrib.mappedName);
        stream->writeStruct(attrib.basicDataTypeStruct);
    }
}
void LoadProgramInputs(BinaryInputStream *stream, std::vector<ProgramInput> *programInputs)
{
    size_t attribCount = stream->readInt<size_t>();
    ASSERT(programInputs->empty());
    if (attribCount > 0)
    {
        programInputs->resize(attribCount);
        for (size_t attribIndex = 0; attribIndex < attribCount; ++attribIndex)
        {
            ProgramInput &attrib = (*programInputs)[attribIndex];
            stream->readString(&attrib.name);
            stream->readString(&attrib.mappedName);
            stream->readStruct(&attrib.basicDataTypeStruct);
        }
    }
}

void SaveUniforms(BinaryOutputStream *stream,
                  const std::vector<LinkedUniform> &uniforms,
                  const std::vector<std::string> &uniformNames,
                  const std::vector<std::string> &uniformMappedNames,
                  const std::vector<VariableLocation> &uniformLocations)
{
    stream->writeVector(uniforms);
    ASSERT(uniforms.size() == uniformNames.size());
    ASSERT(uniforms.size() == uniformMappedNames.size());
    for (const std::string &name : uniformNames)
    {
        stream->writeString(name);
    }
    for (const std::string &name : uniformMappedNames)
    {
        stream->writeString(name);
    }

    stream->writeInt(uniformLocations.size());
    for (const auto &variable : uniformLocations)
    {
        stream->writeInt(variable.arrayIndex);
        stream->writeIntOrNegOne(variable.index);
        stream->writeBool(variable.ignored);
    }
}
void LoadUniforms(BinaryInputStream *stream,
                  std::vector<LinkedUniform> *uniforms,
                  std::vector<std::string> *uniformNames,
                  std::vector<std::string> *uniformMappedNames,
                  std::vector<VariableLocation> *uniformLocations)
{
    ASSERT(uniforms->empty());
    stream->readVector(uniforms);
    if (!uniforms->empty())
    {
        uniformNames->resize(uniforms->size());
        for (size_t uniformIndex = 0; uniformIndex < uniforms->size(); ++uniformIndex)
        {
            stream->readString(&(*uniformNames)[uniformIndex]);
        }
        uniformMappedNames->resize(uniforms->size());
        for (size_t uniformIndex = 0; uniformIndex < uniforms->size(); ++uniformIndex)
        {
            stream->readString(&(*uniformMappedNames)[uniformIndex]);
        }
    }

    const size_t uniformIndexCount = stream->readInt<size_t>();
    ASSERT(uniformLocations->empty());
    for (size_t uniformIndexIndex = 0; uniformIndexIndex < uniformIndexCount; ++uniformIndexIndex)
    {
        VariableLocation variable;
        stream->readInt(&variable.arrayIndex);
        stream->readInt(&variable.index);
        stream->readBool(&variable.ignored);

        uniformLocations->push_back(variable);
    }
}

void SaveSamplerBindings(BinaryOutputStream *stream,
                         const std::vector<SamplerBinding> &samplerBindings,
                         const std::vector<GLuint> &samplerBoundTextureUnits)
{
    stream->writeVector(samplerBindings);
    stream->writeInt(samplerBoundTextureUnits.size());
}
void LoadSamplerBindings(BinaryInputStream *stream,
                         std::vector<SamplerBinding> *samplerBindings,
                         std::vector<GLuint> *samplerBoundTextureUnits)
{
    ASSERT(samplerBindings->empty());
    stream->readVector(samplerBindings);

    ASSERT(samplerBoundTextureUnits->empty());
    size_t boundTextureUnitsCount = stream->readInt<size_t>();
    samplerBoundTextureUnits->resize(boundTextureUnitsCount, 0);
}

void WriteBufferVariable(BinaryOutputStream *stream, const BufferVariable &var)
{
    WriteShaderVar(stream, var);
    WriteActiveVariable(stream, var.activeVariable);

    stream->writeInt(var.bufferIndex);
    WriteBlockMemberInfo(stream, var.blockInfo);
    stream->writeInt(var.topLevelArraySize);
}

void LoadBufferVariable(BinaryInputStream *stream, BufferVariable *var)
{
    LoadShaderVar(stream, var);
    LoadActiveVariable(stream, &(var->activeVariable));

    var->bufferIndex = stream->readInt<int>();
    LoadBlockMemberInfo(stream, &var->blockInfo);
    var->topLevelArraySize = stream->readInt<int>();
}

void WriteShaderVariableBuffer(BinaryOutputStream *stream, const ShaderVariableBuffer &var)
{
    WriteActiveVariable(stream, var.activeVariable);

    stream->writeInt(var.binding);
    stream->writeInt(var.dataSize);
    stream->writeVector(var.memberIndexes);
}

void LoadShaderVariableBuffer(BinaryInputStream *stream, ShaderVariableBuffer *var)
{
    LoadActiveVariable(stream, &var->activeVariable);

    var->binding  = stream->readInt<int>();
    var->dataSize = stream->readInt<unsigned int>();

    ASSERT(var->memberIndexes.empty());
    stream->readVector(&var->memberIndexes);
}

void WriteInterfaceBlock(BinaryOutputStream *stream, const InterfaceBlock &block)
{
    stream->writeString(block.name);
    stream->writeString(block.mappedName);
    stream->writeBool(block.isArray);
    stream->writeInt(block.arrayElement);

    WriteShaderVariableBuffer(stream, block);
}

void LoadInterfaceBlock(BinaryInputStream *stream, InterfaceBlock *block)
{
    block->name         = stream->readString();
    block->mappedName   = stream->readString();
    block->isArray      = stream->readBool();
    block->arrayElement = stream->readInt<unsigned int>();

    LoadShaderVariableBuffer(stream, block);
}
}  // anonymous namespace

ProgramExecutable::ProgramExecutable(rx::GLImplFactory *factory, InfoLog *infoLog)
    : mImplementation(factory->createProgramExecutable(this)),
      mInfoLog(infoLog),
      mActiveSamplerRefCounts{}
{
    memset(&mPODStruct, 0, sizeof(mPODStruct));
    mPODStruct.geometryShaderInputPrimitiveType  = PrimitiveMode::Triangles;
    mPODStruct.geometryShaderOutputPrimitiveType = PrimitiveMode::TriangleStrip;
    mPODStruct.geometryShaderInvocations         = 1;
    mPODStruct.transformFeedbackBufferMode       = GL_INTERLEAVED_ATTRIBS;
    mPODStruct.computeShaderLocalSize.fill(1);

    reset();
}

ProgramExecutable::~ProgramExecutable()
{
    ASSERT(mImplementation == nullptr);
}

void ProgramExecutable::destroy(const Context *context)
{
    ASSERT(mImplementation != nullptr);

    mImplementation->destroy(context);
    SafeDelete(mImplementation);
}

void ProgramExecutable::reset()
{
    mPODStruct.activeAttribLocationsMask.reset();
    mPODStruct.attributesTypeMask.reset();
    mPODStruct.attributesMask.reset();
    mPODStruct.maxActiveAttribLocation = 0;
    mPODStruct.activeOutputVariablesMask.reset();
    mPODStruct.activeSecondaryOutputVariablesMask.reset();

    mPODStruct.defaultUniformRange       = RangeUI(0, 0);
    mPODStruct.samplerUniformRange       = RangeUI(0, 0);
    mPODStruct.imageUniformRange         = RangeUI(0, 0);
    mPODStruct.atomicCounterUniformRange = RangeUI(0, 0);
    mPODStruct.fragmentInoutRange        = RangeUI(0, 0);

    mPODStruct.hasClipDistance         = false;
    mPODStruct.hasDiscard              = false;
    mPODStruct.enablesPerSampleShading = false;
    mPODStruct.hasYUVOutput            = false;

    mPODStruct.advancedBlendEquations.reset();

    mPODStruct.geometryShaderInputPrimitiveType  = PrimitiveMode::Triangles;
    mPODStruct.geometryShaderOutputPrimitiveType = PrimitiveMode::TriangleStrip;
    mPODStruct.geometryShaderInvocations         = 1;
    mPODStruct.geometryShaderMaxVertices         = 0;

    mPODStruct.numViews = -1;

    mPODStruct.drawIDLocation = -1;

    mPODStruct.baseVertexLocation   = -1;
    mPODStruct.baseInstanceLocation = -1;

    mPODStruct.tessControlShaderVertices = 0;
    mPODStruct.tessGenMode               = GL_NONE;
    mPODStruct.tessGenSpacing            = GL_NONE;
    mPODStruct.tessGenVertexOrder        = GL_NONE;
    mPODStruct.tessGenPointMode          = GL_NONE;
    mPODStruct.drawBufferTypeMask.reset();
    mPODStruct.computeShaderLocalSize.fill(1);

    mPODStruct.specConstUsageBits.reset();

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
    mUniformNames.clear();
    mUniformMappedNames.clear();
    mUniformBlocks.clear();
    mUniformLocations.clear();
    mShaderStorageBlocks.clear();
    mAtomicCounterBuffers.clear();
    mBufferVariables.clear();
    mOutputVariables.clear();
    mOutputLocations.clear();
    mSecondaryOutputLocations.clear();
    mSamplerBindings.clear();
    mSamplerBoundTextureUnits.clear();
    mImageBindings.clear();
}

void ProgramExecutable::load(bool isSeparable, gl::BinaryInputStream *stream)
{
    static_assert(MAX_VERTEX_ATTRIBS * 2 <= sizeof(uint32_t) * 8,
                  "Too many vertex attribs for mask: All bits of mAttributesTypeMask types and "
                  "mask fit into 32 bits each");
    static_assert(IMPLEMENTATION_MAX_DRAW_BUFFERS * 2 <= 8 * sizeof(uint32_t),
                  "All bits of mDrawBufferTypeMask and mActiveOutputVariables types and mask fit "
                  "into 32 bits each");

    stream->readStruct(&mPODStruct);

    LoadProgramInputs(stream, &mProgramInputs);
    LoadUniforms(stream, &mUniforms, &mUniformNames, &mUniformMappedNames, &mUniformLocations);

    size_t uniformBlockCount = stream->readInt<size_t>();
    ASSERT(getUniformBlocks().empty());
    mUniformBlocks.resize(uniformBlockCount);
    for (size_t uniformBlockIndex = 0; uniformBlockIndex < uniformBlockCount; ++uniformBlockIndex)
    {
        InterfaceBlock &uniformBlock = mUniformBlocks[uniformBlockIndex];
        LoadInterfaceBlock(stream, &uniformBlock);
        ASSERT(mPODStruct.activeUniformBlockBindings.test(uniformBlockIndex) ==
               (uniformBlock.binding != 0));
    }

    size_t shaderStorageBlockCount = stream->readInt<size_t>();
    ASSERT(getShaderStorageBlocks().empty());
    mShaderStorageBlocks.resize(shaderStorageBlockCount);
    for (size_t shaderStorageBlockIndex = 0; shaderStorageBlockIndex < shaderStorageBlockCount;
         ++shaderStorageBlockIndex)
    {
        InterfaceBlock &shaderStorageBlock = mShaderStorageBlocks[shaderStorageBlockIndex];
        LoadInterfaceBlock(stream, &shaderStorageBlock);
    }

    size_t atomicCounterBufferCount = stream->readInt<size_t>();
    ASSERT(getAtomicCounterBuffers().empty());
    mAtomicCounterBuffers.resize(atomicCounterBufferCount);
    for (size_t bufferIndex = 0; bufferIndex < atomicCounterBufferCount; ++bufferIndex)
    {
        AtomicCounterBuffer &atomicCounterBuffer = mAtomicCounterBuffers[bufferIndex];
        LoadShaderVariableBuffer(stream, &atomicCounterBuffer);
    }

    size_t bufferVariableCount = stream->readInt<size_t>();
    ASSERT(getBufferVariables().empty());
    mBufferVariables.resize(bufferVariableCount);
    for (size_t bufferVarIndex = 0; bufferVarIndex < bufferVariableCount; ++bufferVarIndex)
    {
        LoadBufferVariable(stream, &mBufferVariables[bufferVarIndex]);
    }

    size_t transformFeedbackVaryingCount = stream->readInt<size_t>();
    ASSERT(mLinkedTransformFeedbackVaryings.empty());
    mLinkedTransformFeedbackVaryings.resize(transformFeedbackVaryingCount);
    for (size_t transformFeedbackVaryingIndex = 0;
         transformFeedbackVaryingIndex < transformFeedbackVaryingCount;
         ++transformFeedbackVaryingIndex)
    {
        TransformFeedbackVarying &varying =
            mLinkedTransformFeedbackVaryings[transformFeedbackVaryingIndex];
        stream->readVector(&varying.arraySizes);
        stream->readInt(&varying.type);
        stream->readString(&varying.name);
        varying.arrayIndex = stream->readInt<GLuint>();
    }

    size_t outputCount = stream->readInt<size_t>();
    ASSERT(getOutputVariables().empty());
    mOutputVariables.resize(outputCount);
    for (size_t outputIndex = 0; outputIndex < outputCount; ++outputIndex)
    {
        sh::ShaderVariable &output = mOutputVariables[outputIndex];
        LoadShaderVar(stream, &output);
        output.location = stream->readInt<int>();
        output.index    = stream->readInt<int>();
    }

    size_t outputVarCount = stream->readInt<size_t>();
    ASSERT(getOutputLocations().empty());
    mOutputLocations.resize(outputVarCount);
    for (size_t outputIndex = 0; outputIndex < outputVarCount; ++outputIndex)
    {
        VariableLocation &locationData = mOutputLocations[outputIndex];
        stream->readInt(&locationData.arrayIndex);
        stream->readInt(&locationData.index);
        stream->readBool(&locationData.ignored);
    }

    size_t secondaryOutputVarCount = stream->readInt<size_t>();
    ASSERT(mSecondaryOutputLocations.empty());
    mSecondaryOutputLocations.resize(secondaryOutputVarCount);
    for (size_t outputIndex = 0; outputIndex < secondaryOutputVarCount; ++outputIndex)
    {
        VariableLocation &locationData = mSecondaryOutputLocations[outputIndex];
        stream->readInt(&locationData.arrayIndex);
        stream->readInt(&locationData.index);
        stream->readBool(&locationData.ignored);
    }

    LoadSamplerBindings(stream, &mSamplerBindings, &mSamplerBoundTextureUnits);

    size_t imageBindingCount = stream->readInt<size_t>();
    ASSERT(mImageBindings.empty());
    mImageBindings.resize(imageBindingCount);
    for (size_t imageIndex = 0; imageIndex < imageBindingCount; ++imageIndex)
    {
        ImageBinding &imageBinding = mImageBindings[imageIndex];
        size_t elementCount        = stream->readInt<size_t>();
        imageBinding.textureType   = static_cast<TextureType>(stream->readInt<unsigned int>());
        imageBinding.boundImageUnits.resize(elementCount);
        for (size_t elementIndex = 0; elementIndex < elementCount; ++elementIndex)
        {
            imageBinding.boundImageUnits[elementIndex] = stream->readInt<unsigned int>();
        }
    }

    // These values are currently only used by PPOs, so only load them when the program is marked
    // separable to save memory.
    if (isSeparable)
    {
        for (ShaderType shaderType : getLinkedShaderStages())
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
        }
    }
}

void ProgramExecutable::save(bool isSeparable, gl::BinaryOutputStream *stream) const
{
    static_assert(MAX_VERTEX_ATTRIBS * 2 <= sizeof(uint32_t) * 8,
                  "All bits of mAttributesTypeMask types and mask fit into 32 bits each");
    static_assert(
        IMPLEMENTATION_MAX_DRAW_BUFFERS * 2 <= 8 * sizeof(uint32_t),
        "All bits of mDrawBufferTypeMask and mActiveOutputVariables can be contained in 32 bits");

    ASSERT(mPODStruct.geometryShaderInvocations >= 1 && mPODStruct.geometryShaderMaxVertices >= 0);
    stream->writeStruct(mPODStruct);

    SaveProgramInputs(stream, mProgramInputs);
    SaveUniforms(stream, mUniforms, mUniformNames, mUniformMappedNames, mUniformLocations);

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

    stream->writeInt(getBufferVariables().size());
    for (const BufferVariable &bufferVariable : getBufferVariables())
    {
        WriteBufferVariable(stream, bufferVariable);
    }

    stream->writeInt(getLinkedTransformFeedbackVaryings().size());
    for (const auto &var : getLinkedTransformFeedbackVaryings())
    {
        stream->writeVector(var.arraySizes);
        stream->writeInt(var.type);
        stream->writeString(var.name);

        stream->writeIntOrNegOne(var.arrayIndex);
    }

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

    stream->writeInt(getSecondaryOutputLocations().size());
    for (const auto &outputVar : getSecondaryOutputLocations())
    {
        stream->writeInt(outputVar.arrayIndex);
        stream->writeIntOrNegOne(outputVar.index);
        stream->writeBool(outputVar.ignored);
    }

    SaveSamplerBindings(stream, mSamplerBindings, mSamplerBoundTextureUnits);

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

    // These values are currently only used by PPOs, so only save them when the program is marked
    // separable to save memory.
    if (isSeparable)
    {
        for (ShaderType shaderType : getLinkedShaderStages())
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
        }
    }
}

std::string ProgramExecutable::getInfoLogString() const
{
    return mInfoLog->str();
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

void ProgramExecutable::updateActiveSamplers(const ProgramExecutable &executable)
{
    const std::vector<SamplerBinding> &samplerBindings = executable.getSamplerBindings();
    const std::vector<GLuint> &boundTextureUnits       = executable.getSamplerBoundTextureUnits();

    for (uint32_t samplerIndex = 0; samplerIndex < samplerBindings.size(); ++samplerIndex)
    {
        const SamplerBinding &samplerBinding = samplerBindings[samplerIndex];

        for (uint16_t index = 0; index < samplerBinding.textureUnitsCount; index++)
        {
            GLint textureUnit = samplerBinding.getTextureUnit(boundTextureUnits, index);
            if (++mActiveSamplerRefCounts[textureUnit] == 1)
            {
                uint32_t uniformIndex = executable.getUniformIndexFromSamplerIndex(samplerIndex);
                setActive(textureUnit, samplerBinding, executable.getUniforms()[uniformIndex]);
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
    const std::vector<SamplerBinding> &samplerBindings,
    const std::vector<GLuint> &boundTextureUnits)
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
        for (uint16_t index = 0; index < binding.textureUnitsCount; index++)
        {
            GLuint textureUnit = binding.getTextureUnit(boundTextureUnits, index);
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

void ProgramExecutable::saveLinkedStateInfo(const ProgramState &state)
{
    for (ShaderType shaderType : getLinkedShaderStages())
    {
        const SharedCompiledShaderState &shader = state.getAttachedShader(shaderType);
        ASSERT(shader);
        mPODStruct.linkedShaderVersions[shaderType] = shader->shaderVersion;
        mLinkedOutputVaryings[shaderType]           = shader->outputVaryings;
        mLinkedInputVaryings[shaderType]            = shader->inputVaryings;
        mLinkedUniforms[shaderType]                 = shader->uniforms;
        mLinkedUniformBlocks[shaderType]            = shader->uniformBlocks;
    }
}

bool ProgramExecutable::linkMergedVaryings(const Caps &caps,
                                           const Limitations &limitations,
                                           const Version &clientVersion,
                                           bool webglCompatibility,
                                           const ProgramMergedVaryings &mergedVaryings,
                                           const LinkingVariables &linkingVariables,
                                           bool isSeparable,
                                           ProgramVaryingPacking *varyingPacking)
{
    ShaderType tfStage = GetLastPreFragmentStage(linkingVariables.isShaderStageUsedBitset);

    if (!linkValidateTransformFeedback(caps, clientVersion, mergedVaryings, tfStage))
    {
        return false;
    }

    // Map the varyings to the register file
    // In WebGL, we use a slightly different handling for packing variables.
    gl::PackMode packMode = PackMode::ANGLE_RELAXED;
    if (limitations.noFlexibleVaryingPacking)
    {
        // D3D9 pack mode is strictly more strict than WebGL, so takes priority.
        packMode = PackMode::ANGLE_NON_CONFORMANT_D3D9;
    }
    else if (webglCompatibility)
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

    if (!varyingPacking->collectAndPackUserVaryings(*mInfoLog, caps, packMode, activeShadersMask,
                                                    mergedVaryings, mTransformFeedbackVaryingNames,
                                                    isSeparable))
    {
        return false;
    }

    gatherTransformFeedbackVaryings(mergedVaryings, tfStage);
    updateTransformFeedbackStrides();

    return true;
}

bool ProgramExecutable::linkValidateTransformFeedback(const Caps &caps,
                                                      const Version &clientVersion,
                                                      const ProgramMergedVaryings &varyings,
                                                      ShaderType stage)
{
    // Validate the tf names regardless of the actual program varyings.
    std::set<std::string> uniqueNames;
    for (const std::string &tfVaryingName : mTransformFeedbackVaryingNames)
    {
        if (clientVersion < Version(3, 1) && tfVaryingName.find('[') != std::string::npos)
        {
            *mInfoLog << "Capture of array elements is undefined and not supported.";
            return false;
        }
        if (clientVersion >= Version(3, 1))
        {
            if (IncludeSameArrayElement(uniqueNames, tfVaryingName))
            {
                *mInfoLog << "Two transform feedback varyings include the same array element ("
                          << tfVaryingName << ").";
                return false;
            }
        }
        else
        {
            if (uniqueNames.count(tfVaryingName) > 0)
            {
                *mInfoLog << "Two transform feedback varyings specify the same output variable ("
                          << tfVaryingName << ").";
                return false;
            }
        }
        uniqueNames.insert(tfVaryingName);
    }

    // From OpneGLES spec. 11.1.2.1: A program will fail to link if:
    // the count specified by TransformFeedbackVaryings is non-zero, but the
    // program object has no vertex, tessellation evaluation, or geometry shader
    if (mTransformFeedbackVaryingNames.size() > 0 &&
        !gl::ShaderTypeSupportsTransformFeedback(getLinkedTransformFeedbackStage()))
    {
        *mInfoLog << "Linked transform feedback stage " << getLinkedTransformFeedbackStage()
                  << " does not support transform feedback varying.";
        return false;
    }

    // Validate against program varyings.
    size_t totalComponents = 0;
    for (const std::string &tfVaryingName : mTransformFeedbackVaryingNames)
    {
        std::vector<unsigned int> subscripts;
        std::string baseName = ParseResourceName(tfVaryingName, &subscripts);

        const sh::ShaderVariable *var = FindOutputVaryingOrField(varyings, stage, baseName);
        if (var == nullptr)
        {
            *mInfoLog << "Transform feedback varying " << tfVaryingName
                      << " does not exist in the vertex shader.";
            return false;
        }

        // Validate the matching variable.
        if (var->isStruct())
        {
            *mInfoLog << "Struct cannot be captured directly (" << baseName << ").";
            return false;
        }

        size_t elementCount   = 0;
        size_t componentCount = 0;

        if (var->isArray())
        {
            if (clientVersion < Version(3, 1))
            {
                *mInfoLog << "Capture of arrays is undefined and not supported.";
                return false;
            }

            // GLSL ES 3.10 section 4.3.6: A vertex output can't be an array of arrays.
            ASSERT(!var->isArrayOfArrays());

            if (!subscripts.empty() && subscripts[0] >= var->getOutermostArraySize())
            {
                *mInfoLog << "Cannot capture outbound array element '" << tfVaryingName << "'.";
                return false;
            }
            elementCount = (subscripts.empty() ? var->getOutermostArraySize() : 1);
        }
        else
        {
            if (!subscripts.empty())
            {
                *mInfoLog << "Varying '" << baseName
                          << "' is not an array to be captured by element.";
                return false;
            }
            elementCount = 1;
        }

        // TODO(jmadill): Investigate implementation limits on D3D11
        componentCount = VariableComponentCount(var->type) * elementCount;
        if (mPODStruct.transformFeedbackBufferMode == GL_SEPARATE_ATTRIBS &&
            componentCount > static_cast<GLuint>(caps.maxTransformFeedbackSeparateComponents))
        {
            *mInfoLog << "Transform feedback varying " << tfVaryingName << " components ("
                      << componentCount << ") exceed the maximum separate components ("
                      << caps.maxTransformFeedbackSeparateComponents << ").";
            return false;
        }

        totalComponents += componentCount;
        if (mPODStruct.transformFeedbackBufferMode == GL_INTERLEAVED_ATTRIBS &&
            totalComponents > static_cast<GLuint>(caps.maxTransformFeedbackInterleavedComponents))
        {
            *mInfoLog << "Transform feedback varying total components (" << totalComponents
                      << ") exceed the maximum interleaved components ("
                      << caps.maxTransformFeedbackInterleavedComponents << ").";
            return false;
        }
    }
    return true;
}

void ProgramExecutable::gatherTransformFeedbackVaryings(const ProgramMergedVaryings &varyings,
                                                        ShaderType stage)
{
    // Gather the linked varyings that are used for transform feedback, they should all exist.
    mLinkedTransformFeedbackVaryings.clear();
    for (const std::string &tfVaryingName : mTransformFeedbackVaryingNames)
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

    if (mPODStruct.transformFeedbackBufferMode == GL_INTERLEAVED_ATTRIBS)
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

bool ProgramExecutable::validateSamplersImpl(const Caps &caps) const
{
    // if any two active samplers in a program are of different types, but refer to the same
    // texture image unit, and this is the current program, then ValidateProgram will fail, and
    // DrawArrays and DrawElements will issue the INVALID_OPERATION error.
    for (size_t textureUnit : mActiveSamplersMask)
    {
        if (mActiveSamplerTypes[textureUnit] == TextureType::InvalidEnum)
        {
            mCachedValidateSamplersResult = false;
            return false;
        }

        if (mActiveSamplerFormats[textureUnit] == SamplerFormat::InvalidEnum)
        {
            mCachedValidateSamplersResult = false;
            return false;
        }
    }

    mCachedValidateSamplersResult = true;
    return true;
}

bool ProgramExecutable::linkValidateOutputVariables(
    const Caps &caps,
    const Version &version,
    GLuint combinedImageUniformsCount,
    GLuint combinedShaderStorageBlocksCount,
    const std::vector<sh::ShaderVariable> &outputVariables,
    int fragmentShaderVersion,
    const ProgramAliasedBindings &fragmentOutputLocations,
    const ProgramAliasedBindings &fragmentOutputIndices)
{
    ASSERT(mPODStruct.activeOutputVariablesMask.none());
    ASSERT(mPODStruct.activeSecondaryOutputVariablesMask.none());
    ASSERT(mPODStruct.drawBufferTypeMask.none());
    ASSERT(!mPODStruct.hasYUVOutput);

    mOutputVariables = outputVariables;

    if (fragmentShaderVersion == 100)
    {
        return gatherOutputTypes();
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
                *mInfoLog << "Location of variable " << outputVariable.name
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
            *mInfoLog << "Location of variable " << outputVariable.name
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
            *mInfoLog << "Could not fit output variable into available locations: "
                      << outputVariable.name;
            return false;
        }
    }

    if (!gatherOutputTypes())
    {
        return false;
    }

    if (version >= ES_3_1)
    {
        // [OpenGL ES 3.1] Chapter 8.22 Page 203:
        // A link error will be generated if the sum of the number of active image uniforms used in
        // all shaders, the number of active shader storage blocks, and the number of active
        // fragment shader outputs exceeds the implementation-dependent value of
        // MAX_COMBINED_SHADER_OUTPUT_RESOURCES.
        if (combinedImageUniformsCount + combinedShaderStorageBlocksCount +
                mPODStruct.activeOutputVariablesMask.count() >
            static_cast<GLuint>(caps.maxCombinedShaderOutputResources))
        {
            *mInfoLog
                << "The sum of the number of active image uniforms, active shader storage blocks "
                   "and active fragment shader outputs exceeds "
                   "MAX_COMBINED_SHADER_OUTPUT_RESOURCES ("
                << caps.maxCombinedShaderOutputResources << ")";
            return false;
        }
    }

    return true;
}

bool ProgramExecutable::gatherOutputTypes()
{
    for (const sh::ShaderVariable &outputVariable : mOutputVariables)
    {
        if (outputVariable.isBuiltIn() && outputVariable.name != "gl_FragColor" &&
            outputVariable.name != "gl_FragData" &&
            outputVariable.name != "gl_SecondaryFragColorEXT" &&
            outputVariable.name != "gl_SecondaryFragDataEXT")
        {
            continue;
        }

        unsigned int baseLocation =
            (outputVariable.location == -1 ? 0u
                                           : static_cast<unsigned int>(outputVariable.location));

        const bool secondary =
            outputVariable.index == 1 || (outputVariable.name == "gl_SecondaryFragColorEXT" ||
                                          outputVariable.name == "gl_SecondaryFragDataEXT");

        const ComponentType componentType =
            GLenumToComponentType(VariableComponentType(outputVariable.type));

        // GLSL ES 3.10 section 4.3.6: Output variables cannot be arrays of arrays or arrays of
        // structures, so we may use getBasicTypeElementCount().
        unsigned int elementCount = outputVariable.getBasicTypeElementCount();
        for (unsigned int elementIndex = 0; elementIndex < elementCount; elementIndex++)
        {
            const unsigned int location = baseLocation + elementIndex;
            ASSERT(location < mPODStruct.activeOutputVariablesMask.size());
            ASSERT(location < mPODStruct.activeSecondaryOutputVariablesMask.size());
            if (secondary)
            {
                mPODStruct.activeSecondaryOutputVariablesMask.set(location);
            }
            else
            {
                mPODStruct.activeOutputVariablesMask.set(location);
            }
            const ComponentType storedComponentType =
                gl::GetComponentTypeMask(mPODStruct.drawBufferTypeMask, location);
            if (storedComponentType == ComponentType::InvalidEnum)
            {
                SetComponentTypeMask(componentType, location, &mPODStruct.drawBufferTypeMask);
            }
            else if (storedComponentType != componentType)
            {
                *mInfoLog << "Inconsistent component types for fragment outputs at location "
                          << location;
                return false;
            }
        }

        if (outputVariable.yuv)
        {
            ASSERT(mOutputVariables.size() == 1);
            mPODStruct.hasYUVOutput = true;
        }
    }

    return true;
}

bool ProgramExecutable::linkUniforms(
    const Caps &caps,
    const ShaderMap<std::vector<sh::ShaderVariable>> &shaderUniforms,
    const ProgramAliasedBindings &uniformLocationBindings,
    GLuint *combinedImageUniformsCountOut,
    std::vector<UnusedUniform> *unusedUniformsOutOrNull)
{
    UniformLinker linker(mPODStruct.linkedShaderStages, shaderUniforms);
    if (!linker.link(caps, *mInfoLog, uniformLocationBindings))
    {
        return false;
    }

    linker.getResults(&mUniforms, &mUniformNames, &mUniformMappedNames, unusedUniformsOutOrNull,
                      &mUniformLocations);

    linkSamplerAndImageBindings(combinedImageUniformsCountOut);

    if (!linkAtomicCounterBuffers(caps))
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
           highIter->isSampler() || highIter->isInDefaultBlock() || highIter->isFragmentInOut());

    for (; lowIter != mUniforms.rend() && lowIter->isFragmentInOut(); ++lowIter)
    {
        --low;
    }

    mPODStruct.fragmentInoutRange = RangeUI(low, high);

    highIter = lowIter;
    high     = low;

    for (; lowIter != mUniforms.rend() && lowIter->isAtomicCounter(); ++lowIter)
    {
        --low;
    }

    mPODStruct.atomicCounterUniformRange = RangeUI(low, high);

    highIter = lowIter;
    high     = low;

    for (; lowIter != mUniforms.rend() && lowIter->isImage(); ++lowIter)
    {
        --low;
    }

    mPODStruct.imageUniformRange = RangeUI(low, high);
    *combinedImageUniforms       = 0u;
    // If uniform is a image type, insert it into the mImageBindings array.
    for (unsigned int imageIndex : mPODStruct.imageUniformRange)
    {
        // ES3.1 (section 7.6.1) and GLSL ES3.1 (section 4.4.5), Uniform*i{v} commands
        // cannot load values into a uniform defined as an image. if declare without a
        // binding qualifier, any uniform image variable (include all elements of
        // unbound image array) should be bound to unit zero.
        auto &imageUniform      = mUniforms[imageIndex];
        TextureType textureType = ImageTypeToTextureType(imageUniform.getType());
        const GLuint arraySize  = imageUniform.getBasicTypeElementCount();

        if (imageUniform.getBinding() == -1)
        {
            mImageBindings.emplace_back(
                ImageBinding(imageUniform.getBasicTypeElementCount(), textureType));
        }
        else
        {
            // The arrays of arrays are flattened to arrays, it needs to record the array offset for
            // the correct binding image unit.
            mImageBindings.emplace_back(
                ImageBinding(imageUniform.getBinding() + imageUniform.parentArrayIndex * arraySize,
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

    mPODStruct.samplerUniformRange = RangeUI(low, high);

    // If uniform is a sampler type, insert it into the mSamplerBindings array.
    uint16_t totalCount = 0;
    for (unsigned int samplerIndex : mPODStruct.samplerUniformRange)
    {
        const auto &samplerUniform = mUniforms[samplerIndex];
        TextureType textureType    = SamplerTypeToTextureType(samplerUniform.getType());
        GLenum samplerType         = samplerUniform.getType();
        uint16_t elementCount      = samplerUniform.getBasicTypeElementCount();
        SamplerFormat format       = GetUniformTypeInfo(samplerType).samplerFormat;
        mSamplerBindings.emplace_back(textureType, samplerType, format, totalCount, elementCount);
        totalCount += elementCount;
    }
    mSamplerBoundTextureUnits.resize(totalCount, 0);

    // Whatever is left constitutes the default uniforms.
    mPODStruct.defaultUniformRange = RangeUI(0, low);
}

bool ProgramExecutable::linkAtomicCounterBuffers(const Caps &caps)
{
    for (unsigned int index : mPODStruct.atomicCounterUniformRange)
    {
        auto &uniform = mUniforms[index];

        uniform.blockOffset                    = uniform.getOffset();
        uniform.blockArrayStride               = uniform.isArray() ? 4 : 0;
        uniform.blockMatrixStride              = 0;
        uniform.flagBits.blockIsRowMajorMatrix = false;
        uniform.flagBits.isBlock               = true;

        bool found = false;
        for (uint16_t bufferIndex = 0; bufferIndex < getActiveAtomicCounterBufferCount();
             ++bufferIndex)
        {
            auto &buffer = mAtomicCounterBuffers[bufferIndex];
            if (buffer.binding == uniform.getBinding())
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
            atomicCounterBuffer.binding = uniform.getBinding();
            atomicCounterBuffer.memberIndexes.push_back(index);
            atomicCounterBuffer.unionReferencesWith(uniform);
            mAtomicCounterBuffers.push_back(atomicCounterBuffer);
            uniform.bufferIndex = static_cast<uint16_t>(getActiveAtomicCounterBufferCount() - 1);
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
    if (combinedShaderACBCount > caps.maxCombinedAtomicCounterBuffers)
    {
        *mInfoLog << " combined AtomicCounterBuffers count exceeds limit";
        return false;
    }
    for (gl::ShaderType stage : gl::AllShaderTypes())
    {
        if (perShaderACBCount[stage] > caps.maxShaderAtomicCounterBuffers[stage])
        {
            *mInfoLog << GetShaderTypeString(stage)
                      << " shader AtomicCounterBuffers count exceeds limit";
            return false;
        }
    }
    return true;
}

void ProgramExecutable::copyInputsFromProgram(const ProgramExecutable &executable)
{
    mProgramInputs = executable.getProgramInputs();
}

void ProgramExecutable::copyShaderBuffersFromProgram(const ProgramExecutable &executable,
                                                     ShaderType shaderType)
{
    AppendActiveBlocks(shaderType, executable.getUniformBlocks(), mUniformBlocks);
    AppendActiveBlocks(shaderType, executable.getShaderStorageBlocks(), mShaderStorageBlocks);
    AppendActiveBlocks(shaderType, executable.getAtomicCounterBuffers(), mAtomicCounterBuffers);

    // Buffer variable info is queried through the program, and program pipelines don't access it.
    ASSERT(mBufferVariables.empty());
}

void ProgramExecutable::clearSamplerBindings()
{
    mSamplerBindings.clear();
    mSamplerBoundTextureUnits.clear();
}

void ProgramExecutable::copySamplerBindingsFromProgram(const ProgramExecutable &executable)
{
    const std::vector<SamplerBinding> &bindings = executable.getSamplerBindings();
    const std::vector<GLuint> &textureUnits     = executable.getSamplerBoundTextureUnits();
    uint16_t adjustedStartIndex                 = mSamplerBoundTextureUnits.size();
    mSamplerBoundTextureUnits.insert(mSamplerBoundTextureUnits.end(), textureUnits.begin(),
                                     textureUnits.end());
    for (const SamplerBinding &binding : bindings)
    {
        mSamplerBindings.push_back(binding);
        mSamplerBindings.back().textureUnitsStartIndex += adjustedStartIndex;
    }
}

void ProgramExecutable::copyImageBindingsFromProgram(const ProgramExecutable &executable)
{
    const std::vector<ImageBinding> &bindings = executable.getImageBindings();
    mImageBindings.insert(mImageBindings.end(), bindings.begin(), bindings.end());
}

void ProgramExecutable::copyOutputsFromProgram(const ProgramExecutable &executable)
{
    mOutputVariables          = executable.getOutputVariables();
    mOutputLocations          = executable.getOutputLocations();
    mSecondaryOutputLocations = executable.getSecondaryOutputLocations();
}

void ProgramExecutable::copyUniformsFromProgramMap(
    const ShaderMap<SharedProgramExecutable> &executables)
{
    // Merge default uniforms.
    auto getDefaultRange = [](const ProgramExecutable &state) {
        return state.getDefaultUniformRange();
    };
    mPODStruct.defaultUniformRange =
        AddUniforms(executables, mPODStruct.linkedShaderStages, &mUniforms, &mUniformNames,
                    &mUniformMappedNames, getDefaultRange);

    // Merge sampler uniforms.
    auto getSamplerRange = [](const ProgramExecutable &state) {
        return state.getSamplerUniformRange();
    };
    mPODStruct.samplerUniformRange =
        AddUniforms(executables, mPODStruct.linkedShaderStages, &mUniforms, &mUniformNames,
                    &mUniformMappedNames, getSamplerRange);

    // Merge image uniforms.
    auto getImageRange = [](const ProgramExecutable &state) {
        return state.getImageUniformRange();
    };
    mPODStruct.imageUniformRange =
        AddUniforms(executables, mPODStruct.linkedShaderStages, &mUniforms, &mUniformNames,
                    &mUniformMappedNames, getImageRange);

    // Merge atomic counter uniforms.
    auto getAtomicRange = [](const ProgramExecutable &state) {
        return state.getAtomicCounterUniformRange();
    };
    mPODStruct.atomicCounterUniformRange =
        AddUniforms(executables, mPODStruct.linkedShaderStages, &mUniforms, &mUniformNames,
                    &mUniformMappedNames, getAtomicRange);

    // Merge fragment in/out uniforms.
    auto getInoutRange = [](const ProgramExecutable &state) {
        return state.getFragmentInoutRange();
    };
    mPODStruct.fragmentInoutRange =
        AddUniforms(executables, mPODStruct.linkedShaderStages, &mUniforms, &mUniformNames,
                    &mUniformMappedNames, getInoutRange);

    // Note: uniforms are set through the program, and the program pipeline never needs it.
    ASSERT(mUniformLocations.empty());
}

GLuint ProgramExecutable::getAttributeLocation(const std::string &name) const
{
    for (const ProgramInput &attribute : mProgramInputs)
    {
        if (attribute.name == name)
        {
            return attribute.getLocation();
        }
    }

    return static_cast<GLuint>(-1);
}

void InstallExecutable(const Context *context,
                       const SharedProgramExecutable &toInstall,
                       SharedProgramExecutable *executable)
{
    // There should never be a need to re-install the same executable.
    ASSERT(toInstall.get() != executable->get());

    // Destroy the old executable before it gets deleted.
    UninstallExecutable(context, executable);

    // Install the new executable.
    *executable = toInstall;
}

void UninstallExecutable(const Context *context, SharedProgramExecutable *executable)
{
    if (executable->use_count() == 1)
    {
        (*executable)->destroy(context);
    }

    executable->reset();
}
}  // namespace gl
