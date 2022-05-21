//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLDrawable.hpp
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

#include <CoreFoundation/CoreFoundation.h>
#include <functional>

namespace MTL
{
using DrawablePresentedHandler = void (^)(class Drawable*);

using DrawablePresentedHandlerFunction = std::function<void(class Drawable*)>;

class Drawable : public NS::Referencing<Drawable>
{
public:
    void           addPresentedHandler(const MTL::DrawablePresentedHandlerFunction& function);

    void           present();

    void           presentAtTime(CFTimeInterval presentationTime);

    void           presentAfterMinimumDuration(CFTimeInterval duration);

    void           addPresentedHandler(const MTL::DrawablePresentedHandler block);

    CFTimeInterval presentedTime() const;

    NS::UInteger   drawableID() const;
};

}

_MTL_INLINE void MTL::Drawable::addPresentedHandler(const MTL::DrawablePresentedHandlerFunction& function)
{
    __block DrawablePresentedHandlerFunction blockFunction = function;

    addPresentedHandler(^(Drawable* pDrawable) { blockFunction(pDrawable); });
}

// method: present
_MTL_INLINE void MTL::Drawable::present()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(present));
}

// method: presentAtTime:
_MTL_INLINE void MTL::Drawable::presentAtTime(CFTimeInterval presentationTime)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(presentAtTime_), presentationTime);
}

// method: presentAfterMinimumDuration:
_MTL_INLINE void MTL::Drawable::presentAfterMinimumDuration(CFTimeInterval duration)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(presentAfterMinimumDuration_), duration);
}

// method: addPresentedHandler:
_MTL_INLINE void MTL::Drawable::addPresentedHandler(const MTL::DrawablePresentedHandler block)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(addPresentedHandler_), block);
}

// property: presentedTime
_MTL_INLINE CFTimeInterval MTL::Drawable::presentedTime() const
{
    return Object::sendMessage<CFTimeInterval>(this, _MTL_PRIVATE_SEL(presentedTime));
}

// property: drawableID
_MTL_INLINE NS::UInteger MTL::Drawable::drawableID() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(drawableID));
}
