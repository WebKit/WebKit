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

#include "ExtendableEvent.h"
#include "ExtendableEventInit.h"
#include "JSValueInWrappedObject.h"
#include "MessagePort.h"
#include "ServiceWorker.h"
#include "ServiceWorkerClient.h"
#include <variant>

namespace JSC {
class CallFrame;
class JSValue;
}

namespace WebCore {

class MessagePort;
class ServiceWorker;
class ServiceWorkerClient;

using ExtendableMessageEventSource = std::variant<RefPtr<ServiceWorkerClient>, RefPtr<ServiceWorker>, RefPtr<MessagePort>>;

class ExtendableMessageEvent final : public ExtendableEvent {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(ExtendableMessageEvent);
public:
    struct Init : ExtendableEventInit {
        JSC::JSValue data;
        String origin;
        String lastEventId;
        std::optional<ExtendableMessageEventSource> source;
        Vector<Ref<MessagePort>> ports;
    };

    struct ExtendableMessageEventWithStrongData {
        Ref<ExtendableMessageEvent> event;
        JSC::Strong<JSC::JSObject> strongWrapper; // Keep the wrapper alive until the event is fired, since it is what keeps `data` alive.
    };

    static ExtendableMessageEventWithStrongData create(JSC::JSGlobalObject&, const AtomString& type, const Init&, IsTrusted = IsTrusted::No);
    static ExtendableMessageEventWithStrongData create(JSC::JSGlobalObject&, Vector<Ref<MessagePort>>&&, Ref<SerializedScriptValue>&&, const String& origin, const String& lastEventId, std::optional<ExtendableMessageEventSource>&&);

    ~ExtendableMessageEvent();

    JSValueInWrappedObject& data() { return m_data; }
    JSValueInWrappedObject& cachedPorts() { return m_cachedPorts; }

    const String& origin() const { return m_origin; }
    const String& lastEventId() const { return m_lastEventId; }
    const std::optional<ExtendableMessageEventSource>& source() const { return m_source; }
    const Vector<Ref<MessagePort>>& ports() const { return m_ports; }

private:
    ExtendableMessageEvent(const AtomString&, const Init&, IsTrusted);
    ExtendableMessageEvent(const AtomString&, const String& origin, const String& lastEventId, std::optional<ExtendableMessageEventSource>&&, Vector<Ref<MessagePort>>&&);

    JSValueInWrappedObject m_data;
    String m_origin;
    String m_lastEventId;
    std::optional<ExtendableMessageEventSource> m_source;
    Vector<Ref<MessagePort>> m_ports;
    JSValueInWrappedObject m_cachedPorts;
};

} // namespace WebCore
