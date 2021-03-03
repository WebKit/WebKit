/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSCJSValueInlines.h>

namespace WebCore {
using namespace JSC;

JSValue JSPopStateEvent::state(JSGlobalObject& lexicalGlobalObject) const
{
    if (m_state) {
        // We cannot use a cached object if we are in a different world than the one it was created in.
        if (isWorldCompatible(lexicalGlobalObject, m_state.get()))
            return m_state.get();
        ASSERT_NOT_REACHED();
    }

    // Save the lexicalGlobalObject value to the m_state member of a JSPopStateEvent, and return it, for convenience.
    auto cacheState = [&lexicalGlobalObject, this] (JSC::JSValue eventState) {
        m_state.set(lexicalGlobalObject.vm(), this, eventState);
        return eventState;
    };

    PopStateEvent& event = wrapped();

    if (JSC::JSValue eventState = event.state()) {
        // We need to make sure a PopStateEvent does not leak objects in its lexicalGlobalObject property across isolated DOM worlds.
        // Ideally, we would check that the worlds have different privileges but that's not possible yet.
        if (!isWorldCompatible(lexicalGlobalObject, eventState)) {
            if (auto serializedValue = event.trySerializeState(lexicalGlobalObject))
                eventState = serializedValue->deserialize(lexicalGlobalObject, globalObject());
            else
                eventState = jsNull();
        }
        return cacheState(eventState);
    }
    
    History* history = event.history();
    if (!history || !event.serializedState())
        return cacheState(jsNull());

    // There's no cached value from a previous invocation, nor a lexicalGlobalObject value was provided by the
    // event, but there is a history object, so first we need to see if the lexicalGlobalObject object has been
    // deserialized through the history object already.
    // The current history lexicalGlobalObject object might've changed in the meantime, so we need to take care
    // of using the correct one, and always share the same deserialization with history.lexicalGlobalObject.

    bool isSameState = history->isSameAsCurrentState(event.serializedState());
    JSValue result;

    if (isSameState) {
        JSHistory* jsHistory = jsCast<JSHistory*>(toJS(&lexicalGlobalObject, globalObject(), *history).asCell());
        result = jsHistory->state(lexicalGlobalObject);
    } else
        result = event.serializedState()->deserialize(lexicalGlobalObject, globalObject());

    return cacheState(result);
}

template<typename Visitor>
void JSPopStateEvent::visitAdditionalChildren(Visitor& visitor)
{
    wrapped().state().visit(visitor);
}

DEFINE_VISIT_ADDITIONAL_CHILDREN(JSPopStateEvent);

} // namespace WebCore
