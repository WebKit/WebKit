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

#include "ScriptExecutionContext.h"
#include "V8Binding.h"
#include "V8Callback.h"
#include "V8MutationObserver.h"
#include "V8MutationRecord.h"
#include <wtf/Assertions.h>

namespace WebCore {

V8MutationCallback::V8MutationCallback(v8::Handle<v8::Object> callback, ScriptExecutionContext* context, v8::Handle<v8::Object> owner, v8::Isolate* isolate)
    : ActiveDOMCallback(context)
    , m_callback(callback)
    , m_world(DOMWrapperWorld::getWorld(v8::Context::GetCurrent()))
{
    owner->SetHiddenValue(V8HiddenPropertyName::callback(), callback);
    m_callback.get().MakeWeak(isolate, this, &V8MutationCallback::weakCallback);
}

bool V8MutationCallback::handleEvent(MutationRecordArray* mutations, MutationObserver* observer)
{
    ASSERT(mutations);
    if (!mutations)
        return true;

    if (!canInvokeCallback())
        return true;

    v8::HandleScope handleScope;

    v8::Handle<v8::Context> v8Context = toV8Context(scriptExecutionContext(), m_world.get());
    if (v8Context.IsEmpty())
        return true;

    v8::Context::Scope scope(v8Context);

    v8::Local<v8::Array> mutationsArray = v8::Array::New(mutations->size());
    for (size_t i = 0; i < mutations->size(); ++i)
        mutationsArray->Set(v8Integer(i, v8Context->GetIsolate()), toV8(mutations->at(i).get(), v8::Handle<v8::Object>(), v8Context->GetIsolate()));

    v8::Handle<v8::Value> observerHandle = toV8(observer, v8::Handle<v8::Object>(), v8Context->GetIsolate());
    if (observerHandle.IsEmpty()) {
        if (!isScriptControllerTerminating())
            CRASH();
        return true;
    }

    if (!observerHandle->IsObject())
        return true;

    v8::Handle<v8::Value> argv[] = {
        mutationsArray,
        observerHandle
    };

    bool callbackReturnValue = false;
    return !invokeCallback(m_callback.get(), v8::Handle<v8::Object>::Cast(observerHandle), 2, argv, callbackReturnValue, scriptExecutionContext());
}

} // namespace WebCore
