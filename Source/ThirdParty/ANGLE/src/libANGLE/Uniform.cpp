//
// Copyright 2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "libANGLE/Uniform.h"
#include "common/BinaryStream.h"
#include "libANGLE/ProgramLinkedResources.h"

#include <cstring>

namespace gl
{

ActiveVariable::ActiveVariable()
{
    std::fill(mIds.begin(), mIds.end(), 0);
}

ActiveVariable::~ActiveVariable() {}

ActiveVariable::ActiveVariable(const ActiveVariable &rhs)            = default;
ActiveVariable &ActiveVariable::operator=(const ActiveVariable &rhs) = default;

void ActiveVariable::setActive(ShaderType shaderType, bool used, uint32_t id)
{
    ASSERT(shaderType != ShaderType::InvalidEnum);
    mActiveUseBits.set(shaderType, used);
    mIds[shaderType] = id;
}

void ActiveVariable::unionReferencesWith(const LinkedUniform &other)
{
    mActiveUseBits |= other.mActiveUseBits;
    for (const ShaderType shaderType : AllShaderTypes())
    {
        ASSERT(mIds[shaderType] == 0 || other.getId(shaderType) == 0 ||
               mIds[shaderType] == other.getId(shaderType));
        if (mIds[shaderType] == 0)
        {
            mIds[shaderType] = other.getId(shaderType);
        }
    }
}

LinkedUniform::LinkedUniform() = default;

LinkedUniform::LinkedUniform(GLenum typeIn,
                             GLenum precisionIn,
                             const std::vector<unsigned int> &arraySizesIn,
                             const int bindingIn,
                             const int offsetIn,
                             const int locationIn,
                             const int bufferIndexIn,
                             const sh::BlockMemberInfo &blockInfoIn)
{
    // arrays are always flattened, which means at most 1D array
    ASSERT(arraySizesIn.size() <= 1);

    memset(this, 0, sizeof(*this));
    SetBitField(type, typeIn);
    SetBitField(precision, precisionIn);
    location = locationIn;
    SetBitField(binding, bindingIn);
    SetBitField(offset, offsetIn);
    SetBitField(bufferIndex, bufferIndexIn);
    outerArraySizeProduct = 1;
    SetBitField(arraySize, arraySizesIn.empty() ? 1u : arraySizesIn[0]);
    SetBitField(flagBits.isArray, !arraySizesIn.empty());
    if (!(blockInfoIn == sh::kDefaultBlockMemberInfo))
    {
        flagBits.isBlock               = 1;
        flagBits.blockIsRowMajorMatrix = blockInfoIn.isRowMajorMatrix;
        SetBitField(blockOffset, blockInfoIn.offset);
        SetBitField(blockArrayStride, blockInfoIn.arrayStride);
        SetBitField(blockMatrixStride, blockInfoIn.matrixStride);
    }
}

LinkedUniform::LinkedUniform(const UsedUniform &usedUniform)
{
    ASSERT(!usedUniform.isArrayOfArrays());
    ASSERT(!usedUniform.isStruct());
    ASSERT(usedUniform.active);
    ASSERT(usedUniform.blockInfo == sh::kDefaultBlockMemberInfo);

    // Note: Ensure every data member is initialized.
    flagBitsAsUByte = 0;
    SetBitField(type, usedUniform.type);
    SetBitField(precision, usedUniform.precision);
    SetBitField(imageUnitFormat, usedUniform.imageUnitFormat);
    location          = usedUniform.location;
    blockOffset       = 0;
    blockArrayStride  = 0;
    blockMatrixStride = 0;
    SetBitField(binding, usedUniform.binding);
    SetBitField(offset, usedUniform.offset);

    SetBitField(bufferIndex, usedUniform.bufferIndex);
    SetBitField(parentArrayIndex, usedUniform.parentArrayIndex());
    SetBitField(outerArraySizeProduct, ArraySizeProduct(usedUniform.outerArraySizes));
    SetBitField(outerArrayOffset, usedUniform.outerArrayOffset);
    SetBitField(arraySize, usedUniform.isArray() ? usedUniform.getArraySizeProduct() : 1u);
    SetBitField(flagBits.isArray, usedUniform.isArray());

    id             = usedUniform.id;
    mActiveUseBits = usedUniform.activeVariable.activeShaders();
    mIds           = usedUniform.activeVariable.getIds();

    SetBitField(flagBits.isFragmentInOut, usedUniform.isFragmentInOut);
    SetBitField(flagBits.texelFetchStaticUse, usedUniform.texelFetchStaticUse);
    ASSERT(!usedUniform.isArray() || arraySize == usedUniform.getArraySizeProduct());
}

BufferVariable::BufferVariable()
    : bufferIndex(-1), blockInfo(sh::kDefaultBlockMemberInfo), topLevelArraySize(-1)
{}

BufferVariable::BufferVariable(GLenum typeIn,
                               GLenum precisionIn,
                               const std::string &nameIn,
                               const std::vector<unsigned int> &arraySizesIn,
                               const int bufferIndexIn,
                               const sh::BlockMemberInfo &blockInfoIn)
    : bufferIndex(bufferIndexIn), blockInfo(blockInfoIn), topLevelArraySize(-1)
{
    type       = typeIn;
    precision  = precisionIn;
    name       = nameIn;
    arraySizes = arraySizesIn;
}

BufferVariable::~BufferVariable() {}

ShaderVariableBuffer::ShaderVariableBuffer() : binding(0), dataSize(0) {}

ShaderVariableBuffer::ShaderVariableBuffer(const ShaderVariableBuffer &other) = default;

ShaderVariableBuffer::~ShaderVariableBuffer() {}

int ShaderVariableBuffer::numActiveVariables() const
{
    return static_cast<int>(memberIndexes.size());
}

InterfaceBlock::InterfaceBlock() : isArray(false), isReadOnly(false), arrayElement(0) {}

InterfaceBlock::InterfaceBlock(const std::string &nameIn,
                               const std::string &mappedNameIn,
                               bool isArrayIn,
                               bool isReadOnlyIn,
                               unsigned int arrayElementIn,
                               unsigned int firstFieldArraySizeIn,
                               int bindingIn)
    : name(nameIn),
      mappedName(mappedNameIn),
      isArray(isArrayIn),
      isReadOnly(isReadOnlyIn),
      arrayElement(arrayElementIn),
      firstFieldArraySize(firstFieldArraySizeIn)
{
    binding = bindingIn;
}

InterfaceBlock::InterfaceBlock(const InterfaceBlock &other) = default;

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
}  // namespace gl
