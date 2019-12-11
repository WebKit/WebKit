/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(SERVICE_WORKER)
#include "ExtendableMessageEvent.h"

#include "EventNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ExtendableMessageEvent);

Ref<ExtendableMessageEvent> ExtendableMessageEvent::create(Vector<RefPtr<MessagePort>>&& ports, RefPtr<SerializedScriptValue>&& data, const String& origin, const String& lastEventId, Optional<ExtendableMessageEventSource>&& source)
{
    return adoptRef(*new ExtendableMessageEvent(WTFMove(data), origin, lastEventId, WTFMove(source), WTFMove(ports)));
}

ExtendableMessageEvent::ExtendableMessageEvent(JSC::JSGlobalObject& state, const AtomString& type, const Init& init, IsTrusted isTrusted)
    : ExtendableEvent(type, init, isTrusted)
    , m_data(SerializedScriptValue::create(state, init.data, SerializationErrorMode::NonThrowing))
    , m_origin(init.origin)
    , m_lastEventId(init.lastEventId)
    , m_source(init.source)
    , m_ports(init.ports)
{
}

ExtendableMessageEvent::ExtendableMessageEvent(RefPtr<SerializedScriptValue>&& data, const String& origin, const String& lastEventId, Optional<ExtendableMessageEventSource>&& source, Vector<RefPtr<MessagePort>>&& ports)
    : ExtendableEvent(eventNames().messageEvent, CanBubble::No, IsCancelable::No)
    , m_data(WTFMove(data))
    , m_origin(origin)
    , m_lastEventId(lastEventId)
    , m_source(WTFMove(source))
    , m_ports(WTFMove(ports))
{
}

ExtendableMessageEvent::~ExtendableMessageEvent()
{
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
