//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLIOCommandQueue.hpp
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

#include "MTLIOCommandQueue.hpp"

namespace MTL
{
_MTL_ENUM(NS::Integer, IOPriority) {
    IOPriorityHigh = 0,
    IOPriorityNormal = 1,
    IOPriorityLow = 2,
};

_MTL_ENUM(NS::Integer, IOCommandQueueType) {
    IOCommandQueueTypeConcurrent = 0,
    IOCommandQueueTypeSerial = 1,
};

_MTL_CONST(NS::ErrorDomain, IOErrorDomain);

_MTL_ENUM(NS::Integer, IOError) {
    IOErrorURLInvalid = 1,
    IOErrorInternal = 2,
};

class IOCommandQueue : public NS::Referencing<IOCommandQueue>
{
public:
    void                   enqueueBarrier();

    class IOCommandBuffer* commandBuffer();

    class IOCommandBuffer* commandBufferWithUnretainedReferences();

    NS::String*            label() const;
    void                   setLabel(const NS::String* label);
};

class IOScratchBuffer : public NS::Referencing<IOScratchBuffer>
{
public:
    class Buffer* buffer() const;
};

class IOScratchBufferAllocator : public NS::Referencing<IOScratchBufferAllocator>
{
public:
    class IOScratchBuffer* newScratchBuffer(NS::UInteger minimumSize);

    class IOScratchBuffer* allocateScratchBuffer(NS::UInteger minimumSize);
};

class IOCommandQueueDescriptor : public NS::Copying<IOCommandQueueDescriptor>
{
public:
    static class IOCommandQueueDescriptor* alloc();

    class IOCommandQueueDescriptor*        init();

    NS::UInteger                           maxCommandBufferCount() const;
    void                                   setMaxCommandBufferCount(NS::UInteger maxCommandBufferCount);

    MTL::IOPriority                        priority() const;
    void                                   setPriority(MTL::IOPriority priority);

    MTL::IOCommandQueueType                type() const;
    void                                   setType(MTL::IOCommandQueueType type);

    NS::UInteger                           maxCommandsInFlight() const;
    void                                   setMaxCommandsInFlight(NS::UInteger maxCommandsInFlight);

    class IOScratchBufferAllocator*        scratchBufferAllocator() const;
    void                                   setScratchBufferAllocator(const class IOScratchBufferAllocator* scratchBufferAllocator);
};

class IOFileHandle : public NS::Referencing<IOFileHandle>
{
public:
    NS::String* label() const;
    void        setLabel(const NS::String* label);
};

}

// method: enqueueBarrier
_MTL_INLINE void MTL::IOCommandQueue::enqueueBarrier()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(enqueueBarrier));
}

// method: commandBuffer
_MTL_INLINE MTL::IOCommandBuffer* MTL::IOCommandQueue::commandBuffer()
{
    return Object::sendMessage<MTL::IOCommandBuffer*>(this, _MTL_PRIVATE_SEL(commandBuffer));
}

// method: commandBufferWithUnretainedReferences
_MTL_INLINE MTL::IOCommandBuffer* MTL::IOCommandQueue::commandBufferWithUnretainedReferences()
{
    return Object::sendMessage<MTL::IOCommandBuffer*>(this, _MTL_PRIVATE_SEL(commandBufferWithUnretainedReferences));
}

// property: label
_MTL_INLINE NS::String* MTL::IOCommandQueue::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

_MTL_INLINE void MTL::IOCommandQueue::setLabel(const NS::String* label)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLabel_), label);
}

// property: buffer
_MTL_INLINE MTL::Buffer* MTL::IOScratchBuffer::buffer() const
{
    return Object::sendMessage<MTL::Buffer*>(this, _MTL_PRIVATE_SEL(buffer));
}

// method: newScratchBufferWithMinimumSize:
_MTL_INLINE MTL::IOScratchBuffer* MTL::IOScratchBufferAllocator::newScratchBuffer(NS::UInteger minimumSize)
{
    return Object::sendMessage<MTL::IOScratchBuffer*>(this, _MTL_PRIVATE_SEL(newScratchBufferWithMinimumSize_), minimumSize);
}

// method: allocateScratchBufferWithMinimumSize:
_MTL_INLINE MTL::IOScratchBuffer* MTL::IOScratchBufferAllocator::allocateScratchBuffer(NS::UInteger minimumSize)
{
    return Object::sendMessage<MTL::IOScratchBuffer*>(this, _MTL_PRIVATE_SEL(allocateScratchBufferWithMinimumSize_), minimumSize);
}

// static method: alloc
_MTL_INLINE MTL::IOCommandQueueDescriptor* MTL::IOCommandQueueDescriptor::alloc()
{
    return NS::Object::alloc<MTL::IOCommandQueueDescriptor>(_MTL_PRIVATE_CLS(MTLIOCommandQueueDescriptor));
}

// method: init
_MTL_INLINE MTL::IOCommandQueueDescriptor* MTL::IOCommandQueueDescriptor::init()
{
    return NS::Object::init<MTL::IOCommandQueueDescriptor>();
}

// property: maxCommandBufferCount
_MTL_INLINE NS::UInteger MTL::IOCommandQueueDescriptor::maxCommandBufferCount() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(maxCommandBufferCount));
}

_MTL_INLINE void MTL::IOCommandQueueDescriptor::setMaxCommandBufferCount(NS::UInteger maxCommandBufferCount)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setMaxCommandBufferCount_), maxCommandBufferCount);
}

// property: priority
_MTL_INLINE MTL::IOPriority MTL::IOCommandQueueDescriptor::priority() const
{
    return Object::sendMessage<MTL::IOPriority>(this, _MTL_PRIVATE_SEL(priority));
}

_MTL_INLINE void MTL::IOCommandQueueDescriptor::setPriority(MTL::IOPriority priority)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setPriority_), priority);
}

// property: type
_MTL_INLINE MTL::IOCommandQueueType MTL::IOCommandQueueDescriptor::type() const
{
    return Object::sendMessage<MTL::IOCommandQueueType>(this, _MTL_PRIVATE_SEL(type));
}

_MTL_INLINE void MTL::IOCommandQueueDescriptor::setType(MTL::IOCommandQueueType type)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setType_), type);
}

// property: maxCommandsInFlight
_MTL_INLINE NS::UInteger MTL::IOCommandQueueDescriptor::maxCommandsInFlight() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(maxCommandsInFlight));
}

_MTL_INLINE void MTL::IOCommandQueueDescriptor::setMaxCommandsInFlight(NS::UInteger maxCommandsInFlight)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setMaxCommandsInFlight_), maxCommandsInFlight);
}

// property: scratchBufferAllocator
_MTL_INLINE MTL::IOScratchBufferAllocator* MTL::IOCommandQueueDescriptor::scratchBufferAllocator() const
{
    return Object::sendMessage<MTL::IOScratchBufferAllocator*>(this, _MTL_PRIVATE_SEL(scratchBufferAllocator));
}

_MTL_INLINE void MTL::IOCommandQueueDescriptor::setScratchBufferAllocator(const MTL::IOScratchBufferAllocator* scratchBufferAllocator)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setScratchBufferAllocator_), scratchBufferAllocator);
}

// property: label
_MTL_INLINE NS::String* MTL::IOFileHandle::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

_MTL_INLINE void MTL::IOFileHandle::setLabel(const NS::String* label)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLabel_), label);
}
