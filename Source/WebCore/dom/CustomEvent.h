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

#pragma once

#include "Event.h"
#include "SerializedScriptValue.h"
#include <JavaScriptCore/ScriptValue.h>

namespace WebCore {

class CustomEvent final : public Event {
public:
    virtual ~CustomEvent();

    static Ref<CustomEvent> create(IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new CustomEvent(isTrusted));
    }

    struct Init : EventInit {
        JSC::JSValue detail;
    };

    static Ref<CustomEvent> create(JSC::ExecState& state, const AtomicString& type, const Init& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new CustomEvent(state, type, initializer, isTrusted));
    }

    void initCustomEvent(JSC::ExecState&, const AtomicString& type, bool canBubble, bool cancelable, JSC::JSValue detail = JSC::JSValue::JSUndefined);

    EventInterface eventInterface() const override;

    JSC::JSValue detail() const { return m_detail.jsValue(); }
    
    RefPtr<SerializedScriptValue> trySerializeDetail(JSC::ExecState&);

private:
    CustomEvent(IsTrusted);
    CustomEvent(JSC::ExecState&, const AtomicString& type, const Init& initializer, IsTrusted);

    Deprecated::ScriptValue m_detail; // FIXME: Why is it OK to use a strong reference here? What prevents a reference cycle?
    RefPtr<SerializedScriptValue> m_serializedDetail;
    bool m_triedToSerialize { false };
};

} // namespace WebCore
