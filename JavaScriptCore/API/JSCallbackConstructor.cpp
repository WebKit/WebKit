// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
#include "JSCallbackConstructor.h"

#include "APICast.h"
#include <kjs/JSGlobalObject.h>
#include <kjs/object_object.h>
#include <wtf/Vector.h>

namespace KJS {

const ClassInfo JSCallbackConstructor::info = { "CallbackConstructor", 0, 0};

JSCallbackConstructor::JSCallbackConstructor(ExecState* exec, JSClassRef jsClass, JSObjectCallAsConstructorCallback callback)
    : JSObject(exec->lexicalGlobalObject()->objectPrototype())
    , m_class(jsClass)
    , m_callback(callback)
{
    if (m_class)
        JSClassRetain(jsClass);
}

JSCallbackConstructor::~JSCallbackConstructor()
{
    if (m_class)
        JSClassRelease(m_class);
}

bool JSCallbackConstructor::implementsHasInstance() const
{
    return true;
}

bool JSCallbackConstructor::implementsConstruct() const
{
    return true;
}

JSObject* JSCallbackConstructor::construct(ExecState* exec, const List &args)
{
    JSContextRef ctx = toRef(exec);
    JSObjectRef thisRef = toRef(this);

    if (m_callback) {
        int argumentCount = static_cast<int>(args.size());
        Vector<JSValueRef, 16> arguments(argumentCount);
        for (int i = 0; i < argumentCount; i++)
            arguments[i] = toRef(args[i]);
            
        JSLock::DropAllLocks dropAllLocks;
        return toJS(m_callback(ctx, thisRef, argumentCount, arguments.data(), toRef(exec->exceptionSlot())));
    }
    
    return toJS(JSObjectMake(ctx, m_class, 0));
}

} // namespace KJS
