//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLAccelerationStructureCommandEncoder.hpp
//
// Copyright 2020-2022 Apple Inc.
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

#include "MTLAccelerationStructureCommandEncoder.hpp"
#include "MTLArgument.hpp"
#include "MTLCommandEncoder.hpp"
#include "MTLHeap.hpp"
#include "MTLResource.hpp"

namespace MTL
{
_MTL_OPTIONS(NS::UInteger, AccelerationStructureRefitOptions) {
    AccelerationStructureRefitOptionVertexData = 1,
    AccelerationStructureRefitOptionPerPrimitiveData = 2,
};

class AccelerationStructureCommandEncoder : public NS::Referencing<AccelerationStructureCommandEncoder, CommandEncoder>
{
public:
    void buildAccelerationStructure(const class AccelerationStructure* accelerationStructure, const class AccelerationStructureDescriptor* descriptor, const class Buffer* scratchBuffer, NS::UInteger scratchBufferOffset);

    void refitAccelerationStructure(const class AccelerationStructure* sourceAccelerationStructure, const class AccelerationStructureDescriptor* descriptor, const class AccelerationStructure* destinationAccelerationStructure, const class Buffer* scratchBuffer, NS::UInteger scratchBufferOffset);

    void refitAccelerationStructure(const class AccelerationStructure* sourceAccelerationStructure, const class AccelerationStructureDescriptor* descriptor, const class AccelerationStructure* destinationAccelerationStructure, const class Buffer* scratchBuffer, NS::UInteger scratchBufferOffset, MTL::AccelerationStructureRefitOptions options);

    void copyAccelerationStructure(const class AccelerationStructure* sourceAccelerationStructure, const class AccelerationStructure* destinationAccelerationStructure);

    void writeCompactedAccelerationStructureSize(const class AccelerationStructure* accelerationStructure, const class Buffer* buffer, NS::UInteger offset);

    void writeCompactedAccelerationStructureSize(const class AccelerationStructure* accelerationStructure, const class Buffer* buffer, NS::UInteger offset, MTL::DataType sizeDataType);

    void copyAndCompactAccelerationStructure(const class AccelerationStructure* sourceAccelerationStructure, const class AccelerationStructure* destinationAccelerationStructure);

    void updateFence(const class Fence* fence);

    void waitForFence(const class Fence* fence);

    void useResource(const class Resource* resource, MTL::ResourceUsage usage);

    void useResources(MTL::Resource* resources[], NS::UInteger count, MTL::ResourceUsage usage);

    void useHeap(const class Heap* heap);

    void useHeaps(MTL::Heap* heaps[], NS::UInteger count);

    void sampleCountersInBuffer(const class CounterSampleBuffer* sampleBuffer, NS::UInteger sampleIndex, bool barrier);
};

class AccelerationStructurePassSampleBufferAttachmentDescriptor : public NS::Copying<AccelerationStructurePassSampleBufferAttachmentDescriptor>
{
public:
    static class AccelerationStructurePassSampleBufferAttachmentDescriptor* alloc();

    class AccelerationStructurePassSampleBufferAttachmentDescriptor*        init();

    class CounterSampleBuffer*                                              sampleBuffer() const;
    void                                                                    setSampleBuffer(const class CounterSampleBuffer* sampleBuffer);

    NS::UInteger                                                            startOfEncoderSampleIndex() const;
    void                                                                    setStartOfEncoderSampleIndex(NS::UInteger startOfEncoderSampleIndex);

    NS::UInteger                                                            endOfEncoderSampleIndex() const;
    void                                                                    setEndOfEncoderSampleIndex(NS::UInteger endOfEncoderSampleIndex);
};

class AccelerationStructurePassSampleBufferAttachmentDescriptorArray : public NS::Referencing<AccelerationStructurePassSampleBufferAttachmentDescriptorArray>
{
public:
    static class AccelerationStructurePassSampleBufferAttachmentDescriptorArray* alloc();

    class AccelerationStructurePassSampleBufferAttachmentDescriptorArray*        init();

    class AccelerationStructurePassSampleBufferAttachmentDescriptor*             object(NS::UInteger attachmentIndex);

    void                                                                         setObject(const class AccelerationStructurePassSampleBufferAttachmentDescriptor* attachment, NS::UInteger attachmentIndex);
};

class AccelerationStructurePassDescriptor : public NS::Copying<AccelerationStructurePassDescriptor>
{
public:
    static class AccelerationStructurePassDescriptor*                     alloc();

    class AccelerationStructurePassDescriptor*                            init();

    static class AccelerationStructurePassDescriptor*                     accelerationStructurePassDescriptor();

    class AccelerationStructurePassSampleBufferAttachmentDescriptorArray* sampleBufferAttachments() const;
};

}

// method: buildAccelerationStructure:descriptor:scratchBuffer:scratchBufferOffset:
_MTL_INLINE void MTL::AccelerationStructureCommandEncoder::buildAccelerationStructure(const MTL::AccelerationStructure* accelerationStructure, const MTL::AccelerationStructureDescriptor* descriptor, const MTL::Buffer* scratchBuffer, NS::UInteger scratchBufferOffset)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(buildAccelerationStructure_descriptor_scratchBuffer_scratchBufferOffset_), accelerationStructure, descriptor, scratchBuffer, scratchBufferOffset);
}

// method: refitAccelerationStructure:descriptor:destination:scratchBuffer:scratchBufferOffset:
_MTL_INLINE void MTL::AccelerationStructureCommandEncoder::refitAccelerationStructure(const MTL::AccelerationStructure* sourceAccelerationStructure, const MTL::AccelerationStructureDescriptor* descriptor, const MTL::AccelerationStructure* destinationAccelerationStructure, const MTL::Buffer* scratchBuffer, NS::UInteger scratchBufferOffset)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(refitAccelerationStructure_descriptor_destination_scratchBuffer_scratchBufferOffset_), sourceAccelerationStructure, descriptor, destinationAccelerationStructure, scratchBuffer, scratchBufferOffset);
}

// method: refitAccelerationStructure:descriptor:destination:scratchBuffer:scratchBufferOffset:options:
_MTL_INLINE void MTL::AccelerationStructureCommandEncoder::refitAccelerationStructure(const MTL::AccelerationStructure* sourceAccelerationStructure, const MTL::AccelerationStructureDescriptor* descriptor, const MTL::AccelerationStructure* destinationAccelerationStructure, const MTL::Buffer* scratchBuffer, NS::UInteger scratchBufferOffset, MTL::AccelerationStructureRefitOptions options)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(refitAccelerationStructure_descriptor_destination_scratchBuffer_scratchBufferOffset_options_), sourceAccelerationStructure, descriptor, destinationAccelerationStructure, scratchBuffer, scratchBufferOffset, options);
}

// method: copyAccelerationStructure:toAccelerationStructure:
_MTL_INLINE void MTL::AccelerationStructureCommandEncoder::copyAccelerationStructure(const MTL::AccelerationStructure* sourceAccelerationStructure, const MTL::AccelerationStructure* destinationAccelerationStructure)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(copyAccelerationStructure_toAccelerationStructure_), sourceAccelerationStructure, destinationAccelerationStructure);
}

// method: writeCompactedAccelerationStructureSize:toBuffer:offset:
_MTL_INLINE void MTL::AccelerationStructureCommandEncoder::writeCompactedAccelerationStructureSize(const MTL::AccelerationStructure* accelerationStructure, const MTL::Buffer* buffer, NS::UInteger offset)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(writeCompactedAccelerationStructureSize_toBuffer_offset_), accelerationStructure, buffer, offset);
}

// method: writeCompactedAccelerationStructureSize:toBuffer:offset:sizeDataType:
_MTL_INLINE void MTL::AccelerationStructureCommandEncoder::writeCompactedAccelerationStructureSize(const MTL::AccelerationStructure* accelerationStructure, const MTL::Buffer* buffer, NS::UInteger offset, MTL::DataType sizeDataType)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(writeCompactedAccelerationStructureSize_toBuffer_offset_sizeDataType_), accelerationStructure, buffer, offset, sizeDataType);
}

// method: copyAndCompactAccelerationStructure:toAccelerationStructure:
_MTL_INLINE void MTL::AccelerationStructureCommandEncoder::copyAndCompactAccelerationStructure(const MTL::AccelerationStructure* sourceAccelerationStructure, const MTL::AccelerationStructure* destinationAccelerationStructure)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(copyAndCompactAccelerationStructure_toAccelerationStructure_), sourceAccelerationStructure, destinationAccelerationStructure);
}

// method: updateFence:
_MTL_INLINE void MTL::AccelerationStructureCommandEncoder::updateFence(const MTL::Fence* fence)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(updateFence_), fence);
}

// method: waitForFence:
_MTL_INLINE void MTL::AccelerationStructureCommandEncoder::waitForFence(const MTL::Fence* fence)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(waitForFence_), fence);
}

// method: useResource:usage:
_MTL_INLINE void MTL::AccelerationStructureCommandEncoder::useResource(const MTL::Resource* resource, MTL::ResourceUsage usage)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(useResource_usage_), resource, usage);
}

// method: useResources:count:usage:
_MTL_INLINE void MTL::AccelerationStructureCommandEncoder::useResources(MTL::Resource* resources[], NS::UInteger count, MTL::ResourceUsage usage)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(useResources_count_usage_), resources, count, usage);
}

// method: useHeap:
_MTL_INLINE void MTL::AccelerationStructureCommandEncoder::useHeap(const MTL::Heap* heap)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(useHeap_), heap);
}

// method: useHeaps:count:
_MTL_INLINE void MTL::AccelerationStructureCommandEncoder::useHeaps(MTL::Heap* heaps[], NS::UInteger count)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(useHeaps_count_), heaps, count);
}

// method: sampleCountersInBuffer:atSampleIndex:withBarrier:
_MTL_INLINE void MTL::AccelerationStructureCommandEncoder::sampleCountersInBuffer(const MTL::CounterSampleBuffer* sampleBuffer, NS::UInteger sampleIndex, bool barrier)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(sampleCountersInBuffer_atSampleIndex_withBarrier_), sampleBuffer, sampleIndex, barrier);
}

// static method: alloc
_MTL_INLINE MTL::AccelerationStructurePassSampleBufferAttachmentDescriptor* MTL::AccelerationStructurePassSampleBufferAttachmentDescriptor::alloc()
{
    return NS::Object::alloc<MTL::AccelerationStructurePassSampleBufferAttachmentDescriptor>(_MTL_PRIVATE_CLS(MTLAccelerationStructurePassSampleBufferAttachmentDescriptor));
}

// method: init
_MTL_INLINE MTL::AccelerationStructurePassSampleBufferAttachmentDescriptor* MTL::AccelerationStructurePassSampleBufferAttachmentDescriptor::init()
{
    return NS::Object::init<MTL::AccelerationStructurePassSampleBufferAttachmentDescriptor>();
}

// property: sampleBuffer
_MTL_INLINE MTL::CounterSampleBuffer* MTL::AccelerationStructurePassSampleBufferAttachmentDescriptor::sampleBuffer() const
{
    return Object::sendMessage<MTL::CounterSampleBuffer*>(this, _MTL_PRIVATE_SEL(sampleBuffer));
}

_MTL_INLINE void MTL::AccelerationStructurePassSampleBufferAttachmentDescriptor::setSampleBuffer(const MTL::CounterSampleBuffer* sampleBuffer)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSampleBuffer_), sampleBuffer);
}

// property: startOfEncoderSampleIndex
_MTL_INLINE NS::UInteger MTL::AccelerationStructurePassSampleBufferAttachmentDescriptor::startOfEncoderSampleIndex() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(startOfEncoderSampleIndex));
}

_MTL_INLINE void MTL::AccelerationStructurePassSampleBufferAttachmentDescriptor::setStartOfEncoderSampleIndex(NS::UInteger startOfEncoderSampleIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStartOfEncoderSampleIndex_), startOfEncoderSampleIndex);
}

// property: endOfEncoderSampleIndex
_MTL_INLINE NS::UInteger MTL::AccelerationStructurePassSampleBufferAttachmentDescriptor::endOfEncoderSampleIndex() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(endOfEncoderSampleIndex));
}

_MTL_INLINE void MTL::AccelerationStructurePassSampleBufferAttachmentDescriptor::setEndOfEncoderSampleIndex(NS::UInteger endOfEncoderSampleIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setEndOfEncoderSampleIndex_), endOfEncoderSampleIndex);
}

// static method: alloc
_MTL_INLINE MTL::AccelerationStructurePassSampleBufferAttachmentDescriptorArray* MTL::AccelerationStructurePassSampleBufferAttachmentDescriptorArray::alloc()
{
    return NS::Object::alloc<MTL::AccelerationStructurePassSampleBufferAttachmentDescriptorArray>(_MTL_PRIVATE_CLS(MTLAccelerationStructurePassSampleBufferAttachmentDescriptorArray));
}

// method: init
_MTL_INLINE MTL::AccelerationStructurePassSampleBufferAttachmentDescriptorArray* MTL::AccelerationStructurePassSampleBufferAttachmentDescriptorArray::init()
{
    return NS::Object::init<MTL::AccelerationStructurePassSampleBufferAttachmentDescriptorArray>();
}

// method: objectAtIndexedSubscript:
_MTL_INLINE MTL::AccelerationStructurePassSampleBufferAttachmentDescriptor* MTL::AccelerationStructurePassSampleBufferAttachmentDescriptorArray::object(NS::UInteger attachmentIndex)
{
    return Object::sendMessage<MTL::AccelerationStructurePassSampleBufferAttachmentDescriptor*>(this, _MTL_PRIVATE_SEL(objectAtIndexedSubscript_), attachmentIndex);
}

// method: setObject:atIndexedSubscript:
_MTL_INLINE void MTL::AccelerationStructurePassSampleBufferAttachmentDescriptorArray::setObject(const MTL::AccelerationStructurePassSampleBufferAttachmentDescriptor* attachment, NS::UInteger attachmentIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setObject_atIndexedSubscript_), attachment, attachmentIndex);
}

// static method: alloc
_MTL_INLINE MTL::AccelerationStructurePassDescriptor* MTL::AccelerationStructurePassDescriptor::alloc()
{
    return NS::Object::alloc<MTL::AccelerationStructurePassDescriptor>(_MTL_PRIVATE_CLS(MTLAccelerationStructurePassDescriptor));
}

// method: init
_MTL_INLINE MTL::AccelerationStructurePassDescriptor* MTL::AccelerationStructurePassDescriptor::init()
{
    return NS::Object::init<MTL::AccelerationStructurePassDescriptor>();
}

// static method: accelerationStructurePassDescriptor
_MTL_INLINE MTL::AccelerationStructurePassDescriptor* MTL::AccelerationStructurePassDescriptor::accelerationStructurePassDescriptor()
{
    return Object::sendMessage<MTL::AccelerationStructurePassDescriptor*>(_MTL_PRIVATE_CLS(MTLAccelerationStructurePassDescriptor), _MTL_PRIVATE_SEL(accelerationStructurePassDescriptor));
}

// property: sampleBufferAttachments
_MTL_INLINE MTL::AccelerationStructurePassSampleBufferAttachmentDescriptorArray* MTL::AccelerationStructurePassDescriptor::sampleBufferAttachments() const
{
    return Object::sendMessage<MTL::AccelerationStructurePassSampleBufferAttachmentDescriptorArray*>(this, _MTL_PRIVATE_SEL(sampleBufferAttachments));
}
