/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSHistory.h"

#include "ExceptionCode.h"
#include "Frame.h"
#include "JSDOMBinding.h"
#include "SerializedScriptValue.h"
#include <runtime/JSFunction.h>

using namespace JSC;

namespace WebCore {

JSValue JSHistory::state(ExecState& state) const
{
    History& history = wrapped();

    JSValue cachedValue = m_state.get();
    if (!cachedValue.isEmpty() && !history.stateChanged())
        return cachedValue;

    RefPtr<SerializedScriptValue> serialized = history.state();
    JSValue result = serialized ? serialized->deserialize(&state, globalObject(), 0) : jsNull();
    m_state.set(state.vm(), this, result);
    return result;
}

JSValue JSHistory::pushState(ExecState& state)
{
    RefPtr<SerializedScriptValue> historyState = SerializedScriptValue::create(&state, state.argument(0), 0, 0);
    if (state.hadException())
        return jsUndefined();

    String title = valueToStringWithUndefinedOrNullCheck(&state, state.argument(1));
    if (state.hadException())
        return jsUndefined();

    String url;
    if (state.argumentCount() > 2) {
        url = valueToStringWithUndefinedOrNullCheck(&state, state.argument(2));
        if (state.hadException())
            return jsUndefined();
    }

    ExceptionCodeWithMessage ec;
    wrapped().stateObjectAdded(historyState.release(), title, url, History::StateObjectType::Push, ec);
    setDOMException(&state, ec);

    m_state.clear();

    return jsUndefined();
}

JSValue JSHistory::replaceState(ExecState& state)
{
    RefPtr<SerializedScriptValue> historyState = SerializedScriptValue::create(&state, state.argument(0), 0, 0);
    if (state.hadException())
        return jsUndefined();

    String title = valueToStringWithUndefinedOrNullCheck(&state, state.argument(1));
    if (state.hadException())
        return jsUndefined();

    String url;
    if (state.argumentCount() > 2) {
        url = valueToStringWithUndefinedOrNullCheck(&state, state.argument(2));
        if (state.hadException())
            return jsUndefined();
    }

    ExceptionCodeWithMessage ec;
    wrapped().stateObjectAdded(historyState.release(), title, url, History::StateObjectType::Replace, ec);
    setDOMException(&state, ec);

    m_state.clear();

    return jsUndefined();
}

} // namespace WebCore
