/*
 * Copyright (C) 2009, 2011 Google Inc. All rights reserved.
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
#include "ScriptState.h"

#include "Frame.h"
#include "Node.h"
#include "Page.h"
#include "ScriptController.h"
#include "V8DOMWindow.h"
#include "V8HiddenPropertyName.h"
#include "V8WorkerContext.h"
#include "WorkerContext.h"
#include "WorkerScriptController.h"
#include <v8.h>
#include <wtf/Assertions.h>

namespace WebCore {

template<>
void WeakHandleListener<ScriptState>::callback(v8::Isolate* isolate, v8::Persistent<v8::Value> object, ScriptState* scriptState)
{
    delete scriptState;
}

ScriptState::ScriptState(v8::Handle<v8::Context> context)
    : m_context(context)
{
    WeakHandleListener<ScriptState>::makeWeak(context->GetIsolate(), m_context.get(), this);
}

ScriptState::~ScriptState()
{
}

DOMWindow* ScriptState::domWindow() const
{
    v8::HandleScope handleScope;
    return toDOMWindow(m_context.get());
}

ScriptExecutionContext* ScriptState::scriptExecutionContext() const
{
    v8::HandleScope handleScope;
    return toScriptExecutionContext(m_context.get());
}

ScriptState* ScriptState::forContext(v8::Local<v8::Context> context)
{
    v8::Context::Scope contextScope(context);

    v8::Local<v8::Object> innerGlobal = v8::Local<v8::Object>::Cast(context->Global()->GetPrototype());

    v8::Local<v8::Value> scriptStateWrapper = innerGlobal->GetHiddenValue(V8HiddenPropertyName::scriptState());
    if (!scriptStateWrapper.IsEmpty() && scriptStateWrapper->IsExternal())
        return static_cast<ScriptState*>(v8::External::Cast(*scriptStateWrapper)->Value());

    ScriptState* scriptState = new ScriptState(context);
    innerGlobal->SetHiddenValue(V8HiddenPropertyName::scriptState(), v8::External::New(scriptState));
    return scriptState;
}

ScriptState* ScriptState::current()
{
    v8::HandleScope handleScope;
    v8::Local<v8::Context> context = v8::Context::GetCurrent();
    ASSERT(!context.IsEmpty());
    return ScriptState::forContext(context);
}

DOMWindow* domWindowFromScriptState(ScriptState* scriptState)
{
    return scriptState->domWindow();
}

ScriptExecutionContext* scriptExecutionContextFromScriptState(ScriptState* scriptState)
{
    return scriptState->scriptExecutionContext();
}

bool evalEnabled(ScriptState* scriptState)
{
    v8::HandleScope handleScope;
    return scriptState->context()->IsCodeGenerationFromStringsAllowed();
}

void setEvalEnabled(ScriptState* scriptState, bool enabled)
{
    v8::HandleScope handleScope;
    return scriptState->context()->AllowCodeGenerationFromStrings(enabled);
}

ScriptState* mainWorldScriptState(Frame* frame)
{
    v8::HandleScope handleScope;
    return ScriptState::forContext(frame->script()->mainWorldContext());
}

ScriptState* scriptStateFromNode(DOMWrapperWorld*, Node* node)
{
    // This should be never reached with V8 bindings (WebKit only uses it
    // for non-JS bindings)
    ASSERT_NOT_REACHED();
    return 0;
}

ScriptState* scriptStateFromPage(DOMWrapperWorld*, Page* page)
{
    // This should be only reached with V8 bindings from single process layout tests.
    return mainWorldScriptState(page->mainFrame());
}

#if ENABLE(WORKERS)
ScriptState* scriptStateFromWorkerContext(WorkerContext* workerContext)
{
    WorkerScriptController* script = workerContext->script();
    if (!script)
        return 0;

    v8::HandleScope handleScope;
    return ScriptState::forContext(script->context());
}
#endif

}
