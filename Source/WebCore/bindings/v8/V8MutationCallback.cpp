/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "V8MutationCallback.h"

#include "ScriptController.h"
#include "ScriptExecutionContext.h"
#include "V8Binding.h"
#include "V8MutationObserver.h"
#include "V8MutationRecord.h"
#include <wtf/Assertions.h>

namespace WebCore {

template<>
void WeakHandleListener<V8MutationCallback>::callback(v8::Isolate*, v8::Persistent<v8::Value>, V8MutationCallback* callback)
{
    callback->m_callback.clear();
}

V8MutationCallback::V8MutationCallback(v8::Handle<v8::Function> callback, ScriptExecutionContext* context, v8::Handle<v8::Object> owner, v8::Isolate* isolate)
    : ActiveDOMCallback(context)
    , m_callback(callback)
    , m_world(DOMWrapperWorld::isolatedWorld(v8::Context::GetCurrent()))
{
    owner->SetHiddenValue(V8HiddenPropertyName::callback(), callback);
    WeakHandleListener<V8MutationCallback>::makeWeak(isolate, m_callback.get(), this);
}

void V8MutationCallback::call(const Vector<RefPtr<MutationRecord> >& mutations, MutationObserver* observer)
{
    if (!canInvokeCallback())
        return;

    v8::HandleScope handleScope;

    v8::Handle<v8::Context> v8Context = toV8Context(scriptExecutionContext(), m_world.get());
    if (v8Context.IsEmpty())
        return;

    v8::Context::Scope scope(v8Context);
    v8::Isolate* isolate = v8Context->GetIsolate();

    v8::Handle<v8::Function> callback = v8::Local<v8::Function>::New(isolate, m_callback.get());

    v8::Handle<v8::Value> observerHandle = toV8(observer, v8::Handle<v8::Object>(), isolate);
    if (observerHandle.IsEmpty()) {
        if (!isScriptControllerTerminating())
            CRASH();
        return;
    }

    if (!observerHandle->IsObject())
        return;

    v8::Handle<v8::Object> thisObject = v8::Handle<v8::Object>::Cast(observerHandle);
    v8::Handle<v8::Value> argv[] = { v8Array(mutations, isolate), observerHandle };

    v8::TryCatch exceptionCatcher;
    exceptionCatcher.SetVerbose(true);
    ScriptController::callFunctionWithInstrumentation(scriptExecutionContext(), callback, thisObject, 2, argv);
}

} // namespace WebCore
