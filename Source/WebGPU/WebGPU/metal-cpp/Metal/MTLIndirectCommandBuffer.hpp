//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLIndirectCommandBuffer.hpp
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

#include "MTLIndirectCommandBuffer.hpp"
#include "MTLResource.hpp"

namespace MTL
{
_MTL_OPTIONS(NS::UInteger, IndirectCommandType) {
    IndirectCommandTypeDraw = 1,
    IndirectCommandTypeDrawIndexed = 2,
    IndirectCommandTypeDrawPatches = 4,
    IndirectCommandTypeDrawIndexedPatches = 8,
    IndirectCommandTypeConcurrentDispatch = 32,
    IndirectCommandTypeConcurrentDispatchThreads = 64,
};

struct IndirectCommandBufferExecutionRange
{
    uint32_t location;
    uint32_t length;
} _MTL_PACKED;

class IndirectCommandBufferDescriptor : public NS::Copying<IndirectCommandBufferDescriptor>
{
public:
    static class IndirectCommandBufferDescriptor* alloc();

    class IndirectCommandBufferDescriptor*        init();

    MTL::IndirectCommandType                      commandTypes() const;
    void                                          setCommandTypes(MTL::IndirectCommandType commandTypes);

    bool                                          inheritPipelineState() const;
    void                                          setInheritPipelineState(bool inheritPipelineState);

    bool                                          inheritBuffers() const;
    void                                          setInheritBuffers(bool inheritBuffers);

    NS::UInteger                                  maxVertexBufferBindCount() const;
    void                                          setMaxVertexBufferBindCount(NS::UInteger maxVertexBufferBindCount);

    NS::UInteger                                  maxFragmentBufferBindCount() const;
    void                                          setMaxFragmentBufferBindCount(NS::UInteger maxFragmentBufferBindCount);

    NS::UInteger                                  maxKernelBufferBindCount() const;
    void                                          setMaxKernelBufferBindCount(NS::UInteger maxKernelBufferBindCount);
};

class IndirectCommandBuffer : public NS::Referencing<IndirectCommandBuffer, Resource>
{
public:
    NS::UInteger                  size() const;

    void                          reset(NS::Range range);

    class IndirectRenderCommand*  indirectRenderCommand(NS::UInteger commandIndex);

    class IndirectComputeCommand* indirectComputeCommand(NS::UInteger commandIndex);
};

}

// static method: alloc
_MTL_INLINE MTL::IndirectCommandBufferDescriptor* MTL::IndirectCommandBufferDescriptor::alloc()
{
    return NS::Object::alloc<MTL::IndirectCommandBufferDescriptor>(_MTL_PRIVATE_CLS(MTLIndirectCommandBufferDescriptor));
}

// method: init
_MTL_INLINE MTL::IndirectCommandBufferDescriptor* MTL::IndirectCommandBufferDescriptor::init()
{
    return NS::Object::init<MTL::IndirectCommandBufferDescriptor>();
}

// property: commandTypes
_MTL_INLINE MTL::IndirectCommandType MTL::IndirectCommandBufferDescriptor::commandTypes() const
{
    return Object::sendMessage<MTL::IndirectCommandType>(this, _MTL_PRIVATE_SEL(commandTypes));
}

_MTL_INLINE void MTL::IndirectCommandBufferDescriptor::setCommandTypes(MTL::IndirectCommandType commandTypes)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setCommandTypes_), commandTypes);
}

// property: inheritPipelineState
_MTL_INLINE bool MTL::IndirectCommandBufferDescriptor::inheritPipelineState() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(inheritPipelineState));
}

_MTL_INLINE void MTL::IndirectCommandBufferDescriptor::setInheritPipelineState(bool inheritPipelineState)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setInheritPipelineState_), inheritPipelineState);
}

// property: inheritBuffers
_MTL_INLINE bool MTL::IndirectCommandBufferDescriptor::inheritBuffers() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(inheritBuffers));
}

_MTL_INLINE void MTL::IndirectCommandBufferDescriptor::setInheritBuffers(bool inheritBuffers)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setInheritBuffers_), inheritBuffers);
}

// property: maxVertexBufferBindCount
_MTL_INLINE NS::UInteger MTL::IndirectCommandBufferDescriptor::maxVertexBufferBindCount() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(maxVertexBufferBindCount));
}

_MTL_INLINE void MTL::IndirectCommandBufferDescriptor::setMaxVertexBufferBindCount(NS::UInteger maxVertexBufferBindCount)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setMaxVertexBufferBindCount_), maxVertexBufferBindCount);
}

// property: maxFragmentBufferBindCount
_MTL_INLINE NS::UInteger MTL::IndirectCommandBufferDescriptor::maxFragmentBufferBindCount() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(maxFragmentBufferBindCount));
}

_MTL_INLINE void MTL::IndirectCommandBufferDescriptor::setMaxFragmentBufferBindCount(NS::UInteger maxFragmentBufferBindCount)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setMaxFragmentBufferBindCount_), maxFragmentBufferBindCount);
}

// property: maxKernelBufferBindCount
_MTL_INLINE NS::UInteger MTL::IndirectCommandBufferDescriptor::maxKernelBufferBindCount() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(maxKernelBufferBindCount));
}

_MTL_INLINE void MTL::IndirectCommandBufferDescriptor::setMaxKernelBufferBindCount(NS::UInteger maxKernelBufferBindCount)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setMaxKernelBufferBindCount_), maxKernelBufferBindCount);
}

// property: size
_MTL_INLINE NS::UInteger MTL::IndirectCommandBuffer::size() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(size));
}

// method: resetWithRange:
_MTL_INLINE void MTL::IndirectCommandBuffer::reset(NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(resetWithRange_), range);
}

// method: indirectRenderCommandAtIndex:
_MTL_INLINE MTL::IndirectRenderCommand* MTL::IndirectCommandBuffer::indirectRenderCommand(NS::UInteger commandIndex)
{
    return Object::sendMessage<MTL::IndirectRenderCommand*>(this, _MTL_PRIVATE_SEL(indirectRenderCommandAtIndex_), commandIndex);
}

// method: indirectComputeCommandAtIndex:
_MTL_INLINE MTL::IndirectComputeCommand* MTL::IndirectCommandBuffer::indirectComputeCommand(NS::UInteger commandIndex)
{
    return Object::sendMessage<MTL::IndirectComputeCommand*>(this, _MTL_PRIVATE_SEL(indirectComputeCommandAtIndex_), commandIndex);
}
