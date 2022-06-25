//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLCommandQueue.hpp
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

namespace MTL
{
class CommandQueue : public NS::Referencing<CommandQueue>
{
public:
    NS::String*          label() const;
    void                 setLabel(const NS::String* label);

    class Device*        device() const;

    class CommandBuffer* commandBuffer();

    class CommandBuffer* commandBuffer(const class CommandBufferDescriptor* descriptor);

    class CommandBuffer* commandBufferWithUnretainedReferences();

    void                 insertDebugCaptureBoundary();
};

}

// property: label
_MTL_INLINE NS::String* MTL::CommandQueue::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

_MTL_INLINE void MTL::CommandQueue::setLabel(const NS::String* label)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLabel_), label);
}

// property: device
_MTL_INLINE MTL::Device* MTL::CommandQueue::device() const
{
    return Object::sendMessage<MTL::Device*>(this, _MTL_PRIVATE_SEL(device));
}

// method: commandBuffer
_MTL_INLINE MTL::CommandBuffer* MTL::CommandQueue::commandBuffer()
{
    return Object::sendMessage<MTL::CommandBuffer*>(this, _MTL_PRIVATE_SEL(commandBuffer));
}

// method: commandBufferWithDescriptor:
_MTL_INLINE MTL::CommandBuffer* MTL::CommandQueue::commandBuffer(const MTL::CommandBufferDescriptor* descriptor)
{
    return Object::sendMessage<MTL::CommandBuffer*>(this, _MTL_PRIVATE_SEL(commandBufferWithDescriptor_), descriptor);
}

// method: commandBufferWithUnretainedReferences
_MTL_INLINE MTL::CommandBuffer* MTL::CommandQueue::commandBufferWithUnretainedReferences()
{
    return Object::sendMessage<MTL::CommandBuffer*>(this, _MTL_PRIVATE_SEL(commandBufferWithUnretainedReferences));
}

// method: insertDebugCaptureBoundary
_MTL_INLINE void MTL::CommandQueue::insertDebugCaptureBoundary()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(insertDebugCaptureBoundary));
}
