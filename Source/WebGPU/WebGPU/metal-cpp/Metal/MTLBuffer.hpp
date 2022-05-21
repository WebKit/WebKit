//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLBuffer.hpp
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

#include "MTLResource.hpp"

namespace MTL
{
class Buffer : public NS::Referencing<Buffer, Resource>
{
public:
    NS::UInteger   length() const;

    void*          contents();

    void           didModifyRange(NS::Range range);

    class Texture* newTexture(const class TextureDescriptor* descriptor, NS::UInteger offset, NS::UInteger bytesPerRow);

    void           addDebugMarker(const NS::String* marker, NS::Range range);

    void           removeAllDebugMarkers();

    class Buffer*  remoteStorageBuffer() const;

    class Buffer*  newRemoteBufferViewForDevice(const class Device* device);
};

}

// property: length
_MTL_INLINE NS::UInteger MTL::Buffer::length() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(length));
}

// method: contents
_MTL_INLINE void* MTL::Buffer::contents()
{
    return Object::sendMessage<void*>(this, _MTL_PRIVATE_SEL(contents));
}

// method: didModifyRange:
_MTL_INLINE void MTL::Buffer::didModifyRange(NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(didModifyRange_), range);
}

// method: newTextureWithDescriptor:offset:bytesPerRow:
_MTL_INLINE MTL::Texture* MTL::Buffer::newTexture(const MTL::TextureDescriptor* descriptor, NS::UInteger offset, NS::UInteger bytesPerRow)
{
    return Object::sendMessage<MTL::Texture*>(this, _MTL_PRIVATE_SEL(newTextureWithDescriptor_offset_bytesPerRow_), descriptor, offset, bytesPerRow);
}

// method: addDebugMarker:range:
_MTL_INLINE void MTL::Buffer::addDebugMarker(const NS::String* marker, NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(addDebugMarker_range_), marker, range);
}

// method: removeAllDebugMarkers
_MTL_INLINE void MTL::Buffer::removeAllDebugMarkers()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(removeAllDebugMarkers));
}

// property: remoteStorageBuffer
_MTL_INLINE MTL::Buffer* MTL::Buffer::remoteStorageBuffer() const
{
    return Object::sendMessage<MTL::Buffer*>(this, _MTL_PRIVATE_SEL(remoteStorageBuffer));
}

// method: newRemoteBufferViewForDevice:
_MTL_INLINE MTL::Buffer* MTL::Buffer::newRemoteBufferViewForDevice(const MTL::Device* device)
{
    return Object::sendMessage<MTL::Buffer*>(this, _MTL_PRIVATE_SEL(newRemoteBufferViewForDevice_), device);
}
