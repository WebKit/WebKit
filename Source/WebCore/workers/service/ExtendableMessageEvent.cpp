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

#include "ExtendableMessageEvent.h"

#include "EventNames.h"
#include "JSDOMConvert.h"
#include "JSExtendableMessageEvent.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(ExtendableMessageEvent);

static JSC::Strong<JSC::JSObject> createWrapperAndSetData(JSC::JSGlobalObject& globalObject, ExtendableMessageEvent& event, JSC::JSValue value)
{
    auto& vm = globalObject.vm();
    JSC::Strong<JSC::Unknown> strongData(vm, value);

    Locker<JSC::JSLock> locker(vm.apiLock());
    JSC::Strong<JSC::JSObject> strongWrapper(vm, JSC::jsCast<JSC::JSObject*>(toJSNewlyCreated<IDLInterface<ExtendableMessageEvent>>(globalObject,  *JSC::jsCast<JSDOMGlobalObject*>(&globalObject), Ref { event })));
    event.data().set(vm, strongWrapper.get(), value);

    return strongWrapper;
}

auto ExtendableMessageEvent::create(JSC::JSGlobalObject& globalObject, const AtomString& type, const Init& initializer, IsTrusted isTrusted) -> ExtendableMessageEventWithStrongData
{
    Ref event = adoptRef(*new ExtendableMessageEvent(type, initializer, isTrusted));
    auto strongWrapper = createWrapperAndSetData(globalObject, event.get(), initializer.data);

    return { WTFMove(event), WTFMove(strongWrapper) };
}

auto ExtendableMessageEvent::create(JSC::JSGlobalObject& globalObject, Vector<Ref<MessagePort>>&& ports, Ref<SerializedScriptValue>&& data, const String& origin, const String& lastEventId, std::optional<ExtendableMessageEventSource>&& source) -> ExtendableMessageEventWithStrongData
{
    auto& vm = globalObject.vm();
    Locker<JSC::JSLock> locker(vm.apiLock());

    bool didFail = false;

    auto deserialized = data->deserialize(globalObject, &globalObject, ports, SerializationErrorMode::NonThrowing, &didFail);

    Ref event = adoptRef(*new ExtendableMessageEvent(didFail ? eventNames().messageerrorEvent : eventNames().messageEvent, origin, lastEventId, WTFMove(source), WTFMove(ports)));
    auto strongWrapper = createWrapperAndSetData(globalObject, event.get(), deserialized);

    return { WTFMove(event), WTFMove(strongWrapper) };
}

ExtendableMessageEvent::ExtendableMessageEvent(const AtomString& type, const Init& init, IsTrusted isTrusted)
    : ExtendableEvent(EventInterfaceType::ExtendableMessageEvent, type, init, isTrusted)
    , m_origin(init.origin)
    , m_lastEventId(init.lastEventId)
    , m_source(init.source)
    , m_ports(init.ports)
{
}

ExtendableMessageEvent::ExtendableMessageEvent(const AtomString& type, const String& origin, const String& lastEventId, std::optional<ExtendableMessageEventSource>&& source, Vector<Ref<MessagePort>>&& ports)
    : ExtendableEvent(EventInterfaceType::ExtendableMessageEvent, type, CanBubble::No, IsCancelable::No)
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
