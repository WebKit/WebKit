/*
 * Copyright (C) 2007 Henry Mason (hmason@mac.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#pragma once

#include "DOMWindow.h"
#include "Event.h"
#include "MessagePort.h"
#include "SerializedScriptValue.h"
#include "ServiceWorker.h"
#include <bindings/ScriptValue.h>
#include <wtf/Variant.h>

namespace WebCore {

class Blob;

#if ENABLE(SERVICE_WORKER)
using MessageEventSource = Variant<RefPtr<DOMWindow>, RefPtr<MessagePort>, RefPtr<ServiceWorker>>;
#else
using MessageEventSource = Variant<RefPtr<DOMWindow>, RefPtr<MessagePort>>;
#endif

class MessageEvent final : public Event {
public:
    static Ref<MessageEvent> create(Vector<RefPtr<MessagePort>>&&, RefPtr<SerializedScriptValue>&&, const String& origin = { }, const String& lastEventId = { }, std::optional<MessageEventSource>&& source = std::nullopt);
    static Ref<MessageEvent> create(const AtomicString& type, RefPtr<SerializedScriptValue>&&, const String& origin, const String& lastEventId);
    static Ref<MessageEvent> create(const String& data, const String& origin = { });
    static Ref<MessageEvent> create(Ref<Blob>&& data, const String& origin);
    static Ref<MessageEvent> create(Ref<ArrayBuffer>&& data, const String& origin = { });
    static Ref<MessageEvent> createForBindings();

    struct Init : EventInit {
        JSC::JSValue data;
        String origin;
        String lastEventId;
        std::optional<MessageEventSource> source;
        Vector<RefPtr<MessagePort>> ports;
    };
    static Ref<MessageEvent> create(JSC::ExecState&, const AtomicString& type, Init&&, IsTrusted = IsTrusted::No);

    virtual ~MessageEvent();

    void initMessageEvent(JSC::ExecState&, const AtomicString& type, bool canBubble, bool cancelable, JSC::JSValue data, const String& origin, const String& lastEventId, std::optional<MessageEventSource>&&, Vector<RefPtr<MessagePort>>&&);

    const String& origin() const { return m_origin; }
    const String& lastEventId() const { return m_lastEventId; }
    const std::optional<MessageEventSource>& source() const { return m_source; }
    const Vector<RefPtr<MessagePort>>& ports() const { return m_ports; }

    // FIXME: Remove this when we have custom ObjC binding support.
    SerializedScriptValue* data() const;

    enum DataType {
        DataTypeScriptValue,
        DataTypeSerializedScriptValue,
        DataTypeString,
        DataTypeBlob,
        DataTypeArrayBuffer
    };
    DataType dataType() const { return m_dataType; }
    JSC::JSValue dataAsScriptValue() const { ASSERT(m_dataType == DataTypeScriptValue); return m_dataAsScriptValue; }
    SerializedScriptValue* dataAsSerializedScriptValue() const { ASSERT(m_dataType == DataTypeSerializedScriptValue); return m_dataAsSerializedScriptValue.get(); }
    String dataAsString() const { ASSERT(m_dataType == DataTypeString); return m_dataAsString; }
    Blob* dataAsBlob() const { ASSERT(m_dataType == DataTypeBlob); return m_dataAsBlob.get(); }
    ArrayBuffer* dataAsArrayBuffer() const { ASSERT(m_dataType == DataTypeArrayBuffer); return m_dataAsArrayBuffer.get(); }

    RefPtr<SerializedScriptValue> trySerializeData(JSC::ExecState*);

private:
    MessageEvent();
    MessageEvent(JSC::ExecState&, const AtomicString&, Init&&, IsTrusted);
    MessageEvent(RefPtr<SerializedScriptValue>&& data, const String& origin, const String& lastEventId, std::optional<MessageEventSource>&&, Vector<RefPtr<MessagePort>>&&);
    MessageEvent(const AtomicString& type, RefPtr<SerializedScriptValue>&& data, const String& origin, const String& lastEventId);
    MessageEvent(const String& data, const String& origin);
    MessageEvent(Ref<Blob>&& data, const String& origin);
    MessageEvent(Ref<ArrayBuffer>&& data, const String& origin);

    EventInterface eventInterface() const final;

    DataType m_dataType;
    Deprecated::ScriptValue m_dataAsScriptValue;
    RefPtr<SerializedScriptValue> m_dataAsSerializedScriptValue;
    bool m_triedToSerialize { false };
    String m_dataAsString;
    RefPtr<Blob> m_dataAsBlob;
    RefPtr<ArrayBuffer> m_dataAsArrayBuffer;
    String m_origin;
    String m_lastEventId;
    std::optional<MessageEventSource> m_source;
    Vector<RefPtr<MessagePort>> m_ports;
};

} // namespace WebCore
