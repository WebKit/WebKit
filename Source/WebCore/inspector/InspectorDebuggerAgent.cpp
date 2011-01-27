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
#include "PlatformString.h"
#include "ScriptDebugServer.h"

namespace WebCore {

PassOwnPtr<InspectorDebuggerAgent> InspectorDebuggerAgent::create(InspectorAgent* inspectorAgent, InspectorFrontend* frontend)
{
    OwnPtr<InspectorDebuggerAgent> agent = adoptPtr(new InspectorDebuggerAgent(inspectorAgent, frontend));
    ScriptDebugServer::shared().clearBreakpoints();
    // FIXME(WK44513): breakpoints activated flag should be synchronized between all front-ends
    ScriptDebugServer::shared().setBreakpointsActivated(true);
    ScriptDebugServer::shared().addListener(agent.get(), inspectorAgent->inspectedPage());
    return agent.release();
}

InspectorDebuggerAgent::InspectorDebuggerAgent(InspectorAgent* inspectorAgent, InspectorFrontend* frontend)
    : m_inspectorAgent(inspectorAgent)
    , m_frontend(frontend)
    , m_pausedScriptState(0)
    , m_javaScriptPauseScheduled(false)
    , m_breakpointsRestored(false)
{
}

InspectorDebuggerAgent::~InspectorDebuggerAgent()
{
    ScriptDebugServer::shared().removeListener(this, m_inspectorAgent->inspectedPage());
    m_pausedScriptState = 0;
}

void InspectorDebuggerAgent::activateBreakpoints()
{
    ScriptDebugServer::shared().activateBreakpoints();
}

void InspectorDebuggerAgent::deactivateBreakpoints()
{
    ScriptDebugServer::shared().deactivateBreakpoints();
}

void InspectorDebuggerAgent::setAllJavaScriptBreakpoints(PassRefPtr<InspectorObject> breakpoints)
{
    m_inspectorAgent->state()->setObject(InspectorState::javaScriptBreakpoints, breakpoints);
    if (!m_breakpointsRestored) {
        restoreBreakpoints(m_inspectorAgent->inspectedURLWithoutFragment());
        m_breakpointsRestored = true;
    }
}

void InspectorDebuggerAgent::inspectedURLChanged(const String& url)
{
    m_scriptIDToContent.clear();
    m_urlToSourceIDs.clear();
    restoreBreakpoints(url);
}

void InspectorDebuggerAgent::restoreBreakpoints(const String& inspectedURL)
{
    m_stickyBreakpoints.clear();

    RefPtr<InspectorObject> allBreakpoints = m_inspectorAgent->state()->getObject(InspectorState::javaScriptBreakpoints);
    RefPtr<InspectorArray> breakpoints = allBreakpoints->getArray(inspectedURL);
    if (!breakpoints)
        return;
    for (unsigned i = 0; i < breakpoints->length(); ++i) {
        RefPtr<InspectorObject> breakpoint = breakpoints->get(i)->asObject();
        if (!breakpoint)
            continue;
        String url;
        if (!breakpoint->getString("url", &url))
            continue;
        double lineNumber;
        if (!breakpoint->getNumber("lineNumber", &lineNumber))
            continue;
        double columnNumber;
        if (!breakpoint->getNumber("columnNumber", &columnNumber))
            return;
        String condition;
        if (!breakpoint->getString("condition", &condition))
            continue;
        bool enabled;
        if (!breakpoint->getBoolean("enabled", &enabled))
            continue;
        ScriptBreakpoint scriptBreakpoint((long) lineNumber, (long) columnNumber, condition, enabled);
        setStickyBreakpoint(url, scriptBreakpoint);
    }
}

void InspectorDebuggerAgent::setStickyBreakpoint(const String& url, const ScriptBreakpoint& breakpoint)
{
    InspectedURLToBreakpointsMap::iterator it = m_stickyBreakpoints.find(url);
    if (it == m_stickyBreakpoints.end())
        it = m_stickyBreakpoints.set(url, LocationToBreakpointMap()).first;
    it->second.set(Location(breakpoint.lineNumber, breakpoint.columnNumber), breakpoint);

    URLToSourceIDsMap::iterator urlToSourceIDsIterator = m_urlToSourceIDs.find(url);
    if (urlToSourceIDsIterator == m_urlToSourceIDs.end())
        return;
    const Vector<String>& sourceIDs = urlToSourceIDsIterator->second;
    for (size_t i = 0; i < sourceIDs.size(); ++i)
        restoreBreakpoint(sourceIDs[i], breakpoint);
}

void InspectorDebuggerAgent::setBreakpoint(PassRefPtr<InspectorObject> breakpoint, String* breakpointId, long* actualLineNumber, long* actualColumnNumber)
{
    String sourceID;
    if (!breakpoint->getString("sourceID", &sourceID))
        return;
    double lineNumber;
    if (!breakpoint->getNumber("lineNumber", &lineNumber))
        return;
    double columnNumber;
    if (!breakpoint->getNumber("columnNumber", &columnNumber))
        return;
    String condition;
    if (!breakpoint->getString("condition", &condition))
        return;
    bool enabled;
    if (!breakpoint->getBoolean("enabled", &enabled))
        return;
    ScriptBreakpoint scriptBreakpoint((long) lineNumber, (long) columnNumber, condition, enabled);
    *breakpointId = ScriptDebugServer::shared().setBreakpoint(sourceID, scriptBreakpoint, actualLineNumber, actualColumnNumber);
}

void InspectorDebuggerAgent::removeBreakpoint(const String& breakpointId)
{
    ScriptDebugServer::shared().removeBreakpoint(breakpointId);
}

void InspectorDebuggerAgent::restoreBreakpoint(const String& sourceID, const ScriptBreakpoint& breakpoint)
{
    long actualLineNumber = 0, actualColumnNumber = 0;
    String breakpointId = ScriptDebugServer::shared().setBreakpoint(sourceID, breakpoint, &actualLineNumber, &actualColumnNumber);
    if (breakpointId.isEmpty())
        return;
    RefPtr<InspectorObject> breakpointData = InspectorObject::create();
    breakpointData->setString("id", breakpointId);
    breakpointData->setString("sourceID", sourceID);
    breakpointData->setNumber("lineNumber", actualLineNumber);
    breakpointData->setNumber("columnNumber", actualColumnNumber);
    breakpointData->setString("condition", breakpoint.condition);
    breakpointData->setBoolean("enabled", breakpoint.enabled);
    breakpointData->setNumber("originalLineNumber", breakpoint.lineNumber);
    breakpointData->setNumber("originalColumnNumber", breakpoint.columnNumber);
    m_frontend->breakpointResolved(breakpointId, breakpointData);
}

void InspectorDebuggerAgent::editScriptSource(const String& sourceID, const String& newContent, bool* success, String* result, RefPtr<InspectorValue>* newCallFrames)
{
    if ((*success = ScriptDebugServer::shared().editScriptSource(sourceID, newContent, *result)))
        *newCallFrames = currentCallFrames();
}

void InspectorDebuggerAgent::getScriptSource(const String& sourceID, String* scriptSource)
{
    *scriptSource = m_scriptIDToContent.get(sourceID);
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

void InspectorDebuggerAgent::pause()
{
    schedulePauseOnNextStatement(JavaScriptPauseEventType, InspectorObject::create());
    m_javaScriptPauseScheduled = true;
}

void InspectorDebuggerAgent::resume()
{
    ScriptDebugServer::shared().continueProgram();
}

void InspectorDebuggerAgent::stepOver()
{
    ScriptDebugServer::shared().stepOverStatement();
}

void InspectorDebuggerAgent::stepInto()
{
    ScriptDebugServer::shared().stepIntoStatement();
}

void InspectorDebuggerAgent::stepOut()
{
    ScriptDebugServer::shared().stepOutOfFunction();
}

void InspectorDebuggerAgent::setPauseOnExceptionsState(long pauseState, long* newState)
{
    ScriptDebugServer::shared().setPauseOnExceptionsState(static_cast<ScriptDebugServer::PauseOnExceptionsState>(pauseState));
    *newState = ScriptDebugServer::shared().pauseOnExceptionsState();
}

void InspectorDebuggerAgent::evaluateOnCallFrame(PassRefPtr<InspectorObject> callFrameId, const String& expression, const String& objectGroup, bool includeCommandLineAPI, RefPtr<InspectorValue>* result)
{
    InjectedScript injectedScript = m_inspectorAgent->injectedScriptHost()->injectedScriptForObjectId(callFrameId.get());
    if (!injectedScript.hasNoValue())
        injectedScript.evaluateOnCallFrame(callFrameId, expression, objectGroup, includeCommandLineAPI, result);
}

void InspectorDebuggerAgent::getCompletionsOnCallFrame(PassRefPtr<InspectorObject> callFrameId, const String& expression, bool includeCommandLineAPI, RefPtr<InspectorValue>* result)
{
    InjectedScript injectedScript = m_inspectorAgent->injectedScriptHost()->injectedScriptForObjectId(callFrameId.get());
    if (!injectedScript.hasNoValue())
        injectedScript.getCompletionsOnCallFrame(callFrameId, expression, includeCommandLineAPI, result);
}

PassRefPtr<InspectorValue> InspectorDebuggerAgent::currentCallFrames()
{
    if (!m_pausedScriptState)
        return InspectorValue::null();
    InjectedScript injectedScript = m_inspectorAgent->injectedScriptHost()->injectedScriptFor(m_pausedScriptState);
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

    m_scriptIDToContent.set(sourceID, data);

    if (url.isEmpty())
        return;

    URLToSourceIDsMap::iterator urlToSourceIDsIterator = m_urlToSourceIDs.find(url);
    if (urlToSourceIDsIterator == m_urlToSourceIDs.end())
        urlToSourceIDsIterator = m_urlToSourceIDs.set(url, Vector<String>()).first;
    urlToSourceIDsIterator->second.append(sourceID);

    InspectedURLToBreakpointsMap::iterator stickyBreakpointsIterator = m_stickyBreakpoints.find(url);
    if (stickyBreakpointsIterator == m_stickyBreakpoints.end())
        return;

    const LocationToBreakpointMap& breakpoints = stickyBreakpointsIterator->second;
    for (LocationToBreakpointMap::const_iterator it = breakpoints.begin(); it != breakpoints.end(); ++it)
        restoreBreakpoint(sourceID, it->second);
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

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER) && ENABLE(INSPECTOR)
