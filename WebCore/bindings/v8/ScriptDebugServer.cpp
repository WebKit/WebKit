/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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

#include "Frame.h"
#include "JavaScriptCallFrame.h"
#include "Page.h"
#include "ScriptDebugListener.h"
#include "V8Binding.h"
#include "V8DOMWindow.h"
#include "V8Proxy.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

#if ENABLE(V8_SCRIPT_DEBUG_SERVER)
static Frame* retrieveFrame(v8::Handle<v8::Context> context)
{
    if (context.IsEmpty())
        return 0;

    // Test that context has associated global dom window object.
    v8::Handle<v8::Object> global = context->Global();
    if (global.IsEmpty())
        return 0;

    global = V8DOMWrapper::lookupDOMWrapper(V8DOMWindow::GetTemplate(), global);
    if (global.IsEmpty())
        return 0;

    return V8Proxy::retrieveFrame(context);
}
#endif

ScriptDebugServer& ScriptDebugServer::shared()
{
    DEFINE_STATIC_LOCAL(ScriptDebugServer, server, ());
    return server;
}

ScriptDebugServer::ScriptDebugServer()
    : m_pauseOnExceptionsState(DontPauseOnExceptions)
    , m_pausedPage(0)
{
}

void ScriptDebugServer::setDebuggerScriptSource(const String& scriptSource)
{
    m_debuggerScriptSource = scriptSource;
}

void ScriptDebugServer::addListener(ScriptDebugListener* listener, Page* page)
{
#if ENABLE(V8_SCRIPT_DEBUG_SERVER)
    v8::HandleScope scope;
    v8::Local<v8::Context> debuggerContext = v8::Debug::GetDebugContext();
    v8::Context::Scope contextScope(debuggerContext);

    if (!m_listenersMap.size()) {
        ensureDebuggerScriptCompiled();
        ASSERT(!m_debuggerScript.get()->IsUndefined());
        v8::Debug::SetDebugEventListener2(&ScriptDebugServer::v8DebugEventCallback);
    }
    m_listenersMap.set(page, listener);
    V8Proxy* proxy = V8Proxy::retrieve(page->mainFrame());
    v8::Local<v8::Context> context = proxy->mainWorldContext();

    v8::Handle<v8::Function> getScriptsFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("getScripts")));
    v8::Handle<v8::Value> argv[] = { context->GetData() };
    v8::Handle<v8::Value> value = getScriptsFunction->Call(m_debuggerScript.get(), 1, argv);
    if (value.IsEmpty())
        return;
    ASSERT(!value->IsUndefined() && value->IsArray());
    v8::Handle<v8::Array> scriptsArray = v8::Handle<v8::Array>::Cast(value);
    for (unsigned i = 0; i < scriptsArray->Length(); ++i)
        dispatchDidParseSource(listener, v8::Handle<v8::Object>::Cast(scriptsArray->Get(v8::Integer::New(i))));
#endif
}

void ScriptDebugServer::removeListener(ScriptDebugListener* listener, Page* page)
{
    if (!m_listenersMap.contains(page))
        return;

    if (m_pausedPage == page)
        continueProgram();

    m_listenersMap.remove(page);

    if (m_listenersMap.isEmpty())
        v8::Debug::SetDebugEventListener(0);
    // FIXME: Remove all breakpoints set by the agent.
}

void ScriptDebugServer::setBreakpoint(const String& sourceID, unsigned lineNumber, ScriptBreakpoint breakpoint)
{
#if ENABLE(V8_SCRIPT_DEBUG_SERVER)
    v8::HandleScope scope;
    v8::Local<v8::Context> debuggerContext = v8::Debug::GetDebugContext();
    v8::Context::Scope contextScope(debuggerContext);

    v8::Local<v8::Object> args = v8::Object::New();
    args->Set(v8::String::New("scriptId"), v8String(sourceID));
    args->Set(v8::String::New("lineNumber"), v8::Integer::New(lineNumber));
    args->Set(v8::String::New("condition"), v8String(breakpoint.condition));
    args->Set(v8::String::New("enabled"), v8::Boolean::New(breakpoint.enabled));

    v8::Handle<v8::Function> setBreakpointFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("setBreakpoint")));
    v8::Debug::Call(setBreakpointFunction, args);
#endif
}

void ScriptDebugServer::removeBreakpoint(const String& sourceID, unsigned lineNumber)
{
#if ENABLE(V8_SCRIPT_DEBUG_SERVER)
    v8::HandleScope scope;
    v8::Local<v8::Context> debuggerContext = v8::Debug::GetDebugContext();
    v8::Context::Scope contextScope(debuggerContext);

    v8::Local<v8::Object> args = v8::Object::New();
    args->Set(v8::String::New("scriptId"), v8String(sourceID));
    args->Set(v8::String::New("lineNumber"), v8::Integer::New(lineNumber));

    v8::Handle<v8::Function> removeBreakpointFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("removeBreakpoint")));
    v8::Debug::Call(removeBreakpointFunction, args);
#endif
}

void ScriptDebugServer::clearBreakpoints()
{
#if ENABLE(V8_SCRIPT_DEBUG_SERVER)
    ensureDebuggerScriptCompiled();
    v8::HandleScope scope;
    v8::Local<v8::Context> debuggerContext = v8::Debug::GetDebugContext();
    v8::Context::Scope contextScope(debuggerContext);

    v8::Handle<v8::Function> clearBreakpoints = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("clearBreakpoints")));
    v8::Debug::Call(clearBreakpoints);
#endif
}

void ScriptDebugServer::setBreakpointsActivated(bool enabled)
{
#if ENABLE(V8_SCRIPT_DEBUG_SERVER)
    ensureDebuggerScriptCompiled();
    v8::HandleScope scope;
    v8::Local<v8::Context> debuggerContext = v8::Debug::GetDebugContext();
    v8::Context::Scope contextScope(debuggerContext);

    v8::Local<v8::Object> args = v8::Object::New();
    args->Set(v8::String::New("enabled"), v8::Boolean::New(enabled));
    v8::Handle<v8::Function> setBreakpointsActivated = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("setBreakpointsActivated")));
    v8::Debug::Call(setBreakpointsActivated, args);
#endif
}

ScriptDebugServer::PauseOnExceptionsState ScriptDebugServer::pauseOnExceptionsState()
{
#if ENABLE(V8_SCRIPT_DEBUG_SERVER)
    ensureDebuggerScriptCompiled();
    v8::HandleScope scope;
    v8::Context::Scope contextScope(v8::Debug::GetDebugContext());

    v8::Handle<v8::Function> function = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("pauseOnExceptionsState")));
    v8::Handle<v8::Value> argv[] = { v8::Handle<v8::Value>() };
    v8::Handle<v8::Value> result = function->Call(m_debuggerScript.get(), 0, argv);
    return static_cast<ScriptDebugServer::PauseOnExceptionsState>(result->Int32Value());
#else
    return DontPauseOnExceptions;
#endif
}

void ScriptDebugServer::setPauseOnExceptionsState(PauseOnExceptionsState pauseOnExceptionsState)
{
#if ENABLE(V8_SCRIPT_DEBUG_SERVER)
    ensureDebuggerScriptCompiled();
    v8::HandleScope scope;
    v8::Context::Scope contextScope(v8::Debug::GetDebugContext());

    v8::Handle<v8::Function> setPauseOnExceptionsFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("setPauseOnExceptionsState")));
    v8::Handle<v8::Value> argv[] = { v8::Int32::New(pauseOnExceptionsState) };
    setPauseOnExceptionsFunction->Call(m_debuggerScript.get(), 1, argv);
#endif
}

void ScriptDebugServer::continueProgram()
{
#if ENABLE(V8_SCRIPT_DEBUG_SERVER)
    if (m_pausedPage)
        m_clientMessageLoop->quitNow();
    didResume();
#endif
}

void ScriptDebugServer::stepIntoStatement()
{
#if ENABLE(V8_SCRIPT_DEBUG_SERVER)
    ASSERT(m_pausedPage);
    v8::Handle<v8::Function> function = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("stepIntoStatement")));
    v8::Handle<v8::Value> argv[] = { m_executionState.get() };
    function->Call(m_debuggerScript.get(), 1, argv);
    continueProgram();
#endif
}

void ScriptDebugServer::stepOverStatement()
{
#if ENABLE(V8_SCRIPT_DEBUG_SERVER)
    ASSERT(m_pausedPage);
    v8::Handle<v8::Function> function = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("stepOverStatement")));
    v8::Handle<v8::Value> argv[] = { m_executionState.get() };
    function->Call(m_debuggerScript.get(), 1, argv);
    continueProgram();
#endif
}

void ScriptDebugServer::stepOutOfFunction()
{
#if ENABLE(V8_SCRIPT_DEBUG_SERVER)
    ASSERT(m_pausedPage);
    v8::Handle<v8::Function> function = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("stepOutOfFunction")));
    v8::Handle<v8::Value> argv[] = { m_executionState.get() };
    function->Call(m_debuggerScript.get(), 1, argv);
    continueProgram();
#endif
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

#if ENABLE(V8_SCRIPT_DEBUG_SERVER)
void ScriptDebugServer::v8DebugEventCallback(const v8::Debug::EventDetails& eventDetails)
{
    ScriptDebugServer::shared().handleV8DebugEvent(eventDetails);
}

void ScriptDebugServer::handleV8DebugEvent(const v8::Debug::EventDetails& eventDetails)
{
    v8::DebugEvent event = eventDetails.GetEvent();
    if (event != v8::Break && event != v8::Exception && event != v8::AfterCompile)
        return;

    v8::Handle<v8::Context> eventContext = eventDetails.GetEventContext();
    ASSERT(!eventContext.IsEmpty());

    Frame* frame = retrieveFrame(eventContext);
    if (frame) {
        ScriptDebugListener* listener = m_listenersMap.get(frame->page());
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
                m_executionState.set(eventDetails.GetExecutionState());
                m_pausedPage = frame->page();
                ScriptState* currentCallFrameState = mainWorldScriptState(frame);
                listener->didPause(currentCallFrameState);

                // Wait for continue or step command.
                m_clientMessageLoop->run(m_pausedPage);
                ASSERT(!m_pausedPage);
            }
        }
    }
}
#endif

void ScriptDebugServer::dispatchDidParseSource(ScriptDebugListener* listener, v8::Handle<v8::Object> object)
{
    listener->didParseSource(
        toWebCoreStringWithNullCheck(object->Get(v8::String::New("id"))),
        toWebCoreStringWithNullCheck(object->Get(v8::String::New("name"))),
        toWebCoreStringWithNullCheck(object->Get(v8::String::New("source"))),
        object->Get(v8::String::New("lineOffset"))->ToInteger()->Value());
}

void ScriptDebugServer::ensureDebuggerScriptCompiled()
{
    if (m_debuggerScript.get().IsEmpty()) {
        v8::HandleScope scope;
        v8::Local<v8::Context> debuggerContext = v8::Debug::GetDebugContext();
        v8::Context::Scope contextScope(debuggerContext);
        m_debuggerScript.set(v8::Handle<v8::Object>::Cast(v8::Script::Compile(v8String(m_debuggerScriptSource))->Run()));
    }
}

void ScriptDebugServer::didResume()
{
    m_currentCallFrame.clear();
    m_executionState.clear();
    m_pausedPage = 0;
}

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)
