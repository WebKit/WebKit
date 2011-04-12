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
#include "V8Binding.h"
#include <wtf/StdLibExtras.h>

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

    v8::Handle<v8::Function> function = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("pauseOnExceptionsState")));
    v8::Handle<v8::Value> argv[] = { v8::Handle<v8::Value>() };
    v8::Handle<v8::Value> result = function->Call(m_debuggerScript.get(), 0, argv);
    return static_cast<ScriptDebugServer::PauseOnExceptionsState>(result->Int32Value());
}

void ScriptDebugServer::setPauseOnExceptionsState(PauseOnExceptionsState pauseOnExceptionsState)
{
    ensureDebuggerScriptCompiled();
    v8::HandleScope scope;
    v8::Context::Scope contextScope(v8::Debug::GetDebugContext());

    v8::Handle<v8::Function> setPauseOnExceptionsFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("setPauseOnExceptionsState")));
    v8::Handle<v8::Value> argv[] = { v8::Int32::New(pauseOnExceptionsState) };
    setPauseOnExceptionsFunction->Call(m_debuggerScript.get(), 1, argv);
}

void ScriptDebugServer::setPauseOnNextStatement(bool pause)
{
    if (isPaused())
        return;
    if (pause)
        v8::Debug::DebugBreak();
    else
        v8::Debug::CancelDebugBreak();
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

    m_pausedPageContext = *context;
    v8::Handle<v8::Function> breakProgramFunction = m_breakProgramCallbackTemplate.get()->GetFunction();
    v8::Debug::Call(breakProgramFunction);
    m_pausedPageContext.Clear();
}

void ScriptDebugServer::continueProgram()
{
    if (isPaused())
        quitMessageLoopOnPause();
    m_currentCallFrame.clear();
    m_executionState.clear();
}

void ScriptDebugServer::stepIntoStatement()
{
    ASSERT(isPaused());
    v8::Handle<v8::Function> function = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("stepIntoStatement")));
    v8::Handle<v8::Value> argv[] = { m_executionState.get() };
    function->Call(m_debuggerScript.get(), 1, argv);
    continueProgram();
}

void ScriptDebugServer::stepOverStatement()
{
    ASSERT(isPaused());
    v8::Handle<v8::Function> function = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("stepOverStatement")));
    v8::Handle<v8::Value> argv[] = { m_executionState.get() };
    function->Call(m_debuggerScript.get(), 1, argv);
    continueProgram();
}

void ScriptDebugServer::stepOutOfFunction()
{
    ASSERT(isPaused());
    v8::Handle<v8::Function> function = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("stepOutOfFunction")));
    v8::Handle<v8::Value> argv[] = { m_executionState.get() };
    function->Call(m_debuggerScript.get(), 1, argv);
    continueProgram();
}

bool ScriptDebugServer::editScriptSource(const String& sourceID, const String& newContent, String* error)
{
    ensureDebuggerScriptCompiled();
    v8::HandleScope scope;

    OwnPtr<v8::Context::Scope> contextScope;
    if (!isPaused())
        contextScope.set(new v8::Context::Scope(v8::Debug::GetDebugContext()));

    v8::Handle<v8::Function> function = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("editScriptSource")));
    v8::Handle<v8::Value> argv[] = { v8String(sourceID), v8String(newContent) };

    v8::TryCatch tryCatch;
    tryCatch.SetVerbose(false);
    v8::Handle<v8::Value> result = function->Call(m_debuggerScript.get(), 2, argv);
    if (tryCatch.HasCaught()) {
        v8::Local<v8::Message> message = tryCatch.Message();
        if (!message.IsEmpty())
            *error = toWebCoreStringWithNullOrUndefinedCheck(message->Get());
        else
            *error = "Unknown error.";
        return false;
    }
    ASSERT(!result.IsEmpty());

    // Call stack may have changed after if the edited function was on the stack.
    if (m_currentCallFrame)
        m_currentCallFrame.clear();
    return true;
}

PassRefPtr<JavaScriptCallFrame> ScriptDebugServer::currentCallFrame()
{
    if (!m_currentCallFrame) {
        v8::Handle<v8::Function> currentCallFrameFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("currentCallFrame")));
        v8::Handle<v8::Value> argv[] = { m_executionState.get() };
        v8::Handle<v8::Value> currentCallFrameV8 = currentCallFrameFunction->Call(m_debuggerScript.get(), 1, argv);
        m_currentCallFrame = JavaScriptCallFrame::create(v8::Debug::GetDebugContext(), v8::Handle<v8::Object>::Cast(currentCallFrameV8));
    }
    return m_currentCallFrame;
}

void ScriptDebugServer::interruptAndRun(PassOwnPtr<Task> task)
{
    v8::Debug::DebugBreakForCommand(new ClientDataImpl(task));
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
    thisPtr->breakProgram(v8::Handle<v8::Object>::Cast(args[0]));
    return v8::Undefined();
}

void ScriptDebugServer::breakProgram(v8::Handle<v8::Object> executionState)
{
    // Don't allow nested breaks.
    if (isPaused())
        return;

    ScriptDebugListener* listener = getDebugListenerForContext(m_pausedPageContext);
    if (!listener)
        return;

    m_executionState.set(executionState);
    ScriptState* currentCallFrameState = ScriptState::forContext(m_pausedPageContext);
    listener->didPause(currentCallFrameState);

    runMessageLoopOnPause(m_pausedPageContext);
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
            if (event == v8::Exception) {
                v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(1);
                // Stack trace is empty in case of syntax error. Silently continue execution in such cases.
                if (!stackTrace->GetFrameCount())
                    return;
            }

            m_pausedPageContext = *eventContext;
            breakProgram(eventDetails.GetExecutionState());
            m_pausedPageContext.Clear();
        }
    }
}

void ScriptDebugServer::dispatchDidParseSource(ScriptDebugListener* listener, v8::Handle<v8::Object> object)
{
    listener->didParseSource(
        toWebCoreStringWithNullOrUndefinedCheck(object->Get(v8::String::New("id"))),
        toWebCoreStringWithNullOrUndefinedCheck(object->Get(v8::String::New("name"))),
        toWebCoreStringWithNullOrUndefinedCheck(object->Get(v8::String::New("source"))),
        object->Get(v8::String::New("lineOffset"))->ToInteger()->Value(),
        object->Get(v8::String::New("columnOffset"))->ToInteger()->Value(),
        object->Get(v8::String::New("isContentScript"))->ToBoolean()->Value());
}

void ScriptDebugServer::ensureDebuggerScriptCompiled()
{
    if (m_debuggerScript.get().IsEmpty()) {
        v8::HandleScope scope;
        v8::Local<v8::Context> debuggerContext = v8::Debug::GetDebugContext();
        v8::Context::Scope contextScope(debuggerContext);
        String debuggerScriptSource(reinterpret_cast<const char*>(DebuggerScriptSource_js), sizeof(DebuggerScriptSource_js));
        m_debuggerScript.set(v8::Handle<v8::Object>::Cast(v8::Script::Compile(v8String(debuggerScriptSource))->Run()));
    }
}

bool ScriptDebugServer::isPaused()
{
    return !m_executionState.get().IsEmpty();
}

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)
