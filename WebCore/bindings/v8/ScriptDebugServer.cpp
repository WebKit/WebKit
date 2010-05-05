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

ScriptDebugServer::MessageLoopDispatchHandler ScriptDebugServer::s_messageLoopDispatchHandler = 0;

ScriptDebugServer& ScriptDebugServer::shared()
{
    DEFINE_STATIC_LOCAL(ScriptDebugServer, server, ());
    return server;
}

ScriptDebugServer::ScriptDebugServer()
    : m_pauseOnExceptionsState(DontPauseOnExceptions)
    , m_currentCallFrameState(0)
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
        v8::Debug::SetMessageHandler2(&ScriptDebugServer::onV8DebugMessage);
        v8::Debug::SetHostDispatchHandler(&ScriptDebugServer::onV8DebugHostDispatch, 100 /* ms */);
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

    m_listenersMap.remove(page);

    if (m_listenersMap.isEmpty()) {
        v8::Debug::SetMessageHandler2(0);
        v8::Debug::SetHostDispatchHandler(0);
    }
    // FIXME: Remove all breakpoints set by the agent.
    // FIXME: Force continue if detach happened in nessted message loop while
    // debugger was paused on a breakpoint(as long as there are other
    // attached agents v8 will wait for explicit'continue' message).
    // FIXME: send continue command to v8 if necessary;
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

    v8::Handle<v8::Function> setBreakpointsActivated = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("clearBreakpoints")));
    v8::Debug::Call(setBreakpointsActivated);
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

    v8::Handle<v8::Function> currentCallFrameFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("pauseOnExceptionsState")));
    v8::Handle<v8::Value> argv[] = { v8::Handle<v8::Value>() };
    v8::Handle<v8::Value> result = currentCallFrameFunction->Call(m_debuggerScript.get(), 0, argv);
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

    v8::Handle<v8::Function> currentCallFrameFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("setPauseOnExceptionsState")));
    v8::Handle<v8::Value> argv[] = { v8::Int32::New(pauseOnExceptionsState) };
    currentCallFrameFunction->Call(m_debuggerScript.get(), 1, argv);
#endif
}

void ScriptDebugServer::continueProgram()
{
#if ENABLE(V8_SCRIPT_DEBUG_SERVER)
    String cmd("{\"seq\":1,\"type\":\"request\",\"command\":\"continue\"}");
    v8::Debug::SendCommand(reinterpret_cast<const uint16_t*>(cmd.characters()), cmd.length(), new v8::Debug::ClientData());
    didResume();
#endif
}

void ScriptDebugServer::stepIntoStatement()
{
#if ENABLE(V8_SCRIPT_DEBUG_SERVER)
    String cmd("{\"seq\":1,\"type\":\"request\",\"command\":\"continue\",\"arguments\":{\"stepaction\":\"in\"}}");
    v8::Debug::SendCommand(reinterpret_cast<const uint16_t*>(cmd.characters()), cmd.length(), new v8::Debug::ClientData());
    didResume();
#endif
}

void ScriptDebugServer::stepOverStatement()
{
#if ENABLE(V8_SCRIPT_DEBUG_SERVER)
    String cmd("{\"seq\":1,\"type\":\"request\",\"command\":\"continue\",\"arguments\":{\"stepaction\":\"next\"}}");
    v8::Debug::SendCommand(reinterpret_cast<const uint16_t*>(cmd.characters()), cmd.length(), new v8::Debug::ClientData());
    didResume();
#endif
}

void ScriptDebugServer::stepOutOfFunction()
{
#if ENABLE(V8_SCRIPT_DEBUG_SERVER)
    String cmd("{\"seq\":1,\"type\":\"request\",\"command\":\"continue\",\"arguments\":{\"stepaction\":\"out\"}}");
    v8::Debug::SendCommand(reinterpret_cast<const uint16_t*>(cmd.characters()), cmd.length(), new v8::Debug::ClientData());
    didResume();
#endif
}

ScriptState* ScriptDebugServer::currentCallFrameState()
{
    return m_currentCallFrameState;
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

void ScriptDebugServer::onV8DebugMessage(const v8::Debug::Message& message)
{
    ScriptDebugServer::shared().handleV8DebugMessage(message);
}

void ScriptDebugServer::onV8DebugHostDispatch()
{
    ScriptDebugServer::shared().handleV8DebugHostDispatch();
}

void ScriptDebugServer::handleV8DebugHostDispatch()
{
    if (!s_messageLoopDispatchHandler)
        return;

    Vector<WebCore::Page*> pages;
    for (ListenersMap::iterator it = m_listenersMap.begin(); it != m_listenersMap.end(); ++it)
        pages.append(it->first);
    
    s_messageLoopDispatchHandler(pages);
}

void ScriptDebugServer::handleV8DebugMessage(const v8::Debug::Message& message)
{
    v8::HandleScope scope;

    if (!message.IsEvent())
        return;

    // Ignore unsupported event types.
    if (message.GetEvent() != v8::AfterCompile && message.GetEvent() != v8::Break && message.GetEvent() != v8::Exception)
        return;

    v8::Handle<v8::Context> context = message.GetEventContext();
    // If the context is from one of the inpected tabs it should have its context
    // data. Skip events from unknown contexts.
    if (context.IsEmpty())
        return;

    // Test that context has associated global dom window object.
    v8::Handle<v8::Object> global = context->Global();
    if (global.IsEmpty())
        return;

    global = V8DOMWrapper::lookupDOMWrapper(V8DOMWindow::GetTemplate(), global);
    if (global.IsEmpty())
        return;

    bool handled = false;
    Frame* frame = V8Proxy::retrieveFrame(context);
    if (frame) {
        ScriptDebugListener* listener = m_listenersMap.get(frame->page());
        if (listener) {
            if (message.GetEvent() == v8::AfterCompile) {
                handled = true;
                v8::Context::Scope contextScope(v8::Debug::GetDebugContext());
                v8::Local<v8::Object> args = v8::Object::New();
                args->Set(v8::String::New("eventData"), message.GetEventData());
                v8::Handle<v8::Function> onAfterCompileFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("getAfterCompileScript")));
                v8::Handle<v8::Value> argv[] = { message.GetExecutionState(), args };
                v8::Handle<v8::Value> value = onAfterCompileFunction->Call(m_debuggerScript.get(), 2, argv);
                ASSERT(value->IsObject());
                v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
                dispatchDidParseSource(listener, object);
            } else if (message.GetEvent() == v8::Break || message.GetEvent() == v8::Exception) {
                handled = true;
                m_executionState.set(message.GetExecutionState());
                m_currentCallFrameState = mainWorldScriptState(frame);
                listener->didPause();
                m_currentCallFrameState = 0;
            }
        }
    }

    if (!handled && !message.WillStartRunning())
        continueProgram();
}

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
}

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)
