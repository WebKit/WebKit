//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLResourceStatePass.hpp
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
class ResourceStatePassSampleBufferAttachmentDescriptor : public NS::Copying<ResourceStatePassSampleBufferAttachmentDescriptor>
{
public:
    static class ResourceStatePassSampleBufferAttachmentDescriptor* alloc();

    class ResourceStatePassSampleBufferAttachmentDescriptor*        init();

    class CounterSampleBuffer*                                      sampleBuffer() const;
    void                                                            setSampleBuffer(const class CounterSampleBuffer* sampleBuffer);

    NS::UInteger                                                    startOfEncoderSampleIndex() const;
    void                                                            setStartOfEncoderSampleIndex(NS::UInteger startOfEncoderSampleIndex);

    NS::UInteger                                                    endOfEncoderSampleIndex() const;
    void                                                            setEndOfEncoderSampleIndex(NS::UInteger endOfEncoderSampleIndex);
};

class ResourceStatePassSampleBufferAttachmentDescriptorArray : public NS::Referencing<ResourceStatePassSampleBufferAttachmentDescriptorArray>
{
public:
    static class ResourceStatePassSampleBufferAttachmentDescriptorArray* alloc();

    class ResourceStatePassSampleBufferAttachmentDescriptorArray*        init();

    class ResourceStatePassSampleBufferAttachmentDescriptor*             object(NS::UInteger attachmentIndex);

    void                                                                 setObject(const class ResourceStatePassSampleBufferAttachmentDescriptor* attachment, NS::UInteger attachmentIndex);
};

class ResourceStatePassDescriptor : public NS::Copying<ResourceStatePassDescriptor>
{
public:
    static class ResourceStatePassDescriptor*                     alloc();

    class ResourceStatePassDescriptor*                            init();

    static class ResourceStatePassDescriptor*                     resourceStatePassDescriptor();

    class ResourceStatePassSampleBufferAttachmentDescriptorArray* sampleBufferAttachments() const;
};

}

// static method: alloc
_MTL_INLINE MTL::ResourceStatePassSampleBufferAttachmentDescriptor* MTL::ResourceStatePassSampleBufferAttachmentDescriptor::alloc()
{
    return NS::Object::alloc<MTL::ResourceStatePassSampleBufferAttachmentDescriptor>(_MTL_PRIVATE_CLS(MTLResourceStatePassSampleBufferAttachmentDescriptor));
}

// method: init
_MTL_INLINE MTL::ResourceStatePassSampleBufferAttachmentDescriptor* MTL::ResourceStatePassSampleBufferAttachmentDescriptor::init()
{
    return NS::Object::init<MTL::ResourceStatePassSampleBufferAttachmentDescriptor>();
}

// property: sampleBuffer
_MTL_INLINE MTL::CounterSampleBuffer* MTL::ResourceStatePassSampleBufferAttachmentDescriptor::sampleBuffer() const
{
    return Object::sendMessage<MTL::CounterSampleBuffer*>(this, _MTL_PRIVATE_SEL(sampleBuffer));
}

_MTL_INLINE void MTL::ResourceStatePassSampleBufferAttachmentDescriptor::setSampleBuffer(const MTL::CounterSampleBuffer* sampleBuffer)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSampleBuffer_), sampleBuffer);
}

// property: startOfEncoderSampleIndex
_MTL_INLINE NS::UInteger MTL::ResourceStatePassSampleBufferAttachmentDescriptor::startOfEncoderSampleIndex() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(startOfEncoderSampleIndex));
}

_MTL_INLINE void MTL::ResourceStatePassSampleBufferAttachmentDescriptor::setStartOfEncoderSampleIndex(NS::UInteger startOfEncoderSampleIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStartOfEncoderSampleIndex_), startOfEncoderSampleIndex);
}

// property: endOfEncoderSampleIndex
_MTL_INLINE NS::UInteger MTL::ResourceStatePassSampleBufferAttachmentDescriptor::endOfEncoderSampleIndex() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(endOfEncoderSampleIndex));
}

_MTL_INLINE void MTL::ResourceStatePassSampleBufferAttachmentDescriptor::setEndOfEncoderSampleIndex(NS::UInteger endOfEncoderSampleIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setEndOfEncoderSampleIndex_), endOfEncoderSampleIndex);
}

// static method: alloc
_MTL_INLINE MTL::ResourceStatePassSampleBufferAttachmentDescriptorArray* MTL::ResourceStatePassSampleBufferAttachmentDescriptorArray::alloc()
{
    return NS::Object::alloc<MTL::ResourceStatePassSampleBufferAttachmentDescriptorArray>(_MTL_PRIVATE_CLS(MTLResourceStatePassSampleBufferAttachmentDescriptorArray));
}

// method: init
_MTL_INLINE MTL::ResourceStatePassSampleBufferAttachmentDescriptorArray* MTL::ResourceStatePassSampleBufferAttachmentDescriptorArray::init()
{
    return NS::Object::init<MTL::ResourceStatePassSampleBufferAttachmentDescriptorArray>();
}

// method: objectAtIndexedSubscript:
_MTL_INLINE MTL::ResourceStatePassSampleBufferAttachmentDescriptor* MTL::ResourceStatePassSampleBufferAttachmentDescriptorArray::object(NS::UInteger attachmentIndex)
{
    return Object::sendMessage<MTL::ResourceStatePassSampleBufferAttachmentDescriptor*>(this, _MTL_PRIVATE_SEL(objectAtIndexedSubscript_), attachmentIndex);
}

// method: setObject:atIndexedSubscript:
_MTL_INLINE void MTL::ResourceStatePassSampleBufferAttachmentDescriptorArray::setObject(const MTL::ResourceStatePassSampleBufferAttachmentDescriptor* attachment, NS::UInteger attachmentIndex)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setObject_atIndexedSubscript_), attachment, attachmentIndex);
}

// static method: alloc
_MTL_INLINE MTL::ResourceStatePassDescriptor* MTL::ResourceStatePassDescriptor::alloc()
{
    return NS::Object::alloc<MTL::ResourceStatePassDescriptor>(_MTL_PRIVATE_CLS(MTLResourceStatePassDescriptor));
}

// method: init
_MTL_INLINE MTL::ResourceStatePassDescriptor* MTL::ResourceStatePassDescriptor::init()
{
    return NS::Object::init<MTL::ResourceStatePassDescriptor>();
}

// static method: resourceStatePassDescriptor
_MTL_INLINE MTL::ResourceStatePassDescriptor* MTL::ResourceStatePassDescriptor::resourceStatePassDescriptor()
{
    return Object::sendMessage<MTL::ResourceStatePassDescriptor*>(_MTL_PRIVATE_CLS(MTLResourceStatePassDescriptor), _MTL_PRIVATE_SEL(resourceStatePassDescriptor));
}

// property: sampleBufferAttachments
_MTL_INLINE MTL::ResourceStatePassSampleBufferAttachmentDescriptorArray* MTL::ResourceStatePassDescriptor::sampleBufferAttachments() const
{
    return Object::sendMessage<MTL::ResourceStatePassSampleBufferAttachmentDescriptorArray*>(this, _MTL_PRIVATE_SEL(sampleBufferAttachments));
}
