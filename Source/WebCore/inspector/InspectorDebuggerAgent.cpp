/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "InspectorDebuggerAgent.h"

#if ENABLE(JAVASCRIPT_DEBUGGER) && ENABLE(INSPECTOR)
#include "ContentSearchUtils.h"
#include "InjectedScript.h"
#include "InjectedScriptManager.h"
#include "InspectorFrontend.h"
#include "InspectorState.h"
#include "InspectorValues.h"
#include "InstrumentingAgents.h"
#include "RegularExpression.h"
#include "ScriptDebugServer.h"
#include "ScriptObject.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace DebuggerAgentState {
static const char debuggerEnabled[] = "debuggerEnabled";
static const char javaScriptBreakpoints[] = "javaScriptBreakopints";
};

const char* InspectorDebuggerAgent::backtraceObjectGroup = "backtrace-object-group";

InspectorDebuggerAgent::InspectorDebuggerAgent(InstrumentingAgents* instrumentingAgents, InspectorState* inspectorState, InjectedScriptManager* injectedScriptManager)
    : InspectorBaseAgent<InspectorDebuggerAgent>("Debugger", instrumentingAgents, inspectorState)
    , m_injectedScriptManager(injectedScriptManager)
    , m_frontend(0)
    , m_pausedScriptState(0)
    , m_javaScriptPauseScheduled(false)
    , m_listener(0)
{
    // FIXME: make breakReason optional so that there was no need to init it with "other".
    clearBreakDetails();
}

InspectorDebuggerAgent::~InspectorDebuggerAgent()
{
    ASSERT(!m_instrumentingAgents->inspectorDebuggerAgent());
}

void InspectorDebuggerAgent::enable()
{
    m_instrumentingAgents->setInspectorDebuggerAgent(this);

    // FIXME(WK44513): breakpoints activated flag should be synchronized between all front-ends
    scriptDebugServer().setBreakpointsActivated(true);
    startListeningScriptDebugServer();

    if (m_listener)
        m_listener->debuggerWasEnabled();
}

void InspectorDebuggerAgent::disable()
{
    m_state->setObject(DebuggerAgentState::javaScriptBreakpoints, InspectorObject::create());
    m_instrumentingAgents->setInspectorDebuggerAgent(0);

    stopListeningScriptDebugServer();
    scriptDebugServer().clearBreakpoints();
    clear();

    if (m_listener)
        m_listener->debuggerWasDisabled();
}

bool InspectorDebuggerAgent::enabled()
{
    return m_state->getBoolean(DebuggerAgentState::debuggerEnabled);
}

void InspectorDebuggerAgent::causesRecompilation(ErrorString*, bool* result)
{
    *result = scriptDebugServer().causesRecompilation();
}

void InspectorDebuggerAgent::canSetScriptSource(ErrorString*, bool* result)
{
    *result = scriptDebugServer().canSetScriptSource();
}

void InspectorDebuggerAgent::supportsNativeBreakpoints(ErrorString*, bool* result)
{
    *result = scriptDebugServer().supportsNativeBreakpoints();
}

void InspectorDebuggerAgent::enable(ErrorString*)
{
    if (enabled())
        return;

    enable();
    m_state->setBoolean(DebuggerAgentState::debuggerEnabled, true);

    ASSERT(m_frontend);
}

void InspectorDebuggerAgent::disable(ErrorString*)
{
    if (!enabled())
        return;

    disable();
    m_state->setBoolean(DebuggerAgentState::debuggerEnabled, false);
}

void InspectorDebuggerAgent::restore()
{
    if (enabled()) {
        m_frontend->globalObjectCleared();
        enable();
    }
}

void InspectorDebuggerAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend->debugger();
}

void InspectorDebuggerAgent::clearFrontend()
{
    m_frontend = 0;

    if (!enabled())
        return;

    disable();

    // FIXME: due to m_state->mute() hack in InspectorController, debuggerEnabled is actually set to false only
    // in InspectorState, but not in cookie. That's why after navigation debuggerEnabled will be true,
    // but after front-end re-open it will still be false.
    m_state->setBoolean(DebuggerAgentState::debuggerEnabled, false);
}

void InspectorDebuggerAgent::setBreakpointsActive(ErrorString*, bool active)
{
    if (active)
        scriptDebugServer().activateBreakpoints();
    else
        scriptDebugServer().deactivateBreakpoints();
}

void InspectorDebuggerAgent::didClearMainFrameWindowObject()
{
    m_scripts.clear();
    m_breakpointIdToDebugServerBreakpointIds.clear();
    if (m_frontend)
        m_frontend->globalObjectCleared();
}

static PassRefPtr<InspectorObject> buildObjectForBreakpointCookie(const String& url, int lineNumber, int columnNumber, const String& condition, bool isRegex)
{
    RefPtr<InspectorObject> breakpointObject = InspectorObject::create();
    breakpointObject->setString("url", url);
    breakpointObject->setNumber("lineNumber", lineNumber);
    breakpointObject->setNumber("columnNumber", columnNumber);
    breakpointObject->setString("condition", condition);
    breakpointObject->setBoolean("isRegex", isRegex);
    return breakpointObject;
}

static bool matches(const String& url, const String& pattern, bool isRegex)
{
    if (isRegex) {
        RegularExpression regex(pattern, TextCaseSensitive);
        return regex.match(url) != -1;
    }
    return url == pattern;
}

void InspectorDebuggerAgent::setBreakpointByUrl(ErrorString* errorString, int lineNumber, const String* const optionalURL, const String* const optionalURLRegex, const int* const optionalColumnNumber, const String* const optionalCondition, String* outBreakpointId, RefPtr<InspectorArray>& locations)
{
    if (!optionalURL == !optionalURLRegex) {
        *errorString = "Either url or urlRegex must be specified.";
        return;
    }

    String url = optionalURL ? *optionalURL : *optionalURLRegex;
    int columnNumber = optionalColumnNumber ? *optionalColumnNumber : 0;
    String condition = optionalCondition ? *optionalCondition : "";
    bool isRegex = optionalURLRegex;

    String breakpointId = (isRegex ? "/" + url + "/" : url) + ':' + String::number(lineNumber) + ':' + String::number(columnNumber);
    RefPtr<InspectorObject> breakpointsCookie = m_state->getObject(DebuggerAgentState::javaScriptBreakpoints);
    if (breakpointsCookie->find(breakpointId) != breakpointsCookie->end()) {
        *errorString = "Breakpoint at specified location already exists.";
        return;
    }

    breakpointsCookie->setObject(breakpointId, buildObjectForBreakpointCookie(url, lineNumber, columnNumber, condition, isRegex));
    m_state->setObject(DebuggerAgentState::javaScriptBreakpoints, breakpointsCookie);

    ScriptBreakpoint breakpoint(lineNumber, columnNumber, condition);
    for (ScriptsMap::iterator it = m_scripts.begin(); it != m_scripts.end(); ++it) {
        if (!matches(it->second.url, url, isRegex))
            continue;
        RefPtr<InspectorObject> location = resolveBreakpoint(breakpointId, it->first, breakpoint);
        locations->pushObject(location);
    }
    *outBreakpointId = breakpointId;
}

static bool parseLocation(ErrorString* errorString, RefPtr<InspectorObject> location, String* scriptId, int* lineNumber, int* columnNumber)
{
    if (!location->getString("scriptId", scriptId) || !location->getNumber("lineNumber", lineNumber)) {
        // FIXME: replace with input validation.
        *errorString = "scriptId and lineNumber are required.";
        return false;
    }
    *columnNumber = 0;
    location->getNumber("columnNumber", columnNumber);
    return true;
}

void InspectorDebuggerAgent::setBreakpoint(ErrorString* errorString, PassRefPtr<InspectorObject> location, const String* const optionalCondition, String* outBreakpointId, RefPtr<InspectorObject>& actualLocation)
{
    String scriptId;
    int lineNumber;
    int columnNumber;

    if (!parseLocation(errorString, location, &scriptId, &lineNumber, &columnNumber))
        return;

    String condition = optionalCondition ? *optionalCondition : emptyString();

    String breakpointId = scriptId + ':' + String::number(lineNumber) + ':' + String::number(columnNumber);
    if (m_breakpointIdToDebugServerBreakpointIds.find(breakpointId) != m_breakpointIdToDebugServerBreakpointIds.end())
        return;
    ScriptBreakpoint breakpoint(lineNumber, columnNumber, condition);
    actualLocation = resolveBreakpoint(breakpointId, scriptId, breakpoint);
    if (actualLocation)
        *outBreakpointId = breakpointId;
    else
        *errorString = "Could not resolve breakpoint";
}

void InspectorDebuggerAgent::removeBreakpoint(ErrorString*, const String& breakpointId)
{
    RefPtr<InspectorObject> breakpointsCookie = m_state->getObject(DebuggerAgentState::javaScriptBreakpoints);
    breakpointsCookie->remove(breakpointId);
    m_state->setObject(DebuggerAgentState::javaScriptBreakpoints, breakpointsCookie);

    BreakpointIdToDebugServerBreakpointIdsMap::iterator debugServerBreakpointIdsIterator = m_breakpointIdToDebugServerBreakpointIds.find(breakpointId);
    if (debugServerBreakpointIdsIterator == m_breakpointIdToDebugServerBreakpointIds.end())
        return;
    for (size_t i = 0; i < debugServerBreakpointIdsIterator->second.size(); ++i)
        scriptDebugServer().removeBreakpoint(debugServerBreakpointIdsIterator->second[i]);
    m_breakpointIdToDebugServerBreakpointIds.remove(debugServerBreakpointIdsIterator);
}

void InspectorDebuggerAgent::continueToLocation(ErrorString* errorString, PassRefPtr<InspectorObject> location)
{
    if (!m_continueToLocationBreakpointId.isEmpty()) {
        scriptDebugServer().removeBreakpoint(m_continueToLocationBreakpointId);
        m_continueToLocationBreakpointId = "";
    }

    String scriptId;
    int lineNumber;
    int columnNumber;

    if (!parseLocation(errorString, location, &scriptId, &lineNumber, &columnNumber))
        return;

    ScriptBreakpoint breakpoint(lineNumber, columnNumber, "");
    m_continueToLocationBreakpointId = scriptDebugServer().setBreakpoint(scriptId, breakpoint, &lineNumber, &columnNumber);
    resume(errorString);
}

PassRefPtr<InspectorObject> InspectorDebuggerAgent::resolveBreakpoint(const String& breakpointId, const String& scriptId, const ScriptBreakpoint& breakpoint)
{
    ScriptsMap::iterator scriptIterator = m_scripts.find(scriptId);
    if (scriptIterator == m_scripts.end())
        return 0;
    Script& script = scriptIterator->second;
    if (breakpoint.lineNumber < script.startLine || script.endLine < breakpoint.lineNumber)
        return 0;

    int actualLineNumber;
    int actualColumnNumber;
    String debugServerBreakpointId = scriptDebugServer().setBreakpoint(scriptId, breakpoint, &actualLineNumber, &actualColumnNumber);
    if (debugServerBreakpointId.isEmpty())
        return 0;

    BreakpointIdToDebugServerBreakpointIdsMap::iterator debugServerBreakpointIdsIterator = m_breakpointIdToDebugServerBreakpointIds.find(breakpointId);
    if (debugServerBreakpointIdsIterator == m_breakpointIdToDebugServerBreakpointIds.end())
        debugServerBreakpointIdsIterator = m_breakpointIdToDebugServerBreakpointIds.set(breakpointId, Vector<String>()).first;
    debugServerBreakpointIdsIterator->second.append(debugServerBreakpointId);

    RefPtr<TypeBuilder::Debugger::Location> location = TypeBuilder::Debugger::Location::create()
        .setScriptId(scriptId)
        .setLineNumber(actualLineNumber);
    location->setColumnNumber(actualColumnNumber);
    return location;
}

static PassRefPtr<InspectorObject> scriptToInspectorObject(ScriptObject scriptObject)
{
    if (scriptObject.hasNoValue())
        return 0;
    RefPtr<InspectorValue> value = scriptObject.toInspectorValue(scriptObject.scriptState());
    if (!value)
        return 0;
    return value->asObject();
}

void InspectorDebuggerAgent::searchInContent(ErrorString* error, const String& scriptId, const String& query, const bool* const optionalCaseSensitive, const bool* const optionalIsRegex, RefPtr<InspectorArray>& results)
{
    bool isRegex = optionalIsRegex ? *optionalIsRegex : false;
    bool caseSensitive = optionalCaseSensitive ? *optionalCaseSensitive : false;

    ScriptsMap::iterator it = m_scripts.find(scriptId);
    if (it != m_scripts.end())
        results = ContentSearchUtils::searchInTextByLines(it->second.source, query, caseSensitive, isRegex);
    else
        *error = "No script for id: " + scriptId;
}

void InspectorDebuggerAgent::setScriptSource(ErrorString* error, const String& scriptId, const String& newContent, const bool* const preview, RefPtr<InspectorArray>& newCallFrames, RefPtr<InspectorObject>& result)
{
    bool previewOnly = preview && *preview;
    ScriptObject resultObject;
    if (!scriptDebugServer().setScriptSource(scriptId, newContent, previewOnly, error, &m_currentCallStack, &resultObject))
        return;
    newCallFrames = currentCallFrames();
    RefPtr<InspectorObject> object = scriptToInspectorObject(resultObject);
    if (object)
        result = object;
}

void InspectorDebuggerAgent::getScriptSource(ErrorString* error, const String& scriptId, String* scriptSource)
{
    ScriptsMap::iterator it = m_scripts.find(scriptId);
    if (it != m_scripts.end())
        *scriptSource = it->second.source;
    else
        *error = "No script for id: " + scriptId;
}

void InspectorDebuggerAgent::getFunctionLocation(ErrorString* errorString, const String& functionId, RefPtr<InspectorObject>& location)
{
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(functionId);
    if (injectedScript.hasNoValue()) {
        *errorString = "Inspected frame has gone";
        return;
    }
    injectedScript.getFunctionLocation(errorString, functionId, &location);
}

void InspectorDebuggerAgent::schedulePauseOnNextStatement(const String& breakReason, PassRefPtr<InspectorObject> data)
{
    if (m_javaScriptPauseScheduled)
        return;
    m_breakReason = breakReason;
    m_breakAuxData = data;
    scriptDebugServer().setPauseOnNextStatement(true);
}

void InspectorDebuggerAgent::cancelPauseOnNextStatement()
{
    if (m_javaScriptPauseScheduled)
        return;
    clearBreakDetails();
    scriptDebugServer().setPauseOnNextStatement(false);
}

void InspectorDebuggerAgent::pause(ErrorString*)
{
    if (m_javaScriptPauseScheduled)
        return;
    clearBreakDetails();
    scriptDebugServer().setPauseOnNextStatement(true);
    m_javaScriptPauseScheduled = true;
}

void InspectorDebuggerAgent::resume(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    m_injectedScriptManager->releaseObjectGroup(InspectorDebuggerAgent::backtraceObjectGroup);
    scriptDebugServer().continueProgram();
}

void InspectorDebuggerAgent::stepOver(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    scriptDebugServer().stepOverStatement();
}

void InspectorDebuggerAgent::stepInto(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    scriptDebugServer().stepIntoStatement();
}

void InspectorDebuggerAgent::stepOut(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    scriptDebugServer().stepOutOfFunction();
}

void InspectorDebuggerAgent::setPauseOnExceptions(ErrorString* errorString, const String& stringPauseState)
{
    ScriptDebugServer::PauseOnExceptionsState pauseState;
    if (stringPauseState == "none")
        pauseState = ScriptDebugServer::DontPauseOnExceptions;
    else if (stringPauseState == "all")
        pauseState = ScriptDebugServer::PauseOnAllExceptions;
    else if (stringPauseState == "uncaught")
        pauseState = ScriptDebugServer::PauseOnUncaughtExceptions;
    else {
        *errorString = "Unknown pause on exceptions mode: " + stringPauseState;
        return;
    }
    scriptDebugServer().setPauseOnExceptionsState(static_cast<ScriptDebugServer::PauseOnExceptionsState>(pauseState));
    if (scriptDebugServer().pauseOnExceptionsState() != pauseState)
        *errorString = "Internal error. Could not change pause on exceptions state";
}

void InspectorDebuggerAgent::evaluateOnCallFrame(ErrorString* errorString, const String& callFrameId, const String& expression, const String* const objectGroup, const bool* const includeCommandLineAPI, const bool* const returnByValue, RefPtr<InspectorObject>& result, bool* wasThrown)
{
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(callFrameId);
    if (injectedScript.hasNoValue()) {
        *errorString = "Inspected frame has gone";
        return;
    }
    injectedScript.evaluateOnCallFrame(errorString, m_currentCallStack, callFrameId, expression, objectGroup ? *objectGroup : "", includeCommandLineAPI ? *includeCommandLineAPI : false, returnByValue ? *returnByValue : false, &result, wasThrown);
}

PassRefPtr<InspectorArray> InspectorDebuggerAgent::currentCallFrames()
{
    if (!m_pausedScriptState)
        return InspectorArray::create();
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptFor(m_pausedScriptState);
    if (injectedScript.hasNoValue()) {
        ASSERT_NOT_REACHED();
        return InspectorArray::create();
    }
    return injectedScript.wrapCallFrames(m_currentCallStack);
}

// JavaScriptDebugListener functions

void InspectorDebuggerAgent::didParseSource(const String& scriptId, const Script& script)
{
    // Don't send script content to the front end until it's really needed.
    m_frontend->scriptParsed(scriptId, script.url, script.startLine, script.startColumn, script.endLine, script.endColumn, script.isContentScript ? &script.isContentScript : 0);

    m_scripts.set(scriptId, script);

    if (script.url.isEmpty())
        return;

    RefPtr<InspectorObject> breakpointsCookie = m_state->getObject(DebuggerAgentState::javaScriptBreakpoints);
    for (InspectorObject::iterator it = breakpointsCookie->begin(); it != breakpointsCookie->end(); ++it) {
        RefPtr<InspectorObject> breakpointObject = it->second->asObject();
        bool isRegex;
        breakpointObject->getBoolean("isRegex", &isRegex);
        String url;
        breakpointObject->getString("url", &url);
        if (!matches(script.url, url, isRegex))
            continue;
        ScriptBreakpoint breakpoint;
        breakpointObject->getNumber("lineNumber", &breakpoint.lineNumber);
        breakpointObject->getNumber("columnNumber", &breakpoint.columnNumber);
        breakpointObject->getString("condition", &breakpoint.condition);
        RefPtr<InspectorObject> location = resolveBreakpoint(it->first, scriptId, breakpoint);
        if (location)
            m_frontend->breakpointResolved(it->first, location);
    }
}

void InspectorDebuggerAgent::failedToParseSource(const String& url, const String& data, int firstLine, int errorLine, const String& errorMessage)
{
    m_frontend->scriptFailedToParse(url, data, firstLine, errorLine, errorMessage);
}

void InspectorDebuggerAgent::didPause(ScriptState* scriptState, const ScriptValue& callFrames, const ScriptValue& exception)
{
    ASSERT(scriptState && !m_pausedScriptState);
    m_pausedScriptState = scriptState;
    m_currentCallStack = callFrames;

    if (!exception.hasNoValue()) {
        InjectedScript injectedScript = m_injectedScriptManager->injectedScriptFor(scriptState);
        if (!injectedScript.hasNoValue()) {
            m_breakReason = "exception";
            m_breakAuxData = injectedScript.wrapObject(exception, "backtrace");
        }
    }

    m_frontend->paused(currentCallFrames(), m_breakReason, m_breakAuxData);
    m_javaScriptPauseScheduled = false;

    if (!m_continueToLocationBreakpointId.isEmpty()) {
        scriptDebugServer().removeBreakpoint(m_continueToLocationBreakpointId);
        m_continueToLocationBreakpointId = "";
    }
}

void InspectorDebuggerAgent::didContinue()
{
    m_pausedScriptState = 0;
    m_currentCallStack = ScriptValue();
    clearBreakDetails();
    m_frontend->resumed();
}

void InspectorDebuggerAgent::breakProgram(const String& breakReason, PassRefPtr<InspectorObject> data)
{
    m_breakReason = breakReason;
    m_breakAuxData = data;
    scriptDebugServer().breakProgram();
}

void InspectorDebuggerAgent::clear()
{
    m_pausedScriptState = 0;
    m_currentCallStack = ScriptValue();
    m_scripts.clear();
    m_breakpointIdToDebugServerBreakpointIds.clear();
    m_continueToLocationBreakpointId = String();
    clearBreakDetails();
    m_javaScriptPauseScheduled = false;
}

bool InspectorDebuggerAgent::assertPaused(ErrorString* errorString)
{
    if (!m_pausedScriptState) {
        *errorString = "Can only perform operation while paused.";
        return false;
    }
    return true;
}

void InspectorDebuggerAgent::clearBreakDetails()
{
    m_breakReason = "other";
    m_breakAuxData = 0;
}

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER) && ENABLE(INSPECTOR)
