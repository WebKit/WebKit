/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010-2011 Google Inc. All rights reserved.
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

#if ENABLE(JAVASCRIPT_DEBUGGER)

#include "ScriptDebugServer.h"

#include "ContentSearchUtils.h"
#include "Frame.h"
#include "JSDOMWindowCustom.h"
#include "JSJavaScriptCallFrame.h"
#include "JavaScriptCallFrame.h"
#include "PageConsole.h"
#include "ScriptBreakpoint.h"
#include "ScriptDebugListener.h"
#include "ScriptValue.h"
#include "Sound.h"
#include <debugger/DebuggerCallFrame.h>
#include <parser/SourceProvider.h>
#include <runtime/JSLock.h>
#include <wtf/MainThread.h>
#include <wtf/text/WTFString.h>

using namespace JSC;

namespace WebCore {

class DebuggerCallFrameScope {
public:
    DebuggerCallFrameScope(ScriptDebugServer& debugger)
        : m_debugger(debugger)
    {
        ASSERT(!m_debugger.m_currentDebuggerCallFrame);
        if (m_debugger.m_currentCallFrame)
            m_debugger.m_currentDebuggerCallFrame = DebuggerCallFrame::create(debugger.m_currentCallFrame);
    }

    ~DebuggerCallFrameScope()
    {
        if (m_debugger.m_currentDebuggerCallFrame) {
            m_debugger.m_currentDebuggerCallFrame->invalidate();
            m_debugger.m_currentDebuggerCallFrame = 0;
        }
    }

private:
    ScriptDebugServer& m_debugger;
};


ScriptDebugServer::ScriptDebugServer()
    : m_callingListeners(false)
    , m_pauseOnExceptionsState(DontPauseOnExceptions)
    , m_pauseOnNextStatement(false)
    , m_paused(false)
    , m_runningNestedMessageLoop(false)
    , m_doneProcessingDebuggerEvents(true)
    , m_breakpointsActivated(true)
    , m_pauseOnCallFrame(0)
    , m_currentCallFrame(0)
    , m_recompileTimer(this, &ScriptDebugServer::recompileAllJSFunctions)
    , m_lastExecutedLine(-1)
    , m_lastExecutedSourceId(-1)
{
}

ScriptDebugServer::~ScriptDebugServer()
{
}

String ScriptDebugServer::setBreakpoint(const String& sourceID, const ScriptBreakpoint& scriptBreakpoint, int* actualLineNumber, int* actualColumnNumber)
{
    intptr_t sourceIDValue = sourceID.toIntPtr();
    if (!sourceIDValue)
        return "";
    SourceIdToBreakpointsMap::iterator it = m_sourceIdToBreakpoints.find(sourceIDValue);
    if (it == m_sourceIdToBreakpoints.end())
        it = m_sourceIdToBreakpoints.set(sourceIDValue, LineToBreakpointsMap()).iterator;
    LineToBreakpointsMap::iterator breaksIt = it->value.find(scriptBreakpoint.lineNumber);
    if (breaksIt == it->value.end())
        breaksIt = it->value.set(scriptBreakpoint.lineNumber, BreakpointsInLine()).iterator;

    BreakpointsInLine& breaksVector = breaksIt->value;
    unsigned breaksCount = breaksVector.size();
    for (unsigned i = 0; i < breaksCount; i++) {
        if (breaksVector.at(i).columnNumber == scriptBreakpoint.columnNumber)
            return "";
    }
    breaksVector.append(scriptBreakpoint);

    *actualLineNumber = scriptBreakpoint.lineNumber;
    *actualColumnNumber = scriptBreakpoint.columnNumber;
    return sourceID + ":" + String::number(scriptBreakpoint.lineNumber) + ":" + String::number(scriptBreakpoint.columnNumber);
}

void ScriptDebugServer::removeBreakpoint(const String& breakpointId)
{
    Vector<String> tokens;
    breakpointId.split(":", tokens);
    if (tokens.size() != 3)
        return;
    bool success;
    intptr_t sourceIDValue = tokens[0].toIntPtr(&success);
    if (!success)
        return;
    unsigned lineNumber = tokens[1].toUInt(&success);
    if (!success)
        return;
    unsigned columnNumber = tokens[2].toUInt(&success);
    if (!success)
        return;

    SourceIdToBreakpointsMap::iterator it = m_sourceIdToBreakpoints.find(sourceIDValue);
    if (it == m_sourceIdToBreakpoints.end())
        return;
    LineToBreakpointsMap::iterator breaksIt = it->value.find(lineNumber);
    if (breaksIt == it->value.end())
        return;

    BreakpointsInLine& breaksVector = breaksIt->value;
    unsigned breaksCount = breaksVector.size();
    for (unsigned i = 0; i < breaksCount; i++) {
        if (breaksVector.at(i).columnNumber == static_cast<int>(columnNumber)) {
            breaksVector.remove(i);
            break;
        }
    }
}

bool ScriptDebugServer::hasBreakpoint(intptr_t sourceID, const TextPosition& position, ScriptBreakpoint *hitBreakpoint) const
{
    if (!m_breakpointsActivated)
        return false;

    SourceIdToBreakpointsMap::const_iterator it = m_sourceIdToBreakpoints.find(sourceID);
    if (it == m_sourceIdToBreakpoints.end())
        return false;

    int line = position.m_line.zeroBasedInt();
    int column = position.m_column.zeroBasedInt();
    if (line < 0 || column < 0)
        return false;

    LineToBreakpointsMap::const_iterator breaksIt = it->value.find(line);
    if (breaksIt == it->value.end())
        return false;

    bool hit = false;
    const BreakpointsInLine& breaksVector = breaksIt->value;
    unsigned breaksCount = breaksVector.size();
    unsigned i;
    for (i = 0; i < breaksCount; i++) {
        int breakLine = breaksVector.at(i).lineNumber;
        int breakColumn = breaksVector.at(i).columnNumber;
        // Since frontend truncates the indent, the first statement in a line must match the breakpoint (line,0).
        if ((line != m_lastExecutedLine && line == breakLine && !breakColumn)
            || (line == breakLine && column == breakColumn)) {
            hit = true;
            break;
        }
    }
    if (!hit)
        return false;

    if (hitBreakpoint)
        *hitBreakpoint = breaksVector.at(i);

    // An empty condition counts as no condition which is equivalent to "true".
    if (breaksVector.at(i).condition.isEmpty())
        return true;

    JSValue exception;
    JSValue result = DebuggerCallFrame::evaluateWithCallFrame(m_currentCallFrame, breaksVector.at(i).condition, exception);
    if (exception) {
        // An erroneous condition counts as "false".
        reportException(m_currentCallFrame, exception);
        return false;
    }
    return result.toBoolean(m_currentCallFrame);
}

bool ScriptDebugServer::evaluateBreakpointAction(const ScriptBreakpointAction& breakpointAction) const
{
    DebuggerCallFrame* debuggerCallFrame = currentDebuggerCallFrame();
    switch (breakpointAction.type) {
    case ScriptBreakpointActionTypeLog: {
        DOMWindow& window = asJSDOMWindow(debuggerCallFrame->dynamicGlobalObject())->impl();
        if (PageConsole* console = window.pageConsole())
            console->addMessage(JSMessageSource, LogMessageLevel, breakpointAction.data);
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
        systemBeep();
        break;
    }

    return true;
}

bool ScriptDebugServer::evaluateBreakpointActions(const ScriptBreakpoint& breakpoint) const
{
    for (size_t i = 0; i < breakpoint.actions.size(); ++i) {
        if (!evaluateBreakpointAction(breakpoint.actions[i]))
            return false;
    }

    return true;
}

void ScriptDebugServer::clearBreakpoints()
{
    m_sourceIdToBreakpoints.clear();
}

void ScriptDebugServer::setBreakpointsActivated(bool activated)
{
    m_breakpointsActivated = activated;
}

void ScriptDebugServer::setPauseOnExceptionsState(PauseOnExceptionsState pause)
{
    m_pauseOnExceptionsState = pause;
}

void ScriptDebugServer::setPauseOnNextStatement(bool pause)
{
    m_pauseOnNextStatement = pause;
}

void ScriptDebugServer::breakProgram()
{
    if (m_paused || !m_currentCallFrame)
        return;

    m_pauseOnNextStatement = true;
    pauseIfNeeded(m_currentCallFrame);
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

    m_pauseOnCallFrame = m_currentCallFrame;
    m_doneProcessingDebuggerEvents = true;
}

void ScriptDebugServer::stepOutOfFunction()
{
    if (!m_paused)
        return;

    m_pauseOnCallFrame = m_currentCallFrame ? m_currentCallFrame->callerFrameNoFlags() : 0;
    m_doneProcessingDebuggerEvents = true;
}

bool ScriptDebugServer::canSetScriptSource()
{
    return false;
}

bool ScriptDebugServer::setScriptSource(const String&, const String&, bool, String*, ScriptValue*, ScriptObject*)
{
    // FIXME(40300): implement this.
    return false;
}


void ScriptDebugServer::updateCallStack(ScriptValue*)
{
    // This method is used for restart frame feature that is not implemented yet.
    // FIXME(40300): implement this.
}

DebuggerCallFrame* ScriptDebugServer::currentDebuggerCallFrame() const
{
    ASSERT(m_currentDebuggerCallFrame);
    return m_currentDebuggerCallFrame.get();
}

void ScriptDebugServer::dispatchDidPause(ScriptDebugListener* listener)
{
    ASSERT(m_paused);
    DebuggerCallFrame* debuggerCallFrame = currentDebuggerCallFrame();
    JSGlobalObject* globalObject = debuggerCallFrame->scope()->globalObject();
    JSC::ExecState* state = globalObject->globalExec();
    RefPtr<JavaScriptCallFrame> javaScriptCallFrame = JavaScriptCallFrame::create(debuggerCallFrame);
    JSValue jsCallFrame;
    {
        if (globalObject->inherits(JSDOMGlobalObject::info())) {
            JSDOMGlobalObject* domGlobalObject = jsCast<JSDOMGlobalObject*>(globalObject);
            JSLockHolder lock(state);
            jsCallFrame = toJS(state, domGlobalObject, javaScriptCallFrame.get());
        } else
            jsCallFrame = jsUndefined();
    }
    listener->didPause(state, ScriptValue(state->vm(), jsCallFrame), ScriptValue());
}

void ScriptDebugServer::dispatchDidContinue(ScriptDebugListener* listener)
{
    listener->didContinue();
}

void ScriptDebugServer::dispatchDidParseSource(const ListenerSet& listeners, SourceProvider* sourceProvider, bool isContentScript)
{
    String sourceID = String::number(sourceProvider->asID());

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

bool ScriptDebugServer::isContentScript(ExecState* exec)
{
    return &currentWorld(exec) != &mainThreadNormalWorld();
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

void ScriptDebugServer::sourceParsed(ExecState* exec, SourceProvider* sourceProvider, int errorLine, const String& errorMessage)
{
    if (m_callingListeners)
        return;

    ListenerSet* listeners = getListenersForGlobalObject(exec->lexicalGlobalObject());
    if (!listeners)
        return;
    ASSERT(!listeners->isEmpty());

    m_callingListeners = true;

    bool isError = errorLine != -1;
    if (isError)
        dispatchFailedToParseSource(*listeners, sourceProvider, errorLine, errorMessage);
    else
        dispatchDidParseSource(*listeners, sourceProvider, isContentScript(exec));

    m_callingListeners = false;
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

    m_callingListeners = true;

    if (ListenerSet* listeners = getListenersForGlobalObject(globalObject)) {
        ASSERT(!listeners->isEmpty());
        dispatchFunctionToListeners(*listeners, callback);
    }

    m_callingListeners = false;
}

void ScriptDebugServer::updateCallFrame(CallFrame* callFrame)
{
    m_currentCallFrame = callFrame;
    intptr_t sourceId = DebuggerCallFrame::sourceIdForCallFrame(callFrame);
    if (m_lastExecutedSourceId != sourceId) {
        m_lastExecutedLine = -1;
        m_lastExecutedSourceId = sourceId;
    }
}

void ScriptDebugServer::updateCallFrameAndPauseIfNeeded(CallFrame* callFrame)
{
    updateCallFrame(callFrame);
    pauseIfNeeded(callFrame);
}

void ScriptDebugServer::pauseIfNeeded(CallFrame* callFrame)
{
    if (m_paused)
        return;
 
    JSGlobalObject* dynamicGlobalObject = callFrame->dynamicGlobalObject();
    if (!getListenersForGlobalObject(dynamicGlobalObject))
        return;

    ScriptBreakpoint breakpoint;
    bool didHitBreakpoint = false;
    bool pauseNow = m_pauseOnNextStatement;
    pauseNow |= (m_pauseOnCallFrame == m_currentCallFrame);

    intptr_t sourceId = DebuggerCallFrame::sourceIdForCallFrame(m_currentCallFrame);
    TextPosition position = DebuggerCallFrame::positionForCallFrame(m_currentCallFrame);
    pauseNow |= didHitBreakpoint = hasBreakpoint(sourceId, position, &breakpoint);
    m_lastExecutedLine = position.m_line.zeroBasedInt();
    if (!pauseNow)
        return;

    DebuggerCallFrameScope debuggerCallFrameScope(*this);

    if (didHitBreakpoint) {
        evaluateBreakpointActions(breakpoint);
        if (breakpoint.autoContinue)
            return;
    }

    m_pauseOnCallFrame = 0;
    m_pauseOnNextStatement = false;
    m_paused = true;

    dispatchFunctionToListeners(&ScriptDebugServer::dispatchDidPause, dynamicGlobalObject);
    didPause(dynamicGlobalObject);

    TimerBase::fireTimersInNestedEventLoop();

    m_runningNestedMessageLoop = true;
    m_doneProcessingDebuggerEvents = false;
    runEventLoopWhilePaused();
    m_runningNestedMessageLoop = false;

    didContinue(dynamicGlobalObject);
    dispatchFunctionToListeners(&ScriptDebugServer::dispatchDidContinue, dynamicGlobalObject);

    m_paused = false;
}

void ScriptDebugServer::callEvent(CallFrame* callFrame)
{
    if (!m_paused) {
        updateCallFrame(callFrame);
        pauseIfNeeded(callFrame);
    }
}

void ScriptDebugServer::atStatement(CallFrame* callFrame)
{
    if (!m_paused)
        updateCallFrameAndPauseIfNeeded(callFrame);
}

void ScriptDebugServer::returnEvent(CallFrame* callFrame)
{
    if (m_paused)
        return;

    updateCallFrameAndPauseIfNeeded(callFrame);

    // detach may have been called during pauseIfNeeded
    if (!m_currentCallFrame)
        return;

    // Treat stepping over a return statement like stepping out.
    if (m_currentCallFrame == m_pauseOnCallFrame)
        m_pauseOnCallFrame = m_currentCallFrame->callerFrameNoFlags();

    m_currentCallFrame = m_currentCallFrame->callerFrameNoFlags();
}

void ScriptDebugServer::exception(CallFrame* callFrame, JSValue, bool hasHandler)
{
    if (m_paused)
        return;

    if (m_pauseOnExceptionsState == PauseOnAllExceptions || (m_pauseOnExceptionsState == PauseOnUncaughtExceptions && !hasHandler))
        m_pauseOnNextStatement = true;

    updateCallFrameAndPauseIfNeeded(callFrame);
}

void ScriptDebugServer::willExecuteProgram(CallFrame* callFrame)
{
    if (!m_paused) {
        updateCallFrame(callFrame);
        pauseIfNeeded(callFrame);
    }
}

void ScriptDebugServer::didExecuteProgram(CallFrame* callFrame)
{
    if (m_paused)
        return;

    updateCallFrameAndPauseIfNeeded(callFrame);

    // Treat stepping over the end of a program like stepping out.
    if (!m_currentCallFrame)
        return;
    if (m_currentCallFrame == m_pauseOnCallFrame) {
        m_pauseOnCallFrame = m_currentCallFrame->callerFrameNoFlags();
        if (!m_currentCallFrame)
            return;
    }
    m_currentCallFrame = m_currentCallFrame->callerFrameNoFlags();
}

void ScriptDebugServer::didReachBreakpoint(CallFrame* callFrame)
{
    if (m_paused)
        return;

    m_pauseOnNextStatement = true;
    updateCallFrameAndPauseIfNeeded(callFrame);
}

void ScriptDebugServer::recompileAllJSFunctionsSoon()
{
    m_recompileTimer.startOneShot(0);
}

void ScriptDebugServer::compileScript(JSC::ExecState*, const String&, const String&, String*, String*)
{
    // FIXME(89652): implement this.
}

void ScriptDebugServer::clearCompiledScripts()
{
    // FIXME(89652): implement this.
}

void ScriptDebugServer::runScript(JSC::ExecState*, const String&, ScriptValue*, bool*, String*)
{
    // FIXME(89652): implement this.
}

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)
