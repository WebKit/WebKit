/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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
 */

#ifndef CustomEvent_h
#define CustomEvent_h

#include "Event.h"
#include "SerializedScriptValue.h"
#include <bindings/ScriptValue.h>

namespace WebCore {

struct CustomEventInit : public EventInit {
    Deprecated::ScriptValue detail;
};

class CustomEvent final : public Event {
public:
    virtual ~CustomEvent();

    static Ref<CustomEvent> createForBindings()
    {
        return adoptRef(*new CustomEvent);
    }

    static Ref<CustomEvent> createForBindings(const AtomicString& type, const CustomEventInit& initializer)
    {
        return adoptRef(*new CustomEvent(type, initializer));
    }

    void initCustomEvent(JSC::ExecState&, const AtomicString& type, bool canBubble, bool cancelable, JSC::JSValue detail);

    EventInterface eventInterface() const override;

    JSC::JSValue detail() const { return m_detail.jsValue(); }
    
    RefPtr<SerializedScriptValue> trySerializeDetail(JSC::ExecState&);
    void visitAdditionalChildren(JSC::SlotVisitor&);

private:
    CustomEvent();
    CustomEvent(const AtomicString& type, const CustomEventInit& initializer);

    Deprecated::ScriptValue m_detail; // FIXME: Why is it OK to use a strong reference here? What prevents a reference cycle?
    RefPtr<SerializedScriptValue> m_serializedDetail;
    bool m_triedToSerialize { false };
};

} // namespace WebCore

#endif // CustomEvent_h
