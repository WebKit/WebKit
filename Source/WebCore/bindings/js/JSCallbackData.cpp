/*
 * Copyright (C) 2007-2009, 2015 Apple Inc. All rights reserved.
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
#include "JSCallbackData.h"

#include "Document.h"
#include "JSDOMBinding.h"
#include "JSMainThreadExecState.h"
#include "JSMainThreadExecStateInstrumentation.h"
#include <runtime/Exception.h>

using namespace JSC;
    
namespace WebCore {

JSValue JSCallbackData::invokeCallback(JSObject* callback, MarkedArgumentBuffer& args, CallbackType method, PropertyName functionName, NakedPtr<Exception>& returnedException)
{
    ASSERT(callback);

    auto* globalObject = JSC::jsCast<JSDOMGlobalObject*>(callback->globalObject());
    ASSERT(globalObject);

    ExecState* exec = globalObject->globalExec();
    JSValue function;
    CallData callData;
    CallType callType = CallType::None;

    if (method != CallbackType::Object) {
        function = callback;
        callType = callback->methodTable()->getCallData(callback, callData);
    }
    if (callType == CallType::None) {
        if (method == CallbackType::Function) {
            returnedException = Exception::create(exec->vm(), createTypeError(exec));
            return JSValue();
        }

        ASSERT(!functionName.isNull());
        function = callback->get(exec, functionName);
        callType = getCallData(function, callData);
        if (callType == CallType::None) {
            returnedException = Exception::create(exec->vm(), createTypeError(exec));
            return JSValue();
        }
    }

    ASSERT(!function.isEmpty());
    ASSERT(callType != CallType::None);

    ScriptExecutionContext* context = globalObject->scriptExecutionContext();
    // We will fail to get the context if the frame has been detached.
    if (!context)
        return JSValue();

    InspectorInstrumentationCookie cookie = JSMainThreadExecState::instrumentFunctionCall(context, callType, callData);

    returnedException = nullptr;
    JSValue result = context->isDocument()
        ? JSMainThreadExecState::profiledCall(exec, JSC::ProfilingReason::Other, function, callType, callData, callback, args, returnedException)
        : JSC::profiledCall(exec, JSC::ProfilingReason::Other, function, callType, callData, callback, args, returnedException);

    InspectorInstrumentation::didCallFunction(cookie, context);

    return result;
}

bool JSCallbackDataWeak::WeakOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown>, void* context, SlotVisitor& visitor)
{
    return visitor.containsOpaqueRoot(context);
}

} // namespace WebCore
