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
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "QualifiedName.h"
#include "Settings.h"
#include "V8DOMStringList.h"
#include "V8DOMWindow.h"
#include "V8Element.h"
#include "V8NodeFilterCondition.h"
#include "V8ObjectConstructor.h"
#include "V8WorkerContext.h"
#include "V8XPathNSResolver.h"
#include "WebCoreMemoryInstrumentation.h"
#include "WorkerContext.h"
#include "WorkerScriptController.h"
#include "WorldContextHandle.h"
#include "XPathNSResolver.h"
#include <wtf/MathExtras.h>
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Threading.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuffer.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

v8::Handle<v8::Value> setDOMException(int exceptionCode, v8::Isolate* isolate)
{
    return V8ThrowException::setDOMException(exceptionCode, isolate);
}

v8::Handle<v8::Value> throwError(V8ErrorType errorType, const char* message, v8::Isolate* isolate)
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

v8::Handle<v8::Value> v8Array(PassRefPtr<DOMStringList> stringList, v8::Isolate* isolate)
{
    if (!stringList)
        return v8::Array::New();
    v8::Local<v8::Array> result = v8::Array::New(stringList->length());
    for (unsigned i = 0; i < stringList->length(); ++i)
        result->Set(v8Integer(i, isolate), v8String(stringList->item(i), isolate));
    return result;
}

PassRefPtr<NodeFilter> toNodeFilter(v8::Handle<v8::Value> callback)
{
    return NodeFilter::create(V8NodeFilterCondition::create(callback));
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
    if (std::isnan(numberValue) || std::isinf(numberValue)) {
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
    if (std::isnan(numberValue) || std::isinf(numberValue)) {
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

v8::Persistent<v8::FunctionTemplate> createRawTemplate(v8::Isolate* isolate)
{
    v8::HandleScope scope;
    v8::Local<v8::FunctionTemplate> result = v8::FunctionTemplate::New(V8ObjectConstructor::isValidConstructorMode);
    return v8::Persistent<v8::FunctionTemplate>::New(isolate, result);
}        

PassRefPtr<DOMStringList> toDOMStringList(v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    v8::Local<v8::Value> v8Value(v8::Local<v8::Value>::New(value));

    if (V8DOMStringList::HasInstance(v8Value, isolate)) {
        RefPtr<DOMStringList> ret = V8DOMStringList::toNative(v8::Handle<v8::Object>::Cast(v8Value));
        return ret.release();
    }

    if (!v8Value->IsArray())
        return 0;

    RefPtr<DOMStringList> ret = DOMStringList::create();
    v8::Local<v8::Array> v8Array = v8::Local<v8::Array>::Cast(v8Value);
    for (size_t i = 0; i < v8Array->Length(); ++i) {
        v8::Local<v8::Value> indexedValue = v8Array->Get(v8Integer(i, isolate));
        ret->append(toWebCoreString(indexedValue));
    }
    return ret.release();
}

PassRefPtr<XPathNSResolver> toXPathNSResolver(v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    RefPtr<XPathNSResolver> resolver;
    if (V8XPathNSResolver::HasInstance(value, isolate))
        resolver = V8XPathNSResolver::toNative(v8::Handle<v8::Object>::Cast(value));
    else if (value->IsObject())
        resolver = V8CustomXPathNSResolver::create(value->ToObject(), isolate);
    return resolver;
}

v8::Handle<v8::Object> toInnerGlobalObject(v8::Handle<v8::Context> context)
{
    return v8::Handle<v8::Object>::Cast(context->Global()->GetPrototype());
}

DOMWindow* toDOMWindow(v8::Handle<v8::Context> context)
{
    v8::Handle<v8::Object> global = context->Global();
    ASSERT(!global.IsEmpty());
    global = global->FindInstanceInPrototypeChain(V8DOMWindow::GetTemplate(context->GetIsolate()));
    ASSERT(!global.IsEmpty());
    return V8DOMWindow::toNative(global);
}

ScriptExecutionContext* toScriptExecutionContext(v8::Handle<v8::Context> context)
{
    v8::Handle<v8::Object> global = context->Global();
    v8::Handle<v8::Object> windowWrapper = global->FindInstanceInPrototypeChain(V8DOMWindow::GetTemplate(context->GetIsolate()));
    if (!windowWrapper.IsEmpty())
        return V8DOMWindow::toNative(windowWrapper)->scriptExecutionContext();
#if ENABLE(WORKERS)
    v8::Handle<v8::Object> workerWrapper = global->FindInstanceInPrototypeChain(V8WorkerContext::GetTemplate(context->GetIsolate()));
    if (!workerWrapper.IsEmpty())
        return V8WorkerContext::toNative(workerWrapper)->scriptExecutionContext();
#endif
    // FIXME: Is this line of code reachable?
    return 0;
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

v8::Local<v8::Context> toV8Context(ScriptExecutionContext* context, const WorldContextHandle& worldContext)
{
    if (context->isDocument()) {
        if (Frame* frame = static_cast<Document*>(context)->frame())
            return worldContext.adjustedContext(frame->script());
#if ENABLE(WORKERS)
    } else if (context->isWorkerContext()) {
        if (WorkerScriptController* script = static_cast<WorkerContext*>(context)->script())
            return script->context();
#endif
    }
    return v8::Local<v8::Context>();
}

v8::Local<v8::Context> toV8Context(ScriptExecutionContext* context, DOMWrapperWorld* world)
{
    if (context->isDocument()) {
        if (Frame* frame = static_cast<Document*>(context)->frame()) {
            // FIXME: Store the DOMWrapperWorld for the main world in the v8::Context so callers
            // that are looking up their world with DOMWrapperWorld::isolatedWorld(v8::Context::GetCurrent())
            // won't end up passing null here when later trying to get their v8::Context back.
            if (!world)
                return frame->script()->mainWorldContext();
            return v8::Local<v8::Context>::New(frame->script()->windowShell(world)->context());
        }
#if ENABLE(WORKERS)
    } else if (context->isWorkerContext()) {
        if (WorkerScriptController* script = static_cast<WorkerContext*>(context)->script())
            return script->context();
#endif
    }
    return v8::Local<v8::Context>();
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

    frame->script()->clearForOutOfMemory();

#if PLATFORM(CHROMIUM)
    frame->loader()->client()->didExhaustMemoryAvailableForScript();
#endif

    if (Settings* settings = frame->settings())
        settings->setScriptEnabled(false);

    return true;
}

v8::Local<v8::Value> handleMaxRecursionDepthExceeded()
{
    throwError(v8RangeError, "Maximum call stack size exceeded.", v8::Isolate::GetCurrent());
    return v8::Local<v8::Value>();
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
