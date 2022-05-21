//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLArgument.hpp
//
// Copyright 2020-2021 Apple Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#pragma once

#include "MTLDefines.hpp"
#include "MTLHeaderBridge.hpp"
#include "MTLPrivate.hpp"

#include <Foundation/Foundation.hpp>

#include "MTLArgument.hpp"
#include "MTLTexture.hpp"

namespace MTL
{
_MTL_ENUM(NS::UInteger, DataType) {
    DataTypeNone = 0,
    DataTypeStruct = 1,
    DataTypeArray = 2,
    DataTypeFloat = 3,
    DataTypeFloat2 = 4,
    DataTypeFloat3 = 5,
    DataTypeFloat4 = 6,
    DataTypeFloat2x2 = 7,
    DataTypeFloat2x3 = 8,
    DataTypeFloat2x4 = 9,
    DataTypeFloat3x2 = 10,
    DataTypeFloat3x3 = 11,
    DataTypeFloat3x4 = 12,
    DataTypeFloat4x2 = 13,
    DataTypeFloat4x3 = 14,
    DataTypeFloat4x4 = 15,
    DataTypeHalf = 16,
    DataTypeHalf2 = 17,
    DataTypeHalf3 = 18,
    DataTypeHalf4 = 19,
    DataTypeHalf2x2 = 20,
    DataTypeHalf2x3 = 21,
    DataTypeHalf2x4 = 22,
    DataTypeHalf3x2 = 23,
    DataTypeHalf3x3 = 24,
    DataTypeHalf3x4 = 25,
    DataTypeHalf4x2 = 26,
    DataTypeHalf4x3 = 27,
    DataTypeHalf4x4 = 28,
    DataTypeInt = 29,
    DataTypeInt2 = 30,
    DataTypeInt3 = 31,
    DataTypeInt4 = 32,
    DataTypeUInt = 33,
    DataTypeUInt2 = 34,
    DataTypeUInt3 = 35,
    DataTypeUInt4 = 36,
    DataTypeShort = 37,
    DataTypeShort2 = 38,
    DataTypeShort3 = 39,
    DataTypeShort4 = 40,
    DataTypeUShort = 41,
    DataTypeUShort2 = 42,
    DataTypeUShort3 = 43,
    DataTypeUShort4 = 44,
    DataTypeChar = 45,
    DataTypeChar2 = 46,
    DataTypeChar3 = 47,
    DataTypeChar4 = 48,
    DataTypeUChar = 49,
    DataTypeUChar2 = 50,
    DataTypeUChar3 = 51,
    DataTypeUChar4 = 52,
    DataTypeBool = 53,
    DataTypeBool2 = 54,
    DataTypeBool3 = 55,
    DataTypeBool4 = 56,
    DataTypeTexture = 58,
    DataTypeSampler = 59,
    DataTypePointer = 60,
    DataTypeR8Unorm = 62,
    DataTypeR8Snorm = 63,
    DataTypeR16Unorm = 64,
    DataTypeR16Snorm = 65,
    DataTypeRG8Unorm = 66,
    DataTypeRG8Snorm = 67,
    DataTypeRG16Unorm = 68,
    DataTypeRG16Snorm = 69,
    DataTypeRGBA8Unorm = 70,
    DataTypeRGBA8Unorm_sRGB = 71,
    DataTypeRGBA8Snorm = 72,
    DataTypeRGBA16Unorm = 73,
    DataTypeRGBA16Snorm = 74,
    DataTypeRGB10A2Unorm = 75,
    DataTypeRG11B10Float = 76,
    DataTypeRGB9E5Float = 77,
    DataTypeRenderPipeline = 78,
    DataTypeComputePipeline = 79,
    DataTypeIndirectCommandBuffer = 80,
    DataTypeLong = 81,
    DataTypeLong2 = 82,
    DataTypeLong3 = 83,
    DataTypeLong4 = 84,
    DataTypeULong = 85,
    DataTypeULong2 = 86,
    DataTypeULong3 = 87,
    DataTypeULong4 = 88,
    DataTypeVisibleFunctionTable = 115,
    DataTypeIntersectionFunctionTable = 116,
    DataTypePrimitiveAccelerationStructure = 117,
    DataTypeInstanceAccelerationStructure = 118,
};

_MTL_ENUM(NS::UInteger, ArgumentType) {
    ArgumentTypeBuffer = 0,
    ArgumentTypeThreadgroupMemory = 1,
    ArgumentTypeTexture = 2,
    ArgumentTypeSampler = 3,
    ArgumentTypeImageblockData = 16,
    ArgumentTypeImageblock = 17,
    ArgumentTypeVisibleFunctionTable = 24,
    ArgumentTypePrimitiveAccelerationStructure = 25,
    ArgumentTypeInstanceAccelerationStructure = 26,
    ArgumentTypeIntersectionFunctionTable = 27,
};

_MTL_ENUM(NS::UInteger, ArgumentAccess) {
    ArgumentAccessReadOnly = 0,
    ArgumentAccessReadWrite = 1,
    ArgumentAccessWriteOnly = 2,
};

class Type : public NS::Referencing<Type>
{
public:
    static class Type* alloc();

    class Type*        init();

    MTL::DataType      dataType() const;
};

class StructMember : public NS::Referencing<StructMember>
{
public:
    static class StructMember*  alloc();

    class StructMember*         init();

    NS::String*                 name() const;

    NS::UInteger                offset() const;

    MTL::DataType               dataType() const;

    class StructType*           structType();

    class ArrayType*            arrayType();

    class TextureReferenceType* textureReferenceType();

    class PointerType*          pointerType();

    NS::UInteger                argumentIndex() const;
};

class StructType : public NS::Referencing<StructType, Type>
{
public:
    static class StructType* alloc();

    class StructType*        init();

    NS::Array*               members() const;

    class StructMember*      memberByName(const NS::String* name);
};

class ArrayType : public NS::Referencing<ArrayType, Type>
{
public:
    static class ArrayType*     alloc();

    class ArrayType*            init();

    MTL::DataType               elementType() const;

    NS::UInteger                arrayLength() const;

    NS::UInteger                stride() const;

    NS::UInteger                argumentIndexStride() const;

    class StructType*           elementStructType();

    class ArrayType*            elementArrayType();

    class TextureReferenceType* elementTextureReferenceType();

    class PointerType*          elementPointerType();
};

class PointerType : public NS::Referencing<PointerType, Type>
{
public:
    static class PointerType* alloc();

    class PointerType*        init();

    MTL::DataType             elementType() const;

    MTL::ArgumentAccess       access() const;

    NS::UInteger              alignment() const;

    NS::UInteger              dataSize() const;

    bool                      elementIsArgumentBuffer() const;

    class StructType*         elementStructType();

    class ArrayType*          elementArrayType();
};

class TextureReferenceType : public NS::Referencing<TextureReferenceType, Type>
{
public:
    static class TextureReferenceType* alloc();

    class TextureReferenceType*        init();

    MTL::DataType                      textureDataType() const;

    MTL::TextureType                   textureType() const;

    MTL::ArgumentAccess                access() const;

    bool                               isDepthTexture() const;
};

class Argument : public NS::Referencing<Argument>
{
public:
    static class Argument* alloc();

    class Argument*        init();

    NS::String*            name() const;

    MTL::ArgumentType      type() const;

    MTL::ArgumentAccess    access() const;

    NS::UInteger           index() const;

    bool                   active() const;

    NS::UInteger           bufferAlignment() const;

    NS::UInteger           bufferDataSize() const;

    MTL::DataType          bufferDataType() const;

    class StructType*      bufferStructType() const;

    class PointerType*     bufferPointerType() const;

    NS::UInteger           threadgroupMemoryAlignment() const;

    NS::UInteger           threadgroupMemoryDataSize() const;

    MTL::TextureType       textureType() const;

    MTL::DataType          textureDataType() const;

    bool                   isDepthTexture() const;

    NS::UInteger           arrayLength() const;
};

}

// static method: alloc
_MTL_INLINE MTL::Type* MTL::Type::alloc()
{
    return NS::Object::alloc<MTL::Type>(_MTL_PRIVATE_CLS(MTLType));
}

// method: init
_MTL_INLINE MTL::Type* MTL::Type::init()
{
    return NS::Object::init<MTL::Type>();
}

// property: dataType
_MTL_INLINE MTL::DataType MTL::Type::dataType() const
{
    return Object::sendMessage<MTL::DataType>(this, _MTL_PRIVATE_SEL(dataType));
}

// static method: alloc
_MTL_INLINE MTL::StructMember* MTL::StructMember::alloc()
{
    return NS::Object::alloc<MTL::StructMember>(_MTL_PRIVATE_CLS(MTLStructMember));
}

// method: init
_MTL_INLINE MTL::StructMember* MTL::StructMember::init()
{
    return NS::Object::init<MTL::StructMember>();
}

// property: name
_MTL_INLINE NS::String* MTL::StructMember::name() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(name));
}

// property: offset
_MTL_INLINE NS::UInteger MTL::StructMember::offset() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(offset));
}

// property: dataType
_MTL_INLINE MTL::DataType MTL::StructMember::dataType() const
{
    return Object::sendMessage<MTL::DataType>(this, _MTL_PRIVATE_SEL(dataType));
}

// method: structType
_MTL_INLINE MTL::StructType* MTL::StructMember::structType()
{
    return Object::sendMessage<MTL::StructType*>(this, _MTL_PRIVATE_SEL(structType));
}

// method: arrayType
_MTL_INLINE MTL::ArrayType* MTL::StructMember::arrayType()
{
    return Object::sendMessage<MTL::ArrayType*>(this, _MTL_PRIVATE_SEL(arrayType));
}

// method: textureReferenceType
_MTL_INLINE MTL::TextureReferenceType* MTL::StructMember::textureReferenceType()
{
    return Object::sendMessage<MTL::TextureReferenceType*>(this, _MTL_PRIVATE_SEL(textureReferenceType));
}

// method: pointerType
_MTL_INLINE MTL::PointerType* MTL::StructMember::pointerType()
{
    return Object::sendMessage<MTL::PointerType*>(this, _MTL_PRIVATE_SEL(pointerType));
}

// property: argumentIndex
_MTL_INLINE NS::UInteger MTL::StructMember::argumentIndex() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(argumentIndex));
}

// static method: alloc
_MTL_INLINE MTL::StructType* MTL::StructType::alloc()
{
    return NS::Object::alloc<MTL::StructType>(_MTL_PRIVATE_CLS(MTLStructType));
}

// method: init
_MTL_INLINE MTL::StructType* MTL::StructType::init()
{
    return NS::Object::init<MTL::StructType>();
}

// property: members
_MTL_INLINE NS::Array* MTL::StructType::members() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(members));
}

// method: memberByName:
_MTL_INLINE MTL::StructMember* MTL::StructType::memberByName(const NS::String* name)
{
    return Object::sendMessage<MTL::StructMember*>(this, _MTL_PRIVATE_SEL(memberByName_), name);
}

// static method: alloc
_MTL_INLINE MTL::ArrayType* MTL::ArrayType::alloc()
{
    return NS::Object::alloc<MTL::ArrayType>(_MTL_PRIVATE_CLS(MTLArrayType));
}

// method: init
_MTL_INLINE MTL::ArrayType* MTL::ArrayType::init()
{
    return NS::Object::init<MTL::ArrayType>();
}

// property: elementType
_MTL_INLINE MTL::DataType MTL::ArrayType::elementType() const
{
    return Object::sendMessage<MTL::DataType>(this, _MTL_PRIVATE_SEL(elementType));
}

// property: arrayLength
_MTL_INLINE NS::UInteger MTL::ArrayType::arrayLength() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(arrayLength));
}

// property: stride
_MTL_INLINE NS::UInteger MTL::ArrayType::stride() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(stride));
}

// property: argumentIndexStride
_MTL_INLINE NS::UInteger MTL::ArrayType::argumentIndexStride() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(argumentIndexStride));
}

// method: elementStructType
_MTL_INLINE MTL::StructType* MTL::ArrayType::elementStructType()
{
    return Object::sendMessage<MTL::StructType*>(this, _MTL_PRIVATE_SEL(elementStructType));
}

// method: elementArrayType
_MTL_INLINE MTL::ArrayType* MTL::ArrayType::elementArrayType()
{
    return Object::sendMessage<MTL::ArrayType*>(this, _MTL_PRIVATE_SEL(elementArrayType));
}

// method: elementTextureReferenceType
_MTL_INLINE MTL::TextureReferenceType* MTL::ArrayType::elementTextureReferenceType()
{
    return Object::sendMessage<MTL::TextureReferenceType*>(this, _MTL_PRIVATE_SEL(elementTextureReferenceType));
}

// method: elementPointerType
_MTL_INLINE MTL::PointerType* MTL::ArrayType::elementPointerType()
{
    return Object::sendMessage<MTL::PointerType*>(this, _MTL_PRIVATE_SEL(elementPointerType));
}

// static method: alloc
_MTL_INLINE MTL::PointerType* MTL::PointerType::alloc()
{
    return NS::Object::alloc<MTL::PointerType>(_MTL_PRIVATE_CLS(MTLPointerType));
}

// method: init
_MTL_INLINE MTL::PointerType* MTL::PointerType::init()
{
    return NS::Object::init<MTL::PointerType>();
}

// property: elementType
_MTL_INLINE MTL::DataType MTL::PointerType::elementType() const
{
    return Object::sendMessage<MTL::DataType>(this, _MTL_PRIVATE_SEL(elementType));
}

// property: access
_MTL_INLINE MTL::ArgumentAccess MTL::PointerType::access() const
{
    return Object::sendMessage<MTL::ArgumentAccess>(this, _MTL_PRIVATE_SEL(access));
}

// property: alignment
_MTL_INLINE NS::UInteger MTL::PointerType::alignment() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(alignment));
}

// property: dataSize
_MTL_INLINE NS::UInteger MTL::PointerType::dataSize() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(dataSize));
}

// property: elementIsArgumentBuffer
_MTL_INLINE bool MTL::PointerType::elementIsArgumentBuffer() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(elementIsArgumentBuffer));
}

// method: elementStructType
_MTL_INLINE MTL::StructType* MTL::PointerType::elementStructType()
{
    return Object::sendMessage<MTL::StructType*>(this, _MTL_PRIVATE_SEL(elementStructType));
}

// method: elementArrayType
_MTL_INLINE MTL::ArrayType* MTL::PointerType::elementArrayType()
{
    return Object::sendMessage<MTL::ArrayType*>(this, _MTL_PRIVATE_SEL(elementArrayType));
}

// static method: alloc
_MTL_INLINE MTL::TextureReferenceType* MTL::TextureReferenceType::alloc()
{
    return NS::Object::alloc<MTL::TextureReferenceType>(_MTL_PRIVATE_CLS(MTLTextureReferenceType));
}

// method: init
_MTL_INLINE MTL::TextureReferenceType* MTL::TextureReferenceType::init()
{
    return NS::Object::init<MTL::TextureReferenceType>();
}

// property: textureDataType
_MTL_INLINE MTL::DataType MTL::TextureReferenceType::textureDataType() const
{
    return Object::sendMessage<MTL::DataType>(this, _MTL_PRIVATE_SEL(textureDataType));
}

// property: textureType
_MTL_INLINE MTL::TextureType MTL::TextureReferenceType::textureType() const
{
    return Object::sendMessage<MTL::TextureType>(this, _MTL_PRIVATE_SEL(textureType));
}

// property: access
_MTL_INLINE MTL::ArgumentAccess MTL::TextureReferenceType::access() const
{
    return Object::sendMessage<MTL::ArgumentAccess>(this, _MTL_PRIVATE_SEL(access));
}

// property: isDepthTexture
_MTL_INLINE bool MTL::TextureReferenceType::isDepthTexture() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isDepthTexture));
}

// static method: alloc
_MTL_INLINE MTL::Argument* MTL::Argument::alloc()
{
    return NS::Object::alloc<MTL::Argument>(_MTL_PRIVATE_CLS(MTLArgument));
}

// method: init
_MTL_INLINE MTL::Argument* MTL::Argument::init()
{
    return NS::Object::init<MTL::Argument>();
}

// property: name
_MTL_INLINE NS::String* MTL::Argument::name() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(name));
}

// property: type
_MTL_INLINE MTL::ArgumentType MTL::Argument::type() const
{
    return Object::sendMessage<MTL::ArgumentType>(this, _MTL_PRIVATE_SEL(type));
}

// property: access
_MTL_INLINE MTL::ArgumentAccess MTL::Argument::access() const
{
    return Object::sendMessage<MTL::ArgumentAccess>(this, _MTL_PRIVATE_SEL(access));
}

// property: index
_MTL_INLINE NS::UInteger MTL::Argument::index() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(index));
}

// property: active
_MTL_INLINE bool MTL::Argument::active() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isActive));
}

// property: bufferAlignment
_MTL_INLINE NS::UInteger MTL::Argument::bufferAlignment() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(bufferAlignment));
}

// property: bufferDataSize
_MTL_INLINE NS::UInteger MTL::Argument::bufferDataSize() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(bufferDataSize));
}

// property: bufferDataType
_MTL_INLINE MTL::DataType MTL::Argument::bufferDataType() const
{
    return Object::sendMessage<MTL::DataType>(this, _MTL_PRIVATE_SEL(bufferDataType));
}

// property: bufferStructType
_MTL_INLINE MTL::StructType* MTL::Argument::bufferStructType() const
{
    return Object::sendMessage<MTL::StructType*>(this, _MTL_PRIVATE_SEL(bufferStructType));
}

// property: bufferPointerType
_MTL_INLINE MTL::PointerType* MTL::Argument::bufferPointerType() const
{
    return Object::sendMessage<MTL::PointerType*>(this, _MTL_PRIVATE_SEL(bufferPointerType));
}

// property: threadgroupMemoryAlignment
_MTL_INLINE NS::UInteger MTL::Argument::threadgroupMemoryAlignment() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(threadgroupMemoryAlignment));
}

// property: threadgroupMemoryDataSize
_MTL_INLINE NS::UInteger MTL::Argument::threadgroupMemoryDataSize() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(threadgroupMemoryDataSize));
}

// property: textureType
_MTL_INLINE MTL::TextureType MTL::Argument::textureType() const
{
    return Object::sendMessage<MTL::TextureType>(this, _MTL_PRIVATE_SEL(textureType));
}

// property: textureDataType
_MTL_INLINE MTL::DataType MTL::Argument::textureDataType() const
{
    return Object::sendMessage<MTL::DataType>(this, _MTL_PRIVATE_SEL(textureDataType));
}

// property: isDepthTexture
_MTL_INLINE bool MTL::Argument::isDepthTexture() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isDepthTexture));
}

// property: arrayLength
_MTL_INLINE NS::UInteger MTL::Argument::arrayLength() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(arrayLength));
}
