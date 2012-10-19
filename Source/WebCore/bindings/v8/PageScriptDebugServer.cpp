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
#include "PageScriptDebugServer.h"

#if ENABLE(JAVASCRIPT_DEBUGGER)

#include "Frame.h"
#include "InspectorInstrumentation.h"
#include "Page.h"
#include "ScriptDebugListener.h"
#include "V8Binding.h"
#include "V8DOMWindow.h"
#include "V8RecursionScope.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

static Frame* retrieveFrameWithGlobalObjectCheck(v8::Handle<v8::Context> context)
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

    return toFrameIfNotDetached(context);
}

PageScriptDebugServer& PageScriptDebugServer::shared()
{
    DEFINE_STATIC_LOCAL(PageScriptDebugServer, server, ());
    return server;
}

PageScriptDebugServer::PageScriptDebugServer()
    : ScriptDebugServer()
    , m_pausedPage(0)
{
}

void PageScriptDebugServer::addListener(ScriptDebugListener* listener, Page* page)
{
    ScriptController* scriptController = page->mainFrame()->script();
    if (!scriptController->canExecuteScripts(NotAboutToExecuteScript))
        return;

    v8::HandleScope scope;
    v8::Local<v8::Context> debuggerContext = v8::Debug::GetDebugContext();
    v8::Context::Scope contextScope(debuggerContext);

    if (!m_listenersMap.size()) {
        ensureDebuggerScriptCompiled();
        ASSERT(!m_debuggerScript.get()->IsUndefined());
        v8::Debug::SetDebugEventListener2(&PageScriptDebugServer::v8DebugEventCallback, v8::External::New(this));
    }
    m_listenersMap.set(page, listener);

    V8DOMWindowShell* shell = scriptController->windowShell();
    if (!shell->isContextInitialized())
        return;
    v8::Handle<v8::Context> context = shell->context();
    v8::Handle<v8::Function> getScriptsFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.get()->Get(v8::String::New("getScripts")));
    v8::Handle<v8::Value> argv[] = { context->GetData() };
    v8::Handle<v8::Value> value;
    {
        V8RecursionScope::MicrotaskSuppression scope;
        value = getScriptsFunction->Call(m_debuggerScript.get(), 1, argv);
    }
    if (value.IsEmpty())
        return;
    ASSERT(!value->IsUndefined() && value->IsArray());
    v8::Handle<v8::Array> scriptsArray = v8::Handle<v8::Array>::Cast(value);
    for (unsigned i = 0; i < scriptsArray->Length(); ++i)
        dispatchDidParseSource(listener, v8::Handle<v8::Object>::Cast(scriptsArray->Get(v8Integer(i))));
}

void PageScriptDebugServer::removeListener(ScriptDebugListener* listener, Page* page)
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

void PageScriptDebugServer::setClientMessageLoop(PassOwnPtr<ClientMessageLoop> clientMessageLoop)
{
    m_clientMessageLoop = clientMessageLoop;
}

void PageScriptDebugServer::compileScript(ScriptState* state, const String& expression, const String& sourceURL, String* scriptId, String* exceptionMessage)
{
    ScriptExecutionContext* scriptExecutionContext = state->scriptExecutionContext();
    RefPtr<Frame> protect = static_cast<Document*>(scriptExecutionContext)->frame();
    ScriptDebugServer::compileScript(state, expression, sourceURL, scriptId, exceptionMessage);
    if (!scriptId->isNull())
        m_compiledScriptURLs.set(*scriptId, sourceURL);
}

void PageScriptDebugServer::clearCompiledScripts()
{
    ScriptDebugServer::clearCompiledScripts();
    m_compiledScriptURLs.clear();
}

void PageScriptDebugServer::runScript(ScriptState* state, const String& scriptId, ScriptValue* result, bool* wasThrown, String* exceptionMessage)
{
    String sourceURL = m_compiledScriptURLs.take(scriptId);

    ScriptExecutionContext* scriptExecutionContext = state->scriptExecutionContext();
    Frame* frame = static_cast<Document*>(scriptExecutionContext)->frame();
    InspectorInstrumentationCookie cookie;
    if (frame)
        cookie = InspectorInstrumentation::willEvaluateScript(frame, sourceURL, TextPosition::minimumPosition().m_line.oneBasedInt());

    RefPtr<Frame> protect = frame;
    ScriptDebugServer::runScript(state, scriptId, result, wasThrown, exceptionMessage);

    if (frame)
        InspectorInstrumentation::didEvaluateScript(cookie);
}

ScriptDebugListener* PageScriptDebugServer::getDebugListenerForContext(v8::Handle<v8::Context> context)
{
    v8::HandleScope scope;
    Frame* frame = retrieveFrameWithGlobalObjectCheck(context);
    if (!frame)
        return 0;
    return m_listenersMap.get(frame->page());
}

void PageScriptDebugServer::runMessageLoopOnPause(v8::Handle<v8::Context> context)
{
    v8::HandleScope scope;
    Frame* frame = retrieveFrameWithGlobalObjectCheck(context);
    m_pausedPage = frame->page();

    // Wait for continue or step command.
    m_clientMessageLoop->run(m_pausedPage);

    // The listener may have been removed in the nested loop.
    if (ScriptDebugListener* listener = m_listenersMap.get(m_pausedPage))
        listener->didContinue();
     
    m_pausedPage = 0;
}

void PageScriptDebugServer::quitMessageLoopOnPause()
{
    m_clientMessageLoop->quitNow();
}

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)
