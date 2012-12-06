/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#if ENABLE(INSPECTOR)
#include "InjectedScriptManager.h"

#include "BindingState.h"
#include "DOMWindow.h"
#include "InjectedScriptHost.h"
#include "ScriptObject.h"
#include "V8Binding.h"
#include "V8DOMWindow.h"
#include "V8InjectedScriptHost.h"
#include "V8RecursionScope.h"
#include <wtf/RefPtr.h>

namespace WebCore {

static void WeakReferenceCallback(v8::Persistent<v8::Value> object, void* parameter)
{
    InjectedScriptHost* nativeObject = static_cast<InjectedScriptHost*>(parameter);
    nativeObject->deref();
    object.Dispose();
    object.Clear();
}

static v8::Local<v8::Object> createInjectedScriptHostV8Wrapper(InjectedScriptHost* host)
{
    v8::Local<v8::Function> function = V8InjectedScriptHost::GetTemplate()->GetFunction();
    if (function.IsEmpty()) {
        // Return if allocation failed.
        return v8::Local<v8::Object>();
    }
    v8::Local<v8::Object> instance = V8ObjectConstructor::newInstance(function);
    if (instance.IsEmpty()) {
        // Avoid setting the wrapper if allocation failed.
        return v8::Local<v8::Object>();
    }
    V8DOMWrapper::setNativeInfo(instance, &V8InjectedScriptHost::info, host);
    // Create a weak reference to the v8 wrapper of InspectorBackend to deref
    // InspectorBackend when the wrapper is garbage collected.
    host->ref();
    v8::Persistent<v8::Object> weakHandle = v8::Persistent<v8::Object>::New(instance);
    weakHandle.MakeWeak(host, &WeakReferenceCallback);
    return instance;
}

ScriptObject InjectedScriptManager::createInjectedScript(const String& scriptSource, ScriptState* inspectedScriptState, int id)
{
    v8::HandleScope scope;

    v8::Local<v8::Context> inspectedContext = inspectedScriptState->context();
    v8::Context::Scope contextScope(inspectedContext);

    // Call custom code to create InjectedScripHost wrapper specific for the context
    // instead of calling toV8() that would create the
    // wrapper in the current context.
    // FIXME: make it possible to use generic bindings factory for InjectedScriptHost.
    v8::Local<v8::Object> scriptHostWrapper = createInjectedScriptHostV8Wrapper(m_injectedScriptHost.get());
    if (scriptHostWrapper.IsEmpty())
        return ScriptObject();

    v8::Local<v8::Object> windowGlobal = inspectedContext->Global();

    // Inject javascript into the context. The compiled script is supposed to evaluate into
    // a single anonymous function(it's anonymous to avoid cluttering the global object with
    // inspector's stuff) the function is called a few lines below with InjectedScriptHost wrapper,
    // injected script id and explicit reference to the inspected global object. The function is expected
    // to create and configure InjectedScript instance that is going to be used by the inspector.
    v8::Local<v8::Script> script = v8::Script::Compile(deprecatedV8String(scriptSource));
    V8RecursionScope::MicrotaskSuppression recursionScope;
    v8::Local<v8::Value> v = script->Run();
    ASSERT(!v.IsEmpty());
    ASSERT(v->IsFunction());

    v8::Handle<v8::Value> args[] = {
      scriptHostWrapper,
      windowGlobal,
      v8::Number::New(id),
    };
    v8::Local<v8::Value> injectedScriptValue = v8::Function::Cast(*v)->Call(windowGlobal, 3, args);
    return ScriptObject(inspectedScriptState, v8::Handle<v8::Object>::Cast(injectedScriptValue));
}

bool InjectedScriptManager::canAccessInspectedWindow(ScriptState* scriptState)
{
    v8::HandleScope handleScope;
    v8::Local<v8::Context> context = scriptState->context();
    v8::Local<v8::Object> global = context->Global();
    if (global.IsEmpty())
        return false;
    v8::Handle<v8::Object> holder = global->FindInstanceInPrototypeChain(V8DOMWindow::GetTemplate());
    if (holder.IsEmpty())
        return false;
    Frame* frame = V8DOMWindow::toNative(holder)->frame();

    v8::Context::Scope contextScope(context);
    return BindingSecurity::shouldAllowAccessToFrame(BindingState::instance(), frame, DoNotReportSecurityError);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
