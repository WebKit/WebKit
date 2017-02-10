/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
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
#include "PopStateEvent.h"

#include "EventNames.h"
#include "History.h"
#include <runtime/JSCInlines.h>

namespace WebCore {

PopStateEvent::PopStateEvent(JSC::ExecState& state, const AtomicString& type, const Init& initializer, IsTrusted isTrusted)
    : Event(type, initializer, isTrusted)
    , m_state(state.vm(), initializer.state)
{
}

PopStateEvent::PopStateEvent(RefPtr<SerializedScriptValue>&& serializedState, History* history)
    : Event(eventNames().popstateEvent, false, true)
    , m_serializedState(WTFMove(serializedState))
    , m_history(history)
{
}

PopStateEvent::~PopStateEvent()
{
}

Ref<PopStateEvent> PopStateEvent::create(RefPtr<SerializedScriptValue>&& serializedState, History* history)
{
    return adoptRef(*new PopStateEvent(WTFMove(serializedState), history));
}

Ref<PopStateEvent> PopStateEvent::create(JSC::ExecState& state, const AtomicString& type, const Init& initializer, IsTrusted isTrusted)
{
    return adoptRef(*new PopStateEvent(state, type, initializer, isTrusted));
}

Ref<PopStateEvent> PopStateEvent::createForBindings()
{
    return adoptRef(*new PopStateEvent);
}

RefPtr<SerializedScriptValue> PopStateEvent::trySerializeState(JSC::ExecState& executionState)
{
    ASSERT(!m_state.hasNoValue());
    
    if (!m_serializedState && !m_triedToSerialize) {
        m_serializedState = SerializedScriptValue::create(executionState, m_state.jsValue(), SerializationErrorMode::NonThrowing);
        m_triedToSerialize = true;
    }
    
    return m_serializedState;
}

EventInterface PopStateEvent::eventInterface() const
{
    return PopStateEventInterfaceType;
}

} // namespace WebCore
