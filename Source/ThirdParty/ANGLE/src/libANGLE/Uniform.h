//
// Copyright 2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef LIBANGLE_UNIFORM_H_
#define LIBANGLE_UNIFORM_H_

#include <string>
#include <vector>

#include "angle_gl.h"
#include "common/MemoryBuffer.h"
#include "common/debug.h"
#include "common/utilities.h"
#include "compiler/translator/blocklayout.h"
#include "libANGLE/angletypes.h"

namespace gl
{
class BinaryInputStream;
class BinaryOutputStream;
struct UniformTypeInfo;
struct UsedUniform;
struct LinkedUniform;

// Note: keep this struct memcpy-able: i.e, a simple struct with basic types only and no virtual
// functions. LinkedUniform relies on this so that it can use memcpy to initialize uniform for
// performance.
struct ActiveVariable
{
    ActiveVariable();
    ActiveVariable(const ActiveVariable &rhs);
    ~ActiveVariable();

    ActiveVariable &operator=(const ActiveVariable &rhs);

    ShaderType getFirstActiveShaderType() const
    {
        return static_cast<ShaderType>(ScanForward(mActiveUseBits.bits()));
    }
    void setActive(ShaderType shaderType, bool used, uint32_t id);
    void unionReferencesWith(const LinkedUniform &otherUniform);
    bool isActive(ShaderType shaderType) const
    {
        ASSERT(shaderType != ShaderType::InvalidEnum);
        return mActiveUseBits[shaderType];
    }
    const ShaderMap<uint32_t> &getIds() const { return mIds; }
    uint32_t getId(ShaderType shaderType) const { return mIds[shaderType]; }
    ShaderBitSet activeShaders() const { return mActiveUseBits; }

  private:
    ShaderBitSet mActiveUseBits;
    // The id of a linked variable in each shader stage.  This id originates from
    // sh::ShaderVariable::id or sh::InterfaceBlock::id
    ShaderMap<uint32_t> mIds;
};

// Important: This struct must have basic data types only, so that we can initialize with memcpy. Do
// not put any std::vector or objects with virtual functions in it.
// Helper struct representing a single shader uniform. Most of this structure's data member and
// access functions mirrors ShaderVariable; See ShaderVars.h for more info.
ANGLE_ENABLE_STRUCT_PADDING_WARNINGS
struct LinkedUniform
{
    LinkedUniform();
    LinkedUniform(GLenum typeIn,
                  GLenum precisionIn,
                  const std::vector<unsigned int> &arraySizesIn,
                  const int bindingIn,
                  const int offsetIn,
                  const int locationIn,
                  const int bufferIndexIn,
                  const sh::BlockMemberInfo &blockInfoIn);
    LinkedUniform(const UsedUniform &usedUniform);

    bool isSampler() const { return GetUniformTypeInfo(type).isSampler; }
    bool isImage() const { return GetUniformTypeInfo(type).isImageType; }
    bool isAtomicCounter() const { return IsAtomicCounterType(type); }
    bool isInDefaultBlock() const { return bufferIndex == -1; }
    size_t getElementSize() const { return GetUniformTypeInfo(type).externalSize; }
    GLint getElementComponents() const { return GetUniformTypeInfo(type).componentCount; }

    bool isTexelFetchStaticUse() const { return flagBits.texelFetchStaticUse; }
    bool isFragmentInOut() const { return flagBits.isFragmentInOut; }

    bool isArray() const { return flagBits.isArray; }
    uint16_t getBasicTypeElementCount() const
    {
        ASSERT(flagBits.isArray || arraySize == 1u);
        return arraySize;
    }

    GLenum getType() const { return type; }
    uint16_t getOuterArrayOffset() const { return outerArrayOffset; }
    uint16_t getOuterArraySizeProduct() const { return outerArraySizeProduct; }
    int16_t getBinding() const { return binding; }
    int16_t getOffset() const { return offset; }
    int getBufferIndex() const { return bufferIndex; }
    int getLocation() const { return location; }
    GLenum getImageUnitFormat() const { return imageUnitFormat; }

    ShaderType getFirstActiveShaderType() const
    {
        return static_cast<ShaderType>(ScanForward(mActiveUseBits.bits()));
    }
    void setActive(ShaderType shaderType, bool used, uint32_t _id)
    {
        mActiveUseBits.set(shaderType, used);
        mIds[shaderType] = id;
    }
    bool isActive(ShaderType shaderType) const { return mActiveUseBits[shaderType]; }
    const ShaderMap<uint32_t> &getIds() const { return mIds; }
    uint32_t getId(ShaderType shaderType) const { return mIds[shaderType]; }
    ShaderBitSet activeShaders() const { return mActiveUseBits; }
    GLuint activeShaderCount() const { return static_cast<GLuint>(mActiveUseBits.count()); }

    uint16_t type;
    uint16_t precision;

    int location;

    // These are from sh::struct BlockMemberInfo struct. See locklayout.h for detail.
    uint16_t blockOffset;
    uint16_t blockArrayStride;

    uint16_t blockMatrixStride;
    uint16_t imageUnitFormat;

    // maxUniformVectorsCount is 4K due to we clamp maxUniformBlockSize to 64KB. All of these
    // variable should be enough to pack into 16 bits to reduce the size of mUniforms.
    int16_t binding;
    int16_t bufferIndex;

    int16_t offset;
    uint16_t arraySize;

    uint16_t outerArraySizeProduct;
    uint16_t outerArrayOffset;

    uint16_t parentArrayIndex;
    union
    {
        struct
        {
            uint8_t isFragmentInOut : 1;
            uint8_t texelFetchStaticUse : 1;
            uint8_t isArray : 1;
            uint8_t blockIsRowMajorMatrix : 1;
            uint8_t isBlock : 1;
            uint8_t padding : 3;
        } flagBits;
        uint8_t flagBitsAsUByte;
    };
    ShaderBitSet mActiveUseBits;

    uint32_t id;

    // The id of a linked variable in each shader stage.  This id originates from
    // sh::ShaderVariable::id or sh::InterfaceBlock::id
    ShaderMap<uint32_t> mIds;
};
ANGLE_DISABLE_STRUCT_PADDING_WARNINGS

struct BufferVariable : public sh::ShaderVariable
{
    BufferVariable();
    BufferVariable(GLenum type,
                   GLenum precision,
                   const std::string &name,
                   const std::vector<unsigned int> &arraySizes,
                   const int bufferIndex,
                   const sh::BlockMemberInfo &blockInfo);
    ~BufferVariable();

    void setActive(ShaderType shaderType, bool used, uint32_t _id)
    {
        activeVariable.setActive(shaderType, used, _id);
    }
    bool isActive(ShaderType shaderType) const { return activeVariable.isActive(shaderType); }
    uint32_t getId(ShaderType shaderType) const { return activeVariable.getId(shaderType); }
    ShaderBitSet activeShaders() const { return activeVariable.activeShaders(); }

    ActiveVariable activeVariable;
    int bufferIndex;
    sh::BlockMemberInfo blockInfo;

    int topLevelArraySize;
};

// Parent struct for atomic counter, uniform block, and shader storage block buffer, which all
// contain a group of shader variables, and have a GL buffer backed.
struct ShaderVariableBuffer
{
    ShaderVariableBuffer();
    ShaderVariableBuffer(const ShaderVariableBuffer &other);
    ~ShaderVariableBuffer();

    ShaderType getFirstActiveShaderType() const
    {
        return activeVariable.getFirstActiveShaderType();
    }
    void setActive(ShaderType shaderType, bool used, uint32_t _id)
    {
        activeVariable.setActive(shaderType, used, _id);
    }
    void unionReferencesWith(const LinkedUniform &otherUniform)
    {
        activeVariable.unionReferencesWith(otherUniform);
    }
    bool isActive(ShaderType shaderType) const { return activeVariable.isActive(shaderType); }
    const ShaderMap<uint32_t> &getIds() const { return activeVariable.getIds(); }
    uint32_t getId(ShaderType shaderType) const { return activeVariable.getId(shaderType); }
    ShaderBitSet activeShaders() const { return activeVariable.activeShaders(); }
    int numActiveVariables() const;

    ActiveVariable activeVariable;
    int binding;
    unsigned int dataSize;
    std::vector<unsigned int> memberIndexes;
};

using AtomicCounterBuffer = ShaderVariableBuffer;

// Helper struct representing a single shader interface block
struct InterfaceBlock : public ShaderVariableBuffer
{
    InterfaceBlock();
    InterfaceBlock(const std::string &nameIn,
                   const std::string &mappedNameIn,
                   bool isArrayIn,
                   bool isReadOnlyIn,
                   unsigned int arrayElementIn,
                   unsigned int firstFieldArraySizeIn,
                   int bindingIn);
    InterfaceBlock(const InterfaceBlock &other);

    std::string nameWithArrayIndex() const;
    std::string mappedNameWithArrayIndex() const;

    std::string name;
    std::string mappedName;
    bool isArray;
    // Only valid for SSBOs, specifies whether it has the readonly qualifier.
    bool isReadOnly;
    unsigned int arrayElement;
    unsigned int firstFieldArraySize;
};

}  // namespace gl

#endif  // LIBANGLE_UNIFORM_H_
