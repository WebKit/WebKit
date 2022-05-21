//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLArgumentEncoder.hpp
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
#include "MTLComputePipeline.hpp"
#include "MTLIndirectCommandBuffer.hpp"
#include "MTLIntersectionFunctionTable.hpp"
#include "MTLRenderPipeline.hpp"
#include "MTLSampler.hpp"
#include "MTLTexture.hpp"
#include "MTLVisibleFunctionTable.hpp"

namespace MTL
{
class ArgumentEncoder : public NS::Referencing<ArgumentEncoder>
{
public:
    class Device*          device() const;

    NS::String*            label() const;
    void                   setLabel(const NS::String* label);

    NS::UInteger           encodedLength() const;

    NS::UInteger           alignment() const;

    void                   setArgumentBuffer(const class Buffer* argumentBuffer, NS::UInteger offset);

    void                   setArgumentBuffer(const class Buffer* argumentBuffer, NS::UInteger startOffset, NS::UInteger arrayElement);

    void                   setBuffer(const class Buffer* buffer, NS::UInteger offset, NS::UInteger index);

    void                   setBuffers(MTL::Buffer* buffers[], const NS::UInteger offsets[], NS::Range range);

    void                   setTexture(const class Texture* texture, NS::UInteger index);

    void                   setTextures(MTL::Texture* textures[], NS::Range range);

    void                   setSamplerState(const class SamplerState* sampler, NS::UInteger index);

    void                   setSamplerStates(MTL::SamplerState* samplers[], NS::Range range);

    void*                  constantData(NS::UInteger index);

    void                   setRenderPipelineState(const class RenderPipelineState* pipeline, NS::UInteger index);

    void                   setRenderPipelineStates(MTL::RenderPipelineState* pipelines, NS::Range range);

    void                   setComputePipelineState(const class ComputePipelineState* pipeline, NS::UInteger index);

    void                   setComputePipelineStates(MTL::ComputePipelineState* pipelines, NS::Range range);

    void                   setIndirectCommandBuffer(const class IndirectCommandBuffer* indirectCommandBuffer, NS::UInteger index);

    void                   setIndirectCommandBuffers(MTL::IndirectCommandBuffer* buffers, NS::Range range);

    void                   setAccelerationStructure(const class AccelerationStructure* accelerationStructure, NS::UInteger index);

    class ArgumentEncoder* newArgumentEncoder(NS::UInteger index);

    void                   setVisibleFunctionTable(const class VisibleFunctionTable* visibleFunctionTable, NS::UInteger index);

    void                   setVisibleFunctionTables(const VisibleFunctionTable* visibleFunctionTables[], NS::Range range);

    void                   setIntersectionFunctionTable(const class IntersectionFunctionTable* intersectionFunctionTable, NS::UInteger index);

    void                   setIntersectionFunctionTables(const IntersectionFunctionTable* intersectionFunctionTables[], NS::Range range);
};

}

// property: device
_MTL_INLINE MTL::Device* MTL::ArgumentEncoder::device() const
{
    return Object::sendMessage<MTL::Device*>(this, _MTL_PRIVATE_SEL(device));
}

// property: label
_MTL_INLINE NS::String* MTL::ArgumentEncoder::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

_MTL_INLINE void MTL::ArgumentEncoder::setLabel(const NS::String* label)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLabel_), label);
}

// property: encodedLength
_MTL_INLINE NS::UInteger MTL::ArgumentEncoder::encodedLength() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(encodedLength));
}

// property: alignment
_MTL_INLINE NS::UInteger MTL::ArgumentEncoder::alignment() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(alignment));
}

// method: setArgumentBuffer:offset:
_MTL_INLINE void MTL::ArgumentEncoder::setArgumentBuffer(const MTL::Buffer* argumentBuffer, NS::UInteger offset)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setArgumentBuffer_offset_), argumentBuffer, offset);
}

// method: setArgumentBuffer:startOffset:arrayElement:
_MTL_INLINE void MTL::ArgumentEncoder::setArgumentBuffer(const MTL::Buffer* argumentBuffer, NS::UInteger startOffset, NS::UInteger arrayElement)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setArgumentBuffer_startOffset_arrayElement_), argumentBuffer, startOffset, arrayElement);
}

// method: setBuffer:offset:atIndex:
_MTL_INLINE void MTL::ArgumentEncoder::setBuffer(const MTL::Buffer* buffer, NS::UInteger offset, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setBuffer_offset_atIndex_), buffer, offset, index);
}

// method: setBuffers:offsets:withRange:
_MTL_INLINE void MTL::ArgumentEncoder::setBuffers(MTL::Buffer* buffers[], const NS::UInteger offsets[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setBuffers_offsets_withRange_), buffers, offsets, range);
}

// method: setTexture:atIndex:
_MTL_INLINE void MTL::ArgumentEncoder::setTexture(const MTL::Texture* texture, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTexture_atIndex_), texture, index);
}

// method: setTextures:withRange:
_MTL_INLINE void MTL::ArgumentEncoder::setTextures(MTL::Texture* textures[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTextures_withRange_), textures, range);
}

// method: setSamplerState:atIndex:
_MTL_INLINE void MTL::ArgumentEncoder::setSamplerState(const MTL::SamplerState* sampler, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSamplerState_atIndex_), sampler, index);
}

// method: setSamplerStates:withRange:
_MTL_INLINE void MTL::ArgumentEncoder::setSamplerStates(MTL::SamplerState* samplers[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSamplerStates_withRange_), samplers, range);
}

// method: constantDataAtIndex:
_MTL_INLINE void* MTL::ArgumentEncoder::constantData(NS::UInteger index)
{
    return Object::sendMessage<void*>(this, _MTL_PRIVATE_SEL(constantDataAtIndex_), index);
}

// method: setRenderPipelineState:atIndex:
_MTL_INLINE void MTL::ArgumentEncoder::setRenderPipelineState(const MTL::RenderPipelineState* pipeline, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setRenderPipelineState_atIndex_), pipeline, index);
}

// method: setRenderPipelineStates:withRange:
_MTL_INLINE void MTL::ArgumentEncoder::setRenderPipelineStates(MTL::RenderPipelineState* pipelines, NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setRenderPipelineStates_withRange_), pipelines, range);
}

// method: setComputePipelineState:atIndex:
_MTL_INLINE void MTL::ArgumentEncoder::setComputePipelineState(const MTL::ComputePipelineState* pipeline, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setComputePipelineState_atIndex_), pipeline, index);
}

// method: setComputePipelineStates:withRange:
_MTL_INLINE void MTL::ArgumentEncoder::setComputePipelineStates(MTL::ComputePipelineState* pipelines, NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setComputePipelineStates_withRange_), pipelines, range);
}

// method: setIndirectCommandBuffer:atIndex:
_MTL_INLINE void MTL::ArgumentEncoder::setIndirectCommandBuffer(const MTL::IndirectCommandBuffer* indirectCommandBuffer, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setIndirectCommandBuffer_atIndex_), indirectCommandBuffer, index);
}

// method: setIndirectCommandBuffers:withRange:
_MTL_INLINE void MTL::ArgumentEncoder::setIndirectCommandBuffers(MTL::IndirectCommandBuffer* buffers, NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setIndirectCommandBuffers_withRange_), buffers, range);
}

// method: setAccelerationStructure:atIndex:
_MTL_INLINE void MTL::ArgumentEncoder::setAccelerationStructure(const MTL::AccelerationStructure* accelerationStructure, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setAccelerationStructure_atIndex_), accelerationStructure, index);
}

// method: newArgumentEncoderForBufferAtIndex:
_MTL_INLINE MTL::ArgumentEncoder* MTL::ArgumentEncoder::newArgumentEncoder(NS::UInteger index)
{
    return Object::sendMessage<MTL::ArgumentEncoder*>(this, _MTL_PRIVATE_SEL(newArgumentEncoderForBufferAtIndex_), index);
}

// method: setVisibleFunctionTable:atIndex:
_MTL_INLINE void MTL::ArgumentEncoder::setVisibleFunctionTable(const MTL::VisibleFunctionTable* visibleFunctionTable, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVisibleFunctionTable_atIndex_), visibleFunctionTable, index);
}

// method: setVisibleFunctionTables:withRange:
_MTL_INLINE void MTL::ArgumentEncoder::setVisibleFunctionTables(const MTL::VisibleFunctionTable* visibleFunctionTables[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVisibleFunctionTables_withRange_), visibleFunctionTables, range);
}

// method: setIntersectionFunctionTable:atIndex:
_MTL_INLINE void MTL::ArgumentEncoder::setIntersectionFunctionTable(const MTL::IntersectionFunctionTable* intersectionFunctionTable, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setIntersectionFunctionTable_atIndex_), intersectionFunctionTable, index);
}

// method: setIntersectionFunctionTables:withRange:
_MTL_INLINE void MTL::ArgumentEncoder::setIntersectionFunctionTables(const MTL::IntersectionFunctionTable* intersectionFunctionTables[], NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setIntersectionFunctionTables_withRange_), intersectionFunctionTables, range);
}
