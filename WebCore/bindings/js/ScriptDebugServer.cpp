/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScriptDebugServer.h"

#if ENABLE(JAVASCRIPT_DEBUGGER)

#include "DOMWindow.h"
#include "EventLoop.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "JSDOMWindowCustom.h"
#include "JavaScriptCallFrame.h"
#include "Page.h"
#include "PageGroup.h"
#include "PluginView.h"
#include "ScriptBreakpoint.h"
#include "ScriptController.h"
#include "ScriptDebugListener.h"
#include "ScrollView.h"
#include "Widget.h"
#include <debugger/DebuggerCallFrame.h>
#include <parser/SourceCode.h>
#include <runtime/JSLock.h>
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UnusedParam.h>

using namespace JSC;

namespace WebCore {

ScriptDebugServer& ScriptDebugServer::shared()
{
    DEFINE_STATIC_LOCAL(ScriptDebugServer, server, ());
    return server;
}

ScriptDebugServer::ScriptDebugServer()
    : m_callingListeners(false)
    , m_pauseOnExceptionsState(DontPauseOnExceptions)
    , m_pauseOnNextStatement(false)
    , m_paused(false)
    , m_doneProcessingDebuggerEvents(true)
    , m_breakpointsActivated(true)
    , m_pauseOnCallFrame(0)
    , m_recompileTimer(this, &ScriptDebugServer::recompileAllJSFunctions)
{
}

ScriptDebugServer::~ScriptDebugServer()
{
    deleteAllValues(m_pageListenersMap);
}

void ScriptDebugServer::addListener(ScriptDebugListener* listener, Page* page)
{
    ASSERT_ARG(listener, listener);
    ASSERT_ARG(page, page);

    pair<PageListenersMap::iterator, bool> result = m_pageListenersMap.add(page, 0);
    if (result.second)
        result.first->second = new ListenerSet;

    ListenerSet* listeners = result.first->second;
    listeners->add(listener);

    didAddListener(page);
}

void ScriptDebugServer::removeListener(ScriptDebugListener* listener, Page* page)
{
    ASSERT_ARG(listener, listener);
    ASSERT_ARG(page, page);

    PageListenersMap::iterator it = m_pageListenersMap.find(page);
    if (it == m_pageListenersMap.end())
        return;

    ListenerSet* listeners = it->second;
    listeners->remove(listener);
    if (listeners->isEmpty()) {
        m_pageListenersMap.remove(it);
        delete listeners;
    }

    didRemoveListener(page);
    if (!hasListeners())
        didRemoveLastListener();
}

void ScriptDebugServer::pageCreated(Page* page)
{
    ASSERT_ARG(page, page);

    if (!hasListenersInterestedInPage(page))
        return;
    page->setDebugger(this);
}

bool ScriptDebugServer::hasListenersInterestedInPage(Page* page)
{
    ASSERT_ARG(page, page);

    if (hasGlobalListeners())
        return true;

    return m_pageListenersMap.contains(page);
}

void ScriptDebugServer::setBreakpoint(const String& sourceID, unsigned lineNumber, ScriptBreakpoint breakpoint)
{
    intptr_t sourceIDValue = sourceID.toIntPtr();
    BreakpointsMap::iterator it = m_breakpoints.find(sourceIDValue);
    if (it == m_breakpoints.end())
        it = m_breakpoints.set(sourceIDValue, SourceBreakpoints()).first;
    it->second.set(lineNumber, breakpoint);
}

void ScriptDebugServer::removeBreakpoint(const String& sourceID, unsigned lineNumber)
{
    intptr_t sourceIDValue = sourceID.toIntPtr();
    BreakpointsMap::iterator it = m_breakpoints.find(sourceIDValue);
    if (it != m_breakpoints.end())
        it->second.remove(lineNumber);
}

bool ScriptDebugServer::hasBreakpoint(intptr_t sourceID, unsigned lineNumber) const
{
    if (!m_breakpointsActivated)
        return false;

    BreakpointsMap::const_iterator it = m_breakpoints.find(sourceID);
    if (it == m_breakpoints.end())
        return false;
    SourceBreakpoints::const_iterator breakIt = it->second.find(lineNumber);
    if (breakIt == it->second.end() || !breakIt->second.enabled)
        return false;

    // An empty condition counts as no condition which is equivalent to "true".
    if (breakIt->second.condition.isEmpty())
        return true;

    JSValue exception;
    JSValue result = m_currentCallFrame->evaluate(breakIt->second.condition, exception);
    if (exception) {
        // An erroneous condition counts as "false".
        return false;
    }
    return result.toBoolean(m_currentCallFrame->scopeChain()->globalObject->globalExec());
}

void ScriptDebugServer::clearBreakpoints()
{
    m_breakpoints.clear();
}

void ScriptDebugServer::setBreakpointsActivated(bool activated)
{
    m_breakpointsActivated = activated;
}

void ScriptDebugServer::setPauseOnExceptionsState(PauseOnExceptionsState pause)
{
    m_pauseOnExceptionsState = pause;
}

void ScriptDebugServer::pauseProgram()
{
    m_pauseOnNextStatement = true;
}

void ScriptDebugServer::continueProgram()
{
    if (!m_paused)
        return;

    m_pauseOnNextStatement = false;
    m_doneProcessingDebuggerEvents = true;
}

void ScriptDebugServer::stepIntoStatement()
{
    if (!m_paused)
        return;

    m_pauseOnNextStatement = true;
    m_doneProcessingDebuggerEvents = true;
}

void ScriptDebugServer::stepOverStatement()
{
    if (!m_paused)
        return;

    m_pauseOnCallFrame = m_currentCallFrame.get();
    m_doneProcessingDebuggerEvents = true;
}

void ScriptDebugServer::stepOutOfFunction()
{
    if (!m_paused)
        return;

    m_pauseOnCallFrame = m_currentCallFrame ? m_currentCallFrame->caller() : 0;
    m_doneProcessingDebuggerEvents = true;
}

JavaScriptCallFrame* ScriptDebugServer::currentCallFrame()
{
    if (!m_paused)
        return 0;
    return m_currentCallFrame.get();
}

ScriptState* ScriptDebugServer::currentCallFrameState()
{
    if (!m_paused)
        return 0;
    return m_currentCallFrame->scopeChain()->globalObject->globalExec();        
}

void ScriptDebugServer::dispatchDidParseSource(const ListenerSet& listeners, const JSC::SourceCode& source)
{
    String sourceID = JSC::UString(JSC::UString::from(source.provider()->asID()));
    String url = source.provider()->url();
    String data = JSC::UString(source.data(), source.length());
    int firstLine = source.firstLine();

    Vector<ScriptDebugListener*> copy;
    copyToVector(listeners, copy);
    for (size_t i = 0; i < copy.size(); ++i)
        copy[i]->didParseSource(sourceID, url, data, firstLine);
}

void ScriptDebugServer::dispatchFailedToParseSource(const ListenerSet& listeners, const SourceCode& source, int errorLine, const String& errorMessage)
{
    String url = source.provider()->url();
    String data = JSC::UString(source.data(), source.length());
    int firstLine = source.firstLine();

    Vector<ScriptDebugListener*> copy;
    copyToVector(listeners, copy);
    for (size_t i = 0; i < copy.size(); ++i)
        copy[i]->failedToParseSource(url, data, firstLine, errorLine, errorMessage);
}

static Page* toPage(JSGlobalObject* globalObject)
{
    ASSERT_ARG(globalObject, globalObject);

    JSDOMWindow* window = asJSDOMWindow(globalObject);
    Frame* frame = window->impl()->frame();
    return frame ? frame->page() : 0;
}

void ScriptDebugServer::detach(JSGlobalObject* globalObject)
{
    // If we're detaching from the currently executing global object, manually tear down our
    // stack, since we won't get further debugger callbacks to do so. Also, resume execution,
    // since there's no point in staying paused once a window closes.
    if (m_currentCallFrame && m_currentCallFrame->dynamicGlobalObject() == globalObject) {
        m_currentCallFrame = 0;
        m_pauseOnCallFrame = 0;
        continueProgram();
    }
    Debugger::detach(globalObject);
}

void ScriptDebugServer::sourceParsed(ExecState* exec, const SourceCode& source, int errorLine, const UString& errorMessage)
{
    if (m_callingListeners)
        return;

    Page* page = toPage(exec->dynamicGlobalObject());
    if (!page)
        return;

    m_callingListeners = true;

    bool isError = errorLine != -1;

    if (hasGlobalListeners()) {
        if (isError)
            dispatchFailedToParseSource(m_listeners, source, errorLine, errorMessage);
        else
            dispatchDidParseSource(m_listeners, source);
    }

    if (ListenerSet* pageListeners = m_pageListenersMap.get(page)) {
        ASSERT(!pageListeners->isEmpty());
        if (isError)
            dispatchFailedToParseSource(*pageListeners, source, errorLine, errorMessage);
        else
            dispatchDidParseSource(*pageListeners, source);
    }

    m_callingListeners = false;
}

void ScriptDebugServer::dispatchFunctionToListeners(const ListenerSet& listeners, JavaScriptExecutionCallback callback)
{
    Vector<ScriptDebugListener*> copy;
    copyToVector(listeners, copy);
    for (size_t i = 0; i < copy.size(); ++i)
        (copy[i]->*callback)();
}

void ScriptDebugServer::dispatchFunctionToListeners(JavaScriptExecutionCallback callback, Page* page)
{
    if (m_callingListeners)
        return;

    m_callingListeners = true;

    ASSERT(hasListeners());

    dispatchFunctionToListeners(m_listeners, callback);

    if (ListenerSet* pageListeners = m_pageListenersMap.get(page)) {
        ASSERT(!pageListeners->isEmpty());
        dispatchFunctionToListeners(*pageListeners, callback);
    }

    m_callingListeners = false;
}

void ScriptDebugServer::setJavaScriptPaused(const PageGroup& pageGroup, bool paused)
{
    setMainThreadCallbacksPaused(paused);

    const HashSet<Page*>& pages = pageGroup.pages();

    HashSet<Page*>::const_iterator end = pages.end();
    for (HashSet<Page*>::const_iterator it = pages.begin(); it != end; ++it)
        setJavaScriptPaused(*it, paused);
}

void ScriptDebugServer::setJavaScriptPaused(Page* page, bool paused)
{
    ASSERT_ARG(page, page);

    page->setDefersLoading(paused);

    for (Frame* frame = page->mainFrame(); frame; frame = frame->tree()->traverseNext())
        setJavaScriptPaused(frame, paused);
}

void ScriptDebugServer::setJavaScriptPaused(Frame* frame, bool paused)
{
    ASSERT_ARG(frame, frame);

    if (!frame->script()->canExecuteScripts(NotAboutToExecuteScript))
        return;

    frame->script()->setPaused(paused);

    Document* document = frame->document();
    if (paused)
        document->suspendActiveDOMObjects();
    else
        document->resumeActiveDOMObjects();

    setJavaScriptPaused(frame->view(), paused);
}

void ScriptDebugServer::setJavaScriptPaused(FrameView* view, bool paused)
{
    if (!view)
        return;

    const HashSet<RefPtr<Widget> >* children = view->children();
    ASSERT(children);

    HashSet<RefPtr<Widget> >::const_iterator end = children->end();
    for (HashSet<RefPtr<Widget> >::const_iterator it = children->begin(); it != end; ++it) {
        Widget* widget = (*it).get();
        if (!widget->isPluginView())
            continue;
        static_cast<PluginView*>(widget)->setJavaScriptPaused(paused);
    }
}

void ScriptDebugServer::pauseIfNeeded(Page* page)
{
    if (m_paused)
        return;

    if (!page || !hasListenersInterestedInPage(page))
        return;

    bool pauseNow = m_pauseOnNextStatement;
    pauseNow |= (m_pauseOnCallFrame == m_currentCallFrame);
    pauseNow |= (m_currentCallFrame->line() > 0 && hasBreakpoint(m_currentCallFrame->sourceID(), m_currentCallFrame->line()));
    if (!pauseNow)
        return;

    m_pauseOnCallFrame = 0;
    m_pauseOnNextStatement = false;
    m_paused = true;

    dispatchFunctionToListeners(&ScriptDebugListener::didPause, page);

    setJavaScriptPaused(page->group(), true);

    TimerBase::fireTimersInNestedEventLoop();

    EventLoop loop;
    m_doneProcessingDebuggerEvents = false;
    while (!m_doneProcessingDebuggerEvents && !loop.ended())
        loop.cycle();

    setJavaScriptPaused(page->group(), false);

    m_paused = false;

    dispatchFunctionToListeners(&ScriptDebugListener::didContinue, page);
}

void ScriptDebugServer::callEvent(const DebuggerCallFrame& debuggerCallFrame, intptr_t sourceID, int lineNumber)
{
    if (m_paused)
        return;

    m_currentCallFrame = JavaScriptCallFrame::create(debuggerCallFrame, m_currentCallFrame, sourceID, lineNumber);
    pauseIfNeeded(toPage(debuggerCallFrame.dynamicGlobalObject()));
}

void ScriptDebugServer::atStatement(const DebuggerCallFrame& debuggerCallFrame, intptr_t sourceID, int lineNumber)
{
    if (m_paused)
        return;

    ASSERT(m_currentCallFrame);
    if (!m_currentCallFrame)
        return;

    m_currentCallFrame->update(debuggerCallFrame, sourceID, lineNumber);
    pauseIfNeeded(toPage(debuggerCallFrame.dynamicGlobalObject()));
}

void ScriptDebugServer::returnEvent(const DebuggerCallFrame& debuggerCallFrame, intptr_t sourceID, int lineNumber)
{
    if (m_paused)
        return;

    ASSERT(m_currentCallFrame);
    if (!m_currentCallFrame)
        return;

    m_currentCallFrame->update(debuggerCallFrame, sourceID, lineNumber);
    pauseIfNeeded(toPage(debuggerCallFrame.dynamicGlobalObject()));

    // Treat stepping over a return statement like stepping out.
    if (m_currentCallFrame == m_pauseOnCallFrame)
        m_pauseOnCallFrame = m_currentCallFrame->caller();
    m_currentCallFrame = m_currentCallFrame->caller();
}

void ScriptDebugServer::exception(const DebuggerCallFrame& debuggerCallFrame, intptr_t sourceID, int lineNumber, bool hasHandler)
{
    if (m_paused)
        return;

    ASSERT(m_currentCallFrame);
    if (!m_currentCallFrame)
        return;

    if (m_pauseOnExceptionsState == PauseOnAllExceptions || (m_pauseOnExceptionsState == PauseOnUncaughtExceptions && !hasHandler))
        m_pauseOnNextStatement = true;

    m_currentCallFrame->update(debuggerCallFrame, sourceID, lineNumber);
    pauseIfNeeded(toPage(debuggerCallFrame.dynamicGlobalObject()));
}

void ScriptDebugServer::willExecuteProgram(const DebuggerCallFrame& debuggerCallFrame, intptr_t sourceID, int lineNumber)
{
    if (m_paused)
        return;

    m_currentCallFrame = JavaScriptCallFrame::create(debuggerCallFrame, m_currentCallFrame, sourceID, lineNumber);
    pauseIfNeeded(toPage(debuggerCallFrame.dynamicGlobalObject()));
}

void ScriptDebugServer::didExecuteProgram(const DebuggerCallFrame& debuggerCallFrame, intptr_t sourceID, int lineNumber)
{
    if (m_paused)
        return;

    ASSERT(m_currentCallFrame);
    if (!m_currentCallFrame)
        return;

    m_currentCallFrame->update(debuggerCallFrame, sourceID, lineNumber);
    pauseIfNeeded(toPage(debuggerCallFrame.dynamicGlobalObject()));

    // Treat stepping over the end of a program like stepping out.
    if (m_currentCallFrame == m_pauseOnCallFrame)
        m_pauseOnCallFrame = m_currentCallFrame->caller();
    m_currentCallFrame = m_currentCallFrame->caller();
}

void ScriptDebugServer::didReachBreakpoint(const DebuggerCallFrame& debuggerCallFrame, intptr_t sourceID, int lineNumber)
{
    if (m_paused)
        return;

    ASSERT(m_currentCallFrame);
    if (!m_currentCallFrame)
        return;

    m_pauseOnNextStatement = true;
    m_currentCallFrame->update(debuggerCallFrame, sourceID, lineNumber);
    pauseIfNeeded(toPage(debuggerCallFrame.dynamicGlobalObject()));
}

void ScriptDebugServer::recompileAllJSFunctionsSoon()
{
    m_recompileTimer.startOneShot(0);
}

void ScriptDebugServer::recompileAllJSFunctions(Timer<ScriptDebugServer>*)
{
    JSLock lock(SilenceAssertionsOnly);
    Debugger::recompileAllJSFunctions(JSDOMWindow::commonJSGlobalData());
}

void ScriptDebugServer::didAddListener(Page* page)
{
    recompileAllJSFunctionsSoon();

    if (page)
        page->setDebugger(this);
    else
        Page::setDebuggerForAllPages(this);
}

void ScriptDebugServer::didRemoveListener(Page* page)
{
    if (hasGlobalListeners() || (page && hasListenersInterestedInPage(page)))
        return;

    recompileAllJSFunctionsSoon();

    if (page)
        page->setDebugger(0);
    else
        Page::setDebuggerForAllPages(0);
}

void ScriptDebugServer::didRemoveLastListener()
{
    m_doneProcessingDebuggerEvents = true;
}

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)
