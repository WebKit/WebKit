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

#include <v8.h>

#include "Document.h"
#include "Frame.h"
#include "ScriptExecutionContext.h"
#include "ScriptState.h"
#include "V8CustomBinding.h"
#include "V8Binding.h"
#include "V8Proxy.h"
#include "WorkerContext.h"
#include "WorkerContextExecutionProxy.h"

#include <wtf/Assertions.h>
#include "Frame.h"

namespace WebCore {

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
    cacheArray->Set(v8::Integer::New(cacheArray->Length()), value);
}

void removeHiddenDependency(v8::Handle<v8::Object> object, v8::Local<v8::Value> value, int cacheIndex)
{
    v8::Local<v8::Value> cache = object->GetInternalField(cacheIndex);
    if (!cache->IsArray())
        return;
    v8::Local<v8::Array> cacheArray = v8::Local<v8::Array>::Cast(cache);
    for (int i = cacheArray->Length() - 1; i >= 0; --i) {
        v8::Local<v8::Value> cached = cacheArray->Get(v8::Integer::New(i));
        if (cached->StrictEquals(value)) {
            cacheArray->Delete(i);
            return;
        }
    }
}

bool processingUserGesture()
{
    Frame* frame = V8Proxy::retrieveFrameForEnteredContext();
    return frame && frame->script()->processingUserGesture();
}

bool shouldAllowNavigation(Frame* frame)
{
    Frame* callingFrame = V8Proxy::retrieveFrameForCallingContext();
    return callingFrame && callingFrame->loader()->shouldAllowNavigation(frame);
}

KURL completeURL(const String& relativeURL)
{
    // For histoical reasons, we need to complete the URL using the dynamic frame.
    Frame* frame = V8Proxy::retrieveFrameForEnteredContext();
    if (!frame)
        return KURL();
    return frame->loader()->completeURL(relativeURL);
}

void navigateIfAllowed(Frame* frame, const KURL& url, bool lockHistory, bool lockBackForwardList)
{
    Frame* callingFrame = V8Proxy::retrieveFrameForCallingContext();
    if (!callingFrame)
        return;

    if (!protocolIsJavaScript(url) || ScriptController::isSafeScript(frame))
        frame->redirectScheduler()->scheduleLocationChange(url.string(), callingFrame->loader()->outgoingReferrer(), lockHistory, lockBackForwardList, processingUserGesture());
}

ScriptExecutionContext* getScriptExecutionContext(ScriptState* scriptState)
{
#if ENABLE(WORKERS)
    WorkerContextExecutionProxy* proxy = WorkerContextExecutionProxy::retrieve();
    if (proxy)
        return proxy->workerContext()->scriptExecutionContext();
#endif

    if (scriptState)
        return scriptState->frame()->document()->scriptExecutionContext();
    else {
        Frame* frame = V8Proxy::retrieveFrameForCurrentContext();
        if (frame)
            return frame->document()->scriptExecutionContext();
    }

    return 0;
}

void reportException(ScriptState* scriptState, v8::TryCatch& exceptionCatcher)
{
    String errorMessage;
    int lineNumber = 0;
    String sourceURL;

    // There can be a situation that an exception is thrown without setting a message.
    v8::Local<v8::Message> message = exceptionCatcher.Message();
    if (message.IsEmpty())
        errorMessage = toWebCoreString(exceptionCatcher.Exception()->ToString());
    else {
        errorMessage = toWebCoreString(message->Get());
        lineNumber = message->GetLineNumber();
        sourceURL = toWebCoreString(message->GetScriptResourceName());
    }

    getScriptExecutionContext(scriptState)->reportException(errorMessage, lineNumber, sourceURL);
    exceptionCatcher.Reset();
}

} // namespace WebCore
