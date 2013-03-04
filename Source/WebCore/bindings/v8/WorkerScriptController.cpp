/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
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

#include "WorkerScriptController.h"

#include "DOMTimer.h"
#include "ScriptCallStack.h"
#include "ScriptRunner.h"
#include "ScriptSourceCode.h"
#include "ScriptValue.h"
#include "V8DedicatedWorkerContext.h"
#include "V8Initializer.h"
#include "V8SharedWorkerContext.h"
#include "V8WorkerContext.h"
#include "WorkerContext.h"
#include "WorkerObjectProxy.h"
#include "WorkerThread.h"
#include "WrapperTypeInfo.h"
#include <v8.h>

#if PLATFORM(CHROMIUM)
#include <public/Platform.h>
#include <public/WebWorkerRunLoop.h>
#endif

namespace WebCore {

WorkerScriptController::WorkerScriptController(WorkerContext* workerContext)
    : m_workerContext(workerContext)
    , m_isolate(v8::Isolate::New())
    , m_executionForbidden(false)
    , m_executionScheduledToTerminate(false)
{
    m_isolate->Enter();
    V8PerIsolateData* data = V8PerIsolateData::create(m_isolate);
    m_domDataStore = adoptPtr(new DOMDataStore(WorkerWorld));
    data->setDOMDataStore(m_domDataStore.get());

    V8Initializer::initializeWorker(m_isolate);
}

WorkerScriptController::~WorkerScriptController()
{
    m_domDataStore.clear();
#if PLATFORM(CHROMIUM)
    // The corresponding call to didStartWorkerRunLoop is in
    // WorkerThread::workerThread().
    // See http://webkit.org/b/83104#c14 for why this is here.
    WebKit::Platform::current()->didStopWorkerRunLoop(WebKit::WebWorkerRunLoop(&m_workerContext->thread()->runLoop()));
#endif
    disposeContext();
    V8PerIsolateData::dispose(m_isolate);
    m_isolate->Exit();
    m_isolate->Dispose();
}

void WorkerScriptController::disposeContext()
{
    m_perContextData.clear();
    m_context.clear();
}

bool WorkerScriptController::initializeContextIfNeeded()
{
    if (!m_context.isEmpty())
        return true;

    v8::Persistent<v8::ObjectTemplate> globalTemplate;
    m_context.adopt(v8::Context::New(0, globalTemplate));
    if (m_context.isEmpty())
        return false;

    // Starting from now, use local context only.
    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(m_context.get());

    v8::Context::Scope scope(context);

    m_perContextData = V8PerContextData::create(m_context.get());
    if (!m_perContextData->init()) {
        disposeContext();
        return false;
    }

    // Set DebugId for the new context.
    context->SetEmbedderData(0, v8::String::NewSymbol("worker"));

    // Create a new JS object and use it as the prototype for the shadow global object.
    WrapperTypeInfo* contextType = &V8DedicatedWorkerContext::info;
#if ENABLE(SHARED_WORKERS)
    if (!m_workerContext->isDedicatedWorkerContext())
        contextType = &V8SharedWorkerContext::info;
#endif
    v8::Handle<v8::Function> workerContextConstructor = m_perContextData->constructorForType(contextType);
    v8::Local<v8::Object> jsWorkerContext = V8ObjectConstructor::newInstance(workerContextConstructor);
    if (jsWorkerContext.IsEmpty()) {
        disposeContext();
        return false;
    }

    V8DOMWrapper::associateObjectWithWrapper(PassRefPtr<WorkerContext>(m_workerContext), contextType, jsWorkerContext, m_isolate, WrapperConfiguration::Dependent);

    // Insert the object instance as the prototype of the shadow object.
    v8::Handle<v8::Object> globalObject = v8::Handle<v8::Object>::Cast(m_context->Global()->GetPrototype());
    globalObject->SetPrototype(jsWorkerContext);

    return true;
}

ScriptValue WorkerScriptController::evaluate(const String& script, const String& fileName, const TextPosition& scriptStartPosition, WorkerContextExecutionState* state)
{
    V8GCController::checkMemoryUsage();

    v8::HandleScope handleScope;

    if (!initializeContextIfNeeded())
        return ScriptValue();

    if (!m_disableEvalPending.isEmpty()) {
        m_context->AllowCodeGenerationFromStrings(false);
        m_context->SetErrorMessageForCodeGenerationFromStrings(v8String(m_disableEvalPending, m_context->GetIsolate()));
        m_disableEvalPending = String();
    }

    v8::Isolate* isolate = m_context->GetIsolate();
    v8::Context::Scope scope(m_context.get());

    v8::TryCatch block;

    v8::Handle<v8::String> scriptString = v8String(script, isolate);
    v8::Handle<v8::Script> compiledScript = ScriptSourceCode::compileScript(scriptString, fileName, scriptStartPosition, 0, isolate);
    v8::Local<v8::Value> result = ScriptRunner::runCompiledScript(compiledScript, m_workerContext);

    if (!block.CanContinue()) {
        m_workerContext->script()->forbidExecution();
        return ScriptValue();
    }

    if (block.HasCaught()) {
        v8::Local<v8::Message> message = block.Message();
        state->hadException = true;
        state->errorMessage = toWebCoreString(message->Get());
        state->lineNumber = message->GetLineNumber();
        state->sourceURL = toWebCoreString(message->GetScriptResourceName());
        if (m_workerContext->sanitizeScriptError(state->errorMessage, state->lineNumber, state->sourceURL))
            state->exception = throwError(v8GeneralError, state->errorMessage.utf8().data(), isolate);
        else
            state->exception = ScriptValue(block.Exception());

        block.Reset();
    } else
        state->hadException = false;

    if (result.IsEmpty() || result->IsUndefined())
        return ScriptValue();

    return ScriptValue(result);
}

void WorkerScriptController::evaluate(const ScriptSourceCode& sourceCode, ScriptValue* exception)
{
    if (isExecutionForbidden())
        return;

    WorkerContextExecutionState state;
    evaluate(sourceCode.source(), sourceCode.url().string(), sourceCode.startPosition(), &state);
    if (state.hadException) {
        if (exception)
            *exception = state.exception;
        else
            m_workerContext->reportException(state.errorMessage, state.lineNumber, state.sourceURL, 0);
    }
}

void WorkerScriptController::scheduleExecutionTermination()
{
    // The mutex provides a memory barrier to ensure that once
    // termination is scheduled, isExecutionTerminating will
    // accurately reflect that state when called from another thread.
    {
        MutexLocker locker(m_scheduledTerminationMutex);
        m_executionScheduledToTerminate = true;
    }
    v8::V8::TerminateExecution(m_isolate);
}

bool WorkerScriptController::isExecutionTerminating() const
{
    // See comments in scheduleExecutionTermination regarding mutex usage.
    MutexLocker locker(m_scheduledTerminationMutex);
    return m_executionScheduledToTerminate;
}

void WorkerScriptController::forbidExecution()
{
    ASSERT(m_workerContext->isContextThread());
    m_executionForbidden = true;
}

bool WorkerScriptController::isExecutionForbidden() const
{
    ASSERT(m_workerContext->isContextThread());
    return m_executionForbidden;
}

void WorkerScriptController::disableEval(const String& errorMessage)
{
    m_disableEvalPending = errorMessage;
}

void WorkerScriptController::setException(const ScriptValue& exception)
{
    throwError(*exception.v8Value(), m_context->GetIsolate());
}

WorkerScriptController* WorkerScriptController::controllerForContext()
{
    // Happens on frame destruction, check otherwise GetCurrent() will crash.
    if (!v8::Context::InContext())
        return 0;
    v8::Handle<v8::Context> context = v8::Context::GetCurrent();
    v8::Handle<v8::Object> global = context->Global();
    global = global->FindInstanceInPrototypeChain(V8WorkerContext::GetTemplate(context->GetIsolate(), WorkerWorld));
    // Return 0 if the current executing context is not the worker context.
    if (global.IsEmpty())
        return 0;
    WorkerContext* workerContext = V8WorkerContext::toNative(global);
    return workerContext->script();
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
