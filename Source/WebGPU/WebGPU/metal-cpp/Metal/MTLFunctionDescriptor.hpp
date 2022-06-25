//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLFunctionDescriptor.hpp
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

#include "MTLFunctionDescriptor.hpp"

namespace MTL
{
_MTL_OPTIONS(NS::UInteger, FunctionOptions) {
    FunctionOptionNone = 0,
    FunctionOptionCompileToBinary = 1,
};

class FunctionDescriptor : public NS::Copying<FunctionDescriptor>
{
public:
    static class FunctionDescriptor* alloc();

    class FunctionDescriptor*        init();

    static class FunctionDescriptor* functionDescriptor();

    NS::String*                      name() const;
    void                             setName(const NS::String* name);

    NS::String*                      specializedName() const;
    void                             setSpecializedName(const NS::String* specializedName);

    class FunctionConstantValues*    constantValues() const;
    void                             setConstantValues(const class FunctionConstantValues* constantValues);

    MTL::FunctionOptions             options() const;
    void                             setOptions(MTL::FunctionOptions options);

    NS::Array*                       binaryArchives() const;
    void                             setBinaryArchives(const NS::Array* binaryArchives);
};

class IntersectionFunctionDescriptor : public NS::Copying<IntersectionFunctionDescriptor, MTL::FunctionDescriptor>
{
public:
    static class IntersectionFunctionDescriptor* alloc();

    class IntersectionFunctionDescriptor*        init();
};

}

// static method: alloc
_MTL_INLINE MTL::FunctionDescriptor* MTL::FunctionDescriptor::alloc()
{
    return NS::Object::alloc<MTL::FunctionDescriptor>(_MTL_PRIVATE_CLS(MTLFunctionDescriptor));
}

// method: init
_MTL_INLINE MTL::FunctionDescriptor* MTL::FunctionDescriptor::init()
{
    return NS::Object::init<MTL::FunctionDescriptor>();
}

// static method: functionDescriptor
_MTL_INLINE MTL::FunctionDescriptor* MTL::FunctionDescriptor::functionDescriptor()
{
    return Object::sendMessage<MTL::FunctionDescriptor*>(_MTL_PRIVATE_CLS(MTLFunctionDescriptor), _MTL_PRIVATE_SEL(functionDescriptor));
}

// property: name
_MTL_INLINE NS::String* MTL::FunctionDescriptor::name() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(name));
}

_MTL_INLINE void MTL::FunctionDescriptor::setName(const NS::String* name)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setName_), name);
}

// property: specializedName
_MTL_INLINE NS::String* MTL::FunctionDescriptor::specializedName() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(specializedName));
}

_MTL_INLINE void MTL::FunctionDescriptor::setSpecializedName(const NS::String* specializedName)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSpecializedName_), specializedName);
}

// property: constantValues
_MTL_INLINE MTL::FunctionConstantValues* MTL::FunctionDescriptor::constantValues() const
{
    return Object::sendMessage<MTL::FunctionConstantValues*>(this, _MTL_PRIVATE_SEL(constantValues));
}

_MTL_INLINE void MTL::FunctionDescriptor::setConstantValues(const MTL::FunctionConstantValues* constantValues)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setConstantValues_), constantValues);
}

// property: options
_MTL_INLINE MTL::FunctionOptions MTL::FunctionDescriptor::options() const
{
    return Object::sendMessage<MTL::FunctionOptions>(this, _MTL_PRIVATE_SEL(options));
}

_MTL_INLINE void MTL::FunctionDescriptor::setOptions(MTL::FunctionOptions options)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setOptions_), options);
}

// property: binaryArchives
_MTL_INLINE NS::Array* MTL::FunctionDescriptor::binaryArchives() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(binaryArchives));
}

_MTL_INLINE void MTL::FunctionDescriptor::setBinaryArchives(const NS::Array* binaryArchives)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setBinaryArchives_), binaryArchives);
}

// static method: alloc
_MTL_INLINE MTL::IntersectionFunctionDescriptor* MTL::IntersectionFunctionDescriptor::alloc()
{
    return NS::Object::alloc<MTL::IntersectionFunctionDescriptor>(_MTL_PRIVATE_CLS(MTLIntersectionFunctionDescriptor));
}

// method: init
_MTL_INLINE MTL::IntersectionFunctionDescriptor* MTL::IntersectionFunctionDescriptor::init()
{
    return NS::Object::init<MTL::IntersectionFunctionDescriptor>();
}
