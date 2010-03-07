/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "JSCallbackFunction.h"

#include "APIShims.h"
#include "APICast.h"
#include "CodeBlock.h"
#include "JSFunction.h"
#include "FunctionPrototype.h"
#include <runtime/JSGlobalObject.h>
#include <runtime/JSLock.h>
#include <wtf/Vector.h>

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(JSCallbackFunction);

const ClassInfo JSCallbackFunction::info = { "CallbackFunction", &InternalFunction::info, 0, 0 };

JSCallbackFunction::JSCallbackFunction(ExecState* exec, JSObjectCallAsFunctionCallback callback, const Identifier& name)
    : InternalFunction(&exec->globalData(), exec->lexicalGlobalObject()->callbackFunctionStructure(), name)
    , m_callback(callback)
{
}

JSValue JSCallbackFunction::call(ExecState* exec, JSObject* functionObject, JSValue thisValue, const ArgList& args)
{
    JSContextRef execRef = toRef(exec);
    JSObjectRef functionRef = toRef(functionObject);
    JSObjectRef thisObjRef = toRef(thisValue.toThisObject(exec));

    int argumentCount = static_cast<int>(args.size());
    Vector<JSValueRef, 16> arguments(argumentCount);
    for (int i = 0; i < argumentCount; i++)
        arguments[i] = toRef(exec, args.at(i));

    JSValueRef exception = 0;
    JSValueRef result;
    {
        APICallbackShim callbackShim(exec);
        result = static_cast<JSCallbackFunction*>(functionObject)->m_callback(execRef, functionRef, thisObjRef, argumentCount, arguments.data(), &exception);
    }
    if (exception)
        exec->setException(toJS(exec, exception));

    return toJS(exec, result);
}

CallType JSCallbackFunction::getCallData(CallData& callData)
{
    callData.native.function = call;
    return CallTypeHost;
}

} // namespace JSC
