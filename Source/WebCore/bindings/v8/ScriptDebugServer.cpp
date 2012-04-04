/*
 * Copyright (c) 2010-2011 Google Inc. All rights reserved.
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
#include "ScriptDebugServer.h"

#if ENABLE(JAVASCRIPT_DEBUGGER)

#include "DebuggerScriptSource.h"
#include "JavaScriptCallFrame.h"
#include "ScriptDebugListener.h"
#include "ScriptObject.h"
#include "V8Binding.h"
#include "V8JavaScriptCallFrame.h"
#include "V8RecursionScope.h"
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

namespace WebCore {

namespace {

class ClientDataImpl : public v8::Debug::ClientData {
public:
    ClientDataImpl(PassOwnPtr<ScriptDebugServer::Task> task) : m_task(task) { }
    virtual ~ClientDataImpl() { }
    ScriptDebugServer::Task* task() const { return m_task.get(); }
private:
    OwnPtr<ScriptDebugServer::Task> m_task;
};

}

v8::Local<v8::Value> ScriptDebugServer::callDebuggerMethod(const char* functionName, int argc, v8::Handle<v8::Value> argv[])
{
    v8::Handle<v8::Function> function = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New(functionName)));
    V8RecursionScope::MicrotaskSuppression scope;
    return function->Call(m_debuggerScript.get(), argc, argv);
}

ScriptDebugServer::ScriptDebugServer()
    : m_pauseOnExceptionsState(DontPauseOnExceptions)
    , m_breakpointsActivated(true)
{
}

String ScriptDebugServer::setBreakpoint(const String& sourceID, const ScriptBreakpoint& scriptBreakpoint, int* actualLineNumber, int* actualColumnNumber)
{
    v8::HandleScope scope;
    v8::Local<v8::Context> debuggerContext = v8::Debug::GetDebugContext();
    v8::Context::Scope contextScope(debuggerContext);

    v8::Local<v8::Object> args = v8::Object::New();
    args->Set(v8::String::New("sourceID"), v8String(sourceID));
    args->Set(v8::String::New("lineNumber"), v8::Integer::New(scriptBreakpoint.lineNumber));
    args->Set(v8::String::New("columnNumber"), v8::Integer::New(scriptBreakpoint.columnNumber));
    args->Set(v8::String::New("condition"), v8String(scriptBreakpoint.condition));

    v8::Handle<v8::Function> setBreakpointFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("setBreakpoint")));
    v8::Handle<v8::Value> breakpointId = v8::Debug::Call(setBreakpointFunction, args);
    if (!breakpointId->IsString())
        return "";
    *actualLineNumber = args->Get(v8::String::New("lineNumber"))->Int32Value();
    *actualColumnNumber = args->Get(v8::String::New("columnNumber"))->Int32Value();
    return v8StringToWebCoreString(breakpointId->ToString());
}

void ScriptDebugServer::removeBreakpoint(const String& breakpointId)
{
    v8::HandleScope scope;
    v8::Local<v8::Context> debuggerContext = v8::Debug::GetDebugContext();
    v8::Context::Scope contextScope(debuggerContext);

    v8::Local<v8::Object> args = v8::Object::New();
    args->Set(v8::String::New("breakpointId"), v8String(breakpointId));

    v8::Handle<v8::Function> removeBreakpointFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("removeBreakpoint")));
    v8::Debug::Call(removeBreakpointFunction, args);
}

void ScriptDebugServer::clearBreakpoints()
{
    ensureDebuggerScriptCompiled();
    v8::HandleScope scope;
    v8::Local<v8::Context> debuggerContext = v8::Debug::GetDebugContext();
    v8::Context::Scope contextScope(debuggerContext);

    v8::Handle<v8::Function> clearBreakpoints = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("clearBreakpoints")));
    v8::Debug::Call(clearBreakpoints);
}

void ScriptDebugServer::setBreakpointsActivated(bool activated)
{
    ensureDebuggerScriptCompiled();
    v8::HandleScope scope;
    v8::Local<v8::Context> debuggerContext = v8::Debug::GetDebugContext();
    v8::Context::Scope contextScope(debuggerContext);

    v8::Local<v8::Object> args = v8::Object::New();
    args->Set(v8::String::New("enabled"), v8::Boolean::New(activated));
    v8::Handle<v8::Function> setBreakpointsActivated = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("setBreakpointsActivated")));
    v8::Debug::Call(setBreakpointsActivated, args);

    m_breakpointsActivated = activated;
}

ScriptDebugServer::PauseOnExceptionsState ScriptDebugServer::pauseOnExceptionsState()
{
    ensureDebuggerScriptCompiled();
    v8::HandleScope scope;
    v8::Context::Scope contextScope(v8::Debug::GetDebugContext());

    v8::Handle<v8::Value> argv[] = { v8::Handle<v8::Value>() };
    v8::Handle<v8::Value> result = callDebuggerMethod("pauseOnExceptionsState", 0, argv);
    return static_cast<ScriptDebugServer::PauseOnExceptionsState>(result->Int32Value());
}

void ScriptDebugServer::setPauseOnExceptionsState(PauseOnExceptionsState pauseOnExceptionsState)
{
    ensureDebuggerScriptCompiled();
    v8::HandleScope scope;
    v8::Context::Scope contextScope(v8::Debug::GetDebugContext());

    v8::Handle<v8::Value> argv[] = { v8::Int32::New(pauseOnExceptionsState) };
    callDebuggerMethod("setPauseOnExceptionsState", 1, argv);
}

void ScriptDebugServer::setPauseOnNextStatement(bool pause)
{
    if (isPaused())
        return;
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (pause)
        v8::Debug::DebugBreak(isolate);
    else
        v8::Debug::CancelDebugBreak(isolate);
}

void ScriptDebugServer::breakProgram()
{
    if (!m_breakpointsActivated)
        return;

    if (!v8::Context::InContext())
        return;

    if (m_breakProgramCallbackTemplate.get().IsEmpty()) {
        m_breakProgramCallbackTemplate.set(v8::FunctionTemplate::New());
        m_breakProgramCallbackTemplate.get()->SetCallHandler(&ScriptDebugServer::breakProgramCallback, v8::External::New(this));
    }

    v8::Handle<v8::Context> context = v8::Context::GetCurrent();
    if (context.IsEmpty())
        return;

    m_pausedContext = *context;
    v8::Handle<v8::Function> breakProgramFunction = m_breakProgramCallbackTemplate.get()->GetFunction();
    v8::Debug::Call(breakProgramFunction);
    m_pausedContext.Clear();
}

void ScriptDebugServer::continueProgram()
{
    if (isPaused())
        quitMessageLoopOnPause();
    m_executionState.clear();
}

void ScriptDebugServer::stepIntoStatement()
{
    ASSERT(isPaused());
    v8::Handle<v8::Value> argv[] = { m_executionState.get() };
    callDebuggerMethod("stepIntoStatement", 1, argv);
    continueProgram();
}

void ScriptDebugServer::stepOverStatement()
{
    ASSERT(isPaused());
    v8::Handle<v8::Value> argv[] = { m_executionState.get() };
    callDebuggerMethod("stepOverStatement", 1, argv);
    continueProgram();
}

void ScriptDebugServer::stepOutOfFunction()
{
    ASSERT(isPaused());
    v8::Handle<v8::Value> argv[] = { m_executionState.get() };
    callDebuggerMethod("stepOutOfFunction", 1, argv);
    continueProgram();
}

bool ScriptDebugServer::canSetScriptSource()
{
    return true;
}

bool ScriptDebugServer::setScriptSource(const String& sourceID, const String& newContent, bool preview, String* error, ScriptValue* newCallFrames, ScriptObject* result)
{
    ensureDebuggerScriptCompiled();
    v8::HandleScope scope;

    OwnPtr<v8::Context::Scope> contextScope;
    if (!isPaused())
        contextScope = adoptPtr(new v8::Context::Scope(v8::Debug::GetDebugContext()));

    v8::Handle<v8::Value> argv[] = { v8String(sourceID), v8String(newContent), v8Boolean(preview) };

    v8::TryCatch tryCatch;
    tryCatch.SetVerbose(false);
    v8::Local<v8::Value> v8result = callDebuggerMethod("setScriptSource", 3, argv);
    if (tryCatch.HasCaught()) {
        v8::Local<v8::Message> message = tryCatch.Message();
        if (!message.IsEmpty())
            *error = toWebCoreStringWithNullOrUndefinedCheck(message->Get());
        else
            *error = "Unknown error.";
        return false;
    }
    ASSERT(!v8result.IsEmpty());
    if (v8result->IsObject())
        *result = ScriptObject(ScriptState::current(), v8result->ToObject());

    // Call stack may have changed after if the edited function was on the stack.
    if (!preview && isPaused())
        *newCallFrames = currentCallFrame();
    return true;
}

ScriptValue ScriptDebugServer::currentCallFrame()
{
    ASSERT(isPaused());
    v8::Handle<v8::Value> argv[] = { m_executionState.get() };
    v8::Handle<v8::Value> currentCallFrameV8 = callDebuggerMethod("currentCallFrame", 1, argv);

    ASSERT(!currentCallFrameV8.IsEmpty());
    if (!currentCallFrameV8->IsObject())
        return ScriptValue(v8::Null());

    RefPtr<JavaScriptCallFrame> currentCallFrame = JavaScriptCallFrame::create(v8::Debug::GetDebugContext(), v8::Handle<v8::Object>::Cast(currentCallFrameV8));
    v8::Context::Scope contextScope(m_pausedContext);
    return ScriptValue(toV8(currentCallFrame.release()));
}

void ScriptDebugServer::interruptAndRun(PassOwnPtr<Task> task, v8::Isolate* isolate)
{
    v8::Debug::DebugBreakForCommand(new ClientDataImpl(task), isolate);
}

void ScriptDebugServer::runPendingTasks()
{
    v8::Debug::ProcessDebugMessages();
}

static ScriptDebugServer* toScriptDebugServer(v8::Handle<v8::Value> data)
{
    void* p = v8::Handle<v8::External>::Cast(data)->Value();
    return static_cast<ScriptDebugServer*>(p);
}

v8::Handle<v8::Value> ScriptDebugServer::breakProgramCallback(const v8::Arguments& args)
{
    ASSERT(2 == args.Length());
    
    ScriptDebugServer* thisPtr = toScriptDebugServer(args.Data());
    v8::Handle<v8::Value> exception;
    thisPtr->breakProgram(v8::Handle<v8::Object>::Cast(args[0]), exception);
    return v8::Undefined();
}

void ScriptDebugServer::breakProgram(v8::Handle<v8::Object> executionState, v8::Handle<v8::Value> exception)
{
    // Don't allow nested breaks.
    if (isPaused())
        return;

    ScriptDebugListener* listener = getDebugListenerForContext(m_pausedContext);
    if (!listener)
        return;

    m_executionState.set(executionState);
    ScriptState* currentCallFrameState = ScriptState::forContext(m_pausedContext);
    listener->didPause(currentCallFrameState, currentCallFrame(), ScriptValue(exception));

    runMessageLoopOnPause(m_pausedContext);
}

void ScriptDebugServer::v8DebugEventCallback(const v8::Debug::EventDetails& eventDetails)
{
    ScriptDebugServer* thisPtr = toScriptDebugServer(eventDetails.GetCallbackData());
    thisPtr->handleV8DebugEvent(eventDetails);
}

void ScriptDebugServer::handleV8DebugEvent(const v8::Debug::EventDetails& eventDetails)
{
    v8::DebugEvent event = eventDetails.GetEvent();

    if (event == v8::BreakForCommand) {
        ClientDataImpl* data = static_cast<ClientDataImpl*>(eventDetails.GetClientData());
        data->task()->run();
        return;
    }

    if (event != v8::Break && event != v8::Exception && event != v8::AfterCompile)
        return;

    v8::Handle<v8::Context> eventContext = eventDetails.GetEventContext();
    ASSERT(!eventContext.IsEmpty());

    ScriptDebugListener* listener = getDebugListenerForContext(eventContext);
    if (listener) {
        v8::HandleScope scope;
        if (event == v8::AfterCompile) {
            v8::Context::Scope contextScope(v8::Debug::GetDebugContext());
            v8::Handle<v8::Function> onAfterCompileFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("getAfterCompileScript")));
            v8::Handle<v8::Value> argv[] = { eventDetails.GetEventData() };
            v8::Handle<v8::Value> value = onAfterCompileFunction->Call(m_debuggerScript.get(), 1, argv);
            ASSERT(value->IsObject());
            v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
            dispatchDidParseSource(listener, object);
        } else if (event == v8::Break || event == v8::Exception) {
            v8::Handle<v8::Value> exception;
            if (event == v8::Exception) {
                v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(1);
                // Stack trace is empty in case of syntax error. Silently continue execution in such cases.
                if (!stackTrace->GetFrameCount())
                    return;
                v8::Handle<v8::Object> eventData = eventDetails.GetEventData();
                v8::Handle<v8::Value> exceptionGetterValue = eventData->Get(v8::String::New("exception"));
                ASSERT(!exceptionGetterValue.IsEmpty() && exceptionGetterValue->IsFunction());
                v8::Handle<v8::Value> argv[] = { v8::Handle<v8::Value>() };
                V8RecursionScope::MicrotaskSuppression scope;
                exception = v8::Handle<v8::Function>::Cast(exceptionGetterValue)->Call(eventData, 0, argv);
            }

            m_pausedContext = *eventContext;
            breakProgram(eventDetails.GetExecutionState(), exception);
            m_pausedContext.Clear();
        }
    }
}

void ScriptDebugServer::dispatchDidParseSource(ScriptDebugListener* listener, v8::Handle<v8::Object> object)
{
    String sourceID = toWebCoreStringWithNullOrUndefinedCheck(object->Get(v8::String::New("id")));

    ScriptDebugListener::Script script;
    script.url = toWebCoreStringWithNullOrUndefinedCheck(object->Get(v8::String::New("name")));
    script.source = toWebCoreStringWithNullOrUndefinedCheck(object->Get(v8::String::New("source")));
    script.sourceMappingURL = toWebCoreStringWithNullOrUndefinedCheck(object->Get(v8::String::New("sourceMappingURL")));
    script.startLine = object->Get(v8::String::New("startLine"))->ToInteger()->Value();
    script.startColumn = object->Get(v8::String::New("startColumn"))->ToInteger()->Value();
    script.endLine = object->Get(v8::String::New("endLine"))->ToInteger()->Value();
    script.endColumn = object->Get(v8::String::New("endColumn"))->ToInteger()->Value();
    script.isContentScript = object->Get(v8::String::New("isContentScript"))->ToBoolean()->Value();

    listener->didParseSource(sourceID, script);
}

void ScriptDebugServer::ensureDebuggerScriptCompiled()
{
    if (m_debuggerScript.get().IsEmpty()) {
        v8::HandleScope scope;
        v8::Local<v8::Context> debuggerContext = v8::Debug::GetDebugContext();
        v8::Context::Scope contextScope(debuggerContext);
        String debuggerScriptSource(reinterpret_cast<const char*>(DebuggerScriptSource_js), sizeof(DebuggerScriptSource_js));
        V8RecursionScope::MicrotaskSuppression recursionScope;
        m_debuggerScript.set(v8::Handle<v8::Object>::Cast(v8::Script::Compile(v8String(debuggerScriptSource))->Run()));
    }
}

bool ScriptDebugServer::isPaused()
{
    return !m_executionState.get().IsEmpty();
}

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)
