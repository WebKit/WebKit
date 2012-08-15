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
#include "EventLoop.h"
#include "Frame.h"
#include "JSJavaScriptCallFrame.h"
#include "JavaScriptCallFrame.h"
#include "ScriptBreakpoint.h"
#include "ScriptDebugListener.h"
#include "ScriptValue.h"
#include <debugger/DebuggerCallFrame.h>
#include <parser/SourceProvider.h>
#include <runtime/JSLock.h>
#include <wtf/MainThread.h>
#include <wtf/text/WTFString.h>

using namespace JSC;

namespace WebCore {

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
}

String ScriptDebugServer::setBreakpoint(const String& sourceID, const ScriptBreakpoint& scriptBreakpoint, int* actualLineNumber, int* actualColumnNumber)
{
    intptr_t sourceIDValue = sourceID.toIntPtr();
    if (!sourceIDValue)
        return "";
    SourceIdToBreakpointsMap::iterator it = m_sourceIdToBreakpoints.find(sourceIDValue);
    if (it == m_sourceIdToBreakpoints.end())
        it = m_sourceIdToBreakpoints.set(sourceIDValue, LineToBreakpointMap()).iterator;
    LineToBreakpointMap::iterator breaksIt = it->second.find(scriptBreakpoint.lineNumber + 1);
    if (breaksIt == it->second.end())
        breaksIt = it->second.set(scriptBreakpoint.lineNumber + 1, BreakpointsInLine()).iterator;

    BreakpointsInLine& breaksVector = breaksIt->second;
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
    LineToBreakpointMap::iterator breaksIt = it->second.find(lineNumber + 1);
    if (breaksIt == it->second.end())
        return;

    BreakpointsInLine& breaksVector = breaksIt->second;
    unsigned breaksCount = breaksVector.size();
    for (unsigned i = 0; i < breaksCount; i++) {
        if (breaksVector.at(i).columnNumber == static_cast<int>(columnNumber)) {
            breaksVector.remove(i);
            break;
        }
    }
}

void ScriptDebugServer::updateCurrentStatementPosition(intptr_t sourceID, int line)
{
    if (line < 0)
        return;

    SourceProvider* source = reinterpret_cast<SourceProvider*>(sourceID);

    if (m_currentSourceID != sourceID) {
        String sourceCode = ustringToString(JSC::UString(const_cast<StringImpl*>(source->data())));
        m_currentSourceCode.clear();
        sourceCode.split("\n", true, m_currentSourceCode);
        m_currentSourceID = sourceID;
        m_currentStatementPosition.lineNumber = 0;
        m_currentStatementPosition.columnNumber = 0;
    }

    if (line != m_currentStatementPosition.lineNumber) {
        m_currentStatementPosition.lineNumber = line;
        m_currentStatementPosition.columnNumber = 0;
        return;
    }

    int startLine = source->startPosition().m_line.zeroBasedInt();
    if ((m_currentStatementPosition.lineNumber - startLine - 1) >= static_cast<int>(m_currentSourceCode.size()))
        return;
    const String& codeInLine = m_currentSourceCode[m_currentStatementPosition.lineNumber - startLine - 1];
    if (codeInLine.isEmpty())
        return;
    int nextColumn = codeInLine.find(";", m_currentStatementPosition.columnNumber);
    if (nextColumn != -1) {
        UChar c = codeInLine[nextColumn + 1];
        if (c == ' ' || c == '\t')
            nextColumn += 1;
        m_currentStatementPosition.columnNumber = nextColumn + 1;
    } else
        m_currentStatementPosition.columnNumber = 0;
}

bool ScriptDebugServer::hasBreakpoint(intptr_t sourceID, const TextPosition& position) const
{
    if (!m_breakpointsActivated)
        return false;

    SourceIdToBreakpointsMap::const_iterator it = m_sourceIdToBreakpoints.find(sourceID);
    if (it == m_sourceIdToBreakpoints.end())
        return false;

    int lineNumber = position.m_line.zeroBasedInt();
    int columnNumber = position.m_column.zeroBasedInt();
    if (lineNumber < 0 || columnNumber < 0)
        return false;

    LineToBreakpointMap::const_iterator breaksIt = it->second.find(lineNumber + 1);
    if (breaksIt == it->second.end())
        return false;

    bool hit = false;
    const BreakpointsInLine& breaksVector = breaksIt->second;
    unsigned breaksCount = breaksVector.size();
    unsigned i;
    for (i = 0; i < breaksCount; i++) {
        int breakLine = breaksVector.at(i).lineNumber;
        if (lineNumber == breakLine) {
            hit = true;
            break;
        }
    }
    if (!hit)
        return false;

    // An empty condition counts as no condition which is equivalent to "true".
    if (breaksVector.at(i).condition.isEmpty())
        return true;

    JSValue exception;
    JSValue result = m_currentCallFrame->evaluate(stringToUString(breaksVector.at(i).condition), exception);
    if (exception) {
        // An erroneous condition counts as "false".
        return false;
    }
    return result.toBoolean();
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
    pauseIfNeeded(m_currentCallFrame->dynamicGlobalObject());
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

void ScriptDebugServer::dispatchDidPause(ScriptDebugListener* listener)
{
    ASSERT(m_paused);
    JSGlobalObject* globalObject = m_currentCallFrame->scopeChain()->globalObject.get();
    ScriptState* state = globalObject->globalExec();
    JSValue jsCallFrame;
    {
        if (m_currentCallFrame->isValid() && globalObject->inherits(&JSDOMGlobalObject::s_info)) {
            JSDOMGlobalObject* domGlobalObject = jsCast<JSDOMGlobalObject*>(globalObject);
            JSLockHolder lock(state);
            jsCallFrame = toJS(state, domGlobalObject, m_currentCallFrame.get());
        } else
            jsCallFrame = jsUndefined();
    }
    listener->didPause(state, ScriptValue(state->globalData(), jsCallFrame), ScriptValue());
}

void ScriptDebugServer::dispatchDidContinue(ScriptDebugListener* listener)
{
    listener->didContinue();
}

void ScriptDebugServer::dispatchDidParseSource(const ListenerSet& listeners, SourceProvider* sourceProvider, bool isContentScript)
{
    String sourceID = ustringToString(JSC::UString::number(sourceProvider->asID()));

    ScriptDebugListener::Script script;
    script.url = ustringToString(sourceProvider->url());
    script.source = ustringToString(JSC::UString(const_cast<StringImpl*>(sourceProvider->data())));
    script.startLine = sourceProvider->startPosition().m_line.zeroBasedInt();
    script.startColumn = sourceProvider->startPosition().m_column.zeroBasedInt();
    script.isContentScript = isContentScript;

#if ENABLE(INSPECTOR)
    if (script.url.isEmpty())
        script.url = ContentSearchUtils::findSourceURL(script.source);
#endif

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
    String url = ustringToString(sourceProvider->url());
    String data = ustringToString(JSC::UString(const_cast<StringImpl*>(sourceProvider->data())));
    int firstLine = sourceProvider->startPosition().m_line.oneBasedInt();

    Vector<ScriptDebugListener*> copy;
    copyToVector(listeners, copy);
    for (size_t i = 0; i < copy.size(); ++i)
        copy[i]->failedToParseSource(url, data, firstLine, errorLine, errorMessage);
}

static bool isContentScript(ExecState* exec)
{
    return currentWorld(exec) != mainThreadNormalWorld();
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

void ScriptDebugServer::sourceParsed(ExecState* exec, SourceProvider* sourceProvider, int errorLine, const UString& errorMessage)
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
        dispatchFailedToParseSource(*listeners, sourceProvider, errorLine, ustringToString(errorMessage));
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

void ScriptDebugServer::createCallFrameAndPauseIfNeeded(const DebuggerCallFrame& debuggerCallFrame, intptr_t sourceID, int lineNumber, int columnNumber)
{
    TextPosition textPosition(OrdinalNumber::fromOneBasedInt(lineNumber), OrdinalNumber::fromZeroBasedInt(columnNumber));
    m_currentCallFrame = JavaScriptCallFrame::create(debuggerCallFrame, m_currentCallFrame, sourceID, textPosition);
    pauseIfNeeded(debuggerCallFrame.dynamicGlobalObject());
}

void ScriptDebugServer::updateCallFrameAndPauseIfNeeded(const DebuggerCallFrame& debuggerCallFrame, intptr_t sourceID, int lineNumber, int columnNumber)
{
    ASSERT(m_currentCallFrame);
    if (!m_currentCallFrame)
        return;

    TextPosition textPosition(OrdinalNumber::fromOneBasedInt(lineNumber), OrdinalNumber::fromZeroBasedInt(columnNumber));
    m_currentCallFrame->update(debuggerCallFrame, sourceID, textPosition);
    pauseIfNeeded(debuggerCallFrame.dynamicGlobalObject());
}

void ScriptDebugServer::pauseIfNeeded(JSGlobalObject* dynamicGlobalObject)
{
    if (m_paused)
        return;
 
    if (!getListenersForGlobalObject(dynamicGlobalObject))
        return;

    bool pauseNow = m_pauseOnNextStatement;
    pauseNow |= (m_pauseOnCallFrame == m_currentCallFrame);
    pauseNow |= hasBreakpoint(m_currentCallFrame->sourceID(), m_currentCallFrame->position());
    if (!pauseNow)
        return;

    m_pauseOnCallFrame = 0;
    m_pauseOnNextStatement = false;
    m_paused = true;

    dispatchFunctionToListeners(&ScriptDebugServer::dispatchDidPause, dynamicGlobalObject);
    didPause(dynamicGlobalObject);

    TimerBase::fireTimersInNestedEventLoop();

    EventLoop loop;
    m_doneProcessingDebuggerEvents = false;
    while (!m_doneProcessingDebuggerEvents && !loop.ended())
        loop.cycle();

    didContinue(dynamicGlobalObject);
    dispatchFunctionToListeners(&ScriptDebugServer::dispatchDidContinue, dynamicGlobalObject);

    m_paused = false;
}

void ScriptDebugServer::callEvent(const DebuggerCallFrame& debuggerCallFrame, intptr_t sourceID, int lineNumber, int columnNumber)
{
    if (!m_paused)
        createCallFrameAndPauseIfNeeded(debuggerCallFrame, sourceID, lineNumber, columnNumber);
}

void ScriptDebugServer::atStatement(const DebuggerCallFrame& debuggerCallFrame, intptr_t sourceID, int lineNumber, int columnNumber)
{
    if (!m_paused)
        updateCallFrameAndPauseIfNeeded(debuggerCallFrame, sourceID, lineNumber, columnNumber);
}

void ScriptDebugServer::returnEvent(const DebuggerCallFrame& debuggerCallFrame, intptr_t sourceID, int lineNumber, int columnNumber)
{
    if (m_paused)
        return;

    updateCallFrameAndPauseIfNeeded(debuggerCallFrame, sourceID, lineNumber, columnNumber);

    // detach may have been called during pauseIfNeeded
    if (!m_currentCallFrame)
        return;

    // Treat stepping over a return statement like stepping out.
    if (m_currentCallFrame == m_pauseOnCallFrame)
        m_pauseOnCallFrame = m_currentCallFrame->caller();
    m_currentCallFrame = m_currentCallFrame->caller();
}

void ScriptDebugServer::exception(const DebuggerCallFrame& debuggerCallFrame, intptr_t sourceID, int lineNumber, int columnNumber, bool hasHandler)
{
    if (m_paused)
        return;

    if (m_pauseOnExceptionsState == PauseOnAllExceptions || (m_pauseOnExceptionsState == PauseOnUncaughtExceptions && !hasHandler))
        m_pauseOnNextStatement = true;

    updateCallFrameAndPauseIfNeeded(debuggerCallFrame, sourceID, lineNumber, columnNumber);
}

void ScriptDebugServer::willExecuteProgram(const DebuggerCallFrame& debuggerCallFrame, intptr_t sourceID, int lineNumber, int columnNumber)
{
    if (!m_paused)
        createCallFrameAndPauseIfNeeded(debuggerCallFrame, sourceID, lineNumber, columnNumber);
}

void ScriptDebugServer::didExecuteProgram(const DebuggerCallFrame& debuggerCallFrame, intptr_t sourceID, int lineNumber, int columnNumber)
{
    if (m_paused)
        return;

    updateCallFrameAndPauseIfNeeded(debuggerCallFrame, sourceID, lineNumber, columnNumber);

    // Treat stepping over the end of a program like stepping out.
    if (m_currentCallFrame == m_pauseOnCallFrame)
        m_pauseOnCallFrame = m_currentCallFrame->caller();
    m_currentCallFrame = m_currentCallFrame->caller();
}

void ScriptDebugServer::didReachBreakpoint(const DebuggerCallFrame& debuggerCallFrame, intptr_t sourceID, int lineNumber, int columnNumber)
{
    if (m_paused)
        return;

    m_pauseOnNextStatement = true;
    updateCallFrameAndPauseIfNeeded(debuggerCallFrame, sourceID, lineNumber, columnNumber);
}

void ScriptDebugServer::recompileAllJSFunctionsSoon()
{
    m_recompileTimer.startOneShot(0);
}

void ScriptDebugServer::compileScript(ScriptState*, const String&, const String&, String*, String*)
{
    // FIXME(89652): implement this.
}

void ScriptDebugServer::clearCompiledScripts()
{
    // FIXME(89652): implement this.
}

void ScriptDebugServer::runScript(ScriptState*, const String&, ScriptValue*, bool*, String*)
{
    // FIXME(89652): implement this.
}

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)
