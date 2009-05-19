/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "InspectorFrontend.h"

#include "ConsoleMessage.h"
#include "Frame.h"
#include "InspectorController.h"  // TODO(pfeldman): Extract SpecialPanels to remove include.
#include "JSONObject.h"
#include "Node.h"
#include "ScriptFunctionCall.h"
#include "ScriptObject.h"
#include "ScriptObjectQuarantine.h"
#include "ScriptState.h"
#include "ScriptString.h"

#if ENABLE(JAVASCRIPT_DEBUGGER)
#include <parser/SourceCode.h>
#include <runtime/JSValue.h>
#include <runtime/UString.h>
#endif

namespace WebCore {

static void callSimpleFunction(ScriptState* scriptState, const ScriptObject& thisObject, const char* functionName)
{
    ScriptFunctionCall function(scriptState, thisObject, functionName);
    function.call();
}

InspectorFrontend::InspectorFrontend(ScriptState* scriptState, ScriptObject webInspector)
    : m_scriptState(scriptState)
    , m_webInspector(webInspector)
{
}

InspectorFrontend::~InspectorFrontend() 
{
    m_webInspector = ScriptObject();
}

JSONObject InspectorFrontend::newJSONObject() {
    return JSONObject::createNew(m_scriptState);
}

void InspectorFrontend::addMessageToConsole(const JSONObject& messageObj, const Vector<ScriptString>& frames, const Vector<ScriptValue> wrappedArguments, const String& message)
{
    ScriptFunctionCall function(m_scriptState, m_webInspector, "addMessageToConsole");
    function.appendArgument(messageObj.scriptObject());
    if (!frames.isEmpty()) {
        for (unsigned i = 0; i < frames.size(); ++i)
            function.appendArgument(frames[i]);
    } else if (!wrappedArguments.isEmpty()) {
        for (unsigned i = 0; i < wrappedArguments.size(); ++i)
            function.appendArgument(wrappedArguments[i]);
    } else
        function.appendArgument(message);
    function.call();
}

bool InspectorFrontend::addResource(long long identifier, const JSONObject& resourceObj)
{
    ScriptFunctionCall function(m_scriptState, m_webInspector, "addResource");
    function.appendArgument(identifier);
    function.appendArgument(resourceObj.scriptObject());
    bool hadException = false;
    function.call(hadException);
    return !hadException;
}

bool InspectorFrontend::updateResource(long long identifier, const JSONObject& resourceObj)
{
    ScriptFunctionCall function(m_scriptState, m_webInspector, "updateResource");
    function.appendArgument(identifier);
    function.appendArgument(resourceObj.scriptObject());
    bool hadException = false;
    function.call(hadException);
    return !hadException;
}

void InspectorFrontend::removeResource(long long identifier)
{
    ScriptFunctionCall function(m_scriptState, m_webInspector, "removeResource");
    function.appendArgument(identifier);
    function.call();
}

void InspectorFrontend::updateFocusedNode(Node* node)
{
    ScriptObject quarantinedNode;
    if (!getQuarantinedScriptObject(node, quarantinedNode))
        return;

    ScriptFunctionCall function(m_scriptState, m_webInspector, "updateFocusedNode");
    function.appendArgument(quarantinedNode);
    function.call();
}

void InspectorFrontend::setAttachedWindow(bool attached)
{
    ScriptFunctionCall function(m_scriptState, m_webInspector, "setAttachedWindow");
    function.appendArgument(attached);
    function.call();
}

void InspectorFrontend::inspectedWindowScriptObjectCleared(Frame* frame)
{
    ScriptObject domWindow;
    if (!getQuarantinedScriptObject(frame->domWindow(), domWindow))
        return;

    ScriptFunctionCall function(m_scriptState, m_webInspector, "inspectedWindowCleared");
    function.appendArgument(domWindow);
    function.call();
}

void InspectorFrontend::showPanel(int panel)
{
    const char* showFunctionName;
    switch (panel) {
        case InspectorController::ConsolePanel:
            showFunctionName = "showConsole";
            break;
        case InspectorController::DatabasesPanel:
            showFunctionName = "showDatabasesPanel";
            break;
        case InspectorController::ElementsPanel:
            showFunctionName = "showElementsPanel";
            break;
        case InspectorController::ProfilesPanel:
            showFunctionName = "showProfilesPanel";
            break;
        case InspectorController::ResourcesPanel:
            showFunctionName = "showResourcesPanel";
            break;
        case InspectorController::ScriptsPanel:
            showFunctionName = "showScriptsPanel";
            break;
        default:
            ASSERT_NOT_REACHED();
            showFunctionName = 0;
    }

    if (showFunctionName)
        callSimpleFunction(m_scriptState, m_webInspector, showFunctionName);
}

void InspectorFrontend::populateInterface()
{
    callSimpleFunction(m_scriptState, m_webInspector, "populateInterface");
}

void InspectorFrontend::reset()
{
    callSimpleFunction(m_scriptState, m_webInspector, "reset");
}


#if ENABLE(JAVASCRIPT_DEBUGGER)
void InspectorFrontend::debuggerWasEnabled()
{
    callSimpleFunction(m_scriptState, m_webInspector, "debuggerWasEnabled");
}

void InspectorFrontend::debuggerWasDisabled()
{
    callSimpleFunction(m_scriptState, m_webInspector, "debuggerWasDisabled");
}

void InspectorFrontend::profilerWasEnabled()
{
    callSimpleFunction(m_scriptState, m_webInspector, "profilerWasEnabled");
}

void InspectorFrontend::profilerWasDisabled()
{
    callSimpleFunction(m_scriptState, m_webInspector, "profilerWasDisabled");
}

void InspectorFrontend::parsedScriptSource(const JSC::SourceCode& source)
{
    ScriptFunctionCall function(m_scriptState, m_webInspector, "parsedScriptSource");
    function.appendArgument(static_cast<long long>(source.provider()->asID()));
    function.appendArgument(source.provider()->url());
    function.appendArgument(JSC::UString(source.data(), source.length()));
    function.appendArgument(source.firstLine());
    function.call();
}

void InspectorFrontend::failedToParseScriptSource(const JSC::SourceCode& source, int errorLine, const JSC::UString& errorMessage)
{
    ScriptFunctionCall function(m_scriptState, m_webInspector, "failedToParseScriptSource");
    function.appendArgument(source.provider()->url());
    function.appendArgument(JSC::UString(source.data(), source.length()));
    function.appendArgument(source.firstLine());
    function.appendArgument(errorLine);
    function.appendArgument(errorMessage);
    function.call();
}

void InspectorFrontend::addProfile(const JSC::JSValue& profile)
{
    ScriptFunctionCall function(m_scriptState, m_webInspector, "addProfile");
    function.appendArgument(profile);
    function.call();
}

void InspectorFrontend::setRecordingProfile(bool isProfiling)
{
    ScriptFunctionCall function(m_scriptState, m_webInspector, "setRecordingProfile");
    function.appendArgument(isProfiling);
    function.call();
}

void InspectorFrontend::pausedScript()
{
    callSimpleFunction(m_scriptState, m_webInspector, "pausedScript");
}

void InspectorFrontend::resumedScript()
{
    callSimpleFunction(m_scriptState, m_webInspector, "resumedScript");
}
#endif

#if ENABLE(DATABASE)
bool InspectorFrontend::addDatabase(const JSONObject& dbObject)
{
    ScriptFunctionCall function(m_scriptState, m_webInspector, "addDatabase");
    function.appendArgument(dbObject.scriptObject());
    bool hadException = false;
    function.call(hadException);
    return !hadException;
}
#endif

#if ENABLE(DOM_STORAGE)
bool InspectorFrontend::addDOMStorage(const JSONObject& domStorageObj)
{
    ScriptFunctionCall function(m_scriptState, m_webInspector, "addDOMStorage");
    function.appendArgument(domStorageObj.scriptObject());
    bool hadException = false;
    function.call(hadException);
    return !hadException;
}
#endif

} // namespace WebCore
