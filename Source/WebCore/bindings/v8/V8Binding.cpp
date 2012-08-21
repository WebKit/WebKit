/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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
#include "V8Binding.h"

#include "BindingVisitors.h"
#include "DOMStringList.h"
#include "Element.h"
#include "Frame.h"
#include "MemoryInstrumentation.h"
#include "PlatformString.h"
#include "QualifiedName.h"
#include "Settings.h"
#include "V8DOMStringList.h"
#include "V8DOMWindow.h"
#include "V8Element.h"
#include "V8ObjectConstructor.h"

#include <wtf/MathExtras.h>
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Threading.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuffer.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

v8::Handle<v8::Value> setDOMException(int exceptionCode, v8::Isolate* isolate)
{
    return V8ThrowException::setDOMException(exceptionCode, isolate);
}

v8::Handle<v8::Value> throwError(ErrorType errorType, const char* message, v8::Isolate* isolate)
{
    return V8ThrowException::throwError(errorType, message, isolate);
}

v8::Handle<v8::Value> throwError(v8::Local<v8::Value> exception, v8::Isolate* isolate)
{
    return V8ThrowException::throwError(exception, isolate);
}

v8::Handle<v8::Value> throwTypeError(const char* message, v8::Isolate* isolate)
{
    return V8ThrowException::throwTypeError(message, isolate);
}

v8::Handle<v8::Value> throwNotEnoughArgumentsError(v8::Isolate* isolate)
{
    return V8ThrowException::throwNotEnoughArgumentsError(isolate);
}

static String v8NonStringValueToWebCoreString(v8::Handle<v8::Value> object)
{
    ASSERT(!object->IsString());
    if (object->IsInt32())
        return int32ToWebCoreString(object->Int32Value());

    v8::TryCatch block;
    v8::Handle<v8::String> v8String = object->ToString();
    // Handle the case where an exception is thrown as part of invoking toString on the object.
    if (block.HasCaught()) {
        throwError(block.Exception());
        return StringImpl::empty();
    }
    // This path is unexpected. However there is hypothesis that it
    // might be combination of v8 and v8 bindings bugs. For now
    // just bailout as we'll crash if attempt to convert empty handle into a string.
    if (v8String.IsEmpty()) {
        ASSERT_NOT_REACHED();
        return StringImpl::empty();
    }
    return v8StringToWebCoreString<String>(v8String, DoNotExternalize);
}

static AtomicString v8NonStringValueToAtomicWebCoreString(v8::Handle<v8::Value> object)
{
    ASSERT(!object->IsString());
    return AtomicString(v8NonStringValueToWebCoreString(object));
}

String toWebCoreString(v8::Handle<v8::Value> value)
{
    if (value->IsString())
        return v8StringToWebCoreString<String>(v8::Handle<v8::String>::Cast(value), Externalize);
    return v8NonStringValueToWebCoreString(value);
}

AtomicString toWebCoreAtomicString(v8::Handle<v8::Value> value)
{
    if (value->IsString())
        return v8StringToWebCoreString<AtomicString>(v8::Handle<v8::String>::Cast(value), Externalize);
    return v8NonStringValueToAtomicWebCoreString(value);
}

v8::Handle<v8::Value> v8Array(PassRefPtr<DOMStringList> stringList, v8::Isolate* isolate)
{
    if (!stringList)
        return v8::Array::New();
    v8::Local<v8::Array> result = v8::Array::New(stringList->length());
    for (unsigned i = 0; i < stringList->length(); ++i)
        result->Set(v8Integer(i, isolate), v8String(stringList->item(i), isolate));
    return result;
}

int toInt32(v8::Handle<v8::Value> value, bool& ok)
{
    ok = true;
    
    // Fast case. The value is already a 32-bit integer.
    if (value->IsInt32())
        return value->Int32Value();
    
    // Can the value be converted to a number?
    v8::Local<v8::Number> numberObject = value->ToNumber();
    if (numberObject.IsEmpty()) {
        ok = false;
        return 0;
    }
    
    // Does the value convert to nan or to an infinity?
    double numberValue = numberObject->Value();
    if (isnan(numberValue) || isinf(numberValue)) {
        ok = false;
        return 0;
    }
    
    // Can the value be converted to a 32-bit integer?
    v8::Local<v8::Int32> intValue = value->ToInt32();
    if (intValue.IsEmpty()) {
        ok = false;
        return 0;
    }
    
    // Return the result of the int32 conversion.
    return intValue->Value();
}
    
uint32_t toUInt32(v8::Handle<v8::Value> value, bool& ok)
{
    ok = true;

    // Fast case. The value is already a 32-bit unsigned integer.
    if (value->IsUint32())
        return value->Uint32Value();

    if (value->IsInt32()) {
        int32_t result = value->Int32Value();
        if (result >= 0)
            return result;
    }

    // Can the value be converted to a number?
    v8::Local<v8::Number> numberObject = value->ToNumber();
    if (numberObject.IsEmpty()) {
        ok = false;
        return 0;
    }

    // Does the value convert to nan or to an infinity?
    double numberValue = numberObject->Value();
    if (isnan(numberValue) || isinf(numberValue)) {
        ok = false;
        return 0;
    }

    // Can the value be converted to a 32-bit unsigned integer?
    v8::Local<v8::Uint32> uintValue = value->ToUint32();
    if (uintValue.IsEmpty()) {
        ok = false;
        return 0;
    }

    return uintValue->Value();
}

v8::Persistent<v8::FunctionTemplate> createRawTemplate()
{
    v8::HandleScope scope;
    v8::Local<v8::FunctionTemplate> result = v8::FunctionTemplate::New(V8ObjectConstructor::isValidConstructorMode);
    return v8::Persistent<v8::FunctionTemplate>::New(result);
}        

v8::Persistent<v8::String> getToStringName()
{
    v8::Persistent<v8::String>& toStringName = V8PerIsolateData::current()->toStringName();
    if (toStringName.IsEmpty())
        toStringName = v8::Persistent<v8::String>::New(v8::String::New("toString"));
    return *toStringName;

}

static v8::Handle<v8::Value> constructorToString(const v8::Arguments& args)
{
    // The DOM constructors' toString functions grab the current toString
    // for Functions by taking the toString function of itself and then
    // calling it with the constructor as its receiver. This means that
    // changes to the Function prototype chain or toString function are
    // reflected when printing DOM constructors. The only wart is that
    // changes to a DOM constructor's toString's toString will cause the
    // toString of the DOM constructor itself to change. This is extremely
    // obscure and unlikely to be a problem.
    v8::Handle<v8::Value> value = args.Callee()->Get(getToStringName());
    if (!value->IsFunction()) 
        return v8::String::New("");
    return v8::Handle<v8::Function>::Cast(value)->Call(args.This(), 0, 0);
}

v8::Persistent<v8::FunctionTemplate> getToStringTemplate()
{
    v8::Persistent<v8::FunctionTemplate>& toStringTemplate = V8PerIsolateData::current()->toStringTemplate();
    if (toStringTemplate.IsEmpty())
        toStringTemplate = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(constructorToString));
    return toStringTemplate;
}

void StringCache::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::Binding);
    info.addHashMap(m_stringCache);
}
    
PassRefPtr<DOMStringList> toDOMStringList(v8::Handle<v8::Value> value)
{
    v8::Local<v8::Value> v8Value(v8::Local<v8::Value>::New(value));

    if (V8DOMStringList::HasInstance(v8Value)) {
        RefPtr<DOMStringList> ret = V8DOMStringList::toNative(v8::Handle<v8::Object>::Cast(v8Value));
        return ret.release();
    }

    if (!v8Value->IsArray())
        return 0;

    RefPtr<DOMStringList> ret = DOMStringList::create();
    v8::Local<v8::Array> v8Array = v8::Local<v8::Array>::Cast(v8Value);
    for (size_t i = 0; i < v8Array->Length(); ++i) {
        v8::Local<v8::Value> indexedValue = v8Array->Get(v8Integer(i));
        ret->append(toWebCoreString(indexedValue));
    }
    return ret.release();
}

DOMWindow* toDOMWindow(v8::Handle<v8::Context> context)
{
    v8::Handle<v8::Object> global = context->Global();
    ASSERT(!global.IsEmpty());
    global = V8DOMWrapper::lookupDOMWrapper(V8DOMWindow::GetTemplate(), global);
    ASSERT(!global.IsEmpty());
    return V8DOMWindow::toNative(global);
}

Frame* toFrameIfNotDetached(v8::Handle<v8::Context> context)
{
    DOMWindow* window = toDOMWindow(context);
    if (window->isCurrentlyDisplayedInFrame())
        return window->frame();
    // We return 0 here because |context| is detached from the Frame. If we
    // did return |frame| we could get in trouble because the frame could be
    // navigated to another security origin.
    return 0;
}

V8PerContextData* perContextDataForCurrentWorld(Frame* frame)
{
    V8IsolatedContext* isolatedContext;
    if (UNLIKELY(!!(isolatedContext = V8IsolatedContext::getEntered())))
        return isolatedContext->perContextData();
    return frame->script()->windowShell()->perContextData();
}

bool handleOutOfMemory()
{
    v8::Local<v8::Context> context = v8::Context::GetCurrent();

    if (!context->HasOutOfMemoryException())
        return false;

    // Warning, error, disable JS for this frame?
    Frame* frame = toFrameIfNotDetached(context);
    if (!frame)
        return true;

    frame->script()->clearForClose();
    frame->script()->windowShell()->destroyGlobal();

#if PLATFORM(CHROMIUM)
    PlatformSupport::notifyJSOutOfMemory(frame);
#endif

    if (Settings* settings = frame->settings())
        settings->setScriptEnabled(false);

    return true;
}

void crashIfV8IsDead()
{
    if (v8::V8::IsDead()) {
        // FIXME: We temporarily deal with V8 internal error situations
        // such as out-of-memory by crashing the renderer.
        CRASH();
    }
}

} // namespace WebCore
