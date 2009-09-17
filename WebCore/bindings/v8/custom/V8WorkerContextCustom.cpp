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

#include "DOMTimer.h"
#include "ExceptionCode.h"
#include "ScheduledAction.h"
#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8Proxy.h"
#include "V8Utilities.h"
#include "V8WorkerContextEventListener.h"
#include "WorkerContext.h"
#include "WorkerContextExecutionProxy.h"

namespace WebCore {

ACCESSOR_GETTER(WorkerContextSelf)
{
    INC_STATS(L"DOM.WorkerContext.self._get");
    WorkerContext* workerContext = V8DOMWrapper::convertDOMWrapperToNative<WorkerContext>(info.Holder());
    return WorkerContextExecutionProxy::convertWorkerContextToV8Object(workerContext);
}

ACCESSOR_GETTER(WorkerContextOnerror)
{
    INC_STATS(L"DOM.WorkerContext.onerror._get");
    WorkerContext* workerContext = V8DOMWrapper::convertToNativeObject<WorkerContext>(V8ClassIndex::WORKERCONTEXT, info.Holder());
    if (workerContext->onerror()) {
        V8WorkerContextEventListener* listener = static_cast<V8WorkerContextEventListener*>(workerContext->onerror());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Undefined();
}

ACCESSOR_SETTER(WorkerContextOnerror)
{
    INC_STATS(L"DOM.WorkerContext.onerror._set");
    WorkerContext* workerContext = V8DOMWrapper::convertToNativeObject<WorkerContext>(V8ClassIndex::WORKERCONTEXT, info.Holder());
    V8WorkerContextEventListener* oldListener = static_cast<V8WorkerContextEventListener*>(workerContext->onerror());
    if (value->IsNull()) {
        if (workerContext->onerror()) {
            v8::Local<v8::Object> oldV8Listener = oldListener->getListenerObject();
            removeHiddenDependency(info.Holder(), oldV8Listener, V8Custom::kWorkerContextRequestCacheIndex);
        }

        // Clear the listener.
        workerContext->setOnerror(0);
    } else {
        RefPtr<V8EventListener> listener = workerContext->script()->proxy()->findOrCreateEventListener(v8::Local<v8::Object>::Cast(value), true, false);
        if (listener) {
            if (oldListener) {
                v8::Local<v8::Object> oldV8Listener = oldListener->getListenerObject();
                removeHiddenDependency(info.Holder(), oldV8Listener, V8Custom::kWorkerContextRequestCacheIndex);
            }

            workerContext->setOnerror(listener);
            createHiddenDependency(info.Holder(), value, V8Custom::kWorkerContextRequestCacheIndex);
        }
    }
}

v8::Handle<v8::Value> SetTimeoutOrInterval(const v8::Arguments& args, bool singleShot)
{
    WorkerContext* workerContext = V8DOMWrapper::convertDOMWrapperToNative<WorkerContext>(args.Holder());

    int argumentCount = args.Length();
    if (argumentCount < 1)
        return v8::Undefined();

    v8::Handle<v8::Value> function = args[0];
    int32_t timeout = argumentCount >= 2 ? args[1]->Int32Value() : 0;
    int timerId;

    v8::Handle<v8::Context> v8Context = workerContext->script()->proxy()->context();
    if (function->IsString()) {
        WebCore::String stringFunction = toWebCoreString(function);
        timerId = DOMTimer::install(workerContext, new ScheduledAction(v8Context, stringFunction, workerContext->url()), timeout, singleShot);
    } else if (function->IsFunction()) {
        size_t paramCount = argumentCount >= 2 ? argumentCount - 2 : 0;
        v8::Local<v8::Value>* params = 0;
        if (paramCount > 0) {
            params = new v8::Local<v8::Value>[paramCount];
            for (size_t i = 0; i < paramCount; ++i)
                params[i] = args[i+2];
        }
        // ScheduledAction takes ownership of actual params and releases them in its destructor.
        ScheduledAction* action = new ScheduledAction(v8Context, v8::Handle<v8::Function>::Cast(function), paramCount, params);
        delete [] params;
        timerId = DOMTimer::install(workerContext, action, timeout, singleShot);
    } else
        return v8::Undefined();

    return v8::Integer::New(timerId);
}

CALLBACK_FUNC_DECL(WorkerContextImportScripts)
{
    INC_STATS(L"DOM.WorkerContext.importScripts()");
    if (!args.Length())
        return v8::Undefined();

    String callerURL = V8Proxy::sourceName();
    int callerLine = V8Proxy::sourceLineNumber() + 1;

    Vector<String> urls;
    for (int i = 0; i < args.Length(); i++) {
        v8::TryCatch tryCatch;
        v8::Handle<v8::String> scriptUrl = args[i]->ToString();
        if (tryCatch.HasCaught() || scriptUrl.IsEmpty())
            return v8::Undefined();
        urls.append(toWebCoreString(scriptUrl));
    }

    WorkerContext* workerContext = V8DOMWrapper::convertDOMWrapperToNative<WorkerContext>(args.Holder());

    ExceptionCode ec = 0;
    workerContext->importScripts(urls, callerURL, callerLine, ec);

    if (ec)
        return throwError(ec);

    return v8::Undefined();
}

CALLBACK_FUNC_DECL(WorkerContextSetTimeout)
{
    INC_STATS(L"DOM.WorkerContext.setTimeout()");
    return SetTimeoutOrInterval(args, true);
}

CALLBACK_FUNC_DECL(WorkerContextSetInterval)
{
    INC_STATS(L"DOM.WorkerContext.setInterval()");
    return SetTimeoutOrInterval(args, false);
}

CALLBACK_FUNC_DECL(WorkerContextAddEventListener)
{
    INC_STATS(L"DOM.WorkerContext.addEventListener()");
    WorkerContext* workerContext = V8DOMWrapper::convertDOMWrapperToNative<WorkerContext>(args.Holder());

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
    WorkerContext* workerContext = V8DOMWrapper::convertDOMWrapperToNative<WorkerContext>(args.Holder());
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
