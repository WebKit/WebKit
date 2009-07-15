/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#if ENABLE(WORKERS)

#include "Worker.h"

#include "ExceptionCode.h"
#include "Frame.h"
#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8ObjectEventListener.h"
#include "V8Proxy.h"
#include "V8Utilities.h"
#include "WorkerContext.h"
#include "WorkerContextExecutionProxy.h"

namespace WebCore {

CALLBACK_FUNC_DECL(WorkerConstructor)
{
    INC_STATS(L"DOM.Worker.Constructor");

    if (!args.IsConstructCall()) {
        return throwError("DOM object constructor cannot be called as a function.");
    }

    if (args.Length() == 0) {
        return throwError("Not enough arguments", V8Proxy::SyntaxError);
    }

    v8::TryCatch tryCatch;
    v8::Handle<v8::String> scriptUrl = args[0]->ToString();
    if (tryCatch.HasCaught()) {
        return throwError(tryCatch.Exception());
    }
    if (scriptUrl.IsEmpty())
        return v8::Undefined();

    // Get the script execution context.
    ScriptExecutionContext* context = 0;
    WorkerContextExecutionProxy* proxy = WorkerContextExecutionProxy::retrieve();
    if (proxy)
        context = proxy->workerContext();
    else {
        Frame* frame = V8Proxy::retrieveFrame();
        if (!frame)
            return v8::Undefined();
        context = frame->document();
    }

    // Create the worker object.
    // Note: it's OK to let this RefPtr go out of scope because we also call setDOMWrapper(), which effectively holds a reference to obj.
    ExceptionCode ec = 0;
    RefPtr<Worker> obj = Worker::create(toWebCoreString(scriptUrl), context, ec);

    if (ec)
        return throwError(ec);

    // Setup the standard wrapper object internal fields.
    v8::Handle<v8::Object> wrapperObject = args.Holder();
    V8DOMWrapper::setDOMWrapper(wrapperObject, V8ClassIndex::WORKER, obj.get());

    obj->ref();
    V8DOMWrapper::setJSWrapperForActiveDOMObject(obj.get(), v8::Persistent<v8::Object>::New(wrapperObject));

    return wrapperObject;
}

PassRefPtr<EventListener> getEventListener(Worker* worker, v8::Local<v8::Value> value, bool findOnly)
{
    if (worker->scriptExecutionContext()->isWorkerContext()) {
        WorkerContextExecutionProxy* workerContextProxy = WorkerContextExecutionProxy::retrieve();
        ASSERT(workerContextProxy);
        return workerContextProxy->findOrCreateObjectEventListener(value, false, findOnly);
    }

    V8Proxy* proxy = V8Proxy::retrieve(worker->scriptExecutionContext());
    if (proxy)
        return findOnly ? proxy->findObjectEventListener(value, false) : proxy->findOrCreateObjectEventListener(value, false);

    return 0;
}

ACCESSOR_GETTER(WorkerOnmessage)
{
    INC_STATS(L"DOM.Worker.onmessage._get");
    Worker* worker = V8DOMWrapper::convertToNativeObject<Worker>(V8ClassIndex::WORKER, info.Holder());
    if (worker->onmessage()) {
        V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(worker->onmessage());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Undefined();
}

ACCESSOR_SETTER(WorkerOnmessage)
{
    INC_STATS(L"DOM.Worker.onmessage._set");
    Worker* worker = V8DOMWrapper::convertToNativeObject<Worker>(V8ClassIndex::WORKER, info.Holder());
    V8ObjectEventListener* oldListener = static_cast<V8ObjectEventListener*>(worker->onmessage());
    if (value->IsNull()) {
        if (oldListener) {
            v8::Local<v8::Object> oldV8Listener = oldListener->getListenerObject();
            removeHiddenDependency(info.Holder(), oldV8Listener, V8Custom::kWorkerRequestCacheIndex);
        }

        // Clear the listener.
        worker->setOnmessage(0);
    } else {
        RefPtr<EventListener> listener = getEventListener(worker, value, false);
        if (listener) {
            if (oldListener) {
                v8::Local<v8::Object> oldV8Listener = oldListener->getListenerObject();
                removeHiddenDependency(info.Holder(), oldV8Listener, V8Custom::kWorkerRequestCacheIndex);
            }

            worker->setOnmessage(listener);
            createHiddenDependency(info.Holder(), value, V8Custom::kWorkerRequestCacheIndex);
        }
    }
}

ACCESSOR_GETTER(WorkerOnerror)
{
    INC_STATS(L"DOM.Worker.onerror._get");
    Worker* worker = V8DOMWrapper::convertToNativeObject<Worker>(V8ClassIndex::WORKER, info.Holder());
    if (worker->onerror()) {
        V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(worker->onerror());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Undefined();
}

ACCESSOR_SETTER(WorkerOnerror)
{
    INC_STATS(L"DOM.Worker.onerror._set");
    Worker* worker = V8DOMWrapper::convertToNativeObject<Worker>(V8ClassIndex::WORKER, info.Holder());
    V8ObjectEventListener* oldListener = static_cast<V8ObjectEventListener*>(worker->onerror());
    if (value->IsNull()) {
        if (oldListener) {
            v8::Local<v8::Object> oldV8Listener = oldListener->getListenerObject();
            removeHiddenDependency(info.Holder(), oldV8Listener, V8Custom::kWorkerRequestCacheIndex);
        }

        // Clear the listener.
        worker->setOnerror(0);
    } else {
        RefPtr<EventListener> listener = getEventListener(worker, value, false);
        if (listener) {
            if (oldListener) {
                v8::Local<v8::Object> oldV8Listener = oldListener->getListenerObject();
                removeHiddenDependency(info.Holder(), oldV8Listener, V8Custom::kWorkerRequestCacheIndex);
            }

            worker->setOnerror(listener);
            createHiddenDependency(info.Holder(), value, V8Custom::kWorkerRequestCacheIndex);
        }
    }
}

CALLBACK_FUNC_DECL(WorkerAddEventListener)
{
    INC_STATS(L"DOM.Worker.addEventListener()");
    Worker* worker = V8DOMWrapper::convertToNativeObject<Worker>(V8ClassIndex::WORKER, args.Holder());

    RefPtr<EventListener> listener = getEventListener(worker, args[1], false);
    if (listener) {
        String type = toWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        worker->addEventListener(type, listener, useCapture);

        createHiddenDependency(args.Holder(), args[1], V8Custom::kWorkerRequestCacheIndex);
    }
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(WorkerRemoveEventListener)
{
    INC_STATS(L"DOM.Worker.removeEventListener()");
    Worker* worker = V8DOMWrapper::convertToNativeObject<Worker>(V8ClassIndex::WORKER, args.Holder());

    RefPtr<EventListener> listener = getEventListener(worker, args[1], true);
    if (listener) {
        String type = toWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        worker->removeEventListener(type, listener.get(), useCapture);

        removeHiddenDependency(args.Holder(), args[1], V8Custom::kWorkerRequestCacheIndex);
    }

    return v8::Undefined();
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
