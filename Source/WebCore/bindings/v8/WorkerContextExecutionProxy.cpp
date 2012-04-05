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

#if ENABLE(WORKERS)

#include "WorkerContextExecutionProxy.h"

#include "DedicatedWorkerContext.h"
#include "Event.h"
#include "SafeAllocation.h"
#include "ScriptCallStack.h"
#include "SharedWorker.h"
#include "SharedWorkerContext.h"
#include "V8Binding.h"
#include "V8BindingPerContextData.h"
#include "V8DOMMap.h"
#include "V8DOMWindowShell.h"
#include "V8DedicatedWorkerContext.h"
#include "V8Proxy.h"
#include "V8RecursionScope.h"
#include "V8SharedWorkerContext.h"
#include "Worker.h"
#include "WorkerContext.h"
#include "WorkerScriptController.h"
#include "WrapperTypeInfo.h"
#include <wtf/text/CString.h>

namespace WebCore {

static void reportFatalErrorInV8(const char* location, const char* message)
{
    // FIXME: We temporarily deal with V8 internal error situations such as out-of-memory by crashing the worker.
    CRASH();
}

static void v8MessageHandler(v8::Handle<v8::Message> message, v8::Handle<v8::Value> data)
{
    static bool isReportingException = false;
    // Exceptions that occur in error handler should be ignored since in that case
    // WorkerContext::reportException will send the exception to the worker object.
    if (isReportingException)
        return;
    isReportingException = true;

    // During the frame teardown, there may not be a valid context.
    if (ScriptExecutionContext* context = getScriptExecutionContext()) {
        String errorMessage = toWebCoreString(message->Get());
        int lineNumber = message->GetLineNumber();
        String sourceURL = toWebCoreString(message->GetScriptResourceName());
        context->reportException(errorMessage, lineNumber, sourceURL, 0);
    }

    isReportingException = false;
}

WorkerContextExecutionProxy::WorkerContextExecutionProxy(WorkerContext* workerContext)
    : m_workerContext(workerContext)
{
    initIsolate();
}

WorkerContextExecutionProxy::~WorkerContextExecutionProxy()
{
    dispose();
}

void WorkerContextExecutionProxy::dispose()
{
    // Detach all events from their JS wrappers.
    for (size_t eventIndex = 0; eventIndex < m_events.size(); ++eventIndex) {
        Event* event = m_events[eventIndex];
        if (forgetV8EventObject(event))
          event->deref();
    }
    m_events.clear();

    m_perContextData.clear();

    // Dispose the context.
    if (!m_context.IsEmpty()) {
        m_context.Dispose();
        m_context.Clear();
    }
}

void WorkerContextExecutionProxy::initIsolate()
{
    // Tell V8 not to call the default OOM handler, binding code will handle it.
    v8::V8::IgnoreOutOfMemoryException();
    v8::V8::SetFatalErrorHandler(reportFatalErrorInV8);

    v8::V8::SetGlobalGCPrologueCallback(&V8GCController::gcPrologue);
    v8::V8::SetGlobalGCEpilogueCallback(&V8GCController::gcEpilogue);

    v8::ResourceConstraints resource_constraints;
    uint32_t here;
    resource_constraints.set_stack_limit(&here - kWorkerMaxStackSize / sizeof(uint32_t*));
    v8::SetResourceConstraints(&resource_constraints);

    V8BindingPerIsolateData::ensureInitialized(v8::Isolate::GetCurrent());
}

bool WorkerContextExecutionProxy::initContextIfNeeded()
{
    // Bail out if the context has already been initialized.
    if (!m_context.IsEmpty())
        return true;

    // Setup the security handlers and message listener. This only has
    // to be done once.
    static bool isV8Initialized = false;
    if (!isV8Initialized)
        v8::V8::AddMessageListener(&v8MessageHandler);

    // Create a new environment
    v8::Persistent<v8::ObjectTemplate> globalTemplate;
    m_context = v8::Context::New(0, globalTemplate);
    if (m_context.IsEmpty())
        return false;

    // Starting from now, use local context only.
    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(m_context);

    v8::Context::Scope scope(context);

    m_perContextData = V8BindingPerContextData::create(m_context);
    if (!m_perContextData->init()) {
        dispose();
        return false;
    }

    // Set DebugId for the new context.
    context->SetData(v8::String::New("worker"));

    // Create a new JS object and use it as the prototype for the shadow global object.
    WrapperTypeInfo* contextType = &V8DedicatedWorkerContext::info;
#if ENABLE(SHARED_WORKERS)
    if (!m_workerContext->isDedicatedWorkerContext())
        contextType = &V8SharedWorkerContext::info;
#endif
    v8::Handle<v8::Function> workerContextConstructor = m_perContextData->constructorForType(contextType);
    v8::Local<v8::Object> jsWorkerContext = SafeAllocation::newInstance(workerContextConstructor);
    // Bail out if allocation failed.
    if (jsWorkerContext.IsEmpty()) {
        dispose();
        return false;
    }

    // Wrap the object.
    V8DOMWrapper::setDOMWrapper(jsWorkerContext, contextType, m_workerContext);

    V8DOMWrapper::setJSWrapperForDOMObject(PassRefPtr<WorkerContext>(m_workerContext), v8::Persistent<v8::Object>::New(jsWorkerContext));

    // Insert the object instance as the prototype of the shadow object.
    v8::Handle<v8::Object> globalObject = v8::Handle<v8::Object>::Cast(m_context->Global()->GetPrototype());
    globalObject->SetPrototype(jsWorkerContext);

    return true;
}

bool WorkerContextExecutionProxy::forgetV8EventObject(Event* event)
{
    if (getDOMObjectMap().contains(event)) {
        getDOMObjectMap().forget(event);
        return true;
    }
    return false;
}

ScriptValue WorkerContextExecutionProxy::evaluate(const String& script, const String& fileName, const TextPosition& scriptStartPosition, WorkerContextExecutionState* state)
{
    v8::HandleScope hs;

    if (!initContextIfNeeded())
        return ScriptValue();

    v8::Context::Scope scope(m_context);

    v8::TryCatch exceptionCatcher;

    v8::Local<v8::String> scriptString = v8ExternalString(script);
    v8::Handle<v8::Script> compiledScript = V8Proxy::compileScript(scriptString, fileName, scriptStartPosition);
    v8::Local<v8::Value> result = runScript(compiledScript);

    if (!exceptionCatcher.CanContinue()) {
        m_workerContext->script()->forbidExecution();
        return ScriptValue();
    }

    if (exceptionCatcher.HasCaught()) {
        v8::Local<v8::Message> message = exceptionCatcher.Message();
        state->hadException = true;
        state->errorMessage = toWebCoreString(message->Get());
        state->lineNumber = message->GetLineNumber();
        state->sourceURL = toWebCoreString(message->GetScriptResourceName());
        if (m_workerContext->sanitizeScriptError(state->errorMessage, state->lineNumber, state->sourceURL))
            state->exception = V8Proxy::throwError(V8Proxy::GeneralError, state->errorMessage.utf8().data());
        else
            state->exception = ScriptValue(exceptionCatcher.Exception());

        exceptionCatcher.Reset();
    } else
        state->hadException = false;

    if (result.IsEmpty() || result->IsUndefined())
        return ScriptValue();

    return ScriptValue(result);
}

v8::Local<v8::Value> WorkerContextExecutionProxy::runScript(v8::Handle<v8::Script> script)
{
    if (script.IsEmpty())
        return v8::Local<v8::Value>();

    // Compute the source string and prevent against infinite recursion.
    if (V8RecursionScope::recursionLevel() >= kMaxRecursionDepth) {
        v8::Local<v8::String> code = v8ExternalString("throw RangeError('Recursion too deep')");
        script = V8Proxy::compileScript(code, "", TextPosition::minimumPosition());
    }

    if (V8Proxy::handleOutOfMemory())
        ASSERT(script.IsEmpty());

    if (script.IsEmpty())
        return v8::Local<v8::Value>();

    // Run the script and keep track of the current recursion depth.
    v8::Local<v8::Value> result;
    {
        V8RecursionScope recursionScope(m_workerContext);
        result = script->Run();
    }

    // Handle V8 internal error situation (Out-of-memory).
    if (result.IsEmpty())
        return v8::Local<v8::Value>();

    return result;
}

void WorkerContextExecutionProxy::trackEvent(Event* event)
{
    m_events.append(event);
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
