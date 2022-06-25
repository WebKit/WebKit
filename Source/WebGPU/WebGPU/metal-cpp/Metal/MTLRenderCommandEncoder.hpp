//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLRenderCommandEncoder.hpp
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
#include "MTLCommandEncoder.hpp"
#include "MTLHeap.hpp"
#include "MTLIntersectionFunctionTable.hpp"
#include "MTLRenderCommandEncoder.hpp"
#include "MTLRenderPass.hpp"
#include "MTLResource.hpp"
#include "MTLSampler.hpp"
#include "MTLStageInputOutputDescriptor.hpp"
#include "MTLTexture.hpp"
#include "MTLTypes.hpp"
#include "MTLVisibleFunctionTable.hpp"

namespace MTL
{
_MTL_ENUM(NS::UInteger, PrimitiveType) {
    PrimitiveTypePoint = 0,
    PrimitiveTypeLine = 1,
    PrimitiveTypeLineStrip = 2,
    PrimitiveTypeTriangle = 3,
    PrimitiveTypeTriangleStrip = 4,
};

_MTL_ENUM(NS::UInteger, VisibilityResultMode) {
    VisibilityResultModeDisabled = 0,
    VisibilityResultModeBoolean = 1,
    VisibilityResultModeCounting = 2,
};

struct ScissorRect
{
    NS::UInteger x;
    NS::UInteger y;
    NS::UInteger width;
    NS::UInteger height;
} _MTL_PACKED;

struct Viewport
{
    double originX;
    double originY;
    double width;
    double height;
    double znear;
    double zfar;
} _MTL_PACKED;

_MTL_ENUM(NS::UInteger, CullMode) {
    CullModeNone = 0,
    CullModeFront = 1,
    CullModeBack = 2,
};

_MTL_ENUM(NS::UInteger, Winding) {
    WindingClockwise = 0,
    WindingCounterClockwise = 1,
};

_MTL_ENUM(NS::UInteger, DepthClipMode) {
    DepthClipModeClip = 0,
    DepthClipModeClamp = 1,
};

_MTL_ENUM(NS::UInteger, TriangleFillMode) {
    TriangleFillModeFill = 0,
    TriangleFillModeLines = 1,
};

struct DrawPrimitivesIndirectArguments
{
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t vertexStart;
    uint32_t baseInstance;
} _MTL_PACKED;

struct DrawIndexedPrimitivesIndirectArguments
{
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t indexStart;
    int32_t  baseVertex;
    uint32_t baseInstance;
} _MTL_PACKED;

struct VertexAmplificationViewMapping
{
    uint32_t viewportArrayIndexOffset;
    uint32_t renderTargetArrayIndexOffset;
} _MTL_PACKED;

struct DrawPatchIndirectArguments
{
    uint32_t patchCount;
    uint32_t instanceCount;
    uint32_t patchStart;
    uint32_t baseInstance;
} _MTL_PACKED;

struct QuadTessellationFactorsHalf
{
    uint16_t edgeTessellationFactor[4];
    uint16_t insideTessellationFactor[2];
} _MTL_PACKED;

struct TriangleTessellationFactorsHalf
{
    uint16_t edgeTessellationFactor[3];
    uint16_t insideTessellationFactor;
} _MTL_PACKED;

_MTL_OPTIONS(NS::UInteger, RenderStages) {
    RenderStageVertex = 1,
    RenderStageFragment = 2,
    RenderStageTile = 4,
};

class RenderCommandEncoder : public NS::Referencing<RenderCommandEncoder, CommandEncoder>
{
public:
    void         setRenderPipelineState(const class RenderPipelineState* pipelineState);

    void         setVertexBytes(const void* bytes, NS::UInteger length, NS::UInteger index);

    void         setVertexBuffer(const class Buffer* buffer, NS::UInteger offset, NS::UInteger index);

    void         setVertexBufferOffset(NS::UInteger offset, NS::UInteger index);

    void         setVertexBuffers(MTL::Buffer* buffers[], const NS::UInteger offsets[], NS::Range range);

    void         setVertexTexture(const class Texture* texture, NS::UInteger index);

    void         setVertexTextures(MTL::Texture* textures[], NS::Range range);

    void         setVertexSamplerState(const class SamplerState* sampler, NS::UInteger index);

    void         setVertexSamplerStates(MTL::SamplerState* samplers[], NS::Range range);

    void         setVertexSamplerState(const class SamplerState* sampler, float lodMinClamp, float lodMaxClamp, NS::UInteger index);

    void         setVertexSamplerStates(MTL::SamplerState* samplers[], const float lodMinClamps[], const float lodMaxClamps[], NS::Range range);

    void         setVertexVisibleFunctionTable(const class VisibleFunctionTable* functionTable, NS::UInteger bufferIndex);

    void         setVertexVisibleFunctionTables(const class VisibleFunctionTable* functionTables[], NS::Range range);

    void         setVertexIntersectionFunctionTable(const class IntersectionFunctionTable* intersectionFunctionTable, NS::UInteger bufferIndex);

    void         setVertexIntersectionFunctionTables(const class IntersectionFunctionTable* intersectionFunctionTables[], NS::Range range);

    void         setVertexAccelerationStructure(const class AccelerationStructure* accelerationStructure, NS::UInteger bufferIndex);

    void         setViewport(MTL::Viewport viewport);

    void         setViewports(const MTL::Viewport* viewports, NS::UInteger count);

    void         setFrontFacingWinding(MTL::Winding frontFacingWinding);

    void         setVertexAmplificationCount(NS::UInteger count, const MTL::VertexAmplificationViewMapping* viewMappings);

    void         setCullMode(MTL::CullMode cullMode);

    void         setDepthClipMode(MTL::DepthClipMode depthClipMode);

    void         setDepthBias(float depthBias, float slopeScale, float clamp);

    void         setScissorRect(MTL::ScissorRect rect);

    void         setScissorRects(const MTL::ScissorRect* scissorRects, NS::UInteger count);

    void         setTriangleFillMode(MTL::TriangleFillMode fillMode);

    void         setFragmentBytes(const void* bytes, NS::UInteger length, NS::UInteger index);

    void         setFragmentBuffer(const class Buffer* buffer, NS::UInteger offset, NS::UInteger index);

    void         setFragmentBufferOffset(NS::UInteger offset, NS::UInteger index);

    void         setFragmentBuffers(MTL::Buffer* buffers[], const NS::UInteger offsets[], NS::Range range);

    void         setFragmentTexture(const class Texture* texture, NS::UInteger index);

    void         setFragmentTextures(MTL::Texture* textures[], NS::Range range);

    void         setFragmentSamplerState(const class SamplerState* sampler, NS::UInteger index);

    void         setFragmentSamplerStates(MTL::SamplerState* samplers[], NS::Range range);

    void         setFragmentSamplerState(const class SamplerState* sampler, float lodMinClamp, float lodMaxClamp, NS::UInteger index);

    void         setFragmentSamplerStates(MTL::SamplerState* samplers[], const float lodMinClamps[], const float lodMaxClamps[], NS::Range range);

    void         setFragmentVisibleFunctionTable(const class VisibleFunctionTable* functionTable, NS::UInteger bufferIndex);

    void         setFragmentVisibleFunctionTables(const VisibleFunctionTable* functionTables[], NS::Range range);

    void         setFragmentIntersectionFunctionTable(const class IntersectionFunctionTable* intersectionFunctionTable, NS::UInteger bufferIndex);

    void         setFragmentIntersectionFunctionTables(const class IntersectionFunctionTable* intersectionFunctionTables[], NS::Range range);

    void         setFragmentAccelerationStructure(const class AccelerationStructure* accelerationStructure, NS::UInteger bufferIndex);

    void         setBlendColorRed(float red, float green, float blue, float alpha);

    void         setDepthStencilState(const class DepthStencilState* depthStencilState);

    void         setStencilReferenceValue(uint32_t referenceValue);

    void         setStencilFrontReferenceValue(uint32_t frontReferenceValue, uint32_t backReferenceValue);

    void         setVisibilityResultMode(MTL::VisibilityResultMode mode, NS::UInteger offset);

    void         setColorStoreAction(MTL::StoreAction storeAction, NS::UInteger colorAttachmentIndex);

    void         setDepthStoreAction(MTL::StoreAction storeAction);

    void         setStencilStoreAction(MTL::StoreAction storeAction);

    void         setColorStoreActionOptions(MTL::StoreActionOptions storeActionOptions, NS::UInteger colorAttachmentIndex);

    void         setDepthStoreActionOptions(MTL::StoreActionOptions storeActionOptions);

    void         setStencilStoreActionOptions(MTL::StoreActionOptions storeActionOptions);

    void         drawPrimitives(MTL::PrimitiveType primitiveType, NS::UInteger vertexStart, NS::UInteger vertexCount, NS::UInteger instanceCount);

    void         drawPrimitives(MTL::PrimitiveType primitiveType, NS::UInteger vertexStart, NS::UInteger vertexCount);

    void         drawIndexedPrimitives(MTL::PrimitiveType primitiveType, NS::UInteger indexCount, MTL::IndexType indexType, const class Buffer* indexBuffer, NS::UInteger indexBufferOffset, NS::UInteger instanceCount);

    void         drawIndexedPrimitives(MTL::PrimitiveType primitiveType, NS::UInteger indexCount, MTL::IndexType indexType, const class Buffer* indexBuffer, NS::UInteger indexBufferOffset);

    void         drawPrimitives(MTL::PrimitiveType primitiveType, NS::UInteger vertexStart, NS::UInteger vertexCount, NS::UInteger instanceCount, NS::UInteger baseInstance);

    void         drawIndexedPrimitives(MTL::PrimitiveType primitiveType, NS::UInteger indexCount, MTL::IndexType indexType, const class Buffer* indexBuffer, NS::UInteger indexBufferOffset, NS::UInteger instanceCount, NS::Integer baseVertex, NS::UInteger baseInstance);

    void         drawPrimitives(MTL::PrimitiveType primitiveType, const class Buffer* indirectBuffer, NS::UInteger indirectBufferOffset);

    void         drawIndexedPrimitives(MTL::PrimitiveType primitiveType, MTL::IndexType indexType, const class Buffer* indexBuffer, NS::UInteger indexBufferOffset, const class Buffer* indirectBuffer, NS::UInteger indirectBufferOffset);

    void         textureBarrier();

    void         updateFence(const class Fence* fence, MTL::RenderStages stages);

    void         waitForFence(const class Fence* fence, MTL::RenderStages stages);

    void         setTessellationFactorBuffer(const class Buffer* buffer, NS::UInteger offset, NS::UInteger instanceStride);

    void         setTessellationFactorScale(float scale);

    void         drawPatches(NS::UInteger numberOfPatchControlPoints, NS::UInteger patchStart, NS::UInteger patchCount, const class Buffer* patchIndexBuffer, NS::UInteger patchIndexBufferOffset, NS::UInteger instanceCount, NS::UInteger baseInstance);

    void         drawPatches(NS::UInteger numberOfPatchControlPoints, const class Buffer* patchIndexBuffer, NS::UInteger patchIndexBufferOffset, const class Buffer* indirectBuffer, NS::UInteger indirectBufferOffset);

    void         drawIndexedPatches(NS::UInteger numberOfPatchControlPoints, NS::UInteger patchStart, NS::UInteger patchCount, const class Buffer* patchIndexBuffer, NS::UInteger patchIndexBufferOffset, const class Buffer* controlPointIndexBuffer, NS::UInteger controlPointIndexBufferOffset, NS::UInteger instanceCount, NS::UInteger baseInstance);

    void         drawIndexedPatches(NS::UInteger numberOfPatchControlPoints, const class Buffer* patchIndexBuffer, NS::UInteger patchIndexBufferOffset, const class Buffer* controlPointIndexBuffer, NS::UInteger controlPointIndexBufferOffset, const class Buffer* indirectBuffer, NS::UInteger indirectBufferOffset);

    NS::UInteger tileWidth() const;

    NS::UInteger tileHeight() const;

    void         setTileBytes(const void* bytes, NS::UInteger length, NS::UInteger index);

    void         setTileBuffer(const class Buffer* buffer, NS::UInteger offset, NS::UInteger index);

    void         setTileBufferOffset(NS::UInteger offset, NS::UInteger index);

    void         setTileBuffers(MTL::Buffer* buffers, const NS::UInteger* offsets, NS::Range range);

    void         setTileTexture(const class Texture* texture, NS::UInteger index);

    void         setTileTextures(MTL::Texture* textures[], NS::Range range);

    void         setTileSamplerState(const class SamplerState* sampler, NS::UInteger index);

    void         setTileSamplerStates(MTL::SamplerState* samplers[], NS::Range range);

    void         setTileSamplerState(const class SamplerState* sampler, float lodMinClamp, float lodMaxClamp, NS::UInteger index);

    void         setTileSamplerStates(MTL::SamplerState* samplers[], const float lodMinClamps[], const float lodMaxClamps[], NS::Range range);

    void         setTileVisibleFunctionTable(const class VisibleFunctionTable* functionTable, NS::UInteger bufferIndex);

    void         setTileVisibleFunctionTables(const class VisibleFunctionTable* functionTables[], NS::Range range);

    void         setTileIntersectionFunctionTable(const class IntersectionFunctionTable* intersectionFunctionTable, NS::UInteger bufferIndex);

    void         setTileIntersectionFunctionTables(const class IntersectionFunctionTable* intersectionFunctionTables[], NS::Range range);

    void         setTileAccelerationStructure(const class AccelerationStructure* accelerationStructure, NS::UInteger bufferIndex);

    void         dispatchThreadsPerTile(MTL::Size threadsPerTile);

    void         setThreadgroupMemoryLength(NS::UInteger length, NS::UInteger offset, NS::UInteger index);

    void         useResource(const class Resource* resource, MTL::ResourceUsage usage);

    void         useResources(MTL::Resource* resources[], NS::UInteger count, MTL::ResourceUsage usage);

    void         useResource(const class Resource* resource, MTL::ResourceUsage usage, MTL::RenderStages stages);

    void         useResources(MTL::Resource* resources, NS::UInteger count, MTL::ResourceUsage usage, MTL::RenderStages stages);

    void         useHeap(const class Heap* heap);

    void         useHeaps(MTL::Heap* heaps[], NS::UInteger count);

    void         useHeap(const class Heap* heap, MTL::RenderStages stages);

    void         useHeaps(MTL::Heap* heaps[], NS::UInteger count, MTL::RenderStages stages);

    void         executeCommandsInBuffer(const class IndirectCommandBuffer* indirectCommandBuffer, NS::Range executionRange);

    void         executeCommandsInBuffer(const class IndirectCommandBuffer* indirectCommandbuffer, const class Buffer* indirectRangeBuffer, NS::UInteger indirectBufferOffset);

    void         memoryBarrier(MTL::BarrierScope scope, MTL::RenderStages after, MTL::RenderStages before);

    void         memoryBarrier(MTL::Resource* resources[], NS::UInteger count, MTL::RenderStages after, MTL::RenderStages before);

    void         sampleCountersInBuffer(const class CounterSampleBuffer* sampleBuffer, NS::UInteger sampleIndex, bool barrier);
};

}

// method: setRenderPipelineState:
_MTL_INLINE void MTL::RenderCommandEncoder::setRenderPipelineState(const MTL::RenderPipelineState* pipelineState)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setRenderPipelineState_), pipelineState);
}

// method: setVertexBytes:length:atIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setVertexBytes(const void* bytes, NS::UInteger length, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexBytes_length_atIndex_), bytes, length, index);
}

// method: setVertexBuffer:offset:atIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setVertexBuffer(const MTL::Buffer* buffer, NS::UInteger offset, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexBuffer_offset_atIndex_), buffer, offset, index);
}

// method: setVertexBufferOffset:atIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setVertexBufferOffset(NS::UInteger offset, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexBufferOffset_atIndex_), offset, index);
}

// method: setVertexBuffers:offsets:withRange:
_MTL_INLINE void MTL::RenderCommandEncoder::setVertexBuffers(MTL::Buffer* buffers[], const NS::UInteger offsets[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexBuffers_offsets_withRange_), buffers, offsets, range);
}

// method: setVertexTexture:atIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setVertexTexture(const MTL::Texture* texture, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexTexture_atIndex_), texture, index);
}

// method: setVertexTextures:withRange:
_MTL_INLINE void MTL::RenderCommandEncoder::setVertexTextures(MTL::Texture* textures[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexTextures_withRange_), textures, range);
}

// method: setVertexSamplerState:atIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setVertexSamplerState(const MTL::SamplerState* sampler, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexSamplerState_atIndex_), sampler, index);
}

// method: setVertexSamplerStates:withRange:
_MTL_INLINE void MTL::RenderCommandEncoder::setVertexSamplerStates(MTL::SamplerState* samplers[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexSamplerStates_withRange_), samplers, range);
}

// method: setVertexSamplerState:lodMinClamp:lodMaxClamp:atIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setVertexSamplerState(const MTL::SamplerState* sampler, float lodMinClamp, float lodMaxClamp, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexSamplerState_lodMinClamp_lodMaxClamp_atIndex_), sampler, lodMinClamp, lodMaxClamp, index);
}

// method: setVertexSamplerStates:lodMinClamps:lodMaxClamps:withRange:
_MTL_INLINE void MTL::RenderCommandEncoder::setVertexSamplerStates(MTL::SamplerState* samplers[], const float lodMinClamps[], const float lodMaxClamps[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexSamplerStates_lodMinClamps_lodMaxClamps_withRange_), samplers, lodMinClamps, lodMaxClamps, range);
}

// method: setVertexVisibleFunctionTable:atBufferIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setVertexVisibleFunctionTable(const MTL::VisibleFunctionTable* functionTable, NS::UInteger bufferIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexVisibleFunctionTable_atBufferIndex_), functionTable, bufferIndex);
}

// method: setVertexVisibleFunctionTables:withBufferRange:
_MTL_INLINE void MTL::RenderCommandEncoder::setVertexVisibleFunctionTables(const MTL::VisibleFunctionTable* functionTables[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexVisibleFunctionTables_withBufferRange_), functionTables, range);
}

// method: setVertexIntersectionFunctionTable:atBufferIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setVertexIntersectionFunctionTable(const MTL::IntersectionFunctionTable* intersectionFunctionTable, NS::UInteger bufferIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexIntersectionFunctionTable_atBufferIndex_), intersectionFunctionTable, bufferIndex);
}

// method: setVertexIntersectionFunctionTables:withBufferRange:
_MTL_INLINE void MTL::RenderCommandEncoder::setVertexIntersectionFunctionTables(const MTL::IntersectionFunctionTable* intersectionFunctionTables[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexIntersectionFunctionTables_withBufferRange_), intersectionFunctionTables, range);
}

// method: setVertexAccelerationStructure:atBufferIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setVertexAccelerationStructure(const MTL::AccelerationStructure* accelerationStructure, NS::UInteger bufferIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexAccelerationStructure_atBufferIndex_), accelerationStructure, bufferIndex);
}

// method: setViewport:
_MTL_INLINE void MTL::RenderCommandEncoder::setViewport(MTL::Viewport viewport)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setViewport_), viewport);
}

// method: setViewports:count:
_MTL_INLINE void MTL::RenderCommandEncoder::setViewports(const MTL::Viewport* viewports, NS::UInteger count)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setViewports_count_), viewports, count);
}

// method: setFrontFacingWinding:
_MTL_INLINE void MTL::RenderCommandEncoder::setFrontFacingWinding(MTL::Winding frontFacingWinding)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFrontFacingWinding_), frontFacingWinding);
}

// method: setVertexAmplificationCount:viewMappings:
_MTL_INLINE void MTL::RenderCommandEncoder::setVertexAmplificationCount(NS::UInteger count, const MTL::VertexAmplificationViewMapping* viewMappings)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexAmplificationCount_viewMappings_), count, viewMappings);
}

// method: setCullMode:
_MTL_INLINE void MTL::RenderCommandEncoder::setCullMode(MTL::CullMode cullMode)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setCullMode_), cullMode);
}

// method: setDepthClipMode:
_MTL_INLINE void MTL::RenderCommandEncoder::setDepthClipMode(MTL::DepthClipMode depthClipMode)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setDepthClipMode_), depthClipMode);
}

// method: setDepthBias:slopeScale:clamp:
_MTL_INLINE void MTL::RenderCommandEncoder::setDepthBias(float depthBias, float slopeScale, float clamp)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setDepthBias_slopeScale_clamp_), depthBias, slopeScale, clamp);
}

// method: setScissorRect:
_MTL_INLINE void MTL::RenderCommandEncoder::setScissorRect(MTL::ScissorRect rect)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setScissorRect_), rect);
}

// method: setScissorRects:count:
_MTL_INLINE void MTL::RenderCommandEncoder::setScissorRects(const MTL::ScissorRect* scissorRects, NS::UInteger count)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setScissorRects_count_), scissorRects, count);
}

// method: setTriangleFillMode:
_MTL_INLINE void MTL::RenderCommandEncoder::setTriangleFillMode(MTL::TriangleFillMode fillMode)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTriangleFillMode_), fillMode);
}

// method: setFragmentBytes:length:atIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setFragmentBytes(const void* bytes, NS::UInteger length, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFragmentBytes_length_atIndex_), bytes, length, index);
}

// method: setFragmentBuffer:offset:atIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setFragmentBuffer(const MTL::Buffer* buffer, NS::UInteger offset, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFragmentBuffer_offset_atIndex_), buffer, offset, index);
}

// method: setFragmentBufferOffset:atIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setFragmentBufferOffset(NS::UInteger offset, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFragmentBufferOffset_atIndex_), offset, index);
}

// method: setFragmentBuffers:offsets:withRange:
_MTL_INLINE void MTL::RenderCommandEncoder::setFragmentBuffers(MTL::Buffer* buffers[], const NS::UInteger offsets[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFragmentBuffers_offsets_withRange_), buffers, offsets, range);
}

// method: setFragmentTexture:atIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setFragmentTexture(const MTL::Texture* texture, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFragmentTexture_atIndex_), texture, index);
}

// method: setFragmentTextures:withRange:
_MTL_INLINE void MTL::RenderCommandEncoder::setFragmentTextures(MTL::Texture* textures[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFragmentTextures_withRange_), textures, range);
}

// method: setFragmentSamplerState:atIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setFragmentSamplerState(const MTL::SamplerState* sampler, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFragmentSamplerState_atIndex_), sampler, index);
}

// method: setFragmentSamplerStates:withRange:
_MTL_INLINE void MTL::RenderCommandEncoder::setFragmentSamplerStates(MTL::SamplerState* samplers[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFragmentSamplerStates_withRange_), samplers, range);
}

// method: setFragmentSamplerState:lodMinClamp:lodMaxClamp:atIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setFragmentSamplerState(const MTL::SamplerState* sampler, float lodMinClamp, float lodMaxClamp, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFragmentSamplerState_lodMinClamp_lodMaxClamp_atIndex_), sampler, lodMinClamp, lodMaxClamp, index);
}

// method: setFragmentSamplerStates:lodMinClamps:lodMaxClamps:withRange:
_MTL_INLINE void MTL::RenderCommandEncoder::setFragmentSamplerStates(MTL::SamplerState* samplers[], const float lodMinClamps[], const float lodMaxClamps[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFragmentSamplerStates_lodMinClamps_lodMaxClamps_withRange_), samplers, lodMinClamps, lodMaxClamps, range);
}

// method: setFragmentVisibleFunctionTable:atBufferIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setFragmentVisibleFunctionTable(const MTL::VisibleFunctionTable* functionTable, NS::UInteger bufferIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFragmentVisibleFunctionTable_atBufferIndex_), functionTable, bufferIndex);
}

// method: setFragmentVisibleFunctionTables:withBufferRange:
_MTL_INLINE void MTL::RenderCommandEncoder::setFragmentVisibleFunctionTables(const MTL::VisibleFunctionTable* functionTables[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFragmentVisibleFunctionTables_withBufferRange_), functionTables, range);
}

// method: setFragmentIntersectionFunctionTable:atBufferIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setFragmentIntersectionFunctionTable(const MTL::IntersectionFunctionTable* intersectionFunctionTable, NS::UInteger bufferIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFragmentIntersectionFunctionTable_atBufferIndex_), intersectionFunctionTable, bufferIndex);
}

// method: setFragmentIntersectionFunctionTables:withBufferRange:
_MTL_INLINE void MTL::RenderCommandEncoder::setFragmentIntersectionFunctionTables(const MTL::IntersectionFunctionTable* intersectionFunctionTables[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFragmentIntersectionFunctionTables_withBufferRange_), intersectionFunctionTables, range);
}

// method: setFragmentAccelerationStructure:atBufferIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setFragmentAccelerationStructure(const MTL::AccelerationStructure* accelerationStructure, NS::UInteger bufferIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFragmentAccelerationStructure_atBufferIndex_), accelerationStructure, bufferIndex);
}

// method: setBlendColorRed:green:blue:alpha:
_MTL_INLINE void MTL::RenderCommandEncoder::setBlendColorRed(float red, float green, float blue, float alpha)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setBlendColorRed_green_blue_alpha_), red, green, blue, alpha);
}

// method: setDepthStencilState:
_MTL_INLINE void MTL::RenderCommandEncoder::setDepthStencilState(const MTL::DepthStencilState* depthStencilState)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setDepthStencilState_), depthStencilState);
}

// method: setStencilReferenceValue:
_MTL_INLINE void MTL::RenderCommandEncoder::setStencilReferenceValue(uint32_t referenceValue)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStencilReferenceValue_), referenceValue);
}

// method: setStencilFrontReferenceValue:backReferenceValue:
_MTL_INLINE void MTL::RenderCommandEncoder::setStencilFrontReferenceValue(uint32_t frontReferenceValue, uint32_t backReferenceValue)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStencilFrontReferenceValue_backReferenceValue_), frontReferenceValue, backReferenceValue);
}

// method: setVisibilityResultMode:offset:
_MTL_INLINE void MTL::RenderCommandEncoder::setVisibilityResultMode(MTL::VisibilityResultMode mode, NS::UInteger offset)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVisibilityResultMode_offset_), mode, offset);
}

// method: setColorStoreAction:atIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setColorStoreAction(MTL::StoreAction storeAction, NS::UInteger colorAttachmentIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setColorStoreAction_atIndex_), storeAction, colorAttachmentIndex);
}

// method: setDepthStoreAction:
_MTL_INLINE void MTL::RenderCommandEncoder::setDepthStoreAction(MTL::StoreAction storeAction)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setDepthStoreAction_), storeAction);
}

// method: setStencilStoreAction:
_MTL_INLINE void MTL::RenderCommandEncoder::setStencilStoreAction(MTL::StoreAction storeAction)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStencilStoreAction_), storeAction);
}

// method: setColorStoreActionOptions:atIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setColorStoreActionOptions(MTL::StoreActionOptions storeActionOptions, NS::UInteger colorAttachmentIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setColorStoreActionOptions_atIndex_), storeActionOptions, colorAttachmentIndex);
}

// method: setDepthStoreActionOptions:
_MTL_INLINE void MTL::RenderCommandEncoder::setDepthStoreActionOptions(MTL::StoreActionOptions storeActionOptions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setDepthStoreActionOptions_), storeActionOptions);
}

// method: setStencilStoreActionOptions:
_MTL_INLINE void MTL::RenderCommandEncoder::setStencilStoreActionOptions(MTL::StoreActionOptions storeActionOptions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStencilStoreActionOptions_), storeActionOptions);
}

// method: drawPrimitives:vertexStart:vertexCount:instanceCount:
_MTL_INLINE void MTL::RenderCommandEncoder::drawPrimitives(MTL::PrimitiveType primitiveType, NS::UInteger vertexStart, NS::UInteger vertexCount, NS::UInteger instanceCount)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(drawPrimitives_vertexStart_vertexCount_instanceCount_), primitiveType, vertexStart, vertexCount, instanceCount);
}

// method: drawPrimitives:vertexStart:vertexCount:
_MTL_INLINE void MTL::RenderCommandEncoder::drawPrimitives(MTL::PrimitiveType primitiveType, NS::UInteger vertexStart, NS::UInteger vertexCount)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(drawPrimitives_vertexStart_vertexCount_), primitiveType, vertexStart, vertexCount);
}

// method: drawIndexedPrimitives:indexCount:indexType:indexBuffer:indexBufferOffset:instanceCount:
_MTL_INLINE void MTL::RenderCommandEncoder::drawIndexedPrimitives(MTL::PrimitiveType primitiveType, NS::UInteger indexCount, MTL::IndexType indexType, const MTL::Buffer* indexBuffer, NS::UInteger indexBufferOffset, NS::UInteger instanceCount)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(drawIndexedPrimitives_indexCount_indexType_indexBuffer_indexBufferOffset_instanceCount_), primitiveType, indexCount, indexType, indexBuffer, indexBufferOffset, instanceCount);
}

// method: drawIndexedPrimitives:indexCount:indexType:indexBuffer:indexBufferOffset:
_MTL_INLINE void MTL::RenderCommandEncoder::drawIndexedPrimitives(MTL::PrimitiveType primitiveType, NS::UInteger indexCount, MTL::IndexType indexType, const MTL::Buffer* indexBuffer, NS::UInteger indexBufferOffset)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(drawIndexedPrimitives_indexCount_indexType_indexBuffer_indexBufferOffset_), primitiveType, indexCount, indexType, indexBuffer, indexBufferOffset);
}

// method: drawPrimitives:vertexStart:vertexCount:instanceCount:baseInstance:
_MTL_INLINE void MTL::RenderCommandEncoder::drawPrimitives(MTL::PrimitiveType primitiveType, NS::UInteger vertexStart, NS::UInteger vertexCount, NS::UInteger instanceCount, NS::UInteger baseInstance)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(drawPrimitives_vertexStart_vertexCount_instanceCount_baseInstance_), primitiveType, vertexStart, vertexCount, instanceCount, baseInstance);
}

// method: drawIndexedPrimitives:indexCount:indexType:indexBuffer:indexBufferOffset:instanceCount:baseVertex:baseInstance:
_MTL_INLINE void MTL::RenderCommandEncoder::drawIndexedPrimitives(MTL::PrimitiveType primitiveType, NS::UInteger indexCount, MTL::IndexType indexType, const MTL::Buffer* indexBuffer, NS::UInteger indexBufferOffset, NS::UInteger instanceCount, NS::Integer baseVertex, NS::UInteger baseInstance)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(drawIndexedPrimitives_indexCount_indexType_indexBuffer_indexBufferOffset_instanceCount_baseVertex_baseInstance_), primitiveType, indexCount, indexType, indexBuffer, indexBufferOffset, instanceCount, baseVertex, baseInstance);
}

// method: drawPrimitives:indirectBuffer:indirectBufferOffset:
_MTL_INLINE void MTL::RenderCommandEncoder::drawPrimitives(MTL::PrimitiveType primitiveType, const MTL::Buffer* indirectBuffer, NS::UInteger indirectBufferOffset)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(drawPrimitives_indirectBuffer_indirectBufferOffset_), primitiveType, indirectBuffer, indirectBufferOffset);
}

// method: drawIndexedPrimitives:indexType:indexBuffer:indexBufferOffset:indirectBuffer:indirectBufferOffset:
_MTL_INLINE void MTL::RenderCommandEncoder::drawIndexedPrimitives(MTL::PrimitiveType primitiveType, MTL::IndexType indexType, const MTL::Buffer* indexBuffer, NS::UInteger indexBufferOffset, const MTL::Buffer* indirectBuffer, NS::UInteger indirectBufferOffset)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(drawIndexedPrimitives_indexType_indexBuffer_indexBufferOffset_indirectBuffer_indirectBufferOffset_), primitiveType, indexType, indexBuffer, indexBufferOffset, indirectBuffer, indirectBufferOffset);
}

// method: textureBarrier
_MTL_INLINE void MTL::RenderCommandEncoder::textureBarrier()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(textureBarrier));
}

// method: updateFence:afterStages:
_MTL_INLINE void MTL::RenderCommandEncoder::updateFence(const MTL::Fence* fence, MTL::RenderStages stages)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(updateFence_afterStages_), fence, stages);
}

// method: waitForFence:beforeStages:
_MTL_INLINE void MTL::RenderCommandEncoder::waitForFence(const MTL::Fence* fence, MTL::RenderStages stages)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(waitForFence_beforeStages_), fence, stages);
}

// method: setTessellationFactorBuffer:offset:instanceStride:
_MTL_INLINE void MTL::RenderCommandEncoder::setTessellationFactorBuffer(const MTL::Buffer* buffer, NS::UInteger offset, NS::UInteger instanceStride)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTessellationFactorBuffer_offset_instanceStride_), buffer, offset, instanceStride);
}

// method: setTessellationFactorScale:
_MTL_INLINE void MTL::RenderCommandEncoder::setTessellationFactorScale(float scale)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTessellationFactorScale_), scale);
}

// method: drawPatches:patchStart:patchCount:patchIndexBuffer:patchIndexBufferOffset:instanceCount:baseInstance:
_MTL_INLINE void MTL::RenderCommandEncoder::drawPatches(NS::UInteger numberOfPatchControlPoints, NS::UInteger patchStart, NS::UInteger patchCount, const MTL::Buffer* patchIndexBuffer, NS::UInteger patchIndexBufferOffset, NS::UInteger instanceCount, NS::UInteger baseInstance)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(drawPatches_patchStart_patchCount_patchIndexBuffer_patchIndexBufferOffset_instanceCount_baseInstance_), numberOfPatchControlPoints, patchStart, patchCount, patchIndexBuffer, patchIndexBufferOffset, instanceCount, baseInstance);
}

// method: drawPatches:patchIndexBuffer:patchIndexBufferOffset:indirectBuffer:indirectBufferOffset:
_MTL_INLINE void MTL::RenderCommandEncoder::drawPatches(NS::UInteger numberOfPatchControlPoints, const MTL::Buffer* patchIndexBuffer, NS::UInteger patchIndexBufferOffset, const MTL::Buffer* indirectBuffer, NS::UInteger indirectBufferOffset)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(drawPatches_patchIndexBuffer_patchIndexBufferOffset_indirectBuffer_indirectBufferOffset_), numberOfPatchControlPoints, patchIndexBuffer, patchIndexBufferOffset, indirectBuffer, indirectBufferOffset);
}

// method: drawIndexedPatches:patchStart:patchCount:patchIndexBuffer:patchIndexBufferOffset:controlPointIndexBuffer:controlPointIndexBufferOffset:instanceCount:baseInstance:
_MTL_INLINE void MTL::RenderCommandEncoder::drawIndexedPatches(NS::UInteger numberOfPatchControlPoints, NS::UInteger patchStart, NS::UInteger patchCount, const MTL::Buffer* patchIndexBuffer, NS::UInteger patchIndexBufferOffset, const MTL::Buffer* controlPointIndexBuffer, NS::UInteger controlPointIndexBufferOffset, NS::UInteger instanceCount, NS::UInteger baseInstance)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(drawIndexedPatches_patchStart_patchCount_patchIndexBuffer_patchIndexBufferOffset_controlPointIndexBuffer_controlPointIndexBufferOffset_instanceCount_baseInstance_), numberOfPatchControlPoints, patchStart, patchCount, patchIndexBuffer, patchIndexBufferOffset, controlPointIndexBuffer, controlPointIndexBufferOffset, instanceCount, baseInstance);
}

// method: drawIndexedPatches:patchIndexBuffer:patchIndexBufferOffset:controlPointIndexBuffer:controlPointIndexBufferOffset:indirectBuffer:indirectBufferOffset:
_MTL_INLINE void MTL::RenderCommandEncoder::drawIndexedPatches(NS::UInteger numberOfPatchControlPoints, const MTL::Buffer* patchIndexBuffer, NS::UInteger patchIndexBufferOffset, const MTL::Buffer* controlPointIndexBuffer, NS::UInteger controlPointIndexBufferOffset, const MTL::Buffer* indirectBuffer, NS::UInteger indirectBufferOffset)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(drawIndexedPatches_patchIndexBuffer_patchIndexBufferOffset_controlPointIndexBuffer_controlPointIndexBufferOffset_indirectBuffer_indirectBufferOffset_), numberOfPatchControlPoints, patchIndexBuffer, patchIndexBufferOffset, controlPointIndexBuffer, controlPointIndexBufferOffset, indirectBuffer, indirectBufferOffset);
}

// property: tileWidth
_MTL_INLINE NS::UInteger MTL::RenderCommandEncoder::tileWidth() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(tileWidth));
}

// property: tileHeight
_MTL_INLINE NS::UInteger MTL::RenderCommandEncoder::tileHeight() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(tileHeight));
}

// method: setTileBytes:length:atIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setTileBytes(const void* bytes, NS::UInteger length, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTileBytes_length_atIndex_), bytes, length, index);
}

// method: setTileBuffer:offset:atIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setTileBuffer(const MTL::Buffer* buffer, NS::UInteger offset, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTileBuffer_offset_atIndex_), buffer, offset, index);
}

// method: setTileBufferOffset:atIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setTileBufferOffset(NS::UInteger offset, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTileBufferOffset_atIndex_), offset, index);
}

// method: setTileBuffers:offsets:withRange:
_MTL_INLINE void MTL::RenderCommandEncoder::setTileBuffers(MTL::Buffer* buffers, const NS::UInteger* offsets, NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTileBuffers_offsets_withRange_), buffers, offsets, range);
}

// method: setTileTexture:atIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setTileTexture(const MTL::Texture* texture, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTileTexture_atIndex_), texture, index);
}

// method: setTileTextures:withRange:
_MTL_INLINE void MTL::RenderCommandEncoder::setTileTextures(MTL::Texture* textures[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTileTextures_withRange_), textures, range);
}

// method: setTileSamplerState:atIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setTileSamplerState(const MTL::SamplerState* sampler, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTileSamplerState_atIndex_), sampler, index);
}

// method: setTileSamplerStates:withRange:
_MTL_INLINE void MTL::RenderCommandEncoder::setTileSamplerStates(MTL::SamplerState* samplers[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTileSamplerStates_withRange_), samplers, range);
}

// method: setTileSamplerState:lodMinClamp:lodMaxClamp:atIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setTileSamplerState(const MTL::SamplerState* sampler, float lodMinClamp, float lodMaxClamp, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTileSamplerState_lodMinClamp_lodMaxClamp_atIndex_), sampler, lodMinClamp, lodMaxClamp, index);
}

// method: setTileSamplerStates:lodMinClamps:lodMaxClamps:withRange:
_MTL_INLINE void MTL::RenderCommandEncoder::setTileSamplerStates(MTL::SamplerState* samplers[], const float lodMinClamps[], const float lodMaxClamps[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTileSamplerStates_lodMinClamps_lodMaxClamps_withRange_), samplers, lodMinClamps, lodMaxClamps, range);
}

// method: setTileVisibleFunctionTable:atBufferIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setTileVisibleFunctionTable(const MTL::VisibleFunctionTable* functionTable, NS::UInteger bufferIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTileVisibleFunctionTable_atBufferIndex_), functionTable, bufferIndex);
}

// method: setTileVisibleFunctionTables:withBufferRange:
_MTL_INLINE void MTL::RenderCommandEncoder::setTileVisibleFunctionTables(const MTL::VisibleFunctionTable* functionTables[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTileVisibleFunctionTables_withBufferRange_), functionTables, range);
}

// method: setTileIntersectionFunctionTable:atBufferIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setTileIntersectionFunctionTable(const MTL::IntersectionFunctionTable* intersectionFunctionTable, NS::UInteger bufferIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTileIntersectionFunctionTable_atBufferIndex_), intersectionFunctionTable, bufferIndex);
}

// method: setTileIntersectionFunctionTables:withBufferRange:
_MTL_INLINE void MTL::RenderCommandEncoder::setTileIntersectionFunctionTables(const MTL::IntersectionFunctionTable* intersectionFunctionTables[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTileIntersectionFunctionTables_withBufferRange_), intersectionFunctionTables, range);
}

// method: setTileAccelerationStructure:atBufferIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setTileAccelerationStructure(const MTL::AccelerationStructure* accelerationStructure, NS::UInteger bufferIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTileAccelerationStructure_atBufferIndex_), accelerationStructure, bufferIndex);
}

// method: dispatchThreadsPerTile:
_MTL_INLINE void MTL::RenderCommandEncoder::dispatchThreadsPerTile(MTL::Size threadsPerTile)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(dispatchThreadsPerTile_), threadsPerTile);
}

// method: setThreadgroupMemoryLength:offset:atIndex:
_MTL_INLINE void MTL::RenderCommandEncoder::setThreadgroupMemoryLength(NS::UInteger length, NS::UInteger offset, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setThreadgroupMemoryLength_offset_atIndex_), length, offset, index);
}

// method: useResource:usage:
_MTL_INLINE void MTL::RenderCommandEncoder::useResource(const MTL::Resource* resource, MTL::ResourceUsage usage)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(useResource_usage_), resource, usage);
}

// method: useResources:count:usage:
_MTL_INLINE void MTL::RenderCommandEncoder::useResources(MTL::Resource* resources[], NS::UInteger count, MTL::ResourceUsage usage)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(useResources_count_usage_), resources, count, usage);
}

// method: useResource:usage:stages:
_MTL_INLINE void MTL::RenderCommandEncoder::useResource(const MTL::Resource* resource, MTL::ResourceUsage usage, MTL::RenderStages stages)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(useResource_usage_stages_), resource, usage, stages);
}

// method: useResources:count:usage:stages:
_MTL_INLINE void MTL::RenderCommandEncoder::useResources(MTL::Resource* resources, NS::UInteger count, MTL::ResourceUsage usage, MTL::RenderStages stages)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(useResources_count_usage_stages_), resources, count, usage, stages);
}

// method: useHeap:
_MTL_INLINE void MTL::RenderCommandEncoder::useHeap(const MTL::Heap* heap)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(useHeap_), heap);
}

// method: useHeaps:count:
_MTL_INLINE void MTL::RenderCommandEncoder::useHeaps(MTL::Heap* heaps[], NS::UInteger count)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(useHeaps_count_), heaps, count);
}

// method: useHeap:stages:
_MTL_INLINE void MTL::RenderCommandEncoder::useHeap(const MTL::Heap* heap, MTL::RenderStages stages)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(useHeap_stages_), heap, stages);
}

// method: useHeaps:count:stages:
_MTL_INLINE void MTL::RenderCommandEncoder::useHeaps(MTL::Heap* heaps[], NS::UInteger count, MTL::RenderStages stages)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(useHeaps_count_stages_), heaps, count, stages);
}

// method: executeCommandsInBuffer:withRange:
_MTL_INLINE void MTL::RenderCommandEncoder::executeCommandsInBuffer(const MTL::IndirectCommandBuffer* indirectCommandBuffer, NS::Range executionRange)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(executeCommandsInBuffer_withRange_), indirectCommandBuffer, executionRange);
}

// method: executeCommandsInBuffer:indirectBuffer:indirectBufferOffset:
_MTL_INLINE void MTL::RenderCommandEncoder::executeCommandsInBuffer(const MTL::IndirectCommandBuffer* indirectCommandbuffer, const MTL::Buffer* indirectRangeBuffer, NS::UInteger indirectBufferOffset)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(executeCommandsInBuffer_indirectBuffer_indirectBufferOffset_), indirectCommandbuffer, indirectRangeBuffer, indirectBufferOffset);
}

// method: memoryBarrierWithScope:afterStages:beforeStages:
_MTL_INLINE void MTL::RenderCommandEncoder::memoryBarrier(MTL::BarrierScope scope, MTL::RenderStages after, MTL::RenderStages before)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(memoryBarrierWithScope_afterStages_beforeStages_), scope, after, before);
}

// method: memoryBarrierWithResources:count:afterStages:beforeStages:
_MTL_INLINE void MTL::RenderCommandEncoder::memoryBarrier(MTL::Resource* resources[], NS::UInteger count, MTL::RenderStages after, MTL::RenderStages before)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(memoryBarrierWithResources_count_afterStages_beforeStages_), resources, count, after, before);
}

// method: sampleCountersInBuffer:atSampleIndex:withBarrier:
_MTL_INLINE void MTL::RenderCommandEncoder::sampleCountersInBuffer(const MTL::CounterSampleBuffer* sampleBuffer, NS::UInteger sampleIndex, bool barrier)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(sampleCountersInBuffer_atSampleIndex_withBarrier_), sampleBuffer, sampleIndex, barrier);
}
