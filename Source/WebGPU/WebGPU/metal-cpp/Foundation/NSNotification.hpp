//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Foundation/NSNotification.hpp
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

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#include "NSDefines.hpp"
#include "NSDictionary.hpp"
#include "NSObject.hpp"
#include "NSString.hpp"
#include "NSTypes.hpp"

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace NS
{
using NotificationName = class String*;

class Notification : public NS::Referencing<Notification>
{
public:
    NS::String*     name() const;
    NS::Object*     object() const;
    NS::Dictionary* userInfo() const;
};
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

_NS_INLINE NS::String* NS::Notification::name() const
{
    return Object::sendMessage<NS::String*>(this, _NS_PRIVATE_SEL(name));
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

_NS_INLINE NS::Object* NS::Notification::object() const
{
    return Object::sendMessage<NS::Object*>(this, _NS_PRIVATE_SEL(object));
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

_NS_INLINE NS::Dictionary* NS::Notification::userInfo() const
{
    return Object::sendMessage<NS::Dictionary*>(this, _NS_PRIVATE_SEL(userInfo));
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
