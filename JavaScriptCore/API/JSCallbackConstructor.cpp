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

#include "JSCallbackConstructor.h"
#include "APICast.h"

namespace KJS {

const ClassInfo JSCallbackConstructor::info = { "CallbackConstructor", 0, 0, 0 };

JSCallbackConstructor::JSCallbackConstructor(ExecState* exec, JSObjectCallAsConstructorCallback callback)
    : JSObject(exec->lexicalInterpreter()->builtinObjectPrototype())
    , m_callback(callback)
{
}

bool JSCallbackConstructor::implementsConstruct() const
{
    return true;
}

JSObject* JSCallbackConstructor::construct(ExecState* exec, const List &args)
{
    JSContextRef execRef = toRef(exec);
    JSObjectRef thisRef = toRef(this);

    size_t argumentCount = args.size();
    JSValueRef arguments[argumentCount];
    for (size_t i = 0; i < argumentCount; i++)
        arguments[i] = toRef(args[i]);
    return toJS(m_callback(execRef, thisRef, argumentCount, arguments, toRef(exec->exceptionSlot())));
}

void JSCallbackConstructor::setPrivate(void* data)
{
    m_privateData = data;
}

void* JSCallbackConstructor::getPrivate()
{
    return m_privateData;
}

} // namespace KJS
