/*
 * Copyright (C) 2007 Henry Mason (hmason@mac.com)
 * Copyright (C) 2003-2018 Apple Inc. All rights reserved.
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
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

using namespace JSC;

WTF_MAKE_ISO_ALLOCATED_IMPL(MessageEvent);

MessageEvent::MessageEvent() = default;

inline MessageEvent::MessageEvent(const AtomString& type, Init&& initializer, IsTrusted isTrusted)
    : Event(type, initializer, isTrusted)
    , m_data(JSValueInWrappedObject { initializer.data })
    , m_origin(initializer.origin)
    , m_lastEventId(initializer.lastEventId)
    , m_source(WTFMove(initializer.source))
    , m_ports(WTFMove(initializer.ports))
{
}

inline MessageEvent::MessageEvent(DataType&& data, const String& origin, const String& lastEventId, Optional<MessageEventSource>&& source, Vector<RefPtr<MessagePort>>&& ports)
    : Event(eventNames().messageEvent, CanBubble::No, IsCancelable::No)
    , m_data(WTFMove(data))
    , m_origin(origin)
    , m_lastEventId(lastEventId)
    , m_source(WTFMove(source))
    , m_ports(WTFMove(ports))
{
}

inline MessageEvent::MessageEvent(const AtomString& type, Ref<SerializedScriptValue>&& data, const String& origin, const String& lastEventId)
    : Event(type, CanBubble::No, IsCancelable::No)
    , m_data(WTFMove(data))
    , m_origin(origin)
    , m_lastEventId(lastEventId)
{
}

Ref<MessageEvent> MessageEvent::create(Vector<RefPtr<MessagePort>>&& ports, Ref<SerializedScriptValue>&& data, const String& origin, const String& lastEventId, Optional<MessageEventSource>&& source)
{
    return adoptRef(*new MessageEvent(WTFMove(data), origin, lastEventId, WTFMove(source), WTFMove(ports)));
}

Ref<MessageEvent> MessageEvent::create(const AtomString& type, Ref<SerializedScriptValue>&& data, const String& origin, const String& lastEventId)
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

Ref<MessageEvent> MessageEvent::create(const AtomString& type, Init&& initializer, IsTrusted isTrusted)
{
    return adoptRef(*new MessageEvent(type, WTFMove(initializer), isTrusted));
}

MessageEvent::~MessageEvent() = default;

void MessageEvent::initMessageEvent(const AtomString& type, bool canBubble, bool cancelable, JSValue data, const String& origin, const String& lastEventId, Optional<MessageEventSource>&& source, Vector<RefPtr<MessagePort>>&& ports)
{
    if (isBeingDispatched())
        return;

    initEvent(type, canBubble, cancelable);

    m_data = JSValueInWrappedObject { data };
    m_cachedData = { };
    m_origin = origin;
    m_lastEventId = lastEventId;
    m_source = WTFMove(source);
    m_ports = WTFMove(ports);
    m_cachedPorts = { };
}

EventInterface MessageEvent::eventInterface() const
{
    return MessageEventInterfaceType;
}

} // namespace WebCore
