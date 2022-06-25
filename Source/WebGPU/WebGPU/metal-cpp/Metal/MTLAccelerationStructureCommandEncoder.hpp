//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLAccelerationStructureCommandEncoder.hpp
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
#include "MTLCommandEncoder.hpp"
#include "MTLHeap.hpp"
#include "MTLResource.hpp"

namespace MTL
{
class AccelerationStructureCommandEncoder : public NS::Referencing<AccelerationStructureCommandEncoder, CommandEncoder>
{
public:
    void buildAccelerationStructure(const class AccelerationStructure* accelerationStructure, const class AccelerationStructureDescriptor* descriptor, const class Buffer* scratchBuffer, NS::UInteger scratchBufferOffset);

    void refitAccelerationStructure(const class AccelerationStructure* sourceAccelerationStructure, const class AccelerationStructureDescriptor* descriptor, const class AccelerationStructure* destinationAccelerationStructure, const class Buffer* scratchBuffer, NS::UInteger scratchBufferOffset);

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
