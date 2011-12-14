/*
 * Copyright (c) 2011 Google Inc. All rights reserved.
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
#include "WorkerScriptDebugServer.h"

#if ENABLE(JAVASCRIPT_DEBUGGER) && ENABLE(WORKERS)

#include "ScriptDebugListener.h"
#include "V8DOMWrapper.h"
#include "V8DedicatedWorkerContext.h"
#include "V8SharedWorkerContext.h"
#include "WorkerContext.h"
#include "WorkerContextExecutionProxy.h"
#include "WorkerThread.h"
#include <v8.h>
#include <wtf/MessageQueue.h>

namespace WebCore {

static WorkerContext* retrieveWorkerContext(v8::Handle<v8::Context> context)
{
    v8::Handle<v8::Object> global = context->Global();
    ASSERT(!global.IsEmpty());

    v8::Handle<v8::Object> prototype = v8::Handle<v8::Object>::Cast(global->GetPrototype());
    ASSERT(!prototype.IsEmpty());

    prototype = v8::Handle<v8::Object>::Cast(prototype->GetPrototype());
    ASSERT(!prototype.IsEmpty());

    WrapperTypeInfo* typeInfo = V8DOMWrapper::domWrapperType(prototype);
    if (&V8DedicatedWorkerContext::info == typeInfo)
        return V8DedicatedWorkerContext::toNative(prototype);
    if (&V8SharedWorkerContext::info == typeInfo)
        return V8SharedWorkerContext::toNative(prototype);
    ASSERT_NOT_REACHED();
    return 0;
}

WorkerScriptDebugServer::WorkerScriptDebugServer()
    : ScriptDebugServer()
    , m_pausedWorkerContext(0)
{
}

void WorkerScriptDebugServer::addListener(ScriptDebugListener* listener, WorkerContext* workerContext)
{
    v8::HandleScope scope;
    v8::Local<v8::Context> debuggerContext = v8::Debug::GetDebugContext();
    v8::Context::Scope contextScope(debuggerContext);

    if (!m_listenersMap.size()) {
        // FIXME: synchronize access to this code.
        ensureDebuggerScriptCompiled();
        ASSERT(!m_debuggerScript.get()->IsUndefined());
        v8::Debug::SetDebugEventListener2(&WorkerScriptDebugServer::v8DebugEventCallback, v8::External::New(this));
    }
    m_listenersMap.set(workerContext, listener);
    
    WorkerContextExecutionProxy* proxy = workerContext->script()->proxy();
    if (!proxy)
        return;
    v8::Handle<v8::Context> context = proxy->context();

    v8::Handle<v8::Function> getScriptsFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("getWorkerScripts")));
    v8::Handle<v8::Value> argv[] = { v8::Handle<v8::Value>() };
    v8::Handle<v8::Value> value = getScriptsFunction->Call(m_debuggerScript.get(), 0, argv);
    if (value.IsEmpty())
        return;
    ASSERT(!value->IsUndefined() && value->IsArray());
    v8::Handle<v8::Array> scriptsArray = v8::Handle<v8::Array>::Cast(value);
    for (unsigned i = 0; i < scriptsArray->Length(); ++i)
        dispatchDidParseSource(listener, v8::Handle<v8::Object>::Cast(scriptsArray->Get(v8::Integer::New(i))));
}

void WorkerScriptDebugServer::removeListener(ScriptDebugListener* listener, WorkerContext* workerContext)
{
    if (!m_listenersMap.contains(workerContext))
        return;

    if (m_pausedWorkerContext == workerContext)
        continueProgram();

    m_listenersMap.remove(workerContext);

    if (m_listenersMap.isEmpty())
        v8::Debug::SetDebugEventListener2(0);
}

ScriptDebugListener* WorkerScriptDebugServer::getDebugListenerForContext(v8::Handle<v8::Context> context)
{
    WorkerContext* workerContext = retrieveWorkerContext(context);
    if (!workerContext)
        return 0;
    return m_listenersMap.get(workerContext);
}

void WorkerScriptDebugServer::runMessageLoopOnPause(v8::Handle<v8::Context> context)
{
    WorkerContext* workerContext = retrieveWorkerContext(context);
    WorkerThread* workerThread = workerContext->thread();

    m_pausedWorkerContext = workerContext;

    MessageQueueWaitResult result;
    do {
        result = workerThread->runLoop().runInMode(workerContext, "debugger");
    // Keep waiting until execution is resumed.
    } while (result == MessageQueueMessageReceived && isPaused());
    m_pausedWorkerContext = 0;
    
    // The listener may have been removed in the nested loop.
    if (ScriptDebugListener* listener = m_listenersMap.get(workerContext))
        listener->didContinue();
}

void WorkerScriptDebugServer::quitMessageLoopOnPause()
{
    // FIXME: do exit nested loop when listener is removed on pause.
}

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER) && ENABLE(WORKERS)
