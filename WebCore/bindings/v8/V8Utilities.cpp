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

#include "ChromiumBridge.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "ScriptExecutionContext.h"
#include "ScriptState.h"
#include "V8Binding.h"
#include "V8BindingState.h"
#include "V8DOMWindow.h"
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

ScriptExecutionContext* getScriptExecutionContext()
{
#if ENABLE(WORKERS)
    WorkerScriptController* controller = WorkerScriptController::controllerForContext();
    if (controller) {
        WorkerContextExecutionProxy* proxy = controller->proxy();
        return proxy ? proxy->workerContext()->scriptExecutionContext() : 0;
    }
#endif

    Frame* frame = V8Proxy::retrieveFrameForCurrentContext();
    if (frame)
        return frame->document()->scriptExecutionContext();

    return 0;
}

void logPropertyAccess(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    Frame* target = V8DOMWindow::toNative(info.Holder())->frame();
    Frame* active = V8BindingState::Only()->getActiveWindow()->frame();
    if (target == active)
        return;

    bool crossSite = !V8BindingSecurity::canAccessFrame(V8BindingState::Only(), target, false);
    String propName = toWebCoreString(name);

    // For cross-site, we also want to identify the current event to record repeat accesses.
    unsigned long long eventId = 0;
    if (crossSite) {
        v8::HandleScope handleScope;
        v8::Handle<v8::Context> v8Context = V8Proxy::mainWorldContext(active);
        if (!v8Context.IsEmpty()) {
            v8::Context::Scope scope(v8Context);
            v8::Handle<v8::Object> global = v8Context->Global();
            v8::Handle<v8::Value> jsEvent = global->Get(v8::String::NewSymbol("event"));
            if (V8DOMWrapper::isValidDOMObject(jsEvent))
                eventId = reinterpret_cast<unsigned long long>(V8Event::toNative(v8::Handle<v8::Object>::Cast(jsEvent)));
        }
    }
    active->loader()->client()->logCrossFramePropertyAccess(target, crossSite, propName, eventId);
}

} // namespace WebCore
