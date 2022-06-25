//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLResource.hpp
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
_MTL_ENUM(NS::UInteger, PurgeableState) {
    PurgeableStateKeepCurrent = 1,
    PurgeableStateNonVolatile = 2,
    PurgeableStateVolatile = 3,
    PurgeableStateEmpty = 4,
};

_MTL_ENUM(NS::UInteger, CPUCacheMode) {
    CPUCacheModeDefaultCache = 0,
    CPUCacheModeWriteCombined = 1,
};

_MTL_ENUM(NS::UInteger, StorageMode) {
    StorageModeShared = 0,
    StorageModeManaged = 1,
    StorageModePrivate = 2,
    StorageModeMemoryless = 3,
};

_MTL_ENUM(NS::UInteger, HazardTrackingMode) {
    HazardTrackingModeDefault = 0,
    HazardTrackingModeUntracked = 1,
    HazardTrackingModeTracked = 2,
};

_MTL_OPTIONS(NS::UInteger, ResourceOptions) {
    ResourceStorageModeShared = 0,
    ResourceHazardTrackingModeDefault = 0,
    ResourceCPUCacheModeDefaultCache = 0,
    ResourceOptionCPUCacheModeDefault = 0,
    ResourceCPUCacheModeWriteCombined = 1,
    ResourceOptionCPUCacheModeWriteCombined = 1,
    ResourceStorageModeManaged = 16,
    ResourceStorageModePrivate = 32,
    ResourceStorageModeMemoryless = 48,
    ResourceHazardTrackingModeUntracked = 256,
    ResourceHazardTrackingModeTracked = 512,
};

class Resource : public NS::Referencing<Resource>
{
public:
    NS::String*             label() const;
    void                    setLabel(const NS::String* label);

    class Device*           device() const;

    MTL::CPUCacheMode       cpuCacheMode() const;

    MTL::StorageMode        storageMode() const;

    MTL::HazardTrackingMode hazardTrackingMode() const;

    MTL::ResourceOptions    resourceOptions() const;

    MTL::PurgeableState     setPurgeableState(MTL::PurgeableState state);

    class Heap*             heap() const;

    NS::UInteger            heapOffset() const;

    NS::UInteger            allocatedSize() const;

    void                    makeAliasable();

    bool                    isAliasable();
};

}

// property: label
_MTL_INLINE NS::String* MTL::Resource::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

_MTL_INLINE void MTL::Resource::setLabel(const NS::String* label)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLabel_), label);
}

// property: device
_MTL_INLINE MTL::Device* MTL::Resource::device() const
{
    return Object::sendMessage<MTL::Device*>(this, _MTL_PRIVATE_SEL(device));
}

// property: cpuCacheMode
_MTL_INLINE MTL::CPUCacheMode MTL::Resource::cpuCacheMode() const
{
    return Object::sendMessage<MTL::CPUCacheMode>(this, _MTL_PRIVATE_SEL(cpuCacheMode));
}

// property: storageMode
_MTL_INLINE MTL::StorageMode MTL::Resource::storageMode() const
{
    return Object::sendMessage<MTL::StorageMode>(this, _MTL_PRIVATE_SEL(storageMode));
}

// property: hazardTrackingMode
_MTL_INLINE MTL::HazardTrackingMode MTL::Resource::hazardTrackingMode() const
{
    return Object::sendMessage<MTL::HazardTrackingMode>(this, _MTL_PRIVATE_SEL(hazardTrackingMode));
}

// property: resourceOptions
_MTL_INLINE MTL::ResourceOptions MTL::Resource::resourceOptions() const
{
    return Object::sendMessage<MTL::ResourceOptions>(this, _MTL_PRIVATE_SEL(resourceOptions));
}

// method: setPurgeableState:
_MTL_INLINE MTL::PurgeableState MTL::Resource::setPurgeableState(MTL::PurgeableState state)
{
    return Object::sendMessage<MTL::PurgeableState>(this, _MTL_PRIVATE_SEL(setPurgeableState_), state);
}

// property: heap
_MTL_INLINE MTL::Heap* MTL::Resource::heap() const
{
    return Object::sendMessage<MTL::Heap*>(this, _MTL_PRIVATE_SEL(heap));
}

// property: heapOffset
_MTL_INLINE NS::UInteger MTL::Resource::heapOffset() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(heapOffset));
}

// property: allocatedSize
_MTL_INLINE NS::UInteger MTL::Resource::allocatedSize() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(allocatedSize));
}

// method: makeAliasable
_MTL_INLINE void MTL::Resource::makeAliasable()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(makeAliasable));
}

// method: isAliasable
_MTL_INLINE bool MTL::Resource::isAliasable()
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isAliasable));
}
