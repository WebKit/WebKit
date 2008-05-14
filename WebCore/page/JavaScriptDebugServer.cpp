/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "JavaScriptDebugServer.h"

#include "DOMWindow.h"
#include "EventLoop.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "JSDOMWindow.h"
#include "JavaScriptCallFrame.h"
#include "JavaScriptDebugListener.h"
#include "kjs_proxy.h"
#include "Page.h"
#include "PageGroup.h"
#include "PluginView.h"
#include "ScrollView.h"
#include "Widget.h"
#include <wtf/MainThread.h>

using namespace KJS;

namespace WebCore {

typedef JavaScriptDebugServer::ListenerSet ListenerSet;

JavaScriptDebugServer& JavaScriptDebugServer::shared()
{
    static JavaScriptDebugServer server;
    return server;
}

JavaScriptDebugServer::JavaScriptDebugServer()
    : m_callingListeners(false)
    , m_pauseOnExceptions(false)
    , m_pauseOnNextStatement(false)
    , m_paused(false)
    , m_pauseOnExecState(0)
{
}

JavaScriptDebugServer::~JavaScriptDebugServer()
{
    deleteAllValues(m_pageListenersMap);
    deleteAllValues(m_breakpoints);
}

void JavaScriptDebugServer::addListener(JavaScriptDebugListener* listener)
{
    if (!hasListeners())
        Page::setDebuggerForAllPages(this);

    m_listeners.add(listener);
}

void JavaScriptDebugServer::removeListener(JavaScriptDebugListener* listener)
{
    m_listeners.remove(listener);
    if (!hasListeners()) {
        Page::setDebuggerForAllPages(0);
        resume();
    }
}

void JavaScriptDebugServer::addListener(JavaScriptDebugListener* listener, Page* page)
{
    ASSERT_ARG(page, page);

    if (!hasListeners())
        Page::setDebuggerForAllPages(this);

    pair<PageListenersMap::iterator, bool> result = m_pageListenersMap.add(page, 0);
    if (result.second)
        result.first->second = new ListenerSet;
    ListenerSet* listeners = result.first->second;

    listeners->add(listener);
}

void JavaScriptDebugServer::removeListener(JavaScriptDebugListener* listener, Page* page)
{
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

    if (!hasListeners()) {
        Page::setDebuggerForAllPages(0);
        resume();
    }
}

void JavaScriptDebugServer::pageCreated(Page* page)
{
    if (!hasListeners())
        return;

    page->setDebugger(this);
}

bool JavaScriptDebugServer::hasListenersInterestedInPage(Page* page)
{
    ASSERT_ARG(page, page);

    if (!m_listeners.isEmpty())
        return true;

    return m_pageListenersMap.contains(page);
}

void JavaScriptDebugServer::addBreakpoint(int sourceID, unsigned lineNumber)
{
    HashSet<unsigned>* lines = m_breakpoints.get(sourceID);
    if (!lines) {
        lines = new HashSet<unsigned>;
        m_breakpoints.set(sourceID, lines);
    }

    lines->add(lineNumber);
}

void JavaScriptDebugServer::removeBreakpoint(int sourceID, unsigned lineNumber)
{
    HashSet<unsigned>* lines = m_breakpoints.get(sourceID);
    if (!lines)
        return;

    lines->remove(lineNumber);

    if (!lines->isEmpty())
        return;

    m_breakpoints.remove(sourceID);
    delete lines;
}

bool JavaScriptDebugServer::hasBreakpoint(int sourceID, unsigned lineNumber) const
{
    HashSet<unsigned>* lines = m_breakpoints.get(sourceID);
    if (!lines)
        return false;
    return lines->contains(lineNumber);
}

void JavaScriptDebugServer::clearBreakpoints()
{
    deleteAllValues(m_breakpoints);
    m_breakpoints.clear();
}

void JavaScriptDebugServer::setPauseOnExceptions(bool pause)
{
    m_pauseOnExceptions = pause;
}

void JavaScriptDebugServer::pauseOnNextStatement()
{
    m_pauseOnNextStatement = true;
}

void JavaScriptDebugServer::resume()
{
    m_paused = false;
}

void JavaScriptDebugServer::stepIntoStatement()
{
    if (!m_paused)
        return;

    resume();

    m_pauseOnNextStatement = true;
}

void JavaScriptDebugServer::stepOverStatement()
{
    if (!m_paused)
        return;

    resume();

    if (m_currentCallFrame)
        m_pauseOnExecState = m_currentCallFrame->execState();
    else
        m_pauseOnExecState = 0;
}

void JavaScriptDebugServer::stepOutOfFunction()
{
    if (!m_paused)
        return;

    resume();

    if (m_currentCallFrame && m_currentCallFrame->caller())
        m_pauseOnExecState = m_currentCallFrame->caller()->execState();
    else
        m_pauseOnExecState = 0;
}

JavaScriptCallFrame* JavaScriptDebugServer::currentCallFrame()
{
    if (!m_paused)
        return 0;
    return m_currentCallFrame.get();
}

static void dispatchDidParseSource(const ListenerSet& listeners, const UString& source, int startingLineNumber, const UString& sourceURL, int sourceID)
{
    Vector<JavaScriptDebugListener*> copy;
    copyToVector(listeners, copy);
    for (size_t i = 0; i < copy.size(); ++i)
        copy[i]->didParseSource(source, startingLineNumber, sourceURL, sourceID);
}

static void dispatchFailedToParseSource(const ListenerSet& listeners, const UString& source, int startingLineNumber, const UString& sourceURL, int errorLine, const UString& errorMessage)
{
    Vector<JavaScriptDebugListener*> copy;
    copyToVector(listeners, copy);
    for (size_t i = 0; i < copy.size(); ++i)
        copy[i]->failedToParseSource(source, startingLineNumber, sourceURL, errorLine, errorMessage);
}

static Page* toPage(ExecState* exec)
{
    ASSERT_ARG(exec, exec);

    JSDOMWindow* window = asJSDOMWindow(exec->dynamicGlobalObject());
    ASSERT(window);

    return window->impl()->frame()->page();
}

bool JavaScriptDebugServer::sourceParsed(ExecState* exec, int sourceID, const UString& sourceURL, const UString& source, int startingLineNumber, int errorLine, const UString& errorMessage)
{
    if (m_callingListeners)
        return true;

    Page* page = toPage(exec);
    if (!page)
        return true;

    m_callingListeners = true;

    ASSERT(hasListeners());

    bool isError = errorLine != -1;

    if (!m_listeners.isEmpty()) {
        if (isError)
            dispatchFailedToParseSource(m_listeners, source, startingLineNumber, sourceURL, errorLine, errorMessage);
        else
            dispatchDidParseSource(m_listeners, source, startingLineNumber, sourceURL, sourceID);
    }

    if (ListenerSet* pageListeners = m_pageListenersMap.get(page)) {
        ASSERT(!pageListeners->isEmpty());
        if (isError)
            dispatchFailedToParseSource(*pageListeners, source, startingLineNumber, sourceURL, errorLine, errorMessage);
        else
            dispatchDidParseSource(*pageListeners, source, startingLineNumber, sourceURL, sourceID);
    }

    m_callingListeners = false;
    return true;
}

static void dispatchFunctionToListeners(const ListenerSet& listeners, JavaScriptDebugServer::JavaScriptExecutionCallback callback)
{
    Vector<JavaScriptDebugListener*> copy;
    copyToVector(listeners, copy);
    for (size_t i = 0; i < copy.size(); ++i)
        (copy[i]->*callback)();
}

void JavaScriptDebugServer::dispatchFunctionToListeners(JavaScriptExecutionCallback callback, ExecState* exec)
{
    if (m_callingListeners)
        return;

    Page* page = toPage(exec);
    if (!page)
        return;

    m_callingListeners = true;

    ASSERT(hasListeners());

    WebCore::dispatchFunctionToListeners(m_listeners, callback);
    if (ListenerSet* pageListeners = m_pageListenersMap.get(page)) {
        ASSERT(!pageListeners->isEmpty());
        WebCore::dispatchFunctionToListeners(*pageListeners, callback);
    }

    m_callingListeners = false;
}

void JavaScriptDebugServer::setJavaScriptPaused(const PageGroup& pageGroup, bool paused)
{
    setMainThreadCallbacksPaused(paused);

    const HashSet<Page*>& pages = pageGroup.pages();

    HashSet<Page*>::const_iterator end = pages.end();
    for (HashSet<Page*>::const_iterator it = pages.begin(); it != end; ++it)
        setJavaScriptPaused(*it, false);
}

void JavaScriptDebugServer::setJavaScriptPaused(Page* page, bool paused)
{
    ASSERT_ARG(page, page);

    page->setDefersLoading(paused);

    for (Frame* frame = page->mainFrame(); frame; frame = frame->tree()->traverseNext())
        setJavaScriptPaused(frame, paused);
}

void JavaScriptDebugServer::setJavaScriptPaused(Frame* frame, bool paused)
{
    ASSERT_ARG(frame, frame);

    if (!frame->scriptProxy()->isEnabled())
        return;

    frame->scriptProxy()->setPaused(paused);

    if (JSDOMWindow* window = toJSDOMWindow(frame)) {
        if (paused)
            m_pausedTimeouts.set(frame, window->pauseTimeouts());
        else
            window->resumeTimeouts(m_pausedTimeouts.take(frame));
    }

    setJavaScriptPaused(frame->view(), paused);
}

void JavaScriptDebugServer::setJavaScriptPaused(FrameView* view, bool paused)
{
#if !PLATFORM(MAC)
    if (!view)
        return;

    HashSet<Widget*>* children = static_cast<ScrollView*>(view)->children();
    ASSERT(children);

    HashSet<Widget*>::iterator end = children->end();
    for (HashSet<Widget*>::iterator it = children->begin(); it != end; ++it) {
        Widget* widget = *it;
        if (!widget->isPluginView())
            continue;
        static_cast<PluginView*>(widget)->setJavaScriptPaused(paused);
    }
#endif
}

void JavaScriptDebugServer::pauseIfNeeded(ExecState* exec, int sourceID, int lineNumber)
{
    if (m_paused)
        return;

    Page* page = toPage(exec);
    if (!page || !hasListenersInterestedInPage(page))
        return;

    bool pauseNow = m_pauseOnNextStatement;
    if (!pauseNow && m_pauseOnExecState)
        pauseNow = (m_pauseOnExecState == exec);
    if (!pauseNow && lineNumber > 0)
        pauseNow = hasBreakpoint(sourceID, lineNumber);
    if (!pauseNow)
        return;

    m_pauseOnExecState = 0;
    m_pauseOnNextStatement = false;
    m_paused = true;

    dispatchFunctionToListeners(&JavaScriptDebugListener::didPause, exec);

    setJavaScriptPaused(page->group(), true);

    EventLoop loop;
    while (m_paused && !loop.ended())
        loop.cycle();

    setJavaScriptPaused(page->group(), false);

    m_paused = false;
}

bool JavaScriptDebugServer::callEvent(ExecState* exec, int sourceID, int lineNumber, JSObject*, const List&)
{
    if (m_paused)
        return true;

    if (m_currentCallFrame && m_currentCallFrame->execState() != exec->callingExecState()) {
        m_currentCallFrame->invalidate();
        m_currentCallFrame = 0;
    }

    m_currentCallFrame = JavaScriptCallFrame::create(exec, m_currentCallFrame, sourceID, lineNumber);
    pauseIfNeeded(exec, sourceID, lineNumber);
    return true;
}

bool JavaScriptDebugServer::atStatement(ExecState* exec, int sourceID, int firstLine, int)
{
    if (m_paused)
        return true;
    if (m_currentCallFrame)
        m_currentCallFrame->setLine(firstLine);
    else
        m_currentCallFrame = JavaScriptCallFrame::create(exec, 0, sourceID, firstLine);
    pauseIfNeeded(exec, sourceID, firstLine);
    return true;
}

bool JavaScriptDebugServer::returnEvent(ExecState* exec, int sourceID, int lineNumber, JSObject*)
{
    if (m_paused)
        return true;
    m_currentCallFrame->invalidate();
    m_currentCallFrame = m_currentCallFrame->caller();
    pauseIfNeeded(exec, sourceID, lineNumber);
    return true;
}

bool JavaScriptDebugServer::exception(ExecState* exec, int sourceID, int lineNumber, JSValue*)
{
    if (m_paused)
        return true;
    if (m_currentCallFrame)
        m_currentCallFrame->setLine(lineNumber);
    else
        m_currentCallFrame = JavaScriptCallFrame::create(exec, 0, sourceID, lineNumber);
    if (m_pauseOnExceptions)
        m_pauseOnNextStatement = true;
    pauseIfNeeded(exec, sourceID, lineNumber);
    return true;
}

} // namespace WebCore
