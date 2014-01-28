/*
 * Copyright (C) 2008, 2009, 2013, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2010-2011 Google Inc. All rights reserved.
 * Copyright (C) 2013 University of Washington. All rights reserved.
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

#include "DebuggerCallFrame.h"
#include "JSJavaScriptCallFrame.h"
#include "JSLock.h"
#include "JavaScriptCallFrame.h"
#include "ScriptValue.h"
#include "SourceProvider.h"
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TemporaryChange.h>
#include <wtf/text/WTFString.h>

using namespace JSC;
using namespace Inspector;

namespace Inspector {

ScriptDebugServer::ScriptDebugServer(bool isInWorkerThread)
    : Debugger(isInWorkerThread)
    , m_doneProcessingDebuggerEvents(true)
    , m_callingListeners(false)
{
}

ScriptDebugServer::~ScriptDebugServer()
{
}

JSC::BreakpointID ScriptDebugServer::setBreakpoint(JSC::SourceID sourceID, const ScriptBreakpoint& scriptBreakpoint, unsigned* actualLineNumber, unsigned* actualColumnNumber)
{
    if (!sourceID)
        return JSC::noBreakpointID;

    JSC::Breakpoint breakpoint(sourceID, scriptBreakpoint.lineNumber, scriptBreakpoint.columnNumber, scriptBreakpoint.condition, scriptBreakpoint.autoContinue);
    JSC::BreakpointID id = Debugger::setBreakpoint(breakpoint, *actualLineNumber, *actualColumnNumber);
    if (id != JSC::noBreakpointID && !scriptBreakpoint.actions.isEmpty()) {
#ifndef NDEBUG
        BreakpointIDToActionsMap::iterator it = m_breakpointIDToActions.find(id);
        ASSERT(it == m_breakpointIDToActions.end());
#endif
        const Vector<ScriptBreakpointAction> &actions = scriptBreakpoint.actions;
        m_breakpointIDToActions.set(id, actions);
    }
    return id;
}

void ScriptDebugServer::removeBreakpoint(JSC::BreakpointID id)
{
    ASSERT(id != JSC::noBreakpointID);
    BreakpointIDToActionsMap::iterator it = m_breakpointIDToActions.find(id);
    if (it != m_breakpointIDToActions.end())
        m_breakpointIDToActions.remove(it);

    Debugger::removeBreakpoint(id);
}

bool ScriptDebugServer::evaluateBreakpointAction(const ScriptBreakpointAction& breakpointAction)
{
    DebuggerCallFrame* debuggerCallFrame = currentDebuggerCallFrame();

    switch (breakpointAction.type) {
    case ScriptBreakpointActionTypeLog: {
        dispatchBreakpointActionLog(debuggerCallFrame->exec(), breakpointAction.data);
        break;
    }
    case ScriptBreakpointActionTypeEvaluate: {
        JSValue exception;
        debuggerCallFrame->evaluate(breakpointAction.data, exception);
        if (exception)
            reportException(debuggerCallFrame->exec(), exception);
        break;
    }
    case ScriptBreakpointActionTypeSound:
        dispatchBreakpointActionSound(debuggerCallFrame->exec());
        break;
    case ScriptBreakpointActionTypeProbe: {
        JSValue exception;
        JSValue result = debuggerCallFrame->evaluate(breakpointAction.data, exception);
        if (exception)
            reportException(debuggerCallFrame->exec(), exception);
        
        JSC::ExecState* state = debuggerCallFrame->scope()->globalObject()->globalExec();
        Deprecated::ScriptValue wrappedResult = Deprecated::ScriptValue(state->vm(), exception ? exception : result);
        dispatchDidSampleProbe(state, breakpointAction.identifier, wrappedResult);
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }

    return true;
}

void ScriptDebugServer::clearBreakpoints()
{
    Debugger::clearBreakpoints();
    m_breakpointIDToActions.clear();
}

void ScriptDebugServer::dispatchDidPause(ScriptDebugListener* listener)
{
    ASSERT(isPaused());
    DebuggerCallFrame* debuggerCallFrame = currentDebuggerCallFrame();
    JSGlobalObject* globalObject = debuggerCallFrame->scope()->globalObject();
    JSC::ExecState* state = globalObject->globalExec();
    RefPtr<JavaScriptCallFrame> javaScriptCallFrame = JavaScriptCallFrame::create(debuggerCallFrame);
    JSValue jsCallFrame = toJS(state, globalObject, javaScriptCallFrame.get());
    listener->didPause(state, Deprecated::ScriptValue(state->vm(), jsCallFrame), Deprecated::ScriptValue());
}

void ScriptDebugServer::dispatchBreakpointActionLog(ExecState* exec, const String& message)
{
    if (m_callingListeners)
        return;

    ListenerSet* listeners = getListenersForGlobalObject(exec->lexicalGlobalObject());
    if (!listeners)
        return;
    ASSERT(!listeners->isEmpty());

    TemporaryChange<bool> change(m_callingListeners, true);

    Vector<ScriptDebugListener*> listenersCopy;
    copyToVector(*listeners, listenersCopy);
    for (auto listener : listenersCopy)
        listener->breakpointActionLog(exec, message);
}

void ScriptDebugServer::dispatchBreakpointActionSound(ExecState* exec)
{
    if (m_callingListeners)
        return;

    ListenerSet* listeners = getListenersForGlobalObject(exec->lexicalGlobalObject());
    if (!listeners)
        return;
    ASSERT(!listeners->isEmpty());

    TemporaryChange<bool> change(m_callingListeners, true);

    Vector<ScriptDebugListener*> listenersCopy;
    copyToVector(*listeners, listenersCopy);
    for (auto listener : listenersCopy)
        listener->breakpointActionSound();
}

void ScriptDebugServer::dispatchDidSampleProbe(ExecState* exec, int identifier, const Deprecated::ScriptValue& sample)
{
    if (m_callingListeners)
        return;

    ListenerSet* listeners = getListenersForGlobalObject(exec->lexicalGlobalObject());
    if (!listeners)
        return;
    ASSERT(!listeners->isEmpty());

    TemporaryChange<bool> change(m_callingListeners, true);

    Vector<ScriptDebugListener*> listenersCopy;
    copyToVector(*listeners, listenersCopy);
    for (auto listener : listenersCopy)
        listener->didSampleProbe(exec, identifier, m_hitCount, sample);
}

void ScriptDebugServer::dispatchDidContinue(ScriptDebugListener* listener)
{
    listener->didContinue();
}

void ScriptDebugServer::dispatchDidParseSource(const ListenerSet& listeners, SourceProvider* sourceProvider, bool isContentScript)
{
    JSC::SourceID sourceID = sourceProvider->asID();

    ScriptDebugListener::Script script;
    script.url = sourceProvider->url();
    script.source = sourceProvider->source();
    script.startLine = sourceProvider->startPosition().m_line.zeroBasedInt();
    script.startColumn = sourceProvider->startPosition().m_column.zeroBasedInt();
    script.isContentScript = isContentScript;

    int sourceLength = script.source.length();
    int lineCount = 1;
    int lastLineStart = 0;
    for (int i = 0; i < sourceLength; ++i) {
        if (script.source[i] == '\n') {
            lineCount += 1;
            lastLineStart = i + 1;
        }
    }

    script.endLine = script.startLine + lineCount - 1;
    if (lineCount == 1)
        script.endColumn = script.startColumn + sourceLength;
    else
        script.endColumn = sourceLength - lastLineStart;

    Vector<ScriptDebugListener*> copy;
    copyToVector(listeners, copy);
    for (size_t i = 0; i < copy.size(); ++i)
        copy[i]->didParseSource(sourceID, script);
}

void ScriptDebugServer::dispatchFailedToParseSource(const ListenerSet& listeners, SourceProvider* sourceProvider, int errorLine, const String& errorMessage)
{
    String url = sourceProvider->url();
    const String& data = sourceProvider->source();
    int firstLine = sourceProvider->startPosition().m_line.oneBasedInt();

    Vector<ScriptDebugListener*> copy;
    copyToVector(listeners, copy);
    for (size_t i = 0; i < copy.size(); ++i)
        copy[i]->failedToParseSource(url, data, firstLine, errorLine, errorMessage);
}

void ScriptDebugServer::sourceParsed(ExecState* exec, SourceProvider* sourceProvider, int errorLine, const String& errorMessage)
{
    if (m_callingListeners)
        return;

    ListenerSet* listeners = getListenersForGlobalObject(exec->lexicalGlobalObject());
    if (!listeners)
        return;
    ASSERT(!listeners->isEmpty());

    TemporaryChange<bool> change(m_callingListeners, true);

    bool isError = errorLine != -1;
    if (isError)
        dispatchFailedToParseSource(*listeners, sourceProvider, errorLine, errorMessage);
    else
        dispatchDidParseSource(*listeners, sourceProvider, isContentScript(exec));
}

void ScriptDebugServer::dispatchFunctionToListeners(const ListenerSet& listeners, JavaScriptExecutionCallback callback)
{
    Vector<ScriptDebugListener*> copy;
    copyToVector(listeners, copy);
    for (size_t i = 0; i < copy.size(); ++i)
        (this->*callback)(copy[i]);
}

void ScriptDebugServer::dispatchFunctionToListeners(JavaScriptExecutionCallback callback, JSGlobalObject* globalObject)
{
    if (m_callingListeners)
        return;

    TemporaryChange<bool> change(m_callingListeners, true);

    if (ListenerSet* listeners = getListenersForGlobalObject(globalObject)) {
        ASSERT(!listeners->isEmpty());
        dispatchFunctionToListeners(*listeners, callback);
    }
}

void ScriptDebugServer::notifyDoneProcessingDebuggerEvents()
{
    m_doneProcessingDebuggerEvents = true;
}

bool ScriptDebugServer::needPauseHandling(JSGlobalObject* globalObject)
{
    return !!getListenersForGlobalObject(globalObject);
}

void ScriptDebugServer::handleBreakpointHit(const JSC::Breakpoint& breakpoint)
{
    m_hitCount++;
    BreakpointIDToActionsMap::iterator it = m_breakpointIDToActions.find(breakpoint.id);
    if (it != m_breakpointIDToActions.end()) {
        BreakpointActions& actions = it->value;
        for (size_t i = 0; i < actions.size(); ++i) {
            if (!evaluateBreakpointAction(actions[i]))
                return;
        }
    }
}

void ScriptDebugServer::handleExceptionInBreakpointCondition(JSC::ExecState* exec, JSC::JSValue exception) const
{
    reportException(exec, exception);
}

void ScriptDebugServer::handlePause(Debugger::ReasonForPause, JSGlobalObject* vmEntryGlobalObject)
{
    dispatchFunctionToListeners(&ScriptDebugServer::dispatchDidPause, vmEntryGlobalObject);
    didPause(vmEntryGlobalObject);

    m_doneProcessingDebuggerEvents = false;
    runEventLoopWhilePaused();

    didContinue(vmEntryGlobalObject);
    dispatchFunctionToListeners(&ScriptDebugServer::dispatchDidContinue, vmEntryGlobalObject);
}

const Vector<ScriptBreakpointAction>& ScriptDebugServer::getActionsForBreakpoint(JSC::BreakpointID breakpointID)
{
    ASSERT(breakpointID != JSC::noBreakpointID);

    if (m_breakpointIDToActions.contains(breakpointID))
        return m_breakpointIDToActions.find(breakpointID)->value;
    
    static NeverDestroyed<Vector<ScriptBreakpointAction>> emptyActionVector = Vector<ScriptBreakpointAction>();
    return emptyActionVector;
}

} // namespace Inspector
