/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "InspectorDebuggerAgent.h"

#if ENABLE(JAVASCRIPT_DEBUGGER) && ENABLE(INSPECTOR)
#include "InjectedScript.h"
#include "InjectedScriptHost.h"
#include "InspectorFrontend.h"
#include "InspectorState.h"
#include "InspectorValues.h"
#include "InstrumentingAgents.h"
#include "PlatformString.h"
#include "ScriptDebugServer.h"
#include <wtf/text/StringConcatenate.h>

namespace WebCore {

namespace DebuggerAgentState {
static const char debuggerEnabled[] = "debuggerEnabled";
static const char enableWhenShown[] = "debuggerEnableWhenShown";
static const char javaScriptBreakpoints[] = "javaScriptBreakopints";
};

PassOwnPtr<InspectorDebuggerAgent> InspectorDebuggerAgent::create(InstrumentingAgents* instrumentingAgents, InspectorState* inspectorState, Page* inspectedPage, InjectedScriptHost* injectedScriptHost)
{
    return adoptPtr(new InspectorDebuggerAgent(instrumentingAgents, inspectorState, inspectedPage, injectedScriptHost));
}

InspectorDebuggerAgent::InspectorDebuggerAgent(InstrumentingAgents* instrumentingAgents, InspectorState* inspectorState, Page* inspectedPage, InjectedScriptHost* injectedScriptHost)
    : m_instrumentingAgents(instrumentingAgents)
    , m_inspectorState(inspectorState)
    , m_inspectedPage(inspectedPage)
    , m_injectedScriptHost(injectedScriptHost)
    , m_frontend(0)
    , m_pausedScriptState(0)
    , m_javaScriptPauseScheduled(false)
    , m_listener(0)
{
}

InspectorDebuggerAgent::~InspectorDebuggerAgent()
{
    ScriptDebugServer::shared().removeListener(this, m_inspectedPage);
    m_pausedScriptState = 0;
}

void InspectorDebuggerAgent::startUserInitiatedDebugging()
{
    if (m_frontend)
        enable(false);
    else
        m_inspectorState->setBoolean(DebuggerAgentState::enableWhenShown, true);
}

void InspectorDebuggerAgent::enable(bool restoringFromState)
{
    ASSERT(m_frontend);
    if (!restoringFromState && enabled())
        return;
    m_inspectorState->setBoolean(DebuggerAgentState::debuggerEnabled, true);
    m_instrumentingAgents->setInspectorDebuggerAgent(this);

    ScriptDebugServer::shared().clearBreakpoints();
    // FIXME(WK44513): breakpoints activated flag should be synchronized between all front-ends
    ScriptDebugServer::shared().setBreakpointsActivated(true);
    ScriptDebugServer::shared().addListener(this, m_inspectedPage);

    m_frontend->debuggerWasEnabled();
    if (m_listener)
        m_listener->debuggerWasEnabled();
}

void InspectorDebuggerAgent::disable()
{
    if (!enabled())
        return;
    m_inspectorState->setBoolean(DebuggerAgentState::debuggerEnabled, false);
    m_instrumentingAgents->setInspectorDebuggerAgent(0);

    ScriptDebugServer::shared().removeListener(this, m_inspectedPage);
    clear();

    if (m_frontend)
        m_frontend->debuggerWasDisabled();
    if (m_listener)
        m_listener->debuggerWasDisabled();
}

bool InspectorDebuggerAgent::enabled()
{
    return m_inspectorState->getBoolean(DebuggerAgentState::debuggerEnabled);
}

void InspectorDebuggerAgent::restore()
{
    if (m_inspectorState->getBoolean(DebuggerAgentState::debuggerEnabled))
        enable(true);
}

void InspectorDebuggerAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend;
}

void InspectorDebuggerAgent::enableDebuggerAfterShown()
{
    // Erase sticky breakpoints. If we are restoring from a cookie setFrontend msut be called
    // before the state is loaded from the cookie.
    m_inspectorState->setObject(DebuggerAgentState::javaScriptBreakpoints, InspectorObject::create());
    if (m_inspectorState->getBoolean(DebuggerAgentState::enableWhenShown)) {
        m_inspectorState->setBoolean(DebuggerAgentState::enableWhenShown, false);
        enable(false);
    }
}

void InspectorDebuggerAgent::clearFrontend()
{
    m_frontend = 0;

    if (!enabled())
        return;
    // If the window is being closed with the debugger enabled,
    // remember this state to re-enable debugger on the next window
    // opening.
    disable();
    m_inspectorState->setBoolean(DebuggerAgentState::enableWhenShown, true);
}

void InspectorDebuggerAgent::activateBreakpoints(ErrorString*)
{
    ScriptDebugServer::shared().activateBreakpoints();
}

void InspectorDebuggerAgent::deactivateBreakpoints(ErrorString*)
{
    ScriptDebugServer::shared().deactivateBreakpoints();
}

void InspectorDebuggerAgent::inspectedURLChanged(const String&)
{
    m_scripts.clear();
    m_breakpointIdToDebugServerBreakpointIds.clear();
}

void InspectorDebuggerAgent::setJavaScriptBreakpoint(ErrorString*, const String& url, int lineNumber, int columnNumber, const String& condition, bool enabled, String* outBreakpointId, RefPtr<InspectorArray>* locations)
{
    String breakpointId = makeString(url, ":", String::number(lineNumber), ":", String::number(columnNumber));
    RefPtr<InspectorObject> breakpointsCookie = m_inspectorState->getObject(DebuggerAgentState::javaScriptBreakpoints);
    if (breakpointsCookie->find(breakpointId) != breakpointsCookie->end())
        return;
    RefPtr<InspectorObject> breakpointObject = InspectorObject::create();
    breakpointObject->setString("url", url);
    breakpointObject->setNumber("lineNumber", lineNumber);
    breakpointObject->setNumber("columnNumber", columnNumber);
    breakpointObject->setString("condition", condition);
    breakpointObject->setBoolean("enabled", enabled);
    breakpointsCookie->setObject(breakpointId, breakpointObject);
    m_inspectorState->setObject(DebuggerAgentState::javaScriptBreakpoints, breakpointsCookie);

    ScriptBreakpoint breakpoint(lineNumber, columnNumber, condition, enabled);
    for (ScriptsMap::iterator it = m_scripts.begin(); it != m_scripts.end(); ++it) {
        if (it->second.url != url)
            continue;
        int actualLineNumber = 0, actualColumnNumber = 0;
        if (!resolveBreakpoint(breakpointId, it->first, breakpoint, &actualLineNumber, &actualColumnNumber))
            continue;
        RefPtr<InspectorObject> location = InspectorObject::create();
        location->setString("sourceID", it->first);
        location->setNumber("lineNumber", actualLineNumber);
        location->setNumber("columnNumber", actualColumnNumber);
        locations->get()->pushObject(location);
    }
    *outBreakpointId = breakpointId;
}

void InspectorDebuggerAgent::setJavaScriptBreakpointBySourceId(ErrorString*, const String& sourceId, int lineNumber, int columnNumber, const String& condition, bool enabled, String* outBreakpointId, int* actualLineNumber, int* actualColumnNumber)
{
    String breakpointId = makeString(sourceId, ":", String::number(lineNumber), ":", String::number(columnNumber));
    if (m_breakpointIdToDebugServerBreakpointIds.find(breakpointId) != m_breakpointIdToDebugServerBreakpointIds.end())
        return;
    ScriptBreakpoint breakpoint(lineNumber, columnNumber, condition, enabled);
    if (!resolveBreakpoint(breakpointId, sourceId, breakpoint, actualLineNumber, actualColumnNumber))
        return;
    *outBreakpointId = breakpointId;
}

void InspectorDebuggerAgent::removeJavaScriptBreakpoint(ErrorString*, const String& breakpointId)
{
    RefPtr<InspectorObject> breakpointsCookie = m_inspectorState->getObject(DebuggerAgentState::javaScriptBreakpoints);
    breakpointsCookie->remove(breakpointId);
    m_inspectorState->setObject(DebuggerAgentState::javaScriptBreakpoints, breakpointsCookie);

    BreakpointIdToDebugServerBreakpointIdsMap::iterator debugServerBreakpointIdsIterator = m_breakpointIdToDebugServerBreakpointIds.find(breakpointId);
    if (debugServerBreakpointIdsIterator == m_breakpointIdToDebugServerBreakpointIds.end())
        return;
    for (size_t i = 0; i < debugServerBreakpointIdsIterator->second.size(); ++i)
        ScriptDebugServer::shared().removeBreakpoint(debugServerBreakpointIdsIterator->second[i]);
    m_breakpointIdToDebugServerBreakpointIds.remove(debugServerBreakpointIdsIterator);
}

void InspectorDebuggerAgent::continueToLocation(ErrorString* error, const String& sourceId, int lineNumber, int columnNumber)
{
    if (!m_continueToLocationBreakpointId.isEmpty()) {
        ScriptDebugServer::shared().removeBreakpoint(m_continueToLocationBreakpointId);
        m_continueToLocationBreakpointId = "";
    }
    ScriptBreakpoint breakpoint(lineNumber, columnNumber, "", true);
    m_continueToLocationBreakpointId = ScriptDebugServer::shared().setBreakpoint(sourceId, breakpoint, &lineNumber, &columnNumber);
    resume(error);
}

bool InspectorDebuggerAgent::resolveBreakpoint(const String& breakpointId, const String& sourceId, const ScriptBreakpoint& breakpoint, int* actualLineNumber, int* actualColumnNumber)
{
    ScriptsMap::iterator scriptIterator = m_scripts.find(sourceId);
    if (scriptIterator == m_scripts.end())
        return false;
    Script& script = scriptIterator->second;
    if (breakpoint.lineNumber < script.lineOffset)
        return false;
    if (!script.linesCount) {
        script.linesCount = 1;
        for (size_t i = 0; i < script.data.length(); ++i) {
            if (script.data[i] == '\n')
                script.linesCount += 1;
        }
    }
    if (breakpoint.lineNumber >= script.lineOffset + script.linesCount)
        return false;

    String debugServerBreakpointId = ScriptDebugServer::shared().setBreakpoint(sourceId, breakpoint, actualLineNumber, actualColumnNumber);
    if (debugServerBreakpointId.isEmpty())
        return false;

    BreakpointIdToDebugServerBreakpointIdsMap::iterator debugServerBreakpointIdsIterator = m_breakpointIdToDebugServerBreakpointIds.find(breakpointId);
    if (debugServerBreakpointIdsIterator == m_breakpointIdToDebugServerBreakpointIds.end())
        debugServerBreakpointIdsIterator = m_breakpointIdToDebugServerBreakpointIds.set(breakpointId, Vector<String>()).first;
    debugServerBreakpointIdsIterator->second.append(debugServerBreakpointId);

    return true;
}

void InspectorDebuggerAgent::editScriptSource(ErrorString*, const String& sourceID, const String& newContent, bool* success, String* result, RefPtr<InspectorValue>* newCallFrames)
{
    if ((*success = ScriptDebugServer::shared().editScriptSource(sourceID, newContent, *result)))
        *newCallFrames = currentCallFrames();
}

void InspectorDebuggerAgent::getScriptSource(ErrorString*, const String& sourceID, String* scriptSource)
{
    *scriptSource = m_scripts.get(sourceID).data;
}

void InspectorDebuggerAgent::schedulePauseOnNextStatement(DebuggerEventType type, PassRefPtr<InspectorValue> data)
{
    if (m_javaScriptPauseScheduled)
        return;
    m_breakProgramDetails = InspectorObject::create();
    m_breakProgramDetails->setNumber("eventType", type);
    m_breakProgramDetails->setValue("eventData", data);
    ScriptDebugServer::shared().setPauseOnNextStatement(true);
}

void InspectorDebuggerAgent::cancelPauseOnNextStatement()
{
    if (m_javaScriptPauseScheduled)
        return;
    m_breakProgramDetails = 0;
    ScriptDebugServer::shared().setPauseOnNextStatement(false);
}

void InspectorDebuggerAgent::pause(ErrorString*)
{
    schedulePauseOnNextStatement(JavaScriptPauseEventType, InspectorObject::create());
    m_javaScriptPauseScheduled = true;
}

void InspectorDebuggerAgent::resume(ErrorString*)
{
    ScriptDebugServer::shared().continueProgram();
}

void InspectorDebuggerAgent::stepOver(ErrorString*)
{
    ScriptDebugServer::shared().stepOverStatement();
}

void InspectorDebuggerAgent::stepInto(ErrorString*)
{
    ScriptDebugServer::shared().stepIntoStatement();
}

void InspectorDebuggerAgent::stepOut(ErrorString*)
{
    ScriptDebugServer::shared().stepOutOfFunction();
}

void InspectorDebuggerAgent::setPauseOnExceptionsState(ErrorString*, long pauseState, long* newState)
{
    ScriptDebugServer::shared().setPauseOnExceptionsState(static_cast<ScriptDebugServer::PauseOnExceptionsState>(pauseState));
    *newState = ScriptDebugServer::shared().pauseOnExceptionsState();
}

void InspectorDebuggerAgent::evaluateOnCallFrame(ErrorString*, PassRefPtr<InspectorObject> callFrameId, const String& expression, const String& objectGroup, bool includeCommandLineAPI, RefPtr<InspectorValue>* result)
{
    InjectedScript injectedScript = m_injectedScriptHost->injectedScriptForObjectId(callFrameId.get());
    if (!injectedScript.hasNoValue())
        injectedScript.evaluateOnCallFrame(callFrameId, expression, objectGroup, includeCommandLineAPI, result);
}

void InspectorDebuggerAgent::getCompletionsOnCallFrame(ErrorString*, PassRefPtr<InspectorObject> callFrameId, const String& expression, bool includeCommandLineAPI, RefPtr<InspectorValue>* result)
{
    InjectedScript injectedScript = m_injectedScriptHost->injectedScriptForObjectId(callFrameId.get());
    if (!injectedScript.hasNoValue())
        injectedScript.getCompletionsOnCallFrame(callFrameId, expression, includeCommandLineAPI, result);
}

PassRefPtr<InspectorValue> InspectorDebuggerAgent::currentCallFrames()
{
    if (!m_pausedScriptState)
        return InspectorValue::null();
    InjectedScript injectedScript = m_injectedScriptHost->injectedScriptFor(m_pausedScriptState);
    if (injectedScript.hasNoValue()) {
        ASSERT_NOT_REACHED();
        return InspectorValue::null();
    }
    return injectedScript.callFrames();
}

// JavaScriptDebugListener functions

void InspectorDebuggerAgent::didParseSource(const String& sourceID, const String& url, const String& data, int lineOffset, int columnOffset, ScriptWorldType worldType)
{
    // Don't send script content to the front end until it's really needed.
    m_frontend->parsedScriptSource(sourceID, url, lineOffset, columnOffset, data.length(), worldType);

    m_scripts.set(sourceID, Script(url, data, lineOffset, columnOffset));

    if (url.isEmpty())
        return;

    RefPtr<InspectorObject> breakpointsCookie = m_inspectorState->getObject(DebuggerAgentState::javaScriptBreakpoints);
    for (InspectorObject::iterator it = breakpointsCookie->begin(); it != breakpointsCookie->end(); ++it) {
        RefPtr<InspectorObject> breakpointObject = it->second->asObject();
        String breakpointURL;
        breakpointObject->getString("url", &breakpointURL);
        if (breakpointURL != url)
            continue;
        ScriptBreakpoint breakpoint;
        breakpointObject->getNumber("lineNumber", &breakpoint.lineNumber);
        breakpointObject->getNumber("columnNumber", &breakpoint.columnNumber);
        breakpointObject->getString("condition", &breakpoint.condition);
        breakpointObject->getBoolean("enabled", &breakpoint.enabled);
        int actualLineNumber = 0, actualColumnNumber = 0;
        if (resolveBreakpoint(it->first, sourceID, breakpoint, &actualLineNumber, &actualColumnNumber))
            m_frontend->breakpointResolved(it->first, sourceID, actualLineNumber, actualColumnNumber);
    }
}

void InspectorDebuggerAgent::failedToParseSource(const String& url, const String& data, int firstLine, int errorLine, const String& errorMessage)
{
    m_frontend->failedToParseScriptSource(url, data, firstLine, errorLine, errorMessage);
}

void InspectorDebuggerAgent::didPause(ScriptState* scriptState)
{
    ASSERT(scriptState && !m_pausedScriptState);
    m_pausedScriptState = scriptState;

    if (!m_breakProgramDetails)
        m_breakProgramDetails = InspectorObject::create();
    m_breakProgramDetails->setValue("callFrames", currentCallFrames());

    m_frontend->pausedScript(m_breakProgramDetails);
    m_javaScriptPauseScheduled = false;

    if (!m_continueToLocationBreakpointId.isEmpty()) {
        ScriptDebugServer::shared().removeBreakpoint(m_continueToLocationBreakpointId);
        m_continueToLocationBreakpointId = "";
    }
}

void InspectorDebuggerAgent::didContinue()
{
    m_pausedScriptState = 0;
    m_breakProgramDetails = 0;
    m_frontend->resumedScript();
}

void InspectorDebuggerAgent::breakProgram(DebuggerEventType type, PassRefPtr<InspectorValue> data)
{
    m_breakProgramDetails = InspectorObject::create();
    m_breakProgramDetails->setNumber("eventType", type);
    m_breakProgramDetails->setValue("eventData", data);
    ScriptDebugServer::shared().breakProgram();
}

void InspectorDebuggerAgent::clear()
{
    m_pausedScriptState = 0;
    m_scripts.clear();
    m_breakpointIdToDebugServerBreakpointIds.clear();
    m_continueToLocationBreakpointId = String();
    m_breakProgramDetails.clear();
    m_javaScriptPauseScheduled = false;
}

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER) && ENABLE(INSPECTOR)
