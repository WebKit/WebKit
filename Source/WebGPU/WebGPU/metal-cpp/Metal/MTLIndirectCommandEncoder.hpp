//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLIndirectCommandEncoder.hpp
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

#include "MTLRenderCommandEncoder.hpp"
#include "MTLStageInputOutputDescriptor.hpp"
#include "MTLTypes.hpp"

namespace MTL
{
class IndirectRenderCommand : public NS::Referencing<IndirectRenderCommand>
{
public:
    void setRenderPipelineState(const class RenderPipelineState* pipelineState);

    void setVertexBuffer(const class Buffer* buffer, NS::UInteger offset, NS::UInteger index);

    void setFragmentBuffer(const class Buffer* buffer, NS::UInteger offset, NS::UInteger index);

    void drawPatches(NS::UInteger numberOfPatchControlPoints, NS::UInteger patchStart, NS::UInteger patchCount, const class Buffer* patchIndexBuffer, NS::UInteger patchIndexBufferOffset, NS::UInteger instanceCount, NS::UInteger baseInstance, const class Buffer* buffer, NS::UInteger offset, NS::UInteger instanceStride);

    void drawIndexedPatches(NS::UInteger numberOfPatchControlPoints, NS::UInteger patchStart, NS::UInteger patchCount, const class Buffer* patchIndexBuffer, NS::UInteger patchIndexBufferOffset, const class Buffer* controlPointIndexBuffer, NS::UInteger controlPointIndexBufferOffset, NS::UInteger instanceCount, NS::UInteger baseInstance, const class Buffer* buffer, NS::UInteger offset, NS::UInteger instanceStride);

    void drawPrimitives(MTL::PrimitiveType primitiveType, NS::UInteger vertexStart, NS::UInteger vertexCount, NS::UInteger instanceCount, NS::UInteger baseInstance);

    void drawIndexedPrimitives(MTL::PrimitiveType primitiveType, NS::UInteger indexCount, MTL::IndexType indexType, const class Buffer* indexBuffer, NS::UInteger indexBufferOffset, NS::UInteger instanceCount, NS::Integer baseVertex, NS::UInteger baseInstance);

    void reset();
};

class IndirectComputeCommand : public NS::Referencing<IndirectComputeCommand>
{
public:
    void setComputePipelineState(const class ComputePipelineState* pipelineState);

    void setKernelBuffer(const class Buffer* buffer, NS::UInteger offset, NS::UInteger index);

    void concurrentDispatchThreadgroups(MTL::Size threadgroupsPerGrid, MTL::Size threadsPerThreadgroup);

    void concurrentDispatchThreads(MTL::Size threadsPerGrid, MTL::Size threadsPerThreadgroup);

    void setBarrier();

    void clearBarrier();

    void setImageblockWidth(NS::UInteger width, NS::UInteger height);

    void reset();

    void setThreadgroupMemoryLength(NS::UInteger length, NS::UInteger index);

    void setStageInRegion(MTL::Region region);
};

}

// method: setRenderPipelineState:
_MTL_INLINE void MTL::IndirectRenderCommand::setRenderPipelineState(const MTL::RenderPipelineState* pipelineState)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setRenderPipelineState_), pipelineState);
}

// method: setVertexBuffer:offset:atIndex:
_MTL_INLINE void MTL::IndirectRenderCommand::setVertexBuffer(const MTL::Buffer* buffer, NS::UInteger offset, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexBuffer_offset_atIndex_), buffer, offset, index);
}

// method: setFragmentBuffer:offset:atIndex:
_MTL_INLINE void MTL::IndirectRenderCommand::setFragmentBuffer(const MTL::Buffer* buffer, NS::UInteger offset, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFragmentBuffer_offset_atIndex_), buffer, offset, index);
}

// method: drawPatches:patchStart:patchCount:patchIndexBuffer:patchIndexBufferOffset:instanceCount:baseInstance:tessellationFactorBuffer:tessellationFactorBufferOffset:tessellationFactorBufferInstanceStride:
_MTL_INLINE void MTL::IndirectRenderCommand::drawPatches(NS::UInteger numberOfPatchControlPoints, NS::UInteger patchStart, NS::UInteger patchCount, const MTL::Buffer* patchIndexBuffer, NS::UInteger patchIndexBufferOffset, NS::UInteger instanceCount, NS::UInteger baseInstance, const MTL::Buffer* buffer, NS::UInteger offset, NS::UInteger instanceStride)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(drawPatches_patchStart_patchCount_patchIndexBuffer_patchIndexBufferOffset_instanceCount_baseInstance_tessellationFactorBuffer_tessellationFactorBufferOffset_tessellationFactorBufferInstanceStride_), numberOfPatchControlPoints, patchStart, patchCount, patchIndexBuffer, patchIndexBufferOffset, instanceCount, baseInstance, buffer, offset, instanceStride);
}

// method: drawIndexedPatches:patchStart:patchCount:patchIndexBuffer:patchIndexBufferOffset:controlPointIndexBuffer:controlPointIndexBufferOffset:instanceCount:baseInstance:tessellationFactorBuffer:tessellationFactorBufferOffset:tessellationFactorBufferInstanceStride:
_MTL_INLINE void MTL::IndirectRenderCommand::drawIndexedPatches(NS::UInteger numberOfPatchControlPoints, NS::UInteger patchStart, NS::UInteger patchCount, const MTL::Buffer* patchIndexBuffer, NS::UInteger patchIndexBufferOffset, const MTL::Buffer* controlPointIndexBuffer, NS::UInteger controlPointIndexBufferOffset, NS::UInteger instanceCount, NS::UInteger baseInstance, const MTL::Buffer* buffer, NS::UInteger offset, NS::UInteger instanceStride)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(drawIndexedPatches_patchStart_patchCount_patchIndexBuffer_patchIndexBufferOffset_controlPointIndexBuffer_controlPointIndexBufferOffset_instanceCount_baseInstance_tessellationFactorBuffer_tessellationFactorBufferOffset_tessellationFactorBufferInstanceStride_), numberOfPatchControlPoints, patchStart, patchCount, patchIndexBuffer, patchIndexBufferOffset, controlPointIndexBuffer, controlPointIndexBufferOffset, instanceCount, baseInstance, buffer, offset, instanceStride);
}

// method: drawPrimitives:vertexStart:vertexCount:instanceCount:baseInstance:
_MTL_INLINE void MTL::IndirectRenderCommand::drawPrimitives(MTL::PrimitiveType primitiveType, NS::UInteger vertexStart, NS::UInteger vertexCount, NS::UInteger instanceCount, NS::UInteger baseInstance)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(drawPrimitives_vertexStart_vertexCount_instanceCount_baseInstance_), primitiveType, vertexStart, vertexCount, instanceCount, baseInstance);
}

// method: drawIndexedPrimitives:indexCount:indexType:indexBuffer:indexBufferOffset:instanceCount:baseVertex:baseInstance:
_MTL_INLINE void MTL::IndirectRenderCommand::drawIndexedPrimitives(MTL::PrimitiveType primitiveType, NS::UInteger indexCount, MTL::IndexType indexType, const MTL::Buffer* indexBuffer, NS::UInteger indexBufferOffset, NS::UInteger instanceCount, NS::Integer baseVertex, NS::UInteger baseInstance)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(drawIndexedPrimitives_indexCount_indexType_indexBuffer_indexBufferOffset_instanceCount_baseVertex_baseInstance_), primitiveType, indexCount, indexType, indexBuffer, indexBufferOffset, instanceCount, baseVertex, baseInstance);
}

// method: reset
_MTL_INLINE void MTL::IndirectRenderCommand::reset()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(reset));
}

// method: setComputePipelineState:
_MTL_INLINE void MTL::IndirectComputeCommand::setComputePipelineState(const MTL::ComputePipelineState* pipelineState)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setComputePipelineState_), pipelineState);
}

// method: setKernelBuffer:offset:atIndex:
_MTL_INLINE void MTL::IndirectComputeCommand::setKernelBuffer(const MTL::Buffer* buffer, NS::UInteger offset, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setKernelBuffer_offset_atIndex_), buffer, offset, index);
}

// method: concurrentDispatchThreadgroups:threadsPerThreadgroup:
_MTL_INLINE void MTL::IndirectComputeCommand::concurrentDispatchThreadgroups(MTL::Size threadgroupsPerGrid, MTL::Size threadsPerThreadgroup)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(concurrentDispatchThreadgroups_threadsPerThreadgroup_), threadgroupsPerGrid, threadsPerThreadgroup);
}

// method: concurrentDispatchThreads:threadsPerThreadgroup:
_MTL_INLINE void MTL::IndirectComputeCommand::concurrentDispatchThreads(MTL::Size threadsPerGrid, MTL::Size threadsPerThreadgroup)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(concurrentDispatchThreads_threadsPerThreadgroup_), threadsPerGrid, threadsPerThreadgroup);
}

// method: setBarrier
_MTL_INLINE void MTL::IndirectComputeCommand::setBarrier()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setBarrier));
}

// method: clearBarrier
_MTL_INLINE void MTL::IndirectComputeCommand::clearBarrier()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(clearBarrier));
}

// method: setImageblockWidth:height:
_MTL_INLINE void MTL::IndirectComputeCommand::setImageblockWidth(NS::UInteger width, NS::UInteger height)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setImageblockWidth_height_), width, height);
}

// method: reset
_MTL_INLINE void MTL::IndirectComputeCommand::reset()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(reset));
}

// method: setThreadgroupMemoryLength:atIndex:
_MTL_INLINE void MTL::IndirectComputeCommand::setThreadgroupMemoryLength(NS::UInteger length, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setThreadgroupMemoryLength_atIndex_), length, index);
}

// method: setStageInRegion:
_MTL_INLINE void MTL::IndirectComputeCommand::setStageInRegion(MTL::Region region)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStageInRegion_), region);
}
