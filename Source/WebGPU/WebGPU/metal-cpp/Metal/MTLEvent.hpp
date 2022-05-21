//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLEvent.hpp
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

#include "MTLEvent.hpp"

namespace MTL
{
class Event : public NS::Referencing<Event>
{
public:
    class Device* device() const;

    NS::String*   label() const;
    void          setLabel(const NS::String* label);
};

class SharedEventListener : public NS::Referencing<SharedEventListener>
{
public:
    static class SharedEventListener* alloc();

    MTL::SharedEventListener*         init();

    MTL::SharedEventListener*         init(const dispatch_queue_t dispatchQueue);

    dispatch_queue_t                  dispatchQueue() const;
};

using SharedEventNotificationBlock = void (^)(SharedEvent* pEvent, std::uint64_t value);

class SharedEvent : public NS::Referencing<SharedEvent, Event>
{
public:
    void                     notifyListener(const class SharedEventListener* listener, uint64_t value, const MTL::SharedEventNotificationBlock block);

    class SharedEventHandle* newSharedEventHandle();

    uint64_t                 signaledValue() const;
    void                     setSignaledValue(uint64_t signaledValue);
};

class SharedEventHandle : public NS::Referencing<SharedEventHandle>
{
public:
    static class SharedEventHandle* alloc();

    class SharedEventHandle*        init();

    NS::String*                     label() const;
};

struct SharedEventHandlePrivate
{
} _MTL_PACKED;

}

// property: device
_MTL_INLINE MTL::Device* MTL::Event::device() const
{
    return Object::sendMessage<MTL::Device*>(this, _MTL_PRIVATE_SEL(device));
}

// property: label
_MTL_INLINE NS::String* MTL::Event::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

_MTL_INLINE void MTL::Event::setLabel(const NS::String* label)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLabel_), label);
}

// static method: alloc
_MTL_INLINE MTL::SharedEventListener* MTL::SharedEventListener::alloc()
{
    return NS::Object::alloc<MTL::SharedEventListener>(_MTL_PRIVATE_CLS(MTLSharedEventListener));
}

// method: init
_MTL_INLINE MTL::SharedEventListener* MTL::SharedEventListener::init()
{
    return NS::Object::init<MTL::SharedEventListener>();
}

// method: initWithDispatchQueue:
_MTL_INLINE MTL::SharedEventListener* MTL::SharedEventListener::init(const dispatch_queue_t dispatchQueue)
{
    return Object::sendMessage<MTL::SharedEventListener*>(this, _MTL_PRIVATE_SEL(initWithDispatchQueue_), dispatchQueue);
}

// property: dispatchQueue
_MTL_INLINE dispatch_queue_t MTL::SharedEventListener::dispatchQueue() const
{
    return Object::sendMessage<dispatch_queue_t>(this, _MTL_PRIVATE_SEL(dispatchQueue));
}

// method: notifyListener:atValue:block:
_MTL_INLINE void MTL::SharedEvent::notifyListener(const MTL::SharedEventListener* listener, uint64_t value, const MTL::SharedEventNotificationBlock block)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(notifyListener_atValue_block_), listener, value, block);
}

// method: newSharedEventHandle
_MTL_INLINE MTL::SharedEventHandle* MTL::SharedEvent::newSharedEventHandle()
{
    return Object::sendMessage<MTL::SharedEventHandle*>(this, _MTL_PRIVATE_SEL(newSharedEventHandle));
}

// property: signaledValue
_MTL_INLINE uint64_t MTL::SharedEvent::signaledValue() const
{
    return Object::sendMessage<uint64_t>(this, _MTL_PRIVATE_SEL(signaledValue));
}

_MTL_INLINE void MTL::SharedEvent::setSignaledValue(uint64_t signaledValue)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setSignaledValue_), signaledValue);
}

// static method: alloc
_MTL_INLINE MTL::SharedEventHandle* MTL::SharedEventHandle::alloc()
{
    return NS::Object::alloc<MTL::SharedEventHandle>(_MTL_PRIVATE_CLS(MTLSharedEventHandle));
}

// method: init
_MTL_INLINE MTL::SharedEventHandle* MTL::SharedEventHandle::init()
{
    return NS::Object::init<MTL::SharedEventHandle>();
}

// property: label
_MTL_INLINE NS::String* MTL::SharedEventHandle::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}
