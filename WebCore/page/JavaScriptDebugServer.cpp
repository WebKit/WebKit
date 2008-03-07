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
}

void JavaScriptDebugServer::addListener(JavaScriptDebugListener* listener)
{
    if (m_listeners.isEmpty())
        Page::setDebuggerForAllPages(this);

    m_listeners.add(listener);
}

void JavaScriptDebugServer::removeListener(JavaScriptDebugListener* listener)
{
    m_listeners.remove(listener);
    if (m_listeners.isEmpty())
        Page::setDebuggerForAllPages(0);
}

void JavaScriptDebugServer::pageCreated(Page* page)
{
    if (m_listeners.isEmpty())
        return;

    page->setDebugger(this);
}

static void dispatchDidParseSource(const ListenerSet& listeners, ExecState* state, const String& source, int startingLineNumber, const String& sourceURL, int sourceID)
{
    Vector<JavaScriptDebugListener*> copy;
    copyToVector(listeners, copy);
    for (size_t i = 0; i < copy.size(); ++i)
        copy[i]->didParseSource(state, source, startingLineNumber, sourceURL, sourceID);
}

static void dispatchFailedToParseSource(const ListenerSet& listeners, ExecState* state, const String& source, int startingLineNumber, const String& sourceURL, int errorLine, const String& errorMessage)
{
    Vector<JavaScriptDebugListener*> copy;
    copyToVector(listeners, copy);
    for (size_t i = 0; i < copy.size(); ++i)
        copy[i]->failedToParseSource(state, source, startingLineNumber, sourceURL, errorLine, errorMessage);
}

bool JavaScriptDebugServer::sourceParsed(ExecState* state, int sourceID, const UString& sourceURL, const UString& source, int startingLineNumber, int errorLine, const UString& errorMessage)
{
    if (m_callingListeners)
        return true;
    m_callingListeners = true;

    ASSERT(!m_listeners.isEmpty());
    if (errorLine == -1)
        dispatchDidParseSource(m_listeners, state, source, startingLineNumber, sourceURL, sourceID);
    else
        dispatchFailedToParseSource(m_listeners, state, source, startingLineNumber, sourceURL, errorLine, errorMessage);

    m_callingListeners = false;
    return true;
}

void JavaScriptDebugServer::dispatchFunctionToListeners(JavaScriptExecutionCallback callback, ExecState* state, int sourceID, int lineNumber)
{
    if (m_callingListeners)
        return;
    m_callingListeners = true;

    ASSERT(!m_listeners.isEmpty());
    Vector<JavaScriptDebugListener*> copy;
    copyToVector(m_listeners, copy);
    for (size_t i = 0; i < copy.size(); ++i)
        (copy[i]->*callback)(state, sourceID, lineNumber);

    m_callingListeners = false;
}

bool JavaScriptDebugServer::callEvent(ExecState* state, int sourceID, int lineNumber, JSObject*, const List&)
{
    dispatchFunctionToListeners(&JavaScriptDebugListener::didEnterCallFrame, state, sourceID, lineNumber);
    return true;
}

bool JavaScriptDebugServer::atStatement(ExecState* state, int sourceID, int firstLine, int)
{
    dispatchFunctionToListeners(&JavaScriptDebugListener::willExecuteStatement, state, sourceID, firstLine);
    return true;
}

bool JavaScriptDebugServer::returnEvent(ExecState* state, int sourceID, int lineNumber, JSObject*)
{
    dispatchFunctionToListeners(&JavaScriptDebugListener::willLeaveCallFrame, state, sourceID, lineNumber);
    return true;
}

bool JavaScriptDebugServer::exception(ExecState* state, int sourceID, int lineNumber, JSValue*)
{
    dispatchFunctionToListeners(&JavaScriptDebugListener::exceptionWasRaised, state, sourceID, lineNumber);
    return true;
}

} // namespace WebCore
