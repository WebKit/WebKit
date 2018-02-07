/*
 * Copyright (C) 2007 Henry Mason (hmason@mac.com)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "MessageEvent.h"

#include "Blob.h"
#include "EventNames.h"
#include <JavaScriptCore/JSCInlines.h>

namespace WebCore {

using namespace JSC;

inline MessageEvent::MessageEvent()
    : m_dataType(DataTypeScriptValue)
{
}

inline MessageEvent::MessageEvent(ExecState& state, const AtomicString& type, Init&& initializer, IsTrusted isTrusted)
    : Event(type, initializer, isTrusted)
    , m_dataType(DataTypeScriptValue)
    , m_dataAsScriptValue(state.vm(), initializer.data)
    , m_origin(initializer.origin)
    , m_lastEventId(initializer.lastEventId)
    , m_source(WTFMove(initializer.source))
    , m_ports(WTFMove(initializer.ports))
{
}

inline MessageEvent::MessageEvent(RefPtr<SerializedScriptValue>&& data, const String& origin, const String& lastEventId, std::optional<MessageEventSource>&& source, Vector<RefPtr<MessagePort>>&& ports)
    : Event(eventNames().messageEvent, false, false)
    , m_dataType(DataTypeSerializedScriptValue)
    , m_dataAsSerializedScriptValue(WTFMove(data))
    , m_origin(origin)
    , m_lastEventId(lastEventId)
    , m_source(WTFMove(source))
    , m_ports(WTFMove(ports))
{
}

inline MessageEvent::MessageEvent(const AtomicString& type, RefPtr<SerializedScriptValue>&& data, const String& origin, const String& lastEventId)
    : Event(type, false, false)
    , m_dataType(DataTypeSerializedScriptValue)
    , m_dataAsSerializedScriptValue(WTFMove(data))
    , m_origin(origin)
    , m_lastEventId(lastEventId)
{
}

inline MessageEvent::MessageEvent(const String& data, const String& origin)
    : Event(eventNames().messageEvent, false, false)
    , m_dataType(DataTypeString)
    , m_dataAsString(data)
    , m_origin(origin)
{
}

inline MessageEvent::MessageEvent(Ref<Blob>&& data, const String& origin)
    : Event(eventNames().messageEvent, false, false)
    , m_dataType(DataTypeBlob)
    , m_dataAsBlob(WTFMove(data))
    , m_origin(origin)
{
}

inline MessageEvent::MessageEvent(Ref<ArrayBuffer>&& data, const String& origin)
    : Event(eventNames().messageEvent, false, false)
    , m_dataType(DataTypeArrayBuffer)
    , m_dataAsArrayBuffer(WTFMove(data))
    , m_origin(origin)
{
}

Ref<MessageEvent> MessageEvent::create(Vector<RefPtr<MessagePort>>&& ports, RefPtr<SerializedScriptValue>&& data, const String& origin, const String& lastEventId, std::optional<MessageEventSource>&& source)
{
    return adoptRef(*new MessageEvent(WTFMove(data), origin, lastEventId, WTFMove(source), WTFMove(ports)));
}

Ref<MessageEvent> MessageEvent::create(const AtomicString& type, RefPtr<SerializedScriptValue>&& data, const String& origin, const String& lastEventId)
{
    return adoptRef(*new MessageEvent(type, WTFMove(data), origin, lastEventId));
}

Ref<MessageEvent> MessageEvent::create(const String& data, const String& origin)
{
    return adoptRef(*new MessageEvent(data, origin));
}

Ref<MessageEvent> MessageEvent::create(Ref<Blob>&& data, const String& origin)
{
    return adoptRef(*new MessageEvent(WTFMove(data), origin));
}

Ref<MessageEvent> MessageEvent::create(Ref<ArrayBuffer>&& data, const String& origin)
{
    return adoptRef(*new MessageEvent(WTFMove(data), origin));
}

Ref<MessageEvent> MessageEvent::createForBindings()
{
    return adoptRef(*new MessageEvent);
}

Ref<MessageEvent> MessageEvent::create(ExecState& state, const AtomicString& type, Init&& initializer, IsTrusted isTrusted)
{
    return adoptRef(*new MessageEvent(state, type, WTFMove(initializer), isTrusted));
}

MessageEvent::~MessageEvent() = default;

void MessageEvent::initMessageEvent(ExecState& state, const AtomicString& type, bool canBubble, bool cancelable, JSValue data, const String& origin, const String& lastEventId, std::optional<MessageEventSource>&& source, Vector<RefPtr<MessagePort>>&& ports)
{
    if (isBeingDispatched())
        return;

    initEvent(type, canBubble, cancelable);

    m_dataType = DataTypeScriptValue;
    m_dataAsScriptValue = Deprecated::ScriptValue(state.vm(), data);
    m_dataAsSerializedScriptValue = nullptr;
    m_triedToSerialize = false;
    m_dataAsString = { };
    m_dataAsBlob = nullptr;
    m_dataAsArrayBuffer = nullptr;
    m_origin = origin;
    m_lastEventId = lastEventId;
    m_source = WTFMove(source);
    m_ports = WTFMove(ports);
}

RefPtr<SerializedScriptValue> MessageEvent::trySerializeData(ExecState* exec)
{
    ASSERT(!m_dataAsScriptValue.hasNoValue());
    
    if (!m_dataAsSerializedScriptValue && !m_triedToSerialize) {
        m_dataAsSerializedScriptValue = SerializedScriptValue::create(*exec, m_dataAsScriptValue.jsValue(), SerializationErrorMode::NonThrowing);
        m_triedToSerialize = true;
    }
    
    return m_dataAsSerializedScriptValue;
}

// FIXME: Remove this when we have custom ObjC binding support.
SerializedScriptValue* MessageEvent::data() const
{
    // WebSocket is not exposed in ObjC bindings, thus the data type should always be SerializedScriptValue.
    ASSERT(m_dataType == DataTypeSerializedScriptValue);
    return m_dataAsSerializedScriptValue.get();
}

EventInterface MessageEvent::eventInterface() const
{
    return MessageEventInterfaceType;
}

} // namespace WebCore
