//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLComputePass.hpp
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

namespace MTL
{
class ComputePassSampleBufferAttachmentDescriptor : public NS::Copying<ComputePassSampleBufferAttachmentDescriptor>
{
public:
    static class ComputePassSampleBufferAttachmentDescriptor* alloc();

    class ComputePassSampleBufferAttachmentDescriptor*        init();

    class CounterSampleBuffer*                                sampleBuffer() const;
    void                                                      setSampleBuffer(const class CounterSampleBuffer* sampleBuffer);

    NS::UInteger                                              startOfEncoderSampleIndex() const;
    void                                                      setStartOfEncoderSampleIndex(NS::UInteger startOfEncoderSampleIndex);

    NS::UInteger                                              endOfEncoderSampleIndex() const;
    void                                                      setEndOfEncoderSampleIndex(NS::UInteger endOfEncoderSampleIndex);
};

class ComputePassSampleBufferAttachmentDescriptorArray : public NS::Referencing<ComputePassSampleBufferAttachmentDescriptorArray>
{
public:
    static class ComputePassSampleBufferAttachmentDescriptorArray* alloc();

    class ComputePassSampleBufferAttachmentDescriptorArray*        init();

    class ComputePassSampleBufferAttachmentDescriptor*             object(NS::UInteger attachmentIndex);

    void                                                           setObject(const class ComputePassSampleBufferAttachmentDescriptor* attachment, NS::UInteger attachmentIndex);
};

class ComputePassDescriptor : public NS::Copying<ComputePassDescriptor>
{
public:
    static class ComputePassDescriptor*                     alloc();

    class ComputePassDescriptor*                            init();

    static class ComputePassDescriptor*                     computePassDescriptor();

    MTL::DispatchType                                       dispatchType() const;
    void                                                    setDispatchType(MTL::DispatchType dispatchType);

    class ComputePassSampleBufferAttachmentDescriptorArray* sampleBufferAttachments() const;
};

}

// static method: alloc
_MTL_INLINE MTL::ComputePassSampleBufferAttachmentDescriptor* MTL::ComputePassSampleBufferAttachmentDescriptor::alloc()
{
    return NS::Object::alloc<MTL::ComputePassSampleBufferAttachmentDescriptor>(_MTL_PRIVATE_CLS(MTLComputePassSampleBufferAttachmentDescriptor));
}

// method: init
_MTL_INLINE MTL::ComputePassSampleBufferAttachmentDescriptor* MTL::ComputePassSampleBufferAttachmentDescriptor::init()
{
    return NS::Object::init<MTL::ComputePassSampleBufferAttachmentDescriptor>();
}

// property: sampleBuffer
_MTL_INLINE MTL::CounterSampleBuffer* MTL::ComputePassSampleBufferAttachmentDescriptor::sampleBuffer() const
{
    return Object::sendMessage<MTL::CounterSampleBuffer*>(this, _MTL_PRIVATE_SEL(sampleBuffer));
}

_MTL_INLINE void MTL::ComputePassSampleBufferAttachmentDescriptor::setSampleBuffer(const MTL::CounterSampleBuffer* sampleBuffer)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSampleBuffer_), sampleBuffer);
}

// property: startOfEncoderSampleIndex
_MTL_INLINE NS::UInteger MTL::ComputePassSampleBufferAttachmentDescriptor::startOfEncoderSampleIndex() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(startOfEncoderSampleIndex));
}

_MTL_INLINE void MTL::ComputePassSampleBufferAttachmentDescriptor::setStartOfEncoderSampleIndex(NS::UInteger startOfEncoderSampleIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStartOfEncoderSampleIndex_), startOfEncoderSampleIndex);
}

// property: endOfEncoderSampleIndex
_MTL_INLINE NS::UInteger MTL::ComputePassSampleBufferAttachmentDescriptor::endOfEncoderSampleIndex() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(endOfEncoderSampleIndex));
}

_MTL_INLINE void MTL::ComputePassSampleBufferAttachmentDescriptor::setEndOfEncoderSampleIndex(NS::UInteger endOfEncoderSampleIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setEndOfEncoderSampleIndex_), endOfEncoderSampleIndex);
}

// static method: alloc
_MTL_INLINE MTL::ComputePassSampleBufferAttachmentDescriptorArray* MTL::ComputePassSampleBufferAttachmentDescriptorArray::alloc()
{
    return NS::Object::alloc<MTL::ComputePassSampleBufferAttachmentDescriptorArray>(_MTL_PRIVATE_CLS(MTLComputePassSampleBufferAttachmentDescriptorArray));
}

// method: init
_MTL_INLINE MTL::ComputePassSampleBufferAttachmentDescriptorArray* MTL::ComputePassSampleBufferAttachmentDescriptorArray::init()
{
    return NS::Object::init<MTL::ComputePassSampleBufferAttachmentDescriptorArray>();
}

// method: objectAtIndexedSubscript:
_MTL_INLINE MTL::ComputePassSampleBufferAttachmentDescriptor* MTL::ComputePassSampleBufferAttachmentDescriptorArray::object(NS::UInteger attachmentIndex)
{
    return Object::sendMessage<MTL::ComputePassSampleBufferAttachmentDescriptor*>(this, _MTL_PRIVATE_SEL(objectAtIndexedSubscript_), attachmentIndex);
}

// method: setObject:atIndexedSubscript:
_MTL_INLINE void MTL::ComputePassSampleBufferAttachmentDescriptorArray::setObject(const MTL::ComputePassSampleBufferAttachmentDescriptor* attachment, NS::UInteger attachmentIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setObject_atIndexedSubscript_), attachment, attachmentIndex);
}

// static method: alloc
_MTL_INLINE MTL::ComputePassDescriptor* MTL::ComputePassDescriptor::alloc()
{
    return NS::Object::alloc<MTL::ComputePassDescriptor>(_MTL_PRIVATE_CLS(MTLComputePassDescriptor));
}

// method: init
_MTL_INLINE MTL::ComputePassDescriptor* MTL::ComputePassDescriptor::init()
{
    return NS::Object::init<MTL::ComputePassDescriptor>();
}

// static method: computePassDescriptor
_MTL_INLINE MTL::ComputePassDescriptor* MTL::ComputePassDescriptor::computePassDescriptor()
{
    return Object::sendMessage<MTL::ComputePassDescriptor*>(_MTL_PRIVATE_CLS(MTLComputePassDescriptor), _MTL_PRIVATE_SEL(computePassDescriptor));
}

// property: dispatchType
_MTL_INLINE MTL::DispatchType MTL::ComputePassDescriptor::dispatchType() const
{
    return Object::sendMessage<MTL::DispatchType>(this, _MTL_PRIVATE_SEL(dispatchType));
}

_MTL_INLINE void MTL::ComputePassDescriptor::setDispatchType(MTL::DispatchType dispatchType)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setDispatchType_), dispatchType);
}

// property: sampleBufferAttachments
_MTL_INLINE MTL::ComputePassSampleBufferAttachmentDescriptorArray* MTL::ComputePassDescriptor::sampleBufferAttachments() const
{
    return Object::sendMessage<MTL::ComputePassSampleBufferAttachmentDescriptorArray*>(this, _MTL_PRIVATE_SEL(sampleBufferAttachments));
}
