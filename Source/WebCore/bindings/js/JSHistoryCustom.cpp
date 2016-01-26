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

bool JSHistory::getOwnPropertySlotDelegate(ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    // When accessing History cross-domain, functions are always the native built-in ones.
    // See JSDOMWindow::getOwnPropertySlotDelegate for additional details.

    // Our custom code is only needed to implement the Window cross-domain scheme, so if access is
    // allowed, return false so the normal lookup will take place.
    String message;
    if (shouldAllowAccessToFrame(exec, wrapped().frame(), message))
        return false;

    // Check for the few functions that we allow, even when called cross-domain.
    // Make these read-only / non-configurable to prevent writes via defineProperty.
    if (propertyName == exec->propertyNames().back) {
        slot.setCustom(this, ReadOnly | DontDelete | DontEnum, nonCachingStaticFunctionGetter<jsHistoryPrototypeFunctionBack, 0>);
        return true;
    }
    if (propertyName == exec->propertyNames().forward) {
        slot.setCustom(this, ReadOnly | DontDelete | DontEnum, nonCachingStaticFunctionGetter<jsHistoryPrototypeFunctionForward, 0>);
        return true;
    }
    if (propertyName == exec->propertyNames().go) {
        slot.setCustom(this, ReadOnly | DontDelete | DontEnum, nonCachingStaticFunctionGetter<jsHistoryPrototypeFunctionGo, 1>);
        return true;
    }
    // Allow access to toString() cross-domain, but always Object.toString.
    if (propertyName == exec->propertyNames().toString) {
        slot.setCustom(this, ReadOnly | DontDelete | DontEnum, objectToStringFunctionGetter);
        return true;
    }

    printErrorMessageForFrame(wrapped().frame(), message);
    slot.setUndefined();
    return true;
}

bool JSHistory::putDelegate(ExecState* exec, PropertyName, JSValue, PutPropertySlot&)
{
    if (!shouldAllowAccessToFrame(exec, wrapped().frame()))
        return true;
    return false;
}

bool JSHistory::deleteProperty(JSCell* cell, ExecState* exec, PropertyName propertyName)
{
    JSHistory* thisObject = jsCast<JSHistory*>(cell);
    if (!shouldAllowAccessToFrame(exec, thisObject->wrapped().frame()))
        return false;
    return Base::deleteProperty(thisObject, exec, propertyName);
}

bool JSHistory::deletePropertyByIndex(JSCell* cell, ExecState* exec, unsigned propertyName)
{
    JSHistory* thisObject = jsCast<JSHistory*>(cell);
    if (!shouldAllowAccessToFrame(exec, thisObject->wrapped().frame()))
        return false;
    return Base::deletePropertyByIndex(thisObject, exec, propertyName);
}

void JSHistory::getOwnPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSHistory* thisObject = jsCast<JSHistory*>(object);
    if (!shouldAllowAccessToFrame(exec, thisObject->wrapped().frame()))
        return;
    Base::getOwnPropertyNames(thisObject, exec, propertyNames, mode);
}

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
    if (!shouldAllowAccessToFrame(&state, wrapped().frame()))
        return jsUndefined();

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
    if (!shouldAllowAccessToFrame(&state, wrapped().frame()))
        return jsUndefined();

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
