/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "V8NPUtils.h"

#include "DOMWindow.h"
#include "Frame.h"
#include "PlatformString.h"
#include "npruntime_impl.h"
#include "npruntime_priv.h"
#include "NPV8Object.h"
#include "V8Binding.h"
#include "V8NPObject.h"
#include "V8Proxy.h"

namespace WebCore {

void convertV8ObjectToNPVariant(v8::Local<v8::Value> object, NPObject* owner, NPVariant* result)
{
    VOID_TO_NPVARIANT(*result);

    // It is really the caller's responsibility to deal with the empty handle case because there could be different actions to
    // take in different contexts.
    ASSERT(!object.IsEmpty());

    if (object.IsEmpty())
        return;

    if (object->IsNumber())
        DOUBLE_TO_NPVARIANT(object->NumberValue(), *result);
    else if (object->IsBoolean())
        BOOLEAN_TO_NPVARIANT(object->BooleanValue(), *result);
    else if (object->IsNull())
        NULL_TO_NPVARIANT(*result);
    else if (object->IsUndefined())
        VOID_TO_NPVARIANT(*result);
    else if (object->IsString()) {
        v8::Handle<v8::String> str = object->ToString();
        int length = str->Utf8Length() + 1;
        char* utf8Chars = reinterpret_cast<char*>(malloc(length));
        str->WriteUtf8(utf8Chars, length, 0, v8::String::HINT_MANY_WRITES_EXPECTED);
        STRINGN_TO_NPVARIANT(utf8Chars, length-1, *result);
    } else if (object->IsObject()) {
        DOMWindow* window = toDOMWindow(v8::Context::GetCurrent());
        NPObject* npobject = npCreateV8ScriptObject(0, v8::Handle<v8::Object>::Cast(object), window);
        if (npobject)
            _NPN_RegisterObject(npobject, owner);
        OBJECT_TO_NPVARIANT(npobject, *result);
    }
}

v8::Handle<v8::Value> convertNPVariantToV8Object(const NPVariant* variant, NPObject* npobject)
{
    NPVariantType type = variant->type;

    switch (type) {
    case NPVariantType_Int32:
        return v8Integer(NPVARIANT_TO_INT32(*variant));
    case NPVariantType_Double:
        return v8::Number::New(NPVARIANT_TO_DOUBLE(*variant));
    case NPVariantType_Bool:
        return v8Boolean(NPVARIANT_TO_BOOLEAN(*variant));
    case NPVariantType_Null:
        return v8::Null();
    case NPVariantType_Void:
        return v8::Undefined();
    case NPVariantType_String: {
        NPString src = NPVARIANT_TO_STRING(*variant);
        return v8::String::New(src.UTF8Characters, src.UTF8Length);
    }
    case NPVariantType_Object: {
        NPObject* obj = NPVARIANT_TO_OBJECT(*variant);
        if (obj->_class == npScriptObjectClass)
            return reinterpret_cast<V8NPObject*>(obj)->v8Object;
        return createV8ObjectForNPObject(obj, npobject);
    }
    default:
        return v8::Undefined();
    }
}

// Helper function to create an NPN String Identifier from a v8 string.
NPIdentifier getStringIdentifier(v8::Handle<v8::String> str)
{
    const int kStackBufferSize = 100;

    int bufferLength = str->Utf8Length() + 1;
    if (bufferLength <= kStackBufferSize) {
        // Use local stack buffer to avoid heap allocations for small strings. Here we should only use the stack space for
        // stackBuffer when it's used, not when we use the heap.
        //
        // WriteUtf8 is guaranteed to generate a null-terminated string because bufferLength is constructed to be one greater
        // than the string length.
        char stackBuffer[kStackBufferSize];
        str->WriteUtf8(stackBuffer, bufferLength);
        return _NPN_GetStringIdentifier(stackBuffer);
    }

    v8::String::Utf8Value utf8(str);
    return _NPN_GetStringIdentifier(*utf8);
}

struct ExceptionHandlerInfo {
    ExceptionHandlerInfo* previous;
    ExceptionHandler handler;
    void* data;
};

static ExceptionHandlerInfo* topHandler;

void pushExceptionHandler(ExceptionHandler handler, void* data)
{
    ExceptionHandlerInfo* info = new ExceptionHandlerInfo;
    info->previous = topHandler;
    info->handler = handler;
    info->data = data;
    topHandler = info;
}

void popExceptionHandler()
{
    ASSERT(topHandler);
    ExceptionHandlerInfo* doomed = topHandler;
    topHandler = topHandler->previous;
    delete doomed;
}

ExceptionCatcher::ExceptionCatcher()
{
    if (!topHandler)
        m_tryCatch.SetVerbose(true);
}

ExceptionCatcher::~ExceptionCatcher()
{
    if (!m_tryCatch.HasCaught())
        return;

    if (topHandler)
        topHandler->handler(topHandler->data, *v8::String::Utf8Value(m_tryCatch.Exception()));
}

} // namespace WebCore
