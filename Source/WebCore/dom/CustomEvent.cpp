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

#include "config.h"
#include "CustomEvent.h"

#include <JavaScriptCore/JSCInlines.h>

namespace WebCore {

CustomEvent::CustomEvent(IsTrusted isTrusted)
    : Event(isTrusted)
{
}

CustomEvent::CustomEvent(JSC::ExecState& state, const AtomicString& type, const Init& initializer, IsTrusted isTrusted)
    : Event(type, initializer, isTrusted)
    , m_detail(state.vm(), initializer.detail)
{
}

CustomEvent::~CustomEvent() = default;

void CustomEvent::initCustomEvent(JSC::ExecState& state, const AtomicString& type, bool canBubble, bool cancelable, JSC::JSValue detail)
{
    if (isBeingDispatched())
        return;

    initEvent(type, canBubble, cancelable);

    m_detail = { state.vm(), detail };
    m_serializedDetail = nullptr;
    m_triedToSerialize = false;
}

RefPtr<SerializedScriptValue> CustomEvent::trySerializeDetail(JSC::ExecState& state)
{
    if (!m_triedToSerialize) {
        m_serializedDetail = SerializedScriptValue::create(state, m_detail, SerializationErrorMode::NonThrowing);
        m_triedToSerialize = true;
    }
    return m_serializedDetail;
}

EventInterface CustomEvent::eventInterface() const
{
    return CustomEventInterfaceType;
}

} // namespace WebCore
