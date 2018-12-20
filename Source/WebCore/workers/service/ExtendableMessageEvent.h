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

#pragma once

#if ENABLE(SERVICE_WORKER)

#include "ExtendableEvent.h"
#include "ExtendableEventInit.h"
#include "MessagePort.h"
#include "ServiceWorker.h"
#include "ServiceWorkerClient.h"
#include <wtf/Variant.h>

namespace JSC {
class ExecState;
class JSValue;
}

namespace WebCore {

class MessagePort;
class ServiceWorker;
class ServiceWorkerClient;

using ExtendableMessageEventSource = Variant<RefPtr<ServiceWorkerClient>, RefPtr<ServiceWorker>, RefPtr<MessagePort>>;

class ExtendableMessageEvent final : public ExtendableEvent {
public:
    struct Init : ExtendableEventInit {
        JSC::JSValue data;
        String origin;
        String lastEventId;
        Optional<ExtendableMessageEventSource> source;
        Vector<RefPtr<MessagePort>> ports;
    };

    static Ref<ExtendableMessageEvent> create(JSC::ExecState& state, const AtomicString& type, const Init& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new ExtendableMessageEvent(state, type, initializer, isTrusted));
    }

    static Ref<ExtendableMessageEvent> create(Vector<RefPtr<MessagePort>>&&, RefPtr<SerializedScriptValue>&&, const String& origin = { }, const String& lastEventId = { }, Optional<ExtendableMessageEventSource>&& source = WTF::nullopt);

    ~ExtendableMessageEvent();

    SerializedScriptValue* data() const { return m_data.get(); }
    const String& origin() const { return m_origin; }
    const String& lastEventId() const { return m_lastEventId; }
    const Optional<ExtendableMessageEventSource>& source() const { return m_source; }
    const Vector<RefPtr<MessagePort>>& ports() const { return m_ports; }

    EventInterface eventInterface() const final { return ExtendableMessageEventInterfaceType; }

private:
    ExtendableMessageEvent(JSC::ExecState&, const AtomicString&, const Init&, IsTrusted);
    ExtendableMessageEvent(RefPtr<SerializedScriptValue>&& data, const String& origin, const String& lastEventId, Optional<ExtendableMessageEventSource>&&, Vector<RefPtr<MessagePort>>&&);

    RefPtr<SerializedScriptValue> m_data;
    String m_origin;
    String m_lastEventId;
    Optional<ExtendableMessageEventSource> m_source;
    Vector<RefPtr<MessagePort>> m_ports;
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
