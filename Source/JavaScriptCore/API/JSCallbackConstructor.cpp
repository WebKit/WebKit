/*
 * Copyright (C) 2006-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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

#include "APICallbackFunction.h"
#include "APICast.h"
#include "JSCInlines.h"
#include "JSLock.h"

namespace JSC {

const ClassInfo JSCallbackConstructor::s_info = { "CallbackConstructor", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSCallbackConstructor) };

JSCallbackConstructor::JSCallbackConstructor(JSGlobalObject* globalObject, Structure* structure, JSClassRef jsClass, JSObjectCallAsConstructorCallback callback)
    : Base(globalObject->vm(), structure)
    , m_class(jsClass)
    , m_callback(callback)
{
}

void JSCallbackConstructor::finishCreation(JSGlobalObject* globalObject, JSClassRef jsClass)
{
    Base::finishCreation(globalObject->vm());
    ASSERT(inherits(vm(), info()));
    if (m_class)
        JSClassRetain(jsClass);
}

JSCallbackConstructor::~JSCallbackConstructor()
{
    if (m_class)
        JSClassRelease(m_class);
}

void JSCallbackConstructor::destroy(JSCell* cell)
{
    static_cast<JSCallbackConstructor*>(cell)->JSCallbackConstructor::~JSCallbackConstructor();
}

static JSC_DECLARE_HOST_FUNCTION(constructJSCallbackConstructor);

JSC_DEFINE_HOST_FUNCTION(constructJSCallbackConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return APICallbackFunction::constructImpl<JSCallbackConstructor>(globalObject, callFrame);
}

CallData JSCallbackConstructor::getConstructData(JSCell*)
{
    CallData constructData;
    constructData.type = CallData::Type::Native;
    constructData.native.function = constructJSCallbackConstructor;
    return constructData;
}

} // namespace JSC
