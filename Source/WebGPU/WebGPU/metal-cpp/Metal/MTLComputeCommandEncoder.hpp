//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLComputeCommandEncoder.hpp
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

#include "MTLBuffer.hpp"
#include "MTLCommandBuffer.hpp"
#include "MTLCommandEncoder.hpp"
#include "MTLHeap.hpp"
#include "MTLIntersectionFunctionTable.hpp"
#include "MTLResource.hpp"
#include "MTLSampler.hpp"
#include "MTLTexture.hpp"
#include "MTLTypes.hpp"
#include "MTLVisibleFunctionTable.hpp"

namespace MTL
{
struct DispatchThreadgroupsIndirectArguments
{
    uint32_t threadgroupsPerGrid[3];
} _MTL_PACKED;

struct StageInRegionIndirectArguments
{
    uint32_t stageInOrigin[3];
    uint32_t stageInSize[3];
} _MTL_PACKED;

class ComputeCommandEncoder : public NS::Referencing<ComputeCommandEncoder, CommandEncoder>
{
public:
    MTL::DispatchType dispatchType() const;

    void              setComputePipelineState(const class ComputePipelineState* state);

    void              setBytes(const void* bytes, NS::UInteger length, NS::UInteger index);

    void              setBuffer(const class Buffer* buffer, NS::UInteger offset, NS::UInteger index);

    void              setBufferOffset(NS::UInteger offset, NS::UInteger index);

    void              setBuffers(MTL::Buffer* buffers[], const NS::UInteger offsets[], NS::Range range);

    void              setVisibleFunctionTable(const class VisibleFunctionTable* visibleFunctionTable, NS::UInteger bufferIndex);

    void              setVisibleFunctionTables(const class VisibleFunctionTable* visibleFunctionTables[], NS::Range range);

    void              setIntersectionFunctionTable(const class IntersectionFunctionTable* intersectionFunctionTable, NS::UInteger bufferIndex);

    void              setIntersectionFunctionTables(const class IntersectionFunctionTable* intersectionFunctionTables[], NS::Range range);

    void              setAccelerationStructure(const class AccelerationStructure* accelerationStructure, NS::UInteger bufferIndex);

    void              setTexture(const class Texture* texture, NS::UInteger index);

    void              setTextures(MTL::Texture* textures[], NS::Range range);

    void              setSamplerState(const class SamplerState* sampler, NS::UInteger index);

    void              setSamplerStates(MTL::SamplerState* samplers[], NS::Range range);

    void              setSamplerState(const class SamplerState* sampler, float lodMinClamp, float lodMaxClamp, NS::UInteger index);

    void              setSamplerStates(MTL::SamplerState* samplers[], const float lodMinClamps[], const float lodMaxClamps[], NS::Range range);

    void              setThreadgroupMemoryLength(NS::UInteger length, NS::UInteger index);

    void              setImageblockWidth(NS::UInteger width, NS::UInteger height);

    void              setStageInRegion(MTL::Region region);

    void              setStageInRegion(const class Buffer* indirectBuffer, NS::UInteger indirectBufferOffset);

    void              dispatchThreadgroups(MTL::Size threadgroupsPerGrid, MTL::Size threadsPerThreadgroup);

    void              dispatchThreadgroups(const class Buffer* indirectBuffer, NS::UInteger indirectBufferOffset, MTL::Size threadsPerThreadgroup);

    void              dispatchThreads(MTL::Size threadsPerGrid, MTL::Size threadsPerThreadgroup);

    void              updateFence(const class Fence* fence);

    void              waitForFence(const class Fence* fence);

    void              useResource(const class Resource* resource, MTL::ResourceUsage usage);

    void              useResources(MTL::Resource* resources[], NS::UInteger count, MTL::ResourceUsage usage);

    void              useHeap(const class Heap* heap);

    void              useHeaps(MTL::Heap* heaps[], NS::UInteger count);

    void              executeCommandsInBuffer(const class IndirectCommandBuffer* indirectCommandBuffer, NS::Range executionRange);

    void              executeCommandsInBuffer(const class IndirectCommandBuffer* indirectCommandbuffer, const class Buffer* indirectRangeBuffer, NS::UInteger indirectBufferOffset);

    void              memoryBarrier(MTL::BarrierScope scope);

    void              memoryBarrier(MTL::Resource* resources[], NS::UInteger count);

    void              sampleCountersInBuffer(const class CounterSampleBuffer* sampleBuffer, NS::UInteger sampleIndex, bool barrier);
};

}

// property: dispatchType
_MTL_INLINE MTL::DispatchType MTL::ComputeCommandEncoder::dispatchType() const
{
    return Object::sendMessage<MTL::DispatchType>(this, _MTL_PRIVATE_SEL(dispatchType));
}

// method: setComputePipelineState:
_MTL_INLINE void MTL::ComputeCommandEncoder::setComputePipelineState(const MTL::ComputePipelineState* state)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setComputePipelineState_), state);
}

// method: setBytes:length:atIndex:
_MTL_INLINE void MTL::ComputeCommandEncoder::setBytes(const void* bytes, NS::UInteger length, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setBytes_length_atIndex_), bytes, length, index);
}

// method: setBuffer:offset:atIndex:
_MTL_INLINE void MTL::ComputeCommandEncoder::setBuffer(const MTL::Buffer* buffer, NS::UInteger offset, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setBuffer_offset_atIndex_), buffer, offset, index);
}

// method: setBufferOffset:atIndex:
_MTL_INLINE void MTL::ComputeCommandEncoder::setBufferOffset(NS::UInteger offset, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setBufferOffset_atIndex_), offset, index);
}

// method: setBuffers:offsets:withRange:
_MTL_INLINE void MTL::ComputeCommandEncoder::setBuffers(MTL::Buffer* buffers[], const NS::UInteger offsets[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setBuffers_offsets_withRange_), buffers, offsets, range);
}

// method: setVisibleFunctionTable:atBufferIndex:
_MTL_INLINE void MTL::ComputeCommandEncoder::setVisibleFunctionTable(const MTL::VisibleFunctionTable* visibleFunctionTable, NS::UInteger bufferIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVisibleFunctionTable_atBufferIndex_), visibleFunctionTable, bufferIndex);
}

// method: setVisibleFunctionTables:withBufferRange:
_MTL_INLINE void MTL::ComputeCommandEncoder::setVisibleFunctionTables(const MTL::VisibleFunctionTable* visibleFunctionTables[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVisibleFunctionTables_withBufferRange_), visibleFunctionTables, range);
}

// method: setIntersectionFunctionTable:atBufferIndex:
_MTL_INLINE void MTL::ComputeCommandEncoder::setIntersectionFunctionTable(const MTL::IntersectionFunctionTable* intersectionFunctionTable, NS::UInteger bufferIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setIntersectionFunctionTable_atBufferIndex_), intersectionFunctionTable, bufferIndex);
}

// method: setIntersectionFunctionTables:withBufferRange:
_MTL_INLINE void MTL::ComputeCommandEncoder::setIntersectionFunctionTables(const MTL::IntersectionFunctionTable* intersectionFunctionTables[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setIntersectionFunctionTables_withBufferRange_), intersectionFunctionTables, range);
}

// method: setAccelerationStructure:atBufferIndex:
_MTL_INLINE void MTL::ComputeCommandEncoder::setAccelerationStructure(const MTL::AccelerationStructure* accelerationStructure, NS::UInteger bufferIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setAccelerationStructure_atBufferIndex_), accelerationStructure, bufferIndex);
}

// method: setTexture:atIndex:
_MTL_INLINE void MTL::ComputeCommandEncoder::setTexture(const MTL::Texture* texture, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTexture_atIndex_), texture, index);
}

// method: setTextures:withRange:
_MTL_INLINE void MTL::ComputeCommandEncoder::setTextures(MTL::Texture* textures[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTextures_withRange_), textures, range);
}

// method: setSamplerState:atIndex:
_MTL_INLINE void MTL::ComputeCommandEncoder::setSamplerState(const MTL::SamplerState* sampler, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSamplerState_atIndex_), sampler, index);
}

// method: setSamplerStates:withRange:
_MTL_INLINE void MTL::ComputeCommandEncoder::setSamplerStates(MTL::SamplerState* samplers[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSamplerStates_withRange_), samplers, range);
}

// method: setSamplerState:lodMinClamp:lodMaxClamp:atIndex:
_MTL_INLINE void MTL::ComputeCommandEncoder::setSamplerState(const MTL::SamplerState* sampler, float lodMinClamp, float lodMaxClamp, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSamplerState_lodMinClamp_lodMaxClamp_atIndex_), sampler, lodMinClamp, lodMaxClamp, index);
}

// method: setSamplerStates:lodMinClamps:lodMaxClamps:withRange:
_MTL_INLINE void MTL::ComputeCommandEncoder::setSamplerStates(MTL::SamplerState* samplers[], const float lodMinClamps[], const float lodMaxClamps[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSamplerStates_lodMinClamps_lodMaxClamps_withRange_), samplers, lodMinClamps, lodMaxClamps, range);
}

// method: setThreadgroupMemoryLength:atIndex:
_MTL_INLINE void MTL::ComputeCommandEncoder::setThreadgroupMemoryLength(NS::UInteger length, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setThreadgroupMemoryLength_atIndex_), length, index);
}

// method: setImageblockWidth:height:
_MTL_INLINE void MTL::ComputeCommandEncoder::setImageblockWidth(NS::UInteger width, NS::UInteger height)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setImageblockWidth_height_), width, height);
}

// method: setStageInRegion:
_MTL_INLINE void MTL::ComputeCommandEncoder::setStageInRegion(MTL::Region region)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStageInRegion_), region);
}

// method: setStageInRegionWithIndirectBuffer:indirectBufferOffset:
_MTL_INLINE void MTL::ComputeCommandEncoder::setStageInRegion(const MTL::Buffer* indirectBuffer, NS::UInteger indirectBufferOffset)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStageInRegionWithIndirectBuffer_indirectBufferOffset_), indirectBuffer, indirectBufferOffset);
}

// method: dispatchThreadgroups:threadsPerThreadgroup:
_MTL_INLINE void MTL::ComputeCommandEncoder::dispatchThreadgroups(MTL::Size threadgroupsPerGrid, MTL::Size threadsPerThreadgroup)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(dispatchThreadgroups_threadsPerThreadgroup_), threadgroupsPerGrid, threadsPerThreadgroup);
}

// method: dispatchThreadgroupsWithIndirectBuffer:indirectBufferOffset:threadsPerThreadgroup:
_MTL_INLINE void MTL::ComputeCommandEncoder::dispatchThreadgroups(const MTL::Buffer* indirectBuffer, NS::UInteger indirectBufferOffset, MTL::Size threadsPerThreadgroup)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(dispatchThreadgroupsWithIndirectBuffer_indirectBufferOffset_threadsPerThreadgroup_), indirectBuffer, indirectBufferOffset, threadsPerThreadgroup);
}

// method: dispatchThreads:threadsPerThreadgroup:
_MTL_INLINE void MTL::ComputeCommandEncoder::dispatchThreads(MTL::Size threadsPerGrid, MTL::Size threadsPerThreadgroup)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(dispatchThreads_threadsPerThreadgroup_), threadsPerGrid, threadsPerThreadgroup);
}

// method: updateFence:
_MTL_INLINE void MTL::ComputeCommandEncoder::updateFence(const MTL::Fence* fence)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(updateFence_), fence);
}

// method: waitForFence:
_MTL_INLINE void MTL::ComputeCommandEncoder::waitForFence(const MTL::Fence* fence)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(waitForFence_), fence);
}

// method: useResource:usage:
_MTL_INLINE void MTL::ComputeCommandEncoder::useResource(const MTL::Resource* resource, MTL::ResourceUsage usage)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(useResource_usage_), resource, usage);
}

// method: useResources:count:usage:
_MTL_INLINE void MTL::ComputeCommandEncoder::useResources(MTL::Resource* resources[], NS::UInteger count, MTL::ResourceUsage usage)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(useResources_count_usage_), resources, count, usage);
}

// method: useHeap:
_MTL_INLINE void MTL::ComputeCommandEncoder::useHeap(const MTL::Heap* heap)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(useHeap_), heap);
}

// method: useHeaps:count:
_MTL_INLINE void MTL::ComputeCommandEncoder::useHeaps(MTL::Heap* heaps[], NS::UInteger count)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(useHeaps_count_), heaps, count);
}

// method: executeCommandsInBuffer:withRange:
_MTL_INLINE void MTL::ComputeCommandEncoder::executeCommandsInBuffer(const MTL::IndirectCommandBuffer* indirectCommandBuffer, NS::Range executionRange)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(executeCommandsInBuffer_withRange_), indirectCommandBuffer, executionRange);
}

// method: executeCommandsInBuffer:indirectBuffer:indirectBufferOffset:
_MTL_INLINE void MTL::ComputeCommandEncoder::executeCommandsInBuffer(const MTL::IndirectCommandBuffer* indirectCommandbuffer, const MTL::Buffer* indirectRangeBuffer, NS::UInteger indirectBufferOffset)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(executeCommandsInBuffer_indirectBuffer_indirectBufferOffset_), indirectCommandbuffer, indirectRangeBuffer, indirectBufferOffset);
}

// method: memoryBarrierWithScope:
_MTL_INLINE void MTL::ComputeCommandEncoder::memoryBarrier(MTL::BarrierScope scope)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(memoryBarrierWithScope_), scope);
}

// method: memoryBarrierWithResources:count:
_MTL_INLINE void MTL::ComputeCommandEncoder::memoryBarrier(MTL::Resource* resources[], NS::UInteger count)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(memoryBarrierWithResources_count_), resources, count);
}

// method: sampleCountersInBuffer:atSampleIndex:withBarrier:
_MTL_INLINE void MTL::ComputeCommandEncoder::sampleCountersInBuffer(const MTL::CounterSampleBuffer* sampleBuffer, NS::UInteger sampleIndex, bool barrier)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(sampleCountersInBuffer_atSampleIndex_withBarrier_), sampleBuffer, sampleIndex, barrier);
}
