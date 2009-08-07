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

#include "AbstractWorker.h"

#include "ExceptionCode.h"
#include "ScriptExecutionContext.h"
#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8ObjectEventListener.h"
#include "V8Proxy.h"
#include "V8Utilities.h"
#include "WorkerContextExecutionProxy.h"

namespace WebCore {

PassRefPtr<EventListener> getEventListener(AbstractWorker* worker, v8::Local<v8::Value> value, bool isAttribute, bool findOnly)
{
    if (worker->scriptExecutionContext()->isWorkerContext()) {
        WorkerContextExecutionProxy* workerContextProxy = WorkerContextExecutionProxy::retrieve();
        ASSERT(workerContextProxy);
        return workerContextProxy->findOrCreateObjectEventListener(value, isAttribute, findOnly);
    }

    V8Proxy* proxy = V8Proxy::retrieve(worker->scriptExecutionContext());
    if (proxy) {
        V8EventListenerList* list = proxy->objectListeners();
        return findOnly ? list->findWrapper(value, isAttribute) : list->findOrCreateWrapper<V8ObjectEventListener>(proxy->frame(), value, isAttribute);
    }

    return 0;
}

ACCESSOR_GETTER(AbstractWorkerOnerror)
{
    INC_STATS(L"DOM.AbstractWorker.onerror._get");
    AbstractWorker* worker = V8DOMWrapper::convertToNativeObject<AbstractWorker>(V8ClassIndex::ABSTRACTWORKER, info.Holder());
    if (worker->onerror()) {
        V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(worker->onerror());
        v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        return v8Listener;
    }
    return v8::Undefined();
}

ACCESSOR_SETTER(AbstractWorkerOnerror)
{
    INC_STATS(L"DOM.AbstractWorker.onerror._set");
    AbstractWorker* worker = V8DOMWrapper::convertToNativeObject<AbstractWorker>(V8ClassIndex::ABSTRACTWORKER, info.Holder());
    V8ObjectEventListener* oldListener = static_cast<V8ObjectEventListener*>(worker->onerror());
    if (value->IsNull()) {
        if (oldListener) {
            v8::Local<v8::Object> oldV8Listener = oldListener->getListenerObject();
            removeHiddenDependency(info.Holder(), oldV8Listener, V8Custom::kAbstractWorkerRequestCacheIndex);
        }

        // Clear the listener.
        worker->setOnerror(0);
    } else {
        RefPtr<EventListener> listener = getEventListener(worker, value, true, false);
        if (listener) {
            if (oldListener) {
                v8::Local<v8::Object> oldV8Listener = oldListener->getListenerObject();
                removeHiddenDependency(info.Holder(), oldV8Listener, V8Custom::kAbstractWorkerRequestCacheIndex);
            }

            worker->setOnerror(listener);
            createHiddenDependency(info.Holder(), value, V8Custom::kAbstractWorkerRequestCacheIndex);
        }
    }
}

CALLBACK_FUNC_DECL(AbstractWorkerAddEventListener)
{
    INC_STATS(L"DOM.AbstractWorker.addEventListener()");
    AbstractWorker* worker = V8DOMWrapper::convertToNativeObject<AbstractWorker>(V8ClassIndex::ABSTRACTWORKER, args.Holder());

    RefPtr<EventListener> listener = getEventListener(worker, args[1], false, false);
    if (listener) {
        String type = toWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        worker->addEventListener(type, listener, useCapture);

        createHiddenDependency(args.Holder(), args[1], V8Custom::kAbstractWorkerRequestCacheIndex);
    }
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(AbstractWorkerRemoveEventListener)
{
    INC_STATS(L"DOM.AbstractWorker.removeEventListener()");
    AbstractWorker* worker = V8DOMWrapper::convertToNativeObject<AbstractWorker>(V8ClassIndex::ABSTRACTWORKER, args.Holder());

    RefPtr<EventListener> listener = getEventListener(worker, args[1], false, true);
    if (listener) {
        String type = toWebCoreString(args[0]);
        bool useCapture = args[2]->BooleanValue();
        worker->removeEventListener(type, listener.get(), useCapture);

        removeHiddenDependency(args.Holder(), args[1], V8Custom::kAbstractWorkerRequestCacheIndex);
    }

    return v8::Undefined();
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
