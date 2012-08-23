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
#include "Frame.h"
#include "ScriptExecutionContext.h"
#include "ScriptSourceCode.h"

#if PLATFORM(CHROMIUM)
#include "TraceEvent.h"
#endif

#include "ScriptController.h"
#include "V8Binding.h"
#include "V8RecursionScope.h"
#include "WorkerContext.h"
#include "WorkerContextExecutionProxy.h"
#include "WorkerThread.h"

namespace WebCore {

ScheduledAction::ScheduledAction(v8::Handle<v8::Context> context, v8::Handle<v8::Function> function, int argc, v8::Handle<v8::Value> argv[])
    : m_context(context)
    , m_function(function)
    , m_code(String(), KURL(), TextPosition::belowRangePosition())
{
    m_args.reserveCapacity(argc);
    for (int i = 0; i < argc; ++i)
        m_args.append(v8::Persistent<v8::Value>::New(argv[i]));
}

ScheduledAction::~ScheduledAction()
{
    for (size_t i = 0; i < m_args.size(); ++i)
        m_args[i].Dispose();
}

void ScheduledAction::execute(ScriptExecutionContext* context)
{
    if (context->isDocument()) {
        Frame* frame = static_cast<Document*>(context)->frame();
        if (!frame)
            return;
        if (!frame->script()->canExecuteScripts(AboutToExecuteScript))
            return;
        execute(frame);
    }
#if ENABLE(WORKERS)
    else {
        ASSERT(context->isWorkerContext());
        execute(static_cast<WorkerContext*>(context));
    }
#endif
}

void ScheduledAction::execute(Frame* frame)
{
    v8::HandleScope handleScope;

    v8::Handle<v8::Context> context = v8::Local<v8::Context>::New(m_context.get());
    if (context.IsEmpty())
        return;
    v8::Context::Scope scope(context);

#if PLATFORM(CHROMIUM)
    TRACE_EVENT0("v8", "ScheduledAction::execute");
#endif

    v8::Handle<v8::Function> function = m_function.get();
    if (!function.IsEmpty())
        frame->script()->callFunction(function, context->Global(), m_args.size(), m_args.data());
    else
        frame->script()->compileAndRunScript(m_code);

    // The frame might be invalid at this point because JavaScript could have released it.
}

#if ENABLE(WORKERS)
void ScheduledAction::execute(WorkerContext* worker)
{
    ASSERT(worker->thread()->threadID() == currentThread());

    V8RecursionScope recursionScope(worker);

    v8::Handle<v8::Function> function = m_function.get();
    if (!function.IsEmpty()) {
        v8::HandleScope handleScope;

        v8::Handle<v8::Context> context = v8::Local<v8::Context>::New(m_context.get());
        ASSERT(!context.IsEmpty());
        v8::Context::Scope scope(context);

        function->Call(context->Global(), m_args.size(), m_args.data());
    } else
        worker->script()->evaluate(m_code);
}
#endif

} // namespace WebCore
