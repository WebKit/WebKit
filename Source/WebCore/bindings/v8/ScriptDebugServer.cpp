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
#include "ScopedPersistent.h"
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

class RecursionScopeSuppression {
public:
    RecursionScopeSuppression()
    {
#ifndef NDEBUG
        V8PerIsolateData::current()->incrementInternalScriptRecursionLevel();
#endif
    }

    ~RecursionScopeSuppression()
    {
#ifndef NDEBUG
        V8PerIsolateData::current()->decrementInternalScriptRecursionLevel();
#endif
    }
};

}

v8::Local<v8::Value> ScriptDebugServer::callDebuggerMethod(const char* functionName, int argc, v8::Handle<v8::Value> argv[])
{
    v8::Handle<v8::Function> function = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::NewSymbol(functionName)));
    V8RecursionScope::MicrotaskSuppression scope;
    return function->Call(m_debuggerScript.get(), argc, argv);
}

class ScriptDebugServer::ScriptPreprocessor {
    WTF_MAKE_NONCOPYABLE(ScriptPreprocessor);
public:
    explicit ScriptPreprocessor(const String& preprocessorScript)
    {
        v8::HandleScope scope;

        m_utilityContext.set(v8::Context::New());
        if (m_utilityContext.isEmpty())
            return;

        v8::Context::Scope contextScope(m_utilityContext.get());

        v8::TryCatch tryCatch;

        String wrappedScript = makeString("(", preprocessorScript, ")");
        v8::Handle<v8::String> preprocessor = v8::String::New(wrappedScript.utf8().data(), wrappedScript.utf8().length());
        v8::Handle<v8::Script> script = v8::Script::Compile(preprocessor);

        if (tryCatch.HasCaught())
            return;
        RecursionScopeSuppression suppressionScope;
        v8::Handle<v8::Value> preprocessorFunction = script->Run();

        if (tryCatch.HasCaught() || !preprocessorFunction->IsFunction())
            return;

        m_preprocessorFunction.set(v8::Handle<v8::Function>::Cast(preprocessorFunction));
    }

    String preprocessSourceCode(const String& sourceCode)
    {
        v8::HandleScope scope;

        if (m_preprocessorFunction.isEmpty())
            return sourceCode;

        v8::Local<v8::Context> context = v8::Local<v8::Context>::New(m_utilityContext.get());
        v8::Context::Scope contextScope(context);

        v8::Handle<v8::String> sourceCodeString = v8::String::New(sourceCode.utf8().data(), sourceCode.utf8().length());
        v8::Handle<v8::Value> argv[] = { sourceCodeString };

        v8::TryCatch tryCatch;
        RecursionScopeSuppression suppressionScope;
        v8::Handle<v8::Value> resultValue = m_preprocessorFunction->Call(context->Global(), 1, argv);
        if (tryCatch.HasCaught())
            return sourceCode;

        if (resultValue->IsString()) {
            v8::String::Utf8Value utf8Value(resultValue);
            return String::fromUTF8(*utf8Value, utf8Value.length());
        }

        return sourceCode;
    }

    ~ScriptPreprocessor()
    {
    }

private:
    ScopedPersistent<v8::Context> m_utilityContext;
    String m_preprocessorBody;
    ScopedPersistent<v8::Function> m_preprocessorFunction;
};

ScriptDebugServer::ScriptDebugServer()
    : m_pauseOnExceptionsState(DontPauseOnExceptions)
    , m_breakpointsActivated(true)
{
}

ScriptDebugServer::~ScriptDebugServer()
{
}

String ScriptDebugServer::setBreakpoint(const String& sourceID, const ScriptBreakpoint& scriptBreakpoint, int* actualLineNumber, int* actualColumnNumber)
{
    v8::HandleScope scope;
    v8::Local<v8::Context> debuggerContext = v8::Debug::GetDebugContext();
    v8::Context::Scope contextScope(debuggerContext);

    v8::Local<v8::Object> args = v8::Object::New();
    args->Set(v8::String::NewSymbol("sourceID"), deprecatedV8String(sourceID));
    args->Set(v8::String::NewSymbol("lineNumber"), deprecatedV8Integer(scriptBreakpoint.lineNumber));
    args->Set(v8::String::NewSymbol("columnNumber"), deprecatedV8Integer(scriptBreakpoint.columnNumber));
    args->Set(v8::String::NewSymbol("condition"), deprecatedV8String(scriptBreakpoint.condition));

    v8::Handle<v8::Function> setBreakpointFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::NewSymbol("setBreakpoint")));
    v8::Handle<v8::Value> breakpointId = v8::Debug::Call(setBreakpointFunction, args);
    if (!breakpointId->IsString())
        return "";
    *actualLineNumber = args->Get(v8::String::NewSymbol("lineNumber"))->Int32Value();
    *actualColumnNumber = args->Get(v8::String::NewSymbol("columnNumber"))->Int32Value();
    return toWebCoreString(breakpointId->ToString());
}

void ScriptDebugServer::removeBreakpoint(const String& breakpointId)
{
    v8::HandleScope scope;
    v8::Local<v8::Context> debuggerContext = v8::Debug::GetDebugContext();
    v8::Context::Scope contextScope(debuggerContext);

    v8::Local<v8::Object> args = v8::Object::New();
    args->Set(v8::String::NewSymbol("breakpointId"), deprecatedV8String(breakpointId));

    v8::Handle<v8::Function> removeBreakpointFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::NewSymbol("removeBreakpoint")));
    v8::Debug::Call(removeBreakpointFunction, args);
}

void ScriptDebugServer::clearBreakpoints()
{
    ensureDebuggerScriptCompiled();
    v8::HandleScope scope;
    v8::Local<v8::Context> debuggerContext = v8::Debug::GetDebugContext();
    v8::Context::Scope contextScope(debuggerContext);

    v8::Handle<v8::Function> clearBreakpoints = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::NewSymbol("clearBreakpoints")));
    v8::Debug::Call(clearBreakpoints);
}

void ScriptDebugServer::setBreakpointsActivated(bool activated)
{
    ensureDebuggerScriptCompiled();
    v8::HandleScope scope;
    v8::Local<v8::Context> debuggerContext = v8::Debug::GetDebugContext();
    v8::Context::Scope contextScope(debuggerContext);

    v8::Local<v8::Object> args = v8::Object::New();
    args->Set(v8::String::NewSymbol("enabled"), v8::Boolean::New(activated));
    v8::Handle<v8::Function> setBreakpointsActivated = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::NewSymbol("setBreakpointsActivated")));
    v8::Debug::Call(setBreakpointsActivated, args);

    m_breakpointsActivated = activated;
}

ScriptDebugServer::PauseOnExceptionsState ScriptDebugServer::pauseOnExceptionsState()
{
    ensureDebuggerScriptCompiled();
    v8::HandleScope scope;
    v8::Context::Scope contextScope(v8::Debug::GetDebugContext());

    v8::Handle<v8::Value> argv[] = { v8Undefined() };
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
    class EnableLiveEditScope {
    public:
        EnableLiveEditScope() { v8::Debug::SetLiveEditEnabled(true); }
        ~EnableLiveEditScope() { v8::Debug::SetLiveEditEnabled(false); }
    };

    ensureDebuggerScriptCompiled();
    v8::HandleScope scope;

    OwnPtr<v8::Context::Scope> contextScope;
    if (!isPaused())
        contextScope = adoptPtr(new v8::Context::Scope(v8::Debug::GetDebugContext()));

    v8::Handle<v8::Value> argv[] = { deprecatedV8String(sourceID), deprecatedV8String(newContent), v8Boolean(preview) };

    v8::Local<v8::Value> v8result;
    {
        EnableLiveEditScope enableLiveEditScope;
        v8::TryCatch tryCatch;
        tryCatch.SetVerbose(false);
        v8result = callDebuggerMethod("liveEditScriptSource", 3, argv);
        if (tryCatch.HasCaught()) {
            v8::Local<v8::Message> message = tryCatch.Message();
            if (!message.IsEmpty())
                *error = toWebCoreStringWithUndefinedOrNullCheck(message->Get());
            else
                *error = "Unknown error.";
            return false;
        }
    }
    ASSERT(!v8result.IsEmpty());
    if (v8result->IsObject())
        *result = ScriptObject(ScriptState::current(), v8result->ToObject());

    // Call stack may have changed after if the edited function was on the stack.
    if (!preview && isPaused())
        *newCallFrames = currentCallFrame();
    return true;
}


void ScriptDebugServer::updateCallStack(ScriptValue* callFrame)
{
    if (isPaused())
        *callFrame = currentCallFrame();
}


void ScriptDebugServer::setScriptPreprocessor(const String& preprocessorBody)
{
    m_scriptPreprocessor.clear();
    if (!preprocessorBody.isEmpty())
        m_scriptPreprocessor = adoptPtr(new ScriptPreprocessor(preprocessorBody));
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

    if (event != v8::Break && event != v8::Exception && event != v8::AfterCompile && event != v8::BeforeCompile)
        return;

    v8::Handle<v8::Context> eventContext = eventDetails.GetEventContext();
    ASSERT(!eventContext.IsEmpty());

    ScriptDebugListener* listener = getDebugListenerForContext(eventContext);
    if (listener) {
        v8::HandleScope scope;
        if (event == v8::BeforeCompile) {
            if (!m_scriptPreprocessor)
                return;

            OwnPtr<ScriptPreprocessor> preprocessor(m_scriptPreprocessor.release());
            v8::Context::Scope contextScope(v8::Debug::GetDebugContext());
            v8::Handle<v8::Function> getScriptSourceFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("getScriptSource")));
            v8::Handle<v8::Value> argv[] = { eventDetails.GetEventData() };
            v8::Handle<v8::Value> script = getScriptSourceFunction->Call(m_debuggerScript.get(), 1, argv);

            v8::Handle<v8::Function> setScriptSourceFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("setScriptSource")));
            String patchedScript = preprocessor->preprocessSourceCode(toWebCoreStringWithUndefinedOrNullCheck(script));
            v8::Handle<v8::Value> argv2[] = { eventDetails.GetEventData(), v8String(patchedScript) };
            setScriptSourceFunction->Call(m_debuggerScript.get(), 2, argv2);
            m_scriptPreprocessor = preprocessor.release();
        } else if (event == v8::AfterCompile) {
            v8::Context::Scope contextScope(v8::Debug::GetDebugContext());
            v8::Handle<v8::Function> onAfterCompileFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::NewSymbol("getAfterCompileScript")));
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
                v8::Handle<v8::Value> exceptionGetterValue = eventData->Get(v8::String::NewSymbol("exception"));
                ASSERT(!exceptionGetterValue.IsEmpty() && exceptionGetterValue->IsFunction());
                v8::Handle<v8::Value> argv[] = { v8Undefined() };
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
    String sourceID = toWebCoreStringWithUndefinedOrNullCheck(object->Get(v8::String::NewSymbol("id")));

    ScriptDebugListener::Script script;
    script.url = toWebCoreStringWithUndefinedOrNullCheck(object->Get(v8::String::NewSymbol("name")));
    script.source = toWebCoreStringWithUndefinedOrNullCheck(object->Get(v8::String::NewSymbol("source")));
    script.sourceMappingURL = toWebCoreStringWithUndefinedOrNullCheck(object->Get(v8::String::NewSymbol("sourceMappingURL")));
    script.startLine = object->Get(v8::String::NewSymbol("startLine"))->ToInteger()->Value();
    script.startColumn = object->Get(v8::String::NewSymbol("startColumn"))->ToInteger()->Value();
    script.endLine = object->Get(v8::String::NewSymbol("endLine"))->ToInteger()->Value();
    script.endColumn = object->Get(v8::String::NewSymbol("endColumn"))->ToInteger()->Value();
    script.isContentScript = object->Get(v8::String::NewSymbol("isContentScript"))->ToBoolean()->Value();

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
        m_debuggerScript.set(v8::Handle<v8::Object>::Cast(v8::Script::Compile(deprecatedV8String(debuggerScriptSource))->Run()));
    }
}

v8::Local<v8::Value> ScriptDebugServer::functionScopes(v8::Handle<v8::Function> function)
{
    ensureDebuggerScriptCompiled();

    v8::Handle<v8::Value> argv[] = { function };
    return callDebuggerMethod("getFunctionScopes", 1, argv);
}

v8::Local<v8::Value> ScriptDebugServer::getInternalProperties(v8::Handle<v8::Object>& object)
{
    if (m_debuggerScript.get().IsEmpty())
        return *v8::Undefined();

    v8::Handle<v8::Value> argv[] = { object };
    return callDebuggerMethod("getInternalProperties", 1, argv);
}


bool ScriptDebugServer::isPaused()
{
    return !m_executionState.get().IsEmpty();
}

void ScriptDebugServer::compileScript(ScriptState* state, const String& expression, const String& sourceURL, String* scriptId, String* exceptionMessage)
{
    v8::HandleScope handleScope;
    v8::Handle<v8::Context> context = state->context();
    if (context.IsEmpty())
        return;
    v8::Context::Scope contextScope(context);

    v8::Handle<v8::String> code = deprecatedV8String(expression);
    v8::TryCatch tryCatch;

    v8::ScriptOrigin origin(deprecatedV8String(sourceURL), deprecatedV8Integer(0), deprecatedV8Integer(0));
    v8::Handle<v8::Script> script = v8::Script::New(code, &origin);

    if (tryCatch.HasCaught()) {
        v8::Local<v8::Message> message = tryCatch.Message();
        if (!message.IsEmpty())
            *exceptionMessage = toWebCoreStringWithUndefinedOrNullCheck(message->Get());
        return;
    }
    if (script.IsEmpty())
        return;

    *scriptId = toWebCoreStringWithUndefinedOrNullCheck(script->Id());
    m_compiledScripts.set(*scriptId, adoptPtr(new ScopedPersistent<v8::Script>(script)));
}

void ScriptDebugServer::clearCompiledScripts()
{
    m_compiledScripts.clear();
}

void ScriptDebugServer::runScript(ScriptState* state, const String& scriptId, ScriptValue* result, bool* wasThrown, String* exceptionMessage)
{
    if (!m_compiledScripts.contains(scriptId))
        return;
    v8::HandleScope handleScope;
    ScopedPersistent<v8::Script>* scriptHandle = m_compiledScripts.get(scriptId);
    v8::Local<v8::Script> script = v8::Local<v8::Script>::New(scriptHandle->get());
    m_compiledScripts.remove(scriptId);
    if (script.IsEmpty())
        return;

    v8::Handle<v8::Context> context = state->context();
    if (context.IsEmpty())
        return;
    v8::Context::Scope contextScope(context);

    v8::Local<v8::Value> value;
    v8::TryCatch tryCatch;
    {
        V8RecursionScope recursionScope(state->scriptExecutionContext());
        value = script->Run();
    }

    *wasThrown = false;
    if (tryCatch.HasCaught()) {
        *wasThrown = true;
        *result = ScriptValue(tryCatch.Exception());
        v8::Local<v8::Message> message = tryCatch.Message();
        if (!message.IsEmpty())
            *exceptionMessage = toWebCoreStringWithUndefinedOrNullCheck(message->Get());
    } else
        *result = ScriptValue(value);
}

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)
