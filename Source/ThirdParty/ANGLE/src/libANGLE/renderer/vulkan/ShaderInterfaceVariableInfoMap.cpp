//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ShaderInterfaceVariableInfoMap: Maps shader interface variable SPIR-V ids to their Vulkan
// mapping.
//

#include "libANGLE/renderer/vulkan/ShaderInterfaceVariableInfoMap.h"

namespace rx
{
namespace
{
uint32_t HashSPIRVId(uint32_t id)
{
    ASSERT(id >= sh::vk::spirv::kIdShaderVariablesBegin);
    return id - sh::vk::spirv::kIdShaderVariablesBegin;
}

void LoadShaderInterfaceVariableXfbInfo(gl::BinaryInputStream *stream,
                                        ShaderInterfaceVariableXfbInfo *xfb)
{
    xfb->buffer        = stream->readInt<uint32_t>();
    xfb->offset        = stream->readInt<uint32_t>();
    xfb->stride        = stream->readInt<uint32_t>();
    xfb->arraySize     = stream->readInt<uint32_t>();
    xfb->columnCount   = stream->readInt<uint32_t>();
    xfb->rowCount      = stream->readInt<uint32_t>();
    xfb->arrayIndex    = stream->readInt<uint32_t>();
    xfb->componentType = stream->readInt<uint32_t>();
    xfb->arrayElements.resize(stream->readInt<size_t>());
    for (ShaderInterfaceVariableXfbInfo &arrayElement : xfb->arrayElements)
    {
        LoadShaderInterfaceVariableXfbInfo(stream, &arrayElement);
    }
}

void SaveShaderInterfaceVariableXfbInfo(const ShaderInterfaceVariableXfbInfo &xfb,
                                        gl::BinaryOutputStream *stream)
{
    stream->writeInt(xfb.buffer);
    stream->writeInt(xfb.offset);
    stream->writeInt(xfb.stride);
    stream->writeInt(xfb.arraySize);
    stream->writeInt(xfb.columnCount);
    stream->writeInt(xfb.rowCount);
    stream->writeInt(xfb.arrayIndex);
    stream->writeInt(xfb.componentType);
    stream->writeInt(xfb.arrayElements.size());
    for (const ShaderInterfaceVariableXfbInfo &arrayElement : xfb.arrayElements)
    {
        SaveShaderInterfaceVariableXfbInfo(arrayElement, stream);
    }
}
}  // anonymous namespace

ShaderInterfaceVariableInfo::ShaderInterfaceVariableInfo() {}

// ShaderInterfaceVariableInfoMap implementation.
ShaderInterfaceVariableInfoMap::ShaderInterfaceVariableInfoMap() = default;

ShaderInterfaceVariableInfoMap::~ShaderInterfaceVariableInfoMap() = default;

void ShaderInterfaceVariableInfoMap::clear()
{
    mData.clear();
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mIdToIndexMap[shaderType].clear();
    }
    std::fill(mInputPerVertexActiveMembers.begin(), mInputPerVertexActiveMembers.end(),
              gl::PerVertexMemberBitSet{});
    std::fill(mOutputPerVertexActiveMembers.begin(), mOutputPerVertexActiveMembers.end(),
              gl::PerVertexMemberBitSet{});
}

void ShaderInterfaceVariableInfoMap::save(gl::BinaryOutputStream *stream)
{
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        stream->writeInt(mIdToIndexMap[shaderType].size());
        for (const VariableIndex &variableIndex : mIdToIndexMap[shaderType])
        {
            stream->writeInt(variableIndex.index);
        }
    }

    stream->writeInt(mData.size());
    for (const ShaderInterfaceVariableInfo &info : mData)
    {
        stream->writeInt(info.descriptorSet);
        stream->writeInt(info.binding);
        stream->writeInt(info.location);
        stream->writeInt(info.component);
        stream->writeInt(info.index);
        // PackedEnumBitSet uses uint8_t
        stream->writeInt(info.activeStages.bits());
        SaveShaderInterfaceVariableXfbInfo(info.xfb, stream);
        stream->writeInt(info.fieldXfb.size());
        for (const ShaderInterfaceVariableXfbInfo &xfb : info.fieldXfb)
        {
            SaveShaderInterfaceVariableXfbInfo(xfb, stream);
        }
        stream->writeBool(info.useRelaxedPrecision);
        stream->writeBool(info.varyingIsInput);
        stream->writeBool(info.varyingIsOutput);
        stream->writeInt(info.attributeComponentCount);
        stream->writeInt(info.attributeLocationCount);
    }

    // Store gl_PerVertex members only for stages that have it.
    stream->writeInt(mOutputPerVertexActiveMembers[gl::ShaderType::Vertex].bits());
    stream->writeInt(mInputPerVertexActiveMembers[gl::ShaderType::TessControl].bits());
    stream->writeInt(mOutputPerVertexActiveMembers[gl::ShaderType::TessControl].bits());
    stream->writeInt(mInputPerVertexActiveMembers[gl::ShaderType::TessEvaluation].bits());
    stream->writeInt(mOutputPerVertexActiveMembers[gl::ShaderType::TessEvaluation].bits());
    stream->writeInt(mInputPerVertexActiveMembers[gl::ShaderType::Geometry].bits());
    stream->writeInt(mOutputPerVertexActiveMembers[gl::ShaderType::Geometry].bits());
}
void ShaderInterfaceVariableInfoMap::load(gl::BinaryInputStream *stream)
{
    ShaderInterfaceVariableInfoMap::VariableInfoArray data;
    gl::ShaderMap<ShaderInterfaceVariableInfoMap::IdToIndexMap> idToIndexMap;
    gl::ShaderMap<gl::PerVertexMemberBitSet> inputPerVertexActiveMembers;
    gl::ShaderMap<gl::PerVertexMemberBitSet> outputPerVertexActiveMembers;

    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        size_t idCount = stream->readInt<size_t>();
        for (uint32_t id = 0; id < idCount; ++id)
        {
            uint32_t index               = stream->readInt<uint32_t>();
            idToIndexMap[shaderType][id] = {index};
        }
    }

    size_t dataSize = stream->readInt<size_t>();
    for (size_t infoIndex = 0; infoIndex < dataSize; ++infoIndex)
    {
        ShaderInterfaceVariableInfo info;

        info.descriptorSet = stream->readInt<uint32_t>();
        info.binding       = stream->readInt<uint32_t>();
        info.location      = stream->readInt<uint32_t>();
        info.component     = stream->readInt<uint32_t>();
        info.index         = stream->readInt<uint32_t>();
        // PackedEnumBitSet uses uint8_t
        info.activeStages = gl::ShaderBitSet(stream->readInt<uint8_t>());
        LoadShaderInterfaceVariableXfbInfo(stream, &info.xfb);
        info.fieldXfb.resize(stream->readInt<size_t>());
        for (ShaderInterfaceVariableXfbInfo &xfb : info.fieldXfb)
        {
            LoadShaderInterfaceVariableXfbInfo(stream, &xfb);
        }
        info.useRelaxedPrecision     = stream->readBool();
        info.varyingIsInput          = stream->readBool();
        info.varyingIsOutput         = stream->readBool();
        info.attributeComponentCount = stream->readInt<uint8_t>();
        info.attributeLocationCount  = stream->readInt<uint8_t>();

        data.push_back(info);
    }

    outputPerVertexActiveMembers[gl::ShaderType::Vertex] =
        gl::PerVertexMemberBitSet(stream->readInt<uint8_t>());
    inputPerVertexActiveMembers[gl::ShaderType::TessControl] =
        gl::PerVertexMemberBitSet(stream->readInt<uint8_t>());
    outputPerVertexActiveMembers[gl::ShaderType::TessControl] =
        gl::PerVertexMemberBitSet(stream->readInt<uint8_t>());
    inputPerVertexActiveMembers[gl::ShaderType::TessEvaluation] =
        gl::PerVertexMemberBitSet(stream->readInt<uint8_t>());
    outputPerVertexActiveMembers[gl::ShaderType::TessEvaluation] =
        gl::PerVertexMemberBitSet(stream->readInt<uint8_t>());
    inputPerVertexActiveMembers[gl::ShaderType::Geometry] =
        gl::PerVertexMemberBitSet(stream->readInt<uint8_t>());
    outputPerVertexActiveMembers[gl::ShaderType::Geometry] =
        gl::PerVertexMemberBitSet(stream->readInt<uint8_t>());

    mData.swap(data);
    mIdToIndexMap.swap(idToIndexMap);
    mInputPerVertexActiveMembers.swap(inputPerVertexActiveMembers);
    mOutputPerVertexActiveMembers.swap(outputPerVertexActiveMembers);
}

void ShaderInterfaceVariableInfoMap::setInputPerVertexActiveMembers(
    gl::ShaderType shaderType,
    gl::PerVertexMemberBitSet activeMembers)
{
    // Input gl_PerVertex is only meaningful for tessellation and geometry stages
    ASSERT(shaderType == gl::ShaderType::TessControl ||
           shaderType == gl::ShaderType::TessEvaluation || shaderType == gl::ShaderType::Geometry ||
           activeMembers.none());
    mInputPerVertexActiveMembers[shaderType] = activeMembers;
}

void ShaderInterfaceVariableInfoMap::setOutputPerVertexActiveMembers(
    gl::ShaderType shaderType,
    gl::PerVertexMemberBitSet activeMembers)
{
    // Output gl_PerVertex is only meaningful for vertex, tessellation and geometry stages
    ASSERT(shaderType == gl::ShaderType::Vertex || shaderType == gl::ShaderType::TessControl ||
           shaderType == gl::ShaderType::TessEvaluation || shaderType == gl::ShaderType::Geometry ||
           activeMembers.none());
    mOutputPerVertexActiveMembers[shaderType] = activeMembers;
}

void ShaderInterfaceVariableInfoMap::setVariableIndex(gl::ShaderType shaderType,
                                                      uint32_t id,
                                                      VariableIndex index)
{
    mIdToIndexMap[shaderType][HashSPIRVId(id)] = index;
}

const VariableIndex &ShaderInterfaceVariableInfoMap::getVariableIndex(gl::ShaderType shaderType,
                                                                      uint32_t id) const
{
    return mIdToIndexMap[shaderType].at(HashSPIRVId(id));
}

ShaderInterfaceVariableInfo &ShaderInterfaceVariableInfoMap::getMutable(gl::ShaderType shaderType,
                                                                        uint32_t id)
{
    ASSERT(hasVariable(shaderType, id));
    uint32_t index = getVariableIndex(shaderType, id).index;
    return mData[index];
}

ShaderInterfaceVariableInfo &ShaderInterfaceVariableInfoMap::add(gl::ShaderType shaderType,
                                                                 uint32_t id)
{
    ASSERT(!hasVariable(shaderType, id));
    uint32_t index = static_cast<uint32_t>(mData.size());
    setVariableIndex(shaderType, id, {index});
    mData.resize(index + 1);
    return mData[index];
}

void ShaderInterfaceVariableInfoMap::addResource(gl::ShaderBitSet shaderTypes,
                                                 const gl::ShaderMap<uint32_t> &idInShaderTypes,
                                                 uint32_t descriptorSet,
                                                 uint32_t binding)
{
    uint32_t index = static_cast<uint32_t>(mData.size());
    mData.resize(index + 1);
    ShaderInterfaceVariableInfo *info = &mData[index];

    info->descriptorSet = descriptorSet;
    info->binding       = binding;
    info->activeStages  = shaderTypes;

    for (const gl::ShaderType shaderType : shaderTypes)
    {
        const uint32_t id = idInShaderTypes[shaderType];
        ASSERT(!hasVariable(shaderType, id));
        setVariableIndex(shaderType, id, {index});
    }
}

ShaderInterfaceVariableInfo &ShaderInterfaceVariableInfoMap::addOrGet(gl::ShaderType shaderType,
                                                                      uint32_t id)
{
    if (!hasVariable(shaderType, id))
    {
        return add(shaderType, id);
    }
    else
    {
        uint32_t index = getVariableIndex(shaderType, id).index;
        return mData[index];
    }
}

bool ShaderInterfaceVariableInfoMap::hasVariable(gl::ShaderType shaderType, uint32_t id) const
{
    const uint32_t hashedId = HashSPIRVId(id);
    return hashedId < mIdToIndexMap[shaderType].size() &&
           mIdToIndexMap[shaderType].at(hashedId).index != VariableIndex::kInvalid;
}

bool ShaderInterfaceVariableInfoMap::hasTransformFeedbackInfo(gl::ShaderType shaderType,
                                                              uint32_t bufferIndex) const
{
    return hasVariable(shaderType, SpvGetXfbBufferBlockId(bufferIndex));
}
}  // namespace rx
