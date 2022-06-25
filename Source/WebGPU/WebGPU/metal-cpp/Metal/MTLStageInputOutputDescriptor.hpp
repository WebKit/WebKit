//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLStageInputOutputDescriptor.hpp
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

#include "MTLStageInputOutputDescriptor.hpp"

namespace MTL
{
_MTL_ENUM(NS::UInteger, AttributeFormat) {
    AttributeFormatInvalid = 0,
    AttributeFormatUChar2 = 1,
    AttributeFormatUChar3 = 2,
    AttributeFormatUChar4 = 3,
    AttributeFormatChar2 = 4,
    AttributeFormatChar3 = 5,
    AttributeFormatChar4 = 6,
    AttributeFormatUChar2Normalized = 7,
    AttributeFormatUChar3Normalized = 8,
    AttributeFormatUChar4Normalized = 9,
    AttributeFormatChar2Normalized = 10,
    AttributeFormatChar3Normalized = 11,
    AttributeFormatChar4Normalized = 12,
    AttributeFormatUShort2 = 13,
    AttributeFormatUShort3 = 14,
    AttributeFormatUShort4 = 15,
    AttributeFormatShort2 = 16,
    AttributeFormatShort3 = 17,
    AttributeFormatShort4 = 18,
    AttributeFormatUShort2Normalized = 19,
    AttributeFormatUShort3Normalized = 20,
    AttributeFormatUShort4Normalized = 21,
    AttributeFormatShort2Normalized = 22,
    AttributeFormatShort3Normalized = 23,
    AttributeFormatShort4Normalized = 24,
    AttributeFormatHalf2 = 25,
    AttributeFormatHalf3 = 26,
    AttributeFormatHalf4 = 27,
    AttributeFormatFloat = 28,
    AttributeFormatFloat2 = 29,
    AttributeFormatFloat3 = 30,
    AttributeFormatFloat4 = 31,
    AttributeFormatInt = 32,
    AttributeFormatInt2 = 33,
    AttributeFormatInt3 = 34,
    AttributeFormatInt4 = 35,
    AttributeFormatUInt = 36,
    AttributeFormatUInt2 = 37,
    AttributeFormatUInt3 = 38,
    AttributeFormatUInt4 = 39,
    AttributeFormatInt1010102Normalized = 40,
    AttributeFormatUInt1010102Normalized = 41,
    AttributeFormatUChar4Normalized_BGRA = 42,
    AttributeFormatUChar = 45,
    AttributeFormatChar = 46,
    AttributeFormatUCharNormalized = 47,
    AttributeFormatCharNormalized = 48,
    AttributeFormatUShort = 49,
    AttributeFormatShort = 50,
    AttributeFormatUShortNormalized = 51,
    AttributeFormatShortNormalized = 52,
    AttributeFormatHalf = 53,
};

_MTL_ENUM(NS::UInteger, IndexType) {
    IndexTypeUInt16 = 0,
    IndexTypeUInt32 = 1,
};

_MTL_ENUM(NS::UInteger, StepFunction) {
    StepFunctionConstant = 0,
    StepFunctionPerVertex = 1,
    StepFunctionPerInstance = 2,
    StepFunctionPerPatch = 3,
    StepFunctionPerPatchControlPoint = 4,
    StepFunctionThreadPositionInGridX = 5,
    StepFunctionThreadPositionInGridY = 6,
    StepFunctionThreadPositionInGridXIndexed = 7,
    StepFunctionThreadPositionInGridYIndexed = 8,
};

class BufferLayoutDescriptor : public NS::Copying<BufferLayoutDescriptor>
{
public:
    static class BufferLayoutDescriptor* alloc();

    class BufferLayoutDescriptor*        init();

    NS::UInteger                         stride() const;
    void                                 setStride(NS::UInteger stride);

    MTL::StepFunction                    stepFunction() const;
    void                                 setStepFunction(MTL::StepFunction stepFunction);

    NS::UInteger                         stepRate() const;
    void                                 setStepRate(NS::UInteger stepRate);
};

class BufferLayoutDescriptorArray : public NS::Referencing<BufferLayoutDescriptorArray>
{
public:
    static class BufferLayoutDescriptorArray* alloc();

    class BufferLayoutDescriptorArray*        init();

    class BufferLayoutDescriptor*             object(NS::UInteger index);

    void                                      setObject(const class BufferLayoutDescriptor* bufferDesc, NS::UInteger index);
};

class AttributeDescriptor : public NS::Copying<AttributeDescriptor>
{
public:
    static class AttributeDescriptor* alloc();

    class AttributeDescriptor*        init();

    MTL::AttributeFormat              format() const;
    void                              setFormat(MTL::AttributeFormat format);

    NS::UInteger                      offset() const;
    void                              setOffset(NS::UInteger offset);

    NS::UInteger                      bufferIndex() const;
    void                              setBufferIndex(NS::UInteger bufferIndex);
};

class AttributeDescriptorArray : public NS::Referencing<AttributeDescriptorArray>
{
public:
    static class AttributeDescriptorArray* alloc();

    class AttributeDescriptorArray*        init();

    class AttributeDescriptor*             object(NS::UInteger index);

    void                                   setObject(const class AttributeDescriptor* attributeDesc, NS::UInteger index);
};

class StageInputOutputDescriptor : public NS::Copying<StageInputOutputDescriptor>
{
public:
    static class StageInputOutputDescriptor* alloc();

    class StageInputOutputDescriptor*        init();

    static class StageInputOutputDescriptor* stageInputOutputDescriptor();

    class BufferLayoutDescriptorArray*       layouts() const;

    class AttributeDescriptorArray*          attributes() const;

    MTL::IndexType                           indexType() const;
    void                                     setIndexType(MTL::IndexType indexType);

    NS::UInteger                             indexBufferIndex() const;
    void                                     setIndexBufferIndex(NS::UInteger indexBufferIndex);

    void                                     reset();
};

}

// static method: alloc
_MTL_INLINE MTL::BufferLayoutDescriptor* MTL::BufferLayoutDescriptor::alloc()
{
    return NS::Object::alloc<MTL::BufferLayoutDescriptor>(_MTL_PRIVATE_CLS(MTLBufferLayoutDescriptor));
}

// method: init
_MTL_INLINE MTL::BufferLayoutDescriptor* MTL::BufferLayoutDescriptor::init()
{
    return NS::Object::init<MTL::BufferLayoutDescriptor>();
}

// property: stride
_MTL_INLINE NS::UInteger MTL::BufferLayoutDescriptor::stride() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(stride));
}

_MTL_INLINE void MTL::BufferLayoutDescriptor::setStride(NS::UInteger stride)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStride_), stride);
}

// property: stepFunction
_MTL_INLINE MTL::StepFunction MTL::BufferLayoutDescriptor::stepFunction() const
{
    return Object::sendMessage<MTL::StepFunction>(this, _MTL_PRIVATE_SEL(stepFunction));
}

_MTL_INLINE void MTL::BufferLayoutDescriptor::setStepFunction(MTL::StepFunction stepFunction)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStepFunction_), stepFunction);
}

// property: stepRate
_MTL_INLINE NS::UInteger MTL::BufferLayoutDescriptor::stepRate() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(stepRate));
}

_MTL_INLINE void MTL::BufferLayoutDescriptor::setStepRate(NS::UInteger stepRate)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStepRate_), stepRate);
}

// static method: alloc
_MTL_INLINE MTL::BufferLayoutDescriptorArray* MTL::BufferLayoutDescriptorArray::alloc()
{
    return NS::Object::alloc<MTL::BufferLayoutDescriptorArray>(_MTL_PRIVATE_CLS(MTLBufferLayoutDescriptorArray));
}

// method: init
_MTL_INLINE MTL::BufferLayoutDescriptorArray* MTL::BufferLayoutDescriptorArray::init()
{
    return NS::Object::init<MTL::BufferLayoutDescriptorArray>();
}

// method: objectAtIndexedSubscript:
_MTL_INLINE MTL::BufferLayoutDescriptor* MTL::BufferLayoutDescriptorArray::object(NS::UInteger index)
{
    return Object::sendMessage<MTL::BufferLayoutDescriptor*>(this, _MTL_PRIVATE_SEL(objectAtIndexedSubscript_), index);
}

// method: setObject:atIndexedSubscript:
_MTL_INLINE void MTL::BufferLayoutDescriptorArray::setObject(const MTL::BufferLayoutDescriptor* bufferDesc, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setObject_atIndexedSubscript_), bufferDesc, index);
}

// static method: alloc
_MTL_INLINE MTL::AttributeDescriptor* MTL::AttributeDescriptor::alloc()
{
    return NS::Object::alloc<MTL::AttributeDescriptor>(_MTL_PRIVATE_CLS(MTLAttributeDescriptor));
}

// method: init
_MTL_INLINE MTL::AttributeDescriptor* MTL::AttributeDescriptor::init()
{
    return NS::Object::init<MTL::AttributeDescriptor>();
}

// property: format
_MTL_INLINE MTL::AttributeFormat MTL::AttributeDescriptor::format() const
{
    return Object::sendMessage<MTL::AttributeFormat>(this, _MTL_PRIVATE_SEL(format));
}

_MTL_INLINE void MTL::AttributeDescriptor::setFormat(MTL::AttributeFormat format)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFormat_), format);
}

// property: offset
_MTL_INLINE NS::UInteger MTL::AttributeDescriptor::offset() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(offset));
}

_MTL_INLINE void MTL::AttributeDescriptor::setOffset(NS::UInteger offset)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setOffset_), offset);
}

// property: bufferIndex
_MTL_INLINE NS::UInteger MTL::AttributeDescriptor::bufferIndex() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(bufferIndex));
}

_MTL_INLINE void MTL::AttributeDescriptor::setBufferIndex(NS::UInteger bufferIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setBufferIndex_), bufferIndex);
}

// static method: alloc
_MTL_INLINE MTL::AttributeDescriptorArray* MTL::AttributeDescriptorArray::alloc()
{
    return NS::Object::alloc<MTL::AttributeDescriptorArray>(_MTL_PRIVATE_CLS(MTLAttributeDescriptorArray));
}

// method: init
_MTL_INLINE MTL::AttributeDescriptorArray* MTL::AttributeDescriptorArray::init()
{
    return NS::Object::init<MTL::AttributeDescriptorArray>();
}

// method: objectAtIndexedSubscript:
_MTL_INLINE MTL::AttributeDescriptor* MTL::AttributeDescriptorArray::object(NS::UInteger index)
{
    return Object::sendMessage<MTL::AttributeDescriptor*>(this, _MTL_PRIVATE_SEL(objectAtIndexedSubscript_), index);
}

// method: setObject:atIndexedSubscript:
_MTL_INLINE void MTL::AttributeDescriptorArray::setObject(const MTL::AttributeDescriptor* attributeDesc, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setObject_atIndexedSubscript_), attributeDesc, index);
}

// static method: alloc
_MTL_INLINE MTL::StageInputOutputDescriptor* MTL::StageInputOutputDescriptor::alloc()
{
    return NS::Object::alloc<MTL::StageInputOutputDescriptor>(_MTL_PRIVATE_CLS(MTLStageInputOutputDescriptor));
}

// method: init
_MTL_INLINE MTL::StageInputOutputDescriptor* MTL::StageInputOutputDescriptor::init()
{
    return NS::Object::init<MTL::StageInputOutputDescriptor>();
}

// static method: stageInputOutputDescriptor
_MTL_INLINE MTL::StageInputOutputDescriptor* MTL::StageInputOutputDescriptor::stageInputOutputDescriptor()
{
    return Object::sendMessage<MTL::StageInputOutputDescriptor*>(_MTL_PRIVATE_CLS(MTLStageInputOutputDescriptor), _MTL_PRIVATE_SEL(stageInputOutputDescriptor));
}

// property: layouts
_MTL_INLINE MTL::BufferLayoutDescriptorArray* MTL::StageInputOutputDescriptor::layouts() const
{
    return Object::sendMessage<MTL::BufferLayoutDescriptorArray*>(this, _MTL_PRIVATE_SEL(layouts));
}

// property: attributes
_MTL_INLINE MTL::AttributeDescriptorArray* MTL::StageInputOutputDescriptor::attributes() const
{
    return Object::sendMessage<MTL::AttributeDescriptorArray*>(this, _MTL_PRIVATE_SEL(attributes));
}

// property: indexType
_MTL_INLINE MTL::IndexType MTL::StageInputOutputDescriptor::indexType() const
{
    return Object::sendMessage<MTL::IndexType>(this, _MTL_PRIVATE_SEL(indexType));
}

_MTL_INLINE void MTL::StageInputOutputDescriptor::setIndexType(MTL::IndexType indexType)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setIndexType_), indexType);
}

// property: indexBufferIndex
_MTL_INLINE NS::UInteger MTL::StageInputOutputDescriptor::indexBufferIndex() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(indexBufferIndex));
}

_MTL_INLINE void MTL::StageInputOutputDescriptor::setIndexBufferIndex(NS::UInteger indexBufferIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setIndexBufferIndex_), indexBufferIndex);
}

// method: reset
_MTL_INLINE void MTL::StageInputOutputDescriptor::reset()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(reset));
}
