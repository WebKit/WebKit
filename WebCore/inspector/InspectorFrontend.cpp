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
#include "InspectorJSONObject.h"
#include "Node.h"
#include "ScriptFunctionCall.h"
#include "ScriptObject.h"
#include "ScriptObjectQuarantine.h"
#include "ScriptState.h"
#include "ScriptString.h"
#include <wtf/OwnPtr.h>

#if ENABLE(JAVASCRIPT_DEBUGGER)
#include <parser/SourceCode.h>
#include <runtime/JSValue.h>
#include <runtime/UString.h>
#endif

namespace WebCore {

InspectorFrontend::InspectorFrontend(ScriptState* scriptState, ScriptObject webInspector)
    : m_scriptState(scriptState)
    , m_webInspector(webInspector)
{
}

InspectorFrontend::~InspectorFrontend() 
{
    m_webInspector = ScriptObject();
}

InspectorJSONObject InspectorFrontend::newInspectorJSONObject() {
    return InspectorJSONObject::createNew(m_scriptState);
}

void InspectorFrontend::addMessageToConsole(const InspectorJSONObject& messageObj, const Vector<ScriptString>& frames, const Vector<ScriptValue> wrappedArguments, const String& message)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("addMessageToConsole"));
    function->appendArgument(messageObj.scriptObject());
    if (!frames.isEmpty()) {
        for (unsigned i = 0; i < frames.size(); ++i)
            function->appendArgument(frames[i]);
    } else if (!wrappedArguments.isEmpty()) {
        for (unsigned i = 0; i < wrappedArguments.size(); ++i)
            function->appendArgument(wrappedArguments[i]);
    } else
        function->appendArgument(message);
    function->call();
}

bool InspectorFrontend::addResource(long long identifier, const InspectorJSONObject& resourceObj)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("addResource"));
    function->appendArgument(identifier);
    function->appendArgument(resourceObj.scriptObject());
    bool hadException = false;
    function->call(hadException);
    return !hadException;
}

bool InspectorFrontend::updateResource(long long identifier, const InspectorJSONObject& resourceObj)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("updateResource"));
    function->appendArgument(identifier);
    function->appendArgument(resourceObj.scriptObject());
    bool hadException = false;
    function->call(hadException);
    return !hadException;
}

void InspectorFrontend::removeResource(long long identifier)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("removeResource"));
    function->appendArgument(identifier);
    function->call();
}

void InspectorFrontend::updateFocusedNode(Node* node)
{
    ScriptObject quarantinedNode;
    if (!getQuarantinedScriptObject(node, quarantinedNode))
        return;

    OwnPtr<ScriptFunctionCall> function(newFunctionCall("updateFocusedNode"));
    function->appendArgument(quarantinedNode);
    function->call();
}

void InspectorFrontend::setAttachedWindow(bool attached)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("setAttachedWindow"));
    function->appendArgument(attached);
    function->call();
}

void InspectorFrontend::inspectedWindowScriptObjectCleared(Frame* frame)
{
    ScriptObject domWindow;
    if (!getQuarantinedScriptObject(frame->domWindow(), domWindow))
        return;

    OwnPtr<ScriptFunctionCall> function(newFunctionCall("inspectedWindowCleared"));
    function->appendArgument(domWindow);
    function->call();
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
        callSimpleFunction(showFunctionName);
}

void InspectorFrontend::populateInterface()
{
    callSimpleFunction("populateInterface");
}

void InspectorFrontend::reset()
{
    callSimpleFunction("reset");
}

void InspectorFrontend::resourceTrackingWasEnabled()
{
    callSimpleFunction("resourceTrackingWasEnabled");
}

void InspectorFrontend::resourceTrackingWasDisabled()
{
    callSimpleFunction("resourceTrackingWasDisabled");
}

#if ENABLE(JAVASCRIPT_DEBUGGER)
void InspectorFrontend::attachDebuggerWhenShown()
{
    callSimpleFunction("attachDebuggerWhenShown");
}

void InspectorFrontend::debuggerWasEnabled()
{
    callSimpleFunction("debuggerWasEnabled");
}

void InspectorFrontend::debuggerWasDisabled()
{
    callSimpleFunction("debuggerWasDisabled");
}

void InspectorFrontend::profilerWasEnabled()
{
    callSimpleFunction("profilerWasEnabled");
}

void InspectorFrontend::profilerWasDisabled()
{
    callSimpleFunction("profilerWasDisabled");
}

void InspectorFrontend::parsedScriptSource(const JSC::SourceCode& source)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("parsedScriptSource"));
    function->appendArgument(JSC::UString(JSC::UString::from(source.provider()->asID())));
    function->appendArgument(source.provider()->url());
    function->appendArgument(JSC::UString(source.data(), source.length()));
    function->appendArgument(source.firstLine());
    function->call();
}

void InspectorFrontend::failedToParseScriptSource(const JSC::SourceCode& source, int errorLine, const JSC::UString& errorMessage)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("failedToParseScriptSource"));
    function->appendArgument(source.provider()->url());
    function->appendArgument(JSC::UString(source.data(), source.length()));
    function->appendArgument(source.firstLine());
    function->appendArgument(errorLine);
    function->appendArgument(errorMessage);
    function->call();
}

void InspectorFrontend::addProfile(const JSC::JSValue& profile)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("addProfile"));
    function->appendArgument(profile);
    function->call();
}

void InspectorFrontend::setRecordingProfile(bool isProfiling)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("setRecordingProfile"));
    function->appendArgument(isProfiling);
    function->call();
}

void InspectorFrontend::pausedScript()
{
    callSimpleFunction("pausedScript");
}

void InspectorFrontend::resumedScript()
{
    callSimpleFunction("resumedScript");
}
#endif

#if ENABLE(DATABASE)
bool InspectorFrontend::addDatabase(const InspectorJSONObject& dbObject)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("addDatabase"));
    function->appendArgument(dbObject.scriptObject());
    bool hadException = false;
    function->call(hadException);
    return !hadException;
}
#endif

#if ENABLE(DOM_STORAGE)
bool InspectorFrontend::addDOMStorage(const InspectorJSONObject& domStorageObj)
{
    OwnPtr<ScriptFunctionCall> function(newFunctionCall("addDOMStorage"));
    function->appendArgument(domStorageObj.scriptObject());
    bool hadException = false;
    function->call(hadException);
    return !hadException;
}
#endif

PassOwnPtr<ScriptFunctionCall> InspectorFrontend::newFunctionCall(const String& functionName)
{
    ScriptFunctionCall* function = new ScriptFunctionCall(m_scriptState, m_webInspector, "dispatch");
    function->appendArgument(functionName);
    return function;
}

void InspectorFrontend::callSimpleFunction(const String& functionName)
{
    ScriptFunctionCall function(m_scriptState, m_webInspector, "dispatch");
    function.appendArgument(functionName);
    function.call();
}

} // namespace WebCore
