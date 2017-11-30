//
// Copyright (c) 2010-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "libANGLE/Uniform.h"

#include "common/utilities.h"

#include <cstring>

namespace gl
{

StaticallyUsed::StaticallyUsed()
    : vertexStaticUse(false), fragmentStaticUse(false), computeStaticUse(false)
{
}

StaticallyUsed::~StaticallyUsed()
{
}

StaticallyUsed::StaticallyUsed(const StaticallyUsed &rhs) = default;
StaticallyUsed &StaticallyUsed::operator=(const StaticallyUsed &rhs) = default;

void StaticallyUsed::setStaticUse(GLenum shaderType, bool used)
{
    switch (shaderType)
    {
        case GL_VERTEX_SHADER:
            vertexStaticUse = used;
            break;

        case GL_FRAGMENT_SHADER:
            fragmentStaticUse = used;
            break;

        case GL_COMPUTE_SHADER:
            computeStaticUse = used;
            break;

        default:
            UNREACHABLE();
    }
}

void StaticallyUsed::unionReferencesWith(const StaticallyUsed &other)
{
    vertexStaticUse |= other.vertexStaticUse;
    fragmentStaticUse |= other.fragmentStaticUse;
    computeStaticUse |= other.computeStaticUse;
}

LinkedUniform::LinkedUniform()
    : typeInfo(nullptr), bufferIndex(-1), blockInfo(sh::BlockMemberInfo::getDefaultBlockInfo())
{
}

LinkedUniform::LinkedUniform(GLenum typeIn,
                             GLenum precisionIn,
                             const std::string &nameIn,
                             const std::vector<unsigned int> &arraySizesIn,
                             const int bindingIn,
                             const int offsetIn,
                             const int locationIn,
                             const int bufferIndexIn,
                             const sh::BlockMemberInfo &blockInfoIn)
    : typeInfo(&GetUniformTypeInfo(typeIn)), bufferIndex(bufferIndexIn), blockInfo(blockInfoIn)
{
    type      = typeIn;
    precision = precisionIn;
    name      = nameIn;
    arraySizes = arraySizesIn;
    binding   = bindingIn;
    offset    = offsetIn;
    location  = locationIn;
    ASSERT(!isArrayOfArrays());
    ASSERT(!isArray() || !isStruct());
}

LinkedUniform::LinkedUniform(const sh::Uniform &uniform)
    : sh::Uniform(uniform),
      typeInfo(&GetUniformTypeInfo(type)),
      bufferIndex(-1),
      blockInfo(sh::BlockMemberInfo::getDefaultBlockInfo())
{
    ASSERT(!isArrayOfArrays());
    ASSERT(!isArray() || !isStruct());
}

LinkedUniform::LinkedUniform(const LinkedUniform &uniform)
    : sh::Uniform(uniform),
      StaticallyUsed(uniform),
      typeInfo(uniform.typeInfo),
      bufferIndex(uniform.bufferIndex),
      blockInfo(uniform.blockInfo)
{
}

LinkedUniform &LinkedUniform::operator=(const LinkedUniform &uniform)
{
    sh::Uniform::operator=(uniform);
    StaticallyUsed::operator=(uniform);
    typeInfo             = uniform.typeInfo;
    bufferIndex          = uniform.bufferIndex;
    blockInfo            = uniform.blockInfo;
    return *this;
}

LinkedUniform::~LinkedUniform()
{
}

bool LinkedUniform::isInDefaultBlock() const
{
    return bufferIndex == -1;
}

bool LinkedUniform::isSampler() const
{
    return typeInfo->isSampler;
}

bool LinkedUniform::isImage() const
{
    return typeInfo->isImageType;
}

bool LinkedUniform::isAtomicCounter() const
{
    return IsAtomicCounterType(type);
}

bool LinkedUniform::isField() const
{
    return name.find('.') != std::string::npos;
}

size_t LinkedUniform::getElementSize() const
{
    return typeInfo->externalSize;
}

size_t LinkedUniform::getElementComponents() const
{
    return typeInfo->componentCount;
}

BufferVariable::BufferVariable()
    : bufferIndex(-1), blockInfo(sh::BlockMemberInfo::getDefaultBlockInfo()), topLevelArraySize(-1)
{
}

BufferVariable::BufferVariable(GLenum typeIn,
                               GLenum precisionIn,
                               const std::string &nameIn,
                               const std::vector<unsigned int> &arraySizesIn,
                               const int bufferIndexIn,
                               const sh::BlockMemberInfo &blockInfoIn)
    : bufferIndex(bufferIndexIn), blockInfo(blockInfoIn), topLevelArraySize(-1)
{
    type      = typeIn;
    precision = precisionIn;
    name      = nameIn;
    arraySizes = arraySizesIn;
}

BufferVariable::~BufferVariable()
{
}

ShaderVariableBuffer::ShaderVariableBuffer() : binding(0), dataSize(0)
{
}

ShaderVariableBuffer::ShaderVariableBuffer(const ShaderVariableBuffer &other) = default;

ShaderVariableBuffer::~ShaderVariableBuffer()
{
}

int ShaderVariableBuffer::numActiveVariables() const
{
    return static_cast<int>(memberIndexes.size());
}

InterfaceBlock::InterfaceBlock() : isArray(false), arrayElement(0)
{
}

InterfaceBlock::InterfaceBlock(const std::string &nameIn,
                               const std::string &mappedNameIn,
                               bool isArrayIn,
                               unsigned int arrayElementIn,
                               int bindingIn)
    : name(nameIn), mappedName(mappedNameIn), isArray(isArrayIn), arrayElement(arrayElementIn)
{
    binding = bindingIn;
}

std::string InterfaceBlock::nameWithArrayIndex() const
{
    std::stringstream fullNameStr;
    fullNameStr << name;
    if (isArray)
    {
        fullNameStr << "[" << arrayElement << "]";
    }

    return fullNameStr.str();
}

std::string InterfaceBlock::mappedNameWithArrayIndex() const
{
    std::stringstream fullNameStr;
    fullNameStr << mappedName;
    if (isArray)
    {
        fullNameStr << "[" << arrayElement << "]";
    }

    return fullNameStr.str();
}
}
