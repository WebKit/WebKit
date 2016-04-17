/*
 * Copyright (C) 2016 Canon Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Canon Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "ReadableStreamController.h"

#if ENABLE(STREAMS_API)

#include "JSReadableStream.h"
#include "JSReadableStreamSource.h"
#include "WebCoreJSClientData.h"

namespace WebCore {

static inline JSC::JSValue callFunction(JSC::ExecState& state, JSC::JSValue jsFunction, JSC::JSValue thisValue, const JSC::ArgList& arguments)
{
    JSC::CallData callData;
    JSC::CallType callType = JSC::getCallData(jsFunction, callData);
    return JSC::call(&state, jsFunction, callType, callData, thisValue, arguments);
}

JSC::JSValue ReadableStreamController::invoke(JSC::ExecState& state, JSC::JSObject& object, const char* propertyName, JSC::JSValue parameter)
{
    JSC::JSLockHolder lock(&state);

    JSC::JSValue function = getPropertyFromObject(state, object, propertyName);
    if (state.hadException())
        return JSC::jsUndefined();

    if (!function.isFunction()) {
        if (!function.isUndefined())
            throwVMError(&state, createTypeError(&state, ASCIILiteral("ReadableStream trying to call a property that is not callable")));
        return JSC::jsUndefined();
    }

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(parameter);

    return callFunction(state, function, &object, arguments);
}

bool ReadableStreamController::isControlledReadableStreamLocked() const
{
    JSC::ExecState& state = *globalObject()->globalExec();
    JSC::JSLockHolder lock(&state);

    JSVMClientData& clientData = *static_cast<JSVMClientData*>(state.vm().clientData);
    JSC::JSValue readableStream = m_jsController->get(&state, clientData.builtinNames().controlledReadableStreamPrivateName());
    ASSERT(!state.hadException());

    JSC::JSValue isLocked = globalObject()->builtinInternalFunctions().readableStreamInternals().m_isReadableStreamLockedFunction.get();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(readableStream);
    JSC::JSValue result = callFunction(state, isLocked, JSC::jsUndefined(), arguments);
    ASSERT(!state.hadException());

    return result.isTrue();
}

JSC::JSValue createReadableStream(JSC::ExecState& state, JSDOMGlobalObject* globalObject, ReadableStreamSource* source)
{
    if (!source)
        return JSC::jsNull();

    JSC::JSLockHolder lock(&state);

    JSC::JSValue jsSource = toJS(&state, globalObject, source);
    JSC::Strong<JSC::Unknown> protect(state.vm(), jsSource);

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(jsSource);

    JSC::JSValue constructor = JSReadableStream::getConstructor(state.vm(), globalObject);

    JSC::ConstructData constructData;
    JSC::ConstructType constructType = JSC::getConstructData(constructor, constructData);
    ASSERT(constructType != JSC::ConstructType::None);

    return JSC::construct(&state, constructor, constructType, constructData, arguments);
}

} // namespace WebCore

#endif // ENABLE(STREAMS_API)
