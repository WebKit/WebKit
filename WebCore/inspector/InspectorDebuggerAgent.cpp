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

#if ENABLE(JAVASCRIPT_DEBUGGER)
#include "InjectedScript.h"
#include "InjectedScriptHost.h"
#include "InspectorFrontend.h"
#include "InspectorValues.h"
#include "PlatformString.h"
#include "ScriptDebugServer.h"
#include <wtf/MD5.h>
#include <wtf/text/CString.h>

namespace WebCore {

static String formatBreakpointId(const String& sourceID, unsigned lineNumber)
{
    return String::format("%s:%d", sourceID.utf8().data(), lineNumber);
}

PassOwnPtr<InspectorDebuggerAgent> InspectorDebuggerAgent::create(InspectorController* inspectorController, InspectorFrontend* frontend)
{
    OwnPtr<InspectorDebuggerAgent> agent = adoptPtr(new InspectorDebuggerAgent(inspectorController, frontend));
    ScriptDebugServer::shared().clearBreakpoints();
    // FIXME(WK44513): breakpoints activated flag should be synchronized between all front-ends
    ScriptDebugServer::shared().setBreakpointsActivated(true);
    ScriptDebugServer::shared().addListener(agent.get(), inspectorController->inspectedPage());
    return agent.release();
}

InspectorDebuggerAgent::InspectorDebuggerAgent(InspectorController* inspectorController, InspectorFrontend* frontend)
    : m_inspectorController(inspectorController)
    , m_frontend(frontend)
    , m_pausedScriptState(0)
    , m_breakpointsLoaded(false)
{
}

InspectorDebuggerAgent::~InspectorDebuggerAgent()
{
    ScriptDebugServer::shared().removeListener(this, m_inspectorController->inspectedPage());
    m_pausedScriptState = 0;
}

bool InspectorDebuggerAgent::isDebuggerAlwaysEnabled()
{
    return ScriptDebugServer::shared().isDebuggerAlwaysEnabled();
}

void InspectorDebuggerAgent::activateBreakpoints()
{
    ScriptDebugServer::shared().activateBreakpoints();
}

void InspectorDebuggerAgent::deactivateBreakpoints()
{
    ScriptDebugServer::shared().deactivateBreakpoints();
}

void InspectorDebuggerAgent::setBreakpoint(const String& sourceID, unsigned lineNumber, bool enabled, const String& condition, bool* success, unsigned int* actualLineNumber)
{
    ScriptBreakpoint breakpoint(enabled, condition);
    *success = ScriptDebugServer::shared().setBreakpoint(sourceID, breakpoint, lineNumber, actualLineNumber);
    if (!*success)
        return;

    String url = m_sourceIDToURL.get(sourceID);
    if (url.isEmpty())
        return;

    String breakpointId = formatBreakpointId(sourceID, *actualLineNumber);
    m_breakpointsMapping.set(breakpointId, *actualLineNumber);

    String key = md5Base16(url);
    HashMap<String, SourceBreakpoints>::iterator it = m_stickyBreakpoints.find(key);
    if (it == m_stickyBreakpoints.end())
        it = m_stickyBreakpoints.set(key, SourceBreakpoints()).first;
    it->second.set(*actualLineNumber, breakpoint);
    saveBreakpoints();
}

void InspectorDebuggerAgent::removeBreakpoint(const String& sourceID, unsigned lineNumber)
{
    ScriptDebugServer::shared().removeBreakpoint(sourceID, lineNumber);

    String url = m_sourceIDToURL.get(sourceID);
    if (url.isEmpty())
        return;

    String breakpointId = formatBreakpointId(sourceID, lineNumber);
    HashMap<String, unsigned>::iterator mappingIt = m_breakpointsMapping.find(breakpointId);
    if (mappingIt == m_breakpointsMapping.end())
        return;
    unsigned stickyLine = mappingIt->second;
    m_breakpointsMapping.remove(mappingIt);

    HashMap<String, SourceBreakpoints>::iterator it = m_stickyBreakpoints.find(md5Base16(url));
    if (it == m_stickyBreakpoints.end())
        return;

    it->second.remove(stickyLine);
    saveBreakpoints();
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
    m_breakProgramDetails = InspectorObject::create();
    m_breakProgramDetails->setNumber("eventType", type);
    m_breakProgramDetails->setValue("eventData", data);
    ScriptDebugServer::shared().setPauseOnNextStatement(true);
}

void InspectorDebuggerAgent::cancelPauseOnNextStatement()
{
    m_breakProgramDetails = 0;
    ScriptDebugServer::shared().setPauseOnNextStatement(false);
}

void InspectorDebuggerAgent::pause()
{
    schedulePauseOnNextStatement(JavaScriptPauseEventType, InspectorObject::create());
}

void InspectorDebuggerAgent::resume()
{
    ScriptDebugServer::shared().continueProgram();
}

void InspectorDebuggerAgent::stepOverStatement()
{
    ScriptDebugServer::shared().stepOverStatement();
}

void InspectorDebuggerAgent::stepIntoStatement()
{
    ScriptDebugServer::shared().stepIntoStatement();
}

void InspectorDebuggerAgent::stepOutOfFunction()
{
    ScriptDebugServer::shared().stepOutOfFunction();
}

void InspectorDebuggerAgent::setPauseOnExceptionsState(long pauseState, long* newState)
{
    ScriptDebugServer::shared().setPauseOnExceptionsState(static_cast<ScriptDebugServer::PauseOnExceptionsState>(pauseState));
    *newState = ScriptDebugServer::shared().pauseOnExceptionsState();
}

long InspectorDebuggerAgent::pauseOnExceptionsState()
{
    return ScriptDebugServer::shared().pauseOnExceptionsState();
}

void InspectorDebuggerAgent::clearForPageNavigation()
{
    m_sourceIDToURL.clear();
    m_scriptIDToContent.clear();
    m_stickyBreakpoints.clear();
    m_breakpointsMapping.clear();
    m_breakpointsLoaded = false;
}

String InspectorDebuggerAgent::md5Base16(const String& string)
{
    static const char digits[] = "0123456789abcdef";

    MD5 md5;
    md5.addBytes(reinterpret_cast<const uint8_t*>(string.characters()), string.length() * 2);
    Vector<uint8_t, 16> digest;
    md5.checksum(digest);

    Vector<char, 32> result;
    for (int i = 0; i < 16; ++i) {
        result.append(digits[(digest[i] >> 4) & 0xf]);
        result.append(digits[digest[i] & 0xf]);
    }
    return String(result.data(), result.size());
}

PassRefPtr<InspectorValue> InspectorDebuggerAgent::currentCallFrames()
{
    if (!m_pausedScriptState)
        return InspectorValue::null();
    InjectedScript injectedScript = m_inspectorController->injectedScriptHost()->injectedScriptFor(m_pausedScriptState);
    if (injectedScript.hasNoValue()) {
        ASSERT_NOT_REACHED();
        return InspectorValue::null();
    }
    return injectedScript.callFrames();
}

void InspectorDebuggerAgent::loadBreakpoints()
{
    if (m_breakpointsLoaded)
        return;
    m_breakpointsLoaded = true;

    RefPtr<InspectorValue> parsedSetting = m_inspectorController->loadBreakpoints();
    if (!parsedSetting)
        return;
    RefPtr<InspectorObject> breakpoints = parsedSetting->asObject();
    if (!breakpoints)
        return;
    for (InspectorObject::iterator it = breakpoints->begin(); it != breakpoints->end(); ++it) {
        RefPtr<InspectorObject> breakpointsForURL = it->second->asObject();
        if (!breakpointsForURL)
            continue;
        HashMap<String, SourceBreakpoints>::iterator sourceBreakpointsIt = m_stickyBreakpoints.set(it->first, SourceBreakpoints()).first;
        ScriptBreakpoint::sourceBreakpointsFromInspectorObject(breakpointsForURL, &sourceBreakpointsIt->second);
    }
}

void InspectorDebuggerAgent::saveBreakpoints()
{
    RefPtr<InspectorObject> breakpoints = InspectorObject::create();
    for (HashMap<String, SourceBreakpoints>::iterator it(m_stickyBreakpoints.begin()); it != m_stickyBreakpoints.end(); ++it) {
        if (it->second.isEmpty())
            continue;
        RefPtr<InspectorObject> breakpointsForURL = ScriptBreakpoint::inspectorObjectFromSourceBreakpoints(it->second);
        breakpoints->setObject(it->first, breakpointsForURL);
    }
    m_inspectorController->saveBreakpoints(breakpoints);
}

// JavaScriptDebugListener functions

void InspectorDebuggerAgent::didParseSource(const String& sourceID, const String& url, const String& data, int firstLine, ScriptWorldType worldType)
{
    // Don't send script content to the front end until it's really needed.
    m_frontend->parsedScriptSource(sourceID, url, "", firstLine, worldType);

    m_scriptIDToContent.set(sourceID, data);

    if (url.isEmpty())
        return;

    loadBreakpoints();
    HashMap<String, SourceBreakpoints>::iterator it = m_stickyBreakpoints.find(md5Base16(url));
    if (it != m_stickyBreakpoints.end()) {
        for (SourceBreakpoints::iterator breakpointIt = it->second.begin(); breakpointIt != it->second.end(); ++breakpointIt) {
            int lineNumber = breakpointIt->first;
            if (firstLine > lineNumber)
                continue;
            unsigned actualLineNumber = 0;
            bool success = ScriptDebugServer::shared().setBreakpoint(sourceID, breakpointIt->second, lineNumber, &actualLineNumber);
            if (!success)
                continue;
            m_frontend->restoredBreakpoint(sourceID, url, actualLineNumber, breakpointIt->second.enabled, breakpointIt->second.condition);
            String breakpointId = formatBreakpointId(sourceID, actualLineNumber);
            m_breakpointsMapping.set(breakpointId, lineNumber);
        }
    }
    m_sourceIDToURL.set(sourceID, url);
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

#endif // ENABLE(JAVASCRIPT_DEBUGGER)
