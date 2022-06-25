//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLDepthStencil.hpp
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

#include "MTLDepthStencil.hpp"

namespace MTL
{
_MTL_ENUM(NS::UInteger, CompareFunction) {
    CompareFunctionNever = 0,
    CompareFunctionLess = 1,
    CompareFunctionEqual = 2,
    CompareFunctionLessEqual = 3,
    CompareFunctionGreater = 4,
    CompareFunctionNotEqual = 5,
    CompareFunctionGreaterEqual = 6,
    CompareFunctionAlways = 7,
};

_MTL_ENUM(NS::UInteger, StencilOperation) {
    StencilOperationKeep = 0,
    StencilOperationZero = 1,
    StencilOperationReplace = 2,
    StencilOperationIncrementClamp = 3,
    StencilOperationDecrementClamp = 4,
    StencilOperationInvert = 5,
    StencilOperationIncrementWrap = 6,
    StencilOperationDecrementWrap = 7,
};

class StencilDescriptor : public NS::Copying<StencilDescriptor>
{
public:
    static class StencilDescriptor* alloc();

    class StencilDescriptor*        init();

    MTL::CompareFunction            stencilCompareFunction() const;
    void                            setStencilCompareFunction(MTL::CompareFunction stencilCompareFunction);

    MTL::StencilOperation           stencilFailureOperation() const;
    void                            setStencilFailureOperation(MTL::StencilOperation stencilFailureOperation);

    MTL::StencilOperation           depthFailureOperation() const;
    void                            setDepthFailureOperation(MTL::StencilOperation depthFailureOperation);

    MTL::StencilOperation           depthStencilPassOperation() const;
    void                            setDepthStencilPassOperation(MTL::StencilOperation depthStencilPassOperation);

    uint32_t                        readMask() const;
    void                            setReadMask(uint32_t readMask);

    uint32_t                        writeMask() const;
    void                            setWriteMask(uint32_t writeMask);
};

class DepthStencilDescriptor : public NS::Copying<DepthStencilDescriptor>
{
public:
    static class DepthStencilDescriptor* alloc();

    class DepthStencilDescriptor*        init();

    MTL::CompareFunction                 depthCompareFunction() const;
    void                                 setDepthCompareFunction(MTL::CompareFunction depthCompareFunction);

    bool                                 depthWriteEnabled() const;
    void                                 setDepthWriteEnabled(bool depthWriteEnabled);

    class StencilDescriptor*             frontFaceStencil() const;
    void                                 setFrontFaceStencil(const class StencilDescriptor* frontFaceStencil);

    class StencilDescriptor*             backFaceStencil() const;
    void                                 setBackFaceStencil(const class StencilDescriptor* backFaceStencil);

    NS::String*                          label() const;
    void                                 setLabel(const NS::String* label);
};

class DepthStencilState : public NS::Referencing<DepthStencilState>
{
public:
    NS::String*   label() const;

    class Device* device() const;
};

}

// static method: alloc
_MTL_INLINE MTL::StencilDescriptor* MTL::StencilDescriptor::alloc()
{
    return NS::Object::alloc<MTL::StencilDescriptor>(_MTL_PRIVATE_CLS(MTLStencilDescriptor));
}

// method: init
_MTL_INLINE MTL::StencilDescriptor* MTL::StencilDescriptor::init()
{
    return NS::Object::init<MTL::StencilDescriptor>();
}

// property: stencilCompareFunction
_MTL_INLINE MTL::CompareFunction MTL::StencilDescriptor::stencilCompareFunction() const
{
    return Object::sendMessage<MTL::CompareFunction>(this, _MTL_PRIVATE_SEL(stencilCompareFunction));
}

_MTL_INLINE void MTL::StencilDescriptor::setStencilCompareFunction(MTL::CompareFunction stencilCompareFunction)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStencilCompareFunction_), stencilCompareFunction);
}

// property: stencilFailureOperation
_MTL_INLINE MTL::StencilOperation MTL::StencilDescriptor::stencilFailureOperation() const
{
    return Object::sendMessage<MTL::StencilOperation>(this, _MTL_PRIVATE_SEL(stencilFailureOperation));
}

_MTL_INLINE void MTL::StencilDescriptor::setStencilFailureOperation(MTL::StencilOperation stencilFailureOperation)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setStencilFailureOperation_), stencilFailureOperation);
}

// property: depthFailureOperation
_MTL_INLINE MTL::StencilOperation MTL::StencilDescriptor::depthFailureOperation() const
{
    return Object::sendMessage<MTL::StencilOperation>(this, _MTL_PRIVATE_SEL(depthFailureOperation));
}

_MTL_INLINE void MTL::StencilDescriptor::setDepthFailureOperation(MTL::StencilOperation depthFailureOperation)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setDepthFailureOperation_), depthFailureOperation);
}

// property: depthStencilPassOperation
_MTL_INLINE MTL::StencilOperation MTL::StencilDescriptor::depthStencilPassOperation() const
{
    return Object::sendMessage<MTL::StencilOperation>(this, _MTL_PRIVATE_SEL(depthStencilPassOperation));
}

_MTL_INLINE void MTL::StencilDescriptor::setDepthStencilPassOperation(MTL::StencilOperation depthStencilPassOperation)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setDepthStencilPassOperation_), depthStencilPassOperation);
}

// property: readMask
_MTL_INLINE uint32_t MTL::StencilDescriptor::readMask() const
{
    return Object::sendMessage<uint32_t>(this, _MTL_PRIVATE_SEL(readMask));
}

_MTL_INLINE void MTL::StencilDescriptor::setReadMask(uint32_t readMask)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setReadMask_), readMask);
}

// property: writeMask
_MTL_INLINE uint32_t MTL::StencilDescriptor::writeMask() const
{
    return Object::sendMessage<uint32_t>(this, _MTL_PRIVATE_SEL(writeMask));
}

_MTL_INLINE void MTL::StencilDescriptor::setWriteMask(uint32_t writeMask)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setWriteMask_), writeMask);
}

// static method: alloc
_MTL_INLINE MTL::DepthStencilDescriptor* MTL::DepthStencilDescriptor::alloc()
{
    return NS::Object::alloc<MTL::DepthStencilDescriptor>(_MTL_PRIVATE_CLS(MTLDepthStencilDescriptor));
}

// method: init
_MTL_INLINE MTL::DepthStencilDescriptor* MTL::DepthStencilDescriptor::init()
{
    return NS::Object::init<MTL::DepthStencilDescriptor>();
}

// property: depthCompareFunction
_MTL_INLINE MTL::CompareFunction MTL::DepthStencilDescriptor::depthCompareFunction() const
{
    return Object::sendMessage<MTL::CompareFunction>(this, _MTL_PRIVATE_SEL(depthCompareFunction));
}

_MTL_INLINE void MTL::DepthStencilDescriptor::setDepthCompareFunction(MTL::CompareFunction depthCompareFunction)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setDepthCompareFunction_), depthCompareFunction);
}

// property: depthWriteEnabled
_MTL_INLINE bool MTL::DepthStencilDescriptor::depthWriteEnabled() const
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(isDepthWriteEnabled));
}

_MTL_INLINE void MTL::DepthStencilDescriptor::setDepthWriteEnabled(bool depthWriteEnabled)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setDepthWriteEnabled_), depthWriteEnabled);
}

// property: frontFaceStencil
_MTL_INLINE MTL::StencilDescriptor* MTL::DepthStencilDescriptor::frontFaceStencil() const
{
    return Object::sendMessage<MTL::StencilDescriptor*>(this, _MTL_PRIVATE_SEL(frontFaceStencil));
}

_MTL_INLINE void MTL::DepthStencilDescriptor::setFrontFaceStencil(const MTL::StencilDescriptor* frontFaceStencil)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setFrontFaceStencil_), frontFaceStencil);
}

// property: backFaceStencil
_MTL_INLINE MTL::StencilDescriptor* MTL::DepthStencilDescriptor::backFaceStencil() const
{
    return Object::sendMessage<MTL::StencilDescriptor*>(this, _MTL_PRIVATE_SEL(backFaceStencil));
}

_MTL_INLINE void MTL::DepthStencilDescriptor::setBackFaceStencil(const MTL::StencilDescriptor* backFaceStencil)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setBackFaceStencil_), backFaceStencil);
}

// property: label
_MTL_INLINE NS::String* MTL::DepthStencilDescriptor::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

_MTL_INLINE void MTL::DepthStencilDescriptor::setLabel(const NS::String* label)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLabel_), label);
}

// property: label
_MTL_INLINE NS::String* MTL::DepthStencilState::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

// property: device
_MTL_INLINE MTL::Device* MTL::DepthStencilState::device() const
{
    return Object::sendMessage<MTL::Device*>(this, _MTL_PRIVATE_SEL(device));
}
