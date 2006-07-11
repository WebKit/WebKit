// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "JSCallbackFunction.h"
#include "APICast.h"
#include "function.h"
#include "function_object.h"

namespace KJS {

const ClassInfo JSCallbackFunction::info = { "CallbackFunction", &InternalFunctionImp::info, 0, 0 };

JSCallbackFunction::JSCallbackFunction(ExecState* exec, JSCallAsFunctionCallback callback)
    : InternalFunctionImp(static_cast<FunctionPrototype*>(exec->lexicalInterpreter()->builtinFunctionPrototype()))
    , m_callback(callback)
{
}

bool JSCallbackFunction::implementsCall() const
{
    return true;
}

JSValue* JSCallbackFunction::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
    JSContextRef execRef = toRef(exec);
    JSObjectRef thisRef = toRef(this);
    JSObjectRef thisObjRef = toRef(thisObj);

    size_t argc = args.size();
    JSValueRef argv[argc];
    for (size_t i = 0; i < argc; i++)
        argv[i] = toRef(args[i]);
    return toJS(m_callback(execRef, thisRef, thisObjRef, argc, argv, toRef(exec->exceptionSlot())));
}

void JSCallbackFunction::setPrivate(void* data)
{
    m_privateData = data;
}

void* JSCallbackFunction::getPrivate()
{
    return m_privateData;
}

} // namespace KJS
