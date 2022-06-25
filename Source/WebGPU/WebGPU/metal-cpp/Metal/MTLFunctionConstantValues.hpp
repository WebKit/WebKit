//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLFunctionConstantValues.hpp
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

#include "MTLArgument.hpp"

namespace MTL
{
class FunctionConstantValues : public NS::Copying<FunctionConstantValues>
{
public:
    static class FunctionConstantValues* alloc();

    class FunctionConstantValues*        init();

    void                                 setConstantValue(const void* value, MTL::DataType type, NS::UInteger index);

    void                                 setConstantValues(const void* values, MTL::DataType type, NS::Range range);

    void                                 setConstantValue(const void* value, MTL::DataType type, const NS::String* name);

    void                                 reset();
};

}

// static method: alloc
_MTL_INLINE MTL::FunctionConstantValues* MTL::FunctionConstantValues::alloc()
{
    return NS::Object::alloc<MTL::FunctionConstantValues>(_MTL_PRIVATE_CLS(MTLFunctionConstantValues));
}

// method: init
_MTL_INLINE MTL::FunctionConstantValues* MTL::FunctionConstantValues::init()
{
    return NS::Object::init<MTL::FunctionConstantValues>();
}

// method: setConstantValue:type:atIndex:
_MTL_INLINE void MTL::FunctionConstantValues::setConstantValue(const void* value, MTL::DataType type, NS::UInteger index)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setConstantValue_type_atIndex_), value, type, index);
}

// method: setConstantValues:type:withRange:
_MTL_INLINE void MTL::FunctionConstantValues::setConstantValues(const void* values, MTL::DataType type, NS::Range range)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setConstantValues_type_withRange_), values, type, range);
}

// method: setConstantValue:type:withName:
_MTL_INLINE void MTL::FunctionConstantValues::setConstantValue(const void* value, MTL::DataType type, const NS::String* name)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setConstantValue_type_withName_), value, type, name);
}

// method: reset
_MTL_INLINE void MTL::FunctionConstantValues::reset()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(reset));
}
