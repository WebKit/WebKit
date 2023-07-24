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

void ShaderInterfaceVariableInfoMap::load(
    VariableInfoArray &&data,
    gl::ShaderMap<IdToIndexMap> &&idToIndexMap,
    gl::ShaderMap<gl::PerVertexMemberBitSet> &&inputPerVertexActiveMembers,
    gl::ShaderMap<gl::PerVertexMemberBitSet> &&outputPerVertexActiveMembers)
{
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
