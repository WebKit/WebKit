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

#include "DedicatedWorkerContext.h"
#include "V8Proxy.h"
#include "V8WorkerContextEventListener.h"

namespace WebCore {

ACCESSOR_GETTER(DedicatedWorkerContextOnmessage)
{
    INC_STATS(L"DOM.DedicatedWorkerContext.onmessage._get");
    DedicatedWorkerContext* workerContext = V8DOMWrapper::convertToNativeObject<DedicatedWorkerContext>(V8ClassIndex::DEDICATEDWORKERCONTEXT, info.Holder());
    if (workerContext->onmessage()) {
        V8WorkerContextEventListener* listener = static_cast<V8WorkerContextEventListener*>(workerContext->onmessage());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Undefined();
}

ACCESSOR_SETTER(DedicatedWorkerContextOnmessage)
{
    INC_STATS(L"DOM.DedicatedWorkerContext.onmessage._set");
    DedicatedWorkerContext* workerContext = V8DOMWrapper::convertToNativeObject<DedicatedWorkerContext>(V8ClassIndex::DEDICATEDWORKERCONTEXT, info.Holder());
    V8WorkerContextEventListener* oldListener = static_cast<V8WorkerContextEventListener*>(workerContext->onmessage());
    if (value->IsNull()) {
        if (workerContext->onmessage()) {
            v8::Local<v8::Object> oldV8Listener = oldListener->getListenerObject();
            removeHiddenDependency(info.Holder(), oldV8Listener, V8Custom::kDedicatedWorkerContextRequestCacheIndex);
        }

        // Clear the listener.
        workerContext->setOnmessage(0);
    } else {
        RefPtr<V8EventListener> listener = workerContext->script()->proxy()->findOrCreateEventListener(v8::Local<v8::Object>::Cast(value), false, false);
        if (listener) {
            if (oldListener) {
                v8::Local<v8::Object> oldV8Listener = oldListener->getListenerObject();
                removeHiddenDependency(info.Holder(), oldV8Listener, V8Custom::kDedicatedWorkerContextRequestCacheIndex);
            }

            workerContext->setOnmessage(listener);
            createHiddenDependency(info.Holder(), value, V8Custom::kDedicatedWorkerContextRequestCacheIndex);
        }
    }
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
