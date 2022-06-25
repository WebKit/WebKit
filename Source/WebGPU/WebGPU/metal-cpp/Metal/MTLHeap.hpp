//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLHeap.hpp
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

#include "MTLHeap.hpp"
#include "MTLResource.hpp"

namespace MTL
{
_MTL_ENUM(NS::Integer, HeapType) {
    HeapTypeAutomatic = 0,
    HeapTypePlacement = 1,
    HeapTypeSparse = 2,
};

class HeapDescriptor : public NS::Copying<HeapDescriptor>
{
public:
    static class HeapDescriptor* alloc();

    class HeapDescriptor*        init();

    NS::UInteger                 size() const;
    void                         setSize(NS::UInteger size);

    MTL::StorageMode             storageMode() const;
    void                         setStorageMode(MTL::StorageMode storageMode);

    MTL::CPUCacheMode            cpuCacheMode() const;
    void                         setCpuCacheMode(MTL::CPUCacheMode cpuCacheMode);

    MTL::HazardTrackingMode      hazardTrackingMode() const;
    void                         setHazardTrackingMode(MTL::HazardTrackingMode hazardTrackingMode);

    MTL::ResourceOptions         resourceOptions() const;
    void                         setResourceOptions(MTL::ResourceOptions resourceOptions);

    MTL::HeapType                type() const;
    void                         setType(MTL::HeapType type);
};

class Heap : public NS::Referencing<Heap>
{
public:
    NS::String*             label() const;
    void                    setLabel(const NS::String* label);

    class Device*           device() const;

    MTL::StorageMode        storageMode() const;

    MTL::CPUCacheMode       cpuCacheMode() const;

    MTL::HazardTrackingMode hazardTrackingMode() const;

    MTL::ResourceOptions    resourceOptions() const;

    NS::UInteger            size() const;

    NS::UInteger            usedSize() const;

    NS::UInteger            currentAllocatedSize() const;

    NS::UInteger            maxAvailableSize(NS::UInteger alignment);

    class Buffer*           newBuffer(NS::UInteger length, MTL::ResourceOptions options);

    class Texture*          newTexture(const class TextureDescriptor* desc);

    MTL::PurgeableState     setPurgeableState(MTL::PurgeableState state);

    MTL::HeapType           type() const;

    class Buffer*           newBuffer(NS::UInteger length, MTL::ResourceOptions options, NS::UInteger offset);

    class Texture*          newTexture(const class TextureDescriptor* descriptor, NS::UInteger offset);
};

}

// static method: alloc
_MTL_INLINE MTL::HeapDescriptor* MTL::HeapDescriptor::alloc()
{
    return NS::Object::alloc<MTL::HeapDescriptor>(_MTL_PRIVATE_CLS(MTLHeapDescriptor));
}

// method: init
_MTL_INLINE MTL::HeapDescriptor* MTL::HeapDescriptor::init()
{
    return NS::Object::init<MTL::HeapDescriptor>();
}

// property: size
_MTL_INLINE NS::UInteger MTL::HeapDescriptor::size() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(size));
}

_MTL_INLINE void MTL::HeapDescriptor::setSize(NS::UInteger size)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSize_), size);
}

// property: storageMode
_MTL_INLINE MTL::StorageMode MTL::HeapDescriptor::storageMode() const
{
    return Object::sendMessage<MTL::StorageMode>(this, _MTL_PRIVATE_SEL(storageMode));
}

_MTL_INLINE void MTL::HeapDescriptor::setStorageMode(MTL::StorageMode storageMode)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStorageMode_), storageMode);
}

// property: cpuCacheMode
_MTL_INLINE MTL::CPUCacheMode MTL::HeapDescriptor::cpuCacheMode() const
{
    return Object::sendMessage<MTL::CPUCacheMode>(this, _MTL_PRIVATE_SEL(cpuCacheMode));
}

_MTL_INLINE void MTL::HeapDescriptor::setCpuCacheMode(MTL::CPUCacheMode cpuCacheMode)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setCpuCacheMode_), cpuCacheMode);
}

// property: hazardTrackingMode
_MTL_INLINE MTL::HazardTrackingMode MTL::HeapDescriptor::hazardTrackingMode() const
{
    return Object::sendMessage<MTL::HazardTrackingMode>(this, _MTL_PRIVATE_SEL(hazardTrackingMode));
}

_MTL_INLINE void MTL::HeapDescriptor::setHazardTrackingMode(MTL::HazardTrackingMode hazardTrackingMode)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setHazardTrackingMode_), hazardTrackingMode);
}

// property: resourceOptions
_MTL_INLINE MTL::ResourceOptions MTL::HeapDescriptor::resourceOptions() const
{
    return Object::sendMessage<MTL::ResourceOptions>(this, _MTL_PRIVATE_SEL(resourceOptions));
}

_MTL_INLINE void MTL::HeapDescriptor::setResourceOptions(MTL::ResourceOptions resourceOptions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setResourceOptions_), resourceOptions);
}

// property: type
_MTL_INLINE MTL::HeapType MTL::HeapDescriptor::type() const
{
    return Object::sendMessage<MTL::HeapType>(this, _MTL_PRIVATE_SEL(type));
}

_MTL_INLINE void MTL::HeapDescriptor::setType(MTL::HeapType type)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setType_), type);
}

// property: label
_MTL_INLINE NS::String* MTL::Heap::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

_MTL_INLINE void MTL::Heap::setLabel(const NS::String* label)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLabel_), label);
}

// property: device
_MTL_INLINE MTL::Device* MTL::Heap::device() const
{
    return Object::sendMessage<MTL::Device*>(this, _MTL_PRIVATE_SEL(device));
}

// property: storageMode
_MTL_INLINE MTL::StorageMode MTL::Heap::storageMode() const
{
    return Object::sendMessage<MTL::StorageMode>(this, _MTL_PRIVATE_SEL(storageMode));
}

// property: cpuCacheMode
_MTL_INLINE MTL::CPUCacheMode MTL::Heap::cpuCacheMode() const
{
    return Object::sendMessage<MTL::CPUCacheMode>(this, _MTL_PRIVATE_SEL(cpuCacheMode));
}

// property: hazardTrackingMode
_MTL_INLINE MTL::HazardTrackingMode MTL::Heap::hazardTrackingMode() const
{
    return Object::sendMessage<MTL::HazardTrackingMode>(this, _MTL_PRIVATE_SEL(hazardTrackingMode));
}

// property: resourceOptions
_MTL_INLINE MTL::ResourceOptions MTL::Heap::resourceOptions() const
{
    return Object::sendMessage<MTL::ResourceOptions>(this, _MTL_PRIVATE_SEL(resourceOptions));
}

// property: size
_MTL_INLINE NS::UInteger MTL::Heap::size() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(size));
}

// property: usedSize
_MTL_INLINE NS::UInteger MTL::Heap::usedSize() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(usedSize));
}

// property: currentAllocatedSize
_MTL_INLINE NS::UInteger MTL::Heap::currentAllocatedSize() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(currentAllocatedSize));
}

// method: maxAvailableSizeWithAlignment:
_MTL_INLINE NS::UInteger MTL::Heap::maxAvailableSize(NS::UInteger alignment)
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(maxAvailableSizeWithAlignment_), alignment);
}

// method: newBufferWithLength:options:
_MTL_INLINE MTL::Buffer* MTL::Heap::newBuffer(NS::UInteger length, MTL::ResourceOptions options)
{
    return Object::sendMessage<MTL::Buffer*>(this, _MTL_PRIVATE_SEL(newBufferWithLength_options_), length, options);
}

// method: newTextureWithDescriptor:
_MTL_INLINE MTL::Texture* MTL::Heap::newTexture(const MTL::TextureDescriptor* desc)
{
    return Object::sendMessage<MTL::Texture*>(this, _MTL_PRIVATE_SEL(newTextureWithDescriptor_), desc);
}

// method: setPurgeableState:
_MTL_INLINE MTL::PurgeableState MTL::Heap::setPurgeableState(MTL::PurgeableState state)
{
    return Object::sendMessage<MTL::PurgeableState>(this, _MTL_PRIVATE_SEL(setPurgeableState_), state);
}

// property: type
_MTL_INLINE MTL::HeapType MTL::Heap::type() const
{
    return Object::sendMessage<MTL::HeapType>(this, _MTL_PRIVATE_SEL(type));
}

// method: newBufferWithLength:options:offset:
_MTL_INLINE MTL::Buffer* MTL::Heap::newBuffer(NS::UInteger length, MTL::ResourceOptions options, NS::UInteger offset)
{
    return Object::sendMessage<MTL::Buffer*>(this, _MTL_PRIVATE_SEL(newBufferWithLength_options_offset_), length, options, offset);
}

// method: newTextureWithDescriptor:offset:
_MTL_INLINE MTL::Texture* MTL::Heap::newTexture(const MTL::TextureDescriptor* descriptor, NS::UInteger offset)
{
    return Object::sendMessage<MTL::Texture*>(this, _MTL_PRIVATE_SEL(newTextureWithDescriptor_offset_), descriptor, offset);
}
