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
struct UniformTypeInfo;

struct ActiveVariable
{
    ActiveVariable();
    ActiveVariable(const ActiveVariable &rhs);
    virtual ~ActiveVariable();

    ActiveVariable &operator=(const ActiveVariable &rhs);

    ShaderType getFirstActiveShaderType() const
    {
        return static_cast<ShaderType>(ScanForward(mActiveUseBits.bits()));
    }
    void setActive(ShaderType shaderType, bool used, uint32_t id);
    void unionReferencesWith(const ActiveVariable &other);
    bool isActive(ShaderType shaderType) const
    {
        ASSERT(shaderType != ShaderType::InvalidEnum);
        return mActiveUseBits[shaderType];
    }
    const ShaderMap<uint32_t> &getIds() const { return mIds; }
    uint32_t getId(ShaderType shaderType) const { return mIds[shaderType]; }
    ShaderBitSet activeShaders() const { return mActiveUseBits; }
    GLuint activeShaderCount() const { return static_cast<GLuint>(mActiveUseBits.count()); }

  private:
    ShaderBitSet mActiveUseBits;
    // The id of a linked variable in each shader stage.  This id originates from
    // sh::ShaderVariable::id or sh::InterfaceBlock::id
    ShaderMap<uint32_t> mIds;
};

// Helper struct representing a single shader uniform
struct LinkedUniform : public sh::ShaderVariable, public ActiveVariable
{
    LinkedUniform();
    LinkedUniform(GLenum type,
                  GLenum precision,
                  const std::string &name,
                  const std::vector<unsigned int> &arraySizes,
                  const int binding,
                  const int offset,
                  const int location,
                  const int bufferIndex,
                  const sh::BlockMemberInfo &blockInfo);
    LinkedUniform(const sh::ShaderVariable &uniform);
    LinkedUniform(const LinkedUniform &uniform);
    LinkedUniform &operator=(const LinkedUniform &uniform);
    ~LinkedUniform() override;

    bool isSampler() const { return typeInfo->isSampler; }
    bool isImage() const { return typeInfo->isImageType; }
    bool isAtomicCounter() const { return IsAtomicCounterType(type); }
    bool isInDefaultBlock() const { return bufferIndex == -1; }
    bool isField() const { return name.find('.') != std::string::npos; }
    size_t getElementSize() const { return typeInfo->externalSize; }
    size_t getElementComponents() const { return typeInfo->componentCount; }

    const UniformTypeInfo *typeInfo;

    // Identifies the containing buffer backed resource -- interface block or atomic counter buffer.
    int bufferIndex;
    sh::BlockMemberInfo blockInfo;
    std::vector<unsigned int> outerArraySizes;
    unsigned int outerArrayOffset;
};

struct BufferVariable : public sh::ShaderVariable, public ActiveVariable
{
    BufferVariable();
    BufferVariable(GLenum type,
                   GLenum precision,
                   const std::string &name,
                   const std::vector<unsigned int> &arraySizes,
                   const int bufferIndex,
                   const sh::BlockMemberInfo &blockInfo);
    ~BufferVariable() override;

    int bufferIndex;
    sh::BlockMemberInfo blockInfo;

    int topLevelArraySize;
};

// Parent struct for atomic counter, uniform block, and shader storage block buffer, which all
// contain a group of shader variables, and have a GL buffer backed.
struct ShaderVariableBuffer : public ActiveVariable
{
    ShaderVariableBuffer();
    ShaderVariableBuffer(const ShaderVariableBuffer &other);
    ~ShaderVariableBuffer() override;
    int numActiveVariables() const;

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
