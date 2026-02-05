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
#include "JSMessageEvent.h"
#include "SecurityOrigin.h"
#include <JavaScriptCore/JSCInlines.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace JSC;

WTF_MAKE_TZONE_ALLOCATED_IMPL(MessageEvent);

MessageEvent::MessageEvent()
    : Event(EventInterfaceType::MessageEvent)
{
}

inline MessageEvent::MessageEvent(const AtomString& type, Init&& initializer, IsTrusted isTrusted)
    : Event(EventInterfaceType::MessageEvent, type, initializer, isTrusted)
    , m_data(JSValueTag { })
    , m_origin(initializer.origin)
    , m_lastEventId(initializer.lastEventId)
    , m_source(WTF::move(initializer.source))
    , m_ports(WTF::move(initializer.ports))
    , m_jsData(initializer.data)
{
}

inline MessageEvent::MessageEvent(const AtomString& type, DataType&& data, RefPtr<SecurityOrigin>&& origin, const String& lastEventId, std::optional<MessageEventSource>&& source, Vector<Ref<MessagePort>>&& ports)
    : Event(EventInterfaceType::MessageEvent, type, CanBubble::No, IsCancelable::No)
    , m_data(WTF::move(data))
    , m_origin(WTF::move(origin))
    , m_lastEventId(lastEventId)
    , m_source(WTF::move(source))
    , m_ports(WTF::move(ports))
{
}

auto MessageEvent::create(JSC::JSGlobalObject& globalObject, Ref<SerializedScriptValue>&& data, RefPtr<SecurityOrigin>&& origin, const String& lastEventId, std::optional<MessageEventSource>&& source, Vector<Ref<MessagePort>>&& ports) -> MessageEventWithStrongData
{
    auto& vm = globalObject.vm();
    Locker<JSC::JSLock> locker(vm.apiLock());
    auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(vm);

    bool didFail = false;

    auto deserialized = data->deserialize(globalObject, &globalObject, ports, SerializationErrorMode::NonThrowing, &didFail);
    if (catchScope.exception()) [[unlikely]]
        deserialized = jsUndefined();
    JSC::Strong<JSC::Unknown> strongData(vm, deserialized);

    auto& eventType = didFail ? eventNames().messageerrorEvent : eventNames().messageEvent;
    Ref event = adoptRef(*new MessageEvent(eventType, MessageEvent::JSValueTag { }, WTF::move(origin), lastEventId, WTF::move(source), WTF::move(ports)));
    JSC::Strong<JSC::JSObject> strongWrapper(vm, JSC::jsCast<JSC::JSObject*>(toJS(&globalObject, JSC::jsCast<JSDOMGlobalObject*>(&globalObject), event.get())));
    event->jsData().set(vm, strongWrapper.get(), deserialized);

    return MessageEventWithStrongData { event, WTF::move(strongWrapper) };
}

Ref<MessageEvent> MessageEvent::create(const AtomString& type, DataType&& data, RefPtr<SecurityOrigin>&& origin, const String& lastEventId, std::optional<MessageEventSource>&& source, Vector<Ref<MessagePort>>&& ports)
{
    return adoptRef(*new MessageEvent(type, WTF::move(data), WTF::move(origin), lastEventId, WTF::move(source), WTF::move(ports)));
}

Ref<MessageEvent> MessageEvent::create(DataType&& data, RefPtr<SecurityOrigin>&& origin, const String& lastEventId, std::optional<MessageEventSource>&& source, Vector<Ref<MessagePort>>&& ports)
{
    return create(eventNames().messageEvent, WTF::move(data), WTF::move(origin), lastEventId, WTF::move(source), WTF::move(ports));
}

Ref<MessageEvent> MessageEvent::createForBindings()
{
    return adoptRef(*new MessageEvent);
}

Ref<MessageEvent> MessageEvent::create(const AtomString& type, Init&& initializer, IsTrusted isTrusted)
{
    return adoptRef(*new MessageEvent(type, WTF::move(initializer), isTrusted));
}

MessageEvent::~MessageEvent() = default;

String MessageEvent::origin() const
{
    return WTF::switchOn(m_origin, [](const RefPtr<SecurityOrigin>& origin) {
        return origin ? origin->toString() : emptyString();
    },
    [](const String& origin) {
        return origin;
    });
}

const RefPtr<SecurityOrigin> MessageEvent::securityOrigin() const
{
    auto* origin = std::get_if<RefPtr<SecurityOrigin>>(&m_origin);
    return origin ? *origin : nullptr;
}

void MessageEvent::initMessageEvent(const AtomString& type, bool canBubble, bool cancelable, JSValue data, const String& origin, const String& lastEventId, std::optional<MessageEventSource>&& source, Vector<Ref<MessagePort>>&& ports)
{
    if (isBeingDispatched())
        return;

    initEvent(type, canBubble, cancelable);

    {
        Locker locker { m_concurrentDataAccessLock };
        m_data = JSValueTag { };
    }
    // FIXME: This code is wrong: we should emit a write-barrier. Otherwise, GC can collect it.
    // https://bugs.webkit.org/show_bug.cgi?id=236353
    m_jsData.setWeakly(data);
    m_cachedData.clear();
    m_origin = origin;
    m_lastEventId = lastEventId;
    m_source = WTF::move(source);
    m_ports = WTF::move(ports);
    m_cachedPorts.clear();
}

size_t MessageEvent::memoryCost() const
{
    Locker locker { m_concurrentDataAccessLock };
    return WTF::switchOn(m_data, [](JSValueTag) -> size_t {
        return 0;
    }, [](const String& string) -> size_t {
        return string.sizeInBytes();
    }, [](const Ref<Blob>& blob) -> size_t {
        return blob->memoryCost();
    }, [](const Ref<ArrayBuffer>& buffer) -> size_t {
        return buffer->byteLength();
    });
}

} // namespace WebCore
