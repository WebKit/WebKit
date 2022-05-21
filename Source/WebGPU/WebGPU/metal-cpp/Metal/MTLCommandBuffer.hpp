//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLCommandBuffer.hpp
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

#include "MTLCommandBuffer.hpp"
#include <functional>

namespace MTL
{
_MTL_ENUM(NS::UInteger, CommandBufferStatus) {
    CommandBufferStatusNotEnqueued = 0,
    CommandBufferStatusEnqueued = 1,
    CommandBufferStatusCommitted = 2,
    CommandBufferStatusScheduled = 3,
    CommandBufferStatusCompleted = 4,
    CommandBufferStatusError = 5,
};

_MTL_ENUM(NS::UInteger, CommandBufferError) {
    CommandBufferErrorNone = 0,
    CommandBufferErrorTimeout = 2,
    CommandBufferErrorPageFault = 3,
    CommandBufferErrorAccessRevoked = 4,
    CommandBufferErrorBlacklisted = 4,
    CommandBufferErrorNotPermitted = 7,
    CommandBufferErrorOutOfMemory = 8,
    CommandBufferErrorInvalidResource = 9,
    CommandBufferErrorMemoryless = 10,
    CommandBufferErrorDeviceRemoved = 11,
    CommandBufferErrorStackOverflow = 12,
};

_MTL_OPTIONS(NS::UInteger, CommandBufferErrorOption) {
    CommandBufferErrorOptionNone = 0,
    CommandBufferErrorOptionEncoderExecutionStatus = 1,
};

_MTL_ENUM(NS::Integer, CommandEncoderErrorState) {
    CommandEncoderErrorStateUnknown = 0,
    CommandEncoderErrorStateCompleted = 1,
    CommandEncoderErrorStateAffected = 2,
    CommandEncoderErrorStatePending = 3,
    CommandEncoderErrorStateFaulted = 4,
};

class CommandBufferDescriptor : public NS::Copying<CommandBufferDescriptor>
{
public:
    static class CommandBufferDescriptor* alloc();

    class CommandBufferDescriptor*        init();

    bool                                  retainedReferences() const;
    void                                  setRetainedReferences(bool retainedReferences);

    MTL::CommandBufferErrorOption         errorOptions() const;
    void                                  setErrorOptions(MTL::CommandBufferErrorOption errorOptions);
};

class CommandBufferEncoderInfo : public NS::Referencing<CommandBufferEncoderInfo>
{
public:
    NS::String*                   label() const;

    NS::Array*                    debugSignposts() const;

    MTL::CommandEncoderErrorState errorState() const;
};

_MTL_ENUM(NS::UInteger, DispatchType) {
    DispatchTypeSerial = 0,
    DispatchTypeConcurrent = 1,
};

class CommandBuffer;

using CommandBufferHandler = void (^)(CommandBuffer*);

using HandlerFunction = std::function<void(CommandBuffer*)>;

class CommandBuffer : public NS::Referencing<CommandBuffer>
{
public:
    void                                       addScheduledHandler(const HandlerFunction& function);

    void                                       addCompletedHandler(const HandlerFunction& function);

    class Device*                              device() const;

    class CommandQueue*                        commandQueue() const;

    bool                                       retainedReferences() const;

    MTL::CommandBufferErrorOption              errorOptions() const;

    NS::String*                                label() const;
    void                                       setLabel(const NS::String* label);

    CFTimeInterval                             kernelStartTime() const;

    CFTimeInterval                             kernelEndTime() const;

    class LogContainer*                        logs() const;

    CFTimeInterval                             GPUStartTime() const;

    CFTimeInterval                             GPUEndTime() const;

    void                                       enqueue();

    void                                       commit();

    void                                       addScheduledHandler(const MTL::CommandBufferHandler block);

    void                                       presentDrawable(const class Drawable* drawable);

    void                                       presentDrawableAtTime(const class Drawable* drawable, CFTimeInterval presentationTime);

    void                                       presentDrawableAfterMinimumDuration(const class Drawable* drawable, CFTimeInterval duration);

    void                                       waitUntilScheduled();

    void                                       addCompletedHandler(const MTL::CommandBufferHandler block);

    void                                       waitUntilCompleted();

    MTL::CommandBufferStatus                   status() const;

    NS::Error*                                 error() const;

    class BlitCommandEncoder*                  blitCommandEncoder();

    class RenderCommandEncoder*                renderCommandEncoder(const class RenderPassDescriptor* renderPassDescriptor);

    class ComputeCommandEncoder*               computeCommandEncoder(const class ComputePassDescriptor* computePassDescriptor);

    class BlitCommandEncoder*                  blitCommandEncoder(const class BlitPassDescriptor* blitPassDescriptor);

    class ComputeCommandEncoder*               computeCommandEncoder();

    class ComputeCommandEncoder*               computeCommandEncoder(MTL::DispatchType dispatchType);

    void                                       encodeWait(const class Event* event, uint64_t value);

    void                                       encodeSignalEvent(const class Event* event, uint64_t value);

    class ParallelRenderCommandEncoder*        parallelRenderCommandEncoder(const class RenderPassDescriptor* renderPassDescriptor);

    class ResourceStateCommandEncoder*         resourceStateCommandEncoder();

    class ResourceStateCommandEncoder*         resourceStateCommandEncoder(const class ResourceStatePassDescriptor* resourceStatePassDescriptor);

    class AccelerationStructureCommandEncoder* accelerationStructureCommandEncoder();

    void                                       pushDebugGroup(const NS::String* string);

    void                                       popDebugGroup();
};

}

// static method: alloc
_MTL_INLINE MTL::CommandBufferDescriptor* MTL::CommandBufferDescriptor::alloc()
{
    return NS::Object::alloc<MTL::CommandBufferDescriptor>(_MTL_PRIVATE_CLS(MTLCommandBufferDescriptor));
}

// method: init
_MTL_INLINE MTL::CommandBufferDescriptor* MTL::CommandBufferDescriptor::init()
{
    return NS::Object::init<MTL::CommandBufferDescriptor>();
}

// property: retainedReferences
_MTL_INLINE bool MTL::CommandBufferDescriptor::retainedReferences() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(retainedReferences));
}

_MTL_INLINE void MTL::CommandBufferDescriptor::setRetainedReferences(bool retainedReferences)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setRetainedReferences_), retainedReferences);
}

// property: errorOptions
_MTL_INLINE MTL::CommandBufferErrorOption MTL::CommandBufferDescriptor::errorOptions() const
{
    return Object::sendMessage<MTL::CommandBufferErrorOption>(this, _MTL_PRIVATE_SEL(errorOptions));
}

_MTL_INLINE void MTL::CommandBufferDescriptor::setErrorOptions(MTL::CommandBufferErrorOption errorOptions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setErrorOptions_), errorOptions);
}

// property: label
_MTL_INLINE NS::String* MTL::CommandBufferEncoderInfo::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

// property: debugSignposts
_MTL_INLINE NS::Array* MTL::CommandBufferEncoderInfo::debugSignposts() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(debugSignposts));
}

// property: errorState
_MTL_INLINE MTL::CommandEncoderErrorState MTL::CommandBufferEncoderInfo::errorState() const
{
    return Object::sendMessage<MTL::CommandEncoderErrorState>(this, _MTL_PRIVATE_SEL(errorState));
}

_MTL_INLINE void MTL::CommandBuffer::addScheduledHandler(const HandlerFunction& function)
{
    __block HandlerFunction blockFunction = function;

    addScheduledHandler(^(MTL::CommandBuffer* pCommandBuffer) { blockFunction(pCommandBuffer); });
}

_MTL_INLINE void MTL::CommandBuffer::addCompletedHandler(const HandlerFunction& function)
{
    __block HandlerFunction blockFunction = function;

    addCompletedHandler(^(MTL::CommandBuffer* pCommandBuffer) { blockFunction(pCommandBuffer); });
}

// property: device
_MTL_INLINE MTL::Device* MTL::CommandBuffer::device() const
{
    return Object::sendMessage<MTL::Device*>(this, _MTL_PRIVATE_SEL(device));
}

// property: commandQueue
_MTL_INLINE MTL::CommandQueue* MTL::CommandBuffer::commandQueue() const
{
    return Object::sendMessage<MTL::CommandQueue*>(this, _MTL_PRIVATE_SEL(commandQueue));
}

// property: retainedReferences
_MTL_INLINE bool MTL::CommandBuffer::retainedReferences() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(retainedReferences));
}

// property: errorOptions
_MTL_INLINE MTL::CommandBufferErrorOption MTL::CommandBuffer::errorOptions() const
{
    return Object::sendMessage<MTL::CommandBufferErrorOption>(this, _MTL_PRIVATE_SEL(errorOptions));
}

// property: label
_MTL_INLINE NS::String* MTL::CommandBuffer::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

_MTL_INLINE void MTL::CommandBuffer::setLabel(const NS::String* label)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLabel_), label);
}

// property: kernelStartTime
_MTL_INLINE CFTimeInterval MTL::CommandBuffer::kernelStartTime() const
{
    return Object::sendMessage<CFTimeInterval>(this, _MTL_PRIVATE_SEL(kernelStartTime));
}

// property: kernelEndTime
_MTL_INLINE CFTimeInterval MTL::CommandBuffer::kernelEndTime() const
{
    return Object::sendMessage<CFTimeInterval>(this, _MTL_PRIVATE_SEL(kernelEndTime));
}

// property: logs
_MTL_INLINE MTL::LogContainer* MTL::CommandBuffer::logs() const
{
    return Object::sendMessage<MTL::LogContainer*>(this, _MTL_PRIVATE_SEL(logs));
}

// property: GPUStartTime
_MTL_INLINE CFTimeInterval MTL::CommandBuffer::GPUStartTime() const
{
    return Object::sendMessage<CFTimeInterval>(this, _MTL_PRIVATE_SEL(GPUStartTime));
}

// property: GPUEndTime
_MTL_INLINE CFTimeInterval MTL::CommandBuffer::GPUEndTime() const
{
    return Object::sendMessage<CFTimeInterval>(this, _MTL_PRIVATE_SEL(GPUEndTime));
}

// method: enqueue
_MTL_INLINE void MTL::CommandBuffer::enqueue()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(enqueue));
}

// method: commit
_MTL_INLINE void MTL::CommandBuffer::commit()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(commit));
}

// method: addScheduledHandler:
_MTL_INLINE void MTL::CommandBuffer::addScheduledHandler(const MTL::CommandBufferHandler block)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(addScheduledHandler_), block);
}

// method: presentDrawable:
_MTL_INLINE void MTL::CommandBuffer::presentDrawable(const MTL::Drawable* drawable)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(presentDrawable_), drawable);
}

// method: presentDrawable:atTime:
_MTL_INLINE void MTL::CommandBuffer::presentDrawableAtTime(const MTL::Drawable* drawable, CFTimeInterval presentationTime)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(presentDrawable_atTime_), drawable, presentationTime);
}

// method: presentDrawable:afterMinimumDuration:
_MTL_INLINE void MTL::CommandBuffer::presentDrawableAfterMinimumDuration(const MTL::Drawable* drawable, CFTimeInterval duration)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(presentDrawable_afterMinimumDuration_), drawable, duration);
}

// method: waitUntilScheduled
_MTL_INLINE void MTL::CommandBuffer::waitUntilScheduled()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(waitUntilScheduled));
}

// method: addCompletedHandler:
_MTL_INLINE void MTL::CommandBuffer::addCompletedHandler(const MTL::CommandBufferHandler block)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(addCompletedHandler_), block);
}

// method: waitUntilCompleted
_MTL_INLINE void MTL::CommandBuffer::waitUntilCompleted()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(waitUntilCompleted));
}

// property: status
_MTL_INLINE MTL::CommandBufferStatus MTL::CommandBuffer::status() const
{
    return Object::sendMessage<MTL::CommandBufferStatus>(this, _MTL_PRIVATE_SEL(status));
}

// property: error
_MTL_INLINE NS::Error* MTL::CommandBuffer::error() const
{
    return Object::sendMessage<NS::Error*>(this, _MTL_PRIVATE_SEL(error));
}

// method: blitCommandEncoder
_MTL_INLINE MTL::BlitCommandEncoder* MTL::CommandBuffer::blitCommandEncoder()
{
    return Object::sendMessage<MTL::BlitCommandEncoder*>(this, _MTL_PRIVATE_SEL(blitCommandEncoder));
}

// method: renderCommandEncoderWithDescriptor:
_MTL_INLINE MTL::RenderCommandEncoder* MTL::CommandBuffer::renderCommandEncoder(const MTL::RenderPassDescriptor* renderPassDescriptor)
{
    return Object::sendMessage<MTL::RenderCommandEncoder*>(this, _MTL_PRIVATE_SEL(renderCommandEncoderWithDescriptor_), renderPassDescriptor);
}

// method: computeCommandEncoderWithDescriptor:
_MTL_INLINE MTL::ComputeCommandEncoder* MTL::CommandBuffer::computeCommandEncoder(const MTL::ComputePassDescriptor* computePassDescriptor)
{
    return Object::sendMessage<MTL::ComputeCommandEncoder*>(this, _MTL_PRIVATE_SEL(computeCommandEncoderWithDescriptor_), computePassDescriptor);
}

// method: blitCommandEncoderWithDescriptor:
_MTL_INLINE MTL::BlitCommandEncoder* MTL::CommandBuffer::blitCommandEncoder(const MTL::BlitPassDescriptor* blitPassDescriptor)
{
    return Object::sendMessage<MTL::BlitCommandEncoder*>(this, _MTL_PRIVATE_SEL(blitCommandEncoderWithDescriptor_), blitPassDescriptor);
}

// method: computeCommandEncoder
_MTL_INLINE MTL::ComputeCommandEncoder* MTL::CommandBuffer::computeCommandEncoder()
{
    return Object::sendMessage<MTL::ComputeCommandEncoder*>(this, _MTL_PRIVATE_SEL(computeCommandEncoder));
}

// method: computeCommandEncoderWithDispatchType:
_MTL_INLINE MTL::ComputeCommandEncoder* MTL::CommandBuffer::computeCommandEncoder(MTL::DispatchType dispatchType)
{
    return Object::sendMessage<MTL::ComputeCommandEncoder*>(this, _MTL_PRIVATE_SEL(computeCommandEncoderWithDispatchType_), dispatchType);
}

// method: encodeWaitForEvent:value:
_MTL_INLINE void MTL::CommandBuffer::encodeWait(const MTL::Event* event, uint64_t value)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(encodeWaitForEvent_value_), event, value);
}

// method: encodeSignalEvent:value:
_MTL_INLINE void MTL::CommandBuffer::encodeSignalEvent(const MTL::Event* event, uint64_t value)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(encodeSignalEvent_value_), event, value);
}

// method: parallelRenderCommandEncoderWithDescriptor:
_MTL_INLINE MTL::ParallelRenderCommandEncoder* MTL::CommandBuffer::parallelRenderCommandEncoder(const MTL::RenderPassDescriptor* renderPassDescriptor)
{
    return Object::sendMessage<MTL::ParallelRenderCommandEncoder*>(this, _MTL_PRIVATE_SEL(parallelRenderCommandEncoderWithDescriptor_), renderPassDescriptor);
}

// method: resourceStateCommandEncoder
_MTL_INLINE MTL::ResourceStateCommandEncoder* MTL::CommandBuffer::resourceStateCommandEncoder()
{
    return Object::sendMessage<MTL::ResourceStateCommandEncoder*>(this, _MTL_PRIVATE_SEL(resourceStateCommandEncoder));
}

// method: resourceStateCommandEncoderWithDescriptor:
_MTL_INLINE MTL::ResourceStateCommandEncoder* MTL::CommandBuffer::resourceStateCommandEncoder(const MTL::ResourceStatePassDescriptor* resourceStatePassDescriptor)
{
    return Object::sendMessage<MTL::ResourceStateCommandEncoder*>(this, _MTL_PRIVATE_SEL(resourceStateCommandEncoderWithDescriptor_), resourceStatePassDescriptor);
}

// method: accelerationStructureCommandEncoder
_MTL_INLINE MTL::AccelerationStructureCommandEncoder* MTL::CommandBuffer::accelerationStructureCommandEncoder()
{
    return Object::sendMessage<MTL::AccelerationStructureCommandEncoder*>(this, _MTL_PRIVATE_SEL(accelerationStructureCommandEncoder));
}

// method: pushDebugGroup:
_MTL_INLINE void MTL::CommandBuffer::pushDebugGroup(const NS::String* string)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(pushDebugGroup_), string);
}

// method: popDebugGroup
_MTL_INLINE void MTL::CommandBuffer::popDebugGroup()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(popDebugGroup));
}
