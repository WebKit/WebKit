/*
 * Copyright (C) 2008, 2016 Apple Inc. All rights reserved.
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

#include "Frame.h"
#include "JSDOMConvertNullable.h"
#include "JSDOMConvertStrings.h"
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
    JSValue result = serialized ? serialized->deserialize(state, globalObject()) : jsNull();
    m_state.set(state.vm(), this, result);
    return result;
}

JSValue JSHistory::pushState(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto argCount = state.argumentCount();
    if (UNLIKELY(argCount < 2))
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    auto historyState = SerializedScriptValue::create(state, state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, JSValue());

    // FIXME: title should not be nullable.
    String title = convert<IDLNullable<IDLDOMString>>(state, state.uncheckedArgument(1));
    RETURN_IF_EXCEPTION(scope, JSValue());

    String url;
    if (argCount > 2) {
        url = convert<IDLNullable<IDLUSVString>>(state, state.uncheckedArgument(2));
        RETURN_IF_EXCEPTION(scope, JSValue());
    }

    propagateException(state, scope, wrapped().stateObjectAdded(WTFMove(historyState), title, url, History::StateObjectType::Push));

    m_state.clear();

    return jsUndefined();
}

JSValue JSHistory::replaceState(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto argCount = state.argumentCount();
    if (UNLIKELY(argCount < 2))
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    auto historyState = SerializedScriptValue::create(state, state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, JSValue());

    // FIXME: title should not be nullable.
    String title = convert<IDLNullable<IDLDOMString>>(state, state.uncheckedArgument(1));
    RETURN_IF_EXCEPTION(scope, JSValue());

    String url;
    if (argCount > 2) {
        url = convert<IDLNullable<IDLUSVString>>(state, state.uncheckedArgument(2));
        RETURN_IF_EXCEPTION(scope, JSValue());
    }

    propagateException(state, scope, wrapped().stateObjectAdded(WTFMove(historyState), title, url, History::StateObjectType::Replace));

    m_state.clear();

    return jsUndefined();
}

} // namespace WebCore
