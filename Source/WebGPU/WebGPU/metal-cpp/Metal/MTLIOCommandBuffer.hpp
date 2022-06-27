//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLIOCommandBuffer.hpp
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

#include "MTLIOCommandBuffer.hpp"
#include "MTLTypes.hpp"

namespace MTL
{
_MTL_ENUM(NS::Integer, IOStatus) {
    IOStatusPending = 0,
    IOStatusCancelled = 1,
    IOStatusError = 2,
    IOStatusComplete = 3,
};

using IOCommandBufferHandler = void (^)(class IOCommandBuffer*);

using IOCommandBufferHandlerFunction = std::function<void(class IOCommandBuffer*)>;

class IOCommandBuffer : public NS::Referencing<IOCommandBuffer>
{
public:
    void          addCompletedHandler(const MTL::IOCommandBufferHandlerFunction& function);

    void          addCompletedHandler(const MTL::IOCommandBufferHandler block);

    void          loadBytes(void* pointer, NS::UInteger size, const class IOFileHandle* sourceHandle, NS::UInteger sourceHandleOffset);

    void          loadBuffer(const class Buffer* buffer, NS::UInteger offset, NS::UInteger size, const class IOFileHandle* sourceHandle, NS::UInteger sourceHandleOffset);

    void          loadTexture(const class Texture* texture, NS::UInteger slice, NS::UInteger level, MTL::Size size, NS::UInteger sourceBytesPerRow, NS::UInteger sourceBytesPerImage, MTL::Origin destinationOrigin, const class IOFileHandle* sourceHandle, NS::UInteger sourceHandleOffset);

    void          copyStatusToBuffer(const class Buffer* buffer, NS::UInteger offset);

    void          commit();

    void          waitUntilCompleted();

    void          tryCancel();

    void          addBarrier();

    void          pushDebugGroup( const NS::String* string );

    void          popDebugGroup();

    void          enqueue();

    void          wait(const class SharedEvent* event, uint64_t value);

    void          signalEvent(const class SharedEvent* event, uint64_t value);

    NS::String*   label() const;
    void          setLabel(const NS::String* label);

    MTL::IOStatus status() const;

    NS::Error*    error() const;
};

}

_MTL_INLINE void MTL::IOCommandBuffer::addCompletedHandler(const MTL::IOCommandBufferHandlerFunction& function)
{
    __block IOCommandBufferHandlerFunction blockFunction = function;

    addCompletedHandler(^(IOCommandBuffer* pCommandBuffer) { blockFunction(pCommandBuffer); });
}

// method: addCompletedHandler:
_MTL_INLINE void MTL::IOCommandBuffer::addCompletedHandler(const MTL::IOCommandBufferHandler block)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(addCompletedHandler_), block);
}

// method: loadBytes:size:sourceHandle:sourceHandleOffset:
_MTL_INLINE void MTL::IOCommandBuffer::loadBytes(void* pointer, NS::UInteger size, const class IOFileHandle* sourceHandle, NS::UInteger sourceHandleOffset)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(loadBytes_size_sourceHandle_sourceHandleOffset_), pointer, size, sourceHandle, sourceHandleOffset);
}

// method: loadBuffer:offset:size:sourceHandle:sourceHandleOffset:
_MTL_INLINE void MTL::IOCommandBuffer::loadBuffer(const MTL::Buffer* buffer, NS::UInteger offset, NS::UInteger size, const MTL::IOFileHandle* sourceHandle, NS::UInteger sourceHandleOffset)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(loadBuffer_offset_size_sourceHandle_sourceHandleOffset_), buffer, offset, size, sourceHandle, sourceHandleOffset);
}

// method: loadTexture:slice:level:size:sourceBytesPerRow:sourceBytesPerImage:destinationOrigin:sourceHandle:sourceHandleOffset:
_MTL_INLINE void MTL::IOCommandBuffer::loadTexture(const MTL::Texture* texture, NS::UInteger slice, NS::UInteger level, MTL::Size size, NS::UInteger sourceBytesPerRow, NS::UInteger sourceBytesPerImage, MTL::Origin destinationOrigin, const MTL::IOFileHandle* sourceHandle, NS::UInteger sourceHandleOffset)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(loadTexture_slice_level_size_sourceBytesPerRow_sourceBytesPerImage_destinationOrigin_sourceHandle_sourceHandleOffset_), texture, slice, level, size, sourceBytesPerRow, sourceBytesPerImage, destinationOrigin, sourceHandle, sourceHandleOffset);
}

// method: copyStatusToBuffer:offset:
_MTL_INLINE void MTL::IOCommandBuffer::copyStatusToBuffer(const MTL::Buffer* buffer, NS::UInteger offset)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(copyStatusToBuffer_offset_), buffer, offset);
}

// method: commit
_MTL_INLINE void MTL::IOCommandBuffer::commit()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(commit));
}

// method: waitUntilCompleted
_MTL_INLINE void MTL::IOCommandBuffer::waitUntilCompleted()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(waitUntilCompleted));
}

// method: tryCancel
_MTL_INLINE void MTL::IOCommandBuffer::tryCancel()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(tryCancel));
}

// method: addBarrier
_MTL_INLINE void MTL::IOCommandBuffer::addBarrier()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(addBarrier));
}

// method: pushDebugGroup:
_MTL_INLINE void MTL::IOCommandBuffer::pushDebugGroup( const NS::String* string )
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(pushDebugGroup_), string);
}

// method: popDebugGroup:
_MTL_INLINE void MTL::IOCommandBuffer::popDebugGroup()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(popDebugGroup));
}

// method: enqueue
_MTL_INLINE void MTL::IOCommandBuffer::enqueue()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(enqueue));
}

// method: waitForEvent:value:
_MTL_INLINE void MTL::IOCommandBuffer::wait(const MTL::SharedEvent* event, uint64_t value)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(waitForEvent_value_), event, value);
}

// method: signalEvent:value:
_MTL_INLINE void MTL::IOCommandBuffer::signalEvent(const MTL::SharedEvent* event, uint64_t value)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(signalEvent_value_), event, value);
}

// property: label
_MTL_INLINE NS::String* MTL::IOCommandBuffer::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

_MTL_INLINE void MTL::IOCommandBuffer::setLabel(const NS::String* label)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLabel_), label);
}

// property: status
_MTL_INLINE MTL::IOStatus MTL::IOCommandBuffer::status() const
{
    return Object::sendMessage<MTL::IOStatus>(this, _MTL_PRIVATE_SEL(status));
}

// property: error
_MTL_INLINE NS::Error* MTL::IOCommandBuffer::error() const
{
    return Object::sendMessage<NS::Error*>(this, _MTL_PRIVATE_SEL(error));
}
