/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSPopStateEvent.h"

#include "DOMWrapperWorld.h"
#include "JSHistory.h"
#include <heap/HeapInlines.h>
#include <runtime/JSCJSValueInlines.h>

using namespace JSC;

namespace WebCore {

// Save the state value to the m_state member of a JSPopStateEvent, and return it, for convenience.
static const JSValue& cacheState(ExecState& state, const JSPopStateEvent* event, const JSValue& eventState)
{
    event->m_state.set(state.vm(), event, eventState);
    return eventState;
}

JSValue JSPopStateEvent::state(ExecState& state) const
{
    JSValue cachedValue = m_state.get();
    if (!cachedValue.isEmpty()) {
        // We cannot use a cached object if we are in a different world than the one it was created in.
        if (!cachedValue.isObject() || &worldForDOMObject(cachedValue.getObject()) == &currentWorld(&state))
            return cachedValue;
        ASSERT_NOT_REACHED();
    }

    PopStateEvent& event = wrapped();

    if (auto eventState = event.state()) {
        // We need to make sure a PopStateEvent does not leak objects in its state property across isolated DOM worlds.
        // Ideally, we would check that the worlds have different privileges but that's not possible yet.
        if (eventState.isObject() && &worldForDOMObject(eventState.getObject()) != &currentWorld(&state)) {
            if (auto serializedValue = event.trySerializeState(&state))
                eventState = serializedValue->deserialize(&state, globalObject(), nullptr);
            else
                eventState = jsNull();
        }
        return cacheState(state, this, eventState);
    }
    
    History* history = event.history();
    if (!history || !event.serializedState())
        return cacheState(state, this, jsNull());

    // There's no cached value from a previous invocation, nor a state value was provided by the
    // event, but there is a history object, so first we need to see if the state object has been
    // deserialized through the history object already.
    // The current history state object might've changed in the meantime, so we need to take care
    // of using the correct one, and always share the same deserialization with history.state.

    bool isSameState = history->isSameAsCurrentState(event.serializedState());
    JSValue result;

    if (isSameState) {
        JSHistory* jsHistory = jsCast<JSHistory*>(toJS(&state, globalObject(), *history).asCell());
        result = jsHistory->state(state);
    } else
        result = event.serializedState()->deserialize(&state, globalObject(), 0);

    return cacheState(state, this, result);
}

} // namespace WebCore
