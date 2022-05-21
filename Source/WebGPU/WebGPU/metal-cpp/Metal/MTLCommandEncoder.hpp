//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLCommandEncoder.hpp
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
_MTL_OPTIONS(NS::UInteger, ResourceUsage) {
    ResourceUsageRead = 1,
    ResourceUsageWrite = 2,
    ResourceUsageSample = 4,
};

_MTL_OPTIONS(NS::UInteger, BarrierScope) {
    BarrierScopeBuffers = 1,
    BarrierScopeTextures = 2,
    BarrierScopeRenderTargets = 4,
};

class CommandEncoder : public NS::Referencing<CommandEncoder>
{
public:
    class Device* device() const;

    NS::String*   label() const;
    void          setLabel(const NS::String* label);

    void          endEncoding();

    void          insertDebugSignpost(const NS::String* string);

    void          pushDebugGroup(const NS::String* string);

    void          popDebugGroup();
};

}

// property: device
_MTL_INLINE MTL::Device* MTL::CommandEncoder::device() const
{
    return Object::sendMessage<MTL::Device*>(this, _MTL_PRIVATE_SEL(device));
}

// property: label
_MTL_INLINE NS::String* MTL::CommandEncoder::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

_MTL_INLINE void MTL::CommandEncoder::setLabel(const NS::String* label)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLabel_), label);
}

// method: endEncoding
_MTL_INLINE void MTL::CommandEncoder::endEncoding()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(endEncoding));
}

// method: insertDebugSignpost:
_MTL_INLINE void MTL::CommandEncoder::insertDebugSignpost(const NS::String* string)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(insertDebugSignpost_), string);
}

// method: pushDebugGroup:
_MTL_INLINE void MTL::CommandEncoder::pushDebugGroup(const NS::String* string)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(pushDebugGroup_), string);
}

// method: popDebugGroup
_MTL_INLINE void MTL::CommandEncoder::popDebugGroup()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(popDebugGroup));
}
