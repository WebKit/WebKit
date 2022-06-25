//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLLinkedFunctions.hpp
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
class LinkedFunctions : public NS::Copying<LinkedFunctions>
{
public:
    static class LinkedFunctions* alloc();

    class LinkedFunctions*        init();

    static class LinkedFunctions* linkedFunctions();

    NS::Array*                    functions() const;
    void                          setFunctions(const NS::Array* functions);

    NS::Array*                    binaryFunctions() const;
    void                          setBinaryFunctions(const NS::Array* binaryFunctions);

    NS::Array*                    groups() const;
    void                          setGroups(const NS::Array* groups);

    NS::Array*                    privateFunctions() const;
    void                          setPrivateFunctions(const NS::Array* privateFunctions);
};

}

// static method: alloc
_MTL_INLINE MTL::LinkedFunctions* MTL::LinkedFunctions::alloc()
{
    return NS::Object::alloc<MTL::LinkedFunctions>(_MTL_PRIVATE_CLS(MTLLinkedFunctions));
}

// method: init
_MTL_INLINE MTL::LinkedFunctions* MTL::LinkedFunctions::init()
{
    return NS::Object::init<MTL::LinkedFunctions>();
}

// static method: linkedFunctions
_MTL_INLINE MTL::LinkedFunctions* MTL::LinkedFunctions::linkedFunctions()
{
    return Object::sendMessage<MTL::LinkedFunctions*>(_MTL_PRIVATE_CLS(MTLLinkedFunctions), _MTL_PRIVATE_SEL(linkedFunctions));
}

// property: functions
_MTL_INLINE NS::Array* MTL::LinkedFunctions::functions() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(functions));
}

_MTL_INLINE void MTL::LinkedFunctions::setFunctions(const NS::Array* functions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFunctions_), functions);
}

// property: binaryFunctions
_MTL_INLINE NS::Array* MTL::LinkedFunctions::binaryFunctions() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(binaryFunctions));
}

_MTL_INLINE void MTL::LinkedFunctions::setBinaryFunctions(const NS::Array* binaryFunctions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setBinaryFunctions_), binaryFunctions);
}

// property: groups
_MTL_INLINE NS::Array* MTL::LinkedFunctions::groups() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(groups));
}

_MTL_INLINE void MTL::LinkedFunctions::setGroups(const NS::Array* groups)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setGroups_), groups);
}

// property: privateFunctions
_MTL_INLINE NS::Array* MTL::LinkedFunctions::privateFunctions() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(privateFunctions));
}

_MTL_INLINE void MTL::LinkedFunctions::setPrivateFunctions(const NS::Array* privateFunctions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setPrivateFunctions_), privateFunctions);
}
