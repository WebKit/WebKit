/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
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
#include "ScheduledAction.h"

#include "Document.h"
#include "ScriptExecutionContext.h"
#include "ScriptSourceCode.h"

#if PLATFORM(CHROMIUM)
#include "TraceEvent.h"
#endif

#include "V8Binding.h"
#include "V8Proxy.h"
#include "WorkerContext.h"
#include "WorkerContextExecutionProxy.h"
#include "WorkerThread.h"

namespace WebCore {

ScheduledAction::ScheduledAction(v8::Handle<v8::Context> context, v8::Handle<v8::Function> func, int argc, v8::Handle<v8::Value> argv[])
    : m_context(context)
    , m_code(String(), KURL(), TextPosition::belowRangePosition())
{
    m_function = v8::Persistent<v8::Function>::New(func);

#ifndef NDEBUG
    V8GCController::registerGlobalHandle(SCHEDULED_ACTION, this, m_function);
#endif

    m_argc = argc;
    if (argc > 0) {
        m_argv = new v8::Persistent<v8::Value>[argc];
        for (int i = 0; i < argc; i++) {
            m_argv[i] = v8::Persistent<v8::Value>::New(argv[i]);

#ifndef NDEBUG
    V8GCController::registerGlobalHandle(SCHEDULED_ACTION, this, m_argv[i]);
#endif
        }
    } else
        m_argv = 0;
}

ScheduledAction::~ScheduledAction()
{
    if (m_function.IsEmpty())
        return;

#ifndef NDEBUG
    V8GCController::unregisterGlobalHandle(this, m_function);
#endif
    m_function.Dispose();

    for (int i = 0; i < m_argc; i++) {
#ifndef NDEBUG
        V8GCController::unregisterGlobalHandle(this, m_argv[i]);
#endif
        m_argv[i].Dispose();
    }

    if (m_argc > 0)
        delete[] m_argv;
}

void ScheduledAction::execute(ScriptExecutionContext* context)
{
    if (context->isDocument()) {
        Frame* frame = static_cast<Document*>(context)->frame();
        ScriptController* scriptController = frame->script();
        if (!scriptController->canExecuteScripts(NotAboutToExecuteScript))
            return;
        V8Proxy* proxy = V8Proxy::retrieve(frame);
        execute(proxy);
    }
#if ENABLE(WORKERS)
    else {
        ASSERT(context->isWorkerContext());
        execute(static_cast<WorkerContext*>(context));
    }
#endif
}

void ScheduledAction::execute(V8Proxy* proxy)
{
    ASSERT(proxy);

    v8::HandleScope handleScope;
    v8::Handle<v8::Context> v8Context = v8::Local<v8::Context>::New(m_context.get());
    if (v8Context.IsEmpty())
        return; // JS may not be enabled.

#if PLATFORM(CHROMIUM)
    TRACE_EVENT("ScheduledAction::execute", this, 0);
#endif

    v8::Context::Scope scope(v8Context);

    // FIXME: Need to implement timeouts for preempting a long-running script.
    if (!m_function.IsEmpty() && m_function->IsFunction()) {
        proxy->callFunction(v8::Persistent<v8::Function>::Cast(m_function), v8Context->Global(), m_argc, m_argv);
        Document::updateStyleForAllDocuments();
    } else
        proxy->evaluate(m_code, 0);

    // The 'proxy' may be invalid at this point since JS could have released the owning Frame.
}

#if ENABLE(WORKERS)
void ScheduledAction::execute(WorkerContext* workerContext)
{
    // In a Worker, the execution should always happen on a worker thread.
    ASSERT(workerContext->thread()->threadID() == currentThread());
  
    WorkerScriptController* scriptController = workerContext->script();

    if (!m_function.IsEmpty() && m_function->IsFunction()) {
        v8::HandleScope handleScope;
        v8::Handle<v8::Context> v8Context = v8::Local<v8::Context>::New(m_context.get());
        ASSERT(!v8Context.IsEmpty());
        v8::Context::Scope scope(v8Context);
        m_function->Call(v8Context->Global(), m_argc, m_argv);
    } else
        scriptController->evaluate(m_code);
}
#endif

} // namespace WebCore
