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
#include "Frame.h"
#include "JSDOMWindow.h"
#include "JavaScriptDebugListener.h"
#include "Page.h"

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
{
}

JavaScriptDebugServer::~JavaScriptDebugServer()
{
    deleteAllValues(m_pageListenersMap);
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
    if (!hasListeners())
        Page::setDebuggerForAllPages(0);
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

    if (!hasListeners())
        Page::setDebuggerForAllPages(0);
}

void JavaScriptDebugServer::pageCreated(Page* page)
{
    if (!hasListeners())
        return;

    page->setDebugger(this);
}

static void dispatchDidParseSource(const ListenerSet& listeners, ExecState* exec, const UString& source, int startingLineNumber, const UString& sourceURL, int sourceID)
{
    Vector<JavaScriptDebugListener*> copy;
    copyToVector(listeners, copy);
    for (size_t i = 0; i < copy.size(); ++i)
        copy[i]->didParseSource(exec, source, startingLineNumber, sourceURL, sourceID);
}

static void dispatchFailedToParseSource(const ListenerSet& listeners, ExecState* exec, const UString& source, int startingLineNumber, const UString& sourceURL, int errorLine, const UString& errorMessage)
{
    Vector<JavaScriptDebugListener*> copy;
    copyToVector(listeners, copy);
    for (size_t i = 0; i < copy.size(); ++i)
        copy[i]->failedToParseSource(exec, source, startingLineNumber, sourceURL, errorLine, errorMessage);
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
            dispatchFailedToParseSource(m_listeners, exec, source, startingLineNumber, sourceURL, errorLine, errorMessage);
        else
            dispatchDidParseSource(m_listeners, exec, source, startingLineNumber, sourceURL, sourceID);
    }

    if (ListenerSet* pageListeners = m_pageListenersMap.get(page)) {
        ASSERT(!pageListeners->isEmpty());
        if (isError)
            dispatchFailedToParseSource(*pageListeners, exec, source, startingLineNumber, sourceURL, errorLine, errorMessage);
        else
            dispatchDidParseSource(*pageListeners, exec, source, startingLineNumber, sourceURL, sourceID);
    }

    m_callingListeners = false;
    return true;
}

static void dispatchFunctionToListeners(const ListenerSet& listeners, JavaScriptDebugServer::JavaScriptExecutionCallback callback, ExecState* exec, int sourceID, int lineNumber)
{
    Vector<JavaScriptDebugListener*> copy;
    copyToVector(listeners, copy);
    for (size_t i = 0; i < copy.size(); ++i)
        (copy[i]->*callback)(exec, sourceID, lineNumber);
}

void JavaScriptDebugServer::dispatchFunctionToListeners(JavaScriptExecutionCallback callback, ExecState* exec, int sourceID, int lineNumber)
{
    if (m_callingListeners)
        return;

    Page* page = toPage(exec);
    if (!page)
        return;

    m_callingListeners = true;

    ASSERT(hasListeners());

    WebCore::dispatchFunctionToListeners(m_listeners, callback, exec, sourceID, lineNumber);
    if (ListenerSet* pageListeners = m_pageListenersMap.get(page)) {
        ASSERT(!pageListeners->isEmpty());
        WebCore::dispatchFunctionToListeners(*pageListeners, callback, exec, sourceID, lineNumber);
    }

    m_callingListeners = false;
}

bool JavaScriptDebugServer::callEvent(ExecState* exec, int sourceID, int lineNumber, JSObject*, const List&)
{
    dispatchFunctionToListeners(&JavaScriptDebugListener::didEnterCallFrame, exec, sourceID, lineNumber);
    return true;
}

bool JavaScriptDebugServer::atStatement(ExecState* exec, int sourceID, int firstLine, int)
{
    dispatchFunctionToListeners(&JavaScriptDebugListener::willExecuteStatement, exec, sourceID, firstLine);
    return true;
}

bool JavaScriptDebugServer::returnEvent(ExecState* exec, int sourceID, int lineNumber, JSObject*)
{
    dispatchFunctionToListeners(&JavaScriptDebugListener::willLeaveCallFrame, exec, sourceID, lineNumber);
    return true;
}

bool JavaScriptDebugServer::exception(ExecState* exec, int sourceID, int lineNumber, JSValue*)
{
    dispatchFunctionToListeners(&JavaScriptDebugListener::exceptionWasRaised, exec, sourceID, lineNumber);
    return true;
}

} // namespace WebCore
