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
#include "V8Utilities.h"

#include "BindingState.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "MessagePort.h"
#include "ScriptExecutionContext.h"
#include "ScriptState.h"
#include "V8ArrayBuffer.h"
#include "V8Binding.h"
#include "V8MessagePort.h"
#include "V8Proxy.h"
#include "WorkerContext.h"
#include <v8.h>
#include <wtf/ArrayBuffer.h>

namespace WebCore {

V8AuxiliaryContext::V8AuxiliaryContext()
{
    auxiliaryContext()->Enter();
}

V8AuxiliaryContext::~V8AuxiliaryContext()
{
    auxiliaryContext()->Exit();
}

v8::Persistent<v8::Context>& V8AuxiliaryContext::auxiliaryContext()
{
    v8::Persistent<v8::Context>& context = V8PerIsolateData::current()->auxiliaryContext();
    if (context.IsEmpty())
        context = v8::Context::New();
    return context;
}

// Use an array to hold dependents. It works like a ref-counted scheme.
// A value can be added more than once to the DOM object.
void createHiddenDependency(v8::Handle<v8::Object> object, v8::Local<v8::Value> value, int cacheIndex)
{
    v8::Local<v8::Value> cache = object->GetInternalField(cacheIndex);
    if (cache->IsNull() || cache->IsUndefined()) {
        cache = v8::Array::New();
        object->SetInternalField(cacheIndex, cache);
    }

    v8::Local<v8::Array> cacheArray = v8::Local<v8::Array>::Cast(cache);
    cacheArray->Set(v8Integer(cacheArray->Length()), value);
}

bool extractTransferables(v8::Local<v8::Value> value, MessagePortArray& ports, ArrayBufferArray& arrayBuffers, v8::Isolate* isolate)
{
    if (isUndefinedOrNull(value)) {
        ports.resize(0);
        arrayBuffers.resize(0);
        return true;
    }

    uint32_t length = 0;
    if (value->IsArray()) {
        v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(value);
        length = array->Length();
    } else {
        if (toV8Sequence(value, length).IsEmpty())
            return false;
    }

    v8::Local<v8::Object> transferrables = v8::Local<v8::Object>::Cast(value);

    // Validate the passed array of transferrables.
    for (unsigned int i = 0; i < length; ++i) {
        v8::Local<v8::Value> transferrable = transferrables->Get(i);
        // Validation of non-null objects, per HTML5 spec 10.3.3.
        if (isUndefinedOrNull(transferrable)) {
            setDOMException(DATA_CLONE_ERR, isolate);
            return false;
        }
        // Validation of Objects implementing an interface, per WebIDL spec 4.1.15.
        if (V8MessagePort::HasInstance(transferrable))
            ports.append(V8MessagePort::toNative(v8::Handle<v8::Object>::Cast(transferrable)));
        else if (V8ArrayBuffer::HasInstance(transferrable))
            arrayBuffers.append(V8ArrayBuffer::toNative(v8::Handle<v8::Object>::Cast(transferrable)));
        else {
            throwTypeError();
            return false;
        }
    }
    return true;
}

bool getMessagePortArray(v8::Local<v8::Value> value, MessagePortArray& ports, v8::Isolate* isolate)
{
    ArrayBufferArray arrayBuffers;
    bool result = extractTransferables(value, ports, arrayBuffers, isolate);
    if (!result)
        return false;
    if (arrayBuffers.size() > 0) {
        throwTypeError("MessagePortArray argument must contain only MessagePorts");
        return false;
    }
    return true;
}

void removeHiddenDependency(v8::Handle<v8::Object> object, v8::Local<v8::Value> value, int cacheIndex)
{
    v8::Local<v8::Value> cache = object->GetInternalField(cacheIndex);
    if (!cache->IsArray())
        return;
    v8::Local<v8::Array> cacheArray = v8::Local<v8::Array>::Cast(cache);
    for (int i = cacheArray->Length() - 1; i >= 0; --i) {
        v8::Local<v8::Value> cached = cacheArray->Get(v8Integer(i));
        if (cached->StrictEquals(value)) {
            cacheArray->Delete(i);
            return;
        }
    }
}
    
void transferHiddenDependency(v8::Handle<v8::Object> object,
                              EventListener* oldValue, 
                              v8::Local<v8::Value> newValue, 
                              int cacheIndex)
{
    if (oldValue) {
        V8AbstractEventListener* oldListener = V8AbstractEventListener::cast(oldValue);
        if (oldListener) {
            v8::Local<v8::Object> oldListenerObject = oldListener->getExistingListenerObject();
            if (!oldListenerObject.IsEmpty())
                removeHiddenDependency(object, oldListenerObject, cacheIndex);
        }
    }
    if (!newValue->IsNull() && !newValue->IsUndefined())
        createHiddenDependency(object, newValue, cacheIndex);
}

ScriptExecutionContext* getScriptExecutionContext()
{
#if ENABLE(WORKERS)
    if (WorkerScriptController* controller = WorkerScriptController::controllerForContext())
        return controller->workerContext();
#endif

    return currentDocument(BindingState::instance());
}

void setTypeMismatchException(v8::Isolate* isolate)
{
    setDOMException(TYPE_MISMATCH_ERR, isolate);
}

} // namespace WebCore
