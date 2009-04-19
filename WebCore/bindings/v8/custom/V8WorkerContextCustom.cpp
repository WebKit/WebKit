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

#include "WorkerContextExecutionProxy.h"

#include "ExceptionCode.h"
#include "NotImplemented.h"
#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8Proxy.h"
#include "V8Utilities.h"
#include "V8WorkerContextEventListener.h"
#include "WorkerContext.h"

namespace WebCore {

ACCESSOR_GETTER(WorkerContextSelf)
{
    INC_STATS(L"DOM.WorkerContext.self._get");
    WorkerContext* workerContext = V8Proxy::ToNativeObject<WorkerContext>(V8ClassIndex::WORKERCONTEXT, info.Holder());
    return WorkerContextExecutionProxy::WorkerContextToV8Object(workerContext);
}

ACCESSOR_GETTER(WorkerContextOnmessage)
{
    INC_STATS(L"DOM.WorkerContext.onmessage._get");
    WorkerContext* workerContext = V8Proxy::ToNativeObject<WorkerContext>(V8ClassIndex::WORKERCONTEXT, info.Holder());
    if (workerContext->onmessage()) {
        V8WorkerContextEventListener* listener = static_cast<V8WorkerContextEventListener*>(workerContext->onmessage());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Undefined();
}

ACCESSOR_SETTER(WorkerContextOnmessage)
{
    INC_STATS(L"DOM.WorkerContext.onmessage._set");
    WorkerContext* workerContext = V8Proxy::ToNativeObject<WorkerContext>(V8ClassIndex::WORKERCONTEXT, info.Holder());
    V8WorkerContextEventListener* oldListener = static_cast<V8WorkerContextEventListener*>(workerContext->onmessage());
    if (value->IsNull()) {
        if (workerContext->onmessage()) {
            v8::Local<v8::Object> oldV8Listener = oldListener->getListenerObject();
            removeHiddenDependency(info.Holder(), oldV8Listener, V8Custom::kWorkerContextRequestCacheIndex);
        }

        // Clear the listener.
        workerContext->setOnmessage(0);
    } else {
        RefPtr<V8EventListener> listener = workerContext->script()->proxy()->findOrCreateEventListener(v8::Local<v8::Object>::Cast(value), false, false);
        if (listener) {
            if (oldListener) {
                v8::Local<v8::Object> oldV8Listener = oldListener->getListenerObject();
                removeHiddenDependency(info.Holder(), oldV8Listener, V8Custom::kWorkerContextRequestCacheIndex);
            }

            workerContext->setOnmessage(listener);
            createHiddenDependency(info.Holder(), value, V8Custom::kWorkerContextRequestCacheIndex);
        }
    }
}

v8::Handle<v8::Value> SetTimeoutOrInterval(const v8::Arguments& args, bool singleShot)
{
    WorkerContext* workerContext = V8Proxy::ToNativeObject<WorkerContext>(V8ClassIndex::WORKERCONTEXT, args.Holder());

    int delay = toInt32(args[1]);

    notImplemented();

    return v8::Undefined();
}

v8::Handle<v8::Value> ClearTimeoutOrInterval(const v8::Arguments& args)
{
    WorkerContext* workerContext = V8Proxy::ToNativeObject<WorkerContext>(V8ClassIndex::WORKERCONTEXT, args.Holder());

    int timerId = toInt32(args[0]);
    workerContext->removeTimeout(timerId);

    return v8::Undefined();
}

CALLBACK_FUNC_DECL(WorkerContextImportScripts)
{
    INC_STATS(L"DOM.WorkerContext.importScripts()");
    notImplemented();
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(WorkerContextSetTimeout)
{
    INC_STATS(L"DOM.WorkerContext.setTimeout()");
    return SetTimeoutOrInterval(args, true);
}

CALLBACK_FUNC_DECL(WorkerContextClearTimeout) {
    INC_STATS(L"DOM.WorkerContext.clearTimeout()");
    return ClearTimeoutOrInterval(args);
}

CALLBACK_FUNC_DECL(WorkerContextSetInterval) {
    INC_STATS(L"DOM.WorkerContext.setInterval()");
    return SetTimeoutOrInterval(args, false);
}

CALLBACK_FUNC_DECL(WorkerContextClearInterval)
{
    INC_STATS(L"DOM.WorkerContext.clearInterval()");
    return ClearTimeoutOrInterval(args);
}

CALLBACK_FUNC_DECL(WorkerContextAddEventListener)
{
    INC_STATS(L"DOM.WorkerContext.addEventListener()");
    WorkerContext* workerContext = V8Proxy::ToNativeObject<WorkerContext>(V8ClassIndex::WORKERCONTEXT, args.Holder());

    RefPtr<V8EventListener> listener = workerContext->script()->proxy()->findOrCreateEventListener(v8::Local<v8::Object>::Cast(args[1]), false, false);

    if (listener) {
        String type = toWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        workerContext->addEventListener(type, listener, useCapture);

        createHiddenDependency(args.Holder(), args[1], V8Custom::kWorkerContextRequestCacheIndex);
    }
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(WorkerContextRemoveEventListener)
{
    INC_STATS(L"DOM.WorkerContext.removeEventListener()");
    WorkerContext* workerContext = V8Proxy::ToNativeObject<WorkerContext>(V8ClassIndex::WORKERCONTEXT, args.Holder());
    WorkerContextExecutionProxy* proxy = workerContext->script()->proxy();

    RefPtr<V8EventListener> listener = proxy->findOrCreateEventListener(v8::Local<v8::Object>::Cast(args[1]), false, true);

    if (listener) {
        String type = toWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        workerContext->removeEventListener(type, listener.get(), useCapture);

        removeHiddenDependency(args.Holder(), args[1], V8Custom::kWorkerContextRequestCacheIndex);
    }

    return v8::Undefined();
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
