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

#include <runtime/JSCInlines.h>

namespace WebCore {

static inline bool isValidSource(EventTarget* source)
{
    return !source || source->toDOMWindow() || source->isMessagePort();
}

MessageEventInit::MessageEventInit()
{
}

MessageEvent::MessageEvent()
    : m_dataType(DataTypeScriptValue)
{
}

MessageEvent::MessageEvent(const AtomicString& type, const MessageEventInit& initializer)
    : Event(type, initializer)
    , m_dataType(DataTypeScriptValue)
    , m_dataAsScriptValue(initializer.data)
    , m_origin(initializer.origin)
    , m_lastEventId(initializer.lastEventId)
    , m_source(isValidSource(initializer.source.get()) ? initializer.source : 0)
    , m_ports(std::make_unique<MessagePortArray>(initializer.ports))
{
}

MessageEvent::MessageEvent(const Deprecated::ScriptValue& data, const String& origin, const String& lastEventId, PassRefPtr<EventTarget> source, std::unique_ptr<MessagePortArray> ports)
    : Event(eventNames().messageEvent, false, false)
    , m_dataType(DataTypeScriptValue)
    , m_dataAsScriptValue(data)
    , m_origin(origin)
    , m_lastEventId(lastEventId)
    , m_source(source)
    , m_ports(WTF::move(ports))
{
    ASSERT(isValidSource(m_source.get()));
}

MessageEvent::MessageEvent(PassRefPtr<SerializedScriptValue> data, const String& origin, const String& lastEventId, PassRefPtr<EventTarget> source, std::unique_ptr<MessagePortArray> ports)
    : Event(eventNames().messageEvent, false, false)
    , m_dataType(DataTypeSerializedScriptValue)
    , m_dataAsSerializedScriptValue(data)
    , m_origin(origin)
    , m_lastEventId(lastEventId)
    , m_source(source)
    , m_ports(WTF::move(ports))
{
    ASSERT(isValidSource(m_source.get()));
}

MessageEvent::MessageEvent(const String& data, const String& origin)
    : Event(eventNames().messageEvent, false, false)
    , m_dataType(DataTypeString)
    , m_dataAsString(data)
    , m_origin(origin)
{
}

MessageEvent::MessageEvent(PassRefPtr<Blob> data, const String& origin)
    : Event(eventNames().messageEvent, false, false)
    , m_dataType(DataTypeBlob)
    , m_dataAsBlob(data)
    , m_origin(origin)
{
}

MessageEvent::MessageEvent(PassRefPtr<ArrayBuffer> data, const String& origin)
    : Event(eventNames().messageEvent, false, false)
    , m_dataType(DataTypeArrayBuffer)
    , m_dataAsArrayBuffer(data)
    , m_origin(origin)
{
}

MessageEvent::~MessageEvent()
{
}

void MessageEvent::initMessageEvent(const AtomicString& type, bool canBubble, bool cancelable, const Deprecated::ScriptValue& data, const String& origin, const String& lastEventId, DOMWindow* source, std::unique_ptr<MessagePortArray> ports)
{
    if (dispatched())
        return;

    initEvent(type, canBubble, cancelable);

    m_dataType = DataTypeScriptValue;
    m_dataAsScriptValue = data;
    m_origin = origin;
    m_lastEventId = lastEventId;
    m_source = source;
    m_ports = WTF::move(ports);
}

void MessageEvent::initMessageEvent(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtr<SerializedScriptValue> data, const String& origin, const String& lastEventId, DOMWindow* source, std::unique_ptr<MessagePortArray> ports)
{
    if (dispatched())
        return;

    initEvent(type, canBubble, cancelable);

    m_dataType = DataTypeSerializedScriptValue;
    m_dataAsSerializedScriptValue = data;
    m_origin = origin;
    m_lastEventId = lastEventId;
    m_source = source;
    m_ports = WTF::move(ports);
}

// FIXME: Remove this when we have custom ObjC binding support.
SerializedScriptValue* MessageEvent::data() const
{
    // WebSocket is not exposed in ObjC bindings, thus the data type should always be SerializedScriptValue.
    ASSERT(m_dataType == DataTypeSerializedScriptValue);
    return m_dataAsSerializedScriptValue.get();
}

MessagePort* MessageEvent::messagePort()
{
    if (!m_ports)
        return 0;
    ASSERT(m_ports->size() == 1);
    return (*m_ports)[0].get();
}

void MessageEvent::initMessageEvent(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtr<SerializedScriptValue> data, const String& origin, const String& lastEventId, DOMWindow* source, MessagePort* port)
{
    std::unique_ptr<MessagePortArray> ports;
    if (port) {
        ports = std::make_unique<MessagePortArray>();
        ports->append(port);
    }
    initMessageEvent(type, canBubble, cancelable, data, origin, lastEventId, source, WTF::move(ports));
}

EventInterface MessageEvent::eventInterface() const
{
    return MessageEventInterfaceType;
}

} // namespace WebCore
